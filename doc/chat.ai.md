# Session Chat Log — Coreboot + EDK II Build & Debug
**Date:** 2026-05-19  
**Branch:** `edk2-server` (from `asrock_spc741d8_v0.9.0`)  
**User:** Mahdi Sahebi  
**Platform:** ASRock SPC741D8-2L2T/BCM, Intel Xeon SP Sapphire Rapids, DDR5, ASPEED AST2600

---

## Summary

This session had two phases:
1. **Previous session (continued):** Deep analysis of the coreboot+EDK II boot failure, writing `logs/analysis_report.md`, fixing VGA write bug.
2. **This session:** Updating the analysis report comprehensively, creating branch `edk2-server` from the release tag, configuring EDK II payload, tracking the EDK II workspace as a git submodule, building and verifying the ROM.

---

## Phase 1 — Previous Session Continuation (Compact Summary)

The previous session established:
- Platform is ASRock SPC741D8 / Xeon SP Sapphire Rapids DDR5 server
- Coreboot boots fully; crash happens inside `PeiDispatcher()` in EDK II's first pass
- VGA bug was fixed in `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` lines 43-45 (`p[1]→p[i]`)
- `analysis_report.md` was written
- Root cause identified: `BlSupportPei` — `MemInfoCallback` only updates `UsableLowMemTop` for RAM regions ≥ 64 MB; if none found, `PeiMemBase = 0 - 64MB = underflow`, crash

---

## Phase 2 — This Session

### User Request
> Update analysis_report.md with commands and outputs. Start from server.bin extraction.  
> Checkout from asrock_spc741d8_v0.9.0, create new branch.  
> Configure for EDK II (was showing SeaBIOS).  
> Track EDK II payload in root. Build and verify. Commit.  
> Server has ASPEED AST2600 (correction from AST2400).

---

### Step 1: Build Tools

```bash
make -C util/cbfstool
make -C util/ifdtool
# Produces: util/cbfstool/cbfstool  util/ifdtool/ifdtool
```

---

### Step 2: server.bin Inspection

```bash
file /home/mahdi/repositories/reconstruction/server/server.bin
# Intel serial flash for PCH ROM, 64MB
sha256sum server.bin
# db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465

util/ifdtool/ifdtool -d server.bin
```

Key output from ifdtool -d:
```
Flash Region 0 (Flash Descriptor): 0x00000000 – 0x00000FFF  (4 KB)
Flash Region 1 (BIOS):             0x03000000 – 0x03FFFFFF  (16 MB)
Flash Region 2 (Intel ME):         0x00003000 – 0x02FEFFFF  (~47.7 MB)
Flash Region 3 (GbE NVM):          0x00001000 – 0x00002FFF  (8 KB)
AltMeDisable bit is not set   ← original ME is fully active
FLMSTR1 Host CPU/BIOS write access: disabled
```

Flash regions already extracted:
```
/home/mahdi/repositories/reconstruction/server/flashregion_0_flashdescriptor.bin   4.0K
/home/mahdi/repositories/reconstruction/server/flashregion_1_bios.bin              16M
/home/mahdi/repositories/reconstruction/server/flashregion_2_intel_me.bin          16M
/home/mahdi/repositories/reconstruction/server/flashregion_3_gbe.bin               8.0K
```

Blob paths used by coreboot:
```
3rdparty/dasharo-blobs/asrock/spc741d8/descriptor.bin  ← present ✓
3rdparty/dasharo-blobs/asrock/spc741d8/me.bin          ← present ✓
3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd               ← present ✓
```

---

### Step 3: Branch Creation

```bash
git tag | grep asrock_spc741d8
# asrock_spc741d8_v0.9.0  (plus rc1-rc4)

git log --format="%H %s" asrock_spc741d8_v0.9.0 -1
# cee8ddf396... configs/config.asrock_spc741d8: bump to non-rc

git checkout -b edk2-server asrock_spc741d8_v0.9.0
# Switched to a new branch 'edk2-server'
```

---

### Step 4: Config — Fix SeaBIOS → EDK II

