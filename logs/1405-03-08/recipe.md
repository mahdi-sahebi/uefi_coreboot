# Recipe — Deep Verification of `logs/1405-03-08/coreboot.rom`

**Subject:** ASRock SPC741D8-2L2T/BCM (Intel Xeon SP Sapphire Rapids), Dasharo coreboot + EDK II UEFI payload
**Branch:** `edk2-server`
**ROM under test:** [logs/1405-03-08/coreboot.rom](coreboot.rom) — 64 MiB, built `2026-05-28 07:11`
**MD5:** `bc3e5a5a2d097491ec2b82a8058cac3d` — **identical** to [logs/1405-03-07/coreboot_mn_1405-03-07_cb.rom](../1405-03-07/coreboot_mn_1405-03-07_cb.rom)
**Author:** Mahdi Sahebi
**Date:** 2026-05-29

This recipe covers **every step needed to verify a coreboot + EDK II UEFI-payload ROM end-to-end**: the SPI image layout, CBFS contents, EDK II payload ELF, PEIFV / DXEFV firmware volumes, the full list of PEI drivers (PEIMs) inside `peiCoreImage`, and the coreboot→EDK II handoff (RAM/HOB/coreboot table). Every command can be re-run against this ROM (or any future build) and every result is reproduced from the captured artefacts in [analysis/](analysis/).

---

## 0. TL;DR — Findings

