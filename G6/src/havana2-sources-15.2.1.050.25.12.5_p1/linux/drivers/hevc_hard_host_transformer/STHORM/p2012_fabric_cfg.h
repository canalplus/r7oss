/**
 * \brief Configuration for the fabric
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

#ifndef _P2012_FABRIC_CONFIG_H_
#define _P2012_FABRIC_CONFIG_H_

// =================================================================
//                       Fabric Configuration
// =================================================================

//
// Max number of clusters in the fabric
//
#define P2012_MAX_FABRIC_CLUSTER_COUNT 4


// =================================================================
//                       Fabric NI
// =================================================================
#define P2012_CONF_FAB_NI_NIM0_OFFSET     0x00000000
#define P2012_CONF_FAB_NI_NIM0_SIZE       0x00001000
#define P2012_CONF_FAB_NI_NIM1_OFFSET     0x00001000
#define P2012_CONF_FAB_NI_NIM1_SIZE       0x00001000
#define P2012_CONF_FAB_NI_NIS_OFFSET      0x00002000
#define P2012_CONF_FAB_NI_NIS_SIZE        0x00001000

// =================================================================
//                       Fab mem NI
// =================================================================
#define P2012_CONF_FABMEM_NI_SIZE     0x00003000

#define P2012_CONF_FABMEM_NIS0_OFFSET 0x00000000
#define P2012_CONF_FABMEM_NIS0_SIZE   0x00001000
#define P2012_CONF_FABMEM_NIS1_OFFSET 0x00001000
#define P2012_CONF_FABMEM_NIS1_SIZE   0x00001000
#define P2012_CONF_FABMEM_NIS2_OFFSET 0x00002000
#define P2012_CONF_FABMEM_NIS2_SIZE   0x00001000
#define P2012_CONF_FABMEM_NIS3_OFFSET 0x00003000
#define P2012_CONF_FABMEM_NIS3_SIZE   0x00001000

// Backdoor port index mapping
#define P2012_FAB_MAX_BD_I_IDX 1000  // Index of initiator backdoor port at top level
#define P2012_FAB_MAX_BD_T_IDX 1000  // Idem for target

#endif //_P2012_FABRIC_CONFIG_H_
