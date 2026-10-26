# `server.bin` SPI Flash Layout and Inspection Guide

This guide describes the raw SPI image `/home/uefi/workspace/gooxie_bios/server.bin` from the outside in: Intel Flash Descriptor (IFD) regions, then the recognized structures within each region. The image is treated as read-only. Commands that extract or unpack data write only beneath a separate output directory.

## 1. Image identity and tools

| Property | Observed value |
|---|---|
| File | `/home/uefi/workspace/gooxie_bios/server.bin` |
| Format | Raw Intel SPI flash image |
| Size | 67,108,864 bytes = 64 MiB = `0x4000000` |
| SHA-256 | `db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465` |
| Duplicate captures | `/home/uefi/workspace/gooxie_bios/uefi/server.bin` and `uefi/doc/logs/1405-03-25_gooxi_fsp/server.bin` have the same size and SHA-256 |

Tools used for the recorded inspection:

- Repository `ifdtool` v1.2, called with `-p lbg` (Denverton/C39xx platform decode).
- Repository `cse_fpt` for the Intel CSE Flash Partition Table (FPT).
- Python 3 helper included with this guide for checksum, offset, signature, entropy, and extraction checks.
- UEFIExtract NE A75 for the BIOS FV/FFS report. Its tested Linux archive SHA-256 was `5ee05d0da0c235626de67959d6cd97767e0d7ac67d42c8839e5972e8c23fd5a5`.
- `binwalk` was not installed during this inspection. It is an optional extra signature scanner, not the source of the region boundaries below.

## 2. Top-level map: 64 MiB image to IFD regions

Use `ifdtool -p lbg -d`: the generic no-platform fallback rendered region endpoints incorrectly for this capture. The selected platform output agrees with an independent decode of the raw `FLREG` registers.

| IFD region | Name | Start (inclusive) | End (inclusive) | Size | Interpretation |
|---:|---|---:|---:|---:|---|
| 0 | Flash Descriptor | `0x00000000` | `0x00000fff` | 4 KiB | IFD signature, maps, flash regions, component capabilities, straps, and access masters |
| 3 | GbE | `0x00001000` | `0x00002fff` | 8 KiB | Intel Ethernet NVM |
| 2 | Intel ME | `0x00003000` | `0x02feffff` | 50,253,824 B (47.95 MiB) | Intel CSE/ME firmware and data |
| 5 | Device Exp1 | `0x02ff0000` | `0x02ffffff` | 64 KiB | Contains a PCI option ROM image with an EFI image code type |
| 1 | BIOS | `0x03000000` | `0x03ffffff` | 16 MiB | UEFI firmware volumes, files, modules, and variable stores |

The enabled regions are contiguous, non-overlapping, and cover the full `0x00000000–0x03ffffff` image. There are no unassigned gaps. The raw registers are `FLREG0=0x00000000`, `FLREG1=0x3fff3000`, `FLREG2=0x2fef0003`, `FLREG3=0x00020001`, and `FLREG5=0x2fff2ff0`; `FLREG4` (Platform Data) is disabled because its base is above its limit. Other optional IFD regions are disabled.

The `FLCOMP` decode reports component 1 as 64 MiB and component 2 as unused. The file itself is 64 MiB; do not add the component capacities from a generic decode that does not match this platform.

### Descriptor access summary

`ifdtool -p lbg -d` reports the Host CPU/BIOS master (`FLMSTR1`) can read and write the BIOS and GbE regions, cannot write the descriptor, and cannot read or write the ME region. These are descriptor permissions in the captured image; they do not by themselves establish whether a live board can be flashed through software, because chipset locks and platform state also matter.

The descriptor starts with 16 bytes of `0xff`; the IFD signature is at image offset `0x10` (`5a a5 f0 0f`). `FLMAP0` is at `0x14`, and its FRBA pointer leads to the region-register table at `0x40`.

## 3. Inside the regions

### 3.1 Flash Descriptor

The full descriptor dump is retained as `ifdtool_dump.txt` by the helper. It includes FLMAP pointers, all FLREGs, flash component settings, the VSCC table, PCH straps, processor straps, and master read/write access fields. The platform decode identifies Denverton C39xx. Interpret strap and access bits from the complete dump; do not infer ME execution state from the presence of an ME region alone.

### 3.2 GbE NVM

The 8 KiB GbE region has SHA-256 `51deabec8b170755d7fdf244664f45e773743d72613c3e4ef58ad9ecdcf54fed`. The standard checksum over its first 64 little-endian 16-bit words is `0xbaba`, which is consistent with a valid Intel GbE NVM checksum. This verifies the checksum only; it is not a complete semantic decode of every NVM field.

