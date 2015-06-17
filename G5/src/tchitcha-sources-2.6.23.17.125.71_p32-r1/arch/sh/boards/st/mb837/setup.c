/*
 * arch/sh/boards/st/mb837/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/platform_device.h>
#include <linux/stm/emi.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/bpa2.h>


/* PCF8575 I2C PIO Extender (IC12) */
#define PIO_EXTENDER_BASE 220
#define PIO_EXTENDER_GPIO(port, pin) (PIO_EXTENDER_BASE + (port * 8) + (pin))



#define MB837_NOTPIORESETMII0 PIO_EXTENDER_GPIO(1, 1)
#define MB837_NOTPIORESETMII1 PIO_EXTENDER_GPIO(1, 2)


const char *player_buffers[] = { "BPA2_Region0", "BPA2_Region1", "v4l2-video-buffers",
				 "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };
//const char *bigphys[]        = { "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
        
	{
		.name  = "Player_Buffers", //96MB
		.start = 0,
		.size  = 0x07000000,
		.flags = 0,
		.aka   = player_buffers
	},
        /*
	{
		.name  = "Bigphys_Buffers", //64MB
		.start = 0,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = bigphys
	},
	
	{
		.name  = "gfx-memory",  //64MB
		.start = 0x00000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	}
	*/
};


static void __init mb837_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi7108-MBOARD (mb837) "
			"initialisation\n");
	stx7108_early_device_init();

	stx7108_configure_asc(2, &(struct stx7108_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio21_0,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio21_1,
			.routing.asc3.cts = stx7108_asc3_cts_pio21_4,
			.routing.asc3.rts = stx7108_asc3_rts_pio21_3,
			.hw_flow_control = 1, });
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}



static struct platform_device mb837_led = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "Heartbeat (LD6)", /* J23 1-2 */
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(5, 4),
			},
		},
	},
};



/* J14-B must be fitted */
static int mb837_mii0_phy_reset(void *bus)
{
	static int requested;

	if (!requested && gpio_request(MB837_NOTPIORESETMII0,
				"notPioResetMii0") == 0) {
		gpio_direction_output(MB837_NOTPIORESETMII0, 1);
		requested = 1;
	} else {
		pr_warning("mb837: Failed to request notPioResetMii0!\n");
	}

	if (requested) {
		gpio_set_value(MB837_NOTPIORESETMII0, 0);
		udelay(15000);
		gpio_set_value(MB837_NOTPIORESETMII0, 1);
	}

	return 0;
}

/* J14-A must be fitted */
static int mb837_mii1_phy_reset(void *bus)
{
	static int requested;

	if (!requested && gpio_request(MB837_NOTPIORESETMII1,
				"notPioResetMii1") == 0) {
		gpio_direction_output(MB837_NOTPIORESETMII1, 1);
		requested = 1;
	} else {
		pr_warning("mb837: Failed to request notPioResetMii1!\n");
	}

	if (requested) {
		gpio_set_value(MB837_NOTPIORESETMII1, 0);
		udelay(15000);
		gpio_set_value(MB837_NOTPIORESETMII1, 1);
	}

	return 0;
}

static struct platform_device mb837_phy_devices[] = {
	{
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &mb837_mii0_phy_reset,
		},
	}, {
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct plat_stmmacphy_data) {
			.bus_id = 2,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &mb837_mii1_phy_reset,
		},
	}
};

static struct fixed_phy_status fixed_phy_status[2] = {
	{
		.link = 1,
		.speed = 100,
	}, {
		.link = 1,
		.speed = 100,
	}
};



/* PCF8575 I2C PIO Extender (IC12) */
static struct i2c_board_info mb837_pio_extender = {
	I2C_BOARD_INFO("pcf857x", 0x27),
	.type = "pcf8575",
	.platform_data = &(struct pcf857x_platform_data) {
		.gpio_base = PIO_EXTENDER_BASE,
	},
};



/* Configuration based on Futarque-RC signals train. */
static lirc_scd_t mb837_lirc_scd = {
	.code = 0x3fffc028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};



static struct platform_device *mb837_devices[] __initdata = {
	&mb837_led,
	&mb837_phy_devices[0],
	&mb837_phy_devices[1],
};

void __init mbxxx_configure_nand_flash(struct platform_device *nand_flash)
{
	stx7108_configure_nand(nand_flash);
}

/* SPI PIO Bus for Serial Flash on mb705 peripheral board:
 *	CLK	J42-F closed (J42-E open)
 *	CSn	J42-H closed (J42-G open)
 *      DIn	J30-A closed
 *	DOut	J30-B closed
 * Note, leave SSC0 (I2C MII1) unconfigured  */
static struct platform_device mb837_serial_flash_spi_bus = {
	.name           = "spi_st_pio",
	.id             = 8,
	.num_resources  = 0,
	.dev            = {
		.platform_data =
		&(struct ssc_pio_t) {
			.pio = {{1, 6}, {2, 1}, {2, 0} },
		},
	},
};

