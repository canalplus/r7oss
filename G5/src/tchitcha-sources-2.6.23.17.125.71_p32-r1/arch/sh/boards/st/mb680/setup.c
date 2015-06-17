/*
 * arch/sh/boards/st/mb680/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7105 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/emi.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <sound/stm.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include "../common/common.h"
#include "../common/mb705-epld.h"



#define MB680_GPIO_PCI_SERR stpio_to_gpio(6, 4)



static int ascs[2] __initdata = { 2, 3 };

const char *LMI_SYS_partalias[] = { "BPA2_Region0", "v4l2-coded-video-buffers",  
		"BPA2_Region1", "v4l2-video-buffers" ,"coredisplay-video", NULL };

//const char *GFX_partalias[] = { "gfx-memory", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_SYS",
		.start = 0x44400000,
		.size  = 0x04400000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	},
	{
		.name  = "bigphysarea",
		.start = 0x48800000,
		.size  = 0x01800000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "gfx-memory-1",
		.start = 0x4a000000,
		.size  = 0x02000000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "gfx-memory-0",
		.start = 0x4c000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	},	
};

static void __init mb680_setup(char** cmdline_p)
{
	printk("STMicroelectronics STx7105 Mboard initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
	.routing	= PWM_OUT0_PIO13_0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_UNCONFIGURED) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY),
	.routing =
		SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO3_5 |
		SSC3_SCLK_PIO3_6 | SSC3_MTSR_PIO3_7,
};

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 1,
		.oc_actlow = 1,
		.oc_pinsel = USB0_OC_PIO4_4,
		.pwr_en = 1,
		.pwr_pinsel = USB0_PWR_PIO4_5,
	}, {
		.oc_en = 1,
		.oc_actlow = 1,
		.oc_pinsel = USB1_OC_PIO4_6,
		.pwr_en = 1,
		.pwr_pinsel = USB1_PWR_PIO4_7,
	}
};

static struct platform_device mb680_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "LD5",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(2, 4),
			},
			{
				.name = "LD6",
				.gpio = stpio_to_gpio(2, 3),
			},
		},
	},
};

/*
 * mb680 rev C added software control of the PHY reset, and buffers which
 * allow isolation of the MII pins so that their use as MODE pins is not
 * compromised by the PHY.
 */
static struct stpio_pin *phy_reset, *switch_en;

/*
 * When connected to the mb705, MII reset is controlled by an EPLD register
 * on the mb705.
 * When used standalone a PIO pin is used, and J47-C must be fitted.
 *
 * Timings:
 *    PHY         | Reset low | Post reset stabilisation
 *    ------------+-----------+-------------------------
 *    DB83865     |   150uS   |         20mS
 *    LAN8700     |   100uS   |         800nS
 */
#ifdef CONFIG_SH_ST_MB705
static void ll_phy_reset(void)
{
	mb705_reset(EPLD_EMI_RESET_SW0, 150);
	mdelay(20);
}
#else
static void ll_phy_reset(void)
{
	stpio_set_pin(phy_reset, 0);
	udelay(150);
	stpio_set_pin(phy_reset, 1);
	mdelay(20);
}
#endif

static int mb680_phy_reset(void *bus)
{
	stpio_set_pin(switch_en, 1);
	ll_phy_reset();
	stpio_set_pin(switch_en, 0);

	return 0;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* National Semiconductor DP83865 (rev A/B) or SMSC 8700 (rev C) */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &mb680_phy_reset,
};

