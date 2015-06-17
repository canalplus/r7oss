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

Description: Hades API implementation for Linux kernel

Date        Modification                                    Name
----        ------------                                    --------
30-Jan-13   Copied from Hades model                         Jerome

************************************************************************/

#include <linux/bpa2.h>

#include "osdev_mem.h"
#include "hades_api.h"
#include "driver.h"
#include "driverEmu.h"
#include "elfLoader.h"
#include "sthormAlloc.h"
#include "hades_rab_programming.h"
#include "hevc_hwpe_debug.h"
#include "crc.h" // defines a container for the HWPE debug registers

static pedf_binary_t* PedfRT_Descr_p;
static HostAddress    FabricFirmware;
static int            RabMasterEntries;
static uintptr_t      HadesBaseAddress;

typedef struct HadesContext_s
{
    pedf_instance_t instance;                                      // PEDF ABI struct sent to initialize a new instance. In context to keep info on the current instance.
    tr_info_t* TrInfo_p;                                           // points to current transformer info in Fw image
    pedf_binary_t* PedfRT_Descr_p;                                      // !< binary descriptor of the pedf runtime
    pedf_binary_t* PedfFCApp_Descr_p;                                   // !< binary descriptor of the Fabric Controller pedf application
    pedf_binary_t* PedfApp_Descr_p[P2012_MAX_FABRIC_CLUSTER_COUNT];     // !< binary descriptor of the pedf appcation. one per cluster
    HostAddress fw_addr;                                          // pointer to store the firmware address allocated during InitTransformer
} HadesContext_t;

//
// Use sthorm to allocate memory in Hades L3
//

HadesError_t HADES_Malloc(HostAddress* ha, int size)
{
  if (! sthorm_malloc(ha, size))
    return HADES_NO_MEMORY;
  return HADES_NO_ERROR;
}

void HADES_Free(HostAddress* ha)
{
  sthorm_free(ha);
}

// Converts a host memory address to an Hades L3 address
uint32_t HADES_HostToFabricAddress(HostAddress* ha)
{
    return sthorm_remapHostToFabric((uintptr_t) ha->physical);
}

// Converts a physical address to an Hades L3 address
uint32_t HADES_PhysicalToFabricAddress(void* physical)
{
    return sthorm_remapHostToFabric((uintptr_t) physical);
}

void HADES_Flush(HostAddress* ha)
{
  sthorm_flush(ha);
}

void HADES_Invalidate(HostAddress* ha)
{
  sthorm_invalidate(ha);
}

//

// Send a message to the fabric and block until the response is received
static int RpcFabric(
    pedf_commands_e cmd,
    HostAddress* msgAddr,
    p12_msgHeader_t *MsgHeader_p)
{
    MsgHeader_p->fabricHandler = PedfRT_Descr_p->rtInfo_p->handler;
    MsgHeader_p->requestID = cmd;
    MsgHeader_p->replyID = (uint32_t)MsgHeader_p;

    return sthorm_sendSyncMsg(HADES_HostToFabricAddress(msgAddr), MsgHeader_p);
}

/* parse the FWImage structure to get info on the 'TrNname' transformer */
int HADES_GetTransformerInfo(
    char* TrNname,
    uintptr_t FWImageAddr,
    tr_info_t** TrInfo_pp)
{
    fw_image_t* FwImage_p = (fw_image_t*)FWImageAddr;
    tr_info_t* Info_p;
    uint32_t i;

    if (FwImage_p->Header.StructVersion != HADES_FW_STRUCT_VERSION) {
        return -1;
    }

    for (i=0; i<FwImage_p->Header.TransformerNb; i++) {
        Info_p = &(FwImage_p->Transformer[i]);
        if (strncmp(Info_p->TransformerName, TrNname, TRANSFORMER_NAME_LENGTH) == 0) {
            *TrInfo_pp = Info_p;
            return 0;
        }
    }

    // transformer not found
    *TrInfo_pp = NULL;
    return -1;

}

