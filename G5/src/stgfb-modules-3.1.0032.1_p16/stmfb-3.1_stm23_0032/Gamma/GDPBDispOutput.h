/***********************************************************************
 *
 * File: Gamma/GDPBDispOutput.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * This class implements an output that "mixes" virtual display planes using
 * the BDisp blit compositor onto a GDP hardware plane behaving like a GDMA.
 *
\***********************************************************************/

#ifndef _GDP_BDISP_OUTPUT_H
#define _GDP_BDISP_OUTPUT_H

#include <STMCommon/stmbdispoutput.h>

class CGammaCompositorGDP;

class CGDPBDispOutput: public CSTmBDispOutput
{
public:
  CGDPBDispOutput(CDisplayDevice *pDev,
                  COutput        *pMasterOutput,
                  ULONG          *pBlitterBaseAddr,
                  ULONG           BDispPhysAddr,
                  ULONG           CQOffset,
                  int             qNumber,
                  int             qPriority);

  ~CGDPBDispOutput();

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  void UpdateHW(void);

protected:
  stm_plane_id_t       m_hwPlaneID;
  DMA_Area            *m_pGDPRegisters[2];
  bool                 m_bHWEnabled;
  ULONG                m_ulBrightness;
  ULONG                m_ulOpacity;

  CGammaCompositorGDP *m_pHWPlane;

  bool SetGDPConfiguration(const stm_mode_line_t* pModeLine);

private:
  CGDPBDispOutput(const CGDPBDispOutput&);
  CGDPBDispOutput& operator=(const CGDPBDispOutput&);
};

#endif /* _GDP_BDISP_OUTPUT_H */
