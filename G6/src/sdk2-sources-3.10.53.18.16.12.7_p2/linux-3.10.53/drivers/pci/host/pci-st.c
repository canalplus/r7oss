/*
 * ST PCI express Driver for STMicroelectronics SoCs
 * ST PCIe IPs are built around a Synopsys IP Core.
 *
 * Author: Fabrice Gasnier <fabrice.gasnier@st.com>
 *
 * Copyright (C) 2003-2013 STMicroelectronics (R&D) Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/signal.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>

/* register definitions */
#define TRANSLATION_CONTROL		0x900
/* No effect in RC mode */
#define EP_TRANSLATION_ENABLE		BIT(0)
/* Controls if area is inclusive or exclusive */
#define RC_PASS_ADDR_RANGE		BIT(1)

/* Reserved in RC mode */
#define PIM0_MEM_ADDR_START		0x910
#define PIM1_MEM_ADDR_START		0x914
#define PIM2_MEM_ADDR_START		0x918

/*
 * Base of area reserved for config accesses. Fixed
 * size of 64K.
 */
#define CFG_BASE_ADDRESS		0x92c

#define CFG_REGION_SIZE			65536

/*
 * The PCIe capability registers start at this offset in configuration
 * space. Strictly speaking we should follow the caps linked list pointer,
 * but there seems little point since this is fixed
 */
#define CFG_PCIE_CAP			0x70

/* First 4K of config space has this BDF (bus,device,function) */
#define FUNC0_BDF_NUM			0x930

/*
 * There are other registers to control function 1 etc, split up into
 * 4K chunks. I cannot see any use for these, it is simpler to always
 * use FUNC0 and reprogram it as needed to drive the appropriate config
 * cycle
 */
#define FUNC_BDF_NUM(x)			(0x930 + (((x) / 2) * 4))

/* Reserved in RC mode */
#define POM0_MEM_ADDR_START		0x960
/* Start address of region 0 to be blocked/passed */
#define IN0_MEM_ADDR_START		0x964
/* End address of region 0 to be blocked/passed */
#define IN0_MEM_ADDR_LIMIT		0x968

/* Reserved in RC mode */
#define POM1_MEM_ADDR_START		0x970
/* Start address of region 1 to be blocked/passed */
#define IN1_MEM_ADDR_START		0x974
/* End address of region 1 to be blocked/passed */
#define IN1_MEM_ADDR_LIMIT		0x978

/* MSI registers */
#define MSI_ADDRESS			0x820
#define MSI_UPPER_ADDRESS		0x824
#define MSI_OFFSET_REG(n)		((n) * 0xc)
#define MSI_INTERRUPT_ENABLE(n)		(0x828 + MSI_OFFSET_REG(n))
#define MSI_INTERRUPT_MASK(n)		(0x82c + MSI_OFFSET_REG(n))
#define MSI_INTERRUPT_STATUS(n)		(0x830 + MSI_OFFSET_REG(n))
#define MSI_GPIO_REG			0x888
#define MSI_NUM_ENDPOINTS		8
#define INT_PCI_MSI_NR			(MSI_NUM_ENDPOINTS * 32)

/* This actually containes the LTSSM state machine state */
#define PORT_LOGIC_DEBUG_REG_0		0x728

/* LTSSM state machine values	*/
#define DEBUG_REG_0_LTSSM_MASK		0x1f
#define S_DETECT_QUIET			0x00
#define S_DETECT_ACT			0x01
#define S_POLL_ACTIVE			0x02
#define S_POLL_COMPLIANCE		0x03
#define S_POLL_CONFIG			0x04
#define S_PRE_DETECT_QUIET		0x05
#define S_DETECT_WAIT			0x06
#define S_CFG_LINKWD_START		0x07
#define S_CFG_LINKWD_ACEPT		0x08
#define S_CFG_LANENUM_WAIT		0x09
#define S_CFG_LANENUM_ACEPT		0x0A
#define S_CFG_COMPLETE			0x0B
#define S_CFG_IDLE			0x0C
#define S_RCVRY_LOCK			0x0D
#define S_RCVRY_SPEED			0x0E
#define S_RCVRY_RCVRCFG			0x0F
#define S_RCVRY_IDLE			0x10
#define S_L0				0x11
#define S_L0S				0x12
#define S_L123_SEND_EIDLE		0x13
#define S_L1_IDLE			0x14
#define S_L2_IDLE			0x15
#define S_L2_WAKE			0x16
#define S_DISABLED_ENTRY		0x17
#define S_DISABLED_IDLE			0x18
#define S_DISABLED			0x19
#define S_LPBK_ENTRY			0x1A
#define S_LPBK_ACTIVE			0x1B
#define S_LPBK_EXIT			0x1C
#define S_LPBK_EXIT_TIMEOUT		0x1D
#define S_HOT_RESET_ENTRY		0x1E
#define S_HOT_RESET			0x1F

/* syscfg bits */
#define PCIE_SYS_INT			BIT(5)
#define PCIE_APP_REQ_RETRY_EN		BIT(3)
#define PCIE_APP_LTSSM_ENABLE		BIT(2)
#define PCIE_APP_INIT_RST		BIT(1)
#define PCIE_DEVICE_TYPE		BIT(0)
#define PCIE_DEFAULT_VAL		PCIE_DEVICE_TYPE

/**
 * struct st_msi - msi private data
 * @chip: irq chip for PCIe MSI
 * @used: bitmap for MSI IRQs
 * @regs: PCIe control registers
 * @domain: IRQ domain for MSI controller
 * @mux_irq: MSI irq
 * @reg_lock: Access lock to MSI registers
 * @lock: irq bitmap access lock
 */
struct st_msi {
	struct msi_chip chip;
	DECLARE_BITMAP(used, INT_PCI_MSI_NR);
	void __iomem *regs;
	struct irq_domain *domain;
	int mux_irq;
	spinlock_t reg_lock;
	struct mutex lock;
};

static inline struct st_msi *to_st_msi(struct msi_chip *chip)
{
	return container_of(chip, struct st_msi, chip);
}

struct st_pcie;

/**
 * struct st_pcie_ops - SOC dependent data
 * @init: reference to controller power & reset init routine
 * @enable_ltssm: reference to controller link enable routine
 * @disable_ltssm:  reference to controller link disable routine
 * @phy_auto: flag when phy automatically configured
 * @secure_mode: flag when security is on
 */
struct st_pcie_ops {
	int (*init)(struct st_pcie *st_pcie);
	int (*enable_ltssm)(struct st_pcie *st_pcie);
	int (*disable_ltssm)(struct st_pcie *st_pcie);
	bool phy_auto;
	bool secure_mode;
};

