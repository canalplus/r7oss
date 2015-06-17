/***********************************************************************
 *
 * File: linux/kernel/drivers/media/video/stmvout_ctrl.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
 ***********************************************************************/

#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>

#include <stm_display.h>

#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "linux/stmvout.h"
#include "stmedia.h"
/* #include "stm_v4l2.h" */
#include <linux/dvb/dvb_v4l2_export.h>

typedef enum {
	VOUT_XVP_CONFIG=1,
	VOUT_FMD_CONFIG,
	VOUT_DEI_CONFIG,
	VOUT_MADI_CONFIG,
	VOUT_ACC_CONFIG,
	VOUT_ACM_CONFIG,
	VOUT_AFM_CONFIG,
	VOUT_CCS_CONFIG,
	VOUT_SHARPNESS_CONFIG,
	VOUT_TNR_CONFIG,
	VOUT_IQI_PEAKING_DATA,
	VOUT_IQI_LE_DATA,
	VOUT_IQI_CTI_DATA,
} vout_tuning_data_ctrl_e;

/* these controls are invoked from capture device though tunnelled mode */
static u32 tunnelled_ctrls[] = {
	V4L2_CID_STM_ASPECT_RATIO_CONV_MODE,
	V4L2_CID_DV_STM_TX_ASPECT_RATIO,
	V4L2_CID_STM_INPUT_WINDOW_MODE,
	V4L2_CID_STM_OUTPUT_WINDOW_MODE,
	V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY,
};

static void stmvout_ctrl_cfg_fill(struct v4l2_ctrl_config *const cfg,
			       const struct v4l2_ctrl_ops *ops,
			       const char *const name,
			       s32 minimum,
			       s32 maximum,
			       s32 step,
			       s32 default_value,
			       enum v4l2_ctrl_type type, u32 flags)
{
	cfg->ops = ops;
	cfg->name = name;
	cfg->min = minimum;
	cfg->max = maximum;
	cfg->step = step;
	cfg->def = default_value;
	cfg->type = type;
	cfg->flags = flags;
}

static stm_plane_feature_t
stmvout_get_associated_feature(stm_display_plane_tuning_data_control_t
			       tuning_data_control)
{
	stm_plane_feature_t feature;

	switch (tuning_data_control) {
	case PLANE_CTRL_VIDEO_MADI_TUNING_DATA: 	/*  TODO:check if this is obsolete */
		feature = PLANE_FEAT_VIDEO_MADI;
		break;

	case PLANE_CTRL_VIDEO_FMD_TUNING_DATA:	 	/* TODO:check if this is obsolete */
		feature = PLANE_FEAT_VIDEO_FMD;
		break;

	case PLANE_CTRL_VIDEO_XVP_TUNING_DATA:		/* TODO:check if this is obsolete */
		feature = PLANE_FEAT_VIDEO_XVP;
		break;
	case PLANE_CTRL_VIDEO_ACC_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_ACC;
		break;
	case PLANE_CTRL_VIDEO_ACM_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_ACM;
		break;
	case PLANE_CTRL_VIDEO_AFM_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_AFM;
		break;
	case PLANE_CTRL_VIDEO_CCS_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_CCS;
		break;
	case PLANE_CTRL_VIDEO_SHARPNESS_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_SHARPNESS;
		break;
	case PLANE_CTRL_VIDEO_TNR_TUNING_DATA:
		feature = PLANE_FEAT_VIDEO_TNR;
		break;
	case PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA:
	    feature = PLANE_FEAT_VIDEO_IQI_PEAKING;
	    break;	
	case PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA:
	    feature = PLANE_FEAT_VIDEO_IQI_LE;
	    break;	
	case PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA:
	    feature = PLANE_FEAT_VIDEO_IQI_CTI;
	    break;		    	    
	default:
		feature = 0;
	}
	return feature;
}

static uint32_t
stmvout_get_control_payload_size(stm_display_plane_tuning_data_control_t
				 tuning_data_control, uint32_t revision)
{
	uint32_t size;

	switch (tuning_data_control) {
	case PLANE_CTRL_VIDEO_MADI_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_madi_tuning_data_t);
		else
			size = 0;
		break;

	case PLANE_CTRL_VIDEO_FMD_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_fmd_tuning_data_t);
		else
			size = 0;
		break;

	case PLANE_CTRL_VIDEO_XVP_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_xvp_tuning_data_t);
		else
			size = 0;
		break;
    case  PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_iqi_peaking_tuning_data_t);
		else
			size = 0;
		break;  
    case  PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_iqi_le_tuning_data_t);
		else
			size = 0;
		break;    
    case  PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA:
		if (revision == 1)
			size = sizeof(stm_iqi_cti_tuning_data_t);
		else
			size = 0;
		break;   				  
	default:
		size = 0;
	}
	return size;
}

static stm_tuning_data_t *stmvout_alloc_and_fill_tuning_data(stm_display_plane_h
							     p,
							     stm_display_plane_tuning_data_control_t
							     tuning_data_control,
							     void *data)
{
	uint32_t total_size, revision;
	stm_tuning_data_t *tuning_data = NULL;
	uint32_t data_size;

	if (stm_display_plane_get_tuning_data_revision
	    (p, tuning_data_control, &revision) == 0) {
	   	    
		data_size =
		    stmvout_get_control_payload_size(tuning_data_control,
						     revision);
						     				 
						     				    
        debug_msg(" data_size: %d\n", data_size); 						     				     
		total_size = sizeof(stm_tuning_data_t) + data_size;
		tuning_data = (stm_tuning_data_t *) kmalloc(total_size, GFP_KERNEL);
		tuning_data->size = total_size;
		tuning_data->feature = stmvout_get_associated_feature(tuning_data_control);
		tuning_data->revision = revision;
		/* Now write data at the payload position located just after the structure */
		if (data)
			memcpy(GET_PAYLOAD_POINTER(tuning_data), data,
			       data_size);
	}

	return tuning_data;
}

static int stmvout_set_tuning_data_control_value(stm_display_plane_h p,
						 stm_display_plane_tuning_data_control_t
						 ctrl, void *newVal)
{
	int ret = -1;
	stm_tuning_data_t *ctrltuningdata = NULL;

	ctrltuningdata = stmvout_alloc_and_fill_tuning_data(p, ctrl, newVal);
	if (ctrltuningdata) {
	    debug_msg(" stmvout_set_tuning_data_control_value, ctrltuningdata: %d\n", ctrltuningdata); 	
		ret =
		    stm_display_plane_set_tuning_data_control(p, ctrl,
							      ctrltuningdata);
		kfree(ctrltuningdata);
	}

	return ret;
}

static inline int stmvout_get_tuning_data_control_value(stm_display_plane_h p,
							stm_display_plane_tuning_data_control_t
							ctrl, void *ctrlVal)
{
	int ret = -1;
	stm_tuning_data_t *ctrltuningdata = NULL;

	/* Allocate a temporary tuning data that will be used for the call */
	ctrltuningdata = stmvout_alloc_and_fill_tuning_data(p, ctrl, NULL);
	if (ctrltuningdata) {
	    debug_msg(" stmvout_get_tuning_data_control_value, ctrltuningdata: %d\n", ctrltuningdata); 	
		ret =
		    stm_display_plane_get_tuning_data_control(p, ctrl,
							      ctrltuningdata);
		/* Copy back only the data payload from the data tuning structure into the passed pointer */
		if (ret == 0) {

			memcpy(ctrlVal,
			       (uint8_t *) GET_PAYLOAD_POINTER(ctrltuningdata),
			       GET_PAYLOAD_SIZE(ctrltuningdata));
		}

		/* Now free the temporary tuning data */
		kfree(ctrltuningdata);
	}
	return ret;
}

static int stmvout_set_tuning_data_control(stm_display_plane_h p,
					   vout_tuning_data_ctrl_e ctrl_type,
					   void *newVal_p)
{
	int ret = -1;

	switch (ctrl_type) {

	case VOUT_DEI_CONFIG:
		{
			stm_madi_tuning_data_t dei_data;
			dei_data.config =
			    *(enum PlaneCtrlDEIConfiguration *)newVal_p;
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_MADI_TUNING_DATA,
								  &dei_data);
			break;
		}

	case VOUT_XVP_CONFIG:
		{
			stm_xvp_tuning_data_t xvp_data;
			xvp_data.config =
			    *(enum PlaneCtrlxVPConfiguration *)newVal_p;
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_XVP_TUNING_DATA,
								  &xvp_data);
			break;
		}

	case VOUT_FMD_CONFIG:
		{
			stm_fmd_tuning_data_t fmd_data;
			fmd_data.params = *(stm_fmd_params_t *) newVal_p;
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_FMD_TUNING_DATA,
								  &fmd_data);
			break;
		}
    case VOUT_IQI_PEAKING_DATA:
    {
			stm_iqi_peaking_tuning_data_t iqi_peaking_data;
			iqi_peaking_data = *(stm_iqi_peaking_tuning_data_t *) newVal_p;
			debug_msg("stmvout_set_tuning_data_control: VOUT_IQI_PEAKING_DATA\n"); 	
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA,
								  &iqi_peaking_data);
			break;       
    }
    case VOUT_IQI_LE_DATA:
    {
			stm_iqi_le_tuning_data_t iqi_le_data;
			iqi_le_data = *(stm_iqi_le_tuning_data_t *) newVal_p;
			debug_msg("stmvout_set_tuning_data_control: VOUT_IQI_LE_DATA\n"); 	
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA,
								  &iqi_le_data);
			break;       
    }    
    case VOUT_IQI_CTI_DATA:
    {
			stm_iqi_cti_tuning_data_t iqi_cti_data;
			iqi_cti_data = *(stm_iqi_cti_tuning_data_t *) newVal_p;
			debug_msg("stmvout_set_tuning_data_control: VOUT_IQI_CTI_DATA\n"); 	
			ret =
			    stmvout_set_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA,
								  &iqi_cti_data);
			break;       
    }        
	default:
		ret = -1;
	}

	return ret;
}

