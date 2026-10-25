#!/usr/bin/env python3
"""Turn a UEFIExtract text report into a metadata-only BIOS tree CSV.

The report is treated as input; this tool never extracts or writes firmware
payloads. Nodes decoded from compressed or GUID-defined sections may have no
absolute image offset in UEFIExtract's report and are marked accordingly.
"""

import argparse
import csv
import hashlib
import re
from pathlib import Path

GUID_RE = re.compile(
    r"\b[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
    r"[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\b"
)
IMAGE_LINE = re.compile(r"^\s*Image\s*\|\s*Intel\s*\|\s*([0-9A-Fa-f]+)\s*\|\s*([0-9A-Fa-f]+)")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_report(report_text):
    rows = []
    image_size = None
    in_bios = False
    bios_seen = False
    parents = []

    for line_number, line in enumerate(report_text.splitlines(), 1):
        image_match = IMAGE_LINE.match(line)
        if image_match:
            image_base, image_size = (int(value, 16) for value in image_match.groups())
            if image_base != 0:
                raise ValueError(f"UEFIExtract image base is {image_base:#x}, expected 0")
            continue

        fields = [field.strip() for field in line.split("|", 5)]
        if len(fields) != 6 or not fields[0]:
            continue
        node_type, subtype, base_text, size_text, _crc, tree_text = fields
        if node_type == "Region" and subtype == "BIOS":
            in_bios = True
            bios_seen = True
        elif in_bios and node_type == "Region":
            in_bios = False
        if not in_bios:
            continue

        guid_match = GUID_RE.search(tree_text)
        guid = guid_match.group(0).upper() if guid_match else ""
        # UEFIExtract's tree prefix is a run of hyphens. The BIOS region is
        # depth zero in this CSV; strip its one leading tree level.
        prefix = re.match(r"-+", tree_text)
        depth = max(0, len(prefix.group(0)) - 1) if prefix else 0
        description = tree_text.strip(" -")
        if guid_match:
            description = (tree_text[guid_match.end():].strip(" |-") or
                           tree_text[:guid_match.start()].strip(" -"))
        elif " | " in tree_text:
            description = tree_text.split(" | ", 1)[1].strip()

        if depth < len(parents):
            parents = parents[:depth]
        elif depth > len(parents):
            # Reports may omit a wrapper row. Retain an explicit placeholder
            # so the CSV does not falsely imply a direct parent.
            parents.extend(["<unreported parent>"] * (depth - len(parents)))
        parent_path = " > ".join(parents)
        node_path = " > ".join(part for part in (parent_path, description or node_type) if part)
        parents.append(description or node_type)

        if base_text.upper() == "N/A":
            image_offset = ""
            offset_status = "not given (decoded nested content or parser limitation)"
        else:
            image_offset = f"0x{int(base_text, 16):08x}"
            offset_status = "absolute image offset reported"
        try:
            size = f"0x{int(size_text, 16):x}"
        except ValueError:
            size = ""

        rows.append({
            "depth": depth,
            "type": node_type,
            "subtype_or_encoding": subtype,
            "image_offset": image_offset,
            "size": size,
            "guid": guid,
            "description": description,
            "tree_path": node_path,
            "offset_status": offset_status,
            "payload_exported": "no (metadata report only)",
            "report_line": line_number,
        })

    if image_size is None:
        raise ValueError("report has no Intel image row")
    if not bios_seen or not rows:
        raise ValueError("report has no parsed BIOS region")
    if image_size != 0x4000000:
        raise ValueError(f"report image size is {image_size:#x}, expected 64 MiB")
    return image_size, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="the exact image supplied to UEFIExtract")
    parser.add_argument("report", type=Path, help="UEFIExtract text report")
    parser.add_argument("output", type=Path, help="metadata-only CSV output")
    args = parser.parse_args()

    if args.image.stat().st_size != 0x4000000:
        parser.error("image must be exactly 64 MiB")
    report_size, rows = parse_report(args.report.read_text(errors="replace"))
    digest = sha256(args.image)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    print(f"image_size=0x{report_size:x} image_sha256={digest}")
    print(f"bios_nodes={len(rows)} csv={args.output}")
    print(f"nodes_without_absolute_image_offset={sum(not row['image_offset'] for row in rows)}")


if __name__ == "__main__":
    main()
