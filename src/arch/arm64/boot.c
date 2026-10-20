/* SPDX-License-Identifier: GPL-2.0-only */

#include <cbmem.h>
#include <arch/lib_helpers.h>
#include <arch/stages.h>
#include <arch/transition.h>
#include <bl31.h>
#include <program_loading.h>

static void run_payload(struct prog *prog)
{
	void (*doit)(void *);
	void *arg;

	doit = prog_entry(prog);
	arg = prog_entry_arg(prog);
	u64 payload_spsr = get_eret_el(EL2, SPSR_USE_L);

	if (CONFIG(ARM64_USE_ARM_TRUSTED_FIRMWARE))
		run_bl31((u64)doit, (u64)arg, payload_spsr);
	else if (CONFIG_ARM64_CURRENT_EL == EL3)
		transition_to_el2(doit, arg, payload_spsr);
	else
		doit(arg);
}

void arch_prog_run(struct prog *prog)
{
	vga_line_write(6, "[MN] arch_prog_run - 0");
	void (*doit)(void *);

	if (ENV_RAMSTAGE && prog_type(prog) == PROG_PAYLOAD) {
		vga_line_write(6, "[MN] arch_prog_run - 1");
		run_payload(prog);
		return;
	}

	vga_line_write(6, "[MN] arch_prog_run - 2");
	doit = prog_entry(prog);

	vga_line_write(6, "[MN] arch_prog_run - 3");
	doit(prog_entry_arg(prog));
	vga_line_write(6, "[MN] arch_prog_run - 4");
}

/* Generic stage entry point. Can be overridden by board/SoC if needed. */
__weak void stage_entry(uintptr_t stage_arg)
{
	if (!ENV_ROMSTAGE_OR_BEFORE)
		_cbmem_top_ptr = stage_arg;

	main();
}
