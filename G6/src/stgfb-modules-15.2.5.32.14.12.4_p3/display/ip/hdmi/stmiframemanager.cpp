/***********************************************************************
 *
 * File: display/ip/hdmi/stmiframemanager.cpp
 * Copyright (c) 2008-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>

#include "stmhdmi.h"
#include "stmhdmiregs.h"
#include "stmiframemanager.h"

/*
 * The Gamut metadata slot is first because there are timing constraints
 * on its transmission. However the processing of this slot is only done in
 * the hardware specific implementation that can support it.
 */
static const int max_transmission_slots     = 6;
static const int Gamut_transmission_slot    = 0; /* mutually exclusive with ISRC1 */
static const int ISRC1_transmission_slot    = 0;
static const int Audio_transmission_slot    = 1;
static const int AVI_transmission_slot      = 2;
static const int Muxed_transmission_slot    = 3; /* ACP,SPD,Vendor */
static const int ISRC2_transmission_slot    = 4;
static const int NTSC_transmission_slot     = 4; /* mutually exclusive with ISRC2 */
static const int HDMIVSI_transmission_slot  = 5;


CSTmIFrameManager::CSTmIFrameManager(CDisplayDevice *pDev,
                                     uint32_t        uHDMIOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevRegs    = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_uHDMIOffset = uHDMIOffset;
  m_pParent     = 0;
  m_pMaster     = 0;
  m_lock        = 0;

  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;

  vibe_os_zero_memory(&m_pictureInfo,sizeof(m_pictureInfo));

  m_VICSelectMode              = STM_AVI_VIC_FOLLOW_PICTURE_ASPECT_RATIO;
  m_VICCode                    = 0;

  m_colorspaceMode             = STM_YCBCR_COLORSPACE_AUTO_SELECT;

  m_pictureInfo.picture_aspect_ratio  = STM_WSS_ASPECT_RATIO_UNKNOWN;
  m_pictureInfo.video_aspect_ratio    = STM_WSS_ASPECT_RATIO_UNKNOWN;
  m_pictureInfo.letterbox_style = STM_LETTERBOX_NONE;
  m_pictureInfo.picture_rescale = STM_RESCALE_NONE;

  m_pPictureInfoQueue = 0;
  m_pAudioQueue       = 0;
  m_pISRCQueue        = 0;
  m_pACPQueue         = 0;
  m_pSPDQueue         = 0;
  m_pVendorQueue      = 0;
  m_pGamutQueue       = 0;
  m_pNTSCQueue        = 0;
  m_p3DEXTQueue       = 0;

  m_bAudioFrameValid         = false;
  m_bAVIFrameNeedsUpdate     = false;
  m_bHDMIVSIFrameNeedsUpdate = false;
  m_bTransmitHDMIVSIFrame    = false;
  m_bAVIExtendedColorimetryValid = false;

  m_nAudioFrequencyIndex = 0;

  /*
   * Control the setting of quantization info in the AVI frame based on the
   * hardware signal range setting. By default this is unsupported, we need to
   * be told if the connected TV reports it supports these bits.
   */
  m_VCDBQuantization    = STM_VCDB_QUANTIZATION_UNSUPPORTED;
  m_AVIQuantizationMode = STM_AVI_QUANTIZATION_AUTO;

  m_ulOverscanMode = STM_AVI_NO_SCAN_DATA;
  m_ulContentType  = STM_AVI_IT_UNSPECIFIED;

  vibe_os_zero_memory(&m_AVIFrame,sizeof(stm_hdmi_info_frame_t));
  m_AVIFrame.type    = HDMI_AVI_INFOFRAME_TYPE;
  m_AVIFrame.version = HDMI_AVI_INFOFRAME_VERSION;
  m_AVIFrame.length  = HDMI_AVI_INFOFRAME_LENGTH;
  m_AVIFrame.data[1] = HDMI_AVI_INFOFRAME_OVERSCAN;

  /*
   * Default 3D extended data, all zero, i.e. horizontal subsampling and
   * no additional parallax information.
   */
  vibe_os_zero_memory(&m_3DEXTData,sizeof(stm_hdmi_info_frame_t));

  vibe_os_zero_memory(&m_ACPFrame,sizeof(stm_hdmi_info_frame_t));
  m_ACPFrame.type    = HDMI_ACP_PACKET_TYPE;
  m_ACPFrame.version = HDMI_ACP_TYPE_GENERIC;
  m_nACPTransmissionCount = 0;
  m_nACPTransmissionFrameDelay = 255;

  /*
   * Default SPD data
   */
  vibe_os_zero_memory(&m_SPDFrame,sizeof(stm_hdmi_info_frame_t));
  m_SPDFrame.type    = HDMI_SPD_INFOFRAME_TYPE;
  m_SPDFrame.version = HDMI_SPD_INFOFRAME_VERSION;
  m_SPDFrame.length  = HDMI_SPD_INFOFRAME_LENGTH;

  vibe_os_snprintf((char*)&m_SPDFrame.data[HDMI_SPD_INFOFRAME_VENDOR_START], HDMI_SPD_INFOFRAME_VENDOR_LENGTH, "STM");
  vibe_os_snprintf((char*)&m_SPDFrame.data[HDMI_SPD_INFOFRAME_PRODUCT_START],HDMI_SPD_INFOFRAME_PRODUCT_LENGTH, "STLinux");
  m_SPDFrame.data[HDMI_SPD_INFOFRAME_SPI_OFFSET] = HDMI_SPD_INFOFRAME_SPI_PC;
  m_nMuxedTransmissionCount =0;


  m_bISRCTransmissionInProgress = false;
  vibe_os_zero_memory(&m_HDMIVSIFrame,sizeof(stm_hdmi_info_frame_t));
  vibe_os_zero_memory(&m_CurrentMode,sizeof(stm_display_mode_t));
  vibe_os_zero_memory(&m_VSIFrame,sizeof(stm_hdmi_info_frame_t));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmIFrameManager::~CSTmIFrameManager()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_lock)
    vibe_os_delete_resource_lock(m_lock);

  delete m_pPictureInfoQueue;
  delete m_pAudioQueue;
  delete m_pISRCQueue;
  delete m_pACPQueue;
  delete m_pSPDQueue;
  delete m_pVendorQueue;
  delete m_pGamutQueue;
  delete m_pNTSCQueue;
  delete m_p3DEXTQueue;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmIFrameManager::Create(CSTmHDMI *parent, COutput *master)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!parent || !master)
  {
    TRC( TRC_ID_ERROR, "NULL parent or master pointer" );
    return false;
  }

  m_lock = vibe_os_create_resource_lock();
  if(m_lock == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create object lock" );
    return false;
  }


  m_pParent = parent;
  m_pMaster = master;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


