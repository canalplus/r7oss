/*
 * Host driver emulation layer, common functions
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
 *
 */

#include "driverEmuHce.h"
#include "driverEmuCommon.h"
#include "driverEmu.h"
#include "driver.h"

#include "driverEmuHce.h"

#include "p2012_rt_conf.h"

#include "p2012_memory_map.h"
#include "p2012_pr_control_ipgen_hal.h"
#include "p2012_fc_periph_mailbox_hal.h"
#include "rt_host_abi.h"

/*
 * Utility function to get an integer value from a string
 */
#if 0
// JLX: cannot be used in kernel
int getIntFromStr(const char *str, int fallback)
{
    long val = 0;
    char *endptr;
    errno = 0;
    val = strtol(str, &endptr, 0);
    if (errno != 0) {
        LIB_TRACE(0, "Warning: failed to extract integer value from string %s, using fallback %d\n",
                  str, fallback);
        perror("strtol");
        return fallback;
    }
    if (endptr == str) {
        LIB_TRACE(0, "Warning: string %s doesn't contain an integer, using fallback %d\n",
                str, fallback);
        return fallback;
    }
    return (int) val;
}
#endif

void fabricCtrl_boot(uint32_t base)
{
    LIB_TRACE(0, "Booting fabric controller (id=%d) @ 0x%x\n", P2012_MAX_FABRIC_CLUSTER_COUNT, base);

    // Activate soft-reset of the core (drive to 0)
    hal_write_pr_control_bank_pr_ctrl_soft_reset_uint32(base, 0x0);
    // It is noticed on some boards that decoding time is too high for
    // the first time after board power up. As a workaround calling
    // soft reset twice corrects the issue. Refer bug 59348.
    hal_write_pr_control_bank_pr_ctrl_soft_reset_uint32(base, 0x0);
    // Stop fetch enable signal on the core
    hal_write_pr_control_bank_pr_ctrl_fetch_enable_uint32(base, 0x0);
    // Set the boot address of the core
    // by default the FC always boots at address 0x9DDDD000 in L3 (static address defined in elf header at compil time)
    hal_write_pr_control_bank_pr_ctrl_boot_address_uint32(base, STHORM_BOOT_ADDR_L3);
    // Stop soft-reset (drive to 1)
    hal_write_pr_control_bank_pr_ctrl_soft_reset_uint32(base, P2012_FC_PR_FP_SW_RST_MSK);
    // Activate fetch enable, the core will now boot
    LIB_TRACE(3, "Sending fetch enable to FC\n");
    hal_write_pr_control_bank_pr_ctrl_fetch_enable_uint32(base, P2012_FC_PR_FP_FETCH_ENABLE_MSK);
}

void init_runtimeConf(const uint32_t addr, const int numCluster,
                      const uint32_t l3MemStart, const uint32_t l3MemSize)
{
    runtimeConf_t *runtimeConf;
    int clusterMask = (1 << STHORM_FC_CLUSTERID) | ((1 << numCluster) - 1);

    runtimeConf = (runtimeConf_t *)(addr);

    // FIXME JLX allow dynamic conf of firmware traces
    // LIB_TRACE(0, "Configuring fabric runtime @ 0x%x: rtVerbose=%d pedfVerbose=%d userVerbose=%d\n", addr, rtVerbose, pedfVerbose, userVerbose);

    LIB_TRACE(0, "Specified cluster mask: 0x%x for %d clusters\n", clusterMask, numCluster);

    tlm_write32((uint32_t)&runtimeConf->L3base, l3MemStart); // L3 allocator not needed in runtime for PEDF/HEVC
    tlm_write32((uint32_t)&runtimeConf->L3size, l3MemSize);
    tlm_write32((uint32_t)&runtimeConf->L2size, 0);
    tlm_write32((uint32_t)&runtimeConf->TCDMsize, 0x4000);  //TODO STHORM_IMPL_FC_TCDM_SIZE = 0x4000
    tlm_write8((uint32_t)&runtimeConf->rtVerbose[0], 0);
    tlm_write8((uint32_t)&runtimeConf->rtVerbose[1], 0);
    tlm_write8((uint32_t)&runtimeConf->rtVerbose[2], 0);
    tlm_write8((uint32_t)&runtimeConf->rtVerbose[3], 0);
    tlm_write8((uint32_t)&runtimeConf->userVerbose[0], 0); // PEDF Verbose
    tlm_write8((uint32_t)&runtimeConf->userVerbose[1], 0); // App Verbose
    tlm_write8((uint32_t)&runtimeConf->userVerbose[2], 0);
    tlm_write8((uint32_t)&runtimeConf->userVerbose[3], 0);
    tlm_write32((uint32_t)&runtimeConf->platform, P2012_PLATFORM_ISS);
    tlm_write32((uint32_t)&runtimeConf->clusterMask, clusterMask);
}

/*
 * Used in lowLoader.c to read and load the ELF binary
 */
void sthorm_memcpy(void *dest, void *source, int size)
{
    uint32_t phys_addr = sthorm_remapFabricToHost((uint32_t) dest);
    LIB_TRACE(4,"sthorm_memcpy from host:%p to [fab:%p host:%p] size %d\n", source, dest, (void*)phys_addr, size);
    sthorm_write_block(phys_addr, source, size);
}

void sthorm_memset(void *dest, int value, int size)
{
    uint32_t phys_addr = sthorm_remapFabricToHost((uint32_t) dest);
    LIB_TRACE(4,"sthorm_memset at [fab:%p host:%p] value %d size %d\n", dest, (void*)phys_addr, value, size);
    sthorm_set_block(phys_addr, value, size);
}