/* parse the tr_info_t structure to get info on the 'FWname' binary */
int HADES_GetFirmwareInfo(
    char* TrNname,
    uint8_t FwId,
    uintptr_t FWImageAddr,
    fw_info_t** FwInfo_pp)
{
    tr_info_t* TrInfo_p;
    fw_info_t* Info_p;
    uint32_t i;

    if (HADES_GetTransformerInfo(TrNname, FWImageAddr, &TrInfo_p) != 0) {
        return -1;
    }

    for (i=0; i<TrInfo_p->FwNb; i++) {
        Info_p = &TrInfo_p->FwInfo[i];
        if (Info_p->FwId == FwId) {
            *FwInfo_pp = Info_p;
            return 0;
        }
    }

    // fw not found
    *FwInfo_pp = NULL;
    return -1;

}


/* HADES_LoadFirmware
 * allocates a pedf_binary_t structure and all associated buffers (binaryHost, NameHost and ErrorStrHost)
 * copy FW binary corresponding to transformer name TrName into BinaryHost
 * copy FW name into nameHost
 *
 * output: an opaque handle fw_handle_p on this pedf_binary_t structure
 * returns 0 on success
 */
int HADES_LoadFirmware(
    char* TrNname, uint8_t FwId,
    uintptr_t FWImageAddr,
    fw_handle_t* fw_handle_p)
{
    size_t namelen;
#ifndef VSOC_MODEL
    fw_info_t* FwInfo_p;
    unsigned char* FW_Addr;
#endif
    pedf_binary_t* fw_binary_p;

    if ((fw_binary_p = OSDEV_Calloc(1, sizeof(pedf_binary_t))) == NULL) {
        return -1;
    }
#ifndef VSOC_MODEL
    if (HADES_GetFirmwareInfo(TrNname, FwId, FWImageAddr, &FwInfo_p) != 0) {
        goto error_binary;
    }

    if (HADES_Malloc(&fw_binary_p->binary, FwInfo_p->Size) != HADES_NO_ERROR)
      goto error_binary;
    // copy this binary into binaryHost
    FW_Addr = (uint8_t*)FWImageAddr + FwInfo_p->FwOffset;
    memcpy(fw_binary_p->binary.virtual, FW_Addr, fw_binary_p->binary.size);
    HADES_Flush(&fw_binary_p->binary);
    namelen = strlen(FwInfo_p->Name)+1;
    if (HADES_Malloc(&fw_binary_p->name, namelen) != HADES_NO_ERROR)
      goto error_name;
    strncpy((char *)(fw_binary_p->name.virtual), FwInfo_p->Name, namelen);
    ((char *)(fw_binary_p->name.virtual))[namelen - 1] = '\0';
    HADES_Flush(&fw_binary_p->name);
    if (HADES_Malloc(&fw_binary_p->errorStr, 256) != HADES_NO_ERROR)
      goto error_str;
#endif
    *fw_handle_p = (fw_handle_t)fw_binary_p;
    return 0;

//error handling --------------------------
#ifndef VSOC_MODEL
error_str:
    HADES_Free(&fw_binary_p->name);
error_name:
    HADES_Free(&fw_binary_p->binary);
error_binary:
    OSDEV_Free(fw_binary_p);
    return -1;
#endif
}


int HADES_UnLoadFirmware(
    fw_handle_t fw_handle)
{
    pedf_binary_t* fw_binary_p = (pedf_binary_t*)fw_handle;
#ifndef VSOC_MODEL
    HADES_Free(&fw_binary_p->binary);
    HADES_Free(&fw_binary_p->name);
    HADES_Free(&fw_binary_p->errorStr);
#endif

    OSDEV_Free(fw_binary_p);

    return 0;
}

