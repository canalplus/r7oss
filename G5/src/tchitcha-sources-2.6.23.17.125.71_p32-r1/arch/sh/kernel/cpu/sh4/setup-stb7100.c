/*
 * STx7100/STx7109 Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/pata_platform.h>
#include <linux/dma-mapping.h>
#include <asm/sci.h>
#include <asm/irq-ilc.h>

static unsigned long chip_revision, chip_7109;
static struct sysconf_field *sys_cfg7_0;

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xffe00000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 26, 27, 28, 29 },
	}, {
		.mapbase	= 0xffe80000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 43, 44, 45, 46 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct resource wdt_resource[] = {
	/* Watchdog timer only needs a register address */
	[0] = {
		.start = 0xFFC00008,
		.end = 0xFFC00010,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device wdt_device = {
	.name = "wdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(wdt_resource),
	.resource = wdt_resource,
};

/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE			0x19100000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static u64 st40_dma_mask = DMA_32BIT_MASK;

static struct platform_device st_usb_device =
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE, 169,
		      AHB2STBUS_OHCI_BASE, 168,
			AHB2STBUS_WRAPPER_GLUE_BASE, AHB2STBUS_PROTOCOL_BASE,
			USB_FLAGS_STRAP_16BIT | USB_FLAGS_STRAP_PLL |
			USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE);

void __init stx7100_configure_usb(void)
{
	struct stpio_pin *pin;
	struct sysconf_field *sc;
	u32 reg;

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ( ( chip_7109 && (chip_revision < 2)) ||
	     (!chip_7109 && (chip_revision < 3)) ) {
		pin = stpio_request_pin(5,6, "USBOC", STPIO_OUT);
		stpio_set_pin(pin, 0);
	}

	/*
	 * There have been two changes to the USB power enable signal:
	 *
	 * - 7100 upto and including cut 3.0 and 7109 1.0 generated an
	 *   active high enables signal. From 7100 cut 3.1 and 7109 cut 2.0
	 *   the signal changed to active low.
	 *
	 * - The 710x ref board (mb442) has always used power distribution
	 *   chips which have active high enables signals (on rev A and B
	 *   this was a TI TPS2052, rev C used the ST equivalent a ST2052).
	 *   However rev A and B had a pull up on the enables signal, while
	 *   rev C changed this to a pull down.
	 *
	 * The net effect of all this is that the easiest way to drive
	 * this signal is ignore the USB hardware and drive it as a PIO
	 * pin.
	 *
	 * (Note the USB over current input on the 710x changed from active
	 * high to low at the same cuts, but board revs A and B had a resistor
	 * option to select an inverted output from the TPS2052, so no
	 * software work around is required.)
	 */
	pin = stpio_request_pin(5,7, "USBPWR", STPIO_OUT);
	stpio_set_pin(pin, 1);

#ifndef CONFIG_PM
	sc = sysconf_claim(SYS_CFG, 2, 1, 1, "usb");
	reg = sysconf_read(sc);
	if (reg) {
		sysconf_write(sc, 0);
		mdelay(30);
	}
#endif

	platform_device_register(&st_usb_device);

}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7100.h>
#include <linux/stm/fdma_firmware_7109c2.h>
#include <linux/stm/fdma_firmware_7109c3.h>

static struct stm_plat_fdma_hw stx7100_fdma_hw = {
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
	.dmem_size   = 0x600 << 2, /* 1536 * 4 = 6144 */
	.imem_offset = 0xc000,
	.imem_size   = 0xa00 << 2, /* 2560 * 4 = 10240 */
};

