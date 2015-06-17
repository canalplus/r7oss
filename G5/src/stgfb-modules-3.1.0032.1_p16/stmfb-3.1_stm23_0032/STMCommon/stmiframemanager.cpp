/***********************************************************************
 *
 * File: STMCommon/stmiframemanager.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>
#include <Generic/Output.h>
#include <Generic/MetaDataQueue.h>

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
static const int Reserved_transmission_slot = 5; /* for >P1 colour gamut packets in the future */

CSTmIFrameManager::CSTmIFrameManager(CDisplayDevice *pDev,
                                     ULONG           ulHDMIOffset)
{
  DENTRY();

  m_pDevRegs     = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulHDMIOffset = ulHDMIOffset;
  m_pParent      = 0;
  m_ulLock       = 0;

  g_pIOS->ZeroMemory(&m_pictureInfo,sizeof(m_pictureInfo));

  m_CEAModeSelect              = STM_HDMI_CEA_MODE_FOLLOW_PICTURE_ASPECT_RATIO;

  m_colorspaceMode             = STM_YCBCR_COLORSPACE_AUTO_SELECT;

  m_pictureInfo.pictureAspect  = STM_WSS_OFF;
  m_pictureInfo.videoAspect    = STM_WSS_OFF;
  m_pictureInfo.letterboxStyle = STM_LETTERBOX_NONE;
  m_pictureInfo.pictureRescale = STM_RESCALE_NONE;

  m_pPictureInfoQueue = 0;
  m_pAudioQueue       = 0;
  m_pISRCQueue        = 0;
  m_pACPQueue         = 0;
  m_pSPDQueue         = 0;
  m_pVendorQueue      = 0;
  m_pGamutQueue       = 0;
  m_pNTSCQueue        = 0;

  m_bAudioFrameValid     = false;
  m_bAVIFrameNeedsUpdate = false;

  m_nAudioFrequencyIndex = 0;

  /*
   * Control the setting of quantization info in the AVI frame based on the
   * hardware signal range setting. By default this is unsupported, we need to
   * be told if the connected TV reports it supports these bits.
   */
  m_quantization   = STM_HDMI_AVI_QUANTIZATION_UNSUPPORTED;

  m_ulOverscanMode = HDMI_AVI_INFOFRAME_NOSCANDATA;
  m_ulContentType  = 0;

  g_pIOS->ZeroMemory(&m_AVIFrame,sizeof(stm_hdmi_info_frame_t));
  m_AVIFrame.type    = HDMI_AVI_INFOFRAME_TYPE;
  m_AVIFrame.version = HDMI_AVI_INFOFRAME_VERSION;
  m_AVIFrame.length  = HDMI_AVI_INFOFRAME_LENGTH;
  m_AVIFrame.data[1] = HDMI_AVI_INFOFRAME_OVERSCAN;

  g_pIOS->ZeroMemory(&m_ACPFrame,sizeof(stm_hdmi_info_frame_t));
  m_ACPFrame.type    = HDMI_ACP_PACKET_TYPE;
  m_ACPFrame.version = HDMI_ACP_TYPE_GENERIC;
  m_nACPTransmissionCount = 0;
  m_nACPTransmissionFrameDelay = 255;

  m_bISRCTransmissionInProgress = false;

  DEXIT();
}


CSTmIFrameManager::~CSTmIFrameManager()
{
  DENTRY();

  if(m_ulLock)
    g_pIOS->DeleteResourceLock(m_ulLock);

  delete m_pPictureInfoQueue;
  delete m_pAudioQueue;
  delete m_pISRCQueue;
  delete m_pACPQueue;
  delete m_pSPDQueue;
  delete m_pVendorQueue;
  delete m_pGamutQueue;
  delete m_pNTSCQueue;

  DEXIT();
}


