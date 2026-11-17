# G4DEL OS startup diagnosis

## Current status — 2026-11-17

Current coreboot tip is `5e52d8426503d17fb30e55bae27c3f6bb238d90c` on
`fix/os-startup`; the paired EDK2 tip is
`e606437817b263d7e2a64374f6f7e63ef83f14cd`. The coreboot change removes
assertions for absent per-domain VT-d devices and guards the primary domain0
DRHD path. `make -j6` completed after the commit was pushed. The resulting
ROM SHA-256 is
`f53971e353dc11143c53dc380b876e7ab53b302c02bc92245b826a303d5c5239`.
This is build evidence only: the ROM has not been flashed/read back and the
generated DMAR entries have not been inspected on the server.

The older Kali screenshot reaches `OS EBS MAP ACCEPTED`. That marker was
emitted after the memory-map key was accepted but before EDK2 completed the
rest of `ExitBootServices()` cleanup. Later SOL logs from 1405-08-26 and
1405-08-27 each reach `OS EBS CORE RETURNING status=EFI_SUCCESS`, at elapsed
133 and 121 seconds. This confirms EDK2's EBS core returned to its caller in
both attempts. Neither log contains subsequent Linux EFI-stub or kernel
output: both use `quiet splash` and omit serial-console options. The exact
Linux outcome after handoff is therefore unresolved.

The supplied Fedora Live log is significant: Linux 7.2.11 identifies EDK II,
consumes the firmware memory map and ACPI tables, reports 128 activated
processors and about 64 GiB available memory, unpacks its initramfs,
initializes `simpledrm` and then ASPEED `ast`, and starts `gdm`. This
demonstrates that a Linux EFI boot can pass those platform handoff stages on
the captured firmware. It narrows the reported issue to the Kali-specific boot,
kernel, device, or later graphical session path; it does not establish which
one. Kali's screenshot shows kernel `7.0.12+kali-amd64`, while Fedora's log
shows `7.2.11-300.fc45.x86_64`. The exact failure and cause remain unresolved
pending a serial/VGA trace from the same Kali boot.

The earlier user report says the installed Kali reaches the desktop and then
becomes unresponsive. The supplied 1405-08-26/27 captures end at EBS handoff,
so they do not reproduce the reported desktop freeze or establish whether
these are the same boot path and attempt.

The latest EDK2 trace now prints a readable image device path and available
load options at `OS START` and on every `ExitBootServices` attempt. The
1405-08-26 Shell trace identifies the outer `BOOTX64.EFI` path and GPT
partition; the nested kernel image has a `MemoryMapped(...)` path, which does
not independently identify its source disk. Serial
contains escaped full text in bounded chunks; VGA shows a concise wrapped
summary. Path conversion is performed before the handoff callback, so the
callback does not allocate memory and disturb the map key. The source branches
are `fix/os-startup` in both repositories. The EDK2 change is
`f285869ec015115655e1fb6746bebdfa2a6ff85b`; the preceding coreboot logging
change is `a563369428`. The updated reports are committed and pushed on
coreboot `origin/fix/os-startup`.

The earlier integrated G4DEL build completed with:

```sh
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
```

It used EDK2 revision `f285869ec015115655e1fb6746bebdfa2a6ff85b` and produced:

| Artifact | SHA-256 |
| --- | --- |
| `build/UEFIPAYLOAD.fd` | `3a87c3e9b8c0030a68d2f84ae11522b7ef14f5f9bbd3b294ae66118d07b3ff3b` |
| `build/coreboot.rom` | `bf941aa7617af7ba2ac68811a70806c6e3eff955a1ae5093fd143ae4ac01000b` |

The payload from that build contains the `OS START PATH`, `OS START OPTIONS`,
`OS EBS PATH`, `OS EBS OPTIONS`, and EBS status format strings. It predates
the new `OS EBS CORE RETURNING` checkpoint. This verifies markers in that
earlier artifact only; it does not verify a physical boot with the updated
code.
The ROM has not been flashed or read back, and its hash must not be associated
with the current server firmware until a full-chip readback is recorded.

### Follow-up diagnostic build — 2026-11-17

