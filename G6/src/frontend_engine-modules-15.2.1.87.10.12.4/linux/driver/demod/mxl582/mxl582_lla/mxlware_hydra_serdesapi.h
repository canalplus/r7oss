
/*
* Copyright (c) 2011-2013 MaxLinear, Inc. All rights reserved
*
* License type: GPLv2
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*
* This program may alternatively be licensed under a proprietary license from
* MaxLinear, Inc.
*
* See terms and conditions defined in file 'LICENSE.txt', which is part of this
* source code package.
*/

#ifndef __MXL_HDR_SERDES_API__
#define __MXL_HDR_SERDES_API__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  MXL_HYDRA_SERDES_OP_MODE_0 = 0,   // SerDes OP mode 0
  MXL_HYDRA_SERDES_OP_MODE_1 = 1,   // SerDes OP mode 1
} MXL_HYDRA_SERDES_OP_MODE_E;

MXL_STATUS_E MxLWare_HYDRA_API_CfgSerdesOpMode(UINT8 devId, MXL_HYDRA_SERDES_OP_MODE_E serdesOpMode);

MXL_STATUS_E MxLWare_HYDRA_API_CfgSerdesInterface(UINT8 devId, UINT8 serdesFillerPayloadSize, MXL_BOOL_E serdesStatsReportCtrl);

#ifdef __cplusplus
}
#endif

#endif
