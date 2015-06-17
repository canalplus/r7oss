/*
 * kernel/kgdb.c
 *
 * Maintainer: Jason Wessel <jason.wessel@windriver.com>
 *
 * Copyright (C) 2000-2001 VERITAS Software Corporation.
 * Copyright (C) 2002-2004 Timesys Corporation
 * Copyright (C) 2003-2004 Amit S. Kale <amitkale@linsyssoft.com>
 * Copyright (C) 2004 Pavel Machek <pavel@suse.cz>
 * Copyright (C) 2004-2006 Tom Rini <trini@kernel.crashing.org>
 * Copyright (C) 2004-2006 LinSysSoft Technologies Pvt. Ltd.
 * Copyright (C) 2005-2007 Wind River Systems, Inc.
 * Copyright (C) 2007 MontaVista Software, Inc.
 *
 * Contributors at various stages not listed above:
 *  Jason Wessel ( jason.wessel@windriver.com )
 *  George Anzinger <george@mvista.com>
 *  Anurekh Saxena (anurekh.saxena@timesys.com)
 *  Lake Stevens Instrument Division (Glenn Engel)
 *  Jim Kingdon, Cygnus Support.
 *
 * Original KGDB stub: David Grothe <dave@gcom.com>,
 * Tigran Aivazian <tigran@sco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <linux/reboot.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>
#include <asm/system.h>
#include <linux/kgdb.h>
#include <asm/atomic.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <asm/byteorder.h>
#include <linux/clocksource.h>

extern int pid_max;
/* How many times to count all of the waiting CPUs */
#define ROUNDUP_WAIT		640000	/* Arbitrary, increase if needed. */
#define BUF_THREAD_ID_SIZE	16

/*
 * kgdb_initialized with a value of 1 indicates that kgdb is setup and is
 * all ready to serve breakpoints and other kernel exceptions.  A value of
 * -1 indicates that we have tried to initialize early, and need to try
 * again later.
 */
int kgdb_initialized;
/* Is a host GDB connected to us? */
int kgdb_connected;
EXPORT_SYMBOL(kgdb_connected);

/* Could we be about to try and access a bad memory location?
 * If so we also need to flag this has happened. */
int kgdb_may_fault;

/* All the KGDB handlers are installed */
int kgdb_from_module_registered;
/* Guard for recursive entry */
static int exception_level;

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Common architecture KGDB stub");
#ifdef CONFIG_KGDB_ATTACH_WAIT
static int attachwait = 1;
#else
static int attachwait;
#endif
module_param(attachwait, int, 0644);
MODULE_PARM_DESC(attachwait, "Wait for remote debugger"
		" after an exception");
MODULE_LICENSE("GPL");

/* We provide a kgdb_io_ops structure that may be overriden. */
struct kgdb_io __attribute__((weak)) kgdb_io_ops;
EXPORT_SYMBOL(kgdb_io_ops);

static struct kgdb_io kgdb_io_ops_prev[MAX_KGDB_IO_HANDLERS];
static int kgdb_io_handler_cnt;

/*
 * Holds information about breakpoints in a kernel. These breakpoints are
 * added and removed by gdb.
 */
struct kgdb_bkpt kgdb_break[MAX_BREAKPOINTS];

struct kgdb_arch *kgdb_ops = &arch_kgdb_ops;

static const char hexchars[] = "0123456789abcdef";

static spinlock_t slavecpulocks[NR_CPUS];
static atomic_t procindebug[NR_CPUS];
atomic_t kgdb_setting_breakpoint;
EXPORT_SYMBOL(kgdb_setting_breakpoint);
struct task_struct *kgdb_usethread, *kgdb_contthread;

int debugger_step;
static atomic_t kgdb_sync = ATOMIC_INIT(-1);
atomic_t debugger_active;
EXPORT_SYMBOL(debugger_active);

/* Our I/O buffers. */
static char remcom_in_buffer[BUFMAX];
static char remcom_out_buffer[BUFMAX];
/* Storage for the registers, in GDB format. */
static unsigned long gdb_regs[(NUMREGBYTES + sizeof(unsigned long) - 1) /
			      sizeof(unsigned long)];
/* Storage of registers for handling a fault. */
unsigned long kgdb_fault_jmp_regs[NUMCRITREGBYTES / sizeof(unsigned long)]
 JMP_REGS_ALIGNMENT;
static int kgdb_notify_reboot(struct notifier_block *this,
				unsigned long code, void *x);
struct debuggerinfo_struct {
	void *debuggerinfo;
	struct task_struct *task;
} kgdb_info[NR_CPUS];

/* to keep track of the CPU which is doing the single stepping*/
atomic_t cpu_doing_single_step = ATOMIC_INIT(-1);
int kgdb_softlock_skip[NR_CPUS];

/* reboot notifier block */
static struct notifier_block kgdb_reboot_notifier = {
	.notifier_call  = kgdb_notify_reboot,
	.next           = NULL,
	.priority       = INT_MAX,
};

int __attribute__((weak))
     kgdb_validate_break_address(unsigned long addr)
{
	int error = 0;
	char tmp_variable[BREAK_INSTR_SIZE];
	error = kgdb_get_mem((char *)addr, tmp_variable, BREAK_INSTR_SIZE);
	return error;
}

int __attribute__((weak))
     kgdb_arch_set_breakpoint(unsigned long addr, char *saved_instr)
{
	int error = kgdb_get_mem((char *)addr,
		saved_instr, BREAK_INSTR_SIZE);
	if (error < 0)
		return error;

	error = kgdb_set_mem((char *)addr, kgdb_ops->gdb_bpt_instr,
						 BREAK_INSTR_SIZE);
	if (error < 0)
			return error;
	return 0;
}

int __attribute__((weak))
     kgdb_arch_remove_breakpoint(unsigned long addr, char *bundle)
{

	int error = kgdb_set_mem((char *)addr, (char *)bundle,
		BREAK_INSTR_SIZE);
	if (error < 0)
			return error;
	return 0;
}

unsigned long __attribute__((weak))
    kgdb_arch_pc(int exception, struct pt_regs *regs)
{
	return instruction_pointer(regs);
}

static int hex(char ch)
{
	if ((ch >= 'a') && (ch <= 'f'))
		return (ch - 'a' + 10);
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	return (-1);
}

/* scan for the sequence $<data>#<checksum>	*/
static void get_packet(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int count;
	char ch;

	if (!kgdb_io_ops.read_char)
		return;
	do {
		/* Spin and wait around for the start character, ignore all
		 * other characters */
		while ((ch = (kgdb_io_ops.read_char())) != '$')
			;
		kgdb_connected = 1;
		checksum = 0;
		xmitcsum = -1;

		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < (BUFMAX - 1)) {
			ch = kgdb_io_ops.read_char();
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}
		buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(kgdb_io_ops.read_char()) << 4;
			xmitcsum += hex(kgdb_io_ops.read_char());

			if (checksum != xmitcsum)
				/* failed checksum */
				kgdb_io_ops.write_char('-');
			else
				/* successful transfer */
				kgdb_io_ops.write_char('+');
			if (kgdb_io_ops.flush)
				kgdb_io_ops.flush();
		}
	} while (checksum != xmitcsum);
}

/*
 * Send the packet in buffer.
 * Check for gdb connection if asked for.
 */
