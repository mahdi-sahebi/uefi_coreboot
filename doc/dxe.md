# Gooxie G4DEL DXE, boot menu, and Shell investigation

## Evidence and current result

- `SOLHostCapture_Archive_1_01011970_08_06_43.log` is 97,280 zero bytes and contains no recoverable serial text. The accompanying `Screencast from 20-10-26 15_40_30.m4v` shows the later DXE display. A second supplied capture, `SOLHostCapture_Archive_1_01011970_08_04_54 (7).log`, contains valid coreboot, SEC, and early PEI text; its findings are below.
- In that video, EDK2 completes SEC and PEI. SOL reaches the `D4m` checkpoint in `MdeModulePkg/Core/DxeIplPeim/Ia32/DxeLoadFunc.c` and then stops during the PEI-to-DXE handoff. VGA continues: it shows the later `D4k` checkpoint, `DxeCore entry`, handle and image service checkpoints, and finally `GCD services initialized` at about 1:57. VGA reports `No Signal` at about 1:58.
- `DxeMain.c` prints its GCD checkpoint *before* `ASSERT_EFI_ERROR (Status)`. The visible message therefore proves that `CoreInitializeGcdServices` returned, but does not prove it succeeded. There is no checkpoint between that assertion and completion of dispatcher initialization. The capture does not establish whether the machine stopped, reset, or continued without VGA.
- The active local EDK2 source is `payloads/external/edk2/workspace/mahdi-sahebi`, branch `gooxie-g4del`, initially at `db93215cf18118eeea9ff7369d9e871e08bdc9fa`. Coreboot is also on `gooxie-g4del`, initially at `d6bba3c96e347b0c8d63c7d02396ce35d636fb9c`. Both trees were clean before this investigation.
- Coreboot `.config` requests an EDK2 DEBUG build, serial at 115200, the boot manager, and the UEFI Shell. It retains `CONFIG_EDK2_TAG_OR_REV="origin/feature/serial"`. The active EDK2 build's DXE firmware volume includes both BootManagerMenuApp and Shell, and its compiler flags set `SEC_UART_DEFAULT_BASE=0x2f8`. Whether that exact image was flashed is unknown.

## Source mapping and problems

1. Before this change, `DxeMain.c` called `SecSerialPortConfigure (0)` before its first DXE checkpoint. This rescanned low memory for a coreboot table and could override the `0x2f8` build setting. The compiled DXE Core uses `BaseDebugLibSerialPort`, so a wrong UART selection would silence SOL without stopping VGA. That explanation is plausible, not yet confirmed by a live trace.
2. In the captured build, the DXE Core checkpoints for handle, image, and GCD services printed success-sounding text before checking their return statuses. They could not establish success. The interval after GCD and before dispatcher initialization includes runtime table allocations, library constructors, configuration tables, event and memory setup, firmware volume drivers, and section extraction. The captured build had no fine-grained progress record there.
3. `DxeMain.c` hands control to BDS only after the DXE dispatcher and architectural protocol checks. `BdsEntry.c` then connects consoles and calls `PlatformBootManagerBeforeConsole` and `PlatformBootManagerAfterConsole`; the latter registers the Shell boot option. The capture does not show these stages. The payload includes Shell, so absence from the build is not the observed problem.
4. The later `.log` is unusable and the earlier SOL capture ends before DXE; the flashed ROM/payload identity is unknown; and the video does not distinguish a system reset from loss of video signal. These limits prevent a defensible claim of the final root cause without a new board capture.

## Earlier SOL capture: coreboot and PEI

The 13,720-byte `SOLHostCapture_Archive_1_01011970_08_04_54 (7).log` contains 229 complete lines and no NUL bytes (SHA-256 `3ae0667c8e94a7be3ccb89672c244ae6e452909ac8172eeee2171dfba01913eb`). It records coreboot's payload handoff at `0x802ab0`, EDK2 SEC, the PCD and SmmStore PEIMs returning, and then FaultTolerantWritePei through `FTWa-2`. It contains no DXE IPL, DXE Core, BDS, menu, or Shell checkpoint. Its final complete line is a capture boundary, not evidence that firmware stopped there.

