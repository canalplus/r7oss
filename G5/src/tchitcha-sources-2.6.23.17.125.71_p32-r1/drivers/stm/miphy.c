/*
 * STMicroelectronics MiPHY driver
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <asm/processor.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stm/soc.h>
#include <linux/stm/sysconf.h>
#include "tap.h"



/* To be used by SOC setup code */
int stm_miphy_sysconf_jtag_tick(int tms, int tdi, void *priv)
{
	struct stm_miphy_sysconf_soft_jtag *jtag = priv;

	sysconf_write(jtag->tck, 0);

	sysconf_write(jtag->tms, tms);
	sysconf_write(jtag->tdi, tdi);

	sysconf_write(jtag->tck, 1);

	return sysconf_read(jtag->tdo);
}



static int stm_miphy_ports;
static int stm_miphy_current_port = -1;
static struct stm_tap *stm_miphy_tap;

#define IR_SIZE				3
#define IR_MACROCELL_RESET_ACCESS	0x4	/* 100 */
#define IR_MACRO_MICRO_BUS_ACCESS	0x5	/* 101 */
#define IR_BYPASS			0x7	/* 111 */

static void __init stm_miphy_select(int port)
{
	unsigned int value = 0;
	int chain_size;
	int i;

	pr_debug("%s(port=%d)\n", __func__, port);

	if (stm_miphy_current_port == port)
		return;

	/* Create instructions chain - MACROMICROBUS_ACCESS for selected
	 * port, BYPASS for all other ones */
	for (i = 0; i < stm_miphy_ports; i++) {
		int shift = (stm_miphy_ports - 1 - i) * IR_SIZE;

		if (i == port)
			value |= IR_MACRO_MICRO_BUS_ACCESS << shift;
		else
			value |= IR_BYPASS << shift;
	}

	/* Every port has IR_SIZE bits for instruction */
	chain_size = stm_miphy_ports * IR_SIZE;

	/* Set the instructions */
	stm_tap_shift_ir(stm_miphy_tap, &value, NULL, chain_size);

	stm_miphy_current_port = port;
}

#define DR_SIZE		18
#define DR_WR		0x1	/* DR[1..0] = 01 */
#define DR_RD		0x2	/* DR[1..0] = 10 */
#define DR_ADDR_SHIFT	2	/* DR[9..2] */
#define DR_ADDR_MASK	0xff
#define DR_DATA_SHIFT	10	/* DR[17..10] */
#define DR_DATA_MASK	0xff

static u8 __init stm_miphy_read(int port, u8 addr)
{
	unsigned int value = 0;
	int chain_size;

	pr_debug("%s(port=%d, addr=0x%x)\n", __func__, port, addr);

	BUG_ON(addr & ~DR_ADDR_MASK);

	stm_miphy_select(port);

	/* Create "read access" value */
	value = DR_RD | (addr << DR_ADDR_SHIFT);

	/* Shift value to add BYPASS bits for ports farther in chain */
	value <<= stm_miphy_ports - 1 - port;

	/* Overall chain length is the DR_SIZE for meaningful
	 * command plus 1 bit per every BYPASSed port */
	chain_size = DR_SIZE + (stm_miphy_ports - 1);

	/* Set "read access" command */
	stm_tap_shift_dr(stm_miphy_tap, &value, NULL, chain_size);

	/* Now read back value */
	stm_tap_shift_dr(stm_miphy_tap, NULL, &value, chain_size);

	/* Remove "BYPASSed" bits */
	value >>= stm_miphy_ports - 1 - port;

	/* The RD bit should be cleared by now... */
	BUG_ON(value & DR_RD);

	/* Extract the result */
	value >>= DR_DATA_SHIFT;
	BUG_ON(value & ~DR_DATA_MASK);

	pr_debug("%s()=0x%x\n", __func__, value);

	return value;
}

