/*
 * STx7105/STx7106 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Authors: Stuart Menefy <stuart.menefy@st.com>
 *          Pawel Moll <pawel.moll@st.com>
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
#include <linux/gpio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/stm/clk.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/pata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <asm/irq-ilc.h>
#include <asm/restart.h>



#define STX7105 (cpu_data->type == CPU_STX7105)



static struct {
	u8 sys_cfg;
	u8 alt_max;
} stx7105_pio_sysconf_data[] = {
	[0] = { 19, 5 },
	[1] = { 20, 4 },
	[2] = { 21, 4 },
	[3] = { 25, 4 },
	[4] = { 34, 4 },
	[5] = { 35, 4 },
	[6] = { 36, 4 },
	[7] = { 37, 4 },
	[8] = { 46, 3 },
	[9] = { 47, 4 },
	[10] = { 39, 2 },
	[11] = { 53, 4 },
	[12] = { 48, 5 },
	[13] = { 49, 5 },
	[14] = { 0, 1 },
	[15] = { 50, 4 },
	[16] = { 54, 2 },
};

static void __init stx7105_pio_sysconf(int port, int pin, int alt,
		const char *name)
{
	struct sysconf_field *sc;
	int sys_cfg, alt_max;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7105_pio_sysconf_data));
	BUG_ON(pin < 0 || pin > 7);

	if (port == 14 && alt == 2)
		alt = 1;

	sys_cfg = stx7105_pio_sysconf_data[port].sys_cfg;
	alt_max = stx7105_pio_sysconf_data[port].alt_max;

	if ((sys_cfg == 0) || (alt == -1))
		return;

	BUG_ON(alt < 1 || alt > alt_max);

	alt--;

	if (alt_max > 1) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin, pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, alt & 1);
	}

	if (alt_max > 2) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin + 8, pin + 8, name);
		BUG_ON(!sc);
		sysconf_write(sc, (alt >> 1) & 1);
	}

	if (alt_max > 4) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin + 16, pin + 16, name);
		BUG_ON(!sc);
		sysconf_write(sc, (alt >> 2) & 1);
	}
}



/* USB resources ---------------------------------------------------------- */

static u64 st40_dma_mask = DMA_32BIT_MASK;

#define UHOST2C_BASE(N)			(0xfe100000 + ((N) * 0x00900000))
#define AHB2STBUS_WRAPPER_GLUE_BASE(N)  (UHOST2C_BASE(N))
#define AHB2STBUS_OHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE(N)          (UHOST2C_BASE(N) + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE(N)      (UHOST2C_BASE(N) + 0x000fff00)

static struct platform_device stx7105_usb_devices[] = {
	USB_DEVICE(0, AHB2STBUS_EHCI_BASE(0), evt2irq(0x1720),
		      AHB2STBUS_OHCI_BASE(0), evt2irq(0x1700),
		      AHB2STBUS_WRAPPER_GLUE_BASE(0),
		      AHB2STBUS_PROTOCOL_BASE(0),
		      USB_FLAGS_STRAP_8BIT |
		      USB_FLAGS_STBUS_CONFIG_THRESHOLD_128 |
		      USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
		      USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32),
	USB_DEVICE(1, AHB2STBUS_EHCI_BASE(1), evt2irq(0x13e0),
		      AHB2STBUS_OHCI_BASE(1), evt2irq(0x13c0),
		      AHB2STBUS_WRAPPER_GLUE_BASE(1),
		      AHB2STBUS_PROTOCOL_BASE(1),
		      USB_FLAGS_STRAP_8BIT |
		      USB_FLAGS_STBUS_CONFIG_THRESHOLD_128 |
		      USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8 |
		      USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32),
};


/* PTI Resources ---------------------------------------------------------- */

/* This structure is used by the stx7105_configure_tsin function to verify that the
 * configuration of TSIN inputs are correct.
 */
/* Info: Only 4 TSINs available on ST7105 */
static struct {
	unsigned char active;
	unsigned char config;

	unsigned char serial;
} tsin_config[] = {
	[0] = { 0, 0, 0 },
	[1] = { 0, 0, 0 },
	[2] = { 0, 0, 0 },
	[3] = { 0, 0, 0 },
};

void __init stx7105_configure_tsin(unsigned int port, unsigned char serial)
{
	static struct stpio_pin *pin;
	struct sysconf_field *sc;
	unsigned char configure_tsin = 0;

	/* Only 4 TSINs available on ST7105 */
	if (port > 3) return;

	/* Test if the config will be correct:
	 * TSIN0 parallel => no TSIN2 (serial or parallel)
	 * TSIN1 parallel => no TSIN3 (serial or parallel)
	 * TSIN2 => only serial
	 * TSIN3 => only serial
	 */
	switch(port) {
	case 0:
		/* TSIN2 not configured yet */
		if (tsin_config[2].active == 0) {
			configure_tsin = 1;
		} else {
			/* If TSIN2 is already configured, it must be in serial mode */
			if (serial == 1)
				configure_tsin = 1;
		}
		break;

	case 1:
		/* TSIN3 not configured yet */
		if (tsin_config[3].active == 0) {
			configure_tsin = 1;
		} else {
			/* If TSIN2 is already configured, it must be in serial mode */
			if (serial == 1)
				configure_tsin = 1;
		}
		break;

	case 2:
		/* Config available: only serial */
		if (serial == 1) {
			/* TSIN0 not configured yet */
			if (tsin_config[0].active == 0) {
				configure_tsin = 1;
			} else {
				/* TSIN0 configured in serial mode */
				if (tsin_config[0].serial == 1)
					configure_tsin = 1;
			}
		}
		break;

	case 3:
		/* Config available: only serial */
		if (serial == 1) {
			/* TSIN1 not configured yet */
			if (tsin_config[1].active == 0) {
				configure_tsin = 1;
			} else {
				/* TSIN1 configured in serial mode */
				if (tsin_config[1].serial == 1)
					configure_tsin = 1;
			}
		}
		break;
	}

	/* Configure the TSIN if the check is successful else echo a warning */
	if (configure_tsin == 1) {
		tsin_config[port].active = 1;
		tsin_config[port].serial = serial;

		printk("TSIN: Configure CFG_TSIN0_TSIN1_SELECT\n");
		/* Select TSIN0 and TSIN1 to enter in this order input of the TS Merger */
		sc = sysconf_claim(SYS_CFG, 0, 1, 1, "CFG_TSIN0_TSIN1_SELECT");
		sysconf_write(sc, 0);
		sysconf_release(sc);
		tsin_config[0].config = 1;

		printk("TSIN: Configure CFG_TSIN3_PARALLEL_NOT_SERIAL\n");
		printk("TSIN: Configure CFG_TSIN3_SELECT\n");
		/* Select serial mode for TSIN3 */
		sc = sysconf_claim(SYS_CFG, 0, 4, 4, "CFG_TSIN3_PARALLEL_NOT_SERIAL");
		sysconf_write(sc, 0);
		sysconf_release(sc);
		/* Select TSIN3 to input the TS Merger by TSIN3 */
		sc = sysconf_claim(SYS_CFG, 0, 3, 3, "CFG_TSIN3_SELECT");
		sysconf_write(sc, 1);
		sysconf_release(sc);
		tsin_config[3].config = 1;

		printk("TSIN: Configure CFG_TSIN1_SRC_SELECT\n");
		/* Select TSIN1 to input the TS Merger by TSIN1 */
		sc = sysconf_claim(SYS_CFG, 4, 9, 9, "CFG_TSIN1_SRC_SELECT");
		sysconf_write(sc, 0);
		sysconf_release(sc);
		tsin_config[1].config = 1;

		printk("TSIN: Configure CFG_TSIN2_NOTSELECT\n");
		printk("TSIN: Configure CFG_TSIN2_SRC_SELECT\n");
		/* No other TSINs are routed to TSIN2 input of the TS Merger */
		sc = sysconf_claim(SYS_CFG, 0, 2, 2, "CFG_TSIN2_NOTSELECT");
		sysconf_write(sc, 0);
		sysconf_release(sc);

		/* Select TSIN2 to input the TS Merger by TSIN2 */
		sc = sysconf_claim(SYS_CFG, 4, 10, 10, "CFG_TSIN2_SRC_SELECT");
		sysconf_write(sc, 1);
		sysconf_release(sc);
		tsin_config[2].config = 1;

		switch (port) {
		case 0:
			printk("TSIN: Configure TSIN0 in %s mode\n", (tsin_config[port].serial == 1) ? "serial" : "parallel");

			if (serial == 0) {
				stx7105_pio_sysconf(13, 4, 1, "tsin0data7");
				pin = stpio_request_pin(13, 4, "tsin0data7", STPIO_IN);

				stx7105_pio_sysconf(13, 5, 1, "tsin0btclk");
				pin = stpio_request_pin(13, 5, "tsin0btclk", STPIO_IN);

				stx7105_pio_sysconf(13, 6, 1, "tsin0valid");
				pin = stpio_request_pin(13, 6, "tsin0valid", STPIO_IN);

				stx7105_pio_sysconf(13, 7, 1, "tsin0error");
				pin = stpio_request_pin(13, 7, "tsin0error", STPIO_IN);

				stx7105_pio_sysconf(14, 0, 1, "tsin0pkclk");
				pin = stpio_request_pin(14, 0, "tsin0pkclk", STPIO_IN);

				stx7105_pio_sysconf(14, 1, 1, "tsin0data6");
				pin = stpio_request_pin(14, 1, "tsin0data6", STPIO_IN);

				stx7105_pio_sysconf(14, 2, 1, "tsin0data5");
				pin = stpio_request_pin(14, 2, "tsin0data5", STPIO_IN);

				stx7105_pio_sysconf(14, 3, 1, "tsin0data4");
				pin = stpio_request_pin(14, 3, "tsin0data4", STPIO_IN);

				stx7105_pio_sysconf(14, 4, 1, "tsin0data3");
				pin = stpio_request_pin(14, 4, "tsin0data3", STPIO_IN);

				stx7105_pio_sysconf(14, 5, 1, "tsin0data2");
				pin = stpio_request_pin(14, 5, "tsin0data2", STPIO_IN);

				stx7105_pio_sysconf(14, 6, 1, "tsin0data1");
				pin = stpio_request_pin(14, 6, "tsin0data1", STPIO_IN);

				stx7105_pio_sysconf(14, 7, 1, "tsin0data0");
				pin = stpio_request_pin(14, 7, "tsin0data0", STPIO_IN);
			} else {
				stx7105_pio_sysconf(13, 4, 1, "tsin0serdata");
				pin = stpio_request_pin(13, 4, "tsin0serdata", STPIO_IN);

				stx7105_pio_sysconf(13, 5, 1, "tsin0btclkin");
				pin = stpio_request_pin(13, 5, "tsin0btclkin", STPIO_IN);

				stx7105_pio_sysconf(13, 6, 1, "tsin0valid");
				pin = stpio_request_pin(13, 6, "tsin0valid", STPIO_IN);

				stx7105_pio_sysconf(13, 7, 1, "tsin0error");
				pin = stpio_request_pin(13, 7, "tsin0error", STPIO_IN);

				stx7105_pio_sysconf(14, 0, 1, "tsin0pkclk");
				pin = stpio_request_pin(14, 0, "tsin0pkclk", STPIO_IN);
			}

			stx7105_pio_sysconf(5, 6, 3, "nimCdiseqcIn");
			pin = stpio_request_pin(5, 6, "nimCdiseqcIn", STPIO_IN);
			if (pin != NULL)
			  stpio_free_pin(pin);

			break;

		case 1:
			printk("TSIN: Configure TSIN1 in %s mode\n", (tsin_config[port].serial == 1) ? "serial" : "parallel");

			if (serial == 0) {
				stx7105_pio_sysconf(12, 0, 1, "tsin1data7");
				pin = stpio_request_pin(12, 0, "tsin1data7", STPIO_IN);

				stx7105_pio_sysconf(12, 1, 1, "tsin1btclk");
				pin = stpio_request_pin(12, 1, "tsin1btclk", STPIO_IN);

				stx7105_pio_sysconf(12, 2, 1, "tsin1valid");
				pin = stpio_request_pin(12, 2, "tsin1valid", STPIO_IN);

				stx7105_pio_sysconf(12, 3, 1, "tsin1error");
				pin = stpio_request_pin(12, 3, "tsin1error", STPIO_IN);

				stx7105_pio_sysconf(12, 4, 1, "tsin1pkclk");
				pin = stpio_request_pin(12, 4, "tsin1pkclk", STPIO_IN);

				stx7105_pio_sysconf(12, 5, 1, "tsin1data6");
				pin = stpio_request_pin(12, 5, "tsin1data6", STPIO_IN);

				stx7105_pio_sysconf(12, 6, 1, "tsin1data5");
				pin = stpio_request_pin(12, 6, "tsin1data5", STPIO_IN);

				stx7105_pio_sysconf(12, 7, 1, "tsin1data4");
				pin = stpio_request_pin(12, 7, "tsin1data4", STPIO_IN);

				stx7105_pio_sysconf(13, 0, 1, "tsin1data3");
				pin = stpio_request_pin(13, 0, "tsin1data3", STPIO_IN);

				stx7105_pio_sysconf(13, 1, 1, "tsin1data2");
				pin = stpio_request_pin(13, 1, "tsin1data2", STPIO_IN);

				stx7105_pio_sysconf(13, 2, 1, "tsin1data1");
				pin = stpio_request_pin(13, 2, "tsin1data1", STPIO_IN);

				stx7105_pio_sysconf(13, 3, 1, "tsin1data0");
				pin = stpio_request_pin(13, 3, "tsin1data0", STPIO_IN);
			} else {
				stx7105_pio_sysconf(12, 0, 2, "tsin1serdata");
				pin = stpio_request_pin(12, 0, "tsin1serdata", STPIO_IN);

				stx7105_pio_sysconf(12, 1, 2, "tsin1btclk");
				pin = stpio_request_pin(12, 1, "tsin1btclk", STPIO_IN);

				stx7105_pio_sysconf(12, 2, 2, "tsin1btclkvalid");
				pin = stpio_request_pin(12, 2, "tsin1btclkvalid", STPIO_IN);

				stx7105_pio_sysconf(12, 3, 2, "tsin1error");
				pin = stpio_request_pin(12, 3, "tsin1error", STPIO_IN);

				stx7105_pio_sysconf(12, 4, 2, "tsin1pkclk");
				pin = stpio_request_pin(12, 4, "tsin1pkclk", STPIO_IN);
			}
			break;

		case 2:
			printk("TSIN: Configure TSIN2 in %s mode\n", (tsin_config[port].serial == 1) ? "serial" : "parallel");

			stx7105_pio_sysconf(14, 1, 2, "tsin2serdata");
			pin = stpio_request_pin(14, 1, "tsin2serdata", STPIO_IN);

			stx7105_pio_sysconf(14, 2, 2, "tsin2btclk");
			pin = stpio_request_pin(14, 2, "tsin2btclk", STPIO_IN);

			stx7105_pio_sysconf(14, 3, 2, "tsin2btclkvalid");
			pin = stpio_request_pin(14, 3, "tsin2btclkvalid", STPIO_IN);

			stx7105_pio_sysconf(14, 4, 2, "tsin2error");
			pin = stpio_request_pin(14, 4, "tsin2error", STPIO_IN);

			stx7105_pio_sysconf(14, 5, 2, "tsin2pkclk");
			pin = stpio_request_pin(14, 5, "tsin2pkclk", STPIO_IN);
			break;

		case 3:
			printk("TSIN: Configure TSIN3 in %s mode\n", (tsin_config[port].serial == 1) ? "serial" : "parallel");

			stx7105_pio_sysconf(12, 5, 4, "tsin3serdata");
			pin = stpio_request_pin(12, 5, "tsin3serdata", STPIO_IN);

			stx7105_pio_sysconf(12, 6, 4, "tsin3btclk");
			pin = stpio_request_pin(12, 6, "tsin3btclk", STPIO_IN);

			stx7105_pio_sysconf(12, 7, 4, "tsin3btclkvalid");
			pin = stpio_request_pin(12, 7, "tsin3btclkvalid", STPIO_IN);

			stx7105_pio_sysconf(13, 0, 4, "tsin3error");
			pin = stpio_request_pin(13, 0, "tsin3error", STPIO_IN);

			stx7105_pio_sysconf(13, 1, 4, "tsin3pkclk");
			pin = stpio_request_pin(13, 1, "tsin3pkclk", STPIO_IN);
			break;
		}
	} else {
		printk(KERN_WARNING "Cannot configure TSIN%d in %s mode\n", port, (serial == 1) ? "serial" : "parallel");
	}
}


