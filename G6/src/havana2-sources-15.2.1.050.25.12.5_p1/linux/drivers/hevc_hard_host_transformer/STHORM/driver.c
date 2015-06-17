/*
 * STHORM Host driver
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
 *          Germain Haugou (germain.haugou@st.com)
 *
 */

#include "driverEmuHce.h"
#include "driver.h"
#include "driverEmu.h"
#include "sthormAlloc.h"

#include "p2012_fc_periph_mailbox_hal.h"
#include "p2012_pr_control_ipgen_hal.h"

// ----------------------------------------------------------------------------
// Sending commands to the FC
// ----------------------------------------------------------------------------

/*
 * Send a message to the FC in order to execute one of the base-runtime methods
 */
static inline
int callBaseRuntime(p12_runtime_itf_e functionID, HostAddress* msgAddr, p12_msgHeader_t *msg)
{
    msg->fabricHandler = 0; // Use the default base runtime handler
    msg->requestID = functionID;
    msg->replyID = (uint32_t) msg; // FIXME : using the address of the message will only work on 32bit host
    msg->sender = P12_TARGET_HOST;
    msg->receiver = P12_TARGET_FC;
    //msg->replyCallback is set by sthorm_sendSyncMsg()

    // Synchronous remote call : wait for the answer
    sthorm_flush(msgAddr);
    return sthorm_sendSyncMsg(sthorm_remapHostToFabric((uintptr_t)msgAddr->physical), msg);
}

int sthorm_loadBinary(pedf_binary_t *binaryDesc, uint32_t parentDesc, uint32_t clusterId,
               p12_memBank_e textMemBank, p12_memBank_e dataMemBank)
{
    HostAddress msgAddr;
    p12_loadBinaryMsg_t *msg;
    int32_t result;

#ifdef HEVC_HADES_CUT1
    sthorm_malloc(&msgAddr, sizeof(*msg));
#else
    // USE TCDM for messages instead of DDR : Perf Enhancement
    sthorm_allocMessage(&msgAddr, sizeof(*msg));
#endif
    msg = (p12_loadBinaryMsg_t*) (msgAddr.virtual);

    msg->buff = sthorm_remapHostToFabric((uintptr_t)binaryDesc->binary.physical);
    msg->buffSize = binaryDesc->binary.size;
    msg->parentDesc = parentDesc;
    msg->name = sthorm_remapHostToFabric((uintptr_t)binaryDesc->name.physical);
    msg->textMemBank = textMemBank;
    msg->dataMemBank = dataMemBank;
    msg->clusterId = clusterId;
    msg->errorAddr = sthorm_remapHostToFabric((uintptr_t)binaryDesc->errorStr.physical);
    msg->errorSize = binaryDesc->errorStr.size;

    callBaseRuntime(P12_LOAD_BINARY, &msgAddr, &(msg->hdr));
    sthorm_invalidate(&msgAddr);

    // Save results
    binaryDesc->clusterId = clusterId;
    binaryDesc->xp70BinDesc = msg->binDesc;
    result = msg->result;
    pr_info("STHORM: loadBinary(%s) => result %d\n", (char*)(binaryDesc->name.virtual), result);

#ifdef HEVC_HADES_CUT1
    sthorm_free(&msgAddr);
#else
    sthorm_freeMessage(&msgAddr);
#endif

    return result;
}

#if 0
// JLX: unused
// JLX: if needed, copy from sthorm_loadBinary (using HostAddress type)
int sthorm_unloadBinary(pedf_binary_t *binDesc)
{
    p12_closeBinaryMsg_t *msg;
    int32_t result;

    msg = (p12_closeBinaryMsg_t *) sthorm_malloc(sizeof(p12_closeBinaryMsg_t));

    msg->binDesc = binDesc->xp70BinDesc;

    callBaseRuntime(P12_CLOSE_BINARY, &(msg->hdr));

    result = msg->result;

    // JLX: these are free'd by HADES_UnloadFirmware()
    // sthorm_free_tr(binDesc->binaryHost, binDesc->binaryFabric);
    // sthorm_free_tr(binDesc->nameHost, binDesc->nameFabric);
    // sthorm_free_tr(binDesc->errorStrHost, binDesc->errorStrFabric);

    sthorm_free((uintptr_t)msg);

    return result;
}
#endif

