/***********************************************************************
 *
 * File: Generic/MetaDataQueue.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include "IOS.h"
#include "IDebug.h"
#include "MetaDataQueue.h"
#include "Output.h"

/*
 * This provides a "timed" queue, where entries are only popped off the queue
 * before a requested time or immediately if the requested time is 0. The timing
 * is provided by the parent output's timing generator.
 *
 * The presentation timing can be adjusted on a per queue basis to be an integer
 * number of vsyncs early to allow for hardware programming that is only taken
 * into account on the next vsync.
 */
CMetaDataQueue::CMetaDataQueue(stm_meta_data_type_t type,
                               ULONG                ulQSize,
                               int                  nSyncsEarly)
{
  DENTRY();

  m_type        = type;
  m_lock        = 0;
  m_ulQSize     = ulQSize;
  m_nSyncsEarly = nSyncsEarly;
  m_pParent     = 0;
  m_bIsBusy     = false;
  m_bIsEnabled  = false;
  m_ulWriterPos = 0;
  m_ulReaderPos = 0;

  g_pIOS->ZeroMemory(&m_QueueArea,sizeof(DMA_Area));

  m_pQueue      = 0;

  DEXIT();
}


CMetaDataQueue::~CMetaDataQueue(void)
{
  DENTRY();

  Stop();

  g_pIOS->FreeDMAArea(&m_QueueArea);
  g_pIOS->DeleteResourceLock(m_lock);

  DEXIT();
}


bool CMetaDataQueue::Create(void)
{
  DENTRY();

  m_lock = g_pIOS->CreateResourceLock();
  if(!m_lock)
    return false;

  DEBUGF2(2,("%s: queue size = %lu \n",__PRETTY_FUNCTION__,m_ulQSize));

  g_pIOS->AllocateDMAArea(&m_QueueArea, (m_ulQSize * sizeof(stm_meta_data_t*)), 0, SDAAF_NONE);

  if(!m_QueueArea.pMemory)
  {
    DERROR("failed to allocate queue\n");
    return false;
  }

  g_pIOS->MemsetDMAArea(&m_QueueArea, 0, 0, m_QueueArea.ulDataSize);

  m_pQueue = (stm_meta_data_s**)m_QueueArea.pData;

  DEXIT();
  return true;
}


bool CMetaDataQueue::Start(COutput *parent)
{
  DENTRY();

  if(m_pParent && m_pParent != parent)
    return false;

  m_pParent = parent;

  DEXIT();
  return true;
}


void CMetaDataQueue::Stop(void)
{
  DENTRY();

  Flush();

  m_pParent = 0;

  DEXIT();
}


stm_meta_data_result_t CMetaDataQueue::Queue(stm_meta_data_t *m)
{
  if(m->type != m_type)
  {
    DERROR("Type mismatch\n");
    return STM_METADATA_RES_UNSUPPORTED_TYPE;
  }

  /*
   * We want to try and tell the caller if they are too late to queue the
   * data, which may be time sensitive. But we may also want to be able to queue
   * before we have the output fully running, so that data can be
   * processed in the first vsync. In which case we cannot be quite as accurate
   * in the test.
   */
  if(m_pParent && (m_pParent->GetCurrentDisplayMode() != 0))
  {
    LONG offset = m_pParent->GetFieldOrFrameDuration()*m_nSyncsEarly;

    if((m->presentationTime != 0LL) &&
       ((m->presentationTime-offset) <= m_pParent->GetCurrentVSyncTime()))
    {
      DERROR("Time in the past\n");
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
    }
  }
  else
  {
    if((m->presentationTime != 0LL) &&
       (m->presentationTime <= g_pIOS->GetSystemTime()))
    {
      DERROR("Time in the past\n");
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
    }
  }

  DEBUGF2(3,(FENTRY "@ %p: type = %d m_ulWriterPos = %lu \n",__PRETTY_FUNCTION__,this,(int)m_type,m_ulWriterPos));

  /*
   * Unlike queuing buffers on planes, we might have multiple writers so we
   * do need to lock the insertion on the queue.
   */
  g_pIOS->LockResource(m_lock);

  if(UNLIKELY(m_bIsBusy || (m_pQueue[m_ulWriterPos] != 0)))
  {
    g_pIOS->UnlockResource(m_lock);
    DERROR("Queue busy or full\n");
    return STM_METADATA_RES_QUEUE_BUSY;
  }

  stm_meta_data_addref(m);

  m_pQueue[m_ulWriterPos++] = m;
  if(m_ulWriterPos == m_ulQSize)
    m_ulWriterPos = 0;

  g_pIOS->UnlockResource(m_lock);

  m_bIsEnabled = true;

  DEXIT();
  return STM_METADATA_RES_OK;
}


void CMetaDataQueue::Flush(void)
{
  DENTRY();

  g_pIOS->LockResource(m_lock);
  m_bIsBusy    = true;
  m_bIsEnabled = false;
  g_pIOS->UnlockResource(m_lock);

  stm_meta_data_t *tmp;
  while((tmp = m_pQueue[m_ulReaderPos])!= 0)
  {
    m_pQueue[m_ulReaderPos++] = 0;
    if(m_ulReaderPos == m_ulQSize)
      m_ulReaderPos = 0;

    stm_meta_data_release(tmp);
  }

  m_bIsBusy = false;
  DEXIT();
}


stm_meta_data_t *CMetaDataQueue::Pop(void)
{
  stm_meta_data_t *tmp;

  if(!m_pParent)
    return 0;

  LONG offset = m_pParent->GetFieldOrFrameDuration()*m_nSyncsEarly + (m_pParent->GetFieldOrFrameDuration()/2);

  g_pIOS->LockResource(m_lock);

  if(!m_bIsEnabled || !(tmp = m_pQueue[m_ulReaderPos]))
  {
    g_pIOS->UnlockResource(m_lock);
    return 0;
  }

  if((tmp->presentationTime != 0LL) &&
     (tmp->presentationTime > (m_pParent->GetCurrentVSyncTime()+offset)))
  {
    g_pIOS->UnlockResource(m_lock);
    return 0;
  }

  DEBUGF2(3,(FENTRY "@ %p: type = %d m_ulReaderPos = %lu presentation time = %lld\n",__PRETTY_FUNCTION__,this,(int)m_type,m_ulReaderPos,tmp->presentationTime));

  m_pQueue[m_ulReaderPos++] = 0;
  if(m_ulReaderPos == m_ulQSize)
    m_ulReaderPos = 0;

  g_pIOS->UnlockResource(m_lock);

  return tmp;
}