/**
 * struct st_pcie - private data of the controller
 * @dev: device for this controller
 * @cntrl: PCIe control registers
 * @ahb: Amba->stbus convertor registers
 * @syscfg0: PCIe configuration 0 register, regmap offset
 * @syscfg1: PCIe configuration 1 register, regmap offset
 * @ahb_val: Amba->stbus convertor tuning
 * @msi: private msi structure
 * @phy: associated pcie phy
 * @config_area: PCIe configuration space
 * @io: PCIe I/O space
 * @mem: PCIe non-prefetchable memory space
 * @prefetch: PCIe prefetchable memory space
 * @lmi: memory made available to the controller
 * @abort_lock: prevent config space access race
 * @data: SOC dependent data
 * @regmap: Syscfg registers bank in which PCIe port is configured
 * @pwr: power control
 * @rst: reset control
 * @irq: PCIe INTA, INTB...
 * @irq_lines: PCIe INTx lines number
 * @reset_gpio: optional reset gpio
 */
struct st_pcie {
	struct device *dev;

	void __iomem *cntrl;
	void __iomem *ahb;
	int syscfg0;
	int syscfg1;
	u32 ahb_val;

	struct st_msi *msi;
	struct phy *phy;

	void __iomem *config_area;
	struct resource io;
	struct resource mem;
	struct resource prefetch;
	struct resource busn;
	struct resource *lmi;
	spinlock_t abort_lock;

	const struct st_pcie_ops *data;
	struct regmap *regmap;
	struct reset_control *pwr;
	struct reset_control *rst;
	int irq[4];
	int irq_lines;
	int reset_gpio;
	phys_addr_t config_window_start;
};

static inline struct st_pcie *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

/*
 * Routines to access the DBI port of the synopsys IP. This contains the
 * standard config registers, as well as some other registers. Unfortunately,
 * IP only support word access. Little helper function to build up byte and
 * half word data access.
 *
 * We do not have a spinlock for these, so be careful of your usage. Relies on
 * the config spinlock for config cycles.
 */
static inline u32 shift_data_read(int where, int size, u32 data)
{
	data >>= (8 * (where & 0x3));

	switch (size) {
	case 1:
		data &= 0xff;
		break;
	case 2:
		BUG_ON(where & 1);
		data &= 0xffff;
		break;
	case 4:
		BUG_ON(where & 3);
		break;
	default:
		BUG();
	}

	return data;
}

static u32 dbi_read(struct st_pcie *priv, int where, int size)
{
	u32 data;

	/* Read the dword aligned data */
	data = readl(priv->cntrl + (where & ~0x3));

	return shift_data_read(where, size, data);
}

static inline u8 dbi_readb(struct st_pcie *priv, unsigned addr)
{
	return (u8)dbi_read(priv, addr, 1);
}

static inline u16 dbi_readw(struct st_pcie *priv, unsigned addr)
{
	return (u16)dbi_read(priv, addr, 2);
}

static inline u32 dbi_readl(struct st_pcie *priv, unsigned addr)
{
	return dbi_read(priv, addr, 4);
}

static inline u32 shift_data_write(int where, int size, u32 val, u32 data)
{
	int shift_bits = (where & 0x3) * 8;

	switch (size) {
	case 1:
		data &= ~(0xff << shift_bits);
		data |= ((val & 0xff) << shift_bits);
		break;
	case 2:
		data &= ~(0xffff << shift_bits);
		data |= ((val & 0xffff) << shift_bits);
		BUG_ON(where & 1);
		break;
	case 4:
		data = val;
		BUG_ON(where & 3);
		break;
	default:
		BUG();
	}

	return data;
}

static void dbi_write(struct st_pcie *priv,
		      u32 val, int where, int size)
{
	u32 uninitialized_var(data);
	int aligned_addr = where & ~0x3;

	/* Read the dword aligned data if we have to */
	if (size != 4)
		data = readl(priv->cntrl + aligned_addr);

	data = shift_data_write(where, size, val, data);

	writel(data, priv->cntrl + aligned_addr);
}

static inline void dbi_writeb(struct st_pcie *priv,
			      u8 val, unsigned addr)
{
	dbi_write(priv, (u32) val, addr, 1);
}

static inline void dbi_writew(struct st_pcie *priv,
			      u16 val, unsigned addr)
{
	dbi_write(priv, (u32) val, addr, 2);
}

static inline void dbi_writel(struct st_pcie *priv,
			      u32 val, unsigned addr)
{
	dbi_write(priv, (u32) val, addr, 4);
}


static inline void pcie_cap_writew(struct st_pcie *priv,
				   u16 val, unsigned cap)
{
	dbi_writew(priv, val, CFG_PCIE_CAP + cap);
}

static inline u16 pcie_cap_readw(struct st_pcie *priv,
				 unsigned cap)
{
	return dbi_readw(priv, CFG_PCIE_CAP + cap);
}

/* Time to wait between testing the link in msecs (hardware poll interval) */
#define LINK_LOOP_DELAY_MS 1
/* Total amount of time to wait for the link to come up in msecs */
#define LINK_WAIT_MS 120
#define LINK_LOOP_COUNT (LINK_WAIT_MS / LINK_LOOP_DELAY_MS)

/*
 * Function to test if the link is in an operational state or not. We must
 * ensure the link is operational before we try to do a configuration access.
 */
static int link_up(struct st_pcie *priv)
{
	u32 status;
	int link_up;
	int count = 0;

	/*
	 * We have to be careful here. This is used in config read/write,
	 * The higher levels switch off interrupts, so you cannot use
	 * jiffies to do a timeout, or reschedule
	 */
	do {
		/*
		 * What about L2? I think software intervention is
		 * required to get it out of L2, so in effect the link
		 * is down. Requires more work when/if we implement power
		 * management
		 */
		status = dbi_readl(priv, PORT_LOGIC_DEBUG_REG_0);
		status &= DEBUG_REG_0_LTSSM_MASK;

		link_up = (status == S_L0) || (status == S_L0S) ||
			  (status == S_L1_IDLE);

		/*
		 * It can take some considerable time for the link to actually
		 * come up, caused by the PLLs. Experiments indicate it takes
		 * about 8ms to actually bring the link up, but this can vary
		 * considerably according to the specification. This code should
		 * allow sufficient time
		 */
		if (!link_up)
			mdelay(LINK_LOOP_DELAY_MS);

	} while (!link_up  && ++count < LINK_LOOP_COUNT);

	return link_up;
}

/*
 * On ARM platforms, we actually get a bus error returned when the PCIe IP
 * returns a UR or CRS instead of an OK. What we do to try to work around this
 * is hook the arm async abort exception and then check if the pc value is in
 * the region we expect bus errors could be generated. Fortunately we can
 * constrain the area the CPU will generate the async exception with the use of
 * a barrier instruction
 *
 * The abort_flag is set if we see a bus error returned when we make config
 * requests.  It doesn't need to be an atomic variable, since it can only be
 * looked at safely in the regions protected by the spinlock anyway. However,
 * making it atomic avoids the need for volatile wierdness to prevent the
 * compiler from optimizing incorrectly
 */
static atomic_t abort_flag;

/*
 * Holds the addresses where we are expecting an abort to be generated. We only
 * have to cope with one at a time as config read/write are spinlocked so
 * cannot be in the critical code section at the same time.
 */
static unsigned long abort_start, abort_end;

