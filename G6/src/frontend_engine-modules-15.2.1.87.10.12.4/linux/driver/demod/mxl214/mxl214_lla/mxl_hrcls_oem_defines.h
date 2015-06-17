/*
* Copyright (c) 2011-2014 MaxLinear, Inc. All rights reserved
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

#ifndef __MXL_HRCLS_OEM_DEFINES_H__
#define __MXL_HRCLS_OEM_DEFINES_H__

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#else
#include <stdio.h>
#include <string.h>
#endif

#define MXL_HRCLS_OEM_MAX_BLOCK_WRITE_LENGTH   256     /** maximum number bytes allowed in one I2C block write. Not greater than 256 */
#define MXL_HRCLS_OEM_MAX_BLOCK_READ_LENGTH    800    /** maximum number bytes allowed in one I2C block read. Has to be even number */

#define MXL_MODULE_DEBUG_LEVEL 3
#define MXL_MODULE_DEBUG_OPTIONS MXLDBG_ENTER+MXLDBG_EXIT+MXLDBG_ERROR+MXLDBG_API
#define MXL_MODULE_DEBUG_FCT MxL_HRCLS_PRINT

#define MxL_HRCLS_DEBUG  printk /** To be replaced by customer's own log function */
#define MxL_HRCLS_ERROR  printk /** To be replaced by customer's own log function */
#define MxL_HRCLS_PRINT  printk /** To be replaced by customer's own log function */

#define not_MXL_HRCLS_WAKE_ON_WAN_ENABLED_
#define not_MXL_HRCLS_LITTLE_ENDIAN_

#endif // __MXL_HRCLS_OEM_DEFINES_H__
