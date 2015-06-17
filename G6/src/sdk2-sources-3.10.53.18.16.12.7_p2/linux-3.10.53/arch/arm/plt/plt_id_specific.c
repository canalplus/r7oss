/**
 *****************************************************************************
 * @file plt_id_specific.c
 * @author Kevin PETIT
 *
 * @brief Platform ID implmentation
 *
 * Copyright (C) 2011-2012 Technicolor
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA.
 *
 * Contact Information:
 * technicolor.com
 * 1, rue Jeanne d´Arc
 * 92443 Issy-les-Moulineaux Cedex
 * Paris
 * France
 *
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include "plt_id_priv.h"



plt_platform_id_e plt_id_default_platform = PLT_DTI744_CNL_LAB1;

/* static void plt_id_retrieve_configure_gpios(void) */
/* { */
/* } */

static void plt_id_retrieve_from_gpios(plt_platform_id_e * plt_id, unsigned int *plt_board_id, unsigned int *plt_hw_id)
{
	uint8_t board_id = 0, hw_id = 0;
    
	*plt_id = PLT_PLATFORM_ID_UNDEFINED;
    
	/* Configure Pin Mux and GPIOs */
    /* 	plt_id_retrieve_configure_gpios(); */

	/* 
	 * Read the board ID
	 */
	/* Get GPIO 12.0 */
    if (gpio_get_value_cansleep(64)){
        board_id |= 0x08;
	}
	/* Get GPIO 12.1 */
    if (gpio_get_value_cansleep(65)){
        board_id |= 0x04;
	}
	/* Get GPIO 12.2 */
    if (gpio_get_value_cansleep(66)){
        board_id |= 0x02;
	}
	/* Get GPIO 12.3 */
    if (gpio_get_value_cansleep(67)){
		board_id |= 0x01;
	}
    
	/* Save the read board id */
	*plt_board_id = board_id;

#ifdef HW_ID
	/**
	 * Read the HW ID if we are on a LAB2 or newer platform
	 */
	if (board_id > 0){
        /* Get HW ID */
        /* Example */
        /*
        if (gpio_get_value_cansleep(64) & 0x1){
            hw_id |= 0x08;
        }
        */
	}
#endif
    
    /* Save the read HW id */
    *plt_hw_id = hw_id;
    
	/* Look into the table to find the corresponding platform */
	__plt_id_retrieve_from_gpios_general_case(plt_id, PLT_DTI744_CNL_LAB1, board_id, hw_id);
}

/**
 * Read the core value (speed chip value)
 */
static unsigned int plt_id_read_core(void)
{
    unsigned int core=0;
    void *regmap;

    /* map speed chip register */ 
    regmap = ioremap(0x08A6583C, 4);
    if (regmap) {
        core = ioread32(regmap);
        core = (core >> 10) & 0xF;
        printk(KERN_INFO "PLT: Speed chip %d\n", core);
        iounmap(regmap);
    } 
	return core;
}

plt_id_retrieve_from_gpios_cb _plt_id_retrieve_from_gpios_cb = plt_id_retrieve_from_gpios;
plt_id_not_consistent_cb _plt_id_not_consistent_cb = NULL;
plt_id_gpio_post_hook_cb _plt_id_gpio_post_hook_cb = NULL;
plt_id_read_core_cb _plt_id_read_core_cb = plt_id_read_core;
