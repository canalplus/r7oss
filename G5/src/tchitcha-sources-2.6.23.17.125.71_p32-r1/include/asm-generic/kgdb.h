/*
 * include/asm-generic/kgdb.h
 *
 * This provides the assembly level information so that KGDB can provide
 * a GDB that has been patched with enough information to know to stop
 * trying to unwind the function.
 *
 * Author: Tom Rini <trini@kernel.crashing.org>
 *
 * 2005 (c) MontaVista Software, Inc.
 * 2006 (c) Embedded Alley Solutions, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __ASM_GENERIC_KGDB_H__
#define __ASM_GENERIC_KGDB_H__

struct pt_regs;

#ifdef CONFIG_X86
/**
 *	kgdb_skipexception - Bail of of KGDB when we've been triggered.
 *	@exception: Exception vector number
 *	@regs: Current &struct pt_regs.
 *
 *	On some architectures we need to skip a breakpoint exception when
 *	it occurs after a breakpoint has been removed.
 */
int kgdb_skipexception(int exception, struct pt_regs *regs);
#else
static inline int kgdb_skipexception(int exception, struct pt_regs *regs)
{
	return 0;
}
#endif

#if defined(CONFIG_X86)
/**
 *	kgdb_post_master_code - Save error vector/code numbers.
 *	@regs: Original pt_regs.
 *	@e_vector: Original error vector.
 *	@err_code: Original error code.
 *
 *	This is needed on architectures which support SMP and KGDB.
 *	This function is called after all the slave cpus have been put
 *	to a know spin state and the master CPU has control over KGDB.
 */
extern void kgdb_post_master_code(struct pt_regs *regs, int e_vector,
				  int err_code);

/**
 *	kgdb_disable_hw_debug - Disable hardware debugging while we in kgdb.
 *	@regs: Current &struct pt_regs.
 *
 *	This function will be called if the particular architecture must
 *	disable hardware debugging while it is processing gdb packets or
 *	handling exception.
 */
extern void kgdb_disable_hw_debug(struct pt_regs *regs);
#else
#define kgdb_disable_hw_debug(regs)		do { } while (0)
#define kgdb_post_master_code(regs, v, c)	do { } while (0)
#endif

#ifdef CONFIG_KGDB_ARCH_HAS_SHADOW_INFO
/**
 *	kgdb_shadowinfo - Get shadowed information on @threadid.
 *	@regs: The &struct pt_regs of the current process.
 *	@buffer: A buffer of %BUFMAX size.
 *	@threadid: The thread id of the shadowed process to get information on.
 */
extern void kgdb_shadowinfo(struct pt_regs *regs, char *buffer,
			    unsigned threadid);

/**
 *	kgdb_get_shadow_thread - Get the shadowed &task_struct of @threadid.
 *	@regs: The &struct pt_regs of the current thread.
 *	@threadid: The thread id of the shadowed process to get information on.
 *
 *	RETURN:
 *	This returns a pointer to the &struct task_struct of the shadowed
 *	thread, @threadid.
 */
extern struct task_struct *kgdb_get_shadow_thread(struct pt_regs *regs,
						  int threadid);

/**
 *	kgdb_shadow_regs - Return the shadowed registers of @threadid.
 *	@regs: The &struct pt_regs of the current thread.
 *	@threadid: The thread id we want the &struct pt_regs for.
 *
 *	RETURN:
 *	The a pointer to the &struct pt_regs of the shadowed thread @threadid.
 */
extern struct pt_regs *kgdb_shadow_regs(struct pt_regs *regs, int threadid);
#else
#define kgdb_shadowinfo(regs, buf, threadid)		do { } while (0)
#define kgdb_get_shadow_thread(regs, threadid)		NULL
#define kgdb_shadow_regs(regs, threadid)		NULL
#endif

#endif				/* __ASM_GENERIC_KGDB_H__ */
