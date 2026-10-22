#!/usr/bin/env python3
"""Inspect a G4DEL factory ROM and optionally extract non-BIOS regions.

No factory bytes are written unless --extract is supplied. Never commit the
extracted files: the descriptor, CSE, GbE and device-expansion data are tied
to the board and may contain private configuration.
"""

import argparse
import hashlib
from pathlib import Path

REGIONS = {
    "descriptor": (0x0000000, 0x0001000),
    "gbe": (0x0001000, 0x0003000),
    "me": (0x0003000, 0x2FF0000),
    "device_exp1": (0x2FF0000, 0x3000000),
    "bios": (0x3000000, 0x4000000),
}
EXPECTED_SIZE = 0x4000000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--extract", type=Path, metavar="DIR",
                        help="write descriptor, gbe, me and device_exp1 to DIR")
    parser.add_argument("--compare", type=Path, metavar="ROM",
                        help="verify that ROM preserves all four non-BIOS regions")
    args = parser.parse_args()
    rom = args.image.read_bytes()
    if len(rom) != EXPECTED_SIZE:
        parser.error(f"expected 64 MiB, got {len(rom)} bytes")
    if rom[0x10:0x14] != bytes.fromhex("5aa5f00f"):
        parser.error("missing Intel flash descriptor signature")
    print(f"ROM {args.image}: {len(rom):#x} bytes sha256={sha(rom)}")
    for name, (start, end) in REGIONS.items():
        data = rom[start:end]
        print(f"{name:12} {start:#09x}..{end - 1:#09x} "
              f"{len(data):#x} sha256={sha(data)}")
        if args.extract is not None and name != "bios":
            args.extract.mkdir(parents=True, exist_ok=True)
            (args.extract / f"{name}.bin").write_bytes(data)
    if args.extract is not None:
        print(f"Extracted non-BIOS regions to {args.extract}")
    if args.compare is not None:
        built = args.compare.read_bytes()
        if len(built) != EXPECTED_SIZE:
            parser.error(f"comparison ROM must be 64 MiB, got {len(built)} bytes")
        mismatches = [name for name, (start, end) in REGIONS.items()
                      if name != "bios" and built[start:end] != rom[start:end]]
        if mismatches:
            parser.error("factory regions differ: " + ", ".join(mismatches))
        print("Factory descriptor, GbE, ME and Device Exp1 match byte for byte")


if __name__ == "__main__":
    main()
