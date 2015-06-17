/*
 * STx7108 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <asm/irq-ilc.h>
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
#include <linux/mtd/partitions.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>


/* Helpers */

/* Returns: 1 if being executed on the L2-cached ST40,
 *          0 if executed on the "RT" ST40 */
#define HOST_CORE ((ctrl_inl(CCN_PRR) & (1 << 7)) == 0)

/* Resource definitions */
#define RESOURCE_MEM(_start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}
#define RESOURCE_MEM_NAMED(_name, _start, _size) \
		{ \
			.name = (_name), \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}
#define RESOURCE_IRQ(_irq) \
		{ \
			.start = (_irq), \
			.end = (_irq), \
			.flags = IORESOURCE_IRQ, \
		}
#define RESOURCE_IRQ_NAMED(_name, _irq) \
		{ \
			.name = (_name), \
			.start = (_irq), \
			.end = (_irq), \
			.flags = IORESOURCE_IRQ, \
		}
#define RESOURCE_DMA(_req_no) \
		{ \
			.start = (_req_no), \
			.end = (_req_no), \
			.flags = IORESOURCE_IRQ, \
		}



/* PIO alternative functions configuration -------------------------------- */

/* Function selector */

static void stx7108_pioalt_select(int port, int pin, int alt, const char *name)
{
	struct sysconf_field *sc;
	int group, num;

	pr_debug("%s(port=%d, pin=%d, alt=%d, name='%s')\n",
			__func__, port, pin, alt, name);

	BUG_ON(pin < 0 || pin > 7);
	BUG_ON(alt < 0 || alt > 5);

	switch (port) {
	case 0 ... 14:
		group = SYS_CFG_BANK2;
		num = port;
		break;
	case 15 ... 26:
		group = SYS_CFG_BANK4;
		num = port - 15;
		break;
	default:
		BUG();
		return;
	}

	sc = sysconf_claim(group, num, pin * 4, (pin * 4) + 3, name);

	if (alt == 0) {
		/* Alternative function 0 is a generic I/O mode, in which
		 * case the PIO driver will be in charge (unless the pin
		 * was previously claimed by some alternative...) */
		if (sc) {
			sysconf_write(sc, 0);
			/* Release the relevant sysconf bits now, as
			 * they may be claimed by someone else later */
			sysconf_release(sc);
		}
	} else {
		BUG_ON(!sc);
		sysconf_write(sc, alt);
	}
}

/* Pad configuration */

struct stx7108_pioalt_pad_cfg {
	int oe:2;
	int pu:2;
	int od:2;
};

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_in __initdata = {
	.oe = 0,
	.pu = 0,
	.od = 0,
};
#define IN (&stx7108_pioalt_pad_in)

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_out __initdata = {
	.oe = 1,
	.pu = 0,
	.od = 0,
};
#define OUT (&stx7108_pioalt_pad_out)

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_od __initdata = {
	.oe = 1,
	.pu = 0,
	.od = 1,
};
#define OD (&stx7108_pioalt_pad_od)

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_bidir __initdata = {
	.oe = -1,
	.pu = 0,
	.od = 0,
};
#define BIDIR (&stx7108_pioalt_pad_bidir)


static void stx7108_pioalt_pad(int port, int pin,
		struct stx7108_pioalt_pad_cfg *cfg, const char *name)
{
	struct sysconf_field *sc;
	int group, num, bit;

	pr_debug("%s(port=%d, pin=%d, oe=%d, pu=%d, od=%d, name='%s')\n",
			__func__, port, pin, cfg->oe, cfg->pu, cfg->od, name);

	BUG_ON(pin < 0 || pin > 7);
	BUG_ON(!cfg);

	switch (port) {
	case 0 ... 14:
		group = SYS_CFG_BANK2;
		num = 15 + (port / 4);
		break;
	case 15 ... 26:
		group = SYS_CFG_BANK4;
		port -= 15;
		num = 12 + (port / 4);
		break;
	default:
		BUG();
		return;
	}

	bit = ((port * 8) + pin) % 32;

	if (cfg->oe >= 0) {
		sc = sysconf_claim(group, num, bit, bit, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->oe);
	}

	if (cfg->pu >= 0) {
		sc = sysconf_claim(group, num + 4, bit, bit, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->pu);
	}

	if (cfg->od >= 0) {
		sc = sysconf_claim(group, num + 8, bit, bit, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->od);
	}
}

/* PIO retiming setup */

/* Structure aligned to the "STi7108 Generic Retime Padlogic
 * Application Note" SPEC */
struct stx7108_pioalt_retime_cfg {
	int retime:2;
	int clk1notclk0:2;
	int clknotdata:2;
	int double_edge:2;
	int invertclk:2;
	int delay_input:2;
};

static void stx7108_pioalt_retime(int port, int pin,
		struct stx7108_pioalt_retime_cfg *cfg, const char *name)
{
	struct sysconf_field *sc;
	int group, num;

	pr_debug("%s(port=%d, pin=%d, retime=%d, clk1notclk0=%d, "
			"clknotdata=%d, double_edge=%d, invertclk=%d, "
			"delay_input=%d, name='%s')\n", __func__, port, pin,
			cfg->retime, cfg->clk1notclk0, cfg->clknotdata,
			cfg->double_edge, cfg->invertclk, cfg->delay_input,
			name);

	BUG_ON(pin < 0 || pin > 7);
	BUG_ON(!cfg);

	switch (port) {
	case 0 ... 14:
		group = SYS_CFG_BANK2;
		switch (port) {
		case 1:
			num = 32;
			break;
		case 6 ... 14:
			num = 34 + ((port - 6) * 2);
			break;
		default:
			BUG();
			return;
		}
		break;
	case 15 ... 26:
		group = SYS_CFG_BANK4;
		if (port > 23) {
			BUG();
			return;
		}
		num = 48 + ((port - 15) * 2);
		break;
	default:
		BUG();
		return;
	}

	if (cfg->clk1notclk0 >= 0) {
		sc = sysconf_claim(group, num, 0 + pin, 0 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->clk1notclk0);
	}

	if (cfg->clknotdata >= 0) {
		sc = sysconf_claim(group, num,  8 + pin, 8 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->clknotdata);
	}

	if (cfg->delay_input >= 0) {
		sc = sysconf_claim(group, num, 16 + pin, 16 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->delay_input);
	}

	if (cfg->double_edge >= 0) {
		sc = sysconf_claim(group, num, 24 + pin, 24 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->double_edge);
	}

	if (cfg->invertclk >= 0) {
		sc = sysconf_claim(group, num + 1, 0 + pin, 0 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->invertclk);
	}

	if (cfg->retime >= 0) {
		sc = sysconf_claim(group, num + 1, 8 + pin, 8 + pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, cfg->retime);
	}
}



/* USB resources ---------------------------------------------------------- */

static u64 stx7108_usb_dma_mask = DMA_32BIT_MASK;

static struct platform_device stx7108_usb_devices[] = {
	{
		.name = "st-usb",
		.id = 0,
		.num_resources = 6,
		.resource = (struct resource []) {
			RESOURCE_MEM_NAMED("wrapper",  0xfe000000, 0x100),
			RESOURCE_MEM_NAMED("ohci",     0xfe0ffc00, 0x100),
			RESOURCE_MEM_NAMED("ehci",     0xfe0ffe00, 0x100),
			RESOURCE_MEM_NAMED("protocol", 0xfe0fff00, 0x100),
			RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(59)),
			RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(62)),
		},
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &(struct plat_usb_data) {
				/* .flags set in stx7108_configure_usb() */
			},
		}
	}, {
		.name = "st-usb",
		.id = 1,
		.num_resources = 6,
		.resource = (struct resource []) {
			RESOURCE_MEM_NAMED("wrapper",  0xfe100000, 0x100),
			RESOURCE_MEM_NAMED("ohci",     0xfe1ffc00, 0x100),
			RESOURCE_MEM_NAMED("ehci",     0xfe1ffe00, 0x100),
			RESOURCE_MEM_NAMED("protocol", 0xfe1fff00, 0x100),
			RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(60)),
			RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(63)),
		},
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &(struct plat_usb_data) {
				/* .flags set in stx7108_configure_usb() */
			},
		}
	}, {
		.name = "st-usb",
		.id = 2,
		.num_resources = 6,
		.resource = (struct resource []) {
			RESOURCE_MEM_NAMED("wrapper",  0xfe200000, 0x100),
			RESOURCE_MEM_NAMED("ohci",     0xfe2ffc00, 0x100),
			RESOURCE_MEM_NAMED("ehci",     0xfe2ffe00, 0x100),
			RESOURCE_MEM_NAMED("protocol", 0xfe2fff00, 0x100),
			RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(61)),
			RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(64)),
		},
		.dev = {
			.dma_mask = &stx7108_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &(struct plat_usb_data) {
				/* .flags set in stx7108_configure_usb() */
			},
		}
	}
};

