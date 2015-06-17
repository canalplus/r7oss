/*
 * stack arch i386 probe
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

struct stack_dump_args {
	unsigned long ebp;
};

#ifdef CONFIG_LTT_KERNEL_STACK
/* Kernel specific trace stack dump routines, from arch/i386/kernel/traps.c */

/*
 * Event kernel_dump structures
 * ONLY USE IT ON THE CURRENT STACK.
 * It does not protect against other stack modifications and _will_
 * cause corruption upon race between threads.
 *
 * FIXME : using __kernel_text_address here can break things if a module is
 * loaded during tracing. The real function pointers are OK : if they are on our
 * stack, it means that the module refcount must not be 0, but the problem comes
 * from the "false positives" : that that appears to be a function pointer.
 * The solution to this problem :
 * Without frame pointers, we have one version with spinlock irqsave (never call
 * this in NMI context, and another with a trylock, which can fail.
 */

static inline int dump_valid_stack_ptr(struct thread_info *tinfo, void *p)
{
	return	p > (void *)tinfo &&
		p < (void *)tinfo + THREAD_SIZE - 3;
}

static inline unsigned long dump_context_stack(struct thread_info *tinfo,
		unsigned long *stack, unsigned long ebp,
		char *buffer,
		char **str,
		long *len)
{
	unsigned long addr;

#ifdef	CONFIG_FRAME_POINTER
	while (dump_valid_stack_ptr(tinfo, (void *)ebp)) {
		unsigned long new_ebp;
		if (buffer != NULL) {
			unsigned long *dest = (unsigned long*)*str;
			addr = *(unsigned long *)(ebp + 4);
			*dest = addr;
		} else {
			(*len)++;
		}
		(*str) += sizeof(unsigned long);
		new_ebp = *(unsigned long *)ebp;
		if (new_ebp <= ebp)
			break;
		ebp = new_ebp;
	}
#else
	while (dump_valid_stack_ptr(tinfo, stack)) {
		addr = *stack++;
		if (__kernel_text_address(addr)) {
			if (buffer != NULL) {
				unsigned long *dest = (unsigned long*)*str;
				*dest = addr;
			} else {
				(*len)++;
			}
			(*str) += sizeof(unsigned long);
		}
	}
#endif
	return ebp;
}

static void dump_trace(unsigned long * stack,
		unsigned long ebp,
		char *buffer,
		char **str,
		long *len)
{
	while (1) {
		struct thread_info *context;
		context = (struct thread_info *)
			((unsigned long)stack & (~(THREAD_SIZE - 1)));
		ebp = dump_context_stack(context, stack, ebp,
				buffer, str, len);
		stack = (unsigned long*)context->previous_esp;
		if (!stack)
			break;
	}
}

static char *ltt_serialize_kernel_stack(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	struct stack_dump_args *stargs;

	/* Ugly and crude argument passing hack part 2. */
	stargs = serialize_private;

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
	dump_trace((unsigned long*)stargs, stargs->ebp, buffer, &str,
			&closure->cb_args[0]);

	/* Following alignment for genevent
	 * compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}

/* Expects va args : (struct pt_regs *regs) */
static void ltt_trace_kernel_stack(const struct marker *mdata, void *private,
	const char *fmt, ...)
{
	struct ltt_probe_private_data call_data;
	struct stack_dump_args stargs;
	va_list args;
	struct pt_regs *regs;

#ifndef CONFIG_LTT_KERNEL_STACK
	return;
#endif

	va_start(args, fmt);
	regs = va_arg(args, struct pt_regs *);
	if (unlikely(!regs || user_mode_vm(regs)))
		goto end;
	/* Grab ebp right from our regs */
	asm ("movl %%ebp, %0" : "=r" (stargs.ebp) : );
	/* Ugly and crude argument passing hack part 1. */
	call_data.channel = 0;
	call_data.trace = NULL;
	call_data.force = 0;
	call_data.id = MARKER_CORE_IDS;
	call_data.serialize_private = &stargs;
	ltt_vtrace(mdata, &call_data, fmt, &args);
end:
	va_end(args);
}
#endif /* CONFIG_LTT_KERNEL_STACK */

#ifdef CONFIG_LTT_PROCESS_STACK
/* FIXME : how could we autodetect this.... ?!? disabled for now. */
#if 0
static void dump_fp_process_stack(
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
		*dest = regs->eip;
	else
		(*len)++;
	dest++;

	/* Start at the topmost ebp */
	iter = (uint32_t*)regs->ebp;

	/* Keep on going until we reach the end of the process' stack limit or
	 * find an invalid ebp. */
	while (!get_user(next_ebp, iter)) {
		/* If another thread corrupts the thread user space stack
		 * concurrently */
		if (buffer)
			if (dest == (uint32_t*)(*str) + *len)
				break;
		if (next_ebp <= (uint32_t)(iter+1))
			break;
		if (get_user(addr, (uint32_t*)next_ebp+1))
			break;
		if (buffer)
			*dest = addr;
		else
			(*len)++;
		dest++;
		iter = (uint32_t*)next_ebp;
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

static void dump_nofp_process_stack(
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
		*dest = regs->eip;
	else
		(*len)++;
	dest++;

	/*
	 * FIXME : currently detects code addresses from executable only,
	 * not libraries.
	 */
	/* Start at the top of the user stack */
	prev_iter = beg_iter = iter = (uint32_t*) regs->esp;

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

static char *ltt_serialize_process_stack(char *buffer, char *str,
	struct ltt_serialize_closure *closure,
	void *serialize_private,
	int align, const char *fmt, va_list *args)
{
	struct pt_regs *regs;

	/* Ugly and crude argument passing hack part 2 (revisited). */
	regs = serialize_private;

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
	dump_nofp_process_stack(regs, buffer, &str, &closure->cb_args[0]);

	/* Following alignment for genevent
	 * compatibility */
	if (align)
		str += ltt_align((long)str, sizeof(void*));
	return str;
}

/* Expects va args : (struct pt_regs *regs) */
static void ltt_trace_process_stack(const struct marker *mdata, void *private,
	const char *fmt, ...)
{
	struct ltt_probe_private_data call_data;
	va_list args;
	struct pt_regs *regs;

	va_start(args, fmt);
	regs = va_arg(args, struct pt_regs *);
	if (unlikely(!regs || !user_mode_vm(regs)))
		goto end;
	/* Ugly and crude argument passing hack part 1 (revisited). */
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
		.probe_func = ltt_trace_process_stack,
		.callbacks[0] = ltt_serialize_process_stack,
	},
	{ "stack_arch_syscall_dump_process32_stack", MARK_NOARGS,
		.probe_func = ltt_trace_process_stack,
		.callbacks[0] = ltt_serialize_process_stack,
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
MODULE_DESCRIPTION("Stack i386 probe");