### 3.3 Intel ME / CSE

The ME region contains an `$FPT` header at absolute image offset `0x5000` and a backup `FPTB` header at `0x6000`. The primary header is version `0x21`, entry version `0x10`, with 11 entries. `cse_fpt` reports all 11 entries as valid data (`D,V`). Its offsets are relative to the primary FPT image start (`0x5000`); the table below converts them to absolute SPI offsets.

| FPT entry | Absolute start | Size | End (exclusive) |
|---|---:|---:|---:|
| FPTB | `0x00006000` | 4 KiB | `0x00007000` |
| MFSB | `0x00007000` | 128 KiB | `0x00027000` |
| MFS | `0x00027000` | 388 KiB | `0x00088000` |
| IVB1 | `0x00088000` | 16 KiB | `0x0008c000` |
| IVB2 | `0x0008c000` | 16 KiB | `0x00090000` |
| PSVN | `0x00090000` | 4 KiB | `0x00091000` |
| BIS | `0x00091000` | 4 KiB | `0x00092000` |
| FLOG | `0x00092000` | 4 KiB | `0x00093000` |
| UTOK | `0x00093000` | 8 KiB | `0x00095000` |
| EFS | `0x00095000` | 64 KiB | `0x000a5000` |
| UEP | `0x000a5000` | 8 KiB | `0x000a7000` |

The FPT table is followed in the large ME region by additional signed/structured content. The signature scan found 10 `$CPD` Code Partition Directory candidates at `0xa8000`, `0xc0000`, `0xd5000`, `0xd7000`, `0x1a2000`, `0x1a8000`, `0x1c0000`, `0x1d5000`, `0x1d9000`, and `0x3cb000`; it found 14 `$MN2` manifest candidates at `0xc364`, `0x810a8`, `0xa8078`, `0xc0078`, `0xd51c8`, `0xd601c`, `0xd7048`, `0x1a2048`, `0x1a8078`, `0x1c0078`, `0x1d5738`, `0x1d801c`, `0x1d9048`, and `0x3cb048`. All offsets are absolute in `server.bin`. These are triage hits, not proof that each candidate has been fully validated or that its payload is plaintext. The helper scans and indexes the 11 extracted FPT partitions individually; use binwalk or a CSE-aware parser on those files for further recursive decoding. Signed, compressed, or encrypted vendor modules may remain opaque.

### 3.4 Device Exp1

The 64 KiB region begins with the PCI option-ROM signature `55 aa`. Its PCIR structure is at region-relative offset `0x1c`, with vendor ID `0x8086`, device ID `0x0000`, image length `0x3c` 512-byte units (30 KiB), code type `0x03` (EFI image), and indicator `0x80` (last image). This identifies an EFI PCI option-ROM image; it does not imply that the remaining bytes in the 64 KiB IFD region are part of that ROM image.

### 3.5 BIOS: firmware volumes and nested UEFI contents

The BIOS region begins with a valid PI firmware volume at absolute offset `0x03000000`. A raw-signature scan found 21 `_FVH` strings; validation of FV length, bounds, header length, revision, and 16-bit header checksum produced 15 valid FV headers. The other signature hits are not counted as firmware volumes.

| FV start (absolute) | FV size | Filesystem GUID |
|---:|---:|---|
| `0x03000000` | 512 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03080000` | 512 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03110000` | 6,588 KiB | `5473c07a-3dcb-4dca-bd6f-1e9689e7349a` |
| `0x031130e8` | 128 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x0377f000` | 2,304 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x039bf000` | 256 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x039ff000` | 260 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03a51000` | 4 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03a52000` | 180 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03a7f000` | 260 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03ac0000` | 448 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03b30000` | 384 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03b90000` | 3,328 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03ec8000` | 32 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |
| `0x03ed0000` | 1,216 KiB | `8c8ce578-8a3d-4f1c-9935-896185c32dd3` |

UEFIExtract NE A75 parsed 18 firmware volumes in its tree, including embedded volumes whose absolute base is reported as `N/A` by the tool; it reported 529 FFS file nodes, 1,895 section nodes, and 1,212 NVAR entries. The parsed sections include PE32, TE, raw, GUID-defined, and volume-image sections. The top-level dump reports FIT and microcode entries and an Intel Startup ACM. UEFIExtract also logged that it could not parse the Boot Guard Key Manifest and Boot Policy; those warnings are parser limitations, not a determination that the image is or is not Boot Guard protected.

