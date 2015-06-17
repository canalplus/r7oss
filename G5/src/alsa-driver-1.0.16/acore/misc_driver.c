#define __NO_VERSION__
#include "adriver.h"
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <sound/core.h>

/*
 * platform_device wrapper
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static LIST_HEAD(snd_driver_list);

struct bus_type snd_platform_bus_type;
EXPORT_SYMBOL(snd_platform_bus_type);

/* for platform_device only! */
int snd_compat_driver_register(struct device_driver *driver)
{
	list_add(&driver->list, &snd_driver_list);
	INIT_LIST_HEAD(&driver->device_list);
	return 0;
}
EXPORT_SYMBOL(snd_compat_driver_register);

void snd_compat_driver_unregister(struct device_driver *driver)
{
	struct list_head *p, *n;

	list_del(&driver->list);
	list_for_each_safe(p, n, &driver->device_list) {
		struct platform_device *dev = list_entry(p, struct platform_device, list);
		list_del(p);
		if (driver->remove)
			driver->remove((struct device *)dev);
		kfree(dev);
	}
}
EXPORT_SYMBOL(snd_compat_driver_unregister);

#ifdef CONFIG_PM
static int snd_device_pm_callback(struct pm_dev *pm_dev, pm_request_t rqst, void *data)
{
	struct device *dev = data;
	switch (rqst) {
	case PM_SUSPEND:
		if (dev->driver->suspend)
			dev->driver->suspend(dev, PMSG_SUSPEND);
		break;
	case PM_RESUME:
		if (dev->driver->resume)
			dev->driver->resume(dev);
		break;
	}
	return 0;
}
#endif

struct platform_device *
snd_platform_device_register_simple(const char *name, int id,
				    struct resource *res, int nres)
{
	struct list_head *p;
	struct platform_device *dev;
	int err;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (! dev)
		return ERR_PTR(-ENOMEM);
	snprintf(dev->dev.bus_id, sizeof(dev->dev.bus_id), "%s.%d", name, id);