static int st_pcie_abort(unsigned long addr, unsigned int fsr,
			  struct pt_regs *regs)
{
	unsigned long pc = regs->ARM_pc;

	/*
	 * If it isn't the expected place, then return 1 which will then fall
	 * through to the default error handler. This means that if we get a
	 * bus error for something other than PCIE config read/write accesses
	 * we will not just carry on silently.
	 */
	if (pc < abort_start || pc >= abort_end)
		return 1;

	/* Again, if it isn't an async abort then return to default handler */
	if (!((fsr & (1 << 10)) && ((fsr & 0xf) == 0x6)))
		return 1;

	/* Set abort flag */
	atomic_set(&abort_flag, 1);

	/* Barrier to ensure propogation */
	mb();

	return 0;
}

/*
 * The PCI express core IP expects the following arrangement on it's address
 * bus (slv_haddr) when driving config cycles.
 * bus_number		[31:24]
 * dev_number		[23:19]
 * func_number		[18:16]
 * unused		[15:12]
 * ext_reg_number	[11:8]
 * reg_number		[7:2]
 *
 * Bits [15:12] are unused.
 *
 * In the glue logic there is a 64K region of address space that can be
 * written/read to generate config cycles. The base address of this is
 * controlled by CFG_BASE_ADDRESS. There are 8 16 bit registers called
 * FUNC0_BDF_NUM to FUNC8_BDF_NUM. These split the bottom half of the 64K
 * window into 8 regions at 4K boundaries (quite what the other 32K is for is a
 * mystery). These control the bus,device and function number you are trying to
 * talk to.
 *
 * The decision on whether to generate a type 0 or type 1 access is controlled
 * by bits 15:12 of the address you write to.  If they are zero, then a type 0
 * is generated, if anything else it will be a type 1. Thus the bottom 4K
 * region controlled by FUNC0_BDF_NUM can only generate type 0, all the others
 * can only generate type 1.
 *
 * We only use FUNC0_BDF_NUM and FUNC1_BDF_NUM. Which one you use is selected
 * by bit 12 of the address you write to. The selected register is then used
 * for the top 16 bits of the slv_haddr to form the bus/dev/func, bit 15:12 are
 * wired to zero, and bits 11:2 form the address of the register you want to
 * read in config space.
 *
 * We always write FUNC0_BDF_NUM as a 32 bit write. So if we want type 1
 * accesses we have to shift by 16 so in effect we are writing to FUNC1_BDF_NUM
 */

static inline u32 bdf_num(int bus, int devfn, int is_root_bus)
{
	return ((bus << 8) | devfn) << (is_root_bus ? 0 : 16);
}

static inline unsigned config_addr(int where, int is_root_bus)
{
	return (where & ~3) | (!is_root_bus << 12);
}

static int st_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	u32 bdf;
	u32 data;
	int slot = PCI_SLOT(devfn);
	unsigned long flags;
	struct st_pcie *priv = sys_to_pcie(bus->sysdata);
	int is_root_bus = pci_is_root_bus(bus);
	int retry_count = 0;
	int ret;
#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	void __iomem *raw_config_area = priv->config_area;
	raw_config_area = __stm_unfrob(priv->config_area);
#endif
	/*
	 * PCI express devices will respond to all config type 0 cycles, since
	 * they are point to point links. Thus to avoid probing for multiple
	 * devices on the root bus we simply ignore any request for anything
	 * other than slot 1 if it is on the root bus. The switch will reject
	 * requests for slots it knows do not exist.
	 *
	 * We have to check for the link being up as we will hang if we issue
	 * a config request and the link is down.
	 */
	if (!priv || (is_root_bus && slot != 1) || !link_up(priv)) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	bdf = bdf_num(bus->number, devfn, is_root_bus);

retry:
	/*
	 * Claim lock, FUNC[01]_BDF_NUM has to remain unchanged for the whole
	 * cycle
	 */
	spin_lock_irqsave(&priv->abort_lock, flags);

	/* Set the config packet devfn */
	dbi_writel(priv, bdf, FUNC0_BDF_NUM);
	/*
	 * We have to read it back here, as there is a race condition between
	 * the write to the FUNC0_BDF_NUM and the actual read from the config
	 * address space. These appear to be different stbus targets, and it
	 * looks as if the read can overtake the write sometimes. The result of
	 * this is that you end up reading from the wrong device.
	 */
	dbi_readl(priv, FUNC0_BDF_NUM);

	atomic_set(&abort_flag, 0);
	ret = PCIBIOS_SUCCESSFUL;

#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	/*
	 * We must take the IO spinlock here, to prevent the other CPU issuing
	 * a non config readl/writel. We know irqs are off, so we can use a
	 * standard spinlock
	 */
	spin_lock(&stm_pcie_io_spinlock);
#endif

	/*
	 * Get the addresses of where we expect the aborts to happen. These are
	 * placed as close as possible to the code due to branch length
	 * limitations on the ARM
	 */
	abort_start = (unsigned long)&&config_read_start;
	abort_end = (unsigned long)&&config_read_end;

	/*
	 * Mark start of region where we can expect bus errors. This assumes
	 * that readl is actually a macro not a real function call which is
	 * the case on our ARM platforms.
	 */
config_read_start:
#ifndef CONFIG_STM_PCIE_TRACKER_BUG
	/* Read the dword aligned data */
	data = readl(priv->config_area + config_addr(where, is_root_bus));
#else // CONFIG_STM_PCIE_TRACKER_BUG

	/*
	 * Read the dword aligned data. If we have the tracker bug workaround
	 * enabled, we MUST use the raw variant. Otherwise we will lose the
	 * spinlock when we restart the abort, which would be disasterous.
	 *
	 * This means we have to unfrob the pointer before we call this,
	 * otherwise we will blow up
	 *
	 * It is also vital that whatever is called here does NOT make any
	 * function calls.
	 */

	data = __raw_readl(raw_config_area +
			   config_addr(where, is_root_bus));

#endif // CONFIG_STM_PCIE_TRACKER_BUG

	mb(); /* Barrier to force bus error to go no further than here */

config_read_end:

#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	spin_unlock(&stm_pcie_io_spinlock);
#endif
	/* If the trap handler has fired, then set the data to 0xffffffff */
	if (atomic_read(&abort_flag)) {
		ret = PCIBIOS_DEVICE_NOT_FOUND;
		data = ~0;
	}

	spin_unlock_irqrestore(&priv->abort_lock, flags);

	/*
	 * This is intended to help with when we are probing the bus. The
	 * problem is that the wrapper logic doesn't have any way to
	 * interrogate if the configuration request failed or not. The read
	 * will return 0 if something has gone wrong.
	 *
	 * On the ARM we actually get a real bus error, which is what the
	 * abort_flag check above is checking for.
	 *
	 * Unfortunately this means it is impossible to tell the difference
	 * between when a device doesn't exist (the switch will return a UR
	 * completion) or the device does exist but isn't yet ready to accept
	 * configuration requests (the device will return a CRS completion)
	 *
	 * The result of this is that we will miss devices when probing.
	 *
	 * So if we are trying to read the dev/vendor id on devfn 0 and we
	 * appear to get zero back, then we retry the request.  We know that
	 * zero can never be a valid device/vendor id. The specification says
	 * we must retry for up to a second before we decide the device is
	 * dead. If we are still dead then we assume there is nothing there and
	 * return ~0
	 *
	 * The downside of this is that we incur a delay of 1s for every pci
	 * express link that doesn't have a device connected.
	 */

	if (((where&~3) == 0) && devfn == 0 && (data == 0 || data == ~0)) {
		if (retry_count++ < 1000) {
			mdelay(1);
			goto retry;
		} else {
			*val = ~0;
			return PCIBIOS_DEVICE_NOT_FOUND;
		}
	}

	*val = shift_data_read(where, size, data);

	return ret;
}

