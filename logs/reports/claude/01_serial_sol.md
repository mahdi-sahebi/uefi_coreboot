# Serial / SOL not working — root cause and fix

## Status: FIXED in `.config` (2026-07-28), not yet rebuilt/reflashed

Original state (verified 2026-07-27):

```
# CONFIG_DRIVERS_UART_8250IO is not set
# CONFIG_SMMSTORE is not set
# CONFIG_DRIVERS_GENERIC_CBFS_SERIAL is not set
# CONFIG_EDK2_PRINT_SOL_STRINGS is not set
```

`CONFIG_EDK2_HAVE_2ND_UART` and both `CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION*_DEFAULT_ENABLE`
options didn't even appear in `.config`. The original analysis attributed this solely to
`EDK2_HAVE_2ND_UART` depending on `DRIVERS_UART_8250IO` — true, but **incomplete**: applying only
that fix still left the three EDK2 serial-redirection options hidden. Root-caused during the
actual fix (see below): all three also live inside `if EDK2_DASHARO_SYSTEM_FEATURES` /
`endif` (`payloads/external/edk2/Kconfig.dasharo:86-299`), a second, previously-unnoticed gate
that was also off.

**All of this has now been applied**, via `util/kconfig/merge_config.sh` + `conf --olddefconfig`
(coreboot's real Kconfig resolver, not hand-edited) so dependent/derived symbols were computed
correctly rather than guessed:

```
CONFIG_DRIVERS_UART_8250IO=y
CONFIG_DRIVERS_GENERIC_CBFS_SERIAL=y
CONFIG_EDK2_DASHARO_SYSTEM_FEATURES=y   ← the second gate, discovered during the fix
CONFIG_EDK2_HAVE_2ND_UART=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION_DEFAULT_ENABLE=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION2_DEFAULT_ENABLE=y
CONFIG_EDK2_PRINT_SOL_STRINGS=y
```

Derived side effects, confirmed correct: `CONFIG_CONSOLE_SERIAL=y`, `CONFIG_UART_FOR_CONSOLE=0`,
`CONFIG_TTYS0_BASE=0x3f8`, `CONFIG_TTYS0_BAUD=115200`, `CONFIG_CONSOLE_SERIAL_115200=y` (matches
COM1 wiring already confirmed correct in `bootblock.c`). Enabling `EDK2_DASHARO_SYSTEM_FEATURES`
did **not** pull in unrelated Dasharo submenus — Security/ME/USB/Network/Chipset/Power/PCI/Memory
config options all remain explicitly unset, only the serial-redirection block was affected.

Pre-fix config preserved at `.config.bak-before-serial-fix` for diff/revert if needed.

**Still required: rebuild and reflash** — this is a `.config` change only, not yet baked into a
built ROM.

## Hardware path is fine (unchanged conclusion)

`src/mainboard/asrock/spc741d8/bootblock.c` still correctly opens the SuperIO UART windows and
calls `aspeed_enable_serial(ast_serial_dev, CONFIG_TTYS0_BASE)` against the AST2600-compatible
`superio/aspeed/ast2400` chip declared in `devicetree.cb`. SOL runs over eSPI to the BMC
(per `logs/analysis_report.md`), not a physical DE-9 port — confirmed via
`ipmitool -I lanplus ... sol activate` usage recorded in that file. None of this requires the
`VBIOS_UEFI_1.14.00.zip` ASPEED GOP/VBIOS package (see `03_aspeed_vbios.md`) — that's BMC
graphics firmware, an entirely separate concern from UART routing.

## Remaining step: rebuild and reflash

The `.config` fix above is applied but not yet exercised on hardware:
```
make
# flash the resulting build/coreboot.rom
```

## Why this matters for the current debugging session specifically

Once reflashed, `BlSupportPei.c`'s existing `DEBUG((DEBUG_INFO, "UsableLowMemTop 0x%lx\n", ...))`
and `"PeiMemBase: 0x%lx.\n"` lines (already present in source, confirmed unchanged) will actually
reach a visible channel for the first time. With UART off, **any DEBUG()-macro output from
BlSupportPei was going nowhere** — a second, independent reason "BlSupportPei logs aren't shown"
beyond the Dispatcher-skip issue covered in `02_blsupportpei_hob.md` (that issue has also since
been reverted by the user — see that file). Recent commits in the EDK2 submodule
(`4536a5aa6f` "Logs of BlSupportPei are added") replaced some `DEBUG()` calls with
`edkii_vga_sprintf()` (VGA framebuffer text) specifically to work around serial being off — but
not all of them; some BlSupportPei debug lines were serial-only DEBUG() calls that were invisible
until now. The next boot capture should be done with **both** VGA and serial connected
simultaneously to get two independent traces of the same boot.
