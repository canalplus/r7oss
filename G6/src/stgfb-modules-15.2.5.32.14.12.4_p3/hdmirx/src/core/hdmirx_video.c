/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_video.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/

#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <InternalTypes.h>
#include <hdmirx_RegOffsets.h>

/* Private Typedef -----------------------------------------------*/

//#define ENABLE_DBG_HDRX_VIDEO_INFO

#ifdef ENABLE_DBG_HDRX_VIDEO_INFO
#define DBG_HDRX_VIDEO_INFO	 HDMI_PRINT
#else
#define DBG_HDRX_VIDEO_INFO(a,b...)
#endif

typedef enum
{
  HDMI_VIDEO_MUTE_COLOR_BLACK,
  HDMI_VIDEO_MUTE_COLOR_RED,
  HDMI_VIDEO_MUTE_COLOR_GREEN,
  HDMI_VIDEO_MUTE_COLOR_BLUE
} STHDMIRX_VideoMuteColorType_t;

//Width of HDMI dithering output in bits
typedef enum
{
  HDMI_DITHER_OUT_10BIT = 0,
  HDMI_DITHER_OUT_8BIT
} STHDMIRX_DitherWidth_t;

//HDMI dithering output algorithm
typedef enum
{
  HDMI_DITHER_METHOD_TRUNC = 0,
  HDMI_DITHER_METHOD_ROUND,
  HDMI_DITHER_METHOD_RANDOM,
  HDMI_DITHER_METHOD_SPATIAL
} STHDMIRX_DitherMethod_t;

//Number of frames for HDMI output dithering modulation
typedef enum
{
  HDMI_DITHER_MOD_NONE = 0,
  HDMI_DITHER_MOD_2_FRAME,
  HDMI_DITHER_MOD_4_FRAME,
  HDMI_DITHER_MOD_8_FRAME
} STHDMIRX_DitherModulation_t;
/* Private Defines ------------------------------------------------*/

#define     HDRX_CLK_DIVIDE_2                   0x01
#define     HDRX_VIDEO_PIXEL_FIFO_OFFSET        0x18
#define     HDRX_DVI_CLAMP_WIDTH                0x001f
#define     RGB_BLACK                           0x0000
#define     YUV422_BLACK                        0x8080
#define     YUV444_BLACK                        0x8010

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION     :   sthdmirx_video_subsampler_setup
 USAGE        :   Video Sub Sampling mode.
 INPUT        :   None
 RETURN       :   None
 USED_REGS    :   None
******************************************************************************/
void sthdmirx_video_subsampler_setup(const hdmirx_handle_t Handle,
                                     sthdmirx_sub_sampler_mode_t SubsmplrMode)
{
  if(((hdmirx_route_handle_t *) Handle)->HdmiSubsamplerMode != SubsmplrMode)
    {
      ((hdmirx_route_handle_t *) Handle)->HdmiSubsamplerMode = SubsmplrMode;

      switch (SubsmplrMode)
        {
        case HDMI_SUBSAMPLER_DIV_1:
          DBG_HDRX_VIDEO_INFO("Subsampler: No division\n");
          HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                    HDRX_SUB_SAMPLER), HDRX_SUBSAMPLER_AUTO |
                                   HDRX_VID_CLK_DIVIDE);
          break;

        case HDMI_SUBSAMPLER_DIV_2:
          DBG_HDRX_VIDEO_INFO("Subsampler: Divide by 2\n");
          HDMI_CLEAR_AND_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                            HDRX_SUB_SAMPLER), HDRX_SUBSAMPLER_AUTO |
                                           HDRX_VID_CLK_DIVIDE, HDRX_SUBSAMPLER_SEL |
                                           HDRX_CLK_DIVIDE_2 | HDRX_DATA_SBS_SYNC_EN);
          break;

        case HDMI_SUBSAMPLER_AUTO:
        default:
          DBG_HDRX_VIDEO_INFO("Subsampler: Auto division\n");
          HDMI_CLEAR_AND_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(Handle) +
                                            HDRX_SUB_SAMPLER), HDRX_VID_CLK_DIVIDE,
                                            HDRX_SUBSAMPLER_SEL | HDRX_SUBSAMPLER_AUTO);
          break;
        }
    }
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_pipe_init
 USAGE        :   Initialize the Video pipe Hardware.
 INPUT        :   None
 RETURN       :   None
 USED_REGS    :   None