static int st_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	unsigned long flags;
	u32 bdf;
	u32 data = 0;
	int slot = PCI_SLOT(devfn);
	struct st_pcie *priv = sys_to_pcie(bus->sysdata);
	int is_root_bus = pci_is_root_bus(bus);
	int ret;
#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	void __iomem *raw_config_area = priv->config_area;
	raw_config_area = __stm_unfrob(priv->config_area);
#endif

	if (!priv || (is_root_bus && slot != 1) || !link_up(priv))
		return PCIBIOS_DEVICE_NOT_FOUND;

	bdf = bdf_num(bus->number, devfn, is_root_bus);

	/*
	 * Claim lock, FUNC[01]_BDF_NUM has to remain unchanged for the whole
	 * cycle
	 */
	spin_lock_irqsave(&priv->abort_lock, flags);

	/* Set the config packet devfn */
	dbi_writel(priv, bdf, FUNC0_BDF_NUM);
	dbi_readl(priv, FUNC0_BDF_NUM);

	atomic_set(&abort_flag, 0);

#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	/*
	 * We must take the IO spinlock here, to prevent the other CPU issuing
	 * a non config readl/writel. We know irqs are off, so we can use a
	 * standard spinlock
	 */
	spin_lock(&stm_pcie_io_spinlock);
#endif

	abort_start = (unsigned long)&&config_write_start;
	abort_end = (unsigned long)&&config_write_end;

	/* We can expect bus errors for the read and the write */
config_write_start:

#ifndef CONFIG_STM_PCIE_TRACKER_BUG
	/* Read the dword aligned data */
	if (size != 4)
		data = readl(priv->config_area +
			     config_addr(where, is_root_bus));
#else
	/* Read the dword aligned data */
	if (size != 4)
		data = __raw_readl(raw_config_area +
			     config_addr(where, is_root_bus));
#endif
	mb();

	data = shift_data_write(where, size, val, data);

#ifndef CONFIG_STM_PCIE_TRACKER_BUG
	writel(data, priv->config_area + config_addr(where, is_root_bus));
#else
	__raw_writel(data, raw_config_area + config_addr(where, is_root_bus));
#endif

	mb();

config_write_end:

#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	spin_unlock(&stm_pcie_io_spinlock);
#endif
	ret = atomic_read(&abort_flag) ? PCIBIOS_DEVICE_NOT_FOUND
				       : PCIBIOS_SUCCESSFUL;

	spin_unlock_irqrestore(&priv->abort_lock, flags);

	return ret;
}

static struct pci_ops st_pcie_config_ops = {
	.read = st_pcie_config_read,
	.write = st_pcie_config_write,
};

static void st_pcie_board_reset(struct st_pcie *pcie)
{
	if (!gpio_is_valid(pcie->reset_gpio))
		return;

	if (gpio_direction_output(pcie->reset_gpio, 0)) {
		dev_err(pcie->dev, "Cannot set PERST# (gpio %u) to output\n",
			pcie->reset_gpio);
		return;
	}

	/* From PCIe spec */
	usleep_range(1000, 2000);
	gpio_direction_output(pcie->reset_gpio, 1);

	/*
	 * PCIe specification states that you should not issue any config
	 * requests until 100ms after asserting reset, so we enforce that here
	 */
	usleep_range(100000, 150000);
}

static int st_pcie_hw_setup(struct st_pcie *priv,
				       phys_addr_t lmi_window_start,
				       unsigned long lmi_window_size,
				       phys_addr_t config_window_start)
{
	int err;

	err = priv->data->disable_ltssm(priv);
	if (err)
		return err;

	/* Don't see any point in enabling IO here */
	dbi_writew(priv, PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER, PCI_COMMAND);

	/* Set up the config window to the top of the PCI address space */
	dbi_writel(priv, config_window_start, CFG_BASE_ADDRESS);

	/*
	 * Open up all of memory to the PCI controller. We could do slightly
	 * better than this and exclude the kernel text segment and bss etc.
	 * They are base/limit registers so can be of arbitrary alignment
	 * presumably
	 */
	if (!priv->data->secure_mode) {
		dbi_writel(priv, lmi_window_start, IN0_MEM_ADDR_START);
		dbi_writel(priv, lmi_window_start + lmi_window_size - 1,
		   IN0_MEM_ADDR_LIMIT);
		/* Disable the 2nd region */
		dbi_writel(priv, ~0, IN1_MEM_ADDR_START);
		dbi_writel(priv, 0, IN1_MEM_ADDR_LIMIT);
		dbi_writel(priv, RC_PASS_ADDR_RANGE, TRANSLATION_CONTROL);
	}

	if (priv->ahb) {
		/*  bridge settings */
		writel(priv->ahb_val, priv->ahb + 4);
		/*
		 * Later versions of the AMBA bridge have a merging capability,
		 * whereby reads and writes are merged into an AHB burst
		 * transaction. This breaks PCIe as it will then prefetch a
		 * maximally sized transaction.
		 * We have to disable this capability, which is controlled
		 * by bit 3 of the SD_CONFIG register. Bit 2 (the busy bit)
		 * must always be written as zero. We set the cont_on_error
		 * bit, as this is the reset state.
		*/
		writel(0x2, priv->ahb);
	}

	/* Now assert the board level reset to the other PCIe device */
	st_pcie_board_reset(priv);

	/* Re-enable the link */
	return priv->data->enable_ltssm(priv);
}

static int remap_named_resource(struct platform_device *pdev,
					  const char *name,
					  void __iomem **io_ptr)
{
	struct resource *res;
	resource_size_t size;
	void __iomem *p;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res)
		return -ENXIO;

	size = resource_size(res);

	if (!devm_request_mem_region(&pdev->dev,
				     res->start, size, name))
		return -EBUSY;

	p = devm_ioremap_nocache(&pdev->dev, res->start, size);
	if (!p)
		return -ENOMEM;

	*io_ptr = p;

	return 0;
}

static irqreturn_t st_pcie_sys_err(int irq, void *dev_data)
{
	panic("PCI express serious error raised\n");
	return IRQ_HANDLED;
}

