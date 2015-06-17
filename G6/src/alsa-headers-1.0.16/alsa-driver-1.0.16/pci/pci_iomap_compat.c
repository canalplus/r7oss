#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 8)
#include <linux/pci.h>
#include <asm/io.h>

#ifndef fastcall
#define fastcall
#endif

/*
 * the following code is stripped from linux/lib/iomap.c
 */

#define PIO_OFFSET	0x10000UL
#define PIO_MASK	0x0ffffUL
#define PIO_RESERVED	0x40000UL

#define IO_COND(addr, is_pio, is_mmio) do {			\
	unsigned long port = (unsigned long)addr;		\
	if (port < PIO_RESERVED) {				\
		port &= PIO_MASK;				\
		is_pio;						\
	} else {						\
		is_mmio;					\
	}							\
} while (0)

static unsigned int fastcall ioread8(void __iomem *addr)
{
	IO_COND(addr, return inb(port), return readb(addr));
}
static unsigned int fastcall ioread16(void __iomem *addr)
{
	IO_COND(addr, return inw(port), return readw(addr));
}
static unsigned int fastcall ioread32(void __iomem *addr)
{
	IO_COND(addr, return inl(port), return readl(addr));
}

static void fastcall iowrite8(u8 val, void __iomem *addr)
{
	IO_COND(addr, outb(val,port), writeb(val, addr));
}
static void fastcall iowrite16(u16 val, void __iomem *addr)
{
	IO_COND(addr, outw(val,port), writew(val, addr));
}
static void fastcall iowrite32(u32 val, void __iomem *addr)
{
	IO_COND(addr, outl(val,port), writel(val, addr));
}

/* Create a virtual mapping cookie for an IO port range */
static void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
	if (port > PIO_MASK)
		return NULL;
	return (void __iomem *) (unsigned long) (port + PIO_OFFSET);
}

static void ioport_unmap(void __iomem *addr)
{
	/* Nothing to do */
}

/* Create a virtual mapping cookie for a PCI BAR (memory or IO) */
static void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	unsigned long start = pci_resource_start(dev, bar);
	unsigned long len = pci_resource_len(dev, bar);
	unsigned long flags = pci_resource_flags(dev, bar);

	if (!len || !start)
		return NULL;
	if (maxlen && len > maxlen)
		len = maxlen;
	if (flags & IORESOURCE_IO)
		return ioport_map(start, len);
	if (flags & IORESOURCE_MEM) {
		if (flags & IORESOURCE_CACHEABLE)
			return ioremap(start, len);
		return ioremap_nocache(start, len);
	}
	/* What? */
	return NULL;
}

static void pci_iounmap(struct pci_dev *dev, void __iomem * addr)
{
	IO_COND(addr, /* nothing */, iounmap(addr));
}
#endif
