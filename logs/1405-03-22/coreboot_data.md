# HOB / Payload Handoff Health Report — ASRock SPC741D8 (Xeon SP / Sapphire Rapids)
### coreboot → Dasharo EDK II UEFI payload, log `logs/1405-03-22/log.txt`

## Executive verdict

**Partly healthy — NOT yet provable that EDK II can run to completion from this capture.**

Everything that is actually visible in this log is internally consistent and decodes cleanly: the PHIT (EFI_HOB_HANDOFF_INFO_TABLE) at `0x63900000` is well-formed (version 9, boot_mode 0, sane memory bounds), the payload entry thunk at `0x802580` correctly receives the HOB pointer `0x63900000` and the BasePrintLib/BaseLib code is byte-intact, and the PEI dispatcher shows **forward progress, not a hang** — the suspected `FaultTolerantWritePei`/PEIM[2] (handle `0x829FE8`) was entered and returned (`PeimState 0→1`), and the delayed-dispatch loop completed (`End of DelayedDispath`).

**The single most important finding:** the captured HOB list (walked `H[1..14]` plus the raw window `0x63900000-0x639002FF`) contains **zero RESOURCE_DESCRIPTOR (type 3, EFI_RESOURCE_SYSTEM_MEMORY) HOBs and zero FV (type 5) HOBs**, and the walker's own VALID/INVALID verdict line (`prog_loaders.c:492-501`) was **never captured** (scrolled/overwritten off the 80x25 VGA). EDK II PEI cannot locate and dispatch PEIMs without an FV HOB, and the source's own rule (`end_found && n_sysmem>0 && n_fv>0`, else `HOB list INVALID!`) cannot be satisfied from the bytes we have. PEI demonstrably *did* run (FPDT SEC→PEI→PreMem→PEIM markers exist; two PEIMs dispatched), so type-3/type-5 HOBs almost certainly exist *further down the list* (addresses `>= 0x63901D48`, off-screen) — but that is **inference, not confirmed by this data**. Net: the data we can see is healthy; whole-list EDK-II validity is **inconclusive**, so the honest overall rating is **degraded**.

---

## What was analyzed

Four regions plus the handoff line, all from `logs/1405-03-22/log.txt`:

| # | Region | Source code | Health (verified) |
|---|--------|-------------|-------------------|
| 1 | PHIT @ `0x63900000` (0x38 bytes) + raw HOB dump | `src/lib/prog_loaders.c` `verify_edkii_hob()` | PHIT healthy; print labels misaligned (bug) |
| 2 | Walked HOB list `H[1..14]` + raw `0x63900040+` | `prog_loaders.c` walker; enum `src/drivers/intel/fsp2_0/include/fsp/util.h` | **Inconclusive — no type-3/type-5 captured** |
| 3 | coreboot data / EDK II entry @ `0x802580` | DasharoPayload IA32 image (BaseLib/BasePrintLib) | Healthy |
| 4 | PEI dispatcher state (`Dis-MI` .. `End of DelayedDispath`) | `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` | Dispatcher healthy / forward progress |

**The handoff** (`En:802580,63900000`): coreboot's `selfload`/`arch_prog_run` jumped to the payload entry at `0x802580` with the HOB pointer `0x63900000` as argument. This is the same pointer the PHIT decode and the dispatcher both reference — **CONFIRMED consistent across all three regions.** Note the log's harmless cosmetic glitch `HOB PTR = 0x0x63900000` (a doubled `0x` prefix in the print string).

---

## Region 1 — PHIT @ `0x63900000`

Decoded from the raw bytes (`struct hob_handoff_info_table`, generic header = 8 bytes, body at +8). All multi-byte fields little-endian.

```
63900000: 01 00 38 00 00 00 00 00 09 00 00 00 00 00 00 00
63900010: 00 d0 7f 77 00 00 00 00 00 00 80 63 00 00 00 00
63900020: 00 60 6b 76 00 00 00 00 f8 85 a6 63 00 00 00 00   <- dump2 (+0x28 = f8)
63900020: 00 60 6b 76 00 00 00 00 fb 85 a6 63 00 00 00 00   <- dump1 (+0x28 = fb)
63900030: f0 85 a6 63 00 00 00 00 04 00 10 1d 00 00 00 00
```

