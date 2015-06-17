/***********************************************************************
 *
 * File: linux/drivers/media/video/stmvout_display.c
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

#include <stmdisplay.h>

#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "stmvout.h"

struct standard_list_entry_s
{
  stm_display_mode_t  mode;
  ULONG               tvStandard;
  v4l2_std_id         v4l_standard;
  const char         *name;
};

static const struct standard_list_entry_s standard_list[] = {
  { STVTG_TIMING_MODE_480I59940_13500,   STM_OUTPUT_STD_NTSC_M,    V4L2_STD_NTSC_M,      "NTSC-M"      },
  { STVTG_TIMING_MODE_480I59940_13500,   STM_OUTPUT_STD_NTSC_J,    V4L2_STD_NTSC_M_JP,   "NTSC-J"      },
  { STVTG_TIMING_MODE_480I59940_13500,   STM_OUTPUT_STD_NTSC_443,  V4L2_STD_NTSC_443,    "NTSC-443"    },
  { STVTG_TIMING_MODE_480I59940_13500,   STM_OUTPUT_STD_PAL_M,     V4L2_STD_PAL_M,       "PAL-M"       },
  { STVTG_TIMING_MODE_480I59940_13500,   STM_OUTPUT_STD_PAL_60,    V4L2_STD_PAL_60,      "PAL-60"      },
  { STVTG_TIMING_MODE_576I50000_13500,   STM_OUTPUT_STD_PAL_BDGHI, V4L2_STD_PAL,         "PAL"         },
  { STVTG_TIMING_MODE_576I50000_13500,   STM_OUTPUT_STD_PAL_N,     V4L2_STD_PAL_N,       "PAL-N"       },
  { STVTG_TIMING_MODE_576I50000_13500,   STM_OUTPUT_STD_PAL_Nc,    V4L2_STD_PAL_Nc,      "PAL-Nc"      },
  { STVTG_TIMING_MODE_576I50000_13500,   STM_OUTPUT_STD_SECAM,     V4L2_STD_SECAM,       "SECAM"       },
  { STVTG_TIMING_MODE_480P60000_27027,   STM_OUTPUT_STD_SMPTE293M, V4L2_STD_480P_60,     "480p@60"     },
  { STVTG_TIMING_MODE_480P59940_27000,   STM_OUTPUT_STD_SMPTE293M, V4L2_STD_480P_59_94,  "480p@59.94"  },
  { STVTG_TIMING_MODE_576P50000_27000,   STM_OUTPUT_STD_SMPTE293M, V4L2_STD_576P_50,     "576p"        },
  { STVTG_TIMING_MODE_1080P60000_148500, STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_60,    "1080p@60"    },
  { STVTG_TIMING_MODE_1080P59940_148352, STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_59_94, "1080p@59.94" },
  { STVTG_TIMING_MODE_1080P50000_148500, STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_50,    "1080p@50"    },
  { STVTG_TIMING_MODE_1080P30000_74250,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_30,    "1080p@30"    },
  { STVTG_TIMING_MODE_1080P29970_74176,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_29_97, "1080p@29.97" },
  { STVTG_TIMING_MODE_1080P25000_74250,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_25,    "1080p@25"    },
  { STVTG_TIMING_MODE_1080P24000_74250,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_24,    "1080p@24"    },
  { STVTG_TIMING_MODE_1080P23976_74176,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080P_23_98, "1080p@23.98" },
  { STVTG_TIMING_MODE_1080I60000_74250,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080I_60,    "1080i@60"    },
  { STVTG_TIMING_MODE_1080I59940_74176,  STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080I_59_94, "1080i@59.94" },
  { STVTG_TIMING_MODE_1080I50000_74250_274M, STM_OUTPUT_STD_SMPTE274M, V4L2_STD_1080I_50,"1080i@50"    },
  { STVTG_TIMING_MODE_720P60000_74250,   STM_OUTPUT_STD_SMPTE296M, V4L2_STD_720P_60,     "720p@60"     },
  { STVTG_TIMING_MODE_720P59940_74176,   STM_OUTPUT_STD_SMPTE296M, V4L2_STD_720P_59_94,  "720p@59.94"  },
  { STVTG_TIMING_MODE_720P50000_74250,   STM_OUTPUT_STD_SMPTE296M, V4L2_STD_720P_50,     "720p@50"     },
  { STVTG_TIMING_MODE_480P60000_25200,   STM_OUTPUT_STD_VESA,      V4L2_STD_VGA_60,      "VGA@60"      },
  { STVTG_TIMING_MODE_480P59940_25180,   STM_OUTPUT_STD_VESA,      V4L2_STD_VGA_59_94,   "VGA@59.94"   },
};

/******************************************************************************
 * stmvout_set_video_output
 *
 * helper use to select a display pipeline output to use for this device.
 */
