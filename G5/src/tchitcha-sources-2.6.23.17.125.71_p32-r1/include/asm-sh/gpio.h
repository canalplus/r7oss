/*
 *  include/asm-sh/gpio.h
 *
 *  Copyright (C) 2007 Markus Brunner, Mark Jonas
 *
 *  Addresses for the Pin Function Controller
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_SH_GPIO_H
#define __ASM_SH_GPIO_H

#if defined(CONFIG_CPU_SH3)
#include <asm/cpu/gpio.h>
#elif defined(CONFIG_CPU_SUBTYPE_ST40)
#include <linux/stm/gpio.h>
#endif

/* If using gpiolib, "sub-architectures" must define (only!) gpio_get_value(),
 * gpio_set_value() and gpio_cansleep(). See Documentation/gpio.txt for
 * details. */
#if defined(CONFIG_HAVE_GPIO_LIB)
#include <asm-generic/gpio.h>
#endif

#endif /* __ASM_SH_GPIO_H */