int sthorm_callMain(pedf_binary_t *binDesc, void *arg)
{
    HostAddress msgAddr;
    p12_callMainMsg_t *msg;
    int32_t result;

    sthorm_malloc(&msgAddr, sizeof(*msg));
    msg = (p12_callMainMsg_t*) (msgAddr.virtual);

    msg->binDesc = binDesc->xp70BinDesc;
    msg->arg = (uint32_t) arg;

    LIB_TRACE(2, "Calling main entry point of binary 0x%x with arg %p\n", binDesc->xp70BinDesc, arg);
    callBaseRuntime(P12_CALLMAIN, &msgAddr, &(msg->hdr));

    sthorm_invalidate(&msgAddr);
    result = msg->result;

    sthorm_free(&msgAddr);

    LIB_TRACE(3, "Done calling main entry point of binary 0x%x\n", binDesc->xp70BinDesc);

    return result;
}

// ----------------------------------------------------------------------------
// Remapping functions
// ----------------------------------------------------------------------------

typedef struct {
    uint32_t  fabricAddr;   // Physical address seen by the STxP70 in the fabric
    uint32_t  size;         // Size of the memory region in bytes
    uintptr_t hostAddr;     // Address used by the host, may be virtual
} rab_map_t;

// Remapping table to move between fabric and host addresses
static rab_map_t remap_table[STHORM_REMAP_MAX_ENTRIES];

// Number of entries in the remapping table
static uint32_t remap_entries = 0;

void sthorm_clearRemapTable()
{
    int i;
    remap_entries = 0;
    for (i=0; i<STHORM_REMAP_MAX_ENTRIES; i++) {
        remap_table[i].fabricAddr = 0;
        remap_table[i].size = 0;
        remap_table[i].hostAddr = 0;
    }
}

int sthorm_addRemapEntry(uintptr_t hostAddr, uint32_t fabricAddr, uint32_t size)
{
    if (remap_entries >= STHORM_REMAP_MAX_ENTRIES) {
        return -1;
    }
    LIB_TRACE(2, "New remap entry #%d [Host:0%p Fab:%p Size:%p]\n",
              remap_entries, (void*)hostAddr, (void*)fabricAddr, (void*)size);
    remap_table[remap_entries].fabricAddr = fabricAddr;
    remap_table[remap_entries].hostAddr = hostAddr;
    remap_table[remap_entries].size = size;

    // pr_info("---- STHORM: REMAP:%d: fabric %p, host %p, %d\n", remap_entries, (void*)fabricAddr, (void*)hostAddr, size);
    remap_entries++;
    return 0;
}

// TODO: Eventualy we could do a binary search if the remap_table is ordered.
//       For a small table with the most frequent ranges at the begining
//       a linear search is good enough.
uintptr_t sthorm_remapFabricToHost(uint32_t addr)
{
    uint32_t i;
    uint32_t remapped = addr;
    for (i=0; i<remap_entries; i++) {
        if ( (addr >= remap_table[i].fabricAddr) &&
             (addr < remap_table[i].fabricAddr + remap_table[i].size) ) {
            remapped = addr - remap_table[i].fabricAddr + remap_table[i].hostAddr;
            return remapped;
        }
    }
    pr_err("Error: STHORM: unable to remap fabric address %p to physical\n", (void*)addr);
    return 0;
}

uint32_t sthorm_remapHostToFabric(uintptr_t addr)
{
    uint32_t i;
    uint32_t remapped = addr;
    for (i=0; i<remap_entries; i++) {
        if ( (addr >= remap_table[i].hostAddr) &&
             (addr < remap_table[i].hostAddr + remap_table[i].size) ) {
            remapped = addr - remap_table[i].hostAddr + remap_table[i].fabricAddr;
            return remapped;
        }
    }
    pr_err("Error: STHORM: unable to remap physical address %p to fabric address\n", (void*)addr);
    return 0;
}