To inspect individual firmware files and sections, use UEFIExtract’s `all` mode. Its report offsets are relative to the extracted 16 MiB BIOS region; add `0x03000000` to map a BIOS-relative offset to the original SPI image. For nested images marked `N/A`, preserve the parent/child tree instead of inventing a direct flash offset.

## 4. Reproduce the inspection

### 4.1 Run the companion helper

Choose an empty output directory on a filesystem with at least 100 MiB free. The helper checks the expected size/hash, decodes FLREGs independently, extracts each enabled IFD region and validated FV, lists gaps/overlaps, runs `ifdtool`, scans signatures and entropy, parses/dumps the CSE FPT entries when `cse_fpt` is available, and invokes optional `binwalk` and `UEFIExtract` tools when they are on `PATH`. Its UEFIExtract call uses `report` mode to avoid writing another full copy of the BIOS; use `all` mode separately when you want every parsed item extracted.

```bash
ROOT=/home/uefi/workspace/gooxie_bios
OUT=/tmp/server_bin_inspection
python3 "$ROOT/uefi/doc/logs/inspect_server_bin.py" \
  "$ROOT/server.bin" --out-dir "$OUT"
```

The output directory must be empty or absent. Important outputs:

| Output | Content |
|---|---|
| `image_identity.json` | Input byte count and SHA-256 |
| `ifdtool_dump.txt` | Full platform-selected descriptor decode |
| `regions.json`, `coverage.json`, `ifdtool_crosscheck.json` | FLREG calculations, coverage, and independent range comparison |
| `regions/region_*.bin` | Exact byte slices for each enabled IFD region |
| `cse_fpt_print.txt`, `cse_fpt_dump.txt`, `me_partitions/` | ME FPT inventory and its extracted data partitions |
| `signature_index.json`, `*_signatures.json`, `firmware_volume_candidates.json`, `bios_fvs/` | Absolute-offset signature candidates and validated FV header slices |
| `entropy_64k.tsv` | 64 KiB Shannon entropy windows; a triage clue only, never proof of encryption |
| `binwalk/` or `binwalk_unavailable.txt` | Optional binwalk output or a clear missing-tool notice |
| UEFIExtract `.report.txt`, `.guids.csv` or `uefiextract_unavailable.txt` | Optional recursive BIOS parse and GUID inventory |

To inspect a different capture, the helper rejects an unexpected hash by default. Pass `--allow-hash-mismatch` only when you have identified and intentionally selected that image; the identity and actual hash remain in the output.

### 4.2 Manual IFD commands

```bash
ROOT=/home/uefi/workspace/gooxie_bios
IMG="$ROOT/server.bin"
IFDTOOL="$ROOT/uefi/util/ifdtool/ifdtool"

file "$IMG"
stat -c '%n %s bytes' "$IMG"
sha256sum "$IMG"
"$IFDTOOL" -p lbg -d "$IMG" | tee /tmp/server-ifd-dump.txt

mkdir -p /tmp/server-ifd-extract
cd /tmp/server-ifd-extract
"$IFDTOOL" -p lbg -x "$IMG"
ls -lh
```

`-x` writes files in the current directory, which is why this example changes to a separate extraction directory. Compare each extracted file size to the corresponding `FLREG` range; use the helper’s `ifdtool_crosscheck.json` to see decoded boundaries and independently sliced byte counts together.

### 4.3 Inspect ME FPT partitions and their contents

The `cse_fpt` tool expects the FPT to start at offset 0 or `0x10`. In this capture the primary `$FPT` is at ME-relative `0x2000` (absolute `0x5000`). Create a bounded view long enough for the FPT entries and all entry ranges, then list and dump the 11 partitions:

```bash
OUT=/tmp/server_bin_inspection
ME="$OUT/regions/region_2_intel_me.bin"
FPT_VIEW="$OUT/me_fpt_view.bin"
mkdir -p "$OUT/me_partitions"

# Primary FPT starts at 0x2000 within the ME region; largest entry ends at 0xA2000.
dd if="$ME" of="$FPT_VIEW" bs=1 skip=$((0x2000)) count=$((0xA2000)) status=none
"/home/uefi/workspace/gooxie_bios/uefi/util/cbfstool/cse_fpt" \
  "$FPT_VIEW" print
"/home/uefi/workspace/gooxie_bios/uefi/util/cbfstool/cse_fpt" \
  "$FPT_VIEW" dump -o "$OUT/me_partitions"

for part in "$OUT"/me_partitions/*; do
  echo "===== $part ====="
  file "$part"
  binwalk "$part"                 # if binwalk is installed
  strings -a -n 8 "$part" | head -80
done
```