******************************************************************************/
void sthdmirx_video_pipe_init(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *pInpHandle;
  pInpHandle = (hdmirx_route_handle_t *) Handle;

  pInpHandle->HdmiSubsamplerMode = HDMI_SUBSAMPLER_NONE;

  /* 1. HS, VS & DE regeneartion Enabled
     2. DE signal is decoded from DVi signal
     3. HDRX_VID_REGEN_CTRL.HDRX_DE_WIDTH_CLAMP_EN =1, then clamed DE width is contolled by */
  HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VID_REGEN_CTRL),
                      (HDRX_DVI_HS_REGEN | HDRX_DVI_VS_REGEN));
  HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress + HDRX_DE_WIDTH_CLAMP),
                      HDRX_DVI_CLAMP_WIDTH);

  /*Set the Video Pattern generator */
  HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_PATGEN_CONTROL),
                      0x00);

  if (pInpHandle->HdmiRxMode != STM_HDMIRX_ROUTE_OP_MODE_DVI)
    {
      /* Automatic applying division for HDMI mode from AVI and division is done by VDDS */
      sthdmirx_video_subsampler_setup(Handle, HDMI_SUBSAMPLER_AUTO);
      /* Video buffer overrun restore enable */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VID_CONV_CTRL),
                          (HDRX_VID_BUF_CORRECT_MD | HDRX_VID_ERR_DIS));
      /* Offset for video pixel fifo */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VID_BUF_OFFSET),
                          HDRX_VIDEO_PIXEL_FIFO_OFFSET);
      /* When video mute comes, set the no of frames video mute */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress +
                                HDRX_MD_CHNG_MT_DEL), 0x00);

      /*Video buffer Error filteration -> Need to define for freeman project */
      /*gm_Hdmi_WriteRegByte((U32)(pInpHandle->BaseAddress+HDRX_VID_BUF_FILT_ZONE), 0x18); */

      /* Program the HDMIRx video block controls */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VIDEO_CTRL),
                          (HDRX_VID_VMUTE_AUTO | HDRX_VID_COLOR_AUTO |
                           HDRX_VID_SWAP_R_B_AUTO));

      /* Video Mute colors */
      HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress +
                                HDRX_VIDEO_MUTE_RGB), RGB_BLACK);
      HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress + HDRX_VIDEO_MUTE_YC),
                          YUV444_BLACK);

      /*Video Dither control, this block is applicable only for 444 */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_DITHER_CTRL),
                          0x00);

      /* Data island controls */
      /* Deep Color mode enable, Threshold ( No of frames) to provide the deep color status */
      HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress + HDRX_SDP_CTRL),
                          HDRX_GCP_FRM_TRESH | HDRX_ACR_NCTS_PROG_EN | HDRX_ACR_DISABLE_AUTO_NCTS_STROBE);

      /*Av mute control setup */
      HDMI_WRITE_REG_WORD((U32) (pInpHandle->BaseAddress +
                                 HDRX_SDP_AV_MUTE_CTRL), (HDRX_AV_MUTE_WINDOW_EN |
                                     HDRX_AV_MUTE_AUTO_EN | HDRX_A_MUTE_ON_DVI_EN | HDRX_CLR_AVMUTE_ON_TMT_DIS));
      /*storing undefined packet info */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_PACK_CTRL),
                          0x00);
      /* Packet protection options */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress +
                                HDRX_PACK_GUARD_CTRL), (HDRX_PACKET_SAVE_OPTION |
                                    HDRX_GCP_FOUR_BT_CTRL | HDRX_ACR_FOUR_BT_CTRL));
      /* Set parameters for noise detection */
      HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress + HDRX_NOISE_DET_CTRL),
                          (HDRX_NOISE_DETECTOR_EN | HDRX_NOISE_AUTO_AV_MUTE_EN |
                           0x1100));
      /* Enable authomatic recieving AVI */
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress +
                                HDRX_AUTO_PACK_PROCESS), (HDRX_AVI_PROCESS_MODE |
                                    HDRX_CLR_AVI_INFO_TMOUT_EN | HDRX_CLR_AU_INFO_TMOUT_EN |
                                    (1 << HDRX_AVI_PACK_STABLE_THRESH_SHIFT)));
      HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress +
                                HDRX_AVI_INFOR_TIMEOUT_THRESH), 0xF);

    }
  else
    {
      /*Setup for manual mode, no division. For HDMI will be setup later to auto mode */
      sthdmirx_video_subsampler_setup(Handle, HDMI_SUBSAMPLER_DIV_1);
    }

  /* Trigger on line 10 */
  HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDMI_INPUT_FLAGLINE), 0xA);

  DBG_HDRX_VIDEO_INFO("HDMIRx Video Pipe Init\n");
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_set_mute_color
 USAGE        :   Set the Video mute Color
 INPUT        :   None
 RETURN       :   None
 USED_REGS    :   None
******************************************************************************/