void __init stx7108_configure_usb(int port)
{
	static int initialized;
	struct sysconf_field *sc;
	const struct {
		struct {
			unsigned char port, pin, alt;
		} oc, pwr;
	} usb_pins[] = {
		{ .oc = { 23, 6, 1 }, .pwr = { 23, 7, 1 } },
		{ .oc = { 24, 0, 1 }, .pwr = { 24, 1, 1 } },
		{ .oc = { 24, 2, 1 }, .pwr = { 24, 3, 1 } },
	};
	struct plat_usb_data *plat_data;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7108_usb_devices));

	if (!initialized) {
		/* USB2TRIPPHY_OSCIOK */
		sc = sysconf_claim(SYS_CFG_BANK4, 44, 6, 6, "USB");
		sysconf_write(sc, 1);
		initialized = 1;
	}

	plat_data = stx7108_usb_devices[port].dev.platform_data;
	if (cpu_data->cut_major < 2)
		plat_data->flags = USB_FLAGS_STRAP_8BIT |
				USB_FLAGS_STBUS_CONFIG_OPCODE_LD64_ST64 |
				USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_1 |
				USB_FLAGS_STBUS_CONFIG_THRESHOLD_64;
	else
		plat_data->flags = USB_FLAGS_STRAP_8BIT |
				USB_FLAGS_STBUS_CONFIG_OPCODE_LD64_ST64 |
				USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_2 |
				USB_FLAGS_STBUS_CONFIG_THRESHOLD_128;

	/* Power up USB */
#if !defined(CONFIG_PM)
	sc = sysconf_claim(SYS_CFG_BANK4, 46, port, port, "USB");
	sysconf_write(sc, 0);
	sc = sysconf_claim(SYS_STA_BANK4, 2, port, port, "USB");
	while (sysconf_read(sc))
		cpu_relax();
#endif

	stx7108_pioalt_select(usb_pins[port].pwr.port,
			    usb_pins[port].pwr.pin,
			    usb_pins[port].pwr.alt, "USB");
	stx7108_pioalt_pad(usb_pins[port].pwr.port,
			  usb_pins[port].pwr.pin, OUT, "USB");

	stx7108_pioalt_select(usb_pins[port].oc.port,
			    usb_pins[port].oc.pin,
			    usb_pins[port].oc.alt, "USB");
	stx7108_pioalt_pad(usb_pins[port].oc.port,
			  usb_pins[port].oc.pin, IN, "USB");

	platform_device_register(&stx7108_usb_devices[port]);
}



/* FDMA resources --------------------------------------------------------- */

#if defined(CONFIG_STM_DMA)

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw stx7108_fdma_hw = {
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

static struct stm_plat_fdma_data stx7108_fdma_platform_data = {
	.hw = &stx7108_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7108_fdma_platform_data_addr (&stx7108_fdma_platform_data)

#else

#define stx7108_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7108_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfda00000, 0x10000),
			RESOURCE_IRQ(ILC_IRQ(27)),
		},
		.dev.platform_data = stx7108_fdma_platform_data_addr,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfda10000, 0x10000),
			RESOURCE_IRQ(ILC_IRQ(29)),
		},
		.dev.platform_data = stx7108_fdma_platform_data_addr,
	}, {
		.name = "stm-fdma",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfda20000, 0x10000),
			RESOURCE_IRQ(ILC_IRQ(31)),
		},
		.dev.platform_data = stx7108_fdma_platform_data_addr,
	}
};

static struct platform_device stx7108_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		RESOURCE_MEM(0xfdabb000, 0x1000),
	},
};



/* SSC resources ---------------------------------------------------------- */

static struct platform_device stx7108_ssc_devices[] = {
	{ /* SSC0 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd740000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(33)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 1, 6 }, /* SCLK */
				{ 1, 7 }, /* MTSR */
				{ 2, 0 }, /* MRST */
			},
		}
	}, { /* SSC1 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd741000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(34)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 9, 6 }, /* SCLK */
				{ 9, 7 }, /* MTSR */
				{ 9, 5 }, /* MRST */
			},
		}
	}, { /* SSC2 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd742000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(35)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			/* .pio set in stx7108_configure_ssc_*() */
		},
	}, { /* SSC3 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd743000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(36)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 5, 2 }, /* SCLK */
				{ 5, 3 }, /* MTSR */
				{ 5, 4 }, /* MRST */
			},
		},
	}, { /* SSC4 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd744000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(37)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 13, 6 }, /* SCLK */
				{ 13, 7 }, /* MTSR */
				{ 14, 0 }, /* MRST */
			},
		},
	}, { /* SSC5 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd745000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(38)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 5, 6 }, /* SCLK */
				{ 5, 7 }, /* MTSR */
				{ 5, 5 }, /* MRST */
			},
		},
	}, { /* SSC6 */
		/* .name & .id set in stx7108_configure_ssc_*() */
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfd746000, 0x110),
			RESOURCE_IRQ(ILC_IRQ(39)),
		},
		.dev.platform_data = &(struct ssc_pio_t) {
			.pio = {
				{ 15, 2 }, /* SCLK */
				{ 15, 3 }, /* MTSR */
				{ 15, 4 }, /* MRST */
			},
		},
	}
};

static void __init stx7108_ssc2_set_pios(struct stx7108_ssc_config *config,
		struct ssc_pio_t *plat_data)
{
	switch (config->routing.ssc2.sclk) {
	case stx7108_ssc2_sclk_pio1_3:
		plat_data->pio[0].pio_port = 1;
		plat_data->pio[0].pio_pin = 3;
		break;
	case stx7108_ssc2_sclk_pio14_4:
		plat_data->pio[0].pio_port = 14;
		plat_data->pio[0].pio_pin = 4;
		break;
	default:
		BUG();
		break;
	}

	switch (config->routing.ssc2.mtsr) {
	case stx7108_ssc2_mtsr_pio1_4:
		plat_data->pio[1].pio_port = 1;
		plat_data->pio[1].pio_pin = 4;
		break;
	case stx7108_ssc2_mtsr_pio14_5:
		plat_data->pio[1].pio_port = 14;
		plat_data->pio[1].pio_pin = 5;
		break;
	default:
		BUG();
		break;
	}

	switch (config->routing.ssc2.mrst) {
	case stx7108_ssc2_mrst_pio1_5:
		plat_data->pio[2].pio_port = 1;
		plat_data->pio[2].pio_pin = 5;
		break;
	case stx7108_ssc2_mrst_pio14_6:
		plat_data->pio[2].pio_port = 14;
		plat_data->pio[2].pio_pin = 6;
		break;
	default:
		BUG();
		break;
	}
}

static int __initdata stx7108_ssc_alt_funcs[] = { 2, 1, 2, 2, 1, 2, 1 };

static int __initdata stx7108_ssc_configured[ARRAY_SIZE(stx7108_ssc_devices)];

int __init stx7108_configure_ssc_i2c(int ssc, struct stx7108_ssc_config *config)
{
	static int i2c_busnum;
	struct stx7108_ssc_config default_config = {};
	struct ssc_pio_t *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7108_ssc_devices));
	BUG_ON(stx7108_ssc_configured[ssc]);
	stx7108_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7108_ssc_devices[ssc].name = 
		config->i2c_pio ? "i2c_stm_pio" : "i2c_stm_ssc";
	stx7108_ssc_devices[ssc].id = i2c_busnum;

	plat_data = stx7108_ssc_devices[ssc].dev.platform_data;
	plat_data->clk_unidir = config->clk_unidir;
	if (ssc == 2)
		stx7108_ssc2_set_pios(config, plat_data);

	if (!config->i2c_pio) {
		/* SCLK used as I2C SCL */
		stx7108_pioalt_select(plat_data->pio[0].pio_port,
			plat_data->pio[0].pio_pin,
			stx7108_ssc_alt_funcs[ssc], "ssc)");
		stx7108_pioalt_pad(plat_data->pio[0].pio_port,
			plat_data->pio[0].pio_pin,
			config->clk_unidir ? OUT : OD, "ssc");

		/* MTSR used as I2C SDA */
		stx7108_pioalt_select(plat_data->pio[1].pio_port,
			plat_data->pio[1].pio_pin,
			stx7108_ssc_alt_funcs[ssc], "ssc)");
		stx7108_pioalt_pad(plat_data->pio[1].pio_port,
			plat_data->pio[1].pio_pin, OD, "ssc");
	}

#if defined(CONFIG_I2C_BOARDINFO)
	/* I2C bus number reservation (to prevent any hot-plug
	 * device from using it) */
	i2c_register_board_info(i2c_busnum, NULL, 0);
#endif

	platform_device_register(&stx7108_ssc_devices[ssc]);

	return i2c_busnum++;
}

