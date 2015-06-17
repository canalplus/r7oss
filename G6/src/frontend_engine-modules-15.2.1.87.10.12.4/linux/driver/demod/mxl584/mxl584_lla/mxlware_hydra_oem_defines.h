
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


#ifndef __MXL_HYDRA_OEM_DEFINES_H__
#define __MXL_HYDRA_OEM_DEFINES_H__

/*****************************************************************************************
    Include Header Files
*****************************************************************************************/
#ifdef __MXL_OEM_DRIVER__
#include "mxl_debug.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************************
    Macros
*****************************************************************************************/

#define MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH   248 /* maximum number bytes allowed in one I2C block write */

#define MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN        256

// Flag to set Big Endian host platform
// 0 -> Little Endian host platform
// 1 -> Big Endian host platform
#define MXL_ENABLE_BIG_ENDIAN        (0)

// NOTE: VC 2003 does not support Variadic Macros

#ifdef __MXL_OEM_DRIVER__
#define MXL_HYDRA_DEBUG               MxL_DLL_DEBUG0
#define MXL_HYDRA_ERROR               MxL_DLL_DEBUG0
#define MXL_HYDRA_PRINT               MxL_DLL_DEBUG0
#else
#ifdef __KERNEL__
#define MXL_HYDRA_DEBUG               printk
#define MXL_HYDRA_ERROR               printk
#define MXL_HYDRA_PRINT               printk
#else
#define MXL_HYDRA_DEBUG               printf
#define MXL_HYDRA_ERROR               printf
#define MXL_HYDRA_PRINT               printf
#endif
#endif

#define MXL_MODULE_DEBUG_LEVEL 1
//#define MXL_MODULE_DEBUG_OPTIONS MXLDBG_ERROR+MXLDBG_API
#define MXL_MODULE_DEBUG_OPTIONS MXLDBG_ERROR+MXLDBG_ENTER+MXLDBG_EXIT+MXLDBG_API
//#define MXL_MODULE_DEBUG_OPTIONS MXLDBG_ERROR
#define MXL_MODULE_DEBUG_FCT MXL_HYDRA_DEBUG

typedef struct
{
  UINT8 drvIndex;

  UINT16 i2cAddress;

} oem_data_t;

#define not_MXL_HYDRA_WAKE_ON_WAN_ENABLED_

/*****************************************************************************************
    User-Defined Types (Typedefs)
*****************************************************************************************/

/*****************************************************************************************
    Global Variable Declarations
*****************************************************************************************/

/*****************************************************************************************
    Prototypes
*****************************************************************************************/

#ifdef __cplusplus
}
#endif

#endif // __MXL_HYDRA_OEM_DEFINES_H__
