# Gooxi G4DEL coreboot and EDK2

## Current status — 2026-11-17

The documented 99-commit history cutoff is coreboot
`253bdcd0bf46250237f56930d6f05bfd20d9bbfc`; its paired EDK2 history cutoff is
`f285869ec015115655e1fb6746bebdfa2a6ff85b`. EDK2 follow-up
`e606437817b263d7e2a64374f6f7e63ef83f14cd` clarifies the EBS success marker.
The current coreboot source revision is
`11ab134819afe08493570794397992ab68e82652` (`fix/os-startup`), and the current G4DEL build includes that
commit and EDK2 `e606437817b263d7e2a64374f6f7e63ef83f14cd`:

```sh
make -j4
```

It produced payload SHA-256
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0` and ROM
SHA-256 `beffcba6b235413b2ba80d39f841887dd89635ffb71d90e604c43ee85853bf2f`.
These are local build artifacts; the server's flashed image is unknown until
a full-chip readback is hashed. The supplied Kali screenshot reaches the old
`OS EBS MAP ACCEPTED` marker, printed before EBS cleanup finished. New source
distinguishes map-key acceptance from `OS EBS CORE RETURNING` immediately
before the service returns. Coreboot also logs ACPI domain name/socket/stack/
bus mappings to correlate Linux AML diagnostics without changing generated
tables. Both changes are present in the rebuilt artifacts; no physical boot
has yet validated their runtime output. Fedora's separate log reaches 128 CPUs, initramfs unpack,
ASPEED graphics, and GDM service start, but also records unresolved ACPI names;
it does not explain Kali behavior or prove an interactive desktop. The OS
failure and root cause remain unresolved.

The factory image `/home/uefi/Downloads/server.bin` was recorded in the
[factory inventory](factory_image_inventory.md) with SHA-256
`db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`. It is a
reference image, not a guarantee about current chip contents. Preserve the
factory descriptor, ME, GbE, and Device Exp1 regions according to that
inventory; never commit proprietary factory images or extracted blobs.

## Read and navigate

- [Boot architecture, memory map, SOL, graphics, USB, menu, and Shell](boot_flow_memory_map.md)
- [Engineering timeline and all coreboot commits since `dasharo`](project_history.md)
- [Problem/evidence ledger](problem_ledger.md)
- [Alphabetical glossary and keywords](glossary.md)
- [Specifications, upstream references, and community channels](references.md)
- [Factory image and BIOS inventory](factory_image_inventory.md)
- [OS startup findings and Linux parameters](../g4del_os_startup.md)
- [Kali trace evidence and next-boot procedure](../g4del_kali_boot_trace.md)
- [Historical preboot comparison](../g4del_preboot_failure_isolation.md)

The [coreboot topic map](../g4del_branch_map.md), [DXE investigation](../dxe.md),
[status/test plan](../g4del_status_and_test_plan.md), and
[bring-up plan](../plan.md) preserve dated investigation snapshots. Their
status applies to the evidence available when each was written; current
branch/build/test status is maintained here and in the problem ledger.

## Platform and build facts

`configs/config.gooxi_g4del` selects the G4DEL target. The current configuration
uses a 64 MiB image, at most two sockets, serial console at 115200 baud on I/O
base `0x2f8`, a linear framebuffer, and SMMSTORE v2. EDK2 is selected at
`origin/fix/os-startup` by the tracked configuration; the build command above
explicitly selected the local branch so the exact commit used was
`f285869e…`. The dual-socket IIO routing is a porting input and must not be
treated as hardware-verified without a board trace and PCI inventory.

Build after providing the required local factory inputs as documented in the
[factory inventory](factory_image_inventory.md):

```sh
make defconfig KBUILD_DEFCONFIG=configs/config.gooxi_g4del
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
python3 util/g4del_stock.py /path/to/server.bin --compare build/coreboot.rom
python3 util/g4del_stock.py build/coreboot.rom --verify-layout
build/util/cbfstool/cbfstool build/coreboot.rom print
```

Record both source revisions, the input FSP/factory image identity, build
hashes, and full-chip readback hash with every hardware result. Comparison and
layout checks validate image structure; they do not establish that the board
boots.

## Boot chain and source map

```text
SPI flash / Intel descriptor
  -> coreboot bootblock -> romstage + FSP-M -> postcar -> ramstage + FSP-S
  -> CBFS payload and coreboot tables
  -> EDK2 SEC -> PEI / HOBs -> DXE IPL -> DXE dispatcher -> BDS
  -> console / USB / boot manager -> Shell or EFI OS loader
  -> ExitBootServices -> OS loader and kernel
```

| Responsibility | Main source or artifact |
| --- | --- |
| Board Kconfig, bootblock and memory/IIO setup | `src/mainboard/gooxi/g4del/` |
| Flash regions | `src/mainboard/gooxi/g4del/board.fmd` and `doc/g4del/factory_image_inventory.md` |
| FSP and SoC initialization | `src/soc/intel/xeon_sp/` and `3rdparty/fsp/EagleStreamFspBinPkg/` |
| Payload packing, UART and EDK2 build flags | `payloads/external/edk2/Makefile` |
| SEC/PEI/DXE/BDS/Shell implementation | Nested EDK2 checkout at `payloads/external/edk2/workspace/mahdi-sahebi/` |
| G4DEL evidence and source boundaries | [Boot-flow report](boot_flow_memory_map.md) and [problem ledger](problem_ledger.md) |

For firmware-stage concepts see the [glossary](glossary.md); for external
specifications see [references](references.md). The upstream [coreboot stage
overview](https://doc.coreboot.org/getting_started/architecture.html) and
[CBFS documentation](https://doc.coreboot.org/lib/cbfs.html) explain the
general design but do not replace the board source.

## Hardware acceptance

Acceptance requires a reset-to-OS capture tied to a verified ROM readback,
with serial and VGA recorded together. Verify stage begin/return pairs,
DXE/BDS console initialization, keyboard/menu interaction, USB inventory,
Shell commands, and then each OS path separately. For Kali, preserve the raw
SOL/VGA trace through the EFI handoff and Linux output; use the one-boot
parameters in the [OS startup report](../g4del_os_startup.md). No OS support or
root-cause claim is complete until a reproducible board test demonstrates it.
