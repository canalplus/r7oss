/*
 * arch/sh/boards/st/iptv7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics IPTV PLUGGIN board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/bpa2.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include "../common/common.h"

#define IPTV7105_GPIO_FLASH_WP stpio_to_gpio(6, 5)
#define IPTV7105_GPIO_PCI_IDSEL stpio_to_gpio(10, 2)
#define IPTV7105_GPIO_PCI_SERR stpio_to_gpio(15, 4)
#define IPTV7105_GPIO_POWER_ON_ETH stpio_to_gpio(15, 5)
#define IPTV7105_GPIO_PCI_RESET stpio_to_gpio(15, 7)
#define IPTV7105_GPIO_SPI_BOOT_CLK stpio_to_gpio(15, 0)
#define IPTV7105_GPIO_SPI_BOOT_DOUT stpio_to_gpio(15, 1)
#define IPTV7105_GPIO_SPI_BOOT_NOTCS stpio_to_gpio(15, 2)
#define IPTV7105_GPIO_SPI_BOOT_DIN stpio_to_gpio(15, 3)

const char *player_buffers[] = { "BPA2_Region0", "BPA2_Region1", "v4l2-video-buffers", NULL };
const char *bigphys[]        = { "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "Bigphys_Buffers", //64MB
		.start = 0x46400000,
		.size  = 0x01800000,      
		.flags = 0,
		.aka   = bigphys
	},
	{
		.name  = "Player_Buffers", //96MB
		.start = 0x47c00000,
		.size  = 0x04400000,
		.flags = 0,
		.aka   = player_buffers
	},
	{
		.name  = "gfx-memory-0",  //64MB
		.start = 0x4C000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	},
};

static int ascs[] __initdata = { 0, 2, 3 };

static void __init iptv7105_setup(char **cmdline_p)
{
	printk("STMicroelectronics STx7105 IPTVPluggin board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, ARRAY_SIZE(ascs), 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_UNCONFIGURED) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc3_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO),
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

static int iptv7105_phy_reset(void *bus)
{
	gpio_set_value(IPTV7105_GPIO_POWER_ON_ETH, 0);
	udelay(100);
	gpio_set_value(IPTV7105_GPIO_POWER_ON_ETH, 1);
	udelay(1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel KSZ8041FTL */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &iptv7105_phy_reset,
};

static struct platform_device iptv7105_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1, /* FIXME, should be ILC_EXT_IRQ(6) */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
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
	.type = "m25p32",
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num	= 0,
		.chip_select    = spi_set_cs(2, 4),
		.max_speed_hz   = 5000000,
		.platform_data  = &serialflash_data,
		.mode	   = SPI_MODE_3,
	},
};

/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
	{
		.name   = "Boot firmware",
		.offset = 0,
		.size   = 0x00100000
	}, {
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data nand_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset   = 0,
	.rbn_port		= -1,
	.rbn_pin		= -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup      = 50,	   /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay     = 50,	   /* in us */
	},
	.flex_rbn_connected     = 0,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM */
static struct platform_device nand_device =
STM_NAND_DEVICE("stm-nand-flex", 0, &nand_config,
		nand_parts, ARRAY_SIZE(nand_parts), NAND_USE_FLASH_BBT);

static struct platform_device *iptv7105_devices[] __initdata = {
	&iptv7105_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

/* PCI configuration */

static struct pci_config_data iptv7105_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = IPTV7105_GPIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx7105_pcibios_map_platform_irq(&iptv7105_pci_config, pin);
}

static int __init device_init(void)
{
	/* Configure the SPI Boot as input */
	if (gpio_request(IPTV7105_GPIO_SPI_BOOT_CLK, "SPI Boot CLK") == 0)
		gpio_direction_input(IPTV7105_GPIO_SPI_BOOT_CLK);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot CLK!\n");

	if (gpio_request(IPTV7105_GPIO_SPI_BOOT_DOUT, "SPI Boot DOUT") == 0)
		gpio_direction_input(IPTV7105_GPIO_SPI_BOOT_DOUT);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot DOUT!\n");

	if (gpio_request(IPTV7105_GPIO_SPI_BOOT_NOTCS, "SPI Boot NOTCS") == 0)
		gpio_direction_input(IPTV7105_GPIO_SPI_BOOT_NOTCS);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot NOTCS!\n");

	if (gpio_request(IPTV7105_GPIO_SPI_BOOT_DIN, "SPI Boot DIN") == 0)
		gpio_direction_input(IPTV7105_GPIO_SPI_BOOT_DIN);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot DIN!\n");

	/* The IDSEL line is connected to PIO10.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(IPTV7105_GPIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(IPTV7105_GPIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "iptv7105: Failed to claim PCI_IDSEL PIO!\n");
	/* Setup the PCI_SERR# PIO */
	if (gpio_request(IPTV7105_GPIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(IPTV7105_GPIO_PCI_SERR);
		iptv7105_pci_config.serr_irq =
				gpio_to_irq(IPTV7105_GPIO_PCI_SERR);
		set_irq_type(iptv7105_pci_config.serr_irq,
				IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_ERR "iptv7105: Failed to claim PCI SERR PIO!\n");
	}
	/* And finally! */
	stx7105_configure_pci(&iptv7105_pci_config);

	stx7105_configure_sata(0);
	stx7105_configure_ssc(&ssc_private_info);

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	if (gpio_request(IPTV7105_GPIO_POWER_ON_ETH, "POWER_ON_ETH") == 0)
		gpio_direction_output(IPTV7105_GPIO_POWER_ON_ETH, 1);
	else
		printk(KERN_ERR "iptv7105: Failed to claim POWER_ON_ETH "
				"PIO!\n");
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);

	stx7105_configure_lirc(&lirc_scd);

	stx7105_configure_audio_pins(1, 0, 0, 0);

	/* just permanetly enable the flash*/
	if (gpio_request(IPTV7105_GPIO_FLASH_WP, "FLASH_WP") == 0)
		gpio_direction_output(IPTV7105_GPIO_FLASH_WP, 1);
	else
		printk(KERN_ERR "iptv7105: Failed to claim FLASH_WP PIO!\n");

	stx7105_configure_nand(&nand_device);

	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(iptv7105_devices,
				    ARRAY_SIZE(iptv7105_devices));
}
arch_initcall(device_init);

static void __iomem *iptv7105_ioport_map(unsigned long port,
					   unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_iptv7105 __initmv = {
	.mv_name = "iptv7105",
	.mv_setup = iptv7105_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = iptv7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