	list_for_each(p, &snd_driver_list) {
		struct device_driver *driver = list_entry(p, struct device_driver, list);
		if (! strcmp(driver->name, name)) {
			dev->name = name;
			dev->id = id;
			dev->dev.driver = driver;
			err = driver->probe((struct device *)dev);
			if (err < 0) {
				kfree(dev);
				return ERR_PTR(err);
			}
#ifdef CONFIG_PM
			dev->dev.pm_dev = pm_register(PM_UNKNOWN_DEV, 0,
						      snd_device_pm_callback);
			if (dev->dev.pm_dev)
				dev->dev.pm_dev->data = dev;
#endif
			list_add(&dev->list, &driver->device_list);
			return dev;
		}
	}
	kfree(dev);
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL(snd_platform_device_register_simple);
#endif /* < 2.6.0 */


/*
 * pci_save/restore_config wrapper
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#ifdef CONFIG_PCI
#ifndef CONFIG_HAVE_NEW_PCI_SAVE_STATE
#ifdef CONFIG_HAVE_PCI_SAVED_CONFIG
int snd_pci_compat_save_state(struct pci_dev *pci)
{
	return snd_pci_orig_save_state(pci, pci->saved_config_space);
}
EXPORT_SYMBOL(snd_pci_compat_save_state);
int snd_pci_compat_restore_state(struct pci_dev *pci)
{
	return snd_pci_orig_restore_state(pci, pci->saved_config_space);
}
EXPORT_SYMBOL(snd_pci_compat_restore_state);
#else /* !CONFIG_HAVE_PCI_SAVED_CONFIG */
struct saved_config_tbl {
	struct pci_dev *pci;
	u32 config[16];
};
static struct saved_config_tbl saved_tbl[16];

int snd_pci_compat_save_state(struct pci_dev *pci)
{
	int i;
	/* FIXME: mutex needed for race? */
	for (i = 0; i < ARRAY_SIZE(saved_tbl); i++) {
		if (! saved_tbl[i].pci) {
			saved_tbl[i].pci = pci;
			snd_pci_orig_save_state(pci, saved_tbl[i].config);
			return 0;
		}
	}
	printk(KERN_DEBUG "snd: no pci config space found!\n");
	return 1;
}
EXPORT_SYMBOL(snd_pci_compat_save_state);

int snd_pci_compat_restore_state(struct pci_dev *pci)
{
	int i;
	/* FIXME: mutex needed for race? */
	for (i = 0; i < ARRAY_SIZE(saved_tbl); i++) {
		if (saved_tbl[i].pci == pci) {
			saved_tbl[i].pci = NULL;
			snd_pci_orig_restore_state(pci, saved_tbl[i].config);
			return 0;
		}
	}
	printk(KERN_DEBUG "snd: no saved pci config!\n");
	return 1;
}
EXPORT_SYMBOL(snd_pci_compat_restore_state);
#endif /* CONFIG_HAVE_PCI_SAVED_CONFIG */
#endif /* ! CONFIG_HAVE_NEW_PCI_SAVE_STATE */
#endif
#endif /* >= 2.4.0 */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

#include <linux/slab.h>
#include <asm/io.h>

int try_inc_mod_count(struct module *module)
{
	__MOD_INC_USE_COUNT(module);
	return 1;
}
EXPORT_SYMBOL(try_inc_mod_count);

struct resource *snd_compat_request_region(unsigned long start, unsigned long size, const char *name, int is_memory)
{
	struct resource *resource;

#ifdef CONFIG_SND_DEBUG_MEMORY
	/* DON'T use kmalloc here; the allocated resource is released
	 * by kfree without wrapper in each driver
	 */
	resource = snd_wrapper_kmalloc(sizeof(struct resource), GFP_KERNEL);
#else
	resource = kmalloc(sizeof(struct resource), GFP_KERNEL);
#endif
	if (resource == NULL)
		return NULL;
	if (! is_memory) {
		if (check_region(start, size)) {
			kfree_nocheck(resource);
			return NULL;
		}
		snd_wrapper_request_region(start, size, name);
	}
	memset(resource, 0, sizeof(struct resource));
	resource->name = name;
	resource->start = start;
	resource->end = start + size - 1;
	resource->flags = is_memory ? IORESOURCE_MEM : IORESOURCE_IO;
	return resource;
}
EXPORT_SYMBOL(snd_compat_request_region);

int snd_compat_release_resource(struct resource *resource)
{
	if (!resource)
		return -EINVAL;
	if (resource->flags & IORESOURCE_MEM)
		return 0;
	release_region(resource->start, (resource->end - resource->start) + 1);
	return 0;
}
EXPORT_SYMBOL(snd_compat_release_resource);
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0) && defined(CONFIG_APM)

#include <linux/apm_bios.h>

static spinlock_t pm_devs_lock = SPIN_LOCK_UNLOCKED;
static LIST_HEAD(pm_devs);

#ifdef CONFIG_PCI
static struct pm_dev *pci_compat_pm_dev;
static int pci_compat_pm_callback(struct pm_dev *pdev, pm_request_t rqst, void *data)
{
	struct pci_dev *dev;
	switch (rqst) {
	case PM_SUSPEND:
		pci_for_each_dev(dev) {
			struct pci_driver *drv = snd_pci_compat_get_pci_driver(dev);
			if (drv && drv->suspend)
				drv->suspend(dev, PMSG_SUSPEND);
		}
		break;
	case PM_RESUME:
		pci_for_each_dev(dev) {
			struct pci_driver *drv = snd_pci_compat_get_pci_driver(dev);
			if (drv && drv->resume)
				drv->resume(dev);
		}
		break;
	}	
	return 0;
}
#endif

