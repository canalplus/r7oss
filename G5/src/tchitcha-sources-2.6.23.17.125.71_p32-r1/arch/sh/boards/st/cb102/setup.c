/*
 * arch/sh/boards/st/cb102/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Customer Board 102 support.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <asm/irq-ilc.h>
#include <sound/stm.h>

static int ascs[2] __initdata = {
	2 | (STASC_FLAG_NORTSCTS << 8), /* TTL-level test points only... */
	3 | (STASC_FLAG_NORTSCTS << 8), /* J8001 (3.5mm jack) */
};

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "gfx-memory", "v4l2-video-buffers", NULL };
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
		.name  = "LMI_SYS",
		.start = 0,
		.size  = 0x05000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};


static void __init cb102_setup(char **cmdline_p)
{
	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 1);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data cb102_ssc_private_info = {
	.capability = (ssc0_has(SSC_I2C_CAPABILITY) |
			ssc1_has(SSC_SPI_CAPABILITY) |
			ssc2_has(SSC_I2C_CAPABILITY) |
			ssc3_has(SSC_UNCONFIGURED) |
			ssc4_has(SSC_I2C_CAPABILITY)),
};

static struct mtd_partition cb102_mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data cb102_physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(cb102_mtd_parts_table),
	.parts		= cb102_mtd_parts_table
};

static struct platform_device cb102_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &cb102_physmap_flash_data,
	},
};

static int cb102_phy_reset(void *bus)
{
	static struct stpio_pin *ethreset;

	if (ethreset == NULL)
		ethreset = stpio_request_set_pin(4, 7, "nETH_RESET",
				STPIO_OUT, 1);

	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);
	udelay(1000);
	stpio_set_pin(ethreset, 1);

	return 0;
}

static struct plat_stmmacphy_data cb102_phy_private_data = {
	/* MAC0: SMSC LAN8700 */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = cb102_phy_reset,
};

static struct platform_device cb102_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			/* This should be:
			 * .start = ILC_IRQ(93),
			 * .end = ILC_IRQ(93),
			 * but mode pins setup (all pulled down)
			 * disables nINT pin of LAN8700, so
			 * we are unable to use it... */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &cb102_phy_private_data,
	}
};

#ifdef CONFIG_SND
/* ALSA dummy converters for PCM inputs, to configure required
 * Left Justified mode */

static struct platform_device cb102_snd_spdif_analog_input = {
	.name = "snd_conv_dummy",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "SPDIF/Analog Input",
		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};

static struct platform_device cb102_snd_hdmi_input = {
	.name = "snd_conv_dummy",
	.id = 1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "HDMI Input",
		.source_bus_id = "snd_pcm_reader.1",
		.channel_from = 0,
		.channel_to = 7,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};
#endif

static struct platform_device *cb102_devices[] __initdata = {
	&cb102_physmap_flash,
	&cb102_phy_device,
#ifdef CONFIG_SND
	&cb102_snd_spdif_analog_input,
	&cb102_snd_hdmi_input,
#endif
};

static int __init device_init(void)
{
	struct stpio_pin *pio;

	stx7200_configure_ssc(&cb102_ssc_private_info);

	/* HDMI 5V line is controlled by PIO5.6 and should be controlled
	 * by someone (???) - so far there is no one like this, so will
	 * just enable it here... */
	pio = stpio_request_pin(5, 6, "HDMIOUT_5V_EN", STPIO_OUT);
	BUG_ON(pio == NULL);
	stpio_set_pin(pio, 1);

   	/* turn on a 12MHz signal to the USB bridge */
	pio = stpio_request_pin(5, 5, "USB_CLK_OUT", STPIO_ALT_OUT);
	BUG_ON(pio == NULL);
   	*(unsigned *)0xfd70411c &= ~(0x00004000); // pio 5 output clkgen b observation clock
   	*(unsigned *)0xfd70411c |= 0x10000000; // conf_pad_pio[4] = 1
   	*(unsigned *)0xfd701054 = (0x40 | 0x1c); // clkgnb_clkobs_mux_cfg - usb out(1c) divided by 4
   	/* and configure the reset gpio to the usb bridge and release reset */
	pio = stpio_request_pin(2, 7, "USB_BRIDGE_RESET", STPIO_OUT);
	stpio_set_pin(pio, 1);

	stx7200_configure_usb(0);
	stx7200_configure_usb(1);
	stx7200_configure_usb(2);

	stx7200_configure_ethernet(0, 0, 0, 0);

	stx7200_configure_lirc(NULL);

	return platform_add_devices(cb102_devices, ARRAY_SIZE(cb102_devices));
}
arch_initcall(device_init);

static void __iomem *cb102_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor
	 * core version register, which is guaranted to be available,
	 * and non-writable. */
	return (void __iomem *)CCN_PVR;
}

static void __init cb102_init_irq(void)
{
}

struct sh_machine_vector mv_cb102 __initmv = {
	.mv_name		= "cb102",
	.mv_setup		= cb102_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= cb102_init_irq,
	.mv_ioport_map		= cb102_ioport_map,
};
