# G4DEL preboot gate and HOB capture — 2026-11-22

This note records the screenshots and SOL text supplied from
`/home/uefi/workspace/logs/1405-08-29/1/1405-09-01/2/`. It describes this
capture set only. The flashed image hash was not included. The SOL file's
filesystem modification time is 14:29:45 +04, while the screenshot filenames
run from 14:38:13 through 14:47:11 +04. The SOL file ends before DXE, so it
cannot be paired confidently with the later screenshots or verify whether
their HOB trace was captured.

## Observed boot progress

- The SOL text records FSP-M `FspMemoryInit` returning status `0`, with HOB
  pointer `0x63900000`, and FSP-S `FspSiliconInit` returning status `0`.
- Coreboot loads the EDK2 payload and transfers control to EDK2 SEC. It reports
  a 64 MiB configured ROM while SPI probing reports 128 MiB and JEDEC `00:0000`.
  This discrepancy is visible but does not stop the shown payload handoff; this
  capture does not establish it as the Shell blocker.
- The screenshots progress through DXE driver dispatch and BDS. The visible
  DXE summary reports 77 drivers, 77 loaded, zero failures, and no pending
  runs. The USB driver-status page shows UHCI, EHCI, xHCI, and USB bus driver
  entries initialized.
- BDS displays a USB inventory with six descriptor-ready virtual devices,
  including virtual disk/CD-ROM and keyboard devices. The inventory reports
  eight USB Block I/O handles, no media present, zero Simple File System
  handles, and `Not Ready` for the last open-volume status. This only describes
  the shown USB devices; it does not establish whether an internal OS boot
  target exists.

## Current stop and interpretation

The final screenshots, at 14:46:55 and 14:47:11, show the G4DEL preboot
checks dashboard. It reports 11 checks, one failure, and no warnings:

- **PEI memory init / PHIT free-memory bounds: PASS.** This check passed in
  this capture, unlike the earlier screenshots that showed two failures.
- **HOB list: FAIL**, with the dashboard detail `invalid/truncated list`.
- Memory map, ACPI RSDP/root, CPU status, GOP, Block I/O, file systems, UEFI
  consoles, and Boot paths are shown as passing. The Boot paths check means
  `BootOrder` entries were readable; it does not prove that an OS loader was
  found or launched.

The screen is waiting at `REQUIRED CHECKS FAILED. Press B once to override or
R to rerun.` No screenshot shows either key being pressed, a Shell prompt, or
an OS handoff. Therefore the immediate reason Shell has not appeared is the
preboot gate waiting for input, not a demonstrated freeze inside Shell. The
remaining HOB failure could still be malformed runtime HOB data or an
incorrect validator assumption; these screenshots do not distinguish them.

## Capture gap and next evidence

`SOLHostCapture_Archive_1_11222026_18_07_59.log` is 12,288 bytes and 239
lines. Its final useful marker is EDK2 SEC entry. Since it predates the
screenshots by filesystem time and contains no DXE HOB values, it is an
incomplete or separate text capture, not evidence that the later firmware
run stopped at SEC. The screenshots show checkpoint output but do not expose
the PHIT addresses or the HOB header that caused validation to stop.

For the next run, save the complete raw SOL stream from reset through the
preboot dashboard and retain VGA screenshots with it. Record the flashed ROM
SHA-256. The HOB diagnostic records are now emitted directly to the DXE UART
with a `[GX-HOB]` prefix, independently of the DEBUG/status-code formatting.
Look for `PREBOOT HOB table`, `PREBOOT HOB PHIT`, each `PREBOOT HOB[index]`,
`PREBOOT HOB FAIL`, and `PREBOOT PHIT free range`. These identify
whether the list pointer, PHIT bounds, end pointer, HOB length, end marker, or
walk limit caused the rejection. Do not change the validator's pass/fail rule
until those values are available.

To test progress past the current gate, use the displayed one-time `B`
override or rerun with `R`; record the resulting Shell/boot-manager behavior.
Separately verify a real OS boot target and its filesystem, since this
dashboard's readable `BootOrder` result is not an OS boot test.