static int stmvout_get_tuning_data_control(stm_display_plane_h p,
					   vout_tuning_data_ctrl_e ctrl_type,
					   uint32_t * ctrlVal_p)
{
	int ret = -1;

	switch (ctrl_type) {

	case VOUT_DEI_CONFIG:
		{
			stm_madi_tuning_data_t dei_data;
			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_MADI_TUNING_DATA,
								  &dei_data);
			*(enum PlaneCtrlDEIConfiguration *)ctrlVal_p =
			    dei_data.config;
			break;
		}

	case VOUT_XVP_CONFIG:
		{
			stm_xvp_tuning_data_t xvp_data;
			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_XVP_TUNING_DATA,
								  &xvp_data);
			*(enum PlaneCtrlxVPConfiguration *)ctrlVal_p =
			    xvp_data.config;
			break;
		}

	case VOUT_FMD_CONFIG:
		{
		    
			stm_fmd_tuning_data_t fmd_data;

			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_FMD_TUNING_DATA,
								  &fmd_data);
			*(stm_fmd_params_t *) ctrlVal_p = fmd_data.params;
			break;
		}
    case VOUT_IQI_PEAKING_DATA:
    {
			
			
			stm_iqi_peaking_tuning_data_t  iqi_peaking_data;


			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA,
								  &iqi_peaking_data);
			*(stm_iqi_peaking_tuning_data_t *) ctrlVal_p = iqi_peaking_data;
			break;    
    }
    case VOUT_IQI_LE_DATA:
    {
			
			
			stm_iqi_le_tuning_data_t  iqi_le_data;

			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA,
								  &iqi_le_data);
			*(stm_iqi_le_tuning_data_t *) ctrlVal_p = iqi_le_data;
			break;    
    }   
    case VOUT_IQI_CTI_DATA:
    {
			
			
			stm_iqi_cti_tuning_data_t  iqi_cti_data;

			ret =
			    stmvout_get_tuning_data_control_value(p,
								  PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA,
								  &iqi_cti_data);
			*(stm_iqi_cti_tuning_data_t *) ctrlVal_p = iqi_cti_data;
			break;    
    }         

	default:
		ret = -1;
	}

	return ret;
}

static unsigned long stmvout_get_ctrl_name(int id)
{

	switch (id) {
	case V4L2_CID_BRIGHTNESS:
		return PLANE_CTRL_BRIGHTNESS;
	case V4L2_CID_CONTRAST:
		return PLANE_CTRL_CONTRAST;
	case V4L2_CID_SATURATION:
		return PLANE_CTRL_SATURATION;
	case V4L2_CID_HUE:
		return PLANE_CTRL_TINT;
	case V4L2_CID_SHARPNESS:
		return PLANE_CTRL_VIDEO_SHARPNESS_VALUE;
	case V4L2_CID_STM_XVP_SET_CONFIG:
	case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
	case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
		return VOUT_XVP_CONFIG;
	case V4L2_CID_STM_DEI_SET_MODE:
	case V4L2_CID_STM_DEI_SET_CTRLREG:
		return VOUT_DEI_CONFIG;
	case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
		return PLANE_CTRL_VIDEO_FMD_STATE;
	case V4L2_CID_STM_PLANE_HIDE_MODE:
		return PLANE_CTRL_HIDE_MODE_POLICY;
	case V4L2_CID_STM_CKEY_ENABLE:
	case V4L2_CID_STM_CKEY_FORMAT:
	case V4L2_CID_STM_CKEY_R_CR_MODE:
	case V4L2_CID_STM_CKEY_G_Y_MODE:
	case V4L2_CID_STM_CKEY_B_CB_MODE:
	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
	case V4L2_CID_STM_CKEY_RCr_MINVAL ... V4L2_CID_STM_CKEY_BCb_MAXVAL:
	case V4L2_CID_STM_Z_ORDER:
	case V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY:
	case V4L2_CID_STM_VQ_SHARPNESS:
	case V4L2_CID_STM_VQ_MADI:
	case V4L2_CID_STM_VQ_AFM:
	case V4L2_CID_STM_VQ_TNR:
	case V4L2_CID_STM_VQ_CCS:
	case V4L2_CID_STM_VQ_ACC:
	case V4L2_CID_STM_VQ_ACM:
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:

	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
    case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:	
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:	
    case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:	      
	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:	    
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:  


		return 1000;
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE:
		return PLANE_CTRL_FLICKER_FILTER_STATE;
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:
		return PLANE_CTRL_FLICKER_FILTER_MODE;
	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		return PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE;
	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
		return OUTPUT_CTRL_DISPLAY_ASPECT_RATIO;
	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		return PLANE_CTRL_INPUT_WINDOW_MODE;
	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		return PLANE_CTRL_OUTPUT_WINDOW_MODE;
		/*
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:
	    return PLANE_CTRL_VIDEO_IQI_PEAKING_STATE;
		*/
	default:
		return 0;
	}
}

static unsigned long stmvout_get_state_name(int id)
{
	switch (id) {
	case V4L2_CID_STM_VQ_SHARPNESS:
		return PLANE_CTRL_VIDEO_SHARPNESS_STATE;
	case V4L2_CID_STM_VQ_MADI:
		return PLANE_CTRL_VIDEO_MADI_STATE;
	case V4L2_CID_STM_VQ_AFM:
		return PLANE_CTRL_VIDEO_AFM_STATE;
	case V4L2_CID_STM_VQ_TNR:
		return  PLANE_CTRL_VIDEO_TNR_STATE;
	case V4L2_CID_STM_VQ_CCS:
		return PLANE_CTRL_VIDEO_CCS_STATE;
	case V4L2_CID_STM_VQ_ACC:
		return PLANE_CTRL_VIDEO_ACC_STATE;
	case V4L2_CID_STM_VQ_ACM:
		return PLANE_CTRL_VIDEO_ACM_STATE;
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:

	    return PLANE_CTRL_VIDEO_IQI_PEAKING_STATE;    	
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:
	    return PLANE_CTRL_VIDEO_IQI_LE_STATE;		    
	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
	    return PLANE_CTRL_VIDEO_IQI_CTI_STATE;		   


	default:
		return 0;
	}
}

static unsigned long stmvout_get_tuning_data_name(int id)
{
	switch (id) {
	case V4L2_CID_STM_VQ_SHARPNESS:
		return PLANE_CTRL_VIDEO_SHARPNESS_TUNING_DATA;
	case V4L2_CID_STM_VQ_MADI:
		return PLANE_CTRL_VIDEO_MADI_TUNING_DATA;
	case V4L2_CID_STM_VQ_AFM:
		return PLANE_CTRL_VIDEO_AFM_TUNING_DATA;
	case V4L2_CID_STM_VQ_TNR:
		return  PLANE_CTRL_VIDEO_TNR_TUNING_DATA;
	case V4L2_CID_STM_VQ_CCS:
		return PLANE_CTRL_VIDEO_CCS_TUNING_DATA;
	case V4L2_CID_STM_VQ_ACC:
		return PLANE_CTRL_VIDEO_ACC_TUNING_DATA;
	case V4L2_CID_STM_VQ_ACM:
		return PLANE_CTRL_VIDEO_ACM_TUNING_DATA;
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:

	case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:

	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:

	    return PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA;   
    case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
        return PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA;	          
	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:	    
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:           
        return PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA;	
   

	default:
		return 0;
	}
}



void stmvout_get_controls_from_core(struct _stvout_device *const device)
{
#define GET_STATE(ctrl, old_state, new_state) \
    if (0 != stmvout_get_tuning_data_control (device->hPlane,                \
                                            ctrl,                          \
                                            (uint32_t *) &old_state))      \
      memset (&old_state, 0, sizeof (old_state));                          \
    new_state = old_state;

	if (0 != stm_display_plane_get_control(device->hPlane,
					       PLANE_CTRL_COLOR_KEY,
					       (uint32_t *) & device->state.
					       new_ckey))
		memset(&device->state.old_ckey, 0,
		       sizeof(device->state.old_ckey));
	device->state.new_ckey = device->state.old_ckey;


	GET_STATE(VOUT_FMD_CONFIG, device->state.old_fmd, device->state.new_fmd);

    GET_STATE(VOUT_IQI_PEAKING_DATA, device->state.old_peaking, device->state.new_peaking);
    debug_msg("init PEAKING values: \n");
    debug_msg(" - undershoot: %d\n", device->state.old_peaking.undershoot); 
	debug_msg(" - overshoot: %d\n", device->state.old_peaking.overshoot);
	debug_msg(" - coring_mode: %d\n", device->state.old_peaking.coring_mode);
	debug_msg(" - coring_level: %d\n", device->state.old_peaking.coring_level);
	debug_msg(" - vertical_peaking: %d\n", device->state.old_peaking.vertical_peaking);
	debug_msg(" - clipping_mode: %d\n", device->state.old_peaking.clipping_mode);
	debug_msg(" - bandpass_filter_centerfreq: %d\n", device->state.old_peaking.bandpass_filter_centerfreq);
	debug_msg(" - highpass_filter_cutofffreq: %d\n", device->state.old_peaking.highpass_filter_cutofffreq);
	debug_msg(" - bandpassgain: %d\n", device->state.old_peaking.bandpassgain);
	debug_msg(" - highpassgain: %d\n", device->state.old_peaking.highpassgain);
	debug_msg(" - ver_gain: %d\n", device->state.old_peaking.ver_gain);    
    
    GET_STATE(VOUT_IQI_LE_DATA, device->state.old_le, device->state.new_le);   
    debug_msg("init LE values: \n"); 
	debug_msg(" - fixed_curve: %d\n", device->state.old_le.fixed_curve);
	debug_msg(" - csc_gain: %d\n", device->state.old_le.csc_gain);   
    debug_msg(" - fixed_curve_params.BlackStretchLimitPoint : %d\n", device->state.old_le.fixed_curve_params.BlackStretchLimitPoint); 
    debug_msg(" - fixed_curve_params.WhiteStretchInflexionPoint : %d\n", device->state.old_le.fixed_curve_params.WhiteStretchInflexionPoint);
	debug_msg(" - fixed_curve_params.WhiteStretchLimitPoint : %d\n", device->state.old_le.fixed_curve_params.WhiteStretchLimitPoint);
	debug_msg(" - fixed_curve_params.BlackStretchGain: %d\n", device->state.old_le.fixed_curve_params.BlackStretchGain);
	debug_msg(" - fixed_curve_params.WhiteStretchGain: %d\n", device->state.old_le.fixed_curve_params.WhiteStretchGain);


    GET_STATE(VOUT_IQI_CTI_DATA, device->state.old_cti, device->state.new_cti);    
    debug_msg("init CTI values: \n"); 
	debug_msg(" - strength1: %d\n", device->state.old_cti.strength1);
	debug_msg(" - strength2: %d\n", device->state.old_cti.strength2);  
	
	device->state.new_cti_preset = device->state.old_cti_preset;
	device->state.new_le_preset = device->state.old_le_preset;
	device->state.new_peaking_preset = device->state.old_peaking_preset;


#undef GET_STATE
}