void CSTmIFrameManager::SetHDMIVSIData(const stm_display_mode_t *pMode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_zero_memory(&m_HDMIVSIFrame,sizeof(stm_hdmi_info_frame_t));
  m_HDMIVSIFrame.type    = HDMI_VENDOR_INFOFRAME_TYPE;
  m_HDMIVSIFrame.version = HDMI_VENDOR_INFOFRAME_VERSION;
  m_HDMIVSIFrame.data[1] = 0x03;
  m_HDMIVSIFrame.data[2] = 0x0c;
  m_HDMIVSIFrame.data[3] = 0x0;
  /*
   * The default 2D configuration, also used in case of any configuration error
   */
  m_HDMIVSIFrame.length  = 5;
  m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_NONE; // No information in packet

  if(pMode)
  {
    switch(pMode->mode_params.flags & STM_MODE_FLAGS_3D_MASK)
    {
      case STM_MODE_FLAGS_3D_FRAME_PACKED:
      {
        m_HDMIVSIFrame.length++;
        m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_3D_STRUCTURE;
        m_HDMIVSIFrame.data[5] = HDMI_VSI_3D_FRAME_PACKED;
        m_bTransmitHDMIVSIFrame = true;
        break;
      }
      case STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE:
      {
        /*
         * NOTE: We are not checking this is an interlaced mode here, we are
         *       assuming the HDMI class has already done this.
         */
        m_HDMIVSIFrame.length++;
        m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_3D_STRUCTURE;
        m_HDMIVSIFrame.data[5] = HDMI_VSI_3D_FIELD_ALTERNATIVE;
        m_bTransmitHDMIVSIFrame = true;
        break;
      }
      case STM_MODE_FLAGS_3D_SBS_HALF:
      {
        /*
         * Of the current valid structure types we support only this one
         * should add in the extended data field (HDMI spec 1.4a section 8.2.3)
         */
        m_HDMIVSIFrame.length += 2;
        m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_3D_STRUCTURE;
        m_HDMIVSIFrame.data[5] = HDMI_VSI_3D_SbS_HALF;
        m_HDMIVSIFrame.data[6] = m_3DEXTData.data[6];
        m_bTransmitHDMIVSIFrame = true;
        break;
      }
      case STM_MODE_FLAGS_3D_TOP_BOTTOM:
      {
        m_HDMIVSIFrame.length++;
        m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_3D_STRUCTURE;
        m_HDMIVSIFrame.data[5] = HDMI_VSI_3D_TaB;
        m_bTransmitHDMIVSIFrame = true;
        break;
      }
      case STM_MODE_FLAGS_NONE:
        /*
         * NOTE: If we are dynamically switching from 3D to 2D signaling
         *       we will keep sending a VSI frame with no information in it.
         *       This is similar to what happens with deepcolor packing
         *       information transmission in the GCP when switching back to
         *       normal 24bit color. Although the HDMI 1.4a spec does not
         *       appear to mandate the continued transmission of VSI IFrames
         *       if we stop transmitting 3D content, this has been implemented
         *       by a customer and we assume it was done for compatibility with
         *       certain displays.
         *
         * TODO: Support for 4Kx2K extended VIC codes in the future.
         */
        break;
      default:
        /*
         * We shouldn't get any of the TV panel specific 3D modes here, they
         * should have been filtered out as unsupported by the HDMI class.
         */
        m_bTransmitHDMIVSIFrame = false;
        break;
    }

    if((pMode->mode_params.output_standards == STM_OUTPUT_STD_HDMI_LLC_EXT) &&
      ((pMode->mode_params.flags & STM_MODE_FLAGS_3D_MASK) == 0))
    {
      m_HDMIVSIFrame.data[4] = HDMI_VSI_VIDEO_FORMAT_EXTENDED_RESOLUTION;
      switch(pMode->mode_id)
      {
        case STM_TIMING_MODE_4K2K29970_296703:
        case STM_TIMING_MODE_4K2K30000_297000:
        {
          m_HDMIVSIFrame.length++;
          m_HDMIVSIFrame.data[5] = HDMI_VSI_VIC_4k2k30;
          m_bTransmitHDMIVSIFrame = true;
          break;
        }
        case STM_TIMING_MODE_4K2K25000_297000:
        {
          m_HDMIVSIFrame.length++;
          m_HDMIVSIFrame.data[5] = HDMI_VSI_VIC_4k2k25;
          m_bTransmitHDMIVSIFrame = true;
          break;
        }
        case STM_TIMING_MODE_4K2K23980_296703:
        case STM_TIMING_MODE_4K2K24000_297000:
        {
          m_HDMIVSIFrame.length++;
          m_HDMIVSIFrame.data[5] = HDMI_VSI_VIC_4k2k24;
          m_bTransmitHDMIVSIFrame = true;
          break;
        }
        case STM_TIMING_MODE_4K2K24000_297000_WIDE:
        {
          m_HDMIVSIFrame.length++;
          m_HDMIVSIFrame.data[5] = HDMI_VSI_VIC_4k2k24_WIDE;
          m_bTransmitHDMIVSIFrame = true;
          break;
        }
        default:
        {
          m_HDMIVSIFrame.data[5] = 0x0;
          m_bTransmitHDMIVSIFrame = false;
         break;
        }
      }
    }

    if((m_HDMIVSIFrame.length>5) && ((m_3DEXTData.data[5] & HDMI_VSI_3D_METADATA_PRESENT) != 0))
    {
      /*
       * The metadata (i.e. parallax information) if present can start on
       * either byte 6 or byte 7 depending on if the 3D_Ext_Data field is
       * present in byte 6 (conditional on the 3D structure type); so this
       * is a bit more complex than you would hope.
       *
       * TODO: Clarify as appendix H of HDMI 1.4a can be read in a way that
       *       contradicts section 8.2.3, such that you might think the
       *       metadata always starts at byte 7 and therefore byte 6 always
       *       contains a 3D_Ext_Data field if metadata is present, regardless
       *       of the 3D structure type.
       */
      int metadata_length = (m_3DEXTData.data[m_HDMIVSIFrame.length] & HDMI_VSI_METADATA_LENGTH_MASK);
      /*
       * First copy the metadata type and length field
       */
      m_HDMIVSIFrame.data[m_HDMIVSIFrame.length] = m_3DEXTData.data[m_HDMIVSIFrame.length];
      m_HDMIVSIFrame.length++;
      for(int i=1;i<=metadata_length;i++)
      {
        m_HDMIVSIFrame.data[m_HDMIVSIFrame.length] = m_3DEXTData.data[m_HDMIVSIFrame.length];
        m_HDMIVSIFrame.length++;
      }

      m_HDMIVSIFrame.data[5] |= HDMI_VSI_3D_METADATA_PRESENT;
    }
  }

  InfoFrameChecksum(&m_HDMIVSIFrame);

  m_bHDMIVSIFrameNeedsUpdate = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmIFrameManager::SetDefaultAudioFrame(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  stm_hdmi_info_frame_t audio_frame = {};

  audio_frame.type    = HDMI_AUDIO_INFOFRAME_TYPE;
  audio_frame.version = HDMI_AUDIO_INFOFRAME_VERSION;
  audio_frame.length  = HDMI_AUDIO_INFOFRAME_LENGTH;
  InfoFrameChecksum(&audio_frame);

  vibe_os_lock_resource(m_lock);
  {
    WriteInfoFrame(Audio_transmission_slot,&audio_frame);
    DisableTransmissionSlot(Audio_transmission_slot);
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmIFrameManager::SetDefaultPictureInformation(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_lock_resource(m_lock);
  {
    m_pictureInfo.picture_aspect_ratio   = STM_WSS_ASPECT_RATIO_UNKNOWN;
    m_pictureInfo.video_aspect_ratio     = STM_WSS_ASPECT_RATIO_UNKNOWN;
    m_pictureInfo.letterbox_style        = STM_LETTERBOX_NONE;
    m_pictureInfo.picture_rescale        = STM_RESCALE_NONE;
    m_pictureInfo.bar_data_present       = STM_BARDATA_NONE;
    m_pictureInfo.bar_top_end_line       = 0;
    m_pictureInfo.bar_left_end_pixel     = 0;
    m_pictureInfo.bar_bottom_start_line  = 0;
    m_pictureInfo.bar_right_start_pixel  = 0;

    ForceAVIUpdate();
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmIFrameManager::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_pParent)
    return false;

  if(!IsStarted())
  {
    /*
     * Note: we start the metadata queues with the master output, not our parent,
     * as that is the one that has valid timing information for the last vsync.
     */
    if(m_pPictureInfoQueue)
      m_pPictureInfoQueue->Start(m_pMaster);

    if(m_pAudioQueue)
      m_pAudioQueue->Start(m_pMaster);

    if(m_pISRCQueue)
      m_pISRCQueue->Start(m_pMaster);

    if(m_pACPQueue)
      m_pACPQueue->Start(m_pMaster);

    if(m_pSPDQueue)
      m_pSPDQueue->Start(m_pMaster);

    if(m_pVendorQueue)
      m_pVendorQueue->Start(m_pMaster);

    if(m_pGamutQueue)
      m_pGamutQueue->Start(m_pMaster);

    if(m_pNTSCQueue)
      m_pNTSCQueue->Start(m_pMaster);

    if(m_p3DEXTQueue)
      m_p3DEXTQueue->Start(m_pMaster);

    SetDefaultPictureInformation();

    uint32_t tmp;
    m_pMaster->GetControl(OUTPUT_CTRL_YCBCR_COLORSPACE, &tmp);
    m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(tmp);
  }

  vibe_os_lock_resource(m_lock);
  {
    if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_2X)
      m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP2;
    else if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_4X)
      m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP4;
    else
      m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP1;

    ForceAVIUpdate();


    SetHDMIVSIData(pModeLine);

    /*
     * ACP packets need to be transmitted at least every 300ms; to be be
     * safe we will aim for every 250ms or 1/4 of a second. The approximate
     * number of HDMI frames between transmissions is therefore the mode's
     * framerate (in mHz) / 4000.
     */
    m_nACPTransmissionFrameDelay = pModeLine->mode_params.vertical_refresh_rate / 4000;
    TRC( TRC_ID_MAIN_INFO, "CSTmIFrameManager::Start - ACP transmission delay = %d", m_nACPTransmissionFrameDelay );

    m_CurrentMode = *pModeLine;
  }
  vibe_os_unlock_resource(m_lock);

  SetDefaultAudioFrame();
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmIFrameManager::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Note: we do not stop the metadata queues because we do not
   * want to flush them; we might be dealing with a hotplug event and
   * the queued data may need to be valid across this. It is up to the
   * application to explicitly flush any queues that no longer have valid data
   * when it handles the hotplug.
   */
  vibe_os_lock_resource(m_lock);
  {
    m_CurrentMode.mode_id   = STM_TIMING_MODE_RESERVED;
    m_bAudioFrameValid      = false;
    m_bTransmitHDMIVSIFrame = false;
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmIFrameManager::SetVICSelectMode(stm_avi_vic_selection_t mode)
{
  vibe_os_lock_resource(m_lock);
  {
    m_VICSelectMode = mode;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}


void CSTmIFrameManager::SetAVIContentType(uint32_t type)
{
  vibe_os_lock_resource(m_lock);
  {
    m_ulContentType = type;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}


void CSTmIFrameManager::SetOverscanMode(uint32_t mode)
{
  vibe_os_lock_resource(m_lock);
  {
    m_ulOverscanMode = mode;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}


void CSTmIFrameManager::SetVCDBQuantization(stm_vcdb_quantization_t quantization)
{
  vibe_os_lock_resource(m_lock);
  {
    m_VCDBQuantization = quantization;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}


void CSTmIFrameManager::SetAVIQuantizationMode(stm_avi_quantization_mode_t mode)
{
  vibe_os_lock_resource(m_lock);
  {
    m_AVIQuantizationMode  = mode;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}

void CSTmIFrameManager::SetAVIExtendedColorimetry(bool valid)
{
  vibe_os_lock_resource(m_lock);
  {
    m_bAVIExtendedColorimetryValid  = valid;
    m_bAVIFrameNeedsUpdate = true;
  }
  vibe_os_unlock_resource(m_lock);
}

void CSTmIFrameManager::ProcessPictureInfoQueue(void)
{
  stm_display_metadata_t *m;

  if(m_pPictureInfoQueue && ((m = m_pPictureInfoQueue->Pop()) != 0))
  {
    stm_picture_format_info_t *p = (stm_picture_format_info_t*)&m->data[0];
    if(p->flags & STM_PIC_INFO_LETTERBOX_STYLE)
    {
      m_pictureInfo.letterbox_style = p->letterbox_style;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_PICTURE_ASPECT)
    {
      m_pictureInfo.picture_aspect_ratio = p->picture_aspect_ratio;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_VIDEO_ASPECT)
    {
      m_pictureInfo.video_aspect_ratio = p->video_aspect_ratio;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_RESCALE)
    {
      m_pictureInfo.picture_rescale = p->picture_rescale;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_BAR_DATA)
    {
      m_pictureInfo.bar_left_end_pixel    = p->bar_left_end_pixel;
      m_pictureInfo.bar_top_end_line      = p->bar_top_end_line;
      m_pictureInfo.bar_right_start_pixel = p->bar_right_start_pixel;
      m_pictureInfo.bar_bottom_start_line = p->bar_bottom_start_line;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_BAR_DATA_PRESENT)
    {
      m_pictureInfo.bar_data_present = p->bar_data_present;
      m_bAVIFrameNeedsUpdate = true;
    }
    stm_meta_data_release(m);
    TRC( TRC_ID_UNCLASSIFIED, "Updated Picture Info" );
  }
}


void CSTmIFrameManager::ProcessAudioQueues(void)
{
  stm_display_metadata_t *m;

  /*
   * We process the audio queues, even if audio isn't enabled yet so the
   * timing can be maintained and so we can get the sample frequency for
   * DST audio to correctly setup the HDMI formatter.
   */
  if(m_pAudioQueue && ((m = m_pAudioQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    InfoFrameChecksum(i);

    vibe_os_lock_resource(m_lock);
      WriteInfoFrame(Audio_transmission_slot,i);
    vibe_os_unlock_resource(m_lock);

    TRC( TRC_ID_UNCLASSIFIED, "Updated Audio Frame" );

    m_nAudioFrequencyIndex = (i->data[2]&HDMI_AUDIO_INFOFRAME_FREQ_MASK)>>HDMI_AUDIO_INFOFRAME_FREQ_SHIFT;

    stm_meta_data_release(m);
  }

  if(m_pACPQueue && ((m = m_pACPQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    m_ACPFrame = *i;

    TRC( TRC_ID_UNCLASSIFIED, "Updated ACP Frame" );

    /*
     * Note no checksum on ACP data packets
     */
    stm_meta_data_release(m);
  }
}


void CSTmIFrameManager::UpdateMuxedIFrameSlots(void)
{
  stm_display_metadata_t *m;

  /*
   * We multiplex ACP/SPD/Vendor IFrames into some transmission slot in that
   * order of priority.
   */

  if(m_nACPTransmissionCount == m_nACPTransmissionFrameDelay)
  {
    m_nACPTransmissionCount = 0;

    vibe_os_lock_resource(m_lock);
      WriteInfoFrame(Muxed_transmission_slot,&m_ACPFrame);
    vibe_os_unlock_resource(m_lock);
  }
  else if( m_nMuxedTransmissionCount %2  == 0)
  {

    if(m_pSPDQueue && ((m = m_pSPDQueue->Pop()) != 0))
    {
      stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
      m_SPDFrame = *i;
      stm_meta_data_release(m);
    }
    InfoFrameChecksum(&m_SPDFrame);

    vibe_os_lock_resource(m_lock);
      WriteInfoFrame(Muxed_transmission_slot,&m_SPDFrame);
    vibe_os_unlock_resource(m_lock);

    m_nMuxedTransmissionCount ++;
    TRC( TRC_ID_UNCLASSIFIED, "Send SPD Frame" );

  }
  else
  {
    /*
     * m_nMuxedTransmissionCount %2 is equal to one in this case,
     * Vendor queue is condidate to be transmitted.
     */
    if (m_pVendorQueue && ((m = m_pVendorQueue->Pop()) != 0))
    {
      stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
      m_VSIFrame = *i;
      stm_meta_data_release(m);
    }
    InfoFrameChecksum(&m_VSIFrame);

    vibe_os_lock_resource(m_lock);
      WriteInfoFrame(Muxed_transmission_slot,&m_VSIFrame);
    vibe_os_unlock_resource(m_lock);
    m_nMuxedTransmissionCount =0;
    TRC( TRC_ID_UNCLASSIFIED, "Send VSI Frame" );
  }
  if(!IsStarted())
  {
    /*
     * Stop frames multiplexed transmission when Frame manager is stopped,
     * Disable transmission from this slot.
     */
    vibe_os_lock_resource(m_lock);
      DisableTransmissionSlot(Muxed_transmission_slot);
    vibe_os_unlock_resource(m_lock);
  }

  /*
   * ISRC and Gamut metadata are mutually exclusive as DVD-A cannot have video
   * with an extended colour space. Both have the property that a frame
   * continues to be sent until it either changes or the content it relates to
   * stops being presented. ISRC transmission is terminated by sending a packet
   * with the status field set to 0 with an appropriate timestamp (after the
   * end of the audio track). If video changes from an extended colourspace
   * to a standard colourspace then Gamut metadata should still be sent but with
   * the No_Crnt_GBD bit set in the header bytes. If the content changes from
   * video with an extended colour space to DVD-A requiring ISRC then the
   * last gamut metadata will be overwritten by the ISRC packets; this seems
   * a reasonable behaviour.
   */
  if(m_pISRCQueue && ((m = m_pISRCQueue->Pop()) != 0))
  {
    stm_hdmi_isrc_data_t *i = (stm_hdmi_isrc_data_t*)&m->data[0];

    vibe_os_lock_resource(m_lock);
    {
      if((i->isrc1.version & HDMI_ISRC1_STATUS_MASK) == 0)
      {
        TRC( TRC_ID_UNCLASSIFIED, "Disabling ISRC Frame" );
        DisableTransmissionSlot(ISRC1_transmission_slot);
        DisableTransmissionSlot(ISRC2_transmission_slot);
        m_bISRCTransmissionInProgress = false;
      }
      else
      {
        TRC( TRC_ID_UNCLASSIFIED, "Sending new ISRC Frame" );
        WriteInfoFrame(ISRC1_transmission_slot,&i->isrc1);

        if(i->isrc1.version & HDMI_ISRC1_CONTINUED)
          WriteInfoFrame(ISRC2_transmission_slot,&i->isrc2);
        else
          DisableTransmissionSlot(ISRC2_transmission_slot);

        m_bISRCTransmissionInProgress = true;
      }
    }
    vibe_os_unlock_resource(m_lock);

    stm_meta_data_release(m);
  }

  /*
   * We want to put NTSC VBI frames into the ISRC2 slot as again they are
   * mutually exclusive. But this is a bit trickier to do as they are single
   * shot transmissions. Disabling the slot has to be smarter so we do not
   * incorrectly disable real ISRC2 packets when they are enabled instead.
   */
  if(m_pNTSCQueue && ((m = m_pNTSCQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    InfoFrameChecksum(i);

    vibe_os_lock_resource(m_lock);
      WriteInfoFrame(NTSC_transmission_slot,i);
    vibe_os_unlock_resource(m_lock);

    stm_meta_data_release(m);
  }
  else
  {
    vibe_os_lock_resource(m_lock);
      if(!m_bISRCTransmissionInProgress)
        DisableTransmissionSlot(NTSC_transmission_slot);
    vibe_os_unlock_resource(m_lock);
  }
}


void CSTmIFrameManager::Process3DEXTDataQueue(void)
{
  stm_display_metadata_t *m;

  if(m_p3DEXTQueue && ((m = m_p3DEXTQueue->Pop()) != 0))
  {
    m_3DEXTData = *((stm_hdmi_info_frame_t*)&m->data[0]);
    stm_meta_data_release(m);
    vibe_os_lock_resource(m_lock);
      SetHDMIVSIData(&m_CurrentMode);
    vibe_os_unlock_resource(m_lock);
  }
}


void CSTmIFrameManager::UpdateFrame(void)
{
  if(!IsStarted())
    return;

  TRC( TRC_ID_UNCLASSIFIED, "" );

  ProcessPictureInfoQueue();

  /*
   * Check to see if the master output (which owns the compositor) colorspace
   * configuration has changed.
   */
  {
    uint32_t tmp;
    m_pMaster->GetControl(OUTPUT_CTRL_YCBCR_COLORSPACE, &tmp);
    if(static_cast<stm_ycbcr_colorspace_t>(tmp) != m_colorspaceMode)
    {
      TRC( TRC_ID_ERROR, "Master's colorspace configuration has changed" );
      m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(tmp);
      m_bAVIFrameNeedsUpdate = true;
    }
  }

  if(m_bAVIFrameNeedsUpdate)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Creating new AVI Frame" );
    vibe_os_lock_resource(m_lock);
    {
      /*
       * Make sure someone hasn't called Stop() on another CPU before
       * we re-program the AVI frame.
       */
      if(IsStarted())
      {
        ProgramAVIFrame();
        WriteInfoFrame(AVI_transmission_slot,&m_AVIFrame);
        m_bAVIFrameNeedsUpdate = false;
      }
    }
    vibe_os_unlock_resource(m_lock);
  }

  /*
   * Only enable HDMI VS IFrames when the 3D extended data queue is available
   * or for extended resolution transmission.
   * This just stops us from trying to send these frames with the simple
   * software CPU driven IFrame manager (not usually used in real systems).
   */
  if(LIKELY(m_p3DEXTQueue)||(m_CurrentMode.mode_params.output_standards == STM_OUTPUT_STD_HDMI_LLC_EXT))
  {
     if(LIKELY(m_p3DEXTQueue))
     {
       Process3DEXTDataQueue();
     }
     else
     {
       vibe_os_lock_resource(m_lock);
         SetHDMIVSIData(&m_CurrentMode);
       vibe_os_unlock_resource(m_lock);
     }

    vibe_os_lock_resource(m_lock);
    {
      if(m_bHDMIVSIFrameNeedsUpdate && m_bTransmitHDMIVSIFrame)
      {
        WriteInfoFrame(HDMIVSI_transmission_slot,&m_HDMIVSIFrame);
        m_bHDMIVSIFrameNeedsUpdate = false;
      }
      else if (!m_bTransmitHDMIVSIFrame)
      {
        DisableTransmissionSlot(HDMIVSI_transmission_slot);
      }
    }
    vibe_os_unlock_resource(m_lock);
  }

  ProcessAudioQueues();

  vibe_os_lock_resource(m_lock);
  {
    if(m_bAudioFrameValid)
    {
      EnableTransmissionSlot(Audio_transmission_slot);

      if(m_ACPFrame.version != HDMI_ACP_TYPE_GENERIC)
        m_nACPTransmissionCount++;
      else
        m_nACPTransmissionCount = 0;
    }
    else
    {
      DisableTransmissionSlot(Audio_transmission_slot);
    }
  }
  vibe_os_unlock_resource(m_lock);

  UpdateMuxedIFrameSlots();

}


/*
 * Helpers for ProgramAVIFrame()
 */
uint32_t CSTmIFrameManager::SelectVICCode(void)
{
  int video_code_index;

  switch(m_VICSelectMode)
  {
    case STM_AVI_VIC_FOLLOW_PICTURE_ASPECT_RATIO:
      TRC( TRC_ID_MAIN_INFO, "- CEA mode based on PICAR" );
      video_code_index = (m_pictureInfo.picture_aspect_ratio == STM_WSS_ASPECT_RATIO_4_3)?STM_AR_INDEX_4_3:STM_AR_INDEX_16_9;
      break;
    case STM_AVI_VIC_4_3:
      TRC( TRC_ID_MAIN_INFO, "- force CEA mode 4:3" );
      video_code_index = STM_AR_INDEX_4_3;
      break;
    case STM_AVI_VIC_16_9:
    default:
      TRC( TRC_ID_MAIN_INFO, "- force CEA mode 16:9" );
      video_code_index = STM_AR_INDEX_16_9;
      break;
  }

  if(m_CurrentMode.mode_params.hdmi_vic_codes[video_code_index] == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "- Warning aspect ratio is not supported by mode" );
    /*
     * If the specified picture aspect is invalid for the display mode then
     * select the other video code as we must send a valid video code to the
     * TV. If the picture aspect information conflicts with the video format
     * then the TV will use the video format.
     */
    video_code_index = (video_code_index == STM_AR_INDEX_4_3)?STM_AR_INDEX_16_9:STM_AR_INDEX_4_3;
  }

  m_VICCode = m_CurrentMode.mode_params.hdmi_vic_codes[video_code_index];

  TRC( TRC_ID_MAIN_INFO, "- HDMI VideoCode = %u", m_VICCode );

  return m_VICCode;
}


uint32_t CSTmIFrameManager::SelectPICAR( void)
{
  if (m_CurrentMode.mode_params.hdmi_vic_codes[0] == m_VICCode)
  {
    TRC( TRC_ID_MAIN_INFO, "- PICAR 4:3" );
    return HDMI_AVI_INFOFRAME_PICAR_43;
  }
  else if ( m_CurrentMode.mode_params.hdmi_vic_codes[1] == m_VICCode)
  {
    TRC( TRC_ID_MAIN_INFO, "- PICAR 16:9" );
    return HDMI_AVI_INFOFRAME_PICAR_169;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "- PICAR NoData" );
    return HDMI_AVI_INFOFRAME_PICAR_NODATA;
  }

}


void CSTmIFrameManager::ProgramAVIActiveFormatInfo(void)
{
  switch(m_pictureInfo.video_aspect_ratio)
  {
    case STM_WSS_ASPECT_RATIO_UNKNOWN:
    {
      TRC( TRC_ID_MAIN_INFO, "- WSS OFF" );
      m_AVIFrame.data[1] &= ~HDMI_AVI_INFOFRAME_AFI;
      /*
       * Even though AFI is disabled, don't send an undefined value.
       */
      m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
      break;
    }
    case STM_WSS_ASPECT_RATIO_4_3:
    {
      TRC( TRC_ID_MAIN_INFO, "- WSS 4x3" );
      switch(m_pictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_SHOOT_AND_PROTECT_4_3:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
          break;
        case STM_LETTERBOX_CENTER:
        case STM_LETTERBOX_TOP:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_43_CENTER;
          break;
        case STM_SHOOT_AND_PROTECT_14_9:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_43_SAP_14_9;
          break;
      }

      break;
    }
    case STM_WSS_ASPECT_RATIO_16_9:
    {
      TRC( TRC_ID_MAIN_INFO, "- WSS 16x9" );
      switch(m_pictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
          break;
        case STM_LETTERBOX_CENTER:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_169_CENTER;
          break;
        case STM_LETTERBOX_TOP:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_169_TOP;
          break;
        case STM_SHOOT_AND_PROTECT_14_9:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_169_SAP_14_9;
          break;
        case STM_SHOOT_AND_PROTECT_4_3:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_169_SAP_4_3;
          break;
      }

      break;
    }
    case STM_WSS_ASPECT_RATIO_14_9:
    {
      TRC( TRC_ID_MAIN_INFO, "- WSS 14x9" );
      switch(m_pictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_SHOOT_AND_PROTECT_14_9:
        case STM_SHOOT_AND_PROTECT_4_3:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
          break;
        case STM_LETTERBOX_CENTER:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_149_CENTER;
          break;
        case STM_LETTERBOX_TOP:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_149_TOP;
          break;
      }

      break;
    }
    case STM_WSS_ASPECT_RATIO_GT_16_9:
    {
      TRC( TRC_ID_MAIN_INFO, "- WSS >16x9" );
      switch(m_pictureInfo.letterbox_style)
      {
        case STM_LETTERBOX_NONE:
        case STM_LETTERBOX_TOP:
        case STM_SHOOT_AND_PROTECT_14_9:
        case STM_SHOOT_AND_PROTECT_4_3:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
          break;
        case STM_LETTERBOX_CENTER:
          m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_GT_169_CENTER;
          break;
      }
      break;
    }
  }
}


stm_avi_quantization_mode_t CSTmIFrameManager::SelectRGBQuantization(void)
{
  if(m_AVIQuantizationMode == STM_AVI_QUANTIZATION_AUTO)
  {
    uint32_t val;
    /*
     * If the output path supports conversion between full and limited range
     * RGB color, then use the current state of that conversion to determine
     * the quantization bits in the AVI infoframe in "auto" mode.
     */
    if(m_pParent->GetControl(OUTPUT_CTRL_RGB_QUANTIZATION_CHANGE, &val) != STM_OUT_OK)
    {
      /*
       * Output doesn't support range reduction/expansion so just use the
       * default setting.
       */
      return STM_AVI_QUANTIZATION_DEFAULT;
    }
    else
    {
      switch(val)
      {
        case STM_RGB_QUANTIZATION_EXPAND:
          return STM_AVI_QUANTIZATION_FULL;
        case STM_RGB_QUANTIZATION_REDUCE:
          return STM_AVI_QUANTIZATION_LIMITED;
        case STM_RGB_QUANTIZATION_BYPASS:
        default:
          return STM_AVI_QUANTIZATION_DEFAULT;
      }
    }
  }

  return m_AVIQuantizationMode;
}


void CSTmIFrameManager::ProgramAVIColorspaceInfo(void)
{
  uint32_t outputformat = m_pParent->GetOutputFormat();
  switch (outputformat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_444))
  {
    case STM_VIDEO_OUT_RGB:
      TRC( TRC_ID_MAIN_INFO, "- Set RGB Output" );
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_RGB;
      if(m_VCDBQuantization & STM_VCDB_QUANTIZATION_RGB)
      {
        switch(SelectRGBQuantization())
        {
          case STM_AVI_QUANTIZATION_LIMITED:
            TRC( TRC_ID_MAIN_INFO, "- RGB Quantization Limited" );
            m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_RGB_QUANT_LIMITED;
            break;
          case STM_AVI_QUANTIZATION_FULL:
            TRC( TRC_ID_MAIN_INFO, "- RGB Quantization Full" );
            m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_RGB_QUANT_FULL;
            break;
          case STM_AVI_QUANTIZATION_DEFAULT:
          default:
            TRC( TRC_ID_MAIN_INFO, "- RGB Quantization Default" );
            m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_RGB_QUANT_DEFAULT;
            break;
        }
      }
      if (m_bAVIExtendedColorimetryValid)
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLORIMETRY_EXT;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_AdobeRGB;
      }
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444):
      TRC( TRC_ID_MAIN_INFO, "- Set YUV Output" );
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_YCBCR444;
      break;
    case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422):
      TRC( TRC_ID_MAIN_INFO, "- Set YUV 4:2:2 Output" );
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_YCBCR422;
      break;
    default:
      break;
  }

  if(outputformat & STM_VIDEO_OUT_YUV)
  {
    /*
     * Auto colorspace selection is based on the master's mode active
     * width to avoid complications with pixel repetition.
     */
    const stm_display_mode_t *mastermode = m_pMaster->GetCurrentDisplayMode();

    if((m_colorspaceMode == STM_YCBCR_COLORSPACE_601) ||
       (m_colorspaceMode == STM_YCBCR_COLORSPACE_AUTO_SELECT && mastermode && (mastermode->mode_params.active_area_width <= 720)))
    {
      TRC( TRC_ID_MAIN_INFO, "- 601 colorspace" );
      if (m_bAVIExtendedColorimetryValid)
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLORIMETRY_EXT;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc601;
      }
      else
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLORIMETRY_ITU601;
      }
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "- 709 colorspace" );
      if (m_bAVIExtendedColorimetryValid)
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLORIMETRY_EXT;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc709;
      }
      else
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLORIMETRY_ITU709;
      }
    }

    if(m_VCDBQuantization & STM_VCDB_QUANTIZATION_YCC)
    {
      switch(m_AVIQuantizationMode)
      {
        case STM_AVI_QUANTIZATION_FULL:
          TRC( TRC_ID_MAIN_INFO, "- YCC Quantization Full" );
          m_AVIFrame.data[5] |= HDMI_AVI_INFOFRAME_YCC_QUANT_FULL;
          break;
        case STM_AVI_QUANTIZATION_AUTO:
        case STM_AVI_QUANTIZATION_DEFAULT:
        case STM_AVI_QUANTIZATION_LIMITED:
        default:
          TRC( TRC_ID_MAIN_INFO, "- YCC Quantization Limited" );
          m_AVIFrame.data[5] |= HDMI_AVI_INFOFRAME_YCC_QUANT_LIMITED;
          break;
      }
    }
  }
}


void CSTmIFrameManager::ProgramAVIContentType(void)
{
  if(m_ulContentType < STM_AVI_IT_UNSPECIFIED)
  {
    m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_ITC;
    m_AVIFrame.data[5] |= m_ulContentType<<HDMI_AVI_INFOFRAME_CN_SHIFT;
  }
}


void CSTmIFrameManager::ProgramAVIBarInfo(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_AVIFrame.data[1] |= ((uint8_t)m_pictureInfo.bar_data_present & 0x3) << 2;

  TRC( TRC_ID_MAIN_INFO, "- data[1] = 0x%x", (int)m_AVIFrame.data[1] );

  m_AVIFrame.data[6] = (uint8_t)(m_pictureInfo.bar_top_end_line & 0xff);
  m_AVIFrame.data[7] = (uint8_t)(m_pictureInfo.bar_top_end_line >> 8);

  m_AVIFrame.data[8] = (uint8_t)(m_pictureInfo.bar_bottom_start_line & 0xff);
  m_AVIFrame.data[9] = (uint8_t)(m_pictureInfo.bar_bottom_start_line >> 8);

  m_AVIFrame.data[10] = (uint8_t)(m_pictureInfo.bar_left_end_pixel & 0xff);
  m_AVIFrame.data[11] = (uint8_t)(m_pictureInfo.bar_left_end_pixel >> 8);

  m_AVIFrame.data[12] = (uint8_t)(m_pictureInfo.bar_right_start_pixel & 0xff);
  m_AVIFrame.data[13] = (uint8_t)(m_pictureInfo.bar_right_start_pixel >> 8);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmIFrameManager::ProgramAVIFrame(void)
{

  TRC( TRC_ID_MAIN_INFO, "mode %d",m_CurrentMode.mode_id );

  /*
   * Set the scan mode and turn AFI information on by default to start with.
   * Clear the rest of byte 1.
   */
  m_AVIFrame.data[1] = HDMI_AVI_INFOFRAME_AFI | m_ulOverscanMode;


  /*
   * Set the non uniform re-scale information and clear the rest of byte 3
   */
  m_AVIFrame.data[3] = ((uint8_t)m_pictureInfo.picture_rescale) & HDMI_AVI_INFOFRAME_HV_SCALE_MASK;

  if(m_CurrentMode.mode_params.output_standards == STM_OUTPUT_STD_HDMI_LLC_EXT)
  {
    /* For HDMI certification purpose, aspect ratio
     * is set to "NO DATA" for 4k2k modes
     */
    m_AVIFrame.data[2] = HDMI_AVI_INFOFRAME_PICAR_NODATA;
    m_AVIFrame.data[4] = 0;
  }
  else
  {
    /*
     * Set the picture aspect ratio. The rest of byte 2 is deliberately cleared.
     * Picture aspect ratio has to match apect ratio corresponding to VIC
     * This byte is filled after VIC selection
     */
    m_AVIFrame.data[2] = SelectPICAR();
    m_AVIFrame.data[4] = SelectVICCode();
  }


  /*
   * Clear everything but the pixelrepetition (set by Start) from byte 5.
   */
  m_AVIFrame.data[5] &= HDMI_AVI_INFOFRAME_PIXELREPMASK;

  /*
   * The following helper functions continue to modify m_AVIFrame directly.
   */
  ProgramAVIActiveFormatInfo();
  ProgramAVIColorspaceInfo();
  ProgramAVIContentType();
  ProgramAVIBarInfo();

  InfoFrameChecksum(&m_AVIFrame);
  PrintInfoFrame(&m_AVIFrame);
}


void CSTmIFrameManager::InfoFrameChecksum(stm_hdmi_info_frame_t *frame)
{
  uint8_t sum;

  sum  = frame->type;
  sum += frame->version;
  sum += frame->length;
  for(int i=1;i<=frame->length;i++)
  {
    sum += frame->data[i];
  }

  /* Store the new checksum */
  frame->data[0] = 256 - sum;
  TRC( TRC_ID_UNCLASSIFIED, "CSTmIFrameManager::InfoFrameChecksum = %x", (unsigned)frame->data[0] );
}


void CSTmIFrameManager::PrintInfoFrame(stm_hdmi_info_frame_t *frame)
{
  TRC( TRC_ID_UNCLASSIFIED, "Info frame dump for %p", frame );
  TRC( TRC_ID_UNCLASSIFIED, "==============================" );
  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_HEAD_WD = 0x%08x", (uint32_t)frame->type           | (((uint32_t)frame->version)<<8) | (((uint32_t)frame->length) <<16) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD0 = 0x%08x", (uint32_t)frame->data[0]         | (((uint32_t)frame->data[1])<<8)  | (((uint32_t)frame->data[2])<<16) | (((uint32_t)frame->data[3])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD1 = 0x%08x", (uint32_t)frame->data[4]         | (((uint32_t)frame->data[5])<<8)  | (((uint32_t)frame->data[6])<<16) | (((uint32_t)frame->data[7])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD2 = 0x%08x", (uint32_t)frame->data[8]          | (((uint32_t)frame->data[9])<<8)   | (((uint32_t)frame->data[10])<<16) | (((uint32_t)frame->data[11])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD3 = 0x%08x", (uint32_t)frame->data[12]         | (((uint32_t)frame->data[13])<<8)  | (((uint32_t)frame->data[14])<<16) | (((uint32_t)frame->data[15])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD4 = 0x%08x", (uint32_t)frame->data[16]         | (((uint32_t)frame->data[17])<<8)  | (((uint32_t)frame->data[18])<<16) | (((uint32_t)frame->data[19])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD5 = 0x%08x", (uint32_t)frame->data[20]         | (((uint32_t)frame->data[21])<<8)  | (((uint32_t)frame->data[22])<<16) | (((uint32_t)frame->data[23])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "STM_HDMI_IFRAME_PKT_WD6 = 0x%08x", (uint32_t)frame->data[24]         | (((uint32_t)frame->data[25])<<8)  | (((uint32_t)frame->data[26])<<16) | (((uint32_t)frame->data[27])<<24) );

  TRC( TRC_ID_UNCLASSIFIED, "==============================" );

}


stm_display_metadata_result_t CSTmIFrameManager::QueueMetadata(stm_display_metadata_t *m)
{
  /*
   * TODO, validate that some basics about each IFrame data is correct
   */
  switch(m->type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
      if(m_pPictureInfoQueue)
        return m_pPictureInfoQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_AUDIO_IFRAME:
      if(m_pAudioQueue)
        return m_pAudioQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_ISRC_DATA:
      if(m_pISRCQueue)
        return m_pISRCQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_ACP_DATA:
      if(m_pACPQueue)
        return m_pACPQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_SPD_IFRAME:
      if(m_pSPDQueue)
        return m_pSPDQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_VENDOR_IFRAME:
      if(m_pVendorQueue)
        return m_pVendorQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_COLOR_GAMUT_DATA:
      if(m_pGamutQueue)
        return m_pGamutQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_NTSC_IFRAME:
      if(m_pNTSCQueue)
        return m_pNTSCQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    case STM_METADATA_TYPE_HDMI_VSIF_3D_EXT:
      if(m_p3DEXTQueue)
        return m_p3DEXTQueue->Queue(m);
      else
        return STM_METADATA_RES_UNSUPPORTED_TYPE;

    default:
      return STM_METADATA_RES_UNSUPPORTED_TYPE;
  }

}


void CSTmIFrameManager::FlushMetadata(stm_display_metadata_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
    {
      if(m_pPictureInfoQueue)
        m_pPictureInfoQueue->Flush();

      if(IsStarted())
        SetDefaultPictureInformation();

      break;
    }
    case STM_METADATA_TYPE_AUDIO_IFRAME:
    {
      if(m_pAudioQueue)
        return m_pAudioQueue->Flush();

      SetDefaultAudioFrame();

      break;
    }
    case STM_METADATA_TYPE_ISRC_DATA:
    {
      if(m_pISRCQueue)
      {
        m_pISRCQueue->Flush();
        vibe_os_lock_resource(m_lock);
        {
          DisableTransmissionSlot(ISRC1_transmission_slot);
          DisableTransmissionSlot(ISRC2_transmission_slot);
          m_bISRCTransmissionInProgress = false;
        }
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case STM_METADATA_TYPE_ACP_DATA:
    {
      if(m_pACPQueue)
      {
        m_pACPQueue->Flush();
        m_ACPFrame.version = HDMI_ACP_TYPE_GENERIC;
      }
      break;
    }
    case STM_METADATA_TYPE_SPD_IFRAME:
    {
      if(m_pSPDQueue)
        m_pSPDQueue->Flush();

      break;
    }
    case STM_METADATA_TYPE_VENDOR_IFRAME:
    {
      if(m_pVendorQueue)
        m_pVendorQueue->Flush();

      break;
    }
    case STM_METADATA_TYPE_COLOR_GAMUT_DATA:
    {
      if(m_pGamutQueue)
      {
        m_pGamutQueue->Flush();
        vibe_os_lock_resource(m_lock);
          DisableTransmissionSlot(Gamut_transmission_slot);
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    case STM_METADATA_TYPE_NTSC_IFRAME:
    {
      if(m_pNTSCQueue)
        m_pNTSCQueue->Flush();

      break;
    }
    case STM_METADATA_TYPE_HDMI_VSIF_3D_EXT:
    {
      if(m_p3DEXTQueue)
      {
        m_p3DEXTQueue->Flush();

        vibe_os_lock_resource(m_lock);
          vibe_os_zero_memory(&m_3DEXTData,sizeof(stm_hdmi_info_frame_t));
          if(IsStarted())
            SetHDMIVSIData(&m_CurrentMode);
        vibe_os_unlock_resource(m_lock);
      }
      break;
    }
    default:
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


uint32_t CSTmIFrameManager::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch (ctrl)
  {
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      ForceAVIUpdate();
      break;
    }
    case OUTPUT_CTRL_AVI_VIC_SELECT:
    {
      if(newVal > STM_AVI_VIC_16_9)
        return STM_OUT_INVALID_VALUE;

      SetVICSelectMode(static_cast<stm_avi_vic_selection_t>(newVal));
      break;
    }
    case OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT:
    {
      if(newVal > STM_VCDB_QUANTIZATION_BOTH)
        return STM_OUT_INVALID_VALUE;

      SetVCDBQuantization(static_cast<stm_vcdb_quantization_t>(newVal));
      break;
    }
    case OUTPUT_CTRL_AVI_QUANTIZATION_MODE:
    {
      if(newVal > STM_AVI_QUANTIZATION_FULL)
        return STM_OUT_INVALID_VALUE;

      SetAVIQuantizationMode(static_cast<stm_avi_quantization_mode_t>(newVal));
      break;
    }
    case OUTPUT_CTRL_AVI_SCAN_INFO:
    {
      if(newVal > STM_AVI_UNDERSCAN)
        return STM_OUT_INVALID_VALUE;

      SetOverscanMode(newVal);
      break;
    }
    case OUTPUT_CTRL_AVI_CONTENT_TYPE:
    {
      if(newVal > STM_AVI_IT_UNSPECIFIED)
        return STM_OUT_INVALID_VALUE;

      SetAVIContentType(newVal);
      break;
    }
    case OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO:
    {
      SetAVIExtendedColorimetry(newVal);
      break;
    }
    default:
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CSTmIFrameManager::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_AVI_VIC_SELECT:
    {
      *val = (uint32_t)GetVICSelectMode();
      break;
    }
    case OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT:
    {
      *val = (uint32_t)GetVCDBQuantization();
      break;
    }
    case OUTPUT_CTRL_AVI_QUANTIZATION_MODE:
    {
      *val = (uint32_t)GetAVIQuantizationMode();
      break;
    }
    case OUTPUT_CTRL_AVI_SCAN_INFO:
    {
      *val = GetOverscanMode();
      break;
    }
    case OUTPUT_CTRL_AVI_CONTENT_TYPE:
    {
      *val = GetAVIContentType();
      break;
    }
    case OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO:
    {
      *val= GetAVIExtendedColorimetry();
      break;
    }
    default:
    {
      *val = 0;
      return STM_OUT_NO_CTRL;
    }
  }

  return STM_OUT_OK;
}
/******************************************************************************/

/*
 * This is a simple CPU interrupt driven IFrame manager which only support three
 * slots, AVI,Audio and the muxed ACP/SPD and Vendor frames.
 *
 * All other IFrames and data packets will be thrown away, i.e. this will not
 * be usable for DVD-A or content in an extended colour space.
 */

CSTmCPUIFrames::CSTmCPUIFrames(CDisplayDevice *pDev,uint32_t uHDMIOffset): CSTmIFrameManager(pDev,uHDMIOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_zero_memory(&m_IFrameSlots,sizeof(DMA_Area));
  m_nSlots = max_transmission_slots;
  m_slots = 0;
  m_nNextSlot = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmCPUIFrames::~CSTmCPUIFrames(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_free_dma_area(&m_IFrameSlots);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmCPUIFrames::Create(CSTmHDMI *parent, COutput *master)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CSTmIFrameManager::Create(parent,master))
    return false;

  vibe_os_allocate_dma_area(&m_IFrameSlots,sizeof(stm_hw_iframe_t)*m_nSlots,0,SDAAF_NONE);
  if(m_IFrameSlots.pMemory == 0)
  {
    TRC( TRC_ID_ERROR, "Unable to create IFrame" );
    return false;
  }

  /*
   * Clear everything to 0, which will also disable all slots
   */
  vibe_os_memset_dma_area(&m_IFrameSlots,0,0,m_IFrameSlots.ulDataSize);

  m_slots = (stm_hw_iframe_t*)m_IFrameSlots.pData;
  TRC( TRC_ID_MAIN_INFO, "m_slots = %p",m_slots );

  m_pPictureInfoQueue = new CMetaDataQueue(STM_METADATA_TYPE_PICTURE_INFO,2,0);
  if(!m_pPictureInfoQueue || !m_pPictureInfoQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create picture info queue" );
    return false;
  }

  m_pAudioQueue = new CMetaDataQueue(STM_METADATA_TYPE_AUDIO_IFRAME,2,0);
  if(!m_pAudioQueue || !m_pAudioQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create audio info queue" );
    return false;
  }

  m_pACPQueue = new CMetaDataQueue(STM_METADATA_TYPE_ACP_DATA,2,0);
  if(!m_pACPQueue || !m_pACPQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create ACP queue" );
    return false;
  }

  m_pSPDQueue = new CMetaDataQueue(STM_METADATA_TYPE_SPD_IFRAME,2,0);
  if(!m_pSPDQueue || !m_pSPDQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create SPD queue" );
    return false;
  }

  m_pVendorQueue = new CMetaDataQueue(STM_METADATA_TYPE_VENDOR_IFRAME,10,0);
  if(!m_pVendorQueue || !m_pVendorQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create Vendor queue" );
    return false;
  }


  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTmCPUIFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  return STM_HDMI_INT_IFRAME;
}


void CSTmCPUIFrames::WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  if(transmissionSlot >= m_nSlots)
    return;

  TRC( TRC_ID_UNCLASSIFIED, "Write to Slot %d",transmissionSlot );

  m_slots[transmissionSlot].header =  ((uint32_t)frame->type           |
                                       (((uint32_t)frame->version)<<8) |
                                       (((uint32_t)frame->length) <<16));

  m_slots[transmissionSlot].data[0] = ((uint32_t)frame->data[0]         |
                                       (((uint32_t)frame->data[1])<<8)  |
                                       (((uint32_t)frame->data[2])<<16) |
                                       (((uint32_t)frame->data[3])<<24));

  m_slots[transmissionSlot].data[1] = ((uint32_t)frame->data[4]         |
                                       (((uint32_t)frame->data[5])<<8)  |
                                       (((uint32_t)frame->data[6])<<16) |
                                       (((uint32_t)frame->data[7])<<24));

  m_slots[transmissionSlot].data[2] = ((uint32_t)frame->data[8]          |
                                       (((uint32_t)frame->data[9])<<8)   |
                                       (((uint32_t)frame->data[10])<<16) |
                                       (((uint32_t)frame->data[11])<<24));

  m_slots[transmissionSlot].data[3] = ((uint32_t)frame->data[12]         |
                                       (((uint32_t)frame->data[13])<<8)  |
                                       (((uint32_t)frame->data[14])<<16) |
                                       (((uint32_t)frame->data[15])<<24));

  m_slots[transmissionSlot].data[4] = ((uint32_t)frame->data[16]         |
                                       (((uint32_t)frame->data[17])<<8)  |
                                       (((uint32_t)frame->data[18])<<16) |
                                       (((uint32_t)frame->data[19])<<24));

  m_slots[transmissionSlot].data[5] = ((uint32_t)frame->data[20]         |
                                       (((uint32_t)frame->data[21])<<8)  |
                                       (((uint32_t)frame->data[22])<<16) |
                                       (((uint32_t)frame->data[23])<<24));

  m_slots[transmissionSlot].data[6] = ((uint32_t)frame->data[24]         |
                                       (((uint32_t)frame->data[25])<<8)  |
                                       (((uint32_t)frame->data[26])<<16) |
                                       (((uint32_t)frame->data[27])<<24));

  m_slots[transmissionSlot].enable = STM_HDMI_IFRAME_CFG_EN;
}


void CSTmCPUIFrames::EnableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

  TRC( TRC_ID_UNCLASSIFIED, "Enable Slot %d",transmissionSlot );

  m_slots[transmissionSlot].enable = STM_HDMI_IFRAME_CFG_EN;
}


void CSTmCPUIFrames::DisableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

#ifdef DEBUG
  if(m_slots[transmissionSlot].enable != 0)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Disable Slot %d",transmissionSlot );
  }
#endif

  m_slots[transmissionSlot].enable = 0;
}


void CSTmCPUIFrames::SendNextInfoFrame(void)
{
  if(!IsStarted() || (m_nNextSlot >= m_nSlots))
  {
    TRC( TRC_ID_UNCLASSIFIED, "Finished all slots" );
    return;
  }

  while(!m_slots[m_nNextSlot].enable)
  {
    m_nNextSlot++;
    if(m_nNextSlot >= m_nSlots)
    {
      TRC( TRC_ID_UNCLASSIFIED, "No more enabled slots" );
      return;
    }
  }

  TRC( TRC_ID_UNCLASSIFIED, "Sending Slot %d HDMI Status = 0x%x IFrame FIFO Status = 0x%x", m_nNextSlot, ReadHDMIReg(STM_HDMI_STA), ReadHDMIReg(STM_HDMI_IFRAME_FIFO_STA) );

  WriteHDMIReg(STM_HDMI_IFRAME_HEAD_WD, m_slots[m_nNextSlot].header);

  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD0, m_slots[m_nNextSlot].data[0]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD1, m_slots[m_nNextSlot].data[1]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD2, m_slots[m_nNextSlot].data[2]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD3, m_slots[m_nNextSlot].data[3]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD4, m_slots[m_nNextSlot].data[4]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD5, m_slots[m_nNextSlot].data[5]);
  WriteHDMIReg(STM_HDMI_IFRAME_PKT_WD6, m_slots[m_nNextSlot].data[6]);

  WriteHDMIReg(STM_HDMI_IFRAME_CFG, m_slots[m_nNextSlot].enable);

  m_nNextSlot++;
}


void CSTmCPUIFrames::SendFirstInfoFrame(void)
{
  m_nNextSlot = 0;
  SendNextInfoFrame();
}


void CSTmCPUIFrames::ProcessInfoFrameComplete(uint32_t interruptStatus)
{
  /*
   * Just send the next one in this implementation
   */
  SendNextInfoFrame();
}
