# G4DEL Kali boot trace record

**Updated:** 2026-11-23
**Purpose:** Preserve the investigation history and provide a repeatable next
boot capture for the Kali startup failure reported on the Gooxi G4DEL server.

For the newer 1405-09-02 serial/VGA bundles and a cross-run evidence table,
see the [capture register](g4del_serial_vga_capture_register.md) and
[Shell/OS trace](g4del_shell_gate_and_boot_trace_2026-11-23.md). This report's
older capture findings remain tied to their named runs.

## Scope and current conclusion

**Latest update, 2026-11-24:** The matched SOL and screen recording in
`/home/uefi/workspace/logs/1405-09-02/4/` show `OS EBS CORE RETURNING
status=EFI_SUCCESS`, then a Kali `Loading Linux` splash that does not visibly
advance during the remainder of the video. The current evidence therefore
places this attempt past EDK2's ExitBootServices cleanup. Linux was started
with `quiet splash` and no Linux serial-console parameters, so the exact
post-EBS failure stage remains unknown. The video retains firmware text over
the Kali splash even after the serial VGA-restore routine reports return; this
shows the routine returned but does not prove the restored framebuffer was
clean. See the dated analysis in the
[Shell/OS trace](g4del_shell_gate_and_boot_trace_2026-11-23.md).

The 2026-11-21 test in `/home/uefi/workspace/logs/1405-08-29/3/` reaches the
Kali GRUB menu and displays `OS EBS MAP KEY ACCEPTED` and
`OS START ENTRYPOINT BEGIN`, but the overlay remains on VGA through the end
of the 3:31 screencast. Unlike the 1405-08-26 and 1405-08-27 serial captures,
which recorded `OS EBS CORE RETURNING status=EFI_SUCCESS`, this new set does
not contain the late SOL trace needed to determine whether EDK2 returned from
EBS cleanup. The OS is not shown booting fully. The tested ROM's full-chip
readback hash is not included, so the capture cannot be tied to a specific
build.

The user reports that the original BIOS boots the OS completely on this
server. Assuming the same OS installation and hardware, this A/B result
strongly points to the custom firmware or its OS handoff. It does not by
itself identify whether the cause is EDK2 EBS cleanup, ACPI/memory-map data,
GOP/framebuffer handoff, or a different boot entry/path.

The older 1405-08-26 and 1405-08-27 Kali traces still establish EDK2 return
through EBS for those separate attempts, but neither includes post-EBS Linux
output. The Fedora Live capture advances into Linux startup and GDM, but does
not establish the behavior of the Kali kernel or identify a matching ROM.

The accompanying Fedora Live log advances further: Linux 7.2.11 uses EDK II's
EFI environment, recognizes the ACPI/SPCR data, starts 128 processors, unpacks
initramfs, initializes simple framebuffer and ASPEED graphics, and starts
GDM. This shows that the captured firmware can boot Linux through those
stages. It does not diagnose why Kali 7.0.12 behaves differently, and the
Fedora log has no matching ROM readback hash. The Kali issue remains
unresolved: the 2026-11-24 single-session capture now reaches EBS return and a
static Kali splash, but lacks verbose Linux output and does not show a
completed boot.

This work corrects an ambiguous EBS checkpoint and records the evidence. It
does not change boot policy or permanently alter Kali's GRUB configuration.
The evidence does not yet justify claiming an OS startup fix.

## Source history

The current coreboot worktree is on `fix/os-startup`, based on the G4DEL
`gooxie-g4del` line. Its history includes the Dasharo-era baseline
`7c16974602ba86b18ba4bf767b2b37b64b5d95bf` and the following OS-startup work:

| Repository | Commit | Change |
| --- | --- | --- |
| coreboot | `a56336942815b90cdddc055c235f71ad44f411a1` | Add G4DEL OS startup diagnostics and integrate the payload |
| coreboot | `824cb4a4ffe4a17f881452f75ff8c1db8e68f0af` | Document Shell boot diagnostics |
| EDK2 | `2ebcc016acbcb016a590c66782e73726d054d826` | Add OS startup handoff diagnostics |
| EDK2 | `1503285e20cca53c011c0de0991521f20473fc87` | Trace Shell-launched EFI application boots and image loading |
| EDK2 | `f285869ec015115655e1fb6746bebdfa2a6ff85b` | Log readable EFI image path at OS handoff |
| EDK2 | `e606437817b263d7e2a64374f6f7e63ef83f14cd` | Separate map-key acceptance from the final EBS cleanup checkpoint |
| coreboot | `5e52d8426503d17fb30e55bae27c3f6bb238d90c` | Skip absent per-domain VT-d devices during ACPI generation; guard primary domain0 DRHD |