static int snd_apm_callback(apm_event_t ev)
{
	struct list_head *entry;
	pm_request_t rqst;
	void *data;
	int status;
	
	switch (ev) {
	case APM_SYS_SUSPEND:
	case APM_USER_SUSPEND:
	case APM_CRITICAL_SUSPEND:
		rqst = PM_SUSPEND;
		data = (void *)3;
		break;
	case APM_NORMAL_RESUME:
	case APM_CRITICAL_RESUME:
	case APM_STANDBY_RESUME:		/* ??? */
		rqst = PM_RESUME;
		data = (void *)0;
		break;
	default:
		return 0;
	}
	list_for_each(entry, &pm_devs) {
		struct pm_dev *dev = list_entry(entry, struct pm_dev, entry);
		if ((status = pm_send(dev, rqst, data)))
			return status;
	}
	/* platform_device */
	list_for_each(entry, &snd_driver_list) {
		struct device_driver *driver = list_entry(entry, struct device_driver, list);
		struct list_head *p;
		if (rqst == PM_SUSPEND) {
			if (! driver->suspend)
				continue;
		} else {
			if (! driver->resume)
				continue;
		}
		list_for_each(p, &driver->device_list) {
			struct platform_device *dev = list_entry(p, struct platform_device, list);
			if (rqst == PM_SUSPEND)
				driver->suspend((struct device *)dev, PMSG_SUSPEND);
			else
				driver->resume((struct device *)dev);
		}
	}

	return 0;
}

int __init pm_init(void)
{
	if (apm_register_callback(snd_apm_callback))
		snd_printk(KERN_ERR "apm_register_callback failure!\n");
#ifdef CONFIG_PCI
	pci_compat_pm_dev = pm_register(PM_PCI_DEV, 0, pci_compat_pm_callback);
#endif
	return 0;
}

void __exit pm_done(void)
{
#ifdef CONFIG_PCI
	if (pci_compat_pm_dev)
		pm_unregister(pci_compat_pm_dev);
#endif
	apm_unregister_callback(snd_apm_callback);
}

struct pm_dev *pm_register(pm_dev_t type,
			   unsigned long id,
			   pm_callback callback)
{
	struct pm_dev *dev = kmalloc(sizeof(struct pm_dev), GFP_KERNEL);

	if (dev) {
		unsigned long flags;
		
		memset(dev, 0, sizeof(*dev));
		dev->type = type;
		dev->id = id;
		dev->callback = callback;
		
		spin_lock_irqsave(&pm_devs_lock, flags);
		list_add(&dev->entry, &pm_devs);
		spin_unlock_irqrestore(&pm_devs_lock, flags);
	}
	return dev;
}
EXPORT_SYMBOL(pm_register);

void pm_unregister(struct pm_dev *dev)
{
	if (dev) {
		unsigned long flags;
		
		spin_lock_irqsave(&pm_devs_lock, flags);
		list_del(&dev->entry);
		spin_unlock_irqrestore(&pm_devs_lock, flags);

		kfree(dev);
	}
}
EXPORT_SYMBOL(pm_unregister);

int pm_send(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	int status = 0;
	int prev_state, next_state;
	
	switch (rqst) {
	case PM_SUSPEND:
	case PM_RESUME:
		prev_state = dev->state;
		next_state = (int) data;
		if (prev_state != next_state) {
			if (dev->callback)
				status = (*dev->callback)(dev, rqst, data);
			if (!status) {
				dev->state = next_state;
				dev->prev_state = prev_state;
			}
		} else {
			dev->prev_state = prev_state;
		}
		break;
	default:
		if (dev->callback)
			status = (*dev->callback)(dev, rqst, data);
		break;
	}
	return status;
}
EXPORT_SYMBOL(pm_send);

#endif /* kernel version < 2.3.0 && CONFIG_APM */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 7)
/* wait-for-completion handler emulation */

/* we know this is used below exactly once for at most one waiter */
struct completion {
	int done;
	wait_queue_head_t wait;
};

static inline void init_completion(struct completion *comp)
{
	comp->done = 0;
	init_waitqueue_head(&comp->wait);
}