int __init stx7108_configure_ssc_spi(int ssc, struct stx7108_ssc_config *config)
{
	static int spi_busnum;
	struct stx7108_ssc_config default_config = {};
	struct ssc_pio_t *plat_data;

	BUG_ON(ssc < 0 || ssc >= ARRAY_SIZE(stx7108_ssc_devices));
	BUG_ON(stx7108_ssc_configured[ssc]);
	stx7108_ssc_configured[ssc] = 1;

	if (!config)
		config = &default_config;

	stx7108_ssc_devices[ssc].name = "spi_st_ssc";
	stx7108_ssc_devices[ssc].id = spi_busnum;

	plat_data = stx7108_ssc_devices[ssc].dev.platform_data;
	plat_data->chipselect = config->spi_chipselect;
	if (ssc == 2)
		stx7108_ssc2_set_pios(config, plat_data);

	/* SCLK used as SPI SCK */
	stx7108_pioalt_select(plat_data->pio[0].pio_port,
			plat_data->pio[0].pio_pin,
			stx7108_ssc_alt_funcs[ssc], "ssc)");
	stx7108_pioalt_pad(plat_data->pio[0].pio_port,
			plat_data->pio[0].pio_pin,
			OUT, "ssc");

	/* MTSR used as SPI MOSI */
	stx7108_pioalt_select(plat_data->pio[1].pio_port,
			plat_data->pio[1].pio_pin,
			stx7108_ssc_alt_funcs[ssc], "ssc)");
	stx7108_pioalt_pad(plat_data->pio[1].pio_port,
			plat_data->pio[1].pio_pin, OUT, "ssc");

	/* MRST used as SPI MISO */
	stx7108_pioalt_select(plat_data->pio[2].pio_port,
			plat_data->pio[2].pio_pin,
			stx7108_ssc_alt_funcs[ssc], "ssc)");
	stx7108_pioalt_pad(plat_data->pio[2].pio_port,
			plat_data->pio[2].pio_pin, IN, "ssc");

	platform_device_register(&stx7108_ssc_devices[ssc]);

	return spi_busnum++;
}



/* SATA resources --------------------------------------------------------- */

static struct plat_sata_data stx7108_sata_private_info = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
};

static struct platform_device stx7108_sata_devices[] = {
	SATA_DEVICE(0, 0xfe768000, ILC_IRQ(57), ILC_IRQ(55),
		    &stx7108_sata_private_info),
	SATA_DEVICE(1, 0xfe769000, ILC_IRQ(58), ILC_IRQ(56),
		    &stx7108_sata_private_info),
};

void __init stx7108_configure_sata(int port)
{
	static int initialized;

	BUG_ON(port < 0 || port > 1);

	if (!initialized) {
		struct stm_miphy_sysconf_soft_jtag jtag;
		struct stm_miphy miphy = {
			.ports_num = 2,
			.jtag_tick = stm_miphy_sysconf_jtag_tick,
			.jtag_priv = &jtag,
		};
		struct sysconf_field *sc;

		jtag.tck = sysconf_claim(SYS_CFG_BANK1, 3, 20, 20, "SATA");
		BUG_ON(!jtag.tck);
		jtag.tms = sysconf_claim(SYS_CFG_BANK1, 3, 23, 23, "SATA");
		BUG_ON(!jtag.tms);
		jtag.tdi = sysconf_claim(SYS_CFG_BANK1, 3, 22, 22, "SATA");
		BUG_ON(!jtag.tdi);
		jtag.tdo = sysconf_claim(SYS_STA_BANK1, 4, 1, 1, "SATA");
		BUG_ON(!jtag.tdo);

		/* Shut down both PHYs first, using SATA_x_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 3, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 3);
		sysconf_release(sc);

		/* conf_sata_tap_en */
		sc = sysconf_claim(SYS_CFG_BANK1, 3, 13, 13, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);

		/* TMS should be set to 1 when taking the TAP
		 * machine out of reset... */
		sysconf_write(jtag.tms, 1);

		/* sata_trst_fromconf */
		sc = sysconf_claim(SYS_CFG_BANK1, 3, 21, 21, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		udelay(100);

		/* Power up & initialize PHY(s) (one by one) */

		/* SATA_0_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 3, 3, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);
		stm_miphy_init(&miphy, 0);

		/* SATA_1_POWERDOWN_REQ */
		sc = sysconf_claim(SYS_CFG_BANK4, 46, 4, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);
		stm_miphy_init(&miphy, 1);

		initialized = 1;
	}

	platform_device_register(&stx7108_sata_devices[port]);
}



/* PATA resources --------------------------------------------------------- */

/*
 * EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0
 */
static struct resource stx7108_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = RESOURCE_IRQ(-1),
};

static struct platform_device stx7108_pata_device = {
	.name = "pata_platform",
	.id = -1,
	.num_resources = ARRAY_SIZE(stx7108_pata_resources),
	.resource = stx7108_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7108_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base;

	bank_base = emi_bank_base(bank);
	stx7108_pata_resources[0].start += bank_base;
	stx7108_pata_resources[0].end   += bank_base;
	stx7108_pata_resources[1].start += bank_base;
	stx7108_pata_resources[1].end   += bank_base;
	stx7108_pata_resources[2].start = irq;
	stx7108_pata_resources[2].end   = irq;

	emi_config_pata(bank, pc_mode);

	platform_device_register(&stx7108_pata_device);
}



/* Ethernet MAC resources ------------------------------------------------- */

static void stx7108_gmac_rmii_speed(void *priv, unsigned int speed)
{
	struct sysconf_field *sc = priv;

	sysconf_write(sc, (speed == SPEED_100) ? 1 : 0);
}

static void stx7108_gmac_gmii_gtx_speed(void *priv, unsigned int speed)
{
	void (*txclk_select)(int txclk_250_not_25_mhz) = priv;

	if (txclk_select)
		txclk_select(speed == SPEED_1000);
}

static struct plat_stmmacenet_data stx7108_gmac_private_data[2] = {
	{
		.bus_id = 0,
		.pbl = 32,
		.has_gmac = 1,
		/* .fix_mac_speed set in stx7108_configure_ethernet() */
	}, {
		.bus_id = 1,
		.pbl = 32,
		.has_gmac = 1,
		/* .fix_mac_speed set in stx7108_configure_ethernet() */
	}
};

static struct platform_device stx7108_gmac_devices[2] = {
	{
		.name		= "stmmaceth",
		.id		= 0,
		.num_resources	= 2,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfda88000, 0x8000),
			RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(21)),
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7108_gmac_private_data[0],
		}
	}, {
		.name 		= "stmmaceth",
		.id		= 1,
		.num_resources	= 2,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfe730000, 0x8000),
			RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(23)),
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7108_gmac_private_data[1],
		}
	}
};

struct stx7108_gmac_pin {
	struct {
		unsigned char port, pin, alt;
	} pio[2];
	enum { BYPASS = 1, CLOCK, PHY_CLOCK, DATA, DGTX } type;
	struct stx7108_pioalt_pad_cfg *dir;
};

static struct stx7108_gmac_pin stx7108_gmac_mii_pins[] __initdata = {
	{ { { 9, 3, 1 }, { 15, 5, 2 } }, PHY_CLOCK, },		/* PHYCLK */
	{ { { 6, 0, 1 }, { 16, 0, 2 } }, DATA, OUT},		/* TXD[0] */
	{ { { 6, 1, 1 }, { 16, 1, 2 } }, DATA, OUT },		/* TXD[1] */
	{ { { 6, 2, 1 }, { 16, 2, 2 } }, DATA, OUT },		/* TXD[2] */
	{ { { 6, 3, 1 }, { 16, 3, 2 } }, DATA, OUT },		/* TXD[3] */
	{ { { 7, 0, 1 }, { 17, 1, 2 } }, DATA, OUT },		/* TXER */
	{ { { 7, 1, 1 }, { 15, 7, 2 } }, DATA, OUT },		/* TXEN */
	{ { { 7, 2, 1 }, { 17, 0, 2 } }, CLOCK, IN },		/* TXCLK */
	{ { { 7, 3, 1 }, { 17, 3, 2 } }, BYPASS, IN },		/* COL */
	{ { { 7, 4, 1 }, { 17, 4, 2 } }, BYPASS, BIDIR },	/* MDIO */
	{ { { 7, 5, 1 }, { 17, 5, 2 } }, CLOCK, OUT },		/* MDC */
	{ { { 7, 6, 1 }, { 17, 2, 2 } }, BYPASS, IN },		/* CRS */
	{ { { 7, 7, 1 }, { 15, 6, 2 } }, BYPASS, IN },		/* MDINT */
	{ { { 8, 0, 1 }, { 18, 0, 2 } }, DATA, IN },		/* RXD[0] */
	{ { { 8, 1, 1 }, { 18, 1, 2 } }, DATA, IN },		/* RXD[1] */
	{ { { 8, 2, 1 }, { 18, 2, 2 } }, DATA, IN },		/* RXD[2] */
	{ { { 8, 3, 1 }, { 18, 3, 2 } }, DATA, IN },		/* RXD[3] */
	{ { { 9, 0, 1 }, { 17, 6, 2 } }, DATA, IN },		/* RXDV */
	{ { { 9, 1, 1 }, { 17, 7, 2 } }, DATA, IN },		/* RX_ER */
	{ { { 9, 2, 1 }, { 19, 0, 2 } }, CLOCK, IN },		/* RXCLK */
};

