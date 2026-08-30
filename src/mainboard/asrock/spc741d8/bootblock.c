/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootblock_common.h>
#include <device/pci_ops.h>
#include <intelblocks/lpc_lib.h>
#include <intelblocks/pcr.h>
#include <soc/intel/common/block/lpc/lpc_def.h>
#include <soc/pci_devs.h>
#include <soc/pcr_ids.h>
#include <superio/aspeed/ast2400/ast2400.h>
#include <superio/aspeed/common/aspeed.h>
#include <superio/nuvoton/common/nuvoton.h>
#include <superio/nuvoton/nct6791d/nct6791d.h>
#include <device/pnp_ops.h>
#define PCR_DMI_LPCIOD	0x2770
#define PCR_DMI_LPCIOE	0x2774
#define ASPEED_SIO_PORT 0x2E

void bootblock_mainboard_early_init(void)
{
	/*
	 * Set up decoding windows on PCH over PCR.
	 * Corrected: AST2600 SuperIO config port is 0x2E on this board
	 * (confirmed via superiotool under vendor BIOS), not 0x4E.
	 */
	const uint16_t lpciod = (LPC_IOD_COMB_RANGE | LPC_IOD_COMA_RANGE);
	const uint16_t lpcioe = (LPC_IOE_SUPERIO_2E_2F | LPC_IOE_COMB_EN | LPC_IOE_COMA_EN);

	/* Open IO windows: 0x3f8 for com1 and 0x2f8 for com2 */
	pcr_or32(PID_DMI, PCR_DMI_LPCIOD, lpciod);
	/* LPC I/O enable: com1, com2, and superio at 0x2e/0x2f */
	pcr_or32(PID_DMI, PCR_DMI_LPCIOE, lpcioe);

	pci_write_config16(PCH_DEV_LPC, LPC_IO_DECODE, lpciod);
	pci_write_config16(PCH_DEV_LPC, LPC_IO_ENABLES, lpcioe);
	pci_write_config16(PCH_DEV_LPC, ESPI_CS1_ENABLE, lpcioe);
	/* Removed: ESPI_CS1_ENABLE write — not present in Intel's reference,
	 * no documented bit definition exists, likely unnecessary/incorrect. */

	/* Removed: Nuvoton NCT6791D disable block — superiotool found no
	 * such chip on this board, so this was a no-op inherited from ASRock. */
	/*
	 * Disable the Nuvoton NCT6791D SuperIO UART1.  It is enabled by
	 * default, but the AST2600's is connected to the serial port.
	 */
	

	/* Enable AST2600 SuperIO UART1 at the correct config port */
	const pnp_devfn_t ast_serial_dev = PNP_DEV(ASPEED_SIO_PORT, AST2400_SUART1);
	aspeed_enable_serial(ast_serial_dev, CONFIG_TTYS0_BASE);
}

