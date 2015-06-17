/*
 * Freeman 510/520/530/540 (FLI7510/FLI7520/FLI7530/FLI7540) Setup
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/pata_platform.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/emi.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>
#include <asm/processor.h>


/* Returns: 1 if being executed on relevant chip
 *          0 otherwise */
#define FLI7510 (cpu_data->type == CPU_FLI7510)
#define FLI7520 (cpu_data->type == CPU_FLI7520)
#define FLI7530 (cpu_data->type == CPU_FLI7530)
#define FLI7540 (cpu_data->type == CPU_FLI7540)

/* Returns: 1 if being executed on the "HOST" ST40,
 *          0 if executed on the "RT" ST40 */
#define ST40HOST_CORE ((ctrl_inl(CCN_PRR) & (1 << 7)) != 0)

/* Resource definitions */
#define STM_PLAT_RESOURCE_MEM(_start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}
#define STM_PLAT_RESOURCE_MEM_NAMED(_name, _start, _size) \
		{ \
			.name = (_name), \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}
#define STM_PLAT_RESOURCE_IRQ(_irq) \
		{ \
			.start = (_irq), \
			.end = (_irq), \
			.flags = IORESOURCE_IRQ, \
		}
#define STM_PLAT_RESOURCE_IRQ_NAMED(_name, _irq) \
		{ \
			.name = (_name), \
			.start = (_irq), \
			.end = (_irq), \
			.flags = IORESOURCE_IRQ, \
		}
#define STM_PLAT_RESOURCE_DMA(_req_no) \
		{ \
			.start = (_req_no), \
			.end = (_req_no), \
			.flags = IORESOURCE_IRQ, \
		}



/* USB resources ---------------------------------------------------------- */

#define AHB2STBUS_WRAPPER_GLUE_OFFSET	0x00000
#define AHB2STBUS_OHCI_OFFSET		0xffc00
#define AHB2STBUS_EHCI_OFFSET		0xffe00
#define AHB2STBUS_PROTOCOL_OFFSET	0xfff00

static u64 st40_dma_mask = DMA_32BIT_MASK;

static struct platform_device fli7510_usb_device =
	USB_DEVICE(0, 0xfda00000 + AHB2STBUS_EHCI_OFFSET, ILC_IRQ(54),
			0xfda00000 + AHB2STBUS_OHCI_OFFSET, ILC_IRQ(55),
			0xfda00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
			0xfda00000 + AHB2STBUS_PROTOCOL_OFFSET,
			USB_FLAGS_STRAP_16BIT |
			USB_FLAGS_STRAP_PLL |
			USB_FLAGS_STBUS_CONFIG_THRESHOLD_256 |
			USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
			USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32);

static struct platform_device fli7520_usb_devices[] = {
	USB_DEVICE(0, 0xfda00000 + AHB2STBUS_EHCI_OFFSET, ILC_IRQ(54),
			0xfda00000 + AHB2STBUS_OHCI_OFFSET, ILC_IRQ(55),
			0xfda00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
			0xfda00000 + AHB2STBUS_PROTOCOL_OFFSET,
			USB_FLAGS_STRAP_8BIT |
			USB_FLAGS_STRAP_PLL |
			USB_FLAGS_STBUS_CONFIG_THRESHOLD_128 |
			USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
			USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32),
	USB_DEVICE(1, 0xfdc00000 + AHB2STBUS_EHCI_OFFSET, ILC_IRQ(56),
			0xfdc00000 + AHB2STBUS_OHCI_OFFSET, ILC_IRQ(57),
			0xfdc00000 + AHB2STBUS_WRAPPER_GLUE_OFFSET,
			0xfdc00000 + AHB2STBUS_PROTOCOL_OFFSET,
			USB_FLAGS_STRAP_8BIT |
			USB_FLAGS_STRAP_PLL |
			USB_FLAGS_STBUS_CONFIG_THRESHOLD_128 |
			USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
			USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32),
};

void __init fli7510_configure_usb(int port,
		enum fli7510_usb_ovrcur_mode ovrcur_mode)
{
	static int xtal_initialized;
	struct sysconf_field *sc;
	int clken, rstn;
	int override, ovrcur, polarity;

	BUG_ON(port < 0 || port > 1);

	BUG_ON(FLI7510 && port != 0);

	if (!xtal_initialized++) {
		sc = sysconf_claim(CFG_SPARE_1, 1, 1, "USB_xtal_valid");
		BUG_ON(!sc);
		sysconf_write(sc, FLI7510 ? 0 : 1);
	}

	switch (port) {
	case 0:
		clken = 21;
		rstn = 23;
		override = 12;
		ovrcur = 13;
		polarity = 11;
		if (FLI7510) {
			stpio_request_pin(27, 1, "USB_A_OVRCUR", STPIO_IN);
			stpio_request_pin(27, 2, "USB_A_PWREN", STPIO_ALT_OUT);
		} else {
			stpio_request_pin(26, 3, "USB_A_OVRCUR", STPIO_IN);
			stpio_request_pin(26, 4, "USB_A_PWREN", STPIO_ALT_OUT);
		}
		break;
	case 1:
		clken = 22;
		rstn = 24;
		override = 15;
		ovrcur = 16;
		polarity = 14;
		stpio_request_pin(26, 5, "USB_C_OVRCUR", STPIO_IN);
		stpio_request_pin(26, 6, "USB_C_PWREN", STPIO_ALT_OUT);
		break;
	default:
		BUG();
		return;
	}

	if (!FLI7510) {
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, clken, clken,
				"conf_usb_clk_en");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, rstn, rstn,
				"conf_usb_rst_n");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
	}

	sc = sysconf_claim(CFG_COMMS_CONFIG_1, override, override,
			"usb_enable_pad_override");
	BUG_ON(!sc);
	switch (ovrcur_mode) {
	case fli7510_usb_ovrcur_disabled:
		sysconf_write(sc, 1);
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, ovrcur, ovrcur,
				"usb_ovrcur");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		break;
	default:
		sysconf_write(sc, 0);
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, polarity, polarity,
				"usb_ovrcur_polarity");
		BUG_ON(!sc);
		switch (ovrcur_mode) {
		case fli7510_usb_ovrcur_active_high:
			sysconf_write(sc, 0);
			break;
		case fli7510_usb_ovrcur_active_low:
			sysconf_write(sc, 1);
			break;
		default:
			BUG();
			break;
		}
		break;
	}

	if (FLI7510)
		platform_device_register(&fli7510_usb_device);
	else
		platform_device_register(&fli7520_usb_devices[port]);
}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw fli7510_fdma_hw = {
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

static struct stm_plat_fdma_data fli7510_fdma_platform_data = {
	.hw = &fli7510_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define fli7510_fdma_platform_data_addr &fli7510_fdma_platform_data

#else

#define fli7510_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device fli7510_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd910000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(38)),
		},
		.dev.platform_data = fli7510_fdma_platform_data_addr,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd660000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(36)),
		},
		.dev.platform_data = fli7510_fdma_platform_data_addr,
	}
};

