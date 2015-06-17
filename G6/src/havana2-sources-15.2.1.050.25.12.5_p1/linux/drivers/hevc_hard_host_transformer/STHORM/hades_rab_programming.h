/**
 * \brief defines RAB HAL to program entries and activate RAB
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
 * Authors: Jean-Philippe COUSIN, STMicroelectronics (jean-philippe.cousin@st.com)
 *          Gilles PELISSIER, STMicroelectronics (gilles.pelissier@st.com)
 *
 */

#ifndef _HADES_RAB_PROGRAMMING_H_
#define _HADES_RAB_PROGRAMMING_H_

#include "p2012_rab_ipgen_hal1.h"
#include "p2012_config_mem.h"
#include "hades_memory_map.h"
#include "hades_api.h"

// This functions configures RAB in bypasss mode => no translation applied
static inline void hal2_deactivate_rab(uint32_t rab_base_) {
  uint32_t data=0;
  //activates RAB
  data = RAB_CFG__BYPASS_ON << RAB_CFG__BYPASS_FLOC;
  hal1_write_gp_rab_cfg_uint32(rab_base_+ 0x1000,data);
}

// THis functions configures RAB in translation mode => no translation applied
static inline void hal2_activate_rab(uint32_t rab_base_) {
  uint32_t data=0;
  //activates RAB
  data = RAB_CFG__BYPASS_OFF << RAB_CFG__BYPASS_FLOC;
  hal1_write_gp_rab_cfg_uint32(rab_base_ + 0x1000 ,data);
}

// THis functions configures RAB entry with various parameters: source base, mask, remap address and protection
static inline void hal2_configure_rab_entry(uint32_t rab_base_,
			      uint32_t entry_id_,
			      uint32_t src_base_ ,
			      uint32_t src_max_,
			      uint32_t dst_remap_,
			      uint32_t prot_,
			      uint32_t coherency_) {
   int  entry_ba=0;
   int data =0;
   // compute entry base  address
   entry_ba = rab_base_ + 0x10*entry_id_;

   hal1_write_entry_rab_src_range_min_uint32(entry_ba, src_base_);
   hal1_write_entry_rab_src_range_max_uint32(entry_ba, src_max_);
   hal1_write_entry_rab_dst_remap_uint32(entry_ba, dst_remap_);
   data=  ((prot_ << RAB_CTRL__PROTECTION_FLOC) & RAB_CTRL__PROTECTION_FMSK) |
       ((RAB_CTRL__VALID_ON << RAB_CTRL__VALID_FLOC) & RAB_CTRL__VALID_FMSK) |
       ((coherency_ << RAB_CTRL__COHERENCY_FLOC) & RAB_CTRL__COHERENCY_FMSK);
   hal1_write_entry_rab_ctrl_uint32(entry_ba,data);
}

