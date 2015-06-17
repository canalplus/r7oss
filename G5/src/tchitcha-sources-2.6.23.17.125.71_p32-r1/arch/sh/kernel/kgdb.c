/*
 * arch/sh/kernel/kgdb.c
 *
 * Contains SH-specific low-level support for KGDB.
 *
 * Containes extracts from code by Glenn Engel, Jim Kingdon,
 * David Grothe <dave@gcom.com>, Tigran Aivazian <tigran@sco.com>,
 * Amit S. Kale <akale@veritas.com>,  William Gatliff <bgat@open-widgets.com>,
 * Ben Lee, Steve Chamberlain and Benoit Miller <fulg@iname.com>,
 * Henry Bell <henry.bell@st.com> and Jeremy Siegel <jsiegel@mvista.com>
 *
 * Maintainer: Tom Rini <trini@kernel.crashing.org>
 *
 * 2004 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/kgdb.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <linux/kdebug.h>
#include <asm/system.h>
#include <asm/current.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

extern void per_cpu_trap_init(void);
extern atomic_t cpu_doing_single_step;

/* Function pointers for linkage */
static struct kgdb_regs trap_registers;

/* Globals. */
char in_nmi;			/* Set during NMI to prevent reentry */

/* TRA differs sh3/4 */
#if defined(CONFIG_CPU_SH3)
#define TRA 0xffffffd0
#elif defined(CONFIG_CPU_SH4)
#define TRA 0xff000020
#endif

/* Macros for single step instruction identification */
#define OPCODE_BT(op)	 (((op) & 0xff00) == 0x8900)
#define OPCODE_BF(op)	 (((op) & 0xff00) == 0x8b00)
#define OPCODE_BTF_DISP(op)   (((op) & 0x80) ? (((op) | 0xffffff80) << 1) : \
			      (((op) & 0x7f) << 1))
#define OPCODE_BFS(op)	(((op) & 0xff00) == 0x8f00)
#define OPCODE_BTS(op)	(((op) & 0xff00) == 0x8d00)
#define OPCODE_BRA(op)	(((op) & 0xf000) == 0xa000)
#define OPCODE_BRA_DISP(op)   (((op) & 0x800) ? (((op) | 0xfffff800) << 1) : \
			      (((op) & 0x7ff) << 1))
#define OPCODE_BRAF(op)       (((op) & 0xf0ff) == 0x0023)
#define OPCODE_BRAF_REG(op)   (((op) & 0x0f00) >> 8)
#define OPCODE_BSR(op)	(((op) & 0xf000) == 0xb000)
#define OPCODE_BSR_DISP(op)   (((op) & 0x800) ? (((op) | 0xfffff800) << 1) : \
			      (((op) & 0x7ff) << 1))
#define OPCODE_BSRF(op)       (((op) & 0xf0ff) == 0x0003)
#define OPCODE_BSRF_REG(op)   (((op) >> 8) & 0xf)
#define OPCODE_JMP(op)	(((op) & 0xf0ff) == 0x402b)
#define OPCODE_JMP_REG(op)    (((op) >> 8) & 0xf)
#define OPCODE_JSR(op)	(((op) & 0xf0ff) == 0x400b)
#define OPCODE_JSR_REG(op)    (((op) >> 8) & 0xf)
#define OPCODE_RTS(op)	((op) == 0xb)
#define OPCODE_RTE(op)	((op) == 0x2b)

#define SR_T_BIT_MASK	   0x1
#define STEP_OPCODE	     0xc320
#define BIOS_CALL_TRAP	  0x3f

/* Exception codes as per SH-4 core manual */
#define ADDRESS_ERROR_LOAD_VEC   7
#define ADDRESS_ERROR_STORE_VEC  8
#define TRAP_VEC		 11
#define INVALID_INSN_VEC	 12
#define INVALID_SLOT_VEC	 13
#define NMI_VEC		  14
#define SERIAL_BREAK_VEC	 58

/* Misc static */
static int stepped_address;
static short stepped_opcode;

/* Translate SH-3/4 exception numbers to unix-like signal values */
static int compute_signal(const int excep_code)
{
	switch (excep_code) {
	case INVALID_INSN_VEC:
	case INVALID_SLOT_VEC:
		return SIGILL;
	case ADDRESS_ERROR_LOAD_VEC:
	case ADDRESS_ERROR_STORE_VEC:
		return SIGSEGV;
	case SERIAL_BREAK_VEC:
	case NMI_VEC:
		return SIGINT;
	default:
		/* Act like it was a break/trap. */
		return SIGTRAP;
	}
}

/*
 * Translate the registers of the system into the format that GDB wants.  Since
 * we use a local structure to store things, instead of getting them out
 * of pt_regs, we can just do a memcpy.
 */
void regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *ign)
{
	memcpy(gdb_regs, &trap_registers, sizeof(trap_registers));
}

