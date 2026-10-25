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

## Linear graphics and EDK2 logo

The G4DEL defconfig selects the AST linear framebuffer for EDK2. Coreboot
reports the framebuffer through `LB_TAG_FRAMEBUFFER`; Dasharo's EDK2 graphics
PEIM converts that record into a graphics HOB, and `GraphicsOutputDxe` uses it
to install GOP. The coreboot build also adds the open-source
`Documentation/coreboot_logo.bmp` to the `BOOTSPLASH` FMAP region as CBFS file
`logo.bmp`. At CBMEM ready, coreboot copies that bitmap into CBMEM and exports
the `CB_TAG_LOGO` reference that EDK2's `LogoDxe` reads.

The logo region is not the same as coreboot's native `BOOTSPLASH_IMAGE`
feature: EDK2 displays this image after its graphics and console drivers are
ready. The image is a generic coreboot logo, not Gooxi factory branding.

The AST driver falls back to 1024×768 when a server has no readable EDID. Its
EDID buffer is now initialized before the read, and EDID/framebuffer status is
logged on SOL. VGA text breadcrumbs are disabled in linear mode so no code
writes to the legacy `0xB8000` text aperture after graphics setup. SOL remains
the reliable early diagnostic channel.

To build from the checked-in G4DEL defconfig and verify the logo region:

```sh
make defconfig KBUILD_DEFCONFIG=configs/config.gooxi_g4del
make -j8
build/util/cbfstool/cbfstool build/coreboot.rom print -r BOOTSPLASH
build/util/cbfstool/cbfstool build/coreboot.rom layout -w
```

The current source review also fixed a memory overwrite in EDK2's `LogoDxe`:
the BMP decoder returns native-width dimensions, while `EFI_IMAGE_INPUT`
stores 16-bit dimensions. The driver now receives dimensions into local
`UINTN` values, checks their range, initializes the image flags, and assigns
the protocol fields safely.

Static build and package checks do not prove that the board displays a splash
or menu. The latest hardware capture still ends in `CpuDxe`; the next capture
must show the CPU driver return before BDS, GOP, menu, and Shell can be
evaluated. See the EDK2 document
`doc/1405-08-02/graphics_splash_shell.md` for the source path, log markers,
reference-image findings, and remaining acceptance checks.
