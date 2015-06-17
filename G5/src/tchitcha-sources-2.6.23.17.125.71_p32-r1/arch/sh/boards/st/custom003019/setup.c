/*
 * arch/sh/boards/st/custom003019/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics custom003019-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <sound/stm.h>
#include <linux/stm/nand.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>

/* This global variable holds the RTE version number.*/
/* This information is retrieved from memory at boot time */
static uint8_t g_image_id = 0;
EXPORT_SYMBOL(g_image_id);

static struct i2c_board_info __initdata i2c_device_board_info[] = {
	{
		I2C_BOARD_INFO("STV0900" , 0x68),
	},
	{
		I2C_BOARD_INFO("ak4708" , 0x11),
	},
};

static int ascs[2] __initdata = {
	0 | (STASC_FLAG_SCSUPPORT << 8),	/* ASC0 = SCIF */
	1 | (STASC_FLAG_NORTSCTS << 8),		/* ASC1 = UART */
};


#ifdef CONFIG_BPA2

#include <linux/bpa2.h>

static const char *LMI_SYS_partalias_256[] = {
        "BPA2_Region0",
        "v4l2-coded-video-buffers",
        "BPA2_Region1",
        "v4l2-video-buffers",
        "coredisplay-video",
        NULL
};

static const char *bigphys_partalias_256[] = {
        "gfx-memory-1",
        NULL
};

static struct bpa2_partition_desc bpa2_parts_table[] = {
        {
                .name   = "LMI_SYS",            /* 69MB */
                .start  = 0,
                .size   = 69 * 1024 * 1024,
                .flags  = 0,
                .aka    = LMI_SYS_partalias_256
        },
        {
                .name   = "gfx-memory-0",       /* 16MB */
                .start  = 0x50000000 - 0x01000000,
                .size   = 0x01000000,
                .flags  = 0,
                .aka    = NULL
        },
        {
                .name   = "bigphysarea",        /* 19MB */
                .start  = 0x50000000 - 0x01000000 - 0x01300000,
                .size   = 0x01300000,
                .flags  = 0,
                .aka    = bigphys_partalias_256
        },
        {
                .name   = "scr",                /* 10MB */
                .start  = 0x50000000 - 0x01000000 - 0x01300000 - 0x00A00000,
                .size   = 0x00A00000,
                .flags  = 0,
                .aka    = NULL
        },
};

#endif  /* CONFIG_BPA2 */

#ifdef CONFIG_SH_G5_SPLASH
# define ZREPLAY_IN_KERNEL
# include "splash.h"
#endif

#define register_read32(addr) (*(volatile unsigned int *)(addr))
#define ST7105_PTI_BASE_ADDRESS	0xFE230000
#define PTI_OFFSET		0x9984
#define REBOOT_FLAG_OFFSET	12

static void __init custom003019_setup(char** cmdline_p)
{
	/* Retrieve Image Id from RAM. This is after decryption from FLASH */
	/* by BSL, and before that area of RAM is overwritten during boot. */
	/* --------------------------------------------------------------- */
	g_image_id = *(uint8_t *)0x80000005;


#ifdef CONFIG_SH_G5_SPLASH
	if (register_read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + REBOOT_FLAG_OFFSET) != 1)
		splash();
#endif

	printk("STMicroelectronics custom003019-SDK board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 1);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |	/* */
		ssc1_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |	/* HDMI */
		ssc2_has(SSC_UNCONFIGURED  ) |
		ssc3_has(SSC_UNCONFIGURED)

};

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 0,
		.oc_actlow = 0,
		.pwr_en = 0,
	}, {
		.oc_en = 0,
		.oc_actlow = 0,
		.pwr_en = 0,
	}
};

static struct mtd_partition mtd_parts_table[] = {
	{
		.name = "whole",
		.offset = 0x0,
		.size = 0x400000,
	},
	{
		.name = "factory_data",
		.offset = 0x000FC400,
		.size = 0x400,
	},
	{
		.name = "eeprom",
		.offset = 0x001D0000,
		.size = 0x20000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x00000000,
			.end	= 0x003FFFFF,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev            = {
		.platform_data	= &physmap_flash_data,
	},
};
	
#ifdef CONFIG_LEDS_GPIO
static struct platform_device custom003019_leds_device = {
        .name = "leds-gpio",
        .id = 0,
        .dev.platform_data = &(struct gpio_led_platform_data) {
                .num_leds = 1,
                .leds = (struct gpio_led[]) {
                        {
                                .name = "led:0",
                                .gpio = stpio_to_gpio(14, 1),
                                .active_low = 0,

                                .default_trigger = "timerserv",
                        },
                },
        },
};
#endif

#ifdef CONFIG_MTD_NAND

static struct mtd_partition nand_parts[] = {
	{
		.name	= "slc",
		.offset	= 0,
		.size	= MTDPART_SIZ_FULL
	},
	{
		.name	= "firmware",
		.offset	= 0,
		.size	= (64 * 1024UL * 1024UL)
	},
	{
		.name	= "data",
		.offset	= (64 * 1024UL * 1024UL),
		.size	= (63 * 1024UL * 1024UL)
	},
};

#ifdef CONFIG_MTD_NAND_STM_EMI

static struct plat_stmnand_data nand_config = {
	.emi_withinbankoffset	= 0,
	.rbn_port		= -1,
	.rbn_pin		= -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 25,		/* in us */
	},