static struct stm_plat_fdma_data stx7100_fdma_platform_data = {
	.hw = &stx7100_fdma_hw,
	.fw = &stm_fdma_firmware_7100,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_hw stx7109c2_fdma_hw = {
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
	.dmem_size   = 0x600 << 2, /* 1536 * 4 = 6144 */
	.imem_offset = 0xc000,
	.imem_size   = 0xa00 << 2, /* 2560 * 4 = 10240 */
};

static struct stm_plat_fdma_data stx7109c2_fdma_platform_data = {
	.hw = &stx7109c2_fdma_hw,
	.fw = &stm_fdma_firmware_7109c2,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_hw stx7109c3_fdma_hw = {
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

static struct stm_plat_fdma_data stx7109c3_fdma_platform_data = {
	.hw = &stx7109c3_fdma_hw,
	.fw = &stm_fdma_firmware_7109c3,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7100_fdma_device = {
	.name		= "stm-fdma",
	.id		= -1,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		{
			.start = 0x19220000,
			.end   = 0x1922ffff,
			.flags = IORESOURCE_MEM,
		}, {
			.start = 140,
			.end   = 140,
			.flags = IORESOURCE_IRQ,
		},
	},
};

static void stx7100_fdma_setup(void)
{
#ifdef CONFIG_STM_DMA
	switch (cpu_data->type) {
	case CPU_STB7100:
		stx7100_fdma_device.dev.platform_data =
				&stx7100_fdma_platform_data;
		break;
	case CPU_STB7109:
		switch (cpu_data->cut_major) {
		case 1:
			BUG();
			break;
		case 2:
			stx7100_fdma_device.dev.platform_data =
					&stx7109c2_fdma_platform_data;
			break;
		default:
			stx7100_fdma_device.dev.platform_data =
					&stx7109c3_fdma_platform_data;
			break;
		}
		break;
	default:
		BUG();
		break;
	}
#endif
}

/* SSC resources ----------------------------------------------------------- */
static const char i2c_stm_ssc[] = "i2c_stm_ssc";
static const char i2c_stm_pio[] = "i2c_stm_pio";
static char spi_st[] = "spi_st_ssc";
static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0x18040000, 119, 2, 0, 1, 2),
	STSSC_DEVICE(0x18041000, 118, 3, 0, 1, 2),
	STSSC_DEVICE(0x18042000, 117, 4, 0, 1, 0xff),
};

static int __initdata num_i2c;
static int __initdata num_spi;
void __init stx7100_configure_ssc(struct plat_ssc_data *data)
{
	int i;
	int capability = data->capability;
	struct sysconf_field* ssc_sc;

	for (i=0; i<ARRAY_SIZE(stssc_devices); i++, capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if(capability & SSC_UNCONFIGURED)
			continue;
		if(!i){
			ssc_sc = sysconf_claim(SYS_CFG, 7, 10, 10, "stssc");
			sysconf_write(ssc_sc, 0);
		}

		ssc_sc = sysconf_claim(SYS_CFG, 7, i+1, i+1, "stssc");
		if(capability & SSC_SPI_CAPABILITY){
			stssc_devices[i].name = spi_st;
			sysconf_write(ssc_sc, 0);
			stssc_devices[i].id = num_spi++;
			ssc_pio->chipselect = data->spi_chipselects[i];
		} else {
			stssc_devices[i].name = ((capability & SSC_I2C_PIO) ?
				i2c_stm_pio : i2c_stm_ssc);
			sysconf_write(ssc_sc, 0);
			stssc_devices[i].id = num_i2c++;
			if (capability & SSC_I2C_CLK_UNIDIR)
				ssc_pio->clk_unidir = 1;
		}
		platform_device_register(&stssc_devices[i]);
	}

	/* I2C buses number reservation (to prevent any hot-plug device
	 * from using it) */
#ifdef CONFIG_I2C_BOARDINFO
	i2c_register_board_info(num_i2c - 1, NULL, 0);
#endif
}
/* SATA resources ---------------------------------------------------------- */

static struct resource sata_resource[]= {
	[0] = {
		.start = 0x18000000 + 0x01209000,
		.end   = 0x18000000 + 0x01209000 + 0xfff,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = 0xaa,
		.flags = IORESOURCE_IRQ
	},
};

static struct plat_sata_data sata_private_info;

static struct platform_device sata_device = {
	.name		= "sata_stm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sata_resource),
	.resource	= sata_resource,
	.dev = {
		.platform_data = &sata_private_info,
	}
};

void __init stx7100_configure_sata(void)
{
	if ((! chip_7109) && (chip_revision == 1)) {
		/* 7100 cut 1.x */
		sata_private_info.phy_init = 0x0013704A;
	} else {
		/* 7100 cut 2.x and cut 3.x and 7109 */
		sata_private_info.phy_init = 0x388fc;
	}

	if ((! chip_7109) || (chip_7109 && (chip_revision == 1))) {
		sata_private_info.only_32bit = 1;
		sata_private_info.pc_glue_logic_init = 0x1ff;
	} else {
		sata_private_info.only_32bit = 0;
		sata_private_info.pc_glue_logic_init = 0x100ff;
	}

	platform_device_register(&sata_device);
}

/* PATA resources ---------------------------------------------------------- */

/*
 * EMI A21 = CS1 (active low)
 * EMI A20 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0
 */

static struct resource pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start	= (1<<21),
		.end	= (1<<21) + (8<<17)-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start	= (1<<20) + (6<<17),
		.end	= (1<<20) + (6<<17) + 3,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {	/* IRQ */
		.flags	= IORESOURCE_IRQ,
	}
};