| Offset | Raw | Value | Meaning |
|--------|-----|-------|---------|
| +0x00 | `01 00` | type = 0x0001 | EFI_HOB_TYPE_HANDOFF — matches printed `Type:1`. CONFIRMED |
| +0x02 | `38 00` | length = 0x0038 (56) | Full PHIT size per PI spec. SANE. Printed `L:0` is WRONG (label bug) |
| +0x04 | `00 00 00 00` | reserved = 0 | Generic-header reserved u32. Zero as required |
| +0x08 | `09 00 00 00` | version = 0x9 | EFI_HOB_HANDOFF_TABLE_VERSION = 9. **HEALTHY, non-zero.** Printed `V:0` is WRONG (label bug) |
| +0x0C | `00 00 00 00` | boot_mode = 0 | BOOT_WITH_FULL_CONFIGURATION. Printed `BM:63A685F0` is NOT the boot mode (label bug) |
| +0x10 | `00 d0 7f 77 00 00 00 00` | memory_top = 0x777FD000 | Matches printed `MT:777FD000` |
| +0x18 | `00 00 80 63 00 00 00 00` | memory_bottom = 0x63800000 | Just below the HOB list. Printed `MB:0` is WRONG (= memory_top.hi32) |
| +0x20 | `00 60 6b 76 00 00 00 00` | free_memory_top = 0x766B6000 | Matches printed `FT:766B6000` |
| +0x28 | `f8/fb 85 a6 63 ...` | free_memory_bottom = 0x63A685F8 (dump2) / 0x63A685FB (dump1) | Printed `FB:0` is WRONG; `EL:63A685F8` actually = this field's low32 |
| +0x30 | `f0 85 a6 63 00 00 00 00` | end_of_hob_list = 0x63A685F0 | **Stable, identical in both dumps.** This is the REAL end-of-list |
| +0x38 | `04 00 10 1d 00 00 00 00` | next HOB: type=0x0004, length=0x1D10 | First non-PHIT HOB = GUID_EXTENSION, 7440 bytes (the FPDT blob) |

**Invariants (CONFIRMED):** `memory_bottom (0x63800000) <= memory_top (0x777FD000)`; `free_memory_bottom <= free_memory_top`; `end_of_hob_list (0x63A685F0)` lies inside `[bottom, top]`. The PHIT body is sound.

### The print-label misalignment is a real (cosmetic) bug — CONFIRMED

The printed lines:
```
HOB: 63900000, Type:1, MT:777fd000, MB:0
FT:766b6000, FB:0, EL:63a685f8, V:0, BM:63a685f0, L:0
```
`verify_edkii_hob()` (`prog_loaders.c:319,326`) prints the uint64_t fields with `%X`. coreboot's `vtxprintf` `%X` does `va_arg(unsigned int)` = **32 bits** (confirmed `src/console/vtxprintf.c`), and ramstage is 32-bit (`CONFIG_ARCH_RAMSTAGE_X86_32=y`), so each uint64_t occupies **two** 32-bit stack slots. Every `%X` after the first u64 reads `lo, hi, lo, hi...`, shifting all labels by one half-word. A 32-bit vararg simulation reproduces **both** lines byte-for-byte:
- `MB:0` = memory_top.hi32 (real memory_bottom 0x63800000 never printed)
- `FB:0` = free_memory_top.hi32 (real free_memory_bottom never printed at its label)
- `EL:63A685F8` = free_memory_bottom.lo32 (NOT end_of_hob_list)
- `V:0` = high half (real version = 9)
- `BM:63A685F0` = end_of_hob_list.lo32 (NOT boot_mode; real boot_mode = 0)
- `L:0` = high half (real length = 0x38)

**The "BM looks like a pointer" suspicion was correct in spirit: it IS a pointer (end_of_hob_list) leaking through the format bug.** Real `version=9`, `boot_mode=0`, `length=0x38`. Fix: use `%llX` (with casts) for the u64 fields.

### The fb/f8 byte at +0x28 — benign, and it is NOT EndOfHobList

The byte that differs between the two captures is **free_memory_bottom** (+0x28: `fb` → `0x63A685FB` in dump1, `f8` → `0x63A685F8` in dump2), **not** EndOfHobList. EndOfHobList lives at +0x30 and is **identical (`0x63A685F0`) in both dumps**. free_memory_bottom legitimately floats just above end_of_hob_list as the PEI pool is allocated between captures (delta +11 in dump1, +8 in dump2 — a small, varying pool offset, *not* a fixed "END-HOB size of 8" as one decode framed it). Because the walker uses end_of_hob_list (the stable +0x30 field), list integrity is unaffected. **CONFIRMED benign.**

