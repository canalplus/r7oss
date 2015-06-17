/***********************************************************************
 *
 * File: linux/drivers/media/video/stmvout_ctrl.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
 ***********************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>

#include <stmdisplay.h>

#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "stmvout.h"
#include "stm_v4l2.h"

static unsigned long
stmvout_get_ctrl_name (int id)
{
  switch (id) {
  case V4L2_CID_BRIGHTNESS:
    return STM_CTRL_BRIGHTNESS;
  case V4L2_CID_CONTRAST:
    return STM_CTRL_CONTRAST;
  case V4L2_CID_SATURATION:
    return STM_CTRL_SATURATION;
  case V4L2_CID_HUE:
    return STM_CTRL_HUE;

  case V4L2_CID_STM_IQI_SET_CONFIG:
    return PLANE_CTRL_IQI_CONFIG;
  case V4L2_CID_STM_IQI_DEMO:
    return PLANE_CTRL_IQI_DEMO;
  case V4L2_CID_STM_XVP_SET_CONFIG:
    return PLANE_CTRL_XVP_CONFIG;
  case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
    return PLANE_CTRL_XVP_TNRNLE_OVERRIDE;
  case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
    return PLANE_CTRL_XVP_TNR_TOPBOT;
  case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
    return PLANE_CTRL_DEI_FMD_ENABLE;
  case V4L2_CID_STM_DEI_SET_MODE:
    return PLANE_CTRL_DEI_MODE;
  case V4L2_CID_STM_DEI_SET_CTRLREG:
    return PLANE_CTRL_DEI_CTRLREG;

  case V4L2_CID_STM_PLANE_BRIGHTNESS:
    return PLANE_CTRL_PSI_BRIGHTNESS;
  case V4L2_CID_STM_PLANE_CONTRAST:
    return PLANE_CTRL_PSI_CONTRAST;
  case V4L2_CID_STM_PLANE_SATURATION:
    return PLANE_CTRL_PSI_SATURATION;
  case V4L2_CID_STM_PLANE_HUE:
    return PLANE_CTRL_PSI_TINT;

  case V4L2_CID_STM_PLANE_HIDE_MODE:
    return PLANE_CTRL_HIDE_MODE;

  case V4L2_CID_STM_CKEY_ENABLE:
  case V4L2_CID_STM_CKEY_FORMAT:
  case V4L2_CID_STM_CKEY_R_CR_MODE:
  case V4L2_CID_STM_CKEY_G_Y_MODE:
  case V4L2_CID_STM_CKEY_B_CB_MODE:
  case V4L2_CID_STM_CKEY_MINVAL:
  case V4L2_CID_STM_CKEY_MAXVAL:
  case V4L2_CID_STM_Z_ORDER_FIRST ... V4L2_CID_STM_Z_ORDER_LAST:
    return 1000;

  default:
    return 0;
  }
}


static const u32 stmvout_user_ctrls[] = {
  V4L2_CID_USER_CLASS,
  V4L2_CID_BRIGHTNESS,
  V4L2_CID_CONTRAST,
  V4L2_CID_SATURATION,
  V4L2_CID_HUE,
  0
};

static const u32 stmvout_stmvout_ctrls[] = {
  V4L2_CID_STMVOUT_CLASS,

  V4L2_CID_STM_IQI_SET_CONFIG,
  V4L2_CID_STM_IQI_DEMO,
  V4L2_CID_STM_XVP_SET_CONFIG,
  V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE,
  V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP,
  V4L2_CID_STM_DEI_SET_FMD_ENABLE,
  V4L2_CID_STM_DEI_SET_MODE,
  V4L2_CID_STM_DEI_SET_CTRLREG,

  V4L2_CID_STM_Z_ORDER_VID0,
  V4L2_CID_STM_Z_ORDER_VID1,
  V4L2_CID_STM_Z_ORDER_RGB0,
  V4L2_CID_STM_Z_ORDER_RGB1,
  V4L2_CID_STM_Z_ORDER_RGB2,

  V4L2_CID_STM_Z_ORDER_RGB1_0,
  V4L2_CID_STM_Z_ORDER_RGB1_1,
  V4L2_CID_STM_Z_ORDER_RGB1_2,
  V4L2_CID_STM_Z_ORDER_RGB1_3,
  V4L2_CID_STM_Z_ORDER_RGB1_4,
  V4L2_CID_STM_Z_ORDER_RGB1_5,
  V4L2_CID_STM_Z_ORDER_RGB1_6,
  V4L2_CID_STM_Z_ORDER_RGB1_7,
  V4L2_CID_STM_Z_ORDER_RGB1_8,
  V4L2_CID_STM_Z_ORDER_RGB1_9,
  V4L2_CID_STM_Z_ORDER_RGB1_10,
  V4L2_CID_STM_Z_ORDER_RGB1_11,
  V4L2_CID_STM_Z_ORDER_RGB1_12,
  V4L2_CID_STM_Z_ORDER_RGB1_13,
  V4L2_CID_STM_Z_ORDER_RGB1_14,
  V4L2_CID_STM_Z_ORDER_RGB1_15,

  V4L2_CID_STM_PLANE_BRIGHTNESS,
  V4L2_CID_STM_PLANE_CONTRAST,
  V4L2_CID_STM_PLANE_SATURATION,
  V4L2_CID_STM_PLANE_HUE,

  V4L2_CID_STM_PLANE_HIDE_MODE,

  0
};

static const u32 stmvout_stmvout_extctrls[] = {
  V4L2_CID_STMVOUTEXT_CLASS,

  V4L2_CID_STM_CKEY_ENABLE,
  V4L2_CID_STM_CKEY_FORMAT,
  V4L2_CID_STM_CKEY_R_CR_MODE,
  V4L2_CID_STM_CKEY_G_Y_MODE,
  V4L2_CID_STM_CKEY_B_CB_MODE,
  V4L2_CID_STM_CKEY_MINVAL,
  V4L2_CID_STM_CKEY_MAXVAL,

  /* FMD */
  V4L2_CID_STM_FMD_T_MOV,
  V4L2_CID_STM_FMD_T_NUM_MOV_PIX,
  V4L2_CID_STM_FMD_T_REPEAT,
  V4L2_CID_STM_FMD_T_SCENE,
  /* */
  V4L2_CID_STM_FMD_COUNT_MISS,
  V4L2_CID_STM_FMD_COUNT_STILL,
  V4L2_CID_STM_FMD_T_NOISE,
  V4L2_CID_STM_FMD_K_CFD1,
  V4L2_CID_STM_FMD_K_CFD2,
  V4L2_CID_STM_FMD_K_CFD3,
  V4L2_CID_STM_FMD_K_SCENE,
  V4L2_CID_STM_FMD_D_SCENE,

  /* IQI */
  V4L2_CID_STM_IQI_EXT_ENABLES,
  V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT,
  V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT,
  V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING,
  V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL,
  V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL,
  V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN,
  V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE,
  V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ,
  V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ,
  V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN,
  V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN,
  V4L2_CID_STM_IQI_EXT_LTI_HECGRADIENTOFFSET,
  V4L2_CID_STM_IQI_EXT_LTI_HMMSPREFILTER,
  V4L2_CID_STM_IQI_EXT_LTI_ANTIALIASING,
  V4L2_CID_STM_IQI_EXT_LTI_VERTICAL,
  V4L2_CID_STM_IQI_EXT_LTI_VERTICAL_STRENGTH,
  V4L2_CID_STM_IQI_EXT_LTI_HORIZONTAL_STRENGTH,
  V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1,
  V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2,
  V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN,
  V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN,
  /* CE not supported */

  0
};


/*
 * This needs to be exported for the V4L2 driver structure.
 */
const u32 *stmvout_ctrl_classes[] = {
  stmvout_user_ctrls,
  stmvout_stmvout_ctrls,
  stmvout_stmvout_extctrls,
  NULL
};