static struct stx7108_gmac_pin stx7108_gmac_gmii_pins[] __initdata = {
	{ { { 9, 3, 1 }, { 15, 5, 2 } }, PHY_CLOCK, },		/* PHYCLK */
	{ { { 6, 0, 1 }, { 16, 0, 2 } }, DATA, OUT },		/* TXD[0] */
	{ { { 6, 1, 1 }, { 16, 1, 2 } }, DATA, OUT },		/* TXD[1] */
	{ { { 6, 2, 1 }, { 16, 2, 2 } }, DATA, OUT },		/* TXD[2] */
	{ { { 6, 3, 1 }, { 16, 3, 2 } }, DATA, OUT },		/* TXD[3] */
	{ { { 6, 4, 1 }, { 16, 4, 2 } }, DATA, OUT },		/* TXD[4] */
	{ { { 6, 5, 1 }, { 16, 5, 2 } }, DATA, OUT },		/* TXD[5] */
	{ { { 6, 6, 1 }, { 16, 6, 2 } }, DATA, OUT },		/* TXD[6] */
	{ { { 6, 7, 1 }, { 16, 7, 2 } }, DATA, OUT },		/* TXD[7] */
	{ { { 7, 0, 1 }, { 17, 1, 2 } }, DATA, OUT },		/* TXER */
	{ { { 7, 1, 1 }, { 15, 7, 2 } }, DATA, OUT },		/* TXEN */
	{ { { 7, 2, 1 }, { 17, 0, 2 } }, CLOCK, IN },		/* TXCLK */
	{ { { 7, 3, 1 }, { 17, 3, 2 } }, BYPASS, IN },		/* COL */
	{ { { 7, 4, 1 }, { 17, 4, 2 } }, BYPASS, BIDIR },	/* MDIO */
	{ { { 7, 5, 1 }, { 17, 5, 2 } }, CLOCK, OUT },		/* MDC */
	{ { { 7, 6, 1 }, { 17, 2, 2 } }, BYPASS, IN },		/* CRS */
	{ { { 7, 7, 1 }, { 15, 6, 2 } }, BYPASS, IN },		/* MDINT */
	{ { { 8, 0, 1 }, { 18, 0, 2 } }, DATA, IN }, 		/* RXD[0] */
	{ { { 8, 1, 1 }, { 18, 1, 2 } }, DATA, IN }, 		/* RXD[1] */
	{ { { 8, 2, 1 }, { 18, 2, 2 } }, DATA, IN }, 		/* RXD[2] */
	{ { { 8, 3, 1 }, { 18, 3, 2 } }, DATA, IN }, 		/* RXD[3] */
	{ { { 8, 4, 1 }, { 18, 4, 2 } }, DATA, IN }, 		/* RXD[4] */
	{ { { 8, 5, 1 }, { 18, 5, 2 } }, DATA, IN }, 		/* RXD[5] */
	{ { { 8, 6, 1 }, { 18, 6, 2 } }, DATA, IN }, 		/* RXD[6] */
	{ { { 8, 7, 1 }, { 18, 7, 2 } }, DATA, IN }, 		/* RXD[7] */
	{ { { 9, 0, 1 }, { 17, 6, 2 } }, DATA, IN },		/* RXDV */
	{ { { 9, 1, 1 }, { 17, 7, 2 } }, DATA, IN },		/* RX_ER */
	{ { { 9, 2, 1 }, { 19, 0, 2 } }, CLOCK, IN  },		/* RXCLK */
};

static struct stx7108_gmac_pin stx7108_gmac_gmii_gtx_pins[] __initdata = {
	{ { { 9, 3, 3 }, { 15, 5, 4 } }, PHY_CLOCK, },		/* PHYCLK */
	{ { { 6, 0, 1 }, { 16, 0, 2 } }, DATA, OUT },		/* TXD[0] */
	{ { { 6, 1, 1 }, { 16, 1, 2 } }, DATA, OUT },		/* TXD[1] */
	{ { { 6, 2, 1 }, { 16, 2, 2 } }, DATA, OUT },		/* TXD[2] */
	{ { { 6, 3, 1 }, { 16, 3, 2 } }, DATA, OUT },		/* TXD[3] */
	{ { { 6, 4, 1 }, { 16, 4, 2 } }, DGTX, OUT },		/* TXD[4] */
	{ { { 6, 5, 1 }, { 16, 5, 2 } }, DGTX, OUT },		/* TXD[5] */
	{ { { 6, 6, 1 }, { 16, 6, 2 } }, DGTX, OUT },		/* TXD[6] */
	{ { { 6, 7, 1 }, { 16, 7, 2 } }, DGTX, OUT },		/* TXD[7] */
	{ { { 7, 0, 1 }, { 17, 1, 2 } }, DATA, OUT },		/* TXER */
	{ { { 7, 1, 1 }, { 15, 7, 2 } }, DATA, OUT },		/* TXEN */
	{ { { 7, 2, 1 }, { 17, 0, 2 } }, CLOCK, IN },		/* TXCLK */
	{ { { 7, 3, 1 }, { 17, 3, 2 } }, BYPASS, IN },		/* COL */
	{ { { 7, 4, 1 }, { 17, 4, 2 } }, BYPASS, BIDIR },	/* MDIO */
	{ { { 7, 5, 1 }, { 17, 5, 2 } }, CLOCK, OUT },		/* MDC */
	{ { { 7, 6, 1 }, { 17, 2, 2 } }, BYPASS, IN },		/* CRS */
	{ { { 7, 7, 1 }, { 15, 6, 2 } }, BYPASS, IN },		/* MDINT */
	{ { { 8, 0, 1 }, { 18, 0, 2 } }, DATA, IN }, 		/* RXD[0] */
	{ { { 8, 1, 1 }, { 18, 1, 2 } }, DATA, IN }, 		/* RXD[1] */
	{ { { 8, 2, 1 }, { 18, 2, 2 } }, DATA, IN }, 		/* RXD[2] */
	{ { { 8, 3, 1 }, { 18, 3, 2 } }, DATA, IN }, 		/* RXD[3] */
	{ { { 8, 4, 1 }, { 18, 4, 2 } }, DGTX, IN }, 		/* RXD[4] */
	{ { { 8, 5, 1 }, { 18, 5, 2 } }, DGTX, IN }, 		/* RXD[5] */
	{ { { 8, 6, 1 }, { 18, 6, 2 } }, DGTX, IN }, 		/* RXD[6] */
	{ { { 8, 7, 1 }, { 18, 7, 2 } }, DGTX, IN }, 		/* RXD[7] */
	{ { { 9, 0, 1 }, { 17, 6, 2 } }, DATA, IN },		/* RXDV */
	{ { { 9, 1, 1 }, { 17, 7, 2 } }, DATA, IN },		/* RX_ER */
	{ { { 9, 2, 1 }, { 19, 0, 2 } }, CLOCK, IN  },		/* RXCLK */
};

/* At the time of writing the suggested retime configuration for
 * MII pads in RMII mode was "BYPASS"... */
static struct stx7108_gmac_pin stx7108_gmac_rmii_pins[] __initdata = {
	{ { { 9, 3, 2 }, { 15, 5, 3 } }, BYPASS, },		/* PHYCLK */
	{ { { 6, 0, 1 }, { 16, 0, 2 } }, BYPASS, OUT },		/* TXD[0] */
	{ { { 6, 1, 1 }, { 16, 1, 2 } }, BYPASS, OUT },		/* TXD[1] */
	{ { { 7, 0, 1 }, { 17, 1, 2 } }, BYPASS, OUT },		/* TXER */
	{ { { 7, 1, 1 }, { 15, 7, 2 } }, BYPASS, OUT },		/* TXEN */
	{ { { 7, 4, 1 }, { 17, 4, 2 } }, BYPASS, BIDIR },	/* MDIO */
	{ { { 7, 5, 1 }, { 17, 5, 2 } }, BYPASS, OUT },		/* MDC */
	{ { { 7, 7, 1 }, { 15, 6, 2 } }, BYPASS, IN  },		/* MDINT */
	{ { { 8, 0, 1 }, { 18, 0, 2 } }, BYPASS, IN  },		/* RXD[0] */
	{ { { 8, 1, 1 }, { 18, 1, 2 } }, BYPASS, IN  },		/* RXD[1] */
	{ { { 9, 0, 1 }, { 17, 6, 2 } }, BYPASS, IN  },		/* RXDV */
	{ { { 9, 1, 1 }, { 17, 7, 2 } }, BYPASS, IN  },		/* RX_ER */
};

static struct stx7108_gmac_pin stx7108_gmac_reverse_mii_pins[] __initdata = {
	{ { { 9, 3, 1 }, { 15, 5, 2 } }, PHY_CLOCK, },		/* PHYCLK */
	{ { { 6, 0, 1 }, { 16, 0, 2 } }, DATA, OUT },		/* TXD[-1] */
	{ { { 6, 1, 1 }, { 16, 1, 2 } }, DATA, OUT },		/* TXD[1] */
	{ { { 6, 2, 1 }, { 16, 2, 2 } }, DATA, OUT },		/* TXD[2] */
	{ { { 6, 3, 1 }, { 16, 3, 2 } }, DATA, OUT },		/* TXD[3] */
	{ { { 7, 0, 1 }, { 17, 1, 2 } }, DATA, OUT },		/* TXER */
	{ { { 7, 1, 1 }, { 15, 7, 2 } }, DATA, OUT },		/* TXEN */
	{ { { 7, 2, 1 }, { 17, 0, 2 } }, CLOCK, IN },		/* TXCLK */
	{ { { 7, 3, 2 }, { 17, 3, 3 } }, BYPASS, OUT },		/* COL */
	{ { { 7, 4, 1 }, { 17, 4, 2 } }, BYPASS, BIDIR },	/* MDIO */
	{ { { 7, 5, 2 }, { 17, 5, 3 } }, CLOCK, IN },		/* MDC */
	{ { { 7, 6, 2 }, { 17, 2, 3 } }, BYPASS, OUT },		/* CRS */
	{ { { 7, 7, 1 }, { 15, 6, 2 } }, BYPASS, IN },		/* MDINT */
	{ { { 8, 0, 1 }, { 18, 0, 2 } }, DATA, IN },		/* RXD[0] */
	{ { { 8, 1, 1 }, { 18, 1, 2 } }, DATA, IN },		/* RXD[1] */
	{ { { 8, 2, 1 }, { 18, 2, 2 } }, DATA, IN },		/* RXD[2] */
	{ { { 8, 3, 1 }, { 18, 3, 2 } }, DATA, IN },		/* RXD[3] */
	{ { { 9, 0, 1 }, { 17, 6, 2 } }, DATA, IN },		/* RXDV */
	{ { { 9, 1, 1 }, { 17, 7, 2 } }, DATA, IN },		/* RX_ER */
	{ { { 9, 2, 1 }, { 19, 0, 2 } }, CLOCK, IN },		/* RXCLK */
};