**Region 1 verdict: PHIT data HEALTHY; one confirmed cosmetic logging bug. Not a boot blocker.**

---

## Region 2 — HOB list (walked `H[1..14]` + raw `0x63900040+`)

### Per-HOB table (walked log)

| H[n] | Type label | Type val | Len | Identity / notes |
|------|-----------|----------|-----|------------------|
| (PHIT) | HANDOFF | 0x0001 | 0x38 | Printed on the `HOB:`/`FT:` lines, not as `H[n]` |
| H[1] | GUID | 0x0004 | 0x1D10 | `gEdkiiFpdtExtendedFirmwarePerformanceGuid` `3b387bfd-7abc-4cf2-a0ca-b6a16c1b1b25` — wraps the whole FPDT perf blob |
| H[2] | MEM_POOL | 0x0007 | 40 | Routine PEI pool HOB |
| H[3] | MEM_POOL | 0x0007 | 264 | " |
| H[4] | MEM_POOL | 0x0007 | 264 | " |
| H[5] | MEM_POOL | 0x0007 | 136 | " |
| H[6] | MEM_POOL | 0x0007 | 136 | " |
| H[7] | MEM_POOL | 0x0007 | 520 | " |
| H[8] | MEM_POOL | 0x0007 | 32 | " |
| H[9] | MEM_POOL | 0x0007 | 80 | " |
| H[10] | GUID | 0x0004 | -- | `ea296d92-0b69-423c-8c28-33b4e0a91268` = `gPcdDataBaseHobGuid` (expected) |
| H[11] | GUID | 0x0004 | -- | `9b3ada4f-ae56-4c24-8dea-f03b7558ae50` = `PcdPeim` FILE_GUID (expected) |
| H[12] | GUID | 0x0004 | -- | `8c689f53-e082-452e-82dc-e3e272a44c16` — NOT in EDK II tree; likely coreboot-side handoff HOB (identity unconfirmed) |
| H[13] | MEM_POOL | 0x0007 | 24 | Routine PEI pool HOB |
| H[14] | GUID | 0x0004 | -- | `4a7bd124-cbea-4b3b-9586-11e668e9bcdd` — NOT in EDK II tree; likely coreboot-side handoff HOB (identity unconfirmed) |

### The "type 0x0007 / MEM_POOL" question — RESOLVED (label is correct)

CONFIRMED from `src/drivers/intel/fsp2_0/include/fsp/util.h:115-126`:
```
HOB_TYPE_HANDOFF=0x0001  MEMORY_ALLOCATION=0x0002  RESOURCE_DESCRIPTOR=0x0003
GUID_EXTENSION=0x0004     FV=0x0005                 CPU=0x0006
HOB_TYPE_MEMORY_POOL=0x0007  FV2=0x0009  ...  END_OF_HOB_LIST=0xFFFF
```
Type 7 = MEMORY_POOL, and `hob_type_name()` (`prog_loaders.c`) returns `"MEM_POOL"` via its default branch. **The `H[n] MEM_POOL 0x0007` labels are CORRECT** — these are genuine PI MEMORY_POOL bookkeeping HOBs from the PEI pool allocator, not a mis-decode.

### The raw `11 10 3a 01` records are NOT HOBs — the apparent conflict dissolves

The original premise (that the `11 10 3a 01` records named SEC/PEI/PreMem/PEIM are MEMORY_ALLOCATION HOBs) is **wrong**. The entire raw window `0x63900040-0x639002FF` is **inside H[1]** (the GUID_EXTENSION FPDT HOB, length `0x1D10` → spans `0x63900038..0x63901D48`). The walker does `cur.addr += len` (`prog_loaders.c:475`), so it jumps from `0x63900038` straight to `0x63901D48` and **never parses these records individually**.

