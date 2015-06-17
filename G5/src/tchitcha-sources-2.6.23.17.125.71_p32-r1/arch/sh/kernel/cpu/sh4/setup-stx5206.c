/*
 * STx5206/STx5289/STx5297 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/pata_platform.h>
#include <linux/phy.h>
#include <linux/serial.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/clk.h>
#include <linux/stm/emi.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/sysconf.h>
#include <asm/irl.h>
#include <asm/irq-ilc.h>



static void __init stx5206_pio_sysconf(int port, int pin, int alt,
		const char *name)
{
	struct sysconf_field *sc;
	int reg, bit;

	BUG_ON(port < 0);
	BUG_ON(port > 3);
	BUG_ON(pin < 0);
	BUG_ON(pin > 7);

	if (port > 0) {
		port--;

		reg = 16 + (port / 2);
		bit = 16 * (port % 2) + pin;

		sc = sysconf_claim(SYS_CFG, reg, bit, bit, name);
		sysconf_write(sc, (alt - 1) & 1);

		sc = sysconf_claim(SYS_CFG, reg, bit + 8, bit + 8, name);
		sysconf_write(sc, ((alt - 1) >> 1) & 1);
	}
}



/* USB resources ---------------------------------------------------------- */

static u64 st40_dma_mask = DMA_32BIT_MASK;

#define UHOST2C_BASE			0xfe100000
#define AHB2STBUS_WRAPPER_GLUE_BASE	UHOST2C_BASE
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static struct platform_device stx5206_usb_device = USB_DEVICE(-1,
		AHB2STBUS_EHCI_BASE, evt2irq(0x1720),
		AHB2STBUS_OHCI_BASE, evt2irq(0x1700),
		AHB2STBUS_WRAPPER_GLUE_BASE, AHB2STBUS_PROTOCOL_BASE,
		USB_FLAGS_STRAP_8BIT |
		USB_FLAGS_STBUS_CONFIG_THRESHOLD_128 |
		USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
		USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32);

void __init stx5206_configure_usb(void)
{
	struct sysconf_field *sc;

	/* USB_HOST_SOFT_RESET: active low usb host sof reset */
	sc = sysconf_claim(SYS_CFG, 4, 1, 1, "USB");
	sysconf_write(sc, 1);

#ifndef CONFIG_PM
	/* suspend_from_config: Signal to suspend USB PHY */
	sc = sysconf_claim(SYS_CFG, 10, 5, 5, "USB");
	sysconf_write(sc, 0);

	/* usb_power_down_req: power down request for USB Host module */
	sc = sysconf_claim(SYS_CFG, 32, 4, 4, "USB");
	sysconf_write(sc, 0);
#endif

	platform_device_register(&stx5206_usb_device);

}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw stx5206_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.periph_regs = {
		.sync_reg = 0x8000 + (0xfe2 << 2), /* 0xbf88 */
		.cmd_sta  = 0x8000 + (0xff0 << 2), /* 0xbfc0 */
		.cmd_set  = 0x8000 + (0xff1 << 2), /* 0xbfc4 */
		.cmd_clr  = 0x8000 + (0xff2 << 2), /* 0xbfc8 */
		.cmd_mask = 0x8000 + (0xff3 << 2), /* 0xbfcc */
		.int_sta  = 0x8000 + (0xff4 << 2), /* 0xbfd0 */
		.int_set  = 0x8000 + (0xff5 << 2), /* 0xbfd4 */
		.int_clr  = 0x8000 + (0xff6 << 2), /* 0xbfd8 */
		.int_mask = 0x8000 + (0xff7 << 2), /* 0xbfdc */
	},
	.dmem_offset = 0x8000,
	.dmem_size   = 0x800 << 2, /* 2048 * 4 = 8192 */
	.imem_offset = 0xc000,
	.imem_size   = 0x1000 << 2, /* 4096 * 4 = 16384 */
};

static struct stm_plat_fdma_data stx5206_fdma_platform_data = {
	.hw = &stx5206_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx5206_fdma_platform_data_addr &stx5206_fdma_platform_data

#else

#define stx5206_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx5206_fdma_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		{
			.start = 0xfe220000,
			.end   = 0xfe22ffff,
			.flags = IORESOURCE_MEM,
		}, {
			.start = evt2irq(0x1380),
			.end   = evt2irq(0x1380),
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev.platform_data = stx5206_fdma_platform_data_addr,
};



/* SSC resources ---------------------------------------------------------- */
static const char stx5206_i2c_stm_ssc[] = "i2c_stm_ssc";
static const char stx5206_i2c_stm_pio[] = "i2c_stm_pio";
static char stx5206_spi_dev_name[] = "spi_st_ssc";