static const char *const *stmvout_ctrl_get_menu(u32 id)
{
	static const char *const xvp_config[PCxVPC_COUNT] = {
		[PCxVPC_BYPASS] = "bypass",
		[PCxVPC_FILMGRAIN] = "filmgrain",
		[PCxVPC_TNR] = "tnr",
		[PCxVPC_TNR_BYPASS] = "tnr (bypass)",
		[PCxVPC_TNR_NLEOVER] = "tnr (NLE override)"
	};
	static const char *const dei_mode[PCDEIC_COUNT] = {
		[PCDEIC_3DMOTION] = "enable (3D)",
		[PCDEIC_DISABLED] = "disable",
		[PCDEIC_MEDIAN] = "enable (median)"
	};
	static const char *const plane_hide_mode[] = {
		[VCSPHM_MIXER_PULLSDATA] = "mixer pulls data",
		[VCSPHM_MIXER_DISABLE] = "fully disabled"
	};
	static const char *const colorkey_enable[] = {
		[VCSCEF_DISABLED] = "disable",
		[VCSCEF_ENABLED] = "enable on next VSync",
		[VCSCEF_ACTIVATE_BUFFER] = "disable",
		[VCSCEF_ACTIVATE_BUFFER | VCSCEF_ENABLED] =
		    "enable on next buffer"
	};
	static const char *const colorkey_format[] = {
		[VCSCFM_RGB] = "RGB",
		[VCSCFM_CrYCb] = "CrYCb"
	};
	static const char *const colorkey_mode[] = {
		[VCSCCCM_DISABLED] = "ignore",
		[VCSCCCM_ENABLED] = "enable",
		[VCSCCCM_INVERSE & ~VCSCCCM_ENABLED] = "ignore",
		[VCSCCCM_INVERSE] = "inverse"
	};
	static const char *const sharpness_state[] = {
		[VCSISCM_OFF] = "off",
		[VCSISCM_ON] = "on",
		[VCSISCM_ST_SOFT] = "soft" ,
		[VCSISCM_ST_MEDIUM] = "medium",
		[VCSISCM_ST_STRONG] = "strong",
		[VCSISCM_DEMO_RIGHT] = "demo right",
		[VCSISCM_DEMO_LEFT] = "demo left"
	};

	static const char *const aspect_ratio[] = {
		"Aspect ratio 16:9",
		"Aspect ratio 4:3",
		"Aspect ratio 14:9",
		NULL
	};
	static const char *const aspect_ratio_conv[] = {
		"Aspect ratio conv letter box",
		"Aspect ratio conv pan & scan",
		"Aspect ratio conv combined",
		"Aspect ration conv ignore",
		NULL
	};
	static const char *const peaking_preset[] = {
		[VCSISCM_OFF] = "off",
		[VCSISCM_ON] = "on",
		[VCSISCM_ST_SOFT] = "soft" ,
		[VCSISCM_ST_MEDIUM] = "medium",

		[VCSISCM_ST_STRONG] = "strong",	    
	};
	

	static const char *const peaking_overshoot[] = {
		[VCSIOF_100] = "100",
		[VCSIOF_075] = "75",
		[VCSIOF_050] = "50",
		[VCSIOF_025] = "25"

	};	

	static const char *const peaking_undershoot[] = {
		[VCSIUF_100] = "100",
		[VCSIUF_075] = "75",
		[VCSIUF_050] = "50",
		[VCSIUF_025] = "25"

	};		

	static const char *const peaking_clip_strength[] = {
		[VCSISTRENGTH_NONE] = "none",
		[VCSISTRENGTH_WEAK] = "weak",
		[VCSISTRENGTH_STRONG] = "strong"
	};

	static const char *const peaking_freq[] = {
		[VCSIPFFREQ_0_15_FD2] = "0.15fs/2",
		[VCSIPFFREQ_0_18_FD2] = "0.18fs/2",
		[VCSIPFFREQ_0_22_FD2] = "0.22fs/2",
		[VCSIPFFREQ_0_26_FD2] = "0.26fs/2",
		[VCSIPFFREQ_0_30_FD2] = "0.30fs/2",
		[VCSIPFFREQ_0_33_FD2] = "0.33fs/2",
		[VCSIPFFREQ_0_37_FD2] = "0.37fs/2",
		[VCSIPFFREQ_0_40_FD2] = "0.40fs/2",
		[VCSIPFFREQ_0_44_FD2] = "0.44fs/2",
		[VCSIPFFREQ_0_48_FD2] = "0.48fs/2",
		[VCSIPFFREQ_0_51_FD2] = "0.51fs/2",
		[VCSIPFFREQ_0_55_FD2] = "0.55fs/2",
		[VCSIPFFREQ_0_58_FD2] = "0.58fs/2",
		[VCSIPFFREQ_0_63_FD2] = "0.63fs/2"

	};	
	static const char *const le_preset[] = {
		[VCSISCM_OFF] = "off",
		[VCSISCM_ON] = "on",
		[VCSISCM_ST_SOFT] = "soft" ,
		[VCSISCM_ST_MEDIUM] = "medium",
		[VCSISCM_ST_STRONG] = "strong",	    
	};
	static const char *const cti_preset[] = {
		[VCSISCM_OFF] = "off",
		[VCSISCM_ON] = "on",
		[VCSISCM_ST_SOFT] = "soft" ,
		[VCSISCM_ST_MEDIUM] = "medium",
		[VCSISCM_ST_STRONG] = "strong",	    
	};

	static const char *const cti_strength[] = {
		[VCSICS_NONE] = "none",
		[VCSICS_MIN] = "minimum",
		[VCSICS_MEDIUM] = "medium",
		[VCSICS_STRONG] = "strong"
 	};
	static const char * const stmvout_flicker_filter_mode_menu[] = {
		"Simple",
		"Adaptive",
		NULL,
	};


	switch (id) {

	case V4L2_CID_STM_XVP_SET_CONFIG:
		return xvp_config;
	case V4L2_CID_STM_DEI_SET_MODE:
		return dei_mode;
	case V4L2_CID_STM_PLANE_HIDE_MODE:
		return plane_hide_mode;
	case V4L2_CID_STM_CKEY_ENABLE:
		return colorkey_enable;
	case V4L2_CID_STM_CKEY_FORMAT:
		return colorkey_format;
	case V4L2_CID_STM_CKEY_R_CR_MODE:
	case V4L2_CID_STM_CKEY_G_Y_MODE:
	case V4L2_CID_STM_CKEY_B_CB_MODE:
		return colorkey_mode;
	case V4L2_CID_STM_VQ_SHARPNESS:
		return sharpness_state;
	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
		return aspect_ratio;
	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		return aspect_ratio_conv;
		
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:
		return stmvout_flicker_filter_mode_menu;		
		
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:

	    return peaking_preset;	
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
	    return peaking_undershoot;	  
	case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
		return peaking_overshoot;	   

	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
		return peaking_clip_strength;
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:

		return peaking_freq;			
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:
	    return le_preset;			    
	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
	    return cti_preset;		    		
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
		return cti_strength;

	}

	return NULL;
}