```
63900040: fd 7b 38 3b ...                 GUID_EXT body GUID = FpdtExtendedFwPerf
63900050: c6 1c 00 00 00 00 00 00 01 00.. FPDT_PEI_EXT_PERF_HEADER:
                                          SizeOfAllEntries=0x1CC6, LoadImageCount=0, HobIsFull=1
6390005C: 11 10 3a 01 ...                 FPDT record: Type=0x1011, Length=0x3A(58), Revision=1
```
`Type=0x1011` = `FPDT_DYNAMIC_STRING_EVENT_TYPE`; the header is `{u16 Type; u8 Length=0x3A; u8 Revision=1}` (one-byte length, NOT a u16 `0x013A`), stride 58 bytes — decodes cleanly with ProgressID/ApicID/Timestamp/GUID/ASCII token. ASCII tokens at the raw offsets: `SEC`@0x7E, `PEI`@0xB8, `PreMem`@0xF2, then repeated `PEIM` markers (0x12C, 0x162, 0x19E, 0x216, 0x24E, 0x284, 0x2C2, 0x2FC). Embedded module GUIDs include `b73f81b9-1dfc-487c-824c-0509ee2b0128` = **DebugServicePei** (NOT PcdPeim — one decode mis-byte-ordered/mis-identified this), `52c05b14-...` = PeiMain (PEI Core), `9b3ada4f-...` = PcdPeim (0x190), `a3610442-...` = ReportStatusCodeRouterPei, `fde29a56-...` (0x204). **These are FPDT performance payload, not HOBs.**

**Consequence:** the raw dump and the walked `H[2..14]` are **different things** — the dump shows H[1]'s interior; H[2..14] live at `>= 0x63901D48` and are **not in the dump at all**. There is no genuine contradiction, but neither can H[2..14] be byte-verified from the dump.

### Missing H[0] / summary — truncation, not corruption

The capture has no explicit `H[0]` (the PHIT is printed on the `HOB:`/`FT:` lines), and crucially **no `TOT:`/`GUID:`/`SysRAM=` summary, no END_OF_HOB_LIST line, and no VALID/INVALID verdict**. Row tracing (`verify_edkii_hob` called with `start_row=3`): PHIT block + 14 `H[n]` rows already exceed the 80x25 screen (rows 0-24), and the `En:%X,%X` line is written *after* the walker returns and overwrites the H[1] row. So the summary lands at row 24+ and/or is overwritten — **the tail was lost to row overflow, not a walk-breaking error**. No `BAD len`/`>4096`/`cur.addr > end` error was printed, so the walk most likely continued past H[14] off-screen.

### CRITICAL completeness check — RESOURCE_DESCRIPTOR (type 3) and FV (type 5)?

This is the load-bearing question for "can EDK II run to completion." The walker's own validity rule (verified `prog_loaders.c:492-501`):
```c
if (n_sysmem == 0)  vga_cb_sprintf(row++, "ERR: no SYSTEM_MEMORY res");
if (n_fv == 0)      vga_cb_sprintf(row++, "ERR: no FV HOB");
...
if (end_found && n_sysmem > 0 && n_fv > 0)  /* VALID */
else                                        vga_cb_sprintf(row++, "HOB list INVALID!");
```

| Requirement | Present in capture? | Evidence |
|-------------|--------------------|----------|
| Type 3 RESOURCE_DESCRIPTOR (SYSTEM_MEMORY) | **NOT in capture** | Walked `H[1..14]` = only GUID_EXTENSION + MEMORY_POOL; raw `0x63900000-0x639002FF` is entirely inside H[1] (FPDT). No `H[n] RESOURCE` line, no type-3 bytes. |
| Type 5 FV HOB | **NOT in capture** | Same — no `H[n] FV` line, no type-5 bytes. |
| END_OF_HOB_LIST (0xFFFF) reached | **NOT confirmed** | No END line captured; `end_found` unknown for the captured subset. |
| VALID/INVALID verdict | **NOT captured** | Summary scrolled/overwritten off VGA. |

**This is the crux and it is genuinely INCONCLUSIVE.** Strong *inference* that type-3/type-5 exist below H[14]: (a) FPDT records prove SEC→PEI→PreMem→PEIM ran; (b) the dispatcher (Region 4) successfully `GetFileInfo`'d a PEIM in an FV at `FvHandle=0x800000` with `FvPpi=0x8161A8`, which means an FV HOB was processed earlier; (c) `PeiMemoryInstalled=0` (a-PI:0, 8c-0) means permanent memory is **not yet** installed — consistent with being *before* `BlSupportPei`/`MemInfoCallback` publishes the main SYSTEM_MEMORY resource HOB, so the type-3 SYSTEM_MEMORY HOB legitimately may not exist yet at this exact instant. But the *raw bytes do not prove any of this*, and the walker's VALID/INVALID line was never seen.

**Region 2 verdict: consistent and healthy where observable, but whole-list EDK-II validity UNVERIFIED. This is the report's primary open risk.**

---

## Region 3 — coreboot data / EDK II entry @ `0x802580`

