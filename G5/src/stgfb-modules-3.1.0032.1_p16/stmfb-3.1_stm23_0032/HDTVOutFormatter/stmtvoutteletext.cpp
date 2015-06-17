/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutteletext.cpp
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
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

#include "stmtvoutteletext.h"

#define TTXT_CTRL 0x30
#define TTXT_DATA 0x34

#define TTXT_CTRL_EN (1L<<0)
#define TTXT_CTRL_DELAY_SHIFT 1
#define TTXT_CTRL_DELAY_MASK  (0xf << TTXT_CTRL_DELAY_SHIFT)
#define TTXT_CTRL_LINES_SHIFT 5
#define TTXT_CTRL_LINES_MASK  (0x1ff << TTXT_CTRL_LINES_SHIFT)
#define TTXT_CTRL_BYTES_SHIFT 14
#define TTXT_CTRL_BYTES_MASK  (0x3f << TTXT_CTRL_BYTES_SHIFT)

#define DEBUG_DMA_LEVEL 3

CSTmTVOutTeletext::CSTmTVOutTeletext(CDisplayDevice* pDev,
                                     ULONG           ulRegDENCOffset,
                                     ULONG           ulRegTVOutOffset,
                                     ULONG           ulPhysRegisterBase,
                                     STM_DMA_TRANSFER_PACING pacing): CSTmTeletext(pDev,ulRegDENCOffset)
{
  DENTRY();

  m_ulTVOutOffset      = ulRegTVOutOffset;
  m_pacingLine         = pacing;
  m_teletextDMAChannel = 0;
  m_nTVOutDelay        = 2; /* Design default, 2 clocks */

  /*
   * Create a dummy DMA area for the Teletext data register.
   */
  g_pIOS->ZeroMemory(&m_teletextDataRegister,sizeof(m_teletextDataRegister));
  m_teletextDataRegister.ulPhysical = ulPhysRegisterBase+m_ulTVOutOffset+TTXT_DATA;
  m_teletextDataRegister.ulDataSize = 4;
  m_teletextDataRegister.ulFlags    = SDAAF_UNCACHED;


  DEXIT();
}


CSTmTVOutTeletext::~CSTmTVOutTeletext(void)
{
  DENTRY();

  if(m_teletextDMAChannel != 0)
    g_pIOS->ReleaseDMAChannel(m_teletextDMAChannel);

  DEXIT();
}


bool CSTmTVOutTeletext::Create()
{
  DENTRY();

  if(m_pacingLine != SDTP_DENC1_TELETEXT && m_pacingLine != SDTP_DENC2_TELETEXT)
    return false;

  if(!CSTmTeletext::Create())
    return false;

  m_teletextDMAChannel = g_pIOS->GetDMAChannel(m_pacingLine, 48, SDTF_DST_IS_SINGLE_REGISTER);
  if(m_teletextDMAChannel == 0)
  {
    DERROR("Unable to allocate DMA Channel for teletext\n");
    return false;
  }

  DEXIT();
  return true;
}


void CSTmTVOutTeletext::FlushMetadata(stm_meta_data_type_t type)
{
  DENTRY();

  /*
   * We have to duplicate a little bit of the base class (hint think about
   * this a bit more) to safely stop the DMA processing without race conditions.
   */
  if(type != STM_METADATA_TYPE_EBU_TELETEXT)
    return;

  g_pIOS->LockResource(m_ulLock);
  m_bActive = false;
  g_pIOS->UnlockResource(m_ulLock);

  g_pIOS->StopDMAChannel(m_teletextDMAChannel);
  WriteTVOutReg(TTXT_CTRL,0);

  CSTmTeletext::FlushMetadata(type);
  DEXIT();
}


extern "C" {

void teletext_dma_cb(unsigned long param)
{
  CSTmTVOutTeletext *owner = (CSTmTVOutTeletext *)param;
  owner->DMACompleted();
}

}


void CSTmTVOutTeletext::DMACompleted(void)
{
  /*
   * This will be called within an FDMA interrupt handler context, but we
   * need to protect against the Vsync interrupt pre-empting this and call
   * UpdateHW. Note that this shouldn't happen as it would mean we were
   * processing one of the interrupts _very_ late, but we will do things
   * properly.
   */
  static struct stm_teletext_hw_node tmpNode;

  g_pIOS->LockResource(m_ulLock);

  tmpNode = m_CurrentNode;
  m_CurrentNode = m_PendingNode;
  m_PendingNode.metadataNode = 0;

  g_pIOS->UnlockResource(m_ulLock);

  DEBUGF2(DEBUG_DMA_LEVEL,(FENTRY "@ %p: Finished DMA handle %p @ %lld\n",__PRETTY_FUNCTION__, this, tmpNode.dmaHandle, m_pMaster->GetCurrentVSyncTime()));

  /*
   * We do not release the node we just processed with interrupts disabled
   * as it can be freeing memory which may take some time.
   */
  ReleaseHardwareNode(&tmpNode);

  if(!m_CurrentNode.metadataNode)
  {
    DEBUGF2(DEBUG_DMA_LEVEL,(FEXIT " @ %p: Disabling HW\n",__PRETTY_FUNCTION__, this));
    WriteTVOutReg(TTXT_CTRL,0);
    DisableDENC();
    return;
  }

  g_pIOS->StartDMATransfer(m_CurrentNode.dmaHandle);

  DEBUGF2(DEBUG_DMA_LEVEL,(FEXIT "@ %p: Next DMA handle %p\n",__PRETTY_FUNCTION__, this, m_CurrentNode.dmaHandle));
}


