/*
 * arch/sh/boards/st/cb104_mc/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Carl Shaw <carl.shaw@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Customer Board 104 support.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/sysconf.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <linux/dma-mapping.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>

#define UHOST2C_BASE(N)                 (0xfd200000 + ((N)*0x00100000))

#define AHB2STBUS_WRAPPER_GLUE_BASE(N)  (UHOST2C_BASE(N))
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)

static u64 st40_dma_mask = DMA_32BIT_MASK;
static struct sysconf_field *usb_power_sc[3];

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



static int ascs[2] __initdata = {
    2 | (STASC_FLAG_NORTSCTS << 8), /* J8001 (3.5mm jack) */
    3 | (STASC_FLAG_NORTSCTS << 8), /* TTL-level test points only... */
};

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
    {
        .name  = "LMI_VID",
        .start = 0x81000000,
        .size  = 0x07000000,
        .flags = 0,
        .aka   = LMI_VID_partalias
    },
    {
        .name  = "gfx-memory-0",
        .start = 0x88000000,
        .size  = 0x04000000,
        .flags = 0,
        .aka   = NULL
    },
    {
        .name  = "gfx-memory-1",
        .start = 0x8C000000,
        .size  = 0x04000000,
        .flags = 0,
        .aka   = NULL
    },
    {
        .name  = "LMI_SYS",
        .start = 0,
        .size  = 0x08000000,
        .flags = 0,
        .aka   = LMI_SYS_partalias
    }
};



static void __init cb104_setup(char **cmdline_p)
{
	pr_info("CB104 board initialisation\n");
	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data cb104_ssc_private_info = {
	.capability = (ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		       ssc1_has(SSC_SPI_CAPABILITY)               | 
		       ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		       ssc3_has(SSC_UNCONFIGURED) | ssc4_has(SSC_UNCONFIGURED))
};

static struct plat_ssc_data cb104_ssc_private_info_mfg = {
	.capability = (ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		       ssc1_has(SSC_SPI_CAPABILITY)|
		       ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		       ssc3_has(SSC_UNCONFIGURED)                 |
		       ssc4_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO))
};

static struct mtd_partition cb104_mtd_parts_table[] = {
   {
      .name = "Permanent u-boot",
      .size =   0x00020000,
      .offset = 0x00000000,
      /* mask_flags: MTD_WRITEABLE *//* force read-only */
   },
   {
      .name = "Working u-boot",
      .size =   0x00020000,
      .offset = 0x00020000,
   },
   {
      .name = "System-data",
      .size =   0x00010000,
      .offset = 0x00040000,
   },
   {
      .name = "HDCP data",
      .size =   0x00020000,
      .offset = 0x00050000,
   },
   {
      .name = "Kernel",
      .size =   0x00180000,
      .offset = 0x00070000,
      /* mask_flags: MTD_WRITEABLE *//* force read-only */
   },
   {
      .name = "File system",
      .size =   0x001c0000,
      .offset = 0x001f0000,
   },
   {
      .name = "Big files",
      .size   = 0x00420000,
      .offset = 0x003b0000,
   },
   {
      .name   = "Mfg data",
      .size   = 0x00020000,
      .offset = 0x007d0000,
   }
};

static struct physmap_flash_data cb104_physmap_flash_data = {
    .width      = 2,
    .nr_parts   = ARRAY_SIZE(cb104_mtd_parts_table),
    .parts      = cb104_mtd_parts_table
};

static struct platform_device cb104_physmap_flash = {
    .name       = "physmap-flash",
    .id     = -1,
    .num_resources  = 1,
    .resource   = (struct resource[]) {
        {
            .start      = 0x00000000,
            .end        = 32*1024*1024 - 1,
            .flags      = IORESOURCE_MEM,
        }
    },
    .dev        = {
        .platform_data  = &cb104_physmap_flash_data,
    },
};

static struct mtd_partition nand1_parts[] = {
   {
      .name   = "NAND UBI Space",
      .offset = 0x00000000,
      .size   = MTDPART_SIZ_FULL //256MB
   },
};

static struct plat_stmnand_data nand_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset	= 0,
	.rbn_port		= 2,
	.rbn_pin		= 7,

	/* STM_NAND_FLEX data */
	.flex_rbn_connected	= 1,

	/* STM_NAND_EMI/FLEX timing data */
	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
};

