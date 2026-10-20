# Kconfig / config chain reference — ASRock SPC741D8 (Gooxie G4DEL)

## Golden reference vs. current `.config` — STATUS: applied 2026-07-28

`configs/config.asrock_spc741d8` is the project's known-good overlay. It does **not** itself set
`CONFIG_SMMSTORE` (that line is absent from the overlay file — it relies on Kconfig defaults /
manual `menuconfig` selection), but the overlay's other serial-related options match what
`analyze_1405_04_31.md` recorded as the golden reference. The live `.config` originally diverged
from that overlay on exactly the same points already identified — **all have now been fixed**:

| Symbol | Golden overlay intent | Original `.config` | Current `.config` |
|---|---|---|---|
| `CONFIG_DRIVERS_UART_8250IO` | on (Kconfig default `y` for SuperIO x86 boards) | off | **on** |
| `CONFIG_DRIVERS_GENERIC_CBFS_SERIAL` | on | off | **on** |
| `CONFIG_EDK2_DASHARO_SYSTEM_FEATURES` | on (undocumented 2nd gate, found during fix) | off | **on** |
| `CONFIG_EDK2_HAVE_2ND_UART` | on | absent (dependency not met) | **on** |
| `CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION_DEFAULT_ENABLE` | on | absent | **on** |
| `CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION2_DEFAULT_ENABLE` | on | absent | **on** |
| `CONFIG_EDK2_PRINT_SOL_STRINGS` | — | off | **on** |
| `CONFIG_SMMSTORE` / `CONFIG_SMMSTORE_V2` / `CONFIG_SMMSTORE_SIZE` | needs manual enable | off | **on / on / 0x40000** |

Backups of the pre-fix `.config` are preserved at `.config.bak-before-smmstore-fix` and
`.config.bak-before-serial-fix` (sequential fixes, applied in that order).

### The undocumented second gate

The original analysis (and the first pass of this fix) assumed only `DRIVERS_UART_8250IO` gated
the three EDK2 serial-redirection symbols. Applying just that left them still invisible. Root
cause: all three (`EDK2_HAVE_2ND_UART`, both `EDK2_DASHARO_SERIAL_REDIRECTION*_DEFAULT_ENABLE`)
sit inside `if EDK2_DASHARO_SYSTEM_FEATURES … endif`
(`payloads/external/edk2/Kconfig.dasharo:86-299`), a second Kconfig gate that was also off.
Enabling `CONFIG_EDK2_DASHARO_SYSTEM_FEATURES=y` was required in addition to the UART fix. This
did not pull in unrelated Dasharo submenus — Security/ME/USB/Network/Chipset/Power/PCI/Memory
options all remain explicitly off; only the serial-redirection block was affected.

## Mainboard Kconfig (`src/mainboard/asrock/spc741d8/Kconfig`)

Unconditionally selects:
- `SUPERIO_ASPEED_AST2400` → selects `SUPERIO_ASPEED_COMMON_PRE_RAM` (pre-RAM UART init only)
- `DRIVERS_ASPEED_AST2050` → selects `DRIVERS_ASPEED_AST_COMMON` → conditionally selects
  `HAVE_LINEAR_FRAMEBUFFER` / `HAVE_VGA_TEXT_FRAMEBUFFER` / `VGA` / `SOFTWARE_I2C`, gated on
  `MAINBOARD_DO_NATIVE_VGA_INIT` / `VGA_TEXT_FRAMEBUFFER` / `GENERIC_LINEAR_FRAMEBUFFER` — none
  enabled in the current config, so the VGA POST path is compiled but dormant (see
  `03_aspeed_vbios.md`).

No ASPEED references found in `src/soc/intel/xeon_sp/spr/Kconfig` or `src/acpi/Kconfig` — those
subsystems are unrelated to ASPEED.

## `board.fmd` — SMMSTORE region now matched by Kconfig

`src/mainboard/asrock/spc741d8/board.fmd:10` declares:
```
SMMSTORE(PRESERVE) 256K
```
This FMAP region was already present on disk (added by the two `SMMSTORE is added in board.fmd`
commits). `CONFIG_SMMSTORE` is now also enabled in `.config` (`CONFIG_SMMSTORE_SIZE=0x40000`
matches the 256K region exactly, and matches the intent already recorded in `note.txt` at the
arya root), so `SmmStorePei` will use this region and publish `gVariableFlashInfoHobGuid` —
resolving the root cause described in `02_blsupportpei_hob.md` §5 and
`analyze_1405_04_31.md` §5a/§6.

## Applied `.config` changes (done, via `merge_config.sh` + `conf --olddefconfig`)

```
CONFIG_DRIVERS_UART_8250IO=y
CONFIG_DRIVERS_GENERIC_CBFS_SERIAL=y
CONFIG_EDK2_DASHARO_SYSTEM_FEATURES=y
CONFIG_EDK2_HAVE_2ND_UART=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION_DEFAULT_ENABLE=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION2_DEFAULT_ENABLE=y
CONFIG_EDK2_PRINT_SOL_STRINGS=y
CONFIG_SMMSTORE=y
CONFIG_SMMSTORE_V2=y
CONFIG_SMMSTORE_SIZE=0x40000
```

**Remaining step: rebuild (`make`) and reflash** — none of this takes effect until a new ROM is
built and flashed to hardware.