// Configure RAB default entries from host to fabric
static inline void hal2_configure_rab_slave_default(uint32_t hostBase, int clusterMask, int *nbEntries)
{
  int i = 0;
  int index = 0;
  uint32_t fabricBase = hostBase + HADES_FABRIC_OFFSET;
  uint32_t fabricBaseForHades = HADES_FABRIC_OFFSET; // only the 21 LSB


  // FC
  hal2_configure_rab_entry( fabricBase + HOST_RAB_SLAVE_OFFSET,
                                index++,
                                fabricBaseForHades + HOST_FC_OFFSET,
                                fabricBaseForHades + HOST_FC_OFFSET + FC_SIZE - 1,
                                P2012_CONF_FC_BASE,
                                RAB_READ_WRITE,
                                RAB_WITHOUT_COHERENCY);
  sthorm_addRemapEntry(fabricBase + HOST_FC_OFFSET, P2012_CONF_FC_BASE, FC_SIZE);
  pr_info("RAB SLAVE: CONF_FC: source %08x (%08x) dest %08x size %08x\n",
          fabricBase + HOST_FC_OFFSET, fabricBaseForHades + HOST_FC_OFFSET, P2012_CONF_FC_BASE, FC_SIZE);

#ifndef HEVC_HADES_CUT1
  //disabled: we have only 1 cluster in Cannes2
  //for (i=0; i<P2012_MAX_FABRIC_CLUSTER_COUNT; i++) 
  {
      // Cluster entries
      if (clusterMask & (1<<i)) {
          // Cluster PERIPH
          hal2_configure_rab_entry( fabricBase + HOST_RAB_SLAVE_OFFSET,
                  index++,
                  fabricBaseForHades + HOST_CL_PERIPH_OFFSET(i),
                  fabricBaseForHades + HOST_CL_PERIPH_OFFSET(i) + CL_PERIPH_SIZE - 1,
                  P2012_GET_CONF_ENCPERIPH_BASE(i),
                  RAB_READ_WRITE,
                  RAB_WITHOUT_COHERENCY);
          sthorm_addRemapEntry(fabricBase + HOST_CL_PERIPH_OFFSET(i), P2012_GET_CONF_ENCPERIPH_BASE(i), CL_PERIPH_SIZE);
          pr_info("RAB SLAVE: CL_PERIPH: source %08x (%08x) dest %08x size %08x\n", fabricBase + HOST_CL_PERIPH_OFFSET(i),
                  fabricBaseForHades + HOST_CL_PERIPH_OFFSET(i), P2012_GET_CONF_ENCPERIPH_BASE(i), CL_PERIPH_SIZE);
#endif
          // Cluster DMEM
          hal2_configure_rab_entry( fabricBase + HOST_RAB_SLAVE_OFFSET,
                  index++,
                  fabricBaseForHades + HOST_CL_DMEM_OFFSET(i),
                  fabricBaseForHades + HOST_CL_DMEM_OFFSET(i) + CL_DMEM_SIZE - 1,
                  P2012_GET_CONF_ENCMEM_BASE(i),
                  RAB_READ_WRITE,
                  RAB_WITHOUT_COHERENCY);
          sthorm_addRemapEntry(fabricBase + HOST_CL_DMEM_OFFSET(i), P2012_GET_CONF_ENCMEM_BASE(i), CL_DMEM_SIZE);
          pr_info("RAB SLAVE: CONF_ENCMEM: source %08x (%08x) dest %08x size %08x\n",
                  fabricBase + HOST_CL_DMEM_OFFSET(i), fabricBaseForHades + HOST_CL_DMEM_OFFSET(i), P2012_GET_CONF_ENCMEM_BASE(i), CL_DMEM_SIZE);

#ifndef HEVC_HADES_CUT1
          // Cluster HWPE 0: Output Engine
          hal2_configure_rab_entry( fabricBase + HOST_RAB_SLAVE_OFFSET,
                  index++,
                  fabricBaseForHades + HOST_CL_HWPE_OUTE_OFFSET(i),
                  fabricBaseForHades + HOST_CL_HWPE_OUTE_OFFSET(i) + CL_HWPE_OUTE_SIZE - 1,
                  P2012_GET_CONF_HWPE_BASE(i, 0),         // output engine hwpe_id=0
                  RAB_READ_WRITE,
                  RAB_WITHOUT_COHERENCY);
          sthorm_addRemapEntry(fabricBase + HOST_CL_HWPE_OUTE_OFFSET(i), P2012_GET_CONF_HWPE_BASE(i, 0), CL_HWPE_OUTE_SIZE);
          pr_info("RAB SLAVE: CL_HWPE_OUT: source %08x (%08x) dest %08x size %08x\n", fabricBase + HOST_CL_HWPE_OUTE_OFFSET(i),
                  fabricBaseForHades + HOST_CL_HWPE_OUTE_OFFSET(i), P2012_GET_CONF_HWPE_BASE(i, 0), CL_HWPE_OUTE_SIZE);

          // Cluster HWPE 1: CTB Engine
          hal2_configure_rab_entry( fabricBase + HOST_RAB_SLAVE_OFFSET,
                  index++,
                  fabricBaseForHades + HOST_CL_HWPE_CTBE_OFFSET(i),
                  fabricBaseForHades + HOST_CL_HWPE_CTBE_OFFSET(i) + CL_HWPE_CTBE_SIZE - 1,
                  P2012_GET_CONF_HWPE_BASE(i, 1),
                  RAB_READ_WRITE,
                  RAB_WITHOUT_COHERENCY);
          sthorm_addRemapEntry(fabricBase + HOST_CL_HWPE_CTBE_OFFSET(i), P2012_GET_CONF_HWPE_BASE(i, 1), CL_HWPE_CTBE_SIZE);
          pr_info("RAB SLAVE: CL_HWPE_OUT: source %08x (%08x) dest %08x size %08x\n", fabricBase + HOST_CL_HWPE_CTBE_OFFSET(i),
                  fabricBaseForHades + HOST_CL_HWPE_CTBE_OFFSET(i), P2012_GET_CONF_HWPE_BASE(i, 0), CL_HWPE_CTBE_SIZE);
      }
  }
#endif

  // RAB activation
  hal2_activate_rab(fabricBase + HOST_RAB_SLAVE_OFFSET);

  if (nbEntries) {
    *nbEntries = index;
  }
}

