#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
//#include <mach/hardware.h>
#include <linux/io.h>

/*
 * This is the spinlock we take for every readl/writel call. This is
 * done to ensure ordering, that the two cpus cannot issue read or
 * writes simultaneously to the PCIe cell
 */
__cacheline_aligned DEFINE_SPINLOCK(stm_pcie_io_spinlock);
EXPORT_SYMBOL(stm_pcie_io_spinlock);

/*
 * Two PCI regions. This will include the PCI conf regions as well. This is
 * probably desirable as well. These addresses are for Cannes, but I can't
 * see this workaround applying to enything else!
 */
#define PCI_PHYS_START 0x20000000
#define PCI_PHYS_END   0x40000000

static inline int is_pci_addr(unsigned long phys_addr)
{
	return (phys_addr >= PCI_PHYS_START) && (phys_addr < PCI_PHYS_END);
}

static void __iomem *__stm_ioremap_caller(unsigned long phys_addr,
					  size_t size, unsigned int mtype,
					  void *caller)
{
	volatile void __iomem *virt;

	virt = __arm_ioremap_caller(phys_addr, size, mtype, caller);
	/*
	 * Check if we produce a frobbed pointer - this can only happen
	 * with a vmsplit of 1G/3G and < 1G of memory - this is a
	 * strange split to run anyway.
	 */
	if (__stm_is_frobbed(virt))
		panic("Cannot apply PCIe workaround - increase memory size to > 1G or change vmsplit\n");

	if (is_pci_addr(phys_addr)) {
		if (mtype == MT_DEVICE_CACHED) {
			printk(KERN_ERR "Unable to apply PCIe workaround for cached regions\n");
			if (virt)
				iounmap(virt);
			return NULL;
		}
		/* WC is interesting, depends on usage so issue warning */
		if (mtype == MT_DEVICE_WC)
			printk(KERN_WARNING "PCIE workaround for WC regions is dependant on usage - check!!\n");

		virt = __stm_frob(virt);
	}

	return (void __iomem *) virt;
}

static void __stm_iounmap(volatile void __iomem *virt)
{

	if (__stm_is_frobbed(virt))
		virt = __stm_unfrob(virt);

	/* Check for nobbled pointers */
	__iounmap(virt);
}

void stm_hook_ioremap(void)
{
	arch_ioremap_caller =  __stm_ioremap_caller;
	arch_iounmap = __stm_iounmap;
	printk(KERN_INFO "PCIE tracker fixup enabled\n");
}

