/***********************************************************************
 *
 * File: STMCommon/stmteletext.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMTELETEXT_H
#define _STMTELETEXT_H

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
  volatile stm_meta_data_t *metadataNode;
  bool                      isForTopField;
  UCHAR                     ttxLineDENCRegs[5];
  UCHAR                     lineCount;
  void                     *dmaHandle;
  stm_ebu_teletext_line_t  *data;
};


class CSTmTeletext
{
public:
  CSTmTeletext(CDisplayDevice* pDev, ULONG ulDencOffset);
  virtual ~CSTmTeletext(void);

  virtual bool Create();

  virtual bool Start(ULONG ulLineSize, COutput *master);
  virtual void Stop(void);

  stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  void FlushMetadata(stm_meta_data_type_t);

  virtual void UpdateHW(void) = 0;


protected:
  ULONG                 *m_pDENCReg;
  ULONG                 *m_pDevRegs;
  ULONG                  m_ulLock;
  ULONG                  m_ulLineSize;
  ULONG                  m_ulDENCDelay;
  int                    m_nQueueSize;

  COutput               *m_pMaster;

  DMA_Area               m_HWData;
  stm_teletext_hw_node  *m_pQueue;

  stm_teletext_hw_node   m_CurrentNode;
  stm_teletext_hw_node   m_PendingNode;

  int                    m_nWriteIndex;
  int                    m_nReadIndex;
  volatile bool          m_bActive;

  TIME64                 m_LastQueuedTime;

  void ProgramDENC(stm_teletext_hw_node *);
  void DisableDENC(void);

  int  TeletextDMABlockSize(void) { return sizeof(stm_ebu_teletext_line_t)*MaxTeletextLines; }
  int  TeletextDMABlockOffset(int index) { return TeletextDMABlockSize()*index; }

  virtual stm_meta_data_result_t ConstructTeletextNode(stm_meta_data_t *,int);
  virtual void ReleaseHardwareNode(stm_teletext_hw_node *) = 0;
  virtual void StopDMAEngine(void) = 0;


  /*
   * Unlike the other components the DENC registers are defined
   * by their offset/4 and are only 8bits wide. Therefore we add the
   * register value to m_pDENCReg (ULONG *) without an additional shift,
   * then convert it to a UCHAR *.
   */
  void WriteDENCReg(ULONG reg, UCHAR val) { g_pIOS->WriteByteRegister((UCHAR *)(m_pDENCReg + reg), val); }
  UCHAR ReadDENCReg(ULONG reg) const { return g_pIOS->ReadByteRegister((UCHAR *)(m_pDENCReg + reg)); }

private:
  CSTmTeletext(const CSTmTeletext&);
  CSTmTeletext& operator=(const CSTmTeletext&);
};

#endif // _STMTELETEXT_H
