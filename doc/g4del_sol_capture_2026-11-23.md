# G4DEL SOL and VGA capture findings — 2026-11-23

## Evidence reviewed

The capture set in `/home/uefi/workspace/logs/1405-09-02/3/` contains two SOL
archives, 131 VGA screenshots, and a screen recording. `Archive_1` is exactly
the same file as `/home/uefi/workspace/logs/1405-09-02/1/SOLHostCapture_Archive_1_11222026_20_34_02.log`
(SHA-256 `ef08ad5154b917d490ab519a0a2e4990767becfcf26c060094343bb1c6ead12a`).
Archive 3 reaches DXE entry/checkpoint 16, then contains repeated
`[GX-VGA] row=...` messages and ends before HOB diagnostics or BDS. The later
screenshots show BDS USB inventory, the preboot dashboard, an interactive
Shell, and a clipped apparent `BOOTX64.EFI` image-load attempt. The recording and
screens reach later than the SOL text. These artifacts therefore do not form
one continuous, identifiable same-boot trace, and cannot establish the OS
loader's final failure point.

In the screenshots, the HOB-list check is shown as failed with an
`invalid/truncated list` reason, but this VGA sequence proceeds to an
interactive Shell. Thus the screenshot does not support the HOB check as a
blocker in this run. The last image is clipped around the Shell image-load
attempt and does not prove a completed `LoadImage()`, GRUB, or Linux kernel
handoff. No complete SOL evidence for BDS boot-option processing, `OS START`,
or `OS EBS` is present in these text files. The OS failure cause remains
unresolved until these events are captured on SOL from the same boot as the
VGA evidence.

## Serial-first change

The EDK2 VGA text and framebuffer writers previously copied each changed row
to `DEBUG_ERROR` serial output as `[GX-VGA] row=...`. Repeated dashboard
redraws overwhelmed the useful SOL chronology and obscured where the text
capture ended. Those row echoes and their VGA-row reread helper have been
removed from SEC, PEI, DXE IPL, and SMM-store VGA writers. They still render to
VGA as before. Existing structured serial events remain, including phase
checkpoints, BDS/Shell, `GX-BOOT`, `OS START`, and `GX-EBS` records. This is a
logging-only change: no HOB validation, gate, boot-option, image-loading, or
ExitBootServices behavior was changed.

## Next capture procedure

1. Start an unfiltered raw SOL recording before reset and leave it running
   through the OS result or a stable failure. Use the configured 115200 baud,
   8-N-1 connection and do not start a new archive mid-boot.
2. Record the wall-clock time of reset and capture VGA screenshots/video from
   that same boot. Keep the raw SOL file; do not copy text from the VGA screen
   as a substitute for missing serial output.
3. Record the flashed ROM readback SHA-256, selected `Boot####` entry, GRUB
   entry and full kernel command line, plus any reset or keypress.
4. Check whether the single SOL stream reaches `[GX-BDS]`, `[GX-BOOT]`,
   `OS START ENTRYPOINT begin`, `OS EBS MAP KEY ACCEPTED`, and
   `OS EBS CORE RETURNING`. The last observed marker bounds the remaining
   investigation; a successful EBS return moves the boundary to Linux's early
   boot or its active console.

## Verification status

The clean integrated `make -j6` build succeeded. The extracted
`fallback/payload` contains no `[GX-VGA] row=...` format string and retains
BDS and OS-entry markers. ROM SHA-256:
`957e2a81053e15bfffb2cc08785bb1e5ee4eabf307aed172e2d3c6fd9070decd`.
Filesystem modification time: `2026-11-23 23:07:12 +04` (Asia/Muscat).

This verifies packaging and strings, not board serial behavior. A same-boot
hardware capture is still needed to verify the complete SOL chronology. No OS
boot success or root cause is claimed from the present mismatched artifacts.