static struct platform_device stx5206_ssc_devices[] = {
	STSSC_DEVICE(0xfd040000, evt2irq(0x10e0), 0xff, 0xff, 0xff, 0xff),
	STSSC_DEVICE(0xfd041000, evt2irq(0x10c0), 2, 0, 1, 2),
	STSSC_DEVICE(0xfd042000, evt2irq(0x10a0), 3, 4, 5, 0xff),
	STSSC_DEVICE(0xfd043000, evt2irq(0x1080), 3, 6, 7, 0xff),
};

void __init stx5206_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int capability = data->capability;
	int i;

	for (i = 0; i < ARRAY_SIZE(stx5206_ssc_devices);
			i++, capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc_pio =
				stx5206_ssc_devices[i].dev.platform_data;
		int pin;

		if (capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc_pio->clk_unidir = 1;

		for (pin = 0; pin < (capability & SSC_SPI_CAPABILITY ? 3 : 2);
			       pin++) {
			int portno = ssc_pio->pio[pin].pio_port;
			int pinno  = ssc_pio->pio[pin].pio_pin;

			if (portno == 0xff || pinno == 0xff)
				continue;

			stx5206_pio_sysconf(portno, pinno, 0, "ssc");
		}

		if (capability & SSC_SPI_CAPABILITY) {
			stx5206_ssc_devices[i].name = stx5206_spi_dev_name;
			stx5206_ssc_devices[i].id = num_spi++;
			ssc_pio->chipselect = data->spi_chipselects[i];
		} else {
			stx5206_ssc_devices[i].name =
					((capability & SSC_I2C_PIO) ?
					stx5206_i2c_stm_pio :
					stx5206_i2c_stm_ssc);
			stx5206_ssc_devices[i].id = num_i2c++;
#ifdef CONFIG_I2C_BOARDINFO
			/* I2C buses number reservation (to prevent any
			 * hot-plug device from using it) */
			i2c_register_board_info(num_i2c - 1, NULL, 0);
#endif
		}

		platform_device_register(&stx5206_ssc_devices[i]);
	}
}



/* PATA resources ---------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */

static struct resource stx5206_pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start	= (1 << 20),
		.end	= (1 << 20) + (8 << 17) - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start	= (1 << 21) + (6 << 17),
		.end	= (1 << 21) + (6 << 17) + 3,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {	/* IRQ */
		.flags	= IORESOURCE_IRQ,
	}
};

static struct pata_platform_info stx5206_pata_info = {
	.ioport_shift	= 17,
};

static struct platform_device stx5206_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx5206_pata_resources),
	.resource	= stx5206_pata_resources,
	.dev.platform_data = &stx5206_pata_info,
};

void __init stx5206_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base;

	bank_base = emi_bank_base(bank);
	stx5206_pata_resources[0].start += bank_base;
	stx5206_pata_resources[0].end   += bank_base;
	stx5206_pata_resources[1].start += bank_base;
	stx5206_pata_resources[1].end   += bank_base;
	stx5206_pata_resources[2].start = irq;
	stx5206_pata_resources[2].end   = irq;

	emi_config_pata(bank, pc_mode);

	platform_device_register(&stx5206_pata_device);
}

/* MMC/SD resources ------------------------------------------------------ */

struct stx5206_mmc_pin{
	struct {
		unsigned char port, pin, alt;
	} pio;
	unsigned char dir;
};

static struct stx5206_mmc_pin stx5206_mmc_pins[] __initdata = {
	{ { 3, 0, 1 }, STPIO_IN }, /* Card Detect */
	{ { 3, 1, 1 }, STPIO_IN }, /* MMC Write Protection */
	{ { 3, 2, 1 }, STPIO_ALT_OUT }, /* MMC LED On */
	{ { 3, 3, 1 }, STPIO_ALT_OUT }, /* MMC Power On */
};

static struct platform_device stx5206_mmc_device = {
		.name = "arasan",
		.id = 0,
		.num_resources = 2,
		.resource       = (struct resource[]) {
		{
			.start = 0xFD106000,
			.end   = 0xFD10FFFF,
			.flags  = IORESOURCE_MEM,
		}, {
			.name   = "mmcirq",
			.start  = evt2irq(0xa80),
			.end    = evt2irq(0xa80),
			.flags  = IORESOURCE_IRQ,
		}
	}
};