static void put_packet(char *buffer)
{
	unsigned char checksum;
	int count;
	char ch;

	if (!kgdb_io_ops.write_char)
		return;
	/* $<packet info>#<checksum>. */
	while (1) {
		kgdb_io_ops.write_char('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			kgdb_io_ops.write_char(ch);
			checksum += ch;
			count++;
		}

		kgdb_io_ops.write_char('#');
		kgdb_io_ops.write_char(hexchars[checksum >> 4]);
		kgdb_io_ops.write_char(hexchars[checksum % 16]);
		if (kgdb_io_ops.flush)
			kgdb_io_ops.flush();

		/* Now see what we get in reply. */
		ch = kgdb_io_ops.read_char();

		if (ch == 3)
			ch = kgdb_io_ops.read_char();

		/* If we get an ACK, we are done. */
		if (ch == '+')
			return;

		/* If we get the start of another packet, this means
		 * that GDB is attempting to reconnect.  We will NAK
		 * the packet being sent, and stop trying to send this
		 * packet. */
		if (ch == '$') {
			kgdb_io_ops.write_char('-');
			if (kgdb_io_ops.flush)
				kgdb_io_ops.flush();
			return;
		}
	}
}

/*
 * Wrap kgdb_fault_setjmp() call to enable the kernel faults and save/restore
 * the state before/after a fault has happened.
 * Note that it *must* be inline to work correctly.
 */
static inline int fault_setjmp(void)
{
#ifdef CONFIG_PREEMPT
	int count = preempt_count();
#endif

	/*
	 * kgdb_fault_setjmp() returns 0 after the jump buffer has been setup,
	 * and non-zero when an expected kernel fault has happened.
	 */
	if (kgdb_fault_setjmp(kgdb_fault_jmp_regs) == 0) {
		kgdb_may_fault = 1;
		return 0;
	} else {
#ifdef CONFIG_PREEMPT
		preempt_count() = count;
#endif
		kgdb_may_fault = 0;
		return 1;
	}
}

/*
 * Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null). May return an error.
 */
char *kgdb_mem2hex(char *mem, char *buf, int count)
{
	if (fault_setjmp() != 0)
		return ERR_PTR(-EINVAL);

	/*
	 * Accessing some registers in a single load instruction is
	 * required to avoid bad side effects for some I/O registers.
	 */
	if ((count == 2) && (((long)mem & 1) == 0)) {
		unsigned short tmp_s = *(unsigned short *)mem;

		mem += 2;
#ifdef __BIG_ENDIAN
		*buf++ = hexchars[(tmp_s >> 12) & 0xf];
		*buf++ = hexchars[(tmp_s >> 8) & 0xf];
		*buf++ = hexchars[(tmp_s >> 4) & 0xf];
		*buf++ = hexchars[tmp_s & 0xf];
#else
		*buf++ = hexchars[(tmp_s >> 4) & 0xf];
		*buf++ = hexchars[tmp_s & 0xf];
		*buf++ = hexchars[(tmp_s >> 12) & 0xf];
		*buf++ = hexchars[(tmp_s >> 8) & 0xf];
#endif
	} else if ((count == 4) && (((long)mem & 3) == 0)) {
		unsigned long tmp_l = *(unsigned int *)mem;

		mem += 4;
#ifdef __BIG_ENDIAN
		*buf++ = hexchars[(tmp_l >> 28) & 0xf];
		*buf++ = hexchars[(tmp_l >> 24) & 0xf];
		*buf++ = hexchars[(tmp_l >> 20) & 0xf];
		*buf++ = hexchars[(tmp_l >> 16) & 0xf];
		*buf++ = hexchars[(tmp_l >> 12) & 0xf];
		*buf++ = hexchars[(tmp_l >> 8) & 0xf];
		*buf++ = hexchars[(tmp_l >> 4) & 0xf];
		*buf++ = hexchars[tmp_l & 0xf];
#else
		*buf++ = hexchars[(tmp_l >> 4) & 0xf];
		*buf++ = hexchars[tmp_l & 0xf];
		*buf++ = hexchars[(tmp_l >> 12) & 0xf];
		*buf++ = hexchars[(tmp_l >> 8) & 0xf];
		*buf++ = hexchars[(tmp_l >> 20) & 0xf];
		*buf++ = hexchars[(tmp_l >> 16) & 0xf];
		*buf++ = hexchars[(tmp_l >> 28) & 0xf];
		*buf++ = hexchars[(tmp_l >> 24) & 0xf];
#endif
#ifdef CONFIG_64BIT
	} else if ((count == 8) && (((long)mem & 7) == 0)) {
		unsigned long long tmp_ll = *(unsigned long long *)mem;

		mem += 8;
#ifdef __BIG_ENDIAN
		*buf++ = hexchars[(tmp_ll >> 60) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 56) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 52) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 48) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 44) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 40) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 36) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 32) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 28) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 24) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 20) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 16) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 12) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 8) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 4) & 0xf];
		*buf++ = hexchars[tmp_ll & 0xf];
#else
		*buf++ = hexchars[(tmp_ll >> 4) & 0xf];
		*buf++ = hexchars[tmp_ll & 0xf];
		*buf++ = hexchars[(tmp_ll >> 12) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 8) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 20) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 16) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 28) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 24) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 36) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 32) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 44) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 40) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 52) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 48) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 60) & 0xf];
		*buf++ = hexchars[(tmp_ll >> 56) & 0xf];
#endif
#endif
	} else
		while (count-- > 0) {
			unsigned char ch = *mem++;

			*buf++ = hexchars[ch >> 4];
			*buf++ = hexchars[ch & 0xf];
		}
	kgdb_may_fault = 0;
	*buf = 0;
	return (buf);
}

/*
 * Copy the binary array pointed to by buf into mem.  Fix $, #, and
 * 0x7d escaped with 0x7d.  Return a pointer to the character after
 * the last byte written.
 */
static char *kgdb_ebin2mem(char *buf, char *mem, int count)
{
	if (fault_setjmp() != 0)
		return ERR_PTR(-EINVAL);

	for (; count > 0; count--, buf++)
		if (*buf == 0x7d)
			*mem++ = *(++buf) ^ 0x20;
		else
			*mem++ = *buf;

	kgdb_may_fault = 0;
	return mem;
}

/*
 * Convert the hex array pointed to by buf into binary to be placed in mem.
 * Return a pointer to the character AFTER the last byte written.
 * May return an error.
 */
char *kgdb_hex2mem(char *buf, char *mem, int count)
{
	if (fault_setjmp() != 0)
		return ERR_PTR(-EINVAL);

	if ((count == 2) && (((long)mem & 1) == 0)) {
		unsigned short tmp_s = 0;

#ifdef __BIG_ENDIAN
		tmp_s |= hex(*buf++) << 12;
		tmp_s |= hex(*buf++) << 8;
		tmp_s |= hex(*buf++) << 4;
		tmp_s |= hex(*buf++);
#else
		tmp_s |= hex(*buf++) << 4;
		tmp_s |= hex(*buf++);
		tmp_s |= hex(*buf++) << 12;
		tmp_s |= hex(*buf++) << 8;
#endif
		*(unsigned short *)mem = tmp_s;
		mem += 2;
	} else if ((count == 4) && (((long)mem & 3) == 0)) {
		unsigned long tmp_l = 0;

#ifdef __BIG_ENDIAN
		tmp_l |= hex(*buf++) << 28;
		tmp_l |= hex(*buf++) << 24;
		tmp_l |= hex(*buf++) << 20;
		tmp_l |= hex(*buf++) << 16;
		tmp_l |= hex(*buf++) << 12;
		tmp_l |= hex(*buf++) << 8;
		tmp_l |= hex(*buf++) << 4;
		tmp_l |= hex(*buf++);
#else
		tmp_l |= hex(*buf++) << 4;
		tmp_l |= hex(*buf++);
		tmp_l |= hex(*buf++) << 12;
		tmp_l |= hex(*buf++) << 8;
		tmp_l |= hex(*buf++) << 20;
		tmp_l |= hex(*buf++) << 16;
		tmp_l |= hex(*buf++) << 28;
		tmp_l |= hex(*buf++) << 24;
#endif
		*(unsigned long *)mem = tmp_l;
		mem += 4;
	} else {
		int i;

		for (i = 0; i < count; i++) {
			unsigned char ch = hex(*buf++) << 4;

			ch |= hex(*buf++);
			*mem++ = ch;
		}
	}
	kgdb_may_fault = 0;
	return (mem);
}

