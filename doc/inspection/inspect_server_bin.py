#!/usr/bin/env python3
"""Read-only SPI image inspector for the Gooxi server.bin capture.

The source image is never modified. All generated files are placed in a new,
empty output directory supplied with --out-dir.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import struct
import subprocess
import sys
import uuid
from collections import Counter
from pathlib import Path


EXPECTED_SIZE = 64 * 1024 * 1024
EXPECTED_SHA256 = "db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465"
REGION_NAMES = {
    0: "flash_descriptor",
    1: "bios",
    2: "intel_me",
    3: "gbe",
    4: "platform_data",
    5: "device_exp1",
    6: "secondary_bios",
    7: "reserved7",
    8: "ec",
    9: "device_exp2",
    10: "ie",
    11: "10gbe_0",
    12: "10gbe_1",
    13: "reserved13",
    14: "reserved14",
    15: "ptt",
}
SIGNATURES = {
    b"$FPT": "Intel ME Flash Partition Table candidate",
    b"$CPD": "Intel ME Code Partition Directory candidate",
    b"$MN2": "Intel ME manifest candidate",
    b"_FVH": "UEFI PI Firmware Volume header signature (at FV+0x28)",
    b"MZ": "DOS/PE executable header candidate",
    b"PK\x03\x04": "ZIP archive candidate",
    b"\x1f\x8b\x08": "gzip stream candidate",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def descriptor_regions(image: Path) -> tuple[list[dict], int]:
    with image.open("rb") as stream:
        descriptor = stream.read(4096)
    if len(descriptor) != 4096:
        raise ValueError("Input is shorter than the 4 KiB Intel Flash Descriptor")

    # IFD signature is at 0x10 and FLMAP0 at 0x14; FRBA is in 16-byte units.
    if descriptor[0x10:0x14] != b"\x5a\xa5\xf0\x0f":
        raise ValueError("Intel Flash Descriptor signature is absent at offset 0x10")
    flmap0 = struct.unpack_from("<I", descriptor, 0x14)[0]
    frba = ((flmap0 >> 16) & 0xFF) * 16
    if frba + 5 * 4 > len(descriptor):
        raise ValueError(f"FLREG table at 0x{frba:x} lies outside descriptor")

    regions = []
    for number in range(16):
        register_offset = frba + number * 4
        if register_offset + 4 > len(descriptor):
            break
        register = struct.unpack_from("<I", descriptor, register_offset)[0]
        base_units = register & 0x7FFF
        limit_units = (register >> 16) & 0x7FFF
        base = base_units << 12
        limit = (limit_units << 12) | 0xFFF
        enabled = base_units <= limit_units
        regions.append(
            {
                "number": number,
                "name": REGION_NAMES.get(number, f"region_{number}"),
                "register_offset": register_offset,
                "flreg": f"0x{register:08x}",
                "base": base if enabled else None,
                "limit_inclusive": limit if enabled else None,
                "size": limit - base + 1 if enabled else 0,
                "enabled": enabled,
            }
        )
    return regions, frba


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    length = len(data)
    return -sum((n / length) * math.log2(n / length) for n in counts.values())


def inspect_region(path: Path, region: dict, outdir: Path) -> list[dict]:
    findings = []
    overlap = max((len(sig) for sig in SIGNATURES), default=1) - 1
    tail = b""
    absolute_start = region["base"]
    with path.open("rb") as stream:
        relative = 0
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            data = tail + block
            data_start = relative - len(tail)
            for signature, description in SIGNATURES.items():
                at = 0
                while True:
                    found = data.find(signature, at)
                    if found < 0:
                        break
                    absolute = absolute_start + data_start + found
                    # _FVH is at the fixed offset 0x28 in a standard FV header.
                    if signature == b"_FVH":
                        candidate = data_start + found - 0x28
                        details = f"candidate_FV_base=0x{absolute_start + candidate:x}"
                        if candidate >= 0:
                            with path.open("rb") as fv_stream:
                                fv_stream.seek(candidate)
                                header = fv_stream.read(0x38)
                            if len(header) == 0x38 and header[0x28:0x2C] == b"_FVH":
                                fv_length = struct.unpack_from("<Q", header, 0x20)[0]
                                header_length = struct.unpack_from("<H", header, 0x30)[0]
                                revision = header[0x37]
                                checksum_ok = False
                                if 0x38 <= header_length <= path.stat().st_size - candidate and header_length % 2 == 0:
                                    with path.open("rb") as fv_stream:
                                        fv_stream.seek(candidate)
                                        full_header = fv_stream.read(header_length)
                                    words = struct.unpack("<" + "H" * (header_length // 2), full_header)
                                    checksum_ok = (sum(words) & 0xFFFF) == 0
                                valid = (
                                    header_length >= 0x38
                                    and fv_length >= header_length
                                    and candidate + fv_length <= path.stat().st_size
                                    and checksum_ok
                                )
                                details += (
                                    f"; header_valid={valid}; length=0x{fv_length:x};"
                                    f" header_length=0x{header_length:x}; revision={revision};"
                                    f" checksum_ok={checksum_ok}"
                                )
                    else:
                        details = "signature offset"
                    findings.append(
                        {
                            "absolute_offset": f"0x{absolute:x}",
                            "region_relative_offset": f"0x{absolute - absolute_start:x}",
                            "signature_hex": signature.hex(" "),
                            "description": description,
                            "details": details,
                        }
                    )
                    if signature == b"_FVH" and candidate >= 0 and len(header) == 0x38:
                        findings[-1]["fv_filesystem_guid"] = str(uuid.UUID(bytes_le=header[0x10:0x20]))
                        findings[-1]["fv_length"] = fv_length
                        findings[-1]["fv_header_valid"] = valid
                    at = found + 1
            tail = data[-overlap:] if overlap else b""
            relative += len(block)

    report = outdir / f"{region['name']}_signatures.json"
    report.write_text(json.dumps(findings, indent=2) + "\n")
    return findings


def run_capture(command: list[str], outfile: Path, cwd: Path | None = None) -> int:
    with outfile.open("w", encoding="utf-8") as output:
        output.write("$ " + " ".join(command) + "\n\n")
        result = subprocess.run(
            command,
            cwd=cwd,
            stdout=output,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "image",
        nargs="?",
        type=Path,
        default=Path("/home/uefi/workspace/gooxie_bios/server.bin"),
        help="raw SPI dump (default: canonical server.bin)",
    )
    parser.add_argument("--out-dir", required=True, type=Path, help="new, empty output directory")
    parser.add_argument(
        "--allow-hash-mismatch",
        action="store_true",
        help="inspect a different capture after still reporting its identity",
    )
    args = parser.parse_args()

    image = args.image.resolve()
    outdir = args.out_dir.resolve()
    if not image.is_file():
        parser.error(f"image does not exist: {image}")
    if outdir == image.parent or image in outdir.parents:
        parser.error("output directory must not be the image directory or a child of it")
    if outdir.exists() and any(outdir.iterdir()):
        parser.error(f"output directory must be empty: {outdir}")
    outdir.mkdir(parents=True, exist_ok=True)

    image_size = image.stat().st_size
    image_hash = sha256(image)
    identity = {
        "image": str(image),
        "size_bytes": image_size,
        "size_mib": image_size / (1024 * 1024),
        "sha256": image_hash,
        "expected_size_bytes": EXPECTED_SIZE,
        "expected_sha256": EXPECTED_SHA256,
    }
    (outdir / "image_identity.json").write_text(json.dumps(identity, indent=2) + "\n")
    if image_size != EXPECTED_SIZE:
        print(f"WARNING: expected {EXPECTED_SIZE} bytes, found {image_size}", file=sys.stderr)
    if image_hash != EXPECTED_SHA256:
        message = f"WARNING: SHA-256 differs from canonical capture: {image_hash}"
        if not args.allow_hash_mismatch:
            parser.error(message + " (use --allow-hash-mismatch to continue)")
        print(message, file=sys.stderr)

    regions, frba = descriptor_regions(image)
    (outdir / "regions.json").write_text(
        json.dumps({"frba": frba, "regions": regions}, indent=2) + "\n"
    )

    # Independently extract enabled IFD regions using descriptor-computed ranges.
    regions_dir = outdir / "regions"
    regions_dir.mkdir()
    for region in regions:
        if not region["enabled"]:
            continue
        target = regions_dir / f"region_{region['number']}_{region['name']}.bin"
        with image.open("rb") as source, target.open("wb") as output:
            source.seek(region["base"])
            remaining = region["size"]
            while remaining:
                block = source.read(min(1024 * 1024, remaining))
                if not block:
                    raise ValueError(f"image ends before region {region['number']} is complete")
                output.write(block)
                remaining -= len(block)
        region["file"] = str(target)
        region["sha256"] = sha256(target)
        if target.stat().st_size != region["size"]:
            raise ValueError(f"region size mismatch for {target}")

    (outdir / "regions.json").write_text(
        json.dumps({"frba": frba, "regions": regions}, indent=2) + "\n"
    )

    # List gaps/overlaps between enabled regions, including unclaimed flash bytes.
    enabled = sorted((r for r in regions if r["enabled"]), key=lambda r: r["base"])
    coverage = []
    cursor = 0
    for region in enabled:
        if region["base"] > cursor:
            coverage.append(
                {"kind": "unmapped", "start": f"0x{cursor:x}", "end_inclusive": f"0x{region['base'] - 1:x}", "size": region["base"] - cursor}
            )
        elif region["base"] < cursor:
            coverage.append(
                {"kind": "overlap", "start": f"0x{region['base']:x}", "end_inclusive": f"0x{cursor - 1:x}", "size": cursor - region["base"]}
            )
        coverage.append(
            {"kind": "region", "number": region["number"], "name": region["name"], "start": f"0x{region['base']:x}", "end_inclusive": f"0x{region['limit_inclusive']:x}", "size": region["size"]}
        )
        cursor = max(cursor, region["limit_inclusive"] + 1)
    if cursor < image_size:
        coverage.append(
            {"kind": "unmapped", "start": f"0x{cursor:x}", "end_inclusive": f"0x{image_size - 1:x}", "size": image_size - cursor}
        )
    (outdir / "coverage.json").write_text(json.dumps(coverage, indent=2) + "\n")

    # Decoder output, with explicit platform selection to avoid the generic fallback.
    script_path = Path(__file__).resolve()
    default_ifdtool = script_path.parents[2] / "util" / "ifdtool" / "ifdtool"
    ifdtool = shutil.which("ifdtool") or (str(default_ifdtool) if default_ifdtool.is_file() else None)
    if not ifdtool:
        print("ERROR: ifdtool not found in PATH or expected repository location", file=sys.stderr)
        return 2
    rc = run_capture([ifdtool, "-p", "lbg", "-d", str(image)], outdir / "ifdtool_dump.txt")
    if rc:
        print(f"WARNING: ifdtool -d exited with status {rc}", file=sys.stderr)

    # Cross-check descriptor math against ifdtool's decoded IFD region ranges.
    dump_text = (outdir / "ifdtool_dump.txt").read_text(encoding="utf-8", errors="replace")
    decoded = {
        int(number): (int(start, 16), int(end, 16))
        for number, _name, start, end in re.findall(
            r"Flash Region (\d+) \(([^)]*)\): ([0-9a-fA-F]+) - ([0-9a-fA-F]+)", dump_text
        )
    }
    crosscheck = []
    for region in enabled:
        tool_range = decoded.get(region["number"])
        matches = tool_range == (region["base"], region["limit_inclusive"])
        crosscheck.append(
            {
                "region": region["name"],
                "descriptor_start": f"0x{region['base']:x}",
                "descriptor_end_inclusive": f"0x{region['limit_inclusive']:x}",
                "descriptor_size": region["size"],
                "independent_extract_size": (regions_dir / f"region_{region['number']}_{region['name']}.bin").stat().st_size,
                "ifdtool_decoded_range": (
                    [f"0x{tool_range[0]:x}", f"0x{tool_range[1]:x}"] if tool_range else None
                ),
                "ifdtool_range_matches_descriptor": matches,
            }
        )
    (outdir / "ifdtool_crosscheck.json").write_text(json.dumps(crosscheck, indent=2) + "\n")

    findings = []
    for region in enabled:
        region_file = regions_dir / f"region_{region['number']}_{region['name']}.bin"
        findings.extend(inspect_region(region_file, region, outdir))
    (outdir / "signature_index.json").write_text(json.dumps(findings, indent=2) + "\n")
    (outdir / "firmware_volume_candidates.json").write_text(
        json.dumps(
            [
                finding
                for finding in findings
                if finding["signature_hex"] == "5f 46 56 48"
            ],
            indent=2,
        )
        + "\n"
    )
    bios_region = next((r for r in enabled if r["number"] == 1), None)
    fv_files = []
    if bios_region:
        bios_file = regions_dir / f"region_1_{bios_region['name']}.bin"
        bios_fvs = outdir / "bios_fvs"
        bios_fvs.mkdir()
        with bios_file.open("rb") as source:
            for index, finding in enumerate(
                x
                for x in findings
                if x.get("fv_header_valid")
                and bios_region["base"] <= int(x["absolute_offset"], 16) - 0x28 < bios_region["base"] + bios_region["size"]
            ):
                fv_base = int(finding["absolute_offset"], 16) - 0x28
                fv_size = finding["fv_length"]
                fv_path = bios_fvs / f"fv_{index:02}_{fv_base:08x}.bin"
                with fv_path.open("wb") as target:
                    source.seek(fv_base - bios_region["base"])
                    remaining = fv_size
                    while remaining:
                        block = source.read(min(1024 * 1024, remaining))
                        if not block:
                            raise ValueError(f"truncated firmware volume at 0x{fv_base:x}")
                        target.write(block)
                        remaining -= len(block)
                fv_files.append(
                    {
                        "absolute_offset": f"0x{fv_base:x}",
                        "size": fv_size,
                        "filesystem_guid": finding["fv_filesystem_guid"],
                        "sha256": sha256(fv_path),
                        "file": str(fv_path),
                    }
                )
                fv_region = {
                    "number": 1,
                    "name": f"bios_fv_{fv_base:08x}",
                    "base": fv_base,
                    "size": fv_size,
                }
                inspect_region(fv_path, fv_region, outdir)
        (outdir / "bios_fvs.json").write_text(json.dumps(fv_files, indent=2) + "\n")
        uefiextract = shutil.which("UEFIExtract") or shutil.which("uefiextract")
        if uefiextract:
            ue_rc = run_capture(
                [uefiextract, str(bios_file), "report"], outdir / "uefiextract.log"
            )
            if ue_rc:
                print(f"WARNING: UEFIExtract exited with status {ue_rc}", file=sys.stderr)
        else:
            (outdir / "uefiextract_unavailable.txt").write_text(
                "UEFIExtract was not found in PATH. See the guide for the official release/build steps.\n"
            )

    # CSE FPT layout: create only the bounded prefix needed by valid table entries.
    me_region = next((r for r in enabled if r["number"] == 2), None)
    cse_fpt = shutil.which("cse_fpt")
    default_cse_fpt = script_path.parents[2] / "util" / "cbfstool" / "cse_fpt"
    cse_fpt = cse_fpt or (str(default_cse_fpt) if default_cse_fpt.is_file() else None)
    if me_region and cse_fpt:
        me_file = regions_dir / f"region_2_{me_region['name']}.bin"
        me_bytes = me_file.read_bytes()
        fpt_offset = me_bytes.find(b"$FPT")
        if fpt_offset >= 0 and fpt_offset + 0x20 <= len(me_bytes):
            entry_count = struct.unpack_from("<I", me_bytes, fpt_offset + 4)[0]
            header_length = me_bytes[fpt_offset + 10]
            if not 0 < entry_count <= 1024 or header_length < 0x20:
                raise ValueError("Unsupported or invalid CSE FPT header")
            entries_end = fpt_offset + header_length + entry_count * 0x20
            if entries_end > len(me_bytes):
                raise ValueError("CSE FPT entries extend beyond the ME region")
            max_end = entries_end - fpt_offset
            fpt_entries = []
            for index in range(entry_count):
                entry = fpt_offset + header_length + index * 0x20
                offset, length = struct.unpack_from("<II", me_bytes, entry + 8)
                flags = struct.unpack_from("<I", me_bytes, entry + 28)[0]
                valid = offset != 0 and length != 0 and (flags >> 24) != 0xFF
                if valid:
                    max_end = max(max_end, offset + length)
                fpt_entries.append(
                    {
                        "name": me_bytes[entry : entry + 4].decode("ascii", errors="replace").rstrip("\0"),
                        "offset_from_fpt": offset,
                        "length": length,
                        "flags": f"0x{flags:08x}",
                        "valid": valid,
                        "absolute_offset": f"0x{me_region['base'] + fpt_offset + offset:x}" if valid else None,
                    }
                )
            (outdir / "me_fpt_entries.json").write_text(json.dumps(fpt_entries, indent=2) + "\n")
            if fpt_offset + max_end > len(me_bytes):
                raise ValueError("CSE FPT partition extents extend beyond the ME region")
            fpt_view = outdir / "me_fpt_view.bin"
            fpt_view.write_bytes(me_bytes[fpt_offset : fpt_offset + max_end])
            fpt_output = outdir / "me_partitions"
            fpt_output.mkdir()
            fpt_rc = run_capture(
                [cse_fpt, str(fpt_view), "print"], outdir / "cse_fpt_print.txt"
            )
            dump_rc = run_capture(
                [cse_fpt, str(fpt_view), "dump", "-o", str(fpt_output)],
                outdir / "cse_fpt_dump.txt",
            )
            if fpt_rc or dump_rc:
                print(
                    f"WARNING: cse_fpt returned print={fpt_rc}, dump={dump_rc}",
                    file=sys.stderr,
                )
            for item in fpt_entries:
                if not item["valid"]:
                    continue
                part_path = fpt_output / item["name"]
                if not part_path.is_file():
                    continue
                part_region = {
                    "number": 2,
                    "name": f"me_{item['name']}",
                    "base": me_region["base"] + fpt_offset + item["offset_from_fpt"],
                    "size": item["length"],
                }
                inspect_region(part_path, part_region, outdir)
        else:
            (outdir / "cse_fpt_unavailable.txt").write_text(
                "No supported $FPT header was found in the ME region.\n"
            )
    elif me_region:
        (outdir / "cse_fpt_unavailable.txt").write_text(
            "cse_fpt was not found in PATH or util/cbfstool next to this script.\n"
        )

    # Entropy is a triage signal only; high entropy does not prove encryption.
    entropy_rows = []
    window = 64 * 1024
    for region in enabled:
        region_file = regions_dir / f"region_{region['number']}_{region['name']}.bin"
        with region_file.open("rb") as stream:
            relative = 0
            while True:
                block = stream.read(window)
                if not block:
                    break
                entropy_rows.append(
                    f"{region['name']}\t0x{region['base'] + relative:x}\t{len(block)}\t{entropy(block):.4f}"
                )
                relative += len(block)
    (outdir / "entropy_64k.tsv").write_text("region\tabsolute_offset\tbytes\tshannon_bits_per_byte\n" + "\n".join(entropy_rows) + "\n")

    # binwalk is optional; record scans of each independently extracted region.
    binwalk = shutil.which("binwalk")
    if binwalk:
        scan_dir = outdir / "binwalk"
        scan_dir.mkdir()
        scans = [
            (r["name"], regions_dir / f"region_{r['number']}_{r['name']}.bin")
            for r in enabled
        ]
        scans.extend((f"me_{p.stem}", p) for p in sorted((outdir / "me_partitions").glob("*")) if p.is_file())
        scans.extend((p.stem, p) for p in sorted((outdir / "bios_fvs").glob("*.bin")))
        for name, scan_file in scans:
            run_capture([binwalk, str(scan_file)], scan_dir / f"{name}.txt")
    else:
        (outdir / "binwalk_unavailable.txt").write_text(
            "binwalk was not found in PATH. Install it separately, then scan each file in regions/;\n"
            "reported binwalk offsets are region-relative and must be added to the base in regions.json.\n"
        )

    print(f"Input: {image}")
    print(f"Size: {image_size} bytes; SHA-256: {image_hash}")
    print(f"Output: {outdir}")
    for region in enabled:
        print(
            f"region {region['number']:2} {region['name']:16} "
            f"0x{region['base']:08x}-0x{region['limit_inclusive']:08x} "
            f"{region['size']:>10} bytes"
        )
    print(f"Signature candidates: {len(findings)} (see signature_index.json)")
    print(f"ifdtool range cross-check: {outdir / 'ifdtool_crosscheck.json'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, struct.error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
