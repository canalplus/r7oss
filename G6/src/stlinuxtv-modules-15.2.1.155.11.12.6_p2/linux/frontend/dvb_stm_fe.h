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

Source file name : dvb_stm_fe.h
Author :           Rahul.V

Header for dvb_stm_fe.c

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef DVB_STM_FE_H_
#define DVB_STM_FE_H_

#include "dvb_ipfe.h"

#include <linux/version.h>
#include <dvb_frontend.h>
#include <stm_fe.h>

struct dvb_stm_fe_demod_state {
	u32 dev_ver;
	struct i2c_adapter *i2c;
	const struct demod_config_s *demod_config;
	struct dvb_frontend frontend;
	u32 *verbose;		/* Cached module verbosity */
	stm_fe_demod_h demod_object;
	const struct diseqc_config_s *diseqc_config;
	stm_fe_diseqc_h diseqc_object;
	const struct rf_matrix_config_s *rf_matrix_config;
	stm_fe_rf_matrix_h rf_matrix_object;
	stm_event_subscription_h ev_subs;
	stm_event_subscription_entry_t evt_subs_entry;
	stm_fe_object_info_t *stinfo;
	stm_fe_demod_tx_std_t supported_stds;
};

struct dvb_stm_fe_lnb_state {
	u32 dev_ver;
	const struct lnb_config_s *lnb_config;
	const struct diseqc_config_s *diseqc_config;
	stm_fe_lnb_h lnb_object;
	stm_fe_diseqc_h diseqc_object;
};

struct dvb_stm_fe_ip_state {
	u32 dev_ver;
	const struct ip_config_s *ip_config;
	const struct ipfec_config_s *ipfec_config;
	struct dvb_ipfe ipfe;
	stm_fe_ip_h ip_object;
	stm_fe_ip_fec_h ipfec_object;
};

#define LNB_OBJ(fe) (((struct dvb_stm_fe_lnb_state *)fe->sec_priv)->lnb_object)
#define DMD_OBJ(fe) (((struct dvb_stm_fe_demod_state *)fe->demodulator_priv)->demod_object)
#define DSQ_OBJ(fe) (((struct dvb_stm_fe_demod_state *)fe->demodulator_priv)->diseqc_object)
#define RF_MATRIX_OBJ(fe) (((struct dvb_stm_fe_demod_state *)fe->demodulator_priv)->rf_matrix_object)
#define IP_OBJ(fe) (((struct dvb_stm_fe_ip_state *)(container_of(fe, struct dvb_stm_fe_ip_state, ipfe)))->ip_object)
#define IPFEC_OBJ(fe) (((struct dvb_stm_fe_ip_state *)(container_of(fe, struct dvb_stm_fe_ip_state, ipfe)))->ipfec_object)
//#define IPFEC_OBJ(fe) (((struct dvb_stm_fe_ip_state *)fe->ipfe_priv)->ipfec_object)

#define DEMOD_CHANNEL_INFO_COMMON_VALUE(std, info_s, member, value_p) \
{  \
        switch(std)  \
        {  \
                case STM_FE_DEMOD_TX_STD_DVBS:  \
                                                *value_p = info_s.u_channel.dvbs.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBS2:  \
                default:  \
                          *value_p = info_s.u_channel.dvbs2.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBC:  \
                                                *value_p = info_s.u_channel.dvbc.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBC2:  \
                                                 *value_p = info_s.u_channel.dvbc.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_AC:  \
                                                  *value_p = info_s.u_channel.j83ac.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_B:  \
                                                 *value_p = info_s.u_channel.j83b.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBT:  \
                                                *value_p = info_s.u_channel.dvbt.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBT2:  \
                                                 *value_p = info_s.u_channel.dvbt2.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_ATSC:  \
                                                *value_p = info_s.u_channel.atsc.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_ISDBT:  \
                                                 *value_p = info_s.u_channel.isdbt.member;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_PAL:  \
                case STM_FE_DEMOD_TX_STD_SECAM:  \
                case STM_FE_DEMOD_TX_STD_NTSC:  \
                                                *value_p = info_s.u_channel.analog.member;  \
                break;  \
        } \
}\

