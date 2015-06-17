/***********************************************************************
 *
 * File: hdmirx/src/system/hdmirx_event.c
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
#include <hdmirx_core_export.h>
#include <InternalTypes.h>
#include <stm_hdmirx_os.h>
#include <hdmirx_Combophy.h>

/* Private Typedef -----------------------------------------------*/

//#define ENABLE_DBG_HDRX_EVENT_INFO

#ifdef ENABLE_DBG_HDRX_EVENT_INFO
#define DBG_HDRX_EVENT_INFO	 HDMI_PRINT
#else
#define DBG_HDRX_EVENT_INFO(a,b...)
#endif

/* Private Defines ------------------------------------------------*/

#define HDRX_VENDOR_NAME_LENGTH                 8
#define HDRX_PRODUCT_DESCRIPTION_LENGTH         16
#define HDRX_VENDORSPECIFIC_PAYLOAD_LENGTH      24
#define HDRX_ISRC_DATA_LENGTH                   31
#define HDRX_ACP_INFOFRAME_DATA_LENGTH	        16
#define HDRX_GBD_INFOFRAME_DATA_LENGTH	        28

/* Private macro's ------------------------------------------------*/

#define ENABLE_3D_TIMIMG_MEAS_CNTRL_OVERFLOW_FIX

/* Private Variables -----------------------------------------------*/
stm_hdmirx_aspect_ratio_t const a_AspectRatio[16] =
{
  STM_HDMIRX_ASPECT_RATIO_NONE, STM_HDMIRX_ASPECT_RATIO_NONE,
  STM_HDMIRX_ASPECT_RATIO_16_9_TOP,STM_HDMIRX_ASPECT_RATIO_14_9_TOP,
  STM_HDMIRX_ASPECT_RATIO_16_9_CENTER, STM_HDMIRX_ASPECT_RATIO_NONE,
  STM_HDMIRX_ASPECT_RATIO_NONE, STM_HDMIRX_ASPECT_RATIO_NONE,
  STM_HDMIRX_ASPECT_RATIO_NONE, STM_HDMIRX_ASPECT_RATIO_4_3_CENTER,
  STM_HDMIRX_ASPECT_RATIO_16_9_CENTER,STM_HDMIRX_ASPECT_RATIO_14_9_CENTER,
  STM_HDMIRX_ASPECT_RATIO_NONE, STM_HDMIRX_ASPECT_RATIO_4_3_CENTER_14_9_SP,
  STM_HDMIRX_ASPECT_RATIO_16_9_CENTER_14_9_SP,
  STM_HDMIRX_ASPECT_RATIO_16_9_CENTER__4_3_SP
};

stm_hdmirx_3d_format_t const a_3DFormat[16] =
{
  STM_HDMIRX_3D_FORMAT_FRAME_PACK, STM_HDMIRX_3D_FORMAT_FIELD_ALT,
  STM_HDMIRX_3D_FORMAT_LINE_ALT, STM_HDMIRX_3D_FORMAT_SBYS_FULL,
  STM_HDMIRX_3D_FORMAT_L_D, STM_HDMIRX_3D_FORMAT_L_D_G_GMINUSD,
  STM_HDMIRX_3D_FORMAT_TOP_AND_BOTTOM, STM_HDMIRX_3D_FORMAT_UNKNOWN,
  STM_HDMIRX_3D_FORMAT_SBYS_HALF, STM_HDMIRX_3D_FORMAT_UNKNOWN,
  STM_HDMIRX_3D_FORMAT_UNKNOWN, STM_HDMIRX_3D_FORMAT_UNKNOWN,
  STM_HDMIRX_3D_FORMAT_UNKNOWN, STM_HDMIRX_3D_FORMAT_UNKNOWN,
  STM_HDMIRX_3D_FORMAT_UNKNOWN, STM_HDMIRX_3D_FORMAT_UNKNOWN
};

/* Private functions prototypes ------------------------------------*/
void sthdmirx_print_AVI_frame_packet(hdmirx_route_handle_t *const pInpHandle);
void sthdmirx_print_audio_info_frame_packet(
  hdmirx_route_handle_t *const pInpHandle);
void sthdmirx_print_audio_clk_regeneration(
  hdmirx_route_handle_t *const pInpHandle);
void sthdmirx_print_video_prop_packed_data(
  stm_hdmirx_video_property_t *vidPropData);
void sthdmirx_audio_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_audio_property_t *pAudPropData);
void sthdmirx_video_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_video_property_t *pVidPropData);
void sthdmirx_VS_info_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                stm_hdmirx_vs_info_t *VsInfo);
void sthdmirx_SPD_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_spd_info_t *SpdInfo);
void sthdmirx_MPEG_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                        stm_hdmirx_mpeg_source_info_t *MpegInfo);
void sthdmirx_ACP_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_acp_info_t *AcpInfo);
void sthdmirx_GBD_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_gbd_info_t *GbdInfo);
void sthdmirx_ISRC_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                        stm_hdmirx_isrc_info_t *ISRCInfo);
void sthdmirx_signal_timing_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                      stm_hdmirx_signal_timing_t *pSigTimingData);