static struct pata_platform_info pata_info = {
	.ioport_shift	= 17,
};

static struct platform_device pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pata_resources),
	.resource	= pata_resources,
	.dev = {
		.platform_data = &pata_info,
	}
};

void __init stx7100_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base;

	bank_base = emi_bank_base(bank);
	pata_resources[0].start += bank_base;
	pata_resources[0].end   += bank_base;
	pata_resources[1].start += bank_base;
	pata_resources[1].end   += bank_base;
	pata_resources[2].start = irq;
	pata_resources[2].end   = irq;

	emi_config_pata(bank, pc_mode);

	platform_device_register(&pata_device);
}

/* Ethernet MAC resources -------------------------------------------------- */

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void* priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 1 : 0);
}

/* Hopefully I can remove this now */
static void stb7109eth_hw_setup_null(void)
{
}

static struct plat_stmmacenet_data eth7109_private_data = {
	.bus_id = 0,
	.pbl = 1,
	.has_gmac = 0,
	.fix_mac_speed = fix_mac_speed,
	.hw_setup = stb7109eth_hw_setup_null,
};

static struct platform_device stb7109eth_device = {
        .name           = "stmmaceth",
        .id             = 0,
        .num_resources  = 2,
        .resource       = (struct resource[]) {
        	{
	                .start = 0x18110000,
        	        .end   = 0x1811ffff,
                	.flags  = IORESOURCE_MEM,
        	},
        	{
			.name   = "macirq",
                	.start  = 133,
                	.end    = 133,
                	.flags  = IORESOURCE_IRQ,
        	},
	},
	.dev = {
		.platform_data = &eth7109_private_data,
	}
};

