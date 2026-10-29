/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef GOOXI_G4DEL_VERSION_H
#define GOOXI_G4DEL_VERSION_H

/*
 * Project Manager: Abdol Khaleq Khanduzi
 * Maintainer: Mahdi Sahebi
 * Developer: Amir Hosein Sour Abadi
 */

/* Initial G4DEL coreboot/UEFI diagnostic version. */
#define UEFI_COREBOOT_MAJOR_VER 0
#define UEFI_COREBOOT_MINOR_VER 1
#define UEFI_COREBOOT_MINRO_VER UEFI_COREBOOT_MINOR_VER
#define UEFI_COREBOOT_PATCH_VER 0

/* Evaluated by the compiler for each firmware build (for example, Oct 29 2026). */
#define UEFI_COREBOOT_BUILD_DATE __DATE__

#define UEFI_COREBOOT_STRINGIFY_INNER(value) #value
#define UEFI_COREBOOT_STRINGIFY(value) UEFI_COREBOOT_STRINGIFY_INNER(value)
#define UEFI_COREBOOT_VERSION_STRING \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_MAJOR_VER) "." \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_MINOR_VER) "." \
	UEFI_COREBOOT_STRINGIFY(UEFI_COREBOOT_PATCH_VER)

#endif /* GOOXI_G4DEL_VERSION_H */