void __init stx5289_configure_mmc(void)
{
	struct stx5206_mmc_pin *pins;
	int pins_num, i;
	struct sysconf_field *sc;

	sc = sysconf_claim(SYS_CFG, 18, 19, 19, "mmc");
	sysconf_write(sc, 1);

	pins = stx5206_mmc_pins;
	pins_num = ARRAY_SIZE(stx5206_mmc_pins);

	for (i = 0; i < pins_num; i++) {
		struct stx5206_mmc_pin *pin = &pins[i];

		stpio_request_pin(pin->pio.port, pin->pio.pin, "mmc", pin->dir);
		if (pin->dir != STPIO_IN)
			stx5206_pio_sysconf(pin->pio.port, pin->pio.pin,
					    pin->pio.alt, "mmc");
	}

	platform_device_register(&stx5206_mmc_device);
}

/* Ethernet MAC resources ------------------------------------------------- */

static void stx5206_gmac_fix_mac_speed(void *priv, unsigned int speed)
{
	struct sysconf_field *sc = priv;

	sysconf_write(sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx5206_gmac_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = stx5206_gmac_fix_mac_speed,
};

static struct platform_device stx5206_gmac_device = {
	.name           = "stmmaceth",
	.id             = -1,
	.num_resources  = 2,
	.resource       = (struct resource[]) {
		{
			.start = 0xfd110000,
			.end   = 0xfd117fff,
			.flags  = IORESOURCE_MEM,
		}, {
			.name   = "macirq",
			.start  = evt2irq(0x12a0),
			.end    = evt2irq(0x12a0),
			.flags  = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stx5206_gmac_private_data,
	}
};

void __init stx5206_configure_ethernet(enum stx5206_ethernet_mode mode,
		int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;
	unsigned int phy_intf_sel, enmii;
	unsigned long phy_clk_rate;

	switch (mode) {
	case stx5206_ethernet_mii:
		phy_intf_sel = 0;
		enmii = 1;
		phy_clk_rate = 25000000;
		break;
	case stx5206_ethernet_rmii:
		phy_intf_sel = 0x4;
		enmii = 1;
		phy_clk_rate = 50000000;
		break;
	case stx5206_ethernet_reverse_mii:
		phy_intf_sel = 0;
		enmii = 0;
		phy_clk_rate = 25000000;
		break;
	default:
		BUG();
		return;
	}

	/* ethernet_interface_on */
	sc = sysconf_claim(SYS_CFG, 7, 16, 16, "stmmac");
	sysconf_write(sc, 1);

	/* phy_clk_ext: MII_PHYCLK pad function: 1 = phy clock is external,
	 * 0 = phy clock is provided by STx5289 */
	sc = sysconf_claim(SYS_CFG, 7, 19, 19, "stmmac");
	sysconf_write(sc, ext_clk ? 1 : 0);

	/* mac_speed_sel: 1 = 100Mbps, 0 = 10Mbps (RMII only) */
	stx5206_gmac_private_data.bsp_priv = sysconf_claim(SYS_CFG, 7, 20, 20,
			"stmmac");

	/* phy_intf_sel */
	sc = sysconf_claim(SYS_CFG, 7, 24, 26, "stmmac");
	sysconf_write(sc, phy_intf_sel);

	/* enMii: 1 = MII mode, 0 = Reverse MII mode */
	sc = sysconf_claim(SYS_CFG, 7, 27, 27, "stmmac");
	sysconf_write(sc, enmii);

	/* Set PHY clock frequency (if used) */
	if (!ext_clk) {
		struct clk *phy_clk = clk_get(NULL, "CLKA_ETH_PHY");

		BUG_ON(!phy_clk);
		clk_set_rate(phy_clk, phy_clk_rate);
	}

	stx5206_gmac_private_data.bus_id = phy_bus;

	platform_device_register(&stx5206_gmac_device);
}



/* PWM resources ---------------------------------------------------------- */

static struct platform_device stx5206_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource []) {
		{
			.start	= 0xfd010000,
			.end	= 0xfd010000 + 0x67,
			.flags	= IORESOURCE_MEM
		}, {
			.start	= evt2irq(0x11c0),
			.end	= evt2irq(0x11c0),
			.flags	= IORESOURCE_IRQ
		}
	},
};

