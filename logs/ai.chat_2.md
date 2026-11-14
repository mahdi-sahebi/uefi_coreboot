# Session Chat Log 2 — Bug 2 Fix: BlSupportPei UsableLowMemTop Crash
**Date:** 2026-05-19 (Session 2, continuation of Session 1)
**Branch:** `edk2-server`
**User:** Mahdi Sahebi
**Platform:** ASRock SPC741D8-2L2T/BCM, Intel Xeon SP Sapphire Rapids, DDR5, ASPEED AST2600

---

## Context (from Session 1)

- Branch `edk2-server` created from tag `asrock_spc741d8_v0.9.0`
- EDK II payload configured and built; ROM verified
- Root crash identified: `BlSupportPei.c` `MemInfoCallback` — `UsableLowMemTop` stays 0 when all low-RAM regions are < 64 MB → `PeiMemBase = 0 - 64MB = 0xFFFFFFFFFC000000` → PeiCore triple-fault
- VGA stopped at: `[1]u-Sec:8FE94, PD:8FA30,BM:0,PMI:0` (PeiMain.c:882, just before `PeiDispatcher()`)
- Bug 1 (VGA array index) already fixed in `a69960a18a`

---

## This Session

### Task: Apply Bug 2 Fix, Rebuild, Commit

---

### Step 1: Commit `logs/chat.ai.md` (from Session 1)

```bash
git add -f logs/chat.ai.md
git commit -m "edk2-server: add session chat log"
# [edk2-server f2908b05ac]
```

---

### Step 2: Locate and Fix `BlSupportPei.c`

**File:** `payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/BlSupportPei/BlSupportPei.c`

**Bug location:** lines 347–354 — `MemInfoCallback()`:

```c
// BEFORE (broken):
if (Base < BASE_4GB) {
    //
    // Skip regions smaller than PEI_MEM_SIZE, the highest non-reserved
    // region may be too small.
    //
    if (Size >= PEI_MEM_SIZE) {
        *UsableLowMemTop = Base + Size;
    }
}
```

**Why it crashes on Xeon SP:**  
The Xeon SP DDR5 LBIO table presents low memory (< 4 GB) as many small fragments, none of which individually reach 64 MB (`PEI_MEM_SIZE`). With no region passing the size guard, `UsableLowMemTop` remains 0. In RELEASE builds `ASSERT` is a no-op, so execution continues. Then:

```
PeiMemBase = (UsableLowMemTop - PEI_MEM_SIZE) & ~(BASE_64KB - 1)
           = (0 - 0x4000000)
           = 0xFFFFFFFFFC000000   ← UINT64 underflow
```

`PeiServicesInstallPeiMemory(0xFFFFFFFFFC000000, 64MB)` causes PeiCore to triple-fault silently.

**Fix applied:**

```c
// AFTER (fixed):
if (Base < BASE_4GB) {
    // Track the highest low-RAM region top regardless of individual size.
    // Skipping regions < PEI_MEM_SIZE caused UsableLowMemTop=0 on Xeon SP
    // when low RAM is fragmented into many sub-64MB pieces, leading to
    // PeiMemBase underflow (0 - 64MB = 0xFFFFFFFFFC000000) and PeiCore crash.
    if ((Base + Size) > *UsableLowMemTop) {
        *UsableLowMemTop = Base + Size;
    }
}
```

---

### Step 3: Commit Fix in EDK II Submodule

```bash
# Inside payloads/external/edk2/workspace/dasharo (detached HEAD from asrock branch)
git add DasharoPayloadPkg/BlSupportPei/BlSupportPei.c
git commit -m "DasharoPayloadPkg/BlSupportPei: fix UsableLowMemTop underflow on Xeon SP"
# [detached HEAD c8014c2708]

# Move asrock branch pointer to new commit:
git branch -f asrock HEAD
git checkout asrock
# Switched to branch 'asrock'

git log --oneline -3
# c8014c2708  DasharoPayloadPkg/BlSupportPei: fix UsableLowMemTop underflow on Xeon SP
# 49aeddcf36  Logo.bmp is modified
# a69960a18a  Wrong print is fixed
```