void __init stx7108_configure_ethernet(int port,
		struct stx7108_ethernet_config *config)
{
	int sc_regtype, sc_regnum;
	struct sysconf_field *sc;
	struct stx7108_gmac_pin *pins;
	int pins_num;
	unsigned char phy_sel, enmii;
	int i;

	switch (port) {
	case 0:
		sc_regtype = SYS_CFG_BANK2;
		sc_regnum = 27;

		/* EN_GMAC0 */
		sc = sysconf_claim(SYS_CFG_BANK2, 53, 0, 0, "stmmac");
		sysconf_write(sc, 1);
		break;
	case 1:
		sc_regtype = SYS_CFG_BANK4;
		sc_regnum = 23;

		/* EN_GMAC1 */
		sc = sysconf_claim(SYS_CFG_BANK4, 67, 0, 0, "stmmac");
		sysconf_write(sc, 1);
		break;
	default:
		BUG();
		return;
	};
	/* Configure the bridge to generate more efficient STBus traffic.
	 * Ethernet AD_CONFIG[21:0] = 0x264006 */
	writel(0x264006, stx7108_gmac_devices[port].resource->start + 0x7000);

	switch (config->mode) {
	case stx7108_ethernet_mode_mii:
		phy_sel = 0;
		enmii = 1;
		pins = stx7108_gmac_mii_pins;
		pins_num = ARRAY_SIZE(stx7108_gmac_mii_pins);
		break;
	case stx7108_ethernet_mode_rmii:
		phy_sel = 4;
		enmii = 1;
		pins = stx7108_gmac_rmii_pins;
		pins_num = ARRAY_SIZE(stx7108_gmac_rmii_pins);
		stx7108_gmac_private_data[port].fix_mac_speed =
				stx7108_gmac_rmii_speed;
		/* MIIx_MAC_SPEED_SEL */
		stx7108_gmac_private_data[port].bsp_priv =
				sysconf_claim(sc_regtype, sc_regnum, 1, 1,
						"stmmac");
		break;
	case stx7108_ethernet_mode_gmii:
		phy_sel = 0;
		enmii = 1;
		pins = stx7108_gmac_gmii_pins;
		pins_num = ARRAY_SIZE(stx7108_gmac_gmii_pins);
		break;
	case stx7108_ethernet_mode_gmii_gtx:
		phy_sel = 0;
		enmii = 1;
		pins = stx7108_gmac_gmii_gtx_pins;
		pins_num = ARRAY_SIZE(stx7108_gmac_gmii_gtx_pins);
		stx7108_gmac_private_data[port].fix_mac_speed =
				stx7108_gmac_gmii_gtx_speed;
		stx7108_gmac_private_data[port].bsp_priv =
				config->txclk_select;
		break;
	case stx7108_ethernet_mode_reverse_mii:
		phy_sel = 0;
		enmii = 0;
		pins = stx7108_gmac_reverse_mii_pins;
		pins_num = ARRAY_SIZE(stx7108_gmac_reverse_mii_pins);
		break;
	default:
		BUG();
		return;
	}

	/* MIIx_PHY_SEL */
	sc = sysconf_claim(sc_regtype, sc_regnum, 2, 4, "stmmac");
	sysconf_write(sc, phy_sel);

	/* ENMIIx */
	sc = sysconf_claim(sc_regtype, sc_regnum, 5, 5, "stmmac");
	sysconf_write(sc, enmii);

	pins[0].dir = config->ext_clk ? IN : OUT;

	for (i = 0; i < pins_num; i++) {
		struct stx7108_gmac_pin *pin = &pins[i];
		int portno = pin->pio[port].port;
		int pinno = pin->pio[port].pin;
		struct stx7108_pioalt_retime_cfg retime_cfg = {
			-1, -1, -1, -1, -1, -1 /* -1 means "do not set */
		};

		stx7108_pioalt_select(portno, pinno, pin->pio[port].alt,
				"stmmac");

		stx7108_pioalt_pad(portno, pinno, pin->dir, "stmmac");

		switch (pin->type) {
		case BYPASS:
			retime_cfg.clknotdata = 0;
			retime_cfg.retime = 0;
			break;
		case CLOCK:
			retime_cfg.clknotdata = 1;
			retime_cfg.clk1notclk0 = port;
			break;
		case PHY_CLOCK:
			retime_cfg.clknotdata = 1;
			if (config->mode == stx7108_ethernet_mode_gmii_gtx) {
				retime_cfg.clk1notclk0 = 1;
				retime_cfg.double_edge = 0;
			} else {
				retime_cfg.clk1notclk0 = 0;
			}
			break;
		case DGTX: /* extra configuration for GMII (GTK CLK) */
			if (port == 1) {
				retime_cfg.retime = 1;
				retime_cfg.clk1notclk0 = 1;
				retime_cfg.double_edge = 0;
				retime_cfg.clknotdata = 0;
			} else {
				retime_cfg.retime = 1;
				retime_cfg.clk1notclk0 = 0;
				retime_cfg.double_edge = 0;
				retime_cfg.clknotdata = 0;
			}
			break;
		case DATA:
			retime_cfg.clknotdata = 0;
			retime_cfg.retime = 1;
			retime_cfg.clk1notclk0 = port;
			break;
		default:
			BUG();
			break;
		}
		stx7108_pioalt_retime(portno, pinno, &retime_cfg, "stmmac");
	}

	stx7108_gmac_private_data[port].bus_id = config->phy_bus;

	platform_device_register(&stx7108_gmac_devices[port]);
}



/* Audio output ----------------------------------------------------------- */

void __init stx7108_configure_audio(struct stx7108_audio_config *config)
{
	/* Claim PIO pins as PCM player outputs, depending on
	 * how many DATA outputs are to be used... */
	switch (config->pcm_output_mode) {
	case stx7108_audio_pcm_output_8_channels:
		stx7108_pioalt_select(25, 7, 1, "AUD0PCMOUT_DATA3");
		stx7108_pioalt_pad(25, 7, OUT, "AUD0PCMOUT_DATA3");
		/* Fall-through */
	case stx7108_audio_pcm_output_6_channels:
		stx7108_pioalt_select(25, 6, 1, "AUD0PCMOUT_DATA2");
		stx7108_pioalt_pad(25, 6, OUT, "AUD0PCMOUT_DATA2");
		/* Fall-through */
	case stx7108_audio_pcm_output_4_channels:
		stx7108_pioalt_select(25, 5, 1, "AUD0PCMOUT_DATA1");
		stx7108_pioalt_pad(25, 5, OUT, "AUD0PCMOUT_DATA1");
		/* Fall-through */
	case stx7108_audio_pcm_output_2_channels:
		stx7108_pioalt_select(25, 1, 1, "AUD0PCMOUT_CLKIN_OUT");
		stx7108_pioalt_pad(25, 1, OUT, "AUD0PCMOUT_CLKIN_OUT");
		stx7108_pioalt_select(25, 2, 1, "AUD0PCMOUT_LRCLK");
		stx7108_pioalt_pad(25, 2, OUT, "AUD0PCMOUT_LRCLK");
		stx7108_pioalt_select(25, 3, 1, "AUD0PCMOUT_SCLK");
		stx7108_pioalt_pad(25, 3, OUT, "AUD0PCMOUT_SCLK");
		stx7108_pioalt_select(25, 4, 1, "AUD0PCMOUT_DATA0");
		stx7108_pioalt_pad(25, 4, OUT, "AUD0PCMOUT_DATA0");
		break;
	default:
		BUG();
		break;
	}

	/* Claim PIO pin as SPDIF output... */

	if (config->spdif_output_enabled) {
		stx7108_pioalt_select(26, 0, 1, "AUDSPDIFOUT");
		stx7108_pioalt_pad(26, 0, OUT, "AUDSPDIFOUT");
	}

	/* Claim PIO for the PCM reader inputs... */

	if (config->pcm_input_enabled) {
		stx7108_pioalt_select(26, 1, 1, "AUD0PCMIN_LRCLK");
		stx7108_pioalt_pad(26, 1, IN, "AUD0PCMIN_LRCLK");
		stx7108_pioalt_select(26, 2, 1, "AUD0PCMIN_SCLK");
		stx7108_pioalt_pad(26, 2, IN, "AUD0PCMIN_SCLK");
		stx7108_pioalt_select(26, 3, 1, "AUD0PCMIN_DATA");
		stx7108_pioalt_pad(26, 3, IN, "AUD0PCMIN_DATA");
	}
}



