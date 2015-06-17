/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestdbgfs.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestdbgfs.h
 * Description      : 1. export CRC/MISR to the user space by st_relay module
 *                    2. provide ioctl for user to control CRC/MISR test
 ************************************************************************/
#ifndef __DISPLAYTEST_DBGFS_H
#define __DISPLAYTEST_DBGFS_H

int   displaytest_dbgfs_open(void);
void  displaytest_dbgfs_close(void);
char* displaytest_get_cmd_str(void);
void  displaytest_reset_cmd_str(void);
void  displaytest_set_result_str(char *result);
void  displaytest_reset_result_str(void);
bool  displaytest_get_reset_flag(void);
void  displaytest_clear_reset_flag(void);
#endif

