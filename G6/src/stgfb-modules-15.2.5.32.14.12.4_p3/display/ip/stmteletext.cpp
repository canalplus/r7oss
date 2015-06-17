/***********************************************************************
 *
 * File: display/ip/stmteletext.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
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

#include "stmdencregs.h"
#include "stmteletext.h"

CSTmTeletext::CSTmTeletext(CDisplayDevice* pDev, uint32_t uDencOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pMaster        = 0;
  m_pDevRegs       = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_pDENCReg       = ((uint32_t*)pDev->GetCtrlRegisterBase()) + (uDencOffset>>2);
  m_uLineSize      = 0;
  m_Lock           = 0;
  m_uDENCDelay     = 0x2;
  m_bActive        = false;

  vibe_os_zero_memory(&m_HWData,sizeof(m_HWData));
  m_pQueue = 0;

  m_nQueueSize     = 128;
  m_nWriteIndex    = 0;
  m_nReadIndex     = 0;
  m_LastQueuedTime = (stm_time64_t)0;

  vibe_os_zero_memory(&m_CurrentNode,sizeof(m_CurrentNode));
  vibe_os_zero_memory(&m_PendingNode,sizeof(m_PendingNode));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmTeletext::~CSTmTeletext(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  vibe_os_delete_resource_lock(m_Lock);
  vibe_os_free_dma_area(&m_HWData);
  delete[] m_pQueue;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmTeletext::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((m_Lock = vibe_os_create_resource_lock()) == 0)
    return false;

  vibe_os_allocate_dma_area(&m_HWData,TeletextDMABlockSize()*m_nQueueSize,0,SDAAF_NONE);
  if(m_HWData.pMemory == 0)
  {
    TRC( TRC_ID_ERROR, "Unable to create Teletext HW data pool" );
    return false;
  }

  vibe_os_memset_dma_area(&m_HWData,0,0,m_HWData.ulDataSize);

  m_pQueue = new struct stm_teletext_hw_node[m_nQueueSize];
  if(m_pQueue == 0)
  {
    TRC( TRC_ID_ERROR, "Unable to allocate Teletext node queue" );
    return false;
  }
  // Initialize table of stm_teletext_hw_node structure to 0.
  vibe_os_zero_memory(m_pQueue, m_nQueueSize * sizeof(struct stm_teletext_hw_node));

  /*
   * Point each of the queue elements data pointer to a slot in the DMAArea
   */
  for(int i=0;i<m_nQueueSize;i++)
  {
    m_pQueue[i].metadataNode = 0;
    m_pQueue[i].dmaHandle = 0;
    m_pQueue[i].data = (stm_teletext_line_t*)(m_HWData.pData + TeletextDMABlockOffset(i));
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmTeletext::Start(uint32_t LineSize, COutput *master)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_uLineSize = LineSize;
  m_pMaster    = master;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return false;
}


void CSTmTeletext::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  FlushMetadata(STM_METADATA_TYPE_TELETEXT);
  m_pMaster = 0;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