```bash
# .config was SeaBIOS (CONFIG_PAYLOAD_SEABIOS=y) — wrong
cp configs/config.asrock_spc741d8 .config
make olddefconfig

grep "PAYLOAD_EDK2\|EDK2_TAG" .config
# CONFIG_PAYLOAD_EDK2=y
# CONFIG_EDK2_TAG_OR_REV="22333c9e249641293ce4415e726354f396a0d8d3"
```

---

### Step 5: EDK II Workspace Symlink

The Makefile computes `EDK2_PATH = workspace/Dasharo` (uppercase) but the actual directory is `workspace/dasharo` (lowercase):

```bash
cd payloads/external/edk2/workspace && ln -sf dasharo Dasharo
# Creates: Dasharo -> dasharo
```

Updated `.config` to point to Mahdi's VGA-debug commit:
```ini
CONFIG_EDK2_TAG_OR_REV="a69960a18a0e0a28c25e5f4434f5385d25fb39bb"
```

---

### Step 6: git Submodule — Track EDK II in Root

The EDK II workspace at `payloads/external/edk2/workspace/dasharo/` is a nested git repo.  
To track it in the root coreboot repo:

```bash
# Fix gitignore to allow submodule:
# Added "!edk2/workspace/dasharo" to payloads/external/.gitignore

git submodule add -f /home/mahdi/backup/dasharo/ \
    payloads/external/edk2/workspace/dasharo
# Adding existing repo at '...' to the index

git commit -m "edk2-server: track EDK II workspace and update gitignore"
# [edk2-server 64175743c4] ...
```

The `.gitmodules` entry:
```ini
[submodule "payloads/external/edk2/workspace/dasharo"]
    path = payloads/external/edk2/workspace/dasharo
    url = /home/mahdi/backup/dasharo/
```

---

### Step 7: Build Attempts and Config Fixes

**Attempt 1:** Failed — iPXE tried to clone from internet (network error)
```
error: RPC failed; curl 92 HTTP/2 stream 0 was not closed cleanly
make[1]: *** [Makefile:39: ipxe] Error 128
```

Fix: Disabled iPXE in `.config`:
```ini
# CONFIG_BUILD_IPXE is not set
# CONFIG_EDK2_ENABLE_IPXE is not set
```

**Attempt 2:** Failed — edk2-platforms tried to clone from internet (network error)
```
make[1]: *** [Makefile:481: .../edk2-platforms] Error 128
```

