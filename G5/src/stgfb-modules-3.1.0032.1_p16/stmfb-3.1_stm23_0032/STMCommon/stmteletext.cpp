/***********************************************************************
 *
 * File: STMCommon/stmteletext.cpp
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

#include "stmdencregs.h"
#include "stmteletext.h"

CSTmTeletext::CSTmTeletext(CDisplayDevice* pDev, ULONG ulDencOffset)
{
  DENTRY();

  m_pMaster        = 0;
  m_pDevRegs       = (ULONG*)pDev->GetCtrlRegisterBase();
  m_pDENCReg       = ((ULONG*)pDev->GetCtrlRegisterBase()) + (ulDencOffset>>2);
  m_ulLineSize     = 0;
  m_ulLock         = 0;
  m_ulDENCDelay    = 0x2;
  m_bActive        = false;

  g_pIOS->ZeroMemory(&m_HWData,sizeof(m_HWData));
  m_pQueue = 0;

  m_nQueueSize     = 128;
  m_nWriteIndex    = 0;
  m_nReadIndex     = 0;
  m_LastQueuedTime = (TIME64)0;

  g_pIOS->ZeroMemory(&m_CurrentNode,sizeof(m_CurrentNode));
  g_pIOS->ZeroMemory(&m_PendingNode,sizeof(m_PendingNode));

  DEXIT();
}


CSTmTeletext::~CSTmTeletext(void)
{
  DENTRY();
  g_pIOS->DeleteResourceLock(m_ulLock);
  g_pIOS->FreeDMAArea(&m_HWData);
  g_pIOS->FreeMemory(m_pQueue);
  DEXIT();
}


bool CSTmTeletext::Create()
{
  DENTRY();

  if((m_ulLock = g_pIOS->CreateResourceLock()) == 0)
    return false;

  g_pIOS->AllocateDMAArea(&m_HWData,TeletextDMABlockSize()*m_nQueueSize,0,SDAAF_NONE);
  if(m_HWData.pMemory == 0)
  {
    DERROR("Unable to create Teletext HW data pool\n");
    return false;
  }

  g_pIOS->MemsetDMAArea(&m_HWData,0,0,m_HWData.ulDataSize);

  m_pQueue = (stm_teletext_hw_node*)g_pIOS->AllocateMemory(sizeof(stm_teletext_hw_node)*m_nQueueSize);
  if(m_pQueue == 0)
  {
    DERROR("Unable to allocate Teletext node queue");
    return false;
  }

  /*
   * Point each of the queue elements data pointer to a slot in the DMAArea
   */
  for(int i=0;i<m_nQueueSize;i++)
  {
    m_pQueue[i].metadataNode = 0;
    m_pQueue[i].dmaHandle = 0;
    m_pQueue[i].data = (stm_ebu_teletext_line_t*)(m_HWData.pData + TeletextDMABlockOffset(i));
  }

  DEXIT();
  return true;
}


bool CSTmTeletext::Start(ULONG ulLineSize, COutput *master)
{
  DENTRY();

  m_ulLineSize = ulLineSize;
  m_pMaster    = master;

  DEXIT();

  return false;
}


void CSTmTeletext::Stop(void)
{
  DENTRY();
  FlushMetadata(STM_METADATA_TYPE_EBU_TELETEXT);
  m_pMaster = 0;
  DEXIT();
}


