/***********************************************************************
 *
 * File: display/ip/hdmi/stmv29iframes.cpp
 * Copyright (c) 2009-2011 STMicroelectronics Limited.
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
#include <display/generic/MetaDataQueue.h>

#include "stmhdmi.h"
#include "stmhdmiregs.h"
#include "stmv29iframes.h"


CSTmV29IFrames::CSTmV29IFrames(CDisplayDevice *pDev,uint32_t uHDMIOffset): CSTmIFrameManager(pDev,uHDMIOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_nSlots = 6;
  m_ulConfig = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmV29IFrames::~CSTmV29IFrames(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmV29IFrames::Create(CSTmHDMI *parent, COutput *master)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CSTmIFrameManager::Create(parent,master))
    return false;

  m_pPictureInfoQueue = new CMetaDataQueue(STM_METADATA_TYPE_PICTURE_INFO,2,0);
  if(!m_pPictureInfoQueue || !m_pPictureInfoQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create picture info queue" );
    return false;
  }

  m_p3DEXTQueue = new CMetaDataQueue(STM_METADATA_TYPE_HDMI_VSIF_3D_EXT,2,0);
  if(!m_p3DEXTQueue || !m_p3DEXTQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create 3D extended data queue" );
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

  /*
   * ISRC sized for 3 packets x 3 audio tracks + 1 packet to disable, which
   * is arbitrary but should be sufficient for dealing with very short
   * introduction tracks.
   */
  m_pISRCQueue = new CMetaDataQueue(STM_METADATA_TYPE_ISRC_DATA,10,0);
  if(!m_pISRCQueue || !m_pISRCQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create ISRC queue" );
    return false;
  }

  /*
   * It isn't quite clear how much information can be sent from the CEA861
   * spec. Size it for 1s worth at 30 frames/s for the moment.
   */
  m_pNTSCQueue = new CMetaDataQueue(STM_METADATA_TYPE_NTSC_IFRAME,30,0);
  if(!m_pNTSCQueue || !m_pNTSCQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create NTSC queue" );
    return false;
  }

  /*
   * We need to de-queue color gamut frames one vsync early.
   */
  m_pGamutQueue = new CMetaDataQueue(STM_METADATA_TYPE_COLOR_GAMUT_DATA,30,1);
  if(!m_pGamutQueue || !m_pGamutQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create Gamut queue" );
    return false;
  }

  SetDefaultAudioFrame();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTmV29IFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  /*
   * We need to see the first slot interrupt to process updates to gamut
   * metadata correctly, due to its tight timing requirements.
   */
  return STM_HDMI_INT_IFRAME;
}


bool CSTmV29IFrames::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Override the base class so we can do everything under lock
   */
  vibe_os_lock_resource(m_lock);
  {
    m_CurrentMode.mode_id   = STM_TIMING_MODE_RESERVED;
    m_bAudioFrameValid      = false;
    m_bTransmitHDMIVSIFrame = false;
    m_ulConfig              = 0;

    WriteHDMIReg(STM_HDMI_IFRAME_CFG,m_ulConfig);
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmV29IFrames::UpdateFrame(void)
{
  stm_display_metadata_t *m;

  if(!IsStarted())
    return;

  TRC( TRC_ID_UNCLASSIFIED, "" );

  /*
   * Check to see if we are already processing the gamut queue in the
   * iframe complete handler for hardware slot 1 (software slot 0). If not then
   * we need to check to see if any Gamut data has now been queued and kick off
   * its processing.
   */
  if(((m_ulConfig & STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,1)) == STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_DISABLED,1)) ||
     ((m_ulConfig & STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,1)) == STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_SINGLE_SHOT,1)))
  {
    if(m_pGamutQueue && ((m = m_pGamutQueue->Pop()) != 0))
    {
      stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
      WriteInfoFrameHelper(0,i);

      /*
       * Only enable the slot in field mode if the output is transmitting HDMI.
       * If in DVI mode, skip this so this on the next update frame we process
       * the queue again here. This means the application can queue metadata
       * without worrying about if the connected sink can receive it or not.
       */
      uint32_t val;
      m_pParent->GetControl(OUTPUT_CTRL_VIDEO_OUT_SELECT, &val);
      if(val & STM_VIDEO_OUT_HDMI)
      {
        m_ulConfig &= ~STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,1);
        m_ulConfig |= STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_FIELD,1);
      }
      stm_meta_data_release(m);
    }
  }

  /*
   * Color gamut and ISRC packets should be mutually exclusive, if they are
   * both present then the following call will overwrite the gamut data we
   * have just set.
   */
  CSTmIFrameManager::UpdateFrame();

}