void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;

	if (!chip_7109)
		return;

	eth7109_private_data.bus_id = phy_bus;

	/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
	sc = sysconf_claim(SYS_CFG, 7, 16, 17, "stmmac");
	sysconf_write(sc, 3);

	/* RMII_MODE */
	sc = sysconf_claim(SYS_CFG, 7, 18, 18, "stmmac");
	sysconf_write(sc, rmii_mode ? 1 : 0);

	/* PHY_CLK_EXT */
	sc = sysconf_claim(SYS_CFG, 7, 19, 19, "stmmac");
	sysconf_write(sc, ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	mac_speed_sc = sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Remove the PHY clk */
	stpio_request_pin(3, 7, "stmmac EXTCLK", STPIO_ALT_OUT);

	/* Configure the ethernet MAC PBL depending on the cut of the chip */
	if (chip_revision == 1){
		eth7109_private_data.pbl = 1;
	} else {
		eth7109_private_data.pbl = 32;
	}

	platform_device_register(&stb7109eth_device);
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[]= {
	[0] = {
		.start	= 0x18010000,
		.end	= 0x18010000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= 126,
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx7100_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		if (sys_cfg7_0 == NULL)
			sys_cfg7_0 = sysconf_claim(SYS_CFG, 7, 0, 0, "pwm");
		sysconf_write(sys_cfg7_0, 0);
		stpio_request_pin(4, 6, "PWM", STPIO_ALT_OUT);
	}

	if (data->flags & PLAT_STM_PWM_OUT1) {
		stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
	}

	platform_device_register(&stm_pwm_device);
}

/* SH-RTC resources ----------------------------------------------------------- */

static struct platform_device rtc_device = {
	.name           = "sh-rtc",
	.id             = -1,
	.num_resources  = 2,
	.resource       = (struct resource []) {
		{
			.start = 0xffc80000,
			.end   = 0xffc80000 + 0x3c,
			.flags = IORESOURCE_IO
		}, { /* Shared Period/Carry/Alarm IRQ */
			.start = 20,
			.end   = 20,
			.flags = IORESOURCE_IRQ
		},
	},
};

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
	[0] = {
		.bank = 3,
		.pin  = 3,
		.dir  = STPIO_IN,
                .pinof= 0x00 | LIRC_IR_RX | LIRC_PIO_ON
	},
	[1] = {
		.bank = 3,
		.pin  = 4,
		.dir  = STPIO_IN,
                .pinof= 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
	},
	[2] = {
		.bank = 3,
		.pin  = 5,
		.dir  = STPIO_ALT_OUT,
                .pinof= 0x00 | LIRC_IR_TX /* | LIRC_PIO_ON not available */
	},
	[3] = {
		.bank = 3,
		.pin  = 6,
		.dir  = STPIO_ALT_OUT,
                .pinof= 0x00 | LIRC_IR_TX /* | LIRC_PIO_ON not available */
	}
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7100, the clock settings will be calculated by the driver
	 * from the system clock
	 */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv      = 0, /* automatically calculate */
	.irbperiodmult  = 0,
	.irbperioddiv   = 0,
	.irbontimemult  = 0,
	.irbontimediv   = 0,
#ifdef CONFIG_SH_HMS1
	.irbrxmaxperiod = 0x2328,
#else
	.irbrxmaxperiod = 0x5000,
#endif
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = lirc_pios,
	.num_pio_pins = ARRAY_SIZE(lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 1500000,
#endif
};

static struct platform_device lirc_device =
	STLIRC_DEVICE(0x18018000, 125, -1);

void __init stx7100_configure_lirc(lirc_scd_t *scd)
{
        lirc_private_info.scd_info = scd;

        platform_device_register(&lirc_device);
}

/* Hardware RNG resources -------------------------------------------------- */

static struct platform_device hwrandom_rng_device = {
	.name           = "stm_hwrandom",
	.id             = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0x19250000,
			.end    = 0x19250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

static struct platform_device devrandom_rng_device = {
	.name           = "stm_rng",
	.id             = 0,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0x19250000,
			.end    = 0x19250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	STASC_DEVICE(0x18030000, 123, -1, -1, 0, 0, 1, 4, 7,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0x18031000, 122, -1, -1, 1, 0, 1, 4, 5,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0x18032000, 121, -1, -1, 4, 3, 2, 4, 5,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0x18033000, 120, -1, -1, 5, 0, 1, 2, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

static unsigned int __initdata stm_stasc_fdma_requests_7100[][2] = {
	{ 14, 18 },
	{ 15, 19 },
	{ 16, 20 },
	{ 17, 21 },
};

static unsigned int __initdata stm_stasc_fdma_requests_7109[][2] = {
	{ 12, 16 },
	{ 13, 17 },
	{ 14, 18 },
	{ 15, 19 },
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[ARRAY_SIZE(stm_stasc_devices)] __initdata;
unsigned int stasc_configured_devices_count __initdata = 0;


/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stb7100_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i=0; i<num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		unsigned int *fdma_requests;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];

		if (chip_7109)
			fdma_requests = stm_stasc_fdma_requests_7109[port];
		else
			fdma_requests = stm_stasc_fdma_requests_7100[port];

		pdev->resource[2].start = fdma_requests[0];
		pdev->resource[2].end   = fdma_requests[0];
		pdev->resource[3].start = fdma_requests[1];
		pdev->resource[3].end   = fdma_requests[1];

		switch (port) {
		case 2:
			if (sys_cfg7_0 == NULL)
				sys_cfg7_0 = sysconf_claim(SYS_CFG, 7, 0, 0, "asc");
			sysconf_write(sys_cfg7_0, 0);
			break;
		}

		pdev->id = i;
		((struct stasc_uart_data*)(pdev->dev.platform_data))->flags = flags;
		stasc_configured_devices[stasc_configured_devices_count++] = pdev;
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init stb7100_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stb7100_add_asc);



/* Early resources (sysconf and PIO) --------------------------------------- */

static struct platform_device sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x19001000,
			.end	= 0x19001187,
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

static struct platform_device stpio_devices[] = {
	STPIO_DEVICE(0, 0x18020000, 80),
	STPIO_DEVICE(1, 0x18021000, 84),
	STPIO_DEVICE(2, 0x18022000, 88),
	STPIO_DEVICE(3, 0x18023000, 115),
	STPIO_DEVICE(4, 0x18024000, 114),
	STPIO_DEVICE(5, 0x18025000, 113),
};

/* Initialise devices which are required early in the boot process. */
void __init stx7100_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;

	/* Create a PMB mapping so that the ioremap calls these drivers
	 * will make can be satisfied without having to call get_vm_area
	 * or cause a fault. Its probably also a good for efficiency as
	 * there will be lots of devices in this range.
	 */
	ioremap_nocache(0x18000000, 0x04000000);

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&sysconf_device, 1);
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices),
			 176);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk("%s version %ld.x\n",
	       chip_7109 ? "STx7109" : "STx7100", chip_revision);

	if (chip_7109) {
		boot_cpu_data.type = CPU_STB7109;
		sc = sysconf_claim(SYS_STA, 9, 0, 7, "devid");
		devid = sysconf_read(sc);
		printk("Chip version %ld.%ld\n", (devid >> 4)+1, devid & 0xf);
		boot_cpu_data.cut_minor = devid & 0xf;
		if (devid == 0x24) {
			/*
			 * See ADCS 8135002 "STI7109 CUT 4.0 CHANGES
			 * VERSUS CUT 3.X" for details of this change.
			 */
			printk("Setting version to 4.0 to match commercial branding\n");
			boot_cpu_data.cut_major = 4;
			boot_cpu_data.cut_minor = 0;
		}
	}

	/* Configure the ST40 RTC to source its clock from clockgenB.
	 * In theory this should be board specific, but so far nobody
	 * has ever done this. */
	sc = sysconf_claim(SYS_CFG, 8, 1, 1, "rtc");
	sysconf_write(sc, 1);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static void __init pio_late_setup(void)
{
	int i;
	struct platform_device *pdev = stpio_devices;

	for (i=0; i<ARRAY_SIZE(stpio_devices); i++,pdev++) {
		platform_device_register(pdev);
	}
}

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x18000000,
			.end	= 0x18000000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx7100_postcore_setup(void)
{
	emi_init(0, 0x1a100000);

	return 0;
}
postcore_initcall(stx7100_postcore_setup);

