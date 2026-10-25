/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootstate.h>
#include <console/console.h>
#include <commonlib/helpers.h>
#include <device/device.h>
#include <device/pci_ids.h>
#include <cpu/x86/smm.h>
#include <fmap.h>
#include <pc80/vga.h>
#include <stdio.h>
#include <string.h>
#include <soc/ramstage.h>
#include <soc/smmrelocate.h>
#include <device/i2c_simple.h>

/* AST EDID uses its own GPIO bit-banged I2C implementation. Provide the
 * platform fallback required by the shared I2C API; no hardware I2C bus is
 * configured for this VGA path. */
int platform_i2c_transfer(unsigned int bus, struct i2c_msg *segments, int count)
{
	(void)bus;
	(void)segments;
	(void)count;
	return -1;
}

void smm_mainboard_pci_resource_store_init(struct smm_pci_resource_info *slots, size_t size)
{
	soc_ubox_store_resources(slots, size);
}

/* Log effective build options and the on-flash FMAP before loading EDK2. */
static void g4del_payload_diagnostics(void *unused)
{
	static const char *const names[] = {
		"SI_DESC", "SI_GBE", "SI_ME", "SI_DEVICEEXT", "SI_BIOS",
		"SMMSTORE", "BOOTSPLASH", "FMAP", "COREBOOT",
	};
	struct region area;
	struct device *dev;
	char line[80];
	size_t i;

	printk(BIOS_ERR, "[GX-CONFIG] board=%s sockets=%u rom=0x%x uart=0x%x baud=%u\n",
	       CONFIG_MAINBOARD_DIR, CONFIG_MAX_SOCKET, CONFIG_ROM_SIZE,
	       CONFIG_TTYS0_BASE, CONFIG_TTYS0_BAUD);
	printk(BIOS_ERR, "[GX-CONFIG] smmstore_v2=%u size=0x%x native_vga=%u text=%u linear=%u generic_fb=%u edk2_debug=%u shell=%u serial=%u ref=%s\n",
	       CONFIG(SMMSTORE_V2), CONFIG_SMMSTORE_SIZE,
	       CONFIG(MAINBOARD_DO_NATIVE_VGA_INIT), CONFIG(VGA_TEXT_FRAMEBUFFER),
	       CONFIG(LINEAR_FRAMEBUFFER), CONFIG(GENERIC_LINEAR_FRAMEBUFFER),
	       CONFIG(EDK2_DEBUG), CONFIG(EDK2_HAVE_EFI_SHELL), CONFIG(EDK2_SERIAL_SUPPORT),
	       CONFIG_EDK2_TAG_OR_REV);
	printk(BIOS_ERR, "[GX-CONFIG] EDK2 logo='%s' CBMEM logo='%s'\n",
	       CONFIG_EDK2_BOOTSPLASH_FILE, CONFIG_BOOTSPLASH_REGION_LOGO_FILE);
	for (dev = all_devices; dev; dev = dev->next) {
		if (dev->vendor == PCI_VID_ASPEED)
			printk(BIOS_ERR, "[GX-GFX] ASPEED PCI function vendor=%04x device=%04x class=%06x enabled=%u initialized=%u\n",
			       dev->vendor, dev->device, dev->class, dev->enabled, dev->initialized);
	}
	if (CONFIG(VGA_TEXT_FRAMEBUFFER))
		vga_line_write(12, "G4DEL payload config and flash map: see SOL [GX-CONFIG]/[GX-FMAP]");
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		if (fmap_locate_area(names[i], &area)) {
			printk(BIOS_ERR, "[GX-FMAP] %s lookup FAILED\n", names[i]);
			snprintf(line, sizeof(line), "%s FMAP lookup FAILED", names[i]);
		} else {
			printk(BIOS_ERR, "[GX-FMAP] %s offset=0x%zx size=0x%zx\n",
			       names[i], region_offset(&area), region_sz(&area));
			snprintf(line, sizeof(line), "%s off=%zx size=%zx",
				 names[i], region_offset(&area), region_sz(&area));
			if (region_offset(&area) > CONFIG_ROM_SIZE ||
			    region_sz(&area) > CONFIG_ROM_SIZE - region_offset(&area))
				printk(BIOS_ERR, "[GX-FMAP] %s extends outside configured ROM\n", names[i]);
			if (!strcmp(names[i], "SMMSTORE") && region_sz(&area) != CONFIG_SMMSTORE_SIZE)
				printk(BIOS_ERR, "[GX-FMAP] SMMSTORE size differs from Kconfig\n");
		}
		if (CONFIG(VGA_TEXT_FRAMEBUFFER))
			vga_line_write(14 + i, line);
	}
}

BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_ENTRY, g4del_payload_diagnostics, NULL);