#ifndef HEVC_HADES_CUT1
void HADES_Prog_plug(HadesInitParams_t* params)
{
    // DCHAN IP plug setting
    // Write posting
    uint32_t dchan_mif_wr_offset = 0x2800;
    uint32_t dchan_mif_re_offset = 0x2000;
    uint32_t addr_mif_wr = params->HadesBaseAddress + HOST_CL0_DCHAN_OFFSET + dchan_mif_wr_offset;
    uint32_t addr_mif_re = params->HadesBaseAddress + HOST_CL0_DCHAN_OFFSET + dchan_mif_re_offset;
    int i = 0;

    sthorm_write32(addr_mif_wr + 0x04, 0x1);
    // Disable write posting
    //sthorm_write32(addr_mif_wr, sthorm_read32(addr_mif_wr) & ~(0x01 << 16));
    // Enable write posting
    sthorm_write32(addr_mif_wr, sthorm_read32(addr_mif_wr) | (0x1 << 16));
    for (i = 0; i < 16; i++)
    {
        sthorm_write32(addr_mif_wr + 0x20 + 0x10*i, 0x03);
        sthorm_write32(addr_mif_wr + 0x20 + 0x10*i, sthorm_read32(addr_mif_wr + 0x20 + 0x10*i)|(0x03 << 2));
    }

    sthorm_write32(addr_mif_wr + 0x04, 0);
    sthorm_write32(addr_mif_re + 0x04, 0x1);

    for (i = 0; i<16; i++)
    {
        sthorm_write32(addr_mif_re + 0x20 + 0x10*i, 0x03);
        sthorm_write32(addr_mif_re + 0x20 + 0x10*i, sthorm_read32(addr_mif_re + 0x20 + 0x10*i)|(0x03 << 2));
    }

    sthorm_write32(addr_mif_re + 0x04, 0);
}
#endif

/*******************************************************************
  HADES API implementation
********************************************************************/
HadesError_t HADES_Init(HadesInitParams_t* params)
{
    int nbEntries=0;
    int clusterMask;
    unsigned long hadesl3_base;
    unsigned long hadesl3_size;

    // Register IRQ, map registers
    // Note : skip preprocessor registers at beginning of range
    if (sthorm_init(params->HadesBaseAddress + HADES_FABRIC_OFFSET, params->HadesSize - HADES_FABRIC_OFFSET, params->InterruptNumber, params->partition) != 0)
        return HADES_ERROR;
    HadesBaseAddress = params->HadesBaseAddress;

    bpa2_memory(bpa2_find_part("hades-l3"), &hadesl3_base, &hadesl3_size);
    if (hadesl3_base == 0)
    {
        LIB_TRACE(0, "Unable to find hades-l3 partition settings\n");
        return HADES_ERROR;
    }

    // Configure RABs
    // int clusterMask = (1 << P2012_MAX_FABRIC_CLUSTER_COUNT) | ((1 << get_nb_cluster()) - 1);
    clusterMask = (1 << P2012_MAX_FABRIC_CLUSTER_COUNT) | ((1 << 1) - 1);
    hal2_configure_rab_slave_default((uint32_t)HadesBaseAddress, clusterMask, &nbEntries);
    //pr_info("hadesl3_base: 0x%lx hadesl3_size: %ld\n", hadesl3_base, hadesl3_size);
    hal2_configure_rab_master_default((uint32_t)HadesBaseAddress, &RabMasterEntries, HADES_HOST_DDR_ADDRESS, hadesl3_base, hadesl3_size);

#ifndef HEVC_HADES_CUT1
   HADES_Prog_plug(params); // Should be done in TargetPack
#endif

    return HADES_NO_ERROR;
}

