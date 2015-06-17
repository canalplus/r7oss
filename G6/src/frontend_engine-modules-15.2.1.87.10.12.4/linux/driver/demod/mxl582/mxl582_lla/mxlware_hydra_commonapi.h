
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


#ifndef __MXL_HDR_COMMONAPI_H__
#define __MXL_HDR_COMMONAPI_H__

#ifndef __KERNEL__
#ifndef __midl
#include <stdio.h>
#endif
#else
#include <linux/kernel.h>
#include <linux/module.h>
#endif

#include "maxlineardatatypes.h"
#include "mxlware_hydra_skufeatures.h"    // sku-dependent feature configurations (depending on mxl_hdr_prod_id, which is defined in mxl_hdr_productid.h)
#include "mxlware_hydra_oem_defines.h"    // platform-dependent configurations

#include "maxlineardebug.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MXL_DLL_BUILD_
#undef MXL_HYDRA_API
#define MXL_HYDRA_API extern "C" __declspec(dllexport)
#endif // _MXL_DLL_BUILD_

#ifndef MXL_HYDRA_API
#define MXL_HYDRA_API
#endif

#include "mxlware_hydra_deviceapi.h"

#ifdef _MXL_HYDRA_SERDES_ENABLED_
  #include "mxlware_hydra_serdesapi.h"
#endif
#if defined(_MXL_HYDRA_DISEQC_ENABLED_) || defined(_MXL_HYDRA_FSK_ENABLED_)
  #include "mxlware_hydra_diseqcfskapi.h"
#endif
#include "mxlware_hydra_demodtunerapi.h"
#include "mxlware_hydra_tsmuxctrlapi.h"


MXL_HYDRA_API MXL_STATUS_E MxLWare_HYDRA_API_CfgDrvInit(UINT8 devId, void * oemDataPtr, MXL_HYDRA_DEVICE_E deviceType);
MXL_HYDRA_API MXL_STATUS_E MxLWare_HYDRA_API_CfgDrvUnInit(UINT8 devId);

#ifdef __cplusplus
}
#endif

#endif