static void wait_for_completion(struct completion *comp)
{
	wait_queue_t wait;

	init_waitqueue_entry(&wait, current);
	add_wait_queue(&comp->wait, &wait);
	for (;;) {
		mb();
		if (comp->done)
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
	}
	remove_wait_queue(&comp->wait, &wait);
}

static void complete_and_exit(struct completion *comp, long code)
{
	comp->done = 1;
	wmb();
	wake_up(&comp->wait);
	/*do_exit(code);*/ /* FIXME: not exported from the kernel */
}

#else
#include <linux/completion.h>
#endif /* kernel version < 2.3.0 */

/* workqueue-alike; 2.5.45 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 45)

static int work_caller(void *data)
{
	struct work_struct *works = data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
	lock_kernel();
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 18)
	daemonize();
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 8)
	reparent_to_init();
#endif
	strcpy(current->comm, "snd"); /* FIXME: different names? */

	works->func(works->data);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
	unlock_kernel();
#endif

	return 0;
}

int snd_compat_schedule_work(struct work_struct *works)
{
	return kernel_thread(work_caller, works, 0) >= 0;
}
EXPORT_SYMBOL(snd_compat_schedule_work);

struct workqueue_struct {
	spinlock_t lock;
	const char *name;
	struct list_head worklist;
	int task_pid;
	struct task_struct *task;
	wait_queue_head_t more_work;
	wait_queue_head_t work_done;
	struct completion thread_exited;
};

static void run_workqueue(struct workqueue_struct *wq)
{
	unsigned long flags;

	spin_lock_irqsave(&wq->lock, flags);
	while (!list_empty(&wq->worklist)) {
		struct work_struct *work = list_entry(wq->worklist.next,
						      struct work_struct, entry);
		void (*f) (void *) = work->func;
		void *data = work->data;

		list_del_init(wq->worklist.next);
		spin_unlock_irqrestore(&wq->lock, flags);
		clear_bit(0, &work->pending);
		f(data);
		spin_lock_irqsave(&wq->lock, flags);
		wake_up(&wq->work_done);
	}
	spin_unlock_irqrestore(&wq->lock, flags);
}

void snd_compat_flush_workqueue(struct workqueue_struct *wq)
{
	if (wq->task == current) {
		run_workqueue(wq);
	} else {
		wait_queue_t wait;

		init_waitqueue_entry(&wait, current);
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irq(&wq->lock);
		add_wait_queue(&wq->work_done, &wait);
		while (!list_empty(&wq->worklist)) {
			spin_unlock_irq(&wq->lock);
			schedule();
			spin_lock_irq(&wq->lock);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&wq->work_done, &wait);
		spin_unlock_irq(&wq->lock);
	}
}
EXPORT_SYMBOL(snd_compat_flush_workqueue);

void snd_compat_destroy_workqueue(struct workqueue_struct *wq)
{
	snd_compat_flush_workqueue(wq);
	kill_proc(wq->task_pid, SIGKILL, 1);
	if (wq->task_pid >= 0)
		wait_for_completion(&wq->thread_exited);
	kfree(wq);
}
EXPORT_SYMBOL(snd_compat_destroy_workqueue);

static int xworker_thread(void *data)
{
	struct workqueue_struct *wq = data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
	lock_kernel();
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 18)
	daemonize();
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 8)
	reparent_to_init();
#endif
	strcpy(current->comm, wq->name);

	do {
		run_workqueue(wq);
		wait_event_interruptible(wq->more_work, !list_empty(&wq->worklist));
	} while (!signal_pending(current));

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
	unlock_kernel();
#endif
	complete_and_exit(&wq->thread_exited, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 0)
	return 0;
#endif
}

struct workqueue_struct *snd_compat_create_workqueue(const char *name)
{
	struct workqueue_struct *wq;
	
	BUG_ON(strlen(name) > 10);
	
