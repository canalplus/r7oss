/***********************************************************************
 *
 * File: display/generic/MetaDataQueue.cpp
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
CMetaDataQueue::CMetaDataQueue(stm_display_metadata_type_t type,
                               uint32_t                    uQSize,
                               int                         nSyncsEarly)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_type        = type;
  m_lock        = 0;
  m_uQSize      = uQSize;
  m_nSyncsEarly = nSyncsEarly;
  m_pParent     = 0;
  m_bIsBusy     = false;
  m_bIsEnabled  = false;
  m_uWriterPos = 0;
  m_uReaderPos = 0;

  vibe_os_zero_memory(&m_QueueArea,sizeof(DMA_Area));

  m_pQueue      = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CMetaDataQueue::~CMetaDataQueue(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  Stop();

  vibe_os_free_dma_area(&m_QueueArea);
  vibe_os_delete_resource_lock(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CMetaDataQueue::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_lock = vibe_os_create_resource_lock();
  if(!m_lock)
    return false;

  TRC( TRC_ID_MAIN_INFO, "queue size = %u",m_uQSize );

  vibe_os_allocate_dma_area(&m_QueueArea, (m_uQSize * sizeof(stm_display_metadata_t*)), 0, SDAAF_NONE);

  if(!m_QueueArea.pMemory)
  {
    TRC( TRC_ID_ERROR, "failed to allocate queue" );
    return false;
  }

  vibe_os_memset_dma_area(&m_QueueArea, 0, 0, m_QueueArea.ulDataSize);

  m_pQueue = (stm_display_metadata_s**)m_QueueArea.pData;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CMetaDataQueue::Start(COutput *parent)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pParent && m_pParent != parent)
    return false;

  m_pParent = parent;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CMetaDataQueue::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pParent)
  {
    Flush();

    m_pParent = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


stm_display_metadata_result_t CMetaDataQueue::Queue(stm_display_metadata_t *m)
{
  if(m->type != m_type)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Type mismatch" );
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
    int32_t offset = m_pParent->GetFieldOrFrameDuration()*m_nSyncsEarly;

    if((m->presentation_time != 0LL) &&
       ((m->presentation_time-offset) <= m_pParent->GetCurrentVTGEventTime()))
    {
      TRC( TRC_ID_UNCLASSIFIED, "Time in the past" );
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
    }
  }
  else
  {
    if((m->presentation_time != 0LL) &&
       (m->presentation_time <= vibe_os_get_system_time()))
    {
      TRC( TRC_ID_UNCLASSIFIED, "Time in the past" );
      return STM_METADATA_RES_TIMESTAMP_IN_PAST;
    }
  }

  TRCIN( TRC_ID_UNCLASSIFIED, "%p: type = %d m_uWriterPos = %u",this,(int)m_type,m_uWriterPos );

  /*
   * Unlike queuing buffers on planes, we might have multiple writers so we
   * do need to lock the insertion on the queue.
   */
  vibe_os_lock_resource(m_lock);

  if(UNLIKELY(m_bIsBusy || (m_pQueue[m_uWriterPos] != 0)))
  {
    vibe_os_unlock_resource(m_lock);
    TRC( TRC_ID_UNCLASSIFIED, "Queue busy or full" );
    return STM_METADATA_RES_QUEUE_BUSY;
  }

  stm_meta_data_addref(m);

  m_pQueue[m_uWriterPos++] = m;
  if(m_uWriterPos == m_uQSize)
    m_uWriterPos = 0;

  vibe_os_unlock_resource(m_lock);

  m_bIsEnabled = true;

  return STM_METADATA_RES_OK;
}


void CMetaDataQueue::Flush(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_lock_resource(m_lock);
  m_bIsBusy    = true;
  m_bIsEnabled = false;
  vibe_os_unlock_resource(m_lock);

  stm_display_metadata_t *tmp;
  while((tmp = m_pQueue[m_uReaderPos])!= 0)
  {
    m_pQueue[m_uReaderPos++] = 0;
    if(m_uReaderPos == m_uQSize)
      m_uReaderPos = 0;

    stm_meta_data_release(tmp);
  }

  m_bIsBusy = false;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


stm_display_metadata_t *CMetaDataQueue::Pop(void)
{
  stm_display_metadata_t *tmp;

  if(!m_pParent)
    return 0;

  int32_t offset = m_pParent->GetFieldOrFrameDuration()*m_nSyncsEarly + (m_pParent->GetFieldOrFrameDuration()/2);

  vibe_os_lock_resource(m_lock);

  if(!m_bIsEnabled || !(tmp = m_pQueue[m_uReaderPos]))
  {
    vibe_os_unlock_resource(m_lock);
    return 0;
  }

  if((tmp->presentation_time != 0LL) &&
     (tmp->presentation_time > (m_pParent->GetCurrentVTGEventTime()+offset)))
  {
    vibe_os_unlock_resource(m_lock);
    return 0;
  }

  TRCIN( TRC_ID_UNCLASSIFIED, "%p: type = %d m_uReaderPos = %u presentation time = %lld",this,(int)m_type,m_uReaderPos,tmp->presentation_time );

  m_pQueue[m_uReaderPos++] = 0;
  if(m_uReaderPos == m_uQSize)
    m_uReaderPos = 0;

  vibe_os_unlock_resource(m_lock);

  return tmp;
}

