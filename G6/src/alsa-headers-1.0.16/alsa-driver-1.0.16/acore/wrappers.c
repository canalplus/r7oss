#define __NO_VERSION__
#include "config.h"

#include <linux/version.h>
#include <linux/config.h>
#include <linux/string.h>
#include <linux/sched.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#if defined(CONFIG_MODVERSIONS) && !defined(__GENKSYMS__) && !defined(__DEPEND__)
#include "sndversions.h"
#endif
#endif

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/devfs_fs_kernel.h>

/* defined in adriver.h but we don't include it... */
#include <linux/compiler.h>
#ifndef __nocast
#define __nocast
#endif

#ifndef CONFIG_HAVE_STRLCPY
#define strlcat snd_compat_strlcat
#ifndef BUG_ON
#define BUG_ON(x) /* nothing */
#endif
size_t snd_compat_strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = strlen(dest);
	size_t len = strlen(src);
	size_t res = dsize + len;

	/* This would be a bug */
	BUG_ON(dsize >= count);

	dest += dsize;
	count -= dsize;
	if (len >= count)
		len = count-1;
	memcpy(dest, src, len);
	dest[len] = 0;
	return res;
}
EXPORT_SYMBOL(snd_compat_strlcat);
#endif

#ifndef CONFIG_HAVE_VSNPRINTF
#define vsnprintf snd_compat_vsnprintf
int snd_compat_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
#endif

#ifndef CONFIG_HAVE_SSCANF
#include <linux/ctype.h>

/* this function supports any format as long as it's %x  :-) */
int snd_compat_vsscanf(const char *buf, const char *fmt, va_list args)
{
	const char *str = buf;
	char *next;
	int num = 0;
	unsigned int *p;

	while (*fmt && *str) {
		while (isspace(*fmt))
			++fmt;

		if (!*fmt)
			break;

		if (fmt[0] != '%' || fmt[1] != 'x') {
			printk(KERN_ERR "snd_compat_vsscanf: format isn't %%x\n");
			return 0;
		}
		fmt += 2;

		while (isspace(*str))
			++str;

		if (!*str || !isxdigit(*str))
			break;

		p = (unsigned int*) va_arg(args, unsigned int*);
		*p = (unsigned int) simple_strtoul(str, &next, 0x10);
		++num;

		if (!next)
			break;
		str = next;
	}
	return num;
}
EXPORT_SYMBOL(snd_compat_vsscanf);

int snd_compat_sscanf(const char *buf, const char *fmt, ...)
{
	int res;
	va_list args;

	va_start(args, fmt);
	res = snd_compat_vsscanf(buf, fmt, args);
	va_end(args);
	return res;
}
EXPORT_SYMBOL(snd_compat_sscanf);
#endif

#ifdef CONFIG_HAVE_OLD_REQUEST_MODULE
void snd_compat_request_module(const char *fmt, ...)
{
	char buf[64];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buf, 64, fmt, args);
	if (n < 64 && buf[0])
		request_module(buf);
	va_end(args);
}
EXPORT_SYMBOL(snd_compat_request_module);
#endif

#if defined(CONFIG_DEVFS_FS)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 29)

void snd_compat_devfs_remove(const char *fmt, ...)
{
	char buf[64];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buf, 64, fmt, args);
	if (n < 64 && buf[0]) {
		devfs_handle_t de = devfs_get_handle(NULL, buf, 0, 0, 0, 0);
		devfs_unregister(de);
		devfs_put(de);
	}
	va_end(args);
}
EXPORT_SYMBOL(snd_compat_devfs_remove);

#endif /* 2.5.29 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 67)

int snd_compat_devfs_mk_dir(const char *dir, ...)
{
	char buf[64];
	va_list args;
	int n;

	va_start(args, dir);
	n = vsnprintf(buf, 64, dir, args);
	va_end(args);
	if (n < 64 && buf[0]) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
		return devfs_mk_dir(NULL, buf, strlen(dir), NULL) ? -EIO : 0;
#else
		return devfs_mk_dir(NULL, buf, NULL) ? -EIO : 0;
#endif
	}
	return 0;
}
EXPORT_SYMBOL(snd_compat_devfs_mk_dir);