#if 0
void __init stx7105_configure_tsin(unsigned int port)
{
	static struct stpio_pin *pin;
	
	if (port == 0) {
	   stx7105_pio_sysconf(13, 4, 1, "tsin0serdata");	
	   pin = stpio_request_pin(13, 4, "tsin0serdata", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(13, 5, 1, "tsin0btclkin");	
	   pin = stpio_request_pin(13, 5, "tsin0btclkin", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(13, 6, 1, "tsin0valid");	
	   pin = stpio_request_pin(13, 6, "tsin0valid", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(13, 7, 1, "tsin0error");	
	   pin = stpio_request_pin(13, 7, "tsin0error", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 0, 1, "tsin0pkclk");	
	   pin = stpio_request_pin(14, 0, "tsin0pkclk", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 1, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 1, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 2, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 2, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 3, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 3, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 4, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 4, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 5, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 5, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 6, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 6, "tsin0data", STPIO_ALT_BIDIR);
	   stx7105_pio_sysconf(14, 7, 1, "tsin0data");	
	   pin = stpio_request_pin(14, 7, "tsin0data", STPIO_ALT_BIDIR);
		
	   stx7105_pio_sysconf(5, 6, 3, "nimCdiseqcIn");	
	   pin = stpio_request_pin(5, 6, "nimCdiseqcIn", STPIO_IN);
	}
	else if (port == 1)
	{	
	   stx7105_pio_sysconf(15, 0, 2, "tsin1pkclk");	
	   pin = stpio_request_pin(15, 0, "tsin1pkclk", STPIO_ALT_BIDIR);
		
	   stx7105_pio_sysconf(15, 1, 2, "tsin1btclk");	
	   pin = stpio_request_pin(15, 1, "tsin1btclk", STPIO_ALT_BIDIR);
		
	   stx7105_pio_sysconf(15, 2, 2, "tsin1btclkvalid");	
	   pin = stpio_request_pin(15, 2, "tsin1btclkvalid", STPIO_ALT_BIDIR);		
	
	   stx7105_pio_sysconf(15, 3, 2, "tsin1error");	
	   pin = stpio_request_pin(15, 3, "tsin1error", STPIO_ALT_BIDIR);
		
	   stx7105_pio_sysconf(15, 4, 2, "tsin1serdata");	
	   pin = stpio_request_pin(15, 4, "tsin1serdata", STPIO_ALT_BIDIR);
		
	}
}
#endif

/**
 * stx7105_configure_usb - Configure a USB port
 * @port: USB port number (0 or 1)
 * @init_data: details of how to configure port
 *
 * Configure a USB port.
 *
 * 		  PORT 0	  PORT 1
 *		+-----------------------+-----------+
 * OC (input)	| PIO4.4                | PIO4.6    |
 *		| PIO12.5               | PIO14.6   |
 *		| PIO14.4 (7106 only)   |           |
 *		+-----------------------+-----------+
 * PWR (output)	| PIO4.5@4              | PIO4.7@4  |
 *	@alt	| PIO12.6@3             | PIO14.7@1 |
 *		| PIO14.5@1 (7106 only) |           |
 *		+-----------------------+-----------+
 */
void __init stx7105_configure_usb(int port, struct usb_init_data *data)
{
	struct sysconf_field *sc;

	BUG_ON(port < 0 || port > 1);

	/* USB PHY clock from alternate pad? */
	/* sysconf_claim(SYS_CFG, 40, 2, 2, "USB"); */

#ifndef CONFIG_PM
	/* Power up USB PHY */
	sc = sysconf_claim(SYS_CFG, 32, 6 + port, 6 + port, "USB");
	sysconf_write(sc, 0);

	/* Power up USB host */
	sc = sysconf_claim(SYS_CFG, 32, 4 + port, 4 + port, "USB");
	sysconf_write(sc, 0);
#endif

	/* USB overcurrent enable */
	sc = sysconf_claim(SYS_CFG, 4, 11 + port, 11 + port, "USBOC");
	sysconf_write(sc, data->oc_en ? 1 : 0);

	if (data->oc_en) {
		const struct {
			int pios_num;
			struct {
				int port;
				int pin;
			} pios[3];
		} oc_pins[2] = {
			{ 3, { { 4, 4 }, { 12, 5 }, { 14, 4 } } },
			{ 2, { { 4, 6 }, { 14, 6 } } }
		};
		int pio_port, pio_pin;
		struct stpio_pin *pin;

		BUG_ON(data->oc_pinsel < 0);
		BUG_ON(data->oc_pinsel > oc_pins[port].pios_num);
		BUG_ON(STX7105 && port == 0 && data->oc_pinsel == 2);
		pio_port = oc_pins[port].pios[data->oc_pinsel].port;
		pio_pin = oc_pins[port].pios[data->oc_pinsel].pin;

		if (STX7105)
			sc = sysconf_claim(SYS_CFG, 4, 5 + port, 5 + port,
						"USBOC");
		else if (port == 0)
			sc = sysconf_claim(SYS_CFG, 4, 5, 6, "USBOC");
		else
			sc = sysconf_claim(SYS_CFG, 4, 8, 8, "USBOC");
		sysconf_write(sc, data->oc_pinsel);

		pin = stpio_request_pin(pio_port, pio_pin, "USBOC", STPIO_IN);
		BUG_ON(!pin);

		sc = sysconf_claim(SYS_CFG, 4, 3 + port, 3 + port, "USBOC");
		sysconf_write(sc, data->oc_actlow ? 0 : 1);
	}

	if (data->pwr_en) {
		const struct {
			int pios_num;
			struct {
				int port;
				int pin;
				int alt;
			} pios[3];
		} pwr_pins[2] = {
			{ 3, { { 4, 5, 4 }, { 12, 6, 3 }, { 14, 5, 1 } } },
			{ 2, { { 4, 7, 4 }, { 14, 7, 2 } } }
		};
		int pio_port, pio_pin, pio_alt;
		struct stpio_pin *pin;

		BUG_ON(data->pwr_pinsel < 0);
		BUG_ON(data->pwr_pinsel > pwr_pins[port].pios_num);
		BUG_ON(STX7105 && port == 0 && data->pwr_pinsel == 2);
		pio_port = pwr_pins[port].pios[data->pwr_pinsel].port;
		pio_pin = pwr_pins[port].pios[data->pwr_pinsel].pin;
		pio_alt = pwr_pins[port].pios[data->pwr_pinsel].alt;

		stx7105_pio_sysconf(pio_port, pio_pin, pio_alt, "USBPWR");
		pin = stpio_request_pin(pio_port, pio_pin, "USBPWR",
				STPIO_ALT_OUT);
		BUG_ON(!pin);
	}

	platform_device_register(&stx7105_usb_devices[port]);

}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/fdma_firmware_7200.h>

static struct stm_plat_fdma_hw stx7105_fdma_hw = {
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

static struct stm_plat_fdma_data stx7105_fdma_platform_data = {
	.hw = &stx7105_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7105_fdma_platform_data_addr &stx7105_fdma_platform_data

#else

#define stx7105_fdma_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7105_fdma_devices[] = {
	{
		.name		= "stm-fdma",
		.id		= 0,
		.num_resources	= 2,
		.resource = (struct resource[]) {
			{
				.start = 0xfe220000,
				.end   = 0xfe22ffff,
				.flags = IORESOURCE_MEM,
			}, {
				.start = evt2irq(0x1380),
				.end   = evt2irq(0x1380),
				.flags = IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = stx7105_fdma_platform_data_addr,
	}, {
		.name		= "stm-fdma",
		.id		= 1,
		.num_resources	= 2,
		.resource = (struct resource[]) {
			{
				.start = 0xfe410000,
				.end   = 0xfe41ffff,
				.flags = IORESOURCE_MEM,
			}, {
				.start = evt2irq(0x13a0),
				.end   = evt2irq(0x13a0),
				.flags = IORESOURCE_IRQ,
			},
		},
		.dev.platform_data = stx7105_fdma_platform_data_addr,
	},
};

static struct platform_device stx7105_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start = 0xfe420000,
			.end   = 0xfe420fff,
			.flags = IORESOURCE_MEM,
		},
	},
};



/* SSC resources ---------------------------------------------------------- */
static const char stx7105_i2c_dev_name_ssc[] = "i2c_stm_ssc";
static const char stx7105_i2c_dev_name_pio[] = "i2c_stm_pio";
static char stx7105_spi_dev_name[] = "spi_st_ssc";

static struct platform_device stx7105_ssc_devices[] = {
	STSSC_DEVICE(0xfd040000, evt2irq(0x10e0), 2, 2, 3, 4),
	STSSC_DEVICE(0xfd041000, evt2irq(0x10c0), 2, 5, 6, 7),
	STSSC_DEVICE(0xfd042000, evt2irq(0x10a0), 0xff, 0xff, 0xff, 0xff),
	STSSC_DEVICE(0xfd043000, evt2irq(0x1080), 0xff, 0xff, 0xff, 0xff),
};

void __init stx7105_configure_ssc(struct plat_ssc_data *data)
{
	int num_i2c = 0;
	int num_spi = 0;
	int capability = data->capability;
	int routing = data->routing;
	struct sysconf_field* sc;
	int i;

	const struct {
		unsigned char port, pin, alt;
	} ssc_pios[2][3][4] = { {
		/* SSC2 (SCK on 2.4@2 on 7106 only!) */
		{ { 2, 4, 2 }, { 3, 4, 2 }, { 12, 0, 3 }, { 13, 4, 2 } },
		{ { 2, 0, 3 }, { 3, 5, 2 }, { 12, 1, 3 }, { 13, 5, 2 } },
		{ { 2, 0, 4 }, { 3, 5, 3 }, { 12, 1, 4 }, { 13, 5, 3 } }
	}, {
		/* SSC3 (SCK on 2.7@2 on 7106 only!) */
		{ { 2, 7, 2 }, { 3, 6, 2 }, { 13, 2, 4 }, { 13, 6, 2 } },
		{ { 2, 1, 3 }, { 3, 7, 2 }, { 13, 3, 4 }, { 13, 7, 2 } },
		{ { 2, 1, 4 }, { 3, 7, 3 }, { 13, 3, 5 }, { 13, 7, 3 } }
	} };

	for (i = 0; i < ARRAY_SIZE(stx7105_ssc_devices); i++,
			capability >>= SSC_BITS_SIZE) {
		struct ssc_pio_t *ssc =
				stx7105_ssc_devices[i].dev.platform_data;
		int pin;

		if(capability & SSC_UNCONFIGURED)
			continue;

		if (capability & SSC_I2C_CLK_UNIDIR)
			ssc->clk_unidir = 1;

		switch (i) {
		case 0:
		case 1:
			/* These have fixed routing */
			for (pin = 0; pin < (capability & SSC_SPI_CAPABILITY ?
					3 : 2); pin++) {
				int portno = ssc->pio[pin].pio_port;
				int pinno  = ssc->pio[pin].pio_pin;

				stx7105_pio_sysconf(portno, pinno, 3, "ssc");
			}

			if (capability & SSC_SPI_CAPABILITY) {
				sc = sysconf_claim(SYS_CFG, 16, 3 * i, 3 * i,
						"ssc");
				sysconf_write(sc, 1);
			}
			break;
		case 2:
		case 3:
			/* Complex routing */
			for (pin = 0; pin < (capability & SSC_SPI_CAPABILITY ?
					3 : 2); pin++) {
				int r = (routing >> STX7105_SSC_SHIFT(i, pin))
						& 0x3;
				int bit = ((i == 2) ? 11 : 18) - (pin * 2);
				int portno = ssc_pios[i - 2][pin][r].port;
				int pinno = ssc_pios[i - 2][pin][r].pin;
				int alt = ssc_pios[i - 2][pin][r].alt;

				/* SCLKs on 2.4 & 2.7 are
				 * not available on 7105 */
				BUG_ON(STX7105 && pin == 0 && r == 0);

				sc = sysconf_claim(SYS_CFG, 16, bit, bit + 1,
						"ssc");
				sysconf_write(sc, r);

				ssc->pio[pin].pio_port = portno;
				ssc->pio[pin].pio_pin  = pinno;

				stx7105_pio_sysconf(portno, pinno, alt, "ssc");
			}
			break;
		}

		if (capability & SSC_SPI_CAPABILITY) {
			stx7105_ssc_devices[i].name = stx7105_spi_dev_name;
			stx7105_ssc_devices[i].id = num_spi++;
			ssc->chipselect = data->spi_chipselects[i];
		} else {
			/* I2C buses number reservation (to prevent any
			 * hot-plug device from using it) */
#ifdef CONFIG_I2C_BOARDINFO
			i2c_register_board_info(num_i2c, NULL, 0);
#endif
			stx7105_ssc_devices[i].name =
					((capability & SSC_I2C_PIO) ?
					stx7105_i2c_dev_name_pio :
					stx7105_i2c_dev_name_ssc);
			stx7105_ssc_devices[i].id = num_i2c++;
			if (capability & SSC_I2C_CLK_UNIDIR)
				ssc->clk_unidir = 1;
		}

		platform_device_register(&stx7105_ssc_devices[i]);
	}

}



/* SATA resources --------------------------------------------------------- */

/* It's ok to have same private data for all cases */
static struct plat_sata_data stx7105_sata_private_info = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
};

static struct platform_device stx7105_sata_device =
	SATA_DEVICE(-1, 0xfe209000, evt2irq(0xb00), evt2irq(0xa80),
			&stx7105_sata_private_info);

static struct platform_device stx7106_sata_devices[] = {
	SATA_DEVICE(0, 0xfe208000, evt2irq(0xb00), evt2irq(0xa80),
			&stx7105_sata_private_info),
	SATA_DEVICE(1, 0xfe209000, ILC_EXT_IRQ(34), ILC_EXT_IRQ(33),
			&stx7105_sata_private_info),
};

void __init stx7105_configure_sata(int port)
{
	static int initialized;

	BUG_ON(port < 0 || port > 1);

	if (STX7105 && (port != 0 || cpu_data->cut_major < 3))
		return;

	/* Unfortunately there is a lot of dependencies between
	 * PHYs & controllers... I didn't manage to get everything
	 * working when powering up only selected bits... */
	if (!initialized) {
		struct stm_miphy_sysconf_soft_jtag jtag;
		struct stm_miphy miphy = {
			.jtag_tick = stm_miphy_sysconf_jtag_tick,
			.jtag_priv = &jtag,
		};
		struct sysconf_field *sc;

		jtag.tck = sysconf_claim(SYS_CFG, 33, 0, 0, "SATA");
		BUG_ON(!jtag.tck);
		jtag.tms = sysconf_claim(SYS_CFG, 33, 5, 5, "SATA");
		BUG_ON(!jtag.tms);
		jtag.tdi = sysconf_claim(SYS_CFG, 33, 1, 1, "SATA");
		BUG_ON(!jtag.tdi);
		jtag.tdo = sysconf_claim(SYS_STA, 0, 1, 1, "SATA");
		BUG_ON(!jtag.tdo);

		/* Power up host controllers... */
		sc = sysconf_claim(SYS_CFG, 32, STX7105 ? 11 : 10, 11, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 0);

		/* soft_jtag_en */
		sc = sysconf_claim(SYS_CFG, 33, 6, 6, "SATA soft_jtag_en");
		BUG_ON(!sc);
		sysconf_write(sc, 1);

		/* TMS should be set to 1 when taking the TAP
		 * machine out of reset... */
		sysconf_write(jtag.tms, 1);

		/* trstn_sata */
		sc = sysconf_claim(SYS_CFG, 33, 4, 4, "SATA");
		BUG_ON(!sc);
		sysconf_write(sc, 1);
		udelay(100);

		/* Power up & initialize PHY(s) */
		if (STX7105) {
			/* 7105 */
			miphy.ports_num = 1;

			sc = sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
			BUG_ON(!sc);
			sysconf_write(sc, 0);
			stm_miphy_init(&miphy, 0);
		} else {
			/* 7106 */
			miphy.ports_num = 2;

			/* The second PHY _must not_ be powered while
			 * initializing the first one... */

			sc = sysconf_claim(SYS_CFG, 32, 8, 8, "SATA");
			BUG_ON(!sc);
			sysconf_write(sc, 0);
			stm_miphy_init(&miphy, 0);

			sc = sysconf_claim(SYS_CFG, 32, 9, 9, "SATA");
			BUG_ON(!sc);
			sysconf_write(sc, 0);
			stm_miphy_init(&miphy, 1);
		}

		initialized = 1;
	}

	if (STX7105)
		platform_device_register(&stx7105_sata_device);
	else
		platform_device_register(&stx7106_sata_devices[port]);

}



/* PATA resources --------------------------------------------------------- */


/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */

static struct resource stx7105_pata_resources[] = {
	[0] = {	/* I/O base: CS1=N, CS0=A */
		.start	= (1 << 20),
		.end	= (1 << 20) + (8 << 17) - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
		.start	= (1 << 21) + (6 << 17),
		.end	= (1 << 21) + (6 << 17) + 3,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {	/* IRQ */
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device stx7105_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7105_pata_resources),
	.resource	= stx7105_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift	= 17,
	},
};

void __init stx7105_configure_pata(int bank, int pc_mode, int irq)
{
	unsigned long bank_base = emi_bank_base(bank);

	stx7105_pata_resources[0].start += bank_base;
	stx7105_pata_resources[0].end   += bank_base;
	stx7105_pata_resources[1].start += bank_base;
	stx7105_pata_resources[1].end   += bank_base;
	stx7105_pata_resources[2].start = irq;
	stx7105_pata_resources[2].end   = irq;

	emi_config_pata(bank, pc_mode);

	platform_device_register(&stx7105_pata_device);
}



/* Ethernet MAC resources ------------------------------------------------- */

static void stx7105_gmac_fix_mac_speed(void *priv, unsigned int speed)
{
	struct sysconf_field *sc = priv;

	sysconf_write(sc, (speed == SPEED_100) ? 1 : 0);
}

static struct plat_stmmacenet_data stx7105_gmac_private_data[] = {
	{
		.bus_id = 0,
		.pbl = 32,
		.has_gmac = 1,
		.fix_mac_speed = stx7105_gmac_fix_mac_speed,
	}, {
		.bus_id = 1,
		.pbl = 32,
		.has_gmac = 1,
		.fix_mac_speed = stx7105_gmac_fix_mac_speed,
	}
};

static struct platform_device stx7105_gmac_devices[] = {
	{
		.name           = "stmmaceth",
		.id             = 0,
		.num_resources  = 2,
		.resource       = (struct resource[]) {
			{
				.start = 0xfd110000,
				.end   = 0xfd117fff,
				.flags  = IORESOURCE_MEM,
			}, {
				.name   = "macirq",
				.start  = evt2irq(0x12c0),
				.end    = evt2irq(0x12c0),
				.flags  = IORESOURCE_IRQ,
			},
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7105_gmac_private_data[0],
		}
	}, {
		.name           = "stmmaceth",
		.id             = 1,
		.num_resources  = 2,
		.resource       = (struct resource[]) {
			{
				.start = 0xfd118000,
				.end   = 0xfd11ffff,
				.flags  = IORESOURCE_MEM,
			}, {
				.name   = "macirq",
				.start  = ILC_EXT_IRQ(39),
				.end    = ILC_EXT_IRQ(39),
				.flags  = IORESOURCE_IRQ,
			},
		},
		.dev = {
			.power.can_wakeup = 1,
			.platform_data = &stx7105_gmac_private_data[1],
		}
	}
};

struct stx7105_gmac_pin {
	struct {
		unsigned char port, pin, alt;
	} pio[2];
	unsigned char dir;
};

/* Yes, MDIO is supposed to be configured as ALT_OUT, not ALT_BIDIR... */

static struct stx7105_gmac_pin stx7105_gmac_mii_pins[] __initdata = {
	{ { { 9, 5, 1 }, { 11, 3, 2 } }, /* below */ },		/* PHYCLK */
	{ { { 8, 3, 1 }, /* below */ },  STPIO_ALT_OUT },	/* MDIO */
	{ { { 8, 4, 1 }, /* below */ },  STPIO_ALT_OUT },	/* MDC */
	{ { { 9, 6 },    { 3, 6 } },     STPIO_IN },		/* MDINT */
	{ { { 9, 2 },    { 16, 7 } },    STPIO_IN },		/* TXCLK */
	{ { { 8, 2, 1 }, { 11, 2, 2 } }, STPIO_ALT_OUT },	/* TXEN */
	{ { { 7, 6, 1 }, { 11, 4, 2 } }, STPIO_ALT_OUT },	/* TXD[0] */
	{ { { 7, 7, 1 }, { 11, 5, 2 } }, STPIO_ALT_OUT },	/* TXD[1] */
	{ { { 8, 0, 1 }, { 11, 6, 2 } }, STPIO_ALT_OUT },	/* TXD[2] */
	{ { { 8, 1, 1 }, { 11, 7, 2 } }, STPIO_ALT_OUT },	/* TXD[3] */
	{ { { 8, 5 },    { 16, 6 } },    STPIO_IN },		/* RXCLK */
	{ { { 7, 4 },    { 16, 4 } },    STPIO_IN },		/* RXDV */
	{ { { 7, 5 },    { 16, 5 } },    STPIO_IN },		/* RXER */
	{ { { 8, 6 },    { 16, 0 } },    STPIO_IN },		/* RXD[0] */
	{ { { 8, 7 },    { 16, 1 } },    STPIO_IN },		/* RXD[1] */
	{ { { 9, 0 },    { 16, 2 } },    STPIO_IN },		/* RXD[2] */
	{ { { 9, 1 },    { 16, 3 } },    STPIO_IN },		/* RXD[3] */
	{ { { 9, 3 },    { 15, 6 } },    STPIO_IN },		/* COL */
	{ { { 9, 4 },    { 15, 7 } },    STPIO_IN },		/* CRS */
};

static struct stx7105_gmac_pin stx7105_gmac_gmii_pins[] __initdata = {
	{ { { 9, 5, 1 } },  /* below */ },		/* PHYCLK */
	{ { { 8, 3, 1 } },  STPIO_ALT_OUT },		/* MDIO */
	{ { { 8, 4, 1 } },  STPIO_ALT_OUT },		/* MDC */
	{ { { 9, 6 } },     STPIO_IN },			/* MDINT */
	{ { { 9, 2 } },     STPIO_IN },			/* TXCLK */
#if 0
/* Note: TXER line is not configured (at this moment) as:
   1. It is generally useless and not used at all by a lot of PHYs.
   2. PIO9.7 is muxed with HDMI hot plug detect, which is likely to be used.
   3. Apparently the GMAC (or the SOC) is broken anyway and it doesn't drive
      it correctly ;-) */
	{ { { 9, 7, 1 } },  STPIO_ALT_OUT },		/* TXER */
#endif
	{ { { 8, 2, 1 } },  STPIO_ALT_OUT },		/* TXEN */
	{ { { 7, 6, 1 } },  STPIO_ALT_OUT },		/* TXD[0] */
	{ { { 7, 7, 1 } },  STPIO_ALT_OUT },		/* TXD[1] */
	{ { { 8, 0, 1 } },  STPIO_ALT_OUT },		/* TXD[2] */
	{ { { 8, 1, 1 } },  STPIO_ALT_OUT },		/* TXD[3] */
	{ { { 15, 4, 4 } }, STPIO_ALT_OUT },		/* TXD[4] */
	{ { { 15, 5, 4 } }, STPIO_ALT_OUT },		/* TXD[5] */
	{ { { 11, 0, 4 } }, STPIO_ALT_OUT },		/* TXD[6] */
	{ { { 11, 1, 4 } }, STPIO_ALT_OUT },		/* TXD[7] */
	{ { { 8, 5 } },     STPIO_IN },			/* RXCLK */
	{ { { 7, 4 } },     STPIO_IN },			/* RXDV */
	{ { { 7, 5 } },     STPIO_IN },			/* RXER */
	{ { { 8, 6 } },     STPIO_IN },			/* RXD[0] */
	{ { { 8, 7 } },     STPIO_IN },			/* RXD[1] */
	{ { { 9, 0 } },     STPIO_IN },			/* RXD[2] */
	{ { { 9, 1 } },     STPIO_IN },			/* RXD[3] */
	{ { { 15, 0 } },    STPIO_IN },			/* RXD[4] */
	{ { { 15, 1 } },    STPIO_IN },			/* RXD[5] */
	{ { { 15, 2 } },    STPIO_IN },			/* RXD[6] */
	{ { { 15, 3 } },    STPIO_IN },			/* RXD[7] */
	{ { { 9, 3 } },     STPIO_IN },			/* COL */
	{ { { 9, 4 } },     STPIO_IN },			/* CRS */
};

static struct stx7105_gmac_pin stx7105_gmac_rmii_pins[] __initdata = {
	{ { { 9, 5, 2 }, { 11, 3, 3 } }, /* below */ },		/* REFCLK */
	{ { { 8, 3, 2 }, /* below */ },  STPIO_ALT_OUT },	/* MDIO */
	{ { { 8, 4, 2 }, /* below */ },  STPIO_ALT_OUT },	/* MDC */
	{ { { 9, 6 },    { 3, 6 } },     STPIO_IN },		/* MDINT */
	{ { { 8, 2, 2 }, { 11, 2, 3 } }, STPIO_ALT_OUT },	/* TXEN */
	{ { { 7, 6, 2 }, { 11, 4, 3 } }, STPIO_ALT_OUT },	/* TXD[0] */
	{ { { 7, 7, 2 }, { 11, 5, 3 } }, STPIO_ALT_OUT },	/* TXD[1] */
	{ { { 7, 4 },    { 16, 4 } },    STPIO_IN },		/* CRSDV */
	{ { { 7, 5 },    { 16, 5 } },    STPIO_IN },		/* CRSER */
	{ { { 8, 6 },    { 16, 0 } },    STPIO_IN },		/* RXD[0] */
	{ { { 8, 7 },    { 16, 1 } },    STPIO_IN },		/* RXD[1] */
};

static struct stx7105_gmac_pin stx7105_gmac_reverse_mii_pins[] __initdata = {
	{ { { 9, 5, 1 } }, /* below */ },		/* PHYCLK */
	{ { { 8, 3, 1 } }, STPIO_ALT_OUT },		/* MDIO */
	{ { { 8, 4, 1 } }, STPIO_IN },			/* MDC */
	{ { { 9, 6 } },    STPIO_IN },			/* MDINT */
	{ { { 9, 2 } },    STPIO_IN },			/* TXCLK */
	{ { { 8, 2, 1 } }, STPIO_ALT_OUT },		/* TXEN */
	{ { { 7, 6, 1 } }, STPIO_ALT_OUT },		/* TXD[0] */
	{ { { 7, 7, 1 } }, STPIO_ALT_OUT },		/* TXD[1] */
	{ { { 8, 0, 1 } }, STPIO_ALT_OUT },		/* TXD[2] */
	{ { { 8, 1, 1 } }, STPIO_ALT_OUT },		/* TXD[3] */
	{ { { 8, 5 } },    STPIO_IN },			/* RXCLK */
	{ { { 7, 4 } },    STPIO_IN },			/* RXDV */
	{ { { 7, 5 } },    STPIO_IN },			/* RXER */
	{ { { 8, 6 } },    STPIO_IN },			/* RXD[0] */
	{ { { 8, 7 } },    STPIO_IN },			/* RXD[1] */
	{ { { 9, 0 } },    STPIO_IN },			/* RXD[2] */
	{ { { 9, 1 } },    STPIO_IN },			/* RXD[3] */
	{ { { 7, 4, 1 } }, STPIO_ALT_OUT },		/* EXCOL */
	{ { { 7, 5, 1 } }, STPIO_ALT_OUT },		/* EXCRS */
};

void __init stx7105_configure_ethernet(int port,
		enum stx7105_ethernet_mode mode, int ext_clk, int phy_bus,
		int mii0_mdint_workaround, int mii1_routing)
{
	struct sysconf_field *sc;
	struct stx7105_gmac_pin *pins;
	int pins_num;
	struct {
		unsigned char ouput_enable;
		unsigned char mac_speed_sel;
		unsigned char enmii;
		unsigned char phy_intf_sel;
		unsigned char rmii_mode;
		unsigned char miim_dio_select;
		unsigned char interface_on;
	} fields;
	struct {
		unsigned char enmii;
		unsigned char phy_intf_sel;
		unsigned char rmii_mode;
	} values;
	int i;
	struct clk *phyclk = clk_get(NULL, "CLKA_ETH0_PHY");

	BUG_ON(port < 0 || port > 1);

	if (STX7105 && port != 0)
		return;

	if (!phyclk || IS_ERR(phyclk)) {
		printk(KERN_ERR "Failed to get Ethernet PHY clock!\n");
		phyclk = NULL;
	}

	switch (mode) {
	case stx7105_ethernet_mii:
		values.enmii = 1;
		values.phy_intf_sel = 0;
		values.rmii_mode = 0;
		pins = stx7105_gmac_mii_pins;
		pins_num = ARRAY_SIZE(stx7105_gmac_mii_pins);
		if (phyclk)
			clk_set_rate(phyclk, 25000000);
		break;
	case stx7105_ethernet_rmii:
		values.enmii = 1;
		values.phy_intf_sel = 0;
		values.rmii_mode = 1;
		pins = stx7105_gmac_rmii_pins;
		pins_num = ARRAY_SIZE(stx7105_gmac_rmii_pins);
		if (phyclk)
			clk_set_rate(phyclk, 50000000);
		break;
	case stx7105_ethernet_gmii:
		BUG_ON(port == 1); /* Available on MII0 only */
		values.enmii = 1;
		values.phy_intf_sel = 0;
		values.rmii_mode = 0;
		pins = stx7105_gmac_gmii_pins;
		pins_num = ARRAY_SIZE(stx7105_gmac_gmii_pins);
		if (phyclk)
			clk_set_rate(phyclk, 125000000);
		ext_clk = 0; /* PHYCLK is used as GTXCLK in 1Gbps mode */
		break;
	case stx7105_ethernet_reverse_mii:
		BUG_ON(port == 1); /* Available on MII0 only */
		values.enmii = 0;
		values.phy_intf_sel = 0;
		values.rmii_mode = 0;
		pins = stx7105_gmac_reverse_mii_pins;
		pins_num = ARRAY_SIZE(stx7105_gmac_reverse_mii_pins);
		if (phyclk)
			clk_set_rate(phyclk, 25000000);
		break;
	default:
		BUG();
		return;
	}

	switch (port) {
	case 0:
		fields.mac_speed_sel = 20;
		fields.enmii = 27;
		fields.phy_intf_sel = 25;
		fields.rmii_mode = 18;
		fields.miim_dio_select = 17;
		fields.interface_on = 16;
		break;
	case 1:
		fields.ouput_enable = 31;
		fields.mac_speed_sel = 21;
		fields.enmii = 30;
		fields.phy_intf_sel = 28;
		fields.rmii_mode = 19;
		fields.miim_dio_select = 15;
		fields.interface_on = 14;

		/* eth1_mdiin_src_sel */
		sc = sysconf_claim(SYS_CFG, 16, 4, 4, "stmmac");
		switch (mii1_routing & STX7105_ETHERNET_MII1_MDIO_MASK) {
		case STX7105_ETHERNET_MII1_MDIO_11_0:
			pins[1].pio[1].port = 11;
			pins[1].pio[1].pin = 0;
			pins[1].pio[1].alt = 3;
			sysconf_write(sc, 1);
			break;
		case STX7105_ETHERNET_MII1_MDIO_3_4:
			pins[1].pio[1].port = 3;
			pins[1].pio[1].pin = 4;
			pins[1].pio[1].alt = 4;
			sysconf_write(sc, 0);
			break;
		default:
			BUG();
			break;
		}

		/* eth1_mdcin_src_sel */
		sc = sysconf_claim(SYS_CFG, 16, 5, 5, "stmmac");
		switch (mii1_routing & STX7105_ETHERNET_MII1_MDC_MASK) {
		case STX7105_ETHERNET_MII1_MDC_11_1:
			pins[2].pio[1].port = 11;
			pins[2].pio[1].pin = 1;
			pins[2].pio[1].alt = 3;
			sysconf_write(sc, 1);
			break;
		case STX7105_ETHERNET_MII1_MDC_3_5:
			pins[2].pio[1].port = 3;
			pins[2].pio[1].pin = 5;
			pins[2].pio[1].alt = 4;
			sysconf_write(sc, 0);
			break;
		default:
			BUG();
			break;
		}

		break;
	default:
		BUG();
		return;
	}

	if (port == 1) {
		/* There is no output_enable for port 0 */
		sc = sysconf_claim(SYS_CFG, 7, fields.ouput_enable,
				fields.ouput_enable, "stmmac");
		sysconf_write(sc, 1);
	}

	stx7105_gmac_private_data[port].bsp_priv = sysconf_claim(SYS_CFG, 7,
			fields.mac_speed_sel, fields.mac_speed_sel, "stmmac");

	sc = sysconf_claim(SYS_CFG, 7, fields.enmii, fields.enmii, "stmmac");
	sysconf_write(sc, values.enmii);

	sc = sysconf_claim(SYS_CFG, 7, fields.phy_intf_sel,
			fields.phy_intf_sel, "stmmac");
	sysconf_write(sc, values.phy_intf_sel);

	sc = sysconf_claim(SYS_CFG, 7, fields.rmii_mode,
			fields.rmii_mode, "stmmac");
	sysconf_write(sc, values.rmii_mode);

	sc = sysconf_claim(SYS_CFG, 7, fields.miim_dio_select,
			fields.miim_dio_select, "stmmac");
	sysconf_write(sc, 0);

	sc = sysconf_claim(SYS_CFG, 7, fields.interface_on,
			fields.interface_on, "stmmac");
	sysconf_write(sc, 1);

	pins[0].dir = ext_clk ? STPIO_IN : STPIO_ALT_OUT;

	for (i = 0; i < pins_num; i++) {
		int pio_port = pins[i].pio[port].port;
		int pio_pin = pins[i].pio[port].pin;
		int pio_alt = pins[i].pio[port].alt;
		int pio_dir = pins[i].dir;

		/* MII0 MDINT */
		if (port == 0 && pio_port == 9 && pio_pin == 6 &&
				mii0_mdint_workaround) {
			/* This is a workaround for a problem seen on some
			 * boards, such as the HMP7105, which use the SMSC
			 * LAN8700 with no board level logic to work around
			 * conflicts on mode pin usage with the STx7105.
			 *
			 * Background:
			 * The 8700 uses the MII RXD[3] pin as a mode
			 * selection at reset for whether nINT/TXERR/TXD4 is
			 * used as nINT or TXERR. However the 7105 uses the
			 * same pin as MODE(9), to determine which processor
			 * boots first.
			 * Assuming the pull up/down resistors are configured
			 * so that the ST40 boots first, this is what causes
			 * the 8700 to treat nINT/TXERR as TXERR, which is not
			 * what we want.
			 *
			 * Workaround
			 * Force MII_MDINT to be an output, driven low, to
			 * indicate there is no error. */
			stpio_request_set_pin(9, 6, "stmmac", STPIO_OUT, 0);
		} else {
			stpio_request_pin(pio_port, pio_pin, "stmmac", pio_dir);
			if (pio_dir != STPIO_IN)
				stx7105_pio_sysconf(pio_port, pio_pin, pio_alt,
						"stmmac");
		}
	}

	stx7105_gmac_private_data[port].bus_id = phy_bus;

	platform_device_register(&stx7105_gmac_devices[port]);
}



/* Audio output ----------------------------------------------------------- */

static const char stx7105_audio_pcmout0_label[] = "auddig0_pcmout";
static const char stx7105_audio_pcmout1_label[] = "auddig1_pcmout";
static const char stx7105_audio_spdif_label[] = "aud_spdif";
static const char stx7105_audio_pcmin_label[] = "auddig_pcmin";

void __init stx7105_configure_audio_pins(int pcmout0, int pcmout1, int spdif,
		int pcmin)
{
	struct stpio_pin *pin;
	const char *label;

	/* Claim PIO pins as digital audio outputs (AUDDIG0), depending
	 * on how many DATA outputs are to be used... */

	label = stx7105_audio_pcmout0_label;
	if (pcmout0 > 0) {
		stx7105_pio_sysconf(10, 3, 1, label);
		pin = stpio_request_pin(10, 3, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
		stx7105_pio_sysconf(10, 4, 1, label);
		pin = stpio_request_pin(10, 4, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
		stx7105_pio_sysconf(10, 5, 1, label);
		pin = stpio_request_pin(10, 5, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
		stx7105_pio_sysconf(10, 0, 1, label);
		pin = stpio_request_pin(10, 0, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (pcmout0 > 1) {
		stx7105_pio_sysconf(10, 1, 1, label);
		pin = stpio_request_pin(10, 1, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (pcmout0 > 2) {
		stx7105_pio_sysconf(10, 2, 1, label);
		pin = stpio_request_pin(10, 2, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (pcmout0 > 3) {
		BUG_ON(STX7105); /* 7106 only! */
		stx7105_pio_sysconf(10, 7, 1, label);
		pin = stpio_request_pin(10, 7, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (pcmout0 > 4)
		BUG();

	/* Claim PIO pin as SPDIF output... */

	if (spdif > 0) {
		label = stx7105_audio_spdif_label;
		stx7105_pio_sysconf(10, 6, 1, label);
		pin = stpio_request_pin(10, 6, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (spdif > 1)
		BUG();

	/* Claim second (stereo) set of digital audio outputs (AUDDIG1).
	 * Notice that is has no CLKOUT line, what makes it almost useless... */
	if (pcmout1 > 0) {
		label = stx7105_audio_pcmout1_label;
		if (STX7105) {
			stx7105_pio_sysconf(10, 7, 1, label);
			pin = stpio_request_pin(10, 7, label, STPIO_ALT_OUT);
		} else {
			stx7105_pio_sysconf(11, 2, 1, label);
			pin = stpio_request_pin(11, 2, label, STPIO_ALT_OUT);
		}
		BUG_ON(!pin);
		stx7105_pio_sysconf(11, 0, 1, label);
		pin = stpio_request_pin(11, 0, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
		stx7105_pio_sysconf(11, 1, 1, label);
		pin = stpio_request_pin(11, 1, label, STPIO_ALT_OUT);
		BUG_ON(!pin);
	}
	if (pcmout1 > 1)
		BUG();

	/* Claim PIO pins as digital audio inputs... */

	if (pcmin > 0) {
		BUG_ON(pcmout1 > 0); /* Shared pins */
		label = stx7105_audio_pcmin_label;
		if (STX7105)
			pin = stpio_request_pin(10, 7, label, STPIO_IN);
		else
			pin = stpio_request_pin(11, 2, label, STPIO_IN);
		BUG_ON(!pin);
		pin = stpio_request_pin(11, 0, label, STPIO_IN);
		BUG_ON(!pin);
		pin = stpio_request_pin(11, 1, label, STPIO_IN);
		BUG_ON(!pin);
	}
	if (pcmin > 1)
		BUG();
}



/* PWM resources ---------------------------------------------------------- */

static struct platform_device stx7105_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= 2,
	.resource	= (struct resource []) {
		{
			.start	= 0xfd010000,
			.end	= 0xfd010067,
			.flags	= IORESOURCE_MEM
		}, {
			.start	= evt2irq(0x11c0),
			.end	= evt2irq(0x11c0),
			.flags	= IORESOURCE_IRQ
		}
	},
};

void __init stx7105_configure_pwm(struct plat_stm_pwm_data *data)
{
	int pwm;
	const struct {
		unsigned char port, pin, alt;
	} pwm_pios[2][2] = {
		{ { 4, 4, 3 }, { 13, 0, 3 } }, 	/* PWM0 */
		{ { 4, 5, 3 }, { 13, 1, 3 } },	/* PWM1 */
	};

	stx7105_pwm_device.dev.platform_data = data;

	for (pwm = 0; pwm < 2; pwm++) {
		if (data->flags & (1 << pwm)) {
			int r = (data->routing >> pwm) & 1;
			int port = pwm_pios[pwm][r].port;
			int pin  = pwm_pios[pwm][r].pin;
			int alt  = pwm_pios[pwm][r].alt;

			stx7105_pio_sysconf(port, pin, alt, "pwm");
			stpio_request_pin(port, pin, "pwm", STPIO_ALT_OUT);
		}
	}

	platform_device_register(&stx7105_pwm_device);
}



/* ASC resources ---------------------------------------------------------- */
#define PORT_SC0 (0)
#define PORT_SC1 (1)
static struct platform_device stx7105_asc_devices[] = {
	STASC_SC_DEVICE(0xfd030000, evt2irq(0x1160), 11, 15,
			0xfd048000, 0, 0, 1, 4, 3, 5, 7,
			STPIO_ALT_BIDIR, STPIO_IN, STPIO_OUT,
			STPIO_ALT_OUT, STPIO_OUT, STPIO_IN,
			STASC_FLAG_SCSUPPORT),
		/* oe pin: 2 */
	STASC_DEVICE(0xfd031000, evt2irq(0x1140), 12, 16, 1, 0, 1, 4, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
#if defined (CONFIG_SH_ST_CB180)
       STASC_DEVICE(0xfd032000, evt2irq(0x1120), 13, 17, 12, 0, 1, 2, 3,
               STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
               /* or 4, 0, 1, 2, 3 */
#else
	STASC_DEVICE(0xfd032000, evt2irq(0x1120), 13, 17, 4, 0, 1, 2, 3,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
		/* or 12, 0, 1, 2, 3 */
#endif
	STASC_DEVICE(0xfd033000, evt2irq(0x1100), 14, 18, 5, 0, 1, 3, 2,
		STPIO_ALT_OUT, STPIO_IN, STPIO_IN, STPIO_ALT_OUT),
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation.  */

/* the serial console device */
int stasc_console_device __initdata;

/* Platform devices to register */
struct platform_device
		*stasc_configured_devices[ARRAY_SIZE(stx7105_asc_devices)]
		__initdata;
unsigned int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stx7105_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;
	static int alt_conf[4] = { 3, 4, 3, 2 };
	struct sysconf_field *sc;

	for (i = 0; i < num_ascs; i++) {
		int port;
		unsigned char flags;
		struct platform_device *pdev;
		struct stasc_uart_data *plat_data;

		port = ascs[i] & 0xff;
		flags = ascs[i] >> 8;
		pdev = &stx7105_asc_devices[port];
		plat_data = pdev->dev.platform_data;
		plat_data->flags |= flags;

		if ((port == 2) && (flags & STASC_FLAG_ASC2_PIO12)) {
		  
		  int j;
		  sc = sysconf_claim(SYS_CFG, 7, 1, 2, "asc2");
		  BUG_ON(!sc);
		  sysconf_write(sc, 0x3);
		  
		  alt_conf[2] = 5;
		  for (j = 0; j < 4; j++)
		    plat_data->pios[j].pio_port = 12;
		}

		/* TXD */
		stx7105_pio_sysconf(plat_data->pios[0].pio_port,
				plat_data->pios[0].pio_pin,
				alt_conf[port], "asc");

		/* RTS */
		if (!(plat_data->flags & STASC_FLAG_NORTSCTS))
			stx7105_pio_sysconf(plat_data->pios[3].pio_port,
					plat_data->pios[3].pio_pin,
					alt_conf[port], "asc");

		if ((plat_data->flags & STASC_FLAG_SCSUPPORT))
			stx7105_pio_sysconf(plat_data->pios[1].pio_port,
					plat_data->pios[1].pio_pin,
					alt_conf[port], "asc");

		pdev->id = i;
		if (!STX7105) /* 7106 suffers from the ASC FIFO bug... */
			plat_data->flags |= STASC_FLAG_TXFIFO_BUG;

		stasc_configured_devices[stasc_configured_devices_count++] =
				pdev;
		if ((plat_data->flags & STASC_FLAG_SCSUPPORT)) {
		  sc = sysconf_claim(SYS_CFG, 7, 0, 0, "stascsc");
		  sysconf_write(sc, 1);
		  if (plat_data->pios[0].pio_port == PORT_SC0) {
		      sc = sysconf_claim(SYS_CFG, 7, 4,
					 4, "stascsc");
		      sysconf_write(sc, 1);
		      sc = sysconf_claim(SYS_CFG, 7, 6,
					 8, "stascsc");
		      sysconf_write(sc, 7);
		  }
		}
	}

	stasc_console_device = console;
	/* the console will be always a wakeup-able device */
	stasc_configured_devices[console]->dev.power.can_wakeup = 1;
	device_set_wakeup_enable(&stasc_configured_devices[console]->dev, 0x1);
}

/* Add platform device as configured by board specific code */
static int __init stx7105_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stx7105_add_asc);



/* LiRC resources --------------------------------------------------------- */

static struct lirc_pio stx7105_lirc_pios[] = {
	{
		.bank = 3,
		.pin = 0,
		.dir = STPIO_IN,
		.pinof = 0x00 | LIRC_IR_RX | LIRC_PIO_ON
	}, {
		.bank = 3,
		.pin = 1,
		.dir = STPIO_IN,
		.pinof = 0x00 | LIRC_UHF_RX | LIRC_PIO_ON
	}
#if 0
	,{
		.bank = 3,
		.pin = 2,
		.dir = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	}, {
		.bank = 3,
		.pin = 3,
		.dir = STPIO_ALT_OUT,
		.pinof = 0x00 | LIRC_IR_TX | LIRC_PIO_ON
	},
#endif
};

static struct plat_lirc_data lirc_private_info = {
	/* The clock settings will be calculated by the driver
	 * from the system clock */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv      = 0, /* automatically calculate */
	.irbperiodmult  = 0,
	.irbperioddiv   = 0,
	.irbontimemult  = 0,
	.irbontimediv   = 0,
	.irbrxmaxperiod = 0x5000,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.pio_pin_arr	= stx7105_lirc_pios,
	.num_pio_pins	= ARRAY_SIZE(stx7105_lirc_pios),
#ifdef CONFIG_PM
	.clk_on_low_power = 10000000, /* The lowest valid value is 10MHz ! Otherwise, divider is not set correctly. */
	.maxperiod_on_low_power = 4000,
#endif
};

static struct platform_device stx7105_lirc_device =
	STLIRC_DEVICE(0xfd018000, evt2irq(0x11a0), ILC_EXT_IRQ(4));

void __init stx7105_configure_lirc(lirc_scd_t *scd)
{
	lirc_private_info.scd_info = scd;
//	stx7105_pio_sysconf(3, 2, 3, "lirc");
//	stx7105_pio_sysconf(3, 3, 3, "lirc");
	platform_device_register(&stx7105_lirc_device);
}



/* NAND Setup ------------------------------------------------------------- */

void __init stx7105_configure_nand(struct platform_device *pdev)
{

	/* EMI Bank base address */
	/*  - setup done in stm_nand_emi probe */

	/* NAND Controller base address */
	pdev->resource[0].start	= 0xFE701000;
	pdev->resource[0].end	= 0xFE701FFF;

	/* NAND Controller IRQ */
	pdev->resource[1].start	= evt2irq(0x14a0);
	pdev->resource[1].end	= evt2irq(0x14a0);

	platform_device_register(pdev);
}

/* MMC/SD resources ------------------------------------------------------- */
struct  stx7106_mmc_pin{
	struct {
		unsigned char port, pin, alt;
	} pio;
	unsigned char dir;
};

/* Yes, MMC is supposed to be configured as ALT_OUT, not ALT_BIDIR... */
static struct stx7106_mmc_pin stx7106_mmc_pins[] __initdata = {
	{ { 11, 2, 1 }, STPIO_IN }, /* Card Detect */
	{ { 11, 3, 1 }, STPIO_ALT_OUT }, /* MMC clock */
	{ { 11, 4, 1 }, STPIO_ALT_OUT }, /* MMC command */
	{ { 11, 5, 1 }, STPIO_ALT_OUT }, /* MMC Data 0 */
	{ { 11, 6, 1 }, STPIO_ALT_OUT }, /* MMC Data 1 */
	{ { 11, 7, 1 }, STPIO_ALT_OUT }, /* MMC Data 2 */
	{ { 16, 0, 1 }, STPIO_ALT_OUT }, /* MMC Data 3 */
	{ { 16, 1, 1 }, STPIO_ALT_OUT }, /* MMC Data 4 */
	{ { 16, 2, 1 }, STPIO_ALT_OUT }, /* MMC Data 5 */
	{ { 16, 3, 1 }, STPIO_ALT_OUT }, /* MMC Data 6 */
	{ { 16, 4, 1 }, STPIO_ALT_OUT }, /* MMC Data 7 */
	{ { 16, 5, 1 }, STPIO_ALT_OUT }, /* MMC LED On */
	{ { 16, 6, 1 }, STPIO_ALT_OUT }, /* MMC Power On */
	{ { 16, 7, 1 }, STPIO_IN }, /* MMC Write Protection */
};

static struct platform_device stx7106_mmc_device = {
		.name = "arasan",
		.id = 0,
		.num_resources = 2,
		.resource       = (struct resource[]) {
			{
				.start = 0xfd100000,
				.end   = 0xfd100400,
				.flags  = IORESOURCE_MEM,
			}, {
				.name   = "mmcirq",
				.start  = ILC_EXT_IRQ(41),
				.end    = ILC_EXT_IRQ(41),
				.flags  = IORESOURCE_IRQ,
			}
		}
};

void __init stx7106_configure_mmc(void)
{
	struct sysconf_field *sc;
	struct stx7106_mmc_pin *pins;
	int pins_num, i;

	/* MMC clock comes from the ClockGen_B bank1, channel 2;
	 * this clock has been set to 52MHz.
	 * For supporting SD High-Speed mode we need to set it
	 * to 50MHz.
	 */
	struct clk *clk = clk_get(NULL, "CLKB_FS1_CH2");
	clk_set_rate(clk, 50000000);

	/* Out Enable coms from the MMC */
	sc = sysconf_claim(SYS_CFG, 17, 0, 0, "mmc");
	sysconf_write(sc, 1);

	pins = stx7106_mmc_pins;
	pins_num = ARRAY_SIZE(stx7106_mmc_pins);

	for (i = 0; i < pins_num; i++) {
		struct stx7106_mmc_pin *pin = &pins[i];

		stpio_request_pin(pin->pio.port, pin->pio.pin, "mmc", pin->dir);
		if (pin->dir != STPIO_IN)
			stx7105_pio_sysconf(pin->pio.port, pin->pio.pin,
					    pin->pio.alt, "mmc");
	}

	platform_device_register(&stx7106_mmc_device);
}


/* SPI FSM setup ---------------------------------------------------------- */
static struct platform_device stx7106_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe702000,
			.end	= 0xfe7024ff,
			.flags	= IORESOURCE_MEM,
		},
	},
};

void __init stx7106_configure_spifsm(struct stm_plat_spifsm_data *spifsm_data)
{
	struct stpio_pin *pin;

	/* Not available on stx7105 */
	if (STX7105)
		BUG();

	/* Configure pads for SPIBoot FSM */

	/* Note, output pads must be configured as ALT_OUT rather than ALT_BIDIR
	 * (see bug GNBvd8843).  As a result, FSM dual mode is not supported on
	 * stx7106.
	 */
	stx7105_pio_sysconf(15, 0, 1, "SPIBoot CLK");
	pin = stpio_request_pin(15, 0, "SPIBoot CLK", STPIO_ALT_OUT);
	BUG_ON(!pin);

	stx7105_pio_sysconf(15, 1, 1, "SPIBoot DOUT");
	pin = stpio_request_pin(15, 1, "SPIBoot DOUT", STPIO_ALT_OUT);
	BUG_ON(!pin);

	stx7105_pio_sysconf(15, 2, 1, "SPIBoot NOTCS");
	pin = stpio_request_pin(15, 2, "SPIBoot NOTCS", STPIO_ALT_OUT);
	BUG_ON(!pin);

	pin = stpio_request_pin(15, 3, "SPIBoot DIN", STPIO_IN);
	BUG_ON(!pin);

	stx7106_spifsm_device.dev.platform_data = spifsm_data;

	platform_device_register(&stx7106_spifsm_device);
}



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int stx7105_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin)
{
	int irq;
	int pin_type;

	if ((pin > 4) || (pin < 1))
		return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch (pin_type) {
	case PCI_PIN_ALTERNATIVE:
		/* There is an alternative for the INTA only! */
		if (pin != 1) {
			BUG();
			irq = -1;
			break;
		}
		/* Fall through */
	case PCI_PIN_DEFAULT:
		/* There are only 3 dedicated interrupt lines! */
		if (pin == 4) {
			BUG();
			irq = -1;
			break;
		}
		irq = ILC_EXT_IRQ(pin + 25);
		break;
	case PCI_PIN_UNUSED:
		irq = -1; /* Not used */
		break;
	default:
		irq = pin_type; /* Take whatever interrupt you are told */
		break;
	}

	return irq;
}

static struct platform_device stx7105_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		{
			.name = "Memory",
#ifdef CONFIG_32BIT
			.start = 0xc0000000,
			.end = 0xdfffffff, /* 512 MB */
#else
			.start = 0x08000000,
			.end = 0x0bffffff, /* 64 MB */
#endif
			.flags = IORESOURCE_MEM,
		}, {
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		}, {
			.name = "EMISS",
			.start = 0xfe400000,
			.end = 0xfe4017fc,
			.flags = IORESOURCE_MEM,
		}, {
			.name = "PCI-AHB",
			.start = 0xfe560000,
			.end = 0xfe5600ff,
			.flags = IORESOURCE_MEM,
		}, {
			.name = "DMA",
			.start = evt2irq(0x1280),
			.end = evt2irq(0x1280),
			.flags = IORESOURCE_IRQ,
		}, {
			.name = "Error",
			.start = ILC_EXT_IRQ(25),
			.end = ILC_EXT_IRQ(25),
			.flags = IORESOURCE_IRQ,
		}, { /* Keep this one last */
			.name = "SERR",
			/* .start & .end set in stx7105_configure_pci() */
			.flags = IORESOURCE_IRQ,
		}
	},
};

void __init stx7105_configure_pci(struct pci_config_data *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* LLA clocks have these horrible names... */
	pci_conf->clk_name = "CLKA_PCI";

	/* 7105 cut 3 has req0 wired to req3 to work around NAND problems;
	 * the same story for 7106 */
	pci_conf->req0_to_req3 = !STX7105 || (cpu_data->cut_major >= 3);

	/* Additionally, we are not supposed to configure the req0/req3
	 * to PCI mode on 7105... */
	pci_conf->req0_emi = STX7105 && (cpu_data->cut_major >= 3);

	/* Fill in the default values for the 7105 */
	if (!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	stx7105_pci_device.dev.platform_data = pci_conf;

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI Power");
	sysconf_write(sc, 0); /* We will need to stash this somewhere
				 for power management. */
	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI Power status");
	while (sysconf_read(sc))
		cpu_relax(); /* Loop until powered up */

	/* Claim and set pads into PCI mode */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* PCI_CLOCK_MASTER_NOT_SLAVE:
	 * 0: PCI clock is slave
	 * 1: PCI clock is master */
	sc = sysconf_claim(SYS_CFG, 5, 28, 28, "PCI");
	sysconf_write(sc, 1);

	/* REQ/GNT[0] are dedicated EMI pins */
	BUG_ON(pci_conf->req_gnt[0] != PCI_PIN_DEFAULT);

	/* Configure the REQ/GNT[1..2], muxed with PIOs */
	for (i = 1; i < 4; i++) {
		static const char *req_name[] = {
			"PCI REQ 0",
			"PCI REQ 1",
			"PCI REQ 2",
			"PCI REQ 3"
		};
		static const char *gnt_name[] = {
			"PCI GNT 0 ",
			"PCI GNT 1",
			"PCI GNT 2",
			"PCI GNT 3"
		};

		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			/* Is there REQ/GNT[3] at all? */
			BUG_ON(pci_conf->req0_to_req3 && i == 3);

			if (!stpio_request_pin(6, 4 + i, req_name[i], STPIO_IN))
				printk(KERN_ERR "Unable to configure PIO for "
						"%s\n", req_name[i]);

			if (stpio_request_pin(7, i, gnt_name[i], STPIO_ALT_OUT))
				stx7105_pio_sysconf(7, i, 4, gnt_name[i]);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"%s\n", gnt_name[i]);
			break;
		case PCI_PIN_UNUSED:
			/* Unused is unused - nothing to do */
			break;
		default:
			/* No alternative here... */
			BUG();
			break;
		}
	}

	/* Configure interrupt PIOs */
	for (i = 0; i < 3; i++) {
		static const char *int_name[] = {
			"PCI INT A",
			"PCI INT B",
			"PCI INT C",
		};

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_ALTERNATIVE:
			if (i != 0) {
				BUG();
				break;
			}
			/* PCI_INT0_SRC_SEL:
			 * 0: PCI_INT_FROM_DEVICE[0] is from PIO6[0]
			 * 1: PCI_INT_FROM_DEVICE[0] is from PIO15[3] */
			sc = sysconf_claim(SYS_CFG, 5, 27, 27, "PCI");
			sysconf_write(sc, 1);
			set_irq_type(ILC_EXT_IRQ(26), IRQ_TYPE_LEVEL_LOW);
			if (!stpio_request_pin(15, 3, int_name[0], STPIO_IN))
				printk(KERN_ERR "Unable to claim PIO for "
						"%s\n", int_name[0]);
			/* PCI_INT0_FROM_DEVICE:
			 * 0: Indicates disabled
			 * 1: Indicates PCI_INT_FROM_DEVICE[0] is enabled */
			sc = sysconf_claim(SYS_CFG, 5, 21, 21, "PCI");
			sysconf_write(sc, 1);
			break;
		case PCI_PIN_DEFAULT:
			if (i == 0) {
				/* PCI_INT0_SRC_SEL:
				 * 0: PCI_INT_FROM_DEVICE[0] is from PIO6[0]
				 * 1: PCI_INT_FROM_DEVICE[0] is from PIO15[3] */
				sc = sysconf_claim(SYS_CFG, 5, 27, 27, "PCI");
				sysconf_write(sc, 0);
			}
			set_irq_type(ILC_EXT_IRQ(26 + i), IRQ_TYPE_LEVEL_LOW);
			if (!stpio_request_pin(6, i, int_name[i], STPIO_IN))
				printk(KERN_ERR "Unable to claim PIO for "
						"%s\n", int_name[i]);
			/* PCI_INTn_FROM_DEVICE:
			 * 0: Indicates disabled
			 * 1: Indicates PCI_INT_FROM_DEVICE[n] is enabled */
			sc = sysconf_claim(SYS_CFG, 5, 21 - i, 21 - i, "PCI");
			sysconf_write(sc, 1);
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
	}
	BUG_ON(pci_conf->pci_irq[3] != PCI_PIN_UNUSED);

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (stpio_request_pin(15, 4, "PCI SERR#", STPIO_IN)) {
			pci_conf->serr_irq = gpio_to_irq(stpio_to_gpio(15, 4));
			set_irq_type(pci_conf->serr_irq, IRQ_TYPE_LEVEL_LOW);
		} else {
			printk(KERN_WARNING "%s(): Failed to claim PCI SERR# "
					"PIO!\n", __func__);
			pci_conf->serr_irq = PCI_PIN_UNUSED;
		}
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here */
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		stx7105_pci_device.num_resources--;
	} else {
		/* The SERR IRQ resource is last */
		int res_num = stx7105_pci_device.num_resources - 1;
		struct resource *res = &stx7105_pci_device.resource[res_num];

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	/* LOCK is not claimed as is totally pointless, the SOCs do not
	 * support any form of coherency */

	platform_device_register(&stx7105_pci_device);
}



/* Other resources -------------------------------------------------------- */

static struct platform_device stx7105_ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd000000,
			.end	= 0xfd000900,
			.flags	= IORESOURCE_MEM
		}
	},
};

static struct platform_device stx7105_sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfe001000,
			.end	= 0xfe0011df,
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

static struct platform_device stx7105_pio_devices[] = {
	STPIO_DEVICE(0, 0xfd020000, evt2irq(0xc00)),
	STPIO_DEVICE(1, 0xfd021000, evt2irq(0xc80)),
	STPIO_DEVICE(2, 0xfd022000, evt2irq(0xd00)),
	STPIO_DEVICE(3, 0xfd023000, evt2irq(0x1060)),
	STPIO_DEVICE(4, 0xfd024000, evt2irq(0x1040)),
	STPIO_DEVICE(5, 0xfd025000, evt2irq(0x1020)),
	STPIO_DEVICE(6, 0xfd026000, evt2irq(0x1000)),
	/* Standalone PIO at fe01 - fe01ffff */
	/* Int evt2irq(0xb40)) */
	STPIO10_DEVICE(0xfe010000, evt2irq(0xb40), 7, 10),
};

static struct platform_device stx7105_rng_dev_hwrandom_device = {
	.name	   = "stm_hwrandom",
	.id	     = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

static struct platform_device stx7105_rng_dev_random_device = {
	.name           = "stm_rng",
	.id             = 0,
	.num_resources  = 1,
	.resource       = (struct resource[]){
		{
			.start  = 0xfe250000,
			.end    = 0xfe250fff,
			.flags  = IORESOURCE_MEM
		},
	}
};

static struct platform_device stx7105_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.name = "STx7105/STx7106 chip temperature",
		.pdn = { SYS_CFG, 41, 4, 4 },
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
	},
};

static struct platform_device stx7105_emi = STEMI();

static struct platform_device stx7105_lpc =
	STLPC_DEVICE(0xfd008000, ILC_EXT_IRQ(7), IRQ_TYPE_EDGE_FALLING,
			0, 1, "CLKB_LPC");



/* Early devices initialization ------------------------------------------- */

/* Initialise devices which are required early in the boot process. */
void __init stx7105_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7105_sysconf_device, 1);
	stpio_early_init(stx7105_pio_devices, ARRAY_SIZE(stx7105_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx710%d version %ld.x\n", STX7105 ? 5 : 6,
			chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7105_postcore_setup(void)
{
	int i;

	emi_init(0, 0xfe700000);

	for (i = 0; i < ARRAY_SIZE(stx7105_pio_devices); i++)
		platform_device_register(&stx7105_pio_devices[i]);

	return 0;
}
postcore_initcall(stx7105_postcore_setup);



/* Late resources --------------------------------------------------------- */

static struct platform_device *stx7105_devices[] __initdata = {
	&stx7105_fdma_devices[0],
//	&stx7105_fdma_devices[1],
	&stx7105_fdma_xbar_device,
	&stx7105_sysconf_device,
	&stx7105_ilc3_device,
	&stx7105_rng_dev_hwrandom_device,
	&stx7105_rng_dev_random_device,
	&stx7105_temp_device,
	&stx7105_emi,
	&stx7105_lpc,
};

#include "./platform-pm-stx7105.c"

static int __init stx7105_devices_setup(void)
{
	platform_add_pm_devices(stx7105_pm_devices,
			ARRAY_SIZE(stx7105_pm_devices));

	return platform_add_devices(stx7105_devices,
			ARRAY_SIZE(stx7105_devices));
}
device_initcall(stx7105_devices_setup);

/* Warm reboot --------------------------------------------------------- */

void stx7105_prepare_restart(void)
{
	struct sysconf_field *sc1;
	struct sysconf_field *sc2;
	struct clk *sys_clk = NULL;
	struct clk *osc_clk = NULL;

	/* Ensure the reset period is short and that the reset is not masked */
	sc1 = sysconf_claim(SYS_CFG, 9, 29, 29, "kernel");
	sc2 = sysconf_claim(SYS_CFG, 9, 0, 25, "kernel");
	sysconf_write(sc1, 0x0);
	sysconf_write(sc2, 0x00000a8c);

	/* Slow the EMI clock down. This clock is used to drive serial flash
	 * at boot time, and some larger flash parts need to be driven as
	 * slowly as 20MHz. Note that the SPI boot clock divider is reset to
	 * 2 on watchdog reset. */
	sys_clk = clk_get(NULL, "CLKA_EMI_MASTER");
	osc_clk = clk_get(NULL, "CLKA_REF");
	clk_set_parent(sys_clk, osc_clk);
	clk_set_rate(sys_clk, clk_get_rate(osc_clk));
}

static int __init stx7105_reset_init(void)
{
	struct sysconf_field *sc;

	/* Set the reset chain correctly */
	sc = sysconf_claim(SYS_CFG, 9, 27, 28, "reset_chain");
	sysconf_write(sc, 0);

	/* Release the sysconf bits so the coprocessor driver can claim them */
	sysconf_release(sc);

	/* Add stx_7105_prepare_restart to the list of functions to be called
	 * immediately before a warm reboot. */
	register_prepare_restart_handler(stx7105_prepare_restart);

	return 0;
}
arch_initcall(stx7105_reset_init);

/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2, WDT, HUDI,

	I2S2SPDIF0,					/* Group 0 */
	I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3, SATA_DMAC,
	SATA_HOSTC, DVP, STANDALONE_PIO, AUX_VDP_END_PROC,
	AUX_VDP_FIFO_EMPTY, COMPO_CAP_BF, COMPO_CAP_TF,
	PIO0,
	PIO1,
	PIO2,

	PIO6, PIO5, PIO4, PIO3,				/* Group 1 */
	SSC3, SSC2, SSC1, SSC0,				/* Group 2 */
	UART3, UART2, UART1, UART0,			/* Group 3 */
	IRB_WAKEUP, IRB, PWM, MAFE,			/* Group 4 */
	SBAG, BDISP_AQ, DAA, TTXT,			/* Group 5 */
	EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER,		/* Group 6 */
	LX_DELTAMU, LX_AUD, DCXO, PTI1,			/* Group 7 */
	FDMA0, FDMA1, OHCI1, EHCI1,			/* Group 8 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,		/* Group 9 */
	TVO_DCS0, NAND, DELMU_PP, DELMU_MBE,		/* Group 10 */
	MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,	/* Group 11 */
	MAIN_VTG, AUX_VTG,
	HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP,		/* Group 12 */
	PTI0, PDES_ESA, PDES, PDES_READ_CW,		/* Group 13 */
	TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,		/* Group 14 */
	CRIPTO_SIG_CHK,
	OHCI0, EHCI0, TVO_DCS1, BDISP_CQ,		/* Group 15 */
	ICAM3_KTE, ICAM3, KEY_SCANNER, MES,		/* Group 16 */

	/* interrupt groups */
	GROUP0_0, GROUP0_1,
	GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
	GROUP16
};

static struct intc_vect stx7105_intc_vectors[] = {
	INTC_VECT(TMU0, 0x400),
	INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440), INTC_VECT(TMU2, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(I2S2SPDIF0, 0xa00),
	INTC_VECT(I2S2SPDIF1, 0xa20), INTC_VECT(I2S2SPDIF2, 0xa40),
	INTC_VECT(I2S2SPDIF3, 0xa60), INTC_VECT(SATA_DMAC, 0xa80),
	INTC_VECT(SATA_HOSTC, 0xb00), INTC_VECT(DVP, 0xb20),
	INTC_VECT(STANDALONE_PIO, 0xb40), INTC_VECT(AUX_VDP_END_PROC, 0xb60),
	INTC_VECT(AUX_VDP_FIFO_EMPTY, 0xb80), INTC_VECT(COMPO_CAP_BF, 0xba0),
	INTC_VECT(COMPO_CAP_TF, 0xbc0),
	INTC_VECT(PIO0, 0xc00),
	INTC_VECT(PIO1, 0xc80),
	INTC_VECT(PIO2, 0xd00),

	INTC_VECT(PIO6, 0x1000), INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC3, 0x1080), INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(SBAG, 0x1200), INTC_VECT(BDISP_AQ, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI_PCI, 0x1280), INTC_VECT(GMAC_PMT, 0x12a0),
	INTC_VECT(GMAC, 0x12c0), INTC_VECT(TS_MERGER, 0x12e0),
	INTC_VECT(LX_DELTAMU, 0x1300), INTC_VECT(LX_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA0, 0x1380), INTC_VECT(FDMA1, 0x13a0),
	INTC_VECT(OHCI1, 0x13c0), INTC_VECT(EHCI1, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(TVO_DCS0, 0x1480), INTC_VECT(NAND, 0x14a0),
	INTC_VECT(DELMU_PP, 0x14c0), INTC_VECT(DELMU_MBE, 0x14e0),
	INTC_VECT(MAIN_VDP_FIFO_EMPTY, 0x1500),
		INTC_VECT(MAIN_VDP_END_PROCESSING, 0x1520),
	INTC_VECT(MAIN_VTG, 0x1540), INTC_VECT(AUX_VTG, 0x1560),
	INTC_VECT(HDMI_CEC_WAKEUP, 0x1580), INTC_VECT(HDMI_CEC, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI0, 0x1600), INTC_VECT(PDES_ESA, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PDES_READ_CW, 0x1660),
	INTC_VECT(TKDMA_TKD, 0x1680), INTC_VECT(TKDMA_DMA, 0x16a0),
	INTC_VECT(CRIPTO_SIGDMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI0, 0x1700), INTC_VECT(EHCI0, 0x1720),
	INTC_VECT(TVO_DCS1, 0x1740), INTC_VECT(BDISP_CQ, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(KEY_SCANNER, 0x17c0), INTC_VECT(MES, 0x17e0),
};

static struct intc_group stx7105_intc_groups[] = {
	/* I2S2SPDIF0 is a single bit group */
	INTC_GROUP(GROUP0_0, I2S2SPDIF1, I2S2SPDIF2, I2S2SPDIF3, SATA_DMAC),
	INTC_GROUP(GROUP0_1, SATA_HOSTC, DVP, STANDALONE_PIO,
			AUX_VDP_END_PROC, AUX_VDP_FIFO_EMPTY,
			COMPO_CAP_BF, COMPO_CAP_TF),
	/* PIO0, PIO1, PIO2 are single bit groups */

	INTC_GROUP(GROUP1, PIO6, PIO5, PIO4, PIO3),
	INTC_GROUP(GROUP2, SSC3, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP3, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP4, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP5, SBAG, BDISP_AQ, DAA, TTXT),
	INTC_GROUP(GROUP6, EMPI_PCI, GMAC_PMT, GMAC, TS_MERGER),
	INTC_GROUP(GROUP7, LX_DELTAMU, LX_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP8, FDMA0, FDMA1, OHCI1, EHCI1),
	INTC_GROUP(GROUP9, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP10, TVO_DCS0, NAND, DELMU_PP, DELMU_MBE),
	INTC_GROUP(GROUP11, MAIN_VDP_FIFO_EMPTY, MAIN_VDP_END_PROCESSING,
		   MAIN_VTG, AUX_VTG),
	INTC_GROUP(GROUP12, HDMI_CEC_WAKEUP, HDMI_CEC, HDMI, HDCP),
	INTC_GROUP(GROUP13, PTI0, PDES_ESA, PDES, PDES_READ_CW),
	INTC_GROUP(GROUP14, TKDMA_TKD, TKDMA_DMA, CRIPTO_SIGDMA,
		   CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP15, OHCI0, EHCI0, TVO_DCS1, BDISP_CQ),
	INTC_GROUP(GROUP16, ICAM3_KTE, ICAM3, KEY_SCANNER, MES),
};

static struct intc_prio_reg stx7105_intc_prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },

	/* offset against          31-28,    27-24,    23-20,     19-16,
	   intc2_base below        15-12,     11-8,      7-4,       3-0  */
	{ 0x00000000, 0, 32, 4,
		/* INTPRI00 */ {       0,        0,     PIO2,       PIO1,
				    PIO0, GROUP0_1, GROUP0_0, I2S2SPDIF0 } },
	{ 0x00000004, 0, 32, 4,
		/* INTPRI04 */ {  GROUP8,   GROUP7,   GROUP6,     GROUP5,
				  GROUP4,   GROUP3,   GROUP2,     GROUP1 } },
	{ 0x00000008, 0, 32, 4,
		/* INTPRI08 */ { GROUP16,  GROUP15,  GROUP14,    GROUP13,
				 GROUP12,  GROUP11,  GROUP10,     GROUP9 } },
};

static struct intc_mask_reg stx7105_intc_mask_registers[] = {
	/* offsets against
	 * intc2_base below */
	{ 0x00000040, 0x00000060, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    COMPO_CAP_TF, COMPO_CAP_BF, AUX_VDP_FIFO_EMPTY,	/* 11...8 */
		AUX_VDP_END_PROC,
	    STANDALONE_PIO, DVP, SATA_HOSTC, SATA_DMAC,		/*  7...4 */
	    I2S2SPDIF3, I2S2SPDIF2, I2S2SPDIF1, I2S2SPDIF0 } },	/*  3...0 */
	{ 0x00000044, 0x00000064, 32, /* INTMSK04 / INTMSKCLR04 */
	  { EHCI1, OHCI1, FDMA1, FDMA0,				/* 31..28 */
	    PTI1, DCXO, LX_AUD, LX_DELTAMU,			/* 27..24 */
	    TS_MERGER, GMAC, GMAC_PMT, EMPI_PCI,		/* 23..20 */
	    TTXT, DAA, BDISP_AQ, SBAG,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP,				/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, SSC3, 				/*  7...4 */
	    PIO3, PIO4, PIO5,  PIO6  } },			/*  3...0 */
	{ 0x00000048, 0x00000068, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES, KEY_SCANNER, ICAM3, ICAM3_KTE,			/* 31..28 */
	    BDISP_CQ, TVO_DCS1, EHCI0, OHCI0,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIGDMA, TKDMA_DMA, TKDMA_TKD,/* 23..20 */
	    PDES_READ_CW, PDES, PDES_ESA, PTI0,			/* 19..16 */
	    HDCP, HDMI, HDMI_CEC, HDMI_CEC_WAKEUP,		/* 15..12 */
	    AUX_VTG, MAIN_VTG, MAIN_VDP_END_PROCESSING,		/* 11...8 */
		 MAIN_VDP_FIFO_EMPTY,
	    DELMU_MBE, DELMU_PP, NAND, TVO_DCS0,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(stx7105_intc_desc, "stx7105", stx7105_intc_vectors,
		stx7105_intc_groups, stx7105_intc_mask_registers,
		stx7105_intc_prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;
	unsigned long intc2_base = (unsigned long)ioremap(0xfe001300, 0x100);
	int i;

	ilc_early_init(&stx7105_ilc3_device);

	for (i = 4; i < ARRAY_SIZE(stx7105_intc_prio_registers); i++)
		stx7105_intc_prio_registers[i].set_reg += intc2_base;
	for (i = 0; i < ARRAY_SIZE(stx7105_intc_mask_registers); i++) {
		stx7105_intc_mask_registers[i].set_reg += intc2_base;
		stx7105_intc_mask_registers[i].clr_reg += intc2_base;
	}

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&stx7105_intc_desc);

	for (i = 0; i < 16; i++) {
		set_irq_chip(i, &dummy_irq_chip);
		set_irq_chained_handler(i, ilc_irq_demux);
	}

	ilc_demux_init();
}
