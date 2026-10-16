# Coreboot + EDK II UEFI Payload: Complete Build & Debug Guide
## ASRock SPC741D8-2L2T/BCM (Intel Xeon SP Sapphire Rapids)

**Date:** 2026-05-19  
**Branch:** `edk2-server` (from tag `asrock_spc741d8_v0.9.0`)  
**Author:** Mahdi Sahebi  

---

## Table of Contents

1. [Prerequisites and Tools](#1-prerequisites-and-tools)
2. [server.bin: Extracting and Inspecting the Original Firmware](#2-serverbin-extracting-and-inspecting-the-original-firmware)
3. [Flash Layout — board.fmd Explained](#3-flash-layout--boardfmd-explained)
4. [Device Tree — devicetree.cb Explained](#4-device-tree--devicetreecb-explained)
5. [Build Configuration — .config Explained](#5-build-configuration--config-explained)
6. [Branch Setup — Starting Clean from the Release Tag](#6-branch-setup--starting-clean-from-the-release-tag)
7. [EDK II Workspace — Setup and Git Tracking](#7-edk-ii-workspace--setup-and-git-tracking)
8. [Building the Firmware Image](#8-building-the-firmware-image)
9. [Offline Verification — cbfstool Image Inspection](#9-offline-verification--cbfstool-image-inspection)
10. [Flashing to Hardware](#10-flashing-to-hardware)
11. [HOB Construction: DasharoPayloadPkg vs UefiPayloadPkg](#11-hob-construction-dasharopayloadpkg-vs-uefipayloadpkg)
12. [Boot Sequence: From coreboot to UEFI Shell](#12-boot-sequence-from-coreboot-to-uefi-shell)
13. [Crash Analysis — PeiDispatcher Hang](#13-crash-analysis--peidispatcher-hang)
14. [Active Bugs and Fixes](#14-active-bugs-and-fixes)
15. [Debug Strategy](#15-debug-strategy)

---

## 1. Prerequisites and Tools

### 1.1 Host Packages

```bash
sudo apt-get install -y \
  build-essential git m4 bison flex curl wget unzip python3 \
  libssl-dev libncurses-dev nasm uuid-dev acpica-tools \
  flashrom ipmitool
```

### 1.2 Coreboot Cross-Compiler (build once)

```bash
cd /home/mahdi/repositories/coreboot/dasharo/coreboot
make crossgcc-i386 CPUS=$(nproc)
# Builds a 32-bit cross-compiler into xgcc/ (~20 min first time)
```

### 1.3 Coreboot Utilities (built from repo)

Build `cbfstool` and `ifdtool` from the coreboot source tree:

```bash
make -C util/cbfstool
make -C util/ifdtool
```

Expected output:
```
    HOSTCC     cbfstool/cbfstool (link)
make: Leaving directory '.../util/cbfstool'
   IFDTOOL
cc  ../ifdtool/ifdtool.o ... -o ../ifdtool/ifdtool
make: Leaving directory '.../util/ifdtool'
```

Binaries are at:
- `util/cbfstool/cbfstool`
- `util/ifdtool/ifdtool`

### 1.4 Tool Summary

| Tool | Location | Purpose |
|---|---|---|
| `ifdtool` | `util/ifdtool/ifdtool` | Parse Intel Flash Descriptor, extract/inject regions |
| `cbfstool` | `util/cbfstool/cbfstool` | Inspect/modify CBFS (Coreboot File System) inside the ROM |
| `flashrom` | system package | Read/write the SPI flash chip on the server |
| `ipmitool` | system package | Access BMC, activate serial-over-LAN (SOL) for serial debug |
| `hexdump` | system package | Binary inspection |
| `binwalk` | system package (optional) | Entropy/signature scan of firmware |

---

## 2. server.bin: Extracting and Inspecting the Original Firmware

### 2.1 What is server.bin

`/home/mahdi/repositories/reconstruction/server/server.bin` is a **64 MB** raw SPI flash dump of the server's original BIOS, taken with `flashrom` before any modifications. It contains four Intel-defined regions: Flash Descriptor, BIOS (AMI/Phoenix), Intel ME, and GbE NVM.

**Verify the file:**
```bash
file /home/mahdi/repositories/reconstruction/server/server.bin
ls -lh /home/mahdi/repositories/reconstruction/server/server.bin
sha256sum /home/mahdi/repositories/reconstruction/server/server.bin
```

Actual output:
```
Intel serial flash for PCH ROM
-rw-r--r-- 1 mahdi mahdi 64M Aug 20  2024 server.bin
db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465  server.bin
```

### 2.2 Dump the Intel Flash Descriptor

```bash
util/ifdtool/ifdtool -d /home/mahdi/repositories/reconstruction/server/server.bin
```

Key sections from actual output:
```
File is 67108864 bytes (64 MB)
Peculiar firmware descriptor, assuming Ibex Peak compatibility.
PCH Revision: 5 series Ibex Peak

FLMAP0:  0x00040003     NR=0 (no additional regions), FRBA=0x40, NC=1, FCBA=0x30
FLMAP1:  0x50100608     2 flash components

Found Region Section
FLREG0:  0x00000000   Flash Descriptor:  0x00000000 – 0x00000FFF  (4 KB)
FLREG1:  0x3fff3000   BIOS region:       0x03000000 – 0x03FFFFFF  (16 MB)
FLREG2:  0x2fef0003   Intel ME:          0x00003000 – 0x02FEFFFF  (~47.7 MB)
FLREG3:  0x00020001   GbE NVM:           0x00001000 – 0x00002FFF  (8 KB)
FLREG4:  0x00007fff   Platform Data:     unused

Found Component Section
FLCOMP   0x249c30f7
  Component 1 Density:  64 MB
  Component 2 Density:  32 MB
  Read Clock: 17 MHz  |  Fast Read: 50 MHz

AltMeDisable bit is not set    ← original ME is fully active

FLMSTR1 (Host CPU/BIOS): write access DISABLED for all regions except FD
```

**Critical observations:**
- The BIOS region (`SI_BIOS`) is only the **last 16 MB** (offsets `0x3000000–0x3FFFFFF`) of the 64 MB flash.
- Intel ME occupies **~47.7 MB**, the vast majority of the flash.
- `AltMeDisable bit is not set` means the original ME is fully enabled. Coreboot needs a neutralized ME (`me.bin` with HAP/AltMeDisable bit set).
- `Host CPU/BIOS Region Write Access: disabled` — the SPI flash is write-protected by the original firmware. You must unlock it (see Section 10).

### 2.3 Extract Regions from server.bin

```bash
cd /home/mahdi/repositories/reconstruction/server/
../../../coreboot/dasharo/coreboot/util/ifdtool/ifdtool -x server.bin
```

Produces:
```
flashregion_0_flashdescriptor.bin   4.0K
flashregion_1_bios.bin              16M    ← original AMI/Phoenix BIOS
flashregion_2_intel_me.bin          16M*   ← Intel ME (neutralized copy needed)
flashregion_3_gbe.bin               8.0K   ← GbE NVM (keep as-is)
```

*Note: The displayed ME region size from `ifdtool -x` differs from the actual physical layout
because ifdtool uses a simplified extraction; the extracted `flashregion_2_intel_me.bin` is the
working ME binary that coreboot will embed.*

These extractions are already done and stored at:
```
/home/mahdi/repositories/reconstruction/server/flashregion_0_flashdescriptor.bin
/home/mahdi/repositories/reconstruction/server/flashregion_1_bios.bin
/home/mahdi/repositories/reconstruction/server/flashregion_2_intel_me.bin
/home/mahdi/repositories/reconstruction/server/flashregion_3_gbe.bin
```

### 2.4 Inspect the Original BIOS Region

```bash
# Scan for known firmware signatures
binwalk flashregion_1_bios.bin | head -30
# or
hexdump -C flashregion_1_bios.bin | head -10
```

The original BIOS region contains the proprietary AMI UEFI firmware. The ME binary extracted here
(or the one in `3rdparty/dasharo-blobs/asrock/spc741d8/me.bin`) must have the HAP bit set to
neutralize ME before embedding in the coreboot image.

### 2.5 Prepare Blobs for Coreboot

Coreboot requires two binary blobs:

| Blob | Source | Destination |
|---|---|---|
| `descriptor.bin` | Extracted from server.bin or from dasharo-blobs | `3rdparty/dasharo-blobs/asrock/spc741d8/descriptor.bin` |
| `me.bin` | Neutralized copy from dasharo-blobs | `3rdparty/dasharo-blobs/asrock/spc741d8/me.bin` |

These blobs are **already present** in the Dasharo submodule:
```bash
ls 3rdparty/dasharo-blobs/asrock/spc741d8/
# descriptor.bin  me.bin  README.md
```

If `3rdparty/dasharo-blobs/` is empty (submodule not initialized):
```bash
git submodule update --init 3rdparty/dasharo-blobs
```

---

## 3. Flash Layout — board.fmd Explained

### 3.1 What is .fmd

A `.fmd` (Flash Map Definition) file describes how the SPI flash image is divided into named
regions. Coreboot's build system reads it to:
- Allocate space for each component
- Generate the `FMAP` binary (a machine-readable table embedded in the ROM)
- Drive `cbfstool` operations

### 3.2 board.fmd Annotated

File: `src/mainboard/asrock/spc741d8/board.fmd`

```
FLASH 64M {                                ← total flash size: 64 MB = 0x4000000

  SI_ALL@0x0 0x03000000 {                  ← 48 MB: Intel-managed regions (do not modify)
    SI_DESC@0x0 0x1000                     ←  4 KB at offset 0: Flash Descriptor
    SI_ME@0x3000 0x2fed000                 ← ~47.7 MB: Intel Management Engine
    SI_PT@0x2ff0000 0x10000                ← 64 KB: Intel Platform Data (unused here)
  }

  SI_BIOS@0x3000000 0x1000000 {            ← 16 MB at offset 48 MB: BIOS region
    BOOTSPLASH(CBFS) 1M                    ←  1 MB: Splash image (optional)
    FMAP 0x800                             ←  2 KB: Flash Map table
    COREBOOT(CBFS)                         ← rest: CBFS (Coreboot File System)
  }
}
```

**Key points:**
- `SI_DESC`, `SI_ME`, `SI_PT` are **read-only** from coreboot's perspective; their content comes from the blob files.
- `COREBOOT(CBFS)` is the writable partition where coreboot stages, FSP blobs, the EDK II payload, microcode, etc. are stored.
- `BOOTSPLASH` takes 1 MB of the 16 MB BIOS region, leaving ~15 MB for `COREBOOT(CBFS)`.
- The EDK II payload (`UEFIPAYLOAD.fd`, ~15 MB) must fit inside the `COREBOOT` CBFS partition.

### 3.3 Modifying board.fmd

If you need more space for the EDK II payload:
- Remove or shrink `BOOTSPLASH`: comment out the line or reduce the size.
- You can also remove `#RW_MRC_CACHE` and `#SMMSTORE` (already commented out).

Example: disable bootsplash to reclaim 1 MB:
```diff
- BOOTSPLASH(CBFS) 1M
+ # BOOTSPLASH(CBFS) 1M    ← commented out; 1 MB freed for COREBOOT(CBFS)
```

---

## 4. Device Tree — devicetree.cb Explained

### 4.1 What is devicetree.cb

The device tree (`src/mainboard/asrock/spc741d8/devicetree.cb`) is a coreboot-specific hardware
description file. It uses a C-like syntax to describe the motherboard's device topology and
configuration registers. Unlike Linux DTS, it is compiled into coreboot C code.

### 4.2 Full devicetree.cb Annotated

```
chip soc/intel/xeon_sp/spr          ← Xeon SP Sapphire Rapids (SiPr) SoC driver

  device domain 0 on                ← PCI domain 0 (the main host bridge)

    device pci 1f.0 on              ← PCH eSPI controller (D31:F0 = LPC/eSPI)

      chip superio/common
        device pnp 4e.0 on          ← Super I/O at PNP address 0x4E (AST SUPC)

          chip superio/aspeed/ast2400           ← ASPEED UART super-I/O driver
                                                   (compatible with AST2600 UART)
            register "use_espi" = "1"           ← UART connected via eSPI (not LPC)

            device pnp 4e.2 on      ← Logical Device 2: SUART1
              io 0x60 = 0x3f8       ←   COM1 base I/O at 0x3F8
              irq 0x70 = 0x04       ←   IRQ 4
            end

            device pnp 4e.3 on      ← Logical Device 3: SUART2
              io 0x60 = 0x2f8       ←   COM2 base I/O at 0x2F8
              irq 0x70 = 0x03       ←   IRQ 3
            end
          end
        end
      end

      chip drivers/ipmi              ← IPMI KCS (Keyboard Controller Style) interface
        device pnp ca2.0 on end      ←   KCS at I/O 0xCA2 (standard for AST BMC)
        register "bmc_i2c_address" = "0x20"    ← BMC's I2C address on SMBus
        register "bmc_boot_timeout" = "60"     ← wait up to 60s for BMC to boot
      end

      chip drivers/pc80/tpm          ← Trusted Platform Module (TPM 2.0)
        device pnp 0c31.0 on end     ←   TPM at I/O 0x0C31 (memory-mapped)
      end
    end
  end
end
```

### 4.3 AST2400 vs AST2600 Note

The devicetree uses `chip superio/aspeed/ast2400` while the server's actual BMC chip is an
**ASPEED AST2600**. This is intentional: the `ast2400` driver in coreboot handles the
**Super I/O UART configuration** (setting I/O base addresses and IRQs for COM1/COM2). The
AST2600 exposes a fully compatible Super I/O interface for UART configuration, so the ast2400
driver works correctly.

**If you see UART issues**, check whether coreboot has a dedicated `superio/aspeed/ast2600`
driver that configures additional AST2600-specific features:
```bash
ls src/superio/aspeed/
# ast2400/  common/  ...
```

For basic COM1 operation (needed for `early_puts` serial debug), the ast2400 driver is sufficient
with the AST2600.

---

## 5. Build Configuration — .config Explained

### 5.1 Key Settings in configs/config.asrock_spc741d8

The official Dasharo release config is at `configs/config.asrock_spc741d8`. Use it as the base:

```bash
cp configs/config.asrock_spc741d8 .config
make olddefconfig    # fill in any missing options with defaults
```

### 5.2 Annotated Critical Settings

```ini
# === Board identity ===
CONFIG_BOARD_ASROCK_SPC741D8=y
CONFIG_MAINBOARD_VENDOR="ASROCK"
CONFIG_MAINBOARD_PART_NUMBER="SPC741D8-2L2T/BCM"

# === Flash image ===
CONFIG_BOARD_ROMSIZE_KB_65536=y               # 64 MB ROM
CONFIG_ROM_SIZE=0x04000000                    # 64 MB

# === Intel Binary Blobs ===
CONFIG_IFD_BIN_PATH="3rdparty/dasharo-blobs/asrock/spc741d8/descriptor.bin"
CONFIG_ME_BIN_PATH="3rdparty/dasharo-blobs/asrock/spc741d8/me.bin"
CONFIG_HAVE_IFD_BIN=y
CONFIG_HAVE_ME_BIN=y
CONFIG_DO_NOT_TOUCH_DESCRIPTOR_REGION=y       # don't overwrite SI_DESC at offset 0

# === FSP ===
CONFIG_FSP_FD_PATH="3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd"
CONFIG_FSP_TEMP_RAM_SIZE=0x60000              # 384 KB FSP temp RAM during memory init

# === Cache-As-RAM (before memory is initialized) ===
CONFIG_DCACHE_RAM_BASE=0xfe800000             # CAR starts at top of low 4 GB address space
CONFIG_DCACHE_RAM_SIZE=0x1fff00               # ~32 MB CAR region
CONFIG_DCACHE_BSP_STACK_SIZE=0x40000          # 256 KB BSP stack in CAR

# === Serial Console ===
CONFIG_CONSOLE_SERIAL=y
CONFIG_DRIVERS_UART_8250IO=y
CONFIG_UART_FOR_CONSOLE=0                     # use UART 0 = COM1 at 0x3F8
CONFIG_TTYS0_BASE=0x3f8                       # COM1 I/O base (derived from UART_FOR_CONSOLE=0)
CONFIG_CONSOLE_SERIAL_115200=y               # 115200 baud

# === Payload: EDK II (DasharoPayloadPkg) ===
CONFIG_PAYLOAD_EDK2=y
CONFIG_EDK2_TAG_OR_REV="22333c9e249641293ce4415e726354f396a0d8d3"
CONFIG_EDK2_USE_EDK2_PLATFORMS=y
CONFIG_EDK2_PLATFORMS_REPOSITORY="https://github.com/Dasharo/edk2-platforms"
CONFIG_EDK2_PLATFORMS_TAG_OR_REV="1002a59639f111a2f8178b77d1f5fde0ea8d976f"
CONFIG_EDK2_SERIAL_SUPPORT=y                  # enable EDK II serial output
CONFIG_EDK2_CBMEM_LOGGING=y                   # log coreboot output via CBMEM
```

### 5.3 Settings to Verify or Adjust

| Setting | Default | Recommendation |
|---|---|---|
| `CONFIG_EDK2_TAG_OR_REV` | `22333c9e...` | Point to your custom fork/branch if needed |
| `CONFIG_DEFAULT_CONSOLE_LOGLEVEL_0=y` | 0 (silent) | Change to 8 for maximum verbosity during debug |
| `CONFIG_CBFS_VERIFICATION=y` | on | Can disable to speed up builds during debug |
| `CONFIG_BOOTMEDIA_SMM_BWP=y` | on | Disabling allows software flash write access |

To enable maximum coreboot console output for debugging:
```bash
# In .config, change:
# CONFIG_DEFAULT_CONSOLE_LOGLEVEL_0=y
# to:
CONFIG_DEFAULT_CONSOLE_LOGLEVEL_8=y
CONFIG_MAXIMUM_CONSOLE_LOGLEVEL_8=y
```

---

## 6. Branch Setup — Starting Clean from the Release Tag

### 6.1 Why asrock_spc741d8_v0.9.0

The tag `asrock_spc741d8_v0.9.0` is the **production release** of Dasharo for this board. It
is confirmed to boot coreboot successfully to a UEFI shell on the ASRock SPC741D8. Starting from
this tag gives a known-good baseline.

### 6.2 Commands Executed

```bash
# Verify the tag exists
git tag | grep asrock_spc741d8
# Output:
# asrock_spc741d8_v0.9.0
# asrock_spc741d8_v0.9.0-rc1  ... rc4

# Check what commit the tag points to
git log --format="%H %s" asrock_spc741d8_v0.9.0 -1
# Output:
# cee8ddf3960524f5956737c8216f0d5021f794ef configs/config.asrock_spc741d8: bump to non-rc

# Create and switch to new branch
git checkout -b edk2-server asrock_spc741d8_v0.9.0
# Output:
# warning: unable to rmdir 'payloads/external/edk2/workspace/dasharo': Directory not empty
# Switched to a new branch 'edk2-server'
```

The warning about `workspace/dasharo` is expected — it is a nested git repository that was left
in place from previous work.

### 6.3 Apply the Base Config

```bash
cp configs/config.asrock_spc741d8 .config
make olddefconfig
```

### 6.4 Custom Modifications vs. Default Config

The Dasharo release config already uses EDK II (`CONFIG_PAYLOAD_EDK2=y`) with DasharoPayloadPkg.
Your previous `fixbug` branch added:
1. VGA debug functions in `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c`
2. Extra debug prints in `DasharoPayloadPkg/SecCore/SecMain.c`

These are in the EDK II workspace (nested git repo), not in the coreboot tree itself.

---

## 7. EDK II Workspace — Setup and Git Tracking

### 7.1 How the EDK II Workspace is Managed

The coreboot build system clones the EDK II repository during `make`. The configuration is:

- Repo URL: `CONFIG_EDK2_REPOSITORY` (default: `https://github.com/Dasharo/coreboot.git`)
- Commit/tag: `CONFIG_EDK2_TAG_OR_REV`
- Checkout location: `payloads/external/edk2/workspace/<repo-name>/`

The `payloads/external/edk2/Makefile` handles the clone and checkout automatically.

### 7.2 Current State of the Workspace

The workspace at `payloads/external/edk2/workspace/dasharo/` is already a git repository:

```bash
git -C payloads/external/edk2/workspace remote -v
# local   /home/mahdi/backup/dasharo/  (fetch)
# local   /home/mahdi/backup/dasharo/  (push)
# origin  https://github.com/Dasharo/coreboot.git (fetch)
# origin  https://github.com/Dasharo/coreboot.git (push)

git -C payloads/external/edk2/workspace/dasharo log --oneline -3
# a69960a18a Wrong print is fixed
# ec93e511bd Results of Logs of PeiMain are added
# 1b3fabbe86 Logs of PeiMain are completed
```

### 7.3 Tracking the EDK II Workspace in the Coreboot Repo

**Option A: Git Submodule (recommended)**

Add the EDK II workspace as a git submodule so changes are tracked via their own commit hash:

```bash
# From the coreboot repo root:
git submodule add https://github.com/<your-fork>/dasharo-edk2.git \
    payloads/external/edk2/workspace/dasharo
git submodule update --init

# Record the current commit:
git add payloads/external/edk2/workspace/dasharo
git add .gitmodules
git commit -m "Add EDK II DasharoPayloadPkg as submodule"
```

After this, `git status` will show the submodule as a single entry:
```
modified: payloads/external/edk2/workspace/dasharo (new commits)
```

**Option B: Track source files directly (simpler, less correct)**

Remove the nested `.git` and track files directly:
```bash
rm -rf payloads/external/edk2/workspace/dasharo/.git
git add payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/
git add payloads/external/edk2/workspace/dasharo/MdeModulePkg/Core/Pei/
git commit -m "Add custom EDK II modifications to tracking"
```

**Recommended approach for this project:** Keep the EDK II workspace as its own git repo
(Option A), track your changes there with meaningful commits, and point coreboot to your fork
by updating `CONFIG_EDK2_TAG_OR_REV` in `.config`.

### 7.4 .gitignore Updates Applied

The following entries were added to `.gitignore` on the `edk2-server` branch:

```gitignore
# EDK II payload workspace — track source, exclude build artefacts
payloads/external/edk2/workspace/Build/
payloads/external/edk2/workspace/Conf/BuildEnv.sh
!payloads/external/edk2/workspace/Conf/target.txt
!payloads/external/edk2/workspace/Conf/build_rule.txt
!payloads/external/edk2/workspace/Conf/tools_def.txt
payloads/external/edk2/workspace/dasharo/Build/
payloads/external/edk2/workspace/dasharo/.git/
logs/*.jpg
!logs/*.md
```

---

## 8. Building the Firmware Image

### 8.1 Full Build from Scratch

```bash
cd /home/mahdi/repositories/coreboot/dasharo/coreboot

# 1. Apply the release config as base
cp configs/config.asrock_spc741d8 .config
make olddefconfig

# 2. (Optional) Adjust settings interactively
make menuconfig

# 3. Build (uses all CPUs)
make -j$(nproc) 2>&1 | tee build.log
```

Expected duration: 30–90 minutes (depending on CPU). The EDK II build is the slowest part.

### 8.2 What the Build Does

| Phase | Output | Description |
|---|---|---|
| cbfstool/ifdtool build | `util/cbfstool/cbfstool` | Build host tools |
| Coreboot stages | `build/bootblock.elf`, `build/romstage.elf`, etc. | Compile coreboot C code |
| EDK II payload | `payloads/external/edk2/workspace/Build/.../UEFIPAYLOAD.fd` | EDK II build (longest step) |
| ROM assembly | `build/coreboot.rom` | Pack everything into 64 MB image |

### 8.3 Expected Final Output

```bash
ls -lh build/coreboot.rom
# -rw-r--r-- 1 mahdi mahdi 64M <date> build/coreboot.rom

util/cbfstool/cbfstool build/coreboot.rom print -r COREBOOT
```

Expected CBFS print (region COREBOOT):
```
FMAP REGION: COREBOOT
Name                   Offset     Type           Size   Comp
cbfs_master_header     0x0        header           32    none
fallback/payload       0x80       simple elf   989886    none   ← EDK II (~966 KB)
cpu_microcode_blob     0xF1B80    microcode   1782784    none
intel_fit              0x2A4FC0   intel_fit        80    none
fallback/romstage      0x2A5040   stage         57392    none
fallback/ramstage      0x2B3100   stage        124235    LZMA
config                 0x2D16C0   raw            4102    LZMA
revision               0x2D2700   raw             859    none
build_info             0x2D2A80   raw             124    none
fallback/dsdt.aml      0x2D2B40   raw           18414    none
fspm.bin               0x2D77C0   fsp          3375104   none   ← FSP-M (3.2 MB)
fsps.bin               0x60F800   fsp           185328    LZ4   ← FSP-S
fallback/postcar       0x63CC40   stage         32340    none
fspt.bin               0xEDF7C0   fsp            32768   none   ← FSP-T (32 KB)
bootblock              0xEF87C0   bootblock     28672    none
```

### 8.4 Incremental Rebuild (EDK II changes only)

After changing EDK II source files:

```bash
# Force EDK II rebuild without full coreboot rebuild
rm -f payloads/external/edk2/workspace/Build/DasharoPayloadPkg/RELEASE_COREBOOT/IA32/\
  MdeModulePkg/Core/Pei/PeiMain/OUTPUT/PeiMain.obj
# Then rebuild:
make -j$(nproc)
```

Or clean only the EDK II build:
```bash
rm -rf payloads/external/edk2/workspace/Build/
make -j$(nproc)
```

---

## 9. Offline Verification — cbfstool Image Inspection

Before flashing, verify the ROM is correctly assembled.

### 9.1 Print CBFS Contents

```bash
util/cbfstool/cbfstool build/coreboot.rom print -r COREBOOT
```

Check that `fallback/payload` is present and type is `simple elf`.

### 9.2 Extract and Verify the EDK II Payload

```bash
util/cbfstool/cbfstool build/coreboot.rom extract \
  -r COREBOOT -n fallback/payload -f /tmp/payload_check.elf
file /tmp/payload_check.elf
ls -lh /tmp/payload_check.elf
```

Expected:
```
ELF 32-bit LSB executable, Intel 80386, statically linked
~989886 bytes
```

### 9.3 Verify the Intel FD

```bash
util/ifdtool/ifdtool -d build/coreboot.rom | grep -E "Region|HAP|AltMe"
```

Check that:
- Flash regions match `board.fmd` (SI_DESC at 0, ME at 0x3000, BIOS at 0x3000000)
- `AltMeDisable bit is set` (ME neutralized) — **required for coreboot to work**

### 9.4 Verify FMAP Table

```bash
util/cbfstool/cbfstool build/coreboot.rom layout
```

Expected:
```
This image contains the following sections that can be manipulated with this tool:
 'SI_ALL'      (read-only, size 48M)
 'SI_DESC'     (read-only, size 4096)
 'SI_ME'       (read-only, size 47MB)
 'SI_BIOS'     (size 16M)
 'BOOTSPLASH'  (size 1M)
 'FMAP'        (read-only, size 2KB)
 'COREBOOT'    (size ~15M)
```

### 9.5 Check Bootblock Entry Point

```bash
util/cbfstool/cbfstool build/coreboot.rom extract \
  -r COREBOOT -n bootblock -f /tmp/bootblock.elf
objdump -d /tmp/bootblock.elf | head -30
```

### 9.6 Inspect EDK II Firmware Volume Structure

The EDK II payload starts at physical address `0x800000` when loaded. The FV header at that address must be valid:

```bash
util/cbfstool/cbfstool build/coreboot.rom extract \
  -r COREBOOT -n fallback/payload -f /tmp/payload.elf

# The ELF file wraps the raw FD; extract the load segment:
objcopy -O binary --only-section=.data /tmp/payload.elf /tmp/fv_raw.bin 2>/dev/null \
  || dd if=/tmp/payload.elf of=/tmp/fv_raw.bin bs=1 skip=64 2>/dev/null

# Check FV header signature (should be "_FVH"):
hexdump -C /tmp/fv_raw.bin | head -5
```

Expected first 16 bytes: `00 00 00 00` × 16 (ZeroVector), then `78 E5 8C...` (FFS2 GUID).

---

## 10. Flashing to Hardware

### 10.1 Read Protection Warning

The original server.bin has `Host CPU/BIOS Region Write Access: disabled` in FLMSTR1. This means
**the SPI controller will reject write operations from the host CPU** until the firmware unlocks
the flash.

Options to unlock:
1. **Boot with coreboot** — coreboot's bootblock typically reconfigures flash permissions.
2. **Use IPMI/BMC flash** — the AST2600 BMC can flash the SPI independently:
   ```bash
   # via IPMI tool (if BMC network is configured):
   ipmitool -I lanplus -H <bmc_ip> -U admin -P <pass> \
     raw 0x3c 0x42 0x01 <bank_id>
   # Then SCP the image to the BMC and use flashcp
   ```
3. **Hardware SPI programmer** — clip-on or socket-based programmer as fallback.

### 10.2 Flashing via flashrom (Host CPU)

```bash
# First, read back the current flash to verify access:
sudo flashrom -p internal -r /tmp/current_flash.bin
sha256sum /tmp/current_flash.bin /home/mahdi/repositories/reconstruction/server/server.bin

# Write the new image:
sudo flashrom -p internal -w build/coreboot.rom --ifd -i bios
```

The `--ifd -i bios` flags tell flashrom to write only the BIOS region (16 MB), leaving Intel ME
and GbE intact. **Never overwrite the Intel ME region** — doing so can brick the server.

### 10.3 Verifying the Flash Succeeded

```bash
sudo flashrom -p internal -r /tmp/flash_readback.bin --ifd -i bios
md5sum /tmp/flash_readback.bin
# Compare with: md5sum build/coreboot.rom  (BIOS region only)
```

---

## 11. HOB Construction: DasharoPayloadPkg vs UefiPayloadPkg

### 11.1 What Are HOBs?

**HOBs** (Hand-Off Blocks) are small data structures passed from PEI phase to DXE phase in UEFI.
They describe:
- System memory ranges (base, size, type)
- Firmware volumes
- CPU properties (physical address bits)
- Platform-specific info (ACPI tables, SMBIOS, SMM store, graphics)

HOBs are built during PEI in a temporary RAM heap (in this system: `0x80000–0x88000`, 32 KB).

### 11.2 DasharoPayloadPkg/BlSupportPei (in use)

`DasharoPayloadPkg/BlSupportPei/BlSupportPei.c` is the PEIM that runs in PEI and builds all
HOBs. It reads the **coreboot LBIO table** (at `0x63593000`) to get memory information.

**Call chain:**
```
BlPeiEntryPoint()
  ├── BuildResourceDescriptorHob(0x0,       0x1000)      ← reserved: first 4 KB
  ├── BuildResourceDescriptorHob(0x1000,   0x9F000)      ← conventional: 640 KB
  ├── BuildResourceDescriptorHob(0xA0000, 0x60000)       ← reserved: VGA/BIOS area
  ├── ParseMemoryInfo(MemInfoCallback, &UsableLowMemTop)  ← reads LBIO memory table
  │     └── for each coreboot memory range:
  │           MemInfoCallback(range, &UsableLowMemTop)
  │             ├── if (RAM && Base < 4GB && Size >= 64MB):
  │             │     *UsableLowMemTop = Base + Size   ← track highest low RAM
  │             └── BuildResourceDescriptorHob(Base, Size, type)
  ├── ASSERT(UsableLowMemTop >= 1MB + 64MB)              ← *** CRASH POINT ***
  ├── PeiMemBase = (UsableLowMemTop - 64MB) & ~0xFFFF
  ├── PeiServicesInstallPeiMemory(PeiMemBase, 64MB)       ← give PEI its permanent heap
  ├── BuildGuidDataHob(&gEfiMemoryTypeInformationGuid)
  ├── PeiReportRemainedFvs()                              ← report all FVs
  ├── BuildCpuHob(PhysicalAddressBits, 16)
  ├── ParseSystemTable() → BuildGuidHob(ACPI, SMBIOS)
  ├── ParseAcpiInfo() → BuildGuidHob(ACPI board info)
  ├── ParseGfxInfo() → BuildGuidHob(graphics framebuffer)
  └── IoWrite8(0x21, 0xFF); IoWrite8(0xA1, 0xFF)         ← mask 8259 PIC
```

**Where BlSupportPei gets memory info:**
```c
// CbParseLib.c: ParseMemoryInfo()
Tolud = PciRead32(PCI_LIB_ADDRESS(0,0,0,0xbc)) & 0xFFF00000;   // ← TOLUD register
rec = (struct cb_memory *)FindCbTag(CB_TAG_MEMORY);              // ← find LBIO tag 0x0001
for (each range in rec) {
    MemoryMap.Base = cb_unpack64(Range->start);
    MemoryMap.Size = cb_unpack64(Range->size);
    // ...classify as RAM/reserved/MMIO based on type and TOLUD...
    MemInfoCallback(&MemoryMap, Params);
}
```

### 11.3 UefiPayloadPkg/UefiPayloadEntry (reference — not currently used)

`UefiPayloadPkg/UefiPayloadEntry/UefiPayloadEntry.c` uses the **E820 memory map** as its source
of memory information. Key difference:

| Aspect | DasharoPayloadPkg/BlSupportPei | UefiPayloadPkg/UefiPayloadEntry |
|---|---|---|
| Memory source | Coreboot LBIO table (`CB_TAG_MEMORY`) | E820 BIOS memory map |
| HOB construction | In PEI (BlSupportPei PEIM) | In PEI (dedicated entry PEIM) |
| Low-memory assumption | `UsableLowMemTop` must cover ≥ 64 MB | Uses `mTopOfLowerUsableDram` |
| High-memory (>4 GB) | Handled via HOB with `~TESTED` attribute | Marked separately in E820 |
| TOLUD read | `PciRead32(b0:d0:f0:0xBC)` — **may be wrong on Xeon SP** | Not needed |

The user's observation: *"I see HOB construction in EDK II/UefiPayloadPkg but not in DasharoPayloadPkg"* — the HOB construction **does exist** in DasharoPayloadPkg/BlSupportPei, but it is invoked **inside PEI** as a PEIM, while `UefiPayloadEntry` is the DXE-phase entry. The visual difference is that DasharoPayloadPkg's HOB building code is in a separate PEIM file, not inline in the "entry" file.

---

## 12. Boot Sequence: From coreboot to UEFI Shell

### 12.1 Coreboot Stages (fully working)

```
Power on
  ↓
Bootblock   (in flash, no RAM)
  - LPC/eSPI decode for Aspeed UART
  - Enable COM1 at 0x3F8, 115200 baud
  - Load romstage from CBFS
  ↓
Romstage    (in Cache-As-RAM at 0xFE800000)
  - Call FSP-T (temp RAM init) at 0xFE800000
  - Call FSP-M (memory training) — DDR5 initialization
  - Coreboot table built at 0x63593000 (LBIO)
  - Load postcar
  ↓
Postcar     (tears down CAR, enters DRAM)
  ↓
Ramstage    (in DRAM)
  - Call FSP-S (silicon init)
  - Enumerate devices, ACPI tables, SMBIOS
  - Build coreboot table entries (37 entries, 19 memory ranges)
  - Load and jump to EDK II payload at 0x800000
  ↓
EDK II SecEntry.nasm  (at 0x801620)
  - CLI, set up 32-bit stack
  - Push: BootloaderParameter=0x63593000, BFVBase=0x800000
  -       TempRamBase=0x80000, SizeOfRam=0x10000
  - Call SecStartup()
```

### 12.2 EDK II PEI Phase (crashing)

```
SecStartup()          [SecMain.c]
  ↓
SecStartupPhase2()    [FindPeiCore.c]
  - Scan BFV for PeiCore PEIM
  - Found PeiCore at 0x80E2D0
  - Call PeiCoreEntryPoint(SecCoreData, PPIs)
  ↓
PeiCore() first pass  [PeiMain.c] — OldCoreData=NULL, PMI=0
  - Initialize PEI services (HOB heap at 0x80000, stack at 0x88000)
  - Initialize dispatch infrastructure
  - VGA: "[1]PC:D=0 S=8FE94 P=8030B4"       ← Row 0
  - VGA: "[1]BFV=800000,SZ=FF800000,..."     ← Row 1
  - VGA: "[1]PD=8FA30 PS=0"                  ← Row 2
  - VGA: "[1]k-Ps:8FA34,..."                 ← Row 19
  - VGA: "[1]u-Sec:8FE94,PD:8FA30,BM:0,PMI:0"  ← Row 21 — LAST OUTPUT
  ↓
PeiDispatcher()       [Dispatcher.c]          ← CRASH HERE
  - Attempts to dispatch BFS2-resident PEIMs:
    1. DxeIpl
    2. BlSupportPei   ← most likely crash site
    3. Others
  - System hangs/triple-faults — never returns
```

---

## 13. Crash Analysis — PeiDispatcher Hang

### 13.1 What PeiDispatcher Does

`PeiDispatcher()` (in `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c`) scans the BFV for
PEIM files and invokes each one's entry point. In the first pass (before memory is installed),
its goal is to dispatch PEIMs until one calls `PeiServicesInstallPeiMemory()`.

### 13.2 What is NOT the Crash

The user initially suspected `ProcessDispatchNotifyList(&PrivateData)` at PeiMain.c:873.
That line is **inside `if (OldCoreData != NULL)`** — the second-pass block. Since
`OldCoreData=NULL` in the current run, that block is never executed. The crash happens
**inside `PeiDispatcher()`** on the first pass.

### 13.3 BlSupportPei — The Critical Bug

Reading `BlSupportPei.c` entry point lines 651–733:

```c
EFI_PHYSICAL_ADDRESS  UsableLowMemTop = 0;   // initialized to 0

Status = ParseMemoryInfo(MemInfoCallback, &UsableLowMemTop);
if (EFI_ERROR(Status)) return Status;

// MemInfoCallback only updates UsableLowMemTop when:
//   Base >= 1MB  AND  Base < 4GB  AND  Type == SYSTEM_MEMORY  AND  Size >= 64MB
// → If no such region exists, UsableLowMemTop stays 0

ASSERT(UsableLowMemTop >= BASE_1MB + PEI_MEM_SIZE);    // LINE 728
// PEI_MEM_SIZE = SIZE_64MB = 0x4000000
// In RELEASE build: ASSERT is a NO-OP → execution continues with UsableLowMemTop = 0

PeiMemBase = (UsableLowMemTop - PEI_MEM_SIZE) & (~(BASE_64KB - 1));
// 0 - 0x4000000 = underflow → PeiMemBase = 0xFFFFFFFFFC000000 (UINT64 underflow)

Status = PeiServicesInstallPeiMemory(PeiMemBase, PEI_MEM_SIZE);
// Tries to install memory at 0xFFFFFFFFFC000000 → PeiCore crash / triple fault
```

### 13.4 Why UsableLowMemTop Might Be Zero

The coreboot LBIO table at `0x63593000` has 19 memory ranges. `MemInfoCallback` only sets
`UsableLowMemTop` for a range if **all three** conditions are met:
1. `Base >= BASE_1MB` (0x100000)
2. `Base < BASE_4GB` (0x100000000)
3. `Size >= PEI_MEM_SIZE` (64 MB = 0x4000000)

For a Xeon SP DDR5 server, **typical memory map from coreboot:**

```
Range  Base              Size          Type
  0    0x0000000000000   0x000A0000    LB_MEM_RAM      (conventional: 640 KB)
  1    0x0000000000A0000 0x000060000   LB_MEM_RESERVED (VGA/BIOS)
  2    0x0000000000100000 0x0E0000000  LB_MEM_RAM      (1 MB to ~3.5 GB) ← PASS
  3    0x0000000060000000 0x080000000  LB_MEM_RESERVED (PCI 32-bit MMIO)
  4    0x00000000E0000000 0x020000000  LB_MEM_RESERVED (PCIe)
  ...
  8    0x0000000100000000 <huge size>  LB_MEM_RAM      (high DDR5, > 4 GB)
  ...
```

If range 2 exists (Base=0x100000, Size ≥ 64 MB), `UsableLowMemTop` will be set correctly.
But if the FSP or coreboot splits the low memory range into many small pieces, none of which
is ≥ 64 MB alone, `UsableLowMemTop` remains 0.

**The `PciRead32(PCI_LIB_ADDRESS(0,0,0,0xbc))` TOLUD read** is also suspicious on Xeon SP.
On consumer Intel chips, bus 0 device 0 function 0 is the host bridge with TOLUD at offset
`0xBC`. On **Xeon SP Sapphire Rapids**, bus 0 device 0 function 0 is the PCIe Root Port or
UPI controller — `0xBC` may be a different register or return `0xFFFFFFFF` (device absent).
If `Tolud = 0xFFF00000`, all `CB_MEM_RESERVED` ranges above that will be classified as MMIO
rather than reserved memory. This doesn't crash directly, but misclassifies the memory map.

### 13.5 Crash Confirmation Strategy

To confirm which PEIM crashes, add VGA output to `Dispatcher.c` before each PEIM entry call.
The last GUID shown on screen is the PEIM that crashed.

To confirm whether `BlSupportPei` is the cause, add a VGA print at the very first line of
`BlPeiEntryPoint()` and another just before and after `ParseMemoryInfo()`.

---

## 14. Active Bugs and Fixes

### Bug 1: VGA Write in PeiMain.c — FIXED

**File:** `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` lines 43–45

```diff
-  p[1] = 0x0F00 | (unsigned char)string[1];
+  p[i] = 0x0F00 | (unsigned char)string[i];
   ...
-  p[1] = 0x0F00;
+  p[i] = 0x0F00;
```

Status: Fixed in `fixbug` branch commit `d56c1cd176`. Apply to `edk2-server` branch by
cherry-picking or manually editing.

### Bug 2: BlSupportPei ASSERT Bomb (ACTIVE — root cause of crash)

**File:** `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c` lines 720–733

**Fix A — Remove minimum size requirement:**
```c
// BEFORE (MemInfoCallback, ~line 347):
if (Base < BASE_4GB) {
    if (Size >= PEI_MEM_SIZE) {          // ← only update if >= 64 MB
        *UsableLowMemTop = Base + Size;
    }
}

// AFTER — track any low-RAM region, pick the best one at the end:
if (Base < BASE_4GB) {
    if ((Base + Size) > *UsableLowMemTop) {
        *UsableLowMemTop = Base + Size;  // ← track highest low-mem top
    }
}
```

**Fix B — Guard ASSERT and handle zero case:**
```c
// After ParseMemoryInfo() call, before PeiMemBase calculation:
if (UsableLowMemTop < BASE_1MB + PEI_MEM_SIZE) {
    // Fall back: use first 64 MB above 1 MB
    UsableLowMemTop = BASE_1MB + PEI_MEM_SIZE;
}
PeiMemBase = (UsableLowMemTop - PEI_MEM_SIZE) & (~(BASE_64KB - 1));
```

**Fix C — Add debug dump before the ASSERT:**
This is the first step: add VGA output to see what `UsableLowMemTop` actually is:
```c
// In BlPeiEntryPoint(), right after ParseMemoryInfo():
extern VOID mde_1_edkii_vga_sprintf(INT32 row, const char *fmt, ...);
mde_1_edkii_vga_sprintf(5, "BLP: LMTop=%08X", (UINT32)UsableLowMemTop);
mde_1_edkii_vga_sprintf(6, "BLP: PEI_MEM=%08X", PEI_MEM_SIZE);
```

### Bug 3: PEIM GUID Debug Missing (needed for diagnosis)

**File:** `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c`

Add a VGA print before each PEIM entry point call. Find the call site (search for
`PeimEntryPoint (FileHandle, PeiServices)` or similar):

```bash
grep -n "EntryPoint\|PeimEntry\|InvokeDispatch\|CallPeiModule" \
  MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c | head -20
```

Add before the call:
```c
extern VOID mde_1_edkii_vga_sprintf(INT32 row, const char *fmt, ...);
mde_1_edkii_vga_sprintf(23,
    "PD: %08X-%04X-%04X",
    PeimGuid->Data1, PeimGuid->Data2, PeimGuid->Data3);
mde_1_edkii_vga_sprintf(24,
    "   %02X%02X%02X%02X%02X%02X%02X%02X",
    PeimGuid->Data4[0], PeimGuid->Data4[1],
    PeimGuid->Data4[2], PeimGuid->Data4[3],
    PeimGuid->Data4[4], PeimGuid->Data4[5],
    PeimGuid->Data4[6], PeimGuid->Data4[7]);
```

The GUID displayed when the screen goes blank is the crashing PEIM.

### Bug 4: Serial Port Not Implemented

**File:** `DasharoPayloadPkg/SecCore/SecMain.c`

Add after the existing `#include` block:

```c
#define COM1_BASE 0x3F8

static void early_uart_init(void)
{
    IoWrite8(COM1_BASE + 3, 0x80);   /* DLAB=1 */
    IoWrite8(COM1_BASE + 0, 0x01);   /* divisor low = 1 → 115200 baud */
    IoWrite8(COM1_BASE + 1, 0x00);   /* divisor high = 0 */
    IoWrite8(COM1_BASE + 3, 0x03);   /* 8N1, DLAB=0 */
    IoWrite8(COM1_BASE + 1, 0x00);   /* disable interrupts */
    IoWrite8(COM1_BASE + 2, 0xC7);   /* enable + clear FIFO */
    IoWrite8(COM1_BASE + 4, 0x03);   /* DTR + RTS asserted */
}

static void early_putc(char c)
{
    while ((IoRead8(COM1_BASE + 5) & 0x20) == 0) {}  /* wait for THRE */
    IoWrite8(COM1_BASE, c);
}

static void early_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') early_putc('\r');
        early_putc(*s++);
    }
}
```

Then at the very start of `SecStartup()`:
```c
early_uart_init();
early_puts("[MN] SecStartup entered\r\n");
```

**Why serial may not be visible on screen:** On this server, COM1 (I/O 0x3F8) is routed through
the Aspeed AST2600 BMC via eSPI. It does **not** appear on a physical DE-9 rear panel connector.
Access serial output via IPMI Serial Over LAN:

```bash
ipmitool -I lanplus -H <bmc_ip_address> -U admin -P <password> sol activate
# Then power on/reset the server
# Press Enter if needed to see output
```

---

## 15. Debug Strategy

### 15.1 Immediate Next Steps (ordered)

```
Step 1: Apply Bug 1 fix (VGA write) to edk2-server branch — prevents silent debug breakage
Step 2: Add BlSupportPei VGA dump (Bug 2/Fix C) — reveals UsableLowMemTop value
Step 3: Add PEIM GUID print to Dispatcher.c (Bug 3) — pinpoints crashing PEIM
Step 4: Rebuild, flash, observe VGA output
Step 5: Based on output, apply Bug 2 Fix A or Fix B
Step 6: Implement serial (Bug 4) — gives independent debug channel via IPMI SOL
Step 7: If BlSupportPei fix succeeds, PeiDispatcher completes and PeiCore enters second pass
Step 8: Monitor UEFI Shell boot via VGA/serial
```

### 15.2 VGA Debug Reference

All VGA debug functions are defined in:
- `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` lines 1–155: `mde_1_edkii_vga_*` functions
- `DasharoPayloadPkg/SecCore/edkii_vga.c`: `edkii_vga_*` functions (SecCore phase)

Usage from PeiMain/Dispatcher context:
```c
mde_1_edkii_vga_sprintf(row_number, "fmt string", args...);
// row_number: 0–24 (VGA 80×25 text mode)
// Currently used: rows 0–21 by PeiMain.c
// Use rows 22–24 for new Dispatcher/BlSupportPei messages
```

### 15.3 VGA Layout at Last Known Good State (1405-02-25 screenshot)

```
Row  0:  [1]PC:D=0 S=8FE94 P=8030B4
Row  1:  [1]BFV=800000,SZ=FF800000,TB=80000,TS=10000
Row  2:  [1]PD=8FA30 PS=0
Row 19:  [1]k-Ps:8FA34, SC:8FE94, OD:0
Row 21:  [1]u-Sec:8FE94, PD:8FA30,BM:0,PMI:0   ← LAST VGA LINE
Rows 22–24: blank — add new debug here
```

### 15.4 Expected Output After Bug 2 Fix

If `UsableLowMemTop` is populated correctly by Fix A:
```
Row 22+:  BLP: LMTop=80000000          ← 2 GB low memory top (example)
          BLP: PeiMemBase=7C000000     ← 2GB - 64MB = 1984 MB
          BLP: Install OK
```

If `UsableLowMemTop = 0` appears, the coreboot memory table is not reporting any
low-memory (< 4 GB) RAM region ≥ 64 MB. In that case, apply Fix A to MemInfoCallback
(track any low-RAM region regardless of size).

---

## Summary of Key Files

| File | Purpose | Status |
|---|---|---|
| `src/mainboard/asrock/spc741d8/board.fmd` | Flash map: 64MB layout | Stock — OK |
| `src/mainboard/asrock/spc741d8/devicetree.cb` | Hardware: COM1 at 0x3F8, IPMI KCS | Stock — OK |
| `configs/config.asrock_spc741d8` | Base config with EDK II payload | Stock — use as .config |
| `MdeModulePkg/Core/Pei/PeiMain/PeiMain.c` | PEI core + VGA debug (custom) | Bug 1 fixed; needs cherry-pick |
| `DasharoPayloadPkg/SecCore/SecMain.c` | SEC phase entry + VGA (custom) | Bug 4 pending |
| `DasharoPayloadPkg/BlSupportPei/BlSupportPei.c` | HOB builder from LBIO table | Bug 2 pending (crash root cause) |
| `MdeModulePkg/Core/Pei/Dispatcher/Dispatcher.c` | PEIM dispatcher | Bug 3 pending (diagnostic) |
| `DasharoPayloadPkg/SecCore/edkii_vga.c` | VGA output for SEC phase | Working |
| `3rdparty/dasharo-blobs/asrock/spc741d8/` | ME + descriptor blobs | Required |
| `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd` | Intel FSP for Xeon SP | Required |

---

*Report covers: server.bin (SHA256: db303ef4...), branch edk2-server from tag asrock_spc741d8_v0.9.0,
ifdtool dump, board.fmd, devicetree.cb, .config, EDK II workspace layout, full PEI crash analysis,
BlSupportPei HOB construction code path, MemInfoCallback UsableLowMemTop bug, all recommended fixes.*
