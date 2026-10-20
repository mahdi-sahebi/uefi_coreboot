// /* SPDX-License-Identifier: GPL-2.0-only */

// #include <bootblock_common.h>
// #include <device/pci_ops.h>
// #include <intelblocks/lpc_lib.h>
// #include <intelblocks/pcr.h>
// #include <soc/intel/common/block/lpc/lpc_def.h>
// #include <soc/pci_devs.h>
// #include <soc/pcr_ids.h>
// #include <superio/aspeed/ast2400/ast2400.h>
// #include <superio/aspeed/common/aspeed.h>
// #include <superio/nuvoton/common/nuvoton.h>
// #include <superio/nuvoton/nct6791d/nct6791d.h>
// #include <device/pnp_ops.h>
// #include <arch/io.h>

// #define PCR_DMI_LPCIOD	0x2770
// #define PCR_DMI_LPCIOE	0x2774

// /*
//  * Minimal raw 8250 UART byte-send, independent of coreboot's console
//  * driver (which only ever targets CONFIG_TTYS0_BASE / SUART1). Used here
//  * to put a directly-verifiable test string out over SUART2 (0x2f8), which
//  * is the port wired to SOL, so we can confirm at the byte level whether
//  * anything is actually reaching the AST2600 - no console plumbing
//  * involved.
//  */
// static void raw_uart_tx_string(uint16_t base, const char *str)
// {
// 	while (*str) {
// 		/* Wait until transmit holding register is empty (LSR bit 5) */
// 		int timeout = 100000;
// 		while (!(inb(base + 5) & 0x20) && timeout--)
// 			;
// 		outb(*str++, base);
// 	}
// }

// /*
//  * On use_espi=1 boards, the AST2600's eSPI slave logic requires writing 0
//  * to the IRQ-level register (0x71) for a logical device before that
//  * device's SuperIO config-port accesses are reliably routed over eSPI.
//  * ast2400/superio.c's enable_dev() applies this during normal ramstage
//  * device enumeration, but bootblock_mainboard_early_init() drives SIO
//  * devices directly via raw pnp_*() calls - bypassing enable_dev() - for
//  * LDN_ILPC2AHB (used internally by aspeed_enable_serial()/
//  * aspeed_enable_uart_pin() to reach SCU registers) as well as for
//  * LDN_SUART1/LDN_SUART2 themselves. Without this, those SIO accesses can
//  * be silently dropped by the eSPI slave, so nothing actually reaches the
//  * chip despite the C code being logically correct.
//  */
// static void aspeed_espi_clear_irq_level(pnp_devfn_t dev)
// {
// 	pnp_enter_conf_state(dev);
// 	pnp_set_logical_device(dev);
// 	pnp_write_config(dev, 0x71, 0);
// 	pnp_exit_conf_state(dev);
// }

// void bootblock_mainboard_early_init(void)
// {
// 	/*
// 	 * Set up decoding windows on PCH over PCR. The CPU uses two of AST2600 SIO ports,
// 	 * one is connected to debug header (SUART1) and another is used as SOL (SUART2).
// 	 */
// 	const uint16_t lpciod = (LPC_IOD_COMB_RANGE | LPC_IOD_COMA_RANGE);
// 	const uint16_t lpcioe = (LPC_IOE_EC_4E_4F | LPC_IOE_COMB_EN | LPC_IOE_COMA_EN);

// 	/* Open IO windows: 0x3f8 for com1 and 02f8 for com2 */
// 	pcr_or32(PID_DMI, PCR_DMI_LPCIOD, lpciod);
// 	/* LPC I/O enable: com1 and com2 */
// 	pcr_or32(PID_DMI, PCR_DMI_LPCIOE, lpcioe);

// 	/* Enable com1 (0x3f8), com2 (0x2f8) and superio (0x4e) */
// 	pci_write_config16(PCH_DEV_LPC, LPC_IO_DECODE, lpciod);
// 	pci_write_config16(PCH_DEV_LPC, LPC_IO_ENABLES, lpcioe);
// 	/* AST2600 is on eSPI CS1# */
// 	pci_write_config16(PCH_DEV_LPC, ESPI_CS1_ENABLE, lpcioe);

// 	/*
// 	 * Disable the Nuvoton NCT6791D SuperIO UART1.  It is enabled by
// 	 * default, but the AST2600's is connected to the serial port.
// 	 */
// 	const pnp_devfn_t nvt_serial_dev = PNP_DEV(0x2E, NCT6791D_SP1);
// 	nuvoton_pnp_enter_conf_state(nvt_serial_dev);
// 	pnp_set_logical_device(nvt_serial_dev);
// 	pnp_set_enable(nvt_serial_dev, 0);
// 	nuvoton_pnp_exit_conf_state(nvt_serial_dev);

// 	/*
// 	 * use_espi=1 workaround (see comment above aspeed_espi_clear_irq_level):
// 	 * clear IRQ level for the internal LPC-to-AHB bridge LDN before it is
// 	 * used indirectly by aspeed_enable_serial()/aspeed_enable_uart_pin()
// 	 * to reach SCU registers, and for SUART1/SUART2 themselves before
// 	 * their own config-port accesses.
// 	 */
// 	aspeed_espi_clear_irq_level(PNP_DEV(0x4E, LDN_ILPC2AHB));
// 	aspeed_espi_clear_irq_level(PNP_DEV(0x4E, AST2400_SUART1));
// 	aspeed_espi_clear_irq_level(PNP_DEV(0x4E, AST2400_SUART2));

