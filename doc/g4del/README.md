# Gooxi G4DEL coreboot and EDK2

## Current status — 2026-11-23

The project history documents all 105 coreboot commits from the `dasharo`
merge base through `8abea92b9663bffe84bd962344d9acf13f92a146`
(`fix/os-startup`). Later diagnostics and evidence updates are recorded in
the dated reports below. The latest integrated build is recorded here:

```sh
make -j6
```

After the serial-first logging change, `make clean` followed by `make -j6`
completed on 2026-11-23 using coreboot source `04d4682674` and EDK2 source
`a80b1ae6d2`. The ROM SHA-256 is
`957e2a81053e15bfffb2cc08785bb1e5ee4eabf307aed172e2d3c6fd9070decd`; its
filesystem modification time is `2026-11-23 23:07:12 +04`. Extracting
`fallback/payload` confirmed the VGA row-echo string is absent and BDS/OS-entry
markers remain. This is a local build and payload inspection, not a flashed
image or a hardware boot test. The detailed earlier 2026-11-17 build produced
payload SHA-256
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0` and ROM
SHA-256 `f53971e353dc11143c53dc380b876e7ab53b302c02bc92245b826a303d5c5239`.
The ramstage contains the new per-domain VT-d missing-device warnings and the
guard for a missing primary domain0 IOMMU. These are local build artifacts;
the server's flashed image is unknown until a full-chip readback is hashed.
The supplied Kali screenshot reaches the old
`OS EBS MAP ACCEPTED` marker, printed before EBS cleanup finished. Two later
SOL captures from 1405-08-26 and 1405-08-27 reach
`OS EBS CORE RETURNING status=EFI_SUCCESS` at elapsed 133 and 121 seconds,
but neither contains post-EBS Linux output; both use `quiet splash` without
serial-console parameters. New source
distinguishes map-key acceptance from `OS EBS CORE RETURNING` immediately
before the service returns. Coreboot also logs ACPI domain name/socket/stack/
bus mappings to correlate Linux AML diagnostics without changing generated
tables. The ACPI hardening skips domains without VT-d and never passes a null
primary device to DRHD generation; present secondary DRHD entries remain
eligible for inclusion. The build contains the diagnostics, but only runtime
ACPI table inspection can confirm the generated entries. The operator flashed
the previous ROM on 2026-11-21, but no chip readback or image hash was supplied,
so its contents are not independently identified. The newer local build has
not been flashed. Fedora's separate log reaches 128 CPUs, initramfs
unpack, ASPEED graphics, and GDM service start, but also records unresolved
ACPI names; it does not explain Kali behavior or prove an interactive desktop.
The OS failure and root cause remain unresolved.

The 2026-11-23 serial/VGA register compares the reviewed capture sets from
1405-08-25 through 1405-09-02/3. The latest VGA sequence reaches Shell and
begins a `BOOTX64.EFI` attempt, but its SOL archives stop around DXE checkpoint
16; one SOL archive is duplicated from an earlier set. The current evidence
still cannot identify the OS failure point. See the
[capture register](../g4del_serial_vga_capture_register.md) and the
[latest Shell/OS trace](../g4del_shell_gate_and_boot_trace_2026-11-23.md).

An additional local build completed on 2026-11-21 after the framebuffer
backup guard was added to the nested EDK2 source. It produced payload SHA-256
`88086e6f817e7ff6bcb007bbe82d09eb07b794389918d4661da80af830946b1e` and ROM
SHA-256 `0e30f0628a477d87b9cca14cb5c51c51873b84b56dad1ac46e76368b420b7bf8`.
This new ROM has not been flashed or hardware-verified.

### 2026-11-21 serial and VGA evidence (pre-fix capture)

The files in `/home/uefi/workspace/logs/1405-08-29/` were captured before the
later EDK2 framebuffer-restoration change. `SOLHostCapture_Archive_4_11212026_14_03_07.log`
shows Shell launching `FS0:\EFI\BOOT\BOOTX64.EFI`, the Kali 7.0.12 kernel
and initrd path/options, 76 of 76 DXE drivers loaded and executed without a
recorded load/run failure, and `OS EBS CORE RETURNING status=EFI_SUCCESS` at
103 seconds. The options contain `ro quiet splash` and no Linux serial-console
parameters. There is no post-EBS Linux output, so this capture does not locate
a kernel stall or prove Linux startup completed.

The two 09:32/09:33 VGA screenshots show the Kali graphical background still
visible below a large opaque black diagnostics region. The region contains
firmware handoff/EBS text and garbled remnants; the 10:34 BMC screenshot shows
the same overlay. This is consistent with firmware diagnostics obscuring the
display during handoff, not evidence that the OS is fully booted. The
framebuffer save/restore fix was committed after these captures; a fresh
board capture is needed to verify it restores the display and to determine
whether Linux then progresses. The sibling
`SOLHostCapture_Archive_1_11212026_13_56_09.log` records ASPEED VGA function
`1a03:2000`, a 1024x768x32 linear framebuffer at `0x94000000`, and fallback
to 1024x768 after the EDID read failed; these messages alone do not establish
a display failure. See [P11](problem_ledger.md#p11--fedora-kali-ubuntu-and-windows-boot-reports).

### 2026-11-21 post-flash capture

The follow-up bundle in `/home/uefi/workspace/logs/1405-08-29/2/` proves the
UEFI Shell is interactive: the 11:44 screencast shows a Shell prompt, one
mistyped `FS0:` command, then a successful switch to `FS0:\>`. It does not
show a Kali launch. `SOLHostCapture_Archive_1_11212026_16_12_06.log` ends at
the coreboot-to-EDK2 handoff. `SOLHostCapture_Archive_3_11212026_16_19_25.log`
contains PEI/FTW, GOP setup, DXE memory initialization, and dispatcher start,
but ends while DXE drivers are still being discovered; it has no Shell, OS
launch, or EBS-return checkpoint. Repeated 98,344-byte pool allocation
failures appear before DXE memory services are ready; DXE initialization then
continues, so these lines alone do not show a fatal allocation failure.

The 11:49 VGA stills show the Kali background beneath the diagnostics overlay
and an `OS EBS MAP KEY ACCEPTED` message. They do not show the final
`OS EBS CORE RETURNING` marker or prove the successful EBS cleanup path ran.
Since the final marker is serial-only, the supplied post-flash SOL capture
does not verify the framebuffer restoration. The capture also reports both
FTW workspaces invalid while installing the completion PPI; see
[P07](problem_ledger.md#p07--smmstore-and-ftw).

The matching SOL log reports seven failed allocations of 98,344 bytes, which
is the pool request for the 98,304-byte (24 x 1024 x 4) screen backup plus
allocator overhead. The logger previously drew the overlay despite backup
failure; once allocation worked, it could save its own text as the background.
The EDK2 source now skips framebuffer drawing until that backup succeeds.
The new payload and ROM were rebuilt locally, but not flashed or tested.
Reflash that build, then capture uninterrupted SOL/VGA through the
`OS EBS CORE RETURNING` marker and subsequent Linux startup to verify the fix.

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
