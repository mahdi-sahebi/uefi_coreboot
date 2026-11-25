# G4DEL serial and VGA capture register

## Scope and reading guide

This register indexes the serial/SOL, VGA still, and screen-recording evidence
reviewed during the G4DEL OS boot investigation. Capture folders use the
operator's Solar Hijri date labels; filenames usually contain Gregorian dates
and local capture times. A filename or nearby filesystem timestamp does not
prove that files came from the same reset. Treat each folder as an evidence
bundle only where its serial and VGA progression actually overlaps.

Detailed findings are maintained in the [Kali boot trace](g4del_kali_boot_trace.md),
[HOB/preboot capture note](g4del_latest_capture_2026-11-22.md),
[Shell and OS trace](g4del_shell_gate_and_boot_trace_2026-11-23.md), and
[serial-first change note](g4del_sol_capture_2026-11-23.md). This register
summarizes their observed results and evidence limits.

## Capture-by-capture observations

| Capture location and retained artifacts | Serial/SOL evidence | VGA/image evidence | Conclusion for that capture |
| --- | --- | --- | --- |
| `1405-08-25/`: `bootfile.txt`, two Kali screenshots, one screencast | No SOL text file. Fedora 45 text reports 128 CPUs, initramfs unpack, ASPEED graphics and GDM service start. | The two stills show Kali 7.0.12 loading Linux/initramfs and firmware text with `OS START ENTRYPOINT BEGIN` and the old EBS map-accepted marker. The screencast was not reviewed frame-by-frame. | Fedora progressed farther than the Kali stills prove. Neither artifact establishes a completed Kali boot or a returned EBS cleanup. |
| `1405-08-26/` and `1405-08-27/`: four SOL archives, stills and recordings | The reviewed SOL runs show successful FSP and framebuffer setup. The nested Kali kernel loads with `quiet splash`; both runs log `OS EBS CORE RETURNING status=EFI_SUCCESS` (about 133 s and 121 s). | Still images show Kali/GRUB and the firmware overlay. The images are not synchronized tightly enough to use as exact timestamps for every SOL line. | These are the strongest traces that EDK2 returned from EBS. They contain no post-EBS Linux output, so Linux progress or a full OS boot is unproven. |
| `1405-08-29/1/`: two SOL archives, three stills and a screencast; separate nested `1405-09-01/2/` bundle | The Nov 21 `Archive_4` trace reaches EBS core return at about 103 s; its earlier archive records FSP/display initialization. The nested Nov 22 bundle has a 239-line SOL file that ends around SEC, before the later HOB dashboard. | Nov 21 VGA shows Kali under a large firmware diagnostic overlay. The Nov 22 nested stills show DXE/BDS then a preboot dashboard with PHIT free-memory PASS and HOB-list FAIL (`invalid/truncated list`); no `B` override or Shell is shown. | The Nov 21 OS result is not tied to the nested Nov 22 HOB screen. The Nov 22 immediate stop is the diagnostic gate waiting for operator input; the HOB failure's validity is unresolved. |
| `1405-08-29/2/`: two SOL archives, two stills and a screencast | The longer SOL archive reaches DXE driver discovery but ends before Shell, OS launch, or EBS. | The recording shows an interactive Shell prompt. | Shell availability is established by VGA, but this set does not prove an OS launch or explain a later OS failure. The SOL text is incomplete relative to the screen recording. |
| `1405-08-29/3/`: two SOL archives, three stills and recording | The SOL traces cover FSP/payload and early EDK2 activity, but do not continue through a visible `OS EBS CORE RETURNING` marker for the displayed late OS-entry state. | The stills/recording show Kali GRUB and then a black diagnostic overlay over the Kali background with OS-entry/EBS map-key text. No Linux kernel output, panic, login, or desktop is visible. | The firmware entered the EBS path, but the artifacts do not locate the stall between final EBS cleanup and early Linux. The ROM flashed for this run is unidentified. |
| `1405-09-02/1/`: two SOL archives and one still | Archive 1 records successful FSP-M/FSP-S and stops during coreboot payload loading. Archive 3 reaches DXE IPL to DXE entry, then emits repeated VGA-row text; it ends around DXE checkpoint 16 before HOB/BDS/OS markers. | `Screenshot from 2026-11-22 16-07-34.png.jpeg` shows the preboot dashboard: PHIT free-memory PASS, HOB-list FAIL, other checks passing, and `Press B once to override or R to rerun`; no keypress is shown. | This image documents the blocking preboot gate, not a demonstrated Shell or OS-loader failure. The row echo obscures the SOL chronology. |
| `1405-09-02/2/`: one SOL archive, eight stills and recording | The 846-line SOL file reaches DXE entry/checkpoint 16 and repeated VGA-row output, but contains no Shell, GRUB, Linux image, or EBS records. | Screens/recording show an interactive Shell, a manual `FS0:\EFI\BOOT\BOOTX64.EFI` launch, Kali/GRUB, and the EBS map-key-accepted display. | VGA proves the Shell and Linux EFI-stub/kernel launch path progressed farther than SOL captures. The last visible firmware checkpoint is map-key acceptance; EBS return and Linux kernel progress are not shown. |
| `1405-09-02/3/`: two SOL archives, 131 stills and recording | `Archive_1_11232026_20_19_47.log` is byte-identical to the earlier `/1/` `Archive_1_11222026_20_34_02.log` (SHA-256 `ef08ad5154b917d490ab519a0a2e4990767becfcf26c060094343bb1c6ead12a`). The `/3/` archive 3 is 537 lines and ends around DXE checkpoint 16 amid repeated `[GX-VGA] row=...` messages. It contains no HOB validation details, BDS/Shell, OS entry, or EBS return. | The images progress through BDS USB inventory, the preboot dashboard (HOB-list FAIL), the interactive UEFI Shell and a clipped `BOOTX64.EFI` image-load attempt. The final still does not show a completed `LoadImage`, GRUB, Linux kernel output, or OS boot. | The SOL files do not cover the later VGA events. Archive 1 is a duplicate, not an independent run. The HOB FAIL did not prevent Shell from appearing in the VGA sequence. No OS failure cause is established. |
| `1405-09-02/4/`: `SOLHostCapture_Archive_11_11242026_13_44_31.log` and `Screencast from 24-11-26 09_09_14.m4v` | SOL records interactive Shell commands, successful filesystem resolution and PE loading of `FS0:\EFI\BOOT\BOOTX64.EFI`, then its nested 14,070,272-byte Kali EFI-stub/kernel image (`machine=34404`, x86-64). Load options identify Kali `7.0.12+kali-amd64`, root UUID `dbc71360-463d-4850-b9d8-b1cce9ebe2b4`, `ro quiet splash`. All logged EBS cleanup steps return and `OS EBS CORE RETURNING status=EFI_SUCCESS` is present. The capture starts with two XHCI USB control-transfer `STALL_ERROR` messages while already at Shell. | The recording shows the remote KVM initially at `No Signal`, then Shell storage mappings and the Shell prompt. It later shows the Kali `Loading Linux 7.0.12+kali-amd64` splash with firmware trace text overlaid. That visible screen does not change through the final roughly two minutes of the 5:14 recording; no login, desktop, panic, or Linux text console is shown. | Strongest matched Shell-to-EBS-return evidence so far: firmware image loading and final EBS cleanup are not the stopping point in this attempt. The apparent stall is after successful EBS return, in Linux EFI-stub/kernel startup or an unobserved display/console transition. `quiet splash` and absent Linux serial-console options hide the decisive Linux progress. Cause remains unresolved. |
| `1405-09-02/3/1405-09-04/2/`: `SOLHostCapture_Archive_9_11252026_18_25_44.log`, 5:31 screencast, and one 717x32 still | The 691-line SOL reports HOB validation failure (`end pointer before HOB list`), then continues through Shell, successful `BOOTX64.EFI` and nested 14,512,128-byte Kali image loading (`machine=34404`, x86-64). The kernel options identify Kali `7.0.12+kali-amd64`, the same root UUID, `ro quiet splash`. Two XHCI control-transfer `STALL_ERROR` reports occur at Shell. EBS map key `0x576B` is accepted; all logged cleanup operations return; final marker is `OS EBS core returning status=EFI_SUCCESS`. The ROM readback hash is absent. | The video starts with KVM `No Signal`, shows Shell and Kali startup, and ends on the Kali splash without visible Linux text, panic, login, or desktop. The narrow still is not legible enough to establish a boot state. | The HOB check did not stop this attempt, and the USB errors are not linked to the Kali stall. The trace reaches beyond DXE EBS cleanup, but `quiet splash`, no Linux serial output, and no reachability result leave Linux progress versus a display/console issue unresolved. The flashed ROM revision is unverified. |