static struct platform_device emi = STEMI();
/* Late resources ---------------------------------------------------------- */

static struct platform_device *stx7100_devices[] __initdata = {
	&sci_device,
	&wdt_device,
	&stx7100_fdma_device,
	&sysconf_device,
	&ilc3_device,
	&rtc_device,
	&hwrandom_rng_device,
	&devrandom_rng_device,
	&emi,
};

#include "./platform-pm-stb7100.c"

static int __init stx7100_devices_setup(void)
{
	stx7100_fdma_setup();
	pio_late_setup();

	platform_add_pm_devices(stx7100_pm_devices,
				ARRAY_SIZE(stx7100_pm_devices));

	return platform_add_devices(stx7100_devices,
				    ARRAY_SIZE(stx7100_devices));
}
device_initcall(stx7100_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, RTC, SCIF, WDT, HUDI,

	SATA_DMAC, SATA_HOSTC,
	PIO0, PIO1, PIO2,
	PIO5, PIO4, PIO3, MTP,			/* Group 0 */
	SSC2, SSC1, SSC0,			/* Group 1 */
	UART3, UART2, UART1, UART0,		/* Group 2 */
	IRB_WAKEUP, IRB, PWM, MAFE,		/* Group 3 */
	DISEQC, DAA, TTXT,			/* Group 4 */
	EMPI, ETH_MAC, TS_MERGER,		/* Group 5 */
	ST231_DELTA, ST231_AUD, DCXO, PTI1,	/* Group 6 */
	FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM,	/* Group 7 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,	/* Group 8 */
	MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE,	/* Group 9 */
	VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2,	/* Group 10 */
	BDISP_AQ1, DVP, HDMI, HDCP,			/* Group 11 */
	PTI, PDES_ESA0, PDES, PRES_READ_CW,		/* Group 12 */
	SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK,	/* Group 13 */
	OHCI, EHCI, SATA, BDISP_CQ1,			/* Group 14 */
	ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS,	/* Group 15 */

	/* interrupt groups */
	SATA_SPLIT,
	GROUP0, GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(RTC, 0x480), INTC_VECT(RTC, 0x4a0), INTC_VECT(RTC, 0x4c0),
	INTC_VECT(SCIF, 0x4e0), INTC_VECT(SCIF, 0x500),
		INTC_VECT(SCIF, 0x520), INTC_VECT(SCIF, 0x540),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(SATA_DMAC, 0xa20), INTC_VECT(SATA_HOSTC, 0xa40),
	INTC_VECT(PIO0, 0xc00), INTC_VECT(PIO1, 0xc80), INTC_VECT(PIO2, 0xd00),
	INTC_VECT(MTP, 0x1000),INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(DISEQC, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI, 0x1280), INTC_VECT(ETH_MAC, 0x12a0),
	INTC_VECT(TS_MERGER, 0x12c0),
	INTC_VECT(ST231_DELTA, 0x1300), INTC_VECT(ST231_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA_MBOX, 0x1380), INTC_VECT(FDMA_GP0, 0x13a0),
	INTC_VECT(I2S2SPDIF, 0x13c0), INTC_VECT(CPXM, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(MPEG2, 0x1480), INTC_VECT(DELTA_PRE0, 0x14a0),
	INTC_VECT(DELTA_PRE1, 0x14c0), INTC_VECT(DELTA_MBE, 0x14e0),
	INTC_VECT(VDP_FIFO_EMPTY, 0x1500), INTC_VECT(VDP_END_PROC, 0x1520),
	INTC_VECT(VTG1, 0x1540), INTC_VECT(VTG2, 0x1560),
	INTC_VECT(BDISP_AQ1, 0x1580), INTC_VECT(DVP, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA0, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PRES_READ_CW, 0x1660),
	INTC_VECT(SIG_CHK, 0x1680), INTC_VECT(TKDMA, 0x16a0),
	INTC_VECT(CRIPTO_SIG_DMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
	INTC_VECT(SATA, 0x1740), INTC_VECT(BDISP_CQ1, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(MES_LMI_VID, 0x17c0), INTC_VECT(MES_LMI_SYS, 0x17e0)
};

static struct intc_group groups[] = {
	INTC_GROUP(SATA_SPLIT, SATA_DMAC, SATA_HOSTC),
	INTC_GROUP(GROUP0, PIO5, PIO4, PIO3, MTP),
	INTC_GROUP(GROUP1, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP2, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP3, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP4, DISEQC, DAA, TTXT),
	INTC_GROUP(GROUP5, EMPI, ETH_MAC, TS_MERGER),
	INTC_GROUP(GROUP6, ST231_DELTA, ST231_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP7, FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM),
	INTC_GROUP(GROUP8, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP9, MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE),
	INTC_GROUP(GROUP10, VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2),
	INTC_GROUP(GROUP11, BDISP_AQ1, DVP, HDMI, HDCP),
	INTC_GROUP(GROUP12, PTI, PDES_ESA0, PDES, PRES_READ_CW),
	INTC_GROUP(GROUP13, SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP14, OHCI, EHCI, SATA, BDISP_CQ1),
	INTC_GROUP(GROUP15, ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
						/* 31-28,   27-24,   23-20,   19-16 */
						/* 15-12,    11-8,     7-4,     3-0 */
	{ 0x00000300, 0, 32, 4, /* INTPRI00 */ {       0,       0,    PIO2,    PIO1,
						    PIO0,       0, SATA_SPLIT,    0 } },
	{ 0x00000304, 0, 32, 4, /* INTPRI04 */ {  GROUP7,  GROUP6,  GROUP5,  GROUP4,
						  GROUP3,  GROUP2,  GROUP1,  GROUP0 } },
	{ 0x00000308, 0, 32, 4, /* INTPRI08 */ { GROUP15, GROUP14, GROUP13, GROUP12,
						 GROUP11, GROUP10,  GROUP9,  GROUP8 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0x00000340, 0x00000360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    0, 0, 0, 0,						/* 11...8 */
	    0, 0, 0, 0,						/*  7...4 */
	    0, SATA_HOSTC, SATA_DMAC, 0 } },			/*  3...0 */
	{ 0x00000344, 0x00000364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { CPXM, I2S2SPDIF, FDMA_GP0, FDMA_MBOX,		/* 31..28 */
	    PTI1, DCXO, ST231_AUD, ST231_DELTA,			/* 27..24 */
	    0, TS_MERGER, ETH_MAC, EMPI,			/* 23..20 */
	    TTXT, DAA, DISEQC, 0,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP, 			/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, 0,				/*  7...4 */
	    PIO3, PIO4, PIO5, MTP } },				/*  3...0 */
	{ 0x00000348, 0x00000368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES_LMI_SYS, MES_LMI_VID, ICAM3, ICAM3_KTE, 	/* 31..28 */
	    BDISP_CQ1, SATA, EHCI, OHCI,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIG_DMA, TKDMA, SIG_CHK,	/* 23..20 */
	    PRES_READ_CW, PDES, PDES_ESA0, PTI,			/* 19..16 */
	    HDCP, HDMI, DVP, BDISP_AQ1,				/* 15..12 */
	    VTG2, VTG1, VDP_END_PROC, VDP_FIFO_EMPTY,		/* 11...8 */
	    DELTA_MBE, DELTA_PRE1, DELTA_PRE0, MPEG2,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7100", vectors, groups,
			 mask_registers, prio_registers, NULL);

static struct intc_vect vectors_irlm[] = {
	INTC_VECT(IRL0, 0x240), INTC_VECT(IRL1, 0x2a0),
	INTC_VECT(IRL2, 0x300), INTC_VECT(IRL3, 0x360),
};

static DECLARE_INTC_DESC(intc_desc_irlm, "stx7100_irlm", vectors_irlm, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	void __iomem *intc2_base = ioremap(0x19001000, 0x400);
	int i;

	ilc_early_init(&ilc3_device);

	for (i=4; i<=6; i++)
		prio_registers[i].set_reg += (unsigned long) intc2_base;
	for (i=0; i<=2; i++) {
		mask_registers[i].set_reg += (unsigned long) intc2_base;
		mask_registers[i].clr_reg += (unsigned long) intc2_base;
	}

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);
}

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

void __init plat_irq_setup_pins(int mode)
{
	switch (mode) {
	case IRQ_MODE_IRQ: /* individual interrupt mode for IRL3-0 */
		register_intc_controller(&intc_desc_irlm);
		ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);
		break;
	default:
		BUG();
	}
}