static struct platform_device fli7510_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd980000, 0x1000),
	},
};



/* SSC resources ---------------------------------------------------------- */

static char fli7510_i2c_dev_name[] = "i2c_st";
static char fli7510_spi_dev_name[] = "spi_st_ssc";

static struct platform_device fli7510_ssc_devices[] = {
	[0] = {
		/* .name & .id set in fli7510_configure_ssc() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb40000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(19)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 10, 2 },
				{ 10, 3 },
				{ 0xff, 0xff }
			}
		},
	},
	[1] = {
		/* .name & .id set in fli7510_configure_ssc() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb41000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(20)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 9, 4 },
				{ 9, 5 },
				{ 0xff, 0xff }
			}
		},
	},
	[2] = {
		/* .name & .id set in fli7510_configure_ssc() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb42000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(21)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				/* set in fli7510_configure_ssc() */
				{ 0xff, 0xff },
				{ 0xff, 0xff },
				{ 0xff, 0xff }
			}
		},
	},
	[3] = {
		/* .name & .id set in fli7510_configure_ssc() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb43000, 0x110),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(22)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 10, 0 },
				{ 10, 1 },
				{ 0xff, 0xff }
			}
		},
	},
	[4] = {
		/* .name & .id set in fli7510_configure_ssc() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb44000, 0x110),
			/* set in fli7510_configure_ssc() */
			STM_PLAT_RESOURCE_IRQ(-1),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				/* set in fli7510_configure_ssc() */
				{ 0xff, 0xff },
				{ 0xff, 0xff },
				{ 0xff, 0xff }
			}
		},
	},
};

void __init fli7510_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int i;
	int capability = data->capability;

	for (i = 0; i < ARRAY_SIZE(fli7510_ssc_devices); i++,
			capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc =
				fli7510_ssc_devices[i].dev.platform_data;
		struct sysconf_field *sc;
		struct resource *irq_res;

		if (capability & SSC_UNCONFIGURED)
			continue;

		switch (i) {
		case 0 ... 1:
		case 3:
			BUG_ON(capability & SSC_SPI_CAPABILITY);
			break;
		case 2:
			BUG_ON(capability & SSC_SPI_CAPABILITY);
			if (FLI7510) {
				ssc->pio[0].pio_port = 9;
				ssc->pio[0].pio_pin = 6;
				ssc->pio[1].pio_port = 9;
				ssc->pio[1].pio_pin = 7;
			} else {
				ssc->pio[0].pio_port = 26;
				ssc->pio[0].pio_pin = 7;
				ssc->pio[1].pio_port = 27;
				ssc->pio[1].pio_pin = 6;
			}
			break;
		case 4:
			/* Have to disable SPI boot controller in
			 * order to connect SSC4 to PIOs... */
			sc = sysconf_claim(CFG_COMMS_CONFIG_2, 13, 13,
					"spi_enable");
			BUG_ON(!sc);
			sysconf_write(sc, 0);

			/* Selects the SBAG alternate function on PIO21[6],
			 * PIO21[2],PIO21[3],PIO20[5] and PIO18[2:1] (Ultra)
			 * when "0" : selects MII/RMII/SPI function on
			 *            PIO21/20/18
			 * when "1" : Selects SBAG signals on PIO21/20/18 */
			sc = sysconf_claim(CFG_COMMS_CONFIG_2, 17, 17,
					"sbag_pio_alt_sel");
			BUG_ON(!sc);
			sysconf_write(sc, 0);

			/* Selects SSC4 mode (SPI backup) for PIO21[3]
			 * (SPI_MOSI)
			 * when "0" : in MTSR mode when "1" : in MRST mode */
			sc = sysconf_claim(CFG_COMMS_CONFIG_1, 1, 1,
					"ssc4_sel_mtsrn_mrst");
			BUG_ON(!sc);
			sysconf_write(sc, 0);

			irq_res = &fli7510_ssc_devices[4].resource[1];
			if (FLI7510) {
				irq_res->start = ILC_IRQ(23);
				irq_res->end = ILC_IRQ(23);
				ssc->pio[0].pio_port = 17;
				ssc->pio[0].pio_pin = 2;
				ssc->pio[1].pio_port = 17;
				ssc->pio[1].pio_pin = 3;
				ssc->pio[2].pio_port = 17;
				ssc->pio[2].pio_pin = 5;
			} else {
				irq_res->start = ILC_IRQ(47);
				irq_res->end = ILC_IRQ(47);
				ssc->pio[0].pio_port = 21;
				ssc->pio[0].pio_pin = 2;
				ssc->pio[1].pio_port = 21;
				ssc->pio[1].pio_pin = 3;
				ssc->pio[2].pio_port = 20;
				ssc->pio[2].pio_pin = 5;
			}
			break;
		}

		if (capability & SSC_SPI_CAPABILITY) {
			fli7510_ssc_devices[i].name = fli7510_spi_dev_name;
			fli7510_ssc_devices[i].id = num_spi++;
			ssc->chipselect = data->spi_chipselects[i];
		} else {
			/* I2C bus number reservation (to prevent any hot-plug
			 * device from using it) */
#ifdef CONFIG_I2C_BOARDINFO
			i2c_register_board_info(num_i2c, NULL, 0);
#endif
			fli7510_ssc_devices[i].name = fli7510_i2c_dev_name;
			fli7510_ssc_devices[i].id = num_i2c++;
			if (capability & SSC_I2C_CLK_UNIDIR)
				ssc->clk_unidir = 1;
		}

		platform_device_register(&fli7510_ssc_devices[i]);
	}
}



/* PATA resources --------------------------------------------------------- */

/*
 * EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0
 */

