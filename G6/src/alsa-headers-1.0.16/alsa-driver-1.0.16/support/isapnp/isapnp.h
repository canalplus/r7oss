/*
 *  ISA Plug & Play support
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef LINUX_ISAPNP_H
#define LINUX_ISAPNP_H

#define ISAPNP_ALSA_LOCAL 1

/*
 *  Configuration registers (TODO: change by specification)
 */ 

#define ISAPNP_CFG_ACTIVATE		0x30	/* byte */
#define ISAPNP_CFG_MEM			0x40	/* 4 * dword */
#define ISAPNP_CFG_PORT			0x60	/* 8 * word */
#define ISAPNP_CFG_IRQ			0x70	/* 2 * word */
#define ISAPNP_CFG_DMA			0x74	/* 2 * byte */

/*
 *
 */

#define ISAPNP_VENDOR(a,b,c)	(((((a)-'A'+1)&0x3f)<<2)|\
				((((b)-'A'+1)&0x18)>>3)|((((b)-'A'+1)&7)<<13)|\
				((((c)-'A'+1)&0x1f)<<8))
#define ISAPNP_DEVICE(x)	((((x)&0xf000)>>8)|\
				 (((x)&0x0f00)>>8)|\
				 (((x)&0x00f0)<<8)|\
				 (((x)&0x000f)<<8))
#define ISAPNP_FUNCTION(x)	ISAPNP_DEVICE(x)

/*
 *
 */

#ifdef __KERNEL__

#define ISAPNP_PORT_FLAG_16BITADDR	(1<<0)
#define ISAPNP_PORT_FLAG_FIXED		(1<<1)

struct isapnp_port {
	unsigned short min;		/* min base number */
	unsigned short max;		/* max base number */
	unsigned char align;		/* align boundary */
	unsigned char size;		/* size of range */
	unsigned char flags;		/* port flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_port *next;	/* next port */
};

struct isapnp_irq {
	unsigned short map;		/* bitmaks for IRQ lines */
	unsigned char flags;		/* IRQ flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_irq *next;	/* next IRQ */
};

struct isapnp_dma {
	unsigned char map;		/* bitmask for DMA channels */
	unsigned char flags;		/* DMA flags */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_dma *next;	/* next port */
};

struct isapnp_mem {
	unsigned int min;		/* min base number */
	unsigned int max;		/* max base number */
	unsigned int align;		/* align boundary */
	unsigned int size;		/* size of range */
	unsigned char flags;		/* memory flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_mem *next;	/* next memory resource */
};

struct isapnp_mem32 {
	/* TODO */
	unsigned char data[17];
	struct isapnp_resources *res;	/* parent */
	struct isapnp_mem32 *next;	/* next 32-bit memory resource */
};

#define ISAPNP_RES_PRIORITY_PREFERRED	0
#define ISAPNP_RES_PRIORITY_ACCEPTABLE	1
#define ISAPNP_RES_PRIORITY_FUNCTIONAL	2
#define ISAPNP_RES_PRIORITY_INVALID	65535

struct isapnp_resources {
	unsigned short priority;	/* priority */
	unsigned short dependent;	/* dependent resources */
	struct isapnp_port *port;	/* first port */
	struct isapnp_irq *irq;		/* first IRQ */
	struct isapnp_dma *dma;		/* first DMA */
	struct isapnp_mem *mem;		/* first memory resource */
	struct isapnp_mem32 *mem32;	/* first 32-bit memory */
	struct isapnp_dev *dev;		/* parent */
	struct isapnp_resources *alt;	/* alternative resource (aka dependent resources) */
	struct isapnp_resources *next;	/* next resource */
};

struct isapnp_fixup {
	unsigned short vendor;		/* matching vendor */
	unsigned short device;		/* matching device */
	void (*quirk_function)(struct isapnp_dev *dev);	/* fixup function */
};
                        

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/pci.h>

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 3, 14)
#error "Please, upgrade to 2.3.15 kernel."
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 15)

#ifndef IORESOURCE_IO
struct resource {
	const char *name;
	unsigned long start, end;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};
#endif

/*
 * IO resources have these defined flags.
 */
#define IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define IORESOURCE_IO		0x00000100	/* Resource type */
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_IRQ		0x00000400
#define IORESOURCE_DMA		0x00000800

#define IORESOURCE_PREFETCH	0x00001000	/* No side effects */
#define IORESOURCE_READONLY	0x00002000
#define IORESOURCE_CACHEABLE	0x00004000
#define IORESOURCE_RANGELENGTH	0x00008000
#define IORESOURCE_SHADOWABLE	0x00010000

#define IORESOURCE_UNSET	0x20000000
#define IORESOURCE_AUTO		0x40000000
#define IORESOURCE_BUSY		0x80000000	/* Driver has marked this resource busy */

