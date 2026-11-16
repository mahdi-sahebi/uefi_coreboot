# Firmware engineering notes

This directory documents the Gooxi G4DEL coreboot/EDK2 bring-up, its
engineering history, observed failures, and remaining hardware verification.
The notes are written for firmware maintainers. They distinguish source and
build evidence from board observations; a successful build is never presented
as proof of a successful boot.

## Revision scope

- Coreboot repository: branch `fix/os-startup`, documentation cutoff
  `253bdcd0bf46250237f56930d6f05bfd20d9bbfc`.
- History baseline: merge base of `dasharo` and `fix/os-startup`,
  `7c16974602ba86b18ba4bf767b2b37b64b5d95bf`.
- Paired EDK2 repository: branch `fix/os-startup`, revision
  `f285869ec015115655e1fb6746bebdfa2a6ff85b`.
- The coreboot branch contains 99 commits after the baseline. The timeline
  summarizes them by engineering topic; the complete commit index preserves
  traceability. EDK2 history includes only G4DEL-relevant work, not inherited
  upstream EDK2 history.

These are documentation cutoffs, not claims about which ROM is currently
flashed. See the current G4DEL guide for build hashes and the board-test
boundary.

## Start here

1. [G4DEL overview and current status](g4del/README.md)
2. [Architecture, memory map, and boot flow](g4del/boot_flow_memory_map.md)
3. [Engineering timeline and complete commit index](g4del/project_history.md)
4. [Problem and evidence ledger](g4del/problem_ledger.md)
5. [Glossary and keyword index](g4del/glossary.md)
6. [Annotated specifications, upstream docs, and community channels](g4del/references.md)

## Existing technical reports

- [Factory image and BIOS inventory](g4del/factory_image_inventory.md)
- [Offline SPI/UEFI image inspection](inspection/server_bin_memory_layout.md)
- [DXE/menu/Shell investigation](dxe.md)
- [Preboot ROM comparison](g4del_preboot_failure_isolation.md)
- [Current OS startup findings and one-boot Linux parameters](g4del_os_startup.md)
- [Kali handoff trace and next capture checklist](g4del_kali_boot_trace.md)
- [Historical bring-up plan](plan.md)
- [Development conventions](development_conventions.md)

Reports with dates in their titles or status headings preserve evidence from
that point in the investigation. For current truth, start with the G4DEL
overview and problem ledger; historical reports are not silently rewritten to
look like current board captures.

## Evidence vocabulary

Use these labels consistently:

- **Reported:** an operator's description, not reproduced in a retained trace.
- **Observed:** visible in a named board log, screenshot, or video. Record its
  file, date, and the image identity if known.
- **Verified statically:** established by source inspection, a build, image
  inspection, or a hash comparison; not a physical runtime result.
- **Inferred:** a plausible interpretation that follows from evidence but is
  not directly established by it.
- **Unresolved:** evidence is missing, contradictory, or insufficient to name
  a cause.

## External link policy

Specifications and upstream project documentation support general technical
claims. Source commits, build artifacts, and named captures support G4DEL-
specific claims. Forum and mailing-list posts are linked only when directly
relevant and are identified as discussion rather than proof. Do not infer
G4DEL support from a different board's success report.