/* PWM resources ---------------------------------------------------------- */

static struct platform_device stx7108_pwm_device = {
	.name = "stm-pwm",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		RESOURCE_MEM(0xfd710000, 0x68),
		RESOURCE_IRQ(ILC_IRQ(51)),
	},
	.dev.platform_data = &(struct plat_stm_pwm_data) {},
};

void __init stx7108_configure_pwm(struct stx7108_pwm_config *config)
{
	struct plat_stm_pwm_data *plat_data =
			stx7108_pwm_device.dev.platform_data;

	if (config && config->out0_enabled) {
		stx7108_pioalt_select(26, 4, 1, "pwm");
		stx7108_pioalt_pad(26, 4, OUT, "pwm");
		plat_data->flags |= PLAT_STM_PWM_OUT0;
	}

	if (config && config->out1_enabled) {
		stx7108_pioalt_select(14, 1, 1, "pwm");
		stx7108_pioalt_pad(14, 1, OUT, "pwm");
		plat_data->flags |= PLAT_STM_PWM_OUT1;
	}

	platform_device_register(&stx7108_pwm_device);
}



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7108_devhwrandom_device = {
	.name		= "stm_hwrandom",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource []) {
		RESOURCE_MEM(0xfdabd000, 0x1000),
	}
};

static struct platform_device stx7108_devrandom_device = {
	.name		= "stm_rng",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource []) {
		RESOURCE_MEM(0xfdabd000, 0x1000),
	}
};



/* ASC resources ---------------------------------------------------------- */

static struct platform_device stx7108_asc_devices[] = {
	{
		.name = "stasc",
		/* .id set in stx7108_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource []) {
			RESOURCE_MEM(0xfd730000, 0x100),
			RESOURCE_IRQ(ILC_IRQ(40)),
			RESOURCE_DMA(11),
			RESOURCE_DMA(15),
		},
		.dev.platform_data = &(struct stasc_uart_data) {}
	}, {
		.name = "stasc",
		/* .id set in stx7108_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource []) {
			RESOURCE_MEM(0xfd731000, 0x100),
			RESOURCE_IRQ(ILC_IRQ(41)),
			RESOURCE_DMA(12),
			RESOURCE_DMA(16),
		},
		.dev.platform_data = &(struct stasc_uart_data) {}
	}, {
		.name = "stasc",
		/* .id set in stx7108_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource []) {
			RESOURCE_MEM(0xfd732000, 0x100),
			RESOURCE_IRQ(ILC_IRQ(42)),
			RESOURCE_DMA(13),
			RESOURCE_DMA(17),
		},
		.dev.platform_data = &(struct stasc_uart_data) {}
	}, {
		.name = "stasc",
		/* .id set in stx7108_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource []) {
			RESOURCE_MEM(0xfd733000, 0x100),
			RESOURCE_IRQ(ILC_IRQ(43)),
			RESOURCE_DMA(14),
			RESOURCE_DMA(18),
		},
		.dev.platform_data = &(struct stasc_uart_data) {}
	}
};

struct {
	unsigned char alt;
	struct {
		unsigned char port, pin, dir;
	} pios[4];
} stx7108_asc_pios[] __initdata = {
	{	/* ASC0 */
		.alt = 2,
		.pios = {
			{ .port = 4, .pin = 0, .dir = STPIO_OUT }, /* TXD */
			{ .port = 4, .pin = 1, .dir = STPIO_IN  }, /* RXD */
			{ .port = 4, .pin = 4, .dir = STPIO_IN  }, /* CTS */
			{ .port = 4, .pin = 5, .dir = STPIO_OUT }  /* RTS */
		},
	}, {	/* ASC1 */
		.alt = 1,
		.pios = {
			{ .port = 5, .pin = 1, .dir = STPIO_OUT }, /* TXD */
			{ .port = 5, .pin = 2, .dir = STPIO_IN  }, /* RXD */
			{ .port = 5, .pin = 3, .dir = STPIO_IN  }, /* CTS */
			{ .port = 5, .pin = 4, .dir = STPIO_OUT }  /* RTS */
		},
	}, {	/* ASC2 */
		.alt = 1,
		.pios = {
			{ .port = 14, .pin = 4, .dir = STPIO_OUT }, /* TXD */
			{ .port = 14, .pin = 5, .dir = STPIO_IN  }, /* RXD */
			{ .port = 14, .pin = 7, .dir = STPIO_IN  }, /* CTS */
			{ .port = 14, .pin = 6, .dir = STPIO_OUT }  /* RTS */
		},
	}, {	/* ASC3, variant 1 */
		.alt = 2,
		.pios = {
			{ .port = 21, .pin = 0, .dir = STPIO_OUT }, /* TXD */
			{ .port = 21, .pin = 1, .dir = STPIO_IN  }, /* RXD */
			{ .port = 21, .pin = 4, .dir = STPIO_IN  }, /* CTS */
			{ .port = 21, .pin = 3, .dir = STPIO_OUT }  /* RTS */
		},
	}, {	/* ASC3, variant 2 */
		.alt = 1,
		.pios = {
			{ .port = 24, .pin = 4, .dir = STPIO_OUT }, /* TXD */
			{ .port = 24, .pin = 5, .dir = STPIO_IN  }, /* RXD */
			{ .port = 25, .pin = 0, .dir = STPIO_IN  }, /* CTS */
			{ .port = 24, .pin = 7, .dir = STPIO_OUT }  /* RTS */
		},
	}
};

/*
 * Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.
 */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device *stasc_configured_devices[
		ARRAY_SIZE(stx7108_asc_devices)] __initdata;
unsigned int stasc_configured_devices_count __initdata;

void __init stx7108_configure_asc(int asc, struct stx7108_asc_config *config)
{
	static int configured[ARRAY_SIZE(stx7108_asc_devices)];
	static int tty_id;
	struct stx7108_asc_config default_config = {};
	struct platform_device *pdev;
	struct stasc_uart_data *plat_data;
	int i;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stx7108_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stx7108_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	plat_data->flags = STASC_FLAG_TXFIFO_BUG;
	if (config->hw_flow_control)
		plat_data->flags |= STASC_FLAG_NORTSCTS;

	if (config->is_console) {
		stasc_console_device = tty_id;
		/* the console will be always a wakeup-able device */
		pdev->dev.power.can_wakeup = 1;
		device_set_wakeup_enable(&pdev->dev, 0x1);
	}
	pdev->id = tty_id++;

	for (i = 0; i < (config->hw_flow_control ? 4 : 2); i++) {
		int pio, port, pin, dir;

		pio = asc;
		if (asc == 3)
			switch (i) {
			case 0:
				pio += config->routing.asc3.txd;
				break;
			case 1:
				pio += config->routing.asc3.rxd;
				break;
			case 2:
				pio += config->routing.asc3.cts;
				break;
			case 3:
				pio += config->routing.asc3.rts;
				break;
			}
		BUG_ON(pio >= ARRAY_SIZE(stx7108_asc_pios));

		port = stx7108_asc_pios[pio].pios[i].port;
		pin = stx7108_asc_pios[pio].pios[i].pin;
		dir = stx7108_asc_pios[pio].pios[i].dir;

		plat_data->pios[i].pio_port = port;
		plat_data->pios[i].pio_pin = pin;
		plat_data->pios[i].pio_direction = dir;

		stx7108_pioalt_select(port, pin,
				stx7108_asc_pios[pio].alt, "asc");
		stx7108_pioalt_pad(port, pin,
				dir == STPIO_IN ? IN : OUT, "asc");
	}

	stasc_configured_devices[stasc_configured_devices_count++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stx7108_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx7108_add_asc);



/* LiRC resources --------------------------------------------------------- */

static struct lirc_pio stx7108_lirc_pios[] = {
	{
		.bank  = 2,
		.pin   = 7,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX,
	}, {
		.bank  = 3,
		.pin   = 0,
		.dir   = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX,
	}, {
		.bank  = 3,
		.pin   = 1,
		.dir   = STPIO_OUT,
		.pinof = 0x00 | LIRC_IR_TX,
	}, {
		.bank  = 3,
		.pin   = 2,
		.dir   = STPIO_OUT,
		.pinof = 0x00 | LIRC_IR_TX,
	},
};

static struct plat_lirc_data stx7108_lirc_private_info = {
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
	.pio_pin_arr  = stx7108_lirc_pios,
	.num_pio_pins = ARRAY_SIZE(stx7108_lirc_pios),
#if defined(CONFIG_PM)
	.clk_on_low_power = 1000000,
#endif
};

static struct platform_device stx7108_lirc_device = {
	.name		= "lirc",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource []) {
		RESOURCE_MEM(0xfd718000, 0x234),
		RESOURCE_IRQ(ILC_IRQ(45)),
	},
	.dev = {
		   .power.can_wakeup = 1,
		   .platform_data = &stx7108_lirc_private_info
	}
};