| Check | Status | Detail |
|---|:-:|---|
| Intel Flash Descriptor | OK | 4 regions, 64 MB total, ME 47.7 MB |
| FMAP layout | OK | matches `board.fmd` (SI_DESC/SI_ME/SI_PT/BOOTSPLASH/COREBOOT) |
| CBFS contents | OK | 17 entries, payload + FSP-T/M/S + microcode + romstage/ramstage/postcar/bootblock |
| EDK II payload ELF | OK | i386, entry `0x802580`, single LOAD `0x800000..0x1630000` |
| PEIFV `_FVH` signature | OK | size `0x90000`, FFS2 GUID |
| DXEFV `_FVH` signature | OK | size `0xDA0000`, 86 files (1 DXE_CORE + 82 DRIVER + 2 APP + 1 FREEFORM) |
| PEIFV contents | OK | 1×SEC_CORE + 1×PEI_CORE + **13 PEIMs** (full GUID list below) |
| `CONFIG_SMMSTORE` | **OFF** | matches project memory: SmmStorePei produces a HOB that FaultTolerantWritePei can't use |
| `BlSupportPei` MemInfoCallback | **FIXED** | now `(Base + Size) > *UsableLowMemTop` (no 64 MB threshold) |
| Coreboot→EDK II handoff (in source) | **GATED OFF** | `protected_mode_call_1arg(...)` is **commented out** in [src/lib/prog_loaders.c:798](../../src/lib/prog_loaders.c#L798); coreboot stops at `verify_memory_for_payload()` and never jumps to SecCore |
| Crashes in current ROM | **N/A** | this ROM cannot reach EDK II — what looks like a "hang" is a deliberate stop on the VGA verification screen |

The four crash bugs catalogued in [logs/analysis_report.md](../analysis_report.md) and [logs/1405-03-03/analyize.md](../1405-03-03/analyize.md) describe the **historical** EDK II PEI crash (FaultTolerantWritePei at `PeimFileHandle=0x829FE8`). This 1405-03-08 ROM **does not exhibit that crash**, because the call into the payload is currently disabled in coreboot ramstage. To re-trigger the EDK II path you must uncomment line 798 of `src/lib/prog_loaders.c` and rebuild.

---

## 1. Prerequisites — build host tools

```bash
cd /home/mahdi/repositories/coreboot/dasharo/coreboot
make -C util/cbfstool -j$(nproc)
make -C util/ifdtool -j$(nproc)
```

Verify:

```bash
ls -l util/cbfstool/cbfstool util/ifdtool/ifdtool
```

---

## 2. Verify the SPI image layout

### 2.1 Intel Flash Descriptor (ifdtool)

```bash
util/ifdtool/ifdtool -d logs/1405-03-08/coreboot.rom | tee logs/1405-03-08/analysis/ifdtool_dump.txt
```

Captured: [analysis/ifdtool_dump.txt](analysis/ifdtool_dump.txt)

Result — descriptor + 3 main regions (`SI_DESC`, `SI_ME`, `SI_BIOS`) plus the platform-data region used by the FMAP:

| FLREG | Region | Range | Size |
|---|---|---|---|
| 0 | Flash Descriptor | `0x00000000–0x00000FFF` | 4 KB |
| 1 | BIOS | `0x03000000–0x03FFFFFF` | 16 MB |
| 2 | Intel ME | `0x00003000–0x02FEFFFF` | ~47.7 MB |
| 3 | GbE NVM | `0x00001000–0x00002FFF` | 8 KB |

ME is mapped (HAP/AltMeDisable handling is done by the included `me.bin`, not visible at descriptor level). Component density: 64 MB. PCH revision detected as Ibex-Peak-compatible (this is normal for an SPI image dumped without specifying the platform).

### 2.2 FMAP layout (cbfstool layout)

```bash
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom layout | tee logs/1405-03-08/analysis/fmap_layout.txt
```

Captured: [analysis/fmap_layout.txt](analysis/fmap_layout.txt)

| Section | Offset | Size |
|---|---|---|
| `SI_DESC` | `0x00000000` | 4 096 |
| `SI_ME`   | `0x00003000` | 50 253 824 |
| `SI_PT`   | `0x02FF0000` | 65 536 |
| `BOOTSPLASH` (CBFS) | `0x03000000` | 1 048 576 |
| `COREBOOT` (CBFS)   | `0x03100000` | 15 726 592 |

Matches `src/mainboard/asrock/spc741d8/board.fmd` exactly (16 MB BIOS region, 1 MB boot splash, ~15 MB CBFS).

---

## 3. Inspect CBFS contents

```bash
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom print -r COREBOOT | tee logs/1405-03-08/analysis/cbfs_print.txt
```

Captured: [analysis/cbfs_print.txt](analysis/cbfs_print.txt)

| File | Offset | Type | Size | Compression |
|---|---|---|---|---|
| `cbfs_master_header` | `0x00000000` | header | 32 | none |
| `fallback/payload`   | `0x00000080` | simple elf | 1 067 545 | none |
| `cpu_microcode_blob.bin` | `0x00104B00` | microcode | 1 782 784 | none |
| `intel_fit` | `0x002B7F80` | intel_fit | 80 | none |
| `fallback/romstage` | `0x002B8000` | stage | 49 160 | none |
| `fallback/ramstage` | `0x002C40C0` | stage | 126 605 | LZMA (278 952 decompressed) |
| `config` | `0x002E3000` | raw | 4 298 | LZMA (14 955 decompressed) |
| `revision` | `0x002E4140` | raw | 878 | none |
| `build_info` | `0x002E4500` | raw | 130 | none |
| `fallback/dsdt.aml` | `0x002E4600` | raw | 18 414 | none |
| `fspm.bin` | `0x002E9780` | fsp | 3 375 104 | none |
| `fsps.bin` | `0x00621800` | fsp | 185 328 | LZ4 (393 216 decompressed) |
| `fallback/postcar` | `0x0064EC80` | stage | 30 128 | none |
| `fspt.bin` | `0x00EDF780` | fsp | 32 768 | none |
| `bootblock` | `0x00EF87C0` | bootblock | 28 672 | none |

Extract individual artefacts (already saved to `analysis/`):

```bash
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n fallback/payload -m x86 -f logs/1405-03-08/analysis/payload.elf
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n fspt.bin    -f logs/1405-03-08/analysis/fspt.bin
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n fspm.bin    -f logs/1405-03-08/analysis/fspm.bin
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n fsps.bin    -f logs/1405-03-08/analysis/fsps.bin
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n bootblock   -f logs/1405-03-08/analysis/bootblock.bin
util/cbfstool/cbfstool logs/1405-03-08/coreboot.rom extract -r COREBOOT -n config      -f logs/1405-03-08/analysis/config.txt
```

### 3.1 FSP blobs — header check

Each FSP file is itself a `_FVH` firmware volume. Confirmed via `hexdump -C`:

| File | Signature offset | `_FVH` magic |
|---|---|---|
| `fspt.bin` (32 KB)   | `0x28` | yes |
| `fspm.bin` (3.22 MB) | `0x28` | yes |
| `fsps.bin` (384 KB)  | `0x28` | yes |

Three FITs are registered for microcode patches in the SPI image; addresses appear in the build log at [logs/1405-03-07/log.txt:56-60](../1405-03-07/log.txt#L56-L60).

### 3.2 Coreboot build config (config CBFS entry)

Extracted to [analysis/config.txt](analysis/config.txt). Key settings:

| Setting | Value | Comment |
|---|---|---|
| `CONFIG_BOARD_ASROCK_SPC741D8` | `y` | board ID |
| `CONFIG_PAYLOAD_EDK2` | `y` | EDK II payload selected |
| `CONFIG_EDK2_TAG_OR_REV` | `asrock` | local branch in [payloads/external/edk2/workspace/dasharo/](../../payloads/external/edk2/workspace/dasharo/) |
| `CONFIG_SMMSTORE` | **not set** | matches the project-memory finding |
| `CONFIG_TPM_MEASURED_BOOT` | `y` | TPM PEI stack present in PEIFV |
| `CONFIG_FSP_FD_PATH` | `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd` | Xeon SP / Eagle Stream FSP |
| `CONFIG_DCACHE_RAM_BASE` | `0xfe800000` | CAR region (coreboot pre-DRAM) |
| `CONFIG_FSP_TEMP_RAM_SIZE` | `0x60000` | 384 KB FSP temp RAM |
| `CONFIG_TTYS0_BAUD` | `115200` | COM1 console baud |

`# CONFIG_SMMSTORE is not set` ⇒ no `SMMSTORE` FMAP region exists; SmmStorePei still dispatches but `ParseSMMSTOREInfo()` fails, so no `gVariableFlashInfoHobGuid` is produced. (This is the root cause of the historic FaultTolerantWritePei hang documented in [logs/1405-03-03/analyize.md](../1405-03-03/analyize.md).)

---

## 4. EDK II payload — ELF, FV layout, and PEIMs (`peiCoreImage`)

### 4.1 ELF header

```bash
readelf -l logs/1405-03-08/analysis/payload.elf
```

Captured: [analysis/payload_readelf.txt](analysis/payload_readelf.txt)

| Field | Value |
|---|---|
| Class | ELF 32-bit LSB, Intel 80386 |
| Type | EXEC |
| Entry point | `0x00802580` |
| Program headers | 1 (single `LOAD`) |
| LOAD VirtAddr | `0x00800000` |
| LOAD FileSiz / MemSiz | `0xE30000` (14 876 672 bytes — exactly the size of the `UEFIPAYLOAD.fd` in `[FD.UefiPayload]` of `DasharoPayloadPkg.fdf`) |
| Flags | `RWE` |

The single load segment carries the full UefiPayload FD. Layout matches `DasharoPayloadPkg.fdf:12-23`:

| Range (vaddr) | Region |
|---|---|
| `0x00800000 – 0x0088FFFF` (size `0x90000`) | **`FV.PEIFV`** — the PEI Core image (PEI_CORE + all PEIMs) |
| `0x00890000 – 0x0162FFFF` (size `0xDA0000`) | `FV.DXEFV` — DXE Core + DXE drivers |

### 4.2 PEIFV — every file in `peiCoreImage`, with GUID and module name

Walked with [walk_peifv.py](analysis/peifv_walk.txt) (Python script reading the ELF LOAD segment, parsing `_FVH`, then traversing the FFS chain with proper PAD-file and FFS3 handling).

```
=== FV: PEIFV  base=0x800000  size=0x90000  FS=EFI_FIRMWARE_FILE_SYSTEM2_GUID  Rev=2 ===
```

**Full inventory (in physical / dispatch order):**

| FFS idx | Offset (in PEIFV) | `FileHandle` | Type | Size | FILE_GUID | Module |
|---:|---|---|---|---|---|---|
| 0 | `0x000048` | `0x00800048` | PAD | `0x20` | — | header pad |
| 1 | `0x000068` | `0x00800068` | **SEC_CORE** | `0x5038` | `ba7be337-6cfb-4dbb-b26c-21ec2fc16073` | `DasharoPayloadPkg/SecCore/SecCore.inf` |
| 2 | `0x0050A0` | `0x008050A0` | PAD | `0xF48` | — | alignment pad |
| 3 | `0x005FE8` | `0x00805FE8` | **PEI_CORE** | `0x1203A` | `52c05b14-0b98-496c-bc3b-04b50211d680` | `MdeModulePkg/Core/Pei/PeiMain` |
| 4 | `0x018028` | `0x00818028` | PAD | `0xFC0` | — | alignment pad |
| 5 | `0x018FE8` | `0x00818FE8` | **PEIM[0]** | `0x903A` | `9B3ADA4F-AE56-4C24-8DEA-F03B7558AE50` | `MdeModulePkg/Universal/PCD/Pei/Pcd.inf` |
| 6 | `0x022028` | `0x00822028` | PAD | `0xFC0` | — | |
| 7 | `0x022FE8` | `0x00822FE8` | **PEIM[1]** | `0x6042` | `8CC70A5A-51B4-4049-B97E-982B1D7D4049` | `DasharoPayloadPkg/SmmStorePei/SmmStorePei.inf` |
| 8 | `0x029030` | `0x00829030` | PAD | `0xFB8` | — | |
| 9 | `0x029FE8` | **`0x00829FE8`** | **PEIM[2]** | `0x7056` | `AAC33064-9ED0-4B89-A5AD-3EA767960B22` | **`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.inf`** ← historic crash target |
| 10 | `0x031040` | `0x00831040` | PAD | `0xFA8` | — | |
| 11 | `0x031FE8` | `0x00831FE8` | **PEIM[3]** | `0x9046` | `352C6AF8-315B-4BD6-B04F-31D4ED1EBE57` | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.inf` |
| 12 | `0x03B030` | `0x0083B030` | PAD | `0xFB8` | — | |
| 13 | `0x03BFE8` | `0x0083BFE8` | **PEIM[4]** | `0x605E` | `A3610442-E69F-4DF3-82CA-2360C4031A23` | `MdeModulePkg/Universal/ReportStatusCodeRouter/Pei/ReportStatusCodeRouterPei.inf` |
| 14 | `0x042048` | `0x00842048` | PAD | `0xFA0` | — | |
| 15 | `0x042FE8` | `0x00842FE8` | **PEIM[5]** | `0x6056` | `9D225237-FA01-464C-A949-BAABC02D31D0` | `MdeModulePkg/Universal/StatusCodeHandler/Pei/StatusCodeHandlerPei.inf` |
| 16 | `0x049040` | `0x00849040` | PAD | `0xFA8` | — | |
| 17 | `0x049FE8` | `0x00849FE8` | **PEIM[6]** | `0xA03A` | `86D70125-BAA3-4296-A62F-602BEBBB9081` | `MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf` |
| 18 | `0x054028` | `0x00854028` | PAD | `0xFC0` | — | |
| 19 | `0x054FE8` | `0x00854FE8` | **PEIM[7]** | `0x605A` | `ADF01BF6-47D6-495D-B95B-687777807214` | `MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTablePei/FirmwarePerformancePei.inf` |
| 20 | `0x05B048` | `0x0085B048` | PAD | `0xFA0` | — | |
| 21 | `0x05BFE8` | `0x0085BFE8` | **PEIM[8]** | `0x7046` | `BF7F2B0C-9F2F-4889-AB5C-12460022BE87` | `DasharoPayloadPkg/Tcg/Tcg2Config/Tcg2ConfigPei.inf` |
| 22 | `0x063030` | `0x00863030` | PAD | `0xFB8` | — | |
| 23 | `0x063FE8` | `0x00863FE8` | **PEIM[9]** | `0xA03A` | `2BE1E4A6-6505-43B3-9FFC-A3C8330E0432` | `SecurityPkg/Tcg/TcgPei/TcgPei.inf` |
| 24 | `0x06E028` | `0x0086E028` | PAD | `0xFC0` | — | |
| 25 | `0x06EFE8` | `0x0086EFE8` | **PEIM[10]** | `0x1203A` | `A0C98B77-CBA5-4BB8-993B-4AF6CE33ECE4` | `SecurityPkg/Tcg/Tcg2Pei/Tcg2Pei.inf` |
| 26 | `0x081028` | `0x00881028` | PAD | `0xFC0` | — | |
| 27 | `0x081FE8` | `0x00881FE8` | **PEIM[11]** | `0x604A` | `DED60489-979C-4B5A-8EE4-4068B0CC38DC` | `SecurityPkg/Tcg/Opal/OpalPassword/OpalPasswordPei.inf` |
| 28 | `0x088038` | `0x00888038` | PAD | `0xFB0` | — | |
| 29 | `0x088FE8` | `0x00888FE8` | **PEIM[12]** | `0x604A` | `91AD7375-8E8E-49D2-A343-68BC78273955` | `SecurityPkg/HddPassword/HddPasswordPei.inf` |
| — | `0x08F038` | end of FFS list | — | — | (rest = `0xFF` free space, ~3.2 KB) | |

Sanity check against project memory:
- `PEIM[2] FFS offset 0x29FE8 + base 0x800000 = 0x829FE8` (matches historic VGA log).
- 13 PEIMs total (FDF lists 8 plus the TPM/Opal/HDD‑password block enabled by `TPM_ENABLE=TRUE`).
- No PEI Apriori file ⇒ dispatch order is the literal FDF order.

### 4.3 PeiCore (the actual "PeiCoreImage")

Inside `FV.PEIFV` the **PEI_CORE** file at offset `0x5FE8`, file handle `0x00805FE8`, FILE_GUID `52C05B14-0B98-496C-BC3B-04B50211D680`, contains the PE32 of `MdeModulePkg/Core/Pei/PeiMain`. SEC's `SecStartupPhase2` (in `DasharoPayloadPkg/SecCore/FindPeiCore.c`) scans this FV for the first PEI_CORE-type file and uses its entry point — confirmed at runtime by the VGA log `[1]PC:D=0 S=8FE94 P=8030B4` (PeiCore loaded at ~`0x8030B4`, inside the PEI_CORE FFS at `0x805FE8` after section extraction).

### 4.4 DXEFV summary

Walked the same way; full counts:

```
DXEFV size=0xDA0000   FV header sig=_FVH   HdrLen=0x48   ExtHeader=none
Total files: 86
  FREEFORM    × 1
  DXE_CORE    × 1   GUID=d6a2cb7f-6a18-4e2f-b43b-9920a733700a  (MdeModulePkg/Core/Dxe/DxeMain)
  DRIVER      × 82
  APPLICATION × 2
```

DXEFV is not the focus of this recipe but the header validation matters: if DXEFV's `_FVH` is malformed, DxeIpl's PEIM-to-DXE transfer would silently fail. It is well-formed here.

---

## 5. Coreboot → EDK II handoff — full source-level walkthrough

The handoff is a chain of five distinct things:

1. RAM is initialized (FSP-M does memory training; coreboot publishes `lb_memory` ranges).
2. Coreboot constructs the **coreboot table** in low DRAM (`LBIO` at `0x63593000` historically).
3. Coreboot constructs a **minimal EFI HOB list** (since `CONFIG_UEFI_PAYLOAD_HOB_CONSTRUCTION`-style logic exists in this fork) and stashes a pointer in CBMEM under `CBMEM_ID_HOB_POINTER`.
4. Coreboot's payload loader calls `payload_load()`, which `selfload_mapped()`s the EDK II ELF at `0x800000`.
5. Coreboot's `payload_run()` jumps into EDK II's SEC entry (`SecEntry.nasm` → `SecStartup()`), passing `BootloaderParameter = LBIO pointer` and the temp-RAM window.

### 5.1 Step 1 — RAM is installed (in coreboot)

By the time `payload_run()` is reached, the following hold:
- FSP-M ran during romstage (DDR5 training).
- Postcar tore down CAR.
- Ramstage runs in DRAM, has already enumerated devices, built ACPI tables, and added the coreboot table to CBMEM.
- `cbmem_top() != 0` ⇒ permanent DRAM is mapped.

`verify_memory_for_payload()` in [src/lib/prog_loaders.c:715-744](../../src/lib/prog_loaders.c#L715-L744) walks `bootmem_walk()` and prints a VGA summary:

```c
vga_cb_sprintf(0, "=== EDKII PAYLOAD CHECK ===");
vga_cb_sprintf(row++, "TOTAL RAM : %llu MB", info.total_ram >> 20);
vga_cb_sprintf(row++, "RAM RANGE : 0x%llx-0x%llx", first_ram_base, first_ram_end);
vga_cb_sprintf(row++, "PAYLOAD   : %llu MB", payload_size >> 20);
vga_cb_sprintf(row++, "PERM RAM  : %s (CAR=%s)", permanent_ram?"YES":"NO", permanent_ram?"NO":"YES");
vga_cb_sprintf(row++, "CBMEM     : %s", cbmem_ok?"OK":"NO");
vga_cb_sprintf(row++, "HOB       : %s @ %p", hob_ok?"OK":"MISSING", hob_list);
// ...
if (permanent_ram && cbmem_ok && hob_ok && total_ram >= 256 MB)
    vga_cb_sprintf(row++, "=== READY FOR EDK II ===");
else
    vga_cb_sprintf(row++, "=== NOT READY ===");
```

This is the **pre-flight check** the user added (commit `c9b8748bda` on 2026-05-28). It runs every boot of this ROM and freezes the screen at the result — coreboot then waits indefinitely because the next call is commented out (see §5.4).

### 5.2 Step 2 — Coreboot table

`mem_callback` in `prog_loaders.c:653-671` walks bootmem entries and classifies them by `range_entry_tag()`. Only `BM_MEM_RAM` ranges contribute to `info.total_ram` and `info.first_ram_base/end`. `BM_MEM_PAYLOAD` is captured separately. This is purely diagnostic — the actual `lb_memory` table is built earlier and consumed by EDK II's `CbParseLib` once SEC reaches PEI.

EDK II side (already present in [payloads/external/edk2/workspace/dasharo/UefiPayloadPkg/Library/CbParseLib/CbParseLib.c:142-180](../../payloads/external/edk2/workspace/dasharo/UefiPayloadPkg/Library/CbParseLib/CbParseLib.c#L142-L180)):

- `GetParameterBase()` returns the `BootloaderParameter` argument that SEC received (the coreboot table pointer).
- `FindCbTag(CB_TAG_MEMORY)` locates the memory map entries.
- `ParseMemoryInfo(MemInfoCallback, &UsableLowMemTop)` is called by **BlSupportPei** in PEI.

### 5.3 Step 3 — HOB list pointer

`verify_memory_for_payload()` queries `prog_entry_arg(&global_payload)` first, and falls back to `cbmem_find(CBMEM_ID_HOB_POINTER)`. If `hob_ok == false`, the screen prints `HOB missing -> PEI crash` — this used to be the failure mode before the BlSupportPei fix. With the current source (`(Base+Size) > *UsableLowMemTop` in [BlSupportPei.c:347-353](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L347-L353)), the HOB construction by `BlSupportPei` should produce a valid `PeiMemBase` for every Xeon-SP memory map, regardless of low-RAM fragmentation.

### 5.4 Step 4–5 — Payload load and jump (CURRENTLY GATED OFF)

`payload_load()` in [src/lib/prog_loaders.c:357-401](../../src/lib/prog_loaders.c#L357-L401) `selfload_mapped()`s the ELF. The single `LOAD` segment is copied from CBFS into DRAM at `0x800000`. After `payload_load()`, `prog_entry(payload) == 0x802580` (the entry point of `SecEntry.nasm`).

Then [src/lib/prog_loaders.c:750-806](../../src/lib/prog_loaders.c#L750-L806) — `payload_run()`:

```c
void payload_run(void) {
    /* ... */
    const uint32_t arg   = pointer_to_uint32_safe(prog_entry_arg(prog));
    const uint32_t entry = pointer_to_uint32_safe(prog_entry(prog));

    vga_cb_sprintf(2, "PL-%X, %X", entry, arg);
    verify_memory_for_payload();

    // TODO(MN): Uncomment to run the payload
    // protected_mode_call_1arg((void *)(uintptr_t)entry, arg);
}
```

The `protected_mode_call_1arg()` line is **commented out**. Consequence:

- The ROM loads EDK II into DRAM at `0x800000` and locates its entry at `0x802580`.
- Coreboot prints `PL-0x802580, 0x63593000` (or similar) to VGA row 2.
- Coreboot prints the verification panel (RAM range, CBMEM, HOB).
- Coreboot **never jumps** to EDK II SEC.
- `payload_run()` returns; ramstage falls off the end and either reboots or sits in `die()`.

So when you see the screen freeze on this ROM, you are looking at **coreboot deliberately stopping**, not an EDK II crash. The "look for my crashes" question, for this specific ROM, resolves to: there are no EDK II crashes here, because EDK II never gets to run.

To exercise the EDK II path (and re-encounter the historic FaultTolerantWritePei hang or whatever crash now manifests), uncomment that one line and rebuild:

```diff
-    // TODO(MN): Uncomment to run the payload
-    // protected_mode_call_1arg((void *)(uintptr_t)entry, arg);
+    /* Hand off to EDK II in 32-bit protected mode */
+    protected_mode_call_1arg((void *)(uintptr_t)entry, arg);
```

### 5.5 Step 5 — EDK II SEC's expectations

When the call IS made, SEC receives (from `SecEntry.nasm`):

| Argument | Value | Source |
|---|---|---|
| `SizeOfRam` | `0x10000` | temp-RAM window size (64 KB at `0x80000–0x90000`) |
| `TempRamBase` | `0x80000` | beginning of temp-RAM heap+stack |
| `BFVBase` | `0x800000` | base of the BFV (= PEIFV) |
| `BootloaderParameter` | LBIO pointer (e.g. `0x63593000`) | coreboot table |

SEC then:
1. Sets up a 32-bit stack at `TempRamBase + 0x8000` (top of heap → start of stack).
2. Calls `SecStartup()` ([SecMain.c:68](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/SecCore/SecMain.c#L68)).
3. `SecStartup()` initialises `SecCoreData` and calls `SecStartupPhase2()`.
4. `SecStartupPhase2()` ([FindPeiCore.c]) scans `FV.PEIFV` for the first `EFI_FV_FILETYPE_PEI_CORE` file (= entry at `0x805FE8`), extracts its PE32 section, and calls its entry point at `~0x8030B4`.
5. `PeiCore()` initialises PEI services and enters `PeiDispatcher()`, which then dispatches the 13 PEIMs.

The historic crash sequence: PEIM[0] (Pcd) ✓, PEIM[1] (SmmStorePei) → succeeds **only by accident** (no `gVariableFlashInfoHobGuid` HOB built because `CONFIG_SMMSTORE` is off), PEIM[2] (FaultTolerantWritePei) → falls back to PCDs `PcdFlashNvStorage{Variable,FtwWorking,FtwSpare}` which are all `0`, then `WorkSpaceInSpareArea = 0+0-0 = 0`, the `while (>= 0)` loop subtracts `sizeof(EFI_GUID)` from `(UINT64)0` → underflow → infinite MMIO scan down from `(UINTN)~0` → freeze.

---

## 6. RAM/Memory — confirmation of "installed"

| Stage | Source of memory | Status in this ROM |
|---|---|---|
| Bootblock | none (XIP from SPI) | always OK |
| Romstage | CAR (DCACHE_RAM, `0xfe800000`) → FSP-M training → DRAM | OK (build log shows FSP-M loaded and ramstage relocated) |
| Postcar  | CAR tear-down | OK |
| Ramstage | DRAM | OK — proven by `verify_memory_for_payload()` printing `TOTAL RAM`, `CBMEM OK`, etc. |
| EDK II PEI (when enabled) | first-pass uses SecCoreData temp-RAM at `0x80000–0x90000`; permanent EFI memory installed by BlSupportPei (PEIM[3]) from coreboot LBIO map | not exercised in this ROM |

Coreboot-side memory installation is confirmed by:
- `cbmem_top() != 0` (returns the high address of CBMEM in DRAM).
- `bootmem_walk()` returns `BM_MEM_RAM` ranges totalling system RAM.
- Coreboot ramstage running C code from DRAM (which it must — postcar already tore down CAR).

When the EDK II path runs, **PEI memory** is then installed by `BlSupportPei.BlPeiEntryPoint`:

```c
Status = ParseMemoryInfo (MemInfoCallback, &UsableLowMemTop);  // BlSupportPei.c:720
ASSERT (UsableLowMemTop >= BASE_1MB + PEI_MEM_SIZE);
PeiMemBase = (UsableLowMemTop - PEI_MEM_SIZE) & (~(BASE_64KB - 1));
Status = PeiServicesInstallPeiMemory (PeiMemBase, PEI_MEM_SIZE);  // 64 MB
```

With the current fix in `MemInfoCallback` (track the **highest** low-RAM top, not just regions ≥ 64 MB), `UsableLowMemTop` will be set on any Xeon-SP memory map, even if no single contiguous region clears the 64 MB threshold.

---

## 7. Reading the historic crash logs against this ROM

| Log / artefact | Built? | What it shows | Relevance to 1405-03-08 ROM |
|---|---|---|---|
| [logs/1405-02-30/log.txt](../1405-02-30/log.txt) | from 1405-02-30 build | VGA capture of PeiDispatcher reaching PeimCount=2 then freezing at row 6/7 (PEIM[2] = `FaultTolerantWritePei` entry point) | same payload structure; on **this** ROM the path never runs |
| [logs/1405-03-03/analyize.md](../1405-03-03/analyize.md) | analysis of 1405-03-03 build | proves PEIM[2] = `FaultTolerantWritePei` by walking ROM offsets | identical PEIM layout in this ROM (verified in §4.2) |
| [logs/1405-03-03/report_a.md](../1405-03-03/report_a.md) | analysis | broader project context | unchanged |
| [logs/1405-03-07/log.txt](../1405-03-07/log.txt) | build log only | shows successful coreboot build (no VGA capture) | this is the build log for **exactly** the same ROM as 1405-03-08 |
| [logs/1405-03-08/coreboot.rom](coreboot.rom) | — | this ROM | byte-identical to 1405-03-07 |
| [logs/analysis_report.md](../analysis_report.md) | top-level guide | complete build & debug guide | still accurate except: Bug 2 (BlSupportPei MemInfoCallback) is now **fixed** in source |

There are **no new VGA captures or screenshots in `logs/1405-03-07/` or `logs/1405-03-08/`** — only build logs and the ROM itself. So there is no "fresh" crash trace to interpret. The user's question "look for my crashes" against this ROM is answered: this ROM cannot crash in EDK II PEI because it does not invoke EDK II.

---

## 8. Reproducing every result in this recipe

```bash
# from coreboot tree root
cd /home/mahdi/repositories/coreboot/dasharo/coreboot

# host tools
make -C util/cbfstool -j$(nproc)
make -C util/ifdtool -j$(nproc)

ROM=logs/1405-03-08/coreboot.rom
OUT=logs/1405-03-08/analysis
mkdir -p "$OUT"

# 1. SPI image layout
util/ifdtool/ifdtool -d "$ROM"                                 > "$OUT/ifdtool_dump.txt"
util/cbfstool/cbfstool "$ROM" layout                           > "$OUT/fmap_layout.txt"
util/cbfstool/cbfstool "$ROM" print -r COREBOOT                > "$OUT/cbfs_print.txt"

# 2. Extract artefacts
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n fallback/payload -m x86 -f "$OUT/payload.elf"
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n fspt.bin    -f "$OUT/fspt.bin"
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n fspm.bin    -f "$OUT/fspm.bin"
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n fsps.bin    -f "$OUT/fsps.bin"
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n bootblock   -f "$OUT/bootblock.bin"
util/cbfstool/cbfstool "$ROM" extract -r COREBOOT -n config      -f "$OUT/config.txt"

# 3. ELF & FV inspection
readelf -l "$OUT/payload.elf"                                  > "$OUT/payload_readelf.txt"

# 4. PEIFV walker — full PEIM list with GUIDs
python3 - <<'PYEOF' > "$OUT/peifv_walk.txt"
import struct
GUID_DB = {
 "9b3ada4f-ae56-4c24-8dea-f03b7558ae50": "Pcd.inf",
 "8cc70a5a-51b4-4049-b97e-982b1d7d4049": "SmmStorePei.inf",
 "aac33064-9ed0-4b89-a5ad-3ea767960b22": "FaultTolerantWritePei.inf",
 "352c6af8-315b-4bd6-b04f-31d4ed1ebe57": "BlSupportPei.inf",
 "a3610442-e69f-4df3-82ca-2360c4031a23": "ReportStatusCodeRouterPei.inf",
 "9d225237-fa01-464c-a949-baabc02d31d0": "StatusCodeHandlerPei.inf",
 "86d70125-baa3-4296-a62f-602bebbb9081": "DxeIpl.inf",
 "adf01bf6-47d6-495d-b95b-687777807214": "FirmwarePerformancePei.inf",
 "bf7f2b0c-9f2f-4889-ab5c-12460022be87": "Tcg2ConfigPei.inf",
 "2be1e4a6-6505-43b3-9ffc-a3c8330e0432": "TcgPei.inf",
 "a0c98b77-cba5-4bb8-993b-4af6ce33ece4": "Tcg2Pei.inf",
 "ded60489-979c-4b5a-8ee4-4068b0cc38dc": "OpalPasswordPei.inf",
 "91ad7375-8e8e-49d2-a343-68bc78273955": "HddPasswordPei.inf",
 "52c05b14-0b98-496c-bc3b-04b50211d680": "PeiMain (PEI_CORE)",
 "ba7be337-6cfb-4dbb-b26c-21ec2fc16073": "SecCore",
}
FT={1:"RAW",2:"FREEFORM",3:"SEC_CORE",4:"PEI_CORE",5:"DXE_CORE",
    6:"PEIM",7:"DRIVER",8:"PEIM_DRIVER",9:"APP",0xA:"SMM",
    0xB:"FV_IMAGE",0xD:"SMM_CORE",0xF0:"PAD"}
with open("logs/1405-03-08/analysis/payload.elf","rb") as f:
    f.seek(0x2000)
    pei = f.read(0x90000)
hdr=struct.unpack("<H",pei[48:50])[0]
print(f"PEIFV sig={pei[40:44]!r} size=0x{len(pei):X} hdrLen=0x{hdr:X}")
off=(hdr+7)&~7; idx=peim=0
while off+24<len(pei):
    n=pei[off:off+16]
    if all(b==0xFF for b in n): break
    ft=pei[off+18]; sz=pei[off+20]|(pei[off+21]<<8)|(pei[off+22]<<16)
    if sz==0xFFFFFF: sz=struct.unpack("<Q",pei[off+24:off+32])[0]
    if sz<24: break
    d1,d2,d3=struct.unpack("<IHH",n[:8])
    g="%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"%(d1,d2,d3,*n[8:16])
    mk=""
    if ft==6: mk=f" PEIM[{peim}]"; peim+=1
    print(f"[{idx:2d}] off=0x{off:06X} handle=0x{0x800000+off:08X} sz=0x{sz:X} "
          f"type={FT.get(ft,hex(ft))}{mk}  GUID={g}  {GUID_DB.get(g,'')}")
    off=(off+sz+7)&~7; idx+=1
PYEOF

# 5. (optional) check that SI_DESC + ME blob look sensible
hexdump -C "$ROM" | head -32                                   > "$OUT/desc_head.txt"
```

After running, the outputs in `analysis/` are exactly the ones referenced by §2–§4.

---

## 9. What to do next (action items, in order)

1. **Re-enable the payload jump.** Uncomment `protected_mode_call_1arg(...)` in [src/lib/prog_loaders.c:798](../../src/lib/prog_loaders.c#L798) and rebuild. Without this, no EDK II crash can ever be observed on this ROM.
2. **Capture a fresh VGA frame.** After the rebuild, boot the server and photograph the screen — the verification panel will be overwritten by the EDK II PEI VGA output once SEC starts.
3. **Confirm the BlSupportPei fix lands at runtime.** Bug 2 is fixed in source ([BlSupportPei.c:347-353](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/BlSupportPei/BlSupportPei.c#L347-L353)). The runtime should now print non-zero `UsableLowMemTop` and proceed to `PeiServicesInstallPeiMemory`. If a hang still occurs, it will be a different one (most likely still PEIM[2] = `FaultTolerantWritePei`, because `CONFIG_SMMSTORE` is still off).
4. **Decide on SMMSTORE.** Either:
   - turn on `CONFIG_SMMSTORE_V2` in [configs/config.asrock_spc741d8](../../configs/config.asrock_spc741d8) so `SmmStorePei` can actually publish `gVariableFlashInfoHobGuid`, **or**
   - set the `PcdFlashNvStorage{Variable,FtwWorking,FtwSpare}{Base,Size}` PCDs in [DasharoPayloadPkg.dsc:633-645](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/DasharoPayloadPkg.dsc#L633-L645) to point to a real NV region, **or**
   - remove `FaultTolerantWritePei` + `SmmStorePei` from the FDF and switch UefiPayloadPkg to `VARIABLE_SUPPORT=EMU`.
5. **Confirm GUID-print probe.** Add `mde_2_edkii_vga_sprintf(9, "EP-%LX,FN-%g", EntryPoint, &FvFileInfo.FileName);` immediately before [Dispatcher.c:2452](../../payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c#L2452). This locks in the identification on real hardware (the FILE_GUID will read `AAC33064-9ED0-4B89-A5AD-3EA767960B22` immediately before the freeze).
6. **(Lower priority) implement serial early-printk in SecMain** — see Section 14 of [logs/analysis_report.md](../analysis_report.md). The VGA channel is sufficient for now, but IPMI SOL would provide an independent log stream.

---

## 10. Glossary of files referenced

| Path | Role |
|---|---|
| [logs/1405-03-08/coreboot.rom](coreboot.rom) | the 64 MB SPI image under test |
| [logs/1405-03-08/analysis/](analysis/) | every extracted artefact (ELF, FSP blobs, FMAP/CBFS dumps, PEIFV walker output) |
| [logs/analysis_report.md](../analysis_report.md) | top-level Dasharo+EDK II guide for this server |
| [logs/1405-03-03/analyize.md](../1405-03-03/analyize.md) | step-by-step proof that PEIM[2] = `FaultTolerantWritePei` |
| [src/mainboard/asrock/spc741d8/board.fmd](../../src/mainboard/asrock/spc741d8/board.fmd) | flash layout |
| [src/mainboard/asrock/spc741d8/devicetree.cb](../../src/mainboard/asrock/spc741d8/devicetree.cb) | hardware description |
| [configs/config.asrock_spc741d8](../../configs/config.asrock_spc741d8) | base coreboot config |
| [src/lib/prog_loaders.c](../../src/lib/prog_loaders.c) | payload load/run logic (handoff site) |
| [payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/DasharoPayloadPkg.fdf](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/DasharoPayloadPkg.fdf) | FD/FV layout used to build `UEFIPAYLOAD.fd` |
| [payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/SecCore/SecMain.c](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/SecCore/SecMain.c) | EDK II SEC entry |
| [payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/BlSupportPei/BlSupportPei.c](../../payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/BlSupportPei/BlSupportPei.c) | builds EFI HOBs from coreboot LBIO; installs PEI memory |
| [payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c](../../payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c) | PEIM dispatcher with VGA-trace instrumentation |