/* Interface procedures/functions --------------------------------------------------*/

/******************************************************************************
 FUNCTION     	:   sthdmirx_audio_prop_changed_data_fill
 USAGE        	:   Fills the Audio property data from local storage to the pointer buffer
 INPUT        	:   pointer to audio propert structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_audio_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_audio_property_t *pAudPropData)
{
  U32 aSampleFre[] =
  {
    0, 32000UL, 44100UL, 48000UL, 88200UL, 96000UL, 176400UL,
    192000UL, 22050UL, 24000UL, 8000, 768000UL, 0, 0
  };

  pAudPropData->stream_type =
    pInpHandle->stAudioMngr.stAudioConfig.eHdmiAudioSelectedStream;
  pAudPropData->coding_type =
    pInpHandle->stAudioMngr.stAudioConfig.CodingType;
  pAudPropData->channel_allocation =
    pInpHandle->stAudioMngr.stAudioConfig.uChannelAllocation;
  pAudPropData->channel_count =
    pInpHandle->stAudioMngr.stAudioConfig.uChannelCount;
  pAudPropData->sample_size =
    pInpHandle->stAudioMngr.stAudioConfig.SampleSize;
  pAudPropData->down_mix_inhibit =
    pInpHandle->stInfoPacketData.stAudioInfo.DM_INH;
  pAudPropData->level_shift = pInpHandle->stInfoPacketData.stAudioInfo.LSV;

  if (pInpHandle->stInfoPacketData.stAudioInfo.LFEP > 2)
    {
      pAudPropData->lfe_pb_level = STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_UNKNOWN;
    }
  else
    {
      pAudPropData->lfe_pb_level =
        (stm_hdmirx_audio_lfe_pb_level_t) pInpHandle->stInfoPacketData.stAudioInfo.LFEP;
    }

  if (pInpHandle->stAudioMngr.stAudioConfig.SampleFrequency <
      (sizeof(aSampleFre) / sizeof(U32)))
    {
      pAudPropData->sampling_frequency =
        aSampleFre[pInpHandle->stAudioMngr.stAudioConfig.SampleFrequency];
    }

  DBG_HDRX_EVENT_INFO
  ("\n***********Audio Proprty Event Payload***************\n");
  DBG_HDRX_EVENT_INFO("Stream Type         = %d\n",
                      pAudPropData->stream_type);
  DBG_HDRX_EVENT_INFO("Coding Type         = %d\n",
                      pAudPropData->coding_type);
  DBG_HDRX_EVENT_INFO("Channel Allocation  = %d\n",
                      pAudPropData->channel_allocation);
  DBG_HDRX_EVENT_INFO("Sample Size         = %d\n",
                      pAudPropData->sample_size);
  DBG_HDRX_EVENT_INFO("Sample Freq         = %d KHz\n",
                      (pAudPropData->sampling_frequency / 1000));
  DBG_HDRX_EVENT_INFO("DownMix Inhibit     = %d\n",
                      pAudPropData->down_mix_inhibit);
  DBG_HDRX_EVENT_INFO("Level Shift         = %d\n",
                      pAudPropData->level_shift);
  DBG_HDRX_EVENT_INFO("\n");

}

/******************************************************************************
 FUNCTION     	:   sthdmirx_video_prop_changed_data_fill
 USAGE        	:   Fills the Video property data from local storage to the pointer buffer
 INPUT        	:   pointer to Video propert structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_video_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_video_property_t *pVidPropData)
{
  U8 uColorDepth;

  /* filling the color depth as per spec */
  uColorDepth = pInpHandle->stInfoPacketData.eColorDepth;

  if ((uColorDepth > HDRX_GCP_CD_24BPP) &&
      (uColorDepth <= HDRX_GCP_CD_48BPP))
    {
      if (uColorDepth == HDRX_GCP_CD_30BPP)
        {
          pVidPropData->color_depth = STM_HDMIRX_COLOR_DEPTH_30BPP;
        }
      else if (uColorDepth == HDRX_GCP_CD_36BPP)
        {
          pVidPropData->color_depth = STM_HDMIRX_COLOR_DEPTH_36BPP;
        }
      else if (uColorDepth == HDRX_GCP_CD_48BPP)
        {
          pVidPropData->color_depth = STM_HDMIRX_COLOR_DEPTH_48BPP;
        }
    }
  else
    {
      pVidPropData->color_depth = STM_HDMIRX_COLOR_DEPTH_24BPP;
    }

  pVidPropData->color_format =
    (stm_hdmirx_color_format_t) pInpHandle->stInfoPacketData.stAviInfo.Y;

  switch (pInpHandle->stInfoPacketData.stAviInfo.C)
    {
    case 0:
      pVidPropData->colorimetry = STM_HDMIRX_COLORIMETRY_STD_DEFAULT;
      break;

    case 1:
      pVidPropData->colorimetry = STM_HDMIRX_COLORIMETRY_STD_BT_601;
      break;

    case 2:
      pVidPropData->colorimetry = STM_HDMIRX_COLORIMETRY_STD_BT_709;
      break;
    case 3:
      if (pInpHandle->stInfoPacketData.stAviInfo.EC == 0)
        {
          pVidPropData->colorimetry = STM_HDMIRX_COLORIMETRY_STD_XVYCC_601;
        }
      else if (pInpHandle->stInfoPacketData.stAviInfo.EC == 1)
        {
          pVidPropData->colorimetry = STM_HDMIRX_COLORIMETRY_STD_XVYCC_709;
        }
      else
        {
          HDMI_PRINT("Extended Colorimetry but not defined \n");
        }

      break;
    }

  switch (pInpHandle->stInfoPacketData.stAviInfo.B)
    {
    case 0:
      pVidPropData->bar_info.hbar_valid = FALSE;
      pVidPropData->bar_info.vbar_valid = FALSE;
      break;

    case 1:
      pVidPropData->bar_info.hbar_valid = FALSE;
      pVidPropData->bar_info.vbar_valid = TRUE;
      break;

    case 2:
      pVidPropData->bar_info.hbar_valid = TRUE;
      pVidPropData->bar_info.vbar_valid = FALSE;
      break;
    case 3:
      pVidPropData->bar_info.hbar_valid = TRUE;
      pVidPropData->bar_info.vbar_valid = TRUE;
      break;
    }

  pVidPropData->bar_info.end_of_left_bar =
    pInpHandle->stInfoPacketData.stAviInfo.EndOfLeftBar;
  pVidPropData->bar_info.start_of_right_bar =
    pInpHandle->stInfoPacketData.stAviInfo.StartOfRightBar;
  pVidPropData->bar_info.end_of_top_bar =
    pInpHandle->stInfoPacketData.stAviInfo.EndOfTopBar;
  pVidPropData->bar_info.start_of_bottom_bar =
    pInpHandle->stInfoPacketData.stAviInfo.StartOfBottomBar;

  pVidPropData->scan_info =
    (stm_hdmirx_scan_info_t) pInpHandle->stInfoPacketData.stAviInfo.S;
  pVidPropData->scaling_info =
    (stm_hdmirx_scaling_info_t) pInpHandle->stInfoPacketData.stAviInfo.SC;

  switch (pInpHandle->stInfoPacketData.stAviInfo.M)
    {
    case 1:
      pVidPropData->picture_aspect = STM_HDMIRX_ASPECT_RATIO_4_3;
      break;

    case 2:
      pVidPropData->picture_aspect = STM_HDMIRX_ASPECT_RATIO_16_9;
      break;

    case 3:
      HDMI_PRINT
      ("Picture Aspect Ration Info Field : Reserved for future \n");
      /* no break */
    case 0:
      pVidPropData->picture_aspect = STM_HDMIRX_ASPECT_RATIO_NONE;
      break;
    }

  if (pInpHandle->stInfoPacketData.stAviInfo.R == 8)
    {
      pVidPropData->active_format_aspect = pVidPropData->picture_aspect;
    }
  else
    {
      pVidPropData->active_format_aspect =
        a_AspectRatio[pInpHandle->stInfoPacketData.stAviInfo.R];
    }

  pVidPropData->rgb_quant_range =
    (stm_hdmirx_rgb_quant_range_t) pInpHandle->stInfoPacketData.stAviInfo.Q;
  pVidPropData->pixel_repeat_factor =
    (stm_hdmirx_pixel_repeat_factor_t) pInpHandle->stInfoPacketData.stAviInfo.PR;

  pVidPropData->it_content = FALSE;
  if (pInpHandle->stInfoPacketData.stAviInfo.ITC)
    {
      pVidPropData->it_content = TRUE;
    }

  pVidPropData->video_timing_code =
    (U32) pInpHandle->stInfoPacketData.stAviInfo.VIC;
  pVidPropData->yc_quant_range =
    (stm_hdmirx_yc_quant_range_t) pInpHandle->stInfoPacketData.stAviInfo.YQ;

  if ((pInpHandle->stInfoPacketData.stAviInfo.ITC == 0)
      && (pInpHandle->stInfoPacketData.stAviInfo.CN == 0))
    {
      pVidPropData->content_type = STM_HDMIRX_CONTENT_TYPE_UNKNOWN;
    }
  else
    {
      pVidPropData->content_type =
        (stm_hdmirx_content_type_t) (pInpHandle->stInfoPacketData.stAviInfo.CN + 1);
    }

  pVidPropData->hdmi_video_format = FALSE;
  pVidPropData->video_3d = FALSE;

  if (pInpHandle->stDataAvblFlags.bIsVsInfoAvbl)
    {
      U8 NoOfBytesInPayload;
      NoOfBytesInPayload = pInpHandle->stInfoPacketData.stVSInfo.Length;
      if (NoOfBytesInPayload > 0 && NoOfBytesInPayload < 24)
        {
          if (pInpHandle->stInfoPacketData.stVSInfo.Hdmi_Video_Format == 1)
            {
              HDMI_PRINT("HDMI_VIDEO_FORMAT_VS_INFO\n");
              pVidPropData->hdmi_video_format = TRUE;
              pVidPropData->video_timing_code =
                (U32) pInpHandle->stInfoPacketData.stVSInfo.Payload[1];
            }
          else if (pInpHandle->stInfoPacketData.stVSInfo.Hdmi_Video_Format == 2)
            {
              pVidPropData->video_3d = TRUE;
              pVidPropData->video_property_3d.format =
                (stm_hdmirx_3d_format_t)
                a_3DFormat[pInpHandle->stInfoPacketData.stVSInfo.structure3D & 0x0F];
              if (pVidPropData->video_property_3d.format == STM_HDMIRX_3D_FORMAT_SBYS_HALF)
                {
                  pVidPropData->video_property_3d.extdata.
                  sbs_half_ext_data.sampling_mode =
                    (pInpHandle->stInfoPacketData.stVSInfo.Payload[2] & 0xF0) >> 4;
                  if (pInpHandle->stInfoPacketData.stVSInfo.TDMetaPresent)
                    {
                      pVidPropData->video_property_3d.
                      metatdata.type =
                        (pInpHandle->stInfoPacketData.stVSInfo.Payload[3] & 0xE0) >> 5;
                      pVidPropData->video_property_3d.metatdata.length =
                        pInpHandle->stInfoPacketData.stVSInfo.Payload[3] & 0x14;
                      stm_hdmirx_memcpy(
                        &pVidPropData->video_property_3d.metatdata.metadata[0],
                        &pInpHandle->stInfoPacketData.stVSInfo.Payload[4],
                        (sizeof(U8) *pVidPropData->video_property_3d.metatdata.length));
                    }
                }
              HDMI_PRINT(" 3D VIDEO PRESENT\n");
            }
          else
            {

            }
        }
    }
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_signal_timing_data_fill
 USAGE        	:   Fills the Signal Timing Parameters.
 INPUT        	:   pointer to Signal Timing Parametes.
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_signal_timing_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                      stm_hdmirx_signal_timing_t *pSigTimingData)
{
  sthdmirx_sig_timing_info_t SigTimingData;
  sthdmirx_IFM_timing_params_t IFMTimingData;
  U32 hstart_plus=0;
  U16 vstart_plus=0;

  sthdmirx_IFM_IBD_timing_get((sthdmirx_IFM_context_t *) & pInpHandle->IfmControl,
                              &SigTimingData);
  pSigTimingData->hactive = SigTimingData.Window.Width & 0xfffe;
  pSigTimingData->vActive = SigTimingData.Window.Length & 0xfffe;
  pSigTimingData->vtotal = SigTimingData.VTotal;
  pSigTimingData->htotal = SigTimingData.HTotal;
  pSigTimingData->hsync_polarity = sthdmirx_IFM_HSync_polarity_get(
                                     (sthdmirx_IFM_context_t *) &pInpHandle->IfmControl);
  pSigTimingData->vsync_polarity = sthdmirx_IFM_VSync_polarity_get(
                                     (sthdmirx_IFM_context_t *) &pInpHandle->IfmControl);
  pSigTimingData->pixel_clock_hz = sthdmirx_IFM_pixel_clk_freq_get(
                                     (sthdmirx_IFM_context_t *) &pInpHandle->IfmControl);
  pSigTimingData->pixel_clock_hz = sthdmirx_IFM_pixel_clk_freq_get(
                                     (sthdmirx_IFM_context_t *) &pInpHandle->IfmControl);

  pSigTimingData->scan_type = sthdmirx_IFM_signal_scantype_get(
                                (sthdmirx_IFM_context_t *) &pInpHandle->IfmControl);

  sthdmirx_IFM_signal_timing_get((sthdmirx_IFM_context_t *) & pInpHandle->IfmControl, &IFMTimingData);

  if (IFMTimingData.H_SyncPolarity==0)
  {
     hstart_plus= (IFMTimingData.HPulse *((pSigTimingData->pixel_clock_hz/3000)))/1000;
     if ( (hstart_plus/10)*10+3<hstart_plus)
       hstart_plus=hstart_plus/10 +1;
     else
       hstart_plus=hstart_plus/10;
  }

  if (IFMTimingData.V_SyncPolarity==0)
     vstart_plus = IFMTimingData.VPulse;

  pSigTimingData->hactive_start = SigTimingData.Window.Hstart+hstart_plus;
  pSigTimingData->vactive_start = SigTimingData.Window.Vstart+vstart_plus;

  if(pSigTimingData->scan_type==0)
  {
    pSigTimingData->vtotal = (pSigTimingData->vtotal-IFMTimingData.VPeriod%2)*2+1;
    pSigTimingData->vActive = (pSigTimingData->vActive*2);
    pSigTimingData->vactive_start = (pSigTimingData->vactive_start-IFMTimingData.VPeriod%2)*2+1;
  }

#ifdef ENABLE_3D_TIMIMG_MEAS_CNTRL_OVERFLOW_FIX
  if (pInpHandle->sMeasCtrl.CurrentTimingInfo.VActive > 0x7ff ||
      pInpHandle->sMeasCtrl.CurrentTimingInfo.VTotal > 0x7ff)
    {
      pSigTimingData->vActive =
        pInpHandle->sMeasCtrl.CurrentTimingInfo.VActive & 0xfffe;
      pSigTimingData->vtotal =
        pInpHandle->sMeasCtrl.CurrentTimingInfo.VTotal;
      pSigTimingData->pixel_clock_hz =
        (U32) ((U32)(pSigTimingData->pixel_clock_hz /SigTimingData.VTotal)) * (pSigTimingData->vtotal);
    }
#endif

}