static struct _stvout_device *
_get_device_by_id (struct _stvout_device pipeline_devs[],
                   stm_plane_id_t        id)
{
  int plane;

  if (pipeline_devs)
    for (plane = 0; plane < MAX_PLANES; ++plane)
      if (pipeline_devs[plane].planeConfig->id == id)
        return &pipeline_devs[plane];

  return NULL;
}


void
stmvout_get_controls_from_core (struct _stvout_device * const device)
{
#define GET_STATE(ctrl, old_state, new_state) \
    if (0 != stm_display_plane_get_control (device->pPlane,                \
                                            ctrl,                          \
                                            (unsigned long *) &old_state)) \
      memset (&old_state, 0, sizeof (old_state));                          \
    new_state = old_state;

  GET_STATE(PLANE_CTRL_DEI_FMD_THRESHS,
            device->state.old_fmd, device->state.new_fmd);

  if (0 != stm_display_plane_get_control (device->pPlane,
                                          PLANE_CTRL_IQI_ENABLES,
                                          (unsigned long *) &device->state.old_iqi.enables))
    device->state.old_iqi.enables = 0;
  device->state.new_iqi.enables = device->state.old_iqi.enables;

  GET_STATE (PLANE_CTRL_IQI_PEAKING,
             device->state.old_iqi.peaking, device->state.new_iqi.peaking);
  GET_STATE (PLANE_CTRL_IQI_LTI,
             device->state.old_iqi.lti, device->state.new_iqi.lti);
  GET_STATE (PLANE_CTRL_IQI_CTI,
             device->state.old_iqi.cti, device->state.new_iqi.cti);
  GET_STATE (PLANE_CTRL_IQI_LE,
             device->state.old_iqi.le, device->state.new_iqi.le);
#undef GET_STATE
}

int
stmvout_queryctrl (struct _stvout_device * const pDev,
                   struct _stvout_device  pipeline_devs[],
                   struct v4l2_queryctrl * const pqc)
{
  unsigned long          output_caps;
  struct _stvout_device * const this_dev = pDev;
  int                    ctrl_supported = 1;

  pqc->id = v4l2_ctrl_next (stmvout_ctrl_classes, pqc->id);
  if (!pqc->id)
    return -EINVAL;

  BUG_ON (pDev == NULL);

  if (stm_display_output_get_control_capabilities (pDev->pOutput,
                                                   &output_caps) < 0)
    return -ERESTARTSYS;

