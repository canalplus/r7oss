/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2008-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef _METADATA_QUEUE_H
#define _METADATA_QUEUE_H

class COutput;

/*! \enum stm_display_metadata_result_t
 *  \brief Result values returned when attempting to queue a metadata packet
 */
typedef enum
{
  STM_METADATA_RES_OK,                /*!< */
  STM_METADATA_RES_UNSUPPORTED_TYPE,  /*!< */
  STM_METADATA_RES_TIMESTAMP_IN_PAST, /*!< */
  STM_METADATA_RES_INVALID_DATA,      /*!< */
  STM_METADATA_RES_QUEUE_BUSY,        /*!< */
  STM_METADATA_RES_QUEUE_UNAVAILABLE  /*!< */
} stm_display_metadata_result_t;


class CMetaDataQueue
{
public:
  CMetaDataQueue(stm_display_metadata_type_t type, uint32_t uQSize, int nSyncsEarly);
  virtual ~CMetaDataQueue(void);

  bool Create(void);

  bool Start(COutput *);
  void Stop(void);

  stm_display_metadata_result_t Queue(stm_display_metadata_t *);
  stm_display_metadata_t       *Pop(void);
  void Flush(void);


private:
  stm_display_metadata_type_t m_type;

  void     * m_lock;
  uint32_t   m_uQSize;
  int        m_nSyncsEarly;
  COutput  * m_pParent;

  volatile bool m_bIsBusy;
  volatile bool m_bIsEnabled;

  uint32_t   m_uWriterPos;
  uint32_t   m_uReaderPos;

  DMA_Area   m_QueueArea;
  stm_display_metadata_t ** m_pQueue;
};

#endif /* _METADATA_QUEUE_H */
