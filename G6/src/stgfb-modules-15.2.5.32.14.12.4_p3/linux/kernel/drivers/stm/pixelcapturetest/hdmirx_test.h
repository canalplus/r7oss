/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/hdmirx_test.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 * 25-Jun-12   Created                                         Sonia Varada
 * 25-Oct-13   Modified                                        Akram BB
 *
\***********************************************************************/

#ifndef CAPTURE_TEST_HDMIRX_H
#define CAPTURE_TEST_HDMIRX_H

#ifdef HDMIRX_MODE_DETECTION_TEST

/* Standard Includes ----------------------------------------------*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/time.h>

/* Local Includes -------------------------------------------------*/
#include <stm_hdmirx.h>
#include <hdmirx_drv.h>

#ifndef HDMI_TEST_PRINT
#define HDMI_TEST_PRINT(a,b...)  printk("stmhdmirx: %s: " a, __FUNCTION__ ,##b)
#endif

uint32_t hdmirx_mode_detection_test_start(stm_pixel_capture_input_params_t  *InputParams_p);
void hdmirx_mode_detection_test_stop(void);

#endif /* HDMIRX_MODE_DETECTION_TEST */

#endif /* CAPTURE_TEST_HDMIRX_H */
