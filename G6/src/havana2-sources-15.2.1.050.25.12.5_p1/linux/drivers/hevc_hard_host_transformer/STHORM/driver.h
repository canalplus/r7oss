/*
 * Host driver base API
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

#ifndef _PEDF_HOST_DRIVER_H_
#define _PEDF_HOST_DRIVER_H_

#include <linux/ptrace.h>

#include "pedf_host_abi.h"
#include "sthormAlloc.h"

// JLX: main tracing function
#define LIB_TRACE(level, format, args...) { if (level <= 0) pr_info("HADES_API: " format, ##args); }

/**
 * \defgroup rtDriver Host driver to interact with the base SThorm runtime
 *
 * @{
 */

/**
 * \brief Descriptor of a binary which is loaded on P2012
 *
 * This structure is used by the host to keep track of which binary is
 * loaded where, in order to call it and do resource management.
 */
typedef struct {
    HostAddress binary;
    HostAddress name;

    int clusterId;            //!< Were the binary will execute on SThorm
    HostAddress rtInfo;
    pedf_runtime_info_t* rtInfo_p;   //!< allocated by host, filled by pedf_runtime main entry point

    HostAddress errorStr;

    uint32_t xp70BinDesc;     //!< Handle for the relocated binary, as returned by the SThorm runtime
} pedf_binary_t;

/**
 * \brief Ask the base-runtime to load a binary in P2012
 *
 * The binary will be relocated in P2012 memory space
 *
 * \param[in] binaryDesc    binary descriptor of the FW to be loaded (FW is already copied in memory)
 * \param[in] parentDesc    Parent binary descriptor for symbol resolution (cal be NULL)
 * \param[in] clusterId     Cluster ID where to load the binary
 * \param[in] textMemBank   Memory bank to use for code (.text)
 * \param[in] dataMemBank   Memory bank to use for data (.bss, .data)
 *
 * \return 0 on success
 */
int sthorm_loadBinary(pedf_binary_t *binaryDesc, uint32_t parentDesc, uint32_t clusterId,
               p12_memBank_e textMemBank, p12_memBank_e dataMemBank);

/**
 * \brief Ask the runtime to unload a binary from P2012
 *
 * \param[in] binary    Binary descriptor, as returned by loadBinary()
 *
 * \return 0 on success
 */
int sthorm_unloadBinary(pedf_binary_t *binary);

/**
 * \brief Call the main entry point of a previously loaded binary
 *
 * \param[in] binDesc   Binary descriptor, as returned by loadBinary()
 * \param[in] arg       Argument to pass to the main entry point

 * \return the return value of the executed main entry point
 */
int sthorm_callMain(pedf_binary_t *binDesc, void *arg);

// Maximum number of entries in the table
#define STHORM_REMAP_MAX_ENTRIES 32

/**
 * \brief Add a new entry in the remapping table.
 *
 * If the RAB (Remapping Address Bridge) is used, this table should be filled
 * with the same informations that are programmed in the RAB.
 *
 * \param[in] hostAddr      Address seen from the host
 * \param[in] fabricAddr    Physical address used in the fabric
 * \param[in] size          Size of the memory zone
 *
 * \return 0 on success, -1 if the table is already full
 */
int sthorm_addRemapEntry(uintptr_t hostAddr, uint32_t fabricAddr, uint32_t size);

/**
 * \brief Clear the remapping table
 *
 * When the table is empty, calls to sthorm_remapFabric2Host will do nothing
 */
void sthorm_clearRemapTable(void);

/**
 * \brief Translate a fabric address into a host address
 *
 *  If no matching entry is found in the remap table the input address will be
 *  returned as-is
 *
 * \param[in] fabricAddr    Physical address used in the fabric
 *
 * \return the translated address
 */
uintptr_t sthorm_remapFabricToHost(uint32_t fabricAddr);

/**
 * \brief Translate a host address into a fabric address
 *
 *  If no matching entry is found in the remap table the input address will be
 *  returned as-is
 *
 * \param[in] hostAddr    Address used in the host
 *
 * \return the translated address
 */
uint32_t sthorm_remapHostToFabric(uintptr_t hostAddr);


/**@}*/

#endif /* _PEDF_HOST_DRIVER_H_ */
