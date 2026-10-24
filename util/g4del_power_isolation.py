#!/usr/bin/env python3
"""Build byte-controlled G4DEL ROMs for a pre-coreboot power failure.

Only the two SHA-256-pinned, 64 MiB reference ROMs are accepted. Output ROMs
are diagnostic artifacts; this program does not contact the board or flash it.
"""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROM_SIZE = 0x4000000
DESCRIPTOR_END = 0x1000
GBE_START, GBE_END = 0x1000, 0x3000
ME_START, ME_END = 0x3000, 0x2FF0000
PT_START, BIOS_START = 0x2FF0000, 0x3000000
SMMSTORE_END = 0x3040000
HAP_OFFSET = 0x1D6
DESCRIPTOR_DIFFERENCES = {
    0x81, 0x82, 0x83, 0x85, 0x86, 0x87, 0x89, 0x8A, 0x8B,
    0x91, 0x92, 0x93, HAP_OFFSET,
}
WORKING_SHA256 = "50001ff4303e7852b14f9e0ec7013d8e0db6cc9737db2f64f94f201bd4a0789d"
FAILED_SHA256 = "cb0aae73fcaefdbf0d80d2e3ce05b6cc21709ea21e2dfb7b7323a7399a345d66"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_reference(path, expected_hash):
    image = path.read_bytes()
    if len(image) != ROM_SIZE or digest(image) != expected_hash:
        raise ValueError(f"unexpected source ROM size or SHA-256: {path}")
    if image[0x10:0x14] != bytes.fromhex("5aa5f00f"):
        raise ValueError(f"Intel descriptor signature missing: {path}")
    return image


def changed_regions(candidate, source):
    regions = {
        "descriptor": (0, DESCRIPTOR_END),
        "GbE": (GBE_START, GBE_END),
        "ME": (ME_START, ME_END),
        "Device Exp1": (PT_START, BIOS_START),
        "SMMSTORE": (BIOS_START, SMMSTORE_END),
        "rest of BIOS": (SMMSTORE_END, ROM_SIZE),
    }
    return [name for name, (start, end) in regions.items()
            if candidate[start:end] != source[start:end]]


