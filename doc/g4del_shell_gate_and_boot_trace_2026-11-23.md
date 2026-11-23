# G4DEL Shell gate and OS boot trace — 2026-11-23

## Capture evidence

Reviewed the SOL captures and VGA screenshot in
`/home/uefi/workspace/logs/1405-09-02/`:

- `SOLHostCapture_Archive_1_11222026_20_34_02.log` records successful FSP-M
  memory initialization and FSP-S silicon initialization, then coreboot's
  EDK2 payload load and handoff.
- `SOLHostCapture_Archive_3_11222026_20_37_46.log` continues through PEI,
  reports permanent memory established with no pending PEIM dispatch, and
  reaches the DXE IPL-to-DXE entry checkpoint. This text artifact ends during
  VGA row logging before the HOB diagnostic records; it does not contain a
  Shell launch or OS handoff.
- The screenshot shows the preboot dashboard. PHIT free-memory bounds pass;
  HOB list fails as `invalid/truncated list`; the other displayed checks pass.
  The last visible prompt says `REQUIRED CHECKS FAILED. Press B once to
  override or R to rerun.` No key press is shown.

The failing HOB result is not by itself evidence that the HOB list caused the
Shell not to execute. In the current code, `GxRunPrebootDiagnostics()` runs
before `SyncFvBootOption()` registers the Shell option. On a required failure,
the diagnostic routine waits indefinitely for `B` or `R`. That wait is the
direct, source-confirmed reason this capture never reaches Shell registration
or normal Boot Manager processing. It was introduced with the preboot
dashboard; the previously working `gooxie-g4del` path did not insert this
blocking checkpoint before Shell registration.

## Change made

The dashboard still reports each PASS/WARN/FAIL and logs the HOB failure, but
now continues automatically to Shell registration and BDS regardless of the
diagnostic result. Missing diagnostic protocol or a failed diagnostic run
also logs and continues. The HOB validator and its result are unchanged.

The existing `Boot paths` PASS only means `BootOrder` can be read. It does not
prove that a listed EFI file exists or that it can load. Serial tracing now
prints each current `BootOrder` entry. On an actual boot attempt, EDK2 logs the
Boot#### number, description, attributes, device path, each expanded device
path candidate, file-read result/size/authentication status, and invalid PE
header rejection; the boot option's `LoadImage` result; and its `StartImage`
result. Detailed `LoadImage` tracing is enabled automatically for Boot Manager
requests and records image path resolution, image authentication, PE/COFF load
result, and final status. The OS image entry point, parent, path,
load options, return status, and ExitBootServices path/status remain traced.
These detailed image records go to serial so VGA remains focused on the
dashboard.

No OS loader failure is visible in this capture because it stops before BDS
boot-option processing. The next complete SOL capture should include the
`PREBOOT BootOrder`, `[GX-BOOT]`, `LOAD`, `OS START`, and `OS EBS` records. If a
boot option fails, retain its `LoadImage` status and device path; if it starts
and returns, retain `StartImage return status` and exit data. The upstream
EDK2 boot manager may wait for a key after a `LoadImage` failure, so this
trace will reveal whether that later prompt is reached.

## References

- The UEFI Boot Manager specification describes `BootOrder`, `LoadImage()`,
  and `StartImage()` processing:
  [UEFI Specification 2.10, Boot Manager](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html).
- The TianoCore EDK2 boot manager iterates active boot options and records
  their attempt status:
  [BdsEntry.c](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Universal/BdsDxe/BdsEntry.c)
  and
  [UefiBootManagerLib.h](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Include/Library/UefiBootManagerLib.h).

## Verification status

The integrated `make -j6` build succeeded, including EDK2's
`UEFIPAYLOAD.fd` build. Extracting `fallback/payload` from
`build/coreboot.rom` confirmed the nonblocking continuation, BootOrder, GX-BOOT,
LoadImage, and OS-entry trace strings are present. ROM SHA-256:
`68ffa545e390f52d1fd38cb434da2adcf6e7174341929004fd71706c8ed0a065`.

Board capture is still required to confirm the Shell starts and the new
Boot#### failure records appear on SOL. No claim of OS boot success is made
until a complete hardware run reaches ExitBootServices or the OS.

## Follow-up capture: Shell and Linux boot attempt — 2026-11-23

Reviewed `/home/uefi/workspace/logs/1405-09-02/2/`, including
`SOLHostCapture_Archive_14_11232026_15_02_51.log`, the screenshots from
13:13:55 through 13:15:10, and the accompanying screen recording.

The VGA screenshots establish these additional stages:

1. The EDK2 Shell is running and can browse the EFI system partition. The
   operator manually starts `BOOTX64.EFI` from `FS0:\EFI\BOOT`.
2. A Kali/GRUB graphical menu appears. The boot trace later shows an image
   path beginning `memmapped(...)` and load options beginning
   `BOOT-IMAGE=>BOOT\VMLINUZ...`; this is a Linux kernel image launch, not a
   failed firmware `Boot####` lookup.
3. The trace reaches `OS EBS CONSOLE AND BOOT SERVICES POINTERS CLEARED`.
   In `CoreExitBootServices()`, this message is emitted only after
   `CoreTerminateMemoryMap(MapKey)` succeeded, the ExitBootServices event
   callbacks returned, the memory-protection callback returned, and CPU
   interrupts were disabled. Therefore the former preboot gate and an early
   `LoadImage()` failure are no longer the stopping point. The Linux EFI
   loader has entered the ExitBootServices path and firmware has accepted the
   memory-map key.

This is the furthest stage proved by the visible capture. It does **not** yet
prove that `ExitBootServices()` returned to the Linux EFI stub: after the last
visible checkpoint, this source still recalculates the system-table CRC,
zeros the boot-services table, marks runtime services active, emits the
serial-only `OS EBS CORE RETURNING status=EFI_SUCCESS`, restores the VGA
background, and returns. The screenshots do not show those later steps. The
serial text artifact is incomplete for this run: it ends at DXE checkpoint 16
and contains no Shell, GRUB, Linux image, or ExitBootServices records. It
cannot be used to decide whether the final firmware cleanup returned or what
Linux did afterward.

### Finding and remaining boundary

The current evidence narrows the fault to the final ExitBootServices cleanup
or the Linux EFI-stub/kernel handoff immediately after it. It does not identify
which side fails. If `OS EBS CORE RETURNING` is captured, firmware has returned
success from ExitBootServices and the remaining problem is in the kernel
handoff/early Linux path (or its console), not the earlier firmware boot
manager. If it is absent from a complete SOL capture, inspect the last direct
EBS checkpoint to isolate the cleanup operation that stalls. A static Kali
GRUB splash alone is insufficient to distinguish a kernel stall from a
display-mode/console transition.

Next capture should preserve uninterrupted SOL output through the final EBS
records and the first Linux boot messages. In particular retain
`OS EBS MAP KEY ACCEPTED`, each post-map cleanup checkpoint, and
`OS EBS CORE RETURNING`. Also record the selected GRUB entry's complete kernel
command line and whether serial console output is configured for Linux; the
captured VGA trace truncates the `VMLINUZ` options. The UEFI specification
defines successful ExitBootServices as termination of boot services and
transfer of continued system operation to the OS loader; Linux's EFI stub
then continues the Linux boot path. See [UEFI 2.10 Boot Services](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html)
and [Linux EFI stub documentation](https://www.kernel.org/doc/html/latest/admin-guide/efi-stub.html).

The next firmware image adds paired `[GX-EBS]` checkpoints to direct UART and,
when its preallocated trace backup is available, the pinned VGA log row. The
checkpoints bracket the EBS callbacks, memory-map validation, interrupt and
system-table cleanup, runtime-state transition, VGA restoration, and final
return. The last checkpoint will show which operation failed to return. A
successful `OS EBS core returning status=EFI_SUCCESS` shifts investigation to
Linux's EFI-stub/kernel handoff and its configured serial or graphical
console; a missing return marker alone is not proof until the complete SOL
stream has been captured.

### Additional image-entry visibility

The OS image `CoreStartImage()` begin record, entry-point begin, and
entry-point return are now sent to both serial and the pinned VGA trace row.
This lets the screen show whether DXE called the image at all, and whether
that call returned. Before entry, serial reports explicit rejection reasons
for a previously started image, a prior load failure, unsupported machine
type, emulator-registration failure, or jump-buffer allocation failure.
The existing `GX-BOOT` records still identify the Boot#### option, resolved
path, `LoadImage()` result, and `StartImage()` return status. These are
observation-only changes: image-selection, loading, and return behavior are
unchanged.

Interpret the combined trace as follows: no `OS START ENTRYPOINT begin`
means DXE rejected or failed to start the application (use its preceding
`OS START REJECTED` / `GX-BOOT` record); entry-point begin with no return
means execution passed into that application and did not return to DXE; an
entry-point return records the EFI status. For Linux, pair the entry marker
with the `[GX-EBS]` checkpoints to determine whether the EFI stub reached
ExitBootServices. VGA may retain only the latest event in each pinned row, so
the uninterrupted serial capture remains necessary for full detail.
