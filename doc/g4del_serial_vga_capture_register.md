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

The `1405-08-25/` bundle has `bootfile.txt` rather than an SOL archive. Its
screen recording is present but was not reviewed frame-by-frame. A separate
single still in `1405-08-29/1/1405-09-01/1/` is outside the HOB dashboard
sequence summarized above. VGA filenames and exact observed time ranges for
the central OS handoff sets are listed in the linked detailed reports.

## Overall conclusion

The evidence shows that the apparent stop varies across attempts: some traces
stop at the inserted preboot gate, other screens reach an interactive Shell,
and earlier separate SOL runs show successful EBS return. For the latest
`1405-09-02/3/` screen sequence, the Shell is present and an apparent
`BOOTX64.EFI` attempt is visible, but the serial artifacts are older/incomplete and the last
still is clipped. We cannot conclude whether that attempt failed in
`LoadImage()`, `StartImage()`, final EBS cleanup, or early Linux startup.

The custom-firmware OS failure cause therefore remains **unresolved**. The
user's report that the original BIOS boots the same OS is evidence for a
custom-firmware compatibility or handoff difference, but without matched
original/custom ROM hashes and same-boot SOL/VGA captures it does not identify
the differing handoff data. The serial-first firmware change removes VGA row
redraw spam while retaining VGA output and structured serial markers. Its
successful build and payload-string check establish packaging only; it has
not been validated on the board.

For the next test, capture unfiltered SOL from reset until the OS progresses
or fails, record VGA continuously during that same reset, and identify the
flashed ROM by full-chip readback SHA-256. Preserve `GX-BDS`, `GX-BOOT`,
`OS START ENTRYPOINT begin`, all `GX-EBS` records, and `OS EBS CORE RETURNING`,
then compare the last marker to the first Linux EFI-stub/kernel output.
