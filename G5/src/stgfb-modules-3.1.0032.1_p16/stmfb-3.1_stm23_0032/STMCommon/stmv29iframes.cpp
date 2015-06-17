/***********************************************************************
 *
 * File: STMCommon/stmv29iframes.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
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
#include "stmv29iframes.h"


CSTmV29IFrames::CSTmV29IFrames(CDisplayDevice *pDev,ULONG ulHDMIOffset): CSTmIFrameManager(pDev,ulHDMIOffset)
{
  DENTRY();

  m_nSlots = 6;
  m_ulConfig = 0;

  DEXIT();
}

CSTmV29IFrames::~CSTmV29IFrames(void)
{
  DENTRY();

  DEXIT();
}


bool CSTmV29IFrames::Create(CSTmHDMI *parent, COutput *master)
{
  DENTRY();

  if(!CSTmIFrameManager::Create(parent,master))
    return false;

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

  /*
   * ISRC sized for 3 packets x 3 audio tracks + 1 packet to disable, which
   * is arbitrary but should be sufficient for dealing with very short
   * introduction tracks.
   */
  m_pISRCQueue = new CMetaDataQueue(STM_METADATA_TYPE_ISRC_DATA,10,0);
  if(!m_pISRCQueue || !m_pISRCQueue->Create())
  {
    DERROR("Unable to create ISRC queue\n");
    return false;
  }

  /*
   * It isn't quite clear how much information can be sent from the CEA861
   * spec. Size it for 1s worth at 30 frames/s for the moment.
   */
  m_pNTSCQueue = new CMetaDataQueue(STM_METADATA_TYPE_NTSC_IFRAME,30,0);
  if(!m_pNTSCQueue || !m_pNTSCQueue->Create())
  {
    DERROR("Unable to create NTSC queue\n");
    return false;
  }

  /*
   * We need to de-queue color gamut frames one vsync early.
   */
  m_pGamutQueue = new CMetaDataQueue(STM_METADATA_TYPE_COLOR_GAMUT_DATA,30,1);
  if(!m_pGamutQueue || !m_pGamutQueue->Create())
  {
    DERROR("Unable to create Gamut queue\n");
    return false;
  }

  SetDefaultAudioFrame();

  DEXIT();
  return true;
}


ULONG CSTmV29IFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  /*
   * We need to see the first slot interrupt to process updates to gamut
   * metadata correctly, due to its tight timing requirements.
   */
  return STM_HDMI_INT_IFRAME;
}


bool CSTmV29IFrames::Stop(void)
{
  DENTRY();

  /*
   * Override the base class so we can do everything under interrupt lock
   */
  g_pIOS->LockResource(m_ulLock);

  m_pCurrentMode     = 0;
  m_ulTVStandard     = 0;
  m_bAudioFrameValid = false;
  m_ulConfig         = 0;

  WriteHDMIReg(STM_HDMI_IFRAME_CFG,m_ulConfig);

  g_pIOS->UnlockResource(m_ulLock);

  DEXIT();
  return true;
}


void CSTmV29IFrames::UpdateFrame(void)
{
  stm_meta_data_t *m;

  if(!m_pCurrentMode)
    return;

  DEBUGF2(4,("%s\n",__PRETTY_FUNCTION__));

  if(m_pGamutQueue && ((m = m_pGamutQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    WriteInfoFrameHelper(0,i);
    /*
     * Note: the hardware slots are numbered from 1 not zero.
     */
    m_ulConfig &= ~STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_MASK,1);
    m_ulConfig |= STM_HDMI_IFRAME_CFG_DI_n(STM_HDMI_IFRAME_FIELD,1);
    stm_meta_data_release(m);
  }

  /*
   * Color gamut and ISRC packets should be mutually exclusive, if they are
   * both present then the following call will overwrite the gamut data we
   * have just set.
   */
  CSTmIFrameManager::UpdateFrame();

}


static ULONG slotoffset[6] = {
  STM_HDMI_IFRAME_HEAD_WD,
  STM_HDMI_DI2_HEAD_WD,
  STM_HDMI_DI3_HEAD_WD,
  STM_HDMI_DI4_HEAD_WD,
  STM_HDMI_DI5_HEAD_WD,
  STM_HDMI_DI6_HEAD_WD
};


void CSTmV29IFrames::WriteInfoFrameHelper(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  ULONG reg = slotoffset[transmissionSlot];
  WriteHDMIReg(reg, ((ULONG)frame->type          |
                    (((ULONG)frame->version)<<8) |
                    (((ULONG)frame->length) <<16)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[0]        |
                    (((ULONG)frame->data[1])<<8)  |
                    (((ULONG)frame->data[2])<<16) |
                    (((ULONG)frame->data[3])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[4]        |
                    (((ULONG)frame->data[5])<<8)  |
                    (((ULONG)frame->data[6])<<16) |
                    (((ULONG)frame->data[7])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[8]         |
                    (((ULONG)frame->data[9])<<8)   |
                    (((ULONG)frame->data[10])<<16) |
                    (((ULONG)frame->data[11])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[12]        |
                    (((ULONG)frame->data[13])<<8)  |
                    (((ULONG)frame->data[14])<<16) |
                    (((ULONG)frame->data[15])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[16]        |
                    (((ULONG)frame->data[17])<<8)  |
                    (((ULONG)frame->data[18])<<16) |
                    (((ULONG)frame->data[19])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[20]        |
                    (((ULONG)frame->data[21])<<8)  |
                    (((ULONG)frame->data[22])<<16) |
                    (((ULONG)frame->data[23])<<24)));

  reg += sizeof(ULONG);
  WriteHDMIReg(reg, ((ULONG)frame->data[24]        |
                    (((ULONG)frame->data[25])<<8)  |
                    (((ULONG)frame->data[26])<<16) |
                    (((ULONG)frame->data[27])<<24)));
}


void CSTmV29IFrames::WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *frame)
{
  if(transmissionSlot >= m_nSlots)
    return;

  DEBUGF2(4,("%s: Write to Slot %d\n",__PRETTY_FUNCTION__,transmissionSlot));

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

  DEBUGF2(4,("%s: Enable Slot %d m_ulConfig = 0x%lx\n",__PRETTY_FUNCTION__,transmissionSlot,m_ulConfig));
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


void CSTmV29IFrames::ProcessInfoFrameComplete(ULONG interruptStatus)
{
  stm_meta_data_t *m;

  /*
   * If we got a completion interrupt after being stopped, ignore it
   */
  if(!m_pCurrentMode)
    return;

  /*
   * If the first slot isn't setup to update each field we haven't started any
   * gamut packets, so nothing more to do.
   */
  if((m_ulConfig & STM_HDMI_IFRAME_MASK) != STM_HDMI_IFRAME_FIELD)
    return;

  if(m_pGamutQueue && ((m = m_pGamutQueue->Pop()) != 0))
  {
    stm_hdmi_info_frame_t *i = (stm_hdmi_info_frame_t*)&m->data[0];
    WriteInfoFrameHelper(0,i);
    stm_meta_data_release(m);
  }

}
