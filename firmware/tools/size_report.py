#!/usr/bin/env python3
"""Flash and RAM breakdown of the last `esphome compile dev.yaml`, for firmware/MEMORY.md.

    tools/size_report.py            print the tables MEMORY.md is made of, plus its baseline line
    tools/size_report.py --check    compare the build against the baseline in MEMORY.md, exit 1 on drift

MEMORY.md used to be assembled by hand from esp_idf_size output, which is why it went stale: the
numbers were a chore to redo, so they were redone only now and then. This script produces the same
groupings from the linker map and the ELF symbols, so updating the file is a paste, and the check
can tell when that paste is due.

Groups are by archive (ESP-IDF libraries) and by object file (ESPHome, gplug_smi, libsodium/noise).
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


def symbols():
    nm = next(PIO.glob("packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm"))
    out = subprocess.run([str(nm), "-S", "-C", str(BUILD / "firmware.elf")], check=True, capture_output=True, text=True).stdout
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