If `binwalk` is unavailable, the helper’s signature index records `$FPT`, `$CPD`, `$MN2`, `_FVH`, `MZ`, and common archive/compression signatures with absolute image offsets. Treat these as leads and validate candidate headers before calling them structures.

### 4.4 Inspect BIOS volumes, FFS files, and sections

Install binwalk if desired (`sudo apt install binwalk`). UEFIExtract is not the same as the UEFITool GUI. The tested Linux CLI archive was UEFIExtract NE A75 from the [official UEFITool releases](https://github.com/LongSoft/UEFITool/releases); the tested SHA-256 is recorded above. Download and verify it like this:

```bash
curl -L --fail --output /tmp/UEFIExtract_NE_A75_x64_linux.zip \
  https://github.com/LongSoft/UEFITool/releases/download/A75/UEFIExtract_NE_A75_x64_linux.zip
echo '5ee05d0da0c235626de67959d6cd97767e0d7ac67d42c8839e5972e8c23fd5a5  /tmp/UEFIExtract_NE_A75_x64_linux.zip' | sha256sum --check
unzip -p /tmp/UEFIExtract_NE_A75_x64_linux.zip uefiextract > /tmp/UEFIExtract
chmod +x /tmp/UEFIExtract
```

The UEFIExtract CLI documents `report`, `all`, and `dump` modes in its help. It can also be built from the [official source](https://github.com/LongSoft/UEFITool).

```bash
OUT=/tmp/server_bin_inspection
BIOS="$OUT/regions/region_1_bios.bin"

binwalk "$BIOS"
binwalk -eM "$BIOS"             # optional recursive extraction into a separate output tree
/tmp/UEFIExtract "$BIOS" report   # report + GUID inventory only
/tmp/UEFIExtract "$BIOS" all      # report + GUID inventory + recursive item extraction
```

`/tmp/UEFIExtract "$BIOS" all` creates sibling files such as `region_1_bios.bin.report.txt`, `.guids.csv`, and `.dump/`. Use `/tmp/UEFIExtract -h` for other CLI versions. For individual FFS module bodies, find a file GUID in the report and use the GUID-specific CLI form documented by `UEFIExtract -h`.

### 4.5 Validate GbE and the Device Exp1 option ROM

```bash
GBE="$OUT/regions/region_3_gbe.bin"
DEVEXP="$OUT/regions/region_5_device_exp1.bin"
python3 - "$GBE" "$DEVEXP" <<'PY'
import pathlib, struct, sys
gbe = pathlib.Path(sys.argv[1]).read_bytes()
print('GbE NVM checksum (first 64 LE words):', hex(sum(struct.unpack('<64H', gbe[:128])) & 0xffff))
rom = pathlib.Path(sys.argv[2]).read_bytes()
pcir = rom.find(b'PCIR')
if rom[:2] != b'\x55\xaa' or pcir < 0:
    raise SystemExit('No PCI option ROM / PCIR signature found')
vendor, device = struct.unpack_from('<HH', rom, pcir + 4)
units = struct.unpack_from('<H', rom, pcir + 0x10)[0]
print(f'PCIR offset={pcir:#x}, vendor={vendor:04x}, device={device:04x}, image_bytes={units * 512}, code_type={rom[pcir + 0x14]:#x}, indicator={rom[pcir + 0x15]:#x}')
PY
```

Expected outputs for this capture are checksum `0xbaba`, and PCIR vendor `8086`, device `0000`, image size `30720`, code type `0x3`, indicator `0x80`.

## 5. Interpretation limits and known corrections

- The descriptor is the authority for top-level byte ranges. Use the correct `-p lbg` decode and retain the raw FLREG register values alongside it.
- The older `analysis_report.md` region extraction note described the ME extract as 16 MiB and the second flash component as 32 MiB. That reflected the generic platform decode; for this 64 MiB Denverton image, the ME FLREG span is 50,253,824 bytes and component 2 is unused.
- A signature hit is only a candidate. In particular, raw `MZ` bytes can occur inside compressed or unrelated data; they are not a count of PE executables.
- A valid FV header does not prove every contained file is valid or bootable. UEFIExtract parsed the FFS tree, but its Boot Guard manifest parsing produced warnings. This guide does not claim to reverse engineer proprietary signed ME code or establish runtime flash protection.
- High Shannon entropy can result from compression or encrypted data; entropy alone cannot distinguish them.
- Do not flash any extracted region or modified image based only on this static layout report.