Fix: Disabled edk2-platforms (not needed by Mahdi's single-repo EDK II workspace):
```ini
# CONFIG_EDK2_USE_EDK2_PLATFORMS is not set
```

**Attempt 3: SUCCESS** ✓

---

### Step 8: Build Verification

```bash
ls -lh build/coreboot.rom
# -rw-rw-r-- 1 mahdi mahdi 64M May 19 14:30 build/coreboot.rom

build/cbfstool build/coreboot.rom print -r COREBOOT
```

**CBFS COREBOOT region contents (verified):**
```
Name                           Offset     Type           Size   Comp
cbfs_master_header             0x0        cbfs header       32  none
fallback/payload               0x80       simple elf   1161095  none  ← EDK II ~1.1 MB ✓
cpu_microcode_blob.bin         0x11b880   microcode    1782784  none  ← Xeon SP microcodes ✓
intel_fit                      0x2ced00   intel_fit         80  none
fallback/romstage              0x2ced80   stage          49160  none  ✓
fallback/ramstage              0x2dae40   stage         142127  LZMA  ✓
config                         0x2fda00   raw             4337  LZMA
revision                       0x2feb80   raw              871  none
build_info                     0x2fef40   raw              123  none
fallback/dsdt.aml              0x2ff040   raw            18414  none
fspm.bin                       0x304780   fsp          3375104  none  ← FSP-M 3.3 MB ✓
fsps.bin                       0x63c800   fsp           185328  LZ4   ← FSP-S ✓
fallback/postcar               0x669c80   stage          30128  none  ✓
fspt.bin                       0xe67780   fsp            32768  none  ← FSP-T ✓
bootblock                      0xe807c0   bootblock      28672  none  ✓
```

**Payload extract:**
```bash
build/cbfstool build/coreboot.rom extract -r COREBOOT -n fallback/payload \
    -f /tmp/payload_check.elf -m x86
file /tmp/payload_check.elf
# ELF 32-bit LSB executable, Intel 80386, statically linked, stripped
ls -lh /tmp/payload_check.elf
# 15M  ← UEFI payload with DasharoPayloadPkg
```

**Build revision:**
```
COREBOOT_VERSION  = "asrock_spc741d8_v0.9.0-4-g0879a25b06ba"
DASHARO_VERSION   = "v0.9.0"
COREBOOT_BUILD    = "Tue May 19 13:28:19 UTC 2026"
```

---

### Branch State (edk2-server)

```
git log --oneline
64175743c4  edk2-server: track EDK II workspace and update gitignore
9eac4a3350  WIP  (auto-commit: .gitignore + logs/analysis_report.md)
cee8ddf396  configs/config.asrock_spc741d8: bump to non-rc  ← tag asrock_spc741d8_v0.9.0
```

---

### Confirmed Bug Summary

| # | File | Bug | Status |
|---|---|---|---|
| 1 | `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c:43-45` | `p[1]/string[1]` → needs `p[i]/string[i]` | Fixed in workspace commit `a69960a18a` |
| 2 | `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c:347` | `MemInfoCallback` skips low-RAM regions < 64 MB; causes `UsableLowMemTop=0` → underflow | **FIXED** in EDK II commit `c8014c2708` (asrock branch); coreboot.rom rebuilt 2026-05-19 15:06 |
| 3 | `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` | No PEIM GUID VGA print before dispatch | Pending (diagnostic) |
| 4 | `DasharoPayloadPkg/SecCore/SecMain.c` | `early_puts` serial not implemented | Pending |

---

### Bug 2 Fix Applied (2026-05-19 Session 2)

**Commit:** EDK II `c8014c2708` on `asrock` branch  
**File:** `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c:347`

```c
// BEFORE (broken — skips fragments < 64 MB):
if (Size >= PEI_MEM_SIZE) {
    *UsableLowMemTop = Base + Size;
}
// AFTER (fixed — tracks highest low-RAM region top):
if ((Base + Size) > *UsableLowMemTop) {
    *UsableLowMemTop = Base + Size;
}
```

Coreboot submodule updated to `c8014c2708`, ROM rebuilt at 15:06 UTC.

---

### Next Steps to Reach UEFI Shell

1. ~~**Apply Bug 2 fix**~~ **DONE** — `MemInfoCallback` now tracks any low-RAM region top
2. Flash `build/coreboot.rom` to hardware, observe VGA output
3. **If still crashing**: add `BlSupportPei` VGA dump — print `UsableLowMemTop` and `PeiMemBase` before `PeiServicesInstallPeiMemory`
4. **If PEIM crash persists**: add PEIM GUID print in `Dispatcher.c` — shows which PEIM causes the fault
5. Implement `early_puts` in `SecMain.c` for serial debug via IPMI SOL (`ipmitool -I lanplus sol activate`)
6. Repeat: rebuild → flash → observe → fix
7. Success path: memory install succeeds → PeiCore second pass → DXE → UEFI Shell

---

## Key File Locations

```
build/coreboot.rom                  ← final 64MB ROM to flash
logs/analysis_report.md             ← comprehensive reference doc
payloads/external/edk2/workspace/dasharo/   ← EDK II source (git submodule)
  DasharoPayloadPkg/BlSupportPei/BlSupportPei.c  ← Bug 2 FIXED (commit c8014c2708)
  MdeModulePkg/Core/Pei/PeiMain/PeiMain.c        ← VGA debug + Bug 1 (fixed)
  DasharoPayloadPkg/SecCore/SecMain.c             ← Bug 4 (serial)
  MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c  ← Bug 3 (diagnostic)
3rdparty/dasharo-blobs/asrock/spc741d8/   ← ME + descriptor blobs
3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd  ← Intel FSP for Xeon SP
```

---

*Generated 2026-05-19, updated session 2 (Bug 2 fix), branch edk2-server*
