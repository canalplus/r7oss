/*
 * STx5197 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <asm/irl.h>
#include <asm/irq-ilc.h>

#include "soc-stx5197.h"

struct {
	unsigned char regtype, regnum;
	unsigned char off[2];
} const pio_conf[5] = {
	{ CFG_CTRL_F, {  0,  8} },
	{ CFG_CTRL_F, { 16, 24} },
	{ CFG_CTRL_G, {  0,  8} },
	{ CFG_CTRL_G, { 16, 24} },
	{ CFG_CTRL_O, {  0,  8} }
};

static void stx5197_pio_conf(int bank, int pin, int alt, const char *name)
{
	int regtype = pio_conf[bank].regtype;
	int regnum = pio_conf[bank].regnum;
	int bit[2] = {
		 pio_conf[bank].off[0] + pin,
		 pio_conf[bank].off[1] + pin
	};
	struct sysconf_field *sc[2];

	sc[0] = sysconf_claim(regtype, regnum, bit[0], bit[0], name);
	sc[1] = sysconf_claim(regtype, regnum, bit[1], bit[1], name);
	sysconf_write(sc[0], (alt >> 0) & 1);
	sysconf_write(sc[1], (alt >> 1) & 1);
}

static u64 st40_dma_mask = DMA_32BIT_MASK;

/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE			0xfdd00000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static struct platform_device st_usb =
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE, ILC_IRQ(29),
		      AHB2STBUS_OHCI_BASE, ILC_IRQ(28),
		      AHB2STBUS_WRAPPER_GLUE_BASE,
		      AHB2STBUS_PROTOCOL_BASE,
		      USB_FLAGS_STRAP_16BIT	|
		      USB_FLAGS_STRAP_PLL	|
		      USB_FLAGS_STBUS_CONFIG_THRESHOLD_256 |
		      USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
		      USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32);

void __init stx5197_configure_usb(void)
{
	struct sysconf_field *sc;
#ifndef CONFIG_PM
	/* USB power down */
	sc = sysconf_claim(CFG_CTRL_H, 8, 8, "USB");
	sysconf_write(sc, 0);
#endif
	/* DDR enable for ULPI. 0=8 bit SDR ULPI, 1=4 bit DDR ULPI */
	sc = sysconf_claim(CFG_CTRL_M, 12, 12, "USB");
	sysconf_write(sc, 0);

	platform_device_register(&st_usb);

}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw stx5197_fdma_hw = {
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

static struct stm_plat_fdma_data stx5197_fdma_platform_data = {
	.hw = &stx5197_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx5197_fdma_platform_data_addr &stx5197_fdma_platform_data

#else

#define stx5197_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx5197_fdma_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		{
			.start = 0xfdb00000,
			.end   = 0xfdb0ffff,
			.flags = IORESOURCE_MEM,
		}, {
			.start = ILC_IRQ(34),
			.end   = ILC_IRQ(34),
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev.platform_data = stx5197_fdma_platform_data_addr,
};

/* SSC resources ----------------------------------------------------------- */
static const char i2c_stm_ssc[] = "i2c_stm_ssc";
static const char i2c_stm_pio[] = "i2c_stm_pio";
static char spi_st[] = "spi_st_ssc";

static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd140000, ILC_IRQ(5),  1, 6, 7, 0xff),
	STSSC_DEVICE(0xfd141000, ILC_IRQ(6),  SSC_NO_PIO, 0,0,0),
	STSSC_DEVICE(0xfd142000, ILC_IRQ(17), 3, 3, 2, 0xff),
};

static struct sysconf_field *spi_cs;
static void stx5197_ssc0_cs(void *spi, int is_on)
{
	sysconf_write(spi_cs, is_on ? 0 : 1);
}