int stmvout_set_video_output(stvout_device_t *pDev, enum STM_VOUT_OUTPUT_ID id)
{
  void *newoutput;
  int outputnr;

  debug_msg("%s: pDev = %p id = %d\n",__FUNCTION__,pDev,id);

  if(pDev->currentOutputNum == id)
  {
    /*
     * Already connected to the required output, do nothing
     */
    return 0;
  }

  switch(id)
  {
    case SVOI_MAIN:
      outputnr = pDev->displayPipe.main_output_id;
      break;
    case SVOI_AUX:
    default:
      return -ENODEV;
  }

  debug_msg("%s: outputnr = %d device = %p\n",__FUNCTION__, outputnr, pDev->displayPipe.device);

  newoutput = stm_display_get_output(pDev->displayPipe.device, outputnr);
  if(!newoutput)
    return signal_pending(current) ? -ERESTARTSYS : -EINVAL;

  debug_msg("%s: pOutput = %p\n",__FUNCTION__, newoutput);

  pDev->currentOutputNum = id;

  if(pDev->pOutput)
    stm_display_output_release(pDev->pOutput);

  pDev->pOutput = newoutput;

  return 0;
}


/******************************************************************************
 *  stmvout_get_supported_standards - ioctl helper for VIDIOC_ENUMOUTPUT
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_supported_standards(stvout_device_t *pDev, v4l2_std_id *ids)
{
  ULONG caps;
  *ids = V4L2_STD_UNKNOWN;

  if(stm_display_output_get_capabilities(pDev->pOutput,&caps)<0)
  {
    debug_msg("%s @ %p: Unable to get output capabilities\n",__FUNCTION__,pDev);
    return signal_pending(current) ? -ERESTARTSYS : -EIO;
  }

  debug_msg("%s @ %p: Output capabilities = 0x%lx\n",__FUNCTION__,pDev,caps);

  if(caps & STM_OUTPUT_CAPS_SD_ANALOGUE)
  {
    debug_msg("%s @ %p: adding SD standards\n",__FUNCTION__,pDev);
    *ids |= (V4L2_STD_525_60 | V4L2_STD_625_50);
  }

  if(caps & STM_OUTPUT_CAPS_ED_ANALOGUE)
  {
    debug_msg("%s @ %p: adding ED standards\n",__FUNCTION__,pDev);
    *ids |= V4L2_STD_SMPTE293M;
  }

  if(caps & STM_OUTPUT_CAPS_HD_ANALOGUE)
  {
    debug_msg("%s @ %p: adding HD standards\n",__FUNCTION__,pDev);
    *ids |= (V4L2_STD_VESA        |
             V4L2_STD_SMPTE296M   |
             V4L2_STD_1080P_23_98 |
             V4L2_STD_1080P_24    |
             V4L2_STD_1080P_25    |
             V4L2_STD_1080P_29_97 |
             V4L2_STD_1080P_30    |
             V4L2_STD_1080I_60    |
             V4L2_STD_1080I_59_94 |
             V4L2_STD_1080I_50);

    /*
     * Test to see if the high framerate 1080p modes are available by trying
     * to get the mode descriptor for one, this returns 0 if the hardware cannot
     * support it.
     * FIXME: returns 0, too, if the device lock couldn't be obtained! Should
     * we retry in this case?
     */
    if(stm_display_output_get_display_mode(pDev->pOutput, STVTG_TIMING_MODE_1080P60000_148500) != 0)
    {
      debug_msg("%s @ %p: adding fast 1080p standards\n",__FUNCTION__,pDev);
      *ids |= (V4L2_STD_1080P_60    |
               V4L2_STD_1080P_59_94 |
               V4L2_STD_1080P_50);
    }
  }

  return 0;
}


