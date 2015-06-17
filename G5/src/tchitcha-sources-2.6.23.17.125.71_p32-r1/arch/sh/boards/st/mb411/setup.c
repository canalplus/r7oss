/*
 * arch/sh/boards/st/mb411/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 MBoard board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <sound/stm.h>
#include <asm/io.h>
#include <asm/mb411/epld.h>
#include <asm/irq-stb7100.h>
#include "../common/common.h"

static int ascs[2] __initdata = { 2, 3 };

static void __init mb411_setup(char** cmdline_p)
{
	printk("STMicroelectronics STb7100 MBoard board initialisation\n");

        stx7100_early_device_init();
        stb7100_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0 | PLAT_STM_PWM_OUT1,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0x03e00300,
		.end	= 0x03e00300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 7,
		.end	= 7,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

static struct mtd_partition mtd_parts_table[3] = {
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

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct resource physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
	.num_resources	= 1,
	.resource	= &physmap_flash_resource,
};

static struct plat_stmmacphy_data phy_private_data = {
        .bus_id = 0,
        .phy_addr = 0,
        .phy_mask = 0,
        .interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device mb411_phy_device = {
        .name           = "stmmacphy",
        .id             = 0,
        .num_resources  = 1,
        .resource       = (struct resource[]) {
                {
                        .name   = "phyirq",
                        .start  = 0,
                        .end    = 0,
                        .flags  = IORESOURCE_IRQ,
                },
        },
        .dev = {
                .platform_data = &phy_private_data,
         }
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= EPLD_BASE,
			.end	= EPLD_BASE + EPLD_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};

#ifdef CONFIG_SND
/* Unfortunately PDN and SMUTE can't be controlled using
 * default audio EPLD "firmware"...
 * That is why dummy converter is enough. */
static struct platform_device mb411_snd_ext_dacs = {
	.name = "snd_conv_dummy",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "External DACs",
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 9,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
	},
};
#endif

static struct platform_device *mb411_devices[] __initdata = {
	&epld_device,
	&smc91x_device,
	&physmap_flash,
	&mb411_phy_device,
#ifdef CONFIG_SND
	&mb411_snd_ext_dacs,
#endif
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	stx7100_configure_sata();
	stx7100_configure_pwm(&pwm_private_info);
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_lirc(&lirc_scd);
	stx7100_configure_ethernet(0, 0, 0);

#ifdef CONFIG_PATA_PLATFORM
	/* Set the EPLD ATAPI register to 1, enabling the IDE interface.*/
	epld_write(1, EPLD_ATAPI);
	stx7100_configure_pata(3, 1, 8);
#endif

#ifdef CONFIG_SND
	/* Initialize audio EPLD */
	epld_write(0x1, EPLD_DAC_PNOTS);
	epld_write(0x7, EPLD_DAC_SPMUX);
#endif

	return platform_add_devices(mb411_devices,
                                    ARRAY_SIZE(mb411_devices));
}
device_initcall(device_init);

static void __iomem *mb411_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb411_init_irq(void)
{
	unsigned long epldver;
	unsigned long pcbver;

	epld_early_init(&epld_device);

	epldver = epld_read(EPLD_EPLDVER);
	pcbver = epld_read(EPLD_PCBVER);
	printk("EPLD v%ldr%ld, PCB ver %lX\n",
	       epldver >> 4, epldver & 0xf, pcbver);

	/* Set the ILC to route external interrupts to the the INTC */
	/* Outputs 0-3 are the interrupt pins, 4-7 are routed to the INTC */
	ilc_route_external(ILC_EXT_IRQ0, 4, 0);
	ilc_route_external(ILC_EXT_IRQ1, 5, 0);
	ilc_route_external(ILC_EXT_IRQ2, 6, 0);

        /* Route e/net PHY interrupt to SH4 - only for STb7109 */
#ifdef CONFIG_STMMAC_ETH
	/* Note that we invert the signal - the ste101p is connected
	   to the mb411 as active low. The sh4 INTC expects active high */
	ilc_route_external(ILC_EXT_MDINT, 7, 1);
#else
	ilc_route_external(ILC_EXT_IRQ3, 7, 0);
#endif

	/* ...where they are handled as normal HARP style (encoded) interrpts */
	harp_init_irq();
}

static struct sh_machine_vector mv_mb411 __initmv = {
	.mv_name		= "mb411",
	.mv_setup		= mb411_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb411_init_irq,
	.mv_ioport_map		= mb411_ioport_map,
};
