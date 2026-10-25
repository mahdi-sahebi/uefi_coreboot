# Factory image and BIOS inventory

## Inputs and image identity

The factory reference is `/home/uefi/workspace/gooxie_bios/server.bin`. The
duplicate `/home/uefi/workspace/gooxie_bios/uefi/server.bin` was byte-for-byte
identical. Both are 67,108,864 bytes (64 MiB), SHA-256
`db303ef4cefe79372f508f061d4dafe79cb5d98cfcb84127a7297b4c88100465`.
The user identifies this as the working factory image that reaches the UEFI
menu; that is user-reported operating evidence. The parser results below are
static inspection, not a new factory-board boot.

The rebuilt map retains the factory IFD and data regions, and replaces the
factory BIOS region with coreboot + EDK2. The factory AMI BIOS, setup variables,
and executable firmware are inspection-only and must not be copied into the
open-source image.

## Intel Flash Descriptor map

The following inclusive ranges were decoded from the factory descriptor and
checked against `server.bin` byte offsets. Region hashes are SHA-256 over the
complete region shown.

| IFD region | Start | End, inclusive | Size | SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Descriptor | `0x00000000` | `0x00000fff` | `0x1000` | `a71b8c312544d2e0be8e0148d7ad52053e2735ca663a2a55e1c6637fdc6553b4` |
| GbE | `0x00001000` | `0x00002fff` | `0x2000` | `51deabec8b170755d7fdf244664f45e773743d72613c3e4ef58ad9ecdcf54fed` |
| ME | `0x00003000` | `0x02feffff` | `0x02fed000` | `f7427c9722c0ca7c1150821c2a00ef09d4200090021a0b11aa3b46dd7bb5a4` |
| Device Exp1 | `0x02ff0000` | `0x02ffffff` | `0x10000` | `bf584a4c9512f0dc75f26f60d1ae3de734395b33b4dfa8dc056eb56da176eaa2` |
| BIOS | `0x03000000` | `0x03ffffff` | `0x1000000` | `2a2458ba1dc12c5a9e2dd330a2157ee4ae77772851f37a8b84ddf108f4149cad` |

`ifdtool -p lbg -d server.bin` reports one 64 MiB component and the same four
regions. Running without `-p lbg` emits a “Peculiar firmware descriptor” /
Ibex-Peak compatibility warning; that is an `ifdtool` platform-identification
warning, not a finding that the image is an Ibex Peak board. The descriptor's
HAP bit is clear. Factory descriptor settings are retained; no descriptor
unlock or ME-disable transformation is intended.

The canonical coreboot FMAP name for this IFD interval is `SI_DEVICEEXT`, not
the historical `SI_PT`. Current
[`board.fmd`](../../src/mainboard/gooxi/g4del/board.fmd) uses `SI_DEVICEEXT`.
The `SI_ALL` container covers `[0x00000000, 0x03000000)`; `SI_BIOS` covers
`[0x03000000, 0x04000000)`. These abut and cover the full 64 MiB image.

## Factory and rebuilt layout

The factory image contains the following IFD regions. The coreboot image uses
the same IFD offsets, then partitions the 16 MiB BIOS region as follows:

| Rebuilt FMAP area | Offset | Size | End, inclusive | Purpose |
| --- | ---: | ---: | ---: | --- |
| `SI_DESC` | `0x00000000` | `0x1000` | `0x00000fff` | Preserved factory descriptor |
| `SI_GBE` | `0x00001000` | `0x2000` | `0x00002fff` | Preserved factory GbE NVM |
| `SI_ME` | `0x00003000` | `0x02fed000` | `0x02feffff` | Preserved ME firmware |
| `SI_DEVICEEXT` | `0x02ff0000` | `0x10000` | `0x02ffffff` | Preserved Device Exp1 data |
| `SI_BIOS` | `0x03000000` | `0x1000000` | `0x03ffffff` | Rebuilt firmware region |
| `SMMSTORE` | `0x03000000` | `0x40000` | `0x0303ffff` | Separate EDK2 SMMSTORE v2 backing area |
| `BOOTSPLASH` | `0x03040000` | `0x100000` | `0x0313ffff` | CBFS with the project logo asset |
| `FMAP` | `0x03140000` | `0x800` | `0x031407ff` | FMAP metadata |
| `COREBOOT` | `0x03140800` | `0x00ebf800` | `0x03ffffff` | Coreboot stages, FSPs, and EDK2 payload |

