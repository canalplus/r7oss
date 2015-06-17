/*
 * STx7200 Setup
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
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/stm/emi.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/pata_platform.h>
#include <asm/sci.h>
#include <asm/irq-ilc.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

static unsigned long chip_revision;

/* USB resources ----------------------------------------------------------- */

#define UHOST2C_BASE(N)                 (0xfd200000 + ((N)*0x00100000))

#define AHB2STBUS_WRAPPER_GLUE_BASE(N)  (UHOST2C_BASE(N))
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)

static u64 st40_dma_mask = DMA_32BIT_MASK;


static struct platform_device st_usb[3] = {
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE(0), ILC_IRQ(80),
		      AHB2STBUS_OHCI_BASE(0), ILC_IRQ(81),
		      AHB2STBUS_WRAPPER_GLUE_BASE(0),
		      AHB2STBUS_PROTOCOL_BASE(0),
		      USB_FLAGS_STRAP_8BIT | USB_FLAGS_STRAP_PLL),
	USB_DEVICE(1, AHB2STBUS_EHCI_BASE(1), ILC_IRQ(82),
		      AHB2STBUS_OHCI_BASE(1), ILC_IRQ(83),
		      AHB2STBUS_WRAPPER_GLUE_BASE(1),
		      AHB2STBUS_PROTOCOL_BASE(1),
		      USB_FLAGS_STRAP_8BIT | USB_FLAGS_STRAP_PLL),
	USB_DEVICE(2, AHB2STBUS_EHCI_BASE(2), ILC_IRQ(84),
		      AHB2STBUS_OHCI_BASE(2), ILC_IRQ(85),
		      AHB2STBUS_WRAPPER_GLUE_BASE(2),
		      AHB2STBUS_PROTOCOL_BASE(2),
		      USB_FLAGS_STRAP_8BIT | USB_FLAGS_STRAP_PLL),
};

