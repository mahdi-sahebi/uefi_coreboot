/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootblock_common.h>
#include <console/console.h>
#include <device/pci_ops.h>
#include <intelblocks/lpc_lib.h>
#include <intelblocks/pcr.h>
#include <soc/intel/common/block/lpc/lpc_def.h>
#include <soc/pci_devs.h>
#include <soc/pcr_ids.h>
#include <superio/aspeed/ast2400/ast2400.h>
#include <superio/aspeed/common/aspeed.h>

#include "version.h"

#define PCR_DMI_LPCIOD	0x2770
#define PCR_DMI_LPCIOE	0x2774
#define ASPEED_SIO_PORT	0x2e

void bootblock_mainboard_early_init(void)
{
	/*
	 * Set up decoding windows on PCH over PCR. The AST2600 SuperIO config
	 * port is 0x2e on the previously booting SOL path.
	 */
	const uint16_t lpciod = LPC_IOD_COMB_RANGE | LPC_IOD_COMA_RANGE;
	const uint16_t lpcioe = LPC_IOE_SUPERIO_2E_2F | LPC_IOE_COMB_EN | LPC_IOE_COMA_EN;

	/* Open I/O windows for COM1 (0x3f8) and COM2 (0x2f8). */
	pcr_or32(PID_DMI, PCR_DMI_LPCIOD, lpciod);
	/* Enable COM1, COM2, and SuperIO at 0x2e/0x2f. */
	pcr_or32(PID_DMI, PCR_DMI_LPCIOE, lpcioe);

	pci_write_config16(PCH_DEV_LPC, LPC_IO_DECODE, lpciod);
	pci_write_config16(PCH_DEV_LPC, LPC_IO_ENABLES, lpcioe);
	pci_write_config16(PCH_DEV_LPC, ESPI_CS1_ENABLE, lpcioe);

	/* The board's captured working SOL setup used SUART1 at 0x2f8. */
	const pnp_devfn_t ast_serial_dev = PNP_DEV(ASPEED_SIO_PORT, AST2400_SUART1);
	aspeed_enable_serial(ast_serial_dev, CONFIG_TTYS0_BASE);
}

void bootblock_mainboard_init(void)
{
	printk(BIOS_NOTICE, "[COREBOOT] UEFI_COREBOOT_VERSION=%s BUILD_DATE=%s\n",
		UEFI_COREBOOT_VERSION_STRING, UEFI_COREBOOT_BUILD_DATE);
}
