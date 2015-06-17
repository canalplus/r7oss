/**
 * \brief Memory map at SoC level
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
 * Authors: Bruno GALILEE, STMicroelectronics (bruno.galilee@st.com)
 *          Arthur STOUTCHININ, STMicroelectronics (arthur.stoutchinin@st.com)
 *
 */

#ifndef _P2012_CONFIG_MEM_H_
#define _P2012_CONFIG_MEM_H_

#include "p2012_fabric_cfg.h"  // Declares P2012_FABRIC_CLUSTER_COUNT
#include "p2012_cluster_cfg.h"
#include "p2012_pr_cfg.h"
#include "p2012_fabric_controller_cfg.h"

#ifdef __cplusplus
namespace p2012
{
#endif

// =================================================================
//                       Global IP view
// =================================================================
#define P2012_CONF_IP_BASE          0x10000000
#define P2012_CONF_IP_SIZE          0x50003000

// =================================================================
//                       Fabric Controller
// =================================================================
#define P2012_CONF_FC_BASE           0x50000000
#define P2012_CONF_FC_SIZE           0x08000000
#define P2012_CONF_FC_TCDM_BASE      (P2012_CONF_FC_BASE + P2012_CONF_FC_TCDM_OFFSET)
#define P2012_CONF_FC_DMA_BASE       (P2012_CONF_FC_BASE + P2012_CONF_FC_DMA_OFFSET)
#define P2012_CONF_FC_PERIPH_BASE    (P2012_CONF_FC_BASE + P2012_CONF_FC_PR_OFFSET)
#define P2012_CONF_FC_TIMER_BASE     (P2012_CONF_FC_PERIPH_BASE + P2012_CONF_PRCTRL_TIMER_OFFSET)
#define P2012_CONF_FC_MBX_BASE       (P2012_CONF_FC_PERIPH_BASE + P2012_CONF_PRCTRL_MSG_OFFSET)
#define P2012_CONF_FC_CTRL_BASE      (P2012_CONF_FC_PERIPH_BASE + P2012_CONF_PRCTRL_REG_OFFSET)
#define P2012_CONF_FC_ITCOMB_BASE    (P2012_CONF_FC_PERIPH_BASE + P2012_CONF_PRCTRL_ITC_OFFSET)
#define P2012_CONF_FC_PRT1_BASE      (P2012_CONF_FC_PERIPH_BASE + P2012_CONF_PRCTRL_T1_OFFSET)

#define P2012_CONF_FC_STM_BASE       (P2012_CONF_FC_BASE + P2012_CONF_FC_STM_OFFSET)

// =================================================================
//                           Fabric
// =================================================================
#define P2012_CONF_FABMEM_BASE          0x58000000
#define P2012_IMPL_FABMEM_SIZE          0x00100000
#define P2012_CONF_FABMEM_SIZE          0x08000000

#define P2012_CONF_FABMEM_NI_BASE       0x5FFFC000
#define P2012_CONF_FABMEM_NI_NIS0_BASE  (P2012_CONF_FABMEM_NI_BASE + P2012_CONF_FABMEM_NIS0_OFFSET)
#define P2012_CONF_FABMEM_NI_NIS1_BASE  (P2012_CONF_FABMEM_NI_BASE + P2012_CONF_FABMEM_NIS1_OFFSET)
#define P2012_CONF_FABMEM_NI_NIS2_BASE  (P2012_CONF_FABMEM_NI_BASE + P2012_CONF_FABMEM_NIS2_OFFSET)
#define P2012_CONF_FABMEM_NI_NIS3_BASE  (P2012_CONF_FABMEM_NI_BASE + P2012_CONF_FABMEM_NIS3_OFFSET)

// ----------------------------- ENCMem --------------------------------
#define P2012_CONF_ENCMEM_BASE       0x10000000
#define P2012_CONF_ENCMEM_SIZE       ((P2012_CONF_ENCMEM_CLUST_SIZE) * (P2012_MAX_FABRIC_CLUSTER_COUNT))
#define P2012_GET_CONF_ENCMEM_BASE(cid_) (P2012_CONF_ENCMEM_BASE + (cid_) * P2012_CONF_ENCMEM_CLUST_SIZE)
#define P2012_GET_CONF_ENCMEMTAS_BASE(cid_) (P2012_CONF_ENCMEM_BASE + (cid_) * P2012_CONF_ENCMEM_CLUST_SIZE + (P2012_CONF_ENCMEM_CLUST_SIZE / 2))
#define P2012_CONF_ENCMEM_CLUST_SIZE 0x00008000

// ----------------------------- SNOOP SYNCHRO DETECTION --------------------------------
#define P2012_GET_ENCMEM_SNOOP_SYNCHRO(cid_, pid_) (P2012_CONF_ENCMEM_BASE + P2012_CONF_ENCMEM_SNOOP_OFFSET + (cid_) * P2012_CONF_ENCMEM_CLUST_SIZE + (pid_ << 2))

// ------------------------ Peripherals ----------------------------

// --------------------------- ENCORE ------------------------------
#define P2012_CONF_ENCPERIPH_BASE                   0x20000000
#define P2012_CONF_ENCPERIPH_SIZE                   ((P2012_CONF_ENCPERIPH_CLUST_SIZE) * (P2012_MAX_FABRIC_CLUSTER_COUNT))

#define P2012_GET_CONF_ENCPERIPH_BASE(cid_)         (P2012_CONF_ENCPERIPH_BASE + (cid_) * P2012_CONF_ENCPERIPH_CLUST_SIZE)
#define P2012_GET_CONF_ENCPERIPH_SCRATCHPAD_BASE(cid_) (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_SCRATCHPAD_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_HWS_BASE(cid_)     (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_HWS_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_)      (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_PR_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_TIMER_BASE(cid_)   (P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_) + P2012_CONF_PRCTRL_TIMER_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_MBX_BASE(cid_)     (P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_) + P2012_CONF_PRCTRL_MSG_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_CTRL_BASE(cid_)    (P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_) + P2012_CONF_PRCTRL_REG_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_ITCOMB_BASE(cid_)  (P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_) + P2012_CONF_PRCTRL_ITC_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_PRT1_BASE(cid_)    (P2012_GET_CONF_ENCPERIPH_PR_BASE(cid_) + P2012_CONF_PRCTRL_T1_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_ENC2SIF_BASE(cid_) (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_ENC2SIF_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_SIF_BASE(cid_)     (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_SIF_OFFSET)

#define P2012_GET_CONF_ENCPERIPH_DMA_BASE(cid_, n)  (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_GET_CONF_ENCPERIPH_DMA_OFFSET(n))
#define P2012_GET_CONF_ENCPERIPH_NI_BASE(cid_)      (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_NI_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_NI_NILL_BASE(cid_) (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_NILL_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_NI_NIBW_BASE(cid_) (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_NIBW_OFFSET)
#define P2012_GET_CONF_ENCPERIPH_NI_NIS_BASE(cid_)  (P2012_GET_CONF_ENCPERIPH_BASE(cid_) + P2012_CONF_ENCPERIPH_NIS_OFFSET)

// =================================================================
//                              HWPE
// =================================================================
#define P2012_CONF_HWPE_BASE       0x30000000
#define P2012_CONF_HWPE_CELL_SIZE  0x00008000
#define P2012_CONF_HWPE_SIZE       ((P2012_CONF_HWPE_CLUST_SIZE) * (P2012_MAX_FABRIC_CLUSTER_COUNT))
#define P2012_CONF_HWPE_CLUST_SIZE 0x00008000

#define P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_)      (P2012_CONF_HWPE_BASE + (cid_) * P2012_CONF_HWPE_CLUST_SIZE + (hwpe_id_) * P2012_CONF_HWPE_CELL_SIZE)
#define P2012_GET_CONF_HWPE_SIF_BASE(cid_, hwpe_id_)  (P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_) + P2012_CONF_HWPE_SIF_OFFSET)
#define P2012_GET_CONF_HWPE_CVPU_BASE(cid_, hwpe_id_) (P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_) + P2012_CONF_HWPE_CVPU_OFFSET)
#define P2012_GET_CONF_HWPE_CTRL_BASE(cid_, hwpe_id_) (P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_) + P2012_CONF_HWPE_CTRL_OFFSET)
#define P2012_GET_CONF_HWPE_CMD_BASE(cid_, hwpe_id_)  (P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_) + P2012_CONF_HWPE_CMD_OFFSET)
#define P2012_GET_CONF_HWPE_REGS_BASE(cid_, hwpe_id_) (P2012_GET_CONF_HWPE_BASE(cid_, hwpe_id_) + P2012_CONF_HWPE_REGS_OFFSET)


// =================================================================
//                 External Memory Configuration
// =================================================================
#define P2012_CONF_SOCMEM_BASE        0x80000000
#define P2012_IMPL_SOCMEM_SIZE        0x1FDDD000    // 512 MBs less area reserved for TBDs (easily ARM reachable area), HCE constraint: multiple of 0x1000
#define P2012_CONF_SOCMEM_SIZE        0x70000000

#define P2012_CONF_DDRAM0_BASE        0xE0000000    // HCE2 program area
#define P2012_IMPL_DDRAM0_SIZE        0x10000000    // 256 MBs

// =================================================================
//                               NI
// =================================================================
#define P2012_CONF_FAB_NI_BASE            0x60000000
#define P2012_CONF_FAB_NI_NIM0_BASE       (P2012_CONF_FAB_NI_BASE + P2012_CONF_FAB_NI_NIM0_OFFSET)
#define P2012_CONF_FAB_NI_NIM1_BASE       (P2012_CONF_FAB_NI_BASE + P2012_CONF_FAB_NI_NIM1_OFFSET)
#define P2012_CONF_FAB_NI_NIS_BASE        (P2012_CONF_FAB_NI_BASE + P2012_CONF_FAB_NI_NIS_OFFSET)
#define P2012_CONF_FAB_NI_SIZE            0x00003000

// =================================================================
//                           VIRTUAL IPs
// =================================================================
#define P2012_CONF_HWPU_DBG_REG_BASE  0x9FDDDE00 // Used to test HWPE backdoor target port access
#define P2012_CONF_HWPU_DBG_REG_SIZE  0x00000010
#define P2012_CONF_JTAG_TESTER_BASE   0x9FDDDF00 // Reserve sufficient room for 32 cluster TBDs
#define P2012_CONF_JTAG_TESTER_SIZE   0x00000100
#define P2012_CONF_TB_DRIVER_BASE     (P2012_CONF_JTAG_TESTER_BASE + P2012_CONF_JTAG_TESTER_SIZE)
#define P2012_CONF_TB_DRIVER_SIZE     0x00001000
#define P2012_GET_CONF_TB_DRIVER_BASE(id_) (P2012_CONF_TB_DRIVER_BASE + (id_) * P2012_CONF_TB_DRIVER_SIZE)


#ifdef __cplusplus
} // namespace
#endif

#endif // _P2012_CONFIG_MEM_H_
