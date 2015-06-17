/*
 * arch/sh/boards/st/cb161/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Chris Tomlinson 
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Customer board 161 support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <asm/irq-ilc.h>

static int ascs[2] __initdata = { 2, 3 };

const char *LMI_VID_partalias [] = { "BPA2_Region1", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias [] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID_REGION1",
		.start = 0x81000000,
		.size  = 0x06000000,
	        .flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "gfx-memory",
		.start = 0x88000000,
		.size  = 0x09000000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "LMI_SYS_REGION0",
		.start = 0x4c000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	},
};

static void __init cb161_setup(char **cmdline_p)
{
	printk(KERN_NOTICE "STMicroelectronics STx7200 Mboard "
			"initialisation\n");

	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  = (
			ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
			ssc1_has(SSC_SPI_CAPABILITY) |
			ssc3_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) 
			) ,
};

static struct mtd_partition mtd_parts_table[] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	},
	{
		.name = "stui",
		.size = 0x00020000,
		.offset = 0x00040000,
	},
	{
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00060000,
	},
	{
		.name = "Spare",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00260000,
	}
};

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */
  /*
	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
  */
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 8*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

static struct mtd_partition nand0_parts[] = {
	{
		.name	= "NAND root 1",
		.offset	= 0,
		.size 	= 0x04000000
	}, {
		.name	= "NAND root 2",
		.offset	= MTDPART_OFS_APPEND,
		.size 	= 0x04000000
	}, {
		.name	= "NAND apps 1",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 0x08000000
	}, {
		.name	= "NAND apps 2",
		.offset	= MTDPART_OFS_APPEND,
		.size 	= 0x08000000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct mtd_partition nand1_parts[] = {
	{
		.name	= "NAND BD",
		.offset	= 0,
		.size 	= MTDPART_SIZ_FULL,
	}
};


/* Timing data for onboard NAND */
static struct emi_timing_data nand_timing_data = {
	.rd_cycle_time	 = 40,		 /* times in ns */
	.rd_oee_start	 = 0,
	.rd_oee_end	 = 10,
	.rd_latchpoint	 = 10,

	.busreleasetime  = 10,
	.wr_cycle_time	 = 40,
	.wr_oee_start	 = 0,
	.wr_oee_end	 = 10,

	.wait_active_low = 0,
};

static struct nand_config_data cb161_nand_config0 = {
	.emi_bank		= 1,
	.emi_withinbankoffset	= 0,

	.emi_timing_data	= &nand_timing_data,

	.chip_delay		= 25,
	.mtd_parts		= &nand0_parts,
	.nr_parts		= ARRAY_SIZE(nand0_parts),
	.rbn_port		= 2,
	.rbn_pin		= 7,
};

static struct nand_config_data cb161_nand_config1 = {
	.emi_bank		= 2,
	.emi_withinbankoffset	= 0,

	.emi_timing_data	= &nand_timing_data,

	.chip_delay		= 25,
	.mtd_parts		= &nand1_parts,
	.nr_parts		= ARRAY_SIZE(nand1_parts),
	.rbn_port		= 2,
	.rbn_pin		= 7,
};



static void __init stx7200_configure_dvo(void)
{
	static struct stpio_pin *dvoclk_pio;
	static struct stpio_pin *dvohvsync_pio[2];
	static struct stpio_pin *dvodata_pio[24];
	struct sysconf_field *sc;
		
	dvoclk_pio      = stpio_request_pin(4,1,"DVO Clk"   ,STPIO_ALT_OUT);
	dvodata_pio[0]  = stpio_request_pin(1,3,"DVO Data0" ,STPIO_ALT_OUT);
	dvodata_pio[1]  = stpio_request_pin(1,4,"DVO Data1" ,STPIO_ALT_OUT);
	dvodata_pio[2]  = stpio_request_pin(1,5,"DVO Data2" ,STPIO_ALT_OUT);
	dvodata_pio[3]  = stpio_request_pin(1,6,"DVO Data3" ,STPIO_ALT_OUT);
	dvodata_pio[4]  = stpio_request_pin(1,7,"DVO Data4" ,STPIO_ALT_OUT);
	dvodata_pio[5]  = stpio_request_pin(2,3,"DVO Data5" ,STPIO_ALT_OUT);
	dvodata_pio[6]  = stpio_request_pin(2,4,"DVO Data6" ,STPIO_ALT_OUT);
	dvodata_pio[7]  = stpio_request_pin(2,5,"DVO Data7" ,STPIO_ALT_OUT);
	dvodata_pio[8]  = stpio_request_pin(2,6,"DVO Data8" ,STPIO_ALT_OUT);
	dvodata_pio[9]  = stpio_request_pin(3,0,"DVO Data9" ,STPIO_ALT_OUT);
	dvodata_pio[10] = stpio_request_pin(3,1,"DVO Data10",STPIO_ALT_OUT);
	dvodata_pio[11] = stpio_request_pin(3,2,"DVO Data11",STPIO_ALT_OUT);
	dvodata_pio[12] = stpio_request_pin(3,3,"DVO Data12",STPIO_ALT_OUT);
	dvodata_pio[13] = stpio_request_pin(3,4,"DVO Data13",STPIO_ALT_OUT);
	dvodata_pio[14] = stpio_request_pin(3,5,"DVO Data14",STPIO_ALT_OUT);
	dvodata_pio[15] = stpio_request_pin(3,6,"DVO Data15",STPIO_ALT_OUT);
	dvodata_pio[16] = stpio_request_pin(6,0,"DVO Data16",STPIO_ALT_OUT);
	dvodata_pio[17] = stpio_request_pin(6,2,"DVO Data17",STPIO_ALT_OUT);
	dvodata_pio[18] = stpio_request_pin(7,2,"DVO Data18",STPIO_ALT_OUT);
	dvodata_pio[19] = stpio_request_pin(7,3,"DVO Data19",STPIO_ALT_OUT);
	dvodata_pio[20] = stpio_request_pin(7,4,"DVO Data20",STPIO_ALT_OUT);
	dvodata_pio[21] = stpio_request_pin(7,5,"DVO Data21",STPIO_ALT_OUT);
	dvodata_pio[22] = stpio_request_pin(7,6,"DVO Data22",STPIO_ALT_OUT);
	dvodata_pio[23] = stpio_request_pin(7,7,"DVO Data23",STPIO_ALT_OUT);
	dvohvsync_pio[0] = stpio_request_pin(4,0,"DVO Hsync" ,STPIO_ALT_OUT);
	dvohvsync_pio[1] = stpio_request_pin(3,7,"DVO Vsync" ,STPIO_ALT_OUT);

	sc = sysconf_claim(SYS_CFG, 7, 26, 27, NULL);
	if (sc)
	{
		// Enable DVO on PIOS
		sysconf_write(sc,0x3);
	}
}

static struct plat_stmmacphy_data phy_private_data[2] = {
	{
		/* MII0: SMSC LAN8700 */
		.bus_id = 0,
		.phy_addr = 0,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_RMII,
	}, {
		/* MII1: MB539B connected to J2 */
		.bus_id = 1,
		.phy_addr = 0,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_MII,
	}
};

static struct platform_device cb161_phy_devices[2] = {
	{
		.name		= "stmmacphy",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				/* This should be:
				 * .start = ILC_IRQ(93),
				 * .end = ILC_IRQ(93),
				 * but mode pins setup (MII0_RXD[3] pulled
				 * down) disables nINT pin of LAN8700, so
				 * we are unable to use it... */
				.start	= -1,
				.end	= -1,
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev = {
			.platform_data = &phy_private_data[0],
		}
	}, {
		.name		= "stmmacphy",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				.start	= ILC_IRQ(95),
				.end	= ILC_IRQ(95),
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev = {
			.platform_data = &phy_private_data[1],
		}
	}
};

static struct platform_device *cb161_devices[] __initdata = {
	&physmap_flash,
	&cb161_phy_devices[0],
	&cb161_phy_devices[1],
};

static int __init device_init(void)
{
	stx7200_configure_pwm(&pwm_private_info);
	stx7200_configure_ssc(&ssc_private_info);

	stx7200_configure_usb(0);

	stx7200_configure_sata(0);

	stx7200_configure_ethernet(0, 1, 0, 0);

	stx7200_configure_nand(&cb161_nand_config0);
	stx7200_configure_nand(&cb161_nand_config1);


	//stx7200_configure_lirc(NULL);
	stx7200_configure_dvo();

	return platform_add_devices(cb161_devices, ARRAY_SIZE(cb161_devices));
}
arch_initcall(device_init);

static void __iomem *cb161_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init cb161_init_irq(void)
{
  //epld_early_init(&epld_device);

#if defined(CONFIG_SH_ST_STEM)
	/* The off chip interrupts on the cb161 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active. This will result in
	 * SYS_ITRQ[3..0] = 0100.
	 *
	 * BTW. According to EPLD code author - "masking" interrupts
	 * means "enabling" them... Just to let you know... ;-)
	 */
  //	epld_write(0xff, EPLD_INTMASK0CLR);
  //	epld_write(0xff, EPLD_INTMASK1CLR);
	/* IntPriority(4) <= not STEM_notINTR0 */
  //	epld_write(1 << 4, EPLD_INTMASK0SET);
#endif
}

struct sh_machine_vector mv_cb161 __initmv = {
	.mv_name		= "cb161",
	.mv_setup		= cb161_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= cb161_init_irq,
	.mv_ioport_map		= cb161_ioport_map,
};
