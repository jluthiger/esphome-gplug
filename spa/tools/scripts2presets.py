#!/usr/bin/env python3
"""Convert the Tasmota gPlug scripts (gplug/<variant>/<provider>/script.txt)
into presets.json consumed by the SPA setup wizard (and later by the firmware).

Usage: python3 spa/tools/scripts2presets.py [gplug-dir] [out.json]
"""
import json, re, sys
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "gplug")
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else Path(__file__).resolve().parents[2] / "firmware" / "components" / "gplug_smi" / "presets.json")

# Hardware defaults per variant (from intent.md pin table)
# baud / parity ("N" or "E") / serial_flags (Tasmota so2: 4 = invert RX, 8 = no pullup) describe the
# variant's physical interface and are the same for every preset of a variant. The hardware step
# sends them with the pins, so the firmware's UART already runs right before a profile is chosen and
# the protocol sniffer can listen (GplugSmi::apply_hw_json_). Not the Tasmota `mode`: that also
# encodes the protocol ("o" ASCII vs "r" raw), which differs between a variant's presets.
VARIANTS = [
    {"id": "gplugd",  "name": "gPlugD",   "interface": "P1 (DSMR ASCII or HDLC/DLMS)",
     "pins": {"rx": 4, "red": 6, "green": 5, "blue": 7, "button": 9}, "baud": 115200, "parity": "N", "serial_flags": 0},
    {"id": "gplugde", "name": "gPlugD-E", "interface": "P1 (DSMR ASCII or HDLC/DLMS)",
     "pins": {"rx": 4, "red": 5, "green": 6, "blue": 7, "button": 9}, "baud": 115200, "parity": "N", "serial_flags": 0},
    {"id": "gplugk",  "name": "gPlugK",   "interface": "Kamstrup DLMS push",
     "pins": {"rx": 4, "red": 5, "green": 6, "blue": 7, "button": 9}, "baud": 2400, "parity": "N", "serial_flags": 0},
    {"id": "gplugm",  "name": "gPlugM",   "interface": "CII / M-Bus HDLC/DLMS",
     "pins": {"rx": 7, "red": 1, "green": 4, "blue": 3, "button": 9}, "baud": 2400, "parity": "E", "serial_flags": 12},
]

MODE = {"o": "dsmr", "r": "dlms", "rE1": "dlms"}

# Human names shown in the SPA. Script comments are inconsistent, so name explicitly.
# Protocol and product names only, no prose: the SPA runs in four languages and appends the
# encryption state itself, from the preset's `encrypted` flag (see presetLabel() in the SPA).
NAMES = {
    "gplugd/p1-dsmr": "P1 DSMR",
    "gplugd/p1-hdlc_dlms": "P1 HDLC/DLMS",
    "gplugde/p1-dsmr": "P1 DSMR",
    "gplugde/p1-hdlc_dlms": "P1 HDLC/DLMS Romande Energie",
    "gplugk/dlms-push-1": "Kamstrup DLMS Push",
    "gplugm/romande-energie": "CII HDLC/DLMS Romande Energie",
    "gplugm/universal": "CII HDLC/DLMS universal (L+G E450)",
}

HDR = re.compile(r"^\+1,(\d+),([A-Za-z0-9]+),(\d+),(\d+),(\w+)")
# 1,<match>@<scale>,<label>,<unit>,<name>,<prec>
LINE = re.compile(r"^1,(?P<match>[^,]*?)@(?P<scale>[^,]*),(?P<label>[^,]*),(?P<unit>[^,]*),(?P<name>[^,]*),(?P<prec>\d+)")
OPT = re.compile(r"^1,=so(\d),(.+)$")
TITLE = re.compile(r"^;\s*(gPlug\S*.*?)\s*(\(\d+\.\d+\.\d+\))?\s*$")


def obis_of(match: str) -> str:
    m = re.match(r"pm\(([\d.]+)\)", match)
    if m:
        return m.group(1)
    m = re.match(r"([\d-]+:[\d.]+)\(", match)
    if m:
        return m.group(1)
    return match.strip("()")


def parse(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if not any(l.startswith(">M") for l in text):
        return None  # doc or placeholder, not a script
    p = {"title": None, "uart": None, "options": {}, "obis": []}
    in_m = False
    for l in text:
        l = l.rstrip()
        if p["title"] is None:
            t = TITLE.match(l)
            if t:
                p["title"] = t.group(1).strip()
        if l.startswith(">M"):
            in_m = True
            continue
        if l.startswith(">"):
            in_m = False
        if not in_m:
            continue
        h = HDR.match(l)
        if h:
            rx, mode, _, baud, _ = h.groups()
            p["uart"] = {"rx": int(rx), "mode": mode, "protocol": MODE.get(mode, mode), "baud": int(baud)}
            continue
        o = OPT.match(l)
        if o:
            n, v = o.groups()
            key = {"2": "serial_flags", "3": "buffer", "4": "key"}.get(n, "so" + n)
            p["options"][key] = v
            continue
        m = LINE.match(l)
        if m:
            scale = m["scale"]
            entry = {
                "obis": obis_of(m["match"]),
                "type": "string" if scale.startswith("#") else "number",
                "scale": None if scale.startswith("#") else float(scale.rstrip(")")),
                "label": m["label"].strip() or None,
                "unit": m["unit"].strip() or None,
                "name": m["name"].strip(),
                "precision": int(m["prec"]),
            }
            p["obis"].append(entry)
    return p


SUPPORTED = {v["id"] for v in VARIANTS}  # products on https://gplug.ch/produkte/

presets = []
for script in sorted(ROOT.glob("*/*/script.txt")):
    variant, provider = script.parts[-3], script.parts[-2]
    if variant not in SUPPORTED:
        continue
    parsed = parse(script)
    if not parsed:
        continue
    opts = parsed["options"]
    presets.append({
        "id": f"{variant}/{provider}",
        "variant": variant,
        "name": NAMES.get(f"{variant}/{provider}", parsed["title"] or f"{variant} {provider}"),
        "protocol": parsed["uart"]["protocol"],
        "mode": parsed["uart"]["mode"],
        "baud": parsed["uart"]["baud"],
        "rx": parsed["uart"]["rx"],
        "encrypted": opts.get("key") == "%dKEY%",
        "serial_flags": int(opts["serial_flags"], 16) if "serial_flags" in opts else 0,  # Tasmota so2: 4=invert RX, 8=no pullup
        "buffer": int(opts["buffer"]) if "buffer" in opts else None,
        "obis": parsed["obis"],
        "source": str(script.relative_to(ROOT.parent)),
    })

# The hardware step configures the line from the variant alone; a preset that disagreed would be
# silently overridden by the wrong parameters until the meter step. Refuse to generate that.
by_id = {v["id"]: v for v in VARIANTS}
for p in presets:
    v = by_id[p["variant"]]
    for k in ("baud", "serial_flags"):
        assert p[k] == v[k], f"{p['id']}: {k}={p[k]!r} differs from variant {v['id']} ({v[k]!r})"
    assert (p["mode"] == "rE1") == (v["parity"] == "E"), f"{p['id']}: mode={p['mode']!r} vs variant parity {v['parity']!r}"

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps({"variants": VARIANTS, "presets": presets}, indent=1, ensure_ascii=False))
print(f"{len(presets)} presets -> {OUT}")
for p in presets:
    print(f"  {p['id']:28} {p['protocol']:5} {p['baud']:>6} rx={p['rx']} enc={p['encrypted']!s:5} obis={len(p['obis'])}")