/*
 * While we find nice hex chars, build a long_val.
 * Return number of chars processed.
 */
int kgdb_hex2long(char **ptr, long *long_val)
{
	int hex_val;
	int num = 0;

	*long_val = 0;

	while (**ptr) {
		hex_val = hex(**ptr);
		if (hex_val >= 0) {
			*long_val = (*long_val << 4) | hex_val;
			num++;
		} else
			break;

		(*ptr)++;
	}

	return (num);
}

/* Write memory due to an 'M' or 'X' packet. */
static char *write_mem_msg(int binary)
{
	char *ptr = &remcom_in_buffer[1];
	unsigned long addr;
	unsigned long length;

	if (kgdb_hex2long(&ptr, &addr) > 0 && *(ptr++) == ',' &&
	    kgdb_hex2long(&ptr, &length) > 0 && *(ptr++) == ':') {
		if (binary)
			ptr = kgdb_ebin2mem(ptr, (char *)addr, length);
		else
			ptr = kgdb_hex2mem(ptr, (char *)addr, length);
		if (CACHE_FLUSH_IS_SAFE)
			flush_icache_range(addr, addr + length + 1);
		if (IS_ERR(ptr))
			return ptr;
		return NULL;
	}

	return ERR_PTR(-EINVAL);
}

static inline char *pack_hex_byte(char *pkt, int byte)
{
	*pkt++ = hexchars[(byte >> 4) & 0xf];
	*pkt++ = hexchars[(byte & 0xf)];
	return pkt;
}

static inline void error_packet(char *pkt, int error)
{
	error = -error;
	pkt[0] = 'E';
	pkt[1] = hexchars[(error / 10)];
	pkt[2] = hexchars[(error % 10)];
	pkt[3] = '\0';
}

static char *pack_threadid(char *pkt, unsigned char *id)
{
	char *limit;

	limit = pkt + BUF_THREAD_ID_SIZE;
	while (pkt < limit)
		pkt = pack_hex_byte(pkt, *id++);

	return pkt;
}

void int_to_threadref(unsigned char *id, int value)
{
	unsigned char *scan;
	int i = 4;

	scan = (unsigned char *)id;
	while (i--)
		*scan++ = 0;
	*scan++ = (value >> 24) & 0xff;
	*scan++ = (value >> 16) & 0xff;
	*scan++ = (value >> 8) & 0xff;
	*scan++ = (value & 0xff);
}

static struct task_struct *getthread(struct pt_regs *regs, int tid)
{
	if (init_pid_ns.last_pid == 0)
		return current;

	if (num_online_cpus() &&
	    (tid >= pid_max + num_online_cpus() + kgdb_ops->shadowth))
		return NULL;

	if (kgdb_ops->shadowth && (tid >= pid_max + num_online_cpus()))
		return kgdb_get_shadow_thread(regs, tid - pid_max -
					      num_online_cpus());

	if (tid >= pid_max)
		return idle_task(tid - pid_max);

	if (!tid)
		return NULL;

	return find_task_by_pid(tid);
}

#ifdef CONFIG_SMP
static void kgdb_wait(struct pt_regs *regs)
{
	unsigned long flags;
	int processor;

	local_irq_save(flags);
	processor = raw_smp_processor_id();
	kgdb_info[processor].debuggerinfo = regs;
	kgdb_info[processor].task = current;
	atomic_set(&procindebug[processor], 1);

	/* The master processor must be active to enter here, but this is
	 * gaurd in case the master processor had not been selected if
	 * this was an entry via nmi.
	 */
	while (!atomic_read(&debugger_active))
		cpu_relax();

	/* Wait till master processor goes completely into the debugger.
	 */
	while (!atomic_read(&procindebug[atomic_read(&debugger_active) - 1])) {
		int i = 10;	/* an arbitrary number */

		while (--i)
			cpu_relax();
	}

	/* Wait till master processor is done with debugging */
	spin_lock(&slavecpulocks[processor]);

	kgdb_info[processor].debuggerinfo = NULL;
	kgdb_info[processor].task = NULL;

	/* fix up hardware debug registers on local cpu */
	if (kgdb_ops->correct_hw_break)
		kgdb_ops->correct_hw_break();
	/* Signal the master processor that we are done */
	atomic_set(&procindebug[processor], 0);
	spin_unlock(&slavecpulocks[processor]);
	clocksource_touch_watchdog();
	kgdb_softlock_skip[processor] = 1;
	local_irq_restore(flags);
}
#endif

int kgdb_get_mem(char *addr, unsigned char *buf, int count)
{
	if (fault_setjmp() != 0)
		return -EINVAL;

	while (count) {
		if ((unsigned long)addr < TASK_SIZE) {
			kgdb_may_fault = 0;
			return -EINVAL;
		}
		*buf++ = *addr++;
		count--;
	}
	kgdb_may_fault = 0;
	return 0;
}

int kgdb_set_mem(char *addr, unsigned char *buf, int count)
{
	if (fault_setjmp() != 0)
		return -EINVAL;

	while (count) {
		if ((unsigned long)addr < TASK_SIZE) {
			kgdb_may_fault = 0;
			return -EINVAL;
		}
		*addr++ = *buf++;
		count--;
	}
	kgdb_may_fault = 0;
	return 0;
}
int kgdb_activate_sw_breakpoints(void)
{
	int i;
	int error = 0;
	unsigned long addr;
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != bp_set)
			continue;
		addr = kgdb_break[i].bpt_addr;
		if ((error = kgdb_arch_set_breakpoint(addr,
					kgdb_break[i].saved_instr)))
			return error;

		if (CACHE_FLUSH_IS_SAFE) {
			if (current->mm && addr < TASK_SIZE)
				flush_cache_range(current->mm->mmap_cache,
						addr, addr + BREAK_INSTR_SIZE);
			else
				flush_icache_range(addr, addr +
						BREAK_INSTR_SIZE);
		}

		kgdb_break[i].state = bp_active;
	}
	return 0;
}

static int kgdb_set_sw_break(unsigned long addr)
{
	int i;
	int breakno = -1;
	int error = kgdb_validate_break_address(addr);
	if (error < 0)
		return error;
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == bp_set) &&
			(kgdb_break[i].bpt_addr == addr))
			return -EEXIST;
	}
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state == bp_removed &&
				kgdb_break[i].bpt_addr == addr) {
			breakno = i;
			break;
		}
	}

	if (breakno == -1) {
		for (i = 0; i < MAX_BREAKPOINTS; i++) {
			if (kgdb_break[i].state == bp_none) {
				breakno = i;
				break;
			}
		}
	}
	if (breakno == -1)
		return -E2BIG;

	kgdb_break[breakno].state = bp_set;
	kgdb_break[breakno].type = bp_breakpoint;
	kgdb_break[breakno].bpt_addr = addr;

	return 0;
}