static int st_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct st_pcie *pcie = sys_to_pcie(dev->bus->sysdata);
	int i = (slot - 1 + pin - 1) % 4;

	if (i >= pcie->irq_lines)
		i = 0;

	dev_info(pcie->dev,
		"PCIe map irq: %04d:%02x:%02x.%02x slot %d, pin %d, irq: %d\n",
		pci_domain_nr(dev->bus), dev->bus->number, PCI_SLOT(dev->devfn),
		PCI_FUNC(dev->devfn), slot, pin, pcie->irq[i]);

	return pcie->irq[i];
}

static int st_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct st_pcie *pcie = sys_to_pcie(sys);

	if (pcie->io.start)
		pci_add_resource(&sys->resources, &pcie->io);

	pci_add_resource(&sys->resources, &pcie->mem);
	pci_add_resource(&sys->resources, &pcie->prefetch);
	pci_add_resource(&sys->resources, &pcie->busn);

	return 1;
}

static struct pci_bus *st_pcie_scan(int nr, struct pci_sys_data *sys)
{
	struct st_pcie *pcie = sys_to_pcie(sys);
	struct device_node *np_gpio = NULL;
	int gpio_pin, ret;

	/* DTI744CNL:  wifi reset */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform configuration\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {
		/* Wait for the clock stabilization */
		mdelay(10);

		/* Now the clock is running, we may release wifi reset */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-reset", 0);
		ret = gpio_request(gpio_pin, "wifi-reset");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
			/* Pull wifi reset up */
			gpio_direction_output(gpio_pin, 1);
			gpio_free(gpio_pin);
			/* Wait for being sure wifi module well reset before scanning */
			mdelay(10);
		}
	}
	/* end DTI744CNL:  wifi reset */

	return pci_scan_root_bus(pcie->dev, sys->busnr, &st_pcie_config_ops,
				 sys, &sys->resources);
}

static irqreturn_t st_msi_irq_demux(int mux_irq, void *data)
{
	struct st_pcie *pcie = data;
	struct st_msi *msi = pcie->msi;
	int ep, processed = 0;
	unsigned long status;
	u32 mask;
	u32 offset;
	unsigned int index;
	unsigned int irq;

	for (ep = 0; ep < MSI_NUM_ENDPOINTS; ep++) {
		do {
			status = readl(msi->regs + MSI_INTERRUPT_STATUS(ep));
			mask = readl(msi->regs + MSI_INTERRUPT_MASK(ep));
			status &= ~mask;

			offset = find_first_bit(&status, 32);
			index = ep * 32 + offset;

			/* clear the interrupt */
			writel(1 << offset,
					msi->regs + MSI_INTERRUPT_STATUS(ep));
			readl(msi->regs + MSI_INTERRUPT_STATUS(ep));

			irq = irq_find_mapping(msi->domain, index);
			if (irq) {
				if (test_bit(index, msi->used))
					generic_handle_irq(irq);
				else
					dev_info(pcie->dev, "unhandled MSI\n");
			} else {
				/*
				 * that's weird who triggered this?
				 * just clear it
				 */
				dev_info(pcie->dev, "unexpected MSI\n");
			}

			processed++;
		} while (status);
	}

	return processed > 0 ? IRQ_HANDLED : IRQ_NONE;
}

static inline void set_msi_bit(struct irq_data *data, unsigned int reg_base)
{
	struct st_pcie *pcie = irq_data_get_irq_handler_data(data);
	struct st_msi *msi = pcie->msi;
	int ep = data->hwirq / 32;
	unsigned int offset = data->hwirq & 0x1F;
	int val;
	unsigned long flags;

	spin_lock_irqsave(&msi->reg_lock, flags);

	val = readl(msi->regs + reg_base + MSI_OFFSET_REG(ep));
	val |= 1 << offset;
	writel(val, msi->regs + reg_base + MSI_OFFSET_REG(ep));

	/* Read back for write posting */
	readl(msi->regs + reg_base + MSI_OFFSET_REG(ep));

	spin_unlock_irqrestore(&msi->reg_lock, flags);
}

static inline void clear_msi_bit(struct irq_data *data, unsigned int reg_base)
{
	struct st_pcie *pcie = irq_data_get_irq_handler_data(data);
	struct st_msi *msi = pcie->msi;
	int ep = data->hwirq / 32;
	unsigned int offset = data->hwirq & 0x1F;
	int val;
	unsigned long flags;

	spin_lock_irqsave(&msi->reg_lock, flags);

	val = readl(msi->regs + reg_base + MSI_OFFSET_REG(ep));
	val &= ~(1 << offset);
	writel(val, msi->regs + reg_base + MSI_OFFSET_REG(ep));

	/* Read back for write postng */
	readl(msi->regs + reg_base + MSI_OFFSET_REG(ep));

	spin_unlock_irqrestore(&msi->reg_lock, flags);
}

static void st_enable_msi_irq(struct irq_data *data)
{
	set_msi_bit(data, MSI_INTERRUPT_ENABLE(0));
	/*
	 * The generic code will have masked all interrupts for device that
	 * support the optional Mask capability. Therefore  we have to unmask
	 * interrupts on the device if the device supports the Mask capability.
	 *
	 * We do not have to this in the irq mask/unmask functions, as we can
	 * mask at the MSI interrupt controller itself.
	 */
	unmask_msi_irq(data);
}

static void st_disable_msi_irq(struct irq_data *data)
{
	/* Disable the msi irq on the device */
	mask_msi_irq(data);

	clear_msi_bit(data, MSI_INTERRUPT_ENABLE(0));
}

static void st_mask_msi_irq(struct irq_data *data)
{
	set_msi_bit(data, MSI_INTERRUPT_MASK(0));
}

static void st_unmask_msi_irq(struct irq_data *data)
{
	clear_msi_bit(data, MSI_INTERRUPT_MASK(0));
}

static struct irq_chip st_msi_irq_chip = {
	.name = "ST PCIe MSI",
	.irq_enable = st_enable_msi_irq,
	.irq_disable = st_disable_msi_irq,
	.irq_mask = st_mask_msi_irq,
	.irq_unmask = st_unmask_msi_irq,
};


static void st_msi_init_one(struct st_msi *msi)
{
	int ep;

	/*
	 * Set the magic address the hardware responds to. This has to be in
	 * the range the PCI controller can write to. We just use the value
	 * of the st_msi data structure, but anything will do
	 */
	writel(0, msi->regs + MSI_UPPER_ADDRESS);
	writel(virt_to_phys(msi), msi->regs + MSI_ADDRESS);

	/* Disable everything to start with */
	for (ep = 0; ep < MSI_NUM_ENDPOINTS; ep++) {
		writel(0, msi->regs + MSI_INTERRUPT_ENABLE(ep));
		writel(0, msi->regs + MSI_INTERRUPT_MASK(ep));
		writel(~0, msi->regs + MSI_INTERRUPT_STATUS(ep));
	}
}

static int st_msi_map(struct irq_domain *domain, unsigned int irq,
			 irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &st_msi_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, domain->host_data);
	set_irq_flags(irq, IRQF_VALID);

	return 0;
}

static const struct irq_domain_ops msi_domain_ops = {
	.map = st_msi_map,
};

