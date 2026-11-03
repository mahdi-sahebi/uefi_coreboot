# Gooxi G4DEL coreboot and EDK2 bring-up

## Goal and current evidence

This project replaces the factory AMI BIOS region with an open-source coreboot
and EDK2 payload while preserving the board's factory Intel flash descriptor,
ME, GbE, and Device Exp1 data. The first acceptance target is an interactive
EDK2 Boot Manager menu with the built-in Shell selectable. The next target is
USB enumeration visible on the VGA text/graphics console and SOL, followed by
Shell commands that inspect attached devices and USB storage.

The user reports 64 GiB RAM, four fans, a BMC, and SOL. The flash image is
64 MiB. The target is treated as a two-socket Gooxi G4DEL; the CPU models,
installed logical CPU count, and physical DIMM slot count remain unmeasured.
The Archer City dual-socket IIO table is provisional until the G4DEL port map
is confirmed. The Gooxi SL201-G4 family manual is a platform-family reference,
not evidence for every G4DEL board revision or PCI device ID.

Latest hardware evidence is `/home/uefi/workspace/log/1405-08-02/serial.log`.
It shows FSP-M and FSP-S return, PEI dispatch completes, DXE Core starts
dispatching, and `CpuDxe` enters without a return marker. It does not show
GOP, BDS, an interactive menu, USB enumeration, or Shell. Source and package
checks below are not a substitute for a new reset-to-SOL/VGA board capture.

## Documents and diagrams

- [Factory image, BIOS volume inventory, and offline evidence](factory_image_inventory.md)
- [Flash map and complete coreboot-to-EDK2 boot flow](boot_flow_memory_map.md)
- [Factory BIOS metadata tree (3,691 parsed nodes)](server_bios_inventory.csv)
- [64 MiB flash map diagram](images/flash-map.svg)
- [Boot flow diagram](images/boot-flow.svg)

The two diagrams use exact addresses in their labels. The internal BIOS
subregion view is enlarged to keep the 256 KiB SMMSTORE and 2 KiB FMAP visible.

## Build and local inputs

Use `configs/config.gooxi_g4del` as the build interface. It pins EDK2 to
`ead1df0f527f48955d245a1f50072c6f22f6f007`, enables the linear framebuffer,
and enables the EDK2 Shell. Coreboot's target configuration sets two sockets,
64 MiB ROM, SOL UART I/O base `0x2f8` at 115200 baud, SMMSTORE v2, and disables
descriptor unlocking and ME disabling.

The local factory input procedure is documented in
[`factory_image_inventory.md`](factory_image_inventory.md). Extract only the
four non-BIOS regions into the ignored local `deps/blobs/mainboard/gooxi/g4del`
directory. Never commit `server.bin`, extracted factory blobs, or the vendor
BIOS. The coreboot build reconstructs the BIOS region and adds the project's
open-source `Documentation/coreboot_logo.bmp` to the BOOTSPLASH area.

```sh
make defconfig KBUILD_DEFCONFIG=configs/config.gooxi_g4del
make -j8
python3 util/g4del_stock.py /path/to/server.bin --compare build/coreboot.rom
python3 util/g4del_stock.py build/coreboot.rom --verify-layout
build/util/cbfstool/cbfstool build/coreboot.rom print
```

The compare command checks the descriptor, GbE, ME, and Device Exp1 byte for
byte. The layout command checks IFD/FMAP coverage, the BIOS subregions and
SMMSTORE geometry. Record the build's source commits and ROM hash beside any
hardware test; a build hash alone does not show what is on the board.

## Source map

| Area | Source |
| --- | --- |
| G4DEL board target and provisional dual-socket IIO setup | `src/mainboard/gooxi/g4del/{Kconfig,romstage.c,include/sprsp_ac_iio.h}` |
| 64 MiB flash layout | `src/mainboard/gooxi/g4del/board.fmd` |
| Early LPC/SUART1 SOL setup | `src/mainboard/gooxi/g4del/bootblock.c` |
| FSP and payload/flash diagnostics | `src/soc/intel/xeon_sp/{romstage.c,spr/chip.c}` and `src/mainboard/gooxi/g4del/ramstage.c` |
| ASPEED video and framebuffer handoff | `src/drivers/aspeed/ast2050/ast2050.c`, `src/drivers/aspeed/common/ast_mode_corebootfb.c`, `src/lib/coreboot_table.c` |
| EDK2 build flags, UART base, and G4DEL USB diagnostic define | `payloads/external/edk2/Makefile` |
| EDK2 USB page and menu/Shell code | EDK2 pinned EDK2 commit above; details and links are in the EDK2 [08-02 analysis](../../payloads/external/edk2/workspace/mahdi-sahebi/doc/1405-08-02/analysis.md) and [graphics/menu/Shell status](../../payloads/external/edk2/workspace/mahdi-sahebi/doc/1405-08-02/graphics_splash_shell.md) in the local workspace. |

## External references

- [Gooxi SL201-G4 family manual](https://www.gooxi.us/uploadfiles/2024/01/20240122163352960.pdf) — family-level background, not exact G4DEL proof.
- Coreboot [Intel Flash Descriptor layout](https://doc.coreboot.org/util/ifdtool/layout.html) and [FMD/Flashmap](https://doc.coreboot.org/lib/flashmap.html).
- Coreboot [SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html) and [architecture](https://doc.coreboot.org/getting_started/architecture.html).
- UEFI 2.11 [USB protocols](https://uefi.org/specs/UEFI/2.11/17_Protocols_USB_Support.html).
- [UEFITool and UEFIExtract releases](https://github.com/LongSoft/UEFITool/releases).

## Hardware acceptance checklist

The goal remains open until a reset-to-SOL/VGA test tied to a readback hash
shows `CpuDxe` return, DXE dispatcher completion, BDS and GOP/console setup,
the logo, the inventory page, USB devices on both serial and VGA, menu key
input, Shell launch, and a Shell command response. The test report should
include BMC power/POST evidence and the exact ROM hash. See
[`boot_flow_memory_map.md`](boot_flow_memory_map.md) for the expected marker
order and evidence boundaries.
