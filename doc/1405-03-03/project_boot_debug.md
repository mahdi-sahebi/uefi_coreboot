---
name: Coreboot+EDK II Boot Debug Status
description: Active debugging of ASRock SPC741D8 coreboot+DasharoPayloadPkg EDK II boot failure
type: project
originSessionId: 3b588eef-804b-47d3-9fca-6949378f9da1
---
**Platform:** ASRock SPC741D8-2L2T/BCM (Intel Xeon SP Sapphire Rapids, DDR5)
**BMC:** Aspeed AST2600 (NOT AST2400 — correction from session 2026-05-19)
**Repo:** /home/mahdi/repositories/coreboot/dasharo/coreboot
**Active branch:** `edk2-server` (created from tag `asrock_spc741d8_v0.9.0`)
**EDK II workspace:** payloads/external/edk2/workspace/dasharo/ (separate git repo)

**Current Status (as of 2026-05-25):**
- Branch edk2-server created fresh from asrock_spc741d8_v0.9.0 (known-good release tag)
- Coreboot boots fully (all stages including FSP-T/M/S)
- EDK II SecCore → PeiCore handoff works correctly
- PeiCore first pass reaches PeiDispatcher with OldCoreData=NULL, PMI=0
- **Hang confirmed inside 3rd PEIM = FaultTolerantWritePei** (FILE_GUID
  `AAC33064-9ED0-4B89-A5AD-3EA767960B22`, PeimFileHandle=0x829FE8). Confirmed
  by ROM inspection of build/coreboot.rom: PEIFV starts at 0x800000 and
  PEIM[2] FFS offset is 0x29FE8 → handle 0x829FE8 (exact match to VGA log).
- Dispatch order (no PEI Apriori): Pcd → SmmStorePei → **FaultTolerantWritePei (hang)**
  → BlSupportPei → ... Tcg/Opal/HddPassword stack also present in FV (TPM_ENABLE=TRUE).
- Likely cause: gVariableFlashInfoHobGuid published by SmmStorePei describes a
  flash region that is wrong for this Xeon SP platform, OR SPI working-block
  is not readable from CAR pre-memory. FTW PEI's entry walks into bad MMIO.
- Last VGA frame analysis written to logs/1405-03-03/analyize.md
- Last VGA output before this session: `[1]u-Sec:8FE94, PD:8FA30,BM:0,PMI:0` (PeiMain.c:882)

**Root Cause Identified:**
BlSupportPei (DasharoPayloadPkg/BlSupportPei/BlSupportPei.c lines 720-733):
- `MemInfoCallback` only updates `UsableLowMemTop` if low-RAM region SIZE >= 64 MB
- `ASSERT(UsableLowMemTop >= BASE_1MB + PEI_MEM_SIZE)` — in RELEASE build this is NO-OP
- Then `PeiMemBase = (0 - 64MB)` → UINT64 underflow → 0xFFFFFFFFFC000000
- `PeiServicesInstallPeiMemory(0xFFFFFFFFFC000000, ...)` → PeiCore triple-fault
- Fix: change MemInfoCallback to track highest low-RAM top regardless of size

**Key addresses:**
- UEFI payload loaded at: 0x800000 (8 MB)
- PEI Core entry: 0x80E2D0
- Temp RAM: 0x80000–0x90000 (64 KB: 32KB heap + 32KB stack)
- Coreboot table (LBIO): 0x63593000
- SecCoreData: 0x0008FE94

**Active Bugs:**
1. PeiMain.c:43-45 — `p[1]/string[1]` instead of `p[i]/string[i]` — FIXED in fixbug branch; needs cherry-pick to edk2-server
2. BlSupportPei crash — MemInfoCallback size threshold bug; fix: remove `Size >= PEI_MEM_SIZE` guard
3. PEIM GUID debug — was missing; CONFIRMED via ROM analysis (see logs/1405-03-03/analyize.md). Live confirmation still pending — add `mde_2_edkii_vga_sprintf(9, "EP-%LX,FN-%g", EntryPoint, &FvFileInfo.FileName);` immediately before Dispatcher.c:2452 to print the GUID + entry address right before PeimEntryPoint() runs.
4. Serial port `early_puts` not implemented in SecMain.c — full code in analysis_report.md Section 14
5. FaultTolerantWritePei hang (Dispatcher PEIM[2]) — need to instrument PeimFaultTolerantWriteInitialize and validate gVariableFlashInfoHobGuid produced by SmmStorePei against coreboot's SmmStore region for this server.

**Tools:**
- cbfstool: util/cbfstool/cbfstool (built with `make -C util/cbfstool`)
- ifdtool: util/ifdtool/ifdtool (built with `make -C util/ifdtool`)
- flashrom: system package
- Serial debug: via IPMI SOL (`ipmitool -I lanplus -H <bmc_ip> sol activate`)

**Why:** Mahdi is porting Dasharo/coreboot to this server which originally runs SeaBIOS as the default payload. Changed to EDK II UEFI payload for a UEFI boot environment.

**How to apply:** Start from BlSupportPei crash (Bug 2). The analysis_report.md in logs/ is the comprehensive guide from server.bin extraction to UEFI shell.
