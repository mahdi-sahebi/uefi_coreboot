/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <soc/romstage.h>
#include <sprsp_ac_iio.h>

void mainboard_memory_init_params(FSPM_UPD *mupd)
{
	/* Keep FSP-M diagnostics on the SOL UART used by coreboot and EDK2. */
	mupd->FspmConfig.SerialIoUartDebugEnable = 1;
	mupd->FspmConfig.SerialIoUartDebugIoBase = CONFIG_TTYS0_BASE;

	/* Retain the memory-training settings used by the successful handoff. */
	mupd->FspmConfig.EnableRMT = 0;
	mupd->FspmConfig.AttemptFastBoot = 1;
	mupd->FspmConfig.AttemptFastBootCold = 1;
	mupd->FspmConfig.AdvMemTestOptions = 0;
	mupd->FspmConfig.promoteMrcWarnings = 0;
	mupd->FspmConfig.promoteWarnings = 0;

	/* Provisional two-socket Archer City routing; validate on hardware. */
	printk(BIOS_ERR, "G4DEL: configuring two-socket Archer City IIO table\n");
	soc_config_iio(mupd, ac_iio_pci_port, ac_iio_bifur);
}
