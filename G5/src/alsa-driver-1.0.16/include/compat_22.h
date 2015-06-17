/*
 *  Missing Linux 2.2.x features
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/ioport.h>
#include <asm/byteorder.h>

#ifndef likely
#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)
#endif

#if defined(SND_NEED_USB_WRAPPER) && (defined(CONFIG_USB) || defined(CONFIG_USB_MODULE))
/* include linux/usb.h at first since it defines another compatibility layers, which
 * conflicts with ours...
 */
#define device compat_net_device
#include <linux/usb.h>
#undef device
#undef IORESOURCE_IO
#endif

#ifndef CONFIG_HAVE_DMA_ADDR_T
typedef unsigned long dma_addr_t;
#endif

/*
 */

#ifndef CONFIG_HAVE_MUTEX_MACROS
#define init_MUTEX(x) *(x) = MUTEX
#define DECLARE_MUTEX(x) struct semaphore x = MUTEX
typedef struct wait_queue wait_queue_t;
typedef struct wait_queue * wait_queue_head_t;
#define init_waitqueue_head(x) *(x) = NULL
#define init_waitqueue_entry(q,p) ((q)->task = (p))
#define set_current_state(xstate) do { current->state = xstate; } while (0)

/*
 * Insert a new entry before the specified head..
 */
static __inline__ void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

#endif /* !CONFIG_HAVE_MUTEX_MACROS */

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static __inline__ void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry); 
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* rw_semaphore - replaced with mutex */
#define rw_semaphore semaphore
#define init_rwsem(x) init_MUTEX(x)
#define DECLARE_RWSEM(x) DECLARE_MUTEX(x)
#define down_read(x) down(x)
#define down_write(x) down(x)
#define up_read(x) up(x)
#define up_write(x) up(x)

#define virt_to_page(x) (&mem_map[MAP_NR(x)])
#define get_page(p) atomic_inc(&(p)->count)
#define SetPageReserved(p) set_bit(PG_reserved, &(p)->flags)
#define ClearPageReserved(p) clear_bit(PG_reserved, &(p)->flags)
#define vm_private_data vm_pte
#define NOPAGE_OOM 0
#define NOPAGE_SIGBUS (-1)
#define fops_get(x) (x)
#define fops_put(x) do { ; } while (0)

#define local_irq_save(flags) \
	do { __save_flags(flags); __cli(); } while (0)
#define local_irq_restore(flags) \
	do { __restore_flags(flags); } while (0)
#define local_irq_enable() __sti()

/* Some distributions use modified kill_fasync */
#ifdef CONFIG_OLD_KILL_FASYNC
#include <linux/fs.h>
static inline void snd_compat_kill_fasync(struct fasync_struct **fp, int sig, int band)
{
	kill_fasync(*(fp), sig);
}
#undef kill_fasync
#define kill_fasync(fp, sig, band) snd_compat_kill_fasync(fp, sig, band)
#endif

/* this is identical with tq_struct but the "routine" field is renamed to "func" */
struct tasklet_struct {
	struct tasklet_struct *next;	/* linked list of active bh's */
	unsigned long sync;		/* must be initialized to zero */
	void (*func)(void *);		/* function to call */
	void *data;			/* argument to function */
};

#define tasklet_hi_schedule(t)	do { \
	queue_task((struct tq_struct *)(t), &tq_immediate);\
	mark_bh(IMMEDIATE_BH); \
} while (0)

#define tasklet_init(t,f,d)	do { \
	(t)->next = NULL; \
	(t)->sync = 0; \
	(t)->func = (void (*)(void *))(f); \
	(t)->data = (void *)(d); \
} while (0)

#define tasklet_unlock_wait(t)	while (test_bit(0, &(t)->sync)) { }
#define tasklet_kill(t)		tasklet_unlock_wait(t) /* FIXME: world is not perfect... */

#define del_timer_sync(t) del_timer(t) /* FIXME: not quite correct on SMP */

#define rwlock_init(x) do { *(x) = RW_LOCK_UNLOCKED; } while(0)