int kgdb_deactivate_sw_breakpoints(void)
{
	int i;
	int error = 0;
	unsigned long addr;
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != bp_active)
			continue;
		addr = kgdb_break[i].bpt_addr;
		if ((error = kgdb_arch_remove_breakpoint(addr,
					kgdb_break[i].saved_instr)))
			return error;

		if (CACHE_FLUSH_IS_SAFE && current->mm &&
				addr < TASK_SIZE)
			flush_cache_range(current->mm->mmap_cache,
					addr, addr + BREAK_INSTR_SIZE);
		else if (CACHE_FLUSH_IS_SAFE)
			flush_icache_range(addr,
					addr + BREAK_INSTR_SIZE);
		kgdb_break[i].state = bp_set;
	}
	return 0;
}

static int kgdb_remove_sw_break(unsigned long addr)
{
	int i;

	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == bp_set) &&
			(kgdb_break[i].bpt_addr == addr)) {
			kgdb_break[i].state = bp_removed;
			return 0;
		}
	}
	return -ENOENT;
}

int kgdb_isremovedbreak(unsigned long addr)
{
	int i;
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == bp_removed) &&
			(kgdb_break[i].bpt_addr == addr))
			return 1;
	}
	return 0;
}

int remove_all_break(void)
{
	int i;
	int error;
	unsigned long addr;

	/* Clear memory breakpoints. */
	for (i = 0; i < MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != bp_set)
			continue;
		addr = kgdb_break[i].bpt_addr;
		if ((error = kgdb_arch_remove_breakpoint(addr,
					kgdb_break[i].saved_instr)))
			return error;
		kgdb_break[i].state = bp_removed;
	}

	/* Clear hardware breakpoints. */
	if (kgdb_ops->remove_all_hw_break)
		kgdb_ops->remove_all_hw_break();

	return 0;
}

static inline int shadow_pid(int realpid)
{
	if (realpid)
		return realpid;

	return pid_max + raw_smp_processor_id();
}

static char gdbmsgbuf[BUFMAX + 1];
static void kgdb_msg_write(const char *s, int len)
{
	int i;
	int wcount;
	char *bufptr;

	/* 'O'utput */
	gdbmsgbuf[0] = 'O';

	/* Fill and send buffers... */
	while (len > 0) {
		bufptr = gdbmsgbuf + 1;

		/* Calculate how many this time */
		if ((len << 1) > (BUFMAX - 2))
			wcount = (BUFMAX - 2) >> 1;
		else
			wcount = len;

		/* Pack in hex chars */
		for (i = 0; i < wcount; i++)
			bufptr = pack_hex_byte(bufptr, s[i]);
		*bufptr = '\0';

		/* Move up */
		s += wcount;
		len -= wcount;

		/* Write packet */
		put_packet(gdbmsgbuf);
	}
}

/* Return true if there is a valid kgdb I/O module.  Also if no
 * debugger is attached a message can be printed to the console about
 * waiting for the debugger to attach.
 *
 * The print_wait argument is only to be true when called from inside
 * the core kgdb_handle_exception, because it will wait for the
 * debugger to attach.
 */
int kgdb_io_ready(int print_wait)
{
	if (!kgdb_io_ops.read_char)
		return 0;
	if (kgdb_connected)
		return 1;
	if (atomic_read(&kgdb_setting_breakpoint))
		return 1;
	if (!attachwait)
		return 0;
	if (print_wait)
		printk(KERN_CRIT "KGDB: Waiting for remote debugger\n");
	return 1;
}


/*
 * This function does all command procesing for interfacing to gdb.
 *
 * Locking hierarchy:
 *	interface locks, if any (begin_session)
 *	kgdb lock (debugger_active)
 *
 * Note that since we can be in here prior to our cpumask being filled
 * out, we err on the side of caution and loop over NR_CPUS instead
 * of a for_each_online_cpu.
 *
 */
int kgdb_handle_exception(int ex_vector, int signo, int err_code,
			  struct pt_regs *linux_regs)
{
	unsigned long length;
	unsigned long addr;
	char *ptr;
	unsigned long flags;
	unsigned i;
	long threadid;
	unsigned char thref[8];
	struct task_struct *thread = NULL;
	unsigned procid;
	int numshadowth = num_online_cpus() + kgdb_ops->shadowth;
	long kgdb_usethreadid = 0;
	int error = 0;
	int all_cpus_synced = 0;
	struct pt_regs *shadowregs;
	int processor = raw_smp_processor_id();
	void *local_debuggerinfo;
	int pass_exception = 0;

	/* Panic on recursive debugger calls. */
	if (atomic_read(&debugger_active) == raw_smp_processor_id() + 1) {
		exception_level++;
		addr = kgdb_arch_pc(ex_vector, linux_regs);
		kgdb_deactivate_sw_breakpoints();
		/* If the break point removed ok at the place exception
		 * occurred, try to recover and print a warning to the end
		 * user because the user planted a breakpoint in a place that
		 * KGDB needs in order to function.
		 */
		if (kgdb_remove_sw_break(addr) == 0) {
			exception_level = 0;
			kgdb_skipexception(ex_vector, linux_regs);
			kgdb_activate_sw_breakpoints();
			printk(KERN_CRIT
			"KGDB: re-enter error: breakpoint removed\n");
			WARN_ON(1);
			return 0;
		}
		remove_all_break();
		kgdb_skipexception(ex_vector, linux_regs);
		if (exception_level > 1)
			panic("Recursive entry to debugger");

		printk(KERN_CRIT
			"KGDB: re-enter exception: ALL breakpoints killed\n");
		panic("Recursive entry to debugger");
		return 0;
	}

 acquirelock:

	/*
	 * Interrupts will be restored by the 'trap return' code, except when
	 * single stepping.
	 */
	local_irq_save(flags);

	/* Hold debugger_active */
	procid = raw_smp_processor_id();

	while (1) {
		int i = 25;	/* an arbitrary number */
		if (atomic_read(&kgdb_sync) < 0 &&
			atomic_inc_and_test(&kgdb_sync)) {
			atomic_set(&debugger_active, procid + 1);
			break;
		}

		while (--i)
			cpu_relax();

		if (atomic_read(&cpu_doing_single_step) != -1 &&
				atomic_read(&cpu_doing_single_step) != procid)
			udelay(1);
	}

	/*
	 * Don't enter if the last instance of the exception handler wanted to
	 * come into the debugger again.
	 */
	if (atomic_read(&cpu_doing_single_step) != -1 &&
	    atomic_read(&cpu_doing_single_step) != procid) {
		atomic_set(&debugger_active, 0);
		atomic_set(&kgdb_sync, -1);
		clocksource_touch_watchdog();
		kgdb_softlock_skip[procid] = 1;
		local_irq_restore(flags);
		goto acquirelock;
	}

	if (!kgdb_io_ready(1)) {
		error = 1;
		goto kgdb_restore;
	}

	/*
	* Don't enter if we have hit a removed breakpoint.
	*/
	if (kgdb_skipexception(ex_vector, linux_regs))
		goto kgdb_restore;

	/*
	 * Call the I/O drivers pre_exception routine
	 * if the I/O driver defined one
	 */
	if (kgdb_io_ops.pre_exception)
		kgdb_io_ops.pre_exception();

	kgdb_info[processor].debuggerinfo = linux_regs;
	kgdb_info[processor].task = current;

	kgdb_disable_hw_debug(linux_regs);