void __init stx7108_configure_lirc(struct stx7108_lirc_config *config)
{
	struct stx7108_lirc_config default_config = {};

	if (!config)
		config = &default_config;

	stx7108_lirc_private_info.scd_info = config->scd;

	switch (config->rx_mode) {
	case stx7108_lirc_rx_mode_ir:
		stx7108_pioalt_select(2, 7, 1, "lirc");
		stx7108_lirc_pios[0].pinof |= LIRC_PIO_ON;
		break;
	case stx7108_lirc_rx_mode_uhf:
		stx7108_pioalt_select(3, 0, 1, "lirc");
		stx7108_lirc_pios[1].pinof |= LIRC_PIO_ON;
		break;
	case stx7108_lirc_rx_disabled:
		/* Do nothing */
		break;
	default:
		BUG();
		break;
	}

	if (config->tx_enabled) {
		stx7108_pioalt_select(3, 1, 1, "lirc");
		stx7108_lirc_pios[2].pinof |= LIRC_PIO_ON;
	}

	if (config->tx_od_enabled) {
		stx7108_pioalt_select(3, 2, 1, "lirc");
		stx7108_lirc_pios[3].pinof |= LIRC_PIO_ON;
	}

	platform_device_register(&stx7108_lirc_device);
}

void __init stx7108_configure_nand(struct platform_device *pdev)
{

	/* NAND Controller base address */
	pdev->resource[0].start	= 0xFE901000;
	pdev->resource[0].end	= 0xFE901FFF;

	/* NAND Controller IRQ */
	pdev->resource[1].start	= ILC_IRQ(121);
	pdev->resource[1].end	= ILC_IRQ(121);

	platform_device_register(pdev);
}

/* MMC/SD resources ------------------------------------------------------- */

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_mmc_out __initdata = {
	.oe = 1,
	.pu = 1,
	.od = 1,
};
#define MMC_OUT (&stx7108_pioalt_pad_mmc_out)

static struct stx7108_pioalt_pad_cfg stx7108_pioalt_pad_mmc_bidir __initdata = {
	.oe = 1,
	.pu = 0,
	.od = 0,
};
#define MMC_BIDIR (&stx7108_pioalt_pad_mmc_bidir)


struct stx7108_mmc_pin {
	struct {
		unsigned char port, pin, alt;
	} pio;
	struct stx7108_pioalt_pad_cfg *dir;
};

/* For the MMC a custom pin out/bidir configuration is needed. */
static struct stx7108_mmc_pin stx7108_mmc_pins[] __initdata = {
	{ { 1, 0, 1 }, MMC_OUT },
	{ { 1, 1, 1 }, MMC_OUT }, /* MMC command */
	{ { 1, 2, 1 }, IN }, /* MMC Write Protection */
	{ { 1, 3, 1 }, IN }, /* MMC Card Detect */
	{ { 1, 4, 1 }, MMC_OUT }, /* MMC LED on */
	{ { 1, 5, 1 }, MMC_OUT }, /* MMC Card PWR */
	{ { 0, 0, 1 }, MMC_BIDIR }, /* MMC Data[0]*/
	{ { 0, 1, 1 }, MMC_BIDIR },
	{ { 0, 2, 1 }, MMC_BIDIR },
	{ { 0, 3, 1 }, MMC_BIDIR },
	{ { 0, 4, 1 }, MMC_BIDIR },
	{ { 0, 5, 1 }, MMC_BIDIR },
	{ { 0, 6, 1 }, MMC_BIDIR },
	{ { 0, 7, 1 }, MMC_BIDIR }, /* MMC Data[7]*/
};

static struct platform_device stx7108_mmc_device = {
		.name = "arasan",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			RESOURCE_MEM(0xfdaba000, 0x1000),
			RESOURCE_IRQ_NAMED("mmcirq", ILC_IRQ(120)),
		},
};

#define PIO1_CFG_CLKNODATA	0x100

void __init stx7108_configure_mmc(void)
{
	struct sysconf_field *sc;
	unsigned long value;
	struct stx7108_mmc_pin *pins;
	int pins_num, i;

	/* Output clock */
	sc = sysconf_claim(SYS_CFG_BANK2, 32, 0, 31, "mmc");
	value = sysconf_read(sc);
	value |= PIO1_CFG_CLKNODATA;
	sysconf_write(sc, value);

	pins = stx7108_mmc_pins;
	pins_num = ARRAY_SIZE(stx7108_mmc_pins);

	for (i = 0; i < pins_num; i++) {
		struct stx7108_mmc_pin *pin = &pins[i];

		stx7108_pioalt_select(pin->pio.port, pin->pio.pin, pin->pio.alt,
				      "mmc");
		stx7108_pioalt_pad(pin->pio.port, pin->pio.pin, pin->dir,
				   "mmc");
	}

	platform_device_register(&stx7108_mmc_device);
}


/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when
 * non-standard PIO pads are used as interrupts. */
int stx7108_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin)
{
	int irq;
	int pin_type;

	if ((pin > 4) || (pin < 1))
		return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch (pin_type) {
	case PCI_PIN_DEFAULT:
		irq = ILC_IRQ(122 + pin - 1);
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here... */
		BUG();
		irq = -1;
		break;
	case PCI_PIN_UNUSED:
		irq = -1; /* Not used */
		break;
	default:
		irq = pin_type; /* Take whatever interrupt you are told */
		break;
	}

	return irq;
}

static struct platform_device stx7108_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		RESOURCE_MEM_NAMED("Memory", 0xc0000000,
				960 * 1024 * 1024),
		{
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		RESOURCE_MEM_NAMED("EMISS", 0xfdaa8000, 0x17fc),
		RESOURCE_MEM_NAMED("PCI-AHB", 0xfea08000, 0xff),
		RESOURCE_IRQ_NAMED("DMA", ILC_IRQ(126)),
		RESOURCE_IRQ_NAMED("Error", ILC_IRQ(127)),
		/* SERR interrupt set in stx7105_configure_pci() */
		RESOURCE_IRQ_NAMED("SERR", -1),
	},
};



#define STX7108_PIO_PCI_SERR stpio_to_gpio(6, 6)

struct stx7108_pci_signal {
	int port;
	int pin;
	const char *name;
};

void __init stx7108_configure_pci(struct pci_config_data *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* LLA clocks have these horrible names... */
	pci_conf->clk_name = "CLKA_PCI";

	/* REQ0 is actually wired to REQ3 to work around NAND problems */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

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

	/* Copy over platform specific data to driver */
	stx7108_pci_device.dev.platform_data = pci_conf;

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG_BANK2, 30, 2, 2, "PCI_PWR_DWN_REQ");
	sysconf_write(sc, 0); /* We will need to stash this somewhere
				 for power management. */
	sc = sysconf_claim(SYS_STA_BANK2, 1, 2, 2, "PCI_PWR_DWN_GRANT");
	while (sysconf_read(sc))
		cpu_relax(); /* Loop until powered up */

	/* REQ/GNT[0] are dedicated EMI pins */
	BUG_ON(pci_conf->req_gnt[0] != PCI_PIN_DEFAULT);

	/* Configure the REQ/GNT[1..2], muxed with PIOs */
	for (i = 1; i <= 2; i++) {
		struct stx7108_pci_signal reqs[] = {
			{ /* REQ0 is not a PIO */ },
			{ 8, 6, "REQ1" },
			{ 2, 4, "REQ2" },
		};
		unsigned req_gpio = stpio_to_gpio(reqs[i].port, reqs[i].pin);
		struct stx7108_pci_signal gnts[] = {
			{ /* GNT0 is not a PIO */ },
			{ 8, 7, "GNT1" },
			{ 2, 5, "GNT2" },
		};
		unsigned gnt_gpio = stpio_to_gpio(gnts[i].port, gnts[i].pin);

		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			if (gpio_request(req_gpio, reqs[i].name) == 0) {
				gpio_direction_input(req_gpio);
				stx7108_pioalt_select(reqs[i].port, reqs[i].pin,
						2, reqs[i].name);
			} else {
				printk(KERN_ERR "Failed to claim REQ%d "
						"PIO!\n", i);
				BUG();
			}
			if (gpio_request(gnt_gpio, gnts[i].name) == 0) {
				gpio_direction_input(gnt_gpio);
				stx7108_pioalt_select(gnts[i].port, gnts[i].pin,
						2, gnts[i].name);
			} else {
				printk(KERN_ERR "Failed to claim GNT%d "
						"PIO!\n", i);
				BUG();
			}
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

	/* Configure interrupt PIOs */
	for (i = 0; i < 4; i++) {
		struct stx7108_pci_signal ints[] = {
			{ 8, 4, "PCIA" },
			{ 8, 5, "PCIB" },
			{ 2, 6, "PCIC" },
			{ 3, 4, "PCID" },
		};
		unsigned int_gpio = stpio_to_gpio(ints[i].port, ints[i].pin);

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_DEFAULT:
			if (gpio_request(int_gpio, ints[i].name) == 0) {
				gpio_direction_input(int_gpio);
				stx7108_pioalt_select(ints[i].port, ints[i].pin,
						2, ints[i].name);
				set_irq_type(ILC_IRQ(122 + i),
						IRQ_TYPE_LEVEL_LOW);
			} else {
				printk(KERN_ERR "Failed to claim INT%c PIO!\n",
						'A' + i);
				BUG();
			}
			break;
		case PCI_PIN_ALTERNATIVE:
			/* There is no alternative here ;-) */
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
		if (gpio_request(STX7108_PIO_PCI_SERR, "PCI_SERR#") == 0) {
			gpio_direction_input(STX7108_PIO_PCI_SERR);
			pci_conf->serr_irq = gpio_to_irq(STX7108_PIO_PCI_SERR);
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
	if (pci_conf->serr_irq != PCI_PIN_UNUSED) {
		struct resource *res = platform_get_resource_byname(
				&stx7108_pci_device, IORESOURCE_IRQ, "SERR");

		BUG_ON(!res);
		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	/* LOCK is not claimed as is totally pointless, the SOCs do not
	 * support any form of coherency */

	platform_device_register(&stx7108_pci_device);
}



/* Other resources (ILC, sysconf and PIO) --------------------------------- */

/* L2-Cache is available on the HOST core only! */
static struct platform_device stx7108_l2_cache_device = {
	.name 		= "stm-l2-cache",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		RESOURCE_MEM(0xfef04000, 0x200),
	},
};

static struct platform_device stx7108_st40host_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		RESOURCE_MEM(0xfda34000, 0x900),
	},
};