#define DEMOD_CHANNEL_INFO_BER_VALUE(std, info_s, value_p) \
{  \
        switch(std)  \
        {  \
                case STM_FE_DEMOD_TX_STD_DVBS:  \
                default: \
                         *value_p = info_s.u_channel.dvbs.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBC:  \
                                                *value_p = info_s.u_channel.dvbc.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBC2:  \
                                                 *value_p = info_s.u_channel.dvbc.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_AC:  \
                                                  *value_p = info_s.u_channel.j83ac.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_B:  \
                                                 *value_p = info_s.u_channel.j83b.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBT:  \
                                                *value_p = info_s.u_channel.dvbt.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBT2:  \
                                                 *value_p = info_s.u_channel.dvbt2.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_ATSC:  \
                                                *value_p = info_s.u_channel.atsc.ber;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_ISDBT:  \
                                                 *value_p = info_s.u_channel.isdbt.ber;  \
                break;  \
        } \
}\

#define DEMOD_CHANNEL_INFO_UCB_VALUE(std, info_s, value_p) \
{  \
        switch(std)  \
        {  \
                case STM_FE_DEMOD_TX_STD_DVBC:  \
                default: \
                         *value_p = info_s.u_channel.dvbc.ucb;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBC2:  \
                                                 *value_p = info_s.u_channel.dvbc.ucb;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_AC:  \
                                                  *value_p = info_s.u_channel.j83ac.ucb;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_J83_B:  \
                                                 *value_p = info_s.u_channel.j83b.ucb;  \
                break;  \
                case STM_FE_DEMOD_TX_STD_DVBT:  \
                         *value_p = info_s.u_channel.dvbt.ucb;  \
                break;  \
        } \
}

void dvb_to_fe_tune_params_qpsk(struct dtv_frontend_properties *dvb_tune,
				stm_fe_demod_tune_params_t * fe_tune,
				stm_fe_demod_tune_caps_t * tune_caps);
void dvb_to_fe_tune_params_ofdm(struct dtv_frontend_properties *dvb_tune,
				stm_fe_demod_tune_params_t * fe_tune,
				stm_fe_demod_tune_caps_t * tune_caps);
void dvb_to_fe_tune_params_qam(struct dtv_frontend_properties *dvb_tune,
			       stm_fe_demod_tune_params_t * fe_tune,
			       stm_fe_demod_tune_caps_t * tune_caps);
void dvb_to_fe_tune_params_vsb(struct dtv_frontend_properties *dvb_tune,
			       stm_fe_demod_tune_params_t * fe_tune,
			       stm_fe_demod_tune_caps_t * tune_caps);
void fe_to_dvb_tune_info_qpsk(struct dtv_frontend_properties *dvb_info,
			      stm_fe_demod_channel_info_t * fe_info);
void fe_to_dvb_tune_info_ofdm(struct dtv_frontend_properties *dvb_info,
			      stm_fe_demod_channel_info_t * fe_info);
void fe_to_dvb_tune_info_qam(struct dtv_frontend_properties *dvb_info,
			     stm_fe_demod_channel_info_t * fe_info);
void fe_to_dvb_tune_info_vsb(struct dtv_frontend_properties *dvb_info,
			     stm_fe_demod_channel_info_t * fe_info);

struct dvb_frontend *dvb_stm_fe_demod_attach(struct demod_config_s
					     *demod_config);
int dvb_stm_fe_demod_detach(struct dvb_frontend *fe);

struct dvb_frontend *dvb_stm_fe_lnb_attach(struct dvb_frontend *fe,
					   struct lnb_config_s *lnb_config);
int dvb_stm_fe_lnb_detach(struct dvb_frontend *fe);

struct dvb_frontend *dvb_stm_fe_diseqc_attach(struct dvb_frontend *fe, struct diseqc_config_s
					      *diseqc_config);
int dvb_stm_fe_diseqc_detach(struct dvb_frontend *fe);
struct dvb_frontend *dvb_stm_fe_rf_matrix_attach(struct dvb_frontend *fe,
				struct rf_matrix_config_s *rf_matrix_config);
int dvb_stm_fe_rf_matrix_detach(struct dvb_frontend *fe);

/* ST IPFE */
int dvb_stm_ipfe_attach(struct dvb_ipfe **dvb_ipfe, struct ip_config_s *ip_config);
void dvb_stm_ipfe_detach(struct dvb_ipfe *dvb_ipfe);
int dvb_stm_fe_ip_link(struct dvb_ipfe *fe, stm_fe_ip_h ip_object,
		       stm_fe_object_info_t * stinfo);

/* ST IPFE FEC */
int dvb_stm_fe_ipfec_attach(struct dvb_ipfe *ipfe,
					struct ipfec_config_s *fec_config);
void dvb_stm_fe_ipfec_detach(struct dvb_ipfe *ipfe);
int dvb_stm_fe_demod_link(struct dvb_frontend *fe, stm_fe_demod_h demod_object,
			  stm_fe_object_info_t * stinfo);