  switch (pqc->id) {
  case V4L2_CID_USER_CLASS:
    stm_v4l2_ctrl_query_fill (pqc, "User controls", 0, 0, 0, 0,
                              V4L2_CTRL_TYPE_CTRL_CLASS,
                              V4L2_CTRL_FLAG_READ_ONLY);
    break;
  case V4L2_CID_STMVOUT_CLASS:
    stm_v4l2_ctrl_query_fill (pqc, "STMvout controls", 0, 0, 0, 0,
                              V4L2_CTRL_TYPE_CTRL_CLASS,
                              V4L2_CTRL_FLAG_READ_ONLY);
    break;
  case V4L2_CID_STMVOUTEXT_CLASS:
    stm_v4l2_ctrl_query_fill (pqc, "STMvout ext controls", 0, 0, 0, 0,
                              V4L2_CTRL_TYPE_CTRL_CLASS,
                              V4L2_CTRL_FLAG_READ_ONLY);
    break;

  case V4L2_CID_BRIGHTNESS:
    stm_v4l2_ctrl_query_fill (pqc, "Brightness",
                              0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER,
                              V4L2_CTRL_FLAG_SLIDER);
    ctrl_supported = output_caps & STM_CTRL_CAPS_BRIGHTNESS;
    break;

  case V4L2_CID_CONTRAST:
    stm_v4l2_ctrl_query_fill (pqc, "Contrast",
                              0, 255, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                              V4L2_CTRL_FLAG_SLIDER);
    ctrl_supported = output_caps & STM_CTRL_CAPS_CONTRAST;
    break;

  case V4L2_CID_SATURATION:
    stm_v4l2_ctrl_query_fill (pqc, "Saturation",
                              0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER,
                              V4L2_CTRL_FLAG_SLIDER);
    ctrl_supported = output_caps & STM_CTRL_CAPS_SATURATION;
    break;

  case V4L2_CID_HUE:
    stm_v4l2_ctrl_query_fill (pqc, "Hue",
                              0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER,
                              V4L2_CTRL_FLAG_SLIDER);
    ctrl_supported = output_caps & STM_CTRL_CAPS_HUE;
    break;

  case V4L2_CID_STM_Z_ORDER_VID0 ... V4L2_CID_STM_Z_ORDER_RGB2:
    {
      static const char * const name_map[] = { "Z order for plane YUV0",
                                               "Z order for plane YUV1",
                                               "Z order for plane RGB0",
                                               "Z order for plane RGB1",
                                               "Z order for plane RGB2"
      };
      int depth;
      stm_v4l2_ctrl_query_fill (pqc,
                                name_map[pqc->id - V4L2_CID_STM_Z_ORDER_VID0],
                                0, 4, 1, pqc->id - V4L2_CID_STM_Z_ORDER_VID0,
                                V4L2_CTRL_TYPE_INTEGER, 0);
      ctrl_supported = stm_display_plane_get_depth (this_dev->pPlane,
                                                    this_dev->pOutput,
                                                    &depth);
      if (ctrl_supported < 0)
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    }
    break;

  /* the v plane controls are unsupported atm */
  case V4L2_CID_STM_Z_ORDER_RGB1_0 ... V4L2_CID_STM_Z_ORDER_RGB1_15:
    {
      static const char * const name_map[] = { "Z order for v plane RGB1.0",
                                               "Z order for v plane RGB1.1",
                                               "Z order for v plane RGB1.2",
                                               "Z order for v plane RGB1.3",
                                               "Z order for v plane RGB1.4",
                                               "Z order for v plane RGB1.5",
                                               "Z order for v plane RGB1.6",
                                               "Z order for v plane RGB1.7",
                                               "Z order for v plane RGB1.8",
                                               "Z order for v plane RGB1.9",
                                               "Z order for v plane RGB1.10",
                                               "Z order for v plane RGB1.11",
                                               "Z order for v plane RGB1.12",
                                               "Z order for v plane RGB1.13",
                                               "Z order for v plane RGB1.14",
                                               "Z order for v plane RGB1.15"
      };
      stm_v4l2_ctrl_query_fill (pqc,
                                name_map[pqc->id - V4L2_CID_STM_Z_ORDER_RGB1_0],
                                0, 15, 1, pqc->id - V4L2_CID_STM_Z_ORDER_RGB1_0,
                                V4L2_CTRL_TYPE_INTEGER, V4L2_CTRL_FLAG_DISABLED);
      ctrl_supported = 0;
    }
    break;

  case V4L2_CID_STM_IQI_SET_CONFIG:
  case V4L2_CID_STM_IQI_DEMO:
  case V4L2_CID_STM_IQI_EXT_FIRST ... V4L2_CID_STM_IQI_EXT_LAST:
  case V4L2_CID_STM_XVP_SET_CONFIG:
  case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
  case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
  case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
  case V4L2_CID_STM_DEI_SET_MODE:
  case V4L2_CID_STM_DEI_SET_CTRLREG:

  case V4L2_CID_STM_PLANE_BRIGHTNESS:
  case V4L2_CID_STM_PLANE_CONTRAST:
  case V4L2_CID_STM_PLANE_SATURATION:
  case V4L2_CID_STM_PLANE_HUE:

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
    {
      stm_plane_caps_t planeCaps;
      int              ret;

      if (!this_dev->pPlane || !this_dev->pOutput)
        return -EINVAL;

      ret = stm_display_plane_get_capabilities (this_dev->pPlane, &planeCaps);

      if (ret < 0)
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;

      switch (pqc->id) {
      /* IQI */
      case V4L2_CID_STM_IQI_SET_CONFIG:
        stm_v4l2_ctrl_query_fill (pqc, "IQI config",
                                  PCIQIC_FIRST, PCIQIC_COUNT - 1, 1,
                                  PCIQIC_BYPASS,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;

      case V4L2_CID_STM_IQI_DEMO:
        stm_v4l2_ctrl_query_fill (pqc, "IQI demo",
                                  VCSIDM_OFF, VCSIDM_ON, 1, VCSIDM_OFF,
                                  V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;

      case V4L2_CID_STM_IQI_EXT_ENABLES:
        stm_v4l2_ctrl_query_fill (pqc, "IQI bit enables",
                                  VCSIENABLE_NONE, VCSIENABLE_ALL, 1,
                                  VCSIENABLE_NONE,
                                  V4L2_CTRL_TYPE_MENU,
                                  0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking undershoot factor",
                                  VCSIPOUSF_100, VCSIPOUSF_025, 1,
                                  VCSIPOUSF_100,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking overshoot factor",
                                  VCSIPOUSF_100, VCSIPOUSF_025, 1,
                                  VCSIPOUSF_100,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking manual coring",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking coring level",
                                  0, 63, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:
        stm_v4l2_ctrl_query_fill (pqc, "IQI vertical peaking",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking clipping mode",
                                  VCSISTRENGTH_NONE, VCSISTRENGTH_STRONG, 1,
                                  VCSISTRENGTH_NONE,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking bandpass freq",
                                  VCSIPFFREQ_0_15_FsDiv2, VCSIPFFREQ_0_63_FsDiv2,
                                  1, VCSIPFFREQ_0_15_FsDiv2,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
        stm_v4l2_ctrl_query_fill (pqc, "IQI peaking highpass freq",
                                  VCSIPFFREQ_0_15_FsDiv2, VCSIPFFREQ_0_63_FsDiv2,
                                  1, VCSIPFFREQ_0_15_FsDiv2,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
      case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:
        /* in centi Bel */
        stm_v4l2_ctrl_query_fill (pqc,
                                  &"IQI peaking bandpassgain (cB)\0"
                                   "IQI peaking highpassgain (cB)"
                                  [(pqc->id - V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN) * 30],
                                  -60, 120, 5, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
        /* in centi Bel */
        stm_v4l2_ctrl_query_fill (pqc, "IQI vert peaking gain (cB)",
                                  -60, 120, 5, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_HECGRADIENTOFFSET:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI HEC gradient offset",
                                  0, 8, 4, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_HMMSPREFILTER:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI HMMS prefilter",
                                  VCSISTRENGTH_NONE, VCSISTRENGTH_STRONG, 1,
                                  VCSISTRENGTH_NONE,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_ANTIALIASING:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI antialiasing",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI vertical",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL_STRENGTH:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI ver strength",
                                  0, 15, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LTI_HORIZONTAL_STRENGTH:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LTI hor strength",
                                  0, 31, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
      case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
        stm_v4l2_ctrl_query_fill (pqc,
                                  &"IQI CTI strength 1\0"
                                   "IQI CTI strength 2"
                                  [(pqc->id - V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1) * 19],
                                  VCSICS_NONE, VCSICS_STRONG, 1, VCSICS_NONE,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LE Weight Gain",
                                  0, 31, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED:
        stm_v4l2_ctrl_query_fill (pqc, "IQI LE fixed curve",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT:
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT:
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT:
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT:
        stm_v4l2_ctrl_query_fill (pqc,
                                  &"IQI LE bk strtch inflx pt\0"
                                   "IQI LE bk strtch lim pt  \0"
                                   "IQI LE wt strtch inflx pt\0"
                                   "IQI LE wt strtch lim pt  "
                                  [(pqc->id - V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT) * 26],
                                  0, 1023, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN:
      case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN:
        stm_v4l2_ctrl_query_fill (pqc,
                                  &"IQI LE bk strtch gain\0"
                                   "IQI LE wt strtch gain"
                                  [(pqc->id - V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN) * 22],
                                  0, 100, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_IQI;
        break;
      /* CE not supported */

      case V4L2_CID_STM_XVP_SET_CONFIG:
        stm_v4l2_ctrl_query_fill (pqc, "xVP config",
                                  PCxVPC_FIRST, PCxVPC_COUNT - 1, 1,
                                  PCxVPC_BYPASS, V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & (PLANE_CTRL_CAPS_FILMGRAIN
                                                 | PLANE_CTRL_CAPS_TNR);
        break;

      case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
        stm_v4l2_ctrl_query_fill (pqc, "NLE override",
                                  0, 255, 1, 0, V4L2_CTRL_TYPE_INTEGER, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_TNR;
        break;

      case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
        stm_v4l2_ctrl_query_fill (pqc, "TNR Top/Bottom swap",
                                  0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_TNR;
        break;

      case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
        stm_v4l2_ctrl_query_fill (pqc, "FMD enable",
                                  VCSDSFEM_OFF, VCSDSFEM_ON, 1, VCSDSFEM_ON,
                                  V4L2_CTRL_TYPE_BOOLEAN, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;

      case V4L2_CID_STM_DEI_SET_MODE:
        stm_v4l2_ctrl_query_fill (pqc, "DEI enable",
                                  VCSDSMM_FIRST, VCSDSMM_COUNT - 1, 1,
                                  VCSDSMM_3DMOTION, V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_DEINTERLACE;
        break;

      case V4L2_CID_STM_DEI_SET_CTRLREG:
        stm_v4l2_ctrl_query_fill (pqc, "DEI ctrl register",
                                  INT_MIN, INT_MAX, 1,
                                  0, V4L2_CTRL_TYPE_INTEGER, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_DEINTERLACE;
        break;

      case V4L2_CID_STM_PLANE_BRIGHTNESS:
      case V4L2_CID_STM_PLANE_CONTRAST:
      case V4L2_CID_STM_PLANE_SATURATION:
      case V4L2_CID_STM_PLANE_HUE:
        {
          static const char * const name_map[] = { "Plane Brightness",
                                                   "Plane Contrast",
                                                   "Plane Saturation",
                                                   "Plane Hue"
          };
          stm_v4l2_ctrl_query_fill (pqc,
                                    name_map[pqc->id - V4L2_CID_STM_PLANE_BRIGHTNESS],
                                    0, 255, 1,
                                    128, V4L2_CTRL_TYPE_INTEGER,
                                    V4L2_CTRL_FLAG_SLIDER);
          ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_PSI_CONTROLS;
        }
        break;

      case V4L2_CID_STM_PLANE_HIDE_MODE:
        stm_v4l2_ctrl_query_fill (pqc,
                                  "Plane Hide mode",
                                  VCSPHM_MIXER_PULLSDATA,
                                  VCSPHM_MIXER_DISABLE, 1,
                                  VCSPHM_MIXER_PULLSDATA,
                                  V4L2_CTRL_TYPE_MENU, 0);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_DEINTERLACE;
        break;

      /* FMD */
      case V4L2_CID_STM_FMD_T_MOV:
        stm_v4l2_ctrl_query_fill (pqc, "Moving pixel threshold",
                                  0, INT_MAX, 1, 10, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_T_NUM_MOV_PIX:
        stm_v4l2_ctrl_query_fill (pqc, "Moving block threshold",
                                  0, INT_MAX, 1, 9, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_T_REPEAT:
        stm_v4l2_ctrl_query_fill (pqc, "Threshold on BBD for a field repetition",
                                  0, INT_MAX, 1, 70, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_T_SCENE:
        stm_v4l2_ctrl_query_fill (pqc, "Threshold on BBD for a scene change",
                                  0, INT_MAX, 1, 15, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;

      case V4L2_CID_STM_FMD_COUNT_MISS:
        stm_v4l2_ctrl_query_fill (pqc, "Delay for a film mode detection",
                                  0, INT_MAX, 1, 2, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_COUNT_STILL:
        stm_v4l2_ctrl_query_fill (pqc, "Delay for a still mode detection",
                                  0, INT_MAX, 1, 30, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_T_NOISE:
        stm_v4l2_ctrl_query_fill (pqc, "Noise threshold",
                                  0, INT_MAX, 1, 10, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_K_CFD1:
        stm_v4l2_ctrl_query_fill (pqc, "Consecutive field difference factor 1",
                                  0, INT_MAX, 1, 21, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_K_CFD2:
        stm_v4l2_ctrl_query_fill (pqc, "Consecutive field difference factor 2",
                                  0, INT_MAX, 1, 16, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_K_CFD3:
        stm_v4l2_ctrl_query_fill (pqc, "Consecutive field difference factor 3",
                                  0, INT_MAX, 1, 6, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_K_SCENE:
        stm_v4l2_ctrl_query_fill (pqc, "Percentage of blocks with BBD > t_scene",
                                  0, 100, 1, 25, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;
      case V4L2_CID_STM_FMD_D_SCENE:
        stm_v4l2_ctrl_query_fill (pqc, "Moving pixel threshold",
                                  1, 4, 1, 1, V4L2_CTRL_TYPE_INTEGER,
                                  V4L2_CTRL_FLAG_SLIDER);
        ctrl_supported = planeCaps.ulControls & PLANE_CTRL_CAPS_FMD;
        break;

      default:
        /* shouldn't be reached */
        BUG ();
        return -EINVAL;
      }
    }
    break;

  /* we assume color key is always supported */
  case V4L2_CID_STM_CKEY_ENABLE:
    stm_v4l2_ctrl_query_fill (pqc,
                              "ColorKey", 0, 3, 1, 0, V4L2_CTRL_TYPE_MENU, 0);
    break;
  case V4L2_CID_STM_CKEY_FORMAT:
    stm_v4l2_ctrl_query_fill (pqc,
                              "ColorKey format", 0, 1, 1, 0, V4L2_CTRL_TYPE_MENU,
                              0);
    break;
  case V4L2_CID_STM_CKEY_R_CR_MODE:
  case V4L2_CID_STM_CKEY_G_Y_MODE:
  case V4L2_CID_STM_CKEY_B_CB_MODE:
    {
      static const char * const name_map[] = { "ColorKey R/Cr mode",
                                               "ColorKey G/Y mode",
                                               "ColorKey B/Cb mode"
      };
      stm_v4l2_ctrl_query_fill (pqc,
                                name_map[pqc->id - V4L2_CID_STM_CKEY_R_CR_MODE],
                                0, 3, 1, 0, V4L2_CTRL_TYPE_MENU, 0);
    }
    break;
  case V4L2_CID_STM_CKEY_MINVAL:
  case V4L2_CID_STM_CKEY_MAXVAL:
    {
      static const char * const name_map[] = { "ColorKey minval",
                                               "ColorKey maxval"
      };
      stm_v4l2_ctrl_query_fill (pqc,
                                name_map[pqc->id - V4L2_CID_STM_CKEY_MINVAL],
                                INT_MIN, INT_MAX, 1, 0, V4L2_CTRL_TYPE_INTEGER,
                                0);
    }
    break;

  default:
    return -EINVAL;
  }

  if (!ctrl_supported)
    pqc->flags |= V4L2_CTRL_FLAG_DISABLED;

  return 0;
}


static const char * const *
stmvout_ctrl_get_menu (u32 id)
{
  static const char * const iqi_config[PCIQIC_COUNT] = {
    [PCIQIC_BYPASS]    = "bypass",
    [PCIQIC_ST_SOFT]   = "soft",
    [PCIQIC_ST_MEDIUM] = "medium",
    [PCIQIC_ST_STRONG] = "strong"
  };
  static const char * const xvp_config[PCxVPC_COUNT] = {
    [PCxVPC_BYPASS]      = "bypass",
    [PCxVPC_FILMGRAIN]   = "filmgrain",
    [PCxVPC_TNR]         = "tnr",
    [PCxVPC_TNR_BYPASS]  = "tnr (bypass)",
    [PCxVPC_TNR_NLEOVER] = "tnr (NLE override)"
  };
  static const char * const dei_mode[PCDEIC_COUNT] = {
    [PCDEIC_3DMOTION] = "enable (3D)",
    [PCDEIC_DISABLED] = "disable",
    [PCDEIC_MEDIAN]   = "enable (median)"
  };
  static const char * const plane_hide_mode[] = {
    [VCSPHM_MIXER_PULLSDATA] = "mixer pulls data",
    [VCSPHM_MIXER_DISABLE] = "fully disabled"
  };
  static const char * const colorkey_enable[] = {
    [VCSCEF_DISABLED]                         = "disable",
    [VCSCEF_ENABLED]                          = "enable on next VSync",
    [VCSCEF_ACTIVATE_BUFFER]                  = "disable",
    [VCSCEF_ACTIVATE_BUFFER | VCSCEF_ENABLED] = "enable on next buffer"
  };
  static const char * const colorkey_format[] = {
    [VCSCFM_RGB]   = "RGB",
    [VCSCFM_CrYCb] = "CrYCb"
  };
  static const char * const colorkey_mode[] = {
    [VCSCCCM_DISABLED]                   = "ignore",
    [VCSCCCM_ENABLED]                    = "enable",
    [VCSCCCM_INVERSE & ~VCSCCCM_ENABLED] = "ignore",
    [VCSCCCM_INVERSE]                    = "inverse"
  };
  static const char * const iqi_enables[] = {
    [VCSIENABLE_NONE] = "none",
    [VCSIENABLE_SINXCOMPENSATION] = "sinx",
    [VCSIENABLE_PEAKING] = "peaking",
    [VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "peaking & sinx",
    [VCSIENABLE_LTI] = "lti",
    [VCSIENABLE_LTI | VCSIENABLE_SINXCOMPENSATION] = "lti & sinx",
    [VCSIENABLE_LTI | VCSIENABLE_PEAKING] = "lti & peaking",
    [VCSIENABLE_LTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "lti & peaking & sinx",
    [VCSIENABLE_CTI] = "cti",
    [VCSIENABLE_CTI | VCSIENABLE_SINXCOMPENSATION] = "cti & sinkx",
    [VCSIENABLE_CTI | VCSIENABLE_PEAKING] = "cti & peaking",
    [VCSIENABLE_CTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "cti & peaking & sinx",
    [VCSIENABLE_CTI | VCSIENABLE_LTI] = "cti & lti",
    [VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_SINXCOMPENSATION] = "cti & lti & sinx",
    [VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_PEAKING] = "cti & lti & peaking",
    [VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "cti & lti & peaking & sinx",
    [VCSIENABLE_LE] = "le",
    [VCSIENABLE_LE | VCSIENABLE_SINXCOMPENSATION] = "le & sinx",
    [VCSIENABLE_LE | VCSIENABLE_PEAKING] = "le & peaking",
    [VCSIENABLE_LE | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "le & peaking & sinx",
    [VCSIENABLE_LE | VCSIENABLE_LTI] = "le & lti",
    [VCSIENABLE_LE | VCSIENABLE_LTI | VCSIENABLE_SINXCOMPENSATION] = "le & lti & sinx",
    [VCSIENABLE_LE | VCSIENABLE_LTI | VCSIENABLE_PEAKING] = "le & lti & peaking",
    [VCSIENABLE_LE | VCSIENABLE_LTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "le & lti & peaking & sinx",
    [VCSIENABLE_LE | VCSIENABLE_CTI] = "le & cti",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_SINXCOMPENSATION] = "le & cti & sinx",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_PEAKING] = "le & cti & peaking",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "le & cti & peaking & sinx",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_LTI] = "le & cti & lti",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_SINXCOMPENSATION] = "le & cti & lti & sinx",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_PEAKING] = "le & cti & lti & peaking",
    [VCSIENABLE_LE | VCSIENABLE_CTI | VCSIENABLE_LTI | VCSIENABLE_PEAKING | VCSIENABLE_SINXCOMPENSATION] = "le & cti & lti & peaking & sinx"
  };
  static const char * const peaking_overundershoot[] = {
    [VCSIPOUSF_100] = "100",
    [VCSIPOUSF_075] = "75",
    [VCSIPOUSF_050] = "50",
    [VCSIPOUSF_025] = "25"
  };
  static const char * const iqi_strength[] = {
    [VCSISTRENGTH_NONE] = "none",
    [VCSISTRENGTH_WEAK] = "weak",
    [VCSISTRENGTH_STRONG] = "strong"
  };
  static const char * const iqi_freq[] = {
    [VCSIPFFREQ_0_15_FsDiv2] = "0.15fs/2",
    [VCSIPFFREQ_0_18_FsDiv2] = "0.18fs/2",
    [VCSIPFFREQ_0_22_FsDiv2] = "0.22fs/2",
    [VCSIPFFREQ_0_26_FsDiv2] = "0.26fs/2",
    [VCSIPFFREQ_0_30_FsDiv2] = "0.30fs/2",
    [VCSIPFFREQ_0_33_FsDiv2] = "0.33fs/2",
    [VCSIPFFREQ_0_37_FsDiv2] = "0.37fs/2",
    [VCSIPFFREQ_0_40_FsDiv2] = "0.40fs/2",
    [VCSIPFFREQ_0_44_FsDiv2] = "0.44fs/2",
    [VCSIPFFREQ_0_48_FsDiv2] = "0.48fs/2",
    [VCSIPFFREQ_0_51_FsDiv2] = "0.51fs/2",
    [VCSIPFFREQ_0_55_FsDiv2] = "0.55fs/2",
    [VCSIPFFREQ_0_58_FsDiv2] = "0.58fs/2",
    [VCSIPFFREQ_0_63_FsDiv2] = "0.63fs/2"
  };
  static const char * const cti_strength[] = {
    [VCSICS_NONE]   = "none",
    [VCSICS_MIN]    = "minimum",
    [VCSICS_MEDIUM] = "medium",
    [VCSICS_STRONG] = "strong"
  };

  switch (id) {
  case V4L2_CID_STM_IQI_SET_CONFIG:
    return iqi_config;
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
  case V4L2_CID_STM_IQI_EXT_ENABLES:
    return iqi_enables;
  case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:
  case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:
    return peaking_overundershoot;
  case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE:
    return iqi_strength;
  case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:
  case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:
    return iqi_freq;
  case V4L2_CID_STM_IQI_EXT_LTI_HMMSPREFILTER:
    return iqi_strength;
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1:
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2:
    return cti_strength;
  }

  return NULL;
}


int
stmvout_querymenu (struct _stvout_device * const device,
                   struct _stvout_device   pipeline_devs[],
                   struct v4l2_querymenu * const qmenu)
{
  struct v4l2_queryctrl qctrl;

  qctrl.id = qmenu->id;
  stmvout_queryctrl (device, pipeline_devs, &qctrl);
  return v4l2_ctrl_query_menu (qmenu, &qctrl,
                               (const char **) stmvout_ctrl_get_menu (qmenu->id));
}


static int
stmvout_write_state (struct _stvout_device_state   * const state,
                     const struct v4l2_ext_control * const ctrl)
{
  switch (ctrl->id) {
  case V4L2_CID_STM_CKEY_ENABLE:
    state->ckey.flags |= SCKCF_ENABLE;
    state->ckey.enable = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_FORMAT:
    state->ckey.flags |= SCKCF_FORMAT;
    state->ckey.format = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_R_CR_MODE:
    state->ckey.flags |= SCKCF_R_INFO;
    state->ckey.r_info = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_G_Y_MODE:
    state->ckey.flags |= SCKCF_G_INFO;
    state->ckey.g_info = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_B_CB_MODE:
    state->ckey.flags |= SCKCF_B_INFO;
    state->ckey.b_info = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_MINVAL:
    state->ckey.flags |= SCKCF_MINVAL;
    state->ckey.minval = ctrl->value;
    break;

  case V4L2_CID_STM_CKEY_MAXVAL:
    state->ckey.flags |= SCKCF_MAXVAL;
    state->ckey.maxval = ctrl->value;
    break;

  /* FMD */
  case V4L2_CID_STM_FMD_T_MOV:    state->new_fmd.t_mov = (u32) ctrl->value;    break;
  case V4L2_CID_STM_FMD_T_NUM_MOV_PIX: state->new_fmd.t_num_mov_pix = (u32) ctrl->value; break;
  case V4L2_CID_STM_FMD_T_REPEAT: state->new_fmd.t_repeat = (u32) ctrl->value; break;
  case V4L2_CID_STM_FMD_T_SCENE:  state->new_fmd.t_scene = (u32) ctrl->value;  break;

  case V4L2_CID_STM_FMD_COUNT_MISS:  state->new_fmd.count_miss = (u32) ctrl->value;  break;
  case V4L2_CID_STM_FMD_COUNT_STILL: state->new_fmd.count_still = (u32) ctrl->value; break;
  case V4L2_CID_STM_FMD_T_NOISE: state->new_fmd.t_noise = (u32) ctrl->value; break;
  case V4L2_CID_STM_FMD_K_CFD1:  state->new_fmd.k_cfd1 = (u32) ctrl->value;  break;
  case V4L2_CID_STM_FMD_K_CFD2:  state->new_fmd.k_cfd2 = (u32) ctrl->value;  break;
  case V4L2_CID_STM_FMD_K_CFD3:  state->new_fmd.k_cfd3 = (u32) ctrl->value;  break;
  case V4L2_CID_STM_FMD_K_SCENE: state->new_fmd.k_scene = (u32) ctrl->value; break;
  case V4L2_CID_STM_FMD_D_SCENE: state->new_fmd.d_scene = (u32) ctrl->value; break;

  /* IQI */
  case V4L2_CID_STM_IQI_EXT_ENABLES:               state->new_iqi.enables = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:    state->new_iqi.peaking.undershoot = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:     state->new_iqi.peaking.overshoot = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING: state->new_iqi.peaking.coring_mode = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:  state->new_iqi.peaking.coring_level = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:      state->new_iqi.peaking.vertical_peaking = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE: state->new_iqi.peaking.clipping_mode = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:  state->new_iqi.peaking.bandpass_filter_centerfreq = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:  state->new_iqi.peaking.highpass_filter_cutofffreq = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN:
  case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN:
  case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN:
    /* -6.0 ... 12.0dB */
    {
    static const enum IQIPeakingHorVertGain core_gains[] = {
      IQIPHVG_N6_0DB, IQIPHVG_N5_5DB, IQIPHVG_N5_0DB, IQIPHVG_N4_5DB,
      IQIPHVG_N4_0DB, IQIPHVG_N3_5DB, IQIPHVG_N3_0DB, IQIPHVG_N2_5DB,
      IQIPHVG_N2_0DB, IQIPHVG_N1_5DB, IQIPHVG_N1_0DB, IQIPHVG_N0_5DB,
      IQIPHVG_0DB, IQIPHVG_P0_5DB, IQIPHVG_P1_0DB, IQIPHVG_P1_5DB,
      IQIPHVG_P2_0DB, IQIPHVG_P2_5DB, IQIPHVG_P3_0DB, IQIPHVG_P3_5DB,
      IQIPHVG_P4_0DB, IQIPHVG_P4_5DB, IQIPHVG_P5_0DB, IQIPHVG_P5_5DB,
      IQIPHVG_P6_0DB, IQIPHVG_P6_5DB, IQIPHVG_P7_0DB, IQIPHVG_P7_5DB,
      IQIPHVG_P8_0DB, IQIPHVG_P8_5DB, IQIPHVG_P9_0DB, IQIPHVG_P9_5DB,
      IQIPHVG_P10_0DB, IQIPHVG_P10_5DB, IQIPHVG_P11_0DB, IQIPHVG_P11_5DB,
      IQIPHVG_P12_0DB };
    s32 idx = (ctrl->value / 5) + 12;
    if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN)
      state->new_iqi.peaking.bandpassgain = core_gains[idx];
    else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN)
      state->new_iqi.peaking.highpassgain = core_gains[idx];
    else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN)
      state->new_iqi.peaking.ver_gain = core_gains[idx];
    }
    break;
  case V4L2_CID_STM_IQI_EXT_LTI_HECGRADIENTOFFSET:
    {
    static const enum IQILtiHecGradientOffset core_offsets[] = {
      IQILHGO_0, IQILHGO_4, IQILHGO_8 };
      s32 idx = (u32) ctrl->value / 4;
    state->new_iqi.lti.selective_edge = core_offsets[idx];
    }
    break;
  case V4L2_CID_STM_IQI_EXT_LTI_HMMSPREFILTER: state->new_iqi.lti.hmms_prefilter = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LTI_ANTIALIASING: state->new_iqi.lti.anti_aliasing = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL: state->new_iqi.lti.vertical = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL_STRENGTH: state->new_iqi.lti.ver_strength = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LTI_HORIZONTAL_STRENGTH: state->new_iqi.lti.hor_strength = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1: state->new_iqi.cti.strength1 = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2: state->new_iqi.cti.strength2 = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN: state->new_iqi.le.csc_gain = (u32) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED: state->new_iqi.le.fixed_curve = ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT: state->new_iqi.le.fixed_curve_params.BlackStretchInflexionPoint = (u16) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT: state->new_iqi.le.fixed_curve_params.BlackStretchLimitPoint = (u16) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT: state->new_iqi.le.fixed_curve_params.WhiteStretchInflexionPoint = (u16) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT: state->new_iqi.le.fixed_curve_params.WhiteStretchLimitPoint = (u16) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN: state->new_iqi.le.fixed_curve_params.BlackStretchGain = (u8) ctrl->value; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN: state->new_iqi.le.fixed_curve_params.WhiteStretchGain = (u8) ctrl->value; break;
  /* CE not supported */

  default:
    return -EINVAL;
  }

  return 0;
}


static int
stmvout_read_state (struct v4l2_ext_control           * const ctrl,
                    const struct _stvout_device_state * const state)
{
  switch (ctrl->id) {
  case V4L2_CID_STM_CKEY_ENABLE:    ctrl->value = state->ckey.enable; break;
  case V4L2_CID_STM_CKEY_FORMAT:    ctrl->value = state->ckey.format; break;
  case V4L2_CID_STM_CKEY_R_CR_MODE: ctrl->value = state->ckey.r_info; break;
  case V4L2_CID_STM_CKEY_G_Y_MODE:  ctrl->value = state->ckey.g_info; break;
  case V4L2_CID_STM_CKEY_B_CB_MODE: ctrl->value = state->ckey.b_info; break;
  case V4L2_CID_STM_CKEY_MINVAL:    ctrl->value = state->ckey.minval; break;
  case V4L2_CID_STM_CKEY_MAXVAL:    ctrl->value = state->ckey.maxval; break;

  /* FMD */
  case V4L2_CID_STM_FMD_T_MOV: ctrl->value = state->old_fmd.t_mov; break;
  case V4L2_CID_STM_FMD_T_NUM_MOV_PIX: ctrl->value = state->old_fmd.t_num_mov_pix; break;
  case V4L2_CID_STM_FMD_T_REPEAT: ctrl->value = state->old_fmd.t_repeat; break;
  case V4L2_CID_STM_FMD_T_SCENE: ctrl->value = state->old_fmd.t_scene; break;

  case V4L2_CID_STM_FMD_COUNT_MISS:  ctrl->value = state->old_fmd.count_miss;  break;
  case V4L2_CID_STM_FMD_COUNT_STILL: ctrl->value = state->old_fmd.count_still; break;
  case V4L2_CID_STM_FMD_T_NOISE: ctrl->value = state->old_fmd.t_noise; break;
  case V4L2_CID_STM_FMD_K_CFD1:  ctrl->value = state->old_fmd.k_cfd1;  break;
  case V4L2_CID_STM_FMD_K_CFD2:  ctrl->value = state->old_fmd.k_cfd2;  break;
  case V4L2_CID_STM_FMD_K_CFD3:  ctrl->value = state->old_fmd.k_cfd3;  break;
  case V4L2_CID_STM_FMD_K_SCENE: ctrl->value = state->old_fmd.k_scene; break;
  case V4L2_CID_STM_FMD_D_SCENE: ctrl->value = state->old_fmd.d_scene; break;

  /* IQI */
  case V4L2_CID_STM_IQI_EXT_ENABLES:               ctrl->value = state->old_iqi.enables; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_UNDERSHOOT:    ctrl->value = state->old_iqi.peaking.undershoot; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_OVERSHOOT:     ctrl->value = state->old_iqi.peaking.overshoot; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_MANUAL_CORING: ctrl->value = state->old_iqi.peaking.coring_mode; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_CORING_LEVEL:  ctrl->value = state->old_iqi.peaking.coring_level; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_VERTICAL:      ctrl->value = state->old_iqi.peaking.vertical_peaking; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_CLIPPING_MODE: ctrl->value = state->old_iqi.peaking.clipping_mode; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSFREQ:  ctrl->value = state->old_iqi.peaking.bandpass_filter_centerfreq; break;
  case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSFREQ:  ctrl->value = state->old_iqi.peaking.highpass_filter_cutofffreq; break;

  case V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN: /* -6.0 ... 12.0dB */
  case V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN: /* -6.0 ... 12.0dB */
  case V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN: /* -6.0 ... 12.0dB */
    {
    static const s32 gains[] = {
      -60, -55, -50, -45, -40, -35, -30, -25, -20, -15, -10, -5, 0, 5, 10,
      15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95,
      100, 105, 110, 115, 120 };
    if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_BANDPASSGAIN)
      ctrl->value = gains[state->old_iqi.peaking.bandpassgain];
    else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_HIGHPASSGAIN)
      ctrl->value = gains[state->old_iqi.peaking.highpassgain];
    else if (ctrl->id == V4L2_CID_STM_IQI_EXT_PEAKING_VGAIN)
      ctrl->value = gains[state->old_iqi.peaking.ver_gain];
    }
    break;

  case V4L2_CID_STM_IQI_EXT_LTI_HECGRADIENTOFFSET:
    ctrl->value = state->old_iqi.lti.selective_edge * 4;
    break;
  case V4L2_CID_STM_IQI_EXT_LTI_HMMSPREFILTER: ctrl->value = state->old_iqi.lti.hmms_prefilter; break;
  case V4L2_CID_STM_IQI_EXT_LTI_ANTIALIASING: ctrl->value = state->old_iqi.lti.anti_aliasing; break;
  case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL: ctrl->value = state->old_iqi.lti.vertical; break;
  case V4L2_CID_STM_IQI_EXT_LTI_VERTICAL_STRENGTH: ctrl->value = state->old_iqi.lti.ver_strength; break;
  case V4L2_CID_STM_IQI_EXT_LTI_HORIZONTAL_STRENGTH: ctrl->value = state->old_iqi.lti.hor_strength; break;
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH1: ctrl->value = state->old_iqi.cti.strength1; break;
  case V4L2_CID_STM_IQI_EXT_CTI_STRENGTH2: ctrl->value = state->old_iqi.cti.strength2; break;
  case V4L2_CID_STM_IQI_EXT_LE_WEIGHT_GAIN: ctrl->value = state->old_iqi.le.csc_gain; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_ENABLED: ctrl->value = state->old_iqi.le.fixed_curve; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHINFLEXIONPOINT: ctrl->value = state->old_iqi.le.fixed_curve_params.BlackStretchInflexionPoint; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHLIMITPOINT: ctrl->value = state->old_iqi.le.fixed_curve_params.BlackStretchLimitPoint; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHINFLEXIONPOINT: ctrl->value = state->old_iqi.le.fixed_curve_params.WhiteStretchInflexionPoint; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHLIMITPOINT: ctrl->value = state->old_iqi.le.fixed_curve_params.WhiteStretchLimitPoint; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_BLACKSTRETCHGAIN: ctrl->value = state->old_iqi.le.fixed_curve_params.BlackStretchGain; break;
  case V4L2_CID_STM_IQI_EXT_LE_FIXCURVE_WHITESTRETCHGAIN: ctrl->value = state->old_iqi.le.fixed_curve_params.WhiteStretchGain; break;
  /* CE not supported */

  default:
    return -EINVAL;
  }

  return 0;
}


static int
stmvout_apply_state (struct _stvout_device       *device,
                     struct _stvout_device_state * const state)
{
  int ret = 0;

  if (device->pPlane) {
    struct StmColorKeyConfig ckey;

    /* color key */
    /* if state is now disabled, or state changed from activate_plane to
       activate_buffer, clear the setting on the plane. */
    if (state->ckey.flags & SCKCF_ENABLE
        && (!(state->ckey.enable & VCSCEF_ENABLED)
            || ((state->ckey.enable & VCSCEF_ACTIVATE_BUFFER)
                && !(device->state.ckey.enable & VCSCEF_ACTIVATE_BUFFER)))) {
      memset (&ckey, 0, sizeof (struct StmColorKeyConfig));
      ckey.flags = SCKCF_ALL;
      ret = stm_display_plane_set_control (device->pPlane,
                                           PLANE_CTRL_COLOR_KEY,
                                           (unsigned long) &ckey);
    } else if (!(state->ckey.enable & VCSCEF_ACTIVATE_BUFFER)) {
      /* otherwise just set it (if not per buffer) */
      ckey = state->ckey;
      ckey.flags &= ~VCSCEF_ACTIVATE_BUFFER;
      ret = stm_display_plane_set_control (device->pPlane,
                                           PLANE_CTRL_COLOR_KEY,
                                           (unsigned long) &ckey);
    }
    if (ret)
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;

#define CHECK_STATE(ctrl, old_state, new_state) \
    if (memcmp (&state->old_state, &state->new_state, sizeof (state->old_state))) { \
      ret = stm_display_plane_set_control (device->pPlane,                          \
                                           ctrl,                                    \
                                           (unsigned long) &state->new_state);      \
      if (ret < 0)                                                                  \
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;                    \
      state->old_state = state->new_state;                                          \
    }

    CHECK_STATE (PLANE_CTRL_DEI_FMD_THRESHS, old_fmd, new_fmd);

    /* IQI */
    if (state->old_iqi.enables != state->new_iqi.enables) {
      ret = stm_display_plane_set_control (device->pPlane,
                                           PLANE_CTRL_IQI_ENABLES,
                                           state->new_iqi.enables);
      if (ret < 0)
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
      state->old_iqi.enables = state->new_iqi.enables;
    }

    CHECK_STATE (PLANE_CTRL_IQI_PEAKING, old_iqi.peaking, new_iqi.peaking);
    CHECK_STATE (PLANE_CTRL_IQI_LTI, old_iqi.lti, new_iqi.lti);
    CHECK_STATE (PLANE_CTRL_IQI_CTI, old_iqi.cti, new_iqi.cti);
    CHECK_STATE (PLANE_CTRL_IQI_LE, old_iqi.le, new_iqi.le);
    /* CE not supported */
#undef CHECK_STATE
  }

  return ret;
}


int
stmvout_s_ctrl (struct _stvout_device * const pDev,
                struct _stvout_device   pipeline_devs[],
                struct v4l2_control    * const pctrl)
{
  ULONG                  ctrlname;
  struct _stvout_device *this_dev = pDev;
  int                    ret;

  BUG_ON (pDev == NULL);

  ctrlname = stmvout_get_ctrl_name (pctrl->id);
  if (ctrlname == 0)
    return -EINVAL;

  switch (pctrl->id) {
  case V4L2_CID_STM_IQI_SET_CONFIG:
  case V4L2_CID_STM_IQI_DEMO:
  case V4L2_CID_STM_XVP_SET_CONFIG:
  case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
  case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
  case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
  case V4L2_CID_STM_DEI_SET_MODE:
  case V4L2_CID_STM_DEI_SET_CTRLREG:

  case V4L2_CID_STM_PLANE_BRIGHTNESS:
  case V4L2_CID_STM_PLANE_CONTRAST:
  case V4L2_CID_STM_PLANE_SATURATION:
  case V4L2_CID_STM_PLANE_HUE:

  case V4L2_CID_STM_PLANE_HIDE_MODE:
    if (!this_dev->pPlane)
      return -EINVAL;

    ret = stm_display_plane_set_control (this_dev->pPlane,
                                         ctrlname, pctrl->value);

    if (ret < 0)
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    break;

  case V4L2_CID_STM_Z_ORDER_FIRST ... V4L2_CID_STM_Z_ORDER_LAST:
    {
      static const stm_plane_id_t id_map[] = {
        OUTPUT_VID1, OUTPUT_VID2,
        OUTPUT_GDP1, OUTPUT_GDP2, OUTPUT_GDP3
      };

      if (pctrl->id > V4L2_CID_STM_Z_ORDER_RGB2)
        return -EINVAL;

      this_dev = _get_device_by_id (pipeline_devs,
                                    id_map[pctrl->id - V4L2_CID_STM_Z_ORDER_VID0]);
      if (!this_dev)
        return -EINVAL;

      if (this_dev != pDev && down_interruptible (&this_dev->devLock))
        return -ERESTARTSYS;

      if (!this_dev->pPlane || !this_dev->pOutput) {
        if (this_dev != pDev)
          up (&this_dev->devLock);
        return -EINVAL;
      }

      ret = stm_display_plane_set_depth (this_dev->pPlane, this_dev->pOutput,
                                         pctrl->value, 1);

      if (this_dev != pDev)
        up (&this_dev->devLock);

      if (ret < 0)
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    }
    break;

  case V4L2_CID_STM_CKEY_ENABLE:
  case V4L2_CID_STM_CKEY_FORMAT:
  case V4L2_CID_STM_CKEY_R_CR_MODE:
  case V4L2_CID_STM_CKEY_G_Y_MODE:
  case V4L2_CID_STM_CKEY_B_CB_MODE:
  case V4L2_CID_STM_CKEY_MINVAL:
  case V4L2_CID_STM_CKEY_MAXVAL:
  case V4L2_CID_STM_FMD_FIRST ... V4L2_CID_STM_FMD_LAST:
  case V4L2_CID_STM_IQI_EXT_FIRST ... V4L2_CID_STM_IQI_EXT_LAST:
    {
      /* compat */
      struct v4l2_ext_controls e2;
      struct v4l2_ext_control e = { .id = pctrl->id };
      e.value = pctrl->value;
      e2.ctrl_class = V4L2_CTRL_ID2CLASS (pctrl->id);
      e2.count = 1;
      e2.controls = &e;
      /* don't do write_state() and apply_state() directly here - we want to
         check parameters. */
      return stmvout_s_ext_ctrls (pDev, &e2);
    }
    break;

  case V4L2_CID_BRIGHTNESS:
  case V4L2_CID_CONTRAST:
  case V4L2_CID_SATURATION:
  case V4L2_CID_HUE:
    if (stm_display_output_set_control (pDev->pOutput,
                                        ctrlname, pctrl->value) < 0) {
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    }
    break;

  default:
    return -EINVAL;
  }

  return 0;
}


int
stmvout_g_ctrl (struct _stvout_device * const pDev,
                struct _stvout_device  pipeline_devs[],
                struct v4l2_control   * const pctrl)
{
  ULONG                  ctrlname;
  struct _stvout_device *this_dev = pDev;
  ULONG                  val;
  int                    ret;

  BUG_ON (pDev == NULL);

  ctrlname = stmvout_get_ctrl_name (pctrl->id);
  if (ctrlname == 0)
    return -EINVAL;

  switch (pctrl->id) {
  case V4L2_CID_STM_IQI_SET_CONFIG:
  case V4L2_CID_STM_IQI_DEMO:
  case V4L2_CID_STM_XVP_SET_CONFIG:
  case V4L2_CID_STM_XVP_SET_TNRNLE_OVERRIDE:
  case V4L2_CID_STM_XVP_SET_TNR_TOPBOTSWAP:
  case V4L2_CID_STM_DEI_SET_FMD_ENABLE:
  case V4L2_CID_STM_DEI_SET_MODE:
  case V4L2_CID_STM_DEI_SET_CTRLREG:

  case V4L2_CID_STM_PLANE_BRIGHTNESS:
  case V4L2_CID_STM_PLANE_CONTRAST:
  case V4L2_CID_STM_PLANE_SATURATION:
  case V4L2_CID_STM_PLANE_HUE:

  case V4L2_CID_STM_PLANE_HIDE_MODE:
    if (!this_dev->pPlane)
      return -EINVAL;

    ret = stm_display_plane_get_control (this_dev->pPlane, ctrlname, &val);

    if (ret < 0)
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    break;

  case V4L2_CID_STM_Z_ORDER_FIRST ... V4L2_CID_STM_Z_ORDER_LAST:
    {
      static const stm_plane_id_t id_map[] = {
        OUTPUT_VID1, OUTPUT_VID2,
        OUTPUT_GDP1, OUTPUT_GDP2, OUTPUT_GDP3
      };

      if (pctrl->id > V4L2_CID_STM_Z_ORDER_RGB2)
        return -EINVAL;

      this_dev = _get_device_by_id (pipeline_devs,
                                    id_map[pctrl->id - V4L2_CID_STM_Z_ORDER_VID0]);
      if (!this_dev)
        return -EINVAL;

      if (this_dev != pDev && down_interruptible (&this_dev->devLock))
        return -ERESTARTSYS;

      if (!this_dev->pPlane || !this_dev->pOutput) {
        if (this_dev != pDev)
          up (&this_dev->devLock);
        return -EINVAL;
      }

      ret = stm_display_plane_get_depth (this_dev->pPlane, this_dev->pOutput,
                                         (int *) &val);

      if (this_dev != pDev)
        up (&this_dev->devLock);

      if (ret < 0)
        return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    }
    break;

  case V4L2_CID_STM_CKEY_ENABLE:
  case V4L2_CID_STM_CKEY_FORMAT:
  case V4L2_CID_STM_CKEY_R_CR_MODE:
  case V4L2_CID_STM_CKEY_G_Y_MODE:
  case V4L2_CID_STM_CKEY_B_CB_MODE:
  case V4L2_CID_STM_CKEY_MINVAL:
  case V4L2_CID_STM_CKEY_MAXVAL:
  case V4L2_CID_STM_FMD_FIRST ... V4L2_CID_STM_FMD_LAST:
  case V4L2_CID_STM_IQI_EXT_FIRST ... V4L2_CID_STM_IQI_EXT_LAST:
    {
      /* compat */
      struct v4l2_ext_control e = { .id = pctrl->id };
      if (stmvout_read_state (&e, &pDev->state))
        return -EINVAL;
      pctrl->value = e.value;
    }
    break;

  case V4L2_CID_BRIGHTNESS:
  case V4L2_CID_CONTRAST:
  case V4L2_CID_SATURATION:
  case V4L2_CID_HUE:
    if (stm_display_output_get_control (pDev->pOutput,
                                        ctrlname, &val) < 0)
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
    break;

  default:
    return -EINVAL;
  }

  pctrl->value = val;

  return 0;
}


static int
stmvout_ext_ctrls (struct _stvout_device_state * const state,
                   struct _stvout_device       * const device,
                   struct v4l2_ext_controls    *ctrls,
                   unsigned int                 cmd)
{
  int err = 0;
  int i;

  if (cmd == VIDIOC_G_EXT_CTRLS) {
    for (i = 0; i < ctrls->count; i++) {
      err = stmvout_read_state (&ctrls->controls[i], state);
      if (err) {
        ctrls->error_idx = i;
        break;
      }
    }

    return err;
  }

  for (i = 0; i < ctrls->count; i++) {
    struct v4l2_ext_control * const e = &ctrls->controls[i];
    struct v4l2_queryctrl    qctrl;
    const char              * const *menu_items = NULL;

    qctrl.id = e->id;
    err = stmvout_queryctrl (device, NULL, &qctrl);
    if (err)
      break;
    if (qctrl.type == V4L2_CTRL_TYPE_MENU)
      menu_items = stmvout_ctrl_get_menu (qctrl.id);
    err = v4l2_ctrl_check (e, &qctrl, (const char **) menu_items);
    if (err)
      break;
    err = stmvout_write_state (state, e);
    if (err)
      break;
  }
  if (err)
    ctrls->error_idx = i;

  return err;
}


int
stmvout_g_ext_ctrls (struct _stvout_device    * const device,
                     struct v4l2_ext_controls *ctrls)
{
  if (ctrls->ctrl_class == V4L2_CTRL_CLASS_USER
      || ctrls->ctrl_class == V4L2_CTRL_CLASS_STMVOUT) {
    int                 i;
    struct v4l2_control ctrl;
    int                 err = 0;

    for (i = 0; i < ctrls->count; i++) {
      ctrl.id = ctrls->controls[i].id;
      ctrl.value = ctrls->controls[i].value;
      err = stmvout_g_ctrl (device, NULL, &ctrl);
      ctrls->controls[i].value = ctrl.value;
      if (err) {
        ctrls->error_idx = i;
        break;
      }
    }
    return err;
  }

  return stmvout_ext_ctrls (&device->state, device, ctrls, VIDIOC_G_EXT_CTRLS);
}


int
stmvout_s_ext_ctrls (struct _stvout_device    * const device,
                     struct v4l2_ext_controls *ctrls)
{
  struct _stvout_device_state state;
  int                         ret;

  if (ctrls->ctrl_class == V4L2_CTRL_CLASS_USER
      || ctrls->ctrl_class == V4L2_CTRL_CLASS_STMVOUT) {
    int                 i;
    struct v4l2_control ctrl;
    int                 err = 0;

    for (i = 0; i < ctrls->count; i++) {
      ctrl.id = ctrls->controls[i].id;
      ctrl.value = ctrls->controls[i].value;
      err = stmvout_s_ctrl (device, NULL, &ctrl);
      ctrls->controls[i].value = ctrl.value;
      if (err) {
        ctrls->error_idx = i;
        break;
      }
    }
    return err;
  }

  state = device->state;
  ret = stmvout_ext_ctrls (&state, device, ctrls, VIDIOC_S_EXT_CTRLS);
  if (!ret) {
    ret = stmvout_apply_state (device, &state);
    device->state = state;
  }

  return ret;
}


int
stmvout_try_ext_ctrls (struct _stvout_device    * const device,
                       struct v4l2_ext_controls *ctrls)
{
  struct _stvout_device_state state;

  if (ctrls->ctrl_class != V4L2_CID_STMVOUTEXT_CLASS)
    return -EINVAL;

  state = device->state;
  return stmvout_ext_ctrls (&state, device, ctrls, VIDIOC_TRY_EXT_CTRLS);
}
