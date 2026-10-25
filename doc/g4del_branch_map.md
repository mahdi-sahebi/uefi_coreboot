# G4DEL coreboot topic history

The local topic stack is now based on `feature/dxe-log` at `f4fdd5fd82`. The
original `gooxie-g4del` branch is unchanged. The flash-layout topic adds its
original commit. The Gooxi board-target change was already present in the
base, so an empty marker commit preserves its
position after flash layout. Later topic changes retain their original commits.
The USB/graphics/menu branch follows the integration tip.

| Branch | Rebased commit | Purpose |
| --- | --- | --- |
| `feature/g4del/01-flash-layout` | `5ec882566d` | Flash layout and factory-region preservation |
| `feature/g4del/02-board-target` | `8960118800` | Gooxi two-socket board, FSP/IIO inputs, AST SOL, and board configuration (empty marker; changes already in base) |
| `feature/g4del/03-payload-integration` | `aba30b6c99` | EDK2 build options and UART/menu/TPM selection |
| `feature/g4del/04-firmware-diagnostics` | `d210871c81` | SPI, FSP, flash-map, and payload handoff checkpoints |
| `feature/g4del/05-power-isolation` | `aaff54bb7a` | Pinned diagnostic ROM generator |
| `feature/g4del/06-documentation` | `23bc43da63` | Historical investigation and power-isolation reports |
| `feature/g4del/integration` | `de66602fcd` | Paired build evidence and integration notes |
| `feature/g4del/usb-graphics-menu` | this branch | Splash, graphics handoff, and USB/menu payload integration |

The integration build evidence below is historical and remains tied to its
recorded coreboot and EDK2 source revisions. The current USB/menu branch pins
EDK2 to `ec2535b47c54d8af04c045cafccec58af2d130ea`, its rebased USB/menu tip.

## Final paired build, 2026-10-24

The full build completed with no compiler warnings or errors. The original
full checkouts were returned to their starting branches/commits afterward.

| Input or output | SHA-256 |
| --- | --- |
| Factory `server.bin` (64 MiB) | `db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465` |
| Eagle Stream `Fsp.fd` | `776100021418dba4fb62446e7c420ce2bd161e849fc6d19c9f72b80a6b479e77` |
| `build/UEFIPAYLOAD.fd` | `dec023502f5e6b3fc32fc92eb0977f2ed64114dc69d3bc52a4e2f7531bb02283` |
| `build/coreboot.rom` (64 MiB) | `3aa199c4f2a99422c9b754bf021a7e0bc5003b089bcf82cbdc36ae2d01bfcd7c` |

An identical copy is retained as ignored build artifact
`build/coreboot_g4del_topic_integration.rom`. The full checkout's `.config`
was restored to the original feature branch after the build, so use the
named ROM and its hash when referring to this integration build.

The coreboot integration commit `40d031416f` selects EDK2
`d3f653673e6bc70177094b11c1ee1ecd353b0f2e`; the CBFS-embedded
configuration confirms this pin, `gooxi/g4del`, two sockets, SMMSTORE v2,
SOL at `0x2f8`, the boot menu, and the built-in Shell. It selects the factory
descriptor without HAP or host flash-region unlocking. Byte comparison of
the assembled ROM against `server.bin` passed for descriptor, GbE, ME, and
Device Exp1.

FMAP inspection reports the 256 KiB SMMSTORE area at `0x3000000`, a 1 MiB
BOOTSPLASH region at `0x3040000`, and COREBOOT CBFS at `0x3140800`.
CBFS contains the payload, bootblock, romstage, ramstage, and FSP-T/M/S.
The EDK2 build log lists `FaultTolerantWritePei`, `BlSupportPei`,
`SmmStorePei`, `BootManagerMenuApp`, and `Shell` as built modules.

This is static build evidence. No new host-power, SOL, VGA, menu-input, or
Shell-interaction result was collected, and the descriptor hypothesis remains
unresolved. The older 08-02 ROM is not present locally, so its diagnostic
variants cannot currently be regenerated or flashed from this workspace.

The source review also found a PEI memory-selection risk in EDK2
`DasharoPayloadPkg/BlSupportPei/BlSupportPei.c`: the callback records the
highest end of low RAM and later subtracts 64 MiB without checking that the
entire selected interval is backed by one contiguous usable range. A
fragmented map could therefore select a hole. The available capture does not
establish that this happened on G4DEL; the split keeps the original behavior
and records the risk for the next memory-map trace.