static void __init stm_miphy_write(int port, u8 addr, u8 data)
{
	unsigned int value = 0;

	pr_debug("%s(port=%d, addr=0x%x, data=0x%x)\n",
			__func__, port, addr, data);

	BUG_ON(addr & ~DR_ADDR_MASK);
	BUG_ON(data & ~DR_DATA_MASK);

	stm_miphy_select(port);

	/* Create "write access" value */
	value = DR_WR | (addr << DR_ADDR_SHIFT) | (data << DR_DATA_SHIFT);

	/* Shift value to add BYPASS bits for ports farther in chain */
	value <<= stm_miphy_ports - 1 - port;

	/* Overall chain length is the DR_SIZE for meaningful
	 * command plus 1 bit per every BYPASSed port */
	stm_tap_shift_dr(stm_miphy_tap, &value, NULL,
			DR_SIZE + (stm_miphy_ports - 1));
}



static void __init stm_miphy_start_port0(void)
{
	int timeout;

	/* TODO: Get rid of this */
	if (cpu_data->type == CPU_STX7108) {
		/*Force SATA port 1 in Slumber Mode */
		stm_miphy_write(1, 0x11, 0x8);
		/*Force Power Mode selection from MiPHY soft register 0x11 */
		stm_miphy_write(1, 0x10, 0x4);
	}

	/* Force Macro1 in reset and request PLL calibration reset */

	/* Force PLL calibration reset, PLL reset and assert
	 * Deserializer Reset */
	stm_miphy_write(0, 0x00, 0x16);
	stm_miphy_write(0, 0x11, 0x0);
	/* Force macro1 to use rx_lspd, tx_lspd (by default rx_lspd
	 * and tx_lspd set for Gen1)  */
	stm_miphy_write(0, 0x10, 0x1);

	/* Force Recovered clock on first I-DLL phase & all
	 * Deserializers in HP mode */

	/* Force Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(0, 0x72, 0x40);
	/* Force Des in HP mode on macro1 */
	stm_miphy_write(0, 0x12, 0x00);

	/* Wait for HFC_READY = 0 */
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (stm_miphy_read(0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	/* Restart properly Process compensation & PLL Calibration */

	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(0, 0x41, 0x1E);
	/* comsr compensation reference */
	stm_miphy_write(0, 0x42, 0x28);
	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(0, 0x41, 0x1E);
	/* comsr cal gives more suitable results in fast PVT for comsr
	   used by TX buffer to build slopes making TX rise/fall fall
	   times. */
	stm_miphy_write(0, 0x42, 0x33);
	/* Force VCO current to value defined by address 0x5A */
	stm_miphy_write(0, 0x51, 0x2);
	/* Force VCO current to value defined by address 0x5A */
	stm_miphy_write(0, 0x5A, 0xF);
	/* Enable auto load compensation for pll_i_bias */
	stm_miphy_write(0, 0x47, 0x2A);
	/* Force restart compensation and enable auto load for
	 * Comzc_Tx, Comzc_Rx & Comsr on macro1 */
	stm_miphy_write(0, 0x40, 0x13);

	/* Wait for comzc & comsr done */
	while ((stm_miphy_read(0, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/* Recommended settings for swing & slew rate FOR SATA GEN 1
	 * from CPG */
	stm_miphy_write(0, 0x20, 0x00);
	/* (Tx Swing target 500-550mV peak-to-peak diff) */
	stm_miphy_write(0, 0x21, 0x2);
	/* (Tx Slew target120-140 ps rising/falling time) */
	stm_miphy_write(0, 0x22, 0x4);

	/* Force Macro1 in partial mode & release pll cal reset */
	stm_miphy_write(0, 0x00, 0x10);
	udelay(10);

#if 0
	/* SSC Settings. SSC will be enabled through Link */
	stm_miphy_write(0, 0x53, 0x00); /* pll_offset */
	stm_miphy_write(0, 0x54, 0x00); /* pll_offset */
	stm_miphy_write(0, 0x55, 0x00); /* pll_offset */
	stm_miphy_write(0, 0x56, 0x04); /* SSC Ampl=0.48% */
	stm_miphy_write(0, 0x57, 0x11); /* SSC Ampl=0.48% */
	stm_miphy_write(0, 0x58, 0x00); /* SSC Freq=31KHz */
	stm_miphy_write(0, 0x59, 0xF1); /* SSC Freq=31KHz */
	/*SSC Settings complete*/
#endif

	stm_miphy_write(0, 0x50, 0x8D);
	stm_miphy_write(0, 0x50, 0x8D);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13) */
	while ((stm_miphy_read(0, 0x01) & 0x03) != 0x03)
		;

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	stm_miphy_write(0, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(0, 0x72, 0x00);

	/* Deassert deserializer reset */
	stm_miphy_write(0, 0x00, 0x00);
	/* des_bit_lock_en is set */
	stm_miphy_write(0, 0x02, 0x08);

	/* bit lock detection strength */
	stm_miphy_write(0, 0x86, 0x61);
}

static void __init stm_miphy_start_port1(void)
{
	int timeout;

	/* Force PLL calibration reset, PLL reset and assert Deserializer
	 * Reset */
	stm_miphy_write(1, 0x00, 0x2);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro2 */
	stm_miphy_write(1, 0x40, 0x13);

	/* Force PLL reset  */
	stm_miphy_write(0, 0x00, 0x2);
	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(0, 0x41, 0x1E);
	/* to get more optimum result on comsr calibration giving faster
	 * rise/fall time in SATA spec Gen1 useful for some corner case.*/
	stm_miphy_write(0, 0x42, 0x33);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro1 */
	stm_miphy_write(0, 0x40, 0x13);

	/*Wait for HFC_READY = 0*/
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (stm_miphy_read(0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	stm_miphy_write(1, 0x11, 0x0);
	/* Force macro2 to use rx_lspd, tx_lspd  (by default rx_lspd and
	 * tx_lspd set for Gen1) */
	stm_miphy_write(1, 0x10, 0x1);
	/* Force Rx_Clock on first I-DLL phase on macro2*/
	stm_miphy_write(1, 0x72, 0x40);
	/* Force Des in HP mode on macro2 */
	stm_miphy_write(1, 0x12, 0x00);

	while ((stm_miphy_read(1, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/*RECOMMENDED SETTINGS for Swing & slew rate FOR SATA GEN 1 from CPG*/
	stm_miphy_write(1, 0x20, 0x00);
	/*(Tx Swing target 500-550mV peak-to-peak diff) */
	stm_miphy_write(1, 0x21, 0x2);
	/*(Tx Slew target120-140 ps rising/falling time) */
	stm_miphy_write(1, 0x22, 0x4);
	/*Force Macr21 in partial mode & release pll cal reset */
	stm_miphy_write(1, 0x00, 0x10);
	udelay(10);
	/* Release PLL reset  */
	stm_miphy_write(0, 0x00, 0x0);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13)*/
	while ((stm_miphy_read(1, 0x01) & 0x03) != 0x03)
		cpu_relax();

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	stm_miphy_write(1, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(1, 0x72, 0x00);

	/* Deassert deserializer reset */
	stm_miphy_write(1, 0x00, 0x00);
	/*des_bit_lock_en is set */
	stm_miphy_write(1, 0x02, 0x08);

	/*bit lock detection strength */
	stm_miphy_write(1, 0x86, 0x61);
}



void __init stm_miphy_init(struct stm_miphy *miphy, int port)
{
	BUG_ON(!miphy);
	BUG_ON(miphy->ports_num < 1);
	BUG_ON(miphy->ports_num > 2);

	stm_miphy_ports = miphy->ports_num;
	stm_miphy_tap = stm_tap_init(miphy->jtag_tick, miphy->jtag_priv);

	stm_tap_enable(stm_miphy_tap);

	switch (port) {
	case 0:
		stm_miphy_start_port0();
		break;
	case 1:
		stm_miphy_start_port1();
		break;
	default:
		BUG();
		break;
	}

	stm_tap_disable(stm_miphy_tap);

	stm_tap_free(stm_miphy_tap);
}
