# G4DEL OS startup: diagnosis and capture plan

## Executive assessment

The current report is that the UEFI Shell starts Kali from the installed disk,
Kali reaches the desktop and then becomes unresponsive, Ubuntu live USB reaches
its installer, and Windows live USB does not start. These are user-reported
observations. There is no current ROM hash, matching SOL transcript, VGA video,
or exact Windows error screen in this workspace, so the cause is not yet
established.

Reaching Kali's desktop makes an initramfs failure unlikely: the kernel and
userspace have progressed beyond loading the initrd. A later graphics mode
change, CPU/interrupt or ACPI handoff, storage/device driver, or userspace
failure remains possible. The Windows USB failure may be a separate boot path
or media issue. Do not change ACPI, CPU topology, IOMMU, or graphics settings
permanently until a controlled boot identifies the failing stage.

The repository contains earlier OS notes with different outcomes: a 2026-11-02
entry says Ubuntu reaches “Try Ubuntu” while a Kali capture ends after the EFI
stub reports loading the initrd. The referenced raw capture is not present in
this workspace. Those historical observations must not be treated as a trace
of the newly reported Kali desktop freeze.

## Image and platform facts

- The supplied factory image `/home/uefi/Downloads/server.bin` is 64 MiB and
  has SHA-256
  `db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`.
  This matches the factory image already inventoried in
  [`factory_image_inventory.md`](g4del/factory_image_inventory.md). It is a
  static reference; no new boot on the factory image was performed here.
- The G4DEL configuration uses COM2 at I/O `0x2f8`, 115200 baud, and a linear
  framebuffer. The dual-socket IIO routing remains provisional. `.config`
  sets `CONFIG_MAX_CPUS=255`, while the selected EDK2 build parameters set
  `PcdCpuMaxLogicalProcessorNumber=1`. This is a runtime topology question to
  measure, not proof that the CPU setting caused either OS symptom.
- The generated `build/coreboot.rom` is a local build artifact, not evidence
  of the image currently flashed. Record both the build hash and a full-chip
  readback hash before associating a capture with a source revision.
