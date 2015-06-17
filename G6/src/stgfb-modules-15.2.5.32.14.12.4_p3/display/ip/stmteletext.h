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

#ifndef _STMTELETEXT_H
#define _STMTELETEXT_H

#include <display/generic/MetaDataQueue.h>

/*
 * Teletext processing base class, designed to support both the old TeletextDMA
 * and new FDMA based data transfer.
 */
class CDisplayDevice;
class COutput;
class CSTmTeletext;
class DMAArea;

const int MaxTeletextLines = 18;

struct stm_teletext_hw_node
{
  volatile stm_display_metadata_t *metadataNode;
  bool                             isForTopField;
  uint8_t                          ttxLineDENCRegs[5];
  uint8_t                          lineCount;
  void                            *dmaHandle;
  stm_teletext_line_t         *data;
};


class CSTmTeletext
{
public:
  CSTmTeletext(CDisplayDevice* pDev, uint32_t uDencOffset);
  virtual ~CSTmTeletext(void);

  virtual bool Create();

  virtual bool Start(uint32_t ulLineSize, COutput *master);
  virtual void Stop(void);

  stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  void FlushMetadata(stm_display_metadata_type_t);

  virtual void UpdateHW(void) = 0;


protected:
  uint32_t             * m_pDENCReg;
  uint32_t             * m_pDevRegs;
  void                 * m_Lock;
  uint32_t               m_uLineSize;
  uint32_t               m_uDENCDelay;
  int                    m_nQueueSize;

  COutput              * m_pMaster;

  DMA_Area               m_HWData;
  stm_teletext_hw_node * m_pQueue;

  stm_teletext_hw_node   m_CurrentNode;
  stm_teletext_hw_node   m_PendingNode;

  int                    m_nWriteIndex;
  int                    m_nReadIndex;
  volatile bool          m_bActive;

  stm_time64_t           m_LastQueuedTime;

  void ProgramDENC(stm_teletext_hw_node *);
  void DisableDENC(void);

  int  TeletextDMABlockSize(void) { return sizeof(stm_teletext_line_t)*MaxTeletextLines; }
  int  TeletextDMABlockOffset(int index) { return TeletextDMABlockSize()*index; }

  virtual stm_display_metadata_result_t ConstructTeletextNode(stm_display_metadata_t *,int);
  virtual void ReleaseHardwareNode(stm_teletext_hw_node *) = 0;
  virtual void StopDMAEngine(void) = 0;


  /*
   * Unlike the other components the DENC registers are defined
   * by their offset/4 and are only 8bits wide. Therefore we add the
   * register value to m_pDENCReg (uint32_t *) without an additional shift,
   * then convert it to a uint8_t *.
   */
  void WriteDENCReg(uint32_t reg, uint8_t val) { vibe_os_write_byte_register((uint8_t *)(m_pDENCReg + reg), val); }
  uint8_t ReadDENCReg(uint32_t reg) const { return vibe_os_read_byte_register((uint8_t *)(m_pDENCReg + reg)); }

private:
  CSTmTeletext(const CSTmTeletext&);
  CSTmTeletext& operator=(const CSTmTeletext&);
};

#endif // _STMTELETEXT_H