static struct platform_device mb680_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1, /*FIXME, should be ILC_EXT_IRQ(6), */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct mtd_partition nand1_parts[] = {
	{
		.name	= "NAND1 root",
		.offset	= 0,
		.size 	= MTDPART_SIZ_FULL,
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

static struct platform_device *mb680_devices[] __initdata = {
	&mb680_leds,
	&mb680_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
static lirc_scd_t mb680_lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

#ifdef CONFIG_SH_ST_MB705
static void mb705_epld_pci_reset(void)
{
	mb705_reset(EPLD_EMI_RESET_SW1, 1000);
	/* PCI spec says one second */
	mdelay(10);
}
#endif
/* PCI configuration */

static struct pci_config_data mb680_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in device_init() below */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	/* When connected to the mb705, PCI reset is controlled by an EPLD
	 * register on the mb705. When used standalone a PIO pin is used,
	 * and J47-D, J9-G must be fitted. */
#ifdef CONFIG_SH_ST_MB705
	.pci_reset = mb705_epld_pci_reset,
#else
	.pci_reset_gpio = stpio_to_gpio(15, 6),
#endif
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return  stx7105_pcibios_map_platform_irq(&mb680_pci_config, pin);
}

void __init mbxxx_configure_audio_pins(int *pcm_reader, int *pcm_player)
{
	*pcm_reader = -1;
	*pcm_player = 0;
	stx7105_configure_audio_pins(3, 0, 1, cpu_data->type == CPU_STX7105);
}

void __init mbxxx_configure_nand_flash(struct platform_device *nand_flash)
{
	stx7105_configure_nand(nand_flash);
}

static struct platform_device mb680_serial_flash_spi_bus = {
	.name           = "spi_st_pio",
	.id             = 8,
	.num_resources  = 0,
	.dev            = {
		.platform_data =
		&(struct ssc_pio_t) {
			.pio = {{15, 0}, {15, 1}, {15, 3} },
		},
	},
};

void __init mbxxx_configure_serial_flash(struct spi_board_info *serial_flash,
					 unsigned int n_devices)
{
	/* Specify CSn and SPI bus */
	serial_flash[0].bus_num = 8;
	serial_flash[0].chip_select = spi_set_cs(15, 2);

	/* Register SPI bus and flash devices */
	platform_device_register(&mb680_serial_flash_spi_bus);
	spi_register_board_info(serial_flash, n_devices);
}

static int __init device_init(void)
{
	/* Setup the PCI_SERR# PIO
	 * J20-A - open, J27-E - closed */
	if (gpio_request(MB680_GPIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(MB680_GPIO_PCI_SERR);
		mb680_pci_config.serr_irq = gpio_to_irq(MB680_GPIO_PCI_SERR);
		set_irq_type(mb680_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "mb680: Failed to claim PCI_SERR PIO!\n");
	}
	stx7105_configure_pci(&mb680_pci_config);

	stx7105_configure_sata(0);

	/* Valid only for mb680 rev. A & rev. B (they had two SATA lines) */
	stx7105_configure_sata(1);

	stx7105_configure_pwm(&pwm_private_info);
	stx7105_configure_ssc(&ssc_private_info);

	/*
	 * Note that USB port configuration depends on jumper
	 * settings:
	 *
	 *	  PORT 0	       		PORT 1
	 *	+-----------------------------------------------------------
	 * norm	|  4[4]	J5A:2-3			 4[6]	J10A:2-3
	 * alt	| 12[5]	J5A:1-2  J6F:open	14[6]	J10A:1-2  J11G:open
	 * norm	|  4[5]	J5B:2-3			 4[7]	J10B:2-3
	 * alt	| 12[6]	J5B:1-2  J6G:open	14[7]	J10B:1-2  J11H:open
	 */
	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_reset = stpio_request_set_pin(5, 5, "ResetMII", STPIO_OUT, 1);
	switch_en = stpio_request_set_pin(11, 2, "MIIBusSwitch", STPIO_OUT, 1);
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 1, 0, 0, 0);

	stx7105_configure_lirc(&mb680_lirc_scd);
	stx7105_configure_tsin(0);
	stx7105_configure_tsin(1);

	stx7105_configure_nand(&nand_devices[0]);
	
	return platform_add_devices(mb680_devices, ARRAY_SIZE(mb680_devices));
}
arch_initcall(device_init);


static void __iomem *mb680_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

static void __init mb680_init_irq(void)
{
#ifndef CONFIG_SH_ST_MB705
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
#endif
}

struct sh_machine_vector mv_mb680 __initmv = {
	.mv_name = "mb680",
	.mv_setup = mb680_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_init_irq = mb680_init_irq,
	.mv_ioport_map = mb680_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
