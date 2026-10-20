# Building Open-Source Dasharo (coreboot + EDK II) Firmware for Your Gooxi G4DEL Server

## What you are doing and why

Your Gooxi G4DEL server shipped with a closed, proprietary BIOS — an **AMI Aptio V** image customized by Gooxi. You cannot read, audit, or modify it. The goal of this project is to replace that closed firmware with **Dasharo**, an open-source firmware distribution built on **coreboot** (the open boot core) plus an **EDK II** UEFI payload (the open UEFI layer). Because no one has written a coreboot port specifically named "Gooxi G4DEL," we reuse the **`asrock/spc741d8`** port (the ASRock Rack SPC741D8-2L2T/BCM). That board uses the *same* Intel Xeon SP "Sapphire Rapids" / "Eagle Stream / Archer City" silicon as your Gooxi, so its chipset init, FSP, and memory training all apply directly to your machine.

> You are not starting from zero. Most of the hard setup in this repo is already done. This guide walks you through it end to end so you understand *why* each piece exists, then shows you the one real bug that still stops the boot today and how to fix it.

---

## 1. The big picture: the layers of firmware on your chip

Your server has a single **64 MiB SPI flash chip** (64 MiB = 67,108,864 bytes). That one chip holds *several* different firmwares stacked together. Think of it like a layered cake — the CPU reads the whole thing, and different layers are owned by different programs:

```
┌──────────────────────────────────────────────────────────────┐
│  64 MiB SPI flash chip (one physical part)                     │
├──────────────────────────────────────────────────────────────┤
│  [0x0000000]  Flash Descriptor (IFD)  — the "table of contents"│
│  [0x0003000]  Intel ME firmware       — Intel's hidden co-CPU  │
│  [          ]  GbE                     — network MAC/config     │
│  [0x3000000]  coreboot (OPEN)          — the boot core          │
│                 └─ FSP (Intel blob)    — chipset/memory secrets │
│                 └─ EDK II payload (OPEN)— the UEFI layer         │
└──────────────────────────────────────────────────────────────┘
```

**Flash Descriptor (IFD = Intel Flash Descriptor).** The first 4 KiB of the chip. It is a tiny table that tells the hardware where every other region lives (where ME starts, where the BIOS region starts, etc.) and who is allowed to write to each region. It comes straight from your original `server.bin` — we do not invent it.

**Intel ME (Management Engine).** A small, separate processor baked into the chipset that runs its own firmware (~16 MiB) regardless of the main CPU. In this project the ME firmware has been **neutralized** (its "HAP" bit is set, which tells the ME to halt early). This layer also comes from your `server.bin`.

**GbE (Gigabit Ethernet).** A small (8 KiB) region holding the network controller's MAC address and configuration. Also taken from `server.bin`.

**coreboot (open source).** This is the star of the show: the open replacement for the AMI BIOS. It does the bare-metal bring-up — turning on RAM, initializing devices — and then hands off to a "payload."

**FSP (Firmware Support Package).** A *binary blob from Intel* that coreboot calls to do the parts Intel keeps secret: training the DDR memory (FSP-M) and finishing silicon setup (FSP-S). coreboot is open, but it cannot avoid using this Intel blob on modern Xeon parts.

**EDK II payload (open source).** EDK II is the reference implementation of UEFI (Unified Extensible Firmware Interface — the modern BIOS standard). coreboot loads it last. It provides the UEFI environment: drivers, the boot menu, and eventually the OS launch. In this build it is the **Dasharo** flavor of EDK II.

---

## 2. What you already have

The heavy lifting is done. Here is the current state of your tree:

| Thing | Value / location |
|---|---|
| Repo root | `/home/mahdi/repositories/coreboot/dasharo/coreboot` |
| Git branch | `hob` |
| Origin BIOS dump | `/home/mahdi/repositories/reconstruction/server/server.bin` (64 MiB, sha256 `db303ef4…100465`) |
| Board port in use | `src/mainboard/asrock/spc741d8` (ASRock Rack SPC741D8-2L2T/BCM) |
| Blobs (from `server.bin`) | `3rdparty/blobs/mainboard/asrock/spc741d8/`: `descriptor.bin` (4K), `me.bin` (~16M, neutralized/HAP), `gbe.bin` (8K), `vgabios.bin` |
| Intel FSP blob | `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd` |
| Cross-compiler | `util/crossgcc/xgcc/bin/` — `x86_64-elf-gcc`, `i386-elf-gcc`, `iasl` (already built) |
| EDK II workspace | `payloads/external/edk2/workspace/dasharo` (Dasharo/edk2, commit `a69960a1…`) |
| Built ROM (already exists) | `build/coreboot.rom` (64 MiB / 67,108,864 bytes, sha256 `16dfcfc5…1da323`) |
| Built payload (already exists) | `build/UEFIPAYLOAD.fd` (sha256 `57306a96…8aacd4`) |