This is a slice of the IA32 DasharoPayload image: the SEC entry thunk, four BaseLib/BaseMemoryLib/BaseCpuLib helpers, and BasePrintLib rodata.

### Entry thunk (CONFIRMED — receives the HOB pointer correctly)

```asm
802580: fa                cli                       ; disable interrupts on entry
802581: 8b 44 24 04       mov  eax,[esp+4]          ; load arg1 = HOB ptr 0x63900000
802585: 8b 25 b0 29 80 00 mov  esp,[0x8029b0]       ; switch to payload stack
80258b: 50                push eax                  ; push HOB ptr
80258c: ff 35 ac 29 80 00 push dword [0x8029ac]     ; push base/handoff ptr
802592: 68 00 00 08 00    push 0x80000              ; size arg (524288)
802597: 68 00 00 01 00    push 0x10000              ; size arg (65536)
80259c: e8 ba fe ff ff    call 0x80245b             ; rel32 = -326 -> SEC C core
8025a1: eb fe             jmp  0x8025a1             ; jmp $ (CpuDeadLoop guard)
8025a3: 66 90 (x6) 90     align padding (NOPs)
```
The call target is exact: `nextIP 0x8025a1 + (-326) = 0x80245b`. **The thunk consumes the HOB argument `0x63900000` correctly** — matching `En:802580,63900000`.

### The `eb fe` spin — NORMAL, not a fault

`eb fe` (`jmp $`) at `0x8025a1` is the standard **CpuDeadLoop "should never return" guard** after the SEC call. It executes only if the SEC core ever returns. Surrounding `66 90` are alignment NOPs. **NOT evidence of the hang.**

### Helpers and format strings — INTACT (CONFIRMED, some byte-exact to NASM)

| Addr | Routine | Status |
|------|---------|--------|
| 0x8025b0 | `InternalMathDivRemU64x32` (DivU64x32Remainder) | Byte-identical to a fresh NASM build of the canonical .nasm |
| 0x8025d0 | memset / ZeroMem (rep stosd + byte tail) | Structurally sound |
| 0x8025f0 | `InternalMemCopyMem` (overlap-safe) | Byte-identical to BaseMemoryLibRepStr CopyMem.obj |
| 0x802630 | `InitializeFloatingPointUnits` | Sound except one modrm byte — see below |

Format strings (BasePrintLib) match `MdePkg/Library/BasePrintLib/PrintLibInternal.c` exactly: `": "`, `"\r\n"` (0x80266c), `"<null string>"`, `"<null time>"`, `"<null guid>"`, the GUID format `"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"`, the time format `"%02d/%02d/%04d  %02d:%02d"` (double space is genuine), and `"%08X"`. **The print library is linked and uncorrupted.**

### Minor anomaly: `ldmxcsr` modrm byte (0x802652)

Dump shows `0f ae 16 a2 2d 80 00` (= `ldmxcsr [esi]`); canonical NASM emits `0f ae 15 a2 2d 80 00` (= `ldmxcsr [0x802DA2]`). The trailing disp32 `0x802DA2` is exactly `mMmxControlWord` (= `mFpuControlWord` 0x802DA0 + 2), so the operand is byte-correct and **only the single modrm `rm` bit differs**. Almost certainly a `15→16` transcription typo in the hex dump; even if real it is benign (mis-loads MXCSR from `[esi]` on SSE CPUs) and **unrelated to the PEI dispatcher question.**

**Region 3 verdict: HEALTHY. This region is NOT the source of any crash.**

---

## Region 4 — PEI dispatcher state

Decoded against `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` (format strings verified at the line numbers below).

