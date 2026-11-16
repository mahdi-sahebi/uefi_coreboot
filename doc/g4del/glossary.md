# G4DEL firmware glossary and keywords

Definitions describe how a term is used in these notes. External references
explain the general interface or standard; they do not prove a specific G4DEL
implementation. Board-specific source and evidence are indexed in the
[architecture guide](README.md#boot-chain-and-source-map) and
[problem ledger](problem_ledger.md).

| Keyword | Meaning in this project | Reference |
| --- | --- | --- |
| **ACPI** | Tables and AML interfaces used by the OS to discover platform devices, interrupts, power, and topology. | [ACPI Specification 6.5](https://uefi.org/specs/ACPI/6.5/); see [boot-flow notes](boot_flow_memory_map.md). |
| **BDS** | EDK2 Boot Device Selection phase: connects consoles and follows boot policy to a boot option, menu, or Shell. | [UEFI Boot Manager](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html). |
| **BIOS region** | Intel flash descriptor region containing firmware; distinct from the full 64 MiB SPI image and the non-BIOS regions. | [coreboot flashmap](https://doc.coreboot.org/lib/flashmap.html); [factory inventory](factory_image_inventory.md). |
| **BMC / SOL** | Board Management Controller and its Serial over LAN path for remote serial capture. SOL is an output transport, not necessarily keyboard input. | [Dasharo log capture guide](https://docs.dasharo.com/common-coreboot-docs/dumping_logs/); [SOL route](boot_flow_memory_map.md#sol-and-serial-diagnostic-route). |
| **Boot Services** | UEFI pre-OS services, including image loading, protocol access, pool/page allocation, and memory map queries. They end at successful EBS. | [UEFI Boot Services](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html). |
| **CBFS** | coreboot's ROM archive holding firmware stages, payload, and other files inside a flash region. | [coreboot CBFS documentation](https://doc.coreboot.org/lib/cbfs.html). |
| **CBMEM** | coreboot memory-resident handoff/storage area used to share data such as the boot log, tables, and logo between stages. | [coreboot CBMEM guide](https://doc.coreboot.org/getting_started/cbmem.html). |
| **coreboot tables** | Handoff records describing memory, framebuffer, and other platform information to a payload. | [coreboot tables source documentation](https://doc.coreboot.org/getting_started/architecture.html); [G4DEL boot flow](boot_flow_memory_map.md). |
| **DXE** | UEFI Driver Execution Environment: dispatches drivers and installs protocols before BDS. | [UEFI PI specification](https://uefi.org/specs/PI/1.8A/). |
| **EFI device path** | Structured identity/location path for a device or file, used by UEFI image loading and reported by the new OS handoff trace. | [UEFI Device Path protocol](https://uefi.org/specs/UEFI/2.11/10_Protocols_Device_Path_Protocol.html). |
| **EFI stub** | Linux kernel PE/COFF entry that lets firmware load the kernel as a UEFI image and passes it toward native kernel startup. | [Linux EFI stub documentation](https://docs.kernel.org/6.12/admin-guide/efi-stub.html). |
| **EDK2** | TianoCore's modular firmware development environment and implementation used as the coreboot payload. | [TianoCore EDK II documentation](https://www.tianocore.org/tianocore-wiki.github.io/reference/external-resources/edk_ii.html). |
| **EDID** | Display data used to select a supported video mode; the G4DEL coreboot graphics path logs its selected/fallback mode. | [Linux DRM EDID helpers](https://docs.kernel.org/gpu/drm-kms-helpers.html#edid-helpers). |
| **eSPI / LPC** | PCH low-speed host interfaces used for legacy I/O routing, including serial ports, between platform logic and peripheral controllers. | [coreboot architecture](https://doc.coreboot.org/getting_started/architecture.html); board-specific decode is in `src/mainboard/gooxi/g4del/bootblock.c`. |
| **ExitBootServices (EBS)** | UEFI handoff call that ends boot services when an OS loader supplies the current memory-map key. It marks the firmware logging boundary. | [UEFI ExitBootServices](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html). |
| **FMD / FMAP** | Firmware Map Description is the build input; Flashmap describes bounded regions in the resulting image. | [coreboot FMD/Flashmap](https://doc.coreboot.org/lib/flashmap.html). |
| **FSP** | Intel Firmware Support Package: binary silicon initialization used by coreboot for platform initialization. This target logs FSP-M/S calls; FSP-T presence in the image does not prove a runtime call. | [Intel FSP overview](https://www.intel.com/content/www/us/en/support/articles/000094560/processors/intel-xeon-processors.html); [boot-flow notes](boot_flow_memory_map.md). |
| **Firmware volume / FFS** | EDK2 container and file formats used to package PEIMs, DXE drivers, applications, and data into firmware volumes. | [UEFI PI specification](https://uefi.org/specs/PI/1.8A/). |
| **FTW** | Fault Tolerant Write support for variable-storage updates; G4DEL EDK2 PEI validates SMMSTORE geometry and FTW metadata. | [EDK2 FTW source](https://github.com/tianocore/edk2/tree/master/MdeModulePkg/Universal/FaultTolerantWritePei); [SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html). |
| **GOP** | UEFI Graphics Output Protocol used to expose a framebuffer to DXE applications and consoles. | [UEFI console protocols](https://uefi.org/specs/UEFI/2.11/12_Protocols_Console_Support.html). |
| **HOB** | PEI handoff block: data published by PEI for later phases, including memory, graphics, and variable-store information. | [UEFI PI specification](https://uefi.org/specs/PI/1.8A/). |
| **IFD** | Intel Flash Descriptor; partitions the SPI flash into access-controlled regions such as BIOS, ME, GbE, and board data. | [coreboot Flashmap and descriptor docs](https://doc.coreboot.org/lib/flashmap.html); [image inventory](factory_image_inventory.md). |
| **IIO** | Integrated I/O configuration for Xeon SP socket/PCIe routing. The inherited dual-socket table remains a platform assumption until board PCI topology is captured. | [G4DEL port map and evidence](boot_flow_memory_map.md). |
| **Kconfig** | Declarative configuration system that selects board features and generates build configuration. | [coreboot Kconfig guide](https://doc.coreboot.org/getting_started/kconfig.html). |
| **PEI** | UEFI Pre-EFI Initialization phase: discovers memory, dispatches PEIMs, validates/publishes HOBs, then loads DXE. | [UEFI PI specification](https://uefi.org/specs/PI/1.8A/). |
| **SEC** | UEFI Security phase, first EDK2 phase after the coreboot payload handoff; establishes the environment for PEI. | [UEFI PI specification](https://uefi.org/specs/PI/1.8A/). |
| **SMMSTORE** | coreboot/EDK2 persistent variable storage interface backed by a reserved flash region and mediated by SMM. | [coreboot SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html). |
| **SPI** | Serial Peripheral Interface flash containing descriptor, firmware, and board-specific data. | [coreboot flashmap](https://doc.coreboot.org/lib/flashmap.html); [flash inventory](factory_image_inventory.md). |
| **SMM / SMRAM** | System Management Mode and its protected memory; this G4DEL build uses it as the mediation path for firmware-variable storage. | [coreboot SMMSTORE](https://doc.coreboot.org/drivers/smmstore.html). |
| **Super I/O / SUART** | Legacy serial controller block; G4DEL configures an ASPEED Super I/O UART at I/O `0x2f8` for the configured SOL path. | [G4DEL serial route](boot_flow_memory_map.md#sol-and-serial-diagnostic-route). |
| **UEFI image / PE/COFF** | Loadable UEFI applications and drivers use the PE/COFF image format; Linux can also expose a kernel EFI stub as a PE/COFF image. | [UEFI image entry](https://uefi.org/specs/UEFI/2.11/04_EFI_System_Table.html); [Linux EFI stub](https://docs.kernel.org/6.12/admin-guide/efi-stub.html). |
| **UEFI load options** | UTF-16 arguments attached to a loaded image and passed to its entry point; in this project they can carry Linux command-line arguments. | [UEFI image services](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html); [Linux EFI stub](https://docs.kernel.org/6.12/admin-guide/efi-stub.html). |
| **UEFI Shell** | Firmware application environment for commands such as `devices`, `drivers`, `pci`, and `map`; not a Linux shell and does not provide `lsusb`. | [TianoCore Shell package](https://github.com/tianocore/edk2/tree/master/ShellPkg); [G4DEL Shell checks](boot_flow_memory_map.md#menu-and-shell-selection). |
| **xHCI** | USB host-controller interface used by the DXE USB driver stack for modern USB devices. | [UEFI USB protocols](https://uefi.org/specs/UEFI/2.11/17_Protocols_USB_Support.html). |
| **earlycon / `console=`** | Linux command-line controls for early serial output and selected kernel consoles; G4DEL's one-boot Kali trace uses COM2 and VGA. | [Linux kernel parameters](https://docs.kernel.org/7.2/admin-guide/kernel-parameters.html); [serial console guide](https://docs.kernel.org/admin-guide/serial-console.html). |

## Keyword search map

`AST2600`, `ACPI`, `BDS`, `BIOS`, `CBFS`, `CBMEM`, `COM2`, `DXE`, `EDID`,
`eSPI`, `EBS`, `EDK2`, `EFI stub`, `FFS`, `Firmware Volume`, `FMD`, `FMAP`,
`FSP`, `FTW`, `GOP`, `HOB`, `IFD`, `IIO`, `Kconfig`, `LPC`, `PE/COFF`, `PEI`,
`SEC`, `SMM`, `SMMSTORE`, `SMRAM`, `SOL`, `SPI`, `Super I/O`, `UART`, `UEFI
load options`, `UEFI Shell`, `USB`, `xHCI`.
