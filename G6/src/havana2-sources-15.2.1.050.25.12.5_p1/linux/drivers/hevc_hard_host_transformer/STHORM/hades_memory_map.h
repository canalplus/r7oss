/*
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
 */

/**
  \file hades_memory_map.h
  \brief Hades memory map and rab settings

  Creation date: Wed Nov 09 11:14:44 2012

  \par Module owner:
          Sebastien Simon, STMicroelectronics
  \par Authors:
          Sebastien Simon, STMicroelectronics

  \par Copyright (C) 2009 STMicroelectronics
  \par  Id: $Id$
  \par  Date: $Date$
  \par  Revision: $Rev$
*/

#ifndef H_HADES_MEMORY_MAP
#define H_HADES_MEMORY_MAP

#define RAB_MEM_SIZE          0x00002000        // 8KB for each RAB
#define PREPROC_SIZE          0x00001000        // 6KB for each Preproc
#define FC_SIZE               0x00080000        // 32KB for Fabric Controller

#define CL_HWPE_CTBE_SIZE     0x00008000        // 32KB CTB engine HWPE
#define CL_HWPE_OUTE_SIZE     0x00008000        // 32KB Output engine HWPE
#define CL_UNUSED_SIZE        0x00030000        // 192KB Output engine HWPE
#define CL_DMEM_SIZE          0x00040000        // 256KB shared DMEM
#define CL_PERIPH_SIZE        0x00080000        // 512KB cluster Periph
#define CLUSTER_SIZE          (CL_HWPE_CTBE_SIZE + \
                              CL_HWPE_OUTE_SIZE + \
                              CL_UNUSED_SIZE + \
                              CL_DMEM_SIZE + \
                              CL_PERIPH_SIZE)  // Whole cluster size = 1024KB (0x00100000)

#define CLUSTER_CONFIG_SIZE   0x00004000        // 16KB for cluster configuration (Backdoor RAB, Plug and debug)

#define HADES_PP_OFFSET 	0x00000000	// PP offset from Hades base address
#define HADES_FABRIC_OFFSET 	0x00010000      // Decoder (fabric) offset from Hades base address


// HOST memory space-------------------------------
#define HOST_RAB_SLAVE_OFFSET               (0x00000000)                           // RAB host to fabric (offset from decoder address)
#define HOST_CIC_RAB_MASTER_OFFSET          (HOST_RAB_SLAVE_OFFSET + RAB_MEM_SIZE) // RAB fabric to host
#define HOST_DIC_RAB_MASTER_OFFSET          (HOST_CIC_RAB_MASTER_OFFSET + RAB_MEM_SIZE) // RAB fabric to host
#define HOST_RAB_BACKDOOR_OFFSET            (HOST_DIC_RAB_MASTER_OFFSET + RAB_MEM_SIZE) // RAB host to fabric
#define HOST_RAB_BACKDOOR_1_OFFSET          (HOST_RAB_BACKDOOR_OFFSET)                  // RAB backdoor cluster #1
#define HOST_RAB_BACKDOOR_2_OFFSET          (HOST_RAB_BACKDOOR_1_OFFSET + RAB_MEM_SIZE) // RAB backdoor cluster #2
#define HOST_RAB_BACKDOOR_3_OFFSET          (HOST_RAB_BACKDOOR_2_OFFSET + RAB_MEM_SIZE) // RAB backdoor cluster #3
#define HOST_RAB_BACKDOOR_4_OFFSET          (HOST_RAB_BACKDOOR_3_OFFSET + RAB_MEM_SIZE) // RAB backdoor cluster #4

#define HADES_BACKBONE_OFFSET		0xE000	// Offset from decoder address
#define HADES_IT_CLEAR 			0xC04

#define HADES_CLUSTER0_OFFSET		0x20000
#define HADES_CL0_HWPE_OFFSET		0x800
#define HADES_CL0_HWPE_DEBUG_OFFSET	(HADES_CLUSTER0_OFFSET + HADES_CL0_HWPE_OFFSET)

#define HOST_FC_OFFSET                  (0x000F0000)
#define HOST_FC_UNUSED			(0x00080000)
#define HOST_CLUSTER_OFFSET             (HOST_FC_OFFSET + FC_SIZE + HOST_FC_UNUSED)

#define HOST_CL_PERIPH_OFFSET(cid)      (HOST_CLUSTER_OFFSET + (CLUSTER_SIZE * cid))
#define HOST_CL_DMEM_OFFSET(cid)        (HOST_CL_PERIPH_OFFSET(cid) + CL_PERIPH_SIZE)
#define HOST_CL_HWPE_OUTE_OFFSET(cid)   (HOST_CL_DMEM_OFFSET(cid) + CL_DMEM_SIZE + CL_UNUSED_SIZE)
#define HOST_CL_HWPE_CTBE_OFFSET(cid)   (HOST_CL_HWPE_OUTE_OFFSET(cid) + CL_HWPE_OUTE_SIZE)

#define HOST_DCHAN_OFFSET               0x00070000                // offset from Peripheral cluster0 address 0x2005_0000
#define HOST_CL0_DCHAN_OFFSET           (HOST_CL_PERIPH_OFFSET(0) + HOST_DCHAN_OFFSET)   // offset from Decoder Base Address
// TODO: P2012_config_mem.h is not in line with Hades_IP_Func_Spec (mem size etc..). Use these local defines instead
// HADES memory space------------------------------
#define HADES_L3_BASE               P2012_CONF_SOCMEM_BASE
#define HADES_PEDF_FIRMWARE_ADDRESS (HADES_L3_BASE)
#define HADES_BASE_FIRMWARE_ADDRESS 0x9DDDD000ULL // cf. STHORM_BOOT_ADDR_L3=0x9DDDD000 in rt_host_def.h

#define HADES_BASE_FIRMWARE_SIZE    0x200000   // 2 MB
#define HADES_HOST_DDR_ADDRESS      0xB0000000ULL
#define HADES_HOST_DDR_SIZE         0x21E00000

// Host memory space
// Note: must match BPA2 partitions "Hades_firmware" & Hades_L3 definitions in:
//           - VSOC: parameters of tlm_run.exe
//           - SOC : config.in in SDK2 build dir

// SDK2-14
// TODO => define address with device tree to avoid hard coded value in driver
#define HOST_FIRMWARE_ADDRESS       0x40A00000
#define HOST_BASE_FIRMWARE_ADDRESS  HOST_FIRMWARE_ADDRESS
#define HOST_PEDF_FIRMWARE_ADDRESS  (HOST_FIRMWARE_ADDRESS + HADES_BASE_FIRMWARE_SIZE) // offset 32 MB
// HOST_HOST_DDR_ADDRESS & HOST_HOST_DDR_SIZE are now read through bpa2 linux API

#endif //H_HADES_MEMORY_MAP