> Reassurance: the toolchain is built, the blobs are extracted, the board is selected, and a complete `build/coreboot.rom` already exists from a prior run. You can rebuild it reproducibly with a single `make`. Nothing below requires you to download or guess anything.

---

## 3. Step 0: One-time setup (toolchain + helper tools)

coreboot must be compiled with its *own* cross-compiler — a GCC built specifically to target bare-metal x86, not your host Linux. This avoids subtle bugs from your distro's compiler. In this tree it is already built and lives under `util/crossgcc/xgcc/`. **You do not need to add it to your `PATH`** — coreboot's build finds it automatically via `util/xcompile`.

**Work from the repo root for every command:**

```console
$ cd /home/mahdi/repositories/coreboot/dasharo/coreboot
```

**Confirm the cross-compiler is present:**

```console
$ util/crossgcc/xgcc/bin/x86_64-elf-gcc --version
x86_64-elf-gcc (coreboot toolchain v2024-12-19_e3150e819d) 14.2.0
...
$ util/crossgcc/xgcc/bin/i386-elf-gcc --version
$ util/crossgcc/xgcc/bin/iasl -v
ACPI Component Architecture ... version 20230628
```

(`iasl` is the **I**ntel **A**CPI **S**ource **L**anguage compiler — it builds the ACPI tables that describe your hardware to the OS.)

**Ask the Makefile whether the toolchain matches what this coreboot expects:**

```console
$ make test-toolchain
The coreboot toolchain is the current version.
```

If — and *only* if — that command instead says `The coreboot toolchain is not the current version.`, rebuild the two x86 compilers plus IASL. This is the slow step (~20–40 minutes); use all your CPU cores. **Note the target arch is `x64`, not `x86_64`:**

```console
$ make crossgcc-x64 CPUS=$(nproc)       # 64-bit compiler + IASL
$ make crossgcc-i386 CPUS=$(nproc)      # 32-bit pieces coreboot also needs
```

Each of those also runs `build_iasl`, so `iasl` is built too. (To build every architecture at once: `make crossgcc CPUS=$(nproc)` — but that is overkill for an x86 board.)

**Offline / mirror note.** If the toolchain downloads fail, pull tarballs from the coreboot mirror by appending `BUILDGCC_OPTIONS='-m'`:

```console
$ make crossgcc-x64 CPUS=$(nproc) BUILDGCC_OPTIONS='-m'
```

The compilers always install into `util/crossgcc/xgcc/`, which is exactly where the build auto-detects them.

> Gotcha: there is **no** `crossgcc-x86_64` target — that command errors out. The valid arches are `i386 x64 arm aarch64 riscv ppc64 nds32le`. The binaries are still *named* `x86_64-elf-*`, but the *make target* is `crossgcc-x64`.

**Host packages.** You still need standard host-side tools for building the helper utilities and running the FSP split script: `gcc`, `make`, `python3`, `m4`, `bison`, `flex`, the `zlib`/`openssl` development headers, and `pkg-config`.

**Build the inspection helper tools** (these are also built by the main `make`, but you can build them standalone — and `cbfstool` already exists in this tree):

```console
$ make -C util/cbfstool      # -> build/util/cbfstool/cbfstool and .../fmaptool
$ make -C util/ifdtool       # -> build/util/ifdtool/ifdtool
```

- **`ifdtool`** reads/edits the Intel Flash Descriptor and can split a flash image into its regions.
- **`cbfstool`** lists and edits CBFS (the **C**ore**b**oot **F**ile **S**ystem — how coreboot stores its stages and payload inside the BIOS region).
- **`fmaptool`** works with the FMAP (**F**lash **MAP** — the master region table inside the BIOS area).

---

## 4. Step 1: Get the blobs from your `server.bin` (already done)

coreboot is open, but it still needs four binary pieces it cannot generate: the Intel Flash Descriptor, the ME firmware, the GbE config, and the legacy VGA BIOS. These were extracted from your original 64 MiB dump using `ifdtool -x`, which splits a flash image into its regions:

```console
$ util/ifdtool/ifdtool -x server.bin   # produces flashregion_*.bin files
```

The relevant outputs were copied (and renamed) into:

```
3rdparty/blobs/mainboard/asrock/spc741d8/
├── descriptor.bin   (4 KiB   — the IFD)
├── me.bin           (~16 MiB — Intel ME, neutralized with the HAP bit)
├── gbe.bin          (8 KiB   — network MAC/config)
└── vgabios.bin      (legacy VGA option ROM for early text on the monitor)
```

> The ME in `me.bin` is **neutralized**: the HAP ("High Assurance Platform") strap bit is set so the ME halts itself early. Installing this version requires flashing the *full* chip externally (see Step 5). If you only ever flash the BIOS region, the *factory* ME stays in place. Keep a factory backup so you can always revert.