	wq = kmalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		return NULL;
	memset(wq, 0, sizeof(*wq));
	
	spin_lock_init(&wq->lock);
	INIT_LIST_HEAD(&wq->worklist);
	init_waitqueue_head(&wq->more_work);
	init_waitqueue_head(&wq->work_done);
	init_completion(&wq->thread_exited);
	wq->name = name;
	wq->task_pid = kernel_thread(xworker_thread, wq, 0);
	if (wq->task_pid < 0) {
		printk(KERN_ERR "snd: failed to start thread %s\n", name);
		snd_compat_destroy_workqueue(wq);
		wq = NULL;
	}
	wq->task = find_task_by_pid(wq->task_pid);
	return wq;
}
EXPORT_SYMBOL(snd_compat_create_workqueue);

static void __x_queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&wq->lock, flags);
	work->wq_data = wq;
	list_add_tail(&work->entry, &wq->worklist);
	wake_up(&wq->more_work);
	spin_unlock_irqrestore(&wq->lock, flags);
}

int snd_compat_queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	if (!test_and_set_bit(0, &work->pending)) {
		__x_queue_work(wq, work);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(snd_compat_queue_work);

static void delayed_work_timer_fn(unsigned long __data)
{
	struct work_struct *work = (struct work_struct *)__data;
	struct workqueue_struct *wq = work->wq_data;
	
	if (wq)
		__x_queue_work(wq, work);
	else
		snd_compat_schedule_work(work);
}

int snd_compat_queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay)
{
	struct work_struct *work = &dwork->work;
	struct timer_list *timer = &work->timer;

	if (!test_and_set_bit(0, &work->pending)) {
		work->wq_data = wq;
		timer->expires = jiffies + delay;
		timer->data = (unsigned long)work;
		timer->function = delayed_work_timer_fn;
		add_timer(timer);
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(snd_compat_queue_delayed_work);

int snd_compat_cancel_delayed_work(struct delayed_work *dwork)
{
	struct work_struct *work = &dwork->work;
	int ret;

	ret = del_timer_sync(&work->timer);
	if (ret)
		clear_bit(0, &work->pending);
	return ret;
}
EXPORT_SYMBOL(snd_compat_cancel_delayed_work);

#endif

#ifndef CONFIG_HAVE_KZALLOC
#ifndef CONFIG_SND_DEBUG_MEMORY
/* Don't put this to wrappers.c.  We need to call the kmalloc wrapper here. */
void *snd_compat_kzalloc(size_t size, unsigned int __nocast flags)
{
	void *ret;
	ret = kmalloc(size, flags);
	if (ret)
		memset(ret, 0, size);
	return ret;
}
EXPORT_SYMBOL(snd_compat_kzalloc);
#endif
#endif

#ifndef CONFIG_HAVE_KCALLOC
#ifndef CONFIG_SND_DEBUG_MEMORY
/* Don't put this to wrappers.c.  We need to call the kmalloc wrapper here. */
void *snd_compat_kcalloc(size_t n, size_t size, unsigned int __nocast flags)
{
	if (n != 0 && size > INT_MAX / n)
		return NULL;
	return snd_compat_kzalloc(n * size, flags);
}
EXPORT_SYMBOL(snd_compat_kcalloc);
#endif
#endif

#ifndef CONFIG_HAVE_KSTRDUP
#ifndef CONFIG_SND_DEBUG_MEMORY
char *snd_compat_kstrdup(const char *s, unsigned int __nocast gfp_flags)
{
	int len;
	char *buf;

	if (!s) return NULL;

	len = strlen(s) + 1;
	buf = kmalloc(len, gfp_flags);
	if (buf)
		memcpy(buf, s, len);
	return buf;
}
EXPORT_SYMBOL(snd_compat_kstrdup);
#endif
#endif

#ifdef CONFIG_CREATE_WORKQUEUE_FLAGS

#include <linux/workqueue.h>

struct workqueue_struct *snd_compat_create_workqueue2(const char *name)
{
	return create_workqueue(name, 0);
}
EXPORT_SYMBOL(snd_compat_create_workqueue2);

#endif


/*
 * PnP suspend/resume wrapper
 */
#if defined(CONFIG_PNP) && defined(CONFIG_PM)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#ifndef CONFIG_HAVE_PNP_SUSPEND

#include <linux/pm.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
#include <linux/pm_legacy.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15) || defined(CONFIG_PM_LEGACY)
#define SUPPORT_PM
#endif

#ifdef SUPPORT_PM
struct snd_pnp_pm_devs {
	void *dev;
	void *driver;
	struct pm_dev *pm;
};

static struct snd_pnp_pm_devs snd_pm_devs[16]; /* FIXME */

static void register_pnp_pm_callback(void *dev, void *driver, pm_callback callback)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(snd_pm_devs); i++) {
		if (snd_pm_devs[i].dev)
			continue;
		snd_pm_devs[i].pm = pm_register(PM_ISA_DEV, 0, callback);
		if (snd_pm_devs[i].pm) {
			snd_pm_devs[i].dev = dev;
			snd_pm_devs[i].driver = driver;
			snd_pm_devs[i].pm->data = &snd_pm_devs[i];
		}
		return;
	}
}