bool CSTmIFrameManager::Create(CSTmHDMI *parent, COutput *master)
{
  DENTRY();

  if(!parent || !master)
  {
    DERROR("NULL parent or master pointer");
    return false;
  }

  m_ulLock = g_pIOS->CreateResourceLock();
  if(m_ulLock == 0)
  {
    DERROR("failed to create status lock\n");
    return false;
  }


  m_pParent = parent;
  m_pMaster = master;

  DEXIT();

  return true;
}


void CSTmIFrameManager::SetDefaultAudioFrame(void)
{
  DENTRY();

  stm_hdmi_info_frame_t audio_frame = {};

  audio_frame.type    = HDMI_AUDIO_INFOFRAME_TYPE;
  audio_frame.version = HDMI_AUDIO_INFOFRAME_VERSION;
  audio_frame.length  = HDMI_AUDIO_INFOFRAME_LENGTH;
  InfoFrameChecksum(&audio_frame);
  WriteInfoFrame(Audio_transmission_slot,&audio_frame);

  g_pIOS->LockResource(m_ulLock);
  DisableTransmissionSlot(Audio_transmission_slot);
  g_pIOS->UnlockResource(m_ulLock);

  DEXIT();
}


void CSTmIFrameManager::SetDefaultPictureInformation(void)
{
  DENTRY();

  m_pictureInfo.pictureAspect   = STM_WSS_OFF;
  m_pictureInfo.videoAspect     = STM_WSS_OFF;
  m_pictureInfo.letterboxStyle  = STM_LETTERBOX_NONE;
  m_pictureInfo.pictureRescale  = STM_RESCALE_NONE;
  m_pictureInfo.barEnable       = 0;
  m_pictureInfo.topEndLine      = 0;
  m_pictureInfo.leftEndPixel    = 0;
  m_pictureInfo.bottomStartLine = 0;
  m_pictureInfo.rightStartPixel = 0;

  ForceAVIUpdate();

  DEXIT();
}


bool CSTmIFrameManager::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  DENTRY();

  if(!m_pParent)
    return false;

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

  if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
    m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP2;
  else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
    m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP4;
  else
    m_AVIFrame.data[5] = HDMI_AVI_INFOFRAME_PIXELREP1;

  SetDefaultPictureInformation();

  m_nACPTransmissionFrameDelay = (int)((UTIME64)250000/(UTIME64)COutput::GetFieldOrFrameDurationFromMode(pModeLine));
  DEBUGF2(2,("CSTmIFrameManager::Start - ACP transmission delay = %d\n",m_nACPTransmissionFrameDelay));

  m_pCurrentMode = pModeLine;
  m_ulTVStandard = tvStandard;

  DEXIT();
  return true;
}


bool CSTmIFrameManager::Stop(void)
{
  DENTRY();

  /*
   * Note: we do not stop the metadata queues because we do not
   * want to flush them; we might be dealing with a hotplug event and
   * the queued data may need to be valid across this. It is up to the
   * application to explicitly flush any queues that no longer have valid data
   * when it handles the hotplug.
   */
  g_pIOS->LockResource(m_ulLock);

  m_pCurrentMode     = 0;
  m_ulTVStandard     = 0;
  m_bAudioFrameValid = false;

  g_pIOS->UnlockResource(m_ulLock);

  DEXIT();
  return true;
}