	if (!debugger_step || !kgdb_contthread)
		for (i = 0; i < NR_CPUS; i++)
			spin_lock(&slavecpulocks[i]);

#ifdef CONFIG_SMP
	/* Make sure we get the other CPUs */
	if (!debugger_step || !kgdb_contthread)
		kgdb_roundup_cpus(flags);
#endif

	/* spin_lock code is good enough as a barrier so we don't
	 * need one here */
	atomic_set(&procindebug[processor], 1);

	/* Wait a reasonable time for the other CPUs to be notified and
	 * be waiting for us.  Very early on this could be imperfect
	 * as num_online_cpus() could be 0.*/
	for (i = 0; i < ROUNDUP_WAIT; i++) {
		int cpu;
		int num = 0;
		for (cpu = 0; cpu < NR_CPUS; cpu++) {
			if (atomic_read(&procindebug[cpu]))
				num++;
		}
		if (num >= num_online_cpus()) {
			all_cpus_synced = 1;
			break;
		}
	}

	/* Clear the out buffer. */
	memset(remcom_out_buffer, 0, sizeof(remcom_out_buffer));

	/* Master processor is completely in the debugger */
	kgdb_post_master_code(linux_regs, ex_vector, err_code);
	kgdb_deactivate_sw_breakpoints();
	debugger_step = 0;
	kgdb_contthread = NULL;
	exception_level = 0;

	if (kgdb_connected) {
		/* If we're still unable to roundup all of the CPUs,
		 * send an 'O' packet informing the user again. */
		if (!all_cpus_synced)
			kgdb_msg_write("Not all CPUs have been synced for "
				       "KGDB\n", 39);
		/* Reply to host that an exception has occurred */
		ptr = remcom_out_buffer;
		*ptr++ = 'T';
		*ptr++ = hexchars[(signo >> 4) % 16];
		*ptr++ = hexchars[signo % 16];
		ptr += strlen(strcpy(ptr, "thread:"));
		int_to_threadref(thref, shadow_pid(current->pid));
		ptr = pack_threadid(ptr, thref);
		*ptr++ = ';';

		put_packet(remcom_out_buffer);
	}

	kgdb_usethread = kgdb_info[processor].task;
	kgdb_usethreadid = shadow_pid(kgdb_info[processor].task->pid);