static struct resource fli7510_pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start = (1 << 20),
		.end = (1 << 20) + (8 << 17) - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start = (1 << 21) + (6 << 17),
		.end = (1 << 21) + (6 << 17) + 3,
		.flags = IORESOURCE_MEM,
	},
	[2] = {	/* IRQ */
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device fli7510_pata_device = {
	.name = "pata_platform",
	.id = -1,
	.num_resources = ARRAY_SIZE(fli7510_pata_resources),
	.resource = fli7510_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	}
};

void __init fli7510_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base = emi_bank_base(bank);

	emi_config_pata(bank, pc_mode);

	fli7510_pata_resources[0].start += bank_base;
	fli7510_pata_resources[0].end += bank_base;
	fli7510_pata_resources[1].start += bank_base;
	fli7510_pata_resources[1].end += bank_base;
	fli7510_pata_resources[2].start = irq;
	fli7510_pata_resources[2].end = irq;

	platform_device_register(&fli7510_pata_device);
}



/* Ethernet MAC resources ------------------------------------------------- */

static void fli7510_gmac_fix_speed(void *priv, unsigned int speed)
{
	struct sysconf_field *sc = priv;

	sysconf_write(sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data fli7510_gmac_private_data = {
	.bus_id = 0,
	.pbl = 32,
	.has_gmac = 1,
	.fix_mac_speed = fli7510_gmac_fix_speed,
};

static struct platform_device fli7510_gmac_device = {
	.name = "stmmaceth",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd920000, 0x8000),
		STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(40)),
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &fli7510_gmac_private_data,
	}
};

struct fli7510_gmac_pin {
	unsigned char port, pin, dir;
	char *name;
};

static struct fli7510_gmac_pin *fli7510_gmac_get_pin(
		struct fli7510_gmac_pin *pins, int pins_num, char *name)
{
	int i;

	for (i = 0; i < pins_num; i++)
		if (pins[i].name && strcmp(pins[i].name, name) == 0)
			return &pins[i];

	return NULL;
}

static struct fli7510_gmac_pin fli7510_gmac_mii_pins[] __initdata = {
	{ 18, 0, STPIO_ALT_OUT },	/* MDC */
	{ 18, 1, STPIO_IN },		/* COL */
	{ 18, 2, STPIO_IN },		/* CRS */
	{ 18, 3, STPIO_IN },		/* MDINT */
	{ 18, 4, STPIO_ALT_BIDIR },	/* MDIO */
	{ 18, 5, -1, "PHYCLK" },	/* PHYCLK */
	{ 20, 0, STPIO_ALT_OUT },	/* TXD[0] */
	{ 20, 1, STPIO_ALT_OUT },	/* TXD[1] */
	{ 20, 2, STPIO_ALT_OUT },	/* TXD[2] */
	{ 20, 3, STPIO_ALT_OUT },	/* TXD[3] */
	{ 20, 4, STPIO_ALT_OUT },	/* TXEN */
	{ 20, -1, STPIO_IN, "TXCLK" },	/* TXCLK */
	{ 21, 0, STPIO_IN },		/* RXD[0] */
	{ 21, 1, STPIO_IN },		/* RXD[1] */
	{ 21, 2, STPIO_IN },		/* RXD[2] */
	{ 21, 3, STPIO_IN },		/* RXD[3] */
	{ 21, 4, STPIO_IN },		/* RXDV */
	{ 21, 5, STPIO_IN },		/* RX_ER */
	{ 21, 6, STPIO_IN },		/* RXCLK */
};

static struct fli7510_gmac_pin fli7510_gmac_gmii_pins[] __initdata = {
	{ 18, 0, STPIO_ALT_OUT },	/* MDC */
	{ 18, 1, STPIO_IN },		/* COL */
	{ 18, 2, STPIO_IN },		/* CRS */
	{ 18, 3, STPIO_IN },		/* MDINT */
	{ 18, 4, STPIO_ALT_BIDIR },	/* MDIO */
	{ 18, 5, -1, "PHYCLK" },	/* PHYCLK */
	{ 20, 0, STPIO_ALT_OUT },	/* TXD[0] */
	{ 20, 1, STPIO_ALT_OUT },	/* TXD[1] */
	{ 20, 2, STPIO_ALT_OUT },	/* TXD[2] */
	{ 20, 3, STPIO_ALT_OUT },	/* TXD[3] */
	{ 24, 4, STPIO_ALT_OUT },	/* TXD[4] */
	{ 24, 5, STPIO_ALT_OUT },	/* TXD[5] */
	{ 24, 6, STPIO_ALT_OUT },	/* TXD[6] */
	{ 24, 7, STPIO_ALT_OUT },	/* TXD[7] */
	{ 20, 4, STPIO_ALT_OUT },	/* TXEN */
	{ 20, -1, STPIO_IN, "TXCLK" },	/* TXCLK */
	{ 21, 0, STPIO_IN },		/* RXD[0] */
	{ 21, 1, STPIO_IN },		/* RXD[1] */
	{ 21, 2, STPIO_IN },		/* RXD[2] */
	{ 21, 3, STPIO_IN },		/* RXD[3] */
	{ 24, 0, STPIO_IN },		/* RXD[4] */
	{ 24, 1, STPIO_IN },		/* RXD[5] */
	{ 24, 2, STPIO_IN },		/* RXD[6] */
	{ 24, 3, STPIO_IN },		/* RXD[7] */
	{ 21, 4, STPIO_IN },		/* RXDV */
	{ 21, 5, STPIO_IN },		/* RX_ER */
	{ 21, 6, STPIO_IN },		/* RXCLK */
};

static struct fli7510_gmac_pin fli7510_gmac_rmii_pins[] __initdata = {
	{ 18, 5, -1, "PHYCLK" },	/* PHYCLK */
	{ 18, 0, STPIO_ALT_OUT },	/* MDC */
	{ 18, 3, STPIO_IN },		/* MDINT */
	{ 18, 4, STPIO_ALT_BIDIR },	/* MDIO */
	{ 20, 0, STPIO_ALT_OUT },	/* TXD[0] */
	{ 20, 1, STPIO_ALT_OUT },	/* TXD[1] */
	{ 20, 4, STPIO_ALT_OUT },	/* TXEN */
	{ 21, 0, STPIO_IN },		/* RXD[0] */
	{ 21, 1, STPIO_IN },		/* RXD[1] */
	{ 21, 4, STPIO_IN },		/* RXDV */
	{ 21, 5, STPIO_IN },		/* RX_ER */
};

