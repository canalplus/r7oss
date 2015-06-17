/*
 * stack arch x86_64 probe
 *
 * Part of LTTng
 *
 * Mathieu Desnoyers, March 2007
 *
 * Licensed under the GPLv2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/marker.h>
#include <linux/ptrace.h>
#include <linux/ltt-tracer.h>

#include <asm/desc_defs.h>
#include <asm/user32.h>

#ifdef CONFIG_LTT_KERNEL_STACK
/* Kernel specific trace stack dump routines, from arch/x86_64/kernel/traps.c */

/*
 * Event kernel_dump structures
 * ONLY USE IT ON THE CURRENT STACK.
 * It does not protect against other stack modifications and _will_
 * cause corruption upon race between threads.
 * FIXME : using __kernel_text_address here can break things if a module is
 * loaded during tracing. The real function pointers are OK : if they are on our
 * stack, it means that the module refcount must not be 0, but the problem comes
 * from the "false positives" : that that appears to be a function pointer.
 * The solution to this problem :
 * Without frame pointers, we have one version with spinlock irqsave (never call
 * this in NMI context, and another with a trylock, which can fail.
 */

/* From traps.c */

static unsigned long *trace_in_exception_stack(unsigned cpu,
					unsigned long stack,
					unsigned *usedp, const char **idp)
{
	static char ids[][8] = {
		[DEBUG_STACK - 1] = "#DB",
		[NMI_STACK - 1] = "NMI",
		[DOUBLEFAULT_STACK - 1] = "#DF",
		[STACKFAULT_STACK - 1] = "#SS",
		[MCE_STACK - 1] = "#MC",
#if DEBUG_STKSZ > EXCEPTION_STKSZ
		[N_EXCEPTION_STACKS ... N_EXCEPTION_STACKS + DEBUG_STKSZ / EXCEPTION_STKSZ - 2] = "#DB[?]"
#endif
	};
	unsigned k;

	/*
	 * Iterate over all exception stacks, and figure out whether
	 * 'stack' is in one of them:
	 */
	for (k = 0; k < N_EXCEPTION_STACKS; k++) {
		unsigned long end = per_cpu(orig_ist, cpu).ist[k];
		/*
		 * Is 'stack' above this exception frame's end?
		 * If yes then skip to the next frame.
		 */
		if (stack >= end)
			continue;
		/*
		 * Is 'stack' above this exception frame's start address?
		 * If yes then we found the right frame.
		 */
		if (stack >= end - EXCEPTION_STKSZ) {
			/*
			 * Make sure we only iterate through an exception
			 * stack once. If it comes up for the second time
			 * then there's something wrong going on - just
			 * break out and return NULL:
			 */
			if (*usedp & (1U << k))
				break;
			*usedp |= 1U << k;
			*idp = ids[k];
			return (unsigned long *)end;
		}
		/*
		 * If this is a debug stack, and if it has a larger size than
		 * the usual exception stacks, then 'stack' might still
		 * be within the lower portion of the debug stack:
		 */
#if DEBUG_STKSZ > EXCEPTION_STKSZ
		if (k == DEBUG_STACK - 1 && stack >= end - DEBUG_STKSZ) {
			unsigned j = N_EXCEPTION_STACKS - 1;

			/*
			 * Black magic. A large debug stack is composed of
			 * multiple exception stack entries, which we
			 * iterate through now. Dont look:
			 */
			do {
				++j;
				end -= EXCEPTION_STKSZ;
				ids[j][4] = '1' + (j - N_EXCEPTION_STACKS);
			} while (stack < end - EXCEPTION_STKSZ);
			if (*usedp & (1U << j))
				break;
			*usedp |= 1U << j;
			*idp = ids[j];
			return (unsigned long *)end;
		}
#endif
	}
	return NULL;
}

/*
 * x86-64 can have up to three kernel stacks:
 * process stack
 * interrupt stack
 * severe exception (double fault, nmi, stack fault, debug, mce) hardware stack
 */

static inline int valid_stack_ptr(struct thread_info *tinfo, void *p)
{
	void *t = (void *)tinfo;
        return p > t && p < t + THREAD_SIZE - 3;
}

