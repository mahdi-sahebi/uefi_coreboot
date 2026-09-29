/* SPDX-License-Identifier: GPL-2.0-only */

#include <cbfs.h>
#include <cbmem.h>
#include <console/console.h>
#include <fallback.h>
#include <halt.h>
#include <lib.h>
#include <program_loading.h>
#include <reset.h>
#include <rmodule.h>
#include <romstage_common.h>
#include <security/vboot/vboot_common.h>
#include <stage_cache.h>
#include <symbols.h>
#include <timestamp.h>

////////////// mn

#include <fsp/util.h>                 /* fsp_get_hob_list() */
// #include <vendorcode/intel/fsp/fsp2_0/include/FspHob.h>   /* EFI HOB types */




#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <mode_switch.h>
#include <program_loading.h>
#include <symbols.h>
#include <assert.h>
#include <pc80/vga.h>
#include <stdio.h> 

void hex_dump(const unsigned char *addr, size_t len);
void vga_hex_dump(const unsigned char *addr, size_t len, int start_row);

void hex_dump(const unsigned char *addr, size_t len) {
    size_t i;
    unsigned char buff[17];  // For ASCII side
    const unsigned char *pc = addr;

    for (i = 0; i < len; i++) {
        // Print offset at start of line (every 16 bytes)
        if ((i % 16) == 0) {
            if (i != 0) {
                printk(BIOS_INFO, "  %s\n", buff);  // Print previous ASCII
            }
            printk(BIOS_INFO, "%08lx: ", (unsigned long)(addr + i));  // Offset
        }

        // Hex value
        printk(BIOS_INFO, " %02x", pc[i]);

        // ASCII (printable or '.')
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    // Pad last line if not full
    while ((i % 16) != 0) {
        printk(BIOS_INFO, "   ");
        i++;
    }

    printk(BIOS_INFO, "  %s\n", buff);  // Final ASCII
}

void vga_hex_dump(const unsigned char *addr, size_t len, int start_row) {
	size_t line_start_i;
	int row = start_row;
	char line_buf[128];

	for (line_start_i = 0; line_start_i < len; line_start_i += 16) {
		unsigned char buff[17];
		int buf_pos = 0;
		size_t j;
		// Offset
		buf_pos = snprintf(line_buf, sizeof(line_buf), "%08lx: ", (unsigned long)(addr + line_start_i));
		// Hex and ASCII collection
		for (j = 0; j < 16 && (line_start_i + j < len); j++) {
			unsigned char byte = addr[line_start_i + j];
			buf_pos += snprintf(line_buf + buf_pos, sizeof(line_buf) - buf_pos, "%02x ", byte);
			buff[j] = (byte >= 0x20 && byte <= 0x7e) ? byte : '.';
		}
		// Pad hex
		for (; j < 16; j++) {
			buf_pos += snprintf(line_buf + buf_pos, sizeof(line_buf) - buf_pos, "   ");
		}
		// Terminate ASCII
		buff[j] = '\0';
		// Add ASCII
		snprintf(line_buf + buf_pos, sizeof(line_buf) - buf_pos, "  %s", buff);
		// Write to VGA
		vga_line_write(row++, line_buf);
	}
}





/////////////////////////////////////////////////////
#include <string.h>
#include <stdarg.h> 

#define VGA_CB_FB 0xB8000
#define VGA_CB_COLUMNS 80

char vga_cb_g_buffer[80];
void vga_cb_write_at_offset(unsigned int line, unsigned int offset, const char *string);
void vga_cb_print(unsigned int line, const char *string);
void vga_cb_sprintf(unsigned int row, const char* format, ...);
void vga_cb_clear(void);
void vga_cb_hex_dump(const unsigned char *addr, unsigned int len, int start_row);

void vga_cb_write_at_offset(unsigned int line, unsigned int offset, const char *string)
{
	unsigned int i;
	size_t len;
	unsigned short *p;

	if (!string || line >= 25 || offset >= VGA_CB_COLUMNS)
		return;

	p = (unsigned short *)VGA_CB_FB + (VGA_CB_COLUMNS * line) + offset;
	len = strlen(string);
	if (len > VGA_CB_COLUMNS - offset)
		len = VGA_CB_COLUMNS - offset;

	for (i = 0; i < len; i++) {
		p[i] = 0x0F00 | (unsigned char)string[i];
	}
}


void vga_cb_print(unsigned int line, const char *string)
{
	vga_cb_write_at_offset(line, 0, string);
}

void vga_cb_sprintf(
  unsigned int row,
  const char* format,
  ...)
{
  va_list  marker;
  
  va_start (marker, format);
  vsnprintf(vga_cb_g_buffer, sizeof(vga_cb_g_buffer), format, marker);
  va_end (marker);
  
  vga_cb_print (row, vga_cb_g_buffer);
}

void vga_cb_clear(void)
{
  /* Preserve breadcrumbs from earlier boot phases. */
}

void vga_cb_hex_dump(const unsigned char *addr, unsigned int len, int start_row) 
{
    unsigned int i;
    
    for (i = 0; i < len; i += 16) {
        unsigned int j;
        int row = start_row + (i / 16);
        int offset_pos = 0;
        
        // Write offset character by character
        unsigned long ptr_val = (unsigned long)(addr + i);
        for (j = 28; j > 0; j -= 4) {
            char nibble = (ptr_val >> j) & 0x0F;
            char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            char buf[2] = {c, '\0'};
            vga_cb_write_at_offset(row, offset_pos++, buf);
        }
        {
            char last_nibble = ptr_val & 0x0F;
            char c = (last_nibble < 10) ? ('0' + last_nibble) : ('A' + last_nibble - 10);
            char buf[2] = {c, '\0'};
            vga_cb_write_at_offset(row, offset_pos++, buf);
        }
        vga_cb_write_at_offset(row, offset_pos++, ": ");
        
        // Write hex bytes
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            // Write high nibble
            char high = (byte >> 4) & 0x0F;
            char c1 = (high < 10) ? ('0' + high) : ('A' + high - 10);
            char buf1[2] = {c1, '\0'};
            vga_cb_write_at_offset(row, offset_pos++, buf1);
            // Write low nibble
            char low = byte & 0x0F;
            char c2 = (low < 10) ? ('0' + low) : ('A' + low - 10);
            char buf2[2] = {c2, '\0'};
            vga_cb_write_at_offset(row, offset_pos++, buf2);
            // Write space
            vga_cb_write_at_offset(row, offset_pos++, " ");
        }
        
        // Pad remaining hex spaces
        for (; j < 16; j++) {
            vga_cb_write_at_offset(row, offset_pos++, "   ");
        }
        
        // Write ASCII representation
        vga_cb_write_at_offset(row, offset_pos++, "  ");
        
        for (j = 0; j < 16 && (i + j < len); j++) {
            unsigned char byte = addr[i + j];
            char c = (byte >= 0x20 && byte <= 0x7e) ? (char)byte : '.';
            char buf[2] = {c, '\0'};
            vga_cb_write_at_offset(row, offset_pos + j, buf);
        }
    }
}