// Activate master RABs
static inline void hal2_activate_rab_master(uint32_t hostBase)
{
  uint32_t fabricBase = hostBase + HADES_FABRIC_OFFSET;

  hal2_activate_rab(fabricBase + HOST_CIC_RAB_MASTER_OFFSET);
  hal2_activate_rab(fabricBase + HOST_DIC_RAB_MASTER_OFFSET);
  hal2_activate_rab(fabricBase + HOST_RAB_BACKDOOR_1_OFFSET);
}

// Configure RAB default entries from fabric to host
static inline void hal2_configure_rab_master_default(uint32_t hostBase, int *nbEntries, uint32_t hades_host_address, unsigned long host_host_base, unsigned long host_host_size) {
  int index = 0;
  uint32_t fabricBase = hostBase + HADES_FABRIC_OFFSET;


  /*pr_info("RAB_MASTER_DEFAULT:  source 0x%08x dest 0x%08lx size 0x%08lx\n",*/
  /*hades_host_address, host_host_base, host_host_size);*/

  // Preproc accessible from Hades
  // FIXME JLX: why ?
  /*
  hal2_configure_rab_entry( fabricBase + HOST_RAB_MASTER_OFFSET,
                            index++,
                            HADES_PREPROC_ADDRESS,
                            HADES_PREPROC_ADDRESS + PREPROC_SIZE*2 - 1,
                            hostBase + HOST_PREPROC_OFFSET,
                            RAB_READ_WRITE,
                            RAB_WITHOUT_COHERENCY);
  sthorm_addRemapEntry(hostBase + HOST_PREPROC_OFFSET, HADES_PREPROC_ADDRESS, PREPROC_SIZE*2);
  */

  // Host DDR accessible from Hades
   hal2_configure_rab_entry( fabricBase + HOST_CIC_RAB_MASTER_OFFSET,
                             index,
                             hades_host_address,
                             hades_host_address + host_host_size - 1,
                             host_host_base,
                             RAB_READ_WRITE,
                             RAB_WITHOUT_COHERENCY);

  hal2_configure_rab_entry( fabricBase + HOST_DIC_RAB_MASTER_OFFSET,
                            index,
                            hades_host_address,
                            hades_host_address + host_host_size - 1,
                            host_host_base,
                            RAB_READ_WRITE,
                            RAB_WITHOUT_COHERENCY);

  // Host DDR accessible from Hades cluster 1 Backdoor
  hal2_configure_rab_entry( fabricBase + HOST_RAB_BACKDOOR_1_OFFSET,
                            index,
                            hades_host_address,
                            hades_host_address + host_host_size - 1,
                            host_host_base,
                            RAB_READ_WRITE,
                            RAB_WITHOUT_COHERENCY );

  index++;
  sthorm_addRemapEntry(host_host_base, hades_host_address, host_host_size);

  // RAB activation
  // hal2_activate_rab(fabricBase + HOST_CIC_RAB_MASTER_OFFSET);
  // hal2_activate_rab(fabricBase + HOST_DIC_RAB_MASTER_OFFSET);
  // hal2_activate_rab(fabricBase + HOST_RAB_BACKDOOR_1_OFFSET);

  if (nbEntries) {
    *nbEntries = index;
  }
}

// Configure RAB for Fabric firmware
static inline void hal2_configure_rab_master_firmware(uint32_t hostBase, int* index, uint32_t hades_host_address, uint32_t host_host_address, uint32_t size) {
  uint32_t fabricBase = hostBase + HADES_FABRIC_OFFSET;

  /*pr_info("RAB_MASTER_FW:  source 0x%08x dest 0x%08x size 0x%08x\n",*/
  /*hades_host_address, host_host_address, size);*/

   hal2_configure_rab_entry( fabricBase + HOST_CIC_RAB_MASTER_OFFSET,
                             *index,
                             hades_host_address,
                             hades_host_address + size - 1,
                             host_host_address,
                             RAB_READ_WRITE,
                             RAB_WITHOUT_COHERENCY);

  hal2_configure_rab_entry( fabricBase + HOST_DIC_RAB_MASTER_OFFSET,
		  	    *index,
                            hades_host_address,
                            hades_host_address + size - 1,
                            host_host_address,
                            RAB_READ_WRITE,
                            RAB_WITHOUT_COHERENCY);

  hal2_configure_rab_entry( fabricBase + HOST_RAB_BACKDOOR_1_OFFSET,
		  	    *index,
			    hades_host_address,
			    hades_host_address + size - 1,
			    host_host_address,
			    RAB_READ_WRITE,
			    RAB_WITHOUT_COHERENCY);

  (*index)++;
  sthorm_addRemapEntry(host_host_address, hades_host_address, size);
}

#endif