static int
stmvout_write_state(struct v4l2_ctrl *ctrl,
		struct _stvout_device_state *const state)
{
	switch (ctrl->id) {
	case V4L2_CID_STM_CKEY_ENABLE:
		state->new_ckey.flags |= SCKCF_ENABLE;
		state->new_ckey.enable = ctrl->val;
		break;

	case V4L2_CID_STM_CKEY_FORMAT:
		state->new_ckey.flags |= SCKCF_FORMAT;
		state->new_ckey.format = ctrl->val;
		break;

	case V4L2_CID_STM_CKEY_R_CR_MODE:
		state->new_ckey.flags |= SCKCF_R_INFO;
		state->new_ckey.r_info = ctrl->val;
		break;

	case V4L2_CID_STM_CKEY_G_Y_MODE:
		state->new_ckey.flags |= SCKCF_G_INFO;
		state->new_ckey.g_info = ctrl->val;
		break;

	case V4L2_CID_STM_CKEY_B_CB_MODE:
		state->new_ckey.flags |= SCKCF_B_INFO;
		state->new_ckey.b_info = ctrl->val;
		break;

	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
		state->new_ckey.flags |= SCKCF_MINVAL;
		state->new_ckey.minval = ctrl->val;
		break;

	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
		state->new_ckey.flags |= SCKCF_MAXVAL;
		state->new_ckey.maxval = ctrl->val;
		break;

	case V4L2_CID_STM_CKEY_RCr_MINVAL:
	case V4L2_CID_STM_CKEY_GY_MINVAL:
	case V4L2_CID_STM_CKEY_BCb_MINVAL:
		{
			int shift =
			    16 -
			    ((ctrl->id - V4L2_CID_STM_CKEY_RCr_MINVAL) * 4);
			state->new_ckey.flags |= SCKCF_MINVAL;
			state->new_ckey.minval &= ~(0x000000ff << shift);
			state->new_ckey.minval |=
			    ((ctrl->val & 0xff) << shift);
		}
		break;

	case V4L2_CID_STM_CKEY_RCr_MAXVAL:
	case V4L2_CID_STM_CKEY_GY_MAXVAL:
	case V4L2_CID_STM_CKEY_BCb_MAXVAL:
		{
			int shift =
			    16 -
			    ((ctrl->id - V4L2_CID_STM_CKEY_RCr_MAXVAL) * 4);
			state->new_ckey.flags |= SCKCF_MAXVAL;
			state->new_ckey.maxval &= ~(0x000000ff << shift);
			state->new_ckey.maxval |=
			    ((ctrl->val & 0xff) << shift);
		}
		break;

		/* FMD */
	case V4L2_CID_STM_FMD_T_MOV:
		state->new_fmd.t_mov = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_T_NUM_MOV_PIX:
		state->new_fmd.t_num_mov_pix = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_T_REPEAT:
		state->new_fmd.t_repeat = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_T_SCENE:
		state->new_fmd.t_scene = (u32) ctrl->val;
		break;

	case V4L2_CID_STM_FMD_COUNT_MISS:
		state->new_fmd.count_miss = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_COUNT_STILL:
		state->new_fmd.count_still = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_T_NOISE:
		state->new_fmd.t_noise = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_K_CFD1:
		state->new_fmd.k_cfd1 = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_K_CFD2:
		state->new_fmd.k_cfd2 = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_K_CFD3:
		state->new_fmd.k_cfd3 = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_K_SCENE:
		state->new_fmd.k_scene = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_FMD_D_SCENE:
		state->new_fmd.d_scene = (u32) ctrl->val;
		break;


/* peaking */
    case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:
        state->new_peaking_preset = (u32) ctrl->val;
        break;    
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
		state->new_peaking.undershoot = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
		state->new_peaking.overshoot = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
		state->new_peaking.coring_mode = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
		state->new_peaking.coring_level = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
		state->new_peaking.vertical_peaking = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
		state->new_peaking.clipping_mode = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
		state->new_peaking.bandpass_filter_centerfreq =
		    (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
		state->new_peaking.highpass_filter_cutofffreq =
		    (u32) ctrl->val;

		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
		/* -6.0 ... 12.0dB */
		{
			static const enum IQIPeakingHorVertGain core_gains[] = {
				IQIPHVG_N6_0DB, IQIPHVG_N5_5DB, IQIPHVG_N5_0DB,

				    IQIPHVG_N4_5DB,
				IQIPHVG_N4_0DB, IQIPHVG_N3_5DB, IQIPHVG_N3_0DB,
				    IQIPHVG_N2_5DB,
				IQIPHVG_N2_0DB, IQIPHVG_N1_5DB, IQIPHVG_N1_0DB,
				    IQIPHVG_N0_5DB,
				IQIPHVG_0DB, IQIPHVG_P0_5DB, IQIPHVG_P1_0DB,
				    IQIPHVG_P1_5DB,
				IQIPHVG_P2_0DB, IQIPHVG_P2_5DB, IQIPHVG_P3_0DB,
				    IQIPHVG_P3_5DB,
				IQIPHVG_P4_0DB, IQIPHVG_P4_5DB, IQIPHVG_P5_0DB,
				    IQIPHVG_P5_5DB,
				IQIPHVG_P6_0DB, IQIPHVG_P6_5DB, IQIPHVG_P7_0DB,
				    IQIPHVG_P7_5DB,
				IQIPHVG_P8_0DB, IQIPHVG_P8_5DB, IQIPHVG_P9_0DB,
				    IQIPHVG_P9_5DB,
				IQIPHVG_P10_0DB, IQIPHVG_P10_5DB,
				    IQIPHVG_P11_0DB, IQIPHVG_P11_5DB,

				IQIPHVG_P12_0DB
			};
			s32 idx = (ctrl->val / 5) + 12;
			if (ctrl->id ==

			    V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN)
				state->new_peaking.bandpassgain =
				    core_gains[idx];
			else if (ctrl->id ==
				 V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN)
				state->new_peaking.highpassgain =
				    core_gains[idx];
			else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN)
				state->new_peaking.ver_gain =
				    core_gains[idx];
		}
		break;	
/* LE */
    case V4L2_CID_STM_IQI_EXT_LE_PRESET:
        state->new_le_preset = (u32) ctrl->val;
        break;   
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:
		state->new_le.fixed_curve = ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:
		state->new_le.fixed_curve_params.
		    BlackStretchInflexionPoint = (u16) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
		state->new_le.fixed_curve_params.BlackStretchLimitPoint = (u16) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:
		state->new_le.fixed_curve_params.
		    WhiteStretchInflexionPoint = (u16) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
		state->new_le.fixed_curve_params.WhiteStretchLimitPoint = (u16) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
		state->new_le.fixed_curve_params.BlackStretchGain = (u8) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
		state->new_le.fixed_curve_params.WhiteStretchGain = (u8) ctrl->val;
		break;
/* CTI */
    case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
        state->new_cti_preset = (u32) ctrl->val;
        break;   
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
		state->new_cti.strength1 = (u32) ctrl->val;
		break;
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
		state->new_cti.strength2 = (u32) ctrl->val;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int
stmvout_read_state(struct v4l2_ctrl *ctrl,
		   const struct _stvout_device_state *const state)
{
	switch (ctrl->id) {
	case V4L2_CID_STM_CKEY_ENABLE:
		ctrl->val = state->old_ckey.enable;
		break;
	case V4L2_CID_STM_CKEY_FORMAT:
		ctrl->val = state->old_ckey.format;
		break;
	case V4L2_CID_STM_CKEY_R_CR_MODE:
		ctrl->val = state->old_ckey.r_info;
		break;
	case V4L2_CID_STM_CKEY_G_Y_MODE:
		ctrl->val = state->old_ckey.g_info;
		break;
	case V4L2_CID_STM_CKEY_B_CB_MODE:
		ctrl->val = state->old_ckey.b_info;
		break;
	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
		ctrl->val = state->old_ckey.minval;
		break;
	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
		ctrl->val = state->old_ckey.maxval;
		break;
	case V4L2_CID_STM_CKEY_RCr_MINVAL:
		ctrl->val = (state->old_ckey.minval >> 16) & 0xff;
		break;
	case V4L2_CID_STM_CKEY_RCr_MAXVAL:
		ctrl->val = (state->old_ckey.maxval >> 16) & 0xff;
		break;
	case V4L2_CID_STM_CKEY_GY_MINVAL:
		ctrl->val = (state->old_ckey.minval >> 8) & 0xff;
		break;
	case V4L2_CID_STM_CKEY_GY_MAXVAL:
		ctrl->val = (state->old_ckey.maxval >> 8) & 0xff;
		break;
	case V4L2_CID_STM_CKEY_BCb_MINVAL:
		ctrl->val = (state->old_ckey.minval >> 0) & 0xff;
		break;
	case V4L2_CID_STM_CKEY_BCb_MAXVAL:
		ctrl->val = (state->old_ckey.maxval >> 0) & 0xff;
		break;

		/* FMD */
	case V4L2_CID_STM_FMD_T_MOV:
		ctrl->val = state->old_fmd.t_mov;
		break;
	case V4L2_CID_STM_FMD_T_NUM_MOV_PIX:
		ctrl->val = state->old_fmd.t_num_mov_pix;
		break;
	case V4L2_CID_STM_FMD_T_REPEAT:
		ctrl->val = state->old_fmd.t_repeat;
		break;
	case V4L2_CID_STM_FMD_T_SCENE:
		ctrl->val = state->old_fmd.t_scene;
		break;

	case V4L2_CID_STM_FMD_COUNT_MISS:
		ctrl->val = state->old_fmd.count_miss;
		break;
	case V4L2_CID_STM_FMD_COUNT_STILL:
		ctrl->val = state->old_fmd.count_still;
		break;
	case V4L2_CID_STM_FMD_T_NOISE:
		ctrl->val = state->old_fmd.t_noise;
		break;
	case V4L2_CID_STM_FMD_K_CFD1:
		ctrl->val = state->old_fmd.k_cfd1;
		break;
	case V4L2_CID_STM_FMD_K_CFD2:
		ctrl->val = state->old_fmd.k_cfd2;
		break;
	case V4L2_CID_STM_FMD_K_CFD3:
		ctrl->val = state->old_fmd.k_cfd3;
		break;
	case V4L2_CID_STM_FMD_K_SCENE:
		ctrl->val = state->old_fmd.k_scene;
		break;
	case V4L2_CID_STM_FMD_D_SCENE:
		ctrl->val = state->old_fmd.d_scene;
		break;
	/* peaking */
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:
		ctrl->val = state->old_peaking_preset;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
		ctrl->val = state->old_peaking.undershoot;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
		ctrl->val = state->old_peaking.overshoot;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
		ctrl->val = state->old_peaking.coring_mode;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
		ctrl->val = state->old_peaking.coring_level;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
		ctrl->val = state->old_peaking.vertical_peaking;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
		ctrl->val = state->old_peaking.clipping_mode;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
		ctrl->val = state->old_peaking.bandpass_filter_centerfreq;
		break;
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
		ctrl->val = state->old_peaking.highpass_filter_cutofffreq;
		break;

	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:	/* -6.0 ... 12.0dB */
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:	/* -6.0 ... 12.0dB */
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:	/* -6.0 ... 12.0dB */
		{
			static const s32 gains[] = {
				-60, -55, -50, -45, -40, -35, -30, -25, -20,
				-15, -10, -5, 0, 5, 10,
				15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70,
				75, 80, 85, 90, 95,
				100, 105, 110, 115, 120
			};
			if (ctrl->id ==
					V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN)
				ctrl->val =
					gains[state->old_peaking.bandpassgain];
			else if (ctrl->id ==
					V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN)
				ctrl->val =
					gains[state->old_peaking.highpassgain];
			else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN)
				ctrl->val =
					gains[state->old_peaking.ver_gain];
		}
		break;


/* LE*/
    case V4L2_CID_STM_IQI_EXT_LE_PRESET:
    	ctrl->val = state->old_le_preset;
        break;
	case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:
		ctrl->val = state->old_le.csc_gain;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:
		ctrl->val = state->old_le.fixed_curve;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:
		ctrl->val =
		    state->old_le.fixed_curve_params.
		    BlackStretchInflexionPoint;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
		ctrl->val =
		    state->old_le.fixed_curve_params.BlackStretchLimitPoint;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:
		ctrl->val =
		    state->old_le.fixed_curve_params.
		    WhiteStretchInflexionPoint;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
		ctrl->val =
		    state->old_le.fixed_curve_params.WhiteStretchLimitPoint;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
		ctrl->val =
		    state->old_le.fixed_curve_params.BlackStretchGain;
		break;
	case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
		ctrl->val =
		    state->old_le.fixed_curve_params.WhiteStretchGain;
		break;
/* CTI */
    case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
    	ctrl->val = state->old_cti_preset;
        break;
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
		ctrl->val = state->old_cti.strength1;
		break;
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
		ctrl->val = state->old_cti.strength2;
		break;			
			
		/* CE not supported */
	default:
		return -EINVAL;
	}

	return 0;
}

static int
stmvout_apply_state(struct _stvout_device *device,
		    struct _stvout_device_state *const state)
{
	int ret = 0;