EDK2 `e606437817b263d7e2a64374f6f7e63ef83f14cd` now distinguishes successful
map-key validation (`OS EBS MAP KEY ACCEPTED ... cleanup=IN_PROGRESS`) from
`OS EBS CORE RETURNING status=EFI_SUCCESS`, emitted after core cleanup and
immediately before return. Coreboot also emits `ACPI DOMAIN MAP` with the
generated domain name, socket, stack, and bus. The latter is observational and
does not alter ACPI AML.

The integrated `make -j4` build completed with both markers in
`build/UEFIPAYLOAD.fd` and the ACPI mapping string in the coreboot ramstage.
Payload SHA-256:
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0`;
ROM SHA-256:
`beffcba6b235413b2ba80d39f841887dd89635ffb71d90e604c43ee85853bf2f`.
These checks establish artifact contents only. The ROM has not been flashed
or read back, so this is not a verified fix for Kali or Windows startup.

## Evidence and interpretation

The files in `/home/uefi/workspace/logs/1405-08-25/` contain separate evidence
for two different operating-system paths:

- `bootfile.txt` is a Fedora 45 Live boot using kernel
  `7.2.11-300.fc45.x86_64`. It reports `EFI v2.8 by EDK II`, SPCR UART I/O
  `0x2f8`, 128 activated processors, `Memory: 64798132K/66635184K available`,
  successful initramfs unpack, `simpledrm`, then ASPEED `ast` on
  `0000:02:00.0`, and `Started gdm`. This is strong evidence of progress
  through Linux graphics initialization and GDM service start, but it does not
  prove the Fedora desktop became interactive. Its DMI string reports
  `Dasharo (coreboot+UEFI) 11/15/2026`; no ROM readback hash ties it to a
  specific source revision.
- `Screenshot_20261115_171350_Bale.jpg` and
  `Screenshot_۲۰۲۶۱۱۱۵-۱۹۲۶۱۹_Bale.jpg` show Kali
  `7.0.12+kali-amd64` loading Linux and the initial ramdisk. The firmware
  overlay shows `OS START ENTRYPOINT BEGIN`, the old `OS EBS MAP ACCEPTED`
  marker, and a pointer-only `OS START FILEPATH_REF`. Sequence numbers should
  be used to order the overlaid rows; vertical placement is not chronological.
  These are VGA images, not raw serial output. They establish that the Linux
  EFI image was entered and that EDK2 accepted the submitted map key, but do
  not show the final EBS return checkpoint or Linux progress after handoff.
- The supplied screen recording is
  `Screencast from 15-11-26 10_46_15.m4v`. It has not been decoded into a
  frame-by-frame trace in this report.

Warnings in the Fedora log must be interpreted alongside its later progress.
It reports firmware ACPI tables and starts APs, unpacks initramfs, and brings
up both framebuffer drivers; isolated parser, EDID, or service warnings do not
identify the Kali fault. The log does not establish that Kali uses the same
kernel parameters, EFI loader, ACPI path, or display-driver sequence. Do not
change shared firmware tables, CPU limits, or FSP settings based on those
warnings. First obtain a post-EBS Kali trace and check whether the machine is
frozen or only its display/session is.

The Fedora capture also contains 24 ACPICA unresolved-name errors for
`DI28`, `HQ28`, `HR28`, `PC21`–`PC26`, `PM28`, `PN28`, `UC2D`, and `UD2D`.
They are a concrete ACPI investigation lead, not an established Kali cause:
the same Fedora boot continues through CPU initialization, initramfs, graphics,
and GDM service startup. A source-only diagnostic now prints coreboot ACPI
domain name, socket, stack, and bus mappings. It does not modify ACPI tables.
To determine whether the errors reflect missing runtime SSDT objects or benign
AML references, retain a complete `acpidump -b` from a boot tied to a full-chip
ROM readback, decompile all DSDT/SSDT tables, and match the unresolved names to
their AML scopes and the coreboot mapping lines. See [problem P14 in the
ledger](g4del/problem_ledger.md#p14--acpica-unresolved-namespace-references).

## One-boot Kali kernel diagnostics

At the Kali GRUB menu, highlight the normal boot entry and press `e`. On the
line beginning with `linux`, remove `quiet` and `splash`, then append the
following parameters. Boot this edit with `Ctrl-X` or `F10`; do not save it to
the installed GRUB configuration.

```text
earlycon=uart8250,io,0x2f8,115200 console=tty0 console=ttyS1,115200n8 ignore_loglevel loglevel=8 initcall_debug efi=debug
```

The board's configured SOL UART is COM2 at I/O `0x2f8`; Linux commonly names
COM2 `ttyS1`, which should be confirmed from the capture if output is absent.
`console=tty0` keeps kernel messages on VGA while `console=ttyS1,115200n8`
routes them to serial. `earlycon`, `efi=debug`, `ignore_loglevel`,
`loglevel=8`, and `initcall_debug` expose early EFI and kernel initialization
messages. See the [Linux 7.2 kernel parameter reference](https://docs.kernel.org/7.2/admin-guide/kernel-parameters.html).

## Captures from 2026-11-17

The supplied captures in `/home/uefi/workspace/logs/1405-08-26/` and
`/home/uefi/workspace/logs/1405-08-27/` are workspace evidence and are not
stored in this repository. The serial logs and still images were reviewed;
the video files were excluded.

### 1405-08-26 capture set

`SOLHostCapture_Archive_1_11172026_14_22_17.log` reports FSP-S status zero,
ASPEED EDID read failure with 1024x768 fallback, IPMI KCS/device-ID errors,
and KBC cleanup errors. It shows at least eight complete assertions at
`uncore_acpi.c:542`, then ends mid-assertion; its ending is truncated and
cannot establish what happened after the ACPI output. It also reports no
timestamp table and a flash probe/configuration size mismatch. The separate
`SOLHostCapture_Archive_3_11172026_14_30_32.log` records a successful Shell
launch of `FS0:\EFI\BOOT\BOOTX64.EFI`. Its source device path identifies a
PCI/SCSI path and GPT partition GUID
`D6F197AA-FA83-4187-8F37-5A199919526A`; the file size is 155,648 bytes. The
nested Kali kernel is 14,512,128 bytes and has a `MemoryMapped(...)` image
path rather than a filesystem path. Its options identify Kali 7.0.12, the
root UUID, and `ro quiet splash`. At elapsed 133 seconds EDK2 logs successful
EBS core return. No Linux EFI-stub or kernel output follows.

The still images show the Kali GRUB menu, “Loading Linux” and “Loading initial
ramdisk”, plus the firmware overlay ending at `OS EBS CORE RETURNING`. The
images corroborate the visible handoff stage but do not show post-EBS Linux
progress. Their timing is not independently tied to a particular SOL file.

`SOLHostCapture_Archive_1_11172026_19_54_13.log` records FSP-M and FSP-S
returning status zero. FSP also prints repeated unknown UPI capability-ID
messages; these do not prevent either FSP call from returning success. The
AST driver initializes the framebuffer at 1024x768 after EDID reading fails
and the KVM fallback is selected. KBC buffer cleanup reports status and data
`0xff`; two `No timestamp table found` notices also appear. These are observed
warnings/errors, but this truncated capture does not show that any of them
stopped execution. It also reports a flash-size mismatch
(`probed_size=0x8000000`, `CONFIG_ROM_SIZE=0x4000000`) alongside JEDEC
`00:0000`; record this for reconciliation with the actual flash chip and ROM
configuration, but do not infer it caused the OS issue from this log alone.

The ACPI assertion counts in the 1405-08-27 file are exactly eight at source
line 542 and three at line 390. In the source revision represented by that capture,
these sites were the DMAR and ATSR per-domain VT-d lookups, respectively. The
file contains no assertion at line 448 (the RHSA lookup). The existing null
checks skipped the absent device after logging an assertion; they therefore
identified a source-hardening defect, not evidence that the OS freeze came
from VT-d. The capture ends immediately after the assertion output, so it
does not itself establish what happened next. A separate capture in this
directory reaches Kali EBS success; do not combine the two as one boot.

`SOLHostCapture_Archive_2_11172026_20_01_32.log` records 76 DXE drivers
loaded and executed before Shell use. The launched Kali kernel is
`7.0.12+kali-amd64`; its captured options end in `ro quiet splash`, without
`console=` or `earlycon`. The firmware records successful memory-map-key
acceptance and ends with `OS EBS CORE RETURNING status=EFI_SUCCESS`. This
establishes that EDK2 returned successfully from its EBS core path. It cannot
show whether Linux EFI-stub or the kernel subsequently made progress because
the kernel output was suppressed and no serial console was requested. The
two still images show the firmware overlay but no post-EBS Linux trace. No
Windows stop screen or boot log is present in this capture set.

The nested Kali image metadata reports machine type `34404` (x86-64), size
14,512,128 bytes, and load options containing the Kali kernel path, root UUID,
`ro quiet splash`. Its EDK2 device path is `MemoryMapped(...)`, not a readable
filesystem path, so this log does not identify the disk/partition from which
the nested kernel image originated. The outer `BOOTX64.EFI` launch and the
nested kernel launch are distinct `OS START` events. The EBS map key is
accepted and EDK2's core returns success at 121 seconds; no Linux EFI-stub or
kernel line follows.

Successful EBS core return is therefore present in two separate serial
captures: elapsed 133 seconds in the 1405-08-26 log and 121 seconds in the
1405-08-27 log. Both contain `quiet splash` and neither includes serial
console/early-console options; both end without a post-EBS Linux line. This
repeatability establishes that firmware reached its EBS return checkpoint in
these attempts, but it does not show whether Linux stalled, switched to an
unseen console, or continued without captured output. The 1405-08-26 Shell
trace identifies the outer `BOOTX64.EFI` partition; the nested kernel's
`MemoryMapped(...)` path does not independently identify its source disk.

### ACPI missing-domain handling

`src/soc/intel/xeon_sp/uncore_acpi.c` no longer asserts when a PCI domain
lacks a VT-d function. ATSR, RHSA, and DMAR scans now report the missing
domain and skip its entry. DMAR generation checks the primary domain0 IOMMU
before creating its final DRHD, preventing a NULL device from reaching DRHD
generation while keeping entries already created for other detected
domains. This is a defensive ACPI-generation change; it does not change VT-d
enablement or establish that absent VT-d functions are the cause of any OS
issue. The build verifies compilation; runtime DMAR contents still need to be
checked on the server.

The configured coreboot target was built with `make -j6` on 2026-11-17 after
commit `5e52d8426503d17fb30e55bae27c3f6bb238d90c` was pushed. The build
completed as `Built gooxi/g4del (G4DEL)`. The ramstage ELF contains the
new missing-domain messages for ATSR, RHSA, and DRHD, plus the primary-domain
guard message. The resulting `build/coreboot.rom` SHA-256 is
`f53971e353dc11143c53dc380b876e7ab53b302c02bc92245b826a303d5c5239`; the
EDK2 payload SHA-256 is
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0` and was
unchanged by this coreboot-only edit. This verifies build and diagnostic
strings, not runtime ACPI entries. The ROM has not been flashed or read back.