// 	/* Enable AST2600 SuperIO UART1 */
// 	const pnp_devfn_t ast_serial_dev = PNP_DEV(0x4E, AST2400_SUART1);
// 	aspeed_enable_serial(ast_serial_dev, CONFIG_TTYS0_BASE);




// 	// SOL 
// 	aspeed_enable_uart_pin(ast_serial_dev);

// 	const pnp_devfn_t ast_sol_dev = PNP_DEV(0x4E, AST2400_SUART2);
// 	aspeed_enable_serial(ast_sol_dev, 0x2f8);
// 	aspeed_enable_uart_pin(ast_sol_dev);

// 	/*
// 	 * Raw SOL test print - bypasses coreboot's console driver entirely.
// 	 * Sent repeatedly (with a small delay loop between) in case the SOL
// 	 * terminal on the BMC side connects a moment late.
// 	 */
// 	{
// 		int rep;
// 		for (rep = 0; rep < 5; rep++) {
// 			raw_uart_tx_string(0x2f8, "\r\n*** SOL TEST - Gooxi G4DEL - hello from bootblock ***\r\n");
// 			int delay = 2000000;
// 			while (delay--)
// 				;
// 		}
// 	}
// }

// // /* SPDX-License-Identifier: GPL-2.0-only */

// // #include <bootblock_common.h>
// // #include <device/pci_ops.h>
// // #include <intelblocks/lpc_lib.h>
// // #include <intelblocks/pcr.h>
// // #include <soc/intel/common/block/lpc/lpc_def.h>
// // #include <soc/pci_devs.h>
// // #include <soc/pcr_ids.h>
// // #include <superio/aspeed/ast2400/ast2400.h>
// // #include <superio/aspeed/common/aspeed.h>
// // #include <superio/nuvoton/common/nuvoton.h>
// // #include <superio/nuvoton/nct6791d/nct6791d.h>
// // #include <device/pnp_ops.h>

// // #define PCR_DMI_LPCIOD	0x2770
// // #define PCR_DMI_LPCIOE	0x2774

// // void bootblock_mainboard_early_init(void)
// // {
// // 	/*
// // 	 * Set up decoding windows on PCH over PCR. The CPU uses two of AST2600 SIO ports,
// // 	 * one is connected to debug header (SUART1) and another is used as SOL (SUART2).
// // 	 */
// // 	const uint16_t lpciod = (LPC_IOD_COMB_RANGE | LPC_IOD_COMA_RANGE);
// // 	const uint16_t lpcioe = (LPC_IOE_EC_4E_4F | LPC_IOE_COMB_EN | LPC_IOE_COMA_EN);

// // 	/* Open IO windows: 0x3f8 for com1 and 02f8 for com2 */
// // 	pcr_or32(PID_DMI, PCR_DMI_LPCIOD, lpciod);
// // 	/* LPC I/O enable: com1 and com2 */
// // 	pcr_or32(PID_DMI, PCR_DMI_LPCIOE, lpcioe);

// // 	/* Enable com1 (0x3f8), com2 (0x2f8) and superio (0x4e) */
// // 	pci_write_config16(PCH_DEV_LPC, LPC_IO_DECODE, lpciod);
// // 	pci_write_config16(PCH_DEV_LPC, LPC_IO_ENABLES, lpcioe);
// // 	/* AST2600 is on eSPI CS1# */
// // 	pci_write_config16(PCH_DEV_LPC, ESPI_CS1_ENABLE, lpcioe);

// // 	/*
// // 	 * Disable the Nuvoton NCT6791D SuperIO UART1.  It is enabled by
// // 	 * default, but the AST2600's is connected to the serial port.
// // 	 */
// // 	const pnp_devfn_t nvt_serial_dev = PNP_DEV(0x2E, NCT6791D_SP1);
// // 	nuvoton_pnp_enter_conf_state(nvt_serial_dev);
// // 	pnp_set_logical_device(nvt_serial_dev);
// // 	pnp_set_enable(nvt_serial_dev, 0);
// // 	nuvoton_pnp_exit_conf_state(nvt_serial_dev);

// // 	/* Enable AST2600 SuperIO UART1 */
// // 	const pnp_devfn_t ast_serial_dev = PNP_DEV(0x4E, AST2400_SUART1);
// // 	aspeed_enable_serial(ast_serial_dev, CONFIG_TTYS0_BASE);




// // 	// SOL 
// // 	aspeed_enable_uart_pin(ast_serial_dev);

// // 	const pnp_devfn_t ast_sol_dev = PNP_DEV(0x4E, AST2400_SUART2);
// // 	aspeed_enable_serial(ast_sol_dev, 0x2f8);
// // 	aspeed_enable_uart_pin(ast_sol_dev);


// // }






//////////////////
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
	

	/* Route coreboot's console to AST SUART2, the documented G4DEL SOL port. */
	const pnp_devfn_t ast_sol_dev = PNP_DEV(ASPEED_SIO_PORT, AST2400_SUART2);
	aspeed_enable_serial(ast_sol_dev, CONFIG_TTYS0_BASE);
	aspeed_enable_uart_pin(ast_sol_dev);
}