asmlinkage void ret_from_fork(void);
void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	int count;
	int vbr_val;
	struct pt_regs *kregs;
	unsigned long *tregs;

	if ( p == NULL )
		return;

	__asm__ __volatile__ ("stc vbr, %0":"=r"(vbr_val));
	/* A new fork has pt_regs on the stack from a fork() call (?????) */
	if (p->thread.pc == (unsigned long)ret_from_fork) {
		kregs = (struct pt_regs*)p->thread.sp;
		for (count = 0; count < 16; count++)
			*(gdb_regs++) = kregs->regs[count];
		*(gdb_regs++) = kregs->pc;
		*(gdb_regs++) = kregs->pr;
		*(gdb_regs++) = kregs->gbr;
		*(gdb_regs++) = vbr_val;
		*(gdb_regs++) = kregs->mach;
		*(gdb_regs++) = kregs->macl;
		*(gdb_regs++) = kregs->sr;
		return;
	}

	/*
	 * Otherwise we have to collect the thread registers from the stack
	 * built by switch function (see include/asm-sh/system.h)
	 *
	 * NOTE:
	 * the stack frame for the thread is:
	 *      r14, ... r8, PR, GBR
	 * while the "frame" sent to GDB is:
	 *      r0, ... r15, PR, GBR, VBR, MACH, MACHL, SR
	 */
	tregs = (unsigned long *) p->thread.sp;

	/* r0-r7 ("scratch" registers) */
	for (count = 0; count < 8; count++)
		*(gdb_regs++) = 0xdeadbeef;

	/* r8-r14 (switch stack registers) */
	tregs+=6;
	for (count = 0; count < 7; count++)
		*(gdb_regs++) = *(tregs--);
	tregs+=8;
	*(gdb_regs++) = p->thread.sp;   /* r15 */
	*(gdb_regs++) = p->thread.pc;
	*(gdb_regs++) = *tregs++;       /* PR */
	*(gdb_regs++) = *tregs++;       /* GBR */
	*(gdb_regs++) = vbr_val;
	*(gdb_regs++) = 0xdeadbeef;     /* MACH: scratch   */
	*(gdb_regs++) = 0xdeadbeef;     /* MACHL: scratch  */
	*(gdb_regs++) = SR_FD;	  /* Status Register */
	return;
}

/*
 * Translate the registers values that GDB has given us back into the
 * format of the system.  See the comment above about memcpy.
 */
void gdb_regs_to_regs(unsigned long *gdb_regs, struct pt_regs *ign)
{
	memcpy(&trap_registers, gdb_regs, sizeof(trap_registers));
}

/* Calculate the new address for after a step */
static short *get_step_address(void)
{
	short op = *(short *)trap_registers.pc;
	long addr;

	/* BT */
	if (OPCODE_BT(op)) {
		if (trap_registers.sr & SR_T_BIT_MASK)
			addr = trap_registers.pc + 4 + OPCODE_BTF_DISP(op);
		else
			addr = trap_registers.pc + 2;
	}

	/* BTS */
	else if (OPCODE_BTS(op)) {
		if (trap_registers.sr & SR_T_BIT_MASK)
			addr = trap_registers.pc + 4 + OPCODE_BTF_DISP(op);
		else
			addr = trap_registers.pc + 4;	/* Not in delay slot */
	}

	/* BF */
	else if (OPCODE_BF(op)) {
		if (!(trap_registers.sr & SR_T_BIT_MASK))
			addr = trap_registers.pc + 4 + OPCODE_BTF_DISP(op);
		else
			addr = trap_registers.pc + 2;
	}

	/* BFS */
	else if (OPCODE_BFS(op)) {
		if (!(trap_registers.sr & SR_T_BIT_MASK))
			addr = trap_registers.pc + 4 + OPCODE_BTF_DISP(op);
		else
			addr = trap_registers.pc + 4;	/* Not in delay slot */
	}

	/* BRA */
	else if (OPCODE_BRA(op))
		addr = trap_registers.pc + 4 + OPCODE_BRA_DISP(op);

	/* BRAF */
	else if (OPCODE_BRAF(op))
		addr = trap_registers.pc + 4
		    + trap_registers.regs[OPCODE_BRAF_REG(op)];

	/* BSR */
	else if (OPCODE_BSR(op))
		addr = trap_registers.pc + 4 + OPCODE_BSR_DISP(op);

	/* BSRF */
	else if (OPCODE_BSRF(op))
		addr = trap_registers.pc + 4
		    + trap_registers.regs[OPCODE_BSRF_REG(op)];

	/* JMP */
	else if (OPCODE_JMP(op))
		addr = trap_registers.regs[OPCODE_JMP_REG(op)];

	/* JSR */
	else if (OPCODE_JSR(op))
		addr = trap_registers.regs[OPCODE_JSR_REG(op)];

	/* RTS */
	else if (OPCODE_RTS(op))
		addr = trap_registers.pr;

	/* RTE */
	else if (OPCODE_RTE(op))
		addr = trap_registers.regs[15];

	/* Other */
	else
		addr = trap_registers.pc + 2;

	kgdb_flush_icache_range(addr, addr + 2);
	return (short *)addr;
}

