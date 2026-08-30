# Ranked next steps

## Status (2026-07-28): all source/config-level fixes are DONE — only a rebuild+boot remains

The following have been completed and verified in the working tree:

- ✅ `Dispatcher.c:2319-2321` `if (2 == PeimCount) { continue; }` — commented out (was the
  user's own test workaround, not a real fix).
- ✅ `FaultTolerantWritePei.c:577-580` hard-coded infinite loop — commented out by the user.
- ✅ `SecMain.c`'s `int test = 1; while (test);` — commented out by the user.
- ✅ `CONFIG_SMMSTORE`/`CONFIG_SMMSTORE_V2`/`CONFIG_SMMSTORE_SIZE=0x40000` — enabled in `.config`
  (applied via `merge_config.sh` + `conf --olddefconfig`; see `04_config_kconfig_chain.md`).
- ✅ Serial/SOL — `CONFIG_DRIVERS_UART_8250IO`, `CONFIG_DRIVERS_GENERIC_CBFS_SERIAL`,
  `CONFIG_EDK2_DASHARO_SYSTEM_FEATURES` (the previously-unnoticed second gate),
  `CONFIG_EDK2_HAVE_2ND_UART`, both `SERIAL_REDIRECTION_DEFAULT_ENABLE` options, and
  `CONFIG_EDK2_PRINT_SOL_STRINGS` — all enabled in `.config` (see `01_serial_sol.md` and
  `04_config_kconfig_chain.md`).

**None of this takes effect until rebuilt and reflashed** — everything above is a source/`.config`
change on disk only.

## 1. Rebuild and reflash (now the single blocking step)

```
make
# flash build/coreboot.rom
```
With `CONFIG_SMMSTORE` on, `FaultTolerantWritePei`'s entry point will run for real (the Dispatcher
skip that used to bypass it is gone) and should get real non-zero PCDs instead of hitting the
historic unsigned-underflow hang. With serial on, this boot will be the first one where
BlSupportPei's DEBUG()-macro output is actually visible.

## 2. Get a fresh runtime capture with serial AND VGA both connected

No log folder in `logs/` currently shows BlSupportPei's own debug output firing, and the two
newest build folders (`1405-05-02`, `1405-05-04`) have no runtime capture at all. After the
rebuild, do a real hardware boot with serial connected and save **both** the VGA and serial
output into a new dated `logs/` folder — this is the only way to directly resolve the "is
BlSupportPei's code actually running" question instead of inferring it from source diffs, and to
confirm whether `FaultTolerantWritePei` now completes cleanly with SMMSTORE enabled instead of
hanging.

## 3. Once BlSupportPei is confirmed running, watch specifically for

- `UsableLowMemTop 0x%lx` and `PeiMemBase: 0x%lx.` (already in source, `BlSupportPei.c`) —
  confirms whether Fix A (the `MemInfoCallback` running-max fix, still present/unchanged per
  `analyze_1405_04_31.md` §5a and re-verified in this pass) is producing a sane value on real
  Xeon-SP hardware. If `PeiMemBase` looks like `0xFFFFFFFFFC000000`-ish garbage, Fix B (the
  `ASSERT`-guard clamp proposed in the earlier report, never applied) is still needed as
  defense-in-depth.

## 4. Housekeeping (low priority, no functional impact)

- The EDK2 workspace submodule HEAD is on a detached ref matching an upstream tag
  (`edk2-stable201903-8790-gb8bfb09d99`) rather than a named local branch — same "orphaned
  commits" risk flagged in the prior report; `git checkout -b <name>` once debugging stabilizes.
- Don't spend effort on `VBIOS_UEFI_1.14.00.zip` / ASPEED VGA integration — confirmed unrelated
  to any of the three problems investigated (see `03_aspeed_vbios.md`).
