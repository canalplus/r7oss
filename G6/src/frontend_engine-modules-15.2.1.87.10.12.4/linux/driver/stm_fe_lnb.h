/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_lnb.h
Author :           Rahul.V

Header for stm_fe_lnb.c

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V
11-April-12 Added lnb_config_t in stm_fe_lnb_s		    Ankur Tyagi

************************************************************************/
#ifndef _STM_FE_LNB_H
#define _STM_FE_LNB_H

enum lnb_status_t {
	LNB_STATUS_ON,
	LNB_STATUS_OFF,
	LNB_STATUS_SHORT_CIRCUIT,
	LNB_STATUS_OVER_TEMPERATURE,	/*Supported by LNB21
						-> OTF=1 when Temp >140 */
	LNB_STATUS_SHORTCIRCUIT_OVERTEMPERATURE
};

enum lnbtxmode_t {
	LNB_RX = 0,		/*receiving mode: should be used in Diseqc */
	LNB_TX,			/*transmitting mode: should be used in Diseqc */
	LNB_TTX_NONE		/* For controllers without TTX modes */
};

struct lnb_config_t {
	enum lnb_status_t status;	/*LNB ON_OFF */
	stm_fe_lnb_polarization_t polarization;	/*Hor or Ver polarisation */
	stm_fe_lnb_tone_state_t tonestate;	/*Tone on or OFF */
	enum lnbtxmode_t txmode;
	void *lnbfield;
};

struct stm_fe_lnb_s;

struct lnb_ops_s {
	int (*init)(struct stm_fe_lnb_s *priv);
	int (*setconfig)(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
	int (*getconfig)(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
	int (*term)(struct stm_fe_lnb_s *priv);
	int (*detach)(struct stm_fe_lnb_s *priv);
	int (*suspend)(struct stm_fe_lnb_s *priv);
	int (*resume)(struct stm_fe_lnb_s *priv);
};

struct stm_fe_lnb_s {
	struct list_head list;
	struct list_head *lnb_list_p;
	char lnb_name[32];
	uint32_t lnb_id;
	bool prev_lnb_status_on;
	stm_fe_demod_h demod_h;
	struct lnb_config_s *config;
	STCHIP_Handle_t lnb_h;
	fe_i2c_adapter lnb_i2c;
	struct lnb_ops_s *ops;
	bool bypass_control;
	bool rpm_suspended;
	struct platform_device *lnb_data;
	struct lnb_config_t lnbparams;
	bool dt_enable;
};
int stm_fe_lnb_probe(struct platform_device *pdev);
int stm_fe_lnb_remove(struct platform_device *pdev);

#ifdef CONFIG_PM
int32_t stm_fe_lnb_suspend(struct device *dev);
int32_t stm_fe_lnb_resume(struct device *dev);
int32_t stm_fe_lnb_restore(struct device *dev);
int32_t stm_fe_lnb_freeze(struct device *dev);
#endif

struct stm_fe_lnb_s *lnb_from_name(const char *name);
struct stm_fe_lnb_s *lnb_from_id(uint32_t id);

#endif /* _STM_FE_LNB_H */
