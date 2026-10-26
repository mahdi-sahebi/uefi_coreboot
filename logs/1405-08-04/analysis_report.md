# G4DEL Boot Capture Analysis — 2026-10-26

## Scope and evidence

This report covers the files present in `/home/uefi/workspace/log/1405-08-04/` when inspected on 2026-10-26:

| File | Size | SHA-256 | Inspection |
|---|---:|---|---|
| `SOLHostCapture_Archive_1_10242026_06_18_23.log` | 9,859 bytes | `5774c346be38b6f4422e3059741210c79fa3f8f2efa2ca8656df0d589f7e753e` | Read; 207 lines |
| `SOLHostCapture_Archive_3_10242026_06_19_49.log` | 71,590 bytes | `ea7040ee7775b549a856a026966e79c2862aec113b4889ffbfd9228b3ffd4b47` | Read; 808 lines |
| `Screenshot from 2026-10-26 11-38-28.png.jpeg` | 42,973 bytes | `a2503131a9bad514240a5d20cd746f286360d75913caa5b06578d601dea87544` | Viewed |
| `Screencast from 26-10-26 11_35_04.m4v` | 5,076,908 bytes | `e9e4538e074842d3a40801ce0b2717aeabbd4862d914acbf44f23a0edd3ae0a3` | Container inspected; video not decoded |
| `Screencast from 26-10-26 11_45_03.m4v` | 8,435,277 bytes | `071e88e7b52277c7ffa8a4de6c4f36f330591d2f1329a5110cb0373bb7f20c39` | Container inspected; video not decoded |

Both screencasts are H.264. GStreamer reported that an H.264 Main Profile decoder plugin is missing, so their frames could not be reviewed here. The screenshot is the only directly inspected visual capture.

## Findings

### Coreboot, FSP, and splash asset

- FSP memory initialization and silicon initialization return status `0`; the log continues into device initialization.
- Coreboot reports copying `BOOTSPLASH/logo.bmp` into CBMEM, size `0x30dca` bytes (Archive 1, line 193). This confirms the asset was found and copied, but does not prove EDK2 displayed it.
- The ASPEED device matches vendor `1a03`, device `2000`, and the AST2050 initialization returns success (Archive 1, lines 199–201).

### Display and framebuffer

- Software-I2C EDID reading fails (`status=-2`), and EDID decoding reports failure. Firmware selects its 1024×768 KVM fallback mode (Archive 1, lines 202–205). This is a fallback path, not evidence that the AST initialization failed.
- Coreboot registers a linear framebuffer at `0x94000000`, size `0x300000` (3 MiB), 1024×768, pitch 4096 bytes, 32 bpp (Archive 1, line 206).
- EDK2 reports the same 3 MiB framebuffer and dimensions, pitch 1024 pixels, BGR format, and successful Graphics Info HOB installation (Archive 3, lines 307–310). A graphics-device HOB is unavailable, so the log says GOP will probe PCI display BARs (line 311).
- The screenshot shows readable white DXE status and driver-load text on a black VGA screen. This demonstrates visible text output on VGA at that point. It does not show the boot splash or menu.

### DXE progress and where the trace ends

- DXE Core proceeds through memory setup, handle/image/GCD services, event services, memory protection, vector handoff, decompression, firmware-volume, and section-extraction setup (Archive 3, lines 673–754).
- Four dispatched drivers have successful image loads and successful `StartImage` returns in the captured portion (Archive 3, lines 755–787):
  - `9B680FCE-AD6B-4F3A-B60B-F59899003443`
  - `80CF7257-87AB-47F9-A3FE-D50B76D89541`
  - `D93CE3D8-A7EB-4730-8C8E-CC466A9ECC3C`
  - `6C2004EF-4E0E-4BE4-B14C-340EB4AA5891`
- The dispatcher signals the end of a round and returns from the signal (lines 788–789). The capture then contains many `[GX-DEPEX] ... lookup=Not Found (operand FALSE)` messages and ends mid-line at line 808.
- A missing protocol is a normal false result for a DEPEX `PUSH` operand. These messages alone do not mean a driver failed. In the captured portion there is no driver load/start failure, exception, crash dump, GOP BAR probe result, logo-display result, BDS marker, or boot-menu marker.
- Because the archive stops in the middle of dependency-evaluation output, it does not establish whether DXE later continued, stalled, crashed, or whether the capture itself was cut short. It does not identify a specific DXE driver as the cause.

### Other warnings and assertions

- Archive 3 contains a keyboard-controller cleanup warning near line 46.
- It also contains repeated coreboot assertions in `src/soc/intel/xeon_sp/uncore_acpi.c` at lines 542, 390, and 448 (Archive 3, lines 48–79). They are noteworthy, but this capture later reaches DXE Core and dispatches drivers; the evidence here does not tie those assertions to the missing splash/menu.
- Archive 1 includes repeated early UPI capability messages such as `unknown ID=2` and `unknown ID=5`. No direct link between those messages and the missing EDK2 display is established by these captures.
- Archive 1 repeats the early FSP/UPI preamble starting at line 62. Treat this as repeated captured output; it is not enough by itself to infer a second reset.

## Assessment

The captures establish that the AST framebuffer is initialized and reported to EDK2, the splash asset reaches CBMEM, DXE Core runs, and VGA text is visible. They do **not** establish that GOP binds to the AST device, that the logo blit succeeds, or that BDS displays a menu. No specific DXE crash is proven by the available serial excerpt.

The conspicuous diagnostic problem is that expected missing-protocol DEPEX operands were printed at error severity, producing a large, noisy SOL trace that obscures dispatcher progress. The diagnostic change in the matching EDK2 checkout moves those per-operand messages to `DEBUG_VERBOSE` and adds dispatcher scan progress every 16 discovered drivers plus a per-pass scheduled/ready summary. These new checkpoints are not present in the supplied captures; they require the rebuilt payload to be flashed before they can inform the next diagnosis.

## Follow-up capture checklist

1. Flash the rebuilt ROM and capture SOL and VGA from the same boot.
2. Check for `depex scan begin`, progress, and `depex scan end` markers. The last progress marker will locate the scan if it stops again; the summary will show whether a new dispatch round was queued.
3. Check for GOP BAR candidates and status, successful GOP binding/start, logo image lookup/blit status, and BDS/menu markers. A SOL marker alone is not proof that the splash or menu is visible.
4. Save a VGA screenshot after GOP initialization and during the expected menu. Decode the screencasts with an H.264-capable viewer if video evidence is needed.
5. Investigate the `uncore_acpi.c` assertions separately, preserving their context and checking whether they still occur with successful GOP/menu output.

## Evidence locations

- [Archive 1](./SOLHostCapture_Archive_1_10242026_06_18_23.log)
- [Archive 3](./SOLHostCapture_Archive_3_10242026_06_19_49.log)
- [VGA screenshot](<./Screenshot from 2026-10-26 11-38-28.png.jpeg>)
- Screencasts: `Screencast from 26-10-26 11_35_04.m4v` and `Screencast from 26-10-26 11_45_03.m4v`