void __init stx5206_configure_pwm(struct plat_stm_pwm_data *data)
{
	stx5206_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0)
		stx5206_pio_sysconf(3, 0, 1, "PWM");

	/* OUT1 is not available */
	WARN_ON(data->flags & PLAT_STM_PWM_OUT1);

	platform_device_register(&stx5206_pwm_device);
}



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx5206_rng_dev_hwrandom_device = {
	.name		= "stm_hwrandom",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

static struct platform_device stx5206_rng_dev_random_device = {
	.name		= "stm_rng",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};



/* ASC resources ---------------------------------------------------------- */

static struct platform_device stx5206_asc_devices[] = {
	STASC_DEVICE(0xfd030000, evt2irq(0x1160), 9, 13, 0, 0, 1, 4, 7,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd031000, evt2irq(0x1140), 10, 14, 3, 0xff, 4, 6, 5,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd032000, evt2irq(0x1120), 11, 15, 1, 2, 1, 4, 3,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd033000, evt2irq(0x1100), 12, 16, 2, 4, 3, 5, 6,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[
		ARRAY_SIZE(stx5206_asc_devices)] __initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stx5206_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i = 0; i < num_ascs; i++) {
		int asc, alt;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *plat_data;
		int j;

		asc = ascs[i] & 0xff;
		flags = ascs[i] >> 8;

		pdev = &stx5206_asc_devices[asc];
		plat_data = pdev->dev.platform_data;

		if (asc == 1) {
			/* ASC1 is routed in other way than others... */
			plat_data->pios[0].pio_port = 2;
			plat_data->pios[0].pio_pin = 7;
			alt = 2;
		} else {
			alt = 1;
		}

		for (j = 0; j < (flags & STASC_FLAG_NORTSCTS ? 2 : 4); j++)
			stx5206_pio_sysconf(plat_data->pios[j].pio_port,
					plat_data->pios[j].pio_pin, alt, "asc");

		pdev->id = i;
		plat_data->flags = flags | STASC_FLAG_TXFIFO_BUG;

		stasc_configured_devices[stasc_configured_devices_count++] =
				pdev;
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init stx5206_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx5206_add_asc);



/* LiRC resources --------------------------------------------------------- */
static struct lirc_pio stx5206_lirc_pios[] = {
	[0] = {
		.bank = 1,
		.pin  = 6,
		.dir  = STPIO_ALT_OUT,
	},
	[1] = {
		.bank = 1,
		.pin  = 7,
		.dir  = STPIO_IN,
	},
	[2] = {
		.bank = 3,
		.pin  = 3,
		.dir  = STPIO_ALT_OUT,
	},
};

static struct plat_lirc_data stx5206_lirc_private_info = {
	/* The clock settings will be calculated by the driver
	 * from the system clock */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv      = 0, /* automatically calculate */
	.irbperiodmult  = 0,
	.irbperioddiv   = 0,
	.irbontimemult  = 0,
	.irbontimediv   = 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = stx5206_lirc_pios,
	.num_pio_pins = ARRAY_SIZE(stx5206_lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 1000000,
#endif
};

static struct platform_device stx5206_lirc_device = {
	.name           = "lirc",
	.id             = -1,
	.num_resources  = 2,
	.resource       = (struct resource []) {
		{
			.start = 0xfd018000,
			.end   = 0xfd018000 + 0xa0,
			.flags = IORESOURCE_MEM
		}, {
			.start = evt2irq(0x11a0),
			.end   = evt2irq(0x11a0),
			.flags = IORESOURCE_IRQ
		},
	},
	.dev = {
		   .power.can_wakeup = 1,
		   .platform_data = &stx5206_lirc_private_info
	}
};

void __init stx5206_configure_lirc(enum stx5206_lirc_rx_mode rx_mode,
		int tx_enabled, int tx_od_enabled, lirc_scd_t *scd)
{
	stx5206_lirc_private_info.scd_info = scd;

	switch (rx_mode) {
	case stx5206_lirc_rx_disabled:
		/* Do nothing */
		break;
	case stx5206_lirc_rx_ir:
		stx5206_lirc_pios[1].pinof = LIRC_PIO_ON | LIRC_IR_RX;
		stx5206_pio_sysconf(1, 7, 1, "lirc");
		break;
	case stx5206_lirc_rx_uhf:
		stx5206_lirc_pios[1].pinof = LIRC_PIO_ON | LIRC_IR_RX;
		stx5206_pio_sysconf(1, 7, 2, "lirc");
		break;
	default:
		BUG();
		return;
	}

	if (tx_enabled) {
		stx5206_lirc_pios[2].pinof = LIRC_PIO_ON | LIRC_IR_TX;
		stx5206_pio_sysconf(3, 3, 1, "lirc");
	}

	if (tx_od_enabled) {
		stx5206_lirc_pios[0].pinof = LIRC_PIO_ON | LIRC_IR_TX;
		stx5206_pio_sysconf(1, 6, 2, "lirc");
	}

	platform_device_register(&stx5206_lirc_device);
}



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int stx5206_pcibios_map_platform_irq( struct pci_config_data *pci_config,
		u8 pin)
{
	int result;
	int dedicated_irqs[] = {
		evt2irq(0xa00), /* PCI_INT_FROM_DEVICE[0] */
		evt2irq(0xa20), /* PCI_INT_FROM_DEVICE[1] */
		evt2irq(0xa40), /* PCI_INT_FROM_DEVICE[2] */
		evt2irq(0xa60), /* PCI_INT_FROM_DEVICE[3] */
	};
	int type;

	if (pin < 1 || pin > 4)
		return -1;

	type = pci_config->pci_irq[pin - 1];

	switch (type) {
	case PCI_PIN_ALTERNATIVE:
		/* Actually, there are no alternative pins */
		BUG();
		result = -1;
		break;
	case PCI_PIN_DEFAULT:
		result = dedicated_irqs[pin - 1];
		break;
	case PCI_PIN_UNUSED:
		result = -1; /* Not used */
		break;
	default:
		/* Take whatever interrupt you are told */
		result = type;
		break;
	}

	return result;
}

static struct platform_device stx5206_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		{
			.name = "Memory",
#ifdef CONFIG_32BIT
			.start = 0xc0000000,
			.end = 0xdfffffff, /* 512 MB */
#else
			.start = 0x08000000,
			.end = 0x0bffffff, /* 64 MB */
#endif
			.flags = IORESOURCE_MEM,
		}, {
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		}, {
			.name = "EMISS",
			.start = 0xfe400000,
			.end = 0xfe4017fc,
			.flags = IORESOURCE_MEM,
		}, {
			.name = "PCI-AHB",
			.start = 0xfe560000,
			.end = 0xfe5600ff,
			.flags = IORESOURCE_MEM,
		}, {
			.name = "DMA",
			.start = evt2irq(0x1280),
			.end = evt2irq(0x1280),
			.flags = IORESOURCE_IRQ,
		}, {
			.name = "Error",
			.start = evt2irq(0x1200),
			.end = evt2irq(0x1200),
			.flags = IORESOURCE_IRQ,
		}, { /* Keep this one last */
			.name = "SERR",
			/* .start & .end set in stx5206_configure_pci() */
			.flags = IORESOURCE_IRQ,
		}
	},
};
void __init stx5206_configure_pci(struct pci_config_data *pci_conf)
{
	struct sysconf_field *sc;

	/* Fill in the default values */
	if (!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* The EMI_BUSREQ[0] pin (also know just as EmiBusReq) is
	 * internally wired to the arbiter's PCI request 3 line.
	 * And the answer to the obvious question is: That's right,
	 * the EMI_BUSREQ[3] is not wired at all... */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Copy over platform specific data to driver */
	stx5206_pci_device.dev.platform_data = pci_conf;

	/* pci_master_not_slave: Controls PCI padlogic.
	 * 1: NO DLY cells in EMI Datain/Out paths to/from pads.
	 * 0: DLY cells in EMI Datain/Out paths to/from pads */
	sc = sysconf_claim(SYS_CFG, 4, 11, 11, "PCI");
	sysconf_write(sc, 1);

	/* cfg_pci_enable: PCI select
	 * 1: PCI functionality and pad type enabled on all the shared pads
	 *    for PCI. */
	sc = sysconf_claim(SYS_CFG, 10, 30, 30, "PCI");
	sysconf_write(sc, 1);

	/* cfg_pci_int_1_from_tsout7_or_from_spi_hold: PCI_INT_FROM_DEVICE[1]
	 *    select
	 * 0: PCI_INT_FROM_DEVICE[1] is mapped on TSOUTDATA[7]
	 * 1: PCI_INT_FROM_DEVICE[1] is mapped on SPI_HOLD */
	sc = sysconf_claim(SYS_CFG, 10, 29, 29, "PCI");
	switch (pci_conf->pci_irq[1]) {
	case PCI_PIN_DEFAULT:
		sysconf_write(sc, 0); /* TSOUTDATA[7] */
		break;
	case PCI_PIN_ALTERNATIVE:
		sysconf_write(sc, 1); /* SPI_HOLD */
		break;
	default:
		/* User provided interrupt, do nothing */
		break;
	}

	/* pci_int_from_device_pci_int_to_host: Used to control the pad
	 *    direction.
	 * 0: PCI being device/sattelite (pci_int_to_host)
	 * 1: PCI being host (pci_int_from_device[0]). */
	sc = sysconf_claim(SYS_CFG, 10, 23, 23, "PCI");
	sysconf_write(sc, 1);

	/* emi_pads_mode: defines mode for emi/pci shared pads
	 * 0: bidir TTL
	 * 1: PCI */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* Configure the SERR interrupt (there is no pin dedicated
	 * as input here, have to use PIO or external interrupt) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
	case PCI_PIN_ALTERNATIVE:
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		stx5206_pci_device.num_resources--;
	} else {
		/* The SERR IRQ resource is last */
		int res_num = stx5206_pci_device.num_resources - 1;
		struct resource *res = &stx5206_pci_device.resource[res_num];

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

#if defined(CONFIG_PM)
#warning TODO: PCI Power Managament
#endif
	/* conf_pci_pme_out: Used as enable for SPI_WRITE_PROTECT_PAD when
	 *    used for PCI.
	 * 1: Pad Input
	 * 0: Pad Output. */
	sc = sysconf_claim(SYS_CFG, 31, 22, 22, "PCI");
	sysconf_write(sc, 1);

	/* pci_power_down_req: power down request for PCI module */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI");
	sysconf_write(sc, 0);

	/* power_down_ack_pci: PCI power down acknowledge */
	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI");
	while (sysconf_read(sc))
		cpu_relax();

	platform_device_register(&stx5206_pci_device);
}



/* Other resources -------------------------------------------------------- */

static struct platform_device stx5206_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd000000,
			.end	= 0xfd000000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

static struct platform_device stx5206_sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe001000,
			.end	= 0xfe001000 + 0x1df,
			.flags	= IORESOURCE_MEM
		}
	},
	.dev.platform_data = &(struct plat_sysconf_data) {
		.groups_num = 3,
		.groups = (struct plat_sysconf_group []) {
			PLAT_SYSCONF_GROUP(SYS_DEV, 0x000),
			PLAT_SYSCONF_GROUP(SYS_STA, 0x008),
			PLAT_SYSCONF_GROUP(SYS_CFG, 0x100),
		},
	}
};