HadesError_t HADES_Boot(fw_info_t *FW_Info, uintptr_t FW_image_addr, int FW_image_size)
{
#ifndef VSOC_MODEL
    unsigned char* BaseRT_Addr;
    fw_info_t* FWInfo_p;

    FWInfo_p = FW_Info;
    // in full static link mode, the elf file can be directly loaded
    FWInfo_p->FwOffset = 0;
    FWInfo_p->Size = FW_image_size;
    FWInfo_p->FwId = 0;

    BaseRT_Addr = (unsigned char*)FW_image_addr + FWInfo_p->FwOffset;
    // extract BaseRuntime (elf static binary) in correct address
    host_loadELF(BaseRT_Addr, &FabricFirmware);
    // map hades boot address of fabric firmware to loaded firmware in DDR
    hal2_configure_rab_master_firmware((uint32_t)HadesBaseAddress, &RabMasterEntries,
            STHORM_BOOT_ADDR_L3, (uint32_t)FabricFirmware.physical, FW_image_size);
    /* map STM registers after firmware (512K)
     * !!!!! FW + STM reg <= reserved mem in mem map (currently 2Mb) !!!!! */
    hal2_configure_rab_master_firmware((uint32_t)HadesBaseAddress, &RabMasterEntries,
            0xF0000000, (uint32_t)FabricFirmware.physical + FW_image_size, 0x80000); //STM reg

    // Activate both Master RABs
    hal2_activate_rab_master((uint32_t)HadesBaseAddress);
#else
    pr_info("HADES_Boot: VSOC Model: no firmware loading\n");
#endif

    /* boot base runtime and wait until its initialization's done */
    if (sthorm_boot() != 0)
        return HADES_ERROR;

    return HADES_NO_ERROR;
}

HadesError_t HADES_Term()
{
    sthorm_term();
    HADES_UnLoadFirmware((fw_handle_t)PedfRT_Descr_p);
    sthorm_unmap(&FabricFirmware);
    return HADES_NO_ERROR;
}


// Post a deploy command on the command queue and wait for its completion
HadesError_t HADES_InitTransformer(HadesTransformerParams_t *Params_p, HadesHandle_t * Handle_p)
{
#ifdef VSOC_MODEL
    * Handle_p = NULL;
    return HADES_NO_ERROR;
#else
    uint32_t err;
    HostAddress* ContextAddr_p;
    HadesContext_t * Context_p;
    uintptr_t rtInfo_fabric_p;
    pedf_binary_t* fw_binary_p;

    // keep a handle on all the descriptors in local structure.
    ContextAddr_p = (HostAddress*) OSDEV_Malloc(sizeof(*ContextAddr_p));
    if (ContextAddr_p == NULL) {
        LIB_TRACE(0, "Unable to allocate context\n");
        return -1;
    }
        // actually, only the instance needs to be shared
    if (HADES_Malloc(ContextAddr_p, sizeof(*Context_p)) != HADES_NO_ERROR) {
        LIB_TRACE(0, "Unable to allocate context hades\n");
        goto error_malloc;
    }
    Context_p = (HadesContext_t*) (ContextAddr_p->virtual);
    memset(Context_p, 0, sizeof(*Context_p));
    Context_p->instance.fabricAddr = HADES_HostToFabricAddress(ContextAddr_p); // it's actually the "instance" address that is remapped

    if ((fw_binary_p = OSDEV_Zalloc(sizeof(pedf_binary_t))) == NULL) {
        LIB_TRACE(0, "Unable to alloc pedf_binary\n");
        goto error_malloc;
    }

    PedfRT_Descr_p = (fw_handle_t)fw_binary_p;
    PedfRT_Descr_p->clusterId = STHORM_FC_CLUSTERID;
    PedfRT_Descr_p->binary.size = Params_p->FWImageSize;

    // Then build the runtime info structure to pass as parameter
    LIB_TRACE(0, "Configuring static binary\n");
    if (HADES_Malloc(&Context_p->fw_addr, sizeof(pedf_runtime_info_t)) != HADES_NO_ERROR)
    {
        LIB_TRACE(0, "Out of resources (fw_p pedf_rt_info)\n");
        HADES_Free(&Context_p->fw_addr);
        goto error_malloc;
    }
    PedfRT_Descr_p->rtInfo_p = (pedf_runtime_info_t *) Context_p->fw_addr.virtual;
    LIB_TRACE(0, "%s: PedfRT_Descr_p(%p) ->binary.size: %d clusterId: 0x%x\n", __func__,
            PedfRT_Descr_p, PedfRT_Descr_p->binary.size, (uint32_t)PedfRT_Descr_p->clusterId);

    rtInfo_fabric_p = HADES_PhysicalToFabricAddress(Context_p->fw_addr.physical);
    if (PedfRT_Descr_p->rtInfo_p == NULL || rtInfo_fabric_p == 0) {
        LIB_TRACE(0, "Out of resources\n");
        HADES_Free(&Context_p->fw_addr);
        kfree(fw_binary_p);
        goto error_malloc;
    }

    LIB_TRACE(0, "%s -> pedf_rt (p:0x%x v:0x%x size:%d)\n", __func__,
            (uint32_t)Context_p->fw_addr.physical, (uint32_t)Context_p->fw_addr.virtual, Context_p->fw_addr.size);

    // Finally call the main entry point of the pedf-runtime
    err = sthorm_callMain(PedfRT_Descr_p, (void *)rtInfo_fabric_p);
    if (err != 0) {
        LIB_TRACE(0, "Unable to configure application : %s\n", Params_p->TransformerName);
        HADES_Free(&Context_p->fw_addr);
        kfree(fw_binary_p);
        goto error_malloc;
    }
    HADES_Invalidate(&Context_p->fw_addr);

    // Send message to FC, and block until its response
    HADES_Flush(ContextAddr_p);
    RpcFabric(PEDF_CMD_NEW_INSTANCE, ContextAddr_p, &(Context_p->instance.hdr));
    HADES_Invalidate(ContextAddr_p);

    *Handle_p = ContextAddr_p;
    if (Context_p->instance.retval != 0) {
        goto error_load_all;
    }
    return 0;

error_load_all:
    HADES_UnLoadFirmware((fw_handle_t) Context_p->PedfFCApp_Descr_p);
    HADES_Free(ContextAddr_p);
error_malloc:
    OSDEV_Free(ContextAddr_p);
    return -1;
#endif
}


