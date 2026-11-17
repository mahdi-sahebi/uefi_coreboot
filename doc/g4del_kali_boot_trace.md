# G4DEL Kali boot trace record

**Updated:** 2026-11-17
**Purpose:** Preserve the investigation history and provide a repeatable next
boot capture for the Kali startup failure reported on the Gooxi G4DEL server.

## Scope and current conclusion

The older Kali screenshot reaches the EDK2 memory-map-key acceptance marker,
but that firmware version logged before the rest of `ExitBootServices()`
cleanup. The trace has been improved to distinguish this from
`OS EBS CORE RETURNING`, the final firmware checkpoint before EDK2 returns
success to its caller. The screenshot does not contain that newer marker and
does not prove Linux completed startup.

The accompanying Fedora Live log advances further: Linux 7.2.11 uses EDK II's
EFI environment, recognizes the ACPI/SPCR data, starts 128 processors, unpacks
initramfs, initializes simple framebuffer and ASPEED graphics, and starts
GDM. This shows that the captured firmware can boot Linux through those
stages. It does not diagnose why Kali 7.0.12 behaves differently, and the
Fedora log has no matching ROM readback hash. The Kali issue remains unresolved
until a single-attempt SOL and VGA capture continues after the final EBS
checkpoint.

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

- EDK2 `e606437817b263d7e2a64374f6f7e63ef83f14cd` is pushed to
  `origin/fix/os-startup`; it adds the final EBS cleanup checkpoint. Coreboot
  adds ACPI domain mapping logs on `fix/os-startup`.
- The integrated build succeeds and contains the new EBS checkpoints and the
  ACPI mapping string. The exact artifact hashes and source revision are
  recorded above.
- The server has not been flashed or tested with this build.
- The exact Linux failure point, root cause, and any required behavioral fix
  remain pending the next raw SOL/VGA capture.

## Follow-up capture and ACPI hardening — 2026-11-17

The next supplied capture set is in the workspace-only directory
`/home/uefi/workspace/logs/1405-08-27/`. The two SOL logs and two still images
were reviewed; the video was excluded. `SOLHostCapture_Archive_1_11172026_19_54_13.log`
shows FSP memory initialization succeeding, ASPEED framebuffer setup with
EDID fallback, KBC cleanup errors, and repeated assertions in Xeon-SP ACPI
generation at `uncore_acpi.c:390`, `:448`, and `:542`. The assertions occur
where ATSR, RHSA, and DMAR scans look for a per-domain VT-d function before
already skipping a missing device. They indicate noisy assertions around an
optional/missing domain function; the log does not establish that this
caused the reported Kali freeze.

`SOLHostCapture_Archive_2_11172026_20_01_32.log` shows 76 DXE drivers loaded
and executed, followed by Shell launch of Kali kernel `7.0.12+kali-amd64` with
`quiet splash` and no serial console arguments. The last firmware checkpoint
is `OS EBS CORE RETURNING status=EFI_SUCCESS`. This is evidence that EDK2
returned from the EBS core path. Because the kernel output was quiet and had
no serial console, the capture cannot locate a later Linux stall. The two
still images show the firmware overlay, not post-EBS Linux output. These
captures may be from separate attempts and are not tied to a flashed-ROM
readback hash.

The coreboot fix removes assertions for absent per-domain VT-d devices,
reports their domain paths, and guards the primary domain0 VT-d pointer
before DRHD generation. It leaves valid discovered secondary DRHD entries in
place and does not change VT-d enablement. `make -j6` completed on
2026-11-17; the ramstage contains all new diagnostics. The ROM SHA-256 is
`3a6954cd9196d31745f43dddd73b3eba4b64279eed08efbe8cd64e01a712534a`, and
the unchanged EDK2 payload SHA-256 is
`18d59e6aea928f74b13b0862342f131fca3531031cec123a0cbc2a5abe1759d0`.
These are local build artifacts, not flashed-image evidence. Runtime ACPI
table contents and the Kali boot result are unverified.

For the next Kali attempt, use the one-time verbose GRUB parameters above,
capture raw SOL and VGA through Linux startup, and record a full-chip readback
hash. Keep `quiet splash` removed only for that boot; do not change the
persistent Kali GRUB configuration. Review the first post-EBS kernel line
and preserve the capture with the exact firmware revisions.
