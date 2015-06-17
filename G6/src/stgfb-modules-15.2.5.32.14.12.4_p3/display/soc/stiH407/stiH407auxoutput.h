/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407auxoutput.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STiH407AUXOUTPUT_H
#define _STiH407AUXOUTPUT_H

#include <display/ip/tvout/stmauxtvoutput.h>
#include <display/ip/misr/stmmisrauxtvout.h>

#include "stiH407reg.h"

class CDisplayDevice;
class CGammaMixer;
class CSTmClockLLA;
class CSTmVIP;
class CSTmMisrAuxTVOut;

class CSTiH407AuxOutput: public CSTmAuxTVOutput
{
public:
  CSTiH407AuxOutput(CDisplayDevice        *pDev,
                    CSTmVTG               *pVTG,
                    CSTmTVOutDENC         *pDENC,
                    CGammaMixer           *pMixer,
                    CSTmFSynth            *pFSynth,
                    CSTmHDFormatter       *pHDFormatter,
                    CSTmClockLLA          *pClkDivider,
                    CSTmVIP               *pHDFVIP,
                    CSTmVIP               *pDENCVIP,
              const stm_display_sync_id_t *sync_sel_map);

  virtual ~CSTiH407AuxOutput();

  bool ShowPlane(const CDisplayPlane *);

  // Public interface for MISR collection
  virtual TuningResults SetTuning( uint16_t service,
                                   void    *inputList,
                                   uint32_t inputListSize,
                                   void    *outputList,
                                   uint32_t outputListSize);

protected:
  // Virtual output method implementation required by base class
  bool SetVTGSyncs(const stm_display_mode_t *mode);
  bool ConfigureOutput(uint32_t format);
  void SetAuxClockToHDFormatter(void);
  void ConfigureDisplayClocks(const stm_display_mode_t *mode);
  void DisableClocks(void);

  void EnableDACs(void);
  void DisableDACs(void);
  void PowerDownDACs(void);

private:
  CSTmFSynth                  *m_pFSynth;
  CSTmClockLLA                *m_pClkDivider;
  CSTmVIP                     *m_pHDFVIP;
  CSTmVIP                     *m_pDENCVIP;
  const stm_display_sync_id_t *m_sync_sel_map;


  CSTmMisrAuxTVOut            *m_pMisrAux;
  virtual void SetMisrData(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt);
  virtual void UpdateMisrCtrl(void);

  // Hardware specific register access

  void     WriteReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevReg, (STiH407_TVOUT1_BASE + reg), val); }
  uint32_t ReadReg(uint32_t reg)                { return vibe_os_read_register(m_pDevReg, (STiH407_TVOUT1_BASE +reg)); }

  void WriteBitField(uint32_t RegAddr, int Offset, int Width, uint32_t bitFieldValue)
  {
    #define BIT_FIELD_MASK        (((unsigned int)0xffffffff << (Offset+Width)) | ~((unsigned int)0xffffffff << Offset))
    WriteReg(RegAddr, ( (ReadReg( RegAddr ) & BIT_FIELD_MASK) | ((bitFieldValue << Offset) & ~BIT_FIELD_MASK) ));
  }

  void     WriteSysCfgReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevReg, (STiH407_SYSCFG_CORE + reg), val); }
  uint32_t ReadSysCfgReg(uint32_t reg)                { return vibe_os_read_register(m_pDevReg, (STiH407_SYSCFG_CORE +reg)); }

  CSTiH407AuxOutput(const CSTiH407AuxOutput&);
  CSTiH407AuxOutput& operator=(const CSTiH407AuxOutput&);
};


#endif //_STiH407AUXOUTPUT_H
