# Gooxixe G4DEL / Xeon Sapphire Rapids bring-up plan

## Purpose

Bring up the Gooxixe/Gooxi G4DEL platform using the coreboot implementation
based on the ASRock SPC741D8 board support, with the Dasharo EDK2 payload, and
make it possible to identify the exact point where execution stops before the
EDK2 menu or UEFI Shell.

Implementation is now in progress. The copied `DasharoPayloadPkg` directory
remains out of scope and must not be modified.

## Implementation status

- Coreboot error-only serial policy and board-specific log-level override:
  implemented.
- G4DEL SOL selection on AST SUART2/`0x2f8`: implemented based on the
  existing board comments; verify on hardware before flashing.
- Coreboot payload VGA checkpoints with 500 ms delays: implemented.
- EDK2 PEI/DXE IPL and DXE Core/BDS checkpoints with 500 ms delays:
  implemented in the active EDK2 source tree.
- EDK2 debug/shell/menu configuration: enabled in the active build config.
- Full EDK2 payload build and hardware validation: pending.

## Scope and invariants

- Use the `asrock-spc741d8` board support as the starting point for the Xeon
  Sapphire Rapids platform.
- Confirm the exact Git base branch and commit before changing source. The
  currently checked-out branch must not be assumed to be the intended base.
- Do not modify, regenerate, replace, or reformat the copied
  `DasharoPayloadPkg` directory.
- Do not overwrite existing local changes, generated logs, build output, or
  EDK2 workspace artifacts. Record them first and keep the debugging work in
  separate commits or a separate worktree where practical.
- Keep the instrumentation clearly identifiable and removable after bring-up.
- The first debugging image is allowed to be slow because it pauses after
  checkpoints; this must not become an implicit production default.

## Phase 0: establish the build baseline

1. Record:
   - Git branch, commit, and remotes.
   - The intended `asrock-spc741d8` base branch/commit.
   - `git status` for coreboot and every nested EDK2 repository.
   - The active `.config`, toolchain versions, FSP version, EDK2 repository,
     EDK2 revision, and Dasharo build parameters.
2. Verify that the board selected by Kconfig is the SPC741D8 board and that
   the SoC is Sapphire Rapids/Sapphire Rapids-SP rather than a superficially
   similar Xeon board.
3. Confirm the actual EDK2 source used by the build. The coreboot build system
   can select an EDK2 repository and revision; the source used for the build
   must be the source inspected and instrumented.
4. Build an untouched baseline image before adding new instrumentation. Save:
   - the build log;
   - the CBFS listing;
   - the resulting image hash;
   - the serial/SOL output from a cold boot and a warm reboot.

## Phase 1: verify hardware paths

### Serial and SOL

1. Identify which UART is physically connected to the G4DEL BMC SOL channel.
   The existing SPC741D8 debug code refers to an AST Super I/O UART and a
   possible second UART at I/O base `0x2f8`, but this must be verified against
   the G4DEL wiring and current board configuration.
2. Verify the complete path independently:

   CPU/PCH UART decode -> Super I/O or AST UART -> eSPI/LPC routing -> BMC ->
   SOL client.

3. Use a short, unmistakable early-boot test string only while validating the
   path. Confirm baud rate, data format, flow control, and whether SOL is
   connected to UART1 or UART2.
4. Remove or disable the raw test once the normal coreboot console path is
   proven. The final debug trace must use the configured console path so that
   coreboot and EDK2 output can be correlated.

### VGA

1. Confirm that the VGA text buffer at `0xB8000` is usable at the point where
   the coreboot and PEI checkpoints run.
2. Determine when EDK2 switches from VGA text memory to a GOP/framebuffer or
   reinitializes the console. A write to `0xB8000` after that transition may
   not be visible even though execution continues.
3. Keep VGA checkpoints as an early-boot fallback, and use the active EDK2
   console/debug path for checkpoints after console initialization.

## Phase 2: configure the debug image

### Coreboot serial output

Configure the board debug image to:

- enable `CONSOLE_SERIAL`;
- route the console to the verified UART/SOL path;
- use the coreboot error threshold (`BIOS_ERR`) so errors, critical, alert,
  and emergency messages remain visible while debug/info/spew messages are
  suppressed;
- ensure the setting applies to bootblock, romstage, postcar, ramstage, and
  any relevant SMM diagnostic path.

Use coreboot's supported log-level override mechanism if a board-specific
choice cannot force the console-level choice. Do not rely on a stale generated
`build/config.h`; regenerate configuration and verify the effective symbols.

### EDK2 serial output

Enable EDK2 serial support and ensure that the serial terminal is not disabled
by the payload build parameters. Configure the active EDK2 debug libraries to
emit only `DEBUG_ERROR` (the error bit, `0x80000000`) through the serial
debug path. If the selected EDK2 revision uses both fixed and runtime debug
level PCDs, set both relevant PCDs to the error bit through build parameters
or a non-copied build overlay.

Do not edit the copied `DasharoPayloadPkg` directory to make this change. Use
the coreboot EDK2 build configuration/custom build parameters or a separate
overlay that is explicitly selected by the build.

### Menu and Shell targets

For the debug image:

- enable the EDK2 boot manager/menu;
- enable the boot-manager escape key path;
- set a sufficiently long boot timeout for observation;
- enable the EDK2 UEFI Shell package/application;
- keep serial terminal support enabled;
- make sure the shell is not removed by a release-only package rule;
- disable secure-boot restrictions only if they prevent the debug shell from
  launching, and record that change.

The first success criterion is reaching the EDK2 menu. The second is launching
the UEFI Shell and obtaining a usable prompt over the intended console.

## Phase 3: add checkpoint instrumentation

### Common checkpoint format

Use one common, searchable format on both VGA and serial, for example:

`[GX-DXE] seq=NN phase=<phase> event=<event> status=<EFI status>`

Each checkpoint should include enough context to identify the last completed
operation without requiring a second log source. Status values should be
printed for every EFI operation that can fail. Include addresses only where
they are safe and useful; avoid dereferencing unvalidated pointers merely to
print diagnostics.

### VGA output and delay semantics

1. Centralize checkpoint output in a small helper rather than manually adding
   unrelated print-and-delay pairs throughout the code.
2. Emit the checkpoint to VGA and serial with the same sequence number and
   text.
3. Add a calibrated 500 ms delay after each logical VGA checkpoint. Use the
   EDK2 timer/delay library available in the active PEI/DXE build rather than
   an uncalibrated nested busy loop.
4. A clear-screen operation is not 25 independent diagnostic checkpoints; it
   should not cause 25 delays. A hex dump is one logical checkpoint followed
   by one delay after the dump completes.
5. Put the final delay immediately before a non-returning DXE handoff. A
   checkpoint after a successful handoff is not expected to execute.
6. Make the delay and checkpoint code debug-build-only or controlled by a
   clearly named build option so normal firmware is not slowed down.
7. Bounds-check VGA row and column values and ensure formatted strings cannot
   overrun the VGA line buffer.

### Required checkpoint coverage

Instrument the path in this order:

1. Coreboot payload lookup and payload load.
2. Entry into the EDK2 PEI image.
3. DXE IPL PEIM initialization.
4. PEI shadow registration and permanent-memory discovery.
5. Guided-section and decompression PPI installation.
6. HOB validation and key HOB addresses.
7. DXE Core firmware-volume search.
8. DXE Core file discovery.
9. PEI Load File PPI lookup and DXE Core image load.
10. DXE Core file information and module HOB creation.
11. Stack allocation, IDT setup, page-table creation, and memory protection.
12. End-of-PEI PPI installation.
13. IA32-to-long-mode transition where applicable.
14. Final handoff to `DxeCore`.
15. DXE Core entry and early DXE dispatcher initialization.
16. DXE driver dispatch start, progress, failure, and completion.
17. Console/GOP initialization.
18. Platform Boot Manager entry, boot option enumeration, menu entry, and
    shell launch.

Every error return should produce a serial `DEBUG_ERROR` record and a VGA
record when VGA is still available. Assertions and dead loops should have a
last checkpoint immediately before them.

## Phase 4: build and inspect

1. Regenerate Kconfig output from the intended baseline.
2. Build the EDK2 payload separately first, checking that the build uses the
   intended EDK2 revision and does not rewrite the copied payload directory.
3. Inspect the payload contents and confirm the shell and serial components
   are present.
4. Build coreboot with the payload embedded.
5. Inspect CBFS and confirm the expected payload name, compression, size, and
   board configuration.
6. Preserve the complete build command and logs for reproducibility.
7. Do not flash until the image identity and target programmer/device are
   independently verified.

## Phase 5: hardware test procedure

For each test, capture SOL output, VGA observations, reset type, and the last
checkpoint.

1. Cold boot with no keyboard input.
2. Cold boot with the EDK2 boot-menu escape key held.
3. Warm reboot.
4. Boot with DIMM population matching the known-good SPC741D8/G4DEL setup.
5. Boot with optional PCIe devices removed, then add them back one at a time
   if the failure point changes.
6. Attempt to reach the EDK2 menu.
7. Attempt to launch the UEFI Shell.
8. From the shell, verify memory, PCI, filesystem, and console visibility as
   needed for the next debugging step.

The last checkpoint is the primary result. Do not infer a DXE failure merely
because the VGA screen stopped changing; verify whether serial checkpoints
continued after VGA became unavailable.

## Failure triage map

- No coreboot serial output: investigate UART decode, Super I/O/AST routing,
  eSPI/LPC setup, SOL configuration, or baud mismatch.
- Coreboot output stops before payload entry: investigate CBFS payload lookup,
  decompression, memory mapping, or payload entry address.
- PEI checkpoints stop before DXE Core discovery: investigate HOBs, permanent
  memory, PPIs, and firmware-volume access.
- DXE Core load succeeds but handoff fails: investigate stack, IDT, page
  tables, long-mode transition, memory permissions, and entry-point address.
- Serial reaches DXE but VGA stops: treat this first as a console transition,
  GOP, or VGA ownership issue rather than an execution failure.
- DXE Core starts but no menu: investigate DXE dispatcher errors, missing
  console/GOP drivers, boot-manager policy, boot timeout, and shell package
  inclusion.
- Menu appears but Shell is absent: inspect shell build flags, FV contents,
  boot options, secure-boot policy, and shell launch dependencies.

## Acceptance criteria

The work is complete when all of the following are true:

- The image is built from the intended `asrock-spc741d8` base and the verified
  EDK2 revision.
- The copied `DasharoPayloadPkg` directory remains unmodified.
- Coreboot and EDK2 serial/SOL output show only error-level diagnostics plus
  the explicitly added `DEBUG_ERROR` checkpoints.
- Every checkpoint has a corresponding VGA output while VGA is available.
- Each logical VGA checkpoint is followed by approximately 500 ms of delay.
- The serial trace identifies the last successful stage on failure.
- The system reaches the EDK2 menu.
- The system can launch and use the UEFI Shell, or the remaining blocker is
  narrowed to a specific documented hardware or firmware operation.

## Cleanup after diagnosis

After the failing operation is identified, save the logs and checkpoint map,
then remove or disable the 500 ms delays and temporary raw UART tests. Keep
the useful error handling and a minimal optional checkpoint mode in a separate
debug configuration. Rebuild and compare the normal image against the saved
baseline before considering the work complete.
