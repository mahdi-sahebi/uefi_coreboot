# VGA/EDK2 boot diagnosis and SOL configuration

Date: 2026-10-06

## Result

The last reliable execution point is the PEI dispatcher while entering
`FaultTolerantWritePei.efi` (FTW PEIM). The matching marker is:

```text
MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c:360
GxVgaCheckpoint (10, "[GX] module=FtwPei event=entry status=success");
```

Existing SOL output reaches `Loading PEIM ... FaultTolerantWritePei.efi` and
then stops before later DXE markers. The historical trace also reports:

```text
ASSERT [PeiCore] AcpiTimerLib.c(57): GuidHob != ((void *) 0)
```

This is an early-PEI/HOB diagnostic; it is not evidence that DXE or BDS was
reached. The FTW failure is associated with invalid or zero SMMSTORE geometry:
`WorkSpaceInSpareArea` is calculated in
`MdeModulePkg/Universal/FaultTolerantWritePei/FaultTolerantWritePei.c`, and
invalid geometry can cause the backward workspace scan to underflow or never
find a valid working-block header.

## Applied configuration on `feature/vga`

The branch already contains the SOL configuration equivalent to `feature/log`:

```text
CONFIG_CONSOLE_SERIAL=y
CONFIG_DRIVERS_UART_8250IO=y
CONFIG_DRIVERS_GENERIC_CBFS_SERIAL=y
CONFIG_EDK2_SERIAL_SUPPORT=y
CONFIG_EDK2_DASHARO_SYSTEM_FEATURES=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION_DEFAULT_ENABLE=y
CONFIG_EDK2_DASHARO_SERIAL_REDIRECTION2_DEFAULT_ENABLE=y
CONFIG_EDK2_HAVE_2ND_UART=y
CONFIG_EDK2_DUAL_SERIAL_DEBUG=y
CONFIG_EDK2_PRINT_SOL_STRINGS=y
CONFIG_SMMSTORE=y
CONFIG_SMMSTORE_V2=y
CONFIG_SMMSTORE_SIZE=0x40000
```

The payload remains configured for the `mahdi-sahebi/uefi_edk2` `feature/vga`
revision on this branch, with the existing error-level debug PCDs:

```text
PcdDebugPrintErrorLevel=0x80000000
PcdFixedDebugPrintErrorLevel=0x80000000
```

Root `build/` is now ignored, and no files under it are tracked.

## Validation

Build and flash the image, then capture SOL at 115200 baud during cold and
warm boots. Confirm that the trace continues beyond FTW and later shows DxeIpl,
DxeCore, dispatcher, and BDS markers. Compare the VGA `[GX]` sequence with
the same sequence in SOL; a stopped VGA screen alone does not prove execution
stopped because the console may transition to GOP/framebuffer output.

The supplied M4V is H.264; local frame decoding was unavailable, so the exact
text of its final frame still requires confirmation from a decoded frame or a
fresh hardware SOL capture.

## Build result and remaining limitation

The `feature/vga` build completed successfully. EDK2 built from
`origin/feature/vga`, and `build/coreboot.rom` contains `fallback/payload` plus
the preserved 256 KiB `SMMSTORE` region.

No hardware flash or runtime SOL capture was performed here. The configuration
fix and image build are verified, but the FTW/HOB failure is hardware-confirmed
only after flashing this image and capturing cold and warm boots over SOL.