- The official Gooxi Eagle Stream server specification describes a dual-socket
  Xeon platform with DDR5, PCIe 5.0, USB, and VGA, but it is family/model
  context and does not establish the exact board revision or the server's
  populated hardware. See the [Gooxi specification](https://www.gooxi.us/uploadfiles/2024/08/20240807160116079.pdf).

## Firmware diagnostics added on `fix/os-startup`

EDK2 DXE now writes `[GX-DXE]` checkpoints to the configured serial debug
output and its existing framebuffer diagnostic overlay:

- Each line has a sequence number and elapsed milliseconds from the firmware
  clock when available. An unavailable or stale clock is labeled; logs do not
  report a false zero-time interval.
- Shell execution records the device path, full typed command and load options
  in serial-safe chunks. It brackets `LoadImage` and `StartImage` and reports
  protocol setup, image metadata, statuses, and cleanup. The full command and
  options are visible in the serial capture; VGA retains readable wrapped
  summaries.
- During a Shell `LoadImage`, DXE records firmware-volume/filesystem/load-file
  resolution, file-read begin/return and size, authentication begin/status,
  PE-load begin/return, and final image status. The trace is explicitly enabled
  only around the Shell's `LoadImage` call.
- `OS START BEGIN` reports image name, handle, parent, base, size, machine,
  entry point, and option size. `OS START ENTRYPOINT begin/returned` brackets
  applications that return to firmware. `OS EBS BEGIN/REJECTED/MAP ACCEPTED`
  reports the image and map key. The accepted marker is the firmware handoff
  boundary; firmware cannot log after control transfers to the OS loader.
- VGA wraps long entries in a bounded recent-history area and pins the latest
  Shell execution, image-load, application-start, and ExitBootServices
  milestones so they remain visible when history wraps.

The trace implementation was built into the configured G4DEL payload with:

```sh
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
```

The successful build used EDK2 revision `1503285e20cca53c011c0de0991521f20473fc87`.
The resulting `build/UEFIPAYLOAD.fd` SHA-256 is
`426d3caed9eab3a949ff95c3e499c48cfd2c6e7dec35d589eda25a7479ea3841`, and the
embedded `build/coreboot.rom` SHA-256 is
`8c4ce6a9b63cb6fa9e154f5a3bad8a80c4c2d32e8ceae7bf6d89e1d3c8210a60`. The
payload binary contains the Shell launch, DXE image-load, `StartImage`, and
`ExitBootServices` diagnostic format strings. A live server boot and serial/VGA
capture are still needed to diagnose the observed Kali freeze.

`ExitBootServices` is a useful boundary because the UEFI specification requires
the OS loader to pass the current `GetMemoryMap` key. An invalid key returns
`EFI_INVALID_PARAMETER`, and the loader must obtain a fresh map and retry.
After successful handoff the OS loader assumes responsibility for system
memory. See the [UEFI Boot Services specification](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#efi-boot-services-exitbootservices).

These changes improve observability; they do not yet establish or fix a
behavioral root cause. The EDK2 and coreboot branches must be built together
so the ROM includes the new markers.

The local EDK2 change is commit `2ebcc016ac` and the coreboot changes are
being prepared on local branch `fix/os-startup`. The configured payload
selector is `origin/fix/os-startup`. The current build was verified against
the local EDK2 branch; publishing that branch is required before a clean
checkout can fetch it through the configured `origin/` reference.

Build verification completed on 2026-11-14 with the configured G4DEL target.
The build command used the local branch reference because the new branch is
not yet available from `origin`:

```text
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
```

The command completed with `Built gooxi/g4del (G4DEL)`. The resulting local
artifacts have these SHA-256 values:

| Artifact | SHA-256 |
| --- | --- |
| `build/coreboot.rom` | `d84a4cf6ea0b1a1275daebb458f21a428ce4e2c9ae9b9b2cb0f0b4399fc41fca` |
| `build/UEFIPAYLOAD.fd` | `4bb7e70a1f8401186956265df2309a3d488fc361da93cc5c5305417a05add773` |

## Reproducible capture procedure

1. Build from `configs/config.gooxi_g4del`, record the coreboot and EDK2 commit
   IDs, `build/coreboot.rom` SHA-256, and `build/UEFIPAYLOAD.fd` SHA-256.
2. Flash only through the established recovery process. Read the entire chip
   back and record its SHA-256; do not use the build hash as a substitute for
   readback verification.
3. Start a raw SOL capture before reset at 115200 baud, 8 data bits, no parity,
   one stop bit. Capture VGA as video or sequential photos. Record BMC POST and
   power state, keyboard response, and the timestamp of the last visible
   screen. Keep both artifacts unedited and note the flashed image hash.
4. Repeat each boot path separately: installed Kali disk, Kali live USB,
   Ubuntu live USB, and Windows live USB. Use the same ports and media across
   repeats. Record the exact boot selection and whether the OS is installed,
   live, or running from the installer.
5. At the Kali freeze, check whether ping/SSH still works and whether keyboard
   lock LEDs or input respond. This distinguishes a display/session failure
   from a machine-wide lockup. Record whether the screen is still the desktop,
   black, or showing a kernel/firmware message.
6. For Windows, photograph the complete screen and record the exact stage:
   firmware menu, Windows Boot Manager, spinning logo, recovery screen, or
   black/reboot. Record any stop code and whether the USB device remains
   visible in the UEFI Shell before launch.

## OS-side serial and graphics tests

### Kali and Ubuntu

At GRUB, press `e` for a one-boot test, remove `quiet splash`, and append:

```text
earlycon=uart8250,io,0x2f8,115200 console=tty0 console=ttyS1,115200n8 ignore_loglevel loglevel=8 initcall_debug
```

The coreboot configuration routes the board's SOL UART to COM2 (`0x2f8`).
Linux documents `earlycon` for early kernel messages and `ttyS1` as the typical
COM2 console; actual numbering must be checked in the captured output or in
`/proc/tty/driver/serial`. See the [Linux kernel parameter reference](https://docs.kernel.org/admin-guide/kernel-parameters.html#earlycon) and [serial console guide](https://docs.kernel.org/admin-guide/serial-console.html).

Run separate one-boot A/B tests, changing one parameter at a time:

- Add `nomodeset` to check whether the freeze depends on the kernel graphics
  mode switch. A successful boot points toward the graphics path but does not
  by itself prove a firmware GOP defect.
- If the trace points to CPU bring-up, compare `maxcpus=1` with the normal
  processor count. If it points to power-state transitions, compare
  `intel_idle.max_cstate=1`. If it points to DMA/IOMMU, compare
  `intel_iommu=off`. These are temporary diagnostic controls, not proposed
  permanent settings.

When a live environment remains usable, save `cat /proc/cmdline`, `dmesg -T`,
`lscpu`, `lspci -nnk`, `/proc/interrupts`, and an ACPI table dump (`sudo
acpidump -o acpi.dat`). If the desktop freezes, check reachability from another
host and save the last SOL lines before resetting. Do not edit the installed
Kali GRUB configuration until a one-boot test identifies a useful parameter.

### Windows

Use the displayed Windows Boot Manager/WinPE error and VGA recording as the
first evidence. If the installer can reach a command prompt, enable its boot
log and verbose boot display in the matching BCD store, then preserve
`\Windows\ntbtlog.txt`. Windows kernel serial debugging uses the KD protocol;
it is not plain text output readable from an ordinary SOL terminal and needs a
debugger host plus compatible serial routing. Microsoft's references are
[BCDEdit `/bootdebug`](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/bcdedit--bootdebug),
[BCDEdit `/dbgsettings`](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/bcdedit--dbgsettings),
and [WinPE debugging](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/winpe-debug-apps).

Do not disable Secure Boot or change USB/ACPI policy as a first step. First
identify whether Windows Boot Manager rejects the media, the loader stops,
the kernel starts and loses video, or the system reboots.

## Reading the trace and deciding on a fix

| Last evidence | What it establishes | Next investigation |
| --- | --- | --- |
| No coreboot bootblock marker | Failure is before normal coreboot output or the capture path is wrong | Verify exact chip readback, BMC POST/power state, and UART/SOL path |
| Coreboot handoff but no EDK2 SEC | Payload load/entry boundary or serial configuration | Check CBFS payload, entry address, and SEC serial setup |
| `OS START begin` with no `OS EBS begin` | The UEFI app started but did not reach firmware ExitBootServices | Use its image name, loader output, and VGA to isolate loader/driver/image failure |
| `OS EBS rejected` | Firmware rejected that memory-map key | Preserve status and retry sequence; verify EDK2 memory-map changes during callbacks |
| `OS EBS map accepted` followed by a freeze | Loader reached the firmware-to-OS handoff | Focus on OS kernel, ACPI tables, CPU/APIC topology, framebuffer, and device handoff |
| `OS START returned` with an error | The application returned to its caller rather than transferring to the OS | Investigate the reported EFI status and selected image path |

Once a repeatable checkpoint identifies a subsystem, make the smallest source
change that addresses that evidence, rebuild, compare hashes, and rerun the
same OS matrix. Keep a known recovery image available. Do not call OS support
complete until Kali, Ubuntu, and Windows each boot to a usable desktop or
installer on the same documented firmware image.

## Current completion status

- Branches `fix/os-startup` exist in both repositories locally. The EDK2
  branch is not published to `origin`; its push failed because this
  environment has no GitHub credentials. Coreboot's selector is prepared to
  use `origin/fix/os-startup` once that branch is published.
- The EDK2 handoff diagnostics and coreboot integration built successfully.
  The generated ROM has not been flashed or tested on the server.
- The reported freeze and Windows failure have not been reproduced in this
  environment; no behavior-changing ACPI, CPU, graphics, or USB fix is claimed.
- The root cause, Windows stop point, and full cross-OS acceptance remain
  pending a board capture tied to a verified ROM readback.