## New serial check for EBS memory types — 2026-11-26

Research found [TianoCore issue #13451](https://github.com/tianocore/edk2/issues/13451)
and its merged [fix PR #13458](https://github.com/tianocore/edk2/pull/13458):
on some compilers, an enum signedness mismatch can let high-bit OS-reserved
memory types pass an EBS bounds check and index the memory-type statistics
array out of bounds. The local EDK2 DXE source still has the uncast comparison.
The report says behavior depends on compiler; this payload is built with
`x86_64-linux-gnu-gcc`, for which its reproduction used an unsigned check, and
the G4DEL captures do not yet show whether reserved memory types are present.
Treat this as a testable lead, not the diagnosed Kali failure.

The new diagnostic ROM emits `[GX-EBS-MAPCHECK]` before the comparison for
each descriptor whose type is at least `EfiMaxMemoryType`, including its
unsigned type and range. No behavior or gating was changed. The marker is not
present in the prior captures; the new ROM must be flashed and a matched
SOL/VGA capture made before drawing a hardware conclusion.

The clean integrated build on 2026-11-26 includes the marker in the extracted
payload. ROM SHA-256:
`42840ae85a667e5934c5450b48849ed4319e33c31e30dfac44626458212895f2`; file
creation time: 2026-11-26 00:05:02 +04. This is not a flash readback or a
hardware-tested result.

The `/4/` log and video above were copied unchanged to
`1405-09-02/3/1405-09-04/1/`. Their SHA-256 values are respectively
`394dbaf5481745f1b73a55b66a36de495f6199a847b679e3dd763022f2c8125d` and
`b5c8e48e57b5732f634fa6c77379ed99b4f1034af5a6421577cf07571d24a58f`; this
folder is not an independent boot attempt. The follow-up review added a
framebuffer safeguard: validate active GOP identity and full bounds against
the PEI HOB before framebuffer writes/restoration, restore before a non-Shell
EFI image starts, then keep DXE logging serial-only. The overlay observation
motivates this diagnostic-safety change but does not establish it as the Linux
boot failure cause. Host identity/range tests pass; `make clean` and `make -j8`
completed, producing a 64 MiB ROM with SHA-256
`62030fcba558ab75bcbf708ed2cc8a9e31201303bedd2fe899ba4b2e723b49ea` at
2026-11-25 12:12:58 +04. Hardware testing is still required, and the Kali boot
failure remains unresolved.

The new `/home/uefi/workspace/logs/1405-09-02/3/1405-09-04/2/` bundle has
SHA-256 values:

| Artifact | SHA-256 |
| --- | --- |
| `SOLHostCapture_Archive_9_11252026_18_25_44.log` | `9f16cbfedc535edb4b59210c4737be1264e8acfcd7652d9bc65190bef11e0402` |
| `Screencast from 25-11-26 13_50_04.m4v` | `64d137639c4bcb5ee40dc3949237cd5bf984560f75397deeec9160882c5b7ed6` |
| `Screenshot from 2026-11-25 13-55-50.png.jpeg` | `5ceb3f6b380877690ec4f6d932ec32d60d5b0dfd46654b2e8c859c5dff2dbf2d` |

The SOL and video are grouped in the supplied folder, but no ROM readback hash
was included to identify the firmware image tested. Treat the EBS success
marker as evidence about the firmware path only; the post-EBS Kali failure
cause remains unknown.

For the 1405-09-02/3 sequence, the key retained frames are
`Screenshot from 2026-11-23 22-15-54.png` (BDS USB inventory),
`Screenshot from 2026-11-23 22-16-19.png` (preboot checks with HOB-list
failure), `Screenshot from 2026-11-23 22-16-49.png` (interactive Shell), and
`Screenshot from 2026-11-23 22-17-32.png` (clipped `BOOTX64.EFI` image-load
diagnostic). The source files are in
`/home/uefi/workspace/logs/1405-09-02/3/`.

### SOL text files reviewed

These are the exact retained serial archive filenames covered by this register:

- `1405-08-26/`: `SOLHostCapture_Archive_1_11172026_14_22_17.log`,
  `SOLHostCapture_Archive_3_11172026_14_30_32.log`.
- `1405-08-27/`: `SOLHostCapture_Archive_1_11172026_19_54_13.log`,
  `SOLHostCapture_Archive_2_11172026_20_01_32.log`.
- `1405-08-29/1/`:
  `SOLHostCapture_Archive_1_11212026_13_56_09.log`,
  `SOLHostCapture_Archive_4_11212026_14_03_07.log`.
- `1405-08-29/1/1405-09-01/2/`:
  `SOLHostCapture_Archive_1_11222026_18_07_59.log`.
- `1405-08-29/2/`:
  `SOLHostCapture_Archive_1_11212026_16_12_06.log`,
  `SOLHostCapture_Archive_3_11212026_16_19_25.log`.
- `1405-08-29/3/`:
  `SOLHostCapture_Archive_1_11212026_20_26_27.log`,
  `SOLHostCapture_Archive_5_11212026_20_32_53.log`.
- `1405-09-02/1/`:
  `SOLHostCapture_Archive_1_11222026_20_34_02.log`,
  `SOLHostCapture_Archive_3_11222026_20_37_46.log`.
- `1405-09-02/2/`:
  `SOLHostCapture_Archive_14_11232026_15_02_51.log`.
- `1405-09-02/3/`:
  `SOLHostCapture_Archive_1_11232026_20_19_47.log`,
  `SOLHostCapture_Archive_3_11232026_20_24_40.log`.
- `1405-09-02/4/`:
  `SOLHostCapture_Archive_11_11242026_13_44_31.log`.
- `1405-09-02/3/1405-09-04/2/`:
  `SOLHostCapture_Archive_9_11252026_18_25_44.log`.

The `1405-08-25/` bundle has `bootfile.txt` rather than an SOL archive. Its
screen recording is present but was not reviewed frame-by-frame. A separate
single still in `1405-08-29/1/1405-09-01/1/` is outside the HOB dashboard
sequence summarized above. The `/1405-09-02/4/` recording was decoded at
five-second intervals and reviewed at the Shell, Linux splash, and end states;
the capture filename starts at 09:09:14 on 2026-11-24. VGA filenames and exact
observed states for the central OS handoff sets are listed in the linked
detailed reports.

## Overall conclusion

The evidence shows that the apparent stop varies across attempts: some traces
stop at the inserted preboot gate, other screens reach an interactive Shell,
and earlier separate SOL runs show successful EBS return. For the earlier
`1405-09-02/3/` screen sequence, the Shell is present and an apparent
`BOOTX64.EFI` attempt is visible, but its SOL artifacts are older/incomplete
and the last still is clipped. The subsequent `/4/` capture shows that a
separate attempt completed image loading and EBS cleanup, then remained at the
Kali splash without visible progress. Neither set yet shows Linux console
output or a complete OS boot.

The custom-firmware OS failure cause therefore remains **unresolved**. The
`1405-09-02/4/` serial/video session shows the Shell-launched Kali image
loading, EBS map-key acceptance, all logged firmware cleanup returning, and
`OS EBS CORE RETURNING status=EFI_SUCCESS`. The video then remains on the Kali
`Loading Linux` splash without visible progress for about two minutes, and
firmware trace text remains visible despite the VGA-restore routine's returned
marker. This moves the control-flow boundary beyond firmware EBS cleanup, but
does not prove the framebuffer contents were clean. The kernel was launched
with `quiet splash` and no Linux serial-console options; a static splash cannot
distinguish a kernel stall from hidden boot progress or a graphics/console
problem. The user's report that the original BIOS boots
the same OS is evidence for a custom-firmware compatibility or handoff
difference, but without matched original/custom ROM hashes it does not
identify the differing handoff data. The serial-first firmware change removes VGA row
redraw spam while retaining VGA output and structured serial markers. Its
successful build and payload-string check establish packaging only; it has
not been validated on the board.

The newer `/1405-09-02/3/1405-09-04/2/` bundle again records successful EBS
cleanup (`map_key=0x576B`, `OS EBS core returning status=EFI_SUCCESS`) and a
Kali splash still visible at the 5:31 video end. Its HOB-list failure is
followed by continued Shell and Kali execution, so it is not shown to cause
this stall. The capture lacks Linux serial output, network reachability
results, and a flashed-ROM readback hash. It therefore does not yet justify a
firmware behavior change or identify a Linux fault. Use the documented
one-time verbose GRUB parameters, capture raw SOL/VGA together, test
reachability if the splash freezes, and record the boot entry and ROM hash
before selecting a targeted fix.

For the next test, capture unfiltered SOL from reset until the OS progresses
or fails, record VGA continuously during that same reset, and identify the
flashed ROM by full-chip readback SHA-256. Preserve `GX-BDS`, `GX-BOOT`,
`OS START ENTRYPOINT begin`, all `GX-EBS` records, and `OS EBS CORE RETURNING`,
then compare the last marker to the first Linux EFI-stub/kernel output.