static struct fli7510_gmac_pin fli7510_gmac_reverse_mii_pins[] __initdata = {
	{ 18, 0, STPIO_IN },		/* MDC */
	{ 18, 1, STPIO_ALT_OUT },	/* COL */
	{ 18, 2, STPIO_ALT_OUT },	/* CRS */
	{ 18, 3, STPIO_IN },		/* MDINT */
	{ 18, 4, STPIO_ALT_BIDIR },	/* MDIO */
	{ 18, 5, -1, "PHYCLK" },	/* PHYCLK */
	{ 20, 0, STPIO_ALT_OUT },	/* TXD[0] */
	{ 20, 1, STPIO_ALT_OUT },	/* TXD[1] */
	{ 20, 2, STPIO_ALT_OUT },	/* TXD[2] */
	{ 20, 3, STPIO_ALT_OUT },	/* TXD[3] */
	{ 20, 4, STPIO_ALT_OUT },	/* TXEN */
	{ 20, -1, STPIO_IN, "TXCLK" },	/* TXCLK */
	{ 21, 0, STPIO_IN },		/* RXD[0] */
	{ 21, 1, STPIO_IN },		/* RXD[1] */
	{ 21, 2, STPIO_IN },		/* RXD[2] */
	{ 21, 3, STPIO_IN },		/* RXD[3] */
	{ 21, 4, STPIO_IN },		/* RXDV */
	{ 21, 5, STPIO_IN },		/* RX_ER */
	{ 21, 6, STPIO_IN },		/* RXCLK */
};

void __init fli7510_configure_ethernet(enum fli7510_ethernet_mode mode,
		int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;
	struct fli7510_gmac_pin *pins;
	int pins_num;
	unsigned char phy_sel, enmii;
	struct fli7510_gmac_pin *phyclk;
	int i;

	sc = sysconf_claim(CFG_COMMS_CONFIG_2, 24, 24, "gmac_enable");
	BUG_ON(!sc);
	sysconf_write(sc, 1);

	switch (mode) {
	case fli7510_ethernet_mii:
		phy_sel = 0;
		enmii = 1;
		pins = fli7510_gmac_mii_pins;
		pins_num = ARRAY_SIZE(fli7510_gmac_mii_pins);
		break;
	case fli7510_ethernet_rmii:
		phy_sel = 4;
		enmii = 1;
		pins = fli7510_gmac_rmii_pins;
		pins_num = ARRAY_SIZE(fli7510_gmac_rmii_pins);
		break;
	case fli7510_ethernet_gmii:
		BUG_ON(!FLI7510);
		phy_sel = 0;
		enmii = 1;
		pins = fli7510_gmac_gmii_pins;
		pins_num = ARRAY_SIZE(fli7510_gmac_gmii_pins);
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, 17, 18,
				"conf_pio24_alternate");
		sysconf_write(sc, 2);
		break;
	case fli7510_ethernet_reverse_mii:
		phy_sel = 0;
		enmii = 0;
		pins = fli7510_gmac_reverse_mii_pins;
		pins_num = ARRAY_SIZE(fli7510_gmac_reverse_mii_pins);
		break;
	default:
		BUG();
		return;
	}

	fli7510_gmac_private_data.bsp_priv = sysconf_claim(CFG_COMMS_CONFIG_2,
			25, 25, "gmac_mac_speed");

	sc = sysconf_claim(CFG_COMMS_CONFIG_2, 26, 28, "phy_intf_sel");
	BUG_ON(!sc);
	sysconf_write(sc, phy_sel);

	sc = sysconf_claim(CFG_COMMS_CONFIG_2, 8, 8, "gmac_mii_enable");
	BUG_ON(!sc);
	sysconf_write(sc, enmii);

	sc = sysconf_claim(CFG_COMMS_CONFIG_2, 9, 9, "gmac_phy_clock_sel");
	BUG_ON(!sc);
	phyclk = fli7510_gmac_get_pin(pins, pins_num, "PHYCLK");
	BUG_ON(!phyclk);
	if (ext_clk) {
		phyclk->dir = STPIO_IN;
		sysconf_write(sc, 1);
	} else {
		phyclk->dir = STPIO_ALT_OUT;
		sysconf_write(sc, 0);
	}

	if (mode != fli7510_ethernet_rmii) {
		struct fli7510_gmac_pin *txclk = fli7510_gmac_get_pin(pins,
				pins_num, "TXCLK");

		BUG_ON(!txclk);
		txclk->pin = FLI7510 ? 6 : 5;
	}

	for (i = 0; i < pins_num; i++)
		stpio_request_pin(pins[i].port, pins[i].pin, "stmmac",
				pins[i].dir);

	fli7510_gmac_private_data.bus_id = phy_bus;

	platform_device_register(&fli7510_gmac_device);
}



/* Audio output ----------------------------------------------------------- */

void __init fli7510_configure_audio(struct fli7510_audio_config *config)
{
	int i;

	if (!config)
		return;

	if (config->i2sa_output_mode != fli7510_audio_i2sa_output_disabled) {
		for (i = 0; i < config->i2sa_output_mode; i++)
			stpio_request_pin(6, i, "i2sa_output", STPIO_ALT_OUT);
		for (i = 4; i <= 6; i++)
			stpio_request_pin(6, i, "i2sa_output", STPIO_ALT_OUT);
	}

	if (config->i2sc_output_enabled)
		for (i = 0; i <= 3; i++)
			stpio_request_pin(8, i, "i2sc_output", STPIO_ALT_OUT);

	if (config->spdif_output_enabled)
		stpio_request_pin(6, 7, "spdif_output", STPIO_ALT_OUT);
}



/* PWM resources ---------------------------------------------------------- */

static struct platform_device fli7510_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfdb10000, 0x70),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(27)),
	},
};

void fli7510_configure_pwm(struct plat_stm_pwm_data *data)
{
	int pwm;

	fli7510_pwm_device.dev.platform_data = data;

	for (pwm = 0; pwm < 4; pwm++) {
		WARN_ON(pwm == 0 && (FLI7520 || FLI7530));
		WARN_ON(pwm == 2 && (FLI7520 || FLI7530));
		if (data->flags & (1 << pwm))
			stpio_request_pin(8, 4 + pwm, "pwm", STPIO_ALT_OUT);
	}

	platform_device_register(&fli7510_pwm_device);
}



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device fli7510_devhwrandom_device = {
	.name = "stm_hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd3e0000, 0x30),
	}
};

static struct platform_device fli7510_devrandom_device = {
	.name = "stm_rng",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd3e0000, 0x30),
	}
};



/* ASC resources ---------------------------------------------------------- */