| Line (raw) | Decode | Meaning |
|------------|--------|---------|
| `Dis-MI:0,S:8fe94,PDDT:0,GHOB:0` | PeiMemoryInstalled=0; SecCoreData=0x8FE94; DelayedDispatchTable=0; GHOB=0 | Entry to PeiDispatcher; perm memory not yet installed (expected) |
| `TS-c:260,PDDT:80ec8,S:0` | TableSize=0x260; DelayedDispatchTable=0x80EC8; Status=0 | `:2134` — table freshly **BUILT** via `BuildGuidHob` (`:2107`), not merely "located". Allocation healthy |
| `a-PI:0,PMTRFV:0,HITBM:0,PSPSB:0` | MemInstalled=0; MigrateTempRamFV=0; HandoffBootMode=0; ShadowPeimOnS3=0 | Cold full-config boot, no S3, no temp-RAM-FV migration |
| `PDR-,0,0,1` | PeimDispatcherReenter=0; CurrentPeimFvCount=0; **FvCount=1** | `:2262` — corrected field names. One firmware volume present (NOT a "loop control = 1") |
| `NCHb-,8fa30,0,CVH:80d28,Fp:8161a8,CPC:0` | CoreFvHandle=0x80D28; FvPpi=0x8161A8; CurrentPeimCount=0 | `:2285` — FV2 PPI selected; FvPpi matches the later FIc line |
| `FIc-2-0-0,8161a8-829fe8-8fcfc,6` | PeimCount=2; PeimState=0 (NOT_DISPATCHED); GetFileInfo Status=0; FvPpi=0x8161A8; handle=0x829FE8; &FvFileInfo=0x8FCFC; FileType=6 (PEIM) | `:2346` — GetFileInfo SUCCEEDED; valid PEIM at PEIFV 0x800000 + FFS 0x29FE8. No FV corruption |
| `PLId-8000000e,8fa34-829fe8-8fa00-0-800000` | VerifyPeim Status=**0x8000000E**; PeiServices=0x8FA34; handle=0x829FE8; &EntryPoint=0x8FA00; AuthState=0; FvHandle=0x800000 | `:2435` — see analysis below. Presence of this line implies `PeiLoadImage` returned SUCCESS first (gated at `:2411`) |
| `8c-0-1-0-1-0-0-0` | MemInstalled=0; PeimCount=1; FvCount=0; **PeimState=1 (DISPATCHED)**; ... | `:2492` — PeimState 0→1: entry point WAS called and RETURNED |
| `Pb-80ec8` | DelayedDispatchTable=0x80EC8 | `:2599` — same table as TS-c; consistent |
| `DelayedDis-80ec8,0-0,989680` | Table=0x80EC8; **DelayedGroupId=0; Status=0**; MaxDispatchTime=0x989680 (10,000,000us=10s) | `:389` (`"DelayedDis-%X,%X-%X,%LX"`) — corrected field names; `0,0` are GroupId/Status, NOT Count/DispCount |
| `n-0,1,0,0` | iteration bookkeeping; table Count=0 | Nothing to wait on; trivial pass-through |
| `End of DelayedDispath` | DelayedDispatchDispatcher returned normally (`:549`) | Returns to main PeiDispatcher loop |

### `PLId-8000000e` — VerifyPeim = EFI_NOT_FOUND, dispatched anyway (CONFIRMED)

`0x8000000E` = `ENCODE_ERROR(14)` = **EFI_NOT_FOUND** (not `EFI_SECURITY_VIOLATION`, which is `0x8000001A` = code 26). In `Security.c`, `VerifyPeim` defaults `Status = EFI_NOT_FOUND`; with no Security2/private security PPI installed and no `EFI_AUTH_STATUS_IMAGE_SIGNED` bit (`AuthState=0`), it returns that default unchanged. This is the **benign, expected result for an unsigned PEIM in a build with no security PPI** — it is NOT an image-load or verification failure.

The dispatch branch (verified `Dispatcher.c:2443-2452`):
```c
if (Status != EFI_SECURITY_VIOLATION) {   // 0x8000000E != 0x8000001A -> TRUE
  ...
  Private->Fv[FvCount].PeimState[PeimCount]++;        // :2447  state 0 -> 1
  PeimEntryPoint = (EFI_PEIM_ENTRY_POINT2)(UINTN)EntryPoint;
  PeimEntryPoint (PeimFileHandle, ...);               // :2452  ENTRY POINT CALLED
}
```
So `EFI_NOT_FOUND` falls into the dispatch path and the entry point **is** called. The subsequent `8c-...-1-...` line (PeimState now 1) proves the entry point **returned**. **`0x8000000E` is cosmetic noise, not an error to chase.**

### PEIM[2] / handle `0x829FE8` (suspected FaultTolerantWritePei) — progressed

`FIc-2-...` identifies `PeimCount=2`, handle `0x829FE8` = PEIFV base `0x800000` + FFS offset `0x29FE8`, FileType 6 (PEIM). This is the PEIM prior sessions suspected of hanging (FaultTolerantWritePei, due to SMMSTORE disabled + zero FlashNvStorage PCDs causing a UINT64-underflow scan loop). In **this** capture the PEIM was entered and control returned (`PeimState 0→1`), and the dispatcher proceeded into and out of the delayed-dispatch loop.