	while (kgdb_io_ops.read_char) {
		char *bpt_type;
		error = 0;

		/* Clear the out buffer. */
		memset(remcom_out_buffer, 0, sizeof(remcom_out_buffer));

		get_packet(remcom_in_buffer);

		switch (remcom_in_buffer[0]) {
		case '?':
			/* We know that this packet is only sent
			 * during initial connect.  So to be safe,
			 * we clear out our breakpoints now incase
			 * GDB is reconnecting. */
			remove_all_break();
			/* Also, if we haven't been able to roundup all
			 * CPUs, send an 'O' packet informing the user
			 * as much.  Only need to do this once. */
			if (!all_cpus_synced)
				kgdb_msg_write("Not all CPUs have been "
					       "synced for KGDB\n", 39);
			remcom_out_buffer[0] = 'S';
			remcom_out_buffer[1] = hexchars[signo >> 4];
			remcom_out_buffer[2] = hexchars[signo % 16];
			break;

		case 'g':	/* return the value of the CPU registers */
			thread = kgdb_usethread;

			if (!thread) {
				thread = kgdb_info[processor].task;
				local_debuggerinfo =
				    kgdb_info[processor].debuggerinfo;
			} else {
				local_debuggerinfo = NULL;
				for (i = 0; i < NR_CPUS; i++) {
					/* Try to find the task on some other
					 * or possibly this node if we do not
					 * find the matching task then we try
					 * to approximate the results.
					 */
					if (thread == kgdb_info[i].task)
						local_debuggerinfo =
						    kgdb_info[i].debuggerinfo;
				}
			}

			/* All threads that don't have debuggerinfo should be
			 * in __schedule() sleeping, since all other CPUs
			 * are in kgdb_wait, and thus have debuggerinfo. */
			if (kgdb_ops->shadowth &&
			    kgdb_usethreadid >= pid_max + num_online_cpus()) {
				shadowregs = kgdb_shadow_regs(linux_regs,
							      kgdb_usethreadid -
							      pid_max -
							      num_online_cpus
							      ());
				if (!shadowregs) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}
				regs_to_gdb_regs(gdb_regs, shadowregs);
			} else if (local_debuggerinfo)
				regs_to_gdb_regs(gdb_regs, local_debuggerinfo);
			else {
				/* Pull stuff saved during
				 * switch_to; nothing else is
				 * accessible (or even particularly relevant).
				 * This should be enough for a stack trace. */
				sleeping_thread_to_gdb_regs(gdb_regs, thread);
			}
			kgdb_mem2hex((char *)gdb_regs, remcom_out_buffer,
				     NUMREGBYTES);
			break;

			/* set the value of the CPU registers - return OK */
		case 'G':
			kgdb_hex2mem(&remcom_in_buffer[1], (char *)gdb_regs,
				     NUMREGBYTES);

			if (kgdb_usethread && kgdb_usethread != current)
				error_packet(remcom_out_buffer, -EINVAL);
			else {
				gdb_regs_to_regs(gdb_regs, linux_regs);
				strcpy(remcom_out_buffer, "OK");
			}
			break;

			/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
		case 'm':
			ptr = &remcom_in_buffer[1];
			if (kgdb_hex2long(&ptr, &addr) > 0 && *ptr++ == ',' &&
			    kgdb_hex2long(&ptr, &length) > 0) {
				ptr = kgdb_mem2hex((char *)addr,
					remcom_out_buffer,
					length);
				if (IS_ERR(ptr))
					error_packet(remcom_out_buffer,
						     PTR_ERR(ptr));
			} else
				error_packet(remcom_out_buffer, -EINVAL);
			break;

			/* MAA..AA,LLLL: Write LLLL bytes at address AA..AA */
		case 'M':
			ptr = write_mem_msg(0);
			if (IS_ERR(ptr))
				error_packet(remcom_out_buffer, PTR_ERR(ptr));
			else
				strcpy(remcom_out_buffer, "OK");
			break;
			/* XAA..AA,LLLL: Write LLLL bytes at address AA..AA */
		case 'X':
			ptr = write_mem_msg(1);
			if (IS_ERR(ptr))
				error_packet(remcom_out_buffer, PTR_ERR(ptr));
			else
				strcpy(remcom_out_buffer, "OK");
			break;

			/* kill or detach. KGDB should treat this like a
			 * continue.
			 */
		case 'D':
			error = remove_all_break();
			if (error < 0)
				error_packet(remcom_out_buffer, error);
			else {
				strcpy(remcom_out_buffer, "OK");
				kgdb_connected = 0;
			}
			put_packet(remcom_out_buffer);
			goto default_handle;

		case 'k':
			/* Don't care about error from remove_all_break */
			remove_all_break();
			kgdb_connected = 0;
			goto default_handle;

			/* Reboot */
		case 'R':
			/* For now, only honor R0 */
			if (strcmp(remcom_in_buffer, "R0") == 0) {
				printk(KERN_CRIT "Executing reboot\n");
				strcpy(remcom_out_buffer, "OK");
				put_packet(remcom_out_buffer);
				emergency_sync();
				/* Execution should not return from
				 * machine_restart()
				 */
				machine_restart(NULL);
				kgdb_connected = 0;
				goto default_handle;
			}

			/* query */
		case 'q':
			switch (remcom_in_buffer[1]) {
			case 's':
			case 'f':
				if (memcmp(remcom_in_buffer + 2, "ThreadInfo",
					   10)) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}

				/*
				 * If we have not yet completed in
				 * pidhash_init() there isn't much we
				 * can give back.
				 */
				if (init_pid_ns.last_pid == 0) {
					if (remcom_in_buffer[1] == 'f')
						strcpy(remcom_out_buffer,
						       "m0000000000000001");
					break;
				}

				if (remcom_in_buffer[1] == 'f')
					threadid = 1;

				remcom_out_buffer[0] = 'm';
				ptr = remcom_out_buffer + 1;
				for (i = 0; i < 17 && threadid < pid_max +
				     numshadowth; threadid++) {
					thread = getthread(linux_regs,
							   threadid);
					if (thread) {
						int_to_threadref(thref,
								 threadid);
						pack_threadid(ptr, thref);
						ptr += 16;
						*(ptr++) = ',';
						i++;
					}
				}
				*(--ptr) = '\0';
				break;

			case 'C':
				/* Current thread id */
				strcpy(remcom_out_buffer, "QC");

				threadid = shadow_pid(current->pid);

				int_to_threadref(thref, threadid);
				pack_threadid(remcom_out_buffer + 2, thref);
				break;
			case 'T':
				if (memcmp(remcom_in_buffer + 1,
					   "ThreadExtraInfo,", 16)) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}
				threadid = 0;
				ptr = remcom_in_buffer + 17;
				kgdb_hex2long(&ptr, &threadid);
				if (!getthread(linux_regs, threadid)) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}
				if (threadid < pid_max)
					kgdb_mem2hex(getthread(linux_regs,
							       threadid)->comm,
						     remcom_out_buffer, 16);
				else if (threadid >= pid_max +
					   num_online_cpus())
					kgdb_shadowinfo(linux_regs,
							remcom_out_buffer,
							threadid - pid_max -
							num_online_cpus());
				else {
					static char tmpstr[23 +
							   BUF_THREAD_ID_SIZE];
					sprintf(tmpstr, "Shadow task %d"
						" for pid 0",
						(int)(threadid - pid_max));
					kgdb_mem2hex(tmpstr, remcom_out_buffer,
						     strlen(tmpstr));
				}
				break;
			}
			break;

			/* task related */
		case 'H':
			switch (remcom_in_buffer[1]) {
			case 'g':
				ptr = &remcom_in_buffer[2];
				kgdb_hex2long(&ptr, &threadid);
				thread = getthread(linux_regs, threadid);
				if (!thread && threadid > 0) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}
				kgdb_usethread = thread;
				kgdb_usethreadid = threadid;
				strcpy(remcom_out_buffer, "OK");
				break;

			case 'c':
				ptr = &remcom_in_buffer[2];
				kgdb_hex2long(&ptr, &threadid);
				if (!threadid)
					kgdb_contthread = NULL;
				else {
					thread = getthread(linux_regs,
							   threadid);
					if (!thread && threadid > 0) {
						error_packet(remcom_out_buffer,
							     -EINVAL);
						break;
					}
					kgdb_contthread = thread;
				}
				strcpy(remcom_out_buffer, "OK");
				break;
			}
			break;

			/* Query thread status */
		case 'T':
			ptr = &remcom_in_buffer[1];
			kgdb_hex2long(&ptr, &threadid);
			thread = getthread(linux_regs, threadid);
			if (thread)
				strcpy(remcom_out_buffer, "OK");
			else
				error_packet(remcom_out_buffer, -EINVAL);
			break;
		/* Since GDB-5.3, it's been drafted that '0' is a software
		 * breakpoint, '1' is a hardware breakpoint, so let's do
		 * that.
		 */
		case 'z':
		case 'Z':
			bpt_type = &remcom_in_buffer[1];
			ptr = &remcom_in_buffer[2];

			if (kgdb_ops->set_hw_breakpoint && *bpt_type >= '1') {
				/* Unsupported */
				if (*bpt_type > '4')
					break;
			} else if (*bpt_type != '0' && *bpt_type != '1')
				/* Unsupported. */
				break;
			/* Test if this is a hardware breakpoint, and
			 * if we support it. */
			if (*bpt_type == '1' &&
			    !(kgdb_ops->flags & KGDB_HW_BREAKPOINT))
				/* Unsupported. */
				break;

			if (*(ptr++) != ',') {
				error_packet(remcom_out_buffer, -EINVAL);
				break;
			} else if (kgdb_hex2long(&ptr, &addr)) {
				if (*(ptr++) != ',' ||
				    !kgdb_hex2long(&ptr, &length)) {
					error_packet(remcom_out_buffer,
						     -EINVAL);
					break;
				}
			} else {
				error_packet(remcom_out_buffer, -EINVAL);
				break;
			}

			if (remcom_in_buffer[0] == 'Z' && *bpt_type == '0')
				error = kgdb_set_sw_break(addr);
			else if (remcom_in_buffer[0] == 'z' && *bpt_type == '0')
				error = kgdb_remove_sw_break(addr);
			else if (remcom_in_buffer[0] == 'Z')
				error = kgdb_ops->set_hw_breakpoint(addr,
								    (int)length,
								    *bpt_type);
			else if (remcom_in_buffer[0] == 'z')
				error = kgdb_ops->remove_hw_breakpoint(addr,
							       (int)
							       length,
							       *bpt_type);

			if (error == 0)
				strcpy(remcom_out_buffer, "OK");
			else
				error_packet(remcom_out_buffer, error);

			break;
		case 'C':
			/* C09 == pass exception
			 * C15 == detach kgdb, pass exception
			 * C30 == detach kgdb, stop attachwait, pass exception
			 */
			if (remcom_in_buffer[1] == '0' &&
				remcom_in_buffer[2] == '9') {
				pass_exception = 1;
				remcom_in_buffer[0] = 'c';
			} else if (remcom_in_buffer[1] == '1' &&
					   remcom_in_buffer[2] == '5') {
				pass_exception = 1;
				remcom_in_buffer[0] = 'D';
				remove_all_break();
				kgdb_connected = 0;
				goto default_handle;
			} else if (remcom_in_buffer[1] == '3' &&
					   remcom_in_buffer[2] == '0') {
				pass_exception = 1;
				attachwait = 0;
				remcom_in_buffer[0] = 'D';
				remove_all_break();
				kgdb_connected = 0;
				goto default_handle;
			} else {
				error_packet(remcom_out_buffer, error);
				break;
			}
		case 'c':
		case 's':
			if (kgdb_contthread && kgdb_contthread != current) {
				/* Can't switch threads in kgdb */
				error_packet(remcom_out_buffer, -EINVAL);
				break;
			}
			kgdb_activate_sw_breakpoints();
			/* Followthrough to default processing */
		default:
default_handle:
			error = kgdb_arch_handle_exception(ex_vector, signo,
							   err_code,
							   remcom_in_buffer,
							   remcom_out_buffer,
							   linux_regs);
			if (error >= 0 || remcom_in_buffer[0] == 'D' ||
			    remcom_in_buffer[0] == 'k') {
				error = 0;
				goto kgdb_exit;
			}

		}		/* switch */

		/* reply to the request */
		put_packet(remcom_out_buffer);
	}

 kgdb_exit:
	if (pass_exception)
		error = 1;
	/*
	 * Call the I/O driver's post_exception routine
	 * if the I/O driver defined one.
	 */
	if (kgdb_io_ops.post_exception)
		kgdb_io_ops.post_exception();

	kgdb_info[processor].debuggerinfo = NULL;
	kgdb_info[processor].task = NULL;
	atomic_set(&procindebug[processor], 0);

	if (!debugger_step || !kgdb_contthread) {
		for (i = 0; i < NR_CPUS; i++)
			spin_unlock(&slavecpulocks[i]);
		/* Wait till all the processors have quit
		 * from the debugger. */
		for (i = 0; i < NR_CPUS; i++) {
			while (atomic_read(&procindebug[i])) {
				int j = 10;	/* an arbitrary number */

				while (--j)
					cpu_relax();
			}
		}
	}