stm_meta_data_result_t CSTmTeletext::QueueMetadata(stm_meta_data_t *m)
{
  DENTRY();
  if(m->type != STM_METADATA_TYPE_EBU_TELETEXT)
    return STM_METADATA_RES_UNSUPPORTED_TYPE;

  if(m->presentationTime != (TIME64)0)
  {
    if(m->presentationTime < (m_pMaster->GetCurrentVSyncTime()+m_pMaster->GetFieldOrFrameDuration()))
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;

    if((m_LastQueuedTime != (TIME64)0) && (m->presentationTime < m_LastQueuedTime))
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
  }

  /*
   * We do not have to lock this as mutual exclusion at the external API level
   * guarantees we can only have one writer at a time.
   */
  if(m_pQueue[m_nWriteIndex].metadataNode != 0)
    return STM_METADATA_RES_QUEUE_BUSY;


  stm_meta_data_result_t res = this->ConstructTeletextNode(m,m_nWriteIndex);

  if(res == STM_METADATA_RES_OK)
  {
    if(m->presentationTime != (TIME64)0)
      m_LastQueuedTime = m->presentationTime;

    /*
     * Make sure the data is available to the DMA engine.
     */
    g_pIOS->FlushCache(m_pQueue[m_nWriteIndex].data,
                       sizeof(stm_ebu_teletext_line_t)*m_pQueue[m_nWriteIndex].lineCount);

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

  DEXIT();
  return res;
}


void CSTmTeletext::FlushMetadata(stm_meta_data_type_t type)
{
  DENTRY();

  if(type != STM_METADATA_TYPE_EBU_TELETEXT)
    return;

  g_pIOS->LockResource(m_ulLock);
  m_bActive = false;
  DisableDENC();
  g_pIOS->UnlockResource(m_ulLock);

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

  m_LastQueuedTime = (TIME64)0;

  DEXIT();
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
    WriteDENCReg(DENC_TTXL1, node->ttxLineDENCRegs[0] | (m_ulDENCDelay << DENC_TTXL1_DEL_SHIFT));
    WriteDENCReg(DENC_TTXL2, node->ttxLineDENCRegs[1]);
    WriteDENCReg(DENC_TTXL3, (ReadDENCReg(DENC_TTXL3) & 0x3) | (node->ttxLineDENCRegs[2] & ~0x3));
  }
  else
  {
    WriteDENCReg(DENC_TTXL3, (ReadDENCReg(DENC_TTXL3) & ~0x3) | (node->ttxLineDENCRegs[2] & 0x3));
    WriteDENCReg(DENC_TTXL4, node->ttxLineDENCRegs[3]);
    WriteDENCReg(DENC_TTXL5, node->ttxLineDENCRegs[4]);
  }

  ULONG val = ReadDENCReg(DENC_CFG6) | DENC_CFG6_TTX_ENA;
  WriteDENCReg(DENC_CFG6, val);

  DEBUGF2(3,("%s: DENC_CFG0  = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_CFG0)));
  DEBUGF2(3,("%s: DENC_CFG6  = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_CFG6)));
  DEBUGF2(3,("%s: DENC_TTXL1 = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_TTXL1)));
  DEBUGF2(3,("%s: DENC_TTXL2 = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_TTXL2)));
  DEBUGF2(3,("%s: DENC_TTXL3 = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_TTXL3)));
  DEBUGF2(3,("%s: DENC_TTXL4 = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_TTXL4)));
  DEBUGF2(3,("%s: DENC_TTXL5 = 0x%x\n", __PRETTY_FUNCTION__, (int)ReadDENCReg(DENC_TTXL5)));
}


void CSTmTeletext::DisableDENC(void)
{
  ULONG val = ReadDENCReg(DENC_CFG6) & ~DENC_CFG6_TTX_ENA;
  WriteDENCReg(DENC_CFG6, val);

  WriteDENCReg(DENC_TTXL1, (m_ulDENCDelay << DENC_TTXL1_DEL_SHIFT));
  WriteDENCReg(DENC_TTXL2, 0);
  WriteDENCReg(DENC_TTXL3, 0);
  WriteDENCReg(DENC_TTXL4, 0);
  WriteDENCReg(DENC_TTXL5, 0);
}


stm_meta_data_result_t CSTmTeletext::ConstructTeletextNode(stm_meta_data_t *m, int index)
{
  DENTRY();

  stm_ebu_teletext_t *t = (stm_ebu_teletext_t*)&m->data[0];

  for(int i=0;i<5;i++)
  {
    m_pQueue[index].ttxLineDENCRegs[i] = 0;
  }

  /*
   * Decode the field parity, odd = top field
   */
  m_pQueue[index].isForTopField = (t->ulValidLineMask & 0x1)?false:true;

  int   currentDENCLineRegisterIndex;
  UCHAR currentDENCLineEnableBit;

  if(m_pQueue[index].isForTopField)
  {
    DEBUGF2(3,("%s: Top field\n", __PRETTY_FUNCTION__));
    currentDENCLineRegisterIndex = 0;
    currentDENCLineEnableBit = (1L<<3); /* bit for line 6 top field */
  }
  else
  {
    DEBUGF2(3,("%s: Bottom field\n", __PRETTY_FUNCTION__));
    currentDENCLineRegisterIndex = 2;
    currentDENCLineEnableBit = (1L<<1); /* bit for line 318 bottom field */
  }

  /*
   * We need to copy the valid lines in the metadata structure into a
   * contiguous buffer for the hardware to read. We set the hardware line enable
   * registers in the process.
   */
  stm_ebu_teletext_line_t *dmaBuffer = m_pQueue[index].data;
  m_pQueue[index].lineCount = 0;
  for(int teletextLine = 6;teletextLine < 24;teletextLine++)
  {
    if(t->ulValidLineMask & (1L<<teletextLine))
    {
      /*
       * Use intrinsic copy for the data, the cache will be flushed later
       */
      *dmaBuffer = t->lines[teletextLine-6];

      DEBUGF2(3,("%s: Line%d %lx %lx ...\n", __PRETTY_FUNCTION__,
                                             teletextLine,
                                             ((ULONG *)dmaBuffer)[0],
                                             ((ULONG *)dmaBuffer)[1]));

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

  DEXIT();
  return STM_METADATA_RES_OK;
}