/////////////////////////////////////////////////////


#if ENV_PAYLOAD_LOADER	/* only used on the payload path (run_ramstage env) */
/*
 * Comprehensive EDK II / PI HOB list verification and logging.
 *
 * This uses coreboot's own canonical HOB model (struct hob_header / enum
 * hob_type / struct hob_resource from <fsp/util.h>, already included above)
 * rather than the EDK II MdePkg headers, so every type and constant is
 * guaranteed to be in scope and consistent with the rest of the FSP driver.
 *
 * The PI handoff (PHIT) and firmware-volume HOB bodies are not declared in
 * <fsp/util.h>, so we declare the minimum here, matching the PI spec layout.
 * Note the PI "generic header" is 8 bytes: HobType:16, HobLength:16,
 * Reserved:32 -- coreboot's struct hob_header only models the first 4 bytes
 * and the rest of the driver advances past the body using HOB_HEADER_LEN(8).
 */
#define HOB_GENERIC_HEADER_LEN	8	/* type(2) + length(2) + reserved(4) */

// /* EFI_HOB_HANDOFF_INFO_TABLE (PHIT) -- spec length is 0x38 (56) bytes. */
// struct hob_handoff_info_table {
// 	uint16_t type;
// 	uint16_t length;
// 	uint32_t reserved;
// 	uint32_t version;
// 	uint32_t boot_mode;
// 	uint64_t memory_top;
// 	uint64_t memory_bottom;
// 	uint64_t free_memory_top;
// 	uint64_t free_memory_bottom;
// 	uint64_t end_of_hob_list;
// } __packed;

// /* EFI_HOB_FIRMWARE_VOLUME / FV2 / FV3 all start with base + length. */
// struct hob_firmware_volume {
// 	uint16_t type;
// 	uint16_t length;
// 	uint32_t reserved;
// 	uint64_t base_address;
// 	uint64_t fv_length;
// } __packed;

// #define HOB_TYPE_FV3	0x000C	/* not present in enum hob_type */

// static const char *hob_type_name(uint16_t type)
// {
// 	switch (type) {
// 	case HOB_TYPE_HANDOFF:			return "HANDOFF";
// 	case HOB_TYPE_MEMORY_ALLOCATION:	return "MEM_ALLOC";
// 	case HOB_TYPE_RESOURCE_DESCRIPTOR:	return "RESOURCE";
// 	case HOB_TYPE_GUID_EXTENSION:		return "GUID_EXT";
// 	case HOB_TYPE_FV:			return "FV";
// 	case HOB_TYPE_CPU:			return "CPU";
// 	case HOB_TYPE_MEMORY_POOL:		return "MEM_POOL";
// 	case HOB_TYPE_FV2:			return "FV2";
// 	case HOB_TYPE_FV3:			return "FV3";
// 	case HOB_TYPE_END_OF_HOB_LIST:		return "END_OF_LIST";
// 	default:				return "UNKNOWN";
// 	}
// }

// static const char *hob_resource_type_name(uint32_t type)
// {
// 	switch (type) {
// 	case 0:  return "SYSTEM_MEMORY";
// 	case 1:  return "MMIO";
// 	case 2:  return "IO";
// 	case 3:  return "FIRMWARE_DEVICE";
// 	case 4:  return "MMIO_PORT";
// 	case 5:  return "MEMORY_RESERVED";
// 	case 6:  return "IO_RESERVED";
// 	default: return "?";
// 	}
// }

// static void verify_edkii_hob(void *hob_ptr, int start_row)
// {
// 	int row = start_row;
//     vga_cb_sprintf(row, "HOB: %X,",
//         hob_ptr
//     );


// 	/* PI HOBs are required to be 8-byte aligned. */
// 	if ((uintptr_t)hob_ptr & 0x7)
// 		printk(BIOS_WARNING, "HOB: WARN - pointer %p is not 8-byte aligned\n",
// 		       hob_ptr);

// 	/* 1. The very first HOB must be the Handoff Info Table (PHIT). */
// 	struct hob_handoff_info_table *phit = hob_ptr;

//     vga_cb_sprintf(row, "HOB: %X, Type:%X, MT:%X, MB:%X",// HOB: 
//         phit,// 63900000
//         phit->type,// Type:1
//         phit->memory_top,// MT:777fd000
//         phit->memory_bottom// MB:0
//     );
//     row++;
//     vga_cb_sprintf(row, "FT:%X, FB:%X, EL:%X, V:%X, BM:%X, L:%X",// FT:
//         phit->free_memory_top,// 766b6000, 
//         phit->free_memory_bottom,// 0
//         phit->end_of_hob_list,// 63a685f8
//         phit->version,// 0
//         phit->boot_mode,// 63a685f0
//         phit->length// 0
//     );

//     row++;
//     vga_hex_dump((const unsigned char *)phit, 64, row);// Hex dump:
// 	/*
// 	63900000: 01 00 38 00 00 00 00 00 09 00 00 00 00 00 00 00
// 	63900010: 00 d0 7f 77 00 00 00 00 00 00 80 63 00 00 00 00
// 	63900020: 00 60 6b 76 00 00 00 00 fb 85 a6 63 00 00 00 00
// 	63900030: f0 85 a6 63 00 00 00 00 04 00 10 1d 00 00 00 00
// 	*/

//     row +=5;
// 	if (phit->type != HOB_TYPE_HANDOFF) {
// 		// hex_dump((const unsigned char *)hob_ptr, 64);
// 		return;
// 	}

// 	// /* 2. PHIT consistency checks. */
// 	// bool phit_ok = true;
// 	// if (phit->version == 0) {
// 	// 	printk(BIOS_ERR, "HOB: ERR - PHIT version is 0\n");
// 	// 	phit_ok = false;
// 	// }
// 	// if (mem_bot > mem_top) {
// 	// 	printk(BIOS_ERR, "HOB: ERR - MemBottom > MemTop\n");
// 	// 	phit_ok = false;
// 	// }
// 	// if (free_bot > free_top) {
// 	// 	printk(BIOS_ERR, "HOB: ERR - FreeBottom > FreeTop\n");
// 	// 	phit_ok = false;
// 	// }
// 	// if (end <= (uintptr_t)hob_ptr) {
// 	// 	printk(BIOS_ERR, "HOB: ERR - EndOfHobList 0x%llx <= list start %p\n",
// 	// 	       (unsigned long long)phit->end_of_hob_list, hob_ptr);
// 	// 	phit_ok = false;
// 	// }