/* ISA PnP IRQ specific bits (IORESOURCE_BITS) */
#define IORESOURCE_IRQ_HIGHEDGE		(1<<0)
#define IORESOURCE_IRQ_LOWEDGE		(1<<1)
#define IORESOURCE_IRQ_HIGHLEVEL	(1<<2)
#define IORESOURCE_IRQ_LOWLEVEL		(1<<3)

/* ISA PnP DMA specific bits (IORESOURCE_BITS) */
#define IORESOURCE_DMA_TYPE_MASK	(3<<0)
#define IORESOURCE_DMA_8BIT		(0<<0)
#define IORESOURCE_DMA_8AND16BIT	(1<<0)
#define IORESOURCE_DMA_16BIT		(2<<0)

#define IORESOURCE_DMA_MASTER		(1<<2)
#define IORESOURCE_DMA_BYTE		(1<<3)
#define IORESOURCE_DMA_WORD		(1<<4)

#define IORESOURCE_DMA_SPEED_MASK	(3<<6)
#define IORESOURCE_DMA_COMPATIBLE	(0<<6)
#define IORESOURCE_DMA_TYPEA		(1<<6)
#define IORESOURCE_DMA_TYPEB		(2<<6)
#define IORESOURCE_DMA_TYPEF		(3<<6)

/* ISA PnP memory I/O specific bits (IORESOURCE_BITS) */
#define IORESOURCE_MEM_WRITEABLE	(1<<0)	/* dup: IORESOURCE_READONLY */
#define IORESOURCE_MEM_CACHEABLE	(1<<1)	/* dup: IORESOURCE_CACHEABLE */
#define IORESOURCE_MEM_RANGELENGTH	(1<<2)	/* dup: IORESOURCE_RANGELENGTH */
#define IORESOURCE_MEM_TYPE_MASK	(3<<3)
#define IORESOURCE_MEM_8BIT		(0<<3)
#define IORESOURCE_MEM_16BIT		(1<<3)
#define IORESOURCE_MEM_8AND16BIT	(2<<3)
#define IORESOURCE_MEM_SHADOWABLE	(1<<5)	/* dup: IORESOURCE_SHADOWABLE */
#define IORESOURCE_MEM_EXPANSIONROM	(1<<6)


#define DEVICE_COUNT_COMPATIBLE	4
#define DEVICE_COUNT_IRQ	2
#define DEVICE_COUNT_DMA	2
#define DEVICE_COUNT_RESOURCE	12

/*
 * The pci_dev structure is used to describe both PCI and ISAPnP devices.
 */
struct isapnp_dev {
	int active;			/* device is active */
	int ro;				/* Read/Only */

	struct isapnp_card *bus;	/* bus this device is on */
	struct isapnp_dev *sibling;	/* next device on this bus */
	struct isapnp_dev *next;	/* chain of all devices */

	void		*sysdata;	/* hook for sys-specific extension */
	struct proc_dir_entry *procent;	/* device entry in /proc/bus/pci */

	unsigned int	devfn;		/* encoded device & function index */
	unsigned short	vendor;
	unsigned short	device;
	unsigned short	subsystem_vendor;
	unsigned short	subsystem_device;
	unsigned int	class;		/* 3 bytes: (base,sub,prog-if) */
	unsigned int	hdr_type;	/* PCI header type */
	unsigned int	master : 1;	/* set if device is master capable */

	unsigned short	regs;

	/* device is compatible with these IDs */
	unsigned short vendor_compatible[DEVICE_COUNT_COMPATIBLE];
	unsigned short device_compatible[DEVICE_COUNT_COMPATIBLE];

	/*
	 * Instead of touching interrupt line and base address registers
	 * directly, use the values stored here. They might be different!
	 */
	unsigned int	irq;
	struct resource resource[DEVICE_COUNT_RESOURCE]; /* I/O and memory regions + expansion ROMs */
	struct resource dma_resource[DEVICE_COUNT_DMA];
	struct resource irq_resource[DEVICE_COUNT_IRQ];

	char		name[48];	/* Device name */

	int (*prepare)(struct isapnp_dev *dev);
	int (*activate)(struct isapnp_dev *dev);
	int (*deactivate)(struct isapnp_dev *dev);
};

struct isapnp_card {
	struct isapnp_card *parent;	/* parent bus this bridge is on */
	struct isapnp_card *children;	/* chain of P2P bridges on this bus */
	struct isapnp_card *next;	/* chain of all PCI buses */
	struct pci_ops	*ops;		/* configuration access functions */

	struct isapnp_dev *self;	/* bridge device as seen by parent */
	struct isapnp_dev *devices;	/* devices behind this bridge */

	void		*sysdata;	/* hook for sys-specific extension */
	struct proc_dir_entry *procdir;	/* directory entry in /proc/bus/pci */