---

### Step 4: Update Coreboot Root Submodule Reference

```bash
git -C /home/mahdi/repositories/coreboot/dasharo/coreboot add -f \
    payloads/external/edk2/workspace/dasharo
git -C /home/mahdi/repositories/coreboot/dasharo/coreboot commit \
    -m "edk2-server: update EDK II submodule — fix BlSupportPei UsableLowMemTop crash"
# [edk2-server a96218e382]
```

`CONFIG_EDK2_TAG_OR_REV="asrock"` already resolves to the `asrock` branch — no `.config` change needed.

---

### Step 5: Rebuild Coreboot

```bash
make -j$(nproc)
```

**Output (tail):**
```
fallback/payload               0x80       simple elf    1161095 none
cpu_microcode_blob.bin         0x11b880   microcode     1782784 none
intel_fit                      0x2ced00   intel_fit          80 none
fallback/romstage              0x2ced80   stage           49160 none
fallback/ramstage              0x2dae40   stage          142120 LZMA
config                         0x2fda00   raw              4337 LZMA
revision                       0x2feb80   raw               871 none
build_info                     0x2fef40   raw               123 none
fallback/dsdt.aml              0x2ff040   raw             18414 none
fspm.bin                       0x304780   fsp           3375104 none
fsps.bin                       0x63c800   fsp            185328 LZ4
fallback/postcar               0x669c80   stage           30128 none
fspt.bin                       0xe67780   fsp             32768 none
bootblock                      0xe807c0   bootblock       28672 none

Built asrock/spc741d8 (SPC741D8-2L2T/BCM)
```

---

### Step 6: Verify New ROM

```bash
ls -lh build/coreboot.rom
# -rw-rw-r-- 1 mahdi mahdi 64M May 19 15:06 build/coreboot.rom

build/cbfstool build/coreboot.rom extract -r COREBOOT \
    -n fallback/payload -f /tmp/payload_new.elf -m x86
file /tmp/payload_new.elf
# ELF 32-bit LSB executable, Intel 80386, statically linked, stripped
ls -lh /tmp/payload_new.elf
# 15M
```

EDK II payload present and valid. ✓

---

### Step 7: Final Commit State

```
git log --oneline
28276f264d  edk2-server: update chat log — Bug 2 fixed, ROM rebuilt
a96218e382  edk2-server: update EDK II submodule — fix BlSupportPei UsableLowMemTop crash
f2908b05ac  edk2-server: add session chat log
155b8f472f  Git ignore is updated
0879a25b06  Logo.bmp is modified
ef53f8fb9f  ai
64175743c4  edk2-server: track EDK II workspace and update gitignore
9eac4a3350  WIP
cee8ddf396  configs/config.asrock_spc741d8: bump to non-rc  ← tag asrock_spc741d8_v0.9.0
```

---

## Full Bug Status

| # | File | Bug | Status |
|---|---|---|---|
| 1 | `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c:43-45` | `p[1]/string[1]` → `p[i]/string[i]` | **FIXED** — commit `a69960a18a` |
| 2 | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c:347` | `MemInfoCallback` size guard → `UsableLowMemTop=0` → underflow | **FIXED** — commit `c8014c2708` |
| 3 | `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` | No PEIM GUID VGA print before dispatch | Pending (diagnostic) |
| 4 | `DasharoPayloadPkg/SecCore/SecMain.c` | `early_puts` serial not implemented | Pending |

---

## Next Steps

1. Flash `build/coreboot.rom` to hardware
2. Watch VGA — expect output to progress past `[1]u-Sec:8FE94, PD:8FA30`
3. If new crash: add `BlSupportPei` VGA dump — print `UsableLowMemTop` / `PeiMemBase` before `PeiServicesInstallPeiMemory`
4. If PEIM crash: add PEIM GUID print in `Dispatcher.c`
5. Add `early_puts` in `SecMain.c` for serial via `ipmitool -I lanplus sol activate`
6. Target: PeiCore second pass → DXE → UEFI Shell

---

*Generated 2026-05-19, branch edk2-server, session 2*
