/***********************************************************************
 *
 * File: display/ip/displaytiming/stmflexclkgen_fs660.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_FLEX_CLK_GEN_FS_660_H
#define _STM_FLEX_CLK_GEN_FS_660_H

#include "stmc32_4fs660.h"

class CDisplayDevice;

class CSTmFlexClkGenFS660: public CSTmC32_4FS660
{
public:
  CSTmFlexClkGenFS660(CDisplayDevice *pDev, uint32_t flexclkgen_offset, int generator, int channel);
  ~CSTmFlexClkGenFS660(void);

protected:
  uint32_t*             m_pDevRegs;
  uint32_t              m_uClkGenOffset;
  uint8_t               m_Id;
  int                   m_nsb;

  void WriteReg(uint32_t reg, uint32_t val)
  {
      vibe_os_write_register(m_pDevRegs, (m_uClkGenOffset+reg), val);
  }
  uint32_t ReadReg(uint32_t reg)
  {
      return vibe_os_read_register(m_pDevRegs, (m_uClkGenOffset+reg));
  }
  /* Register bit field Write*/
  void WriteBitField(uint32_t RegAddr, int Offset, int Width, uint32_t bitFieldValue)
  {
    const uint32_t BIT_FIELD_MASK = (((unsigned int)0xffffffff << (Offset+Width)) | ~((unsigned int)0xffffffff << Offset));
    WriteReg(RegAddr, ( (ReadReg( RegAddr ) & BIT_FIELD_MASK) | ((bitFieldValue << Offset) & ~BIT_FIELD_MASK) ));
  }

  void ProgramClock(void);

private:
  CSTmFlexClkGenFS660(const CSTmFlexClkGenFS660&);
  CSTmFlexClkGenFS660& operator=(const CSTmFlexClkGenFS660&);
};

#endif // _STM_FLEX_CLK_GEN_FS_660_H
