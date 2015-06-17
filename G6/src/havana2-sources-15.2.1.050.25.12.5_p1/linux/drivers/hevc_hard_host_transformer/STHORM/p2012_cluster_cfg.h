/**
 * \brief Configuration for the P2012 Platform
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
 * Authors: Arthur STOUTCHININ, STMicroelectronics (arthur.stoutchinin@st.com)
 *          Bruno GALILEE, STMicroelectronics (bruno.galilee@st.com)
 *
 */

#ifndef _P2012_CLUSTER_CONFIG_H_
#define _P2012_CLUSTER_CONFIG_H_


// =================================================================
//                       ENCore Peripherals
// =================================================================
#define P2012_CONF_ENCPERIPH_CLUST_SIZE      0x00100000 // per cluster

// =================================================================
//                 Internal Memories Configuration
// =================================================================
#define P2012_CONF_ENCPERIPH_SCRATCHPAD_OFFSET  0x00000000
#define P2012_IMPL_ENCPERIPH_SCRATCHPAD_SIZE    0x00008000
#define P2012_CONF_ENCPERIPH_SCRATCHPAD_SIZE    0x00010000

// =================================================================
//                       Peripherals
// =================================================================
#define P2012_CONF_ENCPERIPH_HWS_OFFSET      (P2012_CONF_ENCPERIPH_SCRATCHPAD_OFFSET + P2012_CONF_ENCPERIPH_SCRATCHPAD_SIZE)
#define P2012_CONF_ENCPERIPH_HWS_SIZE        0x00010000

#define P2012_CONF_ENCPERIPH_PR_OFFSET       (P2012_CONF_ENCPERIPH_HWS_OFFSET + P2012_CONF_ENCPERIPH_HWS_SIZE)
#define P2012_CONF_ENCPERIPH_PR_SIZE         0x00020000

#define P2012_CONF_ENCPERIPH_ENC2SIF_OFFSET  (P2012_CONF_ENCPERIPH_PR_OFFSET + P2012_CONF_ENCPERIPH_PR_SIZE)
#define P2012_CONF_ENCPERIPH_ENC2SIF_SIZE    0x00010000

#define P2012_CONF_ENCPERIPH_SIF_OFFSET      (P2012_CONF_ENCPERIPH_ENC2SIF_OFFSET + P2012_CONF_ENCPERIPH_ENC2SIF_SIZE)
#define P2012_CONF_ENCPERIPH_SIF_SIZE        0x00010000

#define P2012_CONF_ENCPERIPH_DMA_OFFSET        (P2012_CONF_ENCPERIPH_SIF_OFFSET + P2012_CONF_ENCPERIPH_SIF_SIZE)
#define P2012_CONF_ENCPERIPH_DMA_SIZE          0x00010000
#define P2012_GET_CONF_ENCPERIPH_DMA_OFFSET(n) (P2012_CONF_ENCPERIPH_DMA_OFFSET + (n)*P2012_CONF_ENCPERIPH_DMA_SIZE)


// =================================================================
//                        DMA Channels
// =================================================================
#define P2012_ENCPERIPH_MAX_NB_OF_DMA       2
//#define P2012_ENCPERIPH_SIG_EOT_SIZE      31 // TO FIX, in the spec, the value is 32 when in the model it is 31...
#define P2012_ENCPERIPH_SIG_EOT_SIZE        (P2012_ENCORE_PE_COUNT) // Spec says 2*Npe, but only Npe are connected


// PR input errors
#define P2012_ENC_CTRL_ERROR_HWPE               0 // 0 to H-1
// #define P2012_ENC_CTRL_ERROR_CVP_HW            // H to C-1

// PR bitfield position
#define P2012_ENC_PR_PE0_FETCH_ENABLE_POS 0
#define P2012_ENC_PR_PE0_SW_RST_POS       0
#define P2012_ENC_PR_PE0_IT_NMI_POS       0
#define P2012_ENC_PR_PE_FETCH_ENABLE_MSK  ((1 << (P2012_ENC_PR_PE0_FETCH_ENABLE_POS + P2012_ENCORE_PE_COUNT)) - 1)
#define P2012_ENC_PR_PE_SW_RST_MSK        ((1 << (P2012_ENC_PR_PE0_SW_RST_POS + P2012_ENCORE_PE_COUNT)) - 1)
#define P2012_ENC_PR_PE_IT_NMI_MSK        ((1 << (P2012_ENC_PR_PE0_IT_NMI_POS+ P2012_ENCORE_PE_COUNT)) - 1)


// ITComb connection mapping
#define P2012_CLUST_DMA_ITCOMBI_IDX      28
#define P2012_CLUST_ITCOMBI_DMA_IDX      24
#define P2012_CLUST_ITCOMBI_DMA_SIZE     4


#endif //_P2012_CLUSTER_CONFIG_H_
