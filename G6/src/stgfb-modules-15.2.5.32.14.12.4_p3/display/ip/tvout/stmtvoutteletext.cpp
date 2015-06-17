/***********************************************************************
 *
 * File: display/ip/tvout/stmtvoutteletext.cpp
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

#include "stmtvoutteletext.h"

#define TTXT_CTRL 0x0
#define TTXT_DATA 0x4

#define TTXT_CTRL_EN (1L<<0)
#define TTXT_CTRL_DELAY_SHIFT 1
#define TTXT_CTRL_DELAY_MASK  (0xf << TTXT_CTRL_DELAY_SHIFT)
#define TTXT_CTRL_LINES_SHIFT 5
#define TTXT_CTRL_LINES_MASK  (0x1ff << TTXT_CTRL_LINES_SHIFT)
#define TTXT_CTRL_BYTES_SHIFT 14
#define TTXT_CTRL_BYTES_MASK  (0x3f << TTXT_CTRL_BYTES_SHIFT)


CSTmTVOutTeletext::CSTmTVOutTeletext(CDisplayDevice* pDev,
                                     uint32_t        uRegDENCOffset,
                                     uint32_t        uRegTXTOffset,
                                     uint32_t        uPhysRegisterBase,
                                     STM_DMA_TRANSFER_PACING pacing): CSTmTeletext(pDev,uRegDENCOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_uTXTOffset         = uRegTXTOffset;
  m_pacingLine         = pacing;
  m_teletextDMAChannel = 0;
  m_nTVOutDelay        = 2; /* Design default, 2 clocks */

  /*
   * Create a dummy DMA area for the Teletext data register.
   */
  vibe_os_zero_memory(&m_teletextDataRegister,sizeof(m_teletextDataRegister));
  m_teletextDataRegister.ulPhysical = uPhysRegisterBase+m_uTXTOffset+TTXT_DATA;
  m_teletextDataRegister.ulDataSize = 4;
  m_teletextDataRegister.ulFlags    = SDAAF_UNCACHED;

  vibe_os_zero_memory(&m_CompletedNode,sizeof(stm_teletext_hw_node));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmTVOutTeletext::~CSTmTVOutTeletext(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_teletextDMAChannel != 0)
    vibe_os_release_dma_channel(m_teletextDMAChannel);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmTVOutTeletext::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pacingLine != SDTP_DENC1_TELETEXT && m_pacingLine != SDTP_DENC2_TELETEXT)
    return false;

  if(!CSTmTeletext::Create())
    return false;

  m_teletextDMAChannel = vibe_os_get_dma_channel(m_pacingLine, 48, SDTF_DST_IS_SINGLE_REGISTER);
  if(m_teletextDMAChannel == 0)
  {
    TRC( TRC_ID_ERROR, "Unable to allocate DMA Channel for teletext" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


extern "C" {

void teletext_dma_cb(void *param)
{
  CSTmTVOutTeletext *owner = (CSTmTVOutTeletext *)param;
  owner->DMACompleted();
}

}


void CSTmTVOutTeletext::DMACompleted(void)
{
  /*
   * This will be called within an FDMA interrupt handler context, but we
   * need to protect against the Vsync interrupt pre-empting this and calling
   * UpdateHW. Note that this shouldn't happen as it would mean we were
   * processing one of the interrupts _very_ late, but we will do things
   * properly.
   */
  vibe_os_lock_resource(m_Lock);

  m_CompletedNode = m_CurrentNode;
  m_CurrentNode = m_PendingNode;
  m_PendingNode.metadataNode = 0;

  vibe_os_unlock_resource(m_Lock);

  TRC( TRC_ID_UNCLASSIFIED, "%p: Finished DMA handle %p @ %lld", this, m_CompletedNode.dmaHandle, m_pMaster->GetCurrentVTGEventTime() );

  /*
   * We do not release the node we just processed with interrupts disabled
   * as it can be freeing memory which may take some time.
   */
  ReleaseHardwareNode(&m_CompletedNode);

  if(m_CurrentNode.metadataNode)
  {
    vibe_os_start_dma_transfer(m_CurrentNode.dmaHandle);
    TRC( TRC_ID_UNCLASSIFIED, "%p: Next DMA handle %p", this, m_CurrentNode.dmaHandle );
  }
}


void CSTmTVOutTeletext::UpdateHW(void)
{
  stm_display_metadata_t *m;
  int32_t  offset;
  uint32_t timingevent;
  uint32_t ctrl;

  vibe_os_lock_resource(m_Lock);
  if(!m_bActive)
    goto unlock_and_exit;

  if(!m_pQueue[m_nReadIndex].metadataNode)
  {
    TRC( TRC_ID_UNCLASSIFIED, "%p: Queue Empty Becoming Inactive", this );
    m_bActive = false;
    goto unlock_and_exit;
  }

  m = (stm_display_metadata_t *)m_pQueue[m_nReadIndex].metadataNode;
  offset = m_pMaster->GetFieldOrFrameDuration() + (m_pMaster->GetFieldOrFrameDuration()/2);

  if((m->presentation_time != 0LL) &&
     (m->presentation_time > (m_pMaster->GetCurrentVTGEventTime()+offset)))
  {
    goto unlock_and_exit;
  }

  timingevent = m_pMaster->GetCurrentVTGEvent();
  if(((timingevent & STM_TIMING_EVENT_TOP_FIELD)    && m_pQueue[m_nReadIndex].isForTopField) ||
     ((timingevent & STM_TIMING_EVENT_BOTTOM_FIELD) && !m_pQueue[m_nReadIndex].isForTopField))
  {
    goto unlock_and_exit;
  }

  if(m_PendingNode.metadataNode)
  {
    /*
     * Hmmmm, we haven't processed the last pending node yet, something
     * is late so we will just have to delay.
     */
    goto unlock_and_exit;
  }

  /*
   * The DENC and TVOut registers are double buffered with the Vsync so
   * we program them directly for the next field here.
   *
   * Note that we always send 48bytes, we are assured that the DENC will
   * ignore what it doesn't need for the configured Teletext standard.
   */
  ctrl = (sizeof(stm_teletext_line_t)      << TTXT_CTRL_BYTES_SHIFT) |
         (m_pQueue[m_nReadIndex].lineCount << TTXT_CTRL_LINES_SHIFT) |
         (m_nTVOutDelay                    << TTXT_CTRL_DELAY_SHIFT) |
         TTXT_CTRL_EN;

  WriteTXTReg(TTXT_CTRL, ctrl);
  TRC( TRC_ID_UNCLASSIFIED, "Enabled HW ctrl = 0x%x @ %lld", ReadTXTReg(TTXT_CTRL), m_pMaster->GetCurrentVTGEventTime() );

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
    vibe_os_start_dma_transfer(m_CurrentNode.dmaHandle);
    TRC( TRC_ID_UNCLASSIFIED, "Next DMA handle %p", m_CurrentNode.dmaHandle );
  }

  m_pQueue[m_nReadIndex].metadataNode = 0;
  m_nReadIndex++;
  if(m_nReadIndex == m_nQueueSize)
    m_nReadIndex = 0;

unlock_and_exit:
  vibe_os_unlock_resource(m_Lock);
}


stm_display_metadata_result_t CSTmTVOutTeletext::ConstructTeletextNode(stm_display_metadata_t *m, int index)
{
  stm_display_metadata_result_t res = CSTmTeletext::ConstructTeletextNode(m,index);
  if(res != STM_METADATA_RES_OK)
    return res;

  DMA_transfer transfer[2]; // The second zeroed node terminates the transfer list

  vibe_os_zero_memory(transfer,sizeof(transfer));

  /*
   * Setup the DMA transfer.
   */
  transfer[0].src          = &m_HWData;
  transfer[0].src_offset   = TeletextDMABlockOffset(index);
  transfer[0].dst          = &m_teletextDataRegister;
  transfer[0].dst_offset   = 0;
  transfer[0].size         = sizeof(stm_teletext_line_t) * m_pQueue[index].lineCount;
  transfer[0].completed_cb = teletext_dma_cb;
  transfer[0].cb_param     = this;

  m_pQueue[index].dmaHandle = vibe_os_create_dma_transfer(m_teletextDMAChannel,
                                                        transfer,
                                                        m_pacingLine,
                                                        SDTF_DST_IS_SINGLE_REGISTER);
  if(!m_pQueue[index].dmaHandle)
  {
    TRC( TRC_ID_UNCLASSIFIED, "CSTmTVOutTeletext::ConstructTeletextNode: Unable to create DMA transfer" );
    return STM_METADATA_RES_QUEUE_UNAVAILABLE;
  }

  m_bActive = true;

  return STM_METADATA_RES_OK;
}


void CSTmTVOutTeletext::ReleaseHardwareNode(stm_teletext_hw_node *node)
{
  stm_meta_data_release((stm_display_metadata_t *)node->metadataNode);
  node->metadataNode = 0;

  if(node->dmaHandle)
  {
    vibe_os_delete_dma_transfer(node->dmaHandle);
    node->dmaHandle = 0;
  }
}


void CSTmTVOutTeletext::StopDMAEngine(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  vibe_os_stop_dma_channel(m_teletextDMAChannel);
  WriteTXTReg(TTXT_CTRL,0);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
