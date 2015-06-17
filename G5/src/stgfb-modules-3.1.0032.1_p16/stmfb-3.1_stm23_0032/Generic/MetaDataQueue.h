/***********************************************************************
 *
 * File: Generic/MetaDataQueue.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _METADATA_QUEUE_H
#define _METADATA_QUEUE_H

class COutput;

class CMetaDataQueue
{
public:
  CMetaDataQueue(stm_meta_data_type_t type, ULONG ulQSize, int nSyncsEarly);
  virtual ~CMetaDataQueue(void);

  bool Create(void);

  bool Start(COutput *);
  void Stop(void);

  stm_meta_data_result_t Queue(stm_meta_data_t *);
  stm_meta_data_t       *Pop(void);
  void Flush(void);


private:
  stm_meta_data_type_t m_type;

  ULONG    m_lock;
  ULONG    m_ulQSize;
  int      m_nSyncsEarly;
  COutput *m_pParent;

  volatile bool m_bIsBusy;
  volatile bool m_bIsEnabled;

  ULONG    m_ulWriterPos;
  ULONG    m_ulReaderPos;

  DMA_Area m_QueueArea;
  stm_meta_data_t **m_pQueue;
};

#endif /* _METADATA_QUEUE_H */