stm_display_metadata_result_t CSTmTeletext::QueueMetadata(stm_display_metadata_t *m)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if(m->type != STM_METADATA_TYPE_TELETEXT)
    return STM_METADATA_RES_UNSUPPORTED_TYPE;

  if(m->presentation_time != (stm_time64_t)0)
  {
    if(m->presentation_time < (m_pMaster->GetCurrentVTGEventTime()+m_pMaster->GetFieldOrFrameDuration()))
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;

    if((m_LastQueuedTime != (stm_time64_t)0) && (m->presentation_time < m_LastQueuedTime))
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
  }

  /*
   * We do not have to lock this as mutual exclusion at the external API level
   * guarantees we can only have one writer at a time.
   */
  if(m_pQueue[m_nWriteIndex].metadataNode != 0)
    return STM_METADATA_RES_QUEUE_BUSY;


  stm_display_metadata_result_t res = this->ConstructTeletextNode(m,m_nWriteIndex);

  if(res == STM_METADATA_RES_OK)
  {
    if(m->presentation_time != (stm_time64_t)0)
      m_LastQueuedTime = m->presentation_time;

    /*
     * Make sure the data is available to the DMA engine.
     */
    vibe_os_flush_dma_area(&m_HWData,
                         TeletextDMABlockOffset(m_nWriteIndex),
                         sizeof(stm_teletext_line_t)*m_pQueue[m_nWriteIndex].lineCount);

    /*
     * Make node available for de-queuing
     */
    stm_meta_data_addref(m);
    m_pQueue[m_nWriteIndex].metadataNode = m;

    m_nWriteIndex++;
    if(m_nWriteIndex == m_nQueueSize)
      m_nWriteIndex = 0;

    m_bActive = true;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


void CSTmTeletext::FlushMetadata(stm_display_metadata_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(type != STM_METADATA_TYPE_TELETEXT)
    return;

  vibe_os_lock_resource(m_Lock);
  m_bActive = false;
  DisableDENC();
  vibe_os_unlock_resource(m_Lock);

  this->StopDMAEngine();

  while(m_pQueue[m_nReadIndex].metadataNode != 0)
  {
    this->ReleaseHardwareNode(&m_pQueue[m_nReadIndex]);

    m_nReadIndex++;
    if(m_nReadIndex == m_nQueueSize)
      m_nReadIndex = 0;
  }

  if(m_PendingNode.metadataNode)
    this->ReleaseHardwareNode(&m_PendingNode);

  if(m_CurrentNode.metadataNode)
    this->ReleaseHardwareNode(&m_CurrentNode);

  m_LastQueuedTime = (stm_time64_t)0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmTeletext::ProgramDENC(stm_teletext_hw_node *node)
{
  /*
   * Only update the register bits for the _next_ field as these registers are
   * not double buffered on vsync, we must not disturb the configuration for
   * the current field. Note the third register has bits for both fields!
   */
  if(node->isForTopField)
  {
    WriteDENCReg(DENC_TTXL1, node->ttxLineDENCRegs[0] | (m_uDENCDelay << DENC_TTXL1_DEL_SHIFT));
    WriteDENCReg(DENC_TTXL2, node->ttxLineDENCRegs[1]);
    WriteDENCReg(DENC_TTXL3, (ReadDENCReg(DENC_TTXL3) & 0x3) | (node->ttxLineDENCRegs[2] & ~0x3));
  }
  else
  {
    WriteDENCReg(DENC_TTXL3, (ReadDENCReg(DENC_TTXL3) & ~0x3) | (node->ttxLineDENCRegs[2] & 0x3));
    WriteDENCReg(DENC_TTXL4, node->ttxLineDENCRegs[3]);
    WriteDENCReg(DENC_TTXL5, node->ttxLineDENCRegs[4]);
  }

  uint32_t val = ReadDENCReg(DENC_CFG6) | DENC_CFG6_TTX_ENA;
  WriteDENCReg(DENC_CFG6, val);

  TRC( TRC_ID_UNCLASSIFIED, "DENC_CFG0  = 0x%x", (int)ReadDENCReg(DENC_CFG0) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_CFG6  = 0x%x", (int)ReadDENCReg(DENC_CFG6) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_TTXL1 = 0x%x", (int)ReadDENCReg(DENC_TTXL1) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_TTXL2 = 0x%x", (int)ReadDENCReg(DENC_TTXL2) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_TTXL3 = 0x%x", (int)ReadDENCReg(DENC_TTXL3) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_TTXL4 = 0x%x", (int)ReadDENCReg(DENC_TTXL4) );
  TRC( TRC_ID_UNCLASSIFIED, "DENC_TTXL5 = 0x%x", (int)ReadDENCReg(DENC_TTXL5) );
}


void CSTmTeletext::DisableDENC(void)
{
  uint32_t val = ReadDENCReg(DENC_CFG6) & ~DENC_CFG6_TTX_ENA;
  WriteDENCReg(DENC_CFG6, val);

  WriteDENCReg(DENC_TTXL1, (m_uDENCDelay << DENC_TTXL1_DEL_SHIFT));
  WriteDENCReg(DENC_TTXL2, 0);
  WriteDENCReg(DENC_TTXL3, 0);
  WriteDENCReg(DENC_TTXL4, 0);
  WriteDENCReg(DENC_TTXL5, 0);
}


stm_display_metadata_result_t CSTmTeletext::ConstructTeletextNode(stm_display_metadata_t *m, int index)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  stm_teletext_t *t = (stm_teletext_t*)&m->data[0];

  for(int i=0;i<5;i++)
  {
    m_pQueue[index].ttxLineDENCRegs[i] = 0;
  }

  /*
   * Decode the field parity, odd = top field
   */
  m_pQueue[index].isForTopField = (t->valid_line_mask & 0x1)?false:true;

  int   currentDENCLineRegisterIndex;
  uint8_t currentDENCLineEnableBit;

  if(m_pQueue[index].isForTopField)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Top field" );
    currentDENCLineRegisterIndex = 0;
    currentDENCLineEnableBit = (1L<<3); /* bit for line 6 top field */
  }
  else
  {
    TRC( TRC_ID_UNCLASSIFIED, "Bottom field" );
    currentDENCLineRegisterIndex = 2;
    currentDENCLineEnableBit = (1L<<1); /* bit for line 318 bottom field */
  }

  /*
   * We need to copy the valid lines in the metadata structure into a
   * contiguous buffer for the hardware to read. We set the hardware line enable
   * registers in the process.
   */
  stm_teletext_line_t *dmaBuffer = m_pQueue[index].data;
  m_pQueue[index].lineCount = 0;
  for(int teletextLine = 6;teletextLine < 24;teletextLine++)
  {
    if(t->valid_line_mask & (1L<<teletextLine))
    {
      /*
       * Use intrinsic copy for the data, the cache will be flushed later
       */
      *dmaBuffer = t->lines[teletextLine-6];

      TRC( TRC_ID_UNCLASSIFIED, "Line%d %x %x ...", teletextLine, ((uint32_t *)dmaBuffer)[0], ((uint32_t *)dmaBuffer)[1] );

      dmaBuffer++;

      m_pQueue[index].ttxLineDENCRegs[currentDENCLineRegisterIndex] |= currentDENCLineEnableBit;
      m_pQueue[index].lineCount++;
    }

    currentDENCLineEnableBit >>= 1;
    if(currentDENCLineEnableBit == 0)
    {
      currentDENCLineEnableBit = (1L<<7);
      currentDENCLineRegisterIndex++;
    }

  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return STM_METADATA_RES_OK;
}
