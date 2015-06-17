/*
 * arch/sh/boards/st/sequoia7104idtv/setup.c
 *
 * Copyright (C) 2010 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics SEQUOIA7104-IDTV support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/emi.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

#define SEQ7104IDTV_GPIO_FLASH_WP	stpio_to_gpio(11, 4)
#define SEQ7104IDTV_GPIO_PCI_IDSEL	stpio_to_gpio(10, 2)
#define SEQ7104IDTV_GPIO_PCI_SERR	stpio_to_gpio(15, 4)
#define SEQ7104IDTV_GPIO_PCI_RESET	stpio_to_gpio(15, 7)
#define SEQ7104IDTV_GPIO_POWER_ON_ETH	stpio_to_gpio(15, 5)
#define SEQ7104IDTV_GPIO_SPI_BOOT_CLK	stpio_to_gpio(15, 0)
#define SEQ7104IDTV_GPIO_SPI_BOOT_DOUT	stpio_to_gpio(15, 1)
#define SEQ7104IDTV_GPIO_SPI_BOOT_NOTCS	stpio_to_gpio(15, 2)
#define SEQ7104IDTV_GPIO_SPI_BOOT_DIN	stpio_to_gpio(15, 3)

static int ascs[] __initdata = {
	2 | (STASC_FLAG_NORTSCTS << 8),
	3 | (STASC_FLAG_NORTSCTS << 8),
};

static void __init seq7104idtv_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics SEQUOIA7104-IDTV board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, ARRAY_SIZE(ascs), 0);
}

static struct plat_ssc_data ssc_private_info = {
	.capability =
		ssc0_has(SSC_UNCONFIGURED) |
		ssc1_has(SSC_UNCONFIGURED) |
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

static int seq7104idtv_phy_reset(void *bus)
{
	gpio_set_value(SEQ7104IDTV_GPIO_POWER_ON_ETH, 0);
	udelay(100);
	gpio_set_value(SEQ7104IDTV_GPIO_POWER_ON_ETH, 1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &seq7104idtv_phy_reset,
};

static struct platform_device seq7104idtv_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME, should be ILC_EXT_IRQ(6), */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct mtd_partition mtd_parts_table[] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data seq7104idtv_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device seq7104idtv_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 128 * 1024 * 1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &seq7104idtv_physmap_flash_data,
	},
};

/* Configuration for Serial Flash */
static struct mtd_partition serialflash_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p64",
};

static struct spi_board_info spi_serialflash[] = {
	{
		.modalias	= "m25p80",
		.bus_num	= 8,
		.chip_select	= spi_set_cs(15, 2),
		.max_speed_hz	= 500000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};

static struct platform_device spi_pio_device[] = {
	{
		.name		= "spi_st_pio",
		.id		= 8,
		.num_resources	= 0,
		.dev		= {
			.platform_data =
				&(struct ssc_pio_t) {
					.pio = {{15, 0}, {15, 1}, {15, 3} },
				},
		},
	},
};
/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data nand_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset	= 0,
	.rbn_port		= -1,
	.rbn_pin		= -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,	/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 50,	/* in us */
	},
	.flex_rbn_connected	= 1,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device nand_device =
STM_NAND_DEVICE("stm-nand-flex", 2, &nand_config,
		nand_parts, ARRAY_SIZE(nand_parts), NAND_USE_FLASH_BBT);

static struct platform_device *seq7104idtv_devices[] __initdata = {
	&seq7104idtv_physmap_flash,
	&seq7104idtv_phy_device,
	&spi_pio_device[0],
};

/* PCI configuration */
static struct pci_config_data seq7104idtv_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in device_init() */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = SEQ7104IDTV_GPIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	u32 bank1_start;
	u32 bank2_start;
	struct sysconf_field *sc;
	u32 boot_mode;

	bank1_start = emi_bank_base(1);
	bank2_start = emi_bank_base(2);

	/* Configure FLASH according to boot device mode pins */
	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_mode");
	boot_mode = sysconf_read(sc);
	if (boot_mode == 0x0)
		/* Default configuration */
		pr_info("Configuring FLASH for boot-from-NOR\n");
	else if (boot_mode == 0x1) {
		/* Swap NOR/NAND banks */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		seq7104idtv_physmap_flash.resource[0].start = bank1_start;
		seq7104idtv_physmap_flash.resource[0].end = bank2_start - 1;
		nand_device.id = 0;
	}

	/* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&seq7104idtv_pci_config, pin);
}

static int __init device_init(void)
{
	/* The IDSEL line is connected to PIO10.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(SEQ7104IDTV_GPIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(SEQ7104IDTV_GPIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "iptv7105: Failed to claim PCI_IDSEL PIO!\n");
	/* Setup the PCI_SERR# PIO */
	if (gpio_request(SEQ7104IDTV_GPIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(SEQ7104IDTV_GPIO_PCI_SERR);
		seq7104idtv_pci_config.serr_irq =
				gpio_to_irq(SEQ7104IDTV_GPIO_PCI_SERR);
		set_irq_type(seq7104idtv_pci_config.serr_irq,
			     IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "sequoia7104idtv: Failed to claim PCI SERR PIO!\n");
	}
	stx7105_configure_pci(&seq7104idtv_pci_config);

	stx7105_configure_sata(0);
	stx7105_configure_ssc(&ssc_private_info);

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	if (gpio_request(SEQ7104IDTV_GPIO_POWER_ON_ETH, "POWER_ON_ETH") == 0)
		gpio_direction_output(SEQ7104IDTV_GPIO_POWER_ON_ETH, 1);
	else
		printk(KERN_ERR "sequoia7104idtv: Failed to claim POWER_ON_ETH "
				"PIO!\n");
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);
	stx7105_configure_audio_pins(0, 0, 1, 0);

	/* just permanetly enable the flash*/
	if (gpio_request(SEQ7104IDTV_GPIO_FLASH_WP, "FLASH_WP") == 0)
		gpio_direction_output(SEQ7104IDTV_GPIO_FLASH_WP, 1);
	else
		printk(KERN_ERR "sequoia7104idtv: Failed to claim FLASH_WP PIO!\n");

	stx7105_configure_nand(&nand_device);
	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(seq7104idtv_devices,
				    ARRAY_SIZE(seq7104idtv_devices));
}
arch_initcall(device_init);

static void __iomem *seq7104idtv_ioport_map(unsigned long port,
					    unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_sequoia7104idtv __initmv = {
	.mv_name = "sequoia7104idtv",
	.mv_setup = seq7104idtv_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = seq7104idtv_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