/******************************************************************************
 *  stmvout_set_current_standard - ioctl helper for VIDIOC_S_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_set_current_standard(stvout_device_t *pDev, v4l2_std_id stdid)
{
  int i;
  const stm_mode_line_t* pModeLine = 0;
  const stm_mode_line_t* pCurrentModeLine;
  ULONG tvStandard;

  pCurrentModeLine = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(signal_pending(current))
    return -ERESTARTSYS;

  if(stm_display_output_get_current_tv_standard(pDev->pOutput, &tvStandard)<0)
    return -ERESTARTSYS;

  if(stdid == V4L2_STD_UNKNOWN)
  {
    if(pCurrentModeLine)
    {
      if(stm_display_output_stop(pDev->pOutput)<0)
        return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
    }

    return 0;
  }

  for(i=0; i < ARRAY_SIZE (standard_list); i++)
  {
    if((standard_list[i].v4l_standard & stdid) == stdid)
    {
      debug_msg("%s: found mode = %d tvStandard = 0x%lx (%s) i = %d stdid = 0x%llx\n",
                __FUNCTION__, (s32)standard_list[i].mode,
                standard_list[i].tvStandard, standard_list[i].name, i, stdid);

      pModeLine = stm_display_output_get_display_mode(pDev->pOutput, standard_list[i].mode);
      if(!pModeLine)
      {
        debug_msg("%s: std %llu not supported\n", __FUNCTION__, stdid);
        return signal_pending(current) ? -ERESTARTSYS : -EINVAL;
      }

      break;
    }
  }

  if(i == ARRAY_SIZE (standard_list))
  {
    debug_msg("%s: std not found stdid = 0x%llx\n", __FUNCTION__, stdid);
    return -EINVAL;
  }

  BUG_ON(pModeLine == NULL);

  /*
   * If there is nothing to do, just return
   */
  if((pModeLine == pCurrentModeLine) && (standard_list[i].tvStandard == tvStandard))
    return 0;

  /*
   * If the timing mode has changed and the display is running, try and stop
   * it first. If we cannot stop the display it means a plane is active so
   * return EBUSY.
   */
  if(pCurrentModeLine && (pModeLine != pCurrentModeLine))
  {
    if(stm_display_output_stop(pDev->pOutput)<0)
      return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
  }

  if(stm_display_output_start(pDev->pOutput, pModeLine, standard_list[i].tvStandard)<0)
  {
    err_msg("%s: cannot start standard (%d) '%s'\n",__FUNCTION__, i, standard_list[i].name);
    return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
  }

  debug_msg("%s: std started ok stdid = 0x%llx\n", __FUNCTION__, stdid);

  return 0;
}


/******************************************************************************
 *  stmvout_get_current_standard - ioctl helper for VIDIOC_G_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_current_standard(stvout_device_t *pDev, v4l2_std_id *stdid)
{
  int i;
  unsigned long tvStandard;
  const stm_mode_line_t* pModeLine;

  pModeLine = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pModeLine)
  {
    if(signal_pending(current))
      return -ERESTARTSYS;

    /*
     * Display pipeline is stopped, so return V4L2_STD_UNKNOWN, there is no
     * V4L2_STD_NONE unfortunately.
     */
    *stdid = V4L2_STD_UNKNOWN;
    return 0;
  }

  if(stm_display_output_get_current_tv_standard(pDev->pOutput, &tvStandard)<0)
    return -ERESTARTSYS;

  if(tvStandard & STM_OUTPUT_STD_SMPTE293M)
    tvStandard = STM_OUTPUT_STD_SMPTE293M;

  for(i=0; i < ARRAY_SIZE (standard_list); i++)
  {
    if(standard_list[i].mode == pModeLine->Mode &&
       standard_list[i].tvStandard == tvStandard)
    {
      debug_msg("%s: found mode = %d tvStandard = 0x%lx i = %d stdid = 0x%llx\n",
                __FUNCTION__,(s32)pModeLine->Mode, tvStandard, i,standard_list[i].v4l_standard);
      *stdid = standard_list[i].v4l_standard;
      return 0;
    }
  }

  return -EIO;
}


