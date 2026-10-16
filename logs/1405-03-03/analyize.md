# PEI Dispatcher Hang Analysis — coreboot + EDK II Payload (Xeon SP server)

**Date:** 1405-03-03 (Jalali) / 2026-05-25 (Gregorian)
**Branch:** `edk2-server`
**Target file:** `payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c`
**ROM under test:** `build/coreboot.rom` (64 MiB)
**Symptom:** System freezes during first pass of `PeiDispatcher`; no further debug or VGA output beyond a known set of `mde_2_edkii_vga_*` traces.

---

## 1. Background

The Dasharo coreboot + EDK II Payload firmware for a Xeon SP server hangs early in the
PEI phase. To localize the failure, custom VGA-text-mode logging APIs
(`mde_2_edkii_vga_print` / `mde_2_edkii_vga_sprintf`) were embedded in
`MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c`. Each log writes to a fixed VGA row
(0–24) so the last value on each row reflects the latest invocation; earlier values
are overwritten on subsequent iterations.

The captured VGA frame at the moment of the freeze:

```
row  0  Dis-MI:0,S:8fe94,PDDT:0,GHOB:0
row  1  TS-c:260,PDDT:80ec8,S:0
row  2  a-PI:0,PMTRFV:0,HITBM:0,PSPSB:0
row  3  (blank)
row  4  PDR-,0,0,1
row  5  NCHb-8fa30,0,CVH:80D28,Fp:8161A8,CPC:0
row  6  FIc-2-0-0-8161A8-829FE8-8FCFC,6
row  7  PLId-8000000E,8fa34-829FE8-8FA00-0-800000
row  8  8c-0-1-0-1-0-0-0
row  9  (blank)
row 10  (blank)
row 11  Pb-80EC8
row 12  Delayedis-80EC8,0-0,989680
row 13  n-0,1,0,0
row 14  (blank)
row 15..19  (blank)
row 20  End of DelayedDispath
row 21  (blank)
```

---

## 2. Decoding each row against the source

| Row | Log | Source site | Meaning |
|---:|---|---|---|
| 0 | `Dis-MI:0,S:8fe94,PDDT:0,GHOB:0` | `Dispatcher.c:2083` | `PeiMemoryInstalled=0` (pre-memory), `SecCoreData=0x8FE94`, no `DelayedDispatchTable`, no HOB found → build a new HOB. |
| 1 | `TS-c:260,PDDT:80ec8,S:0` | `Dispatcher.c:2134` | Built `DELAYED_DISPATCH_TABLE` (0x260 bytes) at `0x80EC8`; `InstallPpi`+`NotifyPpi` returned `EFI_SUCCESS`. |
| 2 | `a-PI:0,PMTRFV:0,HITBM:0,PSPSB:0` | `Dispatcher.c:2148` | `PeiMemoryInstalled=0`, `PcdMigrateTemporaryRamFirmwareVolumes=0`, `BootMode=0` (`BOOT_WITH_FULL_CONFIGURATION`), `PcdShadowPeimOnS3Boot=0`. Shadow loop at `:2155` is skipped → rows 3,9,10 stay blank ✓. |
| 4 | `PDR-,0,0,1` | `Dispatcher.c:2262` | First entry into the main do-while; only 1 FV to scan. |
| 5 | `NCHb-8fa30,0,CVH:80D28,Fp:8161A8,CPC:0` | `Dispatcher.c:2285` | `Private=0x8FA30`, `FvCount=0`, `CoreFvHandle=0x80D28`, `FvPpi=0x8161A8`, `CurrentPeimCount=0`. |
| 6 | `FIc-2-0-0-8161A8-829FE8-8FCFC,6` | `Dispatcher.c:2346` | `PeimCount=2` (third PEIM), `PeimState=0` (NOT_DISPATCHED), `GetFileInfo` OK, `FileType=6` = `EFI_FV_FILETYPE_PEIM`. |
| 7 | `PLId-8000000E,8fa34-829FE8-8FA00-0-800000` | `Dispatcher.c:2435` | `VerifyPeim` returned `0x8000000E = EFI_NOT_FOUND`. **Benign** — no Security Architecture PPI; the check at `:2443` only blocks on `EFI_SECURITY_VIOLATION`. |
| 8 | `8c-0-1-0-1-0-0-0` | `Dispatcher.c:2492` | **Last successful PEIM was index 1**. `PeiMemoryInstalled=0`, `PeimCount=1`, `FvCount=0`, `PeimState[1]=1` (DISPATCHED). |
| 11 | `Pb-80EC8` | `Dispatcher.c:2599` | Tail of PeimCount=1's iteration: DelayedDispatch entered. |
| 12 | `Delayedis-80EC8,0-0,989680` | `Dispatcher.c:389` | DelayedDispatchDispatcher: table at `0x80EC8`, completion timeout = `0x989680 µs = 10 s`. |
| 13 | `n-0,1,0,0` | `Dispatcher.c:2617` | Inner-loop snapshot after PeimCount=1: `NeedingDispatch=0`, `DispatchOnThisPass=1`, table `Count=0`, `g_cnt=0`. |
| 20 | `End of DelayedDispath` | `Dispatcher.c:549` | DelayedDispatchDispatcher returned cleanly. |

