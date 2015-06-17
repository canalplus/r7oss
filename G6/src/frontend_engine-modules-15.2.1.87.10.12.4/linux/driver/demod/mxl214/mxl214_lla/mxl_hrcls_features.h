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

#ifndef __MXL_HRCLS_FEATURES_H__
#define __MXL_HRCLS_FEATURES_H__

/*****************************************************************************************
    Include Header Files
*****************************************************************************************/
#ifndef MXL_HRCLS_DEVICE_TYPE_EXTERNAL
#include "mxl_hrcls_productid.h"
#endif // MXL_HRCLS_DEVICE_TYPE_EXTERNAL

/*****************************************************************************************
    Macros
*****************************************************************************************/

// --------------------------------------------------------------------------------------
// Utility macro to extract product specific compiler switch setting based on MXL_HRCLS_PROD_ID
// --------------------------------------------------------------------------------------

#if defined(MXL_HRCLS_265_ENABLE) || defined(MXL_HRCLS_267_ENABLE) || defined(MXL_HRCLS_269_ENABLE)
#define _MXL_HRCLS_SERDES_ENABLED_
#endif

#if defined(MXL_HRCLS_258_ENABLE) || defined(MXL_HRCLS_269_ENABLE) || defined(MXL_HRCLS_254_ENABLE) || defined(MXL_HRCLS_256_ENABLE) || defined(MXL_HRCLS_255_ENABLE) || defined(MXL_HRCLS_252_ENABLE)
#define _MXL_HRCLS_OOB_ENABLED_
#endif

#if defined(MXL_HRCLS_258_ENABLE) || \
	defined(MXL_HRCLS_269_ENABLE) || \
	defined(MXL_HRCLS_254_ENABLE) || \
	defined(MXL_HRCLS_255_ENABLE) || \
	defined(MXL_HRCLS_256_ENABLE) || \
	defined(MXL_HRCLS_252_ENABLE) || \
	defined(MXL_HRCLS_212_ENABLE) || \
	defined(MXL_HRCLS_213_ENABLE) || \
	defined(MXL_HRCLS_214_ENABLE) || \
	defined(_MXL_HRCLS_WAKE_ON_WAN_ENABLED_)
#ifndef _MXL_HRCLS_WAKE_ON_WAN_ENABLED_
	#define _MXL_HRCLS_WAKE_ON_WAN_ENABLED_
#endif
#define _MXL_HRCLS_DEMOD_ENABLED_
#endif

#if defined(MXL_HRCLS_258_ENABLE) || \
	defined(MXL_HRCLS_254_ENABLE) || \
	defined(MXL_HRCLS_255_ENABLE) || \
	defined(MXL_HRCLS_256_ENABLE) || \
	defined(MXL_HRCLS_252_ENABLE) || \
	defined(MXL_HRCLS_212_ENABLE) || \
	defined(MXL_HRCLS_213_ENABLE) || \
	defined(MXL_HRCLS_214_ENABLE)
#define _MXL_HRCLS_XPT_ENABLED_
#endif

#if defined(MXL_HRCLS_265_ENABLE) || \
	defined(MXL_HRCLS_267_ENABLE) || \
	defined(MXL_HRCLS_269_ENABLE) || \
	defined(MXL_HRCLS_212_ENABLE) || \
	defined(MXL_HRCLS_213_ENABLE) || \
	defined(MXL_HRCLS_214_ENABLE) || \
	defined(MXL_HRCLS_254_ENABLE)
#define _MXL_HRCLS_IFOUT_ENABLED_
#endif

/*****************************************************************************************
	Extra options not SKU-dependent
 *****************************************************************************************/

#ifndef MXL_HRCLS_OPTIONS_EXTERNAL
	#define _MXL_HRCLS_DEBUG_ENABLED_
	#define not_MXL_HRCLS_FLOAT_CALC_ENABLED_
	#define not_MXL_HRCLS_XPT_KNOWN_PIDS_ENABLED_
#endif

/*****************************************************************************************
	User-Defined Types (Typedefs)
*****************************************************************************************/

/*****************************************************************************************
	Global Variable Declarations
*****************************************************************************************/

/*****************************************************************************************
	Prototypes
*****************************************************************************************/

#endif // __MXL_HRCLS_FEATURES_H__
