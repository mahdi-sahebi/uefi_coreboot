# Gooxi G4DEL current status and board test

This is the coreboot-side record for the `feature/dxe-log` G4DEL port. The
EDK2 source investigation, marker list and USB/splash test are in
[the payload report](../payloads/external/edk2/workspace/mahdi-sahebi/doc/g4del_dxe_investigation.md).
The older coreboot [DXE notes](dxe.md) and the payload's
[original SOL timeline](../payloads/external/edk2/workspace/mahdi-sahebi/doc/1405_07_29.md)
remain historical evidence.

## Established platform facts and limits

- The build selects `gooxi/g4del`, `CONFIG_MAX_SOCKET=2`, 64 MiB ROM,
  Sapphire Rapids/Eagle Stream FSP-T/M/S, SMMSTORE v2, AST BMC SOL at I/O
  `0x2f8` and 115200 baud, and the `feature/dxe-log` EDK2 payload.
- The board owner reports 64 GiB RAM, four fans, BMC and SOL. CPU population,
  physical DIMM-slot count, USB port topology and exact PCIe IIO routing have
  not been verified. The Archer City two-socket IIO table is provisional.
- The factory 64 MiB `server.bin` was inspected previously. It is absent from
  the present workspace; factory-region equality can be checked against the
  previously validated ROM, but cannot now be independently rechecked against
  `server.bin`.
- The 2026-10-21 text SOL file ends at the old `FTWa-3` marker. A same-date
  SOL screencast continues to EDK2 `DxeCore entry`. No recording proves
  completion of DXE, the menu, USB input, Shell, or the logo on the current
  G4DEL image.

## Flash and FSP layout

`src/mainboard/gooxi/g4del/board.fmd` allocates:

| Offset | Size | Purpose |
| ---: | ---: | --- |
| `0x0000000` | `0x1000` | factory descriptor |
| `0x0001000` | `0x2000` | factory GbE |
| `0x0003000` | `0x2fed000` | factory ME |
| `0x2ff0000` | `0x10000` | factory Device Exp1 |
| `0x3000000` | `0x40000` | separate SMMSTORE v2 |
| `0x3040000` | `0x100000` | optional coreboot BOOTSPLASH slot |
| `0x3140000` | remainder | rebuilt COREBOOT/CBFS |

The 256 KiB SMMSTORE size is the FTW minimum at 64 KiB blocks, per
[coreboot's SMMSTORE documentation](https://doc.coreboot.org/drivers/smmstore.html).
The current `.config` leaves the coreboot splash image off, so a missing
bootsplash warning is expected and the reserved slot is empty. EDK2 packages
its own built-in BMP through `LogoDxe`; its display requires GOP and is
tracked in the payload report. Do not treat the empty coreboot slot as the
cause of a missing EDK2 menu or logo.

The board's FSP-M UPD enables serial debug at `CONFIG_TTYS0_BASE` and uses the
provisional two-socket IIO map. New `[GX-FSP]` messages at `BIOS_ERR` bracket
FspMemoryInit and FspSiliconInit, so they remain visible at the configured
`BIOS_ERR` console threshold. The existing coreboot `[GX]` payload load and
handoff messages include the payload entry and HOB address. The half-second
VGA pause during each payload checkpoint has been removed. No FSP binary or
SPI region size was changed in this diagnostic revision.

## Build and image checks

Run `make -j8` from this coreboot root. Then run:

```sh
sha256sum build/coreboot.rom build/UEFIPAYLOAD.fd
build/cbfstool build/coreboot.rom layout
build/cbfstool build/coreboot.rom print -k
git diff --check
```

The generated ROM must be exactly 67,108,864 bytes and contain FSP-T/M/S and
`fallback/payload` in CBFS. Confirm the payload's generated PEI/DXE firmware
volume manifests include SmmStorePei, FTW, BlSupportPei, DXE IPL, xHCI,
USB bus/keyboard/storage, BootManagerMenuApp and Shell. This establishes
packaging only; firmware-volume inclusion does not prove that a DXE driver
binds to a device. Use the exact ROM hash when naming the next SOL recording.

The forced integrated build on 2026-10-23 passed. Its current ROM SHA-256 is
`5ce4c49d71076f7bae3c0f16dd910a22d89c1ed6a683361ee40650fd63e8af26`;
its `build/UEFIPAYLOAD.fd` SHA-256 is
`e1d680421a048263a80fd6cac89c4ba6c52b1e1276d7d1c5ac4af0edb0c75d6e`.
The first `0x3000000` ROM bytes match `build/coreboot_07-30.rom`, whose
factory regions were checked in the earlier investigation. The factory image
itself is unavailable for a fresh comparison. The EDK2 wrapper's generated
firmware volumes are under `payloads/external/edk2/workspace/Build`, separate
from the payload Git checkout's own `Build` directory. The wrapper FV contains
the `[GX-XHCI]`, `[GX-USB]`, `[GX-LOGO]`, menu, Shell and DXE dispatcher
marker strings. An earlier incremental build kept an old payload; removing
the output artifact forced this verified rebuild.

## Board test and decision points

1. Save a readback of the current known-good firmware and the hash of the
   image flashed for the test. Capture BMC SOL from reset at 115200 baud and
   BMC video simultaneously.
2. Confirm `[GX-FSP]` begin/return pairs, coreboot payload handoff, EDK2 SEC,
   SmmStorePei, FTW, BlSupportPei, DXE IPL and DXE Core. If a pair lacks its
   return, investigate that call and its inputs.
3. Require DXE dispatcher completion and BDS console connection. Check
   `[GX-GOP]` and `[GX-LOGO]` for graphics and logo status. A disappearing
   VGA signal with continuing SOL means video failed or changed mode; it is
   not by itself evidence that the CPU stopped.
4. With a USB keyboard attached and Fast Boot disabled, require `[GX-XHCI]`
   start success, `[GX-USB]` host/device counts, enumeration VID/PID and
   keyboard-ready markers. Verify menu navigation. With a USB stick attached,
   verify mass-storage binding and a Shell mapping if the medium is formatted
   with a supported file system.
5. Select Shell and run `pci -b`, `drivers -b`, `devices -b` and `map -r`.
   `lsusb` is a Linux command and is not packaged in this UEFI Shell. Save the
   full output and screen recording. Menu, Shell, USB and splash are hardware
   verified only when these interactions succeed.

If the next capture stops later in DXE/BDS, use the first missing paired
checkpoint in the EDK2 report, change that failing operation, rebuild and
repeat. Do not alter FSP, IIO or flash layout solely because the previous
recording ended after `DxeCore entry`.