> Caveat (honest): `FIc` reports `PeimCount=2` while the `8c` snapshot reports `PeimCount=1`. Both reference the same loop variable at `:2346` and `:2492`, so they **cannot** differ within one iteration — the two captured lines are from **different loop iterations** overwriting the same VGA rows. The load-bearing fact (`PeimState 0→1` = a PEIM was dispatched and returned) is unambiguous, but mapping it to *exactly* PEIM index 2 vs index 1 is not pinned down by these two rows alone.

### HANG resolved or relocated? Does "End of DelayedDispath" mean forward progress?

**Forward progress — CONFIRMED; the prior FaultTolerantWritePei hang is NOT reproduced in this run.** Evidence chain: GetFileInfo SUCCESS → VerifyPeim returns (no infinite loop in verify) → dispatch branch taken → `PeimEntryPoint` called and **returned** (`8c` PeimState=1) → `PeiCheckAndSwitchStack` returned → `DelayedDispatchDispatcher` ran (Count=0, returned immediately; the 10s `MaxDispatchTime` is just the cap, not an active sleep) → `End of DelayedDispath` printed (`:549`). Control then returns to the top of the PEIM scan loop. The capture ending at `End of DelayedDispath` without a later `End of Dispatching` is a **VGA cutoff, not a hang** — no error/`die` line was emitted.

So: **resolved or not-reproduced in this run, not relocated within these lines.** What this log does NOT show is the *next* iteration (the advance toward PEIM[3] = `BlSupportPei`, which should call `MemInfoCallback`, publish the SYSTEM_MEMORY RESOURCE_DESCRIPTOR, and flip `PeiMemoryInstalled` to 1). That transition — and whether the boot ultimately completes — is beyond this capture.

**Region 4 verdict: dispatcher HEALTHY, forward progress, no hang at this point.**

---

## Consolidated findings (critical first)

| Severity | Region | Issue | Impact on "EDK II runs to completion" |
|----------|--------|-------|----------------------------------------|
| **CRITICAL / inconclusive** | 2 (HOB list) | No RESOURCE_DESCRIPTOR (type 3, SYSTEM_MEMORY) and no FV (type 5) HOB present in captured `H[1..14]` or raw `0x63900000-0x639002FF`; walker VALID/INVALID verdict + END_OF_HOB_LIST never captured (`prog_loaders.c:492-501`). | **Directly load-bearing.** Without an FV HOB, EDK II PEI cannot find/dispatch PEIMs; without a SYSTEM_MEMORY resource HOB the DXE/memory map is incomplete. Strong inference they exist off-screen (PEI ran; FV at 0x800000 was processed; MemInstalled=0 is pre-BlSupportPei), but **UNCONFIRMED**. This is the gating unknown. |
| Warning | 1 / all | u64 PHIT fields printed with `%X` in 32-bit ramstage → labels shifted (`V:0`, `L:0`, `MB:0`, `FB:0` wrong; `BM:63A685F0` = end_of_hob_list.lo; `EL` = free_memory_bottom.lo). `prog_loaders.c:319,326`. | **None on boot** (display only; struct/values are correct). High operational impact: it makes the log misleading and masks the real values during debugging. Fix with `%llX`. |
| Warning | 2 | Capture truncated (80x25 VGA row overflow + `En:` overwrite); no summary, no END marker. | None on boot directly, but it is *why* the critical item above is inconclusive. Re-capture via serial/cbmem. |
| Info | 2 | H[12] `8c689f53-...` and H[14] `4a7bd124-...` GUIDs not found in the EDK II payload tree (likely coreboot-side handoff HOBs). | Likely benign; identity unconfirmed. |
| Info | 3 | `ldmxcsr` modrm `0x16` vs canonical `0x15` at 0x802652 (single bit). | Almost certainly a hex-dump transcription typo; even if real, benign and unrelated. |
| Info | 4 | `PLId-8000000e` = EFI_NOT_FOUND from VerifyPeim. | None — benign default for unsigned PEIM with no security PPI; entry point dispatched anyway. |
| Info | 1 | fb/f8 byte at +0x28 differs between dumps = free_memory_bottom (NOT EndOfHobList, which is stable at +0x30). | None — benign capture-time pool delta. |
| Info | 4 | `FIc` PeimCount=2 vs `8c` PeimCount=1 = different loop iterations overwriting VGA rows. | None on the core conclusion (PeimState 0→1 is valid). |

