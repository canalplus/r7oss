/*
 * arch/sh/boards/st/common/mb562.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics BD-DVD peripherals board support.
 */

#include <linux/init.h>
#include <linux/stm/sysconf.h>
#include <linux/bug.h>
#include <asm/processor.h>

static int __init device_init(void)
{
	struct sysconf_field *sc;

	/* So far valid only for 7200 processor board! */
	BUG_ON(cpu_data->type != CPU_STX7200);

	/* Set up "scenario 1" of audio outputs */

	/* CONF_PAD_AUD[0] = 1
	 * AUDDIG* are connected PCMOUT3_* - 10-channels PCM player #3 */
	sc = sysconf_claim(SYS_CFG, 20, 0, 0, "pcm_player.3");
	sysconf_write(sc, 1);

	/* CONF_PAD_ETH[4] = 1
	 * MII1RXD[3], MII1TXCLK, MII1COL, MII1CRS, MII1DINT & MII1PHYCL
	 * connected to 6-channels PCM player #1 */
	sc = sysconf_claim(SYS_CFG, 41, 20, 20, "pcm_player.1");
	sysconf_write(sc, 1);

	/* CONF_PAD_ETH[5] = 0
	 * MII1CRS is output */
	sc = sysconf_claim(SYS_CFG, 41, 21, 21, "pcm_player.1");
	sysconf_write(sc, 0);

	return 0;
}
arch_initcall(device_init);