int dvb_stm_fe_lnb_link(struct dvb_frontend *fe, stm_fe_lnb_h lnb_object);
int dvb_stm_fe_diseqc_link(struct dvb_frontend *fe,
			   stm_fe_diseqc_h diseqc_object);
int dvb_stm_fe_rf_matrix_link(struct dvb_frontend *fe,
			   stm_fe_rf_matrix_h rf_matrix_object,
			   stm_fe_object_info_t *stinfo);
int dvb_stm_fe_ipfec_link(struct dvb_ipfe *ipfe,
			   stm_fe_ip_fec_h fec_object);
void fill_dvb_info(struct dvb_frontend_info *dvb_info,
		   stm_fe_demod_tune_caps_t * caps, stm_fe_demod_tx_std_t std);

void dvb_stm_fe_release(struct dvb_frontend *fe);

int dvb_stm_fe_init(struct dvb_frontend *fe);
int dvb_stm_fe_sleep(struct dvb_frontend *fe);

int dvb_stm_fe_write(struct dvb_frontend *fe, u8 * buf, int len);

/* if this is set, it overrides the default swzigzag */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
int dvb_stm_fe_tune(struct dvb_frontend *fe,
		    bool re_tune,
		    unsigned int mode_flags,
		    unsigned int *delay, fe_status_t * status);
#else
int dvb_stm_fe_tune(struct dvb_frontend *fe,
		    struct dvb_frontend_parameters *params,
		    unsigned int mode_flags,
		    unsigned int *delay, fe_status_t * status);
#endif
/* get frontend tuning algorithm from the module */
enum dvbfe_algo dvb_stm_fe_get_frontend_algo(struct dvb_frontend *fe);

int dvb_stm_fe_read_status(struct dvb_frontend *fe, fe_status_t * tune_status);
int dvb_stm_fe_read_ber(struct dvb_frontend *fe, u32 * ber_val);
int dvb_stm_fe_read_signal_strength(struct dvb_frontend *fe, u16 * strength);
int dvb_stm_fe_read_snr(struct dvb_frontend *fe, u16 * signal_snr);
int dvb_stm_fe_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks);

int dvb_stm_fe_diseqc_reset_overload(struct dvb_frontend *fe);
int dvb_stm_fe_diseqc_send_master_cmd(struct dvb_frontend *fe,
				      struct dvb_diseqc_master_cmd *cmd);
int dvb_stm_fe_diseqc_recv_slave_reply(struct dvb_frontend *fe,
				       struct dvb_diseqc_slave_reply *reply);
int dvb_stm_fe_diseqc_send_burst(struct dvb_frontend *fe,
				 fe_sec_mini_cmd_t minicmd);
int dvb_stm_fe_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone);
int dvb_stm_fe_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage);
int dvb_stm_fe_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg);
int dvb_stm_fe_dishnetwork_send_legacy_command(struct dvb_frontend *fe,
					       unsigned long cmd);
int dvb_stm_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable);
int dvb_stm_fe_ts_bus_ctrl(struct dvb_frontend *fe, int acquire);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
int dvb_stm_fe_set_rf_input_src(struct dvb_frontend *fe);
#endif
/* These callbacks are for devices that implement their own
 * tuning algorithms, rather than a simple swzigzag
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
enum dvbfe_search dvb_stm_fe_search(struct dvb_frontend *fe);
int dvb_stm_fe_track(struct dvb_frontend *fa);
#else
enum dvbfe_search dvb_stm_fe_search(struct dvb_frontend *fe,
				    struct dvb_frontend_parameters *p);
int dvb_stm_fe_track(struct dvb_frontend *fe,
		     struct dvb_frontend_parameters *p);
#endif
int dvb_stm_fe_ip_start(struct dvb_ipfe *fe);
int dvb_stm_fe_ip_fec_start(struct dvb_ipfe *fe);
int dvb_stm_fe_ip_stop(struct dvb_ipfe *fe);
int dvb_stm_fe_ip_fec_stop(struct dvb_ipfe *fe);
int dvb_stm_fe_ip_set_control(struct dvb_ipfe *fe,
			      fe_ip_control_t flag, void *args);
int dvb_stm_fe_ip_set_fec(struct dvb_ipfe *fe, fe_ip_fec_control_t flag,
			      void *args);
int dvb_stm_fe_ip_get_capability(struct dvb_ipfe *fe, void *args);
#endif /* DVB_STM_FE_H_ */