/******************************************************************************
FUNCTION     	:   sthdmirx_VS_info_data_fill
 USAGE        	:   Fills the Vendor specific infoframe data from local storage to the pointer buffer
 INPUT        	:   pointer to VS Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_VS_info_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                stm_hdmirx_vs_info_t *VsInfo)
{
  VsInfo->length = pInpHandle->stInfoPacketData.stVSInfo.Length;
  VsInfo->ieee_reg_id = pInpHandle->stInfoPacketData.stVSInfo.IeeeId;
  stm_hdmirx_memcpy(&VsInfo->payload,
                    &pInpHandle->stInfoPacketData.stVSInfo.Payload,
                    ((sizeof(U8)) * HDRX_VENDORSPECIFIC_PAYLOAD_LENGTH));
}

/******************************************************************************
FUNCTION     	:   sthdmirx_SPD_info_frame_data_fill
 USAGE        	:   Fills the Source product specific descriptor data from local storage to the pointer buffer
 INPUT        	:   pointer to SPD Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_SPD_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_spd_info_t *SpdInfo)
{

  stm_hdmirx_memcpy(&SpdInfo->description,
                    &pInpHandle->stInfoPacketData.stSpdInfo.ProductName,
                    ((sizeof(U8)) * HDRX_PRODUCT_DESCRIPTION_LENGTH));
  stm_hdmirx_memcpy(&SpdInfo->vendor_name,
                    &pInpHandle->stInfoPacketData.stSpdInfo.VendorName,
                    (((sizeof(U8)) * HDRX_VENDOR_NAME_LENGTH)));
  SpdInfo->device_info = pInpHandle->stInfoPacketData.stSpdInfo.SDI;

}

/******************************************************************************
FUNCTION     	:   sthdmirx_MPEG_info_frame_data_fill
 USAGE        	:   Fills the Mpeg info Frame data from local storage to the pointer buffer
 INPUT        	:   pointer to MPEG Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_MPEG_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                        stm_hdmirx_mpeg_source_info_t *MpegInfo)
{
  MpegInfo->bitrate = pInpHandle->stInfoPacketData.stMpegInfo.MpegBitRate;
  MpegInfo->field_repeat = pInpHandle->stInfoPacketData.stMpegInfo.FR0;
  MpegInfo->picture_type =
    (stm_hdmirx_mpeg_picture_type_t) pInpHandle->stInfoPacketData.stMpegInfo.MF;
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_ACP_info_frame_data_fill
 USAGE        	:   Fills the Audio content protection Info Frame data from local storage to the pointer buffer
 INPUT        	:   pointer to ACP Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_ACP_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_acp_info_t *AcpInfo)
{
  AcpInfo->acp_type = pInpHandle->stInfoPacketData.stAcpInfo.ACPType;
  stm_hdmirx_memcpy(&AcpInfo->acp_data,
                    &pInpHandle->stInfoPacketData.stAcpInfo.ACPData,
                    ((sizeof(U8)) * HDRX_ACP_INFOFRAME_DATA_LENGTH));
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_GBD_info_frame_data_fill
 USAGE        	:   Fills the Gamut Boundary data Info Frame from local storage to the pointer buffer
 INPUT        	:   pointer to GBD Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_GBD_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_gbd_info_t *GbdInfo)
{
  stm_hdmirx_memcpy(&GbdInfo->GBD,
                    &pInpHandle->stInfoPacketData.stMetaDataInfo.GBD,
                    ((sizeof(U8)) * HDRX_GBD_INFOFRAME_DATA_LENGTH));
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_ISRC_info_frame_data_fill
 USAGE        	:   Fills the ISRC Info Frame from local storage to the pointer buffer
 INPUT        	:   pointer to ISRC Info structure
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_ISRC_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                        stm_hdmirx_isrc_info_t *ISRCInfo)
{
  //TBD
  UNUSED_PARAMETER(pInpHandle);
  UNUSED_PARAMETER(ISRCInfo);
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_print_AVI_frame_packet
 USAGE        	:   Prints the AVI info Frame Data
 INPUT        	:   None
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_print_AVI_frame_packet(hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_AVI_INFOFRAME *AVIHdmiData;
  AVIHdmiData = &pInpHandle->stInfoPacketData.stAviInfo;
  DBG_HDRX_EVENT_INFO("\n***********New Data Packet***************\n");
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.Y = %d\n", AVIHdmiData->Y);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.A = %d\n", AVIHdmiData->A);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.B = %d\n", AVIHdmiData->B);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.C = %d\n", AVIHdmiData->C);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.EC = %d\n", AVIHdmiData->EC);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.M = %d\n", AVIHdmiData->M);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.Q = %d\n", AVIHdmiData->Q);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.R = %d\n", AVIHdmiData->R);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.S = %d\n", AVIHdmiData->S);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.SC = %d\n", AVIHdmiData->SC);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.Y = %d\n", AVIHdmiData->Y);
  DBG_HDRX_EVENT_INFO("Bp_AVIHdmiData.ITC = %d\n", AVIHdmiData->ITC);
  DBG_HDRX_EVENT_INFO("VIC = 0x%x\n", AVIHdmiData->VIC);
  DBG_HDRX_EVENT_INFO("YQ =0x%x\n", AVIHdmiData->YQ);
  DBG_HDRX_EVENT_INFO("CN =0x%x\n", AVIHdmiData->CN);
  DBG_HDRX_EVENT_INFO("PR =0x%x\n", AVIHdmiData->PR);
  DBG_HDRX_EVENT_INFO("Top bar = 0x%x\n", AVIHdmiData->EndOfTopBar);
  DBG_HDRX_EVENT_INFO("Bottom bar = 0x%x\n",AVIHdmiData->StartOfBottomBar);
  DBG_HDRX_EVENT_INFO("Left bar = 0x%x\n", AVIHdmiData->EndOfLeftBar);
  DBG_HDRX_EVENT_INFO("Right bar = 0x%x\n", AVIHdmiData->StartOfRightBar);
  DBG_HDRX_EVENT_INFO("***********End of AVI Info Frame*************\n\n");
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_print_video_prop_packed_data
 USAGE        	:   Prints the video property Data
 INPUT        	:   None
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_print_video_prop_packed_data(
  stm_hdmirx_video_property_t *vidPropData)
{
  DBG_HDRX_EVENT_INFO
  ("\n***********Video Property Data***************\n");
  DBG_HDRX_EVENT_INFO("Color Depth          : %d\n",
                      vidPropData->color_depth);
  DBG_HDRX_EVENT_INFO("ColorFormat          : %d\n",
                      vidPropData->color_format);
  DBG_HDRX_EVENT_INFO("ColorColorimetry     : %d\n",
                      vidPropData->colorimetry);
  DBG_HDRX_EVENT_INFO("H BarInfo[Valid:%d]  : ",
                      vidPropData->bar_info.hbar_valid);
  DBG_HDRX_EVENT_INFO("{%d}\n", vidPropData->bar_info.end_of_left_bar);
  DBG_HDRX_EVENT_INFO("{%d}\n", vidPropData->bar_info.start_of_right_bar);
  DBG_HDRX_EVENT_INFO("V BarInfo[Valid:%d]  : ",
                      vidPropData->bar_info.vbar_valid);
  DBG_HDRX_EVENT_INFO("{%d}\n", vidPropData->bar_info.end_of_top_bar);
  DBG_HDRX_EVENT_INFO("{%d}\n",
                      vidPropData->bar_info.start_of_bottom_bar);
  DBG_HDRX_EVENT_INFO("ScanInfo             : %d\n",
                      vidPropData->scan_info);
  DBG_HDRX_EVENT_INFO("ScalingInfo          : %d\n",
                      vidPropData->scaling_info);
  DBG_HDRX_EVENT_INFO("PictureAR            : %d\n",
                      vidPropData->picture_aspect);
  DBG_HDRX_EVENT_INFO("ActiveFormatAR       : %d\n",
                      vidPropData->active_format_aspect);
  DBG_HDRX_EVENT_INFO("RGBQuantRange        : %d\n",
                      vidPropData->rgb_quant_range);
  DBG_HDRX_EVENT_INFO("PRFactor             : %d\n",
                      vidPropData->pixel_repeat_factor);
  DBG_HDRX_EVENT_INFO("ITContent            : %d\n",
                      vidPropData->it_content);
  DBG_HDRX_EVENT_INFO("VideoTimingCode      : %d\n",
                      vidPropData->video_timing_code);
  DBG_HDRX_EVENT_INFO("*******************END*********************\n\n");

}

/******************************************************************************
 FUNCTION     	:   sthdmirx_print_audio_info_frame_packet
 USAGE        	:   Prints the Audio info Frame Data
 INPUT        	:   None
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_print_audio_info_frame_packet(
  hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_AUDIO_INFOFRAME *Sp_Buffer;
  Sp_Buffer = &pInpHandle->stInfoPacketData.stAudioInfo;

  DBG_HDRX_EVENT_INFO("***********New Audio Packet Data***************\n");
  DBG_HDRX_EVENT_INFO("CheckSum          0x%x\n", Sp_Buffer->CheckSum);
  DBG_HDRX_EVENT_INFO("CC                0x%x\n", Sp_Buffer->CC);
  DBG_HDRX_EVENT_INFO("PB1_Rsvd          0x%x\n", Sp_Buffer->PB1_Rsvd);
  DBG_HDRX_EVENT_INFO("CT                0x%x\n", Sp_Buffer->CT);
  DBG_HDRX_EVENT_INFO("SS                0x%x\n", Sp_Buffer->SS);
  DBG_HDRX_EVENT_INFO("SF                0x%x\n", Sp_Buffer->SF);
  DBG_HDRX_EVENT_INFO("PB2_Rsvd          0x%x\n", Sp_Buffer->PB2_Rsvd);
  DBG_HDRX_EVENT_INFO("FormatCodingType  0x%x\n",Sp_Buffer->FormatCodingType);
  DBG_HDRX_EVENT_INFO("CA                0x%x\n", Sp_Buffer->CA);
  DBG_HDRX_EVENT_INFO("PB3_Rsvd          0x%x\n", Sp_Buffer->PB3_Rsvd);
  DBG_HDRX_EVENT_INFO("LSV               0x%x\n", Sp_Buffer->LSV);
  DBG_HDRX_EVENT_INFO("DM_INH            0x%x\n", Sp_Buffer->DM_INH);
}

/******************************************************************************
 FUNCTION     	:   sthdmirx_print_audio_clk_regeneration
 USAGE        	:   Prints the Audio clock regeneration Data
 INPUT        	:   None
 RETURN       	:   None
 USED_REGS      :   None
******************************************************************************/
void sthdmirx_print_audio_clk_regeneration(
  hdmirx_route_handle_t *const pInpHandle)
{
  HDRX_ACR_PACKET *Sp_Buffer;
  Sp_Buffer = &pInpHandle->stInfoPacketData.stAcrInfo;

  DBG_HDRX_EVENT_INFO("\n***********New ACR Packet Data***************\n");
  DBG_HDRX_EVENT_INFO("N          :0x%x\n", Sp_Buffer->N);
  DBG_HDRX_EVENT_INFO("CTS        :0x%x\n", Sp_Buffer->CTS);

}