Rows that **should** be written but are blank:
- row 14 (`n-` after the do-while body, `Dispatcher.c:2654`)
- row 21 (`End of Dispatching`, `Dispatcher.c:2665`)

---

## 3. The two `n-` prints — point of confusion clarified

There are **two** different `mde_2_edkii_vga_sprintf` calls whose format starts with `"n-"`:

| Row | Location | Fires when |
|---:|---|---|
| 13 | `Dispatcher.c:2617`, **inside** `for (PeimCount…)` after `DelayedDispatchDispatcher` | once per PeimCount iteration |
| 14 | `Dispatcher.c:2654`, **after** both `for` loops, immediately before `while` evaluates | once per outer do-while pass |

The captured `n-0,1,0,0` is on **row 13** (inner-loop print), so it is the
tail of the last *successful* PeimCount=1 iteration — not the post-loop snapshot.

The post-loop `n-` (row 14) **never executed**, so the `while` condition
was **never evaluated**, and `"End of Dispatching"` (row 21) was never reached.
If row 14 *had* printed those values, `(0 && 1) || (0 > 0) == 0`, the loop
would exit and row 21 would print — the user's logic was correct, but
execution never reached that point.

---

## 4. Where execution is actually stuck

The PEIM at index 2 (third iteration of `for (PeimCount…)`) entered and overwrote
**rows 6 and 7 only**:

