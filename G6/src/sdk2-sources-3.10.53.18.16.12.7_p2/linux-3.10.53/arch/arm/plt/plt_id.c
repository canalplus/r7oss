/**
 *****************************************************************************
 * @file plt_id.c
 * @author Kevin PETIT <kevin.petit@technicolor.com> / Franck MARILLET
 *
 * @brief Core plt_id code
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>

#include "plt_id_priv.h"

/* platform id retrieved from kernel command line */
plt_platform_id_e  plt_id_from_kcl	= PLT_PLATFORM_ID_UNDEFINED;

plt_platform_id_e  plt_id			= PLT_PLATFORM_ID_UNDEFINED;

unsigned int plt_id_hw				= PLT_ID_HW_ID_UNDEFINED;
unsigned int plt_id_board			= PLT_ID_BOARD_ID_UNDEFINED;
unsigned int plt_id_core            = PLT_ID_CORE_UNDEFINED;

/** Retrieve PLATFORM_ID from kernel command line
 *
 * @param[in] string    NULL terminated string containing the value
 * @return    1
 * @remarks This function is called as many times as the parameter is found in the kernel
 *          command line. Only first valid occurence is used.
 */
static int plt_id_retrieve_from_kernel_command_line(char *string)
{
	int plt_id;

    if ((plt_id_from_kcl == PLT_PLATFORM_ID_UNDEFINED) && (string != NULL)) {
		/* Search for the PLATFORM_ID in the PLATFORM_ID table */
		for (plt_id = 0; plt_id < PLT_MAX_PLATFORM_ID; plt_id++) {
			if (!strcmp(string, plt_id_names[plt_id])) {
				plt_id_from_kcl = plt_id;
				printk(KERN_INFO "kcl: %s\n", plt_id_names[plt_id]);
				break;
			}
		}

		if (plt_id == PLT_MAX_PLATFORM_ID) {
			/* PLATFORM_ID Not found in the table */
			printk(KERN_ERR "\nWarning: PLATFORM_ID in kernel command line not supported: %s\n", string);
		}
	}
	return 1; /* For this param, do not call other callback function than this one */
}

/* Register a callback to be notified of the PLATFORM_ID found in kernel command line */
__setup(PLT_PLATFORM_ID_PARAM_NAME, plt_id_retrieve_from_kernel_command_line);

/**
 * Handle the general case of matching board_id/hw_id
 */
void __plt_id_retrieve_from_gpios_general_case(
	plt_platform_id_e *plt_id,
	plt_platform_id_e starting_id,
	uint32_t board_id,
	uint32_t hw_id
){
	plt_platform_id_e plt_index;

	for (plt_index = starting_id; plt_index < PLT_MAX_PLATFORM_ID; plt_index++) {
		if (plt_board_id_field[plt_index] == board_id) {
			*plt_id = plt_index;
			break;
		}
	}

	if (*plt_id == PLT_PLATFORM_ID_UNDEFINED) {
		printk(KERN_ERR "%s: Unknown board_id 0x%04x / hw_id 0x%04x : platform detection failed!\n\n",
				__func__, board_id, hw_id);
	}
}

int plt_id_init(void)
{
    /* We start with an undefined value */
    plt_platform_id_e plt_id_from_gpio = PLT_PLATFORM_ID_UNDEFINED;
	int warn_unconsistent = 1;

	/* Read from the GPIOs if we have a callback for that */
    if (_plt_id_retrieve_from_gpios_cb != NULL) {
		_plt_id_retrieve_from_gpios_cb(&plt_id_from_gpio, &plt_id_board, &plt_id_hw);
	}

	if ((plt_id_from_kcl == PLT_PLATFORM_ID_UNDEFINED) && (plt_id_from_gpio == PLT_PLATFORM_ID_UNDEFINED)){
		/* None is valid: Use the default platform */
        plt_id = plt_id_default_platform;
        printk("PLT: Platform not detected (command line or GPIOs), use default: %s\n", plt_id_names[plt_id]);
    }
    else if (plt_id_from_kcl != PLT_PLATFORM_ID_UNDEFINED){
        /* A valid platform id was retrieved from kernel command line: use it */
        plt_id = plt_id_from_kcl;
        printk("PLT: Platform (from kernel command line): %s\n", plt_id_names[plt_id]);

        /* Check consistency with the value read from GPIOs */
        if (plt_id_from_gpio != plt_id_from_kcl){
			
			if (_plt_id_not_consistent_cb != NULL) {
				warn_unconsistent = _plt_id_not_consistent_cb(&plt_id_from_gpio, &plt_id_from_kcl);
			}

			/* Print a warning */
			if (warn_unconsistent) {
	        	printk("PLT: WARNING, value read on GPIOs (but not used) is different: %s\n",
						plt_id_names[plt_id_from_gpio]);
			}
        }
        /* else: values are consistent: nothing to do */
    }
    else{
        /*
		 * No (or no valid) platform id in kernel command line.
         * => use the one detected from GPIOs.
         */
        plt_id = plt_id_from_gpio;
        printk("PLT: Platform (GPIOs detection): %s\n", plt_id_names[plt_id]);
		if (_plt_id_gpio_post_hook_cb != NULL) {
			_plt_id_gpio_post_hook_cb(plt_id_from_gpio);
		}
    }

	/* Read the core value if we have a callback for that */
    if (_plt_id_read_core_cb != NULL) {
		plt_id_core = _plt_id_read_core_cb();
	}
    
	return 0;
}

subsys_initcall(plt_id_init);

/*******************************************************************************
 *                           EXPORTED FUNCTIONS                                *
 ******************************************************************************/

/**
 * Get the platform id
 */
plt_platform_id_e plt_id_get(void)
{
	return plt_id;
}
EXPORT_SYMBOL(plt_id_get);

/**
 * Get the platform id name
 */
const char *plt_id_get_name(void)
{
	return plt_id_names[plt_id];
}
EXPORT_SYMBOL(plt_id_get_name);

/**
 * Get the HW ID
 */
int plt_id_get_hw_id(void)
{
	return plt_id_hw;
}
EXPORT_SYMBOL(plt_id_get_hw_id);

/**
 * Get the board ID
 */
int plt_id_get_board_id(void)
{
	return plt_id_board;
}
EXPORT_SYMBOL(plt_id_get_board_id);

/**
 * Get the core value
 */
unsigned int plt_id_get_core(void)
{
	return plt_id_core;
}
EXPORT_SYMBOL(plt_id_get_core);

