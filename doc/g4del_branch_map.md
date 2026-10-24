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