static void unregister_pnp_pm_callback(void *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(snd_pm_devs); i++) {
		if (snd_pm_devs[i].dev == dev) {
			snd_pm_devs[i].dev = NULL;
			snd_pm_devs[i].driver = NULL;
			if (snd_pm_devs[i].pm) {
				pm_unregister(snd_pm_devs[i].pm);
				snd_pm_devs[i].pm = NULL;
			}
			return;
		}
	}
}

static int snd_pnp_dev_pm_callback(struct pm_dev *dev, pm_request_t req, void *data)
{
	struct snd_pnp_pm_devs *pm = dev->data;
	struct pnp_dev *pdev = pm->dev;
	struct snd_pnp_driver *driver = pm->driver;

	switch (req) {
	case PM_SUSPEND:
		driver->suspend(pdev, PMSG_SUSPEND);
		break;
	case PM_RESUME:
		driver->resume(pdev);
		break;
	}
	return 0;
}

static int snd_pnp_dev_probe(struct pnp_dev *dev, const struct pnp_device_id *dev_id)
{
	struct snd_pnp_driver *driver = (struct snd_pnp_driver *)dev->driver;
	int err = driver->probe(dev, dev_id);
	if (err >= 0)
		register_pnp_pm_callback(dev, driver, snd_pnp_dev_pm_callback);
	return err;
}

static void snd_pnp_dev_remove(struct pnp_dev *dev)
{
	struct snd_pnp_driver *driver = (struct snd_pnp_driver *)dev->driver;
	unregister_pnp_pm_callback(dev);
	driver->remove(dev);
}
#endif /* SUPPORT_PM */

#undef pnp_register_driver

int snd_pnp_register_driver(struct snd_pnp_driver *driver)
{
	driver->real_driver.name = driver->name;
	driver->real_driver.id_table = driver->id_table;
	driver->real_driver.flags = driver->flags;
#ifdef SUPPORT_PM
	if (driver->suspend || driver->resume) {
		driver->real_driver.probe = snd_pnp_dev_probe;
		driver->real_driver.remove = snd_pnp_dev_remove;
	} else
#endif
	{
		driver->real_driver.probe = driver->probe;
		driver->real_driver.remove = driver->remove;
	}
	return pnp_register_driver(&driver->real_driver);
}
EXPORT_SYMBOL(snd_pnp_register_driver);

#ifdef SUPPORT_PM
/*
 * for card
 */