// 	/* 3. Walk the whole list, validating bounds and tallying entries. */
// 	uint32_t total = 0, n_res = 0, n_sysmem = 0, n_fv = 0;
// 	uint32_t n_guid = 0, n_memalloc = 0, n_cpu = 0, n_other = 0;
// 	uint64_t total_sysmem = 0;
// 	bool end_found = false;
// 	const uintptr_t end = (uintptr_t)phit->end_of_hob_list;

// 	union {
// 		struct hob_header *hdr;
// 		uintptr_t addr;
// 	} cur = { .hdr = hob_ptr };

// 	/*
// 	 * Walk until the END_OF_HOB_LIST type marker, exactly like coreboot's own
// 	 * fsp_hob_iterator (hand_off_block.c).  Per the PI spec EfiEndOfHobList
// 	 * points *at* the END HOB, so it is used only as a loose backstop here and
// 	 * never as the primary terminator -- otherwise the END HOB itself, which
// 	 * sits at cur.addr == end, would be skipped.
// 	 */
// 	for (;;) {
// 		struct hob_header *h = cur.hdr;
// 		uint16_t type = h->type;
// 		uint16_t len  = h->length;

// 		/* A length < header size (or 0) would loop forever -- stop. */
// 		if (len < HOB_GENERIC_HEADER_LEN) {
// 			vga_cb_sprintf(row++, "H[%u] BAD len %u - STOP", total, len);
// 			break;
// 		}
// 		/* Backstop: never read past the PHIT's declared end of list. */
// 		if (end && cur.addr > end) {
// 			vga_cb_sprintf(row++, "H[%u] past EndOfHobList - STOP", total);
// 			break;
// 		}

// 		switch (type) {
// 		case HOB_TYPE_HANDOFF:
// 			vga_cb_sprintf(row++, "H[%u] %s len=%u",
// 				       total, hob_type_name(type), len);
// 			break;
// 		case HOB_TYPE_RESOURCE_DESCRIPTOR: {
// 			const struct hob_resource *r = fsp_hob_header_to_resource(h);
// 			n_res++;
// 			vga_cb_sprintf(row++, "H[%u] RES %s s=0x%llx l=0x%llx",
// 				       total, hob_resource_type_name(r->type),
// 				       (unsigned long long)r->addr,
// 				       (unsigned long long)r->length);
// 			if (r->type == 0 /* EFI_RESOURCE_SYSTEM_MEMORY */) {
// 				n_sysmem++;
// 				total_sysmem += r->length;
// 			}
// 			break;
// 		}
// 		case HOB_TYPE_FV:
// 		case HOB_TYPE_FV2:
// 		case HOB_TYPE_FV3: {
// 			const struct hob_firmware_volume *fv = (const void *)h;
// 			n_fv++;
// 			vga_cb_sprintf(row++, "H[%u] FV b=0x%llx l=0x%llx",
// 				       total, (unsigned long long)fv->base_address,
// 				       (unsigned long long)fv->fv_length);
// 			break;
// 		}
// 		case HOB_TYPE_GUID_EXTENSION: {
// 			const uint8_t *g = (const uint8_t *)h + HOB_GENERIC_HEADER_LEN;
// 			n_guid++;
// 			vga_cb_sprintf(row++,
// 				       "H[%u] GUID %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",
// 				       total,
// 				       (uint32_t)g[0] | ((uint32_t)g[1] << 8) |
// 						((uint32_t)g[2] << 16) | ((uint32_t)g[3] << 24),
// 				       (uint16_t)(g[4] | (g[5] << 8)),
// 				       (uint16_t)(g[6] | (g[7] << 8)),
// 				       g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
// 			break;
// 		}
// 		case HOB_TYPE_MEMORY_ALLOCATION:
// 			n_memalloc++;
// 			vga_cb_sprintf(row++, "H[%u] %s len=%u",
// 				       total, hob_type_name(type), len);
// 			break;
// 		case HOB_TYPE_CPU: {
// 			const uint8_t *b = (const uint8_t *)h + HOB_GENERIC_HEADER_LEN;
// 			n_cpu++;
// 			vga_cb_sprintf(row++, "H[%u] CPU mem=%u io=%u", total, b[0], b[1]);
// 			break;
// 		}
// 		case HOB_TYPE_END_OF_HOB_LIST:
// 			end_found = true;
// 			vga_cb_sprintf(row++, "H[%u] END_OF_HOB_LIST", total);
// 			break;
// 		default:
// 			n_other++;
// 			vga_cb_sprintf(row++, "H[%u] %s 0x%04x len=%u",
// 				       total, hob_type_name(type), type, len);
// 			break;
// 		}

// 		total++;
// 		if (end_found)
// 			break;
// 		if (total > 4096) {
// 			vga_cb_sprintf(row++, "ERR: >4096 HOBs, abort");
// 			break;
// 		}
// 		cur.addr += len;
// 	}

// 	/* 4. Summary + cross-checks. */
// 	vga_cb_sprintf(row++, "TOT:%u RES:%u RAM:%u FV:%u",
// 		       total, n_res, n_sysmem, n_fv);
// 	vga_cb_sprintf(row++, "GUID:%u MA:%u CPU:%u OTH:%u",
// 		       n_guid, n_memalloc, n_cpu, n_other);
// 	vga_cb_sprintf(row++, "SysRAM=%llu MB",
// 		       (unsigned long long)(total_sysmem >> 20));

// 	if (end_found && end && cur.addr != end)
// 		vga_cb_sprintf(row++, "WARN: END@0x%llx != EndOfList",
// 			       (unsigned long long)cur.addr);

// 	if (!end_found)
// 		vga_cb_sprintf(row++, "ERR: no END_OF_HOB_LIST");
// 	if (n_sysmem == 0)
// 		vga_cb_sprintf(row++, "ERR: no SYSTEM_MEMORY res");
// 	if (n_fv == 0)
// 		vga_cb_sprintf(row++, "ERR: no FV HOB");

// 	/* 5. Final verdict (PHIT type already validated via early return above). */
// 	if (end_found && n_sysmem > 0 && n_fv > 0)
// 		vga_cb_sprintf(row++, "HOB list VALID for EDK II");
// 	else
// 		vga_cb_sprintf(row++, "HOB list INVALID!");

// 	/* Logs of prints:
// H[1] GUID 3b387bfd-7abc-4cf2-a0cab6a16c1b1b25
// H[2] MEM_POOL 0x0007 len=40
// H[3] MEM_POOL 0x0007 len=264
// H[4] MEM_POOL 0x0007 len=264
// H[5] MEM_POOL 0x0007 len=136
// H[6] MEM_POOL 0x0007 len=136
// H[7] MEM_POOL 0x0007 len=520
// H[8] MEM_POOL 0x0007 len=32
// H[9] MEM_POOL 0x0007 len=80
// H[10] GUID ea296d92-0b69-423c-8c2833b4e0a91268
// H[11] GUID 9b3ada4f-ae56-4c24-8deaf03b7558ae50
// H[12] GUID 8c689f53-e082-452e-82dce3e272a44c16
// H[13] MEM_POOL 0x0007 len=24
// H[14] GUID 4a7bd124-cbea-4b3b-958611e668e9bcdd