void __init mbxxx_configure_serial_flash(struct spi_board_info *serial_flash,
					 unsigned int n_devices)
{
	/* Specify CSn and SPI bus */
	serial_flash[0].bus_num = 8;
	serial_flash[0].chip_select = spi_set_cs(1, 7);

	/* Register SPI bus and flash devices */
	platform_device_register(&mb837_serial_flash_spi_bus);
	spi_register_board_info(serial_flash, n_devices);
}



/* PCI configuration */
static struct pci_config_data mb837_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_DEFAULT,
		[3] = PCI_PIN_DEFAULT,
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = PCI_PIN_UNUSED,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7108_pcibios_map_platform_irq(&mb837_pci_config, pin);
}

void __init mbxxx_configure_audio_pins(int *pcm_reader, int *pcm_player)
{
	*pcm_reader = -1;
	*pcm_player = 2;
	stx7108_configure_audio(&(struct stx7108_audio_config) {
			.pcm_output_mode = stx7108_audio_pcm_output_8_channels,
			.pcm_input_enabled = 1,
			.spdif_output_enabled = 1, });
}


static int __init mb837_device_init(void)
{
	int ssc2_i2c;
	int phy_bus;

	/* PCI jumper settings:
	 * J18 not fitted, J19 2-3, J30-G fitted, J34-B fitted,
	 * J35-A fitted, J35-B not fitted, J35-C fitted, J35-D not fitted,
	 * J39-E fitted, J39-F not fitted */
	stx7108_configure_pci(&mb837_pci_config);

	/* MII1 & TS Connectors (inc. Cable Card one) - J42E & J42G */
	/* Disable if using Serial FLASH on mb705 peripheral board */
	/*
	  stx7108_configure_ssc_i2c(0, NULL);
	*/

	/* STRec to going to MB705 - J41C & J41D */
	stx7108_configure_ssc_i2c(1, NULL);
	/* MII0 & PIO Extender - J42A & J42C */
	ssc2_i2c = stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio1_3,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio1_4, });
	/* NIM AB - J25 1-2 & J27 1-2 */
	stx7108_configure_ssc_i2c(3, NULL);
	/* NIM CDI - J24 1-2 & J26 1-2 */
	stx7108_configure_ssc_i2c(5, NULL);
	/* HDMI - J55C & J55D */
	stx7108_configure_ssc_i2c(6, NULL);

	/* PIO extender is connected to SSC2 */
	i2c_register_board_info(ssc2_i2c, &mb837_pio_extender, 1);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
			.rx_mode = stx7108_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1,
			.scd = &mb837_lirc_scd, });

	/* J56A & J56B fitted */
	stx7108_configure_usb(0);
	/* J53A & J53B fitted */
	stx7108_configure_usb(1);
	/* J53C & J53D fitted */
	stx7108_configure_usb(2);

	stx7108_configure_sata(0);
	stx7108_configure_sata(1);

#ifdef CONFIG_SH_ST_MB837_STMMAC0
#ifdef CONFIG_SH_ST_MB837_STMMAC0_FIXED_PHY
	/* Use fixed phy bus (bus 0) phy 1 */
	phy_bus = STMMAC_PACK_BUS_ID(0, 1);
	BUG_ON(fixed_phy_add(PHY_POLL, 1, &fixed_phy_status[0]));
#else
	/* Use stmmacphy bus 1 defined above */
	phy_bus = 1;
#endif
	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = phy_bus });
#endif

#ifdef CONFIG_SH_ST_MB837_STMMAC1
#ifdef CONFIG_SH_ST_MB837_STMMAC1_FIXED_PHY
	/* Use fixed phy bus (bus 0) phy 2 */
	phy_bus = STMMAC_PACK_BUS_ID(0, 2);
	BUG_ON(fixed_phy_add(PHY_POLL, 2, &fixed_phy_status[1]));
#else
	/* Use stmmacphy bus 2 defined above */
	phy_bus = 2;
#endif
	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = phy_bus });
#endif
	/* MMC Hardware settings:
	 * - Jumpers to be set:
	 * 	J52[A..H] , J51[A..C] , J51-D, J42-B and J42-D
	 * - Jumpers to be unset: J42-A and J42-C
	 * HW workaround on board:
	 * MMC lines pull-up resistors
	 * In mb837 v1.0 schematic, the following pull-up values are wrong:
	 * R(MMC_Cmd) = R258-2 = 61k and  R(MMC_Data2) = R261 = 10k.
	 * They should be replaced by: R(MMC_Data2) = 61k R(MMC_Cmd) = 10k.
	 */
	stx7108_configure_mmc();

	return platform_add_devices(mb837_devices, ARRAY_SIZE(mb837_devices));
}
arch_initcall(mb837_device_init);

static void __iomem *mb837_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_mb837 __initmv = {
	.mv_name = "mb837",
	.mv_setup = mb837_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = mb837_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
