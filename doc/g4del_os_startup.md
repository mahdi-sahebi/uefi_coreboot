# G4DEL OS startup diagnosis

## Current status — 2026-11-17

The older Kali screenshot reaches `OS EBS MAP ACCEPTED`. That marker was
emitted after the memory-map key was accepted but before EDK2 completed the
rest of `ExitBootServices()` cleanup. It proves that the key matched, but not
that the service returned to the Linux EFI stub. The trace now distinguishes
`OS EBS MAP KEY ACCEPTED` from `OS EBS CORE RETURNING`, the final checkpoint
immediately before EDK2 returns success to its caller. Neither marker proves
that Linux completed startup.

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
becomes unresponsive. The screenshots currently available capture an earlier
boot stage, so they are not a trace of that reported desktop freeze and do not
resolve whether both observations came from the same attempt.

The latest EDK2 trace now prints a readable image device path and available
load options at `OS START` and on every `ExitBootServices` attempt. Serial
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