// 	*/
// }
#endif /* ENV_PAYLOAD_LOADER */



////////////////////////////


void run_romstage(void)
{
	if (!CONFIG(SEPARATE_ROMSTAGE)) {
		/* Call romstage instead of loading it as a cbfs file. */
		timestamp_add_now(TS_ROMSTAGE_START);
		romstage_main();
		dead_code();
	}

	struct prog romstage =
		PROG_INIT(PROG_ROMSTAGE, CONFIG_CBFS_PREFIX "/romstage");

	vboot_run_logic();

	timestamp_add_now(TS_COPYROM_START);

	if (ENV_X86 && CONFIG(BOOTBLOCK_NORMAL)) {
		if (legacy_romstage_select_and_load(&romstage) != CB_SUCCESS)
			goto fail;
	} else {
		if (cbfs_prog_stage_load(&romstage))
			goto fail;
	}

	timestamp_add_now(TS_COPYROM_END);

	console_time_report();

	prog_run(&romstage);

fail:
	if (CONFIG(BOOTBLOCK_CONSOLE))
		die_with_post_code(POSTCODE_INVALID_ROM,
				   "Couldn't load romstage.\n");
	halt();
}

int __weak prog_locate_hook(struct prog *prog) { return 0; }

static void run_ramstage_from_resume(struct prog *ramstage)
{
	/* Load the cached ramstage to runtime location. */
	stage_cache_load_stage(STAGE_RAMSTAGE, ramstage);

	ramstage->cbfs_type = CBFS_TYPE_STAGE;
	prog_set_arg(ramstage, (void *)cbmem_top());

	if (prog_entry(ramstage) != NULL) {
		printk(BIOS_INFO, "Jumping to image.\n");
		prog_run(ramstage);
	}

	printk(BIOS_ERR, "ramstage cache invalid.\n");
	board_reset();
}

static int load_relocatable_ramstage(struct prog *ramstage)
{
	struct rmod_stage_load rmod_ram = {
		.cbmem_id = CBMEM_ID_RAMSTAGE,
		.prog = ramstage,
	};

	return rmodule_stage_load(&rmod_ram);
}
void preload_ramstage(void)
{
	if (!CONFIG(CBFS_PRELOAD))
		return;

	printk(BIOS_INFO, "Preloading ramstage\n");

	cbfs_preload(CONFIG_CBFS_PREFIX "/ramstage");
}
void __noreturn run_ramstage(void)
{
	struct prog ramstage =
		PROG_INIT(PROG_RAMSTAGE, CONFIG_CBFS_PREFIX "/ramstage");

	/* Call "end of romstage" here if postcar stage doesn't exist */
	if (ENV_POSTCAR)
		timestamp_add_now(TS_POSTCAR_END);
	else
		timestamp_add_now(TS_ROMSTAGE_END);

	vboot_run_logic();

	/*
	 * Only x86 systems using ramstage stage cache currently take the same
	 * firmware path on resume.
	 */
	if (ENV_X86 && resume_from_stage_cache())
		run_ramstage_from_resume(&ramstage);

	timestamp_add_now(TS_COPYRAM_START);

	if (ENV_X86) {
		if (load_relocatable_ramstage(&ramstage))
			goto fail;
	} else {
		if (cbfs_prog_stage_load(&ramstage))
			goto fail;
	}

	stage_cache_add(STAGE_RAMSTAGE, &ramstage);

	timestamp_add_now(TS_COPYRAM_END);

	console_time_report();

	/* This overrides the arg fetched from the relocatable module */
	prog_set_arg(&ramstage, (void *)cbmem_top());

	prog_run(&ramstage);

fail:
	die_with_post_code(POSTCODE_INVALID_ROM, "Ramstage was not loaded!\n");
}

#if ENV_PAYLOAD_LOADER // gc-sections should take care of this

static struct prog global_payload = 
	PROG_INIT(PROG_PAYLOAD, CONFIG_CBFS_PREFIX "/payload");

void payload_preload(void)
{
	if (!CONFIG(CBFS_PRELOAD))
		return;

	cbfs_preload(global_payload.name);
}

