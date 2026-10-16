# ASPEED driver / `VBIOS_UEFI_1.14.00.zip` — relevance assessment

## Verdict: not required for serial, RAM/memory init, or HOB/BlSupportPei

### What the zip actually is

`VBIOS_UEFI_1.14.00.zip` extracts to four sibling folders — `UEFI` (GOP `.efi` drivers for
X64/AARCH64, AST2500/2600/2700), `UEFI_ROM` (same GOP drivers as legacy option-ROM `.rom`
blobs), `VBIOS` (legacy 16-bit VGA BIOS `.800` blobs with DOS `FLASH.EXE` flashing tools), and
`VBIOS+UEFI ROM` (merged binaries + `flash.efi`). The top-level `readme.txt` says these files
are meant to be renamed and copied into the **BMC's own firmware** (`fmc_tool/prebuild`), e.g.
`uefi_x64_2700.rom → uefi_x64_ast2700.bin`. There is **no source code** in the archive at all —
no `.inf`/`.dec`/`.dsc`, no C files, just prebuilt binaries and text release notes. This is
ASPEED's standard AST2500/2600/2700 GOP+VBIOS binary distribution for **the BMC's graphics
option ROM** — it has no relationship to the coreboot host-side build.

### coreboot's own ASPEED code (separate from the zip, already present and building)

- `src/superio/aspeed/ast2400` — Super I/O driver providing `aspeed_enable_serial()`, used in
  `bootblock.c` for UART routing. Already active, unrelated to the zip.
- `src/drivers/aspeed/{common,ast2050}` — a VGA framebuffer/DRM-style POST driver ported from
  Linux's `ast` DRM driver. Gated by `MAINBOARD_DO_NATIVE_VGA_INIT` / `GENERIC_LINEAR_FRAMEBUFFER`.

### Kconfig chain

`src/mainboard/asrock/spc741d8/Kconfig` selects both `SUPERIO_ASPEED_AST2400` and
`DRIVERS_ASPEED_AST2050` unconditionally. `DRIVERS_ASPEED_AST2050` selects
`DRIVERS_ASPEED_AST_COMMON`, which conditionally selects `HAVE_LINEAR_FRAMEBUFFER` /
`HAVE_VGA_TEXT_FRAMEBUFFER` / `VGA` / `SOFTWARE_I2C` — but only `if MAINBOARD_DO_NATIVE_VGA_INIT`
etc., none of which are enabled in `configs/config.asrock_spc741d8`. `devicetree.cb` only
instantiates `chip superio/aspeed/ast2400` (UART), never anything under `drivers/aspeed`. So the
VGA POST driver is Kconfig-selected (compiled in) but not actually wired to run — at most this
affects a coreboot-native VGA text console, which is cosmetic, not functional for boot.

### Why it's irrelevant to the three things asked about

- **Serial**: handled by `superio/aspeed/ast2400` + `aspeed_enable_serial()`, already active,
  unrelated to the BMC GOP binaries in the zip.
- **RAM/memory init**: goes through FSP-M on Xeon-SP (`src/soc/intel/xeon_sp/spr`), has zero
  interaction with ASPEED VGA or BMC firmware.
- **HOB / BlSupportPei**: PEI-phase EDK2 constructs tied to the SoC FSP binary, not to any
  graphics driver or BMC-side option ROM.

### Recommendation

Do not spend time integrating `VBIOS_UEFI_1.14.00.zip` as part of debugging serial/RAM/HOB —
it's the wrong layer entirely (BMC graphics firmware vs. host UEFI/PEI). If a VGA console is
wanted later purely for cosmetic/manufacturing purposes, that's a separate, low-priority task
involving `MAINBOARD_DO_NATIVE_VGA_INIT` in Kconfig, not this zip.