The full step-by-step of how these were extracted is documented in **`logs/analysis_report.md`** — refer to it if you ever need to re-extract. It's already done here, so this section is just for understanding.

---

## 5. Step 2: Configure coreboot for the board

coreboot's configuration lives in a single file at the repo root called **`.config`**. It is a long list of `CONFIG_*=...` lines that select your board, ROM size, payload, blobs, and features. This is managed by **Kconfig**, the same configuration system the Linux kernel uses.

### Fast path (recommended): reuse the good `.config` already in this tree

The `.config` in this repo is already correct for this board. Just normalize it against the current Kconfig rules:

```console
$ cd /home/mahdi/repositories/coreboot/dasharo/coreboot
$ make olddefconfig
```

If it asks you no questions, you are set.

**Confirm the important selections landed** (these are already true here):

```console
$ grep -E 'CONFIG_BOARD_ASROCK_SPC741D8|CONFIG_MAINBOARD_DIR|CONFIG_COREBOOT_ROMSIZE_KB=|CONFIG_PAYLOAD_EDK2|CONFIG_PAYLOAD_FILE|CONFIG_FSP_FULL_FD|CONFIG_HAVE_IFD_BIN|CONFIG_HAVE_ME_BIN|CONFIG_IFD_BIN_PATH|CONFIG_ME_BIN_PATH' .config
CONFIG_BOARD_ASROCK_SPC741D8=y
CONFIG_MAINBOARD_DIR="asrock/spc741d8"
CONFIG_COREBOOT_ROMSIZE_KB=65536
CONFIG_PAYLOAD_EDK2=y
CONFIG_PAYLOAD_FILE="build/UEFIPAYLOAD.fd"
CONFIG_FSP_FULL_FD=y
CONFIG_HAVE_IFD_BIN=y
CONFIG_HAVE_ME_BIN=y
CONFIG_IFD_BIN_PATH="3rdparty/blobs/mainboard/asrock/spc741d8/descriptor.bin"
CONFIG_ME_BIN_PATH="3rdparty/blobs/mainboard/asrock/spc741d8/me.bin"
```

What these mean in plain language:
- `CONFIG_BOARD_ASROCK_SPC741D8=y` and `CONFIG_MAINBOARD_DIR="asrock/spc741d8"` — the board we are building for.
- `CONFIG_COREBOOT_ROMSIZE_KB=65536` — 65,536 KiB = **64 MiB**, which *must* match your physical chip and the origin `server.bin`.
- `CONFIG_PAYLOAD_EDK2=y` + `CONFIG_PAYLOAD_FILE="build/UEFIPAYLOAD.fd"` — the EDK II UEFI payload, built automatically (Step 3).
- `CONFIG_FSP_FULL_FD=y` — use the single full FSP blob and split it during the build.
- `CONFIG_HAVE_IFD_BIN`/`CONFIG_HAVE_ME_BIN` + paths — pull the descriptor and ME from your extracted blobs.

### From-scratch path (optional, to learn the menus)

If you want to see the menus, **back up the good config first**:

```console
$ cp .config .config.known-good
$ make distclean        # WARNING: deletes .config AND build/ — only after the backup above
$ make menuconfig
```

In `menuconfig`:
- **Mainboard → Mainboard vendor** = `ASRock`; **Mainboard model** = `SPC741D8-2L2T/BCM` (this is the `spc741d8` dir). Do *not* pick the Intel "Archer City CRB" variant unless you specifically want the bare reference board.
- **Chipset** → enable *Add Intel descriptor.bin* and *Add Intel ME/TXE firmware*, with paths `3rdparty/blobs/mainboard/asrock/spc741d8/descriptor.bin` and `.../me.bin`; set **ROM chip size** = `64 MiB`.
- **Payload → Payload to add** = `Tianocore coreboot payload package (EDK II)`.

Save and exit, then run `make olddefconfig` to settle dependencies.

### The flash layout: `board.fmd`

The BIOS region's internal layout is described by a **`.fmd`** file (**F**lash **M**ap **D**escriptor — a small text file that gets compiled into the binary FMAP). For this board:

```console
$ cat src/mainboard/asrock/spc741d8/board.fmd
FLASH 64M {
	SI_ALL@0x0 0x03000000 {
		SI_DESC@0x0 0x1000
		SI_ME@0x3000 0x2fed000
		SI_PT@0x2ff0000 0x10000
	}

	SI_BIOS@0x3000000 0x1000000 {
		#RW_MRC_CACHE@0x3000000 0x10000
		#SMMSTORE(PRESERVE) 256K
		BOOTSPLASH(CBFS) 1M
		FMAP 0x800
		COREBOOT(CBFS)
	}
}
```