### Next Kali boot

Use the one-time GRUB edit above: remove `quiet splash` and add the listed
`earlycon`, VGA/serial console, EFI debug, and initcall parameters. Do not
save the edit. Capture raw SOL and VGA from reset through Linux startup, and
include firmware revisions plus a full-chip flashed-ROM readback hash. The
specific finding needed is the first post-EBS Linux message and the last
checkpoint before any stall. A later capture should also confirm that VT-d
missing-domain warnings replace assertions and preserve valid DRHD entries.

## Next boot capture

1. Before reset, start a raw SOL capture at 115200 baud, 8 data bits, no
   parity, one stop bit; record from reset until the machine stalls or reaches
   a usable desktop. Also record VGA as continuous video or sequential
   screenshots.
2. Use the one-time GRUB edit above. Do not change Kali's persistent GRUB
   configuration or firmware boot policy for this capture.
3. Record the exact boot selection, board/BMC state, date/time, whether the
   keyboard still responds, and whether the machine answers ping/SSH after the
   display stops. Preserve the raw files unedited.
4. Record the firmware source commit, EDK2 source commit, build artifact
   hashes, and a full-chip readback hash of the flashed ROM. A build hash alone
   does not establish which ROM is on the board.
5. Review the final EDK2 path/options and EBS status, then the Linux EFI-stub
   and kernel messages. If EBS is accepted but Linux emits no further serial
   output, compare the console routing and early EFI-stub boundary before
   inferring a kernel hang. Firmware cannot log after successful control
   transfer.

Keep the historical reports in `doc/` as separate records. Do not treat the
successful Fedora log, the earlier Kali screenshot, or the current build as a
reproduction of the same boot attempt.