void payload_load(void)
{
	struct prog *payload = &global_payload;
	void *mapping;

	timestamp_add_now(TS_LOAD_PAYLOAD);
	printk(BIOS_DEBUG, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_load\n");
	vga_cb_print(1, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_load");

	if (prog_locate_hook(payload))
		goto out;

	payload->cbfs_type = CBFS_TYPE_QUERY;
	printk(BIOS_DEBUG, "[GX] module=Coreboot event=checkpoint status=entry phase=cbfs_payload_lookup\n");
	vga_cb_print(2, "[GX] module=Coreboot event=checkpoint status=entry phase=cbfs_payload_lookup");
	mapping = cbfs_type_map(prog_name(payload), NULL, &payload->cbfs_type);

	if (!mapping) {
		printk(BIOS_ERR, "[GX] module=Coreboot event=error status=missing phase=cbfs_payload_lookup\n");
		vga_cb_print(3, "[GX] module=Coreboot event=error status=missing phase=cbfs_payload_lookup");
		goto out;
	}

	switch (prog_cbfs_type(payload)) {
	case CBFS_TYPE_SELF: /* Simple ELF */
		printk(BIOS_DEBUG, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_selfload\n");
		vga_cb_print(3, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_selfload");
		selfload_mapped(payload, mapping, BM_MEM_RAM);
		break;
	case CBFS_TYPE_FIT_PAYLOAD: /* Flattened image tree */
		if (CONFIG(PAYLOAD_FIT_SUPPORT)) {
			printk(BIOS_DEBUG, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_fit_load\n");
			vga_cb_print(3, "[GX] module=Coreboot event=checkpoint status=entry phase=payload_fit_load");
			fit_payload(payload, mapping);
			break;
		}
		__fallthrough;
	default: {
		printk(BIOS_ERR, "[GX] module=Coreboot event=error status=unsupported phase=payload_type\n");
		vga_cb_print(3, "[GX] module=Coreboot event=error status=unsupported phase=payload_type");
		die_with_post_code(POSTCODE_INVALID_ROM,
				   "Unsupported payload type %d.\n", payload->cbfs_type);

		vga_line_write(0, "[MN]                          payload_load() - Unsupported payload type");
		break;
	}
	}

	cbfs_unmap(mapping);
	out:
	if (prog_entry(payload) == NULL) {
		printk(BIOS_ERR, "[GX] module=Coreboot event=error status=not_loaded phase=payload_load\n");
		vga_cb_print(4, "[GX] module=Coreboot event=error status=not_loaded phase=payload_load");
		die_with_post_code(POSTCODE_INVALID_ROM, "Payload not loaded.\n");

		vga_line_write(0, "[MN]                           payload_load() - Payload not loaded.");
	} else {
		printk(BIOS_DEBUG, "[GX] module=Coreboot event=exit status=success phase=payload_load entry=%p\n",
			prog_entry(payload));
		vga_cb_print(4, "[GX] module=Coreboot event=exit status=success phase=payload_load");
	}
}

// void payload_run(void)
// {
// 	vga_line_write(4, "[MN] payload_run - 1");
// 	struct prog *payload = &global_payload;

// 	/* Reset to booting from this image as late as possible */
// 	boot_successful();
// 	vga_line_write(4, "[MN] payload_run - 2");

// 	printk(BIOS_INFO, "Jumping to boot code at %p(%p)\n",
// 		prog_entry(payload), prog_entry_arg(payload));

// 	vga_line_write(4, "[MN] payload_run - 3");
// 	post_code(POSTCODE_ENTER_ELF_BOOT);

// 	vga_line_write(4, "[MN] payload_run - 4");
// 	timestamp_add_now(TS_SELFBOOT_JUMP);

// 	vga_line_write(4, "[MN] payload_run - 5");
// 	/* Before we go off to run the payload, see if
// 	 * we stayed within our bounds.
// 	 */
// 	checkstack(_estack, 0);

// 	vga_line_write(4, "[MN] payload_run - 6");
// 	// prog_run(payload);



// /////////////////////////////////
// 	mn_prog_run_0(payload);
// 	struct prog *prog = payload;
// 	vga_line_write(4, "[MN] payload_run - 7");
// 	// mn_prog_run_1(payload);

// #if ENV_RAMSTAGE && ENV_X86_64
// 	vga_line_write(4, "[MN] payload_run - 7-0");
// 	const uint32_t arg = pointer_to_uint32_safe(prog_entry_arg(prog));
// 	const uint32_t entry = pointer_to_uint32_safe(prog_entry(prog));

// 	/* On x86 coreboot payloads expect to be called in protected mode */
// 	protected_mode_call_1arg((void *)(uintptr_t)entry, arg);

// 	vga_line_write(4, "[MN] payload_run - 7-1");
// #else
// 	vga_line_write(4, "[MN] payload_run - 8-0");
// #if ENV_X86_64
// 	void (*doit)(void *arg);
// #else
// 	/* Ensure the argument is pushed on the stack. */
// 	asmlinkage void (*doit)(void *arg);
// #endif
// 	vga_line_write(4, "[MN] payload_run - 8-1");
// 	doit = prog_entry(prog);

// 	uintptr_t entryAddr = (uintptr_t)doit;
// 	char temp[32];
// 	memset(temp, 0x00, sizeof(temp));
// 	temp[0] = '0';
// 	temp[1] = 'x';
// 	int32_t index = 0;
// 	while (entryAddr) {
// 		temp[index + 2] = (entryAddr % 10) + '0';
// 		entryAddr /= 10;
// 		index++;
// 	}
// 	vga_line_write(5, temp);


//     // printf("print [MN] payload_run - 8-2\n");  // Final ASCII
// 	printk(BIOS_INFO, "printk [MN] payload_run - 8-2\n");

// 	vga_line_write(4, "[MN] payload_run - 8-2");
// 	hex_dump((unsigned char*)doit, 256);
// 	vga_hex_dump((unsigned char*)doit, 256, 5);
// 	vga_line_write(4, "[MN] payload_run - 8-2-1");
// 	doit(prog_entry_arg(prog));
// 	vga_line_write(4, "[MN] payload_run - 8_3");
// #endif

// 	vga_line_write(4, "[MN] payload_run - 9");
// }

#include <bootmem.h>



/* Struct to pass data to the walk callback */
struct vga_mem_info {
    int row;
    int ram_found;
    uint64_t total_ram;
    uint64_t first_ram_base;
    uint64_t first_ram_end;
    uint64_t first_ram_size;
    int region_count;
};

// /* Callback function for bootmem_walk() */
// static bool print_memory_range(const struct range_entry *r, void *arg);
// static bool print_memory_range(const struct range_entry *r, void *arg)
// {
//     struct vga_mem_info *info = (struct vga_mem_info *)arg;
//     uint64_t base = range_entry_base(r);
//     uint64_t end = range_entry_end(r) - 1;
//     uint64_t size = range_entry_size(r);
//     enum bootmem_type tag = range_entry_tag(r);

//     // Build type string
//     const char *type_str;
//     switch (tag) {
//         case BM_MEM_RAM:           type_str = "RAM"; break;
//         case BM_MEM_RESERVED:      type_str = "RESERVED"; break;
//         case BM_MEM_SOFT_RESERVED: type_str = "SOFT_RESERVED"; break;
//         case BM_MEM_ACPI:          type_str = "ACPI"; break;
//         case BM_MEM_NVS:           type_str = "NVS"; break;
//         case BM_MEM_UNUSABLE:      type_str = "UNUSABLE"; break;
//         case BM_MEM_VENDOR_RSVD:   type_str = "VENDOR_RSVD"; break;
//         case BM_MEM_TABLE:         type_str = "TABLE"; break;
//         case BM_MEM_RAMSTAGE:      type_str = "RAMSTAGE"; break;
//         case BM_MEM_PAYLOAD:       type_str = "PAYLOAD"; break;
//         default:                   type_str = "UNKNOWN"; break;
//     }

//     // Print region to VGA
//     vga_cb_sprintf(info->row, "%2d. %016llx-%016llx: %s (%llu MB)",
//         info->region_count, base, end, type_str, size / (1024 * 1024));

//     // Track RAM info
//     if (tag == BM_MEM_RAM) {
//         info->ram_found = 1;
//         info->total_ram += size;
        
//         // Save first RAM region details
//         if (info->first_ram_size == 0) {
//             info->first_ram_base = base;
//             info->first_ram_end = end;
//             info->first_ram_size = size;
//         }
//     }

//     info->row++;
//     info->region_count++;
    
//     return true; // Continue walking
// }

/* Main verification function - call this before launching payload */
void verify_memory_for_payload(void);

// void verify_memory_for_payload(void)
// {
//     struct vga_mem_info info;
//     int summary_row;

//     // Initialize info struct
//     info.row = 2;
//     info.ram_found = 0;
//     info.total_ram = 0;
//     info.first_ram_base = 0;
//     info.first_ram_end = 0;
//     info.first_ram_size = 0;
//     info.region_count = 0;

//     // Clear screen and print header
//     vga_cb_clear();
//     vga_cb_sprintf(0, "=== MEMORY VERIFICATION FOR EDK II ===");
//     vga_cb_sprintf(1, "--------------------------------------");

//     // Walk all memory regions using public API
//     bootmem_walk(print_memory_range, &info);

//     // Print summary
//     summary_row = info.row + 1;
    
//     if (info.ram_found) {
//         vga_cb_sprintf(summary_row++, "=== RAM STATUS: INITIALIZED ===");
//         vga_cb_sprintf(summary_row++, "Total RAM Regions: %d", info.region_count);
//         vga_cb_sprintf(summary_row++, "Total Available RAM: %llu MB (%llu KB)",
//             info.total_ram / (1024 * 1024), info.total_ram / 1024);
//         vga_cb_sprintf(summary_row++, "Total Available RAM: 0x%llx bytes", 
//             info.total_ram);
        
//         // Print first RAM region details
//         if (info.first_ram_size > 0) {
//             vga_cb_sprintf(summary_row++, "First RAM Region:");
//             vga_cb_sprintf(summary_row++, "  Start: 0x%016llx", info.first_ram_base);
//             vga_cb_sprintf(summary_row++, "  End:   0x%016llx", info.first_ram_end);
//             vga_cb_sprintf(summary_row++, "  Size:  %llu MB (0x%llx bytes)", 
//                 info.first_ram_size / (1024 * 1024), info.first_ram_size);
//         }

//         // Check if memory is sufficient for payload (at least 1MB)
//         if (info.total_ram >= (1 * 1024 * 1024)) {
//             vga_cb_sprintf(summary_row++, "=== MEMORY CHECK: PASSED ===");
//             vga_cb_sprintf(summary_row++, "System ready for EDK II handoff");
//         } else {
//             vga_cb_sprintf(summary_row++, "=== MEMORY CHECK: FAILED ===");
//             vga_cb_sprintf(summary_row++, "ERROR: Insufficient RAM for EDK II!");
//         }
//     } else {
//         vga_cb_sprintf(summary_row++, "=== RAM STATUS: NOT INITIALIZED ===");
//         vga_cb_sprintf(summary_row++, "ERROR: No RAM regions found!");
//         vga_cb_sprintf(summary_row++, "EDK II will likely fail to boot!");
//     }
// }

#include <cbmem.h>
#include <bootmem.h>
#include <program_loading.h>

/*
 * NOTE: the block below (duplicate "minimal" HOB typedefs, mem_callback,
 * dump_hob_summary and verify_memory_for_payload) is superseded by
 * verify_edkii_hob() above and is no longer called from the active payload
 * path. It is disabled because its local EFI_HOB_* typedefs clash with the real
 * EDK II definitions pulled in transitively through <fsp/util.h>.
 */
#if 0
/* Minimal HOB definitions */
typedef struct {
    uint16_t HobType;
    uint16_t HobLength;
} EFI_HOB_GENERIC_HEADER;

typedef struct {
    EFI_HOB_GENERIC_HEADER Header;
    uint32_t Version;
    uint32_t BootMode;
    uint64_t MemoryTop;
    uint64_t MemoryBottom;
    uint64_t FreeMemoryTop;
    uint64_t FreeMemoryBottom;
    uint64_t EndOfHobList;
} EFI_HOB_HANDOFF_INFO_TABLE;

typedef struct {
    EFI_HOB_GENERIC_HEADER Header;
    uint32_t ResourceType;
    uint32_t ResourceAttribute;
    uint64_t PhysicalStart;
    uint64_t ResourceLength;
} EFI_HOB_RESOURCE_DESCRIPTOR;

#define EFI_HOB_TYPE_HANDOFF               0x0001
#define EFI_HOB_TYPE_RESOURCE_DESCRIPTOR   0x0003
#define EFI_RESOURCE_SYSTEM_MEMORY         0x00000000

struct payload_check {
    int row;
    uint64_t total_ram;
    uint64_t first_ram_base;
    uint64_t first_ram_end;
    uint64_t payload_size;
    bool cbmem_ok;
    bool hob_ok;
    bool permanent_ram;
};

static bool mem_callback(const struct range_entry *r, void *arg)
{
    struct payload_check *info = arg;
    uint64_t base = range_entry_base(r);
    uint64_t size = range_entry_size(r);
    enum bootmem_type tag = range_entry_tag(r);

    if (tag == BM_MEM_RAM) {
        info->total_ram += size;
        if (info->first_ram_base == 0) {
            info->first_ram_base = base;
            info->first_ram_end = base + size - 1;
        }
    } else if (tag == BM_MEM_PAYLOAD) {
        info->payload_size = size;
    }
    return true;
}

static void dump_hob_summary(void *hob_list, int *row)
{
    EFI_HOB_HANDOFF_INFO_TABLE *hob_hdr = hob_list;
    if (!hob_hdr || hob_hdr->Header.HobType != EFI_HOB_TYPE_HANDOFF) {
        vga_cb_sprintf((*row)++, "HOB: Invalid");// HOB: Invalid
        return;
    }

    vga_cb_sprintf((*row)++, "HOB v=%d Top=0x%llx", hob_hdr->Version, hob_hdr->MemoryTop);

    EFI_HOB_GENERIC_HEADER *hob = hob_list;
    int count = 0;
    while ((uintptr_t)hob < (uintptr_t)hob_hdr->EndOfHobList && count < 3) {
        if (hob->HobType == EFI_HOB_TYPE_RESOURCE_DESCRIPTOR) {
            EFI_HOB_RESOURCE_DESCRIPTOR *res = (void*)hob;
            if (res->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
                vga_cb_sprintf((*row)++, "HOB RAM:0x%llx (%lluMB)", 
                               res->PhysicalStart, res->ResourceLength >> 20);
            }
        }
        hob = (void*)((uintptr_t)hob + hob->HobLength);
        count++;
    }
}

void verify_memory_for_payload(void)
{
    struct payload_check info = { .row = 1 };

    vga_cb_clear();
    vga_cb_sprintf(0, "=== EDKII PAYLOAD CHECK ===");// === EDKII PAYLOAD CHECK ===

    bootmem_walk(mem_callback, &info);

    /* Safe permanent RAM detection */
    info.cbmem_ok = (cbmem_top() != 0);
    info.permanent_ram = info.cbmem_ok;
    if (!info.permanent_ram && info.total_ram > (8ULL << 20))
        info.permanent_ram = true;

    void *hob_list = prog_entry_arg(&global_payload);
    if (!hob_list)
        hob_list = cbmem_find(CBMEM_ID_HOB_POINTER);

    info.hob_ok = (hob_list != NULL);

    /* Display */
    vga_cb_sprintf(info.row++, "TOTAL RAM : %llu MB", 
        info.total_ram >> 20// 65076
    );
    vga_cb_sprintf(info.row++, "RAM RANGE : 0x%llx-0x%llx", 
                   info.first_ram_base, // 1000
                   info.first_ram_end// 9ffff
                );
    vga_cb_sprintf(info.row++, "PAYLOAD   : %llu MB", 
        info.payload_size >> 20
    );
    vga_cb_sprintf(info.row++, "PERM RAM  : %s (CAR=%s)", 
                   info.permanent_ram ? "YES" : "NO",// YES
                   info.permanent_ram ? "NO" : "YES"// NO
                );
    vga_cb_sprintf(info.row++, "CBMEM     : %s", 
        info.cbmem_ok ? "OK" : "NO"// OK
    );
    vga_cb_sprintf(info.row++, "HOB       : %s @ %p", 
        info.hob_ok ? "OK" : "MISSING", // OK
        hob_list);//63583000

    if (info.hob_ok)
        dump_hob_summary(hob_list, &info.row);

    if (info.permanent_ram && info.cbmem_ok && info.hob_ok && info.total_ram >= (256ULL << 20)) {
        vga_cb_sprintf(info.row++, "=== READY FOR EDK II ===");// === READY FOR EDK II ===
        vga_cb_sprintf(info.row++, "Permanent memory installed");// Permanent memory installed"
    } else {
        vga_cb_sprintf(info.row++, "=== NOT READY ===");
        if (!info.permanent_ram)
            vga_cb_sprintf(info.row++, "Still on CAR!");
        if (!info.hob_ok)
            vga_cb_sprintf(info.row++, "HOB missing -> PEI crash");
    }

    vga_cb_sprintf(info.row + 1, "Continuing...");// Continuing...
}
#endif /* 0 - superseded by verify_edkii_hob() */





// void payload_run(void)
// {
//     struct prog *payload = &global_payload;
//     /* Reset to booting from this image as late as possible */
//     boot_successful();
//     printk(BIOS_INFO, "Jumping to boot code at %p(%p)\n",
//         prog_entry(payload), prog_entry_arg(payload));
//     post_code(POSTCODE_ENTER_ELF_BOOT);
//     timestamp_add_now(TS_SELFBOOT_JUMP);
//     /* Before we go off to run the payload, see if
//      * we stayed within our bounds.
//      */
//     checkstack(_estack, 0);
//     // prog_run(payload);
//     //////////////////////////////////
//     // mn_prog_run_0(payload);
//     struct prog *prog = payload;

// // #if ENV_RAMSTAGE  && ENV_X86_64
//     const uint32_t arg = pointer_to_uint32_safe(prog_entry_arg(prog));
//     const uint32_t entry = pointer_to_uint32_safe(prog_entry(prog));


//     vga_cb_sprintf(2, "PL-%X, %X",
// 		entry,
// 		arg
// 	);
	

//     // hex_dump((unsigned char*)entry, 128);  // Serial/console dump
//     // vga_hex_dump((unsigned char*)entry, 128, 6);  // VGA dump starting lower

//     // hex_dump((unsigned char*)arg, 128);  // Serial/console dump
//     // vga_hex_dump((unsigned char*)arg, 128, 6);  // VGA dump starting lower

// 	verify_memory_for_payload();



//     /* On x86 coreboot payloads expect to be called in protected mode */
// 	// TODO(MN): Uncomment to run the payload
//     // protected_mode_call_1arg((void *)(uintptr_t)entry, arg);


// // #else
// //     void (*doit)(void *arg);
// //     doit = prog_entry(prog);
// //     if (!doit) {
// //         vga_line_write(1, "[MN] ERROR: Invalid payload entry!");
// //         return;  // Or halt
// //     }

// //     doit(prog_entry_arg(prog));
// //     vga_line_write(5, "[MN] payload_run - 8_3");  // Won't run
// // #endif
// }






#include <delay.h>

void payload_run(void)
{
    struct prog *payload = &global_payload;

    boot_successful();
	printk(BIOS_DEBUG, "[GX] module=Coreboot event=handoff status=entry phase=payload_run entry=%p\n",
		prog_entry(payload));
	vga_cb_print(5, "[GX] module=Coreboot event=handoff status=entry phase=payload_run");
    printk(BIOS_INFO, "Jumping to boot code at %p\n", prog_entry(payload));
    post_code(POSTCODE_ENTER_ELF_BOOT);
    timestamp_add_now(TS_SELFBOOT_JUMP);
    checkstack(_estack, 0);

    /* ---- GET THE REAL HOB LIST ---- */
    void *hob_list = (void *)fsp_get_hob_list();   // primary source
    if (!hob_list) {
        /* Fallback: try CBMEM (pointer stored as value) */
        void **cbmem_hob_ptr = cbmem_find(CBMEM_ID_HOB_POINTER);
        if (cbmem_hob_ptr)
            hob_list = *cbmem_hob_ptr;
    }

    if (!hob_list) {
		printk(BIOS_ERR, "[GX] module=Coreboot event=error status=missing phase=edk2_hob_list\n");
		vga_cb_print(6, "[GX] module=Coreboot event=error status=missing phase=edk2_hob_list");
        // vga_cb_clear();
        vga_cb_sprintf(2, "FATAL: No HOB list found!");
        die("No HOB list for EDK II payload.\n");
    }

    // /* Clear VGA and print verification */
    // // vga_cb_clear();
    // vga_cb_sprintf(0, "HOB verification");// HOB verification
    // vga_cb_sprintf(1, "HOB PTR = %p", // HOB PTR = 
	// 	hob_list);//0x63900000

    // verify_edkii_hob(hob_list, 3);

    /* Now call the payload with the correct argument (HOB pointer) */
	uint32_t entry = pointer_to_uint32_safe(prog_entry(payload));
	uint32_t arg   = pointer_to_uint32_safe(hob_list);
	printk(BIOS_DEBUG, "[GX] module=Coreboot event=handoff status=ready phase=edk2_payload entry=0x%x hob=0x%x\n",
		entry, arg);
	vga_cb_print(6, "[GX] module=Coreboot event=handoff status=ready phase=edk2_payload");

    // vga_cb_sprintf(10, "En:%X,%X", // En:
	// 	entry, // 802580
	// 	arg);// 63900000
	// mdelay(5000);

	// vga_cb_clear();
	// vga_cb_hex_dump((void*)arg, 384, 0);
	/* Print hex dump:
63900000:01 00 38 00 00 00 00 00 09 00 00 00 00 00 00 00
63900010:00 d0 7f 77 00 00 00 00 00 00 80 63 00 00 00 00
63900020:00 60 6b 76 00 00 00 00 f8 85 a6 63 00 00 00 00
63900030:f0 85 a6 63 00 00 00 00 04 00 10 1d 00 00 00 00
63900040:fd 7b 38 3b bc 7a f2 4c a0 ca b6 a1 6c 1b 1b 25
63900050:c6 1c 00 00 00 00 00 00 01 00 00 00 11 10 3a 01
63900060:00 00 00 00 00 00 88 ee 7a 5e 01 00 00 00 14 4b
63900070:c0 52 98 0b 6c 49 bc 3b 04 b5 02 11 d6 80 53 45
63900080:43 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
63900090:00 00 00 00 00 00 11 10 3a 01 50 00 00 00 00 00
639000a0:dc f9 7a 5e 01 00 00 00 14 5b c0 52 98 0b 6c 49
639000b0:bc 3b 04 b5 02 11 d6 80 50 45 49 00 00 00 00 00
639000c0:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
639000d0:11 10 3a 01 40 00 00 00 00 00 d4 ff 7a 4e 01 00
639000e0:00 00 14 5b c0 52 98 0b 6c 49 bc 3b 04 b5 02 11
639000f0:d6 80 50 72 65 4d 65 6d 00 00 00 00 00 00 00 00
63900100:00 00 00 00 00 00 00 00 00 00 11 10 3a 01 01 00
63900110:00 00 00 00 2c 9b 7b 5d 01 00 00 00 b9 81 3f b7
63900120:fc 1d 7c 48 82 4c 05 09 ee 2b 01 28 50 45 49 4d
63900130:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
63900140:00 00 00 00 11 10 3a 01 02 00 00 00 00 00 10 a5
63900150:7b 5e 01 00 00 00 b9 81 3f b7 fc 1d 7c 48 82 4c
63900160:05 09 ee 2b 01 28 50 45 49 4d 00 00 00 00 00 00
63900170:00 00 00 00 00 00 00 00 00 00 00 00 00 00 11 10
	*/
	// mdelay(5000);

	// vga_cb_clear();
	// vga_cb_hex_dump((unsigned char*)arg + 384, 384, 0);
	/* Print Hex dump:
63900180:3a 01 01 00 00 00 00 00 b0 16 7c 5e 01 00 00 00
63900190:4f da 3a 9b 56 ae 24 4c 8d ea f0 3b 75 58 ae 50
639001a0:50 45 49 4d 00 00 00 00 00 00 00 00 00 00 00 00
639001b0:00 00 00 00 00 00 00 00 11 10 3a 01 02 00 00 00
639001c0:00 00 99 3a 7c 5e 01 00 00 00 4f da 3a 9b 56 ae
639001d0:24 4c 8d ea f0 3b 75 58 ae 50 50 45 49 4d 00 00
639001e0:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
639001f0:00 00 11 10 3a 01 01 00 00 00 00 00 56 69 7c 5e
63900200:01 00 00 00 56 9a e2 fd 97 c1 e1 4a bb 98 79 2b
63900210:2f 09 72 5d 50 45 49 4d 00 00 00 00 00 00 00 00
63900220:00 00 00 00 00 00 00 00 00 00 00 00 11 10 3a 01
63900230:02 00 00 00 00 00 74 3a 8c 5e 01 00 00 00 56 9a
63900240:e2 fd 97 c1 e1 4a bb 98 79 2b 2f 09 72 5d 50 45
63900250:49 4d 00 00 00 00 00 00 00 00 00 00 00 00 00 00
63900260:00 00 00 00 00 00 11 10 3a 01 01 00 00 00 00 00
63900270:52 62 8c 5e 01 00 00 00 42 04 61 a3 9f e6 f3 4d
63900280:82 ca 23 60 c3 03 1a 23 50 45 49 4d 00 00 00 00
63900290:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
639002a0:11 10 3a 01 02 00 00 00 00 00 12 73 8c 5e 01 00
639002b0:00 00 42 04 61 a3 9f e6 f3 4d 82 ca 23 60 c4 03
639002c0:1a 23 50 45 49 4d 00 00 00 00 00 00 00 00 00 00
639002d0:00 00 00 00 00 00 00 00 00 00 11 10 3a 01 01 00
639002e0:00 00 00 00 c4 9a 8c 5e 01 00 00 00 06 88 e7 75
639002f0:8f c6 39 48 8a 68 b2 90 84 82 06 59 50 45 49 4d
	*/
	// mdelay(5000);

	// vga_cb_clear();
	// vga_cb_hex_dump((void*)entry, 384, 0);
	/* Print hex dump:
00802580:fa 8b 44 24 04 8b 25 b0 29 80 00 50 ff 35 ac 29
00802590:80 00 68 00 00 08 00 68 00 00 01 00 e8 ba fe ff
008025a0:ff eb fe 66 90 66 90 66 90 66 90 66 90 66 90 90
008025b0:8b 4c 24 0c 8b 44 24 08 31 d2 f7 f1 50 8b 44 24
008025C0:08 F7 F1 8B 4C 24 14 E3 02 89 11 5A C3 66 90 90
008025D0:57 31 C0 8B 7C 24 08 8B 4C 24 0C 89 CA C1 E9 02
008025E0:83 E2 03 57 F3 AB 89 D1 F3 AA 58 5F c3 66 90 90
008025f0:56 57 8b 74 24 10 8b 7c 24 0c 8b 54 24 14 8d 44
00802600:16 ff 39 fe 73 04 39 f8 73 0c 89 d1 83 e2 03 c1
00802610:e9 02 f3 a5 eb 07 89 c6 8d 7c 17 ff fd 89 d1 f3
00802620:a4 fc 8b 44 24 0c 5f 5e c3 66 90 66 90 66 90 90
00802630:53 9b db e3 d9 2d a0 2d 80 00 b8 01 00 00 00 0f
00802640:a2 0f ba e2 19 73 12 0f 20 e0 0d 00 02 00 00 0f
00802650:22 e0 0f ae 16 a2 2d 80 00 5b c3 3a 20 00 3c 6e
00802660:75 6c 6c 20 73 74 72 69 6e 67 3e 00 0d 0a 00 0d
00802670:00 3c 6e 75 6c 6c 20 74 69 6d 65 3e 00 3c 6e 75
00802680:6c 6c 20 67 75 69 64 3e 00 25 30 38 78 2d 25 30
00802690:34 78 2d 25 30 34 78 2d 25 30 32 78 25 30 32 78
008026a0:2d 25 30 32 78 25 30 32 78 25 30 32 78 25 30 32
008026b0:78 25 30 32 78 25 30 32 78 00 25 30 32 64 2f 25
008026c0:30 32 64 2f 25 30 34 64 20 20 25 30 32 64 3a 25
008026d0:30 32 64 00 25 30 38 58 00 20 20 20 20 20 20 20
008026e0:20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20
008026f0:20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20
	*/
	// mdelay(5000);

    protected_mode_call_1arg((void *)(uintptr_t)entry, arg);
}
#endif