/******************************************************************************
 FUNCTION     	:   sthdmirx_evaluate_packet_events
 USAGE        	:   Handles the events notification which are related to input processing route
 INPUT        	:
 RETURN       	:
 USED_REGS      :
******************************************************************************/
stm_error_code_t sthdmirx_evaluate_packet_events(
  hdmirx_route_handle_t *const pInpHandle)
{
  BOOL bIsVideoPropChangeNotifyRequired = FALSE;

  if (pInpHandle->bIsNoSignalNotify == TRUE)
    {

      pInpHandle->bIsNoSignalNotify = FALSE;
      pInpHandle->signal_status =
        STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT;
      HDMI_PRINT("**** STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT (NoSignal)\n");
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT);

    }

  if (pInpHandle->bIsSignalPresentNotify == TRUE)
    {
      pInpHandle->bIsSignalPresentNotify = FALSE;
      pInpHandle->signal_status = STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT;
      HDMI_PRINT("**** STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT (SignalPresent)\n");
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT);
      if (pInpHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
        bIsVideoPropChangeNotifyRequired = TRUE;
    }

  /*Evaluate the info frame related event notification */
  /* Need to check the events evaluation rate, is it ok to check all packet events, send the notification *///TBD
  if ((pInpHandle->stNewDataFlags.bIsNewAviInfo == TRUE) ||
      (pInpHandle->stNewDataFlags.bIsNewVsInfo == TRUE))
    {

      if (((pInpHandle->stNewDataFlags.bIsNewAviInfo == TRUE) &&
           (pInpHandle->stNewDataFlags.bIsNewVsInfo == TRUE)) ||
          (pInpHandle->stNewDataFlags.bIsNewAviInfo == TRUE))
        {
          bIsVideoPropChangeNotifyRequired = TRUE;

        }
      else
        {
          if ((pInpHandle->stInfoPacketData.stVSInfo.Hdmi_Video_Format == 1)||
              (pInpHandle->stInfoPacketData.stVSInfo.Hdmi_Video_Format == 2))
            {
              bIsVideoPropChangeNotifyRequired = TRUE;
            }
        }
    }

  if (bIsVideoPropChangeNotifyRequired == TRUE)
    {
      if(pInpHandle->signal_status==STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
        {
          HDMI_PRINT("**** STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT\n");
          hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                                   STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT);
          pInpHandle->stNewDataFlags.bIsNewAviInfo = FALSE;
        }
    }

  if (pInpHandle->bIsAudioPropertyChanged == TRUE)
    {
      HDMI_PRINT("**** STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT\n");
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT);
      pInpHandle->bIsAudioPropertyChanged = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewAudioInfo == TRUE)
    {
      /* just for debug purpose */
    }

  if (pInpHandle->stNewDataFlags.bIsNewAcs == TRUE)
    {
      /* just for debug purpose */

    }

  if (pInpHandle->stNewDataFlags.bIsNewVsInfo == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT);
      HDMI_PRINT("New Vendor Specific Info Frame \n");
      pInpHandle->stNewDataFlags.bIsNewVsInfo = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewSpdInfo == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_SPD_INFO_UPDATE_EVT);
      HDMI_PRINT("New Source product descriptor info Frame \n");
      pInpHandle->stNewDataFlags.bIsNewSpdInfo = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewMpegInfo == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_MPEGSOURCE_INFO_UPDATE_EVT);
      HDMI_PRINT("New Mpeg Info Frame \n");
      pInpHandle->stNewDataFlags.bIsNewMpegInfo = FALSE;
    }

  if ((pInpHandle->stNewDataFlags.bIsNewIsrc1Info == TRUE) ||
      (pInpHandle->stNewDataFlags.bIsNewIsrc2Info == TRUE))
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_ISRC_INFO_UPDATE_EVT);
      HDMI_PRINT("New ISRC1 or ISRC2 Info Frame \n");
      pInpHandle->stNewDataFlags.bIsNewIsrc1Info = FALSE;
      pInpHandle->stNewDataFlags.bIsNewIsrc2Info = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewAcpInfo == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_ACP_INFO_UPDATE_EVT);
      HDMI_PRINT("New Audio Content Protections \n");
      pInpHandle->stNewDataFlags.bIsNewAcpInfo = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewAcrInfo == TRUE)
    {
      sthdmirx_print_audio_clk_regeneration(pInpHandle);
      pInpHandle->stNewDataFlags.bIsNewAcrInfo = FALSE;
    }

  if (pInpHandle->stNewDataFlags.bIsNewGcpInfo == TRUE)
    {
      HDMI_PRINT("New General Control Packet data \n");	//TBD
      //     hdmirx_notify_route_evts((hdmirx_handle_t *)pInpHandle->Handle,STM_HDMIRX_ROUTE_GBD_INFO_UPDATE_EVT); //TBD
      pInpHandle->stNewDataFlags.bIsNewGcpInfo = FALSE;

    }

  if (pInpHandle->stNewDataFlags.bIsNewMetaDataInfo == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                               STM_HDMIRX_ROUTE_GBD_INFO_UPDATE_EVT);
      HDMI_PRINT("New Gamut Meta Data packet\n");
      pInpHandle->stNewDataFlags.bIsNewMetaDataInfo = FALSE;
    }
  if (pInpHandle->bIsHDCPAuthenticationDetected == TRUE)
    {
      hdmirx_notify_route_evts((hdmirx_handle_t *) pInpHandle->Handle,
                                        STM_HDMIRX_ROUTE_HDCP_AUTHENTICATION_DETECTED_EVT);
      HDMI_PRINT("Authentication Detected\n");
      pInpHandle->bIsHDCPAuthenticationDetected = FALSE;
    }
  return 0;
}