// Post a undeploy command on the command queue and wait for its completion
HadesError_t HADES_TermTransformer(HadesHandle_t Handle)
{
#ifdef VSOC_MODEL
    return HADES_NO_ERROR;
#else
    HostAddress* ContextAddr_p = (HostAddress*) Handle;
    HadesContext_t * Context_p = (HadesContext_t *)(ContextAddr_p->virtual);
    HadesError_t error = HADES_NO_ERROR;

#ifndef HEVC_HADES_CUT1
    RpcFabric(PEDF_CMD_DESTROY, ContextAddr_p, &(Context_p->instance.hdr));

    HADES_Invalidate(ContextAddr_p);
    error = (Context_p->instance.retval == 0)? HADES_NO_ERROR: HADES_ERROR;
#endif

    HADES_Free(&Context_p->fw_addr);
    HADES_Free(ContextAddr_p);
    OSDEV_Free(ContextAddr_p);

    return error;
#endif
}

void HADES_GetHWPE_Debug_Status(void)
{
    HadesDebugStatus_t debug;
    HADES_GetHWPE_Debug(&debug.crc);
    debug.status_red_0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_RED_0_OFFSET);
    debug.status_red_1 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_RED_1_OFFSET);
    debug.status_pipe_0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_PIPE_0_OFFSET);
    debug.status_pipe_1 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_PIPE_1_OFFSET);
    debug.status_ipred = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_IPRED_OFFSET);
    debug.status_mvpred = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_MVPRED_OFFSET);
    debug.status_hwcfg = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_HWCFG_OFFSET);
    debug.status_xpredmac_cmd_0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_XPREDMAC_CMD_0_OFFSET);
    debug.status_xpredmac_cmd_1 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_XPREDMAC_CMD_1_OFFSET);
    debug.status_xpredop_0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_XPREDOP_0_OFFSET);
    debug.status_xpredop_1 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_XPREDOP_1_OFFSET);
    debug.status_dbk = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_DBK_OFFSET);
    debug.status_resize = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_RESIZE_OFFSET);
    debug.status_general_0_hwpe0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_GENERAL_0_HWPE0_OFFSET);
    debug.status_general_1_hwpe0 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_GENERAL_1_HWPE0_OFFSET);
    debug.status_general_0_hwpe1 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_STATUS_GENERAL_0_HWPE1_OFFSET);
    debug.mcc_hit = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_HIT_OFFSET);
    debug.mcc_out_ld16 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_OUT_LD16_OFFSET);
    debug.mcc_out_ld32 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_OUT_LD32_OFFSET);
    debug.mcc_out_ld64 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_OUT_LD64_OFFSET);
    debug.mcc_in_ld16 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_IN_LD16_OFFSET);
    debug.mcc_in_ld32 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_IN_LD32_OFFSET);
    debug.mcc_in_ld64 = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_MCC_IN_LD64_OFFSET);

    pr_info("%-25s 0x%08x\n", "hwc_ctb_rx", debug.crc.hwc_ctb_rx);
    pr_info("%-25s 0x%08x\n", "hwc_sli_rx", debug.crc.hwc_sli_rx);
    pr_info("%-25s 0x%08x\n", "hwc_mvp_tx", debug.crc.hwc_mvp_tx);
    pr_info("%-25s 0x%08x\n", "hwc_red_tx", debug.crc.hwc_red_tx);
    pr_info("%-25s 0x%08x\n", "mvp_ppb_rx", debug.crc.mvp_ppb_rx);
    pr_info("%-25s 0x%08x\n", "mvp_ppb_tx", debug.crc.mvp_ppb_tx);
    pr_info("%-25s 0x%08x\n", "mvp_cmd_tx", debug.crc.mvp_cmd_tx);
    pr_info("%-25s 0x%08x\n", "mac_cmd_tx", debug.crc.mac_cmd_tx);
    pr_info("%-25s 0x%08x\n", "mac_dat_tx", debug.crc.mac_dat_tx);
    pr_info("%-25s 0x%08x\n", "xop_cmd_tx", debug.crc.xop_cmd_tx);
    pr_info("%-25s 0x%08x\n", "xop_dat_tx", debug.crc.xop_dat_tx);
    pr_info("%-25s 0x%08x\n", "red_dat_rx", debug.crc.red_dat_rx);
    pr_info("%-25s 0x%08x\n", "red_cmd_tx", debug.crc.red_cmd_tx);
    pr_info("%-25s 0x%08x\n", "red_dat_tx", debug.crc.red_dat_tx);
    pr_info("%-25s 0x%08x\n", "pip_cmd_tx", debug.crc.pip_cmd_tx);
    pr_info("%-25s 0x%08x\n", "pip_dat_tx", debug.crc.pip_dat_tx);
    pr_info("%-25s 0x%08x\n", "ipr_cmd_tx", debug.crc.ipr_cmd_tx);
    pr_info("%-25s 0x%08x\n", "ipr_dat_tx", debug.crc.ipr_dat_tx);
    pr_info("%-25s 0x%08x\n", "dbk_cmd_tx", debug.crc.dbk_cmd_tx);
    pr_info("%-25s 0x%08x\n", "dbk_dat_tx", debug.crc.dbk_dat_tx);
    pr_info("%-25s 0x%08x\n", "sao_cmd_tx", debug.crc.sao_cmd_tx);
    pr_info("%-25s 0x%08x\n", "rsz_cmd_tx", debug.crc.rsz_cmd_tx);
    pr_info("%-25s 0x%08x\n", "os_o4_tx", debug.crc.os_o4_tx);
    pr_info("%-25s 0x%08x\n", "os_r2b_tx", debug.crc.os_r2b_tx);
    pr_info("%-25s 0x%08x\n", "os_rsz_tx", debug.crc.os_rsz_tx);
    pr_info("%-25s 0x%08x\n", "status_red_0", debug.status_red_0);
    pr_info("%-25s 0x%08x\n", "status_red_1", debug.status_red_1);
    pr_info("%-25s 0x%08x\n", "status_pipe_0", debug.status_pipe_0);
    pr_info("%-25s 0x%08x\n", "status_pipe_1", debug.status_pipe_1);
    pr_info("%-25s 0x%08x\n", "status_ipred", debug.status_ipred);
    pr_info("%-25s 0x%08x\n", "status_mvpred", debug.status_mvpred);
    pr_info("%-25s 0x%08x\n", "status_hwcfg", debug.status_hwcfg);
    pr_info("%-25s 0x%08x\n", "status_xpredmac_cmd_0", debug.status_xpredmac_cmd_0);
    pr_info("%-25s 0x%08x\n", "status_xpredmac_cmd_1", debug.status_xpredmac_cmd_1);
    pr_info("%-25s 0x%08x\n", "status_xpredop_0", debug.status_xpredop_0);
    pr_info("%-25s 0x%08x\n", "status_xpredop_1", debug.status_xpredop_1);
    pr_info("%-25s 0x%08x\n", "status_dbk", debug.status_dbk);
    pr_info("%-25s 0x%08x\n", "status_resize", debug.status_resize);
    pr_info("%-25s 0x%08x\n", "status_general_0_hwpe0", debug.status_general_0_hwpe0);
    pr_info("%-25s 0x%08x\n", "status_general_1_hwpe0", debug.status_general_1_hwpe0);
    pr_info("%-25s 0x%08x\n", "status_general_0_hwpe1", debug.status_general_0_hwpe1);
    pr_info("%-25s 0x%08x\n", "mcc_hit", debug.mcc_hit);
    pr_info("%-25s 0x%08x\n", "mcc_out_ld16", debug.mcc_out_ld16);
    pr_info("%-25s 0x%08x\n", "mcc_out_ld32", debug.mcc_out_ld32);
    pr_info("%-25s 0x%08x\n", "mcc_out_ld64", debug.mcc_out_ld64);
    pr_info("%-25s 0x%08x\n", "mcc_in_ld16", debug.mcc_in_ld16);
    pr_info("%-25s 0x%08x\n", "mcc_in_ld32", debug.mcc_in_ld32);
    pr_info("%-25s 0x%08x\n", "mcc_in_ld64", debug.mcc_in_ld64);
    pr_info("___KILL_TEST___\n");
}

