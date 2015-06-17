/*
 * Host driver common emulation layer API
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *
 */

#ifndef _HOST_DRIVER_EMU_COMMON_h_
#define _HOST_DRIVER_EMU_COMMON_h_

#include <linux/types.h>

int getIntFromStr(const char *str, int fallback);
int getIntFromEnv(const char *name, int fallback);

void fabricCtrl_boot(uint32_t base);

void init_runtimeConf(const uint32_t addr, const int numCluster,
                      const uint32_t l3MemStart, const uint32_t l3MemSize);

#endif /* _HOST_DRIVER_EMU_COMMON_h_ */