	unsigned char	number;		/* bus number */
	unsigned char	primary;	/* number of primary bridge */
	unsigned char	secondary;	/* number of secondary bridge */
	unsigned char	subordinate;	/* max number of subordinate buses */

	char		name[48];
	unsigned short	vendor;
	unsigned short	device;
	unsigned int	serial;		/* serial number */
	unsigned char	pnpver;		/* Plug & Play version */
	unsigned char	productver;	/* product version */
	unsigned char	checksum;	/* if zero - checksum passed */
	unsigned char	pad1;
};

#define ISAPNP_ANY_ID           0xffff
#define ISAPNP_CARD_DEVS        8

#define ISAPNP_CARD_ID(_va, _vb, _vc, _device) \
                card_vendor: ISAPNP_VENDOR(_va, _vb, _vc), card_device: ISAPNP_DEVICE(_device)
#define ISAPNP_CARD_END \
                card_vendor: 0, card_device: 0
#define ISAPNP_DEVICE_ID(_va, _vb, _vc, _function) \
                { vendor: ISAPNP_VENDOR(_va, _vb, _vc), function: ISAPNP_FUNCTION(_function) }

#define ISAPNP_CARD_TABLE(name) \
		MODULE_GENERIC_TABLE(isapnp_card, name)

struct isapnp_card_id {
	unsigned long driver_data;      /* data private to the driver */
	unsigned short card_vendor, card_device;
	struct {
		unsigned short vendor, function;
	} devs[ISAPNP_CARD_DEVS];       /* logical devices */
};

#else

#define isapnp_dev pci_dev
#define isapnp_card pci_bus

#endif

#ifdef CONFIG_ISAPNP

#define __ISAPNP__

/* lowlevel configuration */
int isapnp_present(void);
int isapnp_cfg_begin(int csn, int device);
int isapnp_cfg_end(void);
unsigned char isapnp_read_byte(unsigned char idx);
unsigned short isapnp_read_word(unsigned char idx);
unsigned int isapnp_read_dword(unsigned char idx);
void isapnp_write_byte(unsigned char idx, unsigned char val);
void isapnp_write_word(unsigned char idx, unsigned short val);
void isapnp_write_dword(unsigned char idx, unsigned int val);
void isapnp_wake(unsigned char csn);
void isapnp_device(unsigned char device);
void isapnp_activate(unsigned char device);
void isapnp_deactivate(unsigned char device);
/* manager */
struct isapnp_card *isapnp_find_card(unsigned short vendor,
				     unsigned short device,
				     struct isapnp_card *from);
struct isapnp_dev *isapnp_find_dev(struct isapnp_card *card,
				   unsigned short vendor,
				   unsigned short function,
				   struct isapnp_dev *from);
int isapnp_probe_cards(const struct isapnp_card_id *ids,
		       int (*probe)(struct isapnp_card *card,
				const struct isapnp_card_id *id));
/* misc */
void isapnp_resource_change(struct resource *resource,
			    unsigned long start,
			    unsigned long size);
/* init/main.c */
int isapnp_init(void);

#else /* !CONFIG_ISAPNP */

/* lowlevel configuration */
extern inline int isapnp_present(void) { return 0; }
extern inline int isapnp_cfg_begin(int csn, int device) { return -ENODEV; }
extern inline int isapnp_cfg_end(void) { return -ENODEV; }
extern inline unsigned char isapnp_read_byte(unsigned char idx) { return 0xff; }
extern inline unsigned short isapnp_read_word(unsigned char idx) { return 0xffff; }
extern inline unsigned int isapnp_read_dword(unsigned char idx) { return 0xffffffff; }
extern inline void isapnp_write_byte(unsigned char idx, unsigned char val) { ; }
extern inline void isapnp_write_word(unsigned char idx, unsigned short val) { ; }
extern inline void isapnp_write_dword(unsigned char idx, unsigned int val) { ; }
extern void isapnp_wake(unsigned char csn) { ; }
extern void isapnp_device(unsigned char device) { ; }
extern void isapnp_activate(unsigned char device) { ; }
extern void isapnp_deactivate(unsigned char device) { ; }
/* manager */
extern struct isapnp_card *isapnp_find_card(unsigned short vendor,
				            unsigned short device,
				            struct isapnp_card *from) { return NULL; }
extern struct isapnp_dev *isapnp_find_dev(struct isapnp_card *card,
					  unsigned short vendor,
					  unsigned short function,
					  struct isapnp_dev *from) { return NULL; }
int isapnp_probe_cards(const struct isapnp_card_id *ids,
		       int (*probe)(struct isapnp_card *card,
				const struct isapnp_card_id *id)) { return 0; }
/* misc */
extern void isapnp_resource_change(struct resource *resource,
				   unsigned long start,
				   unsigned long size) { ; }

#endif /* CONFIG_ISAPNP */

#endif /* __KERNEL__ */
#endif /* LINUX_ISAPNP_H */