static struct platform_device stx5206_pio_devices[] = {
	STPIO_DEVICE(0, 0xfd020000, evt2irq(0x1060)),
	STPIO_DEVICE(1, 0xfd021000, evt2irq(0x1040)),
	STPIO_DEVICE(2, 0xfd022000, evt2irq(0x1020)),
	STPIO_DEVICE(3, 0xfd023000, evt2irq(0x1000)),
};

static struct platform_device stx5206_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.name = "STx5206 chip temperature",
		.pdn = { SYS_CFG, 41, 4, 4 },
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
	},
};



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx5206_postcore_setup(void)
{
	int i;

	emi_init(0, 0xfe700000);

	for (i = 0; i < ARRAY_SIZE(stx5206_pio_devices); i++)
		platform_device_register(&stx5206_pio_devices[i]);

	return 0;
}
postcore_initcall(stx5206_postcore_setup);



/* Early devices initialization ------------------------------------------- */

/* Initialise devices which are required early in the boot process. */
void __init stx5206_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx5206_sysconf_device, 1);
	stpio_early_init(stx5206_pio_devices, ARRAY_SIZE(stx5206_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	/* Route the GPIO block to the PIOx pads, as by default the
	 * SATFE owns them... (why oh why???) */

	sc = sysconf_claim(SYS_CFG, 10, 13, 13, "cfg_satfe_gpio_on_pio");
	sysconf_write(sc, 1);

	/* Get the SOC revision */

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx5206 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static struct platform_device stx5206_emi = STEMI();

static struct platform_device stx5206_lpc =
	STLPC_DEVICE(0xfd008000, ILC_EXT_IRQ(7), IRQ_TYPE_EDGE_FALLING,
			0, 1, "CLKB_LPC");

/* Late devices initialisation -------------------------------------------- */

static struct platform_device *stx5206_devices[] __initdata = {
	&stx5206_fdma_device,
	&stx5206_sysconf_device,
	&stx5206_ilc3_device,
	&stx5206_rng_dev_hwrandom_device,
	&stx5206_rng_dev_random_device,
	&stx5206_temp_device,
	&stx5206_emi,
	&stx5206_lpc,
};

#include "./platform-pm-stx5206.c"

static int __init stx5206_devices_setup(void)
{
	platform_add_pm_devices(stx5206_pm_devices,
			ARRAY_SIZE(stx5206_pm_devices));

	return platform_add_devices(stx5206_devices,
			ARRAY_SIZE(stx5206_devices));
}
device_initcall(stx5206_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */

	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,

	PCI_DEV0,					/* Group 0 */
	PCI_DEV1, PCI_DEV2, PCI_DEV3, MMC,
	SPI_SEQ, SPI_DATA, AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
		COMPO_CAP_TF, COMPO_CAP_BF,
	NAND_SEQ,
	NAND_DATA,

	PIO3, PIO2, PIO1, PIO0,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	PCI_ERROR, FDMA_FLAG, DAA, TTXT,		/* Group 5 */
	PCI_DMA, GMAC, TS_MERGER, SBAG_HDMI_CEC_WAKEUP,	/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, GMAC_PMT,		/* Group 7 */
	FDMA, BDISP_CQ2, BDISP_CQ1, HDMI_CEC,		/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	TVO_DCS0, DELMU_PP, NAND, DELMU_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
		MAIN_VTG, AUX_VTG,
	BDISP_AQ4, BDISP_AQ3, BDISP_AQ2, BDISP_AQ1,	/* Group 12 */
	PTI, PDES_ESA, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,		/* Group 14 */
		CRIPTO_SIG_CHK,
	OHCI, EHCI, TVO_DCS1, FILTER,			/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES,		/* Group 16 */

	/* interrupt groups */

	GROUP0_0, GROUP0_1,
	GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
	GROUP16
};

static struct intc_vect stx5206_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(PCI_DEV0, 0xa00),
	INTC_VECT(PCI_DEV1, 0xa20), INTC_VECT(PCI_DEV2, 0xa40),
		INTC_VECT(PCI_DEV3, 0xa60), INTC_VECT(MMC, 0xa80),
	INTC_VECT(SPI_SEQ, 0xb00), INTC_VECT(SPI_DATA, 0xb20),
		INTC_VECT(AUX_VDP_END_PROC, 0xb40),
		INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb60),
		INTC_VECT(COMPO_CAP_TF, 0xb80),
		INTC_VECT(COMPO_CAP_BF, 0xba0),
	INTC_VECT(NAND_SEQ, 0xc00),
	INTC_VECT(NAND_DATA, 0xc80),

	INTC_VECT(PIO3, 0x1000), INTC_VECT(PIO2, 0x1020),
		INTC_VECT(PIO1, 0x1040), INTC_VECT(PIO0, 0x1060),
	INTC_VECT(SSC3, 0x1080), INTC_VECT(SSC2, 0x10a0),
		INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
		INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
		INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(PCI_ERROR, 0x1200), INTC_VECT(FDMA_FLAG, 0x1220),
		INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(PCI_DMA, 0x1280), INTC_VECT(GMAC, 0x12a0),
		INTC_VECT(TS_MERGER, 0x12c0),
		INTC_VECT(SBAG_HDMI_CEC_WAKEUP, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
		INTC_VECT(DCXO, 0x1340), INTC_VECT(GMAC_PMT, 0x1360),
	INTC_VECT(FDMA, 0x1380), INTC_VECT(BDISP_CQ2, 0x13a0),
		INTC_VECT(BDISP_CQ1, 0x13c0), INTC_VECT(HDMI_CEC, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
		INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(TVO_DCS0, 0x1480), INTC_VECT(DELMU_PP, 0x14a0),
		INTC_VECT(NAND, 0x14c0), INTC_VECT(DELMU_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500),
		INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
		INTC_VECT(MAIN_VTG, 0x1540),
		INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(BDISP_AQ4, 0x1580), INTC_VECT(BDISP_AQ3, 0x15a0),
		INTC_VECT(BDISP_AQ2, 0x15c0), INTC_VECT(BDISP_AQ1, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA, 0x1620),
		INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
		INTC_VECT(CRIPTO_SIGDMA, 0x16c0),
		INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
		INTC_VECT(TVO_DCS1, 0x1740), INTC_VECT(FILTER, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
		INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES, 0x17e0),
};

static struct intc_group stx5206_intc_groups[] = {
	/* PCI_DEV0 is a single bit group member */
	INTC_GROUP(GROUP0_0, PCI_DEV1, PCI_DEV2, PCI_DEV3, MMC),
	INTC_GROUP(GROUP0_1, SPI_SEQ, SPI_DATA, AUX_VDP_END_PROC,
			AUX_VDP_FIFO_EMPTY, COMPO_CAP_TF, COMPO_CAP_BF),
	/* PIO0, PIO1, PIO2 are single bit groups members */

	INTC_GROUP(GROUP1, PIO3, PIO2, PIO1, PIO0),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, PCI_ERROR, FDMA_FLAG, DAA, TTXT),
	INTC_GROUP(GROUP6, PCI_DMA, GMAC, TS_MERGER, SBAG_HDMI_CEC_WAKEUP),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, GMAC_PMT),
	INTC_GROUP(GROUP8, FDMA, BDISP_CQ2, BDISP_CQ1, HDMI_CEC),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, TVO_DCS0, DELMU_PP, NAND, DELMU_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
			MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, BDISP_AQ4, BDISP_AQ3, BDISP_AQ2, BDISP_AQ1),
	INTC_GROUP(GROUP13, PTI, PDES_ESA, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,
			CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI, EHCI, TVO_DCS1, FILTER),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES)
};