static int st_msi_alloc(struct st_msi *chip)
{
	int msi;

	mutex_lock(&chip->lock);

	msi = find_first_zero_bit(chip->used, INT_PCI_MSI_NR);
	if (msi < INT_PCI_MSI_NR)
		set_bit(msi, chip->used);
	else
		msi = -ENOSPC;

	mutex_unlock(&chip->lock);

	return msi;
}

static void st_msi_free(struct st_msi *chip, unsigned long irq)
{
	struct device *dev = chip->chip.dev;

	mutex_lock(&chip->lock);

	if (!test_bit(irq, chip->used))
		dev_err(dev, "trying to free unused MSI#%lu\n", irq);
	else
		clear_bit(irq, chip->used);

	mutex_unlock(&chip->lock);
}

static int st_setup_msi_irq(struct msi_chip *chip, struct pci_dev *pdev,
			       struct msi_desc *desc)
{
	struct st_msi *msi = to_st_msi(chip);
	struct msi_msg msg;
	unsigned int irq;
	int hwirq;

	hwirq = st_msi_alloc(msi);
	if (hwirq < 0)
		return hwirq;

	irq = irq_create_mapping(msi->domain, hwirq);
	if (!irq)
		return -EINVAL;

	irq_set_msi_desc(irq, desc);

	/*
	 * Set up the data the card needs in order to raise
	 * interrupt. The format of the data word is
	 * [7:5] Selects endpoint register
	 * [4:0] Interrupt number to raise
	 */
	msg.data = hwirq;
	msg.address_hi = 0;
	msg.address_lo = virt_to_phys(msi);

	write_msi_msg(irq, &msg);

	return 0;
}

static void st_teardown_msi_irq(struct msi_chip *chip, unsigned int irq)
{
	struct st_msi *msi = to_st_msi(chip);
	struct irq_data *d = irq_get_irq_data(irq);

	st_msi_free(msi, d->hwirq);
}

static int st_pcie_enable_msi(struct st_pcie *pcie)
{
	int err;
	struct platform_device *pdev = to_platform_device(pcie->dev);
	struct st_msi *msi;

	msi = devm_kzalloc(&pdev->dev, sizeof(*msi), GFP_KERNEL);
	if (!msi)
		return -ENOMEM;

	pcie->msi = msi;

	spin_lock_init(&msi->reg_lock);
	mutex_init(&msi->lock);

	/* Copy over the register pointer for convenience */
	msi->regs = pcie->cntrl;

	msi->chip.dev = pcie->dev;
	msi->chip.setup_irq = st_setup_msi_irq;
	msi->chip.teardown_irq = st_teardown_msi_irq;

	msi->domain = irq_domain_add_linear(pcie->dev->of_node, INT_PCI_MSI_NR,
					    &msi_domain_ops, &msi->chip);
	if (!msi->domain) {
		dev_err(&pdev->dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	err = platform_get_irq_byname(pdev, "msi");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get IRQ: %d\n", err);
		goto err;
	}

	msi->mux_irq = err;

	st_msi_init_one(msi);

	err = request_irq(msi->mux_irq, st_msi_irq_demux, 0,
			  st_msi_irq_chip.name, pcie);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request IRQ: %d\n", err);
		goto err;
	}

	return 0;

err:
	irq_domain_remove(msi->domain);
	return err;
}

static int st_pcie_disable_msi(struct st_pcie *pcie)
{
	int i, irq;
	struct st_msi *msi = pcie->msi;

	st_msi_init_one(msi);

	if (msi->mux_irq > 0)
		free_irq(msi->mux_irq, pcie);

	for (i = 0; i < INT_PCI_MSI_NR; i++) {
		irq = irq_find_mapping(msi->domain, i);
		if (irq > 0)
			irq_dispose_mapping(irq);
	}

	irq_domain_remove(msi->domain);

	return 0;
}

static int st_pcie_parse_dt(struct st_pcie *pcie)
{
	struct device_node *np = pcie->dev->of_node;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource res;
	int err;

	if (of_pci_range_parser_init(&parser, np)) {
		dev_err(pcie->dev, "missing \"ranges\" property\n");
		return -EINVAL;
	}

	for_each_of_pci_range(&parser, &range) {
		of_pci_range_to_resource(&range, np, &res);

		dev_dbg(pcie->dev, "start 0x%08x end 0x%08x flags 0x%08lx\n",
				res.start, res.end, res.flags);

		switch (res.flags & IORESOURCE_TYPE_BITS) {
		case IORESOURCE_IO:
			memcpy(&pcie->io, &res, sizeof(res));
			pcie->io.name = "I/O";
			break;

		case IORESOURCE_MEM:
			if (res.flags & IORESOURCE_PREFETCH) {
				memcpy(&pcie->prefetch, &res, sizeof(res));
				pcie->prefetch.name = "PREFETCH";
			} else {
				memcpy(&pcie->mem, &res, sizeof(res));
				pcie->mem.name = "MEM";
			}
			break;
		}
	}

	err = of_pci_parse_bus_range(np, &pcie->busn);
	if (err < 0) {
		dev_err(pcie->dev, "failed to parse ranges property: %d\n",
			err);
		pcie->busn.name = np->name;
		pcie->busn.start = 0;
		pcie->busn.end = 0xff;
		pcie->busn.flags = IORESOURCE_BUS;
	}

	return 0;
}

static int st_pcie_enable(struct st_pcie *pcie)
{
	struct hw_pci hw;

	memset(&hw, 0, sizeof(hw));

	hw.nr_controllers = 1;
	hw.private_data = (void **)&pcie;
	hw.setup = st_pcie_setup;
	hw.map_irq = st_pcie_map_irq;
	hw.scan = st_pcie_scan;
	hw.ops = &st_pcie_config_ops;

	pci_common_init_dev(pcie->dev, &hw);

	return 0;
}

static int st_pcie_init(struct st_pcie *pcie)
{
	int ret;

	ret = reset_control_deassert(pcie->pwr);
	if (ret) {
		dev_err(pcie->dev, "unable to bring out of powerdown\n");
		return ret;
	}

	ret = reset_control_deassert(pcie->rst);
	if (ret) {
		dev_err(pcie->dev, "unable to bring out of softreset\n");
		return ret;
	}

	/* Set device type : Root Complex */
	ret = regmap_write(pcie->regmap, pcie->syscfg0, PCIE_DEVICE_TYPE);
	if (ret < 0) {
		dev_err(pcie->dev, "unable to set device type\n");
		return ret;
	}

	usleep_range(1000, 2000);
	return ret;
}

/* STiH407 */
static int stih407_pcie_enable_ltssm(struct st_pcie *pcie)
{
	if (!pcie->syscfg1)
		return -EINVAL;

	return regmap_update_bits(pcie->regmap, pcie->syscfg1,
			PCIE_APP_LTSSM_ENABLE,	PCIE_APP_LTSSM_ENABLE);
}

static int stih407_pcie_disable_ltssm(struct st_pcie *pcie)
{
	if (!pcie->syscfg1)
		return -EINVAL;

	return regmap_update_bits(pcie->regmap, pcie->syscfg1,
			PCIE_APP_LTSSM_ENABLE, 0);
}