static struct platform_device nand_devices[] = {
	STM_NAND_DEVICE("stm-nand-emi", 1, &nand_config,
			nand1_parts, ARRAY_SIZE(nand1_parts), 0),
};

#ifdef CONFIG_SND
/* ALSA dummy converters for PCM inputs, to configure required
 * Left Justified mode */

static struct platform_device cb104_snd_spdif_analog_input = {
	.name = "snd_conv_dummy",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "SPDIF/Analog Input",
		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};

static struct platform_device cb104_snd_hdmi_input = {
	.name = "snd_conv_dummy",
	.id = 1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "HDMI Input",
		.source_bus_id = "snd_pcm_reader.1",
		.channel_from = 0,
		.channel_to = 7,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};
#endif

static struct platform_device *cb104_devices[] __initdata = {
	&cb104_physmap_flash,
#ifdef CONFIG_SND
	&cb104_snd_spdif_analog_input,
	&cb104_snd_hdmi_input,
#endif
};

static int __init device_init(void)
{
    const unsigned char oc_pins[3] = {0, 2, 5};
    struct sysconf_field *sc;
    int port;
    struct stpio_pin *pio;
    int ret;

    pio = stpio_request_pin(1,7,"Mfg", STPIO_IN);

    if (!stpio_get_pin(pio))
    {
       //we are in mfg mode
       printk("****We are in manufacturing mode.****\n");              

#ifdef CONFIG_SH_ST_CB104
       //enable power to external USB connectors
       pio = stpio_request_pin(7, 3, "USB1 power", STPIO_ALT_OUT);
       if (pio != NULL)
	  stpio_set_pin(pio, 1);
       pio = stpio_request_pin(7, 4, "USB2 power", STPIO_ALT_OUT);
       if (pio != NULL)
	  stpio_set_pin(pio, 1);
#endif

       //Configure ssc4 as a I2C bus.  This is only needed for
       //manufacturing.
       stx7200_configure_ssc(&cb104_ssc_private_info_mfg);
    }else{
       //we are in normal mode
       stx7200_configure_ssc(&cb104_ssc_private_info);
    }

#ifdef CONFIG_SH_ST_CB104_MC
    //enable power to external USB connectors
    pio = stpio_request_pin(7, 3, "USB1 power", STPIO_ALT_OUT);
    if (pio != NULL)
       stpio_set_pin(pio, 1);
    pio = stpio_request_pin(7, 4, "USB2 power", STPIO_ALT_OUT);
    if (pio != NULL)
       stpio_set_pin(pio, 1);
#endif

    stx7200_configure_nand(&nand_devices[0]);
    stx7200_configure_lirc(NULL);

    ret = platform_add_devices(cb104_devices, ARRAY_SIZE(cb104_devices));
    /* route USB and parts of MAFE instead of DVO.
     * conf_pad_pio[2] = 0 */
    sc = sysconf_claim(SYS_CFG, 7, 26, 26, "usb");
    sysconf_write(sc, 0);

    /* DVO output selection (probably ignored).
     * conf_pad_pio[3] = 0 */
    sc = sysconf_claim(SYS_CFG, 7, 27, 27, "usb");
    sysconf_write(sc, 0);

    for (port = 1; port < 3; port++)
    {
        usb_power_sc[port] = sysconf_claim(SYS_CFG, 22, 3+port, 3+port, "usb");
        sysconf_write(sc, 0);

        pio = stpio_request_pin(7, oc_pins[port], "USB oc", STPIO_IN);

        platform_device_register(&st_usb[port]);
    }

    return ret;
}
arch_initcall(device_init);

static void __iomem *cb104_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}


struct sh_machine_vector mv_cb104 __initmv = {
	.mv_name = "cb104",
	.mv_setup = cb104_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = cb104_ioport_map,
};

static int __init parse_onboard_partitions(char *line)
{
	char *cmdl = (line);	/* start scan from '=' char */
	int params[4];

	while (*cmdl) {
		cmdl = get_options(cmdl, 4, params);
		if (params[0] != 3) {
			printk("Bad parameters for MTD Partition. "
					"They should be: "
					"region-number,size,offset\n");
			return 1;
		}
		cb104_mtd_parts_table[params[1]].size = params[2];
		cb104_mtd_parts_table[params[1]].offset = params[3];
	}
	return 1;
}
__setup("mtd_partitions=", parse_onboard_partitions);
