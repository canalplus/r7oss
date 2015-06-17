/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_data.h

STLinuxTV object list handling code

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DVB_DATA_H
#define _DVB_DATA_H

#include "dvb_adaptation.h"

typedef enum DVB_LIST {
	DEMOD = 0,
	LNB,
	DISEQC,
	RF_MATRIX,
	DEMUX,
#ifdef CONFIG_STLINUXTV_CRYPTO
	CA,
#endif
	IP,
	IPFEC,
	MAX_IDX,
} DVB_LIST_T;

void initDataSets(void);
void addObjectToList(struct list_head *to_add, DVB_LIST_T list_idx);
void removeObjectFromList(struct list_head *to_del, DVB_LIST_T list_idx);
void *getObjectFromlistIdx(int idx, DVB_LIST_T list_idx);
void *stm_dvb_object_get_by_index(DVB_LIST_T list_id, int index);

#define GET_DEMOD_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_demod_s*)(getObjectFromlistIdx(_idx_, DEMOD))
#define GET_LNB_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_lnb_s*)(getObjectFromlistIdx(_idx_, LNB))
#define GET_DISEQC_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_diseqc_s*)(getObjectFromlistIdx(_idx_, DISEQC))
#define GET_RF_MATRIX_OBJECT(_name_, _idx_) (_name_ = \
	(struct stm_dvb_rf_matrix_s *)(getObjectFromlistIdx(_idx_, RF_MATRIX)))
#define GET_DEMUX_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_demux_s*)(getObjectFromlistIdx(_idx_, DEMUX))
#ifdef CONFIG_STLINUXTV_CRYPTO
#define GET_CA_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_ca_s*)(getObjectFromlistIdx(_idx_, CA))
#endif
#define GET_IP_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_ip_s*)(getObjectFromlistIdx(_idx_, IP))
#define GET_IPFEC_OBJECT(_name_, _idx_) _name_ = (struct stm_dvb_ipfec_s*)(getObjectFromlistIdx(_idx_, IPFEC))

#endif /* _DVB_DATA_H */