static struct platform_device fli7510_asc_devices[] = {
	[0] = {
		.name = "stasc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb30000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(24)),
			STM_PLAT_RESOURCE_DMA(5),
			STM_PLAT_RESOURCE_DMA(6),
		},
		.dev.platform_data = &(struct stasc_uart_data) {
			.pios = {
				[0] = {	/* TXD */
					.pio_port = 9,
					.pio_pin = 3,
					.pio_direction = STPIO_ALT_OUT,
				},
				[1] = { /* RXD */
					.pio_port = 9,
					.pio_pin = 2,
					.pio_direction = STPIO_IN,
				},
				[2] = {	/* CTS */
					.pio_port = 9,
					.pio_pin = 1,
					.pio_direction = STPIO_IN,
				},
				[3] = { /* RTS */
					.pio_port = 9,
					.pio_pin = 0,
					.pio_direction = STPIO_ALT_OUT,
				},
			},
		}
	},
	[1] = {
		.name = "stasc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb31000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(25)),
			STM_PLAT_RESOURCE_DMA(7),
			STM_PLAT_RESOURCE_DMA(8),
		},
		.dev.platform_data = &(struct stasc_uart_data) {
			.pios = {
				[0] = {	/* TXD */
					.pio_port = 25,
					.pio_pin = 5,
					.pio_direction = STPIO_ALT_OUT,
				},
				[1] = { /* RXD */
					.pio_port = 25,
					.pio_pin = 4,
					.pio_direction = STPIO_IN,
				},
				[2] = {	/* CTS */
					.pio_port = 25,
					.pio_pin = 3,
					.pio_direction = STPIO_IN,
				},
				[3] = { /* RTS */
					.pio_port = 25,
					.pio_pin = 2,
					.pio_direction = STPIO_ALT_OUT,
				},
			},
		}
	},
	[2] = {
		.name = "stasc",
		/* .id set in fli7510_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfdb32000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(26)),
			STM_PLAT_RESOURCE_DMA(9),
			STM_PLAT_RESOURCE_DMA(10),
		},
		.dev.platform_data = &(struct stasc_uart_data) {
			.pios = {
				[0] = {	/* TXD */
					.pio_port = 25,
					.pio_pin = 7,
					.pio_direction = STPIO_ALT_OUT,
				},
				[1] = { /* RXD */
					.pio_port = 25,
					.pio_pin = 6,
					.pio_direction = STPIO_IN,
				},
			},
		}
	},
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[
		ARRAY_SIZE(fli7510_asc_devices)] __initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init fli7510_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;

	for (i = 0; i < num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *uart_data;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &fli7510_asc_devices[port];
		uart_data = pdev->dev.platform_data;

		BUG_ON(port == 3 && !(flags & STASC_FLAG_NORTSCTS));

		pdev->id = i;
		((struct stasc_uart_data *)(pdev->dev.platform_data))->flags =
				flags | STASC_FLAG_TXFIFO_BUG;
		stasc_configured_devices[stasc_configured_devices_count] = pdev;
		stasc_configured_devices_count++;
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init fli7510_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(fli7510_add_asc);



/* LiRC resources --------------------------------------------------------- */

static struct lirc_pio fli7510_lirc_pios[] = {
	{
		.bank = 26,
		.pin = 2,
		.pinof = 0x00 | LIRC_IR_RX | LIRC_PIO_ON,
		.dir = STPIO_IN,
	},
};

static struct plat_lirc_data fli7510_lirc_private_info = {
	/* The clock settings will be calculated by the driver
	 * from the system clock */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv	= 0, /* automatically calculate */
	.irbperiodmult	= 0,
	.irbperioddiv	= 0,
	.irbontimemult	= 0,
	.irbontimediv	= 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr  = fli7510_lirc_pios,
	.num_pio_pins = ARRAY_SIZE(fli7510_lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 1000000,
#endif
};

static struct platform_device fli7510_lirc_device = {
	.name = "lirc",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfdb18000, 0xe0),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(28)),
	},
	.dev = {
		   .power.can_wakeup = 1,
		   .platform_data = &fli7510_lirc_private_info
	}
};

void __init fli7510_configure_lirc(lirc_scd_t *scd)
{
	fli7510_lirc_private_info.scd_info = scd;

	platform_device_register(&fli7510_lirc_device);
}

void __init fli7510_configure_nand(struct platform_device *pdev)
{
	/* EMI Bank base address */
	/*  - setup done in stm_nand_emi probe */

	/* NAND Controller base address */
	pdev->resource[0].start	= 0xFD101000;
	pdev->resource[0].end	= 0xFD101FFF;

	/* NAND Controller IRQ */
	pdev->resource[1].start = ILC_IRQ(35);
	pdev->resource[1].end	= ILC_IRQ(35);

	platform_device_register(pdev);
}