/* The command loop, read and act on requests */
int kgdb_arch_handle_exception(int e_vector, int signo, int err_code,
			       char *remcom_in_buffer, char *remcom_out_buffer,
			       struct pt_regs *ign)
{
	unsigned long addr;
	char *ptr = &remcom_in_buffer[1];

	/* Examine first char of buffer to see what we need to do */
	switch (remcom_in_buffer[0]) {
	case 'c':		/* Continue at address AA..AA (optional) */
	case 's':		/* Step one instruction from AA..AA */
		/* Try to read optional parameter, PC unchanged if none */
		if (kgdb_hex2long(&ptr, &addr))
			trap_registers.pc = addr;

		atomic_set(&cpu_doing_single_step, -1);
		if (remcom_in_buffer[0] == 's') {
			/* Replace the instruction immediately after the
			 * current instruction (i.e. next in the expected
			 * flow of control) with a trap instruction, so that
			 * returning will cause only a single instruction to
			 * be executed. Note that this model is slightly
			 * broken for instructions with delay slots
			 * (e.g. B[TF]S, BSR, BRA etc), where both the branch
			 * and the instruction in the delay slot will be
			 * executed.
			 */
			/* Determine where the target instruction will send
			 * us to */
			unsigned short *next_addr = get_step_address();
			stepped_address = (int)next_addr;

			/* Replace it */
			stepped_opcode = *(short *)next_addr;
			*next_addr = STEP_OPCODE;

			/* Flush and return */
			kgdb_flush_icache_range((long)next_addr,
						(long)next_addr + 2);
			if (kgdb_contthread)
				atomic_set(&cpu_doing_single_step,
					   smp_processor_id());
		}
		return 0;
	}
	return -1;
}


/*
 * When an exception has occured, we are called.  We need to set things
 * up so that we can call kgdb_handle_exception to handle requests from
 * the remote GDB.
 */
void kgdb_exception_handler(struct pt_regs *regs)
{
	int excep_code, vbr_val;
	int trapa_value = *(volatile unsigned long *)(TRA);
	int count;

	/* Copy kernel regs (from stack) */
	for (count = 0; count < 16; count++)
		trap_registers.regs[count] = regs->regs[count];
	trap_registers.pc = regs->pc;
	trap_registers.pr = regs->pr;
	trap_registers.sr = regs->sr;
	trap_registers.gbr = regs->gbr;
	trap_registers.mach = regs->mach;
	trap_registers.macl = regs->macl;

	__asm__ __volatile__("stc vbr, %0":"=r"(vbr_val));
	trap_registers.vbr = vbr_val;

	/* Get the execption code. */
	__asm__ __volatile__("stc r2_bank, %0":"=r"(excep_code));

	excep_code >>= 5;

	/* If we got an NMI, and KGDB is not yet initialized, call
	 * breakpoint() to try and initialize everything for us. */
	if (excep_code == NMI_VEC && !kgdb_initialized) {
		breakpoint();
		return;
	}

#ifdef TRA
	/* TRAP_VEC exception indicates a software trap inserted in place of
	 * code by GDB so back up PC by one instruction, as this instruction
	 * will later be replaced by its original one.  Do NOT do this for
	 * trap 0xff, since that indicates a compiled-in breakpoint which
	 * will not be replaced (and we would retake the trap forever) */
	if ((excep_code == TRAP_VEC) && (trapa_value != (0x3c << 2)))
		trap_registers.pc -= 2;
#endif

	/* If we have been single-stepping, put back the old instruction.
	 * We use stepped_address in case we have stopped more than one
	 * instruction away. */
	if (stepped_opcode != 0) {
		*(short *)stepped_address = stepped_opcode;
		kgdb_flush_icache_range(stepped_address, stepped_address + 2);
	}
	stepped_opcode = 0;

	/* Call the stub to do the processing.  Note that not everything we
	 * need to send back and forth lives in pt_regs. */
	kgdb_handle_exception(excep_code, compute_signal(excep_code), 0, regs);

	/* Copy back the (maybe modified) registers */
	for (count = 0; count < 16; count++)
		regs->regs[count] = trap_registers.regs[count];
	regs->pc = trap_registers.pc;
	regs->pr = trap_registers.pr;
	regs->sr = trap_registers.sr;
	regs->gbr = trap_registers.gbr;
	regs->mach = trap_registers.mach;
	regs->macl = trap_registers.macl;
}

int __init kgdb_arch_init(void)
{
	return 0;
}

struct kgdb_arch arch_kgdb_ops = {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
	.gdb_bpt_instr = { 0x32, 0xc3 },
#else /* ! CONFIG_CPU_LITTLE_ENDIAN */
	.gdb_bpt_instr = { 0xc3, 0x32 },
#endif
};