static struct st_pcie_ops stih407_pcie_ops = {
	.init = st_pcie_init,
	.enable_ltssm = stih407_pcie_enable_ltssm,
	.disable_ltssm = stih407_pcie_disable_ltssm,
};

static struct st_pcie_ops stih407_pcie_ops_sec = {
	.init = st_pcie_init,
	.enable_ltssm = stih407_pcie_enable_ltssm,
	.disable_ltssm = stih407_pcie_disable_ltssm,
	.secure_mode = true,
};

static int st_pcie_enable_ltssm(struct st_pcie *pcie)
{
	return regmap_write(pcie->regmap, pcie->syscfg0,
			PCIE_DEFAULT_VAL | PCIE_APP_LTSSM_ENABLE);
}

static int st_pcie_disable_ltssm(struct st_pcie *pcie)
{
	return regmap_write(pcie->regmap, pcie->syscfg0, PCIE_DEFAULT_VAL);
}

static struct st_pcie_ops st_pcie_ops = {
	.init = st_pcie_init,
	.enable_ltssm = st_pcie_enable_ltssm,
	.disable_ltssm = st_pcie_disable_ltssm,
};

static struct st_pcie_ops stid127_pcie_ops = {
	.init = st_pcie_init,
	.enable_ltssm = st_pcie_enable_ltssm,
	.disable_ltssm = st_pcie_disable_ltssm,
	.phy_auto = true,
};

static const struct of_device_id st_pcie_of_match[] = {
	{ .compatible = "st,stih407-pcie", .data = (void *)&stih407_pcie_ops},
	{ .compatible = "st,stih407-pcie-sec", .data = (void *)&stih407_pcie_ops_sec},
	{ .compatible = "st,stih416-pcie", .data = (void *)&st_pcie_ops},
	{ .compatible = "st,stid127-pcie", .data = (void *)&stid127_pcie_ops},
	{ },
};

#ifdef CONFIG_PM
static int st_pcie_suspend(struct device *pcie_dev)
{
	struct st_pcie *pcie = dev_get_drvdata(pcie_dev);
	struct device_node *np_gpio = NULL;
	int gpio_pin,ret;

	/* To guarantee a real phy initialization on resume */
	if (!pcie->data->phy_auto)
		phy_exit(pcie->phy);

    
	/* DTI744CNL:  cut off wifi-power-en gpio in CPS mode */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform suspend\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {
		/* Enable wifi power */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-power-en", 0);
		ret = gpio_request(gpio_pin, "wifi-power-en");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            pr_info("shutdown wifi-power-en low\n");
			gpio_direction_output(gpio_pin, 0);
			gpio_free(gpio_pin);
		}
	}
	/* end DTI744CNL:  cut off wifi-power-en */


	return 0;
}

static int st_pcie_resume(struct device *pcie_dev)
{
	struct st_pcie *pcie = dev_get_drvdata(pcie_dev);
	struct device_node *np_gpio = NULL;
	int gpio_pin,ret;

	/* DTI744CNL: power on wifi */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform resume\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {
		/* Cut wifi power off */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-power-en", 0);
		ret = gpio_request(gpio_pin, "wifi-power-en");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            pr_info("resume wifi-power-en low\n");
			gpio_direction_output(gpio_pin, 0);
			gpio_free(gpio_pin);
		}

		/* Pull wifi reset off */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-reset", 0);
		ret = gpio_request(gpio_pin, "wifi-reset");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            pr_info("resume wifi-reset low\n");
			gpio_direction_output(gpio_pin, 0);
			gpio_free(gpio_pin);
		}

		/* Pull wifi power on */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-power-en", 0);
		ret = gpio_request(gpio_pin, "wifi-power-en");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            /* Maintain wifi power off at least 200ms */
            mdelay(200);
            pr_info("resume wifi-power-en high\n");
			gpio_direction_output(gpio_pin, 1);
			gpio_free(gpio_pin);
		}

	}
	/* end DTI744CNL:  cut off wifi-power-en */

	ret = pcie->data->init(pcie);
	if (ret) {
		pr_err("%s: Error on pcie init\n", __func__);
		return ret;
	}

	ret = pcie->data->disable_ltssm(pcie);
	if (ret) {
		pr_err("%s: Error on pcie disable_ltssm\n", __func__);
		return ret;
	}

	if (!pcie->data->phy_auto)
		phy_init(pcie->phy);

	st_pcie_hw_setup(pcie,
		pcie->lmi->start,
		resource_size(pcie->lmi),
		pcie->config_window_start);

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		/* MSI configuration */
		st_msi_init_one(pcie->msi);
	}

	/* DTI744CNL: pull wifi reset on */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform resume\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {

		/* Pull wifi reset on */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-reset", 0);
		ret = gpio_request(gpio_pin, "wifi-reset");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            /* wait 10ms for the start of pcie clk */
            mdelay(10);
            pr_info("resume wifi-reset high\n");
			gpio_direction_output(gpio_pin, 1);
			gpio_free(gpio_pin);
		}

	}
	/* end DTI744CNL:  pull wifi reset on */

	return 0;
}

const struct dev_pm_ops st_pcie_pm_ops = {
	.suspend_noirq = st_pcie_suspend,
	.resume_noirq = st_pcie_resume,
};
#define ST_PCIE_PM	(&st_pcie_pm_ops)
#else
#define ST_PCIE_PM	NULL
#endif

/**
 * st_pcie_shutdown
 * @pdev: platform device pointer
 * Description: this function is just to program the PMT block when
 * shutdown is invoked.
 */
static void st_pcie_shutdown(struct platform_device *pdev)
{
	struct device_node *np_gpio = NULL;
	int gpio_pin, ret;

	/* DTI744CNL:  cut off wifi-power-en gpio in DCPS mode */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform configuration\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {
		/* Enable wifi power */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-power-en", 0);
		ret = gpio_request(gpio_pin, "wifi-power-en");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
            pr_info("shutdown wifi-power-en low\n");
			gpio_direction_output(gpio_pin, 0);
			gpio_free(gpio_pin);
		}
	}
	/* end DTI744CNL:  cut off wifi-power-en */

}

