/***********************************************************************
 *
 * File: display/ip/stmfdvo.h
 * Copyright (c) 2008-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFDVO_H
#define _STMFDVO_H

#include <display/generic/Output.h>
#include <display/ip/sync/dvo/stmsyncdvo.h>
#include <display/ip/sync/dvo/stmawgdvo.h>

#include <vibe_os.h>


class CDisplayDevice;
class CSTmSyncDvo;


class CSTmFDVO: public COutput
{
public:
  CSTmFDVO(uint32_t id, CDisplayDevice *, uint32_t ulFDVOOffset, COutput *,const char *name = "fdvo0");
  virtual ~CSTmFDVO();

  virtual bool Create (void);
  virtual void CleanUp(void);

  virtual OutputResults Start(const stm_display_mode_t*);
  virtual void DisableClocks(void) { }
  bool Stop(void);
  void Suspend(void);
  void Resume(void);

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;

  bool HandleInterrupts(void);

protected:
  uint32_t *m_pFDVORegs;

  COutput       *m_pMasterOutput;
  CSTmSyncDvo    m_SyncDvo;

  uint8_t        m_u422ChromaFilter;
  bool           m_bInvertDataClock;
  stm_awg_dvo_parameters_t m_sAwgCodeParams;

  virtual const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;

  virtual bool SetOutputFormat(uint32_t format, const stm_display_mode_t* pModeLine);

  void WriteFDVOReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pFDVORegs , reg, val); }
  uint32_t ReadFDVOReg(uint32_t reg)            { return vibe_os_read_register(m_pFDVORegs , reg); }

  void UpdateOutputMode(const stm_display_mode_t&);

private:
  CSTmFDVO(const CSTmFDVO&);
  CSTmFDVO& operator=(const CSTmFDVO&);
};


#endif /* _STMFDVO_H */
