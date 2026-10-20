/* SPDX-License-Identifier: GPL-2.0-only */

#include <program_loading.h>
#include <vm.h>
#include <arch/boot.h>
#include <arch/encoding.h>
#include <arch/smp/smp.h>
#include <mcall.h>
#include <cbfs.h>
#include <console/console.h>

struct arch_prog_run_args {
	struct prog *prog;
	struct prog *opensbi;
};

/*
 * A pointer to the Flattened Device Tree passed to coreboot by the boot ROM.
 * Presumably this FDT is also in ROM.
 *
 * This pointer is only used in ramstage!
 */

static void do_arch_prog_run(struct arch_prog_run_args *args)
{
	char str[50] = 0;
	memset(str, ' ', sizeof(str));
	memcpy(str, "[MN] do_arch_prog_run", strlen("[MN] do_arch_prog_run"));
f
	vga_line_write(8, str);
	int hart_id = HLS()->hart_id;
	struct prog *prog = args->prog;
	void *fdt = HLS()->fdt;

	if (prog_cbfs_type(prog) == CBFS_TYPE_FIT_PAYLOAD) {
		vga_line_write(8, "[MN] do_arch_prog_run - 1");
		fdt = prog_entry_arg(prog);
		vga_line_write(8, "[MN] do_arch_prog_run - 2");
	}

	if (ENV_RAMSTAGE && prog_type(prog) == PROG_PAYLOAD) {
		if (CONFIG(RISCV_OPENSBI)) {
			// tell OpenSBI to switch to Supervisor mode before jumping to payload
			run_payload_opensbi(prog, fdt, args->opensbi, RISCV_PAYLOAD_MODE_S);
		} else {
			run_payload(prog, fdt, RISCV_PAYLOAD_MODE_S);
		}
	} else {
		void (*doit)(int hart_id, void *fdt, void *arg) = prog_entry(prog);
		doit(hart_id, fdt, prog_entry_arg(prog));
	}

	die("Failed to run stage");
}

void arch_prog_run(struct prog *prog)
{
	vga_line_write(6, "[MN] arch_prog_run - 0");
	struct arch_prog_run_args args = {};

	args.prog = prog;

	/* In case of OpenSBI we have to load it before resuming all HARTs */
	if (ENV_RAMSTAGE && CONFIG(RISCV_OPENSBI)) {
		vga_line_write(6, "[MN] arch_prog_run - 1");
		struct prog sbi = PROG_INIT(PROG_OPENSBI, CONFIG_CBFS_PREFIX"/opensbi");

		if (!selfload_check(&sbi, BM_MEM_OPENSBI)) {
			vga_line_write(6, "[MN] arch_prog_run - 2");
			die("OpenSBI load failed");
			vga_line_write(6, "[MN] arch_prog_run - 3");
		}

		args.opensbi = &sbi;
	}

	vga_line_write(7, "[MN] arch_prog_run - 4");
	smp_resume((void (*)(void *))do_arch_prog_run, &args);
	vga_line_write(7, "[MN] arch_prog_run - 5");
}
