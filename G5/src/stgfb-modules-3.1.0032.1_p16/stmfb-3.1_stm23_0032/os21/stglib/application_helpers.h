/***********************************************************************
 *
 * File: os21/stglib/application_helpers.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef APP_HELPERS_H
#define APP_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <os21.h>
#include <os21/interrupt.h>

#include <stmdisplay.h>

#if defined(__SH4__)
#include <os21/st40.h>
#include <os21/st40/cache.h>
#elif defined(__ST200__)
#include <os21/st200.h>
#include <os21/st200/cache.h>
#endif

#if defined(CONFIG_STB7100)

#if defined(CONFIG_STI5202)
#include <os21/st40/sti5202.h>
#else
#include <os21/st40/stb7100.h>
#endif
#include <soc/stb7100/stb7100reg.h>

#elif defined(CONFIG_STI7200)

#include <os21/st40/sti7200.h>
#include <soc/sti7200/sti7200reg.h>

#elif defined(CONFIG_STI7111)

#include <os21/st40/sti7111.h>
#include <soc/sti7111/sti7111reg.h>

#elif defined(CONFIG_STI7141)

#include <os21/st40/sti7141.h>
/* Yes this really is the 7111 include file */
#include <soc/sti7111/sti7111reg.h>

#elif defined(CONFIG_STI7105)

#include <os21/st40/sti7105.h>
#include <soc/sti7105/sti7105reg.h>

#elif defined(CONFIG_STI7106)

#include <os21/st40/sti7106.h>
#include <soc/sti7106/sti7106reg.h>

#elif defined(CONFIG_STI5206)

#include <os21/st40/sti5206.h>
#include <soc/sti5206/sti5206reg.h>

#elif defined(CONFIG_STI7108)

#include <os21/st40/stx7108.h>
#include <soc/sti7108/sti7108reg.h>

#else
#error Undefined chip type, check your makefile options
#endif

extern volatile unsigned char *hotplug_poll_pio;
extern int hotplug_pio_pin;

/* PIO Config register defined for HDMI Hotplug ------------------------------*/
#define PIO_INPUT 0x10
#define PIO_PnC0  0x20
#define PIO_PnC1  0x30
#define PIO_PnC2  0x40


void create_test_pattern(char *pBuf,unsigned long width,unsigned long height, unsigned long stride);
void setup_soc(void);
void setup_analogue_voltages(stm_display_output_t *pOutput);
stm_display_output_t *get_hdmi_output(stm_display_device_t *pDev);
stm_display_output_t *get_dvo_output(stm_display_device_t *pDev);
interrupt_t          *get_hdmi_interrupt();
interrupt_t          *get_bdisp_interrupt();
interrupt_t          *get_main_vtg_interrupt();

int hdmi_isr(void *data);
int get_yesno(void);

#endif