Reading this top to bottom:
- `FLASH 64M` — the whole chip is 64 MiB. This *must* equal `CONFIG_COREBOOT_ROMSIZE_KB=65536` and your physical part.
- `SI_ALL@0x0 0x03000000` — the lower 48 MiB, owned by Intel: descriptor + ME + a small partition table region.
  - `SI_DESC@0x0 0x1000` — the 4 KiB Intel Flash Descriptor.
  - `SI_ME@0x3000 0x2fed000` — the ME firmware region.
  - `SI_PT@0x2ff0000 0x10000` — an Intel partition-table region.
- `SI_BIOS@0x3000000 0x1000000` — the top 16 MiB: this is where coreboot lives.
  - `BOOTSPLASH(CBFS) 1M` — space reserved for a boot logo image stored in CBFS.
  - `FMAP 0x800` — the binary flash-map table itself.
  - `COREBOOT(CBFS)` — the main CBFS area holding bootblock, romstage, ramstage, postcar, FSP-S, and the EDK II payload.

> **Important:** the `#SMMSTORE(PRESERVE) 256K` line is **commented out** (the leading `#`), and `CONFIG_SMMSTORE` is not set. SMMSTORE is the flash "drawer" where UEFI saves its settings. Its absence is the direct cause of the boot hang you'll read about in §9. Leave it commented for now unless you are doing the fix in §11.

You can confirm there is no SMMSTORE region later with the inspection tools (Step 4).

---

## 6. Step 3: Build the image

From the repo root, **one command does everything** — it builds the host tools, builds the EDK II payload, splits the FSP, and assembles the final ROM:

```console
$ cd /home/mahdi/repositories/coreboot/dasharo/coreboot
$ make -j$(nproc)
```

What `make` does automatically, in order:

1. **Builds host utilities** into `build/util/`: `cbfstool` (`build/util/cbfstool/cbfstool`), `fmaptool` (`build/util/cbfstool/fmaptool`), and `ifdtool` (`build/util/ifdtool/ifdtool`).
2. **Splits the full FSP.** Driven by `CONFIG_FSP_FULL_FD=y`, it runs `python 3rdparty/fsp/Tools/SplitFspBin.py` on `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd` to produce `build/Fsp_M.fd` (memory init) and `build/Fsp_S.fd` (silicon init).
3. **Compiles coreboot** — bootblock, romstage, ramstage, postcar — using the `xgcc` toolchain.
4. **Builds the EDK II payload automatically.** Because `CONFIG_PAYLOAD_EDK2=y` / `CONFIG_EDK2_UEFIPAYLOAD=y`, the same `make` descends into `payloads/external/edk2`. Its Makefile checks out the Dasharo/edk2 repo into `payloads/external/edk2/workspace/dasharo` and runs the EDK II `build` of `DasharoPayloadPkg/DasharoPayloadPkg.dsc` (with `-t COREBOOT -D BOOTLOADER=COREBOOT -D BUILD_ARCH=X64`). It produces **`build/UEFIPAYLOAD.fd`**, which `CONFIG_PAYLOAD_FILE` then packs into CBFS. **You do not build the payload separately.**

**The final artifact** is `build/coreboot.rom` — the complete 64 MiB image (descriptor + ME + BIOS) ready to flash. It already exists in this tree from a prior run; a fresh full build reproduces it. If anything fails, the build log is `build/build.log`.

**Expected artifacts, sizes, and checksums:**

```console
$ ls -l build/coreboot.rom
-rw-rw-r-- 1 mahdi mahdi 67108864 ... build/coreboot.rom      # 64 MiB exactly
$ sha256sum build/coreboot.rom build/UEFIPAYLOAD.fd
16dfcfc5...1da323  build/coreboot.rom
57306a96...8aacd4  build/UEFIPAYLOAD.fd
```

A reproducible rebuild from the same sources and blobs should match these, or be very close — embedded build IDs and timestamps can cause tiny differences.

**Rebuild tips:**
- To force a fresh payload: `rm -f build/UEFIPAYLOAD.fd && make -j$(nproc)`.
- To just re-pack coreboot after the payload exists: plain `make -j$(nproc)`.
- **Before switching to a *different* mainboard, always `make distclean`** (after `cp .config .config.known-good`) — stale `build/` artifacts and a stale `.config` cause confusing failures. For a config-only refresh that keeps your selections, use `make olddefconfig`, *not* `distclean`.

