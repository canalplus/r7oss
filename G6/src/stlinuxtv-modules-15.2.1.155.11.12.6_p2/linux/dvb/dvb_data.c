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

Source file name : dvb_data.c

STLinuxTV object list handling code

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#include "dvb_data.h"

static struct list_head object_lists[MAX_IDX];

void initDataSets(void)
{
	int idx;
	for (idx = 0; idx < MAX_IDX; idx++) {
		INIT_LIST_HEAD(&object_lists[idx]);
	}
}

void addObjectToList(struct list_head *to_add, DVB_LIST_T list_idx)
{
	if (list_idx < MAX_IDX) {
		list_add_tail(to_add, &object_lists[list_idx]);
	}
}

void removeObjectFromList(struct list_head *to_del, DVB_LIST_T list_idx)
{
	list_del(to_del);
}

void removeObjectFromlistIdx(int idx, DVB_LIST_T list_idx)
{

}

void *getObjectFromlistIdx(int idx, DVB_LIST_T list_idx)
{
	struct list_head *pos;

	if (list_idx >= MAX_IDX) {
		return NULL;
	}

	list_for_each(pos, &object_lists[list_idx]) {
		switch (list_idx) {
		case DEMOD:
			{
				struct stm_dvb_demod_s *dvb_demod;
				dvb_demod =
				    list_entry(pos, struct stm_dvb_demod_s,
					       list);
				if (dvb_demod->demod_id == idx)
					return dvb_demod;

			}
			break;

		case LNB:
			{
				struct stm_dvb_lnb_s *dvb_lnb;
				dvb_lnb =
				    list_entry(pos, struct stm_dvb_lnb_s, list);
				if (dvb_lnb->lnb_id == idx)
					return dvb_lnb;

			}
			break;

		case DISEQC:
			{
				struct stm_dvb_diseqc_s *dvb_diseqc;
				dvb_diseqc =
				    list_entry(pos, struct stm_dvb_diseqc_s,
					       list);
				if (dvb_diseqc->diseqc_id == idx)
					return dvb_diseqc;

			}
			break;

		case RF_MATRIX:
			{
				struct stm_dvb_rf_matrix_s *dvb_rf_matrix;
				dvb_rf_matrix =
				    list_entry(pos, struct stm_dvb_rf_matrix_s,
					       list);
				if (dvb_rf_matrix->id == idx)
					return dvb_rf_matrix;

			}
			break;

		case DEMUX:
			{
				struct stm_dvb_demux_s *dvb_demux;
				dvb_demux =
				    list_entry(pos, struct stm_dvb_demux_s,
					       list);
				if (dvb_demux->demux_id == idx)
					return dvb_demux;
			}
			break;

#ifdef CONFIG_STLINUXTV_CRYPTO
		case CA:
			{
				struct stm_dvb_ca_s *dvb_ca;
				dvb_ca =
				    list_entry(pos, struct stm_dvb_ca_s, list);
				if (dvb_ca->ca_id == idx)
					return dvb_ca;
			}
			break;
#endif

		case IP:
			{
				struct stm_dvb_ip_s *dvb_ip;
				dvb_ip =
				    list_entry(pos, struct stm_dvb_ip_s, list);
				if (dvb_ip->ip_id == idx)
					return dvb_ip;
			}
			break;

		case IPFEC:
			{
				struct stm_dvb_ipfec_s *dvb_ipfec;
				dvb_ipfec =
					list_entry(pos, struct stm_dvb_ipfec_s, list);
				if (dvb_ipfec->ipfec_id == idx)
					return dvb_ipfec;
			}
			break;

		default:
			return NULL;
		}
	}
	return NULL;
}

/**
 * stm_dvb_object_get_by_index
 * This function returns the element from the list based on
 * the position in the list.
 */
void *stm_dvb_object_get_by_index(DVB_LIST_T list_id, int index)
{
	struct list_head *pos;
	void *obj;
	int count = 0;

	if (list_id >= MAX_IDX)
		return NULL;

	list_for_each(pos, &object_lists[list_id]) {
		switch (list_id) {
		case DEMOD:
			obj = list_entry(pos, struct stm_dvb_demod_s, list);
			if (index == count++)
				return obj;

			break;

		case LNB:
			obj = list_entry(pos, struct stm_dvb_lnb_s, list);
			if (index == count++)
				return obj;

			break;

		case DISEQC:
			obj = list_entry(pos, struct stm_dvb_diseqc_s, list);
			if (index == count++)
				return obj;

			break;

		case RF_MATRIX:
			obj = list_entry(pos, struct stm_dvb_rf_matrix_s, list);
			if (index == count++)
				return obj;

			break;

		case DEMUX:
			obj = list_entry(pos, struct stm_dvb_demux_s, list);
			if (index == count++)
				return obj;

			break;

#ifdef CONFIG_STLINUXTV_CRYPTO
		case CA:
			obj = list_entry(pos, struct stm_dvb_ca_s, list);
			if (index == count++)
				return obj;

			break;
#endif

		case IP:
			obj = list_entry(pos, struct stm_dvb_ip_s, list);
			if (index == count++)
				return obj;

			break;

		case IPFEC:
			obj = list_entry(pos, struct stm_dvb_ipfec_s, list);
			if (index == count++)
				return obj;

			break;

		default:
			return NULL;
		}
	}

	return NULL;
}