static int snd_pnp_card_pm_callback(struct pm_dev *dev, pm_request_t req, void *data)
{
	struct snd_pnp_pm_devs *pm = dev->data;
	struct pnp_card_link *pdev = pm->dev;
	struct snd_pnp_card_driver *driver = pm->driver;

	switch (req) {
	case PM_SUSPEND:
		driver->suspend(pdev, PMSG_SUSPEND);
		break;
	case PM_RESUME:
		driver->resume(pdev);
		break;
	}
	return 0;
}

static int snd_pnp_card_probe(struct pnp_card_link *dev, const struct pnp_card_device_id *dev_id)
{
	struct snd_pnp_card_driver *driver = (struct snd_pnp_card_driver *)dev->driver;
	int err = driver->probe(dev, dev_id);
	if (err >= 0)
		register_pnp_pm_callback(dev, driver, snd_pnp_card_pm_callback);
	return err;
}

static void snd_pnp_card_remove(struct pnp_card_link *dev)
{
	struct snd_pnp_card_driver *driver = (struct snd_pnp_card_driver *)dev->driver;
	unregister_pnp_pm_callback(dev);
	driver->remove(dev);
}
#endif /* SUPPORT_PM */

#undef pnp_register_card_driver

int snd_pnp_register_card_driver(struct snd_pnp_card_driver *driver)
{
	driver->real_driver.name = driver->name;
	driver->real_driver.id_table = driver->id_table;
	driver->real_driver.flags = driver->flags;
#ifdef SUPPORT_PM
	if (driver->suspend || driver->resume) {
		driver->real_driver.probe = snd_pnp_card_probe;
		driver->real_driver.remove = snd_pnp_card_remove;
	} else
#endif
	{
		driver->real_driver.probe = driver->probe;
		driver->real_driver.remove = driver->remove;
	}
	return pnp_register_card_driver(&driver->real_driver);
}
EXPORT_SYMBOL(snd_pnp_register_card_driver);

#endif /* ! CONFIG_HAVE_PNP_SUSPEND */
#endif /* 2.6 */
#endif /* PNP && PM */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/firmware.h>
#ifdef NEEDS_COMPAT_FW_LOADER

extern int mod_firmware_load(const char *fn, char **fp);

static int snd_try_load_firmware(const char *path, const char *name,
				 struct firmware *firmware)
{
	char filename[30 + FIRMWARE_NAME_MAX];

	sprintf(filename, "%s/%s", path, name);
	firmware->size = mod_firmware_load(filename, (char **)&firmware->data);
	if (firmware->size)
		printk(KERN_INFO "Loaded '%s'.", filename);
	return firmware->size;
}

int snd_compat_request_firmware(const struct firmware **fw, const char *name)
{
	struct firmware *firmware;

	*fw = NULL;
	firmware = kmalloc(sizeof *firmware, GFP_KERNEL);
	if (!firmware)
		return -ENOMEM;
	if (!snd_try_load_firmware("/lib/firmware", name, firmware) &&
	    !snd_try_load_firmware("/lib/hotplug/firmware", name, firmware) &&
	    !snd_try_load_firmware("/usr/lib/hotplug/firmware", name, firmware)) {
		kfree(firmware);
		return -EIO;
	}
	*fw = firmware;
	return 0;
}
EXPORT_SYMBOL(snd_compat_request_firmware);

void snd_compat_release_firmware(const struct firmware *fw)
{
	if (fw) {
		vfree(fw->data);
		kfree(fw);
	}
}
EXPORT_SYMBOL(snd_compat_release_firmware);

#endif /* NEEDS_COMPAT_FW_LOADER */
#endif /* !2.6 */

/* ISA drivers */
#ifdef CONFIG_ISA
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <linux/device.h>
#include <linux/isa.h>
#ifdef HAVE_DUMMY_SND_ISA_WRAPPER
#include <linux/list.h>
#include <linux/err.h>

#define MAX_ISA_NUMS	8

struct isa_platform_link {
	struct platform_driver platform;
	struct isa_driver *isa;
	unsigned int cards;
	struct platform_device *pdevs[MAX_ISA_NUMS];
	struct list_head list;
};
	