	.flex_rbn_connected	= 0,
};

static struct platform_device nand_device = STM_NAND_DEVICE("stm-nand-emi", 2, &nand_config,
	nand_parts, ARRAY_SIZE(nand_parts), 0);

#endif	/* CONFIG_MTD_NAND_STM_EMI */
#endif	/* CONFIG_MTD_NAND */

void *stv090x_fe_state = NULL;
int (*stv090x_set_gpio_fct)(void *state, u8 gpio_number, u8 cfg_value) = NULL;

int set_ant_bridge_mode(u8 activate_bridge)
{
	if (!stv090x_fe_state || !stv090x_set_gpio_fct)
		return -ENXIO;

	if (activate_bridge)
		return stv090x_set_gpio_fct(stv090x_fe_state, 0x1, 0x1);
	else
		return stv090x_set_gpio_fct(stv090x_fe_state, 0x1, 0x0);
}

EXPORT_SYMBOL(stv090x_fe_state);
EXPORT_SYMBOL(stv090x_set_gpio_fct);
EXPORT_SYMBOL(set_ant_bridge_mode);

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button custom003019_buttons[] = {
	{
		.code = KEY_Q,
		.gpio = stpio_to_gpio(6, 5),
		.active_low = 1,
		.type = EV_KEY,
		.desc = "BUTTON_CPU",
		.wakeup = 0,
		.debounce_interval = 100,	/* 100 ms */
	},
	{
		.code = KEY_W,
		.gpio = stpio_to_gpio(6, 6),
		.active_low = 1,
		.type = EV_KEY,
		.desc = "BUTTON_PLUS",
		.wakeup = 0,
		.debounce_interval = 100,	/* 100 ms */
	},
	{
		.code = KEY_E,
		.gpio = stpio_to_gpio(6, 7),
		.active_low = 1,
		.type = EV_KEY,
		.desc = "BUTTON_MIN",
		.wakeup = 0,
		.debounce_interval = 100,	/* 100 ms */
	}
};


static struct gpio_keys_platform_data custom003019_button_data = {
	.buttons = custom003019_buttons,
	.nbuttons = ARRAY_SIZE(custom003019_buttons),
	.rep = 1,
};


static struct platform_device custom003019_button_device = {
	.name = "gpio-keys",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &custom003019_button_data,
	}
};
#endif


static struct platform_device *custom003019_devices[] __initdata = {
#ifdef CONFIG_LEDS_GPIO
        &custom003019_leds_device,
#endif
        &physmap_flash,
#if defined(CONFIG_MTD_NAND_PLATFORM) && !defined(PACE_CYFRA_SUPPORT)
	&custom003019_nand_device,
#endif
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
	&custom003019_button_device,
#endif
};

#define AHB2STBUS_SW_RESET		0x10
#define UHOST2C_BASE(N)			(0xfe100000 + ((N)*0x00900000))
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)

#define OHCI_CTRL_IR	(1 << 8)	/* interrupt routing */
#define OHCI_INTR_OC	(1 << 30)	/* ownership change */
#define OHCI_INTR_MIE	(1 << 31)	/* all disable */
#define OHCI_OCR	(1 << 3)	/* ownership change request */
#define OHCI_CTRL_RWC	(1 << 9)	/* remote wakeup connected */

#define OHCI_HC_CTRL	0x04
#define OHCI_HC_CMD_STA	0x08
#define OHCI_HC_INT_EN	0x10
#define OHCI_HC_INT_DIS	0x14