#ifndef fastcall
#define fastcall
#endif

#ifndef __init
#define __init
#endif
#ifndef __initdata
#define __initdata
#endif
#ifndef __exit
#define __exit
#endif
#ifndef __exitdata
#define __exitdata
#endif
#ifndef __devinit
#define __devinit
#endif
#ifndef __devinitdata
#define __devinitdata
#endif
#ifndef __devexit
#define __devexit
#endif
#ifndef __devexitdata
#define __devexitdata
#endif

#ifdef MODULE
  #ifndef module_init
  #define module_init(x)      int init_module(void) { return x(); }
  #define module_exit(x)      void cleanup_module(void) { x(); }
  #endif
  #ifndef THIS_MODULE
  #define THIS_MODULE	      (&__this_module)
  #endif
  int try_inc_mod_count(struct module *mod);
#else
  #define module_init(x)
  #define module_exit(x)
  #define THIS_MODULE NULL
  #define try_inc_mod_count(x) 1
#endif

#define MODULE_GENERIC_TABLE(gtype,name)        \
static const unsigned long __module_##gtype##_size \
  __attribute__ ((unused)) = sizeof(struct gtype##_id); \
static const struct gtype##_id * __module_##gtype##_table \
  __attribute__ ((unused)) = name
#define MODULE_DEVICE_TABLE(type,name)          \
  MODULE_GENERIC_TABLE(type##_device,name)

/**
 * list_for_each        -       iterate over a list
 * @pos:        the &struct list_head to use as a loop counter.
 * @head:       the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

#ifndef IORESOURCE_IO
struct resource {
	const char *name;
	unsigned long start, end;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};

#define IORESOURCE_IO           0x00000100      /* Resource type */
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_CACHEABLE	0x00004000
#endif

static inline void snd_wrapper_request_region(unsigned long from, unsigned long extent, const char *name)
{
	request_region(from, extent, name);
}

#undef request_region
#define request_region(start,size,name) snd_compat_request_region(start,size,name,0)
#define request_mem_region(start,size,name) snd_compat_request_region(start,size,name,1)
#define release_resource(res) snd_compat_release_resource(res)

struct resource *snd_compat_request_region(unsigned long start, unsigned long size, const char *name, int is_memory);
int snd_compat_release_resource(struct resource *resource);

/* these functions are used for ISA buffer allocation, too, so they stay
 * outside of CONFIG_PCI
 */
#define pci_alloc_consistent snd_pci_compat_alloc_consistent
#define pci_free_consistent snd_pci_compat_free_consistent
void *snd_pci_compat_alloc_consistent(struct pci_dev *, long, dma_addr_t *);
void snd_pci_compat_free_consistent(struct pci_dev *, long, void *, dma_addr_t);

#ifdef CONFIG_PCI

/* New-style probing supporting hot-pluggable devices */

#define PCI_PM_CTRL		4	/* PM control and status register */
#define PCI_PM_CTRL_STATE_MASK	0x0003	/* Current power state (D0 to D3) */

#define PCI_ANY_ID (~0)

#define pci_get_drvdata snd_pci_compat_get_driver_data
#define pci_set_drvdata snd_pci_compat_set_driver_data

#define pci_set_dma_mask snd_pci_compat_set_dma_mask

#undef pci_enable_device
#define pci_enable_device snd_pci_compat_enable_device
#define pci_disable_device snd_pci_compat_disable_device
#define pci_register_driver snd_pci_compat_register_driver
#define pci_unregister_driver snd_pci_compat_unregister_driver
#define pci_set_power_state snd_pci_compat_set_power_state
#define pci_match_device snd_pci_compat_match_device

#define pci_dma_supported snd_pci_compat_dma_supported

#define pci_dev_g(n) list_entry(n, struct pci_dev, global_list)
#define pci_dev_b(n) list_entry(n, struct pci_dev, bus_list)

#define pci_for_each_dev(dev) \
	for(dev = pci_devices; dev; dev = dev->next)

#undef pci_resource_start
#define pci_resource_start(dev,bar) \
	(((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_SPACE) ? \
	 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_IO_MASK) : \
	 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_MEM_MASK))
#undef pci_resource_end
#define pci_resource_end(dev,bar) \
	(pci_resource_start(dev,bar) + snd_pci_compat_get_size((dev),(bar)))
#undef pci_resource_len
#define pci_resource_len(dev,bar) \
	((pci_resource_start((dev),(bar)) == 0 &&	\
	  pci_resource_end((dev),(bar)) ==		\
	  pci_resource_start((dev),(bar))) ? 0 :	\
							\
	(pci_resource_end((dev),(bar)) -		\
	 pci_resource_start((dev),(bar)) + 1))
#undef pci_resource_flags
#define pci_resource_flags(dev,bar) (snd_pci_compat_get_flags((dev),(bar)))

#define pci_request_region(dev,bar,name) snd_pci_compat_request_region(dev,bar,name)
#define pci_release_region(dev,bar) snd_pci_compat_release_region(dev,bar)
#define pci_request_regions(dev,name) snd_pci_compat_request_regions(dev,name)
#define pci_release_regions(dev) snd_pci_compat_release_regions(dev)

#define pci_save_state(dev) snd_pci_compat_save_state(dev)
#define pci_restore_state(dev) snd_pci_compat_restore_state(dev)

struct pci_device_id {
	unsigned int vendor, device;		/* Vendor and device ID or PCI_ANY_ID */
	unsigned int subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	unsigned int class, class_mask;		/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;		/* Data private to the driver */
};

struct pci_driver {
	struct list_head node;
	struct pci_dev *dev;
	char *name;
	void *owner;
	const struct pci_device_id *id_table;	/* NULL if wants all devices */
	int (*probe)(struct pci_dev *dev, const struct pci_device_id *id); /* New device inserted */
	void (*remove)(struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
	int (*suspend)(struct pci_dev *dev, u32 stgate);	/* Device suspended */
	int (*resume)(struct pci_dev *dev);	/* Device woken up */
};

/*
 *
 */
const struct pci_device_id * snd_pci_compat_match_device(const struct pci_device_id *ids, struct pci_dev *dev);
int snd_pci_compat_register_driver(struct pci_driver *drv);
void snd_pci_compat_unregister_driver(struct pci_driver *drv);
struct pci_driver *snd_pci_compat_get_pci_driver(struct pci_dev *dev);
unsigned long snd_pci_compat_get_size (struct pci_dev *dev, int n_base);
int snd_pci_compat_get_flags (struct pci_dev *dev, int n_base);
int snd_pci_compat_set_power_state(struct pci_dev *dev, int new_state);
int snd_pci_compat_enable_device(struct pci_dev *dev);
void snd_pci_compat_disable_device(struct pci_dev *dev);
int snd_pci_compat_find_capability(struct pci_dev *dev, int cap);
int snd_pci_compat_dma_supported(struct pci_dev *, dma_addr_t mask);
unsigned long snd_pci_compat_get_dma_mask(struct pci_dev *);
int snd_pci_compat_set_dma_mask(struct pci_dev *, unsigned long mask);
void * snd_pci_compat_get_driver_data (struct pci_dev *dev);
void snd_pci_compat_set_driver_data (struct pci_dev *dev, void *driver_data);
int snd_pci_compat_request_region(struct pci_dev *pdev, int bar, char *res_name);
void snd_pci_compat_release_region(struct pci_dev *pdev, int bar);
int snd_pci_compat_request_regions(struct pci_dev *pdev, char *res_name);
void snd_pci_compat_release_regions(struct pci_dev *pdev);
int snd_pci_compat_save_state(struct pci_dev *pdev);
int snd_pci_compat_restore_state(struct pci_dev *pdev);

#define pci_module_init	snd_pci_compat_register_driver

#endif /* CONFIG_PCI */

/*
 * Power management requests
 */
enum
{
	PM_SUSPEND, /* enter D1-D3 */
	PM_RESUME,  /* enter D0 */

	/* enable wake-on */
	PM_SET_WAKEUP,

	/* bus resource management */
	PM_GET_RESOURCES,
	PM_SET_RESOURCES,

	/* base station management */
	PM_EJECT,
	PM_LOCK,
};

typedef int pm_request_t;

/*
 * Device types
 */
enum
{
	PM_UNKNOWN_DEV = 0, /* generic */
	PM_SYS_DEV,	    /* system device (fan, KB controller, ...) */
	PM_PCI_DEV,	    /* PCI device */
	PM_USB_DEV,	    /* USB device */
	PM_SCSI_DEV,	    /* SCSI device */
	PM_ISA_DEV,	    /* ISA device */
};

typedef int pm_dev_t;

/*
 * System device hardware ID (PnP) values
 */
enum
{
	PM_SYS_UNKNOWN = 0x00000000, /* generic */
	PM_SYS_KBC =	 0x41d00303, /* keyboard controller */
	PM_SYS_COM =	 0x41d00500, /* serial port */
	PM_SYS_IRDA =	 0x41d00510, /* IRDA controller */
	PM_SYS_FDC =	 0x41d00700, /* floppy controller */
	PM_SYS_VGA =	 0x41d00900, /* VGA controller */
	PM_SYS_PCMCIA =	 0x41d00e00, /* PCMCIA controller */
};

#define __LINUX_PM_H	/* <linux/pm.h> in 2.2.18 is a bit stripped */

/*
 * Device identifier
 */
#define PM_PCI_ID(dev) ((dev)->bus->number << 16 | (dev)->devfn)

/*
 * Request handler callback
 */
struct pm_dev;

typedef int (*pm_callback)(struct pm_dev *dev, pm_request_t rqst, void *data);

/*
 * Dynamic device information
 */
struct pm_dev
{
	pm_dev_t	 type;
	unsigned long	 id;
	pm_callback	 callback;
	void		*data;

	unsigned long	 flags;
	int		 state;
	int		 prev_state;

	struct list_head entry;
};

#ifdef CONFIG_APM

int pm_init(void);
void pm_done(void);

#define CONFIG_PM

#define PM_IS_ACTIVE() 1

/*
 * Register a device with power management
 */
struct pm_dev *pm_register(pm_dev_t type,
			   unsigned long id,
			   pm_callback callback);

/*
 * Unregister a device with power management
 */
void pm_unregister(struct pm_dev *dev);

/*
 * Send a request to a single device
 */
int pm_send(struct pm_dev *dev, pm_request_t rqst, void *data);

#else /* CONFIG_APM */

#define PM_IS_ACTIVE() 0

extern inline struct pm_dev *pm_register(pm_dev_t type,
					 unsigned long id,
					 pm_callback callback)
{
	return 0;
}

extern inline void pm_unregister(struct pm_dev *dev) {}

extern inline int pm_send(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	return 0;
}

#endif /* CONFIG_APM */

/*
 * ISA space access
 */
#define __ISA_IO_base phys_to_virt(0)
#define isa_readb(a) readb(phys_to_virt(a))
#define isa_readw(a) readw(phys_to_virt(a))
#define isa_readl(a) readl(phys_to_virt(a))
#define isa_writeb(b,a) writeb(b,phys_to_virt(a))
#define isa_writew(w,a) writew(w,phys_to_virt(a))
#define isa_writel(l,a) writel(l,phys_to_virt(a))
#define isa_memset_io(a,b,c)		memset_io(phys_to_virt(a),(b),(c))
#define isa_memcpy_fromio(a,b,c)	memcpy_fromio((a),phys_to_virt(b),(c))
#define isa_memcpy_toio(a,b,c)		memcpy_toio(phys_to_virt(a),(b),(c))

/* USB wrapper */
#if defined(SND_NEED_USB_WRAPPER) && (defined(CONFIG_USB) || defined(CONFIG_USB_MODULE))

#include <linux/usb.h>

struct usb_ctrlrequest {
	__u8 bRequestType;
	__u8 bRequest;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;
} __attribute__ ((packed));

struct snd_compat_usb_device_id {
	__u16		match_flags;
	__u16		idVendor;
	__u16		idProduct;
	__u16		bcdDevice_lo, bcdDevice_hi;
	__u8		bDeviceClass;
	__u8		bDeviceSubClass;
	__u8		bDeviceProtocol;
	__u8		bInterfaceClass;
	__u8		bInterfaceSubClass;
	__u8		bInterfaceProtocol;
	unsigned long	driver_info;
};

struct usb_device;
struct usb_interface;

struct snd_compat_usb_driver {
	const char *name;
	void *(*probe)(struct usb_device *dev, unsigned intf, const struct snd_compat_usb_device_id *id);
	void (*disconnect)(struct usb_device *, void *);
	struct list_head driver_list;
	const struct snd_compat_usb_device_id *id_table;
};

int snd_compat_usb_register(struct snd_compat_usb_driver *);
void snd_compat_usb_deregister(struct snd_compat_usb_driver *);
void snd_compat_usb_driver_claim_interface(struct snd_compat_usb_driver *, struct usb_interface *iface, void *ptr);

#undef usb_driver
#undef usb_register
#undef usb_deregister
#undef usb_driver_claim_interface

#define usb_driver snd_compat_usb_driver
#define usb_device_id snd_compat_usb_device_id
#define usb_register(drv) snd_compat_usb_register(drv)
#define usb_deregister(drv) snd_compat_usb_deregister(drv)
#define usb_driver_claim_interface(drv,if,ptr) snd_compat_usb_driver_claim_interface(drv,if,ptr)

#define USB_DEVICE_ID_MATCH_VENDOR		0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT		0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO		0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI		0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS		0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS	0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL	0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS		0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS	0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL	0x0200

#define USB_DEVICE_ID_MATCH_DEVICE		(USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT)
#define USB_DEVICE_ID_MATCH_DEV_RANGE		(USB_DEVICE_ID_MATCH_DEV_LO | USB_DEVICE_ID_MATCH_DEV_HI)
#define USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION	(USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_DEV_RANGE)
#define USB_DEVICE_ID_MATCH_DEV_INFO \
	(USB_DEVICE_ID_MATCH_DEV_CLASS | USB_DEVICE_ID_MATCH_DEV_SUBCLASS | USB_DEVICE_ID_MATCH_DEV_PROTOCOL)
#define USB_DEVICE_ID_MATCH_INT_INFO \
	(USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL)

/* Some useful macros */
#define USB_DEVICE(vend,prod) \
	match_flags: USB_DEVICE_ID_MATCH_DEVICE, idVendor: (vend), idProduct: (prod)
#define USB_DEVICE_VER(vend,prod,lo,hi) \
	match_flags: USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION, idVendor: (vend), idProduct: (prod), bcdDevice_lo: (lo), bcdDevice_hi: (hi)
#define USB_DEVICE_INFO(cl,sc,pr) \
	match_flags: USB_DEVICE_ID_MATCH_DEV_INFO, bDeviceClass: (cl), bDeviceSubClass: (sc), bDeviceProtocol: (pr)
#define USB_INTERFACE_INFO(cl,sc,pr) \
	match_flags: USB_DEVICE_ID_MATCH_INT_INFO, bInterfaceClass: (cl), bInterfaceSubClass: (sc), bInterfaceProtocol: (pr)

#endif /* USB */

#ifdef __SMP__
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#endif
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()
#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

#define dump_stack() /*NOP*/

static inline int abs(int val)
{
	return (val < 0) ? -val : val;
}

#ifndef cpu_relax
#define cpu_relax()
#endif

#define might_sleep() do { } while (0)

#ifndef __constant_cpu_to_le32
#ifdef __LITTLE_ENDIAN
#define __constant_cpu_to_le32(x) (x)
#else
#define __constant_cpu_to_le32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))
#endif
#endif

#endif /* <2.3.0 */