void __init stx5197_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int i;
	int capability = data->capability;
	int routing = data->routing;
	struct sysconf_field *sc;
	const unsigned char alt_ssc[3] = { 2, 0xff, 1 };
	const unsigned char alt_pio[3] = { 1, 0xff, 0 };

	for (i = 0; i < ARRAY_SIZE(stssc_devices);
	     i++, capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if (capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc_pio->clk_unidir = 1;

		switch (i) {
		case 0:
			/* SSC0 can either drive the SPI pins (in which
			 * case it is SPI) or PIO1[7:6] (I2C).
			 */

			/* spi_bootnotcomms
			 * 0: SSC0 -> PIO1[7:6], 1: SSC0 -> SPI */
			sc = sysconf_claim(CFG_CTRL_M, 14, 14,
					   "ssc");

			if (capability & SSC_SPI_CAPABILITY) {
				sysconf_write(sc, 1);
				ssc_pio->pio[0].pio_port = SSC_NO_PIO;

				spi_cs = sysconf_claim(CFG_CTRL_M,
						       13, 13, "ssc");
				sysconf_write(spi_cs, 1);
				ssc_pio->chipselect = stx5197_ssc0_cs;
			} else {
				sysconf_write(sc, 0);
			}

			/* pio_functionality_on_pio1_7.
			 * 0: QAM validation, 1: Normal PIO */
			sc = sysconf_claim(CFG_CTRL_I, 2, 2, "ssc");
			sysconf_write(sc, 1);

			break;

		case 1:
			BUG_ON(capability & SSC_SPI_CAPABILITY);

			if (routing & SSC1_QPSK) {
				/* qpsk_debug_config
				 *  0 IP289 I2C input from PIO1[0:1]
				 *  1 IP289 input from BE COMMS SSC1
				 */
				sc = sysconf_claim(CFG_CTRL_C,
						   1, 1, "ssc");
				sysconf_write(sc, 1);
			} else {
				  /* 0: QPSK repeater interface is routed to
				   *    QAM_SCLT/SDAT.
				   * 1: SSC1 is routed to QAM_SCLT/SDAT.
				   */
				  sc = sysconf_claim(CFG_CTRL_K,
						     27, 27, "ssc");
				  sysconf_write(sc, 1);
			}
			break;

		case 2:
			/* SSC2 always drives PIO3[3:2] */
			BUG_ON(capability & SSC_SPI_CAPABILITY);
			break;
		}

		if (ssc_pio->pio[0].pio_port != SSC_NO_PIO) {
			int pin;

			for (pin = 0; pin < 2; pin++) {
				int portno = ssc_pio->pio[pin].pio_port;
				int pinno  = ssc_pio->pio[pin].pio_pin;
				int alt;

				if (capability & SSC_SPI_CAPABILITY)
#ifdef CONFIG_SPI_STM_PIO
					alt = alt_pio[i];
#else
					alt = alt_ssc[i];
#endif
				else
#ifdef CONFIG_I2C_ST40_PIO
					alt = alt_pio[i];
#else
					alt = alt_ssc[i];
#endif

				stx5197_pio_conf(portno, pinno, alt, "ssc");
			}
		}

		if (capability & SSC_SPI_CAPABILITY) {
			stssc_devices[i].name = spi_st;
			stssc_devices[i].id = num_spi++;
		} else {
			stssc_devices[i].name = ((capability & SSC_I2C_PIO) ?
				i2c_stm_pio : i2c_stm_ssc);
			stssc_devices[i].id = num_i2c++;
		}

		platform_device_register(&stssc_devices[i]);
	}

	/* I2C buses number reservation (to prevent any hot-plug device
	 * from using it) */
#ifdef CONFIG_I2C_BOARDINFO
	i2c_register_board_info(num_i2c - 1, NULL, 0);
#endif
}

/* Ethernet MAC resources -------------------------------------------------- */

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void *priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx5197eth_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.fix_mac_speed = fix_mac_speed,
};

static struct platform_device stx5197eth_device = {
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfde00000,
			.end	= 0xfde0ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(24),
			.end	= ILC_IRQ(24),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup    = 1,
		.platform_data = &stx5197eth_private_data,
	}
};

