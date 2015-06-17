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
************************************************************************/

#include "osdev_mem.h"
//#include "osdev_time.h"

#include "hades_api.h"
#include "crcdriver.h"
#include "memtest.h"

HadesError_t HADES_Malloc(HostAddress* ha, int size)
{
	ha->size = size;
	ha->virtual = OSDEV_Malloc(size);
	ha->physical = ha->virtual;
	return HADES_NO_ERROR;
}

void HADES_Free(HostAddress* ha)
{
	OSDEV_Free(ha->virtual);
}

// Converts a host memory address to an Hades L3 address
uint32_t HADES_HostToFabricAddress(HostAddress* ha)
{
	return (uint32_t)(ha->physical);
}

uint32_t HADES_PhysicalToFabricAddress(void* physical)
{
	return (uint32_t)physical;
}

void HADES_Flush(HostAddress* ha)
{
}

void HADES_Invalidate(HostAddress* ha)
{
}

/*******************************************************************
  HADES API implementation
********************************************************************/
HadesError_t HADES_Init(HadesInitParams_t* params)
{
    pr_info("HADES_STUB: Init: regs %p/%d, IRQ %d, partition %s\n", (void*)(params->HadesBaseAddress), params->HadesSize, params->InterruptNumber, params->partition);
    return HADES_NO_ERROR;
}


HadesError_t HADES_Boot(uintptr_t FW_image_addr)
{
    pr_info("HADES_STUB: Boot\n");
    return HADES_NO_ERROR;
}

HadesError_t HADES_Term()
{
    pr_info("HADES_STUB: Term\n");
    return HADES_NO_ERROR;
}

HadesError_t HADES_InitTransformer(HadesTransformerParams_t *Params_p, HadesHandle_t * Handle_p)
{
    *Handle_p = OSDEV_Malloc(1);
    pr_info("HADES_STUB: InitTransformer => %p\n", *Handle_p);
    return HADES_NO_ERROR;
}


HadesError_t HADES_TermTransformer(
    HadesHandle_t Handle)
{
    pr_info("HADES_STUB: TermTransformer(%p)\n", Handle);
    OSDEV_Free(Handle);
    return HADES_NO_ERROR;
}

HadesError_t HADES_ProcessCommand( HadesHandle_t Handle, uintptr_t FrameParams_fabric_p )
{
    HadesAttr_t* Attr = (HadesAttr_t*)FrameParams_fabric_p;

    // pr_info("HADES_STUB: ProcessCommand(%p)\n", Handle);
    // OSDEV_SleepMilliSeconds(30);

#ifdef HEVC_HADES_CUT1
    ((hevcdecpix_status_t*)(Attr->hades_status_struct_p))->error_code = HEVC_DECODER_NO_ERROR;
#else
    ((hades_status_t*)(Attr->hades_status_struct_p))->status.hevc_mme_status.error_code = HEVC_DECODER_NO_ERROR;
#endif

    return HADES_NO_ERROR;
}

/*******************************************************************
  CRC stub
********************************************************************/

int CRCDriver_Init(const char* device_name)
{
	pr_info("HADES_STUB: CRCDriver_Init\n");
	return 0;
}

int CRCDriver_GetChecker(int minor, CheckerHandle_t* handle)
{
	*handle = NULL;
	return 0;
}

int CRCDriver_ReleaseChecker(int minor)
{
	return 0;
}

CheckerStatus_t CHK_Release(CheckerHandle_t handle)
{
	return CHECKER_NO_ERROR;
}

CheckerStatus_t CHK_CheckPPCRC(CheckerHandle_t handle, FrameCRC_t* computed_crc)
{
	return CHECKER_NO_ERROR;
}

CheckerStatus_t CHK_CheckDecoderCRC(CheckerHandle_t handle, FrameCRC_t* decoder_computed_crc)
{
	return CHECKER_NO_ERROR;
}

CheckerStatus_t CHK_ShutdownComputed(CheckerHandle_t handle)
{
	return CHECKER_NO_ERROR;
}

unsigned int decode_time = 0;


/*******************************************************************
  DUMPER stub
********************************************************************/
void O4_dump(const char* dirname, unsigned int decode_index, void* luma, void* chroma, unsigned int luma_size, unsigned int width, unsigned int height) {}
void R2B_dump(const char* dirname, int decimated, unsigned int decode_index, void* luma, void* chroma, unsigned int width, unsigned int height, unsigned int stride) {}

/*******************************************************************
  MEMORY TEST stub
********************************************************************/
void MT_MonitorBuffers(uint32_t source_id, uint32_t source_mask, int buffers_number, MT_AddressRange_t allowed_ranges[]) {}
void MT_StartMonitoring(int skip) {}
void MT_StopMonitoring(void) {}
MT_Buffer_t MT_Buffers[] = {};
int MT_BuffersSize = 0;

