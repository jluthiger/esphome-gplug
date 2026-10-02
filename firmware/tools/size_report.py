#!/usr/bin/env python3
"""Flash and RAM breakdown of the last `esphome compile dev.yaml`, for firmware/MEMORY.md.

    tools/size_report.py            print the tables MEMORY.md is made of, plus its baseline line
    tools/size_report.py --check    compare the build against the baseline in MEMORY.md, exit 1 on drift

MEMORY.md used to be assembled by hand from esp_idf_size output, which is why it went stale: the
numbers were a chore to redo, so they were redone only now and then. This script produces the same
groupings from the linker map and the ELF symbols, so updating the file is a paste, and the check
can tell when that paste is due.

Groups are by archive (ESP-IDF libraries) and by object file (ESPHome, gplug_smi, libsodium/noise).
The `gplug_smi` row is split once more -- HTTP API, ArduinoJson, MQTT, meter decoding, store -- from
the linker map's input sections, since the whole component compiles into two object files.
"Bytes in the image" counts what is stored in flash: code and read-only data run from flash, plus
IRAM code and .data initial values, which are copied to RAM at boot.
"""
import gzip
import json
import re
import subprocess
import sys
from pathlib import Path

FW = Path(__file__).resolve().parent.parent
BUILD = FW / ".esphome/build/gplug/.pioenvs/gplug"
MEMORY_MD = FW / "MEMORY.md"
PIO = Path.home() / ".platformio"
SLOT = 0x160000     # app0/app1 size in the partition table
SRAM = 321296       # DRAM total esp_idf_size reports for the C3

# Drift the check tolerates before MEMORY.md counts as stale. Small enough that a new feature or a
# grown SPA shows up, large enough that the version string CI substitutes does not.
TOL_IMAGE = 1024
TOL_RAM = 512

BASELINE_RE = re.compile(r"<!-- size-baseline (.*?) -->")


