/***********************************************************************
 *
 * File: display/ip/hdf/stmhdf_v4_3.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDF_V4_3_H
#define _STM_HDF_V4_3_H

#include <stm_display.h>
#include <display/ip/hdf/stmhdf.h>

class CSTmHDFormatter;

class CSTmHDF_V4_3: public CSTmHDFormatter
{
public:
  CSTmHDF_V4_3( CDisplayDevice      *pDev,
                uint32_t             TVFmt,
                uint32_t             AWGRamOffset,
                uint32_t             AWGRamSize,
                bool                 bHasSeparateCbCrRescale,
                bool                 bHas4TapSyncFilter);

  virtual ~CSTmHDF_V4_3();

  void Resume(void);

  bool SetInputParams(const bool IsMainInput, uint32_t format);
  void SetDencSource(const bool IsMainInput);
  void SetSignalRangeClipping(void);
  void SetUpsampler(const int multiple);

protected:
  void SetYCbCrReScalers(void);
  void EnableAWG (void);
  void DisableAWG (void);

private:
  uint32_t                       m_AWGRam;
  uint32_t                       m_AWGRamSize;

  CSTmHDF_V4_3(const CSTmHDF_V4_3&);
  CSTmHDF_V4_3& operator=(const CSTmHDF_V4_3&);

  void SetupDefaultDACRouting(void);

  void WriteAWGReg(uint32_t reg, uint16_t val) { vibe_os_write_register(m_pDevRegs, (m_AWGRam + reg), val); }
  uint16_t ReadAWGReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_AWGRam + reg)); }
};


#endif //_STM_HDF_V4_3_H