extern struct file_operations snd_fops;
int snd_compat_devfs_mk_cdev(dev_t dev, umode_t mode, const char *fmt, ...)
{
	char buf[64];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buf, 64, fmt, args);
	va_end(args);
	if (n < 64 && buf[0]) {
		devfs_register(NULL, buf, DEVFS_FL_DEFAULT,
			       major(dev), minor(dev), mode,
			       &snd_fops, NULL);
	}
	return 0;
}
EXPORT_SYMBOL(snd_compat_devfs_mk_cdev);

#endif /* 2.5.67 */

#endif /* CONFIG_DEVFS_FS */

#ifndef CONFIG_HAVE_PCI_DEV_PRESENT
#include <linux/pci.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 0)
/* for pci_device_id compatibility layer */
#include "compat_22.h"
#endif
int snd_pci_dev_present(const struct pci_device_id *ids)
{
	while (ids->vendor || ids->subvendor) {
		if (pci_find_device(ids->vendor, ids->subvendor, NULL))
			return 1;
		ids++;
	}
	return 0;
}
EXPORT_SYMBOL(snd_pci_dev_present);
#endif

/*
 * msleep wrapper
 */
#ifndef CONFIG_HAVE_MSLEEP
#include <linux/delay.h>
void snd_compat_msleep(unsigned int msecs)
{
	unsigned long timeout = ((msecs) * HZ + 999) / 1000;

	while (timeout) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		timeout = schedule_timeout(timeout);
	}
}
EXPORT_SYMBOL(snd_compat_msleep);
#endif

#ifndef CONFIG_HAVE_MSLEEP_INTERRUPTIBLE
#include <linux/delay.h>
unsigned long snd_compat_msleep_interruptible(unsigned int msecs)
{
	unsigned long timeout = ((msecs) * HZ + 999) / 1000;

	while (timeout && !signal_pending(current)) {
		set_current_state(TASK_INTERRUPTIBLE);
		timeout = schedule_timeout(timeout);
	}
	return (timeout * 1000) / HZ;
}
EXPORT_SYMBOL(snd_compat_msleep_interruptible);
#endif /* < 2.6.6 */

/* wrapper for new irq handler type */
#ifndef CONFIG_SND_NEW_IRQ_HANDLER
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
typedef int (*snd_irq_handler_t)(int, void *);
struct irq_list {
	snd_irq_handler_t handler;
	void *data;
	struct list_head list;
};
	
struct pt_regs *snd_irq_regs;
EXPORT_SYMBOL(snd_irq_regs);

#if defined(IRQ_NONE) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static irqreturn_t irq_redirect(int irq, void *data, struct pt_regs *reg)
{
	struct irq_list *list = data;
	irqreturn_t val;
	snd_irq_regs = reg;
	val = list->handler(irq, list->data);
	snd_irq_regs = NULL;
	return val;
}
#else
static void irq_redirect(int irq, void *data, struct pt_regs *reg)
{
	struct irq_list *list = data;
	snd_irq_regs = reg;
	list->handler(irq, list->data);
	snd_irq_regs = NULL;
}
#endif

static LIST_HEAD(irq_list_head);
static DEFINE_MUTEX(irq_list_mutex);

int snd_request_irq(unsigned int irq, snd_irq_handler_t handler,
		    unsigned long irq_flags, const char *str, void *data)
{
	struct irq_list *list = kmalloc(sizeof(*list), GFP_KERNEL);
	int err;

	if (!list)
		return -ENOMEM;
	list->handler = handler;
	list->data = data;
	err = request_irq(irq, irq_redirect, irq_flags, str, list);
	if (err) {
		kfree(list);
		return err;
	}
	mutex_lock(&irq_list_mutex);
	list_add(&list->list, &irq_list_head);
	mutex_unlock(&irq_list_mutex);
	return 0;
}
EXPORT_SYMBOL(snd_request_irq);

void snd_free_irq(unsigned int irq, void *data)
{
	struct list_head *p;

	mutex_lock(&irq_list_mutex);
	list_for_each(p, &irq_list_head) {
		struct irq_list *list = list_entry(p, struct irq_list, list);
		if (list->data == data) {
			free_irq(irq, list);
			list_del(p);
			kfree(list);
			break;
		}
	}
	mutex_unlock(&irq_list_mutex);
}
EXPORT_SYMBOL(snd_free_irq);
#endif /* !CONFIG_SND_NEW_IRQ_HANDLER */