static struct platform_device stx7108_st40rt_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		RESOURCE_MEM(0xfda3c000, 0x900),
	},
};

static struct platform_device stx7108_sysconf_devices[] = {
	{
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfde30000, 0x34),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK0,
					.offset = 0,
					.name = "BANK0 SYS_STA",
				}, {
					.group = SYS_CFG_BANK0,
					.offset = 4,
					.name = "BANK0 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfde20000, 0x94),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK1,
					.offset = 0,
					.name = "BANK1 SYS_STA",
				}, {
					.group = SYS_CFG_BANK1,
					.offset = 0x3c,
					.name = "BANK1 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 2,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfda50000, 0xfc),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = SYS_CFG_BANK2,
					.offset = 0,
					.name = "BANK2 SYS_CFG",
				}, {
					.group = SYS_STA_BANK2,
					.offset = 0xe4,
					.name = "BANK2 SYS_STA",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 3,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfd500000, 0x40),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = SYS_STA_BANK3,
					.offset = 0,
					.name = "BANK3 SYS_STA",
				}, {
					.group = SYS_CFG_BANK3,
					.offset = 0x18,
					.name = "BANK3 SYS_CFG",
				}
			},
		}
	}, {
		.name		= "sysconf",
		.id		= 4,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			RESOURCE_MEM(0xfe700000, 0x12c),
		},
		.dev.platform_data = &(struct plat_sysconf_data) {
			.groups_num = 2,
			.groups = (struct plat_sysconf_group []) {
				{
					.group = SYS_CFG_BANK4,
					.offset = 0,
					.name = "BANK4 SYS_CFG",
				}, {
					.group = SYS_STA_BANK4,
					.offset = 0x11c,
					.name = "BANK4 SYS_STA",
				}
			},
		}
	},
};

static struct platform_device stx7108_pio_devices[] = {
	/* COMMS block PIOs */
	STPIO_DEVICE(0, 0xfd720000, ILC_IRQ(129)),
	STPIO_DEVICE(1, 0xfd721000, ILC_IRQ(130)),
	STPIO_DEVICE(2, 0xfd722000, ILC_IRQ(131)),
	STPIO_DEVICE(3, 0xfd723000, ILC_IRQ(132)),
	STPIO_DEVICE(4, 0xfd724000, ILC_IRQ(133)),
	STPIO_DEVICE(5, 0xfd725000, ILC_IRQ(134)),
	STPIO_DEVICE(6, 0xfd726000, ILC_IRQ(135)),
	STPIO_DEVICE(7, 0xfd727000, ILC_IRQ(136)),
	STPIO_DEVICE(8, 0xfd728000, ILC_IRQ(137)),
	STPIO_DEVICE(9, 0xfd729000, ILC_IRQ(138)),

	/* PIO_SW_0 */
	STPIO_DEVICE(10, 0xfda60000, ILC_IRQ(151)),
	STPIO_DEVICE(11, 0xfda61000, ILC_IRQ(152)),
	STPIO_DEVICE(12, 0xfda62000, ILC_IRQ(153)),
	STPIO_DEVICE(13, 0xfda63000, ILC_IRQ(154)),
	STPIO_DEVICE(14, 0xfda64000, ILC_IRQ(155)),

	/* PIO_NE_0 */
	STPIO_DEVICE(15, 0xfe740000, ILC_IRQ(139)),
	STPIO_DEVICE(16, 0xfe741000, ILC_IRQ(140)),
	STPIO_DEVICE(17, 0xfe742000, ILC_IRQ(141)),
	STPIO_DEVICE(18, 0xfe743000, ILC_IRQ(142)),
	STPIO_DEVICE(19, 0xfe744000, ILC_IRQ(143)),
	STPIO_DEVICE(20, 0xfe745000, ILC_IRQ(144)),
	STPIO_DEVICE(21, 0xfe746000, ILC_IRQ(145)),
	STPIO_DEVICE(22, 0xfe747000, ILC_IRQ(146)),
	STPIO_DEVICE(23, 0xfe748000, ILC_IRQ(147)),
	STPIO_DEVICE(24, 0xfe749000, ILC_IRQ(148)),

	/* PIO_NE_1 */
	STPIO_DEVICE(25, 0xfe720000, ILC_IRQ(149)),
	STPIO_DEVICE(26, 0xfe721000, ILC_IRQ(150)),
};

static struct platform_device stx7108_emi = STEMI();



/* Early devices initialization ------------------------------------------- */

/* Initialise devices which are required early in the boot process
 * (called from the board setup file) */
void __init stx7108_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stx7108_sysconf_devices,
			ARRAY_SIZE(stx7108_sysconf_devices));

	stpio_early_init(stx7108_pio_devices,
			ARRAY_SIZE(stx7108_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_STA_BANK1, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7108 version %ld.x, %s core\n", chip_revision,
			HOST_CORE ? "HOST" : "RT");

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7108_postcore_setup(void)
{
	int err = 0;
	int i;

	emi_init(0, 0xfe900000);

	for (i = 0; i < ARRAY_SIZE(stx7108_pio_devices) && !err; i++) {
		int j;

		/* Not all alternative function selectors are set to 0
		 * (PIO function) on reset, so let's be sure... */
		for (j = 0; j < 8; j++)
			stx7108_pioalt_select(stx7108_pio_devices[i].id, j, 0,
					"PIO");

		err = platform_device_register(&stx7108_pio_devices[i]);
	}

	if (!err && HOST_CORE)
		err = platform_device_register(&stx7108_l2_cache_device);

	return err;
}
postcore_initcall(stx7108_postcore_setup);



/* Late devices initialisation -------------------------------------------- */

static struct platform_device *stx7108_late_devices[] __initdata = {
	&stx7108_fdma_devices[0],
	&stx7108_fdma_devices[1],
	&stx7108_fdma_devices[2],
	&stx7108_fdma_xbar_device,
	&stx7108_sysconf_devices[0],
	&stx7108_sysconf_devices[1],
	&stx7108_sysconf_devices[2],
	&stx7108_sysconf_devices[3],
	&stx7108_sysconf_devices[4],
	&stx7108_devhwrandom_device,
	&stx7108_devrandom_device,
	&stx7108_emi,
};

#include "./platform-pm-stx7108.c"

static int __init stx7108_late_device_setup(void)
{
	int err;

	platform_add_pm_devices(stx7108_pm_devices,
			ARRAY_SIZE(stx7108_pm_devices));

	if (HOST_CORE)
		err = platform_device_register(&stx7108_st40host_ilc3_device);
	else
		err = platform_device_register(&stx7108_st40rt_ilc3_device);
	if (err != 0)
		return err;

	return platform_add_devices(stx7108_late_devices,
				    ARRAY_SIZE(stx7108_late_devices));
}
device_initcall(stx7108_late_device_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2,
	WDT,
	HUDI,
};

static struct intc_vect stx7108_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_prio_reg stx7108_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,     0 } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(stx7108_intc_desc, "stx7108",
		stx7108_intc_vectors, NULL,
		NULL, stx7108_intc_prio_registers, NULL);

void __init plat_irq_setup(void)
{
	void *intc2_base;

	register_intc_controller(&stx7108_intc_desc);

	if (HOST_CORE) {
		intc2_base = ioremap(0xfda30000, 0x100);
		ilc_early_init(&stx7108_st40host_ilc3_device);
	} else {
		intc2_base = ioremap(0xfda38000, 0x100);
		ilc_early_init(&stx7108_st40rt_ilc3_device);
	}

	/* Currently we route all ILC3 interrupts to the 0'th output,
	 * which is connected to INTC2: group 0 interrupt 0 (INTEVT
	 * code 0xa00) */

	/* Enable the INTC2 */
	writel(7, intc2_base + 0x00);	/* INTPRI00 */
	writel(1, intc2_base + 0x60);	/* INTMSKCLR00 */

	/* Set up the demux function */
	set_irq_chip(evt2irq(0xa00), &dummy_irq_chip);
	set_irq_chained_handler(evt2irq(0xa00), ilc_irq_demux);

	ilc_demux_init();
}
