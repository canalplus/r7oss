/***********************************************************************
 *
 * File: STMCommon/stmbdisp.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_BDISP_H
#define _STM_BDISP_H

#include <STMCommon/stmbdispaq.h>
//#include <STMCommon/stmbdispcq.h>


#define STM_BDISP_MAX_CQs   2
#define STM_BDISP_MAX_AQs   4


class CSTmBDisp
{
public:
  CSTmBDisp (ULONG *pBlitterBaseAddr,
             stm_blitter_device_priv m_eBDispDevice,
             ULONG  drawOps = BDISP_VALID_DRAW_OPS, /* Available operations. These may need to be */
             ULONG  copyOps = BDISP_VALID_COPY_OPS, /* altered for specific chip instantiations   */
             ULONG  nAQs    = 1,                    /* Number of queues */
             ULONG  nCQs    = 2,                    /* Number of queues */
             ULONG  nLineBufferLength = 128);       /* line buffer length */
  virtual ~CSTmBDisp (void);

  bool Create (void);

  CSTmBDispAQ *GetAQ (unsigned int qNumber);
//  CSTmBDispCQ *GetCQ (unsigned int qNumber);

protected:

  /* PLUG register setup. May need to be tweaked for performance for different
     platforms. */
  virtual void ConfigurePlugs (void);

  void  WriteBDispReg (ULONG reg, ULONG val) { g_pIOS->WriteRegister       (m_pBDispReg + (reg>>2), val); }
  ULONG ReadBDispReg  (ULONG reg)            { return g_pIOS->ReadRegister (m_pBDispReg + (reg>>2));      }

  unsigned long m_nLineBufferLength;
  stm_blitter_device_priv m_eBDispDevice;

private:
  ULONG *m_pBDispReg;

  unsigned int  m_nAQs;
  unsigned int  m_nCQs;

  unsigned long m_BDispAQdrawOps;
  unsigned long m_BDispAQcopyOps;


  CSTmBDispAQ *m_pAQs[STM_BDISP_MAX_AQs];
//  CSTmBDispCQ *m_pCQs[STM_BDISP_MAX_CQs];
};

#endif // _STM_BDISP_H