static void reset_usb(int nb)
{
	printk("usb reset \n");
	if (readl (AHB2STBUS_OHCI_BASE(nb) + OHCI_HC_CTRL ) & OHCI_CTRL_IR)
	 {
		u32 temp;
		printk("usb reset - OHCI_CTRL_IR=1\n");

		/* this timeout is arbitrary.  we make it long, so systems
		 * depending on usb keyboards may be usable even if the
		 * BIOS/SMM code seems pretty broken.
		 */
		temp = 500;	/* arbitrary: five seconds */

		writel (OHCI_INTR_OC,AHB2STBUS_OHCI_BASE(nb)+OHCI_HC_INT_EN);
		writel (OHCI_OCR,AHB2STBUS_OHCI_BASE(nb)+OHCI_HC_CMD_STA);
		while (readl (AHB2STBUS_OHCI_BASE(nb)+OHCI_HC_CTRL) & OHCI_CTRL_IR) {
			mdelay (10);
			printk("wait\n");

			if (--temp == 0) {
				printk ( "USB HC takeover failed!"
					"  (BIOS/SMM bug)\n");
				break;
			}
		}
			/* Disable HC interrupts */
	}
	writel (OHCI_INTR_MIE, AHB2STBUS_OHCI_BASE(nb)+OHCI_HC_INT_DIS);
	writel (0,AHB2STBUS_OHCI_BASE(nb)+OHCI_HC_CTRL);
	writel(1, AHB2STBUS_PROTOCOL_BASE(nb) + AHB2STBUS_SW_RESET);
	mdelay(100);
	writel(0, AHB2STBUS_PROTOCOL_BASE(nb) + AHB2STBUS_SW_RESET);
	printk("reset end\n");

}

static int custom003019_usbhub_reset(void)
{
	struct stpio_pin * usb_reset_pin;
	
	reset_usb(0);
	reset_usb(1);
	
	usb_reset_pin = stpio_request_pin(14, 4, "RESET_USBnot", STPIO_OUT);
	stpio_set_pin(usb_reset_pin, 0);
	mdelay(10);
	stpio_set_pin(usb_reset_pin, 1);
	udelay(1000);

	stpio_free_pin(usb_reset_pin);
	
	return 1;
}

static int custom003019_frontend_reset(void)
{
	struct stpio_pin *rstn_demod;

	/* reset of DVB-S demodulator (front end) */
	rstn_demod = stpio_request_pin(15, 3, "RESET_DEMODnot", STPIO_OUT);
	/* STM STV0900: pin RESETB must remain active (low) until at least 3 ms
	 * after the last power supply has stabilized, then transit from low to
	 * high.
	 * TODO check rise time of power supply line further to rising edge of
	 * LOW_PW_FEnot */
	if (rstn_demod)
		stpio_set_pin(rstn_demod, 0);
	mdelay(5);
	if (rstn_demod)
		stpio_set_pin(rstn_demod, 1);
	if (rstn_demod) {
		printk("STV0900 is now out of reset\n");
		stpio_free_pin(rstn_demod);
		return 1;
	}

	return 0;
}

static int custom003019_wlan_reset(void)
{
	struct stpio_pin *reset, *enable;


	reset = stpio_request_pin(11, 6, "WIFI_RESETnot", STPIO_OUT);
	if (!reset)
		return 1;
	enable = stpio_request_pin(11, 7, "WIFI_ENA", STPIO_OUT);
	if (!reset)
		return 1;

	/* Pull lines down */
	stpio_set_pin(enable, 0);
	stpio_set_pin(reset, 0);

	/* Pull back up after appropriate delay */
	mdelay(3000);
	stpio_set_pin(enable, 1);
	mdelay(50);
	stpio_set_pin(reset, 1);

	stpio_free_pin(reset);
	stpio_free_pin(enable);

	return 0;
}

static int __init device_init(void)
{
	stx7105_configure_ssc(&ssc_private_info);

	custom003019_usbhub_reset();
	custom003019_frontend_reset();
	custom003019_wlan_reset();

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	stx7105_configure_lirc(NULL);
	stx7105_configure_tsin(0, 1);

	stx7105_configure_ethernet(0, 0, 0, 0, 0, 0);
	printk("i2c_init\n");
	i2c_register_board_info(0,i2c_device_board_info,
				ARRAY_SIZE(i2c_device_board_info));

#ifdef CONFIG_MTD_NAND_STM_EMI
	stx7105_configure_nand(&nand_device);
#endif

	return platform_add_devices(custom003019_devices, ARRAY_SIZE(custom003019_devices));
}
arch_initcall(device_init);

static void __iomem *custom003019_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init custom003019_init_irq(void)
{
#ifndef CONFIG_SH_ST_MB705
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
#endif
}

static void custom003019_reset(void)
{
//	long jump2_flag = 0xfe239970;

#define DEFAULT_RESET_CHAIN     0xa8c
#define SYS_CONFIG_BASE         0xfe001000
#define SYS_CONFIG_9            (void __iomem *)(SYS_CONFIG_BASE + 0x124)

	/* Restore the reset chain. Note that we cannot use the sysconfig
         * API here, as it calls kmalloc(GFP_KERNEL) but we get here with
         * interrupts disabled.
         */
        writel(DEFAULT_RESET_CHAIN, SYS_CONFIG_9);

//        *( (unsigned int *)(jump2_flag + 4) ) = 0;
}

struct sh_machine_vector mv_custom003019 __initmv = {
	.mv_name		= "custom003019",
	.mv_setup		= custom003019_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= custom003019_init_irq,
	.mv_ioport_map		= custom003019_ioport_map,
	.mv_reset		= custom003019_reset,
};

