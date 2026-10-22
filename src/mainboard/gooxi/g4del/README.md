# Gooxi G4DEL bring-up

Select `Gooxi / G4DEL` in `make menuconfig`. This target uses a 64 MiB image,
two Sapphire Rapids sockets, the AST SOL UART at `0x2f8`, and a 256 KiB
SMMSTORE v2 region. The PCIe IIO table is the Archer City dual-socket table
until G4DEL routing is verified on hardware. Empty DIMM slots are not reported
because the physical slot count is unknown; populated DIMMs come from FSP.

The factory image is a local build input. Inspect its descriptor before
extracting anything:

```sh
util/ifdtool/ifdtool -p lbg -d /media/sf_shared/server.bin
python3 util/g4del_stock.py /media/sf_shared/server.bin
python3 util/g4del_stock.py /media/sf_shared/server.bin \
  --extract deps/blobs/mainboard/gooxi/g4del
```

The extractor writes `descriptor.bin`, `gbe.bin`, `me.bin` and
`device_exp1.bin`. Configure their paths in menuconfig if another directory
is used. These files are local and must not be committed. The stock BIOS
region is intentionally excluded; coreboot and EDK2 replace it. Keep
`DO_NOT_TOUCH_DESCRIPTOR_REGION` selected, with descriptor unlocking and ME
disabling off.

After building, verify the four factory regions and inspect the layout:

```sh
python3 util/g4del_stock.py /media/sf_shared/server.bin \
  --compare build/coreboot.rom
util/ifdtool/ifdtool -p lbg -d build/coreboot.rom
build/cbfstool build/coreboot.rom layout
build/cbfstool build/coreboot.rom print
```

The existing SOL log predates FTW delay removal. The first board test still
needs to confirm PEI and DXE return, menu input, and Shell interaction.

The old coreboot messages are separate from the PEI stop: missing optional
bootsplash, `SF size 0x8000000` versus the configured 64 MiB, socket and
ACPI/VT-d assertions, and missing TPM. Coreboot still handed off to EDK2.
The new socket setting addresses one known mismatch; a new reset-to-menu
SOL capture must resolve the others.
