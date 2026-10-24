# G4DEL coreboot topic history

This local branch stack reconstructs the changes after `gooxie-g4del`
(`d6bba3c96e`). The source was `feature/dxe-log` at `f4fdd5fd82`;
that branch and its commit hashes were not rewritten.

| Branch | Commit | Purpose |
| --- | --- | --- |
| `feature/g4del/01-flash-layout` | `9f04a307af` | 64 MiB FMD, Device Exp1 input, and factory-region inspection |
| `feature/g4del/02-board-target` | `1db097581e` | Gooxi two-socket board, FSP/IIO inputs, AST SOL, and board configuration |
| `feature/g4del/03-payload-integration` | `5a4dc9461b` | EDK2 build options and UART/menu/TPM selection |
| `feature/g4del/04-firmware-diagnostics` | `681809873e` | SPI, FSP, flash-map, and payload handoff checkpoints |
| `feature/g4del/05-power-isolation` | `72e14ab77b` | Pinned diagnostic ROM generator |
| `feature/g4del/06-documentation` | `cb736b19e1` | Historical investigation and power-isolation reports |

The tree at the documentation commit matches `f4fdd5fd82` byte for byte.
The final integration branch pins the local EDK2 integration commit, so its
`.config` differs from that original tree. A clean clone cannot resolve this
unpublished EDK2 commit until the EDK2 branch is made available locally or
published later.

The factory descriptor, ME, GbE, and Device Exp1 blobs are local ignored
inputs. The source 08-02 failed ROM and generated isolation candidates are
currently absent; the descriptor remains a suspected, unproven cause of the
host power failure. No branch in this series is a hardware-verified menu or
Shell release.

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

The coreboot integration commit `834c17729a` selects EDK2
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
