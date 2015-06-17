/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDF_V5_0_H
#define _STM_HDF_V5_0_H

#include <stm_display.h>
#include <display/ip/hdf/stmhdf.h>

class CSTmHDFormatter;

class CSTmHDF_V5_0: public CSTmHDFormatter
{
public:
  CSTmHDF_V5_0( CDisplayDevice      *pDev,
                uint32_t             TVOReg,
                uint32_t             TVFmt,
                uint32_t             AWGRam,
                uint32_t             AWGRamSize);

  virtual ~CSTmHDF_V5_0();

  void Resume(void);

  bool SetInputParams(const bool IsMainInput, uint32_t format);
  void SetDencSource(const bool IsMainInput) { };
  void SetSignalRangeClipping(void) { };

protected:
  void SetYCbCrReScalers(void);
  void EnableAWG (void);
  void DisableAWG (void);

private:
  uint32_t                       m_TVOReg;
  uint32_t                       m_AWGRam;
  uint32_t                       m_AWGRamSize;

  CSTmHDF_V5_0(const CSTmHDF_V5_0&);
  CSTmHDF_V5_0& operator=(const CSTmHDF_V5_0&);

  void SetSDDACUpsampler(const int multiple);

  void WriteAWGReg(uint32_t reg, uint16_t val) { vibe_os_write_register(m_pDevRegs, (m_AWGRam + reg), val); }
  uint16_t ReadAWGReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_AWGRam + reg)); }

  void WriteTVOReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_TVOReg + reg), val); }
  uint32_t ReadTVOReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_TVOReg + reg)); }
};


#endif // _STM_HDF_V5_0_H