void stx5197_configure_ethernet(int rmii, int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;

	stx5197eth_private_data.bus_id = phy_bus;

	/* Ethernet interface on */
	sc = sysconf_claim(CFG_CTRL_E, 0, 0, "stmmac");
	sysconf_write(sc, 1);

	/* MII plyclk out enable: 0=output, 1=input */
	sc = sysconf_claim(CFG_CTRL_E, 6, 6, "stmmac");
	sysconf_write(sc, ext_clk);

	/* MAC speed*/
	mac_speed_sc = sysconf_claim(CFG_CTRL_E, 1, 1, "stmmac");

	/* RMII/MII pin mode */
	sc = sysconf_claim(CFG_CTRL_E, 7, 8, "stmmac");
	sysconf_write(sc, rmii ? 2 : 3);

	/* MII mode */
	sc = sysconf_claim(CFG_CTRL_E, 2, 2, "stmmac");
	sysconf_write(sc, rmii ? 0 : 1);

	platform_device_register(&stx5197eth_device);
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[] = {
	[0] = {
		.start	= 0xfd110000,
		.end	= 0xfd110000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= ILC_IRQ(43),
		.end	= ILC_IRQ(43),
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx5197_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		stx5197_pio_conf(2, 4, 1, "pwm");
		stpio_request_pin(2, 4, "pwm", STPIO_ALT_OUT);
	}

	platform_device_register(&stm_pwm_device);
}

/* LiRC resources ---------------------------------------------------------- */
static struct lirc_pio lirc_pios[] = {
	[0] = {
		.bank  = 2,
		.pin   = 5,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX  | LIRC_PIO_ON
	},
	[1] = {
		.bank  = 2,
		.pin   = 6,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
		/* To have UHF available on :
		   MB704: need one wire from J14-C to J14-E
		   MB676: need one wire from  J6-E to J15-A */
	},
	[2] = {
		.bank  = 2,
		.pin   = 7,
		.dir   = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	}
};

static struct plat_lirc_data lirc_private_info = {
	/* The clock settings will be calculated by the driver
	 * from the system clock
	 */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv      = 0, /* automatically calculate */
	.irbperiodmult  = 0,
	.irbperioddiv   = 0,
	.irbontimemult  = 0,
	.irbontimediv   = 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = lirc_pios,
	.num_pio_pins = ARRAY_SIZE(lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = XTAL,
#endif
};

static struct platform_device lirc_device = 
	STLIRC_DEVICE(0xfd118000, ILC_IRQ(19), ILC_EXT_IRQ(4));

void __init stx5197_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;

	platform_device_register(&lirc_device);
}

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	STASC_DEVICE(0xfd130000, ILC_IRQ(7), 8, 10,
		     0, 0, 1, 5, 4,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd131000, ILC_IRQ(8), 9, 11,
		     4, 0, 1, 3, 2,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd132000, ILC_IRQ(12), 3, 5,
		     1, 2, 3, 5, 4,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd133000, ILC_IRQ(13), 4, 6,
		     2, 0, 1, 2, 5,
		     STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

static const unsigned char asc_alt[4][4] = {
	{ 0, 0, 2, 2 },
	{ 2, 2, 3, 2 },
	{ 1, 1, 1, 1 },
	{ 1, 1, 1, 1 },
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[ARRAY_SIZE(stm_stasc_devices)]
	__initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stx5197_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i = 0; i < num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *uart_data;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];
		uart_data = pdev->dev.platform_data;

		/* Tx */
		stx5197_pio_conf(uart_data->pios[0].pio_port,
				uart_data->pios[0].pio_pin,
				asc_alt[port][0], "asc");
		/* Rx */
		stx5197_pio_conf(uart_data->pios[1].pio_port,
				uart_data->pios[1].pio_pin,
				asc_alt[port][1], "asc");

		if (!(flags & STASC_FLAG_NORTSCTS)) {
			/* CTS */
			stx5197_pio_conf(uart_data->pios[2].pio_port,
					 uart_data->pios[2].pio_pin,
					 asc_alt[port][2], "asc");
			/* RTS */
			stx5197_pio_conf(uart_data->pios[3].pio_port,
					 uart_data->pios[3].pio_pin,
					 asc_alt[port][3], "asc");
		}
		pdev->id = i;
		((struct stasc_uart_data *)(pdev->dev.platform_data))->flags =
			flags;
		stasc_configured_devices[stasc_configured_devices_count++] =
			pdev;
	}

	stasc_console_device = console;
}

/* Add platform device as configured by board specific code */
static int __init stx5197_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx5197_add_asc);

/* Early resources (sysconf and PIO) --------------------------------------- */

#ifdef CONFIG_PROC_FS

#define SYSCONF_FIELD(field) _SYSCONF_FIELD(#field, field)
#define _SYSCONF_FIELD(name, group, num) case num: return name

static const char *stx5197_sysconf_hd_field_name(int num)
{
	switch (num) {

	SYSCONF_FIELD(CFG_CTRL_C);
	SYSCONF_FIELD(CFG_CTRL_D);
	SYSCONF_FIELD(CFG_CTRL_E);
	SYSCONF_FIELD(CFG_CTRL_F);
	SYSCONF_FIELD(CFG_CTRL_G);
	SYSCONF_FIELD(CFG_CTRL_H);
	SYSCONF_FIELD(CFG_CTRL_I);
	SYSCONF_FIELD(CFG_CTRL_J);

	SYSCONF_FIELD(CFG_CTRL_K);
	SYSCONF_FIELD(CFG_CTRL_L);
	SYSCONF_FIELD(CFG_CTRL_M);
	SYSCONF_FIELD(CFG_CTRL_N);
	SYSCONF_FIELD(CFG_CTRL_O);
	SYSCONF_FIELD(CFG_CTRL_P);
	SYSCONF_FIELD(CFG_CTRL_Q);
	SYSCONF_FIELD(CFG_CTRL_R);

	SYSCONF_FIELD(CFG_MONITOR_C);
	SYSCONF_FIELD(CFG_MONITOR_D);
	SYSCONF_FIELD(CFG_MONITOR_E);
	SYSCONF_FIELD(CFG_MONITOR_F);
	SYSCONF_FIELD(CFG_MONITOR_G);
	SYSCONF_FIELD(CFG_MONITOR_H);
	SYSCONF_FIELD(CFG_MONITOR_I);
	SYSCONF_FIELD(CFG_MONITOR_J);

	SYSCONF_FIELD(CFG_MONITOR_K);
	SYSCONF_FIELD(CFG_MONITOR_L);
	SYSCONF_FIELD(CFG_MONITOR_M);
	SYSCONF_FIELD(CFG_MONITOR_N);
	SYSCONF_FIELD(CFG_MONITOR_O);
	SYSCONF_FIELD(CFG_MONITOR_P);
	SYSCONF_FIELD(CFG_MONITOR_Q);
	SYSCONF_FIELD(CFG_MONITOR_R);

	}

	return "???";
}