// Post an exec command on the command queue
HadesError_t HADES_ProcessCommand( HadesHandle_t Handle, uintptr_t FrameParams_fabric_p )
{
#ifndef VSOC_MODEL
    HostAddress* ContextAddr_p = (HostAddress*) Handle;
    HadesContext_t * Context_p = (HadesContext_t *)(ContextAddr_p->virtual);
#endif
    int ret;
    HostAddress runAddr;
    pedf_run_t *run;

    if (HADES_Malloc(&runAddr, sizeof(*run)) != HADES_NO_ERROR)
      return -1;
    run = (pedf_run_t*) (runAddr.virtual);

    run->fabricAddr = HADES_HostToFabricAddress(&runAddr);
#ifdef VSOC_MODEL
    run->instanceFabric = 0;
#else
    run->instanceFabric = (uint32_t) Context_p->instance.fabricAddr;
#endif
    // The PEDF-runtime gets the attributes address directly
    run->attributes = (uint32_t)FrameParams_fabric_p;
    run->fabricDone = 0;
    run->hostDone = 0; // unused
    HADES_Flush(&runAddr);

    ret = RpcFabric(PEDF_CMD_EXEC, &runAddr, &run->hdr);
    if (ret == -1)
        HADES_GetHWPE_Debug_Status();

    HADES_Invalidate(&runAddr);
    if (! run->fabricDone)
      pr_err("Error: HADES_ProcessCommand: fabric state machine error\n");

    HADES_Free(&runAddr);

    return HADES_NO_ERROR;
}

