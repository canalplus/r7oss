/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : hades_api.h

Description: Hades API header

Date        Modification                                    Name
----        ------------                                    --------
30-Jan-13   Copied from Hades model                         Jerome

************************************************************************/
#ifndef H_HADES_API
#define H_HADES_API

#include <linux/types.h>

#include "sthormAlloc.h"
#include "crc.h"

// #define VSOC_MODEL // no boot, no firmware loading, no init transformer, fixed RABs
#define HEVC_VERBOSE pr_info // per-picture traces
// #define VERBOSE

#define BASE_RUNTIME_STR "Base_Runtime"
#define PEDF_RUNTIME_STR "PEDF_Runtime"

#define PATH_LENGTH 256
#define TRANSFORMER_NAME_LENGTH 16
#define MAX_TRANSFORMER 16
#define MAX_CLUSTER 4
#define FC_ID 255
#define TRANSLATION_TABLE_SIZE 2

#define HADES_HEVC_TRANSFORMER_NAME "hevc" // JLX: added because part of the API

#define HADES_FW_STRUCT_VERSION 10
typedef struct {
	uint32_t  StructVersion;     // version of this struct, just in case
	uint32_t  TransformerNb;    // number of pedf_application
} header_t;
typedef struct {
	char      Name[PATH_LENGTH]; // fw full path and name
	uint32_t  FwOffset;          // offset of this fw image in the file
	uint32_t  Size;              // fw size
	uint8_t   FwId;              // corresponds to the cluster number for this fw. = FC_ID (255) for the fabric ctrl
} fw_info_t;
typedef struct {
	char      TransformerName[TRANSFORMER_NAME_LENGTH];
	uint8_t   FwNb;              // nb of cluster used by this transformer
	fw_info_t FwInfo[MAX_CLUSTER];
} tr_info_t;
typedef struct {
    header_t  Header;
    tr_info_t Transformer[MAX_TRANSFORMER];
} fw_image_t;

/**
 * \brief Opaque handle on Hades API
 */
typedef struct HADES_Init_params_s {
    uintptr_t HadesBaseAddress;
    uint32_t  HadesSize;
    uint32_t  InterruptNumber;
    char*     partition;	// BPA2 partition name
} HadesInitParams_t;

typedef void* fw_handle_t;

typedef void* HadesHandle_t;

typedef struct HadesTransformerParams_s
{
    char TransformerName[TRANSFORMER_NAME_LENGTH];
    uintptr_t FWImageAddr;
    int       FWImageSize;
} HadesTransformerParams_t;

typedef struct HadesAttr_s
{
    uintptr_t hades_cmd_struct_p;
    uintptr_t hades_status_struct_p;
} HadesAttr_t;

typedef enum
{
    HADES_NO_ERROR               =0,
    HADES_ERROR                  =1,
    HADES_INVALID_HANDLE         =2,
    HADES_NO_MEMORY              =3
} HadesError_t;

typedef struct HadesTranslationTable_s
{
    uint32_t NbEntries;
    uintptr_t BlockBase[TRANSLATION_TABLE_SIZE];
    uint32_t BlockSize[TRANSLATION_TABLE_SIZE];
    int32_t AddressOffset[TRANSLATION_TABLE_SIZE];    // host to fabric offset (= host_addr - Fabric_addr)
} HadesTranslationTable_t;

typedef struct HadesDebugStatus_s
{
    FrameCRC_t crc;
    uint32_t status_red_0;
    uint32_t status_red_1;
    uint32_t status_pipe_0;
    uint32_t status_pipe_1;
    uint32_t status_ipred;
    uint32_t status_mvpred;
    uint32_t status_hwcfg;
    uint32_t status_xpredmac_cmd_0;
    uint32_t status_xpredmac_cmd_1;
    uint32_t status_xpredop_0;
    uint32_t status_xpredop_1;
    uint32_t status_dbk;
    uint32_t status_resize;
    uint32_t status_general_0_hwpe0;
    uint32_t status_general_1_hwpe0;
    uint32_t status_general_0_hwpe1;
    uint32_t mcc_hit;
    uint32_t mcc_out_ld16;
    uint32_t mcc_out_ld32;
    uint32_t mcc_out_ld64;
    uint32_t mcc_in_ld16;
    uint32_t mcc_in_ld32;
    uint32_t mcc_in_ld64;
} HadesDebugStatus_t;


HadesError_t HADES_Malloc(HostAddress* ha, int size);
void         HADES_Free  (HostAddress* ha);
// Converts a host memory address to an Hades L3 address
uint32_t HADES_HostToFabricAddress(HostAddress* ha);
uint32_t HADES_PhysicalToFabricAddress(void* physical);

// Manages flush & invalidation of the cache
// Allows the host and Hades to share the contents of physical memory
void HADES_Flush(HostAddress* ha);
void HADES_Invalidate(HostAddress* ha);

HadesError_t HADES_Init(HadesInitParams_t* params);
HadesError_t HADES_Boot(fw_info_t *FWInfo, uintptr_t FW_addr_p, int FW_image_size);
HadesError_t HADES_Term(void);
HadesError_t HADES_InitTransformer( HadesTransformerParams_t *InitParams, HadesHandle_t *Handle );
HadesError_t HADES_ProcessCommand( HadesHandle_t Handle, uintptr_t FrameParams );
HadesError_t HADES_TermTransformer( HadesHandle_t Handle );
void         HADES_GetHWPE_Debug(FrameCRC_t* computed_crc);
// capability
// abort

#endif // H_HADES_API