The path change retains the converted path from `OS START` for use in
`ExitBootServices`; it does not allocate during the EBS callback, which could
change the memory map key. Each EBS attempt logs image metadata, readable path,
load options, map key, and result. `OS EBS MAP KEY ACCEPTED` identifies a
successful map-key check while cleanup is still in progress.
`OS EBS CORE RETURNING` is emitted after core cleanup immediately before
returning success. Serial carries full escaped text. VGA pins/overwrites short
summaries, so serial sequence numbers are the authoritative event ordering.

## Supplied evidence

The input bundle is `/home/uefi/workspace/logs/1405-08-25/`:

- `bootfile.txt` records Fedora 45 Live, kernel
  `7.2.11-300.fc45.x86_64`. It reports EDK II, SPCR UART I/O `0x2f8`, 128
  activated CPUs, about 64 GiB available memory, initramfs unpack, `simpledrm`,
  ASPEED `ast` at `0000:02:00.0`, and `Started gdm`. This is strong evidence
  of boot progress, but does not prove an interactive desktop or identify the
  exact ROM revision because no full-chip readback hash accompanies it. It is
  not the Kali trace.
- `Screenshot_20261115_171350_Bale.jpg` and
  `Screenshot_۲۰۲۶۱۱۱۵-۱۹۲۶۱۹_Bale.jpg` show Kali
  `7.0.12+kali-amd64`, “Loading Linux” and “Loading initial ramdisk”, followed
  by the overlay's `OS START ENTRYPOINT BEGIN`, old `OS EBS MAP ACCEPTED`, and
  pointer-only `OS START FILEPATH_REF` messages. It shows that the EFI image
  was entered and the map key was accepted, but not that EDK2 returned from
  EBS cleanup or that Linux continued afterward.
- `Screencast from 15-11-26 10_46_15.m4v` is present but has not been reviewed
  as decoded frames in this report.

The Fedora capture narrows the investigation: it demonstrates that the
recorded firmware can hand off to Linux, bring up 128 CPUs and ACPI, unpack an
initramfs, and initialize the display stack through GDM service startup. It
cannot prove those subsystems are correct for Kali's older kernel or different
loader path. Fedora warnings are not evidence of the Kali cause. Keep the
historical reports in `doc/` separate from these files.

## Integrated build record

The configured G4DEL ROM and EDK2 payload were rebuilt using the tracked
`origin/fix/os-startup` EDK2 revision, now
`e606437817b263d7e2a64374f6f7e63ef83f14cd`:

```sh
make -j4
```

The build completed with `Built gooxi/g4del (G4DEL)`. The binary contains
strings for `OS START PATH`, `OS START OPTIONS`, `OS EBS PATH`,
`OS EBS OPTIONS`, the map-key acceptance message, and the final
`OS EBS CORE RETURNING` message.

| Artifact | SHA-256 |
| --- | --- |
| `build/UEFIPAYLOAD.fd` | `18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0` |
| `build/coreboot.rom` | `beffcba6b235413b2ba80d39f841887dd89635ffb71d90e604c43ee85853bf2f` |

The payload contains both `OS EBS MAP KEY ACCEPTED ... cleanup=IN_PROGRESS`
and `OS EBS CORE RETURNING status=EFI_SUCCESS`. The coreboot ramstage ELF
contains the `ACPI DOMAIN MAP` diagnostic string. This confirms compilation
and inclusion in the artifacts; it does not confirm that the board emitted
these messages or that Kali boots successfully.

These are local build hashes only. The current server's flashed image is
unknown until the chip is read back and hashed. No physical boot has yet been
performed with this artifact.

## One-time Kali GRUB edit

At the GRUB menu, select the Kali entry and press `e`. On the `linux` line,
remove `quiet` and `splash`, then append this exact parameter string:

```text
earlycon=uart8250,io,0x2f8,115200 console=tty0 console=ttyS1,115200n8 ignore_loglevel loglevel=8 initcall_debug efi=debug
```