void CSTmTVOutTeletext::UpdateHW(void)
{
  if(!m_bActive)
    return;

  if(!m_pQueue[m_nReadIndex].metadataNode)
  {
    DEBUGF2(DEBUG_DMA_LEVEL,(FEXIT " @ %p: Queue Empty Becoming Inactive\n",__PRETTY_FUNCTION__, this));
    m_bActive = false;
    return;
  }

  stm_meta_data_t *m = (stm_meta_data_t *)m_pQueue[m_nReadIndex].metadataNode;
  LONG offset = m_pMaster->GetFieldOrFrameDuration() + (m_pMaster->GetFieldOrFrameDuration()/2);

  if((m->presentationTime != 0LL) &&
     (m->presentationTime > (m_pMaster->GetCurrentVSyncTime()+offset)))
  {
    return;
  }

  stm_field_t current_field = m_pMaster->GetCurrentVTGVSync();
  if((current_field == STM_TOP_FIELD    && m_pQueue[m_nReadIndex].isForTopField) ||
     (current_field == STM_BOTTOM_FIELD && !m_pQueue[m_nReadIndex].isForTopField))
  {
     return;
  }

  g_pIOS->LockResource(m_ulLock);

  if(m_PendingNode.metadataNode)
  {
    /*
     * Hmmmm, we haven't processed the last pending node yet, something
     * is late so we will just have to delay.
     */
    g_pIOS->UnlockResource(m_ulLock);
    return;
  }

  /*
   * The DENC and TVOut registers are double buffered with the Vsync so
   * we program them directly for the next field here.
   *
   * Note that we always send 48bytes, we are assured that the DENC will
   * ignore what it doesn't need for the configured Teletext standard.
   */
  ULONG ctrl = (sizeof(stm_ebu_teletext_line_t)  << TTXT_CTRL_BYTES_SHIFT) |
               (m_pQueue[m_nReadIndex].lineCount << TTXT_CTRL_LINES_SHIFT) |
               (m_nTVOutDelay                    << TTXT_CTRL_DELAY_SHIFT) |
               TTXT_CTRL_EN;

  WriteTVOutReg(TTXT_CTRL, ctrl);
  DEBUGF2(DEBUG_DMA_LEVEL,("%s: Enabled HW ctrl = 0x%lx @ %lld \n",__PRETTY_FUNCTION__, ReadTVOutReg(TTXT_CTRL), m_pMaster->GetCurrentVSyncTime()));

  ProgramDENC(&m_pQueue[m_nReadIndex]);

  if(m_CurrentNode.metadataNode)
  {
    /*
     * We have to wait to program the FDMA until the DMA completion callback
     * for this field's data has been called.
     */
    m_PendingNode = m_pQueue[m_nReadIndex];
  }
  else
  {
    m_CurrentNode = m_pQueue[m_nReadIndex];
    g_pIOS->StartDMATransfer(m_CurrentNode.dmaHandle);
    DEBUGF2(DEBUG_DMA_LEVEL,("%s: Next DMA handle %p\n",__PRETTY_FUNCTION__, m_CurrentNode.dmaHandle));
  }

  m_pQueue[m_nReadIndex].metadataNode = 0;
  m_nReadIndex++;
  if(m_nReadIndex == m_nQueueSize)
    m_nReadIndex = 0;

  g_pIOS->UnlockResource(m_ulLock);
}


stm_meta_data_result_t CSTmTVOutTeletext::ConstructTeletextNode(stm_meta_data_t *m, int index)
{
  stm_meta_data_result_t res = CSTmTeletext::ConstructTeletextNode(m,index);
  if(res != STM_METADATA_RES_OK)
    return res;

  DMA_transfer transfer[2];

  g_pIOS->ZeroMemory(transfer,sizeof(transfer));

  /*
   * Setup the DMA transfer.
   */
  transfer[0].src          = &m_HWData;
  transfer[0].src_offset   = TeletextDMABlockOffset(index);
  transfer[0].dst          = &m_teletextDataRegister;
  transfer[0].dst_offset   = 0;
  transfer[0].size         = sizeof(stm_ebu_teletext_line_t) * m_pQueue[index].lineCount;
  transfer[0].completed_cb = teletext_dma_cb;
  transfer[0].cb_param     = (ULONG)this;

  m_pQueue[index].dmaHandle = g_pIOS->CreateDMATransfer(m_teletextDMAChannel,
                                                        transfer,
                                                        m_pacingLine,
                                                        SDTF_DST_IS_SINGLE_REGISTER);
  if(!m_pQueue[index].dmaHandle)
    return STM_METADATA_RES_QUEUE_UNAVAILABLE;

  m_bActive = true;

  return STM_METADATA_RES_OK;
}


void CSTmTVOutTeletext::ReleaseHardwareNode(stm_teletext_hw_node *node)
{
  stm_meta_data_release((stm_meta_data_t *)node->metadataNode);
  node->metadataNode = 0;

  if(node->dmaHandle)
  {
    g_pIOS->DeleteDMATransfer(node->dmaHandle);
    node->dmaHandle = 0;
  }
}


void CSTmTVOutTeletext::StopDMAEngine(void)
{
  DENTRY();
  g_pIOS->StopDMAChannel(m_teletextDMAChannel);
  DEXIT();
}