/*
 * Workaround for USB problems on 7200 cut 1; alternative to RC delay on board
*/
static void __init usb_soft_jtag_reset(void)
{
	int i, j;
	struct sysconf_field *sc;

	sc = sysconf_claim(SYS_CFG, 33, 0, 6, NULL);

	/* ENABLE SOFT JTAG */
	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB*/
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i=0; i<=53; i++) {
		if((i==0)||(i==1)||(i==19)||(i==36)) {
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);
		}

		if((i==53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i=0; i<=53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDO */
	for (i=0; i<=366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	for(j=0; j<2; j++) {
		for (i=0; i<=365; i++) {
			if ((i == 71) || (i== 192) || (i == 313)) {
				sysconf_write(sc, 0x00000044);
				sysconf_write(sc, 0x00000045);
			}
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);

			if ((i == 365))	{
				sysconf_write(sc, 0x0000004c);
				sysconf_write(sc, 0x0000004d);
			}
		}
	}

	for (i=0; i<=366; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
  	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB */
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i=0; i<=53; i++) {
		if((i==0)||(i==1)||(i==18)||(i==19)||(i==36)||(i==37)) {
			sysconf_write(sc, 0x00000046);
			sysconf_write(sc, 0x00000047);
		}
		if((i==53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);


	for (i=0; i<=53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i=0; i<=366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	mdelay(20);
	sysconf_write(sc, 0x00000040);

	sysconf_release(sc);
}

void __init stx7200_configure_usb(int port)
{
	static int first = 1;
	const unsigned char power_pins[3] = {1, 3, 4};
	const unsigned char oc_pins[3] = {0, 2, 5};
	struct stpio_pin *pio;
	struct sysconf_field *sc;
	struct plat_usb_data *plat_data;

	if (first) {
		/* route USB and parts of MAFE instead of DVO.
		 * conf_pad_pio[2] = 0 */
		sc = sysconf_claim(SYS_CFG, 7, 26, 26, "usb");
		sysconf_write(sc, 0);

		/* DVO output selection (probably ignored).
		 * conf_pad_pio[3] = 0 */
		sc = sysconf_claim(SYS_CFG, 7, 27, 27, "usb");
		sysconf_write(sc, 0);

		/* Enable soft JTAG mode for USB and SATA
		 * soft_jtag_en = 1 */
		sc = sysconf_claim(SYS_CFG, 33, 6, 6, "usb");
		sysconf_write(sc, 1);
		sysconf_release(sc);
		/* tck = tdi = trstn_usb = tms_usb = 0 */
		sc = sysconf_claim(SYS_CFG, 33, 0, 3, "usb");
		sysconf_write(sc, 0);
		sysconf_release(sc);

		if (cpu_data->cut_major < 2)
			usb_soft_jtag_reset();

		first = 0;
	}

#ifndef CONFIG_PM
	/* Power up port */
	sc = sysconf_claim(SYS_CFG, 22, 3+port, 3+port, "usb");
	sysconf_write(sc, 0);
#endif

	pio = stpio_request_pin(7, power_pins[port], "USB power",
				STPIO_ALT_OUT);
	stpio_set_pin(pio, 1);

	if (cpu_data->cut_major < 2)
		pio = stpio_request_pin(7, oc_pins[port], "USB oc",
					STPIO_ALT_BIDIR);
	else
		pio = stpio_request_pin(7, oc_pins[port], "USB oc",
					STPIO_IN);

	/*
	 * On 7200c1 the AMBA-to-STBus bridge cannot be configured to work in
	 * "threshold-triggered" mode.
	 */
	plat_data = st_usb[port].dev.platform_data;
	if (cpu_data->cut_major < 2)
		plat_data->flags |= USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE;
	else
		plat_data->flags |= USB_FLAGS_STBUS_CONFIG_THRESHOLD_256 |
				USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
				USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32;

	platform_device_register(&st_usb[port]);
}

/* SATA resources ---------------------------------------------------------- */

/* Ok to have same private data for both controllers */
static struct plat_sata_data sata_private_info = {
	.phy_init = 0x0,
	.pc_glue_logic_init = 0x0,
	.only_32bit = 0,
};

static struct platform_device sata_device[2] = {
	SATA_DEVICE(0, 0xfd520000, ILC_IRQ(89), ILC_IRQ(88),
		    &sata_private_info),
	SATA_DEVICE(1, 0xfd521000, ILC_IRQ(91), ILC_IRQ(90),
		    &sata_private_info),
};

void __init stx7200_configure_sata(unsigned int port)
{
	static int initialised_phy;

	if ((cpu_data->cut_major >= 3) && (!initialised_phy)) {
		struct stm_miphy_sysconf_soft_jtag jtag;
		struct sysconf_field *sc;

		jtag.tck = sysconf_claim(SYS_CFG, 33, 0, 0, "SATA");
		BUG_ON(!jtag.tck);
		jtag.tms = sysconf_claim(SYS_CFG, 33, 5, 5, "SATA");
		BUG_ON(!jtag.tms);
		jtag.tdi = sysconf_claim(SYS_CFG, 33, 1, 1, "SATA");
		BUG_ON(!jtag.tdi);
		jtag.tdo = sysconf_claim(SYS_STA, 0, 1, 1, "SATA");
		BUG_ON(!jtag.tdo);

		/* SOFT_JTAG_EN */
		sc = sysconf_claim(SYS_CFG, 33, 6, 6, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);

		/* TMS should be set to 1 when taking the TAP
		 * machine out of reset... */
		sysconf_write(jtag.tms, 1);

		/* SATA_TRSTN */
		sc = sysconf_claim(SYS_CFG, 33, 4, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		udelay(100);

		stm_miphy_init(&(struct stm_miphy) {
				.ports_num = 1,
				.jtag_tick = stm_miphy_sysconf_jtag_tick,
				.jtag_priv = &jtag, }, 0);

		initialised_phy = 1;
	}

	BUG_ON(port > 1);
	platform_device_register(sata_device + port);
}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw stx7200_fdma_hw = {
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

static struct stm_plat_fdma_data stx7200_fdma_platform_data = {
	.hw = &stx7200_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7200_fdma_platform_data_addr &stx7200_fdma_platform_data

#else

#define stx7200_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7200_fdma_devices[] = {
	{
		.name		= "stm-fdma",
		.id		= 0,
		.num_resources	= 2,
		.resource = (struct resource[]) {
			{
				.start = 0xfd810000,
				.end   = 0xfd81ffff,
				.flags = IORESOURCE_MEM,
			}, {
				.start = ILC_IRQ(13),
				.end   = ILC_IRQ(13),
				.flags = IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = stx7200_fdma_platform_data_addr,
	}, {
		.name		= "stm-fdma",
		.id		= 1,
		.num_resources	= 2,
		.resource = (struct resource[2]) {
			{
				.start = 0xfd820000,
				.end   = 0xfd82ffff,
				.flags = IORESOURCE_MEM,
			}, {
				.start = ILC_IRQ(15),
				.end   = ILC_IRQ(15),
				.flags = IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = stx7200_fdma_platform_data_addr,
	},
};

static struct platform_device stx7200_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start = 0xfd830000,
			.end   = 0xfd830fff,
			.flags = IORESOURCE_MEM,
		},
	},
};

/* SSC resources ----------------------------------------------------------- */
static const char i2c_stm_ssc[] = "i2c_stm_ssc";
static const char i2c_stm_pio[] = "i2c_stm_pio";
static char spi_st[] = "spi_st_ssc";
static struct platform_device stssc_devices[] = {
	STSSC_DEVICE(0xfd040000, ILC_IRQ(108), 2, 0, 1, 2),
	STSSC_DEVICE(0xfd041000, ILC_IRQ(109), 3, 0, 1, 2),
	STSSC_DEVICE(0xfd042000, ILC_IRQ(110), 4, 0, 1, 0xff),
	STSSC_DEVICE(0xfd043000, ILC_IRQ(111), 5, 0, 1, 2),
	STSSC_DEVICE(0xfd044000, ILC_IRQ(112), 7, 6, 7, 0xff),
};

static int __initdata num_i2c;
static int __initdata num_spi;
void __init stx7200_configure_ssc(struct plat_ssc_data *data)
{
	int i;
	int capability = data->capability;
	struct sysconf_field* ssc_sc;

	for (i=0; i<ARRAY_SIZE(stssc_devices); i++, capability >>= SSC_BITS_SIZE){
		struct ssc_pio_t *ssc_pio = stssc_devices[i].dev.platform_data;

		if(capability & SSC_UNCONFIGURED)
			continue;
		/* We only support SSC as master, so always set up as such.
		 * ssc<x>_mux_sel = 0 */
		ssc_sc = sysconf_claim(SYS_CFG, 7, i, i, "ssc");
		if(capability & SSC_SPI_CAPABILITY){
			/* !!FIXME!!
			   For some reason, nand_rb signal (PIO2[7]) must be
			   disabled for SSC0/SPI to get input */
			if (i == 0) {
				ssc_sc = sysconf_claim(SYS_CFG,
						       7, 15, 15, "ssc");
				sysconf_write(ssc_sc, 0);
			}

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

/* PATA resources ---------------------------------------------------------- */

/*
 * EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0
 */

static struct resource pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start	= (1<<20),
		.end	= (1<<20) + (8<<17)-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start	= (1<<21) + (6<<17),
		.end	= (1<<21) + (6<<17) + 3,
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

void __init stx7200_configure_pata(int bank, int pc_mode, int irq)
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

static struct sysconf_field *mac_speed_sc[2];

static void fix_mac_speed(void *priv, unsigned int speed)
{
	unsigned port = (unsigned)priv;

	sysconf_write(mac_speed_sc[port], (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stmmaceth_private_data[2] = {
{
	.pbl = 32,
	.has_gmac = 0,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = (void*)0,
}, {
	.pbl = 32,
	.has_gmac = 0,
	.fix_mac_speed = fix_mac_speed,
	.bsp_priv = (void*)1,
} };

static struct platform_device stmmaceth_device[2] = {
{
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd500000,
			.end	= 0xfd50ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(92),
			.end	= ILC_IRQ(92),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stmmaceth_private_data[0],
	}
}, {
	.name		= "stmmaceth",
	.id		= 1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd510000,
			.end	= 0xfd51ffff,
			.flags	= IORESOURCE_MEM,
		},
		{
			.name	= "macirq",
			.start	= ILC_IRQ(94),
			.end	= ILC_IRQ(94),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.power.can_wakeup = 1,
		.platform_data = &stmmaceth_private_data[1],
	}
} };

void stx7200_configure_ethernet(int port, int rmii_mode, int ext_clk,
				int phy_bus)
{
	struct sysconf_field *sc;

	stmmaceth_private_data[port].bus_id = phy_bus;

	/* Route Ethernet pins to output */
	/* bit26-16: conf_pad_eth(10:0) */
	if (port == 0) {
		/* MII0: conf_pad_eth(0) = 0 (ethernet) */
		sc = sysconf_claim(SYS_CFG, 41, 16, 16, "stmmac");
		sysconf_write(sc, 0);
	} else {
		/* MII1: conf_pad_eth(2) = 0, (3)=0, (4)=0, (9)=0, (10)=0 (eth)
		 * MII1: conf_pad_eth(6) = 0 (MII1TXD[0] = output)
		 * (remaining bits have no effect in ethernet mode */
		sc = sysconf_claim(SYS_CFG, 41, 16+2, 16+10, "stmmac");
		sysconf_write(sc, 0);
	}

	/* DISABLE_MSG_FOR_WRITE=0 */
	sc = sysconf_claim(SYS_CFG, 41, 14+port, 14+port, "stmmac");
	sysconf_write(sc, 0);

	/* DISABLE_MSG_FOR_READ=0 */
	sc = sysconf_claim(SYS_CFG, 41, 12+port, 12+port, "stmmac");
	sysconf_write(sc, 0);

	/* VCI_ACK_SOURCE = 0 */
	sc = sysconf_claim(SYS_CFG, 41, 6+port, 6+port, "stmmac");
	sysconf_write(sc, 0);

	/* ETHERNET_INTERFACE_ON (aka RESET) = 1 */
	sc = sysconf_claim(SYS_CFG, 41, 8+port, 8+port, "stmmac");
	sysconf_write(sc, 1);

	/* RMII_MODE */
	sc = sysconf_claim(SYS_CFG, 41, 0+port, 0+port, "stmmac");
	sysconf_write(sc, rmii_mode ? 1 : 0);

	/* PHY_CLK_EXT */
	sc = sysconf_claim(SYS_CFG, 41, 2+port, 2+port, "stmmac");
	sysconf_write(sc, ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	mac_speed_sc[port] = sysconf_claim(SYS_CFG, 41, 4+port, 4+port, "stmmac");

	platform_device_register(&stmmaceth_device[port]);
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[]= {
	[0] = {
		.start	= 0xfd010000,
		.end	= 0xfd010000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= ILC_IRQ(114),
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx7200_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		stpio_request_pin(4, 6, "PWM", STPIO_ALT_OUT);
	}

	if (data->flags & PLAT_STM_PWM_OUT1) {
		stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
	}

	platform_device_register(&stm_pwm_device);
}

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
                .pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
	[3] = {
		.bank = 3,
		.pin  = 6,
		.dir  = STPIO_ALT_OUT,
                .pinof= 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	}
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7200, the clock settings will be calculated by the driver
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
	.clk_on_low_power = 30000000,
#endif
};

static struct platform_device lirc_device =
	STLIRC_DEVICE(0xfd018000, ILC_IRQ(116), ILC_IRQ(117));

void __init stx7200_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;

	platform_device_register(&lirc_device);
}

/* NAND setup -------------------------------------------------------------- */

void __init stx7200_configure_nand(struct platform_device *pdev)
{
	/* EMI Bank base address */
	/*  - setup done in stm_nand_emi probe */

	/* NAND Controller base address */
	pdev->resource[0].start	= 0xFDF01000;
	pdev->resource[0].end	= 0xFDF01FFF;

	/* NAND Controller IRQ */
	pdev->resource[1].start	= ILC_IRQ(123);
	pdev->resource[1].end	= ILC_IRQ(123);

	platform_device_register(pdev);
}


/* Hardware RNG resources -------------------------------------------------- */

static struct platform_device hwrandom_rng_device = {
	.name	   = "stm_hwrandom",
	.id	     = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0xfdb70000,
			.end    = 0xfdb70fff,
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
			.start  = 0xfdb70000,
			.end    = 0xfdb70fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	STASC_DEVICE(0xfd030000, ILC_IRQ(104), 19, 23, 0, 0, 1, 4, 7,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0xfd031000, ILC_IRQ(105), 20, 24, 1, 0, 1, 4, 5,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT), /* oe pin: 6 */
	STASC_DEVICE(0xfd032000, ILC_IRQ(106), 21, 25, 4, 3, 2, 4, 5,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
	STASC_DEVICE(0xfd033000, ILC_IRQ(107), 22, 26, 5, 4, 3, 5, 6,
			STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
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
void __init stx7200_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;
	struct sysconf_field *sc7_29 = NULL;

	for (i=0; i<num_ascs; i++) {
		int port;
		struct platform_device *pdev;
		struct sysconf_field *sc;
		unsigned char flags;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stm_stasc_devices[port];

		if ((port == 0) || (port == 1)) {
			/* Route UART0/1 and MPX instead of DVP to pins:
			 * conf_pad_pio[5] = 0 */
			if (sc7_29 == NULL)
				sc7_29 = sysconf_claim(SYS_CFG, 7, 29, 29, "asc");
			sysconf_write(sc7_29, 0);
		}

		switch (ascs[i]) {
		case 0:
			/* Route UART0 instead of PDES to pins.
			 * pdes_scmux_out = 0 */
			sc = sysconf_claim(SYS_CFG, 0,0,0, "asc");
			sysconf_write(sc, 0);
			break;

		case 1:
			/* Route UART1 RTS/CTS instead of dvo to pins.
			 * conf_pad_pio[0] = 0 */
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				sc = sysconf_claim(SYS_CFG, 7, 24, 24, "asc");
				sysconf_write(sc, 0);
			}
			break;

		case 2:
			/* Route UART2&3 or SCI inputs instead of DVP to pins.
			 * conf_pad_dvp = 0 */
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				sc = sysconf_claim(SYS_CFG, 40, 16, 16, "asc");
				sysconf_write(sc, 0);
			}

			/* Route UART2&3/SCI outputs instead of DVP to pins.
			 * conf_pad_pio[1]=0 */
			sc = sysconf_claim(SYS_CFG, 7, 25, 25, "asc");
			sysconf_write(sc, 0);

			/* No idea, more routing.
			 * conf_pad_pio[0] = 0 */
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				sc = sysconf_claim(SYS_CFG, 7, 24, 24, "asc");
				sysconf_write(sc, 0);
			}
			break;

		case 3:
			/* No idea, more routing.
			 * conf_pad_pio[4] = 0 */
			if (!(flags & STASC_FLAG_NORTSCTS)) {
				sc = sysconf_claim(SYS_CFG, 7, 28, 28, "asc");
				sysconf_write(sc, 0);
			}
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
static int __init stb7200_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stb7200_add_asc);

/* Early resources (sysconf and PIO) --------------------------------------- */

static struct platform_device sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd704000,
			.end	= 0xfd7041d3,
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
	STPIO_DEVICE(0, 0xfd020000, ILC_IRQ(96) ),
	STPIO_DEVICE(1, 0xfd021000, ILC_IRQ(97) ),
	STPIO_DEVICE(2, 0xfd022000, ILC_IRQ(98) ),
	STPIO_DEVICE(3, 0xfd023000, ILC_IRQ(99) ),
	STPIO_DEVICE(4, 0xfd024000, ILC_IRQ(100) ),
	STPIO_DEVICE(5, 0xfd025000, ILC_IRQ(101) ),
	STPIO_DEVICE(6, 0xfd026000, ILC_IRQ(102) ),
	STPIO_DEVICE(7, 0xfd027000, ILC_IRQ(103) ),
};

/* Initialise devices which are required early in the boot process. */
void __init stx7200_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&sysconf_device, 1);
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices),
		ILC_FIRST_IRQ+ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) +1;
	boot_cpu_data.cut_major = chip_revision;

	printk("STx7200 version %ld.x\n", chip_revision);

	/* ClockgenB powers up with all the frequency synths bypassed.
	 * Enable them all here.  Without this, USB 1.1 doesn't work,
	 * as it needs a 48MHz clock which is separate from the USB 2
	 * clock which is derived from the SATA clock. */
	ctrl_outl(0, 0xFD701048);

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
			.start	= 0xfd804000,
			.end	= 0xfd804000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

/* Pre-arch initialisation ------------------------------------------------- */

static int __init stx7200_postcore_setup(void)
{
	emi_init(0, 0xfdf00000);

	return 0;
}
postcore_initcall(stx7200_postcore_setup);

/* Late resources ---------------------------------------------------------- */

static struct platform_device emi = STEMI();

static struct platform_device stx7200_lpc =
	STLPC_DEVICE(0xfd008000, ILC_IRQ(120), IRQ_TYPE_EDGE_FALLING,
			0, 1, "CLKB_LPC");

static struct platform_device *stx7200_devices[] __initdata = {
	&stx7200_fdma_devices[0],
	/* &stx7200_fdma_devices[1], */
	&stx7200_fdma_xbar_device,
	&sysconf_device,
	&ilc3_device,
	&hwrandom_rng_device,
	&devrandom_rng_device,
	&emi,
	&stx7200_lpc,
};

#include "./platform-pm-stx7200.c"

static int __init stx7200_devices_setup(void)
{
	pio_late_setup();

	platform_add_pm_devices(stx7200_pm_devices,
				ARRAY_SIZE(stx7200_pm_devices));

	return platform_add_devices(stx7200_devices,
				    ARRAY_SIZE(stx7200_devices));
}
device_initcall(stx7200_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	TMU0, TMU1, TMU2, RTC, SCIF, WDT, HUDI,
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
};

static struct intc_prio_reg prio_registers[] = {
					/*  15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */ { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */ {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */ {    0,    0,    0,  HUDI } },
};

static DECLARE_INTC_DESC(intc_desc, "stx7200", vectors, NULL,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int irq;
	struct sysconf_field *sc;

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);

	for (irq=0; irq<16; irq++) {
		set_irq_chip(irq, &dummy_irq_chip);
		set_irq_chained_handler(irq, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