The final four BIOS offsets were checked against the rebuilt
`build/coreboot.rom` with `util/g4del_stock.py --verify-layout` and
`cbfstool ... layout`. The ROM has a 64 MiB total size, all four factory
non-BIOS regions match `server.bin` byte for byte, and its SMMSTORE, BOOTSPLASH,
FMAP, and COREBOOT ranges cover the BIOS region without gaps or overlaps. The
old 08-02 image predates `SI_DEVICEEXT` and does not validate this rebuilt map.

`SMMSTORE` is 256 KiB and 64 KiB aligned, the four-block minimum used by the
current SMMSTORE v2/FTW configuration. It is independent of the factory AMI
variable stores; neither vendor store is copied into the new image. The local
build's initial region must be inspected and recorded (expected erased bytes)
before use; the FMD preserve flag is for update/rebuild handling, not proof
that factory variable values were imported. The final static build check found
all 262,144 bytes of SMMSTORE erased (`0xff`); the board has not yet run FTW
against this newly built region.

## Rebuilt image and FSP inputs (2026-10-25)

The current local artifacts were built from coreboot
`6e4458e8660fd0d32d921f3cd95543f8afc75e05` and EDK2
`ead1df0f527f48955d245a1f50072c6f22f6f007`. They have not been flashed or
read back from the board.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/coreboot.rom` | 67,108,864 | `7169a0d953b62239d50cf6f24aa185244876334b9c9d54f7becc1ed02a77fce3` |
| `build/UEFIPAYLOAD.fd` | 14,876,672 | `87e3939fce12305e1a71bd4e945f544ca6b31939df2a9145cbe6514d196d9c76` |
| `3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd` | 3,801,088 | `776100021418dba4fb62446e7c420ce2bd161e849fc6d19c9f72b80a6b479e77` |

The FSP input is split by coreboot's `3rdparty/fsp/Tools/SplitFspBin.py`.
All three components identify as `$SPR-SP$`, spec version 2.3, header revision
6, image revision 1.1.1 build 917. The header component types are FSP-T, FSP-M,
and FSP-S respectively.

The stock BIOS FSP volumes have the same `$SPR-SP$` ID, spec 2.3, and header
revision 6, with image revision 1.1.1 build 631. The complete FV hashes below
cover the FV range shown, not just the FSP binary payload. Their differing
hashes and build numbers identify distinct inputs; by themselves they do not
show a boot defect. The existing 08-02 capture reports FSP-M and FSP-S return
status zero for the image tested at that time.

| Stock FV | Image offset | Size | SHA-256 | FSP image revision |
| --- | ---: | ---: | --- | --- |
| FSP-S | `0x03b30000` | `0x60000` | `2424ba0f7bed3d704259976199e66a01f018f80f9024d73d58120ad22ee04a15` | 1.1.1 build 631 |
| FSP-M | `0x03b90000` | `0x338000` | `083e14cece22089acf0a7f2d2c62932481434d7fd7ab0adde69ba89ed7f51ef4` | 1.1.1 build 631 |
| FSP-T | `0x03ec8000` | `0x8000` | `86971932fec5ae021c4d36b250f0ffef08a72090f969fe2b922c3157103b9cfe` | 1.1.1 build 631 |

| Component | Size | Image base | SHA-256 |
| --- | ---: | ---: | --- |
| FSP-T | 32,768 | `0xfff38000` | `65ea3ea31750d3840d6534024cce2e0cabb83ae48f724e9ded2adac5f381bd84` |
| FSP-M | 3,375,104 | `0xffc00000` | `0853e533c8e055dd4582261c446ffe90ba6d31cb54d22635065d4b0212f58bc6` |
| FSP-S | 393,216 | `0xffba0000` | `2f31b08e7cebaedb0fae6ca569ef0b51b53e3c583ab01f7e21c7edf1e63881cd` |

CBFS inspection confirms that the ROM carries the EDK2 payload as
`fallback/payload` (simple ELF, 1,187,297 bytes) and the three FSP components.
`BOOTSPLASH/logo.bmp` is the bundled 5,172-byte open-source coreboot logo,
compressed to 5,172 bytes in CBFS with an uncompressed size of 200,138 bytes;
the extracted bitmap compares byte for byte with
`Documentation/coreboot_logo.bmp`. The 1 MiB region has remaining free space.
The payload's EDK2 manifest lists BootManagerMenuApp, Shell, GOP, LogoDxe,
xHCI, USB bus, USB keyboard, USB mouse, and USB mass-storage modules. This is
static package evidence only; the logs and visible device output on the board
remain unverified.

## Contents of the factory BIOS region

UEFIExtract NE alpha 76 parsed the 16 MiB BIOS region at image offset
`0x03000000`. The run produced **3,691 metadata rows**: 18 firmware volumes,
529 FFS files, 1,895 sections, 1,212 NVAR entries, 8 NVAR GUID stores, 22 free
space rows, 3 padding rows, 2 Intel microcode records, and one startup-AP-data
record, in addition to the BIOS region row. The full parent/child metadata
tree is in [`server_bios_inventory.csv`](server_bios_inventory.csv).

All 18 firmware volumes, with sizes and GUIDs as reported, are listed below.
`N/A` means UEFIExtract exposed the nested volume after decoding a parent
section but did not map its decompressed child bytes back to an absolute SPI
offset. It is deliberately not presented as an absolute address.

| Image offset | Size | FFS format | FV GUID |
| ---: | ---: | --- | --- |
| `0x03000000` | `0x80000` | FFSv2 | `FA4974FC-AF1D-4E5D-BDC5-DACD6D27BAEC` |
| `0x03080000` | `0x80000` | FFSv2 | `FA4974FC-AF1D-4E5D-BDC5-DACD6D27BAEC` |
| `0x03110000` | `0x66f000` | FFSv3 | `4F1C52D3-D824-4D2A-A2F0-EC40C23C5916` |
| `N/A` | `0x1be6000` | FFSv2 | `5C60F367-A505-419A-859E-2A4FF6CA6FE5` |
| `0x0377f000` | `0x240000` | FFSv2 | `39DAA42C-87CA-4632-8AF3-1EB3D2A2D855` |
| `0x039bf000` | `0x40000` | FFSv2 | `013B9639-D6D5-410F-B7A9-F9173C56ECDA` |
| `N/A` | `0x40000` | FFSv2 | `7EE3A288-DB88-4687-BF0F-CD028F2C861C` |
| `0x039ff000` | `0x41000` | FFSv2 | `9844E666-A856-4022-BAB4-7E6CFE7BDED0` |
| `0x03a51000` | `0x1000` | FFSv2 | `AFDD39F1-19D7-4501-A730-CE5A27E1154B` |
| `0x03a52000` | `0x2d000` | FFSv2 | `184BB4FC-4901-4784-A8EF-187EB0AFEC65` |
| `0x03a7f000` | `0x41000` | FFSv2 | `652CF0CB-FA67-47D8-9E5D-BDC4F2A685B4` |
| `0x03ac0000` | `0x70000` | FFSv2 | `14E428FA-1A12-4875-B637-8B3CC87FDF07` |
| `N/A` | `0x70000` | FFSv2 | `65093C01-F342-4450-8BCD-296037C54848` |
| `0x03b30000` | `0x60000` | FFSv2 | `1638673D-EFE6-400B-951F-ABAC2CB31C60` |
| `N/A` | `0xf0000` | FFSv2 | `E143D9B5-8EEE-466E-8A27-E7AE3416307F` |
| `0x03b90000` | `0x338000` | FFSv2 | `8AD52B63-F7EC-48C5-B2CD-45DB4F14044B` |
| `0x03ec8000` | `0x8000` | FFSv2 | `52F1AFB6-78A6-448F-8274-F370549AC5D0` |
| `0x03ed0000` | `0x130000` | FFSv2 | `61C0F511-A691-4F54-974F-B9A42172CE53` |

The report tree contains SEC, PEI, DXE, SMM, BDS, Setup, NVAR stores, and
compressed/GUID-defined sections. It does **not** label an item `Logo`,
`Splash`, or `.bmp`. This is not proof that the factory BIOS has no logo: raw
or compressed resources may have generic names. The open-source build uses
`Documentation/coreboot_logo.bmp`; it is not the Gooxi factory artwork.

UEFIExtract reported 2,043 child rows without physical image offsets, mostly
inside decoded/compressed volume-image sections. The CSV records their size,
GUID/type, parent path, section type/subtype, and the missing-offset status.
UEFIExtract's row type/subtype is the compression/section classification when
recognized; for a `GUID defined` section, the reported GUID is preserved but
the parser's text report does not always name the codec. No vendor payloads
were exported for this inventory. NVAR names/GUIDs and lengths are metadata;
variable values are not recorded.

### FSP facts

The stock BIOS has FSP-S, FSP-M, and FSP-T firmware-volume records at
`0x03b30000`, `0x03b90000`, and `0x03ec8000`. FSP-M and FSP-S are the calls
observed in the 08-02 serial log; they each report status zero. FSP-T is
present in the image/CBFS, but a runtime `FspTempRamInit` call is not shown in
that capture. The current coreboot build takes `Fsp.fd` from
`3rdparty/fsp/EagleStreamFspBinPkg/Fsp.fd`, splits it with
`3rdparty/fsp/Tools/SplitFspBin.py`, and packages the components. Record the
actual build input hash and FSP header IDs/revisions from the next successful
build; do not infer equality with the factory FSP based only on version
strings.

## Offline inspection artifacts: image attribution

The historical offline artifacts are useful as procedures, not as G4DEL
image evidence:

1. `/home/uefi/workspace/gooxie_bios/offline_inspection.tar.xz` contains a
   recipe describing `logs/1405-03-08/coreboot.rom` as an ASRock
   SPC741D8-2L2T/BCM build from 2026-05-28, on branch `edk2-server`, with
   claimed MD5 `bc3e5a5a2d097491ec2b82a8058cac3d`. The ROM itself is not in that
   archive. The associated config selects `asrock/spc741d8`, reports
   `SI_PT`, and has SMMSTORE off. Those findings are not G4DEL-specific.
2. The local offline directory contains
   `uefi/doc/offline_inspection/coreboot.rom`, whose SHA-256 is
   `1b1b93183a61ae2391c8c1000aa6ba5c6417c8db7688e78073be0230685c6870` and
   whose matching config also selects ASRock. The recipe's stated MD5 does
   not match this included file: the included file's MD5 is
   `493a572ef3c85a494ce9b76d0086436f`. This is an artifact/report identity
   mismatch; do not attribute its output to the factory image.
3. `uefi/doc/offline_inspection/coreboot_1405-03-08.rom` is a distinct
   64 MiB file (SHA-256
   `74958133109034dc89b19786749cd4b7cf44eba9780315c90ed7d47a0831703b`). No
   matching report/hash pairing was found for it, so its board identity is
   left unassigned.

The factory `server.bin` is separate from all of the above ROMs. Its current
hash and IFD boundaries are listed at the top of this document.

## Reproduce the image inspection

The inspection is read-only unless `--extract DIR` is supplied. Use the
factory image as input and keep extracted blobs in an ignored local build
directory:

```sh
util/ifdtool/ifdtool -p lbg -d /home/uefi/workspace/gooxie_bios/server.bin
python3 util/g4del_stock.py /home/uefi/workspace/gooxie_bios/server.bin
python3 util/g4del_stock.py /home/uefi/workspace/gooxie_bios/server.bin \
  --extract deps/blobs/mainboard/gooxi/g4del