> Two things that will stop the build cold, by design: `WARNINGS_ARE_ERRORS=y` means any compiler warning aborts the build (read `build/build.log` and fix the real warning — don't disable this), and the blobs are **mandatory** (`CONFIG_USE_BLOBS=y`) — a wrong blob path stops the build with a missing-file error.
>
> Also note: the repo's `build.sh` is a Dasharo per-board helper that has **no** `asrock/spc741d8` / Gooxi target. Do *not* run `./build.sh g4del`. Use plain `make` as above.

---

## 7. Step 4: Inspect the image

Before flashing, verify the image is what you expect. The tools were built in Step 0/3.

**1. Inspect the Intel flash descriptor regions** (confirm they match `board.fmd`):

```console
$ build/util/ifdtool/ifdtool -d /home/mahdi/repositories/coreboot/dasharo/coreboot/build/coreboot.rom
# Expect: Flash Descriptor region at 0x0,
#         ME region spanning roughly 0x3000-0x2fef000,
#         BIOS region 0x3000000-0x3ffffff (16 MiB).
```

To extract the regions (to diff against your blobs): `build/util/ifdtool/ifdtool -x build/coreboot.rom`.

**2. List the CBFS contents** (stages, payload, FSP, configs) inside the BIOS region:

```console
$ build/util/cbfstool/cbfstool /home/mahdi/repositories/coreboot/dasharo/coreboot/build/coreboot.rom print
# Expect to see: bootblock, fallback/romstage, fallback/ramstage,
#                fallback/postcar, the FSP-S blob, config/revision,
#                and the payload (fallback/payload = the EDK II UEFIPAYLOAD).
```

`cbfstool` auto-detects the `COREBOOT` region; if it complains, add `-r COREBOOT`.

**3. Dump the full flash map** (confirm BOOTSPLASH, FMAP, COREBOOT, and 64 MiB total, and that there is **no** SMMSTORE region):

```console
$ build/util/cbfstool/cbfstool build/coreboot.rom layout -w
$ build/util/cbfstool/cbfstool build/coreboot.rom read -r FMAP -f /tmp/fmap.bin
$ build/util/cbfstool/cbfstool build/coreboot.rom print -r FMAP
```

The absence of an SMMSTORE region here corroborates the hang root cause in §9.

**4. Confirm size and integrity:**

```console
$ ls -l /home/mahdi/repositories/coreboot/dasharo/coreboot/build/coreboot.rom
-rw-rw-r-- 1 mahdi mahdi 67108864 ...        # must be 67108864 bytes = 64 MiB
$ sha256sum build/coreboot.rom build/UEFIPAYLOAD.fd
```

> A **size mismatch** between the ROM image and the physical chip is a classic way to brick a board. `build/coreboot.rom` must be exactly 67,108,864 bytes, and `flashrom` must detect a 64 MiB chip.

---

## 8. Step 5: Flash it to the server

There are two ways to write the chip. **Strongly prefer the external programmer** — it is the only path that cannot be bricked by software, and it is the only way to actually install the neutralized ME.

### Recommended: external SPI programmer (safe, unbrickable-from-software)

1. **Power the server OFF and unplug it.**
2. Clip a **SOIC-8 / SOIC-16 test clip** (e.g. Pomona 5250/5252) onto the BIOS SPI flash chip, driven by a **CH341A** or any flashrom-supported programmer (`ch341a_spi`; or a Dediprog with `dediprog`; or a Raspberry Pi with `linux_spi`). The chip is a 64 MiB (512 Mbit) part.

3. **Verify the programmer sees the chip:**

```console
$ flashrom -p ch341a_spi
# Prints the detected chip. If multiple match, pin the exact one, e.g.:
$ flashrom -p ch341a_spi -c "MX25L51245G"
```

4. **Read and keep TWO factory backups before writing anything**, and verify they are identical:

```console
$ flashrom -p ch341a_spi -r /home/mahdi/repositories/coreboot/dasharo/coreboot/factory_backup1.bin
$ flashrom -p ch341a_spi -r /home/mahdi/repositories/coreboot/dasharo/coreboot/factory_backup2.bin
$ sha256sum factory_backup1.bin factory_backup2.bin
# The two must match. The original chip is sha256 db303ef4...100465.
```

5. **Write the full 64 MiB image** (descriptor + ME + BIOS). `flashrom` auto-verifies after writing:

```console
$ flashrom -p ch341a_spi -w /home/mahdi/repositories/coreboot/dasharo/coreboot/build/coreboot.rom
```

Because you are external, you can write the *whole* chip — including the descriptor and ME regions — which is exactly what makes this safe and what installs the neutralized ME.

### Internal flashrom (later convenience, riskier)

Boot a Linux on the running board. Read first: `flashrom -p internal -r current.bin`. A full internal write will almost certainly **fail** on the descriptor (`SI_DESC`) and ME (`SI_ME`) regions, because the Intel flash descriptor **locks** host writes to those regions. If you must use internal, restrict to the BIOS region with a layout:

```console
$ flashrom -p internal --ifd -i bios -w build/coreboot.rom   # writes only SI_BIOS@0x3000000 0x1000000
```

> **Descriptor / ME warnings.** `build/coreboot.rom` embeds the 4K `descriptor.bin` and the ~16 MiB **neutralized** `me.bin`. If you flash *only* the BIOS region internally, the board keeps the **factory (non-neutralized) ME**. To actually install the neutralized ME you must flash the full 64 MiB externally. Always keep `factory_backup1.bin` so you can fully restore the original chip.

After an external flash, remove the clip, reconnect power, and watch the serial console / VGA (§12). **Heads-up:** with this exact build the board boots coreboot all the way into the EDK II payload and then **hangs in the PEI Dispatcher** — that is expected and documented (next section), *not* a bad flash. To capture the boot, you can use **Serial-over-LAN via the BMC/IPMI** (§12).

---

## 9. What happens when it boots (and where it gets stuck today)

Here is the full boot chain, in order. The first part — coreboot — runs cleanly. The hang is near the end, in the EDK II payload.

1. **Power on.** The Intel Xeon SP "Sapphire Rapids" CPU (Eagle Stream / Archer City) starts executing its first instruction from the SPI flash. Fans spin up. The first firmware to run is **coreboot**, the open replacement for the Gooxi AMI BIOS.
2. **coreboot stage 1 — bootblock.** The tiny first piece. There is no usable RAM yet, so it sets up **cache-as-RAM** (using CPU cache as temporary memory) and loads the next stage. *(coreboot's stages are bootblock → romstage → ramstage → postcar; PEI/DXE are EDK II terms that come later.)*
3. **coreboot stage 2 — romstage.** Calls **FSP-M** (the memory-init part of Intel's FSP blob), which trains the DDR memory so real RAM becomes usable. The blob is `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd`.
4. **coreboot stage 3 — ramstage.** With RAM working, coreboot enumerates and initializes devices (PCIe, etc.), calls **FSP-S** (silicon init), and runs the VGA option ROM (`vgabios.bin`) so text can appear on the monitor.
5. **coreboot stage 4 — postcar, then the payload jump.** coreboot builds a "coreboot table" (memory map, framebuffer, ACPI info) and loads the payload. The handoff happens in `src/lib/prog_loaders.c` (`payload_load` → `selfload_mapped` → `prog_run`). The payload is **EDK II** (`build/UEFIPAYLOAD.fd`).
6. **EDK II phase 1 — SEC ("Security," the UEFI entry point).** Code: `DasharoPayloadPkg/SecCore/SecMain.c`, function `SecStartup()`. It receives the pointer to coreboot's table as `BootloaderParameter`, sets up a temporary stack, and prepares to call the next core.
7. **EDK II phase 2 — PEI ("Pre-EFI Initialization").** The PEI Core takes over (`MdeModulePkg/Core/Pei/PeiMain/PeiMain.c`). PEI runs a series of small drivers called **PEIMs** (PEI Modules) and records findings in **HOBs** (Hand-Off Blocks — small data records passed forward, e.g. "here is your RAM," "here is where variables live").
8. **The PEI Dispatcher** runs each PEIM in order. On this build: `PcdPeim` → `SmmStorePei` → `FaultTolerantWritePei` → `BlSupportPei` → … `SmmStorePei` is supposed to find the flash area for UEFI variables and record it in a HOB; `FaultTolerantWritePei` (FTW = the mechanism that prevents variable corruption on power loss) then uses that info.
9. **THE HANG.** The boot freezes during this first Dispatcher pass, inside **PEIM[2] = `FaultTolerantWritePei`**. It never gets past PEI.
10. **EDK II phase 3 — DXE (Driver eXecution Environment)** *(never reached)*: where the bulk of UEFI drivers load.
11. **EDK II phase 4 — BDS (Boot Device Selection)** *(never reached)*: shows the logo + boot menu and launches the OS.

> **Crash point (code-traced).** The freeze is inside `MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c`, function `PeimFaultTolerantWriteInitialize()` — file GUID `AAC33064-9ED0-4B89-A5AD-3EA767960B22`, dispatched as PEIM[2] at handle `0x829FE8`. It asks for the addresses of the "working" and "spare" variable-storage regions, gets **zeros** back, fails its (release-build, disabled) ASSERT at line 246, then enters the spare-block search loop at lines 289–301 where it loops forever.

> **Root cause, in plain language.** coreboot was built with the SMMSTORE feature **off** (`CONFIG_SMMSTORE` not set) and with **no SMMSTORE region** in the flash layout (the `#SMMSTORE(PRESERVE) 256K` line in `board.fmd` is commented out). SMMSTORE is the flash drawer where UEFI saves its settings ("variables"). Because it does not exist, `SmmStorePei` never creates the hand-off note (`gVariableFlashInfoHobGuid`) telling the next module where variable storage lives. When `FaultTolerantWritePei` runs, it falls back to fixed config values called **PCDs** (Platform Configuration Data) — but in `DasharoPayloadPkg.dsc` every relevant `PcdFlashNvStorage*` value is **0**. So the driver thinks variable storage is at address 0 with size 0. It computes `spare start + spare size − work size = 0 + 0 − 0 = 0`, then inside the loop subtracts a GUID's 16 bytes from 0. Because the value is an unsigned 64-bit integer, `0 − 16` does not go negative — it **underflows** and wraps to `0xFFFFFFFFFFFFFFF0`, near the top of the 64-bit range. The loop scans memory downward from there essentially forever, touching MMIO addresses, and the machine appears frozen.

*(There is also a separate, latent bug in `BlSupportPei`'s `MemInfoCallback` — a 64 MiB threshold that underflows `PeiMemBase` — but you never reach it because of the FTW hang above.)*

---

## 10. Why you don't see the EDK II splash logo

Two independent reasons:

1. **The boot dies two phases too early.** The UEFI logo is **not** drawn in PEI. It is painted much later, in the **BDS** phase, by a component called **`BootLogoLib`** (the same code behind the OS-handoff "BGRT" logo and the quiet-boot screen). Because the boot freezes back in PEI (inside `FaultTolerantWritePei`), it never reaches DXE or BDS, so the logo code never runs.
2. **No logo is even configured.** In `.config`, `CONFIG_EDK2_BOOTSPLASH_FILE` is empty (`""`) and the coreboot-side `CONFIG_BOOTSPLASH_IMAGE` is not set. The build log shows the EDK II option `BOOTSPLASH_IMAGE=TRUE` was passed, but that only *enables the drawing path* inside BDS — it would still show nothing until both the PEI hang is fixed *and* an actual image file is supplied.

So even if you added a logo image today, you would see nothing until the §11 fix is applied.

---

## 11. How to fix the hang

Pick one. **Option 1 is recommended** — it makes UEFI variables actually work and is the cleanest fix.

**Fix Option 1 — enable SMMSTORE and give it real flash space (recommended).** In your coreboot `.config`, enable SMMSTORE (use the V2 variant, `CONFIG_SMMSTORE_V2`). Then in `src/mainboard/asrock/spc741d8/board.fmd`, **un-comment** the SMMSTORE line inside the `SI_BIOS` region so a real region exists:

```diff
 	SI_BIOS@0x3000000 0x1000000 {
 		#RW_MRC_CACHE@0x3000000 0x10000
-		#SMMSTORE(PRESERVE) 256K
+		SMMSTORE(PRESERVE) 256K
 		BOOTSPLASH(CBFS) 1M
 		FMAP 0x800
 		COREBOOT(CBFS)
 	}
```

Now `SmmStorePei` finds the region, builds the `gVariableFlashInfoHobGuid` HOB, and `FaultTolerantWritePei` gets valid, non-zero addresses. (Touches `.config` + `board.fmd`; rebuild with `make -j$(nproc)`.)

**Fix Option 2 — hard-code the storage location.** In `payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/DasharoPayloadPkg.dsc` (around lines 633–645), set the currently-zero PCDs — `PcdFlashNvStorageVariableBase/Base64`, `PcdFlashNvStorageFtwWorkingBase/Base64`, `PcdFlashNvStorageFtwSpareBase/Base64` and the matching `…Size` PCDs — to a real reserved NV flash region with non-zero base and size. This stops the `0 − 16` underflow.

**Fix Option 3 — drop the on-flash variable stack entirely.** In `DasharoPayloadPkg.dsc`, switch the variable backend to emulation (`VARIABLE_SUPPORT=EMU`) and remove the `SmmStorePei` and `FaultTolerantWritePei` PEIM entries from the `.dsc`/`.fdf` so PEIM[2] never runs. Good for first bring-up; variables will not persist across reboots.

**Fix Option 4 — defensive code patch.** In `payloads/external/edk2/workspace/dasharo/MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c`, after fetching `WorkSpaceAddress` / `SpareAreaAddress` / lengths (around lines 229–246), add an explicit check that returns early (e.g. `return EFI_SUCCESS` without installing the broken state) when any base or size is 0, instead of relying on the ASSERT at line 246 that is compiled out in release builds. This prevents the underflow in the loop at lines 289–301.

---

## 12. How to watch it boot / debug

- **Serial over LAN via the BMC/IPMI (best — captures the whole boot).** The BMC (Baseboard Management Controller) is the always-on management chip; IPMI is its remote-management protocol. From another computer:

  ```console
  $ ipmitool -I lanplus -H <BMC-IP> -U <user> -P <pass> sol activate
  ```

  This pipes the server's serial console to your terminal so you watch coreboot and EDK II print live and see exactly where it stops.

- **The VGA monitor on the server.** Instead of the old Gooxi splash, you get coreboot's text/debug lines and the custom `[MN]` markers added in `src/lib/prog_loaders.c` (near `payload_load`). The screen goes quiet and stays frozen once PEI enters `FaultTolerantWritePei` — no UEFI logo, no boot menu.

- **CBMEM console (`cbmem -c`) — only after a successful boot to an OS.** coreboot keeps its log in memory; the `cbmem` tool (from coreboot's `util/cbmem`) dumps it. **Caveat:** because this build hangs *inside the payload*, you cannot reach an OS to run `cbmem` on the failing boot. It is most useful for confirming coreboot's own stages ran cleanly up to the payload jump, or after you apply a fix.

- **What the last good line looks like.** On the working path coreboot finishes its stages and prints the payload-jump markers; the last healthy sign is control entering EDK II (SEC → PeiCore → the PEI Dispatcher listing `PcdPeim`, then `SmmStorePei`, then `FaultTolerantWritePei`). The final line you will ever see is the dispatch of **PEIM[2] `FaultTolerantWritePei` (handle `0x829FE8`)**; after that the output stops and never advances to `BlSupportPei` or DXE. That dead stop is the hang.

---

## 13. Glossary

- **coreboot** — the open-source firmware that does bare-metal hardware bring-up (RAM, devices) and then jumps to a payload. The open replacement for the Gooxi AMI BIOS.
- **Dasharo** — an open-source firmware *distribution* built on coreboot + an EDK II payload, with downstream patches. This repo is the Dasharo flavor.
- **FSP (Firmware Support Package)** — a binary blob from Intel that performs chipset secrets coreboot can't: FSP-M trains memory, FSP-S finishes silicon init.
- **EDK II** — the open reference implementation of UEFI. Here it is the payload coreboot loads.
- **payload** — the program coreboot runs after hardware init. Here: the EDK II UEFI firmware (`build/UEFIPAYLOAD.fd`).
- **UEFI** — Unified Extensible Firmware Interface, the modern BIOS standard.
- **PEI** — Pre-EFI Initialization; the early EDK II phase that runs PEIMs and records HOBs.
- **DXE** — Driver eXecution Environment; the EDK II phase where most UEFI drivers load (never reached today).
- **BDS** — Boot Device Selection; the EDK II phase that shows the logo + boot menu and launches the OS (never reached today).
- **HOB (Hand-Off Block)** — a small data record passed forward between phases (e.g. where RAM or variable storage lives).
- **PEIM (PEI Module)** — a small driver run by the PEI Dispatcher.
- **FTW (Fault Tolerant Write)** — the mechanism that prevents UEFI variable corruption if power is lost mid-write. The hanging module is `FaultTolerantWritePei`.
- **SMMSTORE** — the flash region where UEFI saves its variables (settings). Disabled in this build; its absence causes the hang.
- **IFD (Intel Flash Descriptor)** — the first 4 KiB of the chip; the table of contents and access-control for all regions.
- **ME (Management Engine)** — Intel's separate co-processor and its firmware (~16 MiB). Neutralized (HAP bit set) in `me.bin`.
- **GbE** — Gigabit Ethernet region; holds the NIC MAC address and config (8 KiB).
- **CBFS (CoreBoot File System)** — how coreboot stores its stages, FSP-S, and the payload inside the BIOS region.
- **FMAP (Flash MAP)** — the binary master table of regions inside the BIOS area.
- **FMD (Flash Map Descriptor)** — the human-readable text file (`board.fmd`) that compiles into the FMAP.
- **blob** — a closed binary firmware piece (descriptor, ME, GbE, VGA BIOS, FSP) that coreboot uses but cannot build from source.

---

## 14. Where to go next / references

- **`logs/analysis_report.md`** — the full extraction → UEFI-shell guide (how the blobs were pulled out of `server.bin`, complete version of §4).
- **`logs/1405-03-03/analyize.md`** — VGA frame analysis of what appears on the monitor during boot.
- **`doc/vendor_inspection.md`** — confirms the origin BIOS vendor (Gooxi, AMI Aptio V, Intel Archer City / Eagle Stream Xeon SP, board G4DEL).
- **`src/mainboard/asrock/spc741d8/board.fmd`** — the flash layout you'll edit for the SMMSTORE fix (§11, Option 1).
- **`payloads/external/edk2/workspace/dasharo/DasharoPayloadPkg/DasharoPayloadPkg.dsc`** — the EDK II payload config (PCDs in §11, Options 2–3).
- **coreboot documentation:** https://doc.coreboot.org/ — especially the *Tutorial* and *Lessons* sections.
- **Dasharo documentation:** https://docs.dasharo.com/ — distribution-specific guidance and supported-board notes.

You have already done the hardest parts: the toolchain is built, the blobs are extracted, the board is configured, and a full `build/coreboot.rom` exists. Your next concrete milestone is the §11 Option 1 SMMSTORE fix — that is the single change standing between the current PEI hang and reaching the UEFI shell. Take it one step at a time; you've got this.
