#!/usr/bin/env python3
"""Package Mini firmware with its notices; never writes to a card or device."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate_mini_hex(data):
    memory = (ROOT / "platforms/imx/linker/TrackerMini/memory.ld").read_text()
    region = re.search(r"QSPI_FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*(0x[\da-fA-F]+),\s*LENGTH\s*=\s*(0x[\da-fA-F]+)", memory)
    if not region:
        raise ValueError("Cannot read Mini linker flash region")
    origin, length = (int(x, 16) for x in region.groups())
    upper, minimum, maximum, size, eof = 0, 2**32, 0, 0, False
    for number, line in enumerate(data.decode("ascii").splitlines(), 1):
        if eof or not line.startswith(":"):
            raise ValueError(f"Invalid Intel HEX record at line {number}")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) % 256:
            raise ValueError(f"Invalid Intel HEX length/checksum at line {number}")
        count, kind = record[0], record[3]
        address = int.from_bytes(record[1:3], "big")
        if kind == 4:
            if count != 2:
                raise ValueError("Invalid extended address record")
            upper = int.from_bytes(record[4:6], "big") << 16
        elif kind == 0:
            start, end = upper + address, upper + address + count
            if not origin <= start < end <= origin + length:
                raise ValueError("Firmware data is outside the Mini application region")
            minimum, maximum = min(minimum, start), max(maximum, end)
            size += count
        elif kind == 1:
            if count != 0:
                raise ValueError("Invalid EOF record")
            eof = True
        elif kind != 5:
            raise ValueError(f"Unsupported Intel HEX record type {kind}")
    if not eof or minimum != origin:
        raise ValueError("Incomplete image or incorrect Mini application origin")
    return {"origin": hex(minimum), "end_exclusive": hex(maximum), "payload_bytes": size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    parser.add_argument("--build-tag", default="808p03")
    args = parser.parse_args()
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("VERSION must contain a semantic version")
    data = args.firmware.read_bytes()
    flash = validate_mini_hex(data)
    args.out.mkdir(parents=True, exist_ok=True)
    firmware_name = f"TrackerMini_KIT_{version}.ptf"
    if len(firmware_name) > 31:
        raise ValueError("Firmware filename exceeds the upstream bootloader limit")
    firmware = args.out / firmware_name
    if firmware.resolve() != args.firmware.resolve():
        shutil.copyfile(args.firmware, firmware)
    published = ROOT / "firmware/mini" / firmware_name
    matches_tested = published.is_file() and digest(published.read_bytes()) == digest(data)
    manifest = {
        "project": "Lumen KIT", "version": version, "status": "proof of concept",
        "authors": ["KRUChY", "Lumen (AI engineering collaborator)"],
        "target": "Polyend Tracker Mini", "ce_base_version": "1.0.0",
        "build_tag": args.build_tag, "firmware": firmware_name, "sha256": digest(data),
        "intel_hex": flash, "matches_user_confirmed_808p03_image": matches_tested,
        "upstream_commit": "9b9a13b56fef94710e5c1d4bf1a321681080a6c8",
    }
    manifest_data = (json.dumps(manifest, indent=2) + "\n").encode()
    (args.out / "manifest.json").write_bytes(manifest_data)
    instructions = f"""# Lumen KIT {version} — proof of concept

Authors: KRUChY & Lumen (AI engineering collaborator).
Based on Polyend Tracker Community Edition. See the included licenses/notices.

MINI ONLY. Back up your card and keep a known-good Mini firmware.
Copy Firmware/{firmware_name} to /Firmware/ on the microSD card.
On Mini: Config > Firmware > Firmware update. Select that file.
Then use a new project: SYN 1 > KIT > Edit Patch > Init, with 1 voice.

C4 BD / D4 Snare / E4 Closed Hat / F4 Clap / G4 Open Hat / A4 Cowbell.
The mapping repeats in every octave. a-f = Pitch, Decay, Tone, Drive, Level, Pan.
63 is neutral. Audio files are desktop renders, not hardware recordings.

KIT version: {version}; CE base: 1.0.0; Build: {args.build_tag}.
Version 0.1 is an exploration of possibilities, not a finished product.
This is not an official Polyend release or a circuit-accurate Roland emulation.

Repository: https://github.com/mkruszyniak-omarchy/lumen-kit-tracker
"""
    entries = {
        f"Firmware/{firmware_name}": data,
        "README.md": instructions.encode(),
        "AUTHORS.md": (ROOT / "AUTHORS.md").read_bytes(),
        "LICENSE": (ROOT / "LICENSE").read_bytes(),
        "NOTICE.md": (ROOT / "NOTICE.md").read_bytes(),
        "LICENSE-POLYSYNTH.txt": (ROOT / "synth/lib/LICENSE-PROPRIETARY.txt").read_bytes(),
        "manifest.json": manifest_data,
    }
    for audio in sorted((ROOT / "examples/audio").glob("*.wav")):
        entries[f"Audio/{audio.name}"] = audio.read_bytes()
    entries["SHA256SUMS"] = ("\n".join(f"{digest(body)}  {name}" for name, body in sorted(entries.items())) + "\n").encode()
    archive = args.out / f"Lumen_KIT_{version}_Mini.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for name, body in sorted(entries.items()):
            output.writestr(name, body)
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None:
            raise ValueError("ZIP integrity check failed")
    assets = (firmware, archive, args.out / "manifest.json")
    (args.out / "SHA256SUMS").write_text("\n".join(f"{digest(path.read_bytes())}  {path.name}" for path in assets) + "\n")
    print(f"Packaged {firmware_name}; SHA256 {digest(data)}")
    print(f"Matches user-confirmed image: {matches_tested}")


if __name__ == "__main__":
    main()