1. `FIc-2-0-0-8161A8-829FE8-8FCFC,6` written (`Dispatcher.c:2346`).
2. `PeiLoadImage` succeeded (entry-point loaded; otherwise row 7 wouldn't be reached).
3. `VerifyPeim` returned `EFI_NOT_FOUND` and `PLId-8000000E,...` written
   (`Dispatcher.c:2435`).
4. Code branched into `if (Status != EFI_SECURITY_VIOLATION)` (`:2443`) and called:
   ```c
   PeimEntryPoint (PeimFileHandle, (const EFI_PEI_SERVICES **)PeiServices);
   ```
   at **`Dispatcher.c:2452`**.
5. **Control never returned** from this call. Rows 8 (`8a/8b/8c`), 11–13
   for PeimCount=2 were never overwritten — they still display PeimCount=1's
   values.

This pinpoints the freeze: **inside the entry point of the third PEIM, before any
permanent memory is installed (still on the CAR/temporary-RAM stack near
`0x8FA00`).** The `&EntryPoint=0x8FA00` log argument is the address of the
local variable; the actual function address is the value at that location.

The `0x8000000E` return from `VerifyPeim` is **not** the bug — it is the
expected outcome when no Security Architecture PPI exists yet to authenticate
the image; dispatch is deliberately allowed to proceed.

---

## 5. ROM image verification — identifying the third PEIM

To confirm which PEIM is at `PeimFileHandle = 0x829FE8`, the on-disk image
`build/coreboot.rom` was inspected directly.

### 5.1 CBFS layout

```
$ build/cbfstool build/coreboot.rom print
FMAP REGION: COREBOOT
Name                           Offset     Type           Size   Comp
cbfs_master_header             0x0        cbfs header        32 none
fallback/payload               0x80       simple elf    1067545 none
cpu_microcode_blob.bin         0x104b00   microcode     1782784 none
intel_fit                      0x2b7f80   intel_fit          80 none
fallback/romstage              0x2b8000   stage           49160 none
fallback/ramstage              0x2c40c0   stage          125826 LZMA
…
fallback/postcar               0x64ec80   stage           30128 none
fspt.bin                       0xedf780   fsp             32768 none
bootblock                      0xef87c0   bootblock       28672 none
```

The EDK II payload is `fallback/payload` (simple ELF, IA-32).

### 5.2 Payload ELF

```
$ readelf -l /tmp/payload.elf
Entry point 0x802580
LOAD  0x002000 0x00800000 0x00800000 0xe30000 0xe30000 RWE
```

Single load segment: **base 0x00800000**, size **0xE30000** — exactly
`[FD.UefiPayload]` in `DasharoPayloadPkg/DasharoPayloadPkg.fdf:12-17`.

The FD layout (FDF lines 19–23):

| Range (memory) | Region |
|---|---|
| `0x800000 – 0x88FFFF` (size `0x90000`) | **FV.PEIFV** |
| `0x890000 – 0xE2FFFF` (size `0xDA0000`) | FV.DXEFV |

### 5.3 Walking PEIFV — FFS file table

Parsed `_FVH` header (`hdr_len=0x48`) and FFS chain (script in §7).
PEIM-type files (FFS type 0x06) listed in dispatch order:

| PEIM idx | FFS offset (in PEIFV) | Computed `PeimFileHandle` | Size | FILE_GUID | Module |
|---:|---|---|---|---|---|
| 0 | `0x018FE8` | `0x818FE8` | `0x903A` | `9B3ADA4F-AE56-4C24-8DEA-F03B7558AE50` | `MdeModulePkg/Universal/PCD/Pei/Pcd.inf` |
| 1 | `0x022FE8` | `0x822FE8` | `0x6042` | `8CC70A5A-51B4-4049-B97E-982B1D7D4049` | `DasharoPayloadPkg/SmmStorePei/SmmStorePei.inf` |
| **2** | **`0x029FE8`** | **`0x829FE8`** | **`0x7056`** | **`AAC33064-9ED0-4B89-A5AD-3EA767960B22`** | **`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.inf`** |
| 3 | `0x031FE8` | `0x831FE8` | `0x9046` | `352C6AF8-315B-4BD6-B04F-31D4ED1EBE57` | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.inf` |
| 4 | `0x03BFE8` | `0x83BFE8` | `0x605E` | `A3610442-E69F-4DF3-82CA-2360C4031A23` | `MdeModulePkg/Universal/ReportStatusCodeRouter/Pei/ReportStatusCodeRouterPei.inf` |
| 5 | `0x042FE8` | `0x842FE8` | `0x6056` | `9D225237-FA01-464C-A949-BAABC02D31D0` | `MdeModulePkg/Universal/StatusCodeHandler/Pei/StatusCodeHandlerPei.inf` |
| 6 | `0x049FE8` | `0x849FE8` | `0xA03A` | `86D70125-BAA3-4296-A62F-602BEBBB9081` | `MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf` |
| 7 | `0x054FE8` | `0x854FE8` | `0x605A` | `ADF01BF6-47D6-495D-B95B-687777807214` | `MdeModulePkg/Universal/Acpi/FirmwarePerformanceDataTablePei/FirmwarePerformancePei.inf` |
| 8 | `0x05BFE8` | `0x85BFE8` | `0x7046` | `BF7F2B0C-9F2F-4889-AB5C-12460022BE87` | `DasharoPayloadPkg/Tcg/Tcg2Config/Tcg2ConfigPei.inf` |
| 9 | `0x063FE8` | `0x863FE8` | `0xA03A` | `2BE1E4A6-6505-43B3-9FFC-A3C8330E0432` | `SecurityPkg/Tcg/TcgPei/TcgPei.inf` |
| 10 | `0x06EFE8` | `0x86EFE8` | `0x1203A` | `A0C98B77-CBA5-4BB8-993B-4AF6CE33ECE4` | `SecurityPkg/Tcg/Tcg2Pei/Tcg2Pei.inf` |
| 11 | `0x081FE8` | `0x881FE8` | `0x604A` | `DED60489-979C-4B5A-8EE4-4068B0CC38DC` | `SecurityPkg/Tcg/Opal/OpalPassword/OpalPasswordPei.inf` |
| 12 | `0x088FE8` | `0x888FE8` | `0x604A` | `91AD7375-8E8E-49D2-A343-68BC78273955` | `SecurityPkg/HddPassword/HddPasswordPei.inf` |

### 5.4 Cross-check

```
PeimFileHandle (logged) = 0x829FE8
PEIFV base              = 0x800000
PEIM[2] FFS offset      = 0x029FE8
0x800000 + 0x029FE8     = 0x829FE8   ← MATCH
```

**The third PEIM is `FaultTolerantWritePei` — confirmed against the ROM.**

Note: the build contains 13 PEIMs (the basic FDF lists only 8 because the
TPM/Opal/HDD-password blocks are gated by `!if $(TPM_ENABLE)` and similar
flags — those flags are evidently `TRUE` in this build).

---

## 6. What is wrong with this image

### 6.1 Direct cause of the freeze

`FaultTolerantWritePei` (entry `PeimFaultTolerantWriteInitialize`,
`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.inf`,
`Depex = TRUE`) is the third PEIM. On entry it:

1. Looks up the `gVariableFlashInfoHobGuid` HOB published by `SmmStorePei`
   (the prior PEIM, which dispatched OK — `8c-...-1-0-1-...`) via
   `VariableFlashInfoLib`.
2. Reads the SPI/NV **working-block** header from flash.
3. Publishes `gEdkiiFaultTolerantWriteGuid`.

The FDF itself warns about the ordering dependency
(`DasharoPayloadPkg/DasharoPayloadPkg.fdf:50–55`):

```
# Mind the relative order of these PEIMs:
#  1. SmmStorePei produces gVariableFlashInfoHobGuid
#  2. FaultTolerantWritePei consumes gVariableFlashInfoHobGuid through
#     VariableFlashInfoLib and produces gEdkiiFaultTolerantWriteGuid
#  3. BlSupportPei consumes gEdkiiFaultTolerantWriteGuid
```

The hang is *inside* this PEIM's entry, before any permanent memory has been
installed (`PeiMemoryInstalled = 0`). Highest-probability root causes:

- **`gVariableFlashInfoHobGuid` content is wrong on this server platform.**
  `SmmStorePei` produced *a* HOB, but its base/length values may not describe
  the actual NV region exposed by coreboot's SmmStore implementation on Xeon SP.
  FTW PEI then walks off into unmapped MMIO and the CPU stalls on the
  non-responding cycle.
- **The NV/SPI working block is not yet readable** at the moment FTW PEI runs
  on this server (SPI controller, BIOS-region decode, or coreboot's
  `SMMSTORE_V2` mapping not yet configured for the payload context).

### 6.2 Why the build looks suspicious for a server target

- **UefiPayload is a payload, but a *full* PEI phase is being built** —
  13 PEIMs including the entire SecurityPkg TPM/Opal/HddPassword stack. On a
  payload that runs *after* coreboot (which has already done memory init via
  FSP-M / MRC), the FTW + SmmStore + Variable PEI chain assumes a flash-storage
  layout that matches what coreboot's SmmStore advertises.
- **No PEI Apriori** file is present in the FDF, so the dispatch order is the
  literal FDF order. The comment block above the three sensitive PEIMs is the
  only thing preventing accidental reorder — there is no PEI APRIORI enforcing
  it at build time.
- The previously-known crash from the project memory (PeiDispatcher first
  pass) is therefore not random: the very first dependency-chain transition
  (SmmStorePei → FaultTolerantWritePei) is where the platform-specific
  assumption breaks.

### 6.3 Things the logs prove are **not** the cause

- `VerifyPeim` returning `EFI_NOT_FOUND` (the `0x8000000E` on row 7) — this is
  expected when no Security-Arch PPI exists; dispatch proceeds by design.
- `PeiLoadImage` — succeeded (control reached `:2435`).
- `DepexSatisfied` — returned TRUE for PeimCount=2 (control took the
  `else` branch at `:2334`, hence the `FIb`/`FIc` writes).
- The PEI Core dispatcher itself — flowing correctly; the very-first iteration
  (PeimCount=0) and the second (PeimCount=1) both completed and ran
  `DelayedDispatchDispatcher` cleanly (rows 11, 12, 13, 20 all written).

---

## 7. Reproduction & verification artifacts

### 7.1 Tools used

- `build/cbfstool ... print` — CBFS inventory.
- `build/cbfstool ... extract -n fallback/payload -m x86 -f /tmp/payload.elf` — extract the payload ELF.
- `readelf -l` — confirm the LOAD segment maps `0x00800000–0xE30000`.
- A Python script that parses the FV header and walks FFS files.

### 7.2 FV walker (Python, abridged)

```python
import struct
with open('/tmp/payload.elf','rb') as f:
    f.seek(0x2000)            # ELF LOAD offset → vaddr 0x800000
    fv = f.read(0x90000)      # PEIFV size from FDF

hdr_len = struct.unpack('<H', fv[48:50])[0]    # 0x48
off = (hdr_len + 7) & ~7
peim_idx = 0
while off + 24 < len(fv):
    name  = fv[off:off+16]
    ftype = fv[off+18]
    size  = fv[off+20] | (fv[off+21]<<8) | (fv[off+22]<<16)
    if size < 24 and name == b'\xff'*16: break
    if ftype == 0x06:
        print(f"PEIM[{peim_idx}] off=0x{off:06X} fileHandle=0x{0x800000+off:X}")
        peim_idx += 1
    off = (off + size + 7) & ~7
```

The FFS-offset → file-handle equation that confirms the identification:

```
PeimFileHandle = PEIFV_base + FFS_offset
0x829FE8       = 0x800000   + 0x029FE8
```

### 7.3 Suggested next instrumentation

To watch the freeze happen on the actual third PEIM (and prove it on the
running hardware as well as in the image), add one print **before**
`Dispatcher.c:2452`:

```c
mde_2_edkii_vga_sprintf(9, "EP-%LX,FN-%g",
  (UINT64)EntryPoint,
  &FvFileInfo.FileName);
```

`FileName` for the freeze candidate is
`AAC33064-9ED0-4B89-A5AD-3EA767960B22`. Once confirmed live, instrument
`PeimFaultTolerantWriteInitialize` (its first call is
`GetVariableFlashFtwSpareInfo()` from `VariableFlashInfoLib`) to determine
whether the freeze is in the HOB lookup or in the SPI read of the working
block header.

---

## 8. Conclusions

1. **Where the system is stuck:** Inside the entry point of the third PEIM
   dispatched from PEIFV — confirmed both by VGA-trace analysis and by direct
   inspection of `build/coreboot.rom`.
2. **Which PEIM:** `FaultTolerantWritePei`
   (FILE_GUID `AAC33064-9ED0-4B89-A5AD-3EA767960B22`,
    entry `PeimFaultTolerantWriteInitialize`,
    `PeimFileHandle = 0x829FE8`).
3. **Why dispatcher state suggests a hang (not a panic):**
   The post-iteration `n-` print on row 14 and the `End of Dispatching`
   print on row 21 never executed; CPU is still inside `PeimEntryPoint(...)`.
   `VerifyPeim`'s `EFI_NOT_FOUND` (row 7) is **not** the cause.
4. **What the image likely gets wrong on this server:** Either the HOB
   produced by `SmmStorePei` describes an incorrect NV-flash region on this
   Xeon SP platform, or the SPI/NV working block is not yet readable at the
   moment FTW PEI runs from CAR-stack memory. The build has no PEI Apriori
   ordering protection; only an FDF comment guards the
   SmmStore → FTW → BlSupport dependency chain.
5. **Action items** (in priority order):
   - Add the GUID-print probe at `Dispatcher.c:2452` to lock in the
     identification on real hardware.
   - Instrument `PeimFaultTolerantWriteInitialize` entry and its first
     `VariableFlashInfoLib` call.
   - Validate the HOB published by `SmmStorePei` against the actual SmmStore
     region exposed by coreboot for this server (`cbfstool layout` /
     `cbmem -C`).
   - Consider gating `FaultTolerantWritePei` and the SPI-variable stack
     behind a server-specific feature flag until the NV region is verified
     functional pre-memory.
