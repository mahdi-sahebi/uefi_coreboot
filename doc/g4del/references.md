# G4DEL annotated references

These links are intended to explain standards, upstream behavior, and where
to ask project questions. They are not evidence that a feature works on this
server. Board-specific claims must cite a source revision, named build, or
dated capture in the [problem ledger](problem_ledger.md). Last checked while
preparing this note: 2026-11-16.

## Coreboot

- [Architecture and boot stages](https://doc.coreboot.org/getting_started/architecture.html)
  — bootblock, romstage, postcar, ramstage, and payload roles.
- [CBFS](https://doc.coreboot.org/lib/cbfs.html) — ROM contents, payload
  placement, and relation between CBFS and FMAP.
- [FMD and Flashmap](https://doc.coreboot.org/lib/flashmap.html) — build-time
  flash map and region geometry.
- [Kconfig](https://doc.coreboot.org/getting_started/kconfig.html) — board
  configuration and generated build options.
- [SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html) — persistent
  firmware variables through SMM-backed flash storage.
- [Community forums and channels](https://doc.coreboot.org/community/forums.html)
  — mailing list, archives, Matrix, IRC, Discord, and public meetings. For
  board support questions, provide the exact board, source commits, full-chip
  readback identity, and unedited logs.

## UEFI, PI, and EDK2

- [UEFI specifications index](https://uefi.org/specifications) — official
  published UEFI and Platform Initialization specifications.
- [ACPI Specification 6.5](https://uefi.org/specs/ACPI/6.5/) — ACPI tables,
  device description, resource reporting, and OS power-management interfaces.
- [UEFI 2.11 Boot Services](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html)
  — `LoadImage`, `StartImage`, memory allocation, `GetMemoryMap`, and
  `ExitBootServices`. The EBS notification restrictions explain why the G4DEL
  path string is converted before the callback.
- [UEFI 2.11 Boot Manager](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html)
  — boot options and boot flow.
- [UEFI 2.11 EFI System Table](https://uefi.org/specs/UEFI/2.11/04_EFI_System_Table.html)
  — EFI image entry, loaded-image protocols, console handles, and system
  tables.
- [UEFI 2.11 Device Path protocol](https://uefi.org/specs/UEFI/2.11/10_Protocols_Device_Path_Protocol.html)
  — device path structure and file/device identity.
- [UEFI 2.11 Console Support protocols](https://uefi.org/specs/UEFI/2.11/12_Protocols_Console_Support.html)
  — text input/output and graphics output interfaces.
- [UEFI 2.11 USB protocols](https://uefi.org/specs/UEFI/2.11/17_Protocols_USB_Support.html)
  — host-controller, USB I/O, and related protocols.
- [UEFI PI 1.8 Errata A](https://uefi.org/specs/PI/1.8A/) — SEC, PEI, HOB,
  DXE, dispatcher, and firmware-volume interfaces.
- [TianoCore EDK II documentation](https://www.tianocore.org/tianocore-wiki.github.io/reference/external-resources/edk_ii.html)
  — upstream repository, stable releases, documentation, and community entry
  points.
- [EDK II development process](https://www.tianocore.org/tianocore-wiki.github.io/development/contribution-guides/edk_ii_development_process.html)
  — contribution and change process.
- [EDK II upstream repository](https://github.com/tianocore/edk2) — source,
  issues, pull requests, and discussions. The G4DEL fork/branch history is
  linked from [project history](project_history.md).

## Intel FSP and platform material

- [Intel FSP for Xeon Scalable platforms](https://www.intel.com/content/www/us/en/support/articles/000094560/processors/intel-xeon-processors.html)
  — what FSP provides and how it is integrated with coreboot/EDK2.
- [Intel FSP External Architecture Specification](https://cdrdv2-public.intel.com/644852/644852_2.3_Firmware-Support-Package-External-Architecture-Specification.pdf)
  — FSP interfaces and architecture (version-specific; use the revision that
  matches the binary/toolchain when investigating an ABI detail).
- [Gooxi SL201-G4 family manual](https://www.gooxi.us/uploadfiles/2024/01/20240122163352960.pdf)
  — family-level hardware background only. It does not confirm the exact
  G4DEL board revision or its populated PCI devices.
- [Dasharo log collection guide](https://docs.dasharo.com/common-coreboot-docs/dumping_logs/)
  — practical log capture examples. Dasharo guidance does not establish that
  the G4DEL uses the same hardware or firmware configuration.
- [ASRock Rack SPC741D8-2L2T/BCM Dasharo overview](https://docs.dasharo.com/variants/asrock_spc741d8/overview/)
  — reference-board context for the inherited port. It is not a G4DEL
  compatibility statement.

## Linux OS handoff and diagnostics

- [Linux 7.2 kernel parameters](https://docs.kernel.org/7.2/admin-guide/kernel-parameters.html)
  — `earlycon`, `efi=debug`, `ignore_loglevel`, `loglevel`, and
  `initcall_debug` parameters used by the one-boot trace.
- [Linux serial console](https://docs.kernel.org/admin-guide/serial-console.html)
  — `console=ttyS1,...`, `console=tty0`, and multi-console behavior.
- [Linux EFI stub](https://docs.kernel.org/6.12/admin-guide/efi-stub.html)
  — how the kernel is loaded as an EFI image and how its command line/initrd
  handoff works.
- [Linux DRM/KMS EDID helpers](https://docs.kernel.org/gpu/drm-kms-helpers.html#edid-helpers)
  — display identification data relevant to mode selection, not proof of a
  firmware GOP defect.

## Discussion venues and issue reports

- **Coreboot:** use the official [community channels](https://doc.coreboot.org/community/forums.html)
  and [mailing-list archives](https://mail.coreboot.org/hyperkitty/list/coreboot%40coreboot.org/).
- **EDK2:** use the current [TianoCore mailing lists](https://www.tianocore.org/tianocore-wiki.github.io/community/communications/mailing_lists.html)
  (`devel` for development, `discuss` for general questions) and the
  [upstream issue tracker](https://github.com/tianocore/edk2/issues).
- **Dasharo:** use its [documentation/support portal](https://docs.dasharo.com/)
  for Dasharo product questions; identify this work as a custom G4DEL port.
- The reviewed public material did not identify a directly relevant
  G4DEL-specific forum thread for the failures in this ledger. General
  upstream forum traffic is therefore not cited as evidence for this board.
  When raising a new discussion, include exact commits, ROM/readback hashes,
  hardware revision, BMC/SOL setup, and unedited logs while excluding
  proprietary firmware blobs and secrets.