/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int fli7510_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin)
{
	int result;
	int type;

	BUG_ON(!FLI7510);

	if (pin < 1 || pin > 4)
		return -1;

	type = pci_config->pci_irq[pin - 1];

	switch (type) {
	case PCI_PIN_ALTERNATIVE:
		/* Actually there are no alternative pins... */
		BUG();
		result = -1;
		break;
	case PCI_PIN_DEFAULT:
		/* Only INTA/INTB are described as "dedicated" PCI
		 * interrupts and even if these two are described as that,
		 * they are actually just "normal" external interrupt
		 * inputs (INT2 & INT3)... Additionally, depending
		 * on the spec version, the number below may seem wrong,
		 * but believe me - they are correct :-) */
		switch (pin) {
		case 1 ... 2:
			result = ILC_IRQ(119 - (pin - 1));
			/* Configure the ILC input to be active low,
			 * which is the PCI way... */
			set_irq_type(result, IRQ_TYPE_LEVEL_LOW);
			break;
		default:
			/* Other should be passed just as interrupt number
			 * (eg. result of the ILC_IRQ() macro) */
			BUG();
			result = -1;
			break;
		}
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

static struct platform_device fli7510_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("Memory", 0xc0000000,
				512 * 1024 * 1024),
		{
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("EMISS", 0xfd200000, 0x1800),
		STM_PLAT_RESOURCE_MEM_NAMED("PCI-AHB", 0xfd080000, 0x100),
		STM_PLAT_RESOURCE_IRQ_NAMED("DMA", ILC_IRQ(47)),
		STM_PLAT_RESOURCE_IRQ_NAMED("Error", ILC_IRQ(48)),
		/* Keep this one last, set in fli7510_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("SERR", -1),
	},
};

void __init fli7510_configure_pci(struct pci_config_data *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	BUG_ON(!FLI7510);

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

	/* The EMI_BUS_REQ[0] pin (also know just as EmiBusReq) is
	 * internally wired to the arbiter's PCI request 3 line.
	 * And the answer to the obvious question is: That's right,
	 * the EMI_BUSREQ[3] is not wired at all... */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Copy over platform specific data to driver */
	fli7510_pci_device.dev.platform_data = pci_conf;

	/* REQ/GNT[0] are dedicated EMI pins */
	BUG_ON(pci_conf->req_gnt[0] != PCI_PIN_DEFAULT);

	/* REQ/GNT[1..2] PIOs setup */
	for (i = 1; i <= 2; i++) {
		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			/* emiss_bus_req_enable */
			sc = sysconf_claim(CFG_COMMS_CONFIG_1,
					24 + (i - 1), 24 + (i - 1), "PCI");
			sysconf_write(sc, 1);
			stpio_request_pin(15, (i - 1) * 2, "PCI REQ",
					STPIO_IN);
			stpio_request_pin(15, ((i - 1) * 2) + 1, "PCI GNT",
					STPIO_ALT_OUT);
			break;
		case PCI_PIN_UNUSED:
			/* Unused is unused - nothing to do */
			break;
		default:
			/* No alternative here... */
			BUG();
			break;
		}
	}

	/* REG/GNT[3] are... unavailable... */
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Claim "dedicated" interrupt pins... */
	for (i = 0; i < 4; i++) {
		static const char *int_name[] = {
			"PCI INTA",
			"PCI INTB",
		};

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_DEFAULT:
			if (i > 1) {
				BUG();
				break;
			}
			stpio_request_pin(25, i, int_name[i], STPIO_IN);
			break;
		case PCI_PIN_ALTERNATIVE:
			/* No alternative here... */
			BUG();
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
	}

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (stpio_request_pin(16, 6, "PCI SERR#", STPIO_IN)) {
			pci_conf->serr_irq = gpio_to_irq(stpio_to_gpio(16, 6));
			set_irq_type(pci_conf->serr_irq, IRQ_TYPE_LEVEL_LOW);
		} else {
			printk(KERN_WARNING "%s(): Failed to claim PCI SERR# "
					"PIO!\n", __func__);
			pci_conf->serr_irq = PCI_PIN_UNUSED;
		}
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here */
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		fli7510_pci_device.num_resources--;
	} else {
		/* The SERR IRQ resource is last */
		int res_num = fli7510_pci_device.num_resources - 1;
		struct resource *res = &fli7510_pci_device.resource[res_num];

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}


#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* pci_pwr_dwn_req */
	sc = sysconf_claim(CFG_PWR_DWN_CTL, 2, 2, "PCI");
	sysconf_write(sc, 0);

	/* status_pci_pwr_dwn_grant */
	sc = sysconf_claim(CFG_PCI_ROPC_STATUS, 18, 18, "PCI");
	while (sysconf_read(sc))
		cpu_relax();

	platform_device_register(&fli7510_pci_device);
}



/* Other resources (ILC, sysconf and PIO) --------------------------------- */

/* L2-Cache is available on the FLI75[234]0 HOST core only! */
static struct platform_device fli7520_l2_cache_device = {
	.name = "stm-l2-cache",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfe600000, 0x200),
	},
};

static struct platform_device fli7510_st40host_ilc3_device = {
	.name = "ilc3",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd218000, 0x900),
	},
};

static struct platform_device fli7510_st40rt_ilc3_device = {
	.name = "ilc3",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xfd210000, 0x900),
	},
};



#ifdef CONFIG_PROC_FS

#define SYSCONF_REG(field) _SYSCONF_REG(#field, field)
#define _SYSCONF_REG(name, group, num) case num: return name

static const char *fli7510_sysconf_prb_pu_cfg_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_RESET_CTL);
	SYSCONF_REG(CFG_BOOT_CTL);
	SYSCONF_REG(CFG_SYS1);
	SYSCONF_REG(CFG_MPX_CTL);
	SYSCONF_REG(CFG_PWR_DWN_CTL);
	SYSCONF_REG(CFG_SYS2);
	SYSCONF_REG(CFG_MODE_PIN_STATUS);
	SYSCONF_REG(CFG_PCI_ROPC_STATUS);
	}

	return "???";
}

static const char *fli7510_sysconf_prb_pu_cfg_2(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_ST40_HOST_BOOT_ADDR);
	SYSCONF_REG(CFG_ST40_CTL_BOOT_ADDR);
	SYSCONF_REG(CFG_SYS10);
	SYSCONF_REG(CFG_RNG_BIST_CTL);
	SYSCONF_REG(CFG_SYS12);
	SYSCONF_REG(CFG_SYS13);
	SYSCONF_REG(CFG_SYS14);
	SYSCONF_REG(CFG_EMI_ROPC_STATUS);
	}

	return "???";
}

static const char *fli7510_sysconf_trs_spare_0(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_COMMS_CONFIG_1);
	SYSCONF_REG(CFG_TRS_CONFIG);
	SYSCONF_REG(CFG_COMMS_CONFIG_2);
	SYSCONF_REG(CFG_USB_SOFT_JTAG);
	SYSCONF_REG(CFG_TRS_SPARE_REG5_NOTUSED_0);
	SYSCONF_REG(CFG_TRS_CONFIG_2);
	SYSCONF_REG(CFG_COMMS_TRS_STATUS);
	SYSCONF_REG(CFG_EXTRA_ID1_LSB);
	}

	return "???";
}

static const char *fli7510_sysconf_trs_spare_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_SPARE_1);
	SYSCONF_REG(CFG_SPARE_2);
	SYSCONF_REG(CFG_SPARE_3);
	SYSCONF_REG(CFG_TRS_SPARE_REG4_NOTUSED);
	SYSCONF_REG(CFG_TRS_SPARE_REG5_NOTUSED_1);
	SYSCONF_REG(CFG_TRS_SPARE_REG6_NOTUSED);
	SYSCONF_REG(CFG_DEVICE_ID);
	SYSCONF_REG(CFG_EXTRA_ID1_MSB);
	}

	return "???";
}

