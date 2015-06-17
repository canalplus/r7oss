/* calibrate.c: default delay calibration
 *
 *  <linux>/arch/sh/lib/calibrate.c
 *  -------------------------------------------------------------------------
 *  Copyright (C) 2008 STMicroelectronics
 *  Author:  Virlinzi Francesco <francesco.virlinzi@st.com>
 *  -------------------------------------------------------------------------
 *  May be copied or modified under the terms of the GNU General Public V2
 *  License.  See linux/COPYING for more information.
 *  -------------------------------------------------------------------------
 */

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>

#include <asm/timex.h>
#include <asm/clock.h>
#include <asm/param.h>

void __devinit calibrate_delay(void)
{
	struct clk *cclk = clk_get(NULL, "sh4_clk");
	if (IS_ERR(cclk)) {
		panic("Cannot get CPU clock!");
	}

	loops_per_jiffy = (clk_get_rate(cclk) / 2) / HZ;
/*
 * The number '2' in the previous formula comes from
 * the number of instructions in the __delay loop
 * (see lib/delay.c)
 */
	printk(KERN_INFO "SH4 "
			"%lu.%02lu BogoMIPS PRESET (lpj=%lu)\n",
			loops_per_jiffy/(500000/HZ),
			(loops_per_jiffy/(5000/HZ)) % 100,
			loops_per_jiffy);
}