/* Must be called with preemption disabled */
static void dump_trace(unsigned long *stack,
		char *buffer,
		char **str,
		long *len)
{
	const unsigned cpu = smp_processor_id();
	unsigned long *irqstack_end = (unsigned long *)cpu_pda(cpu)->irqstackptr;
	unsigned used = 0;
	struct thread_info *tinfo;

#define HANDLE_STACK(cond) \
	do while (cond) { \
		unsigned long addr = *stack++; \
		if (__kernel_text_address(addr)) { \
			/* \
			 * If the address is either in the text segment of the \
			 * kernel, or in the region which contains vmalloc'ed \
			 * memory, it *may* be the address of a calling \
			 * routine; if so, print it so that someone tracing \
			 * down the cause of the crash will be able to figure \
			 * out the call path that was taken. \
			 */ \
			if (buffer != NULL) { \
				unsigned long *dest = (unsigned long*)*str; \
				*dest = addr; \
			} else { \
				(*len)++; \
			} \
			(*str) += sizeof(unsigned long); \
		} \
	} while (0)

	/*
	 * Print function call entries in all stacks, starting at the
	 * current stack address. If the stacks consist of nested
	 * exceptions
	 */
	for (;;) {
		const char *id;
		unsigned long *estack_end;
		estack_end = trace_in_exception_stack(cpu, (unsigned long)stack,
						&used, &id);

		if (estack_end) {
			HANDLE_STACK (stack < estack_end);
			/*
			 * We link to the next stack via the
			 * second-to-last pointer (index -2 to end) in the
			 * exception stack:
			 */
			stack = (unsigned long *) estack_end[-2];
			continue;
		}
		if (irqstack_end) {
			unsigned long *irqstack;
			irqstack = irqstack_end -
				(IRQSTACKSIZE - 64) / sizeof(*irqstack);

			if (stack >= irqstack && stack < irqstack_end) {
				HANDLE_STACK (stack < irqstack_end);
				/*
				 * We link to the next stack (which would be
				 * the process stack normally) the last
				 * pointer (index -1 to end) in the IRQ stack:
				 */
				stack = (unsigned long *) (irqstack_end[-1]);
				irqstack_end = NULL;
				continue;
			}
		}
		break;
	}

	/*
	 * This handles the process stack:
	 */
	tinfo = current_thread_info();
	HANDLE_STACK (valid_stack_ptr(tinfo, stack));
#undef HANDLE_STACK
}