---

## Verdict & next steps

### Direct answer: "Are all the coreboot data healthy and correct for EDK II to run to completion?"

**Partly — and not provable from this log.** Of the four regions, three are confirmed healthy: the PHIT is well-formed (version 9, boot_mode 0, sane bounds), the payload entry/code at `0x802580` is byte-intact and correctly receives the HOB pointer, and the PEI dispatcher shows clean forward progress (the suspected FaultTolerantWritePei hang is **not** reproduced — the PEIM was dispatched and returned, `EFI_NOT_FOUND` from VerifyPeim is benign).

The blocker to a clean "yes" is **Region 2**: the captured HOB list contains **no type-3 SYSTEM_MEMORY RESOURCE_DESCRIPTOR and no type-5 FV HOB**, and the walker's own VALID/INVALID verdict and END_OF_HOB_LIST marker scrolled off the 80x25 screen. EDK II PEI/DXE cannot complete without these. Circumstantial evidence (PEI demonstrably ran SEC→PEI→PreMem→PEIM, an FV at `0x800000` was processed by the dispatcher, and `PeiMemoryInstalled=0` indicates we are *before* `BlSupportPei` installs permanent memory) makes it **likely** they exist further down the (off-screen) list — but that is **inference, not confirmation**. Honest overall rating: **degraded / inconclusive on the HOB side**, healthy everywhere observable.

### Prioritized next actions

1. **Capture the full HOB walk to serial or cbmem, not VGA** (highest priority). The 80x25 screen drops exactly the lines that answer the question. Route `verify_edkii_hob()` output to the serial console / cbmem console so you see the `TOT:`/`n_res`/`n_sysmem`/`n_fv` tallies, the `ERR: no SYSTEM_MEMORY res` / `ERR: no FV HOB` lines, the END_OF_HOB_LIST, and the final `VALID` vs `HOB list INVALID!` verdict. This converts the critical "inconclusive" into a definite pass/fail.
2. **Specifically confirm >= 1 type-3 (EFI_RESOURCE_SYSTEM_MEMORY) and >= 1 type-5 FV HOB exist** below `0x63901D48`. If they do not, the HOB list is genuinely invalid and EDK II cannot proceed — that would be the real root cause, distinct from the FTW theory.
3. **Fix the `%X`/u64 print bug** in `prog_loaders.c:319,326` (use `%llX` with casts). This is low-effort and removes a persistent source of debugging confusion (the misleading `V:0`, `BM:<pointer>`, `MB:0`, `FB:0`).
4. **Capture the next dispatcher iteration** to confirm advance into PEIM[3] = `BlSupportPei`, the `MemInfoCallback` execution, and the `PeiMemoryInstalled` 0→1 transition. That is the step that publishes the SYSTEM_MEMORY HOB and tells you whether the boot truly progresses past temp-RAM into permanent memory.
5. **Re-evaluate the SMMSTORE/FTW PCD theory against this evidence.** This log shows the suspected FaultTolerantWritePei PEIM dispatching and returning normally — so either the prior hang was elsewhere, was already mitigated, or moves later. Do not assume the FTW-underflow loop is the active failure based on this capture; tie that conclusion only to a log that actually shows the hang. (Project-context items — SMMSTORE disabled, zero FlashNvStorage PCDs, BlSupportPei MemInfoCallback — remain plausible but are **not** demonstrated as the failure by `logs/1405-03-22/log.txt`.)

### Confirmed vs inference (summary)
- **CONFIRMED:** PHIT structure healthy; print-label misalignment is a real cosmetic bug; MEM_POOL/type-0x0007 label correct; raw `11 10 3a 01` records are FPDT perf data inside H[1], not HOBs; entry thunk receives HOB ptr and calls `0x80245b`; format strings intact; `eb fe` is a normal CpuDeadLoop guard; `0x8000000E` = EFI_NOT_FOUND (benign); PEIM dispatched + returned (PeimState 0→1); `End of DelayedDispath` = forward progress, no hang here.
- **INFERENCE (likely but unproven):** type-3 SYSTEM_MEMORY and type-5 FV HOBs exist below H[14]; the suspected hang is resolved/relocated rather than fixed; H[12]/H[14] GUIDs are coreboot-side handoff HOBs.
- **INCONCLUSIVE (must re-capture):** whole-HOB-list EDK-II validity (VALID/INVALID verdict and END marker), and whether the boot proceeds into BlSupportPei and to completion.