static const char *fli7510_sysconf_vdec_pu_cfg_0(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_TOP_SPARE_REG1);
	SYSCONF_REG(CFG_TOP_SPARE_REG2);
	SYSCONF_REG(CFG_TOP_SPARE_REG3);
	SYSCONF_REG(CFG_ST231_DRA2_DEBUG);
	SYSCONF_REG(CFG_ST231_AUD1_DEBUG);
	SYSCONF_REG(CFG_ST231_AUD2_DEBUG);
	SYSCONF_REG(CFG_REG7_0);
	SYSCONF_REG(CFG_INTERRUPT);
	}

	return "???";
}

static const char *fli7510_sysconf_vdec_pu_cfg_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_ST231_DRA2_PERIPH_REG1);
	SYSCONF_REG(CFG_ST231_DRA2_BOOT_REG2);
	SYSCONF_REG(CFG_ST231_AUD1_PERIPH_REG3);
	SYSCONF_REG(CFG_ST231_AUD1_BOOT_REG4);
	SYSCONF_REG(CFG_ST231_AUD2_PERIPH_REG5);
	SYSCONF_REG(CFG_ST231_AUD2_BOOT_REG6);
	SYSCONF_REG(CFG_REG7_1);
	SYSCONF_REG(CFG_INTERRUPT_REG8);
	}

	return "???";
}

static const char *fli7510_sysconf_vout_spare(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_REG1_VOUT_PIO_ALT_SEL);
	SYSCONF_REG(CFG_REG2_VOUT_PIO_ALT_SEL);
	SYSCONF_REG(CFG_VOUT_SPARE_REG3);
	SYSCONF_REG(CFG_REG4_DAC_CTRL);
	SYSCONF_REG(CFG_REG5_VOUT_DEBUG_PAD_CTL);
	SYSCONF_REG(CFG_REG6_TVOUT_DEBUG_CTL);
	SYSCONF_REG(CFG_REG7_UNUSED);
	}

	return "???";
}

#endif

static struct platform_device fli7510_sysconf_devices[] = {
	{
		.name = "sysconf",
		.id = 0,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd220000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = PRB_PU_CFG_1,
					.offset = 0,
					.name = "PRB_PU_CFG_1",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_prb_pu_cfg_1,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 1,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd228000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = PRB_PU_CFG_2,
					.offset = 0,
					.name = "PRB_PU_CFG_2",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_prb_pu_cfg_2,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 2,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd9ec000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = TRS_SPARE_REGS_0,
					.offset = 0,
					.name = "TRS_SPARE_REGS_0",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_trs_spare_0,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 3,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd9f4000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = TRS_SPARE_REGS_1,
					.offset = 0,
					.name = "TRS_SPARE_REGS_1",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_trs_spare_1,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 4,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd7a0000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = VDEC_PU_CFG_0,
					.offset = 0,
					.name = "VDEC_PU_CFG_0",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_vdec_pu_cfg_0,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 5,
		.num_resources = 1,
		.resource = (struct resource []) {
			STM_PLAT_RESOURCE_MEM(0xfd7c0000, 0x20),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = VDEC_PU_CFG_1,
					.offset = 0,
					.name = "VDEC_PU_CFG_1",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_vdec_pu_cfg_1,
#endif
				},
			},
		}
	}, {
		.name = "sysconf",
		.id = 6,
		.num_resources = 1,
		.resource = (struct resource []) {
			{
				/* .start & .end set in
				 * fli7510_sysconf_setup() */
				.flags = IORESOURCE_MEM,
			}
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = VOUT_SPARE_REGS,
					.offset = 0,
					.name = "VOUT_SPARE_REGS",
#ifdef CONFIG_PROC_FS
					.field_name =
						fli7510_sysconf_vout_spare,
#endif
				},
			},
		}
	}
};

static void fli7510_sysconf_setup(void)
{
	struct resource *mem_res = &fli7510_sysconf_devices[6].resource[0];

	if (FLI7510)
		mem_res->start = 0xfd5e8000;
	else
		mem_res->start = 0xfd5d4000;

	mem_res->end = mem_res->start + 0x20 - 1;
}


static struct platform_device fli7510_pio_devices[] = {
	STPIO_DEVICE(0, 0xfd5c0000, ILC_IRQ(75)),
	STPIO_DEVICE(1, 0xfd5c4000, ILC_IRQ(76)),
	STPIO_DEVICE(2, 0xfd5c8000, ILC_IRQ(77)),
	STPIO_DEVICE(3, 0xfd5cc000, ILC_IRQ(78)),
	STPIO_DEVICE(4, 0xfd5d0000, ILC_IRQ(79)),
	STPIO_DEVICE(5, 0xfd5d4000, ILC_IRQ(80)),
	STPIO_DEVICE(6, 0xfd5d8000, -1),
	STPIO_DEVICE(7, 0xfd5dc000, -1),
	STPIO_DEVICE(8, 0xfd5e0000, -1),
	STPIO_DEVICE(9, 0xfd5e4000, -1),
	STPIO_DEVICE(10, 0xfd984000, ILC_IRQ(125)),
	STPIO_DEVICE(11, 0xfd988000, -1),
	STPIO_DEVICE(12, 0xfd98c000, -1),
	STPIO_DEVICE(13, 0xfd990000, ILC_IRQ(2)),
	STPIO_DEVICE(14, 0xfd994000, ILC_IRQ(3)),
	STPIO_DEVICE(15, 0xfd998000, ILC_IRQ(81)),
	STPIO_DEVICE(16, 0xfd99c000, ILC_IRQ(82)),
	STPIO_DEVICE(17, 0xfd9a0000, -1),
	STPIO_DEVICE(18, 0xfd9a4000, -1),
	STPIO_DEVICE(19, 0xfd9a8000, -1),
	STPIO_DEVICE(20, 0xfd9ac000, -1),
	STPIO_DEVICE(21, 0xfd9b0000, -1),
	STPIO_DEVICE(22, 0xfd9b4000, ILC_IRQ(83)),
	STPIO_DEVICE(23, 0xfd9b8000, ILC_IRQ(84)),
	STPIO_DEVICE(24, 0xfd9bc000, ILC_IRQ(85)),
	STPIO_DEVICE(25, 0xfd9c0000, -1),
	STPIO_DEVICE(26, 0xfd9c4000, -1),
	STPIO_DEVICE(27, 0xfd9c8000, -1),
};

