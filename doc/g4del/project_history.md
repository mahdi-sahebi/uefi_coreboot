# G4DEL engineering history

## Revision boundaries

This history starts at the merge base of coreboot `dasharo` and the current
G4DEL topic line, `7c16974602ba86b18ba4bf767b2b37b64b5d95bf`, and ends at
coreboot `253bdcd0bf46250237f56930d6f05bfd20d9bbfc` (`fix/os-startup`). The
range contains 99 commits, including merge commits. A full chronological index
appears below; the milestones summarize the engineering story without
assigning unverified hardware outcomes to source changes.

The paired EDK2 source is the separate
[`uefi_edk2`](https://github.com/mahdi-sahebi/uefi_edk2) checkout on
`fix/os-startup` at `f285869ec015115655e1fb6746bebdfa2a6ff85b`. The table at the
end of the timeline lists all four commits after the EDK2 `gooxie-g4del`
integration base, `609f681c4d4897d018502eca19fdcaca77265f78` (merge of the
USB fix). That base already contains the earlier G4DEL porting, graphics,
USB, and DXE work. Earlier branch-map context remains in the nested checkout's
historical `doc/g4del_branch_map.md` and linked reports; this document does
not copy its general EDK2 history.

The 99-commit index intentionally ends at `253bdcd0bf`; follow-up OS-startup
diagnostic changes are outside that index. Current follow-up tips are coreboot
[`11ab134819`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/11ab134819afe08493570794397992ab68e82652)
and EDK2
[`e606437817`](https://github.com/mahdi-sahebi/uefi_edk2/commit/e606437817b263d7e2a64374f6f7e63ef83f14cd).
The coreboot follow-up adds runtime ACPI domain-to-socket/stack/bus mapping
logs and updates the evidence reports; it does not change generated ACPI
tables.

## Topic timeline

### 1. Dasharo-derived coreboot and payload foundation — August to October 2026

The branch began from an ASRock/SPC741D8-oriented integration. Initial work
added an EDK2 payload, serial/SOL and VGA breadcrumbs, SPI/flash regions, and
SMMSTORE support. Several revisions switched between UART routes and payload
branches while determining how the board's BMC SOL path was wired. The branch
then added a distinct Gooxi G4DEL board target rather than continuing to treat
the ASRock board as the target. The actual hardware route and platform
population still require board evidence; source comments and a successful
build are not proof.

### 2. G4DEL stage, flash, FSP, and handoff diagnostics — November 2026

The G4DEL target registered its 64 MiB flash map and emitted coreboot/FSP and
payload handoff checkpoints. Factory-region preservation became an explicit
build invariant. The EDK2 side added SEC/PEI/DXE and Shell/menu tracing, while
coreboot gained offline image inspection and SPI operation logging. These
changes made boundaries observable and made the ROM structure checkable; they
did not by themselves establish successful firmware execution on the server.

### 3. USB inventory, graphics, framebuffer, and menu path

USB instrumentation was limited to the BSP to avoid duplicated multiprocessor
output, then extended to report controller and device information. The
graphics path exported coreboot framebuffer data to EDK2, packaged a generic
coreboot splash image, and configured the EDK2 graphics/Shell path. VGA
text-aperture output and GOP/framebuffer output are separate mechanisms; a
visible early breadcrumb cannot prove later GOP, menu, keyboard, or USB
operation. The relevant design and evidence boundary are explained in the
[boot-flow notes](boot_flow_memory_map.md).

### 4. Flash isolation, reproducibility, and branch integration

Factory image inspection and the `g4del_stock.py` utility documented IFD/FMAP
regions and compared preserved non-BIOS bytes. A power-isolation report
prepared controlled ROM variants after a reported preboot failure; its
descriptor/permission hypothesis remains unconfirmed. Integration work also
fixed or recorded host build prerequisites (Python 3 FSP splitting, Pillow for
logo conversion), payload branch naming/pinning, and portable EDK2 submodule
URLs. The version banner uses a reproducible build date. See the
[problem ledger](problem_ledger.md) for status and limits of each issue.

### 5. OS startup diagnostics and Kali evidence — November 2026

The `fix/os-startup` work added Shell `LoadImage`/`StartImage` checkpoints,
DXE image-load stages, and `ExitBootServices` reporting. It then replaced the
pointer-only image-path line with a readable device path and load options at
image start and each EBS attempt. Full escaped strings go to serial and
bounded summaries to VGA. Path conversion is done before the EBS callback so
the callback does not allocate memory and alter the map key.

The Fedora log records 128 CPUs, initramfs unpack, ASPEED graphics, and GDM
service startup; it is not the Kali failure trace and does not prove an
interactive desktop. The Kali screenshot reaches the older
`OS EBS MAP ACCEPTED` marker, which was emitted before EDK2 completed EBS
cleanup. A follow-up EDK2 change now separates map-key acceptance from the
final core-cleanup checkpoint before returning success. Coreboot adds ACPI
domain-to-socket/stack/bus logging to correlate a set of unresolved AML names
observed in Fedora. This changes diagnostics only; it neither establishes
those names as the Kali cause nor alters ACPI tables. The cause remains
unresolved pending a raw SOL/VGA capture from the exact ROM read back from the
board.
See the [Kali trace report](../g4del_kali_boot_trace.md) and
[OS startup report](../g4del_os_startup.md).

## G4DEL EDK2 commit milestones

These commits are in the separate EDK2 repository, after `gooxie-g4del`; they
are paired with coreboot commits that select or integrate the payload:

| Commit | Change |
| --- | --- |
| [`58fd58e38d`](https://github.com/mahdi-sahebi/uefi_edk2/commit/58fd58e38df4380ed6aa242e1f3a51d8b87f9e56) | Add the G4DEL firmware version banner. |
| [`2ebcc016ac`](https://github.com/mahdi-sahebi/uefi_edk2/commit/2ebcc016acbcb016a590c66782e73726d054d826) | Add OS startup handoff diagnostics. |
| [`1503285e20`](https://github.com/mahdi-sahebi/uefi_edk2/commit/1503285e20cca53c011c0de0991521f20473fc87) | Trace Shell-launched EFI applications and image loading. |
| [`f285869ec0`](https://github.com/mahdi-sahebi/uefi_edk2/commit/f285869ec015115655e1fb6746bebdfa2a6ff85b) | Log readable EFI image path and options at OS handoff. |
| [`e606437817`](https://github.com/mahdi-sahebi/uefi_edk2/commit/e606437817b263d7e2a64374f6f7e63ef83f14cd) | Distinguish EBS map-key acceptance from final core cleanup. |

## Complete coreboot commit index

Each entry links to its immutable GitHub commit. Dates use the commit author date. Merge commits are included so
the index covers the exact 99-commit range, not only first-parent or
non-merge source changes.

| Date | Commit | Subject |
| --- | --- | --- |
| 2026-08-18 | [`f7d5a4af41`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/f7d5a4af41c0c00775d885e88096ce41a6acaad4) | VGA output is added |
| 2026-08-19 | [`bc2572b196`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/bc2572b196923d3ca1944ad129fd51039c868f85) | Serial is enabled |
| 2026-08-20 | [`c07c5d4081`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/c07c5d4081b905a0a26e75edbfd49dcae339cffb) | [LOG] Add SOL logs |
| 2026-08-22 | [`9b8a841689`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/9b8a841689df86b94c8b7025e0cd515bdab0d8b4) | [ADD] EDK2 Platforms is enabled |
| 2026-08-22 | [`e6bc3e0f17`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/e6bc3e0f177e748364a7b87ea44910ba3e886321) | Use DEBUG build of EDKII |
| 2026-08-24 | [`39222408ac`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/39222408ac87e4bc42d2cf0559f541b8e361d30f) | [UPDATE] EDK II is added as payload |
| 2026-08-29 | [`a5c9e185eb`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/a5c9e185eb4f3379161a698d48657045c2347cfb) | [ADD] Config files updated for asrock |
| 2026-08-29 | [`112ed6ccd0`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/112ed6ccd0f79a2d7fca9e31970261604daf8669) | [FIX] SOL and serial are enabled in the coreboot |
| 2026-08-30 | [`b34c4cd095`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b34c4cd0957209460cec05bbae08a5d739b40c68) | [FEATURE] SPI flash is enabled |
| 2026-08-30 | [`6b4e448384`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/6b4e448384e7f81c567333393bf95bb29bbe1758) | [UPDATE] Stack size is increased |
| 2026-08-30 | [`cd6d15d4d3`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/cd6d15d4d3ed8ba0896fd5d7f3ae138da95c86ce) | [UPDATE] Verbose flags are enaled for debugging |
| 2026-08-30 | [`b7a99799c8`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b7a99799c864fbcb890608ff9f6e52bc2eef77de) | [FEAT] SMMTORE, SI_BIOS regions are added into the flahs layout |
| 2026-08-30 | [`e6632ebab1`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/e6632ebab19a668c0bc89fd40eedd577356ddc6a) | [DOC] Log files of tests are added |
| 2026-08-30 | [`7ec684fe47`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/7ec684fe47e8a82ed2f38f251dddee51972dbf46) | [ADD] Flash descriptor, me and gbe binary files are added |
| 2026-09-02 | [`d8a7b52f45`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/d8a7b52f45cef967889c74b3e6e2f3502b9d5607) | [ADD] Doc of crossgcc is added |
| 2026-09-15 | [`7e96d53c1c`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/7e96d53c1cfc78897a4d9bf7313364e548bf9d22) | [UPDATE] EDK2 repository is set to uefi_edk2 |
| 2026-09-16 | [`ba573e8a0e`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/ba573e8a0ea8124a34e3770f241a4dbc5038f6aa) | [ADD] Crossgcc files are tracked |
| 2026-09-19 | [`f6dacffd6a`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/f6dacffd6a3178062262502841264c49e354f089) | [FIX] Branch name of edk2-platoforms is modfied |
| 2026-09-21 | [`3ab3f320d1`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/3ab3f320d1d688f742745d5d8c1544453f53cf21) | [UPDATE] Logs of dispatcher are updated |
| 2026-09-28 | [`fb152bc0aa`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/fb152bc0aac3d966e1f0dae92521004e2232a78c) | [FIX] Payload's branch is updated |
| 2026-09-29 | [`6e0f90e951`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/6e0f90e9513fcf64563585668dd379600ef18aab) | Add GX payload handoff serial and VGA markers |
| 2026-10-02 | [`b91c5a6869`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b91c5a6869192c582b5bbe226fa2795901385e81) | [FIX] Propagate dual UART diagnostics to test coreboot |
| 2026-10-02 | [`f9244fab1d`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/f9244fab1d9b52297d5ed8d216312c6153770551) | [FIX] EDK2_DUAL_SERIAL_DEBUG is set on |
| 2026-10-02 | [`c7bd97c451`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/c7bd97c4512de093b8f48854e708a69362bb8bb5) | Configure G4DEL EDK2 dual-UART diagnostics |
| 2026-10-04 | [`b765f82423`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b765f82423f2d18d9c91d55ba7f71fd52d98d93e) | [FIX] Align G4DEL EDK2 payload configuration |
| 2026-10-04 | [`43e87f22ce`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/43e87f22ce1f3cb901507fab60ed6f883163a8f6) | [FIX] Enable G4DEL FSP HOB diagnostics |
| 2026-10-05 | [`164769ec48`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/164769ec482f9f6c1b147f391897e68532cee295) | [UPDATE] Payload config is resoted |
| 2026-10-05 | [`72cf2361f9`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/72cf2361f999c1201174c814967c7513949e9ae5) | [FIX] Update coreboot configuration |
| 2026-10-05 | [`4c4530fc9e`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/4c4530fc9ec272273826728ac516ed544e183e4c) | Synchronize coreboot build configuration |
| 2026-10-05 | [`fe5974941f`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/fe5974941f32665441195bbfe7e537a1189de322) | Instrument G4DEL EDK2 bring-up |
| 2026-10-05 | [`638c80b002`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/638c80b002dc8d15481fd8e226903c22fc857faa) | [FIX] EDK2 VGA initialized |
| 2026-10-06 | [`c5c7b583ae`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/c5c7b583aebcc69a3361f4e537f4c6d60a620a2d) | Document VGA failure and enable build ignore |
| 2026-10-06 | [`4a14bb28ef`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/4a14bb28ef307a99304d6802bef5f8d06b94fb1b) | Use feature/log ignore rules |
| 2026-10-13 | [`dbd04c95f8`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/dbd04c95f8955a730361970743adac61b338a682) | [FIX] Path of blobs is modified |
| 2026-10-14 | [`0d335f326f`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0d335f326f3b00ae072e275446f7bab125c38731) | Align EDK2 graphical VGA payload configuration |
| 2026-10-14 | [`1fd7233174`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/1fd7233174289cccf73735d0fc2411193ab77031) | Restore SPC741D8 VGA and SUART1 diagnostics |
| 2026-10-14 | [`1bae645341`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/1bae645341ab328f93375a6a362f6be6796a2186) | Restore reference VGA payload build |
| 2026-10-14 | [`ab374a625e`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/ab374a625e853343bfa2be0d82dd39fc64bebd9a) | Pin rebased EDK2 VGA revision |
| 2026-10-14 | [`fa8784dc92`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/fa8784dc9223f536127d2a1af29e37ef87e77036) | Select EDK2 feature/vga payload branch |
| 2026-10-16 | [`ee72965d78`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/ee72965d78ab1ce838d818b035ce68136641e8ff) | Reproduce VGA firmware without large build artifacts |
| 2026-10-17 | [`a1a2d2c802`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/a1a2d2c802c59026b6dd0e62e9b7b583ef443bef) | Use local SPC741D8 firmware blobs |
| 2026-10-18 | [`93faf3970e`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/93faf3970e1a5caed47f17baf0d5762940e3708c) | Align SPC741D8 SOL routing and select serial EDK2 payload |
| 2026-10-19 | [`aa70e0be0e`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/aa70e0be0e43ce8b7010d83521e48627d35fe995) | asrock/spc741d8: clean up bootblock UART initialization |
| 2026-10-19 | [`217e425241`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/217e4252416a27a0eaa6e1d28c840f157c5ce159) | asrock/spc741d8: trace boot stages and EDK2 payload handoff |
| 2026-10-20 | [`d32d63c4a2`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/d32d63c4a20171ff647d39a8c9888f19958ec1de) | Restore SUART2 SOL setup and pin VGA mirroring EDK2 payload |
| 2026-10-20 | [`fc85dbc938`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/fc85dbc938f7033ffa48bb1fba7ffce0d61ee18d) | Enable AST2600 eSPI SOL and EDK2 serial redirection |
| 2026-10-20 | [`ea4d356f2c`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/ea4d356f2cb6cdec51d1434b3374643c2c59f405) | asrock/spc741d8: restore captured SUART1 SOL route |
| 2026-11-02 | [`0e5e7c10ca`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0e5e7c10cad7a4863cbe5df95cfbf99fec9eea6d) | [DOC] Add development conventions |
| 2026-11-02 | [`ea54e520f5`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/ea54e520f575eb7a3b157379be6d5fdc4d20f5ca) | [ADD] Register G4DEL board and flash layout |
| 2026-11-02 | [`8883575ce8`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/8883575ce8df667e4aabd461b5a08f0052cb58af) | [FEAT] Route early boot through SOL console |
| 2026-11-02 | [`9a5a6de814`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/9a5a6de814a02d046350ce4a6a711bcb05141e7d) | [FEAT] Add G4DEL ACPI and FSP tracing |
| 2026-11-02 | [`5336bacb05`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/5336bacb05c134e655e0c7d416aa44f96ce9b496) | [UPDATE] Preserve G4DEL factory flash regions |
| 2026-11-02 | [`22b99b6b20`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/22b99b6b20bb495a9315ba01b6d28b99a464ec9d) | [FEAT] Configure G4DEL DXE serial payload |
| 2026-11-02 | [`5aae221609`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/5aae221609e532f8da8223e10a415c1206c58a24) | [FEAT] Enable G4DEL USB diagnostics |
| 2026-11-02 | [`3dec2d9337`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/3dec2d93372f97c2dc5125a79e0a3fb199007db9) | [UPDATE] Limit USB Shell diagnostics to BSP |
| 2026-11-02 | [`3e785f80b8`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/3e785f80b8504fc4e52a7c44955ba59fe205df87) | [UPDATE] Select EDK2 USB branch locally |
| 2026-11-02 | [`573c1dedfd`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/573c1dedfd06a9d786d25ffc18f357f28bf0b4ac) | [UPDATE] Select EDK2 USB board configuration |
| 2026-11-02 | [`1924a990cf`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/1924a990cfad5e44046656ef7d0f9a13a127dc52) | [FEAT] Initialize G4DEL linear framebuffer |
| 2026-11-02 | [`0ce10710fb`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0ce10710fbda23afbf5cfad4de2b18452de8b066) | [FEAT] Export G4DEL framebuffer to EDK2 |
| 2026-11-02 | [`d7f2559bd3`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/d7f2559bd3037f17487040eac658ec0a6cdfd675) | [ADD] Package G4DEL splash image |
| 2026-11-02 | [`b72855dde2`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b72855dde2898971105d705698a636ac50d951bf) | [UPDATE] Pass framebuffer settings to EDK2 |
| 2026-11-02 | [`1e82c16b14`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/1e82c16b1497cd6cda5f09498be7a015f8fb1e0f) | [UPDATE] Enable G4DEL splash and framebuffer |
| 2026-11-02 | [`b0ddc85f45`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/b0ddc85f45d95dbb8a7f81e6bb74704976230c88) | [UPDATE] Select EDK2 graphics branch |
| 2026-11-02 | [`2570e212c6`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/2570e212c6b0de019f8be9a1e1a69c0e5bb1622d) | [ADD] Add factory image inspection tools |
| 2026-11-02 | [`30fddd3425`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/30fddd3425e836006b1b8afdc3697aed0d418cbc) | [DOC] Record G4DEL boot flow and flash map |
| 2026-11-02 | [`dd53c160db`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/dd53c160db650636737cd5a2c66b262b2a9a4d8a) | [DOC] Record DXE and preboot findings |
| 2026-11-02 | [`c7cd7ce13a`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/c7cd7ce13a1c0983594036076c074bb9f67636d9) | [DOC] Preserve analysis and branch history |
| 2026-11-02 | [`f37d2e1022`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/f37d2e102289e3d7fc3f7d95e144286fcb19b509) | [UPDATE] Select EDK2 inspection branch |
| 2026-11-02 | [`7031643a12`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/7031643a12485702625b3333ec6eced46861d13c) | [UPDATE] Trace G4DEL SPI flash operations |
| 2026-11-02 | [`6218830f6a`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/6218830f6a59c3840af787fc8be7313fb103ca1d) | [ADD] Add G4DEL firmware version banner |
| 2026-11-02 | [`e6f9f3bf8f`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/e6f9f3bf8fd699e8183def111dae8d44b05382dd) | [ADD] Add G4DEL power isolation ROM generator |
| 2026-11-02 | [`0fcf74a34d`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0fcf74a34d021b4cb49be2eb307b526d750b4cc3) | [UPDATE] Select EDK2 integration branch |
| 2026-11-02 | [`0785975a61`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0785975a61ac3dd1e8bd3eb59869e3a1d22110bb) | [UPDATE] Select fixed EDK2 USB inventory |
| 2026-11-02 | [`559342bf24`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/559342bf2478ae4190a9bc5dc6c216fa67ffb445) | [FIX] Use reproducible date in G4DEL banner |
| 2026-11-03 | [`2dfd7da899`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/2dfd7da89975afba7bdf4aa57024c21a0690becb) | [UPDATE] Select renamed EDK2 USB fix branch |
| 2026-11-08 | [`aedceea14f`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/aedceea14fed2701ee81d854fbddeaac2ff12939) | [UPDATE] Pin merged EDK2 integration revision |
| 2026-11-10 | [`0617cfac40`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0617cfac406bfaf2f363d94927010219f8526cc4) | [FIX] Run FSP split tool with Python 3 |
| 2026-11-10 | [`716e6a9561`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/716e6a9561417dc11c05aa830bf9622018661f79) | [UPDATE] Select G4DEL EDK2 payload branch |
| 2026-11-11 | [`2efa43b42b`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/2efa43b42be19abaaae702f3f259ed7895063c1d) | [FIX] Accept Pillow for EDK2 logo conversion |
| 2026-11-11 | [`45a926bece`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/45a926bece48a4a6398c81a057c81a6746a1da92) | Build G4DEL image from tracked ASRock firmware regions |
| 2026-11-11 | [`7e0ea4b876`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/7e0ea4b876e28809837f94754a32d157c1005f48) | Use portable EDK2 submodule URL |
| 2026-11-14 | [`441ff211d8`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/441ff211d843ec8331a046709c4f174da86b8e8d) | Apply final payload merge resolution |
| 2026-11-14 | [`a81ab169d9`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/a81ab169d962320e6dfc6b5ba361d1b551238f6a) | Merge pull request #14 from mahdi-sahebi/feature/payload |
| 2026-11-14 | [`44b17606e0`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/44b17606e0101c222ffef9b703fc5facba77ccd2) | Merge pull request #15 from mahdi-sahebi/feature/dxe |
| 2026-11-14 | [`1fb7440732`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/1fb74407325febdb042d13f62ebd80f5f345fe97) | Merge pull request #16 from mahdi-sahebi/feature/usb |
| 2026-11-14 | [`25352ad13f`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/25352ad13fe46ab46ef18f423d4c97e66562589f) | Merge pull request #17 from mahdi-sahebi/feature/graphics |
| 2026-11-14 | [`a2c6795c0a`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/a2c6795c0a3aeb81fa682b6c5df2fdafe8920b97) | Merge pull request #18 from mahdi-sahebi/feature/inspection |
| 2026-11-14 | [`0537b8c6a1`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/0537b8c6a1e65026983e5dce6ab809a0071d190c) | Merge pull request #19 from mahdi-sahebi/feature/integration |
| 2026-11-14 | [`e64f75b92a`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/e64f75b92aaab48c347becc5f35ea4b9736db65b) | Match backup origin-fix-usb tree |
| 2026-11-14 | [`8e15296855`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/8e152968559a1e878d776b8aff290b578f7a3b6a) | Merge pull request #20 from mahdi-sahebi/fix/usb |
| 2026-11-14 | [`405b346859`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/405b3468594ce8a45ddd956eb7a2888f5a7ba8ef) | [UPDATE] EDK2 branch name is set on gooxie-g4del |
| 2026-11-14 | [`acf87303ec`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/acf87303ec60f928076637cd07d1fb58120f50cb) | Restore buildable G4DEL firmware configuration |
| 2026-11-14 | [`6d5c2931d6`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/6d5c2931d62c7bf3374b1b2e94f7d0a20a865427) | Merge pull request #21 from mahdi-sahebi/fix/usb |
| 2026-11-14 | [`672c88d7a1`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/672c88d7a17f5967e964fa509de09a9b891e454a) | Add G4DEL firmware version banner |
| 2026-11-14 | [`a563369428`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/a56336942815b90cdddc055c235f71ad44f411a1) | Add G4DEL OS startup diagnostics |
| 2026-11-15 | [`824cb4a4ff`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/824cb4a4ffe4a17f881452f75ff8c1db8e68f0af) | Document Shell boot diagnostics |
| 2026-11-16 | [`474bbeb923`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/474bbeb92375472d06fefdf8d07140852213d82d) | Document current G4DEL Kali handoff trace |
| 2026-11-16 | [`e62de5e044`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/e62de5e044a1464afa503e3b72b4cf6a064fad79) | Clarify G4DEL trace evidence and publication status |
| 2026-11-16 | [`253bdcd0bf`](https://github.com/mahdi-sahebi/uefi_coreboot/commit/253bdcd0bf46250237f56930d6f05bfd20d9bbfc) | Keep G4DEL report branch status current |
