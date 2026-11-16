# G4DEL problem and evidence ledger

This ledger separates what was reported from what a source diff, build, or
board capture establishes. The historical timeline covers coreboot
`253bdcd0bf` and EDK2 `f285869ec0`; follow-up working changes described below
are newer and not yet board-verified. “Implemented” means a source change
exists; it does not mean the hardware result is verified.

## Status key

- **Reported:** operator description without a matching retained trace.
- **Observed:** visible in a named capture or reproducible tool output.
- **Verified statically:** established in source, a build, image inspection,
  or hash comparison.
- **Inferred:** plausible explanation that has not been isolated by a test.
- **Unresolved:** more than one explanation remains or the necessary capture is
  absent.

## Problem index

| ID | Problem | Stage | Current confidence | Current disposition |
| --- | --- | --- | --- | --- |
| P01 | G4DEL support initially inherited from a different board target | Platform port | Source history | Dedicated G4DEL target exists; hardware wiring/population still needs verification. |
| P02 | Reported preboot power failure with one ROM and recovery on another | SPI/descriptor/board power | Unresolved | Controlled descriptor/permission ROM variants were prepared; no conclusive board result in this source range. |
| P03 | Protect factory data and establish image identity | Flash layout/build | Statically verified | Inventory and layout tools exist; flashed ROM identity requires a chip readback. |
| P04 | SOL/serial route disagreement and lost stage output | Bootblock through EDK2 | Partially configured; hardware verification pending | Current configuration selects COM2 `0x2f8`; confirm from reset against BMC capture. |
| P05 | Early VGA breadcrumbs disappear or graphics fail later | Coreboot/EDK2 graphics | Source configured; hardware result incomplete | Framebuffer-to-GOP handoff and diagnostics exist; require a fresh VGA trace. |
| P06 | PEI low-memory selection/HOB bounds risks | PEI memory handoff | Static risk recorded | Validation was added in EDK2 history; confirm exact current paths and capture HOB/memory results. |
| P07 | FTW/SMMSTORE metadata failures, delays, or invalid workspaces | PEI variable storage | Source changes and old observations | Bounds/geometry checks and trace points exist; current board variable-storage behavior is unverified. |
| P08 | Earlier trace stopped at or near DXE/Core CPU initialization | DXE dispatcher | Historical capture only | Added paired service/driver checkpoints; current G4DEL board completion is not established by this trace. |
| P09 | USB diagnostics and device inventory correctness | DXE/BDS/USB | Source/build verified; hardware unverified | Inventory and shell inspection path exist; verify actual controllers, devices, keyboard, storage. |
| P10 | Menu/Shell availability versus actual usable input | BDS/console | Packaged/configured; hardware unverified | Shell and menu are included; require visible menu selection and a successful Shell command. |
| P11 | Linux and Windows boot failures or freezes | EFI OS loader/kernel | Reported and partly observed; root cause unresolved | Fedora reaches GDM service start; Kali capture stops around EBS; Windows evidence is absent. |
| P12 | EBS screenshot did not identify the launched EFI path/options or final cleanup | EDK2 OS handoff | Source instrumentation updated; board unverified | Readable path/options and distinct map-key/final-return checkpoints; physical verification pending. |
| P13 | Captures and source/build image identity do not always align | Evidence/reproduction | Unresolved for older captures | Pair every new trace with firmware revisions, build hashes, and full-chip readback hash. |
| P14 | ACPICA reports unresolved ACPI namespace references | ACPI tables / Linux AML interpreter | Observed on Fedora; cause and impact unresolved | Add runtime domain-name mapping; preserve full ACPI table dump from the same ROM. |

## Detailed records

### P01 — Board identity and inherited support

- **Symptom/history:** Early work reused ASRock Rack SPC741D8 configuration and
  UART setup while targeting the Gooxi G4DEL. The source lineage includes
  changes restoring SUART routes and later registering a dedicated Gooxi
  board target.
- **Evidence:** The coreboot history records G4DEL board registration and
  board-specific Kconfig, device tree, bootblock, flash map, and IIO inputs.
  Those files establish selected configuration, not the server's populated
  hardware or exact board revision.