Use `Ctrl-X` or `F10` to boot the edited entry once. Do not save the edit to
Kali's installed GRUB configuration. `console=tty0` keeps kernel messages on
VGA; `console=ttyS1,115200n8` selects the typical Linux COM2 device. The
firmware configuration uses COM2 at I/O `0x2f8`; if serial output does not
appear, verify the UART mapping from the trace. The options request early UART
output and verbose EFI, kernel, and initcall diagnostics. See the [Linux 7.2
kernel parameter reference](https://docs.kernel.org/7.2/admin-guide/kernel-parameters.html).

## Next-boot capture checklist

1. Start a raw SOL capture before reset at 115200 baud, 8-N-1. Do not filter
   or reformat it. Record VGA as continuous video or sequential photos.
2. Boot the installed Kali loader and use the one-time GRUB parameters above.
   Capture from reset through the stall, reboot, or successful desktop.
3. Record the selected disk/EFI loader, exact date and time, the visible last
   message, keyboard/LED response, and whether ping or SSH works after a
   display freeze. Note the BMC power/POST state.
4. Record the source commit IDs, build artifact hashes, and a full-chip
   readback hash of the flashed ROM. Keep the readback hash with the SOL and
   VGA files so the capture is tied to the tested firmware.
5. Preserve the original capture files. Review the readable EBS image path,
   options, each EBS status and retry, then continue into Linux EFI-stub and
   early-kernel output. Firmware cannot produce messages after successful
   control transfer.

If EBS succeeds but Linux output stops, the exact last kernel line can narrow
the next test: console routing, EFI-stub exit, decompressor/kernel entry,
CPU/APIC startup, ACPI discovery, drivers, or graphics mode transition. Only
make a temporary one-variable-at-a-time boot test after the trace identifies a
useful subsystem. Keep all firmware boot policy unchanged until then.

## Status

- EDK2 `e606437817b263d7e2a64374f6f7e63ef83f14cd` added the final EBS cleanup
  checkpoint and was superseded on `origin/fix/os-startup` by the nested-load
  tracing commit `5aa16e0dfc7e2c86ca8c575b7ce93ca667d4641a`. Coreboot
  `5e52d8426503d17fb30e55bae27c3f6bb238d90c` is pushed to the same branch; it
  removes missing VT-d assertions and guards primary DRHD generation.
- The integrated `make -j6` build succeeds and contains the EBS checkpoints,
  ACPI mapping string, and missing-domain diagnostics. The exact artifact
  hashes and source revision are recorded above.
- A physical flash/test was reported on 2026-11-21 and the associated captures
  are analyzed below. The flashed image is not identified by a readback hash.
- The exact stopping point, root cause, and required behavioral fix remain
  unresolved; the new screen capture stops at the EBS map-key marker and the
  SOL logs end before that event.

## Follow-up capture and ACPI hardening — 2026-11-17

The reviewed workspace-only capture sets are in
`/home/uefi/workspace/logs/1405-08-26/` and
`/home/uefi/workspace/logs/1405-08-27/`. Serial logs and still images were
reviewed; videos were excluded. The 1405-08-26 ACPI log contains at least
eight complete assertions at `uncore_acpi.c:542` and is truncated mid-line.
The 1405-08-27 ACPI log contains eight assertions at `:542` and three at
`:390`; neither log contains an assertion at `:448`. In the captured source,
these are the DMAR and ATSR per-domain VT-d lookups; the RHSA lookup is at
`:448`. Existing null checks skipped absent devices after printing the
assertions. This indicates noisy error handling, not that VT-d caused the
Kali symptom. The ACPI logs end at/truncate around their diagnostic output;
their relationship to the separate OS launch traces is not established.

Other boot diagnostics show FSP calls returning success, AST framebuffer
initialization succeeding with 1024x768 fallback after EDID failure, and
KBC/IPMI/timestamp warnings. The logs also report a 128 MiB flash probe size
against the configured 64 MiB ROM and JEDEC `00:0000`. These messages are not
shown to stop firmware; verify flash identity and probe reporting if they
recur. The still images show the Kali GRUB menu, Linux/initial-ramdisk loading,
and EBS overlay. They are not tied unambiguously to a specific serial file.

`SOLHostCapture_Archive_3_11172026_14_30_32.log` in the 1405-08-26 folder
records Shell loading `FS0:\EFI\BOOT\BOOTX64.EFI`, a 155,648-byte x86-64
image, from a PCI/SCSI path whose GPT partition GUID is
`D6F197AA-FA83-4187-8F37-5A199919526A`. Its nested 14,512,128-byte Kali kernel
has a `MemoryMapped(...)` device path and options containing the root UUID and
`quiet splash`. `SOLHostCapture_Archive_2_11172026_20_01_32.log` in the
1405-08-27 folder records 76 DXE drivers loaded/executed and the same nested
Kali image/options. Both traces show `OS EBS CORE RETURNING status=EFI_SUCCESS`
with no Linux EFI-stub or kernel output afterward. The 1405-08-26 trace also
captures the outer EFI file's disk/partition path; the nested image path
does not identify its source disk. No matching full-chip ROM readback hash
accompanies these traces.

The coreboot fix removes assertions for absent per-domain VT-d devices,
reports their domain paths, and guards the primary domain0 VT-d pointer
before DRHD generation. It leaves valid discovered secondary DRHD entries in
place and does not change VT-d enablement. `make -j6` completed on
2026-11-17 after coreboot commit
`5e52d8426503d17fb30e55bae27c3f6bb238d90c` was pushed to
`origin/fix/os-startup`; the worktree is clean. The ramstage contains all new
diagnostics. The ROM SHA-256 is
`f53971e353dc11143c53dc380b876e7ab53b302c02bc92245b826a303d5c5239`, and
the unchanged EDK2 payload SHA-256 is
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0`.
These are local build artifacts, not flashed-image evidence. Runtime ACPI
table contents and the Kali boot result are unverified.

The successful EBS core return appears in two separate traces: elapsed 133
seconds in the 1405-08-26 log and 121 seconds in the 1405-08-27 log. Both
contain `quiet splash` and neither includes serial console/early-console
options. This repeatability establishes that firmware reached EBS return in
these attempts, but it does not show whether Linux stalled, changed to an
unseen console, or continued without captured output. The outer
`BOOTX64.EFI` file path is identified in the 1405-08-26 Shell trace; the
nested kernel's `MemoryMapped(...)` path does not independently identify its
source disk or partition.

For the next Kali attempt, use the one-time verbose GRUB parameters above,
capture raw SOL and VGA through Linux startup, and record a full-chip readback
hash. Keep `quiet splash` removed only for that boot; do not change the
persistent Kali GRUB configuration. Review the first post-EBS kernel line
and preserve the capture with the exact firmware revisions.

### EDK2 trace expansion and upstream references — 2026-11-17

The previous traces establish that EDK2's `ExitBootServices()` service
returned `EFI_SUCCESS`; they do not show what Linux did afterward. The code
now keeps Shell-originated image-load tracing enabled while the Shell-launched
EFI application runs. This captures nested `LoadImage()` calls made by an EFI
loader, including the source device path, parent image path, source buffer and
size, resolution/file-read/authentication checkpoints, PE load result, image
metadata, and load options. The `CoreStartImage` entry checkpoint now includes
the image handle, base, size, machine type, and entry point. VGA pins the
latest nested load checkpoint and the readable parent-loader path; serial
retains the full escaped path/options and sequence/elapsed-time records.

These are observation changes only. They do not change image loading, EBS
policy, or Linux execution. A firmware message cannot be emitted after a
successful EBS handoff because boot services are no longer available. If the
new trace again ends at successful EBS, a one-boot Linux early console is
needed to locate the first kernel-side checkpoint.

Upstream references clarify this boundary and the next capture:

- The [Linux EFI stub documentation](https://docs.kernel.org/admin-guide/efi-stub.html)
  describes Linux kernels booted as EFI applications and how the EFI stub
  receives command-line options and initrd information.
- The [EDK2 OVMF runtime configuration notes](https://github.com/tianocore/edk2/blob/master/OvmfPkg/RUNTIME_CONFIG.md)
  describe OVMF's modern path, where the kernel is loaded as an EFI binary and
  the Linux stub handles initrd loading and EBS. This is an architectural
  reference only: G4DEL does not use OVMF, so the page does not prove its exact
  loader implementation.
- The [Linux EFI stub helper](https://github.com/torvalds/linux/blob/master/drivers/firmware/efi/libstub/efi-stub-helper.c)
  shows the Linux-side memory-map / EBS call and retry behavior. EDK2's
  [ExitBootServices service definition](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Uefi/UefiSpec.h)
  documents `EFI_SUCCESS` as termination of boot services and an incorrect map
  key as `EFI_INVALID_PARAMETER`.
- The [Linux kernel parameter reference](https://docs.kernel.org/admin-guide/kernel-parameters.html)
  describes `quiet`, `ignore_loglevel`, `loglevel`, and `earlycon`. The existing
  one-time GRUB parameters should therefore expose kernel messages on VGA and
  the configured COM2 UART after firmware logging ends.
- A targeted [TianoCore EDK2 issue search for ExitBootServices and Linux](https://github.com/tianocore/edk2/issues?q=is%3Aissue+ExitBootServices+Linux)
  did not reveal a report matching the supplied G4DEL evidence: successful EBS
  return followed by no captured kernel output. This is not proof that no
  related upstream issue exists; the current captures lack post-EBS Linux
  output needed to compare a kernel failure signature.
- Related reports exist outside the EDK2 tracker, but differ materially:
  [QEMU issue 1801](https://gitlab.com/qemu-project/qemu/-/work_items/1801)
  reports an ARM guest with an EDK2 firmware image stopping around Linux's EFI
  stub/EBS messages, while this server is x86-64 physical hardware and its
  firmware trace records successful EBS return. A [Linux EFI mailing-list
  report](https://lkml.rescloud.iu.edu/1908.2/00031.html) describes an early
  x86 EFI boot freeze in `parse_setup_data()`, and a [ThinkPad T14s EFI-stub
  patch discussion](https://lkml.iu.edu/2411.3/05044.html) discusses EBS
  failures. They show that Linux/firmware handoff defects can occur, but are
  not evidence of the G4DEL cause and do not justify applying their fixes here.

The next capture must use the new EDK2 payload and the one-time verbose Kali
parameters. Record the exact firmware commits and a full-chip ROM readback
hash. If Linux messages appear, use their final line and VGA state to direct
the next diagnosis. If they do not, first verify the UART/console selection
and compare VGA against the raw SOL capture; an empty serial trace alone does
not prove a kernel hang.

The implementation is committed and pushed as EDK2
[`5aa16e0dfc7e2c86ca8c575b7ce93ca667d4641a`](https://github.com/mahdi-sahebi/uefi_edk2/commit/5aa16e0dfc7e2c86ca8c575b7ce93ca667d4641a)
on `origin/fix/os-startup`. The configured G4DEL build completed with `make -j6`
after forcing the EDK2 payload dependency to rebuild. The payload contains
`SHELL IMAGE_TRACE enable nested LoadImage diagnostics`, `LOAD PARENT_PATH`,
and the expanded `OS START ENTRYPOINT` format. Artifact hashes:

| Artifact | SHA-256 |
| --- | --- |
| `build/UEFIPAYLOAD.fd` | `9dfa502448a943b78fbba8b2ee1ff725499748ba02558be0b3c7e3baaa84d455` |
| `build/coreboot.rom` | `9425434a1c4634cb41ec724986262f82e66276835e5e7c0618c0b72f1bcaeb5b` |

At the time of this 2026-11-17 build record, these hashes identified local
build outputs only; the ROM had not been flashed or read back and no matching
server capture was available. The later 2026-11-21 test is documented below,
but it still lacks a full-chip readback hash and late SOL output.

## Post-flash test capture — 2026-11-21

The user reported flashing and testing firmware. The supplied files are in
`/home/uefi/workspace/logs/1405-08-29/3/`:

- `SOLHostCapture_Archive_1_11212026_20_26_27.log`
- `SOLHostCapture_Archive_5_11212026_20_32_53.log`
- `Screenshot from 2026-11-21 19-32-05.png`
- `Screenshot from 2026-11-21 19-32-13.png`
- `Screenshot from 2026-11-21 19-32-23.png`
- `Screencast from 21-11-26 15_59_16.m4v` (3:31)

### What the captures show

The first SOL archive records successful FSP-M memory initialization and
FSP-S silicon initialization. Coreboot identifies the ASPEED `1a03:2000`
display device, initializes its linear framebuffer at `0x94000000` in
1024x768 mode, and falls back to that mode because EDID reading fails. It
continues into payload loading. The second archive starts at the coreboot to
EDK2 handoff and records EDK2 PEI activity, including GOP/framebuffer
information and VGA diagnostic output. Coreboot reports the configured
reference as `origin/fix/os-startup`; this is not an exact source revision or
proof of which image was programmed.

The 19:32:05 still shows a clean Kali GRUB menu. In the later stills, after
starting the kernel EFI entry, a large black firmware diagnostic area appears
over the Kali background. It includes `OS EBS MAP KEY ACCEPTED` with
`STATUS=EFI_SUCCESS` and `OS START ENTRYPOINT BEGIN`. The screencast remains
visually at this state through its final sampled frame at 3:19; it does not
show Linux kernel output, a panic, a login prompt, or a desktop. Therefore the
OS is not shown booting fully, and the overlay remains visible in this run.
Because the flashed ROM is unidentified, this does not establish whether the
intended screen-cleanup revision was tested.

The map-key marker is evidence that the map-key check succeeded and EBS
cleanup began. It is not the serial-only `OS EBS CORE RETURNING` marker, which
would establish that EDK2's core cleanup returned successfully. The SOL files
are not a continuous capture through the displayed OS-entry event: the first
ends during payload loading, while the second reaches only early PEI/SEC
activity and stops before the late OS handoff. As a result, the captures do
not localize the stall to an EBS cleanup callback, framebuffer restoration,
the EFI loader, or Linux itself. The persistent overlay is consistent with a
failure or stall before the display restoration is visible, but does not prove
which of those occurred.

### Firmware messages that need follow-up

- FTW reports an invalid working workspace and an invalid spare workspace.
  It nevertheless installs its completion PPI successfully and the PEIM
  returns, so this capture does not show FTW stopping boot. Variable storage
  writes and persistence were not tested.
- A PEIM reports `Error when parsing platform info, Status = Not Found` and
  returns `Not Found`; subsequent SEC migration checkpoints appear. This is a
  non-success status worth investigating, but the log does not show it
  stopping execution or causing the later OS-start screen.
- EDID read failure is followed by the expected 1024x768 fallback and
  successful framebuffer initialization.
- SOL uses `[ERROR]` prefixes for many ordinary begin/success diagnostics as
  well as actual error reports. Interpret the message text and status rather
  than treating every prefixed line as a fatal error.
- VGA text-renderer diagnostics appear as replacement/garbled characters in
  the serial files. The VGA stills are the readable evidence for what was
  drawn on screen.

### Limits and next capture

No full-chip readback hash or exact coreboot/EDK2 commit IDs were supplied
with this test, so its behavior cannot be tied to a specific built ROM. The
SOL and VGA evidence are also not continuous, synchronized views of the same
late boot interval. Do not claim that this run reached `OS EBS CORE RETURNING`
or that Linux entered its kernel based on these files.

For the next attempt, capture raw SOL from reset through the visible stall or
successful Linux startup while recording continuous VGA at the same time.
Keep the one-time verbose Kali kernel parameters described above, and record
the coreboot and EDK2 revisions plus a full-chip readback hash of the flashed
ROM. Preserve the first post-map-key and `OS EBS CORE RETURNING` lines and the
first Linux EFI-stub/kernel messages. If the final EBS marker appears, use the
first Linux line (or the absence of output alongside VGA/network/keyboard
state) to choose the next diagnosis. If the final marker is absent, instrument
the EBS cleanup path before changing Linux or ACPI behavior.

## Original BIOS comparison — 2026-11-22

The user reports that the same server boots the OS completely when the
original BIOS flash is installed, while the custom firmware test above does
not show completion. This strongly argues against a generally broken OS
installation or failed core hardware. Treat it as a user report: the
original-BIOS SOL/VGA trace, exact boot entry, kernel options, and matched ROM
hashes have not been supplied, so equivalence of the two boot paths is not
established.

The leading diagnosis is a custom-firmware compatibility/handoff defect. The
2026-11-21 custom-firmware screen reaches the map-key-accepted marker, but
matching late SOL output is missing. In the current EDK2 source,
`CoreExitBootServices()` next notifies ExitBootServices event handlers, runs
the memory-protection exit callback, disables timer/CPU interrupts, clears
the boot-services table, logs `OS EBS CORE RETURNING` to serial, and restores
the saved framebuffer. A hang in that interval could explain a persistent
overlay; the captures do not prove that this is the failing interval. The
older successful-EBS traces and the Nov 21 VGA stills are separate attempts
and must not be combined as one boot.

Next preserve a readback/hash of the working original BIOS and of the custom
ROM, then repeat a controlled test per image with the same disk, UEFI boot
entry, kernel, and one-time verbose console options. Capture uninterrupted
SOL and VGA together. If the custom trace lacks `OS EBS CORE RETURNING`, add
serial checkpoints around each EBS notification and cleanup operation and
identify the last callback reached. If it prints the marker, firmware
returned from EBS; collect early Linux output and compare the custom and
original BIOS ACPI tables, EFI memory maps, and framebuffer/GOP information.
Check network reachability or SSH while VGA appears frozen to distinguish a
graphics-only failure from a kernel/system stall. Avoid changing ACPI, VT-d,
or Linux boot policy until this comparison identifies the divergent handoff
data or checkpoint.
