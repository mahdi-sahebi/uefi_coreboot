# Session Report — PEI Dispatcher Hang Diagnosis

**Date:** 1405-03-03 (Jalali) / 2026-05-25 (Gregorian)
**Repo:** `/home/mahdi/repositories/coreboot/dasharo/coreboot`
**Branch:** `edk2-server`
**Platform:** ASRock SPC741D8 (Intel Xeon SP Sapphire Rapids), BMC AST2600
**File under investigation:** `payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c`
**Build under test:** `build/coreboot.rom` (64 MiB)

This file is a faithful record of the entire diagnostic session: the user's
questions, the analysis performed, and the conclusions reached. It is
intended to stand alone for future reviewers without requiring the original
chat transcript.

---

## Turn 1 — Initial diagnostic request

### User input

VGA output captured from `mde_2_edkii_vga_*` instrumentation embedded in
`Dispatcher.c`. The user asked: *"What is the reason for the hang and freeze?"*

```
Dis-MI:0,S:8fe94,PDDT:0,GHOB:0
TS-c:260,PDDT:80ec8,S:0
a-PI:0,PMTRFV:0,HITBM:0,PSPSB:0

PDR-,0,0,1
NCHb-8fa30,0,CVH:80D28,Fp:8161A8,CPC:0
FIc-2-0-0-8161A8-829FE8-8FCFC,6
PLId-8000000E,8fa34-829FE8-8FA00-0-800000
8c-0-1-0-1-0-0-0


Pb-80EC8
Delayedis-80EC8,0-0,989680
n-0,1,0,0




End of DelayedDispath
```

### Analysis

`Dispatcher.c` was read; each `mde_2_edkii_vga_sprintf` call was located
(via `grep` on the row-prefix strings: `Dis-MI`, `TS-c`, `a-PI`, `PDR-`,
`NCHb-`, `FIc-`, `PLId-`, `Pb-`, `8c-`, `n-`) and mapped to its line number
and surrounding code path.

Row-by-row decode:

| Row | Log | Source site | Meaning |
|---:|---|---|---|
| 0 | `Dis-MI:0,S:8fe94,PDDT:0,GHOB:0` | `Dispatcher.c:2083` | Pre-memory (`PeiMemoryInstalled=0`), `SecCoreData=0x8FE94`, no Delayed Dispatch HOB found → new HOB will be built. |
| 1 | `TS-c:260,PDDT:80ec8,S:0` | `:2134` | Delayed Dispatch table allocated at `0x80EC8` (size 0x260), PPI install + EndOfPei notify both succeeded. |
| 2 | `a-PI:0,PMTRFV:0,HITBM:0,PSPSB:0` | `:2148` | Pre-memory, no migration, BootMode=0, no S3 shadow. Shadow block at `:2155` skipped (rows 3,9,10 stay blank — consistent). |
| 4 | `PDR-,0,0,1` | `:2262` | First entry into the main do-while; 1 FV to scan. |
| 5 | `NCHb-8fa30,0,CVH:80D28,Fp:8161A8,CPC:0` | `:2285` | FV located: handle `0x80D28`, FvPpi `0x8161A8`. |
| 6 | `FIc-2-0-0-8161A8-829FE8-8FCFC,6` | `:2346` | **PeimCount=2** (third PEIM), state=NOT_DISPATCHED, GetFileInfo OK, file type 6 = `EFI_FV_FILETYPE_PEIM`. |
| 7 | `PLId-8000000E,8fa34-829FE8-8FA00-0-800000` | `:2435` | `VerifyPeim` returned `0x8000000E = EFI_NOT_FOUND` — **benign**. The check at `:2443` only blocks `EFI_SECURITY_VIOLATION`. |
| 8 | `8c-0-1-0-1-0-0-0` | `:2492` | Last successful PEIM was index **1**; PeimState[1]=DISPATCHED. |
| 11 | `Pb-80EC8` | `:2599` | Tail of PeimCount=1's iteration. |
| 12 | `Delayedis-80EC8,0-0,989680` | `:389` | Inside DelayedDispatchDispatcher; completion timeout `0x989680 µs = 10 s`. |
| 13 | `n-0,1,0,0` | `:2617` | Inner-loop snapshot after PeimCount=1. |
| 20 | `End of DelayedDispath` | `:549` | DelayedDispatchDispatcher returned cleanly. |

### Conclusion of Turn 1

Two distinct iterations were layered on the same VGA rows. The flow:

- **PeimCount=0** ran completely → rows 6..13 written.
- **PeimCount=1** ran completely → rows 6..13 overwritten.
- **PeimCount=2** began and overwrote rows 6 and 7 only:
  - `FIc-2-...` written (`:2346`)
  - `PLId-8000000E,...` written (`:2435`)
  - `PeimEntryPoint(PeimFileHandle, PeiServices)` called (`:2452`) — **never returned**.
  - Row 8 (`8a/8b/8c`) for PeimCount=2 never written.

The hang is **inside the entry point of the third PEIM**, on the CAR stack
(stack near `0x8FA00`, since `&EntryPoint = 0x8FA00`). `PLId 0x8000000E` is
**not** the cause.

---

## Turn 2 — Why isn't "End of Dispatching" printed?

### User input

The user highlighted lines 2653–2665 (the second `n-` print at row 14, the
do-while condition, and `End of Dispatching` at row 21) and asked:

> *"The condition of while is false [given n-0,1,0,0], so it must break and print 'End of Dispatching' — why didn't it happen?"*

### Analysis

There are **two** `mde_2_edkii_vga_sprintf` calls starting with `"n-"`:

| Row | Location | Fires |
|---:|---|---|
| 13 | `Dispatcher.c:2617` **inside** `for (PeimCount…)` after DelayedDispatchDispatcher | once per PeimCount iteration |
| 14 | `Dispatcher.c:2654` **after** both `for` loops, just before `while` | once per outer do-while pass |

Counting rows in the captured frame:

```
…
row 11  Pb-80EC8
row 12  Delayedis-80EC8,0-0,989680
row 13  n-0,1,0,0          ← INNER n- (line 2617)
row 14  (blank)            ← the user's selected n- never ran
…
row 20  End of DelayedDispath
row 21  (blank)            ← "End of Dispatching" never ran
```

The `n-0,1,0,0` displayed on the screen is the **inner** print (row 13),
not the post-do-while print (row 14). The user's logic was correct
— `(0 && 1) || (0 > 0) == 0` — but the inner `for (PeimCount…)` loop
**never finished**, so the `while` condition was never evaluated, and
row 14 / row 21 never ran.

### Conclusion of Turn 2

The CPU is still inside `PeimEntryPoint()` of PeimCount=2 from
`Dispatcher.c:2452`. The hang is upstream of `while`.

---

## Turn 3 — What is the third PEIM? Is that OK?

### Analysis

Searched the EDK II workspace for an FDF that declares a PEI FV. The
default `UefiPayloadPkg/UefiPayloadPkg.fdf` has no PEI FV (only PLDFV +
DXEFV). The dasharo build uses
`DasharoPayloadPkg/DasharoPayloadPkg.fdf` instead, which defines:

```
[FD.UefiPayload]
BaseAddress   = 0x800000
Size          = 0xE30000
…
0x00000000|0x090000
FV = PEIFV          ; PEIFV at FD-relative 0  → memory  0x800000..0x88FFFF
0x00090000|0xDA0000
FV = DXEFV          ; DXEFV at FD-relative 0x90000 → memory 0x890000..0xE2FFFF
```

`FvHandle = 0x800000` from the VGA log = the FD base, confirming PEIFV.

PEIMs listed in `FV.PEIFV` (in FDF order, filtered to `MODULE_TYPE = PEIM`;
SecCore is type SEC and PeiMain is PEI_CORE, neither dispatched here):

| Idx | PEIM |
|---:|---|
| 0 | `MdeModulePkg/Universal/PCD/Pei/Pcd.inf` |
| 1 | `DasharoPayloadPkg/SmmStorePei/SmmStorePei.inf` |
| **2** | **`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.inf`** |
| 3 | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.inf` |
| 4 | `ReportStatusCodeRouterPei.inf` |
| 5 | `StatusCodeHandlerPei.inf` |
| 6 | `DxeIpl.inf` |
| 7 | `FirmwarePerformancePei.inf` |

`FaultTolerantWritePei.inf` properties:
- `FILE_GUID = AAC33064-9ED0-4b89-A5AD-3EA767960B22`
- `ENTRY_POINT = PeimFaultTolerantWriteInitialize`
- `[Depex] TRUE`
- `LibraryClasses` includes `VariableFlashInfoLib`

The FDF itself contains an explicit warning above this block:

```
# Mind the relative order of these PEIMs:
#  1. SmmStorePei produces gVariableFlashInfoHobGuid
#  2. FaultTolerantWritePei consumes gVariableFlashInfoHobGuid through
#     VariableFlashInfoLib and produces gEdkiiFaultTolerantWriteGuid
#  3. BlSupportPei consumes gEdkiiFaultTolerantWriteGuid
```

### Conclusion of Turn 3

The third PEIM is **`FaultTolerantWritePei`**. It is **not** OK — this is
the hanging PEIM. Two probable root causes consistent with a pre-memory
hang on CAR stack:

1. SmmStorePei published `gVariableFlashInfoHobGuid` with wrong base/length
   for this Xeon SP server; FTW PEI then walks off into unmapped MMIO and
   the CPU stalls on the non-responding cycle.
2. SPI/NV working block is not yet readable when FTW PEI runs in
   pre-memory; FTW's first read of the working-block header hangs.

A live-confirmation probe was suggested:

```c
mde_2_edkii_vga_sprintf(9, "EP-%LX,FN-%g",
  (UINT64)EntryPoint,
  &FvFileInfo.FileName);
