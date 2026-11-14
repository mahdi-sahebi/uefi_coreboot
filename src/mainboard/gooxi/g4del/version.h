/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef GOOXI_G4DEL_VERSION_H
#define GOOXI_G4DEL_VERSION_H

#include <build.h>

/*
 * Project Manager: Abdol Khaleq Khanduzi
 * Maintainer: Mahdi Sahebi
 * Developer: Amir Hosein Sour Abadi
 */

/* Initial G4DEL coreboot/UEFI diagnostic version. */
#define MC_VERSION_MAJOR 1
#define MC_VERSION_MINOR 0
#define MC_VERSION_PATCH 0

#define UEFI_COREBOOT_MAJOR_VER MC_VERSION_MAJOR
#define UEFI_COREBOOT_MINOR_VER MC_VERSION_MINOR
#define UEFI_COREBOOT_MINRO_VER UEFI_COREBOOT_MINOR_VER
#define UEFI_COREBOOT_PATCH_VER MC_VERSION_PATCH

/* Use coreboot's reproducible timestamp for the board firmware banner. */
#define UEFI_COREBOOT_BUILD_DATE COREBOOT_BUILD

#define UEFI_COREBOOT_STRINGIFY_INNER(value) #value
#define UEFI_COREBOOT_STRINGIFY(value) UEFI_COREBOOT_STRINGIFY_INNER(value)
#define UEFI_COREBOOT_VERSION_STRING \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_MAJOR_VER) "." \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_MINOR_VER) "." \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_PATCH_VER)

#endif /* GOOXI_G4DEL_VERSION_H */
