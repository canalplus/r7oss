/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.



This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.



This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.



You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
#ifndef _TMBSL_TDA182I2_INSTANCE_CUSTOM_H
#define _TMBSL_TDA182I2_INSTANCE_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Types and defines:							      */
/*============================================================================*/

/* Driver settings version definition */
/* SW Settings Customer Number */
#define TMBSL_TDA182I2_SETTINGS_CUSTOMER_NUM	0
/* SW Settings Project Number  */
#define TMBSL_TDA182I2_SETTINGS_PROJECT_NUM	3
/* SW Settings Major Version   */
#define TMBSL_TDA182I2_SETTINGS_MAJOR_VER	0
/* SW Settings Minor Version   */
#define TMBSL_TDA182I2_SETTINGS_MINOR_VER	3

/* Custom Driver Instance Parameters: (Path 0) */
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MASTER	\
	/* Current Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* Minimum Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* RF */	       \
	0, \
	/* Standard mode */		  \
	tmTDA182I2_DVBT_8MHz, \
	/* Master */   \
	True, \
	/* LT_Enable */    \
	0, \
	/* PSM_AGC1 */	      \
	1, \
	/* AGC1_6_15dB */	     \
	0, \

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MASTER_DIGITAL	\
	/* Current Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* Minimum Power state */		\
	tmTDA182I2_PowerStandbyWithLNAOnAndWithXtalOnAndSynthe, \
	/* RF */	       \
	0, \
	/* Standard mode */		  \
	tmTDA182I2_DVBT_8MHz, \
	/* Master */   \
	True, \
	/* LT_Enable */    \
	1, \
	/* PSM_AGC1 */	      \
	0, \
	/* AGC1_6_15dB */	     \
	1, \

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_SLAVE	\
	/* Current Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* Minimum Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* RF */	       \
	0, \
	/* Standard mode */		  \
	tmTDA182I2_DVBT_8MHz, \
	/* Master */   \
	False, \
	/* LT_Enable */    \
	0, \
	/* PSM_AGC1 */	      \
	1, \
	/* AGC1_6_15dB */	     \
	0, \

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_SLAVE_DIGITAL	\
	/* Current Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* Minimum Power state */		\
	tmTDA182I2_PowerStandbyWithXtalOn, \
	/* RF */	       \
	0, \
	/* Standard mode */		  \
	tmTDA182I2_DVBT_8MHz, \
	/* Master */   \
	False, \
	/* LT_Enable */    \
	0, \
	/* PSM_AGC1 */	      \
	1, \
	/* AGC1_6_15dB */	     \
	0, \

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL	\
	{	/* Std_Array */		/* DVB-T 6MHz */ \
		/* IF */	       \
		3600000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_7MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_min_8pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_4MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_4_094ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_2_047ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_94dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Enabled \
	},		     \
	{	/* DVB-T 7MHz */	       \
		/* IF */	       \
		4200000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_min_8pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_85MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_4_094ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_2_047ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_94dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Enabled \
	},		     \
	{	/* DVB-T 8MHz */	       \
		/* IF modified by ST for ACI; 4.5MHz instead of 4.8MHz*/ \
		4500000, \
		/* CF_Offset */		      \
		0, \
		/* LPF modified by ST for ACI; 8MHz instead of 9MHz*/	\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_min_8pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_85MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_4_094ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_2_047ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_94dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Enabled \
	},		     \
	{	/* QAM 6MHz */		     \
		/* IF */	       \
		3600000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_6MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_min_8pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		True, \
		/* LTO_STO_immune */ \
		True, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* QAM 8MHz */		     \
		/* IF */	       \
		5000000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_9MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_min_8pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_85MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		True, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* ISDBT 6MHz */	       \
		/* IF */	       \
		3250000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_6MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_6Vpp_min_10_3_19_7dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_4MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* ATSC 6MHz */		      \
		/* IF */	       \
		3250000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_6MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_6Vpp_min_10_3_19_7dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_4MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d100_u94dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_104dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_104dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_104dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d112_u107dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d112_u107dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_3_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_3_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* DMB-T 8MHz */	       \
		/* IF */	       \
		4000000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \

#define  TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG	\
	{	/* Analog M/N */	       \
		/* IF */	       \
		5400000, \
		/* CF_Offset */		      \
		1750000, \
		/* LPF */		\
		tmTDA182I2_LPF_6MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog B */		     \
		/* IF */	       \
		6400000, \
		/* CF_Offset */		      \
		2250000, \
		/* LPF */		\
		tmTDA182I2_LPF_7MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band*/		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog G/H */	       \
		/* IF */	       \
		6750000, \
		/* CF_Offset */		      \
		2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Enabled \
	},		     \
	{	/* Analog I */		     \
		/* IF */	       \
		7250000, \
		/* CF_Offset */		      \
		2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP Low_band */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP High_band*/	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog D/K */	       \
		/* IF */	       \
		6850000, \
		/* CF_Offset */		      \
		2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog L */		     \
		/* IF */	       \
		6750000, \
		/* CF_Offset */		      \
		2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog L' */		      \
		/* IF */	       \
		1250000, \
		/* CF_Offset */		      \
		-2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Analog FM Radio */		    \
		/* IF */	       \
		1250000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_1_5MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_0_85MHz, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x02, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* Blind Scanning copy of PAL-I */		 \
		/* IF */	       \
		7250000, \
		/* CF_Offset */		      \
		2750000, \
		/* LPF */		\
		tmTDA182I2_LPF_8MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_0_7Vpp_min_9_21dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Disabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Disabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_96dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d105_u100dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d105_u100dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Enabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Disabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_0_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Disabled, \
		/* GSK : settings V2.0.0  */		   \
		0x01, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Disabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Frozen, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	},		     \
	{	/* ScanXpress */	       \
		/* IF */	       \
		5000000, \
		/* CF_Offset */		      \
		0, \
		/* LPF */		\
		tmTDA182I2_LPF_9MHz, \
		/* LPF_Offset */	       \
		tmTDA182I2_LPFOffset_0pc, \
		/* IF_Gain */		    \
		tmTDA182I2_IF_AGC_Gain_1Vpp_min_6_24dB, \
		/* IF_Notch */		     \
		tmTDA182I2_IF_Notch_Enabled, \
		/* IF_HPF */		   \
		tmTDA182I2_IF_HPF_Disabled, \
		/* DC_Notch */		     \
		tmTDA182I2_DC_Notch_Enabled, \
		/* AGC1_LNA_TOP */		 \
		tmTDA182I2_AGC1_LNA_TOP_d95_u89dBuV, \
		/* AGC1 DN Time Constant */ \
		tmTDA182I2_AGC1_Do_step_8_188ms, \
		/* AGC2_RF_Attenuator_TOP */		   \
		tmTDA182I2_AGC2_RF_Attenuator_TOP_d90_u84dBuV, \
		/* AGC2 DN Time Constant */ \
		tmTDA182I2_AGC2_Do_step_8_188ms, \
		/* AGC3_RF_AGC_TOP_Low_band */		     \
		tmTDA182I2_AGC3_RF_AGC_TOP_100dBuV, \
		/* AGC3_RF_AGC_TOP_High_band */		      \
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* start freq for LTE settings */  \
		754000000, \
		/* AGC3_RF_AGC_TOP_LTE */		\
		tmTDA182I2_AGC3_RF_AGC_TOP_102dBuV, \
		/* AGC4_IR_Mixer_TOP */		      \
		tmTDA182I2_AGC4_IR_Mixer_TOP_d110_u105dBuV, \
		/* AGC5_IF_AGC_TOP */		    \
		tmTDA182I2_AGC5_IF_AGC_TOP_d110_u105dBuV, \
		/* AGC5_Detector_HPF */		      \
		tmTDA182I2_AGC5_Detector_HPF_Disabled, \
		/* AGC3_Adapt */	       \
		tmTDA182I2_AGC3_Adapt_Enabled, \
		/* AGC3_Adapt_TOP  */	  \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC3_Adapt_TOP LTE */     \
		tmTDA182I2_AGC3_Adapt_TOP_2_Step, \
		/* AGC5_Atten_3dB */		   \
		tmTDA182I2_AGC5_Atten_3dB_Enabled, \
		/* GSK : settings V2.0.0  */		   \
		0x0e, \
		/* H3H5_VHF_Filter6 */		     \
		tmTDA182I2_H3H5_VHF_Filter6_Enabled, \
		/* LPF_Gain */		     \
		tmTDA182I2_LPF_Gain_Free, \
		/* AGC1_freeze */ \
		False, \
		/* LTO_STO_immune */ \
		False, \
		/* PD Underload  */ \
		tmTDA182I2_PD_Udld_Disabled \
	}
/* Custom Driver Instance Parameters: (Path 1) */

/******************************************************************/
/* Mode selection for PATH0					  */
/******************************************************************/

#ifdef TMBSL_TDA18272

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH0 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_MASTER
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG

#else

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH0 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_MASTER_DIGITAL
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL

#endif

/******************************************************************/
/* Mode selection for PATH1					  */
/******************************************************************/

#ifdef TMBSL_TDA18272

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH1 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_SLAVE
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL
#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH1 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG

#else

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH1 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_SLAVE_DIGITAL

#define TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1 \
				TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL

#endif

/******************************************************************/
/* End of Mode selection					  */
/******************************************************************/

#ifdef __cplusplus
}
#endif
#endif				/* _TMBSL_TDA182I2_INSTANCE_CUSTOM_H */