#ifdef CONFIG_SMP
	/* This delay has a real purpose.  The problem is that if you
	 * are single-stepping, you are sending an NMI to all the
	 * other processors to stop them.  Interrupts come in, but
	 * don't get handled.  Then you let them go just long enough
	 * to get into their interrupt routines and use up some stack.
	 * You stop them again, and then do the same thing.  After a
	 * while you blow the stack on the other processors.  This
	 * delay gives some time for interrupts to be cleared out on
	 * the other processors.
	 */
	if (debugger_step)
		mdelay(2);
#endif
 kgdb_restore:
	/* Free debugger_active */
	atomic_set(&debugger_active, 0);
	atomic_set(&kgdb_sync, -1);
	clocksource_touch_watchdog();
	kgdb_softlock_skip[processor] = 1;
	local_irq_restore(flags);

	return error;
}

/*
 * GDB places a breakpoint at this function to know dynamically
 * loaded objects. It's not defined static so that only one instance with this
 * name exists in the kernel.
 */

int module_event(struct notifier_block *self, unsigned long val, void *data)
{
	return 0;
}

static struct notifier_block kgdb_module_load_nb = {
	.notifier_call = module_event,
};

int kgdb_nmihook(int cpu, void *regs)
{
#ifdef CONFIG_SMP
	if (!atomic_read(&procindebug[cpu]) &&
		atomic_read(&debugger_active) != (cpu + 1)) {
		kgdb_wait((struct pt_regs *)regs);
		return 0;
	}
#endif
	return 1;
}

/*
 * This is called when a panic happens.  All we need to do is
 * breakpoint().
 */
static int kgdb_panic_notify(struct notifier_block *self, unsigned long cmd,
			     void *ptr)
{
	if (atomic_read(&debugger_active) != 0) {
		printk(KERN_ERR "KGDB: Cannot handle panic while"
		"debugger active\n");
		dump_stack();
		return NOTIFY_DONE;
	}
	breakpoint();

	return NOTIFY_DONE;
}

static struct notifier_block kgdb_panic_notifier = {
	.notifier_call = kgdb_panic_notify,
};

/*
 * Initialization that needs to be done in either of our entry points.
 */
static void __init kgdb_internal_init(void)
{
	int i;

	/* Initialize our spinlocks. */
	for (i = 0; i < NR_CPUS; i++)
		spin_lock_init(&slavecpulocks[i]);

	for (i = 0; i < MAX_BREAKPOINTS; i++)
		kgdb_break[i].state = bp_none;

	/* Initialize the I/O handles */
	memset(&kgdb_io_ops_prev, 0, sizeof(kgdb_io_ops_prev));

	/* We can't do much if this fails */
	register_module_notifier(&kgdb_module_load_nb);

	kgdb_initialized = 1;
}

static void kgdb_register_for_panic(void)
{
	/* Register for panics(). */
	/* The registration is done in the kgdb_register_for_panic
	 * routine because KGDB should not try to handle a panic when
	 * there are no kgdb_io_ops setup. It is assumed that the
	 * kgdb_io_ops are setup at the time this method is called.
	 */
	if (!kgdb_from_module_registered) {
		atomic_notifier_chain_register(&panic_notifier_list,
					&kgdb_panic_notifier);
		kgdb_from_module_registered = 1;
	}
}

static void kgdb_unregister_for_panic(void)
{
	/* When this routine is called KGDB should unregister from the
	 * panic handler and clean up, making sure it is not handling any
	 * break exceptions at the time.
	 */
	if (kgdb_from_module_registered) {
		kgdb_from_module_registered = 0;
		atomic_notifier_chain_unregister(&panic_notifier_list,
					  &kgdb_panic_notifier);
	}
}

int kgdb_register_io_module(struct kgdb_io *local_kgdb_io_ops)
{

	if (kgdb_connected) {
		printk(KERN_ERR "kgdb: Cannot load I/O module while KGDB "
		       "connected.\n");
		return -EINVAL;
	}

	/* Save the old values so they can be restored */
	if (kgdb_io_handler_cnt >= MAX_KGDB_IO_HANDLERS) {
		printk(KERN_ERR "kgdb: No more I/O handles available.\n");
		return -EINVAL;
	}

	/* Check to see if there is an existing driver and if so save its
	 * values.  Also check to make sure the same driver was not trying
	 * to re-register.
	 */
	if (kgdb_io_ops.read_char != NULL &&
		kgdb_io_ops.read_char != local_kgdb_io_ops->read_char) {
		memcpy(&kgdb_io_ops_prev[kgdb_io_handler_cnt],
		       &kgdb_io_ops, sizeof(struct kgdb_io));
		kgdb_io_handler_cnt++;
	}

	/* Initialize the io values for this module */
	memcpy(&kgdb_io_ops, local_kgdb_io_ops, sizeof(struct kgdb_io));

	/* Make the call to register kgdb if is not initialized */
	kgdb_register_for_panic();

	return 0;
}
EXPORT_SYMBOL(kgdb_register_io_module);

void kgdb_unregister_io_module(struct kgdb_io *local_kgdb_io_ops)
{
	int i;

	/* Unregister KGDB if there were no other prior io hooks, else
	 * restore the io hooks.
	 */
	if (kgdb_io_handler_cnt > 0 && kgdb_io_ops_prev[0].read_char != NULL) {
		/* First check if the hook that is in use is the one being
		 * removed */
		if (kgdb_io_ops.read_char == local_kgdb_io_ops->read_char) {
			/* Set 'i' to the value of where the list should be
			 * shifed */
			i = kgdb_io_handler_cnt - 1;
			memcpy(&kgdb_io_ops, &kgdb_io_ops_prev[i],
			       sizeof(struct kgdb_io));
		} else {
			/* Simple case to remove an entry for an I/O handler
			 * that is not in use */
			for (i = 0; i < kgdb_io_handler_cnt; i++) {
				if (kgdb_io_ops_prev[i].read_char ==
				    local_kgdb_io_ops->read_char)
					break;
			}
		}

		/* Shift all the entries in the handler array so it is
		 * ordered from oldest to newest.
		 */
		kgdb_io_handler_cnt--;
		for (; i < kgdb_io_handler_cnt; i++)
			memcpy(&kgdb_io_ops_prev[i], &kgdb_io_ops_prev[i + 1],
			       sizeof(struct kgdb_io));

		/* Handle the case if we are on the last element and set it
		 * to NULL; */
		memset(&kgdb_io_ops_prev[kgdb_io_handler_cnt], 0,
				sizeof(struct kgdb_io));

		if (kgdb_connected)
			printk(KERN_ERR "kgdb: WARNING: I/O method changed "
			       "while kgdb was connected state.\n");
	} else {
		/* KGDB is no longer able to communicate out, so
		 * unregister our hooks and reset state. */
		kgdb_unregister_for_panic();
		if (kgdb_connected) {
			printk(KERN_CRIT "kgdb: I/O module was unloaded while "
					"a debugging session was running.  "
					"KGDB will be reset.\n");
			if (remove_all_break() < 0)
				printk(KERN_CRIT "kgdb: Reset failed.\n");
			kgdb_connected = 0;
		}
		memset(&kgdb_io_ops, 0, sizeof(struct kgdb_io));
	}
}
EXPORT_SYMBOL(kgdb_unregister_io_module);