- Coreboot reports `SF size 0x8000000` versus `CONFIG_ROM_SIZE 0x4000000`: detected SPI flash is 128 MiB while this ROM is configured as 64 MiB. Coreboot nevertheless loads and enters the EDK2 payload. Verify the G4DEL flash layout and image placement before changing ROM size.
- Coreboot reports 64 assertions at `src/soc/intel/xeon_sp/spr/cpu.c:75` and one at `util.c:83`; both check a socket index against `CONFIG_MAX_SOCKET`. This inherited ASRock configuration sets `CONFIG_MAX_SOCKET=1`. The G4DEL-B is a dual-socket-capable board ([Gooxi manual](https://www.gooxi.com/uploadfiles/2025/03/20250311103819293.pdf)); the assertions indicate at least one enumerated CPU/socket index outside the configured range. Coreboot continues, but the mismatch may invalidate later CPU, IIO, and ACPI assumptions. Do not simply change the count while the inherited IIO table has settings only for socket 0; identify the installed CPUs and port the second-socket table first.
- ACPI DMAR construction reports assertions at `uncore_acpi.c:390`, `:448`, and `:542` when an expected VT-d PCI device is absent, then completes table writing and payload handoff. This may reflect the same incomplete board port, but the capture does not show it blocking EDK2.
- PEI reports `AcpiTimerLib.c(57): GuidHob != NULL` twice before the timer's `gUefiAcpiBoardInfoGuid` HOB is available. The code creates that HOB later in `BlSupportPei`; the capture does not show whether it was ultimately built. The PCD and SmmStore PEIMs continue after these assertions. Confirm timer/HOB availability in the next complete PEI/DXE trace.
- `logo.bmp` missing, TPM probe failure, and keyboard-controller reads of `0xff` are also present. They did not prevent SEC/PEI entry in this capture; investigate their respective hardware/configuration paths separately if they remain in the new ROM.

## Implementation plan

1. Preserve the working VGA display and the symbolic EDK2 revision setting. Keep DXE Core on the board-selected `0x2f8` UART instead of rescanning a possibly stale low-memory table; record a raw serial marker at DXE entry to distinguish UART routing from debug-library failure.
2. Add serial-only checkpoints with EFI return statuses across the GCD-to-dispatcher interval, then at BDS entry, console connection, boot option registration, menu launch, and Shell launch. Keep UART waits bounded and avoid long checkpoint delays.
3. Build EDK2 and coreboot, confirm that the ROM embeds the new payload, and record artifact hashes. Compare the flashed image identity before attributing the captured behavior to this source.
4. On G4DEL, capture VGA and SOL together through cold and warm boots. Use the last successful checkpoint and reset evidence to fix the specific failing operation; remove diagnostic noise after the cause is known. Verify the boot menu and an interactive UEFI Shell, including a boot with SOL disconnected.

## Implemented diagnostic build

- DXE Core now uses the board-selected serial base without a second low-memory CB-table scan. It writes a direct serial marker before its first normal debug checkpoint. SEC still parses the CB table using its bootloader parameter.
- Serial-only trace records now include handle, image, and GCD return statuses and bracket the operations from GCD through DXE dispatcher initialization. Successful VGA checkpoint content and order are preserved; those three lines display `FAILED` if the respective call returns an EFI error.
- BDS, BootManagerMenuApp, and Shell use the bounded serial library in this build. Their markers cover BDS entry, console connection, Shell option registration, menu status and entry, boot-option launch, and Shell entry.
- The first build exposed a constructor cycle when those UEFI modules used the generic 16550 serial library. Binding them to the existing `SecSerialPortLib` resolved it. No build source outside the active EDK2 tree was needed.

## Verification record

- EDK2 DEBUG payload build: **passed** on 2026-10-21. `build/UEFIPAYLOAD.fd` is 14,876,672 bytes; SHA-256 `29cc01b1df31b4e98bcda9b2b03bddc1866ae71d724d4442b52925599bfc6464`.
- Coreboot ROM build: **passed** on 2026-10-21. `build/coreboot.rom` is 67,108,864 bytes; SHA-256 `ad8aa425005cc9e25d1aad64cd0f9a1f8d7d4ac42273288e245dc54e14c98ff0`.
- The DXE firmware-volume manifest includes BootManagerMenuApp and Shell. The compiled DXE Core uses `SEC_UART_DEFAULT_BASE=0x2f8` and enables `DEBUG_ERROR`. CBFS contains `fallback/payload`; its extracted ELF has SHA-256 `ca415f1cd6c13e931b4de23085afc19c3e560fc0bc4ca404a9583daf8e568295` and contains the new `[GX-DXE]`, `[GX-BDS]`, `[GX-MENU]`, and `[GX-SHELL]` markers. This confirms the local ROM embeds the newly built payload.
- The configured revision remains `CONFIG_EDK2_TAG_OR_REV="origin/feature/serial"`, as requested. The EDK2 changes are committed as `d05d3facf911f90cfe75faafd28e5c435534e60e` on `feature/g4del-dxe-sol-menu`. A fresh checkout of `origin/feature/serial` will not contain those changes unless they are merged into that branch or the revision is explicitly overridden. The successful local build used the modified EDK2 working tree before commit.
- **Pending on G4DEL:** identify the flashed ROM, capture new VGA and SOL together, record the last checkpoint and EFI status, determine whether the observed `No Signal` was a reset or video loss, confirm the root cause, and verify an interactive menu and Shell with SOL connected and disconnected. The current local build cannot prove those hardware outcomes.

## Reading the next G4DEL capture

- VGA reaches `DxeCore entry` but SOL lacks `[GX-DXE] serial entry`: investigate the UART decode/BMC capture path at the DXE transition. The direct marker bypasses DebugLib.
- Any `status=...` other than `Success`, or a VGA line ending in `FAILED`: investigate that returned error before the next initialization step. A success-sounding checkpoint in the original video is insufficient evidence.
- If GCD or later memory setup fails, compare the PEI memory HOBs with the detected socket count and the inherited `CONFIG_MAX_SOCKET=1` configuration. This is a testable hypothesis from the earlier SOL assertions, not an established cause of the DXE failure.
- SOL stops between two named DXE stages: inspect the operation between those records. If `[GX-DXE] DXE dispatcher completed` appears but `[GX-BDS] entry` does not, inspect architectural protocols and the BDS handoff.
- `[GX-BDS]` reaches console connection but the display says `No Signal`: investigate video mode/GOP or console routing. `[GX-MENU] entry` and `[GX-SHELL] entry` distinguish application launch from an invisible user interface.
