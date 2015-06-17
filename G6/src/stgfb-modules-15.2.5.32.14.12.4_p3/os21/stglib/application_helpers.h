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
#include <stdint.h>
#include <stdbool.h>

#include <os21.h>
#include <os21/interrupt.h>

#include <stm_display.h>

#if defined(__SH4__)
#include <os21/st40.h>
#include <os21/st40/cache.h>
#elif defined(__ST200__)
#include <os21/st200.h>
#include <os21/st200/cache.h>
#endif

//#if defined(CONFIG_xxxxxxxxxxx)
//#else
#error Undefined chip type, check your makefile options
//#endif

#ifndef TRUE
#define TRUE 1
#endif

extern volatile unsigned char *hotplug_poll_pio;
extern int hotplug_pio_pin;

/* PIO Config register defined for HDMI Hotplug ------------------------------*/
#define PIO_INPUT 0x10
#define PIO_PnC0  0x20
#define PIO_PnC1  0x30
#define PIO_PnC2  0x40

#if defined(__cplusplus)
extern "C" {
#endif

extern void create_test_pattern(char *pBuf,unsigned long width,unsigned long height, unsigned long stride);
extern void setup_soc(void);
extern void setup_analogue_voltages(stm_display_output_h output);

extern stm_display_output_h  get_analog_output(stm_display_device_h dev, int findmain);
extern stm_display_output_h  get_hdmi_output(stm_display_device_h dev);
extern stm_display_output_h  get_dvo_output(stm_display_device_h dev);
extern interrupt_t          *get_hdmi_interrupt();
extern interrupt_t          *get_main_vtg_interrupt();
extern stm_display_source_h  get_and_connect_source_to_plane(stm_display_device_h dev, stm_display_plane_h pPlane);
extern stm_display_plane_h   get_and_connect_gfx_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput);
extern stm_display_plane_h   get_and_connect_video_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput);
extern stm_display_plane_h   get_and_connect_cursor_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput);

struct display_update_s
{
  stm_display_device_h dev;
  stm_display_output_h output;
  semaphore_t         *frameupdate_sem;
  uint32_t             timingID;
};

extern struct display_update_s *start_display_update_thread(stm_display_device_h, stm_display_output_h);

extern int hdmi_isr(void *data);
extern int get_yesno(void);

#if defined(__cplusplus)
}
#endif

#endif /* APP_HELPERS_H */
