/*
 * Runtime configuration structure
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
 * Authors: Germain Haugou (germain.haugou@st.com)
 *          Bruno Lavigueur (bruno.lavigueur@st.com)
 *          Vincent Gagne (vincent.gagne@st.com)
 *
 */

#ifndef __P12_RT_CONF_H__
#define __P12_RT_CONF_H__

#define FC_STATICMSG_MAX_MSG_SIZE 72
#define FC_STATICMSG_MAX_NUM_MSG 6
#define FC_STATICMSG_BUFFER_SIZE (FC_STATICMSG_MAX_MSG_SIZE * FC_STATICMSG_MAX_NUM_MSG)

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t hasHost;
  uint32_t L3base;
  uint32_t L3size;
  uint32_t L2size;
  uint32_t TCDMsize;
   uint8_t rtVerbose[4];
   uint8_t userVerbose[4];
  uint32_t platform;
  uint32_t clusterMask;
  uint32_t cvpInit;
#ifdef __STHORM_DEBUG__
  uint32_t testTask;
  uint32_t testBinaryName;
  uint32_t testBinaryAddr;
  uint32_t testBinarySize;
#endif
  uint8_t msgBuffer[FC_STATICMSG_BUFFER_SIZE];
} runtimeConf_t;

#define P2012_PLATFORM_COSIM 0
#define P2012_PLATFORM_ISS 1
#define P2012_PLATFORM_POSIX 2

#endif /* __P12_RT_CONF_H__ */
