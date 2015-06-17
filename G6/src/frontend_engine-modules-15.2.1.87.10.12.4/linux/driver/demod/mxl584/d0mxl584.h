/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : d0mxl584.h
Author :           Hemendra

Header for mxl584 LLA wrapper

Date        Modification                                    Name
----        ------------                                    --------
9-Apr-13   Created                                           HS

************************************************************************/
#ifndef _D0MXL584_H
#define _D0MXL584_H

#define MXL_HYDRA_NBREGS 2500
#define MXL_HYDRA_NBFIELDS 7800

#define FILL_CUSTOM_SEARCH_PARAMS_S1_S2(priv, std_in, s_params, params) {\
	s_params.SearchRange = 5000000; \
	s_params.SearchAlgo = FE_SAT_COLD_START; \
	/*s_params.Path = priv->demod_id;*/ \
	s_params.Modulation = FE_SAT_MOD_UNKNOWN; \
\
	if (std_in == STM_FE_DEMOD_TX_STD_DVBS) \
		s_params.Standard = FE_SAT_SEARCH_DVBS1; \
	else if (std_in == STM_FE_DEMOD_TX_STD_DVBS2) \
		s_params.Standard = FE_SAT_SEARCH_DVBS2; \
\
	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2) { \
		s_params.Frequency = params->freq; \
		s_params.SymbolRate = params->sr; \
		s_params.PunctureRate = \
			stmfe_set_sat_fec_rate(params->fec); \
		s_params.IQ_Inversion = FE_SAT_IQ_AUTO;\
	} \
\
	if (std_in == STM_FE_DEMOD_TX_STD_S1_S2) { \
		s_params.Standard = FE_SAT_AUTO_SEARCH; \
		s_params.IQ_Inversion = FE_SAT_IQ_AUTO; \
	} \
}

int mxl584_init(struct stm_fe_demod_s *priv);
int mxl584_tune(struct stm_fe_demod_s *priv, bool *lock);
int mxl584_scan(struct stm_fe_demod_s *priv, bool *lock);
int mxl584_status(struct stm_fe_demod_s *priv, bool *locked);
int mxl584_tracking(struct stm_fe_demod_s *priv);
int mxl584_unlock(struct stm_fe_demod_s *priv);
int stm_fe_mxl584_attach(struct stm_fe_demod_s *demod_priv);
int stm_fe_mxl584_detach(struct stm_fe_demod_s *demod_priv);
int mxl584_term(struct stm_fe_demod_s *demod_priv);
int mxl584_abort(struct stm_fe_demod_s *priv, bool abort);
int mxl584_standby(struct stm_fe_demod_s *priv, bool standby);
int mxl584_ts_pid_start(stm_object_h demod_object, stm_object_h
			demux_object, uint32_t pid);

int mxl584_ts_pid_stop(stm_object_h demod_object, stm_object_h
			demux_object, uint32_t pid);




#endif /* _D0MXL584_H */