static int __init st_pcie_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct st_pcie *pcie;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_gpio = NULL;
	int err, serr_irq;
	int gpio_pin, ret;

	/* DTI744CNL:  cut off wifi-power-en */
	if (of_machine_is_compatible("st,custom001303"))
	{
		pr_info("%s: specific platform configuration\n", __FUNCTION__);

		np_gpio = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
	}

	if (np_gpio != NULL) {
		/* Enable wifi power */
		gpio_pin = of_get_named_gpio(np_gpio, "wifi-power-en", 0);
		ret = gpio_request(gpio_pin, "wifi-power-en");
		if (ret != 0) {
			of_node_put(np_gpio);
		}
		else {
			/* Maintain wifi power off at least 200ms */
			mdelay(200);
			gpio_direction_output(gpio_pin, 1);
			gpio_free(gpio_pin);
		}
	}
	/* end DTI744CNL:  cut off wifi-power-en */
	
	pcie = devm_kzalloc(&pdev->dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	memset(pcie, 0, sizeof(*pcie));

	pcie->dev = &pdev->dev;

	spin_lock_init(&pcie->abort_lock);

	err = st_pcie_parse_dt(pcie);
	if (err < 0)
		return err;

	pcie->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(pcie->regmap)) {
		dev_err(pcie->dev, "No syscfg phandle specified\n");
		return PTR_ERR(pcie->regmap);
	}

	err = remap_named_resource(pdev, "pcie cntrl", &pcie->cntrl);
	if (err)
		return err;

	err = remap_named_resource(pdev, "pcie ahb", &pcie->ahb);
	if (!err) {
		if (of_property_read_u32(np, "st,ahb-fixup", &pcie->ahb_val)) {
			dev_err(pcie->dev, "missing ahb-fixup property\n");
			return -EINVAL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie cs");
	if (!res)
		return -ENXIO;

	/* Check that this has sensible values */
	if ((resource_size(res) != CFG_REGION_SIZE) ||
	    (res->start & (CFG_REGION_SIZE - 1))) {
		dev_err(pcie->dev, "Invalid config space properties\n");
		return -EINVAL;
	}
	pcie->config_window_start = res->start;

	err = remap_named_resource(pdev, "pcie cs", &pcie->config_area);
	if (err)
		return err;

	pcie->lmi = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"mem-window");
	if (!pcie->lmi)
		return -ENXIO;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "syscfg0");
	if (!res)
		return -ENXIO;
	pcie->syscfg0 = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "syscfg1");
	if (res)
		pcie->syscfg1 = res->start;

	/* request pcie syserr interrupt */
	serr_irq = platform_get_irq_byname(pdev, "pcie syserr");
	if (serr_irq < 0) {
		dev_err(&pdev->dev, "failed to get syserr IRQ: %d\n",
				serr_irq);
		return serr_irq;
	}

	err = devm_request_irq(&pdev->dev, serr_irq, st_pcie_sys_err,
			      IRQF_DISABLED, "pcie syserr", pcie);
	if (err) {
		dev_err(&pdev->dev, "failed to register syserr IRQ: %d\n",
				err);
		return err;
	}

	/* At least one pcie interrupt */
	err = platform_get_irq_byname(pdev, "pcie inta");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get inta IRQ: %d\n", err);
		return err;
	}

	pcie->irq[pcie->irq_lines++] = err;

	err = platform_get_irq_byname(pdev, "pcie intb");
	if (err > 0)
		pcie->irq[pcie->irq_lines++] = err;

	err = platform_get_irq_byname(pdev, "pcie intc");
	if (err > 0)
		pcie->irq[pcie->irq_lines++] = err;

	err = platform_get_irq_byname(pdev, "pcie intd");
	if (err > 0)
		pcie->irq[pcie->irq_lines++] = err;

	pcie->data = of_match_node(st_pcie_of_match, np)->data;
	if (!pcie->data) {
		dev_err(&pdev->dev, "compatible data not provided\n");
		return -EINVAL;
	}

	pcie->pwr = devm_reset_control_get(&pdev->dev, "power");
	if (IS_ERR(pcie->pwr)) {
		dev_err(&pdev->dev, "power reset control not defined\n");
		return -EINVAL;
	}

	pcie->rst = devm_reset_control_get(&pdev->dev, "softreset");
	if (IS_ERR(pcie->rst)) {
		dev_err(&pdev->dev, "Soft reset control not defined\n");
		return -EINVAL;
	}

	/*
	 * We have to initialise the PCIe cell on some hardware before we can
	 * talk to the phy
	 */
	err = pcie->data->init(pcie);
	if (err)
		return err;

	err = pcie->data->disable_ltssm(pcie);
	if (err) {
		dev_err(&pdev->dev, "failed to disable ltssm\n");
		return err;
	}

	if (!pcie->data->phy_auto) {

		/* Now claim the associated miphy */
		pcie->phy = devm_phy_get(&pdev->dev, "pcie_phy");
		if (IS_ERR(pcie->phy)) {
			dev_err(&pdev->dev, "no PHY configured\n");
			return PTR_ERR(pcie->phy);
		}

		err = phy_init(pcie->phy);
		if (err < 0) {
			dev_err(&pdev->dev, "Cannot init PHY: %d\n", err);
			return err;
		}

	}

	/* Claim the GPIO for PRST# if available */
	pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (!gpio_is_valid(pcie->reset_gpio))
		dev_dbg(&pdev->dev, "No reset-gpio configured\n");
	else {
		err = devm_gpio_request(&pdev->dev,
				pcie->reset_gpio,
				"PCIe reset");
		if (err) {
			dev_err(&pdev->dev, "Cannot request reset-gpio %d\n",
				pcie->reset_gpio);
			return err;
		}
	}

	/* Now do all the register poking */
	err = st_pcie_hw_setup(pcie, pcie->lmi->start,
				resource_size(pcie->lmi),
				pcie->config_window_start);
	if (err) {
		dev_err(&pdev->dev, "Error while doing hw setup: %d\n", err);
		goto disable_phy;
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		/* MSI configuration */
		err = st_pcie_enable_msi(pcie);
		if (err)
			goto disable_link;
	}

	if (IS_ENABLED(CONFIG_ARM)) {
		/*
		 * We have to hook the abort handler so that we can intercept
		 * bus errors when doing config read/write that return UR,
		 * which is flagged up as a bus error
		 */
		hook_fault_code(16+6, st_pcie_abort, SIGBUS, 0,
			"imprecise external abort");
	}

	/* And now hook this into the generic driver */
	err = st_pcie_enable(pcie);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable PCIe ports: %d\n", err);
		goto disable_msi;
	}

	platform_set_drvdata(pdev, pcie);

	dev_info(&pdev->dev, "Initialized\n");
	return 0;

disable_msi:
	if (IS_ENABLED(CONFIG_PCI_MSI))
		st_pcie_disable_msi(pcie);
disable_link:
	pcie->data->disable_ltssm(pcie);
disable_phy:
	if (!pcie->data->phy_auto)
		phy_exit(pcie->phy);

	return err;
}

MODULE_DEVICE_TABLE(of, st_pcie_of_match);

static struct platform_driver st_pcie_driver = {
	.shutdown = st_pcie_shutdown,
	.driver = {
		.name = "st-pcie",
		.owner = THIS_MODULE,
		.of_match_table = st_pcie_of_match,
		.pm = ST_PCIE_PM,
	},
};

/* ST PCIe driver does not allow module unload */
static int __init pcie_init(void)
{
	return platform_driver_probe(&st_pcie_driver, st_pcie_probe);
}
device_initcall(pcie_init);

MODULE_AUTHOR("Fabrice Gasnier <fabrice.gasnier@st.com>");
MODULE_DESCRIPTION("PCI express Driver for ST SoCs");
MODULE_LICENSE("GPLv2");