def esp_idf_size(*args):
    py = PIO / "penv/bin/python"
    out = subprocess.run([str(py if py.exists() else sys.executable), "-m", "esp_idf_size", "--format", "json2",
                          *args, str(BUILD / "firmware.map")], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def tool(name):
    """Path of a toolchain binary; the version is in the package directory name, so glob for it."""
    return str(next(PIO.glob(f"packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-{name}")))


def run(args, stdin=None):
    return subprocess.run(args, input=stdin, check=True, capture_output=True, text=True).stdout


def symbols():
    out = run([tool("nm"), "-S", "-C", str(BUILD / "firmware.elf")])
    syms = {}
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4:
            syms[parts[3]] = (parts[2], int(parts[1], 16))
    return syms


def sections(entry):
    return {s: x["size"] for mt in entry["memory_types"].values() for s, x in mt["sections"].items()}


def in_image(sec):
    return (sec.get(".flash.text", 0) + sec.get(".flash.rodata", 0) + sec.get(".flash.appdesc", 0)
            + sec.get(".iram0.text", 0) + sec.get(".dram0.data", 0))


def in_ram(sec):
    return sum(v for k, v in sec.items() if k.startswith((".iram0", ".dram0")))


WIFI = ("libnet80211.a", "libpp.a", "libwpa_supplicant.a", "libphy.a")
NET = ("liblwip.a", "libespressif__mdns.a", "libhttp_parser.a", "libesp_http_server.a", "libesp_netif.a",
       "libzorxx__multipart-parser.a")
CRYPTO = ("libmbedcrypto.a", "libmbedtls.a", "libmbedx509.a")
# The linker merges string literals from every object into one pool and credits it to the first
# contributor. In this build that is the API connection; its .rodata is the pool, not API code.
STRING_POOL_OBJ = "components/api/api_connection.cpp.o"


def embedded_sizes():
    """Byte length of each embedded file, produced the way the components' __init__.py produce them,
    so the anonymous setup()::uint8_t_id* arrays can be named by length."""
    comp = FW / "components"
    gz = lambda b: len(gzip.compress(b, compresslevel=9, mtime=0))
    presets = json.loads((comp / "gplug_smi/presets.json").read_text(encoding="utf-8"))
    return {
        len((comp / "gplug_smi/spa.html.gz").read_bytes()): "SPA",
        gz(json.dumps(presets, separators=(",", ":"), ensure_ascii=False).encode()): "presets",
        gz((comp / "captive_portal/captive.html").read_bytes()): "captive page",
        len((comp / "gplug_smi/icon.png").read_bytes()): "icon",
        gz((comp / "gplug_smi/manifest.webmanifest").read_bytes()): "manifest",
    }


def owner(archive, obj):
    name = archive.rsplit("/", 1)[-1]
    if name in WIFI:
        return "wifi"
    if name in NET:
        return "net"
    if name in CRYPTO or "/libsodium/" in obj or "/noise-c/" in obj:
        return "crypto"
    if archive != "(exe)":
        return "idf"
    if "/components/gplug_smi/" in obj:
        return "gplug"
    if obj.endswith("/src/main.cpp.o"):
        return "main"
    if "/src/esphome/" in obj:
        return "esphome"
    return "idf"


# Splitting the `gplug_smi` row. The component is two translation units -- gplug_smi.cpp, with every
# header-only decoder, store and route handler inlined into it, and mqtt.cpp -- so esp_idf_size,
# which stops at the object file, reports it as one lump. These rules split it finer: the linker map
# lists one input section per function and per literal pool with its address and owning object, and
# addr2line turns each address into the source file it was compiled from (the headers the function
# was inlined from, which is what we want to group by). Each section is matched as
# "<source file>|<demangled owner symbol>" against the patterns below, first match wins, so a
# pattern can name either a header or the methods of gplug_smi.cpp that belong to a part.
GPLUG_SECTION = re.compile(r"^\s+(0x[0-9a-f]{8,16})\s+(0x[0-9a-f]+)\s+\S*/gplug_smi/(gplug_smi|mqtt)\.cpp\.o$")
GPLUG_PARTS = [
    ("ArduinoJson, inlined where it parses the POST bodies (`/api/meter`, `/api/hw`, `/api/mqtt`, "
     "`/api/key/check`, `/api/wifi/scan`) and re-serializes the stored meter and MQTT settings",
     r"/ArduinoJson/"),
    ("HTTP API: route dispatch, the JSON bodies and the CSV export (`handleRequest`, `json_*`, `handle_*`)",
     r"::(json_[a-z_0-9]*|handle_[a-z_0-9]*|send_gz_|send_json_|read_body_|canHandle|handleRequest"
     r"|isRequestHandlerTrivial|socket_json_|socket_count_|json_escape|append_num|bytes_to_hex)\b"),
    ("MQTT: client, template compiler and renderer, status payload (`mqtt.cpp`, `mqtt_template.h`, `mqtt_status.h`)",
     r"/mqtt(_template|_status)?\.(h|cpp)|gplug_mqtt::|::mqtt_[a-z_0-9]*\("),
    ("Meter decoding: HDLC + DLMS/COSEM, DSMR/P1, AES-GCM, protocol sniffing, frame log",
     r"/(dlms_decoder|dsmr_parser|aes_gcm|protocol_sniff|frame_log)\.h"
     r"|::(on_dlms_apdu_|on_dsmr_value_|apply_dlms_value_|note_energy_exact_)\b"),
    ("History and event store: 15-min records on the `data` partition, CSV rows, event log in NVS",
     r"/(history_store|history_csv|partition_flash|event_log)\.h"
     r"|::(hist_|log_)[a-z_0-9]*\(|::wh_to_record|::csv_local_time"),
]
GPLUG_REST = ("Everything else: lifecycle, config apply and NVS, Home Assistant entities, LED and button, "
              "update check, heap sampler, and the `std::string`/`std::vector` code inlined into them")


def gplug_sections():
    """(address, size, owner symbol, is literal pool) of every image section of the two gplug_smi objects.

    The map repeats the whole input-section list once under "Discarded input sections" (COMDAT
    duplicates the linker threw away), so parsing starts at the memory map proper.
    """
    text = (BUILD / "firmware.map").read_text(errors="replace")
    text = text[text.index("Linker script and memory map"):]
    out, sec = [], None
    for line in text.splitlines():
        if line.startswith(" .") and not line.startswith("  "):
            sec = line.strip().split()[0]
        found = GPLUG_SECTION.match(line)
        if found and sec and not sec.startswith((".debug", ".note", ".riscv", ".comment")):
            # ".rodata._ZN...str1.4" -> "_ZN...": the literal pool of one function.
            name = sec.split(".", 2)[2].removesuffix(".str1.4") if sec.count(".") >= 2 else "?"
            out.append((int(found.group(1), 16), int(found.group(2), 16), name,
                        sec.endswith(".str1.4")))
    return out


def gplug_rows():
    """Rows for the `gplug_smi` sub-table, plus its total and how much of it is string literals.

    The total is larger than the "`gplug_smi` code" row of the image table: the literal pools
    (.str1.4) are part of the merged pool that esp_idf_size credits to the API connection, i.e. to
    the "String literals from all code" row. Keeping them here is what makes the parts comparable --
    a JSON route is mostly literals -- so the sub-table states both numbers.
    """
    secs = gplug_sections()
    demangled = run([tool("c++filt")], "\n".join(s[2] for s in secs)).splitlines()
    files = run([tool("addr2line"), "-e", str(BUILD / "firmware.elf")],
                "\n".join(hex(s[0]) for s in secs)).splitlines()
    sizes = {label: [0, 0] for label in [label for label, _ in GPLUG_PARTS] + [GPLUG_REST]}
    for (_, size, _, is_literal), name, where in zip(secs, demangled, files):
        key = f"{where}|{name}"
        label = next((lbl for lbl, pattern in GPLUG_PARTS if re.search(pattern, key)), GPLUG_REST)
        sizes[label][0] += size
        sizes[label][1] += size if is_literal else 0
    return sizes, sum(v[0] for v in sizes.values()), sum(v[1] for v in sizes.values())


def measure():
    files = esp_idf_size("--files")
    archives_of = esp_idf_size("--archives")
    total = esp_idf_size()
    syms = symbols()

    img = dict.fromkeys(("wifi", "idf", "crypto", "net", "strings", "esphome", "gplug", "web", "main"), 0)
    ram = dict.fromkeys(("wifi", "idf", "net", "esphome"), 0)
    # --files keys are "archive:object"; (exe) objects carry their own path.
    for key, entry in files.items():
        archive, _, obj = key.partition(":")
        sec = sections(entry)
        grp = owner(archive, obj)
        size = in_image(sec)
        if obj.endswith(STRING_POOL_OBJ):
            img["strings"] += sec.get(".flash.rodata", 0)
            size -= sec.get(".flash.rodata", 0)
        if grp == "main":
            # Generated main.cpp: its .rodata is the embedded files (setup()::uint8_t_id* arrays).
            img["web"] += sec.get(".flash.rodata", 0)
            size -= sec.get(".flash.rodata", 0)
        img[grp] += size
        data_bss = sec.get(".dram0.data", 0) + sec.get(".dram0.bss", 0)
        if grp in ("wifi", "net"):
            ram[grp] += data_bss
        elif grp in ("esphome", "gplug", "main"):
            ram["esphome"] += data_bss
        else:
            ram["idf"] += data_bss

    # Alignment padding between input sections belongs to no object; it is what the per-object sums
    # fall short of esp_idf_size's own totals (about 1.6 kB, mostly in .rodata).
    image = total["total_size"]
    img["pad"] = image - sum(img.values())
    assert 0 <= img["pad"] < 8192, img["pad"]

    layout = {mt["name"]: mt for mt in total["layout"]}
    dram = layout["DRAM"]
    smi = syms.get("gplug_smi__gplug_smi_gplugsmi_id__pstorage", ("b", 0))[1]
    stack = syms.get("esphome::loop_task_stack", ("b", 0))[1]
    ram["esphome"] -= smi + stack
    # ESP-IDF globals take the RAM remainder (padding, .noinit), so the table adds up to the total.
    ram["idf"] = dram["used"] - dram["parts"][".text"]["size"] - smi - stack - ram["wifi"] - ram["net"] - ram["esphome"]
    web =sorted(((s, n) for n, (t, s) in syms.items() if n.startswith("setup()::uint8_t_id")), reverse=True)
    return {
        "image": image, "img": img, "text": layout["Flash Code"]["used"],
        "rodata": layout["Flash Data"]["used"], "iram": dram["parts"][".text"]["size"],
        "data": dram["parts"][".data"]["size"], "dram": dram["used"],
        "smi": smi, "stack": stack, "ram": ram, "web": web,
    }


kb = lambda b: f"{b / 1024:.1f} kB"
pct = lambda b, of: f"{100 * b / of:.1f} %"


def report(m):
    i, r = m["img"], m["ram"]
    names = embedded_sizes()
    print(f"<!-- size-baseline image={m['image']} dram={m['dram']} gplug_smi_obj={m['smi']} -->\n")
    print(f"Image {m['image']:,} B = {m['image'] / 1024:.0f} kB in a {SLOT // 1024} kB slot: "
          f"**{pct(m['image'], SLOT)} full, {(SLOT - m['image']) / 1024:.0f} kB headroom**\n")
    print(f"By section: {m['text'] / 1024:.0f} kB code run from flash, {m['rodata'] / 1024:.0f} kB read-only data, "
          f"{m['iram'] / 1024:.0f} kB IRAM code and {m['data'] / 1024:.0f} kB `.data` initial values\n")
    rows = [
        ("Wi-Fi driver, WPA supplicant, PHY (`libnet80211`, `libpp`, `libwpa_supplicant`, `libphy`)", i["wifi"]),
        ("ESP-IDF system (FreeRTOS, libc/printf, HAL, flash + NVS drivers, heap, UART, OTA, HTTP client + esp-tls, MQTT client)", i["idf"]),
        ("Crypto (mbedTLS: AES-GCM, TLS + X.509 + CA bundle for the update check; Noise/Ed25519 for the encrypted API)", i["crypto"]),
        ("Networking (lwIP, ESP-IDF HTTP server + parser, mDNS)", i["net"]),
        ("String literals from all code", i["strings"]),
        ("ESPHome core and components (incl. the captive_portal fork)", i["esphome"]),
        ("`gplug_smi` code", i["gplug"]),
        ("Embedded web files, gzipped except the PNG: " + ", ".join(
            f"{names.get(s, '`' + n + '`')} {kb(s)}" for s, n in m["web"]), i["web"]),
        ("ESPHome-generated `main.cpp` setup code", i["main"]),
        ("Linker alignment padding (no owning object)", i["pad"]),
    ]
    print("| Part | Size | Share of image |\n|---|---|---|")
    for label, size in rows:
        print(f"| {label} | {kb(size)} | {pct(size, m['image'])} |")
    print()
    parts, gplug_total, literals = gplug_rows()
    print(f"Inside `gplug_smi`: {gplug_total:,} B = {kb(gplug_total)}, of which {kb(literals)} string "
          f"literals that the image table counts in its \"String literals\" row, not in "
          f"\"`gplug_smi` code\" ({kb(i['gplug'])}).\n")
    print("| Part of `gplug_smi` | Size | Of it literals | Share |\n|---|---|---|---|")
    for label, (size, lit) in sorted(parts.items(), key=lambda kv: -kv[1][0]):
        print(f"| {label} | {kb(size)} | {kb(lit)} | {pct(size, gplug_total)} |")
    print()
    rrows = [
        ("IRAM code (interrupts, flash driver, scheduler, Wi-Fi)", m["iram"]),
        ("`GplugSmi` object (`gplug_smi__gplug_smi_gplugsmi_id__pstorage`)", m["smi"]),
        ("Wi-Fi globals (connection manager, power management, WPA state)", r["wifi"]),
        ("ESP-IDF globals (scheduler lists, ISR stack, stdio, driver state)", r["idf"]),
        ("ESPHome loop task stack (`esphome::loop_task_stack`)", m["stack"]),
        ("lwIP and mDNS (DNS table, mDNS task stack)", r["net"]),
        ("Other component objects (logger, remaining ESPHome components)", r["esphome"]),
    ]
    print("| Part | Size | Share of SRAM |\n|---|---|---|")
    for label, size in rrows:
        print(f"| {label} | {kb(size)} | {pct(size, SRAM)} |")
    print(f"| **Static total** | **{kb(m['dram'])}** | **{pct(m['dram'], SRAM)}** |")
    print(f"| **Left for the heap at boot** | **{kb(SRAM - m['dram'])}** | **{pct(SRAM - m['dram'], SRAM)}** |")


def check(m):
    found = BASELINE_RE.search(MEMORY_MD.read_text())
    if not found:
        print("MEMORY.md has no size-baseline line; run tools/size_report.py and paste its output", file=sys.stderr)
        return 1
    base = dict(kv.split("=") for kv in found.group(1).split())
    now = {"image": m["image"], "dram": m["dram"], "gplug_smi_obj": m["smi"]}
    tol = {"image": TOL_IMAGE, "dram": TOL_RAM, "gplug_smi_obj": TOL_RAM}
    drift = [f"{k}: MEMORY.md {int(base[k]):,} B, build {now[k]:,} B ({now[k] - int(base[k]):+,} B)"
             for k in now if abs(now[k] - int(base.get(k, 0))) > tol[k]]
    if not drift:
        print(f"MEMORY.md matches the build (image {now['image']:,} B, static RAM {now['dram']:,} B)")
        return 0
    print("firmware/MEMORY.md is out of date with the last build:\n  " + "\n  ".join(drift), file=sys.stderr)
    print("Run firmware/tools/size_report.py, update the tables, the snapshot date and the baseline line.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    if not (BUILD / "firmware.map").exists():
        print(f"no build at {BUILD}; run `esphome compile dev.yaml` first", file=sys.stderr)
        sys.exit(2)
    m = measure()
    sys.exit(check(m) if "--check" in sys.argv else report(m) or 0)
