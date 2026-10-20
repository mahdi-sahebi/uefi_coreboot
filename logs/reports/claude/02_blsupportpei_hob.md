# PEI Dispatcher / FaultTolerantWritePei / BlSupportPei / HOB — deep trace

Source: EDK2 workspace submodule at
`payloads/external/edk2/workspace/dasharo` (HEAD `b8bfb09d99`, tracked
`edk2-stable201903-8790-gb8bfb09d99`). Commit history (newest first) directly diffed:

```
b8bfb09d99 Logs of Secphase are updated
b0f667e345 PEIM[2] is ignored
c65fda2b75 Timerlib is removed from faulttolerant
46e33474ed Logs of FaultTolerantPeim are added
4536a5aa6f Logs of BlSupportPei are added
5958563c97 CHeckpoint
```

## 1. The dispatcher skip has been reverted — PEIM[2] dispatch is restored

`MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c:2319-2321` — **the user has confirmed this
`continue` was their own deliberate test**, added to isolate the FaultTolerantWritePei hang by
skipping it. It has now been commented out (2026-07-27):

```c
// if (2 == PeimCount) {
//   continue;
// }
```

While it was active, it lived inside the standard `for (PeimCount = Private->CurrentPeimCount;
PeimCount < Private->Fv[FvCount].PeimCount; PeimCount++)` loop (line 2315-2317). A `continue`
there just advances the loop counter — it did not skip or corrupt dispatch of any later PEIM
(`BlSupportPei` at index 3 was always reached normally in the same pass); the only side effect
was `PeimState[2]` never leaving `NOT_DISPATCHED`, keeping `Private->PeimNeedingDispatch` `TRUE`
forever and forcing repeated FV re-scans.

**Consequence of the revert: `FaultTolerantWritePei`'s entry point
(`PeimFaultTolerantWriteInitialize`) will run again on the next build/boot.** This makes §2 below
(the unsigned-underflow bug) live again — it is no longer dead code, and must be addressed (via
`CONFIG_SMMSTORE`, see §5) before the next flash attempt, or the historic PEIM[2] hang will
return exactly as originally observed.

Any "all parameters are 0" output previously observed for FaultTolerantWritePei, while the skip
was active, was necessarily the **Dispatcher's own pre-call debug print** of that PEIM's
FvFileInfo/PCD fields (Dispatcher.c's `mde_2_edkii_vga_sprintf(...)` block, added in
`4536a5aa6f`), not output from inside `FaultTolerantWritePei.c` itself — since that PEIM's entry
point was never called at all during that period.

## 2. FaultTolerantWritePei.c's underflow bug is unchanged and now live again (debug traps removed)

`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c:528-539`:

```c
WorkSpaceInSpareArea = SpareAreaAddress + SpareAreaLength - WorkSpaceLength;
...
while (WorkSpaceInSpareArea >= SpareAreaAddress) {
  ...
  WorkSpaceInSpareArea -= sizeof(EFI_GUID);   // line 539 — unsigned underflow when args are 0
}
```

This is the exact unsigned-underflow loop from the earlier `analyze_1405_04_31.md` §6 diagram —
**still present and unguarded, and reachable again now that §1's Dispatcher skip is reverted**.
The debug prints added in `46e33474ed` show `Status/WorkSpaceAddress/Size/SpareAreaAddress/
SpareAreaLength` are all `0` — consistent with the already-diagnosed root cause (`CONFIG_SMMSTORE`
off → PCDs default to `0` per `DasharoPayloadPkg.dsc` `[PcdsDynamicDefault]`), not a new or
different bug. All `ASSERT_EFI_ERROR` calls that used to guard the calls feeding into this
computation were commented out, so nothing stops execution from reaching the underflow path.

**The hard-coded infinite loop previously at lines 577-580** (immediately before
`PeiServicesInstallPpi(&mPpiListVariable)`) **has been commented out by the user**:

```c
// int i = 10;
// while (i) {
//   i = 20;
// }
```

Good — this was a breakpoint-style debugging trap, not a fix, and removing it was correct. But
its removal means the underflow loop above is now the **only** thing standing between a normal
boot and a hang once `FaultTolerantWritePei`'s entry point runs again — enabling `CONFIG_SMMSTORE`
(§5) before the next boot attempt is now the critical remaining step.

`TimerLib`/`MicroSecondDelay` usage was stripped in `c65fda2b75` then effectively restored in
`46e33474ed` — net no change to the underflow logic itself.

## 3. The SecMain.c infinite-loop trap has also been removed

The `int test = 1; while (test);` previously inserted in `DasharoPayloadPkg/SecCore/SecMain.c`
right after the `(*PeiCoreEntryPoint)(...)` call (in `SecStartupPhase2`, before the "Should not
come here" comment, added in `b8bfb09d99`) **has been commented out by the user**. That call site
is only reached if `PeiCore()` itself returns, which it shouldn't in normal operation, so this
was always a low-risk debugging artifact — its removal is correct housekeeping and needs no
further action.

Same commit also unified several `DEBUG()` macro calls in `FindPeiCore.c`/`SecMain.c`/`edkii_vga.c`
into `edkii_vga_sprintf()` (VGA-framebuffer text output) — a workaround for serial being off (see
`01_serial_sol.md`), but incomplete: not every DEBUG() call in the affected files was converted,
so some output will still only appear once serial is fixed.

## 4. HOB construction mechanics (confirmed from `manual_doc/04_coreboot_hob.md`)

The HOB list (`gHobList` in PEI / `mHobList` in DXE) is a **runtime-only** structure built in
temp-RAM/CBMEM during SEC/PEI — it cannot be recovered from a static flash image at all; the
manual doc explicitly demonstrates this by showing the DXE-side global is just a `!= NULL` ASSERT
and a format string (`"HOBLIST address in DXE = 0x%p"`). This matters for the user's belief that
"HOB is constructed in BlSupportPei, but its code isn't called": **that belief is only partially
right and is a symptom, not the disease**. `BlSupportPei` (PEIM index 3, not 2) is not itself
blocked by anything found in the Dispatcher diffs — it's on track to be called normally.
The confusion is very plausibly caused by conflating:
- FaultTolerantWritePei's Dispatcher-skipped, all-zero debug prints (index 2), with
- BlSupportPei's own debug prints (index 3), which are real but currently split between
  VGA-framebuffer output (visible) and serial DEBUG() macros (invisible until §1 of
  `01_serial_sol.md` is fixed).

**No runtime capture in `logs/` currently shows BlSupportPei's own prints firing at all** (see
`00_SUMMARY.md` point 4) — the last live trace (`logs/1405-04-21/`) predates all of the commits
analyzed in this section, so it cannot confirm or refute current behavior. A fresh boot capture,
ideally with serial enabled per `01_serial_sol.md`, is needed to observe BlSupportPei's actual
current behavior directly rather than inferring it from source diffs.

## 5. `CONFIG_SMMSTORE` chain — still the root enabler of the FaultTolerantWritePei bug

`.config` still shows `# CONFIG_SMMSTORE is not set`, even though `board.fmd` was edited twice
(`SMMSTORE is added in board.fmd` ×2 in the coreboot git log) to carve out an FMAP region. The
FMAP region existing on disk does nothing unless `CONFIG_SMMSTORE`/`CONFIG_SMMSTORE_V2` are
actually turned on so `SmmStorePei` uses it and publishes `gVariableFlashInfoHobGuid` — see
`05_next_steps.md` item 2 for exact settings.