/*
 * There are times we need to call a tasklet to cause a breakpoint
 * as calling breakpoint() at that point might be fatal.  We have to
 * check that the exception stack is setup, as tasklets may be scheduled
 * prior to this.  When that happens, it is up to the architecture to
 * schedule this when it is safe to run.
 */
static void kgdb_tasklet_bpt(unsigned long ing)
{
	if (CHECK_EXCEPTION_STACK())
		breakpoint();
}
EXPORT_SYMBOL(kgdb_tasklet_breakpoint);

DECLARE_TASKLET(kgdb_tasklet_breakpoint, kgdb_tasklet_bpt, 0);

/*
 * This function can be called very early, either via early_param() or
 * an explicit breakpoint() early on.
 */
static void __init kgdb_early_entry(void)
{
	/* Let the architecture do any setup that it needs to. */
	kgdb_arch_init();

	/*
	 * Don't try and do anything until the architecture is able to
	 * setup the exception stack.  In this case, it is up to the
	 * architecture to hook in and look at us when they are ready.
	 */

	if (!CHECK_EXCEPTION_STACK()) {
		kgdb_initialized = -1;
		/* any kind of break point is deferred to late_init */
		return;
	}

	/* Now try the I/O. */
	/* For early entry kgdb_io_ops.init must be defined */
	if (!kgdb_io_ops.init || kgdb_io_ops.init()) {
		/* Try again later. */
		kgdb_initialized = -1;
		return;
	}

	/* Finish up. */
	kgdb_internal_init();

	/* KGDB can assume that if kgdb_io_ops.init was defined that the
	 * panic registion should be performed at this time. This means
	 * kgdb_io_ops.init did not come from a kernel module and was
	 * initialized statically by a built in.
	 */
	if (kgdb_io_ops.init)
		kgdb_register_for_panic();
}

/*
 * This function will always be invoked to make sure that KGDB will grab
 * what it needs to so that if something happens while the system is
 * running, KGDB will get involved.  If kgdb_early_entry() has already
 * been invoked, there is little we need to do.
 */
static int __init kgdb_late_entry(void)
{
	int need_break = 0;

	/* If kgdb_initialized is -1 then we were passed kgdbwait. */
	if (kgdb_initialized == -1)
		need_break = 1;

	/*
	 * If we haven't tried to initialize KGDB yet, we need to call
	 * kgdb_arch_init before moving onto the I/O.
	 */
	if (!kgdb_initialized)
		kgdb_arch_init();

	if (kgdb_initialized != 1) {
		if (kgdb_io_ops.init && kgdb_io_ops.init()) {
			/* When KGDB allows I/O via modules and the core
			 * I/O init fails KGDB must default to defering the
			 * I/O setup, and appropriately print an error about
			 * it.
			 */
			printk(KERN_ERR "kgdb: Could not setup core I/O "
			       "for KGDB.\n");
			printk(KERN_INFO "kgdb: Defering I/O setup to kernel "
			       "module.\n");
			memset(&kgdb_io_ops, 0, sizeof(struct kgdb_io));
		}

		kgdb_internal_init();

		/* KGDB can assume that if kgdb_io_ops.init was defined that
		 * panic registion should be performed at this time. This means
		 * kgdb_io_ops.init did not come from a kernel module and was
		 * initialized statically by a built in.
		 */
		if (kgdb_io_ops.init)
			kgdb_register_for_panic();
	}

	/* Registering to reboot notifier list*/
	register_reboot_notifier(&kgdb_reboot_notifier);

	/* Now do any late init of the I/O. */
	if (kgdb_io_ops.late_init)
		kgdb_io_ops.late_init();

	if (need_break) {
		printk(KERN_CRIT "kgdb: Waiting for connection from remote"
		       " gdb...\n");
		breakpoint();
	}

	return 0;
}

late_initcall(kgdb_late_entry);

/*
 * This function will generate a breakpoint exception.  It is used at the
 * beginning of a program to sync up with a debugger and can be used
 * otherwise as a quick means to stop program execution and "break" into
 * the debugger.
 */
void breakpoint(void)
{
	atomic_set(&kgdb_setting_breakpoint, 1);
	wmb(); /* Sync point before breakpoint */
	BREAKPOINT();
	wmb(); /* Sync point after breakpoint */
	atomic_set(&kgdb_setting_breakpoint, 0);
}
EXPORT_SYMBOL(breakpoint);

#ifdef CONFIG_MAGIC_SYSRQ
static void sysrq_handle_gdb(int key, struct tty_struct *tty)
{
	if (!kgdb_io_ops.read_char) {
		printk(KERN_CRIT "ERROR: No KGDB I/O module available\n");
		return;
	}
	if (!kgdb_connected)
		printk(KERN_CRIT "Entering KGDB\n");
	breakpoint();
}
static struct sysrq_key_op sysrq_gdb_op = {
	.handler = sysrq_handle_gdb,
	.help_msg = "Gdb",
	.action_msg = "GDB",
};

static int gdb_register_sysrq(void)
{
	printk(KERN_INFO "Registering GDB sysrq handler\n");
	register_sysrq_key('g', &sysrq_gdb_op);
	return 0;
}

module_init(gdb_register_sysrq);
#endif

static int kgdb_notify_reboot(struct notifier_block *this,
		unsigned long code, void *x)
{

	unsigned long flags;

	/* If we're debugging, or KGDB has not connected, don't try
	 * and print. */
	if (!kgdb_connected || atomic_read(&debugger_active) != 0)
		return 0;
	if ((code == SYS_RESTART) || (code == SYS_HALT) ||
		(code == SYS_POWER_OFF)) {
		local_irq_save(flags);
		put_packet("X00");
		local_irq_restore(flags);
	}
	return NOTIFY_DONE;
}

#ifdef CONFIG_KGDB_CONSOLE
void kgdb_console_write(struct console *co, const char *s, unsigned count)
{
	unsigned long flags;

	/* If we're debugging, or KGDB has not connected, don't try
	 * and print. */
	if (!kgdb_connected || atomic_read(&debugger_active) != 0)
		return;

	local_irq_save(flags);
	kgdb_msg_write(s, count);
	local_irq_restore(flags);
}

struct console kgdbcons = {
	.name = "kgdb",
	.write = kgdb_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED,
};
static int __init kgdb_console_init(void)
{
	register_console(&kgdbcons);
	return 0;
}

console_initcall(kgdb_console_init);
#endif

static int __init opt_kgdb_attachwait(char *str)
{
	attachwait = 1;
	return 0;
}
static int __init opt_kgdb_enter(char *str)
{
	/* We've already done this by an explicit breakpoint() call. */
	if (kgdb_initialized)
		return 0;

	kgdb_early_entry();
	attachwait = 1;
	if (kgdb_initialized == 1)
		printk(KERN_CRIT "Waiting for connection from remote "
		       "gdb...\n");
	else {
		printk(KERN_CRIT "KGDB cannot initialize I/O yet.\n");
		return 0;
	}

	breakpoint();

	return 0;
}

early_param("kgdbwait", opt_kgdb_enter);
early_param("attachwait", opt_kgdb_attachwait);