	if (device->hPlane) {

#define CHECK_STATE(ctrl, old_state, new_state) \
    if (memcmp (&state->old_state, &state->new_state, sizeof (state->old_state))) { \
      ret = stmvout_set_tuning_data_control(device->hPlane,                          \
                                           ctrl,                                    \
                                           &state->new_state);      \
      if (ret < 0)                                                                  \
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;                    \
      state->old_state = state->new_state;                                          \
    }
		/* color key */
		if (memcmp
		    (&state->old_ckey, &state->new_ckey,
		     sizeof(state->old_ckey))) {
			/* if state is now disabled, or state changed from activate_plane to
			   activate_buffer, clear the setting on the plane. */
			if (state->new_ckey.flags & SCKCF_ENABLE
			    && (!(state->new_ckey.enable & VCSCEF_ENABLED)
				||
				((state->new_ckey.
				  enable & VCSCEF_ACTIVATE_BUFFER)
				 && !(state->old_ckey.
				      enable & VCSCEF_ACTIVATE_BUFFER)))) {
				stm_color_key_config_t ckey;

				memset(&ckey, 0, sizeof(ckey));
				ckey.flags = SCKCF_ALL;
				ret =
				    stm_display_plane_set_control(device->
								  hPlane,
								  PLANE_CTRL_COLOR_KEY,
								  (unsigned
								   long)&ckey);
				if (ret)
					return signal_pending(current) ?
					    -ERESTARTSYS : -EBUSY;
			} else
			    if (!
				(state->new_ckey.
				 enable & VCSCEF_ACTIVATE_BUFFER)) {
				/* otherwise just set it (if not per buffer) */
				ret =
				    stm_display_plane_set_control(device->
								  hPlane,
								  PLANE_CTRL_COLOR_KEY,
								  (unsigned
								   long)&state->
								  new_ckey);
				if (ret)
					return signal_pending(current) ?
					    -ERESTARTSYS : -EBUSY;
			}
			state->old_ckey = state->new_ckey;
		}

		CHECK_STATE(VOUT_FMD_CONFIG, old_fmd, new_fmd);
		/* CE not supported */
#undef CHECK_STATE
	}