static struct platform_device fli7520_pio_devices[] = {
	/* Some bits of the documentation refer to the following
	 * PIO ports 5-9 as ports 0-4... */
	STPIO_DEVICE(5, 0xfd5c0000, ILC_IRQ(75)),
	STPIO_DEVICE(6, 0xfd5c4000, ILC_IRQ(76)),
	STPIO_DEVICE(7, 0xfd5c8000, ILC_IRQ(77)),
	STPIO_DEVICE(8, 0xfd5cc000, ILC_IRQ(78)),
	STPIO_DEVICE(9, 0xfd5d0000, ILC_IRQ(79)),
	STPIO_DEVICE(10, 0xfd984000, ILC_IRQ(125)),
	STPIO_DEVICE(11, 0xfd988000, -1),
	STPIO_DEVICE(12, 0xfd98c000, -1),
	STPIO_DEVICE(13, 0xfd990000, ILC_IRQ(2)),
	STPIO_DEVICE(14, 0xfd994000, ILC_IRQ(3)),
	STPIO_DEVICE(15, 0xfd998000, ILC_IRQ(81)),
	STPIO_DEVICE(16, 0xfd99c000, ILC_IRQ(82)),
	STPIO_DEVICE(17, 0xfd9a0000, -1),
	STPIO_DEVICE(18, 0xfd9a4000, -1),
	STPIO_DEVICE(19, 0xfd9a8000, -1),
	STPIO_DEVICE(20, 0xfd9ac000, -1),
	STPIO_DEVICE(21, 0xfd9b0000, -1),
	STPIO_DEVICE(22, 0xfd9b4000, ILC_IRQ(83)),
	STPIO_DEVICE(23, 0xfd9b8000, ILC_IRQ(84)),
	STPIO_DEVICE(24, 0xfd9bc000, ILC_IRQ(85)),
	STPIO_DEVICE(25, 0xfd9c0000, -1),
	STPIO_DEVICE(26, 0xfd9c4000, -1),
	STPIO_DEVICE(27, 0xfd9c8000, -1),
	STPIO_DEVICE(28, 0xfd9cc000, -1),
	STPIO_DEVICE(29, 0xfd9d0000, -1),
};



/* Early devices initialization ------------------------------------------- */

/* Initialise devices which are required early in the boot process
 * (called from the board setup file) */
void __init fli7510_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long verid;
	char *chip_variant;
	unsigned long devid;
	unsigned long chip_revision;

	verid = *((unsigned *)0xfd9e9078) >> 16;

	if (FLI7510) {
		if (verid != 0x1d56)
			printk(KERN_WARNING "Wrong chip variant data, "
					"assuming FLI7510!\n");
		chip_variant = "510";
	} else {
		/* CPU should be detected as 520 so far... */
		WARN_ON(!CPU_FLI7520);

		switch (verid) {
		case 0x1d60:
			boot_cpu_data.type = CPU_FLI7520;
			chip_variant = "520";
			break;
		case 0x1d6a:
			boot_cpu_data.type = CPU_FLI7530;
			chip_variant = "530";
			break;
		case 0x1d74:
			boot_cpu_data.type = CPU_FLI7540;
			chip_variant = "540";
			break;
		default:
			printk(KERN_WARNING "Wrong chip variant data, "
					"assuming FLI7540!\n");
			boot_cpu_data.type = CPU_FLI7540;
			chip_variant = "520/530/540";
			break;
		}
	}

	/* Initialise PIO and sysconf drivers */

	fli7510_sysconf_setup();
	sysconf_early_init(fli7510_sysconf_devices,
			ARRAY_SIZE(fli7510_sysconf_devices));

	if (FLI7510)
		stpio_early_init(fli7510_pio_devices,
				ARRAY_SIZE(fli7510_pio_devices),
				ILC_FIRST_IRQ + ILC_NR_IRQS);
	else
		stpio_early_init(fli7520_pio_devices,
				ARRAY_SIZE(fli7520_pio_devices),
				ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(CFG_DEVICE_ID, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28);
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "Freeman %s version %ld.x, ST40%s core\n",
			chip_variant, chip_revision,
			ST40HOST_CORE ? "HOST" : "RT");

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

/* EMI access is required very early */
static int __init fli7510_postcore_setup(void)
{
	int result = 0;
	int i;

	emi_init(0, 0xfd100000);

	if (FLI7510) {
		for (i = 0; i < ARRAY_SIZE(fli7510_pio_devices) &&
				result == 0; i++)
			result |= platform_device_register(
					&fli7510_pio_devices[i]);
	} else {
		result = platform_device_register(&fli7520_l2_cache_device);
		for (i = 0; i < ARRAY_SIZE(fli7520_pio_devices) &&
				result == 0; i++)
			result |= platform_device_register(
					&fli7520_pio_devices[i]);
	}

	return result;
}
postcore_initcall(fli7510_postcore_setup);



/* Late resources --------------------------------------------------------- */

static struct platform_device *fli7510_late_devices[] __initdata = {
	&fli7510_fdma_devices[0],
	&fli7510_fdma_devices[1],
	&fli7510_fdma_xbar_device,
	&fli7510_sysconf_devices[0],
	&fli7510_sysconf_devices[1],
	&fli7510_sysconf_devices[2],
	&fli7510_sysconf_devices[3],
	&fli7510_sysconf_devices[4],
	&fli7510_sysconf_devices[5],
	&fli7510_sysconf_devices[6],
	&fli7510_devhwrandom_device,
	&fli7510_devrandom_device,
};

static int __init fli7510_devices_setup(void)
{
	int result;

	if (ST40HOST_CORE)
		result = platform_device_register(
				&fli7510_st40host_ilc3_device);
	else
		result = platform_device_register(&fli7510_st40rt_ilc3_device);
	if (result != 0)
		return result;

	return platform_add_devices(fli7510_late_devices,
				    ARRAY_SIZE(fli7510_late_devices));
}
device_initcall(fli7510_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,
};

static struct intc_vect fli7510_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg fli7510_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(fli7510_intc_desc, "fli7510",
		fli7510_intc_vectors, NULL,
		NULL, fli7510_intc_prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int irq;

	register_intc_controller(&fli7510_intc_desc);

	/* The ILC outputs (16 lines) are connected to the IRL0..3
	 * INTC inputs via priority level encoder */
	for (irq = 0; irq < 15; irq++) {
		set_irq_chip(irq, &dummy_irq_chip);
		set_irq_chained_handler(irq, ilc_irq_demux);
	}

	if (ST40HOST_CORE)
		ilc_early_init(&fli7510_st40host_ilc3_device);
	else
		ilc_early_init(&fli7510_st40rt_ilc3_device);

	ilc_demux_init();
}