void sthdmirx_video_set_mute_color(const hdmirx_handle_t Handle,
                                   STHDMIRX_VideoMuteColorType_t bRGB,
                                   STHDMIRX_VideoMuteColorType_t YCbCr)
{
  DBG_HDRX_VIDEO_INFO(" TODO : Will be added at later stage\n");
  UNUSED_PARAMETER(Handle);
  UNUSED_PARAMETER(bRGB);
  UNUSED_PARAMETER(YCbCr);
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_set_dithering
 USAGE        :   Set the Video dithering mode.
 INPUT        :   None
 RETURN       :   None
 USED_REGS    :   HDRX_DITHER_CTRL
******************************************************************************/
void sthdmirx_video_set_dithering(const hdmirx_handle_t Handle,
                                  BOOL B_Enable,
                                  STHDMIRX_DitherWidth_t W_DitherWidth,
                                  STHDMIRX_DitherMethod_t W_DitherMethod,
                                  STHDMIRX_DitherModulation_t W_DitherMod)
{
  U8 B_HdmiDitherCtrl = (U8) ((B_Enable) ? HDRX_DITHER_EN : 0);

  B_HdmiDitherCtrl |=
    (W_DitherWidth << 1) | (W_DitherMethod << HDRX_DITH_SEL_SHIFT) |
    (W_DitherMod << HDRX_DITH_MOD_SHIFT);

  HDMI_WRITE_REG_BYTE((GET_CORE_BASE_ADDRS(Handle) + HDRX_DITHER_CTRL),
                      B_HdmiDitherCtrl);

  DBG_HDRX_VIDEO_INFO("API call: gm_HdmiVideoSetDithering, HDMI_DITHER_CTRL = 0x%x\n",
                      HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(Handle) + HDRX_DITHER_CTRL));
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_HW_pixel_repeatfactor_get
 USAGE        :   Get Pixel Repeat Factor from Hdmi Hardware
 INPUT        :   None
 RETURN       :   Pixel repeat factor.
 USED_REGS    :   HDRX_AVI_AUTO_FIELDS
******************************************************************************/
stm_hdmirx_pixel_repeat_factor_t sthdmirx_video_HW_pixel_repeatfactor_get(const
    hdmirx_handle_t Handle)
{
  stm_hdmirx_pixel_repeat_factor_t stPixRepeat;
  stPixRepeat = HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                         HDRX_AVI_AUTO_FIELDS)) & HDRX_AVI_PR_CTRL;
  return stPixRepeat;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_HW_color_depth_get
 USAGE        :   Get color depth status from Hdmi Hardware
 INPUT        :   None
 RETURN       :   24bpp/30bpp/36bpp/48bpp
 USED_REGS    :   HDRX_COLOR_DEPH_STATUS
******************************************************************************/
U8 sthdmirx_video_HW_color_depth_get(const hdmirx_handle_t Handle)
{
  U8 stColorDepth;
  stColorDepth = HDMI_READ_REG_BYTE((U32)(GET_CORE_BASE_ADDRS(Handle) +
                                          HDRX_COLOR_DEPH_STATUS)) & HDRX_VID_COLOR_DEPH;
  return stColorDepth;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_HW_color_space_get
 USAGE        :   Get color space status from Hdmi Hardware
 INPUT        :   None
 RETURN       :   RGB/YUV444/YUV422
 USED_REGS    :   HDRX_AVI_AUTO_FIELDS
******************************************************************************/
stm_hdmirx_color_format_t sthdmirx_video_HW_color_space_get(const
    hdmirx_handle_t Handle)
{
  stm_hdmirx_color_format_t stColorSpace;
  stColorSpace = (HDMI_READ_REG_BYTE((U32) (GET_CORE_BASE_ADDRS(Handle) +
                                     HDRX_AVI_AUTO_FIELDS)) & HDRX_AVI_YUV_CTRL) >> 4;
  return stColorSpace;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_HW_AVmute_status_get
 USAGE        :   Get AV mute status from Hdmi Hardware
 INPUT        :   None
 RETURN       :   TRUE: AV mute is set, FALSE =  No AV Mute signal
 USED_REGS    :   HDRX_SDP_STATUS
******************************************************************************/
BOOL sthdmirx_video_HW_AVmute_status_get(const hdmirx_handle_t Handle)
{
  BOOL bIsAvmute;
  bIsAvmute = (HDMI_READ_REG_BYTE((U32) (GET_CORE_BASE_ADDRS(Handle) + HDRX_SDP_STATUS)) & HDRX_AV_MUTE_GCP_STATUS) ? TRUE : FALSE;
  return bIsAvmute;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_enable_raw_sync
 USAGE        :   Enable raw sync for australian modes
 INPUT        :   None
 RETURN       :
 USED_REGS    :   HDRX_VID_REGEN_CTRL
******************************************************************************/
void sthdmirx_video_enable_raw_sync(const hdmirx_route_handle_t *pInpHandle)
{
  HDMI_CLEAR_REG_BITS_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VID_REGEN_CTRL),
                           (HDRX_DVI_HS_REGEN | HDRX_DVI_VS_REGEN));
  return;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_video_enable_regen_sync
 USAGE        :   Enable regenerated sync
 INPUT        :   None
 RETURN       :
 USED_REGS    :   HDRX_VID_REGEN_CTRL
******************************************************************************/
void sthdmirx_video_enable_regen_sync(const hdmirx_route_handle_t *pInpHandle)
{
  HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_VID_REGEN_CTRL),
                      (HDRX_DVI_HS_REGEN | HDRX_DVI_VS_REGEN));
  return;
}

/*End of File*/