static const char *stx5197_sysconf_hs_field_name(int num)
{
	switch (num) {

	SYSCONF_FIELD(CFG_CTRL_A);
	SYSCONF_FIELD(CFG_CTRL_B);

	SYSCONF_FIELD(CFG_MONITOR_A);
	SYSCONF_FIELD(CFG_MONITOR_B);

	}

	return "???";
}

#endif

static struct platform_device stx5197_sysconf_devices[] = {
	{
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.start	= 0xfd901000,
				.end	= 0xfd90107f,
				.flags	= IORESOURCE_MEM
			}
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = HD_CFG,
					.offset = 0,
					.name = "High Density group ",
#ifdef CONFIG_PROC_FS
					.field_name =
						stx5197_sysconf_hd_field_name,
#endif
				},
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.start	= 0xfd902000,
				.end	= 0xfd90200f,
				.flags	= IORESOURCE_MEM
			}
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = HS_CFG,
					.offset = 0,
					.name = "High Speed group ",
#ifdef CONFIG_PROC_FS
					.field_name =
						stx5197_sysconf_hs_field_name,
#endif
				},
			},
		}
	},
};

static struct platform_device stpio_devices[] = {
	STPIO_DEVICE(0, 0xfd120000, ILC_IRQ(0)),
	STPIO_DEVICE(1, 0xfd121000, ILC_IRQ(1)),
	STPIO_DEVICE(2, 0xfd122000, ILC_IRQ(2)),
	STPIO_DEVICE(3, 0xfd123000, ILC_IRQ(3)),
	STPIO_DEVICE(4, 0xfd124000, ILC_IRQ(4)),
};

/* Initialise devices which are required early in the boot process. */
void __init stx5197_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stx5197_sysconf_devices,
			ARRAY_SIZE(stx5197_sysconf_devices));
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices),
			 ILC_FIRST_IRQ+ILC_NR_IRQS);

	sc = sysconf_claim(CFG_MONITOR_H, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx5197 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static void __init pio_late_setup(void)
{
	int i;
	struct platform_device *pdev = stpio_devices;

	for (i = 0; i < ARRAY_SIZE(stpio_devices); i++, pdev++)
		platform_device_register(pdev);
}

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd100000,
			.end	= 0xfd100000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx5197_postcore_setup(void)
{
	emi_init(0, 0xfde30000);

	return 0;
}
postcore_initcall(stx5197_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static int __init stx5197_subsys_setup(void)
{
	/*
	 * We need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts are
	 * available.
	 */
	pio_late_setup();

	return 0;
}
subsys_initcall(stx5197_subsys_setup);

static struct platform_device emi = STEMI();

static struct platform_device stx5197_lpc =
	STLPC_DEVICE(0xfdc00000, -1, -1, 1, 0, NULL);

static struct platform_device *stx5197_devices[] __initdata = {
	&stx5197_fdma_device,
	&stx5197_sysconf_devices[0],
	&stx5197_sysconf_devices[1],
	&ilc3_device,
	&emi,
	&stx5197_lpc,
};

#include "./platform-pm-stx5197.c"

static int __init stx5197_devices_setup(void)
{
	platform_add_pm_devices(stx5197_pm_devices,
		ARRAY_SIZE(stx5197_pm_devices));

	return platform_add_devices(stx5197_devices,
				    ARRAY_SIZE(stx5197_devices));
}
device_initcall(stx5197_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(intc_desc, "stx5197", vectors, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int i;

	register_intc_controller(&intc_desc);

	for (i = 0; i < 16; i++) {
		/*
		 * This is a hack to allow for the fact that we don't
		 * register a chip type for the IRL lines. Without
		 * this the interrupt type is "no_irq_chip" which
		 * causes problems when trying to register the chained
		 * handler.
		 */
		set_irq_chip(i, &dummy_irq_chip);

		set_irq_chained_handler(i, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