/tmp/g4del-tools/uefiextract/uefiextract \
  /home/uefi/workspace/gooxie_bios/server.bin report
python3 util/g4del_bios_inventory.py \
  /home/uefi/workspace/gooxie_bios/server.bin \
  /home/uefi/workspace/gooxie_bios/server.bin.report.txt \
  doc/g4del/server_bios_inventory.csv
```

The UEFITool archive used was NE alpha 76, SHA-256
`1bee0a3556d47288a0cfbfb8f08ca791a35826686cf8fb998b5f2cefdc860619`; the
executable hash was `ced816b0dd6ef678ec11c16a605b06c850594c692589531d610ed95f50d24df8`.
UEFIExtract also emitted warnings for unknown ME parsing, JEDEC/VSCC parsing,
unreferenced FIT candidates, and BootGuard Key/Policy Manifest parsing. These
are parser limitations/warnings in the firmware inventory and do not by
themselves establish a boot failure. The FIT/startup-ACM records it did parse
remain in the CSV tree.

Check the new ROM before flashing:

```sh
python3 util/g4del_stock.py /home/uefi/workspace/gooxie_bios/server.bin \
  --compare build/coreboot.rom
python3 util/g4del_stock.py build/coreboot.rom --verify-layout
build/util/cbfstool/cbfstool build/coreboot.rom layout
build/util/cbfstool/cbfstool build/coreboot.rom print
```

The first command is the byte-for-byte check for the four preserved IFD
regions. The second checks all FMAP areas, bounds, SMMSTORE alignment, and
contiguous coverage of the full 64 MiB IFD/FMAP map.

## References

- [Intel Flash Descriptor layout](https://doc.coreboot.org/util/ifdtool/layout.html)
- [coreboot FMD / Flashmap](https://doc.coreboot.org/lib/flashmap.html)
- [SMMSTORE driver](https://doc.coreboot.org/drivers/smmstore.html)
- [UEFITool / UEFIExtract releases](https://github.com/LongSoft/UEFITool/releases)
- [Gooxi SL201-G4 family manual](https://www.gooxi.us/uploadfiles/2024/01/20240122163352960.pdf) — family reference only.