void HADES_GetHWPE_Debug(FrameCRC_t* computed_crc)
{
  // get HWPE regs
  computed_crc->hwc_ctb_rx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_CTBCMD_OFFSET);
  computed_crc->hwc_sli_rx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_SLICMD_OFFSET);
  computed_crc->hwc_mvp_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_HWCFG_MVPRED_OFFSET);
  computed_crc->hwc_red_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_HWCFG_RED_OFFSET);
  computed_crc->mvp_ppb_rx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_PPBIN_OFFSET);
  computed_crc->mvp_ppb_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_PPBOUT_OFFSET);
  computed_crc->mvp_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_MVPRED_XPREDMAC_OFFSET);
  computed_crc->mac_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_XPREDMAC_XPREDOP_CMD_OFFSET);
  computed_crc->mac_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_XPREDMAC_XPREDOP_DATA_OFFSET);
  computed_crc->xop_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_XPREDOP_IPRED_CMD_OFFSET);
  computed_crc->xop_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_XPREDOP_IPRED_DATA_OFFSET);
  computed_crc->red_dat_rx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_RESDATA_OFFSET);
  computed_crc->red_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_RED_PIPE_CMD_OFFSET);
  computed_crc->red_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_RED_PIPE_DATA_OFFSET);
  computed_crc->pip_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_PIPE_IPRED_CMD_OFFSET);
  computed_crc->pip_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_PIPE_IPRED_DATA_OFFSET);
  computed_crc->ipr_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_IPRED_OUT_CMD_OFFSET);
  computed_crc->ipr_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_IPRED_OUT_DATA_OFFSET);
  computed_crc->dbk_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_DEB_SAO_CMD_OFFSET);
  computed_crc->dbk_dat_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_DEB_SAO_DATA_OFFSET);
  computed_crc->sao_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_SAO_OUT_OFFSET);
  computed_crc->rsz_cmd_tx = sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_RESIZE_OUT_OFFSET);
  computed_crc->os_o4_tx =   sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_OUT_REF_OFFSET);
  computed_crc->os_r2b_tx =  sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_OUT_DISP_OFFSET);
  computed_crc->os_rsz_tx =  sthorm_read32(HadesBaseAddress + HADES_CL0_HWPE_DEBUG_OFFSET + HEVC_HWPE_DEBUG_CRC_OUT_DECIM_OFFSET);

  computed_crc->mask |=  (1llu << CRC_BIT_HWC_CTB_RX) | (1llu << CRC_BIT_HWC_SLI_RX) | (1llu << CRC_BIT_HWC_MVP_TX) | (1llu << CRC_BIT_HWC_RED_TX) | (1llu << CRC_BIT_MVP_PPB_RX)
                       | (1llu << CRC_BIT_MVP_PPB_TX) | (1llu << CRC_BIT_MVP_CMD_TX) | (1llu << CRC_BIT_MAC_CMD_TX) | (1llu << CRC_BIT_MAC_DAT_TX) | (1llu << CRC_BIT_XOP_CMD_TX)
                       | (1llu << CRC_BIT_XOP_DAT_TX) | (1llu << CRC_BIT_RED_DAT_RX) | (1llu << CRC_BIT_RED_CMD_TX) | (1llu << CRC_BIT_RED_DAT_TX) | (1llu << CRC_BIT_PIP_CMD_TX)
                       | (1llu << CRC_BIT_PIP_DAT_TX) | (1llu << CRC_BIT_IPR_CMD_TX) | (1llu << CRC_BIT_IPR_DAT_TX) | (1llu << CRC_BIT_DBK_CMD_TX) | (1llu << CRC_BIT_DBK_DAT_TX)
                       | (1llu << CRC_BIT_SAO_CMD_TX) | (1llu << CRC_BIT_RSZ_CMD_TX) | (1llu << CRC_BIT_OS_O4_TX)   | (1llu << CRC_BIT_OS_R2B_TX)  | (1llu << CRC_BIT_OS_RSZ_TX);
}
