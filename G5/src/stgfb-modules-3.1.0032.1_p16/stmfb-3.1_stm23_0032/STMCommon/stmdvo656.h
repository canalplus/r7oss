/***********************************************************************
 *
 * File: STMCommon/stmdvo656.h
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
\***********************************************************************/

#ifndef _STMDVO656_H
#define _STMDVO656_H

#include <Generic/Output.h>

class CDisplayDevice;

class CSTmDVO656: public COutput
{
public:
  CSTmDVO656(CDisplayDevice *, ULONG ulRegOffset, COutput *);
  virtual ~CSTmDVO656();

  ULONG GetCapabilities(void) const;

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  bool CanShowPlane(stm_plane_id_t planeID);
  bool ShowPlane   (stm_plane_id_t planeID);
  void HidePlane   (stm_plane_id_t planeID);
 
  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

  bool HandleInterrupts(void);
  
protected:
  ULONG* m_pDevRegs;
  ULONG  m_ulDVOOffset;

  COutput *m_pMasterOutput;
    
  void WriteDVOReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulDVOOffset+reg)>>2), val); }
  ULONG ReadDVOReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulDVOOffset+reg)>>2)); }

private:
  CSTmDVO656(const CSTmDVO656&);
  CSTmDVO656& operator=(const CSTmDVO656&);
};


#define STM_DVO_CTL_656       0x00
#define STM_DVO_CTL_PADS      0x04 /* STi5528 & 8K?   */
#define STM_DVO_PROG_DELAY    0x04 /* STm8010 others? */
#define STM_DVO_VPO_656       0x08
#define STM_DVO_VPS_656       0x0c

#define DVO_CTL656_EN656_NOT_CCIR601 (1L<<0)
#define DVO_CTL656_POE               (1L<<1)
#define DVO_CTL656_ADDYDO            (1L<<2)
#define DVO_CTL656_ADDYDS            (1L<<3)
#define DVO_CTL656_POY               (1L<<4)
#define DVO_CTL656_POU               (1L<<5)
#define DVO_CTL656_SF_SHIFT          8
#define DVO_CTL656_SF_MASK           (7L<<8)
#define DVO_CTL656_FFZ               (1L<<11)
#define DVO_CTL656_DIS_1_254         (1L<<16)
#define DVO_CTL656_16BIT_N8BIT       (1L<<24)
#define DVO_CTL656_444_N_422         (1L<<25)

#define DVO_SF_625_LINE (2L << DVO_CTL656_SF_SHIFT)
#define DVO_SF_525_LINE (3L << DVO_CTL656_SF_SHIFT)

#endif /* _GAMMA_DVO_H */