static char *ltt_serialize_kernel_stack(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	unsigned long *stack = serialize_private;

	if (align)
		str += ltt_align((long)str,
			max(sizeof(int), sizeof(unsigned long)));
	if (buffer)
		*(int*)str = (int)closure->cb_args[0];
	str += sizeof(int);

	if (!buffer)
		closure->cb_args[0] = 0;

	if (align)
		str += ltt_align((long)str, sizeof(unsigned long));
	dump_trace(stack, buffer, &str, &closure->cb_args[0]);

	/* Following alignment for genevent compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}

/* Expects va args : (struct pt_regs *regs) */
static void ltt_trace_kernel_stack(const struct marker *mdata, void *private,
	const char *fmt, ...)
{
	struct ltt_probe_private_data call_data;
	struct pt_regs *regs;
	va_list args;

	va_start(args, fmt);
	regs = va_arg(args, struct pt_regs *);
	if (unlikely(!regs || user_mode_vm(regs)))
		goto end;
	call_data.channel = 0;
	call_data.trace = NULL;
	call_data.force = 0;
	call_data.id = MARKER_CORE_IDS;
	call_data.serialize_private = regs;
	ltt_vtrace(mdata, &call_data, fmt, &args);
end:
	va_end(args);
}
#endif /* CONFIG_LTT_KERNEL_STACK */

#ifdef CONFIG_LTT_PROCESS_STACK
/* FIXME : how could we autodetect this.... ?!? disabled for now. */
#if 0
static void dump_fp_process32_stack(
		struct pt_regs *regs,
		char *buffer,
		char **str,
		long *len)
{
	uint32_t addr;
	uint32_t next_ebp;
	uint32_t *iter;
	uint32_t *dest = (uint32_t*)*str;

	if (buffer)
		*dest = PTR_LOW(regs->rip);
	else
		(*len)++;
	dest++;

	/* Start at the topmost ebp */
	iter = (uint32_t*)PTR_LOW(regs->rbp);

	/* Keep on going until we reach the end of the process' stack limit or
	 * find an invalid ebp. */
	while (!get_user(next_ebp, iter)) {
		/* If another thread corrupts the thread user space stack
		 * concurrently */
		if (buffer)
			if (dest == (uint32_t*)(*str) + *len)
				break;
		if (PTR_LOW(next_ebp) <= (unsigned long)(iter+1))
			break;
		if (get_user(addr, (uint32_t*)PTR_LOW(next_ebp)+1))
			break;
		if (buffer)
			*dest = addr;
		else
			(*len)++;
		dest++;
		iter = (uint32_t*)PTR_LOW(next_ebp);
	}

	/* Concurrently been changed : pad with zero */
	if (buffer)
		while (dest < (uint32_t*)(*str) + *len) {
			*dest = 0;
			dest++;
		}
	*str = (char*)dest;
}
#endif //0

static void dump_nofp_process32_stack(
		struct pt_regs *regs,
		char *buffer,
		char **str,
		long *len)
{
	uint32_t addr;
	uint32_t *iter;
	uint32_t *dest = (uint32_t*)*str;
	uint32_t *prev_iter, *beg_iter;

	if (buffer)
		*dest = PTR_LOW(regs->rip);
	else
		(*len)++;
	dest++;

	/*
	 * FIXME : currently detects code addresses from executable only,
	 * not libraries.
	 */
	/* Start at the top of the user stack */
	prev_iter = beg_iter = iter = (uint32_t*)PTR_LOW(regs->rsp);

	while (!get_user(addr, iter)) {
		if (buffer)
			if (dest == (uint32_t*)(*str) + *len)
				break;
		/* Does this LOOK LIKE an address in the program */
		if (addr > current->mm->start_code &&
			addr < current->mm->end_code) {
			if (buffer)
				*dest = addr;
			else
				(*len)++;
			dest++;
		}
		prev_iter = iter;
		iter++;
		if (iter >
			prev_iter + CONFIG_LTT_PROCESS_MAX_FUNCTION_STACK)
			break;
		if (iter > beg_iter + CONFIG_LTT_PROCESS_MAX_STACK_LEN)
			break;
	}

	/* Concurrently been changed : pad with zero */
	if (buffer)
		while (dest < (uint32_t*)(*str) + *len) {
			*dest = 0;
			dest++;
		}
	*str = (char*)dest;
}

#if 0
static void dump_fp_process64_stack(
		struct pt_regs *regs,
		char *buffer,
		char **str,
		long *len)
{
	uint64_t addr;
	uint64_t next_rbp;
	uint64_t *iter;
	uint64_t *dest = (uint64_t*)*str;

	if (buffer)
		*dest = regs->rip;
	else
		(*len)++;
	dest++;

	/* Start at the topmost rbp */
	iter = (uint64_t*)regs->rbp;

	/* Keep on going until we reach the end of the process' stack limit or
	 * find an invalid ebp. */
	while (!get_user(next_rbp, iter)) {
		/* If another thread corrupts the thread user space stack
		 * concurrently */
		if (buffer)
			if (dest == (uint64_t*)(*str) + *len)
				break;
		if (next_rbp <= (uint64_t)(iter+1))
			break;
		if (get_user(addr, (uint64_t*)next_rbp+1))
			break;
		if (buffer)
			*dest = addr;
		else
			(*len)++;
		dest++;
		iter = (uint64_t*)next_rbp;
	}

	/* Concurrently been changed : pad with zero */
	if (buffer)
		while (dest < (uint64_t*)(*str) + *len) {
			*dest = 0;
			dest++;
		}
	*str = (char*)dest;
}
#endif //0

static void dump_nofp_process64_stack(
		struct pt_regs *regs,
		char *buffer,
		char **str,
		long *len)
{
	uint64_t addr;
	uint64_t *iter;
	uint64_t *dest = (uint64_t*)*str;
	uint64_t *prev_iter, *beg_iter;

	if (buffer)
		*dest = regs->rip;
	else
		(*len)++;
	dest++;

	/*
	 * FIXME : currently detects code addresses from executable only,
	 * not libraries.
	 */
	/* Start at the top of the user stack */
	prev_iter = beg_iter = iter = (uint64_t*) regs->rsp;

	while (!get_user(addr, iter)) {
		if (buffer)
			if (dest == (uint64_t*)(*str) + *len)
				break;
		/* Does this LOOK LIKE an address in the program */
		if (addr > current->mm->start_code &&
			addr < current->mm->end_code) {
			if (buffer)
				*dest = addr;
			else
				(*len)++;
			dest++;
		}
		prev_iter = iter;
		iter++;
		if (iter >
			prev_iter + CONFIG_LTT_PROCESS_MAX_FUNCTION_STACK)
			break;
		if (iter > beg_iter + CONFIG_LTT_PROCESS_MAX_STACK_LEN)
			break;
	}

	/* Concurrently been changed : pad with zero */
	if (buffer)
		while (dest < (uint64_t*)(*str) + *len) {
			*dest = 0;
			dest++;
		}
	*str = (char*)dest;
}

static char *ltt_serialize_process32_stack(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	struct pt_regs *regs;

	regs = serialize_private;

	if (align)
		str += ltt_align((long)str,
			max(sizeof(int), sizeof(uint32_t)));

	if (buffer)
		*(int*)str = (int)closure->cb_args[0];
	str += sizeof(int);

	if (!buffer)
		closure->cb_args[0] = 0;

	if (align)
		str += ltt_align((long)str, sizeof(uint32_t));
	dump_nofp_process32_stack(regs, buffer, &str,
		&closure->cb_args[0]);

	/* Following alignment for genevent compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}

static char *ltt_serialize_process64_stack(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	struct pt_regs *regs;

	regs = serialize_private;

	if (align)
		str += ltt_align((long)str,
			max(sizeof(int), sizeof(uint64_t)));

	if (buffer)
		*(int*)str = (int)closure->cb_args[0];
	str += sizeof(int);

	if (!buffer)
		closure->cb_args[0] = 0;

	if (align)
		str += ltt_align((long)str, sizeof(uint64_t));
	dump_nofp_process64_stack(regs, buffer, &str,
		&closure->cb_args[0]);

	/* Following alignment for genevent compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}


/* Expects va args : (struct pt_regs *regs) */
static void ltt_trace_process32_stack(const struct marker *mdata, void *private,
	const char *fmt, ...)
{
	struct ltt_probe_private_data call_data;
	va_list args;
	struct pt_regs *regs;

	if (!test_thread_flag(TIF_IA32))
		return;

	va_start(args, fmt);
	regs = va_arg(args, struct pt_regs *);
	if (unlikely(!regs || !user_mode_vm(regs)))
		goto end;
	call_data.channel = 0;
	call_data.trace = NULL;
	call_data.force = 0;
	call_data.id = MARKER_CORE_IDS;
	call_data.serialize_private = regs;
	ltt_vtrace(mdata, &call_data, fmt, &args);
end:
	va_end(args);
}

/* Expects va args : (struct pt_regs *regs) */
static void ltt_trace_process64_stack(const struct marker *mdata, void *private,
	const char *fmt, ...)
{
	struct ltt_probe_private_data call_data;
	va_list args;
	struct pt_regs *regs;

	if (test_thread_flag(TIF_IA32))
		return;

	va_start(args, fmt);
	regs = va_arg(args, struct pt_regs *);
	if (unlikely(!regs || !user_mode_vm(regs)))
		goto end;
	call_data.channel = 0;
	call_data.trace = NULL;
	call_data.force = 0;
	call_data.id = MARKER_CORE_IDS;
	call_data.serialize_private = regs;
	ltt_vtrace(mdata, &call_data, fmt, &args);
end:
	va_end(args);
}
#endif /* CONFIG_LTT_PROCESS_STACK */

static struct ltt_available_probe probe_array[] =
{
#ifdef CONFIG_LTT_KERNEL_STACK
	{ "stack_arch_irq_dump_kernel_stack", MARK_NOARGS,
		.probe_func = ltt_trace_kernel_stack,
		.callbacks[0] = ltt_serialize_kernel_stack,
	},
	{ "stack_arch_nmi_dump_kernel_stack", MARK_NOARGS,
		.probe_func = ltt_trace_kernel_stack,
		.callbacks[0] = ltt_serialize_kernel_stack,
	},
	{ "stack_arch_syscall_dump_kernel_stack", MARK_NOARGS,
		.probe_func = ltt_trace_kernel_stack,
		.callbacks[0] = ltt_serialize_kernel_stack,
	},
#endif /* CONFIG_LTT_KERNEL_STACK */
#ifdef CONFIG_LTT_PROCESS_STACK
	{ "stack_arch_irq_dump_process32_stack", MARK_NOARGS,
		.probe_func = ltt_trace_process32_stack,
		.callbacks[0] = ltt_serialize_process32_stack,
	},
	{ "stack_arch_syscall_dump_process32_stack", MARK_NOARGS,
		.probe_func = ltt_trace_process32_stack,
		.callbacks[0] = ltt_serialize_process32_stack,
	},
	{ "stack_arch_irq_dump_process64_stack", MARK_NOARGS,
		.probe_func = ltt_trace_process64_stack,
		.callbacks[0] = ltt_serialize_process64_stack,
	},
	{ "stack_arch_syscall_dump_process64_stack", MARK_NOARGS,
		.probe_func = ltt_trace_process64_stack,
		.callbacks[0] = ltt_serialize_process64_stack,
	},
#endif /* CONFIG_LTT_PROCESS_STACK */
};

static int __init probe_init(void)
{
	int result, i;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++) {
		result = ltt_probe_register(&probe_array[i]);
		if (result)
			printk(KERN_INFO "LTT unable to register probe %s\n",
				probe_array[i].name);
	}
	return 0;
}

static void __exit probe_fini(void)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++) {
		err = ltt_probe_unregister(&probe_array[i]);
		BUG_ON(err);
	}
}

module_init(probe_init);
module_exit(probe_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("Stack x86_64 probe");
