/**
 *****************************************************************************
 * @file plt_id_priv.h
 * @author Kevin PETIT <kevin.petit@technicolor.com>
 *
 * @brief Private header for the plt_id implementers
 *
 * Copyright (C) 2012 Technicolor
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
#ifndef _PLT_ID_PRIV_H_
#define _PLT_ID_PRIV_H_

#include <plt/plt_id.h>

/* Function that can be used by implementers iterate the platforms list */
extern void __plt_id_retrieve_from_gpios_general_case(
	plt_platform_id_e *plt_id,
	plt_platform_id_e starting_id,
	uint32_t board_id,
	uint32_t hw_id
);

/* Define types for callbacks */
typedef void (*plt_id_retrieve_from_gpios_cb)(plt_platform_id_e *, unsigned int *board_id, unsigned int *hw_id);
typedef int (*plt_id_not_consistent_cb)(plt_platform_id_e *id_from_gpio, plt_platform_id_e *id_from_kcl);
typedef void (*plt_id_gpio_post_hook_cb)(plt_platform_id_e id_from_gpio);
typedef unsigned int (*plt_id_read_core_cb)(void);

/* The following variables must be exported by an implementer */
extern plt_platform_id_e plt_id_default_platform;
extern plt_id_retrieve_from_gpios_cb _plt_id_retrieve_from_gpios_cb;
extern plt_id_gpio_post_hook_cb _plt_id_gpio_post_hook_cb;
extern plt_id_not_consistent_cb _plt_id_not_consistent_cb;
extern plt_id_read_core_cb _plt_id_read_core_cb;

#endif /* #ifndef _PLT_ID_PRIV_H_ */

