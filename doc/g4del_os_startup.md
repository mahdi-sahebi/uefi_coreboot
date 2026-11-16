# G4DEL OS startup diagnosis

## Current status — 2026-11-16

The board capture supplied so far reaches `OS EBS MAP ACCEPTED` while starting
Kali. That means EDK2 accepted the OS loader's memory map and completed its
firmware handoff path. It does **not** show whether the Linux EFI stub,
decompressor, early kernel, device initialization, or graphical session then
completed. The failure point and root cause remain unconfirmed until a new
serial/VGA capture is reviewed.

The latest EDK2 trace now prints a readable image device path and available
load options at `OS START` and on every `ExitBootServices` attempt. Serial
contains escaped full text in bounded chunks; VGA shows a concise wrapped
summary. Path conversion is performed before the handoff callback, so the
callback does not allocate memory and disturb the map key. The source branches
are `fix/os-startup` in both repositories. The EDK2 change is
`f285869ec015115655e1fb6746bebdfa2a6ff85b`; the preceding coreboot logging
change is `a563369428` (the current documentation update is committed
separately).

The integrated G4DEL build completed with:

```sh
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
```

It used EDK2 revision `f285869ec015115655e1fb6746bebdfa2a6ff85b` and produced:

| Artifact | SHA-256 |
| --- | --- |
| `build/UEFIPAYLOAD.fd` | `3a87c3e9b8c0030a68d2f84ae11522b7ef14f5f9bbd3b294ae66118d07b3ff3b` |
| `build/coreboot.rom` | `bf941aa7617af7ba2ac68811a70806c6e3eff955a1ae5093fd143ae4ac01000b` |

The payload contains the `OS START PATH`, `OS START OPTIONS`, `OS EBS PATH`,
`OS EBS OPTIONS`, and successful EBS status format strings. This verifies the
markers are present in the built artifact; it does not verify a physical boot.
The ROM has not been flashed or read back, and its hash must not be associated
with the current server firmware until a full-chip readback is recorded.

## Evidence and interpretation

The files in `/home/uefi/workspace/logs/1405-08-25/` contain separate evidence
for two different operating-system paths:

- `bootfile.txt` is a Fedora 45 Live boot using kernel
  `7.2.11-300.fc45.x86_64`. It shows initramfs/root discovery and proceeds to
  `Started gdm` and the graphical session. This is a successful Fedora live
  boot, not a Kali failure trace.
- `Screenshot_20261115_171350_Bale.jpg` and
  `Screenshot_۲۰۲۶۱۱۱۵-۱۹۲۶۱۹_Bale.jpg` show Kali
  `7.0.12+kali-amd64` loading Linux and the initial ramdisk. The firmware
  overlay reaches `OS START BEGIN` and `OS EBS MAP ACCEPTED`; the older firmware
  message prints only a pointer for the image path. The screenshot therefore
  establishes the firmware handoff boundary, not successful Linux startup.
- The supplied screen recording is
  `Screencast from 15-11-26 10_46_15.m4v`. It has not been decoded into a
  frame-by-frame trace in this report.

The Fedora log's ACPI and graphics warnings are not evidence of the Kali
failure: that Fedora session proceeds to the desktop, and the logs come from a
different boot. No specific initramfs defect is established. If the Kali
failure happens after EBS, investigate the Linux EFI stub and kernel boot,
including serial/graphics handoff, ACPI, CPU/APIC topology, and device
initialization, using the next capture before changing firmware policy.

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
