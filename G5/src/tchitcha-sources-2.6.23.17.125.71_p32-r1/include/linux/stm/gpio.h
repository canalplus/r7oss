/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_GPIO_H
#define __LINUX_STM_GPIO_H

/* Lines 0-199 are reserved for internal SoC PIOs.
 * Extenders may use numbers 200 to (ARCH_NR_GPIOS - 1). */

/* Using standard libgpio methods */

#define gpio_get_value __gpio_get_value
#define gpio_set_value __gpio_set_value
#define gpio_cansleep  __gpio_cansleep

int gpio_to_irq(unsigned gpio);
int irq_to_gpio(unsigned irq);

#endif