def inspect_image(path, cbfstool, ifdtool):
    layout = subprocess.run([str(cbfstool), str(path), "layout", "-w"],
                            capture_output=True, text=True, check=True).stdout
    for region in ("SI_DESC", "SI_BIOS", "SMMSTORE", "COREBOOT"):
        if f"'{region}'" not in layout:
            raise ValueError(f"{path}: FMAP lacks {region}")
    if "'SMMSTORE' (preserve, size 262144, offset 50331648)" not in layout:
        raise ValueError(f"{path}: unexpected SMMSTORE geometry")

    cbfs = subprocess.run([str(cbfstool), str(path), "print"],
                          capture_output=True, text=True, check=True).stdout
    for module in ("bootblock", "fallback/payload", "fspt.bin",
                   "fspm.bin", "fsps.bin"):
        if not re.search(rf"^{re.escape(module)}\s", cbfs, re.MULTILINE):
            raise ValueError(f"{path}: CBFS lacks {module}")

    ifd = subprocess.run([str(ifdtool), "-p", "lbg", "-d", str(path)],
                         capture_output=True, text=True, check=True).stdout
    hap = re.search(r"HAP bit is (not )?set", ifd)
    host_permissions = re.search(r"FLMSTR1:\s+(0x[0-9a-fA-F]+)", ifd)
    if hap is None or host_permissions is None:
        raise ValueError(f"{path}: cannot decode descriptor policy")
    return {"hap_set": hap.group(1) is None,
            "host_permissions": host_permissions.group(1)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--working", type=Path,
                        default=Path("/home/uefi/workspace/coreboot/build/coreboot_07-26.rom"))
    parser.add_argument("--failed", type=Path,
                        default=Path("build/coreboot_08-02.rom"))
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/g4del_isolation"))
    parser.add_argument("--cbfstool", type=Path, default=Path("build/cbfstool"))
    parser.add_argument("--ifdtool", type=Path,
                        default=Path("build/util/ifdtool/ifdtool"))
    args = parser.parse_args()

    working = read_reference(args.working, WORKING_SHA256)
    failed = read_reference(args.failed, FAILED_SHA256)
    if working[ME_START:ME_END] != failed[ME_START:ME_END]:
        raise ValueError("ME differs between references; descriptor isolation is invalid")
    if working[BIOS_START:SMMSTORE_END] != failed[BIOS_START:SMMSTORE_END]:
        raise ValueError("SMMSTORE differs between references; preserve its contents first")
    offsets = {i for i in range(DESCRIPTOR_END) if working[i] != failed[i]}
    if offsets != DESCRIPTOR_DIFFERENCES or (working[HAP_OFFSET], failed[HAP_OFFSET]) != (1, 0):
        raise ValueError("unexpected descriptor differences; inspect before composing images")

    hap_only = bytearray(failed)
    hap_only[HAP_OFFSET] = working[HAP_OFFSET]
    unlock_only = bytearray(failed)
    for offset in DESCRIPTOR_DIFFERENCES - {HAP_OFFSET}:
        unlock_only[offset] = working[offset]
    factory_gbe_pt = bytearray(working)
    factory_gbe_pt[GBE_START:GBE_END] = failed[GBE_START:GBE_END]
    factory_gbe_pt[PT_START:BIOS_START] = failed[PT_START:BIOS_START]

    experiments = [
        ("08-02_with_07-26_descriptor.rom", working[:DESCRIPTOR_END] + failed[DESCRIPTOR_END:],
         failed, ["descriptor"], "Tests the combined working descriptor settings"),
        ("08-02_hap_only.rom", bytes(hap_only), failed, ["descriptor"],
         "Tests HAP while retaining factory access permissions"),
        ("08-02_unlock_only.rom", bytes(unlock_only), failed, ["descriptor"],
         "Tests working access permissions while retaining factory HAP state"),
        ("08-02_with_07-26_nonbios.rom", working[:BIOS_START] + failed[BIOS_START:],
         failed, ["descriptor", "GbE", "Device Exp1"],
         "Tests the current BIOS with all working non-BIOS regions"),
        ("07-26_with_08-02_descriptor.rom", failed[:DESCRIPTOR_END] + working[DESCRIPTOR_END:],
         working, ["descriptor"], "Tests the factory descriptor with the working BIOS"),
        ("07-26_with_08-02_nonbios.rom", failed[:BIOS_START] + working[BIOS_START:],
         working, ["descriptor", "GbE", "Device Exp1"],
         "Tests the working BIOS with all current non-BIOS regions"),
        ("07-26_with_factory_gbe_pt.rom", bytes(factory_gbe_pt), working,
         ["GbE", "Device Exp1"], "Tests factory GbE and Device Exp1 with the working BIOS"),
    ]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "working": {"path": str(args.working), "sha256": WORKING_SHA256,
                    "descriptor": inspect_image(args.working, args.cbfstool, args.ifdtool)},
        "failed": {"path": str(args.failed), "sha256": FAILED_SHA256,
                   "descriptor": inspect_image(args.failed, args.cbfstool, args.ifdtool)},
        "rom_size": ROM_SIZE,
        "board_test": "pending",
        "experiments": [],
    }
    for filename, image, source, expected_regions, purpose in experiments:
        if len(image) != ROM_SIZE or changed_regions(image, source) != expected_regions:
            raise ValueError(f"{filename}: unexpected byte changes")
        if image[ME_START:ME_END] != failed[ME_START:ME_END]:
            raise ValueError(f"{filename}: ME bytes changed")
        path = args.output_dir / filename
        if path.exists():
            if digest(path.read_bytes()) != digest(image):
                raise FileExistsError(f"refusing to overwrite different ROM: {path}")
        else:
            with path.open("xb") as output:
                output.write(image)
        policy = inspect_image(path, args.cbfstool, args.ifdtool)
        manifest["experiments"].append({
            "file": str(path), "sha256": digest(image), "size": len(image),
            "changed_regions": expected_regions, "purpose": purpose,
            "descriptor": policy, "board_test": "pending",
        })
        print(f"{digest(image)}  {path}  changed={','.join(expected_regions)}  "
              f"hap={policy['hap_set']} host_permissions={policy['host_permissions']}")

    manifest_path = args.output_dir / "manifest.json"
    manifest_text = json.dumps(manifest, indent=2) + "\n"
    if manifest_path.exists() and manifest_path.read_text() != manifest_text:
        raise FileExistsError(f"refusing to overwrite different manifest: {manifest_path}")
    if not manifest_path.exists():
        with manifest_path.open("x") as output:
            output.write(manifest_text)


if __name__ == "__main__":
    main()