/******************************************************************************
 *  stmvout_get_display_standard - ioctl helper VIDIOC_ENUM_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_display_standard(stvout_device_t       *pDev,
                                 struct v4l2_standard  *std)
{
  int i,found;
  const stm_mode_line_t* pModeLine = 0;

  debug_msg("%s: pDev = %p std = %p\n",__FUNCTION__,pDev, std);

  BUG_ON(pDev == NULL || std == NULL);

  debug_msg("%s: index = %d\n",__FUNCTION__,std->index);

  for(i=0,found=-1; i < ARRAY_SIZE (standard_list);i++)
  {
    /*
     * stm_display_output_get_display_mode() only returns a valid mode line
     * if the requested mode is available on this output. So we keep trying
     * modes until we have found the nth one to match the enumeration index
     * in the v4l2_standard structure.
     */
    pModeLine = stm_display_output_get_display_mode(pDev->pOutput, standard_list[i].mode);
    if(signal_pending(current))
      return -ERESTARTSYS;

    if(pModeLine)
    {
      found++;
      if(found == (s32)std->index)
        break;
    }
  }


  if(i == ARRAY_SIZE (standard_list))
  {
    debug_msg("%s: end of list\n",__FUNCTION__);
    return -EINVAL;
  }

  debug_msg("%s: i = %d found = %d pModeLine = %p\n",__FUNCTION__,i,found,pModeLine);

  BUG_ON(pModeLine == NULL);

  strcpy(std->name, standard_list[i].name);
  std->id                      = standard_list[i].v4l_standard;
  std->framelines              = pModeLine->TimingParams.LinesByFrame;
  std->frameperiod.numerator   = 1000;
  std->frameperiod.denominator = pModeLine->ModeParams.FrameRate;

  if(pModeLine->ModeParams.ScanType == SCAN_I)
    std->frameperiod.denominator = std->frameperiod.denominator/2;

  return 0;
}


/******************************************************************************
 *  stmvout_get_display_size - ioctl helper for VIDIOC_CROPCAP
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_display_size(stvout_device_t *pDev, struct v4l2_cropcap *cap)
{
  const stm_mode_line_t *pCurrentMode;
  ULONG tvStandard;
  LONG  numerator;
  ULONG denominator;

  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
    return signal_pending(current) ? -ERESTARTSYS : -EIO;

  if(stm_display_output_get_current_tv_standard(pDev->pOutput, &tvStandard)<0)
    return -ERESTARTSYS;

  /*
   * The output bounds and default output size are the full active video area
   * for the display mode with an origin at (0,0). The display driver will
   * position plane horizontally and vertically on the display, based on the
   * current display mode.
   */
  cap->bounds.left   = 0;
  cap->bounds.top    = 0;
  cap->bounds.width  = pCurrentMode->ModeParams.ActiveAreaWidth;
  cap->bounds.height = pCurrentMode->ModeParams.ActiveAreaHeight;

  cap->defrect = cap->bounds;

  /*
   * We have a pixel aspect ratio in the mode line, but it is the ratio x/y
   * to convert the image ratio to the picture aspect ratio, i.e. 4:3 or 16:9.
   * This is not what the V4L2 API reports, it reports the ratio y/x (yes the
   * other way about) to convert from the pixel sampling frequency to square
   * pixels. But we use the mode line pixel aspect ratios to determine square
   * pixel modes.
   */
  numerator   = pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_4_3].numerator;
  denominator = pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_4_3].denominator;

  if(denominator == 0)
  {
    /*
     * This must be a 16:9 only HD or VESA mode, which means it should always
     * have a 1:1 pixel aspect ratio.
     */
    numerator   = pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_16_9].numerator;
    denominator = pCurrentMode->ModeParams.PixelAspectRatios[AR_INDEX_16_9].denominator;
  }

  if(numerator == 1 && denominator == 1)
  {
    cap->pixelaspect.numerator   = 1;
    cap->pixelaspect.denominator = 1;
  }
  else
  {
    /*
     * We assume that the SMPTE293M SD progressive modes have the
     * same aspect ratio as the SD interlaced modes.
     */
    if(pCurrentMode->TimingParams.LinesByFrame == 525)
    {
      cap->pixelaspect.numerator   = 11; /* From V4L2 API Spec for NTSC(-J) */
      cap->pixelaspect.denominator = 10;
    }
    else if(pCurrentMode->TimingParams.LinesByFrame == 625)
    {
      cap->pixelaspect.numerator   = 54; /* From V4L2 API Spec for PAL/SECAM */
      cap->pixelaspect.denominator = 59;
    }
    else
    {
      /*
       * Err, don't know, default to 1:1
       */
      cap->pixelaspect.numerator   = 1;
      cap->pixelaspect.denominator = 1;
    }
  }

  return 0;
}
