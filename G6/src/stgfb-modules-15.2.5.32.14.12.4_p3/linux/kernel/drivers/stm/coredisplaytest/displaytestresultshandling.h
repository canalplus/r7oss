/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestresultshandling.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestresultshandling.h
 *
 ************************************************************************/
#ifndef __DISPLAYTESTRESULTSHANDLING_H
#define __DISPLAYTESTRESULTSHANDLING_H

void displaytest_relayfs_write(int relayType, int32_t length, char *string, uint32_t index);

void print_ovp_test_results(struct ovpDisplayTest *data);
void print_ivp_test_results(struct ivpDisplayTest *data);

void export_capability(struct stmcore_displaytest *pDisplayTest);

#endif


