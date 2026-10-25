#!/usr/bin/env python3
"""Inspect a G4DEL factory ROM and optionally extract non-BIOS regions.

No factory bytes are written unless --extract is supplied. Never commit the
extracted files: the descriptor, CSE, GbE and device-expansion data are tied
to the board and may contain private configuration.
"""

import argparse
import hashlib
import struct
from pathlib import Path

REGIONS = {
    "descriptor": (0x0000000, 0x0001000),
    "gbe": (0x0001000, 0x0003000),
    "me": (0x0003000, 0x2FF0000),
    "device_exp1": (0x2FF0000, 0x3000000),
    "bios": (0x3000000, 0x4000000),
}
EXPECTED_SIZE = 0x4000000
EXPECTED_FMAP_AREAS = {
    "SI_DESC": (0x00000000, 0x00001000),
    "SI_GBE": (0x00001000, 0x00002000),
    "SI_ME": (0x00003000, 0x02FED000),
    "SI_DEVICEEXT": (0x02FF0000, 0x00010000),
    "SI_BIOS": (0x03000000, 0x01000000),
}


def parse_fmap(rom):
    """Return FMAP areas keyed by name, rejecting malformed candidates."""
    areas = []
    cursor = 0
    while True:
        fmap_offset = rom.find(b"__FMAP__", cursor)
        if fmap_offset < 0:
            break
        cursor = fmap_offset + 1
        try:
            _major, _minor, _base, image_size, _name, area_count = struct.unpack_from(
                "<BBQI32sH", rom, fmap_offset + 8
            )
            if image_size != len(rom) or area_count > 512:
                continue
            area_offset = fmap_offset + 56
            candidate = {}
            for _ in range(area_count):
                offset, size, raw_name, _flags = struct.unpack_from(
                    "<II32sH", rom, area_offset
                )
                area_offset += 42
                name = raw_name.split(b"\0", 1)[0].decode("ascii")
                if not name or offset > len(rom) or size > len(rom) - offset:
                    raise ValueError("invalid FMAP area")
                candidate[name] = (offset, size)
            if all(name in candidate for name in EXPECTED_FMAP_AREAS):
                areas.append(candidate)
        except (struct.error, UnicodeDecodeError, ValueError):
            continue
    if len(areas) != 1:
        raise ValueError(f"expected one complete FMAP, found {len(areas)}")
    return areas[0]


def verify_layout(rom):
    """Check that FMAP accounts for the IFD regions and BIOS subregions."""
    areas = parse_fmap(rom)
    for name, expected in EXPECTED_FMAP_AREAS.items():
        actual = areas[name]
        if actual != expected:
            raise ValueError(
                f"FMAP {name} is {actual[0]:#x}+{actual[1]:#x}, "
                f"expected {expected[0]:#x}+{expected[1]:#x}"
            )

    required = [areas[name] for name in EXPECTED_FMAP_AREAS]
    ordered = sorted(required)
    cursor = 0
    for offset, size in ordered:
        if offset != cursor:
            raise ValueError(f"IFD/FMAP gap or overlap at {cursor:#x}, next region {offset:#x}")
        cursor += size
    if cursor != EXPECTED_SIZE:
        raise ValueError(f"IFD/FMAP regions end at {cursor:#x}, expected {EXPECTED_SIZE:#x}")

    bios_offset, bios_size = areas["SI_BIOS"]
    for name, expected_size in (("SMMSTORE", 0x40000), ("BOOTSPLASH", 0x100000)):
        if name not in areas:
            raise ValueError(f"FMAP is missing {name}")
        offset, size = areas[name]
        if size != expected_size or offset < bios_offset or size > bios_offset + bios_size - offset:
            raise ValueError(f"FMAP {name} has invalid BIOS range {offset:#x}+{size:#x}")
    if areas["SMMSTORE"][0] % 0x10000 or areas["SMMSTORE"][1] % 0x10000:
        raise ValueError("SMMSTORE is not aligned to the 64 KiB logical block size")

    fmap_offset, fmap_size = areas.get("FMAP", (None, None))
    coreboot_offset, coreboot_size = areas.get("COREBOOT", (None, None))
    if fmap_offset is None or coreboot_offset is None:
        raise ValueError("FMAP or COREBOOT BIOS subregion is missing")
    for name, (offset, size) in (("FMAP", (fmap_offset, fmap_size)),
                                 ("COREBOOT", (coreboot_offset, coreboot_size))):
        if offset < bios_offset or size == 0 or size > bios_offset + bios_size - offset:
            raise ValueError(f"FMAP {name} has invalid BIOS range {offset:#x}+{size:#x}")

    siblings = sorted(
        ((name, *areas[name]) for name in ("SMMSTORE", "BOOTSPLASH", "FMAP", "COREBOOT")),
        key=lambda item: item[1],
    )
    cursor = bios_offset
    for name, offset, size in siblings:
        if offset < cursor:
            raise ValueError(f"BIOS subregion {name} overlaps the previous area")
        if offset > cursor:
            raise ValueError(f"BIOS subregion gap before {name} at {cursor:#x}")
        cursor = offset + size
    if cursor != bios_offset + bios_size:
        raise ValueError(
            f"BIOS areas end at {cursor:#x}, expected {bios_offset + bios_size:#x}"
        )
    print("FMAP layout covers the IFD regions and contains aligned, non-overlapping BIOS areas")
    for name in (*EXPECTED_FMAP_AREAS, "SMMSTORE", "BOOTSPLASH", "FMAP", "COREBOOT"):
        offset, size = areas[name]
        print(f"FMAP {name:12} {offset:#09x}..{offset + size - 1:#09x} size={size:#x}")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--extract", type=Path, metavar="DIR",
                        help="write descriptor, gbe, me and device_exp1 to DIR")
    parser.add_argument("--compare", type=Path, metavar="ROM",
                        help="verify that ROM preserves all four non-BIOS regions")
    parser.add_argument("--verify-layout", action="store_true",
                        help="verify the 64 MiB IFD/FMAP and BIOS subregion map")
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
    if args.verify_layout:
        try:
            verify_layout(rom)
        except ValueError as error:
            parser.error(str(error))


if __name__ == "__main__":
    main()