- **Change:** A dedicated `src/mainboard/gooxi/g4del/` target is built from the
  existing Intel Xeon-SP support. The Archer City dual-socket IIO table
  remains a porting input.
- **Status/next step:** **Verified statically; physical mapping unresolved.**
  Capture PCI inventory, CPU/socket count, DIMM topology, UART wiring, and
  ASPEED IDs on the actual board. Do not infer these from a family manual.
- **References:** [boot-flow report](boot_flow_memory_map.md),
  [platform glossary](glossary.md),
  [Gooxi family manual](https://www.gooxi.us/uploadfiles/2024/01/20240122163352960.pdf).

### P02 — Reported preboot failure and descriptor hypotheses

- **Symptom/history:** An operator reported that a flashed firmware image
  failed before host power/fan/SOL output while another image could power on.
  A controlled ROM comparison report retained hashes and candidate images.
- **Evidence:** The report identified static differences in descriptor
  permissions/HAP and prepared one-variable ROM variants. That comparison is
  useful for hypothesis testing; it cannot establish a physical cause without
  controlled programming, readback, and board results.
- **Change:** A power-isolation ROM generator and a dated test matrix were
  added. No permanent descriptor/ME policy change is justified by the
  retained evidence.
- **Status/next step:** **Reported; cause unresolved.** Re-run only with known
  recovery media, confirmed full-chip reads, and one controlled variant per
  test; record BMC and host power state separately.
- **References:** [preboot isolation report](../g4del_preboot_failure_isolation.md),
  [flash inventory](factory_image_inventory.md),
  [coreboot flashmap documentation](https://doc.coreboot.org/lib/flashmap.html).

### P03 — Factory-region preservation and image identity

- **Problem:** The replacement image must preserve board-specific factory
  regions and avoid confusing a local build artifact with the image actually
  programmed on the server.
- **Evidence:** Factory image inspection records the 64 MiB input and IFD
  regions. The G4DEL utility checks region geometry and can compare the
  descriptor, GbE, ME, and Device Exp1 regions byte-for-byte.
- **Change:** `board.fmd`, the factory inventory, and `util/g4del_stock.py`
  make layout checks repeatable. Proprietary image and extracted region files
  remain local build inputs and are not repository documentation artifacts.
- **Status/next step:** **Static checks available; current chip identity
  unresolved.** Record input hash, output hash, programmer log, and full-chip
  readback hash for each board run.
- **References:** [factory image inventory](factory_image_inventory.md),
  [image inspection guide](../inspection/server_bin_memory_layout.md),
  [CBFS](https://doc.coreboot.org/lib/cbfs.html) and
  [FMAP](https://doc.coreboot.org/lib/flashmap.html).

### P04 — Serial/SOL routing and missing checkpoints

- **Symptom/history:** Serial output varied across SPC741D8-derived UART
  routes and EDK2 build revisions. A DXE issue was also traced to an extra
  low-memory coreboot-table scan that could override the board-selected UART
  base.
- **Evidence:** Current `.config` selects serial console, 115200 baud, and
  `CONFIG_TTYS0_BASE=0x2f8`; EDK2 payload parameters select the same base.
  Prior SOL captures prove output for their specific unidentified image only.
- **Change:** G4DEL bootblock and EDK2 logging route diagnostics through the
  configured COM2 path; the DXE investigation documents removing a second
  UART rescan. Source selection alone is not an end-to-end SOL test.
- **Status/next step:** **Configured; runtime verification pending.** Capture
  raw SOL from reset and compare markers at bootblock, FSP, payload, SEC, PEI,
  DXE, BDS, and Shell. If VGA advances but SOL stops, inspect UART routing
  separately from execution.
- **References:** [SOL route and boot flow](boot_flow_memory_map.md#sol-and-serial-diagnostic-route),
  [DXE investigation](../dxe.md),
  [coreboot console documentation](https://doc.coreboot.org/getting_started/architecture.html),
  [coreboot discussion channels](https://doc.coreboot.org/community/forums.html).

### P05 — VGA text memory, framebuffer, and GOP

- **Symptom/history:** Direct `0xB8000` text writes are not reliable after the
  platform switches from VGA text mode to a linear framebuffer. A blank or
  changed screen could mean video handoff failure while firmware continues.
- **Evidence:** Source configures the linear framebuffer and exports its
  metadata through coreboot tables/HOBs to EDK2 GOP. Old screenshots do not
  establish that the same path initialized on the current image.
- **Change:** Framebuffer setup, logo transfer, EDK2 GOP diagnostics, and a
  bounded VGA logger were added over time.
- **Status/next step:** **Source configured; current board behavior
  unresolved.** Record VGA video together with SOL, framebuffer/GOP status,
  display mode changes, and last serial checkpoint.
- **References:** [graphics path](boot_flow_memory_map.md#coreboot-and-edk2-graphics-path),
  [coreboot architecture](https://doc.coreboot.org/getting_started/architecture.html),
  [UEFI graphics output protocol](https://uefi.org/specs/UEFI/2.11/12_Protocols_Console_Support.html).

### P06 — PEI memory and HOB validity

- **Problem/risk:** Earlier source analysis identified a possible low-memory
  selection underflow when no qualifying contiguous fragment was recorded,
  plus unsafe assumptions around HOB addresses/lengths. A risk in selecting a
  64 MiB PEI memory range across fragmented low RAM was documented.
- **Evidence:** These are source-level failure modes, not proof that the
  G4DEL board hit the path. Older captures reached later PEI stages, but the
  logs do not isolate a single commit as the reason.
- **Change:** EDK2 topic history records memory selection, HOB bounds, and
  allocation checks. The current topic retains related bring-up notes.
- **Status/next step:** **Static hardening documented; current-runtime memory
  map unresolved.** Capture HOB ranges, PEI memory base/size, socket count,
  and the return from `PeiServicesInstallPeiMemory()` on the physical board.
- **References:** [boot-flow PEI analysis](boot_flow_memory_map.md#edk2-sec-pei-dxe-and-bds),
  [PI specification](https://uefi.org/specs/PI/1.8A/),
  [EDK II repository](https://github.com/tianocore/edk2).

### P07 — SMMSTORE and FTW

- **Symptom/history:** Earlier PEI captures included invalid FTW workspace
  observations and timer assertions. SMMSTORE/HOB geometry and range
  assumptions created additional risks for variable storage.
- **Evidence:** The historical report notes that FTW still installed its
  completion PPI and PEI dispatch continued; timer assertions were not shown
  to stop the boot. Source review identified geometry and allocation checks
  that needed hardening.
- **Change:** EDK2 history added HOB validation, range checks, completion
  status logs, and removal of an artificial debug loop. The flash map reserves
  256 KiB for SMMSTORE v2.
- **Status/next step:** **Source changes present; variable persistence and
  current board behavior unverified.** Confirm HOB geometry, FTW PPI
  installation and status, then test variable writes/reboot persistence only
  after the system reaches an interactive environment.
- **References:** [flash geometry](factory_image_inventory.md),
  [boot-flow PEI analysis](boot_flow_memory_map.md#edk2-sec-pei-dxe-and-bds),
  [coreboot SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html),
  [PI specification](https://uefi.org/specs/PI/1.8A/).

### P08 — DXE dispatch and CPU initialization

- **Symptom/history:** A historical capture ended at `CpuDxe` entry; paired
  return, BDS, and menu evidence was absent. Another earlier trace reached DXE
  Core in video but had ambiguous serial output and no identified flashed
  image.
- **Evidence:** The captures identify only their last visible marker. They do
  not prove whether execution halted, reset, or continued with loss of video.
- **Change:** EDK2 added status-return checkpoints around DXE services,
  dispatcher progress, CPU/MP initialization, BDS console connection, menu,
  and Shell entry. Coreboot added stage/FSP/payload markers.
- **Status/next step:** **Instrumentation present; latest board outcome
  unknown.** Capture paired begin/return markers and identify the flashed ROM
  before making CPU/APIC or memory-topology changes. `CONFIG_MAX_CPUS=255`,
  two configured sockets, and EDK2's PCD limit are configuration facts, not
  proof of active CPUs.
- **References:** [DXE investigation](../dxe.md),
  [stage map](boot_flow_memory_map.md),
  [coreboot architecture](https://doc.coreboot.org/getting_started/architecture.html).

### P09 — USB inventory and diagnostics

- **Problem/history:** The bring-up required a useful view of USB host
  controllers/devices in firmware, while early diagnostics risked duplicated
  output across processors and one inventory fix required matching row
  arguments.
- **Evidence:** Commit history records a BSP-only diagnostic path and the
  inventory alignment fix. The configured modules and compiled strings do not
  prove a USB device enumerated on the G4DEL board.
- **Change:** EDK2 adds controller/device descriptors and paths to the
  inventory screen and logs. It explicitly cannot report negotiated per-device
  link speed through the used protocol.
- **Status/next step:** **Build/source verified; device behavior
  unverified.** Capture controller status and descriptors for keyboard and
  storage, then test input and read a file from the Shell.
- **References:** [USB path](boot_flow_memory_map.md#usb-discovery-and-display-behavior),
  [UEFI USB protocols](https://uefi.org/specs/UEFI/2.11/17_Protocols_USB_Support.html),
  [TianoCore EDK II](https://www.tianocore.org/tianocore-wiki.github.io/reference/external-resources/edk_ii.html).

### P10 — Menu, console input, and Shell

- **Problem:** An image containing the menu and Shell can still fail to
  connect console input, draw the menu, accept keys, or start the Shell.
  Serial redirection is output and does not by itself supply keyboard input.
- **Evidence:** FDF/DSC and build manifests show packaging/configuration.
  Current captures do not demonstrate a successful menu interaction or Shell
  command on the board.
- **Change:** BDS registers the menu/Shell path and diagnostics bracket menu
  launch and Shell entry.
- **Status/next step:** **Packaged; physical acceptance pending.** Record
  `ConIn`/`ConOut`, menu-ready and input events, select Shell, then run
  `devices -b`, `drivers -b`, `pci -b`, and `map -r`.
- **References:** [menu and Shell behavior](boot_flow_memory_map.md#menu-and-shell-selection),
  [UEFI Boot Manager specification](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html),
  [TianoCore community and mailing lists](https://www.tianocore.org/tianocore-wiki.github.io/community/communications/mailing_lists.html).

### P11 — Fedora, Kali, Ubuntu, and Windows boot reports

- **Reported:** Installed Kali was described as reaching the desktop and then
  becoming unresponsive; Ubuntu live media was described as reaching its
  installer; Windows live media was described as not starting. These are
  operator reports unless tied to the exact capture and ROM.
- **Observed:** The supplied Fedora Live log (kernel
  `7.2.11-300.fc45.x86_64`) reports 128 activated processors, about 64 GiB
  available memory, initramfs unpack, simpledrm and ASPEED graphics, and
  `Started gdm`. It also reports 24 ACPICA unresolved-name errors involving
  `DI28`, `HQ28`, `HR28`, `PC21`–`PC26`, `PM28`, `PN28`, `UC2D`, and `UD2D`.
  Separate Kali screenshots (kernel `7.0.12+kali-amd64`) show Linux and
  initial ramdisk loading, `OS START ENTRYPOINT BEGIN`, and the older EDK2
  `OS EBS MAP ACCEPTED` marker. The latter only proved the map key matched;
  it was emitted before the rest of EBS cleanup. No matching Windows screen
  or stop code is in the supplied bundle.
- **Change:** EDK2 logs the EFI image path/options and each EBS attempt; a
  follow-up source change distinguishes map-key acceptance from the final
  core cleanup checkpoint.
  The Kali report documents a one-boot GRUB edit using `earlycon`, dual
  `console=`, `ignore_loglevel`, `loglevel=8`, `initcall_debug`, and `efi=debug`.
- **Status/next step:** **OS cause unresolved.** Capture raw SOL and VGA from
  reset through the stall with the exact ROM readback hash. Confirm whether
  the system remains responsive over network when video stops. Keep the GRUB
  edit one-time; do not change permanent OS or firmware policy before evidence.
- **References:** [OS startup report](../g4del_os_startup.md),
  [Kali capture procedure](../g4del_kali_boot_trace.md),
  [Linux EFI stub](https://docs.kernel.org/6.12/admin-guide/efi-stub.html),
  [Linux serial console](https://docs.kernel.org/admin-guide/serial-console.html),
  [Linux kernel parameters](https://docs.kernel.org/7.2/admin-guide/kernel-parameters.html).

### P12 — EBS trace hid image identity

- **Problem:** The supplied screenshot showed a pointer for the EFI device
  path. It could not identify which loader or options were involved.
- **Change:** EDK2 commit `f285869ec015115655e1fb6746bebdfa2a6ff85b` logs a
  readable path and options at `OS START`, then repeats them per EBS attempt.
  It retains path text before EBS so the callback does not allocate memory
  while the OS loader is using a memory-map key.
- **Follow-up:** EDK2 source now labels successful memory-map-key validation
  `OS EBS MAP KEY ACCEPTED ... cleanup=IN_PROGRESS` and emits
  `OS EBS CORE RETURNING status=EFI_SUCCESS` after core cleanup, immediately
  before returning. This avoids treating a pre-cleanup marker as proof that
  `ExitBootServices()` returned. Coreboot's Xeon-SP ACPI code also logs
  generated ACPI domain name/socket/stack/bus tuples; this is diagnostic only
  and does not alter AML tables.
- **Status:** The original path/options change is verified in the historical
  integrated build. Follow-up checkpoints and ACPI mapping are source changes
  pending a fresh integrated build and board boot. Neither proves an OS fix.
- **References:** [Kali trace report](../g4del_kali_boot_trace.md),
  [UEFI EBS requirements](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html),
  [UEFI loaded-image information](https://uefi.org/specs/UEFI/2.11/04_EFI_System_Table.html).

### P14 — ACPICA unresolved namespace references

- **Observed:** Fedora's `bootfile.txt` contains 24 ACPICA errors from AML
  namespace lookup (`dswload2`/`psobject`) for the 13 unique names listed in
  P11. Despite those errors, this same log continues through Linux CPU and
  memory initialization, initramfs unpack, graphics initialization, and GDM
  service start. They are therefore not a demonstrated cause of the reported
  Kali freeze.
- **Verified statically:** The checked-out source names Xeon-SP IIO domains
  from socket and stack identifiers in `src/soc/intel/xeon_sp/acpi.c`; the
  existing generated DSDT does not contain all reported names. That is
  insufficient to identify their source: runtime SSDTs are generated
  separately and the evidence bundle has no full runtime ACPI table dump tied
  to a readback-identified ROM.
- **Change:** Coreboot now emits `ACPI DOMAIN MAP` lines with name, type,
  socket, stack, and bus to correlate firmware topology with Linux table
  diagnostics. It does not change the generated ACPI namespace or AML.
- **Status/next step:** **Observed lead; origin and impact unresolved.** On the
  exact firmware under test, save all ACPI tables with `sudo acpidump -b`,
  decompile the complete DSDT/SSDT set with ACPICA `iasl`, identify the AML
  `Scope` and references containing each unresolved name, and compare them to
  the new domain map. Repeat on Windows only if a table defect is established.
- **References:** [ACPI specification](https://uefi.org/specs/ACPI/6.5/),
  [ACPICA tools](https://acpica.org/downloads).

### P13 — Capture, revision, and flashed-image provenance

- **Problem:** Some older captures do not identify the image flashed; a
  build's hash does not establish the current chip contents. The available
  material includes separate dates, OSes, and capture types.
- **Evidence:** The supplied directory includes a Fedora kernel log, Kali
  screenshots, and a screen recording; they must not be combined as one boot.
  No matching raw post-EBS Linux capture is currently retained.
- **Change:** Current docs record source revisions and artifact hashes and
  require a full-chip readback hash for new tests.
- **Status/next step:** **Historical provenance incomplete.** For each new
  run, preserve raw SOL/VGA, board/BMC state, UTC/local timestamp, exact
  boot choice, source revisions, build hashes, and readback hash in one
  report directory.
- **References:** [current Kali trace record](../g4del_kali_boot_trace.md),
  [factory inventory](factory_image_inventory.md),
  [Dasharo log capture guide](https://docs.dasharo.com/common-coreboot-docs/dumping_logs/).