```

right before `Dispatcher.c:2452`. The expected GUID printout is
`AAC33064-9ED0-4B89-A5AD-3EA767960B22`.

---

## Turn 4 — Confirm against the ROM, and what's wrong with the image?

### Procedure

```
$ build/cbfstool build/coreboot.rom print
…
fallback/payload               0x80       simple elf    1067545 none
…
$ build/cbfstool build/coreboot.rom extract \
      -n fallback/payload -f /tmp/payload.elf -m x86
$ file /tmp/payload.elf
/tmp/payload.elf: ELF 32-bit LSB executable, Intel 80386, version 1 (SYSV), statically linked, stripped
$ readelf -l /tmp/payload.elf
Entry point 0x802580
LOAD  0x002000 0x00800000 0x00800000 0xe30000 0xe30000 RWE
```

Single LOAD segment maps **0x00800000 .. 0xE30000** — matches the FDF
`[FD.UefiPayload]` exactly.

A Python script (reproduced in §7 of `analyize.md`) walks the `_FVH`-headed
PEIFV (offset 0x2000 in the ELF) and prints every FFS file. Output
(abridged to PEIMs only):

```
PEIM idx  off-in-PEIFV   PeimFileHandle  size      FILE_GUID                              Module
[0]       0x018FE8       0x818FE8        0x903A    9B3ADA4F-AE56-4C24-8DEA-F03B7558AE50   PCD/Pei/Pcd.inf
[1]       0x022FE8       0x822FE8        0x6042    8CC70A5A-51B4-4049-B97E-982B1D7D4049   SmmStorePei
[2]       0x029FE8       0x829FE8        0x7056    AAC33064-9ED0-4B89-A5AD-3EA767960B22   FaultTolerantWritePei   ← matches log
[3]       0x031FE8       0x831FE8        0x9046    352C6AF8-315B-4BD6-B04F-31D4ED1EBE57   BlSupportPei
[4]       0x03BFE8       0x83BFE8        0x605E    A3610442-E69F-4DF3-82CA-2360C4031A23   ReportStatusCodeRouterPei
[5]       0x042FE8       0x842FE8        0x6056    9D225237-FA01-464C-A949-BAABC02D31D0   StatusCodeHandlerPei
[6]       0x049FE8       0x849FE8        0xA03A    86D70125-BAA3-4296-A62F-602BEBBB9081   DxeIpl
[7]       0x054FE8       0x854FE8        0x605A    ADF01BF6-47D6-495D-B95B-687777807214   FirmwarePerformancePei
[8]       0x05BFE8       0x85BFE8        0x7046    BF7F2B0C-9F2F-4889-AB5C-12460022BE87   Tcg2ConfigPei
[9]       0x063FE8       0x863FE8        0xA03A    2BE1E4A6-6505-43B3-9FFC-A3C8330E0432   TcgPei
[10]      0x06EFE8       0x86EFE8        0x1203A   A0C98B77-CBA5-4BB8-993B-4AF6CE33ECE4   Tcg2Pei
[11]      0x081FE8       0x881FE8        0x604A    DED60489-979C-4B5A-8EE4-4068B0CC38DC   OpalPasswordPei
[12]      0x088FE8       0x888FE8        0x604A    91AD7375-8E8E-49D2-A343-68BC78273955   HddPasswordPei
```

Cross-check that nails the identification:

```
PEIFV base + PEIM[2] FFS offset
0x800000   + 0x029FE8                = 0x829FE8   == PeimFileHandle in log  ✓
```

### What's wrong with this image

1. **Direct cause:** `FaultTolerantWritePei` is the third PEIM dispatched
   and freezes on entry — confirmed against both the VGA trace and the
   on-disk ROM image.
2. **Build pulls in a full PEI security stack** (13 PEIMs including TPM,
   Opal, HDD-password) on a *payload* that runs *after* coreboot's FSP-M.
   `TPM_ENABLE` and friends are evidently `TRUE`.
3. **No PEI Apriori** file is built into PEIFV; dispatch order is the
   literal FDF order. The SmmStore → FTW → BlSupport ordering is guarded
   only by an FDF comment, not by an Apriori list.
4. **FTW's first action consumes `gVariableFlashInfoHobGuid`** (produced
   by SmmStorePei, which did dispatch successfully) and then touches
   SPI/NV working-block flash storage from CAR stack pre-memory. Either
   the HOB content is wrong for this Xeon SP platform's SmmStore region,
   or the working-block flash window is not yet readable at that point in
   the boot.

### Things ruled out

- `VerifyPeim` returning `EFI_NOT_FOUND` (`0x8000000E`) — expected when no
  Security-Arch PPI exists; dispatcher allows it by design.
- `PeiLoadImage` — succeeded (control reached `:2435`).
- `DepexSatisfied` — returned TRUE (control took the `else` at `:2334`).
- PEI Core dispatcher itself — flowing correctly; PeimCount=0 and 1 both
  completed, and `DelayedDispatchDispatcher` ran cleanly.

---

## Turn 5 — Comprehensive document written

User asked for a full report; produced
[`logs/1405-03-03/analyize.md`](analyize.md) covering all of §1–§6 above
plus reproduction artifacts (cbfstool / readelf / FV-walker Python) and a
prioritized action-item list.

---

## Turn 6 — Memory file saved to logs

The `project_boot_debug.md` memory file was copied to
[`logs/1405-03-03/project_boot_debug.md`](project_boot_debug.md). It was
also updated in the memory store to record:

- Confirmed culprit: PEIM[2] = `FaultTolerantWritePei` (GUID
  `AAC33064-9ED0-4B89-A5AD-3EA767960B22`, `PeimFileHandle=0x829FE8`).
- ROM-side verification (PEIFV base + offset arithmetic).
- New bug #5 covering the FaultTolerantWritePei hang and instrumentation
  next step.

---

## Turn 7 — This file

Saved the full session transcript to `logs/1405-03-03/report_a.md` (this
document).

---

## Final conclusions

1. **Hang location:** Inside the entry point of `FaultTolerantWritePei`
   (third PEIM dispatched from PEIFV), called from `Dispatcher.c:2452`.
   Pre-memory, CAR stack near `0x8FA00`.

2. **Identification confidence: high.** Two independent lines of evidence:
   - VGA trace shows PeimCount=2 wrote `FIc` and `PLId` but never `8a/8b/8c`,
     so control never returned from `PeimEntryPoint(...)`.
   - ROM inspection: `PEIFV base (0x800000) + PEIM[2] FFS offset (0x29FE8) =
     0x829FE8`, exactly matching the `PeimFileHandle` value printed at row 7.

3. **What is wrong with the image:** The build dispatches
   `FaultTolerantWritePei` while still in pre-memory (CAR), with
   `SmmStorePei`-produced `gVariableFlashInfoHobGuid` as its only handle
   on the NV-flash region. On this Xeon SP platform, either that HOB
   describes the wrong region or the working-block is not yet readable —
   FTW PEI's first flash access stalls the CPU.

4. **Recommended next steps:**
   1. Add `EP-/FN-` probe at `Dispatcher.c:2452` to confirm the GUID live.
   2. Instrument `PeimFaultTolerantWriteInitialize` and its first
      `VariableFlashInfoLib` call.
   3. Validate the HOB published by `SmmStorePei` against coreboot's
      SmmStore region for this server.
   4. Consider gating `FaultTolerantWritePei` and the SPI-variable stack
      behind a feature flag until the NV region is verified functional
      pre-memory.
   5. Optionally add a PEI Apriori list so the SmmStore → FTW →
      BlSupport ordering is enforced by build rather than by FDF
      comments.

---

## Companion files in this directory

- `analyize.md` — full structured analysis report.
- `project_boot_debug.md` — point-in-time snapshot of the project memory.
- `report_a.md` — this session transcript.
- `1405-03-03.jpeg` — VGA screenshot at freeze (source of the captured log text).
- `log.txt` — raw text capture of the VGA output.
- `coreboot_mn_1405-03-03_loop.rom` — the 64 MiB ROM image inspected in §4.
- `coreboot_mn_1405-03-03_loop.zip` — packaged build artefacts.
- `UEFIPAYLOAD.fd` — the EDK II Firmware Device image (same content as the
  ELF payload's LOAD segment).