void CSTmIFrameManager::UpdateFrame(void)
{
  stm_meta_data_t *m;

  if(!m_pCurrentMode)
    return;

  DEBUGF2(4,("%s\n",__PRETTY_FUNCTION__));

  if(m_pPictureInfoQueue && ((m = m_pPictureInfoQueue->Pop()) != 0))
  {
    stm_picture_format_info_t *p = (stm_picture_format_info_t*)&m->data[0];
    if(p->flags & STM_PIC_INFO_LETTERBOX)
    {
      m_pictureInfo.letterboxStyle = p->letterboxStyle;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_PICTURE_ASPECT)
    {
      m_pictureInfo.pictureAspect = p->pictureAspect;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_VIDEO_ASPECT)
    {
      m_pictureInfo.videoAspect = p->videoAspect;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_RESCALE)
    {
      m_pictureInfo.pictureRescale = p->pictureRescale;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_BAR_DATA)
    {
      m_pictureInfo.leftEndPixel    = p->leftEndPixel;
      m_pictureInfo.topEndLine      = p->topEndLine;
      m_pictureInfo.rightStartPixel = p->rightStartPixel;
      m_pictureInfo.bottomStartLine = p->bottomStartLine;
      m_bAVIFrameNeedsUpdate = true;
    }
    if(p->flags & STM_PIC_INFO_BAR_DATA_ENABLE)
    {
      m_pictureInfo.barEnable = p->barEnable;
      m_bAVIFrameNeedsUpdate = true;
    }
    stm_meta_data_release(m);
    DEBUGF2(3,("%s: Updated Picture Info\n",__PRETTY_FUNCTION__));
  }

  if(m_bAVIFrameNeedsUpdate)
  {
    DEBUGF2(3,("%s: Creating new AVI Frame\n",__PRETTY_FUNCTION__));
    ProgramAVIFrame();
    WriteInfoFrame(AVI_transmission_slot,&m_AVIFrame);
    m_bAVIFrameNeedsUpdate = false;
  }

  /*
   * We process the audio queues, even if audio isn't enabled yet so the
   * timing can be maintained and so we can get the sample frequency for
   * DST audio to correctly setup the HDMI formatter.
   */
  if(m_pAudioQueue && ((m = m_pAudioQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    InfoFrameChecksum(i);
    WriteInfoFrame(Audio_transmission_slot,i);

    DEBUGF2(3,("%s: Updated Audio Frame\n",__PRETTY_FUNCTION__));

    m_nAudioFrequencyIndex = (i->data[2]&HDMI_AUDIO_INFOFRAME_FREQ_MASK)>>HDMI_AUDIO_INFOFRAME_FREQ_SHIFT;

    stm_meta_data_release(m);
  }

  if(m_pACPQueue && ((m = m_pACPQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    m_ACPFrame = *i;

    DEBUGF2(3,("%s: Updated ACP Frame\n",__PRETTY_FUNCTION__));

    /*
     * Note no checksum on ACP data packets
     */
    stm_meta_data_release(m);
  }


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

  /*
   * We multiplex ACP/SPD/Vendor IFrames into some transmission slot in that
   * order of priority.
   */
  if(m_nACPTransmissionCount == m_nACPTransmissionFrameDelay)
  {
    m_nACPTransmissionCount = 0;
    WriteInfoFrame(Muxed_transmission_slot,&m_ACPFrame);
  }
  else if(m_pSPDQueue && ((m = m_pSPDQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    InfoFrameChecksum(i);
    WriteInfoFrame(Muxed_transmission_slot,i);

    DEBUGF2(4,("%s: Send SPD Frame\n",__PRETTY_FUNCTION__));

    stm_meta_data_release(m);
  }
  else if(m_pVendorQueue && ((m = m_pVendorQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    InfoFrameChecksum(i);
    WriteInfoFrame(Muxed_transmission_slot,i);
    stm_meta_data_release(m);
  }
  else
  {
    /*
     * All of the frames multiplexed in this slot are "one shot" so if there
     * is nothing in any of the queues, disable transmission from this slot.
     */
    DisableTransmissionSlot(Muxed_transmission_slot);
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

    if((i->isrc1.version & HDMI_ISRC1_STATUS_MASK) == 0)
    {
      DEBUGF2(4,("%s: Disabling ISRC Frame\n",__PRETTY_FUNCTION__));
      DisableTransmissionSlot(ISRC1_transmission_slot);
      DisableTransmissionSlot(ISRC2_transmission_slot);
      m_bISRCTransmissionInProgress = false;
    }
    else
    {
      DEBUGF2(4,("%s: Sending new ISRC Frame\n",__PRETTY_FUNCTION__));
      WriteInfoFrame(ISRC1_transmission_slot,&i->isrc1);

      if(i->isrc1.version & HDMI_ISRC1_CONTINUED)
        WriteInfoFrame(ISRC2_transmission_slot,&i->isrc2);
      else
        DisableTransmissionSlot(ISRC2_transmission_slot);

      m_bISRCTransmissionInProgress = true;
    }

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
    WriteInfoFrame(NTSC_transmission_slot,i);
    stm_meta_data_release(m);
  }
  else
  {
    if(!m_bISRCTransmissionInProgress)
      DisableTransmissionSlot(NTSC_transmission_slot);
  }
}


void CSTmIFrameManager::ProgramAVIFrame(void)
{
  int video_code_index;

  DEBUGF2(2,("%s: mode %d @ %p\n",__PRETTY_FUNCTION__,m_pCurrentMode->Mode,m_pCurrentMode));

  switch(m_CEAModeSelect)
  {
    case STM_HDMI_CEA_MODE_FOLLOW_PICTURE_ASPECT_RATIO:
      DEBUGF2(2,("%s - CEA mode based on PICAR\n",__PRETTY_FUNCTION__));
      video_code_index = (m_pictureInfo.pictureAspect == STM_WSS_4_3)?AR_INDEX_4_3:AR_INDEX_16_9;
      break;
    case STM_HDMI_CEA_MODE_4_3:
      DEBUGF2(2,("%s - force CEA mode 4:3\n",__PRETTY_FUNCTION__));
      video_code_index = AR_INDEX_4_3;
      break;
    case STM_HDMI_CEA_MODE_16_9:
    default:
      DEBUGF2(2,("%s - force CEA mode 16:9\n",__PRETTY_FUNCTION__));
      video_code_index = AR_INDEX_16_9;
      break;
  }

  if(m_pCurrentMode->ModeParams.HDMIVideoCodes[video_code_index] == 0)
  {
    DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - Warning aspect ratio is not supported by mode\n"));
    /*
     * If the specified picture aspect is invalid for the display mode then
     * select the other video code as we must send a valid video code to the
     * TV. If the picture aspect information conflicts with the video format
     * then the TV will use the video format.
     */
    video_code_index = (video_code_index == AR_INDEX_4_3)?AR_INDEX_16_9:AR_INDEX_4_3;
  }

  m_AVIFrame.data[4] = m_pCurrentMode->ModeParams.HDMIVideoCodes[video_code_index];
  DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - HDMI VideoCode = %ld\n",m_pCurrentMode->ModeParams.HDMIVideoCodes[video_code_index]));

  /*
   * Set the scan mode and turn AFI information on by default to start with.
   * Clear the rest of byte 1.
   */
  m_AVIFrame.data[1] = HDMI_AVI_INFOFRAME_AFI | m_ulOverscanMode;

  /*
   * Set the picture aspect ratio. The rest of byte 2 is deliberately cleared.
   */
  switch (m_pictureInfo.pictureAspect)
  {
    case STM_WSS_4_3:
      DEBUGF2(2,("%s - PICAR 4:3\n",__PRETTY_FUNCTION__));
      m_AVIFrame.data[2] = HDMI_AVI_INFOFRAME_PICAR_43;
      break;
    case STM_WSS_16_9:
      DEBUGF2(2,("%s - PICAR 16:9\n",__PRETTY_FUNCTION__));
      m_AVIFrame.data[2] = HDMI_AVI_INFOFRAME_PICAR_169;
      break;
    case STM_WSS_OFF:
    default:
      DEBUGF2(2,("%s - PICAR NoData\n",__PRETTY_FUNCTION__));
      m_AVIFrame.data[2] = HDMI_AVI_INFOFRAME_PICAR_NODATA;
      break;
  }

  /*
   * Set the non uniform re-scale information and clear the rest of byte 3
   */
  m_AVIFrame.data[3] = ((UCHAR)m_pictureInfo.pictureRescale) & HDMI_AVI_INFOFRAME_HV_SCALE_MASK;

  m_AVIFrame.data[5] &= ~(HDMI_AVI_INFOFRAME_YCC_QUANT_MASK|
                          HDMI_AVI_INFOFRAME_CN_MASK);

  switch(m_pictureInfo.videoAspect)
  {
    case STM_WSS_OFF:
    {
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - WSS OFF\n"));
      m_AVIFrame.data[1] &= ~HDMI_AVI_INFOFRAME_AFI;
      /*
       * Even though AFI is disabled, don't send an undefined value.
       */
      m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_AFI_SAMEP;
      break;
    }
    case STM_WSS_4_3:
    {
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - WSS 4x3\n"));
      switch(m_pictureInfo.letterboxStyle)
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
    case STM_WSS_16_9:
    {
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - WSS 16x9\n"));
      switch(m_pictureInfo.letterboxStyle)
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
    case STM_WSS_14_9:
    {
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - WSS 14x9\n"));
      switch(m_pictureInfo.letterboxStyle)
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
    case STM_WSS_GT_16_9:
    {
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - WSS >16x9\n"));
      switch(m_pictureInfo.letterboxStyle)
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

  switch (m_pParent->GetOutputFormat() & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422))
  {
    case STM_VIDEO_OUT_RGB:
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - Set RGB Output\n"));
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_RGB;
      if(m_quantization & STM_HDMI_AVI_QUANTIZATION_RGB)
      {
        if(m_pParent->GetSignalRange() == STM_SIGNAL_VIDEO_RANGE)
          m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_RGB_QUANT_LIMITED;
        else
          m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_RGB_QUANT_FULL;
      }
      m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_AdobeRGB;

      break;
    case STM_VIDEO_OUT_YUV:
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - Set YUV Output\n"));
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_YCBCR444;

      if((m_colorspaceMode == STM_YCBCR_COLORSPACE_601) ||
         (m_colorspaceMode == STM_YCBCR_COLORSPACE_AUTO_SELECT && (m_pCurrentMode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) == 0))
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLO_ITU601;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc601;
      }
      else
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLO_ITU709;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc709;
      }
      break;
    case STM_VIDEO_OUT_422:
      DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - Set YUV 4:2:2 Output\n"));
      m_AVIFrame.data[1] |= HDMI_AVI_INFOFRAME_YCBCR422;

      if((m_colorspaceMode == STM_YCBCR_COLORSPACE_601) ||
         (m_colorspaceMode == STM_YCBCR_COLORSPACE_AUTO_SELECT && (m_pCurrentMode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) == 0))
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLO_ITU601;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc601;
      }
      else
      {
        m_AVIFrame.data[2] |= HDMI_AVI_INFOFRAME_COLO_ITU709;
        m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_EC_xvYcc709;
      }
      break;
    default:
      break;
  }

  if(m_ulContentType & (1L<<2))
  {
    m_AVIFrame.data[3] |= HDMI_AVI_INFOFRAME_ITC;
    m_AVIFrame.data[5] |= (m_ulContentType&0x3)<<HDMI_AVI_INFOFRAME_CN_SHIFT;
  }

  if(m_pictureInfo.barEnable)
    m_AVIFrame.data[1] |= (HDMI_AVI_INFOFRAME_H_BARVALID|HDMI_AVI_INFOFRAME_V_BARVALID);

  DEBUGF2(2,("CSTmIFrameManager::ProgramAVIFrame - data[1] = 0x%x\n",(int)m_AVIFrame.data[1]));

  m_AVIFrame.data[6] = (UCHAR)(m_pictureInfo.topEndLine & 0xff);
  m_AVIFrame.data[7] = (UCHAR)(m_pictureInfo.topEndLine >> 8);

  m_AVIFrame.data[8] = (UCHAR)(m_pictureInfo.bottomStartLine & 0xff);
  m_AVIFrame.data[9] = (UCHAR)(m_pictureInfo.bottomStartLine >> 8);

  m_AVIFrame.data[10] = (UCHAR)(m_pictureInfo.leftEndPixel & 0xff);
  m_AVIFrame.data[11] = (UCHAR)(m_pictureInfo.leftEndPixel >> 8);

  m_AVIFrame.data[12] = (UCHAR)(m_pictureInfo.rightStartPixel & 0xff);
  m_AVIFrame.data[13] = (UCHAR)(m_pictureInfo.rightStartPixel >> 8);

  InfoFrameChecksum(&m_AVIFrame);
  PrintInfoFrame(&m_AVIFrame);
}


void CSTmIFrameManager::InfoFrameChecksum(stm_hdmi_info_frame_t *frame)
{
UCHAR sum;
int i;

  sum  = frame->type;
  sum += frame->version;
  sum += frame->length;
  for(i=1;i<=frame->length;i++)
  {
    sum += frame->data[i];
  }

  /* Store the new checksum */
  frame->data[0] = 256 - sum;
  DEBUGF2(5,("CSTmIFrameManager::InfoFrameChecksum = %x\n",(unsigned)frame->data[0]));
}


void CSTmIFrameManager::PrintInfoFrame(stm_hdmi_info_frame_t *frame)
{
  DEBUGF2(3,("Info frame dump for %p\n",frame));
  DEBUGF2(3,("==============================\n"));
  DEBUGF2(3,("STM_HDMI_IFRAME_HEAD_WD = 0x%08lx\n", (ULONG)frame->type           |
                                                    (((ULONG)frame->version)<<8) |
                                                    (((ULONG)frame->length) <<16)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD0 = 0x%08lx\n", (ULONG)frame->data[0]         |
                                                    (((ULONG)frame->data[1])<<8)  |
                                                    (((ULONG)frame->data[2])<<16) |
                                                    (((ULONG)frame->data[3])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD1 = 0x%08lx\n", (ULONG)frame->data[4]         |
                                                    (((ULONG)frame->data[5])<<8)  |
                                                    (((ULONG)frame->data[6])<<16) |
                                                    (((ULONG)frame->data[7])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD2 = 0x%08lx\n", (ULONG)frame->data[8]          |
                                                    (((ULONG)frame->data[9])<<8)   |
                                                    (((ULONG)frame->data[10])<<16) |
                                                    (((ULONG)frame->data[11])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD3 = 0x%08lx\n", (ULONG)frame->data[12]         |
                                                    (((ULONG)frame->data[13])<<8)  |
                                                    (((ULONG)frame->data[14])<<16) |
                                                    (((ULONG)frame->data[15])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD4 = 0x%08lx\n", (ULONG)frame->data[16]         |
                                                    (((ULONG)frame->data[17])<<8)  |
                                                    (((ULONG)frame->data[18])<<16) |
                                                    (((ULONG)frame->data[19])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD5 = 0x%08lx\n", (ULONG)frame->data[20]         |
                                                    (((ULONG)frame->data[21])<<8)  |
                                                    (((ULONG)frame->data[22])<<16) |
                                                    (((ULONG)frame->data[23])<<24)));

  DEBUGF2(3,("STM_HDMI_IFRAME_PKT_WD6 = 0x%08lx\n", (ULONG)frame->data[24]         |
                                                    (((ULONG)frame->data[25])<<8)  |
                                                    (((ULONG)frame->data[26])<<16) |
                                                    (((ULONG)frame->data[27])<<24)));

  DEBUGF2(3,("==============================\n"));

}


stm_meta_data_result_t CSTmIFrameManager::QueueMetadata(stm_meta_data_t *m)
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

    default:
      return STM_METADATA_RES_UNSUPPORTED_TYPE;
  }

}


void CSTmIFrameManager::FlushMetadata(stm_meta_data_type_t type)
{
  DENTRY();

  switch(type)
  {
    case STM_METADATA_TYPE_PICTURE_INFO:
    {
      if(m_pPictureInfoQueue)
        m_pPictureInfoQueue->Flush();

      if(m_pCurrentMode)
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
        g_pIOS->LockResource(m_ulLock);
        DisableTransmissionSlot(ISRC1_transmission_slot);
        DisableTransmissionSlot(ISRC2_transmission_slot);
        m_bISRCTransmissionInProgress = false;
        g_pIOS->UnlockResource(m_ulLock);
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
        DisableTransmissionSlot(Gamut_transmission_slot);
      }
      break;
    }
    case STM_METADATA_TYPE_NTSC_IFRAME:
    {
      if(m_pNTSCQueue)
        m_pNTSCQueue->Flush();

      break;
    }
    default:
      break;
  }

  DEXIT();
}


/******************************************************************************/

/*
 * This is a simple CPU interrupt driven IFrame manager which only support three
 * slots, AVI,Audio and the muxed ACP/SPD and Vendor frames.
 *
 * All other IFrames and data packets will be thrown away, i.e. this will not
 * be usable for DVD-A or content in an extended colour space.
 */

CSTmCPUIFrames::CSTmCPUIFrames(CDisplayDevice *pDev,ULONG ulHDMIOffset): CSTmIFrameManager(pDev,ulHDMIOffset)
{
  DENTRY();

  g_pIOS->ZeroMemory(&m_IFrameSlots,sizeof(DMA_Area));
  m_nSlots = max_transmission_slots;
  m_slots = 0;
  m_nNextSlot = 0;

  DEXIT();
}

CSTmCPUIFrames::~CSTmCPUIFrames(void)
{
  DENTRY();

  g_pIOS->FreeDMAArea(&m_IFrameSlots);

  DEXIT();
}


bool CSTmCPUIFrames::Create(CSTmHDMI *parent, COutput *master)
{
  DENTRY();

  if(!CSTmIFrameManager::Create(parent,master))
    return false;

  g_pIOS->AllocateDMAArea(&m_IFrameSlots,sizeof(stm_hw_iframe_t)*m_nSlots,0,SDAAF_NONE);
  if(m_IFrameSlots.pMemory == 0)
  {
    DERROR("Unable to create IFrame\n");
    return false;
  }

  /*
   * Clear everything to 0, which will also disable all slots
   */
  g_pIOS->MemsetDMAArea(&m_IFrameSlots,0,0,m_IFrameSlots.ulDataSize);

  m_slots = (stm_hw_iframe_t*)m_IFrameSlots.pData;
  DEBUGF2(2,("%s: m_slots = %p\n",__PRETTY_FUNCTION__,m_slots));

  m_pPictureInfoQueue = new CMetaDataQueue(STM_METADATA_TYPE_PICTURE_INFO,2,0);
  if(!m_pPictureInfoQueue || !m_pPictureInfoQueue->Create())
  {
    DERROR("Unable to create picture info queue\n");
    return false;
  }

  m_pAudioQueue = new CMetaDataQueue(STM_METADATA_TYPE_AUDIO_IFRAME,2,0);
  if(!m_pAudioQueue || !m_pAudioQueue->Create())
  {
    DERROR("Unable to create audio info queue\n");
    return false;
  }

  m_pACPQueue = new CMetaDataQueue(STM_METADATA_TYPE_ACP_DATA,2,0);
  if(!m_pACPQueue || !m_pACPQueue->Create())
  {
    DERROR("Unable to create ACP queue\n");
    return false;
  }

  m_pSPDQueue = new CMetaDataQueue(STM_METADATA_TYPE_SPD_IFRAME,2,0);
  if(!m_pSPDQueue || !m_pSPDQueue->Create())
  {
    DERROR("Unable to create SPD queue\n");
    return false;
  }

  m_pVendorQueue = new CMetaDataQueue(STM_METADATA_TYPE_VENDOR_IFRAME,10,0);
  if(!m_pVendorQueue || !m_pVendorQueue->Create())
  {
    DERROR("Unable to create Vendor queue\n");
    return false;
  }

  SetDefaultAudioFrame();

  DEXIT();
  return true;
}


ULONG CSTmCPUIFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  return STM_HDMI_INT_IFRAME;
}


void CSTmCPUIFrames::WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  if(transmissionSlot >= m_nSlots)
    return;

  DEBUGF2(4,("%s: Write to Slot %d\n",__PRETTY_FUNCTION__,transmissionSlot));

  m_slots[transmissionSlot].header =  ((ULONG)frame->type          |
                                       (((ULONG)frame->version)<<8) |
                                       (((ULONG)frame->length) <<16));

  m_slots[transmissionSlot].data[0] = ((ULONG)frame->data[0]        |
                                       (((ULONG)frame->data[1])<<8)  |
                                       (((ULONG)frame->data[2])<<16) |
                                       (((ULONG)frame->data[3])<<24));

  m_slots[transmissionSlot].data[1] = ((ULONG)frame->data[4]        |
                                       (((ULONG)frame->data[5])<<8)  |
                                       (((ULONG)frame->data[6])<<16) |
                                       (((ULONG)frame->data[7])<<24));

  m_slots[transmissionSlot].data[2] = ((ULONG)frame->data[8]         |
                                       (((ULONG)frame->data[9])<<8)   |
                                       (((ULONG)frame->data[10])<<16) |
                                       (((ULONG)frame->data[11])<<24));

  m_slots[transmissionSlot].data[3] = ((ULONG)frame->data[12]        |
                                       (((ULONG)frame->data[13])<<8)  |
                                       (((ULONG)frame->data[14])<<16) |
                                       (((ULONG)frame->data[15])<<24));

  m_slots[transmissionSlot].data[4] = ((ULONG)frame->data[16]        |
                                       (((ULONG)frame->data[17])<<8)  |
                                       (((ULONG)frame->data[18])<<16) |
                                       (((ULONG)frame->data[19])<<24));

  m_slots[transmissionSlot].data[5] = ((ULONG)frame->data[20]        |
                                       (((ULONG)frame->data[21])<<8)  |
                                       (((ULONG)frame->data[22])<<16) |
                                       (((ULONG)frame->data[23])<<24));

  m_slots[transmissionSlot].data[6] = ((ULONG)frame->data[24]        |
                                       (((ULONG)frame->data[25])<<8)  |
                                       (((ULONG)frame->data[26])<<16) |
                                       (((ULONG)frame->data[27])<<24));

  m_slots[transmissionSlot].enable = STM_HDMI_IFRAME_CFG_EN;
}


void CSTmCPUIFrames::EnableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

  DEBUGF2(4,("%s: Enable Slot %d\n",__PRETTY_FUNCTION__,transmissionSlot));

  m_slots[transmissionSlot].enable = STM_HDMI_IFRAME_CFG_EN;
}


void CSTmCPUIFrames::DisableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

#ifdef DEBUG
  if(m_slots[transmissionSlot].enable != 0)
  {
    DEBUGF2(4,("%s: Disable Slot %d\n",__PRETTY_FUNCTION__,transmissionSlot));
  }
#endif

  m_slots[transmissionSlot].enable = 0;
}


void CSTmCPUIFrames::SendNextInfoFrame(void)
{
  if(!m_pCurrentMode || (m_nNextSlot >= m_nSlots))
  {
    DEBUGF2(4,("%s: Finished all slots\n",__PRETTY_FUNCTION__));
    return;
  }

  while(!m_slots[m_nNextSlot].enable)
  {
    m_nNextSlot++;
    if(m_nNextSlot >= m_nSlots)
    {
      DEBUGF2(4,("%s: No more enabled slots\n",__PRETTY_FUNCTION__));
      return;
    }
  }

  DEBUGF2(3,("%s: Sending Slot %d HDMI Status = 0x%lx IFrame FIFO Status = 0x%lx\n",
                                     __PRETTY_FUNCTION__,
                                     m_nNextSlot,
                                     ReadHDMIReg(STM_HDMI_STA),
                                     ReadHDMIReg(STM_HDMI_IFRAME_FIFO_STA)));

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


void CSTmCPUIFrames::ProcessInfoFrameComplete(ULONG interruptStatus)
{
  /*
   * Just send the next one in this implementation
   */
  SendNextInfoFrame();
}
