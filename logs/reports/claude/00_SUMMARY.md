# Gooxie G4DEL / ASRock SPC741D8 — Boot Debug Synthesis (2026-07-27, fixes applied 2026-07-28)

Compiled by cross-referencing: `analyze_1405_04_31.md`, `doc.zip` (`recipe.md`,
`offline_inspection/manual_doc/*`), the EDK2 submodule commit history
(`payloads/external/edk2/workspace/dasharo`), `logs/` capture folders, the current
`.config`, the Kconfig chain, `src/drivers/aspeed`, and `VBIOS_UEFI_1.14.00.zip`.

## TL;DR — what's actually going on right now

1. **UART/SOL — FIXED in `.config`, not yet rebuilt/flashed.** All six required options
   (`CONFIG_DRIVERS_UART_8250IO`, `CONFIG_DRIVERS_GENERIC_CBFS_SERIAL`,
   `CONFIG_EDK2_DASHARO_SYSTEM_FEATURES`, `CONFIG_EDK2_HAVE_2ND_UART`, both
   `SERIAL_REDIRECTION_DEFAULT_ENABLE` options, `CONFIG_EDK2_PRINT_SOL_STRINGS`) are now enabled,
   applied via coreboot's real Kconfig resolver (`merge_config.sh` + `conf --olddefconfig`).
   `CONFIG_EDK2_DASHARO_SYSTEM_FEATURES` was a previously-unnoticed second gate discovered while
   applying this fix — the original analysis only flagged `DRIVERS_UART_8250IO` as blocking the
   three EDK2 options, which was necessary but not sufficient. See `01_serial_sol.md` and
   `04_config_kconfig_chain.md`.

2. **`CONFIG_SMMSTORE` — FIXED in `.config`, not yet rebuilt/flashed.**
   `CONFIG_SMMSTORE`/`CONFIG_SMMSTORE_V2`/`CONFIG_SMMSTORE_SIZE=0x40000` are now enabled, matching
   `board.fmd`'s pre-existing 256K SMMSTORE region. This lets `SmmStorePei` publish
   `gVariableFlashInfoHobGuid`, giving `FaultTolerantWritePei` real non-zero PCDs instead of the
   `[PcdsDynamicDefault]` zeros that caused the historic unsigned-underflow hang.

3. **Status as of 2026-07-27→28: all three debug artifacts identified in the original pass have
   been reverted by the user, and are confirmed still reverted:**
   - `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c:2319-2321` — the
     `if (2 == PeimCount) { continue; }` skip was the user's own deliberate test (added to
     isolate the FaultTolerantWritePei hang), now **commented out**. PEIM[2] dispatch is
     restored to normal.
   - `MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c:577-580` and
     `DasharoPayloadPkg/SecCore/SecMain.c` (post `PeiCoreEntryPoint` call) — the hard-coded
     infinite-loop debug traps (`while (i) { i = 20; }` / `while(test);`) were also the user's
     own bisection scaffolding, now **commented out** by the user directly.
   - **Important consequence, now resolved:** with the Dispatcher skip removed,
     `FaultTolerantWritePei`'s entry point will run again on the next build — but with
     `CONFIG_SMMSTORE` now enabled (item 2 above), the underlying unsigned-underflow bug in
     `WorkSpaceInSpareArea` (`FaultTolerantWritePei.c:528-539`, still present in source, but
     should no longer be *triggered* since its inputs will no longer be zero) should not
     manifest. **This is not yet confirmed on real hardware** — see item 4.
   - See `02_blsupportpei_hob.md` for the full trace.

**Only remaining step for items 1-3: rebuild (`make`) and reflash.** Everything above is a
source/`.config` change on disk that has not yet been exercised on hardware.

4. **ASPEED / `VBIOS_UEFI_1.14.00.zip` integration is NOT required** for serial, RAM, or HOB.
   The zip is prebuilt **BMC-side** GOP/VBIOS firmware (AST2500/2600/2700 option-ROM binaries
   + DOS/UEFI flashing utilities for the BMC's own flash) — no source code, and no
   relationship to coreboot's boot path. coreboot's own `src/drivers/aspeed` (VGA POST driver)
   and `src/superio/aspeed/ast2400` (UART routing, already active) are unrelated to this zip
   and already Kconfig-selected. See `03_aspeed_vbios.md`.

5. **No runtime capture exists for the newest builds — this is the next thing needed.**
   `logs/1405-05-02/` and `logs/1405-05-04/` contain build logs only — no VGA/serial capture
   block. The last real on-hardware trace is `logs/1405-04-21/`, which still ends at PEIM[2]
   (`FaultTolerantWritePei`, FileHandle `0x829FE8`) with no BlSupportPei output — predating all
   the fixes above. **A fresh boot with the current tree (post rebuild+reflash) is required to
   confirm items 1-3 actually resolved the hang and the serial/HOB visibility problem** —
   nothing here is confirmed on real hardware yet.

## Files in this report set

- `01_serial_sol.md` — UART/SOL root cause and fix steps
- `02_blsupportpei_hob.md` — PEI dispatcher / FaultTolerantWritePei / BlSupportPei / HOB deep trace
- `03_aspeed_vbios.md` — ASPEED driver + VBIOS zip relevance assessment
- `04_config_kconfig_chain.md` — Kconfig/config.asrock_spc741d8 chain reference
- `05_next_steps.md` — ranked, concrete action list

## Contradictions found between docs / Kconfig / source

- `analyze_1405_04_31.md` (§5a) states the `MemInfoCallback` fix (Fix A) "is confirmed present."
  This is still true in the current tree (unchanged since that report — verified again in this
  pass). That report's §6 unsigned-underflow diagram for `FaultTolerantWritePei` (PEIM[2]) would
  have been directly applicable again once the Dispatcher `continue`-skip was reverted — but
  `CONFIG_SMMSTORE` has now also been enabled (see `04_config_kconfig_chain.md`), so the loop's
  inputs should no longer be zero. Needs hardware confirmation (see item 5 above).
- `manual_doc/05_verify_pei_core_image.md` reports "0 PEI files found" in PEIFV via a naive
  ELF-offset walk — this contradicts `recipe.md`'s authoritative byte-exact FFS walk (1 SEC_CORE
  + 1 PEI_CORE + 13 PEIMs). This is a parser artifact of that specific script, not a real defect;
  don't let it resurface as a "PEI Core missing" concern.
- `board.fmd` was edited twice for SMMSTORE (`SMMSTORE is added in board.fmd` ×2 in git log) — the
  FMAP region existed on disk before the matching `CONFIG_SMMSTORE` Kconfig option did. This
  mismatch has now been resolved (see `04_config_kconfig_chain.md`).