static struct intc_prio_reg stx5206_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },

	/* offset against          31-28,    27-24,    23-20,     19-16,
	   intc2_base below        15-12,     11-8,      7-4,       3-0  */
	{ 0x00000000, 0, 32, 4,
		/* INTPRI00 */ {       0,        0, NAND_SEQ, NAND_DATA,
				    PIO0, GROUP0_1, GROUP0_0,  PCI_DEV0} },
	{ 0x00000004, 0, 32, 4,
		/* INTPRI04 */ {  GROUP8,   GROUP7,   GROUP6,    GROUP5,
				  GROUP4,   GROUP3,   GROUP2,    GROUP1 } },
	{ 0x00000008, 0, 32, 4,
		/* INTPRI08 */ { GROUP16,  GROUP15,  GROUP14,   GROUP13,
				 GROUP12,  GROUP11,  GROUP10,    GROUP9 } },
};

static struct intc_mask_reg stx5206_intc_mask_registers[] = {
	/* offsets against
	 * intc2_base below */
	{ 0x00000040, 0x00000060, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, 0, NAND_DATA, NAND_SEQ,				/* 15..12 */
	    0, COMPO_CAP_BF, COMPO_CAP_TF, AUX_VDP_FIFO_EMPTY,	/* 11...8 */
	    AUX_VDP_END_PROC, SPI_DATA, SPI_SEQ, MMC,		/*  7...4 */
	    PCI_DEV3, PCI_DEV2, PCI_DEV1, PCI_DEV0 } },		/*  3...0 */
	{ 0x00000044, 0x00000064, 32, /* INTMSK04 / INTMSKCLR04 */
	  { HDMI_CEC, BDISP_CQ1, BDISP_CQ2, FDMA,		/* 31..28 */
	    GMAC_PMT, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    SBAG_HDMI_CEC_WAKEUP, TS_MERGER, GMAC, PCI_DMA,	/* 23..20 */
	    TTXT, DAA, FDMA_FLAG, PCI_ERROR,			/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3, 				/*  7...4 */
	    PIO0, PIO1, PIO2, PIO3 } },				/*  3...0 */
	{ 0x00000048, 0x00000068, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES, KEY_SCANNER, ICAM3, ICAM3_KTE,			/* 31..28 */
	    FILTER, TVO_DCS1, EHCI, OHCI,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIGDMA, TKDMA_DMA, TKDMA_TKD,/* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA, PTI,			/* 19..16 */
	    BDISP_AQ1, BDISP_AQ2, BDISP_AQ3, BDISP_AQ4,		/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING,		/* 11...8 */
		MAIN_VDP_FIFO_EMPTY,
	    DELMU_MBE, NAND, DELMU_PP, TVO_DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(stx5206_intc_desc, "stx5206", stx5206_intc_vectors,
		stx5206_intc_groups, stx5206_intc_mask_registers,
		stx5206_intc_prio_registers, NULL);

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

void __init plat_irq_setup(void)
{
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001300, 0x100);
	unsigned int intc_irl_irqs[] = { evt2irq(0x240), evt2irq(0x2a0),
		evt2irq(0x300), evt2irq(0x360) };
	int i;

	for (i = 4; i < ARRAY_SIZE(stx5206_intc_prio_registers); i++)
		stx5206_intc_prio_registers[i].set_reg += intc2_base;

	for (i = 0; i < ARRAY_SIZE(stx5206_intc_mask_registers); i++) {
		stx5206_intc_mask_registers[i].set_reg += intc2_base;
		stx5206_intc_mask_registers[i].clr_reg += intc2_base;
	}

	register_intc_controller(&stx5206_intc_desc);

	/* Disable encoded interrupts */
	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);

	/* IRL0-3 are simply connected to ILC remote outputs 1-4 */

	for (i = 0; i < ARRAY_SIZE(intc_irl_irqs); i++) {
		/* This is a hack to allow for the fact that we don't
		 * register a chip type for the IRL lines. Without
		 * this the interrupt type is "no_irq_chip" which
		 * causes problems when trying to register the chained
		 * handler. */
		set_irq_chip(intc_irl_irqs[i], &dummy_irq_chip);

		set_irq_chained_handler(intc_irl_irqs[i], ilc_irq_demux);
	}
	ilc_early_init(&stx5206_ilc3_device);
	ilc_demux_init();
}

void __init stx5206_configure_nand(struct platform_device *pdev)
{
	/* NAND Controller base address */
	pdev->resource[0].start	= 0xFE701000;
	pdev->resource[0].end	= 0xFE701FFF;

	/* NAND Controller IRQ */
	pdev->resource[1].start	= evt2irq(0x14c0);
	pdev->resource[1].end	= evt2irq(0x14c0);

	platform_device_register(pdev);
}

static struct platform_device stx5206_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe702000,
			.end	= 0xfe7024ff,
			.flags	= IORESOURCE_MEM,
		},
	},
};

void __init stx5206_configure_spifsm(struct stm_plat_spifsm_data *spifsm_data)
{
	stx5206_spifsm_device.dev.platform_data = spifsm_data;

	platform_device_register(&stx5206_spifsm_device);
}