static const uint32_t slotoffset[6] = {
  STM_HDMI_IFRAME_HEAD_WD,
  STM_HDMI_DI2_HEAD_WD,
  STM_HDMI_DI3_HEAD_WD,
  STM_HDMI_DI4_HEAD_WD,
  STM_HDMI_DI5_HEAD_WD,
  STM_HDMI_DI6_HEAD_WD
};


void CSTmV29IFrames::WriteInfoFrameHelper(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  uint32_t reg = slotoffset[transmissionSlot];
  WriteHDMIReg(reg, ((uint32_t)frame->type          |
                    (((uint32_t)frame->version)<<8) |
                    (((uint32_t)frame->length) <<16)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[0]        |
                    (((uint32_t)frame->data[1])<<8)  |
                    (((uint32_t)frame->data[2])<<16) |
                    (((uint32_t)frame->data[3])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[4]        |
                    (((uint32_t)frame->data[5])<<8)  |
                    (((uint32_t)frame->data[6])<<16) |
                    (((uint32_t)frame->data[7])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[8]         |
                    (((uint32_t)frame->data[9])<<8)   |
                    (((uint32_t)frame->data[10])<<16) |
                    (((uint32_t)frame->data[11])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[12]        |
                    (((uint32_t)frame->data[13])<<8)  |
                    (((uint32_t)frame->data[14])<<16) |
                    (((uint32_t)frame->data[15])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[16]        |
                    (((uint32_t)frame->data[17])<<8)  |
                    (((uint32_t)frame->data[18])<<16) |
                    (((uint32_t)frame->data[19])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[20]        |
                    (((uint32_t)frame->data[21])<<8)  |
                    (((uint32_t)frame->data[22])<<16) |
                    (((uint32_t)frame->data[23])<<24)));

  reg += sizeof(uint32_t);
  WriteHDMIReg(reg, ((uint32_t)frame->data[24]        |
                    (((uint32_t)frame->data[25])<<8)  |
                    (((uint32_t)frame->data[26])<<16) |
                    (((uint32_t)frame->data[27])<<24)));
}


void CSTmV29IFrames::WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  if(transmissionSlot >= m_nSlots)
    return;

  TRC( TRC_ID_UNCLASSIFIED, "Write to Slot %d",transmissionSlot );

  WriteInfoFrameHelper(transmissionSlot,frame);
  EnableTransmissionSlot(transmissionSlot);
}


void CSTmV29IFrames::EnableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

  /*
   * Note: the hardware slots are numbered from 1 not zero.
   */
  m_ulConfig &= ~STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,transmissionSlot+1);
  m_ulConfig |= STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_SINGLE_SHOT,transmissionSlot+1);

  TRC( TRC_ID_UNCLASSIFIED, "Enable Slot %d m_ulConfig = 0x%x",transmissionSlot,m_ulConfig );
}


void CSTmV29IFrames::DisableTransmissionSlot(int transmissionSlot)
{
  if(transmissionSlot >= m_nSlots)
    return;

  /*
   * Note: the hardware slots are numbered from 1 not zero.
   */
  m_ulConfig &= ~STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,transmissionSlot+1);
}


void CSTmV29IFrames::SendFirstInfoFrame(void)
{
  WriteHDMIReg(STM_HDMI_DI_DMA_CFG, 0);
  WriteHDMIReg(STM_HDMI_IFRAME_CFG, m_ulConfig);
}


void CSTmV29IFrames::ProcessInfoFrameComplete(uint32_t interruptStatus)
{
  stm_display_metadata_t *m;

  /*
   * If we got a completion interrupt after being stopped, ignore it
   */
  if(!IsStarted())
    return;

  /*
   * If the first slot isn't setup to update each field we haven't started any
   * gamut packets, so nothing more to do.
   */
  if((m_ulConfig & STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,1)) != STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_FIELD,1))
    return;

  if(m_pGamutQueue && ((m = m_pGamutQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    WriteInfoFrameHelper(0,i);
    stm_meta_data_release(m);
  }
  /*
   * Colour Gamut packets are persistent until there is a flush. So we do NOT
   * turn the slot off when the queue is empty.
   */
}