	return ret;
}

static int stmvout_s_ctrl(struct v4l2_ctrl *ctrl)
{
	uint32_t ctrlname, statename, tuningdataname;
	int ret;
	stm_display_plane_control_range_t scaler_range;
	struct _stvout_device *pDev = container_of(ctrl->handler,
					struct _stvout_device, ctrl_handler);
	BUG_ON(pDev == NULL);
	
	

	ctrlname = stmvout_get_ctrl_name(ctrl->id);
	if (ctrlname == 0) {
		err_msg("stmvout_get_ctrl_name()  Failed. returned ret : %d\n", ctrlname);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_STM_XVP_SET_CONFIG:
	case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
	case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
	case V4L2_CID_STM_DEI_SET_MODE:
	case V4L2_CID_STM_DEI_SET_CTRLREG:
		if (!pDev->hPlane)
			return -EINVAL;

		ret = stmvout_set_tuning_data_control(pDev->hPlane,
						      ctrlname,
						      &(ctrl->val));
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;
		break;

	case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
	case V4L2_CID_STM_PLANE_HIDE_MODE:
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE:
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:
		if (!pDev->hPlane)
			return -EINVAL;

		ret = stm_display_plane_set_control(pDev->hPlane,
						    ctrlname, ctrl->val);
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;
		break;

	case V4L2_CID_STM_Z_ORDER:
	{
		stm_display_output_h hOutput;
		if (!pDev->hPlane)
			return -EINVAL;

		/* get the output display ... */
		if ((hOutput =
		     stmvout_get_output_display_handle(pDev)) == NULL) {
			err_msg("V4L2 setting Z_ORDER while the device has no output!\n");
			return -EINVAL;
		}

		ret = stm_display_plane_set_depth(pDev->hPlane, hOutput,
						  ctrl->val, 1);

		if (ret < 0)
			return signal_pending(current) ? -ERESTARTSYS :
			    -EBUSY;
	}
		break;

	case V4L2_CID_STM_CKEY_ENABLE:
	case V4L2_CID_STM_CKEY_FORMAT:
	case V4L2_CID_STM_CKEY_R_CR_MODE:
	case V4L2_CID_STM_CKEY_G_Y_MODE:
	case V4L2_CID_STM_CKEY_B_CB_MODE:
	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
	case V4L2_CID_STM_CKEY_RCr_MINVAL ... V4L2_CID_STM_CKEY_BCb_MAXVAL:
	case V4L2_CID_STM_FMD_FIRST ... V4L2_CID_STM_FMD_LAST:
	{
		struct _stvout_device_state state = pDev->state;
		ret = stmvout_write_state(ctrl, &state);
		if (!ret) {
			ret = stmvout_apply_state(pDev, &state);
			pDev->state = state;
		}
	}
		break;
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}
		ret = stm_display_plane_get_control_range(pDev->hPlane,
							  ctrlname,
							  &scaler_range);
		if (ret) {
			err_msg("Get Control Range Failed in s_ctrl - %d. Exiting", ret);
			return ret;
		}
		debug_msg
		    ("Scaler range values : Min : %d, Max : %d Default : %d Step : %d",
		     scaler_range.min_val, scaler_range.max_val,
		     scaler_range.default_val, scaler_range.step);
		if (ctrl->val > scaler_range.max_val
		    || ctrl->val < scaler_range.min_val)
			return -EINVAL;
		ret =
		    stm_display_plane_set_control(pDev->hPlane, ctrlname,
						  ctrl->val);
		if (ret < 0) {
			err_msg("stm_display_plane_set_control()  Failed. returned ret : %d\n", ret);
			return ret;
		}
		/* Currently the stmfb is operating in sync mode. This currently returns error but
		   the sync operation seems to work */
		ret = stm_display_plane_apply_sync_controls(pDev->hPlane);
		if(ret < 0) {
			err_msg( "stm_display_plane_apply_sync_controls Failed\n");
			/* do not return here - the api returns error eventhough it desired has effect*/
		}

		break;
	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		ret = stm_display_plane_set_control(pDev->hPlane,
				PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE,
				ctrl->val);

		if (ret != 0) {
			err_msg("Cannot set PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;

		break;
	case V4L2_CID_STM_VQ_MCDI:
	case V4L2_CID_STM_VQ_AFM:
	case V4L2_CID_STM_VQ_TNR:
	case V4L2_CID_STM_VQ_CCS:
	case V4L2_CID_STM_VQ_ACC:
	case V4L2_CID_STM_VQ_ACM:
	case V4L2_CID_STM_VQ_SHARPNESS:
	{
		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}

		/* set tuning */
		tuningdataname = stmvout_get_tuning_data_name(ctrl->id);
		if (tuningdataname == 0)
			return -EINVAL;
		ret = stm_display_plane_set_tuning_data_control(pDev->hPlane, tuningdataname, NULL);
		if (ret < 0) {
			err_msg("stm_display_plane_set_tuning_data_control()  Failed. returned ret : %d\n", ret);
			return ret;
		}

		/* set state */
		statename = stmvout_get_state_name(ctrl->id);
		if (statename == 0){
			err_msg("stmvout_get_state_name()  Failed. returned ret : %d\n", statename);
			return -EINVAL;
		}
		ret = stm_display_plane_set_control(pDev->hPlane, statename,ctrl->val);
		if (ret < 0) {
			err_msg("stm_display_plane_set_control()  Failed. returned ret : %d\n", ret);
			return ret;
		}

		/* fvdp apply in hw */
		ret = stm_display_plane_apply_sync_controls(pDev->hPlane);
		if(ret < 0) {
			err_msg("stm_display_plane_apply_sync_controls Failed\n");
			/* do not return here - the api returns error eventhough it desired has effect*/
		}

		break;
	}
	
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:
	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
    case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET: /* possible values: ON, OFF, SOFT, MEDIUM, STRONG */
    {
        struct _stvout_device_state state = pDev->state;
    
		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}
		/* set state */
		statename = stmvout_get_state_name(ctrl->id);
		if (statename == 0){
			err_msg("stmvout_get_state_name()  Failed. returned ret : %d\n", statename);
			return -EINVAL;
		}
		ret = stmvout_write_state(ctrl, &state);	

		if(!ret) { 
		    ret = stm_display_plane_set_control(pDev->hPlane, statename,ctrl->val);
		    if (ret < 0) {
			    err_msg("stm_display_plane_set_control()  Failed. returned ret : %d\n", ret);
			    return ret;
		    }
		    
		    if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_PRESET) { 
		        state.old_peaking_preset = state.new_peaking_preset;
		        stmvout_get_tuning_data_control(pDev->hPlane,  VOUT_IQI_PEAKING_DATA, (uint32_t *)&state.old_peaking);
		    }     
		    
		    if (ctrl->id == V4L2_CID_STM_IQI_EXT_LE_PRESET) { 
		         state.old_le_preset = state.new_le_preset;
		         stmvout_get_tuning_data_control(pDev->hPlane,  VOUT_IQI_LE_DATA, (uint32_t *)&state.old_le);
		    } 
		    
		    if (ctrl->id == V4L2_CID_STM_IQI_EXT_CTI_PRESET) { 	    
		        state.old_cti_preset = state.new_cti_preset;
		        stmvout_get_tuning_data_control(pDev->hPlane,  VOUT_IQI_CTI_DATA, (uint32_t *)&state.old_cti);
		    }     
		    
		    
	        pDev->state = state;
	        
		}			        
        break;
    }
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:	
    case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:    
    {
        struct _stvout_device_state state = pDev->state;
        
 		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}	
		ret = stmvout_write_state(ctrl, &state);		
		if (!ret) {
            ret = stmvout_set_tuning_data_control(pDev->hPlane, VOUT_IQI_PEAKING_DATA, &state.new_peaking);      
            if (ret < 0)                                                                  
                return signal_pending (current) ? -ERESTARTSYS : -EBUSY;  		
	        state.old_peaking = state.new_peaking;         
	        pDev->state = state;	
		}
        break;
    }
    case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
    {
        struct _stvout_device_state state = pDev->state;
        
 		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}	
		ret = stmvout_write_state(ctrl, &state);		
		if (!ret) {
            ret = stmvout_set_tuning_data_control(pDev->hPlane, VOUT_IQI_LE_DATA, &state.new_le);      
            if (ret < 0)                                                                  
                return signal_pending (current) ? -ERESTARTSYS : -EBUSY;  		
	        state.old_le = state.new_le; 
	        pDev->state = state;	
		}
        break;        
    
    }
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
	{
        struct _stvout_device_state state = pDev->state;


 		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}	
		ret = stmvout_write_state(ctrl, &state);		
		if (!ret) {
            ret = stmvout_set_tuning_data_control(pDev->hPlane, VOUT_IQI_CTI_DATA, &state.new_cti);      
            if (ret < 0)                                                                  
                return signal_pending (current) ? -ERESTARTSYS : -EBUSY;  		
	        state.old_cti = state.new_cti; 	        
	        pDev->state = state;	
		}
        break;    	    
	

	}
	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
	{
		/* get the output display ... */
		stm_display_output_h hOutput;
		stm_rational_t DisplayAspectRatio;

		if ((hOutput =
		     stmvout_get_output_display_handle(pDev)) == NULL) {
			err_msg("V4L2 setting V4L2_CID_DV_STM_TX_ASPECT_RATIO and no output!\n");
			return -EIO;
		}

		switch (ctrl->val) {
		case VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9:
			DisplayAspectRatio.numerator = 16;
			DisplayAspectRatio.denominator = 9;
			break;
		case VCSOUTPUT_DISPLAY_ASPECT_RATIO_4_3:
			DisplayAspectRatio.numerator = 4;
			DisplayAspectRatio.denominator = 3;
			break;
		case VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9:
			DisplayAspectRatio.numerator = 14;
			DisplayAspectRatio.denominator = 9;
			break;
		default:
			err_msg
				("Invalid OUTPUT_CTRL_DISPLAY_ASPECT_RATIO value (%u)\n",
				 ctrl->val);
			return -EINVAL;
		}

		ret = stm_display_output_set_compound_control(hOutput,
								 OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,
								 &DisplayAspectRatio);
		if (ret != 0) {
			err_msg("Cannot set OUTPUT_CTRL_DISPLAY_ASPECT_RATIO (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;
	}
		break;

	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		ret = stm_display_plane_set_control(pDev->hPlane,
					PLANE_CTRL_INPUT_WINDOW_MODE,
					ctrl->val);
		if (ret != 0) {
			err_msg("Cannot set PLANE_CTRL_INPUT_WINDOW_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;
		break;

	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		ret = stm_display_plane_set_control(pDev->hPlane,
					PLANE_CTRL_OUTPUT_WINDOW_MODE,
					ctrl->val);
		if (ret != 0) {
			err_msg("Cannot set PLANE_CTRL_OUTPUT_WINDOW_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else
			return ret;
		break;
	case V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY:
		if(!pDev->source_info)
			return -EINVAL;

		if(pDev->source_info->iface_type != STM_SOURCE_PIXELSTREAM_IFACE)
			return -EINVAL;

		if(ctrl->val == VCSPIXEL_TIMING_UNSTABLE) {
			memset(&(pDev->source_info->pixel_param), 0,
				sizeof(pDev->source_info->pixel_param));
			ret = stm_display_source_pixelstream_set_signal_status(
				pDev->source_info->iface_handle,
				PIXELSTREAM_SOURCE_STATUS_UNSTABLE);

			if (ret)
				err_msg("Unable to set signal status %x", ret);
		}else {/* stable signal */

			ret = stm_display_source_pixelstream_set_signal_status(
				pDev->source_info->iface_handle,
				PIXELSTREAM_SOURCE_STATUS_STABLE);

			if (ret)
				err_msg("Unable to set signal status %x", ret);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int stmvout_g_ctrl(struct v4l2_ctrl *ctrl)
{
	uint32_t ctrlname, statename;
	uint32_t val;
	int ret = 0;
	struct _stvout_device *pDev = container_of(ctrl->handler,
			struct _stvout_device, ctrl_handler);

	BUG_ON(pDev == NULL);


	ctrlname = stmvout_get_ctrl_name(ctrl->id);

	if (ctrlname == 0) {
		err_msg("stmvout_get_ctrl_name()  Failed. returned ret : %d\n", ctrlname);
		return -EINVAL;
	}
	
	switch (ctrl->id) {
	case V4L2_CID_STM_XVP_SET_CONFIG:
	case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
	case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
	case V4L2_CID_STM_DEI_SET_CTRLREG:
	case V4L2_CID_STM_DEI_SET_MODE:
		if (!pDev->hPlane)
			return -EINVAL;

		ret =
		    stmvout_get_tuning_data_control(pDev->hPlane, ctrlname,
						    &val);

		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		break;

	case V4L2_CID_STM_PLANE_HIDE_MODE:
	case V4L2_CID_STM_DEI_SET_FMD_ENABLE:

	case V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE:
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:

		if (!pDev->hPlane)
			return -EINVAL;

		ret =
		    stm_display_plane_get_control(pDev->hPlane, ctrlname, &val);

		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		break;

	case V4L2_CID_STM_Z_ORDER:
	{
		stm_display_output_h hOutput;

		if (!pDev->hPlane)
			return -EINVAL;

		/* get the output display ... */
		if ((hOutput =
		     stmvout_get_output_display_handle(pDev)) == NULL) {
			err_msg("V4L2 getting Z_ORDER while the device has no output!\n");
			return -EINVAL;
		}

		ret =
		    stm_display_plane_get_depth(pDev->hPlane, hOutput,
						(int32_t *) & val);

		if (ret < 0)
			return signal_pending(current) ? -ERESTARTSYS :
			    -EBUSY;
	}
		break;

	case V4L2_CID_STM_CKEY_ENABLE:
	case V4L2_CID_STM_CKEY_FORMAT:
	case V4L2_CID_STM_CKEY_R_CR_MODE:
	case V4L2_CID_STM_CKEY_G_Y_MODE:
	case V4L2_CID_STM_CKEY_B_CB_MODE:
	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
	case V4L2_CID_STM_CKEY_RCr_MINVAL ... V4L2_CID_STM_CKEY_BCb_MAXVAL:
	{
		/* compat */
		struct v4l2_ctrl e = {.id = ctrl->id };
		if (stmvout_read_state(&e, &pDev->state))
			return -EINVAL;
		val = e.val;

	
		break;
    }

	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_SHARPNESS:

		if (!pDev->hPlane) {
			err_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}
		ret =
			stm_display_plane_get_control(pDev->hPlane,
						  ctrlname, &val);
		if (ret) {
			err_msg("stm_display_plane_get_control Returned - %d current_value = %d",
				 ret, val);
			return ret;
		}
		ctrl->val = val;
		debug_msg
			("stm_display_plane_get_control current_value = %d \n",
			 val);

		break;

		

	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:	
    case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET: /* available values: ON, OFF, SOFT, MEDIUM, STRONG */
    {
    
        struct v4l2_ctrl e = {.id = ctrl->id };
    

		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}
		/* get state name */
		statename = stmvout_get_state_name(ctrl->id);
		if (statename == 0){
			err_msg("stmvout_get_state_name()  Failed. returned ret : %d\n", statename);
			return -EINVAL;
		}

		
		ret =
			stm_display_plane_get_control(pDev->hPlane,
						  statename, &val);
		if (ret) {
			err_msg("stm_display_plane_get_control failed, returned - %d current_value = %d",
				 ret, val);
			return ret;
		}
		
		if (stmvout_read_state(&e, &pDev->state))
			return -EINVAL;		
		
		ctrl->val = e.val;
		
		break;
	}	
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:	
    case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:  
    case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:	  
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:    
    {
    
        struct v4l2_ctrl e = {.id = ctrl->id };
                
 		if (!pDev->hPlane) {
			debug_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}
		
		if (stmvout_read_state(&e, &pDev->state))
			return -EINVAL;
		val = e.val;
   
        break;
    }				

	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		ret = stm_display_plane_get_control(pDev->hPlane,
				PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE,
				&val);

		if (ret != 0) {
			err_msg("Cannot get PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;

		break;
	case V4L2_CID_STM_VQ_MCDI:
	case V4L2_CID_STM_VQ_AFM:
	case V4L2_CID_STM_VQ_TNR:
	case V4L2_CID_STM_VQ_CCS:
	case V4L2_CID_STM_VQ_ACC:
	case V4L2_CID_STM_VQ_ACM:
	case V4L2_CID_STM_VQ_SHARPNESS:
	{

		if (!pDev->hPlane) {
			err_msg("Invalid Plane handle.. \n");
			return -EINVAL;
		}

		statename = stmvout_get_state_name(ctrl->id);
		if (statename == 0) {
			err_msg("invalid state name\n");
			return -EINVAL;
		}

		ret =
			stm_display_plane_get_control(pDev->hPlane,
						  statename, &val);
		if (ret) {
			err_msg("stm_display_plane_get_control Returned - %d current_value = %d",
				 ret, val);
			return ret;
		}
		ctrl->val = val;
		debug_msg
			("stm_display_plane_get_control current_value = %d \n",
			 val);

		break;
	}
	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
	{
		/* get the output display ... */
		stm_display_output_h hOutput;
		stm_rational_t DisplayAspectRatio;

		if ((hOutput =
		     stmvout_get_output_display_handle(pDev)) == NULL) {
			err_msg("V4L2 getting V4L2_CID_DV_STM_TX_ASPECT_RATIO and no output!\n");
			return -EIO;
		}

		ret = stm_display_output_get_compound_control(hOutput,
								 OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,
								 &DisplayAspectRatio);
		if (ret != 0) {
			err_msg("Cannot get OUTPUT_CTRL_DISPLAY_ASPECT_RATIO (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		else {
			if (DisplayAspectRatio.numerator == 16
				&& DisplayAspectRatio.denominator == 9) {
				val = VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9;
			} else if (DisplayAspectRatio.numerator == 4
				   && DisplayAspectRatio.denominator == 3) {
				val = VCSOUTPUT_DISPLAY_ASPECT_RATIO_4_3;
			} else if (DisplayAspectRatio.numerator == 14
				   && DisplayAspectRatio.denominator == 9) {
				val = VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9;
			} else {
				err_msg("Invalid value for get OUTPUT_CTRL_DISPLAY_ASPECT_RATIO (%d/%d)\n",
					DisplayAspectRatio.numerator,
					DisplayAspectRatio.denominator);
				return -EINVAL;
			}
		}
	}
		break;

	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		ret = stm_display_plane_get_control(pDev->hPlane,
					PLANE_CTRL_INPUT_WINDOW_MODE,
					&val);
		if (ret != 0) {
			err_msg("Cannot get PLANE_CTRL_INPUT_WINDOW_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;

		break;

	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		ret = stm_display_plane_get_control(pDev->hPlane,
					PLANE_CTRL_OUTPUT_WINDOW_MODE,
					&val);
		if (ret != 0) {
			err_msg("Cannot get PLANE_CTRL_OUTPUT_WINDOW_MODE (%d)\n",
				 ret);
		}
		if (ret == -EINTR)
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;

		break;

	default:
		return -EINVAL;
	}

	if(!ret)
		ctrl->val = val;

	return ret;
}


static int stmvout_try_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	union {
		s32 val;
		s64 val64;
		char *string;
	} temp_val;

	/* since the values in v4l2 layer are not in sync with those in the kpi, we update
	the current values by querying them here. This is required for the ctrl framework to
	decide to invoke s_ctrl */
	if(ctrl->flags & V4L2_CTRL_FLAG_VOLATILE){
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			temp_val.string = kzalloc(ctrl->maximum, GFP_KERNEL);
			if(!temp_val.string)
				return -ENOMEM;
			/* strings are always 0-terminated */
			strcpy(temp_val.string, ctrl->string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			temp_val.val64 = ctrl->val64;
			break;
		default:
			temp_val.val = ctrl->val;
			break;
		}

		ret = ctrl->ops->g_volatile_ctrl(ctrl);
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			/* strings are always 0-terminated */
			strcpy(ctrl->cur.string, ctrl->string);
			strcpy(ctrl->string, temp_val.string);
			kfree(temp_val.string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			ctrl->cur.val64 = ctrl->val64;
			ctrl->val64 = temp_val.val64;
			break;
		default:
			ctrl->cur.val = ctrl->val;
			ctrl->val = temp_val.val;
			break;
		}
	}
	return ret;
}

static const struct v4l2_ctrl_ops stmvout_ctrl_ops = {
	.try_ctrl = stmvout_try_ctrl,
	.s_ctrl = stmvout_s_ctrl,
	.g_volatile_ctrl = stmvout_g_ctrl,
};

static int stmvout_fill_std_ctrl(struct _stvout_device *const pDev,u32 id,
		  stm_display_plane_control_range_t *range, u32 *flags)
{
	int ret = 0;
	switch(id){
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_SHARPNESS:
	{
		uint32_t plane_caps;
		*flags = V4L2_CTRL_FLAG_VOLATILE|V4L2_CTRL_FLAG_SLIDER;

		memset(range, 0, sizeof(*range));
		/*
		 * Get the plane capabilities
		 */
		if ((ret =
		     stm_display_plane_get_capabilities(pDev->hPlane,
							&plane_caps)) < 0) {
			err_msg("plane get capabilities returned error - %d",
				ret);
			break;
		}

		if (!(plane_caps & PLANE_CAPS_VIDEO)){
			ret = -EINVAL;
			break;
		}
		ret = stm_display_plane_get_control_range(pDev->hPlane,
					stmvout_get_ctrl_name(id),
					range);
		if (ret) {

		    err_msg("Get Control Range Failed - %d\n" , ret);

			break;
		}
		break;
	}
	default:
		return -EINVAL;
	}

	return ret;
}


static int
stmvout_fill_custom_ctrl(struct _stvout_device *const pDev,
		  struct v4l2_ctrl_config *const cfg)
{
	bool ctrl_supported = false;
	int ret = 0;
	struct v4l2_ctrl control;

	BUG_ON(pDev == NULL);
	control.handler = &pDev->ctrl_handler;
	
	switch (cfg->id) {
	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		control.id = cfg->id;
		ret = stmvout_g_ctrl(&control);
		if(ret)
			break;

		ctrl_supported = true;

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
				"Plane aspect ratio conversion_mode",
				VCSASPECT_RATIO_CONV_LETTER_BOX,
				VCSASPECT_RATIO_CONV_IGNORE, 0, control.val,
				V4L2_CTRL_TYPE_MENU, V4L2_CTRL_FLAG_VOLATILE);
		break;
	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		control.id = cfg->id;
		ret = stmvout_g_ctrl(&control);
		if(ret)
			break;

		ctrl_supported = true;

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
					"Input window mode",
						  0, 1, 1, control.val,
						  V4L2_CTRL_TYPE_BOOLEAN, V4L2_CTRL_FLAG_VOLATILE);
		break;
	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		control.id = cfg->id;
		ret = stmvout_g_ctrl(&control);
		if(ret)
			break;

		ctrl_supported = true;

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
				"Output window mode", 0, 1, 1, control.val,
				V4L2_CTRL_TYPE_BOOLEAN, V4L2_CTRL_FLAG_VOLATILE);
		break;
	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
				"Output aspect ratio",
				VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9,
				VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9, 0, 0,
				V4L2_CTRL_TYPE_MENU, V4L2_CTRL_FLAG_VOLATILE);
		ctrl_supported = true;
		break;
	case V4L2_CID_STM_Z_ORDER:

		ctrl_supported = true;

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
					  "Z order",
					  1, 7, 1, 1,
					  V4L2_CTRL_TYPE_INTEGER, V4L2_CTRL_FLAG_VOLATILE);

		break;

	case V4L2_CID_STM_ANTI_FLICKER_FILTER_STATE:
	case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:
	{
		stm_plane_capabilities_t caps;
		enum v4l2_ctrl_type type = V4L2_CTRL_TYPE_INTEGER;
		s32 step = 1;


		/*
		 * This control is only applicable for GDP, so, we need to check
		 * the plane caps and register this with GDP handle only
		 */
		ret = stm_display_plane_get_capabilities(pDev->hPlane, &caps);
		if (ret) {
			printk(KERN_ERR "%s(): Failed to get %s plane control\n", __func__, pDev->name);
			break;
		}

		if (!(caps & PLANE_CAPS_GRAPHICS)) {
			ret = -EOPNOTSUPP;
			break;
		}

		ctrl_supported = true;

		switch(cfg->id) {
		case V4L2_CID_STM_ANTI_FLICKER_FILTER_MODE:
			/*
			 * Local variables are already setup for filter state,
			 * so, we need to update only for filter mode, as,
			 * this is being registered as menu control
			 */
			step = 0;
			type = V4L2_CTRL_TYPE_MENU;
			break;

		default:
			break;
		}

		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
				"Flicker filter",
				0, 1, step, 0, type,
				V4L2_CTRL_FLAG_VOLATILE);
		break;
	}

	case V4L2_CID_STM_XVP_SET_CONFIG:
	case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
	case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
	case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
	case V4L2_CID_STM_DEI_SET_MODE:
	case V4L2_CID_STM_DEI_SET_CTRLREG:

	case V4L2_CID_STM_PLANE_HIDE_MODE:

	case V4L2_CID_STM_FMD_T_MOV:
	case V4L2_CID_STM_FMD_T_NUM_MOV_PIX:
	case V4L2_CID_STM_FMD_T_REPEAT:
	case V4L2_CID_STM_FMD_T_SCENE:
	case V4L2_CID_STM_FMD_COUNT_MISS:
	case V4L2_CID_STM_FMD_COUNT_STILL:
	case V4L2_CID_STM_FMD_T_NOISE:
	case V4L2_CID_STM_FMD_K_CFD1:
	case V4L2_CID_STM_FMD_K_CFD2:
	case V4L2_CID_STM_FMD_K_CFD3:
	case V4L2_CID_STM_FMD_K_SCENE:
	case V4L2_CID_STM_FMD_D_SCENE:

	
	case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:
	case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:	
    case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
	case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:	
	case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
	case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:  
	case V4L2_CID_STM_IQI_EXT_LE_PRESET:	  
    case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:	
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
    case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
 	case V4L2_CID_STM_IQI_EXT_CTI_PRESET:	  
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
	case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:        

 		
	{

			int ret;
			if (!pDev->hPlane)
				return -EINVAL;
			switch (cfg->id) {

			case V4L2_CID_STM_VQ_SHARPNESS:

				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Sharpness state",
							  VCSISCM_OFF,
							  VCSISCM_COUNT - 1, 0,
							  VCSISCM_ON,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_SHARPNESS,
				     &ctrl_supported);


				break;

			case V4L2_CID_STM_DEI_SET_MODE:

				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "DEI enable",
							  VCSDSMM_FIRST,
							  VCSDSMM_COUNT - 1, 0,
							  VCSDSMM_3DMOTION,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_MADI,
				     &ctrl_supported);


				break;

			case V4L2_CID_STM_XVP_SET_CONFIG:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "xVP config",
							  PCxVPC_FIRST,
							  PCxVPC_COUNT - 1, 0,
							  PCxVPC_BYPASS,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_XVP,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "NLE override",
							  0, 255, 1, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_XVP,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "TNR Top/Bottom swap",
							  0, 1, 1, 0,
							  V4L2_CTRL_TYPE_BOOLEAN,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_XVP,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "FMD enable",
							  VCSDSFEM_OFF,
							  VCSDSFEM_ON, 1,
							  VCSDSFEM_ON,
							  V4L2_CTRL_TYPE_BOOLEAN,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_DEI_SET_CTRLREG:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "DEI ctrl register",
							  INT_MIN, INT_MAX, 1,
							  0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_MADI,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_PLANE_HIDE_MODE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Plane Hide mode",
							  VCSPHM_MIXER_PULLSDATA,
							  VCSPHM_MIXER_DISABLE,
							  0,
							  VCSPHM_MIXER_PULLSDATA,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_MADI,
				     &ctrl_supported);

				break;

				/* FMD */
			case V4L2_CID_STM_FMD_T_MOV:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Moving pixel threshold",
							  0, INT_MAX, 1, 10,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_T_NUM_MOV_PIX:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Moving block threshold",
							  0, INT_MAX, 1, 9,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_T_REPEAT:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Threshold on BBD for a field repetition",
							  0, INT_MAX, 1, 70,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_T_SCENE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Threshold on BBD for a scene change",
							  0, INT_MAX, 1, 15,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_COUNT_MISS:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Delay for a film mode detection",
							  0, INT_MAX, 1, 2,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_COUNT_STILL:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Delay for a still mode detection",
							  0, INT_MAX, 1, 30,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_T_NOISE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Noise threshold", 0,
							  INT_MAX, 1, 10,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_K_CFD1:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Consecutive field difference factor 1",
							  0, INT_MAX, 1, 21,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_K_CFD2:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Consecutive field difference factor 2",
							  0, INT_MAX, 1, 16,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_K_CFD3:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Consecutive field difference factor 3",
							  0, INT_MAX, 1, 6,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_K_SCENE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Percentage of blocks with BBD > t_scene",
							  0, 100, 1, 25,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

			case V4L2_CID_STM_FMD_D_SCENE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "Moving pixel threshold",
							  1, 4, 1, 1,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_FMD,
				     &ctrl_supported);

				break;

				
/* PEAKING */		

			case V4L2_CID_STM_IQI_EXT_PEAKING_PRESET:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking enable/disable mode & preset",
							  CONTROL_OFF,
							  CONTROL_SHARP, 0,
							  PCIQIC_BYPASS,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
							  
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane,PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);
				
				
				printk("stm_display_plane_is_feature_applicable V4L2_CID_STM_IQI_EXT_PEAKING_PRESET = %d\n", ret);
				     
				break;
		
			case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking undershoot factor",
							  VCSIUF_100,
							  VCSIUF_025, 0,
							  VCSIUF_100,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking overshoot factor",
							  VCSIOF_100,
							  VCSIOF_025, 0,
							  VCSIOF_100,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking manual coring",
							  0, 1, 1, 0,
							  V4L2_CTRL_TYPE_BOOLEAN,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking coring level",
							  0, 63, 1, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER | V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI vertical peaking",
							  0, 1, 1, 0,
							  V4L2_CTRL_TYPE_BOOLEAN,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking clipping mode",
							  VCSISTRENGTH_NONE,
							  VCSISTRENGTH_STRONG,
							  0, VCSISTRENGTH_NONE,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking bandpass freq",
							  VCSIPFFREQ_0_15_FD2,
							  VCSIPFFREQ_0_63_FD2,
							  0,
							  VCSIPFFREQ_0_15_FD2,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI peaking highpass freq",
							  VCSIPFFREQ_0_15_FD2,
							  VCSIPFFREQ_0_63_FD2,
							  0,
							  VCSIPFFREQ_0_15_FD2,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);


				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
			case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:
				/* in centi Bel */
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  &"IQI peaking bandpassgain (cB)\0"
							  "IQI peaking highpassgain (cB)"
							  [(cfg->id -
							    V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN)
							   * 30], -60, 120, 5,
							  0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;
			case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
				/* in centi Bel */
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI vert peaking gain (cB)",
							  -60, 120, 5, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_PEAKING,
				     &ctrl_supported);

				break;					
/* LE*/	
			case V4L2_CID_STM_IQI_EXT_LE_PRESET:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI LE enable/disable mode & preset",
							  CONTROL_OFF,
							  CONTROL_SHARP, 0,
							  CONTROL_OFF,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
							  
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane,PLANE_FEAT_VIDEO_IQI_LE,
				     &ctrl_supported);
				     
				break;	
			case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI LE Weight Gain",
							  0, 31, 1, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_LE,
				     &ctrl_supported);
				break;
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI LE fixed curve",
							  0, 1, 1, 0,
							  V4L2_CTRL_TYPE_BOOLEAN,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_LE,
				     &ctrl_supported);
				break;
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  &"IQI LE bk strtch inflx pt\0"
							  "IQI LE bk strtch lim pt  \0"
							  "IQI LE wt strtch inflx pt\0"
							  "IQI LE wt strtch lim pt  "
							  [(cfg->id -
							    V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT)
							   * 26], 0, 1023, 1, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_LE,
				     &ctrl_supported);
				break;
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
			case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  &"IQI LE bk strtch gain\0"
							  "IQI LE wt strtch gain"
							  [(cfg->id -
							    V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN)
							   * 22], 0, 100, 1, 0,
							  V4L2_CTRL_TYPE_INTEGER,
							  V4L2_CTRL_FLAG_SLIDER|V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_LE,
 				     &ctrl_supported);

 				break;	
 /* CTI */		
 			case V4L2_CID_STM_IQI_EXT_CTI_PRESET:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  "IQI LE enable/disable mode & preset",
							  CONTROL_OFF,
							  CONTROL_SHARP, 0,
							  CONTROL_OFF,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
							  
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane,PLANE_FEAT_VIDEO_IQI_CTI,
				     &ctrl_supported);
				     
				break;	
 
 		
			case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
			case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
				stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
							  &"IQI CTI strength 1\0"
							  "IQI CTI strength 2"
							  [(cfg->id -
							    V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1)
							   * 19], VCSICS_NONE,
							  VCSICS_STRONG, 0,
							  VCSICS_NONE,
							  V4L2_CTRL_TYPE_MENU,
							  V4L2_CTRL_FLAG_VOLATILE);
				ret =
				    stm_display_plane_is_feature_applicable
				    (pDev->hPlane, PLANE_FEAT_VIDEO_IQI_CTI,
				     &ctrl_supported);
				break; 				
			
 				
 				
				/* CE not supported */				

			default:
				/* shouldn't be reached */
				BUG();
				return -EINVAL;
			}
		}
		break;

		/* we assume color key is always supported */
	case V4L2_CID_STM_CKEY_ENABLE:
		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
					  "ColorKey", 0, 3, 0, 0,
					  V4L2_CTRL_TYPE_MENU, V4L2_CTRL_FLAG_VOLATILE);
		ctrl_supported = true;
		break;
	case V4L2_CID_STM_CKEY_FORMAT:
		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
					  "ColorKey format", 0, 1, 0, 0,
					  V4L2_CTRL_TYPE_MENU, V4L2_CTRL_FLAG_VOLATILE);
		ctrl_supported = true;
		break;
	case V4L2_CID_STM_CKEY_R_CR_MODE:
	case V4L2_CID_STM_CKEY_G_Y_MODE:
	case V4L2_CID_STM_CKEY_B_CB_MODE:
		{
			static const char *const name_map[] =
			    { "ColorKey R/Cr mode",
				"ColorKey G/Y mode",
				"ColorKey B/Cb mode"
			};
			ctrl_supported = true;
			stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
						  name_map[cfg->id -
							   V4L2_CID_STM_CKEY_R_CR_MODE],
						  0, 3, 0, 0,
						  V4L2_CTRL_TYPE_MENU, V4L2_CTRL_FLAG_VOLATILE);
		}
		break;
	case _deprecated_V4L2_CID_STM_CKEY_MINVAL:
	case _deprecated_V4L2_CID_STM_CKEY_MAXVAL:
		{
			static const char *const name_map[] =
			    { "ColorKey minval",
				"ColorKey maxval"
			};
			ctrl_supported = true;
			stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
						  name_map[cfg->id -
							   _deprecated_V4L2_CID_STM_CKEY_MINVAL],
						  INT_MIN, INT_MAX, 1, 0,
						  V4L2_CTRL_TYPE_INTEGER, V4L2_CTRL_FLAG_VOLATILE);
		}
		break;
	case V4L2_CID_STM_CKEY_RCr_MINVAL ... V4L2_CID_STM_CKEY_BCb_MAXVAL:
		{
			static const char *const name_map[] =
			    { "R/Cr ColorKey minval",
				"R/Cr ColorKey maxval",
				"G/Y ColorKey minval",
				"G/Y ColorKey maxval",
				"B/Cb ColorKey minval",
				"B/Cb ColorKey maxval"
			};
			ctrl_supported = true;
			stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
						  name_map[cfg->id -
							   V4L2_CID_STM_CKEY_RCr_MINVAL],
						  0, 255, 1, 0,
						  V4L2_CTRL_TYPE_INTEGER, V4L2_CTRL_FLAG_VOLATILE);
		}
		break;
	case V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY:

		ctrl_supported = true;
		stmvout_ctrl_cfg_fill(cfg, &stmvout_ctrl_ops,
			"Set pixel stream timing stability",
			VCSPIXEL_TIMING_UNSTABLE, VCSPIXEL_TIMING_STABLE, 1,
			VCSPIXEL_TIMING_UNSTABLE, V4L2_CTRL_TYPE_INTEGER, 0);
		break;
	default:
		return -EINVAL;
	}

	if (!ctrl_supported)
		cfg->flags |= V4L2_CTRL_FLAG_DISABLED;

	return ret;
}