static LIST_HEAD(snd_isa_link_list);

static struct isa_platform_link *get_isa_link(struct isa_driver *driver)
{
	struct isa_platform_link *p;
	list_for_each_entry(p, &snd_isa_link_list, list) {
		if (p->isa == driver)
			return p;
	}
	return NULL;
}

static struct isa_platform_link *get_pdev_link(struct platform_device *pdev)
{
	struct platform_driver *pdrv = container_of(pdev->dev.driver,
						    struct platform_driver,
						    driver);
	return (struct isa_platform_link *)pdrv;
}


static int snd_isa_platform_probe(struct platform_device *pdev)
{
	struct isa_platform_link *p = get_pdev_link(pdev);
	int n = pdev->id;

	if (!p)
		return -EINVAL;
	p = get_pdev_link(pdev);
	if (p->isa->match)
		if (!p->isa->match(&pdev->dev, n))
			return -EINVAL;
	return p->isa->probe(&pdev->dev, n);
}

static int snd_isa_platform_remove(struct platform_device *pdev)
{
	struct isa_platform_link *p = get_pdev_link(pdev);
	int n = pdev->id;

	if (!p)
		return 0;
	p->isa->remove(&pdev->dev, n);
	return 0;
}

#ifdef CONFIG_PM
static int snd_isa_platform_suspend(struct platform_device *pdev,
				    pm_message_t state)
{
	struct isa_platform_link *p = get_pdev_link(pdev);
	int n = pdev->id;

	if (!p)
		return -EINVAL;
	return p->isa->suspend(&pdev->dev, n, state);
}

static int snd_isa_platform_resume(struct platform_device *pdev)
{
	struct isa_platform_link *p = get_pdev_link(pdev);
	int n = pdev->id;

	if (!p)
		return -EINVAL;
	return p->isa->resume(&pdev->dev, n);
}
#endif

int snd_isa_register_driver(struct isa_driver *driver, unsigned int nums)
{
	int i, cards, err;
	struct isa_platform_link *p;

	if (nums >= MAX_ISA_NUMS)
		return -EINVAL;

	if (get_isa_link(driver))
		return -EBUSY;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	INIT_LIST_HEAD(&p->list);
	p->isa = driver;
	p->platform.probe = snd_isa_platform_probe;
	p->platform.driver.name = driver->driver.name;
	if (driver->remove)
		p->platform.remove = snd_isa_platform_remove;
#ifdef CONFIG_PM
	if (driver->suspend)
		p->platform.suspend = snd_isa_platform_suspend;
	if (driver->resume)
		p->platform.resume = snd_isa_platform_resume;
#endif
	err = platform_driver_register(&p->platform);
	if (err < 0) {
		kfree(p);
		return err;
	}
	list_add(&p->list, &snd_isa_link_list);

	cards = 0;
	for (i = 0; i < nums; i++) {
		struct platform_device *device;
		device = platform_device_register_simple((char *)
							 driver->driver.name,
							 i, NULL, 0);
		if (IS_ERR(device))
			continue;
		p->pdevs[i] = device;
		cards++;
	}
	if (!cards) {
		snd_isa_unregister_driver(driver);
		return -ENODEV;
       }
       return 0;
}
EXPORT_SYMBOL(snd_isa_register_driver);

void snd_isa_unregister_driver(struct isa_driver *driver)
{
	struct isa_platform_link *p = get_isa_link(driver);
	int i;

	if (!p)
		return;
	for (i = 0; i < MAX_ISA_NUMS; i++)
		if (p->pdevs[i])
			platform_device_unregister(p->pdevs[i]);
	platform_driver_unregister(&p->platform);
	list_del(&p->list);
	kfree(p);
}
EXPORT_SYMBOL(snd_isa_unregister_driver);

#endif /* HAVE_DUMMY_SND_ISA_WRAPPER */
#endif /* < 2.6.18 */
#endif /* CONFIG_ISA */
