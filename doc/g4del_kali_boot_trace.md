# G4DEL Kali boot trace record

**Updated:** 2026-11-16
**Purpose:** Preserve the investigation history and provide a repeatable next
boot capture for the Kali startup failure reported on the Gooxi G4DEL server.

## Scope and current conclusion

The current evidence reaches the EDK2 `ExitBootServices` success marker while
loading Kali. That places the last confirmed firmware event at the OS loader
handoff. It does not prove the Linux EFI stub or kernel completed startup, and
it does not identify a firmware defect or Linux root cause. The next boot must
capture Linux output after the EBS boundary to locate the failure.

This work extends diagnostics and records the evidence. It does not change
boot policy, permanently alter Kali's GRUB configuration, or claim to fix an
unconfirmed OS startup bug.

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

The last EDK2 change retains the converted path from `OS START` for use in
`ExitBootServices`; it does not allocate during the EBS callback, which could
change the memory map key. Each EBS attempt logs image metadata, readable path,
load options, map key, and result. Serial receives complete escaped text in
bounded chunks. VGA receives short summaries that wrap within the diagnostic
area.

## Supplied evidence

The input bundle is `/home/uefi/workspace/logs/1405-08-25/`:

- `bootfile.txt` records Fedora 45 Live, kernel
  `7.2.11-300.fc45.x86_64`. The log reaches dracut root setup and `Started
  gdm`, so it documents a successful Fedora graphical boot. It is not the
  Kali boot trace.
- `Screenshot_20261115_171350_Bale.jpg` and
  `Screenshot_۲۰۲۶۱۱۱۵-۱۹۲۶۱۹_Bale.jpg` show Kali
  `7.0.12+kali-amd64`, “Loading Linux” and “Loading initial ramdisk”, followed
  by the firmware overlay's `OS START BEGIN` and `OS EBS MAP ACCEPTED` lines.
  The older message shows the image path as a pointer, not a readable path.
- `Screencast from 15-11-26 10_46_15.m4v` is present but has not been reviewed
  as decoded frames in this report.

The screenshot establishes that firmware accepted an OS loader memory map; it
does not establish that Linux initialized successfully. The Fedora ACPI or
graphics warnings are from a separate successful boot and are not a diagnosed
cause of the Kali behavior. The historical reports in `doc/` remain separate
records and should not be conflated with these files.

## Integrated build record

The configured G4DEL ROM and EDK2 payload were rebuilt using the local
`fix/os-startup` EDK2 revision:

```sh
make -j4 CONFIG_EDK2_TAG_OR_REV=fix/os-startup
```

The build completed with `Built gooxi/g4del (G4DEL)`. The binary contains
strings for `OS START PATH`, `OS START OPTIONS`, `OS EBS PATH`,
`OS EBS OPTIONS`, and `OS EBS MAP ACCEPTED status=EFI_SUCCESS`.

| Artifact | SHA-256 |
| --- | --- |
| `build/UEFIPAYLOAD.fd` | `3a87c3e9b8c0030a68d2f84ae11522b7ef14f5f9bbd3b294ae66118d07b3ff3b` |
| `build/coreboot.rom` | `bf941aa7617af7ba2ac68811a70806c6e3eff955a1ae5093fd143ae4ac01000b` |

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

- Instrumentation commit `f285869ec015115655e1fb6746bebdfa2a6ff85b` is pushed
  to EDK2 `origin/fix/os-startup`. The coreboot documentation update is being
  committed and pushed on `fix/os-startup`.
- The integrated build succeeds and contains the requested path/handoff
  diagnostics.
- The server has not been flashed or tested with this build.
- The exact Linux failure point, root cause, and any required behavioral fix
  remain pending the next raw SOL/VGA capture.