int stvmout_init_ctrl_handler(struct _stvout_device *const pDev)
{
	int ret = 0, id, ctrl_num, j;
	struct v4l2_ctrl_config cfg;
	u32 stmvout_std_ctrls[] = {
		V4L2_CID_BRIGHTNESS,
		V4L2_CID_CONTRAST,
		V4L2_CID_SATURATION,
		V4L2_CID_HUE,
		V4L2_CID_SHARPNESS,
	};
	stm_display_plane_control_range_t range;
	u32 flags = 0;
	struct v4l2_ctrl *ctrl;


	stmvout_get_controls_from_core(pDev);


	/* Even though the number of controls is not required to be accurate,
	we try to give good estimate*/

	ctrl_num = V4L2_CID_STM_EXT_LAST - V4L2_CID_IMAGE_PROC_STM_VOUT_BASE + 1;
	ret = v4l2_ctrl_handler_init(&pDev->ctrl_handler, ctrl_num);
	if(ret){
		printk("Control registration failed for stmvout ret = %d", ret);
		return ret;
	}

	for(id = V4L2_CID_IMAGE_PROC_STM_VOUT_BASE; id <= V4L2_CID_STM_EXT_LAST; id++){
		memset(&cfg, 0, sizeof(cfg));
		cfg.id = id;
		ret = stmvout_fill_custom_ctrl(pDev, &cfg);
		if (ret){


			if(signal_pending(current))

				return -ERESTARTSYS;
			continue;/* we ignore other errors and proceed to add ctrls*/
		}

		if(cfg.type == V4L2_CTRL_TYPE_MENU){
			cfg.qmenu = stmvout_ctrl_get_menu(id);
		}
		v4l2_ctrl_new_custom(&pDev->ctrl_handler, &cfg, NULL);

		if (pDev->ctrl_handler.error) {
			ret = pDev->ctrl_handler.error;
			printk("Custom Control registration failed for stmvout ret = %d", ret);
			return ret;
		}
	}

	for(j = 0; j < ARRAY_SIZE(stmvout_std_ctrls); j++){

		ret = stmvout_fill_std_ctrl(pDev, stmvout_std_ctrls[j], &range, &flags);
		if (ret){
			if(signal_pending(current))
				return -ERESTARTSYS;
			continue;/* we ignore other errors and proceed to add ctrls*/
		}

		/* FIXME: temporary workaround because FVDP_GetControlsRange is not OK - bug34020 */
		range.max_val = 256;
		range.step = 1;

		ctrl = v4l2_ctrl_new_std(&pDev->ctrl_handler, &stmvout_ctrl_ops, stmvout_std_ctrls[j],
					range.min_val, range.max_val, range.step,
					range.default_val);
		if (pDev->ctrl_handler.error) {
			ret = pDev->ctrl_handler.error;
			printk("Std Control registration failed for stmvout ret = %d", ret);
			return ret;
		}
		ctrl->flags |= flags;
	}
	for(j = 0; j < ARRAY_SIZE(tunnelled_ctrls); j++){
		memset(&cfg, 0, sizeof(cfg));
		cfg.id = tunnelled_ctrls[j];

		ret = stmvout_fill_custom_ctrl(pDev, &cfg);
		if (ret){

			if(signal_pending(current))
				return -ERESTARTSYS;
			continue;/* we ignore other errors and proceed to add ctrls*/
		}

		if(cfg.type == V4L2_CTRL_TYPE_MENU){
			cfg.qmenu = stmvout_ctrl_get_menu(cfg.id);
		}
		v4l2_ctrl_new_custom(&pDev->ctrl_handler, &cfg, NULL);

		if (pDev->ctrl_handler.error) {
			ret = pDev->ctrl_handler.error;
			printk("-Custom Control registration failed for stmvout ret = %d", ret);
			return ret;
		}
	}
	return 0;
}
