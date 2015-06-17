/***********************************************************************
 *
 * File: Gamma/GammaMixer.h
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_MIXER_H
#define _GAMMA_MIXER_H

#include <Generic/DisplayMixer.h>

class CGammaMixer: public CDisplayMixer
{
public:
  CGammaMixer(CDisplayDevice* pDev, ULONG ulRegOffset, stm_plane_id_t ulVBILinkedPlane = OUTPUT_NONE);
  ~CGammaMixer();

  bool Start(const stm_mode_line_t *);
  void Stop(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  bool EnablePlane(stm_plane_id_t planeID, stm_mixer_activation_t act = STM_MA_NONE);
  bool DisablePlane(stm_plane_id_t planeID);
  bool EnablePlaneForce(stm_plane_id_t planeID, stm_mixer_activation_t act = STM_MA_NONE);
  bool DisablePlaneForce(stm_plane_id_t planeID);

  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

protected:
  ULONG*          m_pGammaReg;    // Memory mapped registers
  ULONG           m_ulRegOffset;  // Mixer address register offset
  int             m_crossbarSize; // The number of plane entries in the crossbar

  stm_plane_id_t  m_ulVBILinkedPlane; // The real plane ID for OUTPUT_VBI

  ULONG           m_ulAVO;
  ULONG           m_ulAVS;
  ULONG           m_ulBCO;
  ULONG           m_ulBCS;

  bool                   m_bHasYCbCrMatrix;
  stm_ycbcr_colorspace_t m_colorspaceMode;

  ULONG           m_lock;
  ULONG           m_planesMask;

  virtual bool CanEnablePlane(stm_plane_id_t planeID);
  virtual void SetMixerForPlanes(stm_plane_id_t planeID, bool isEnabled, stm_mixer_activation_t act);

  ULONG GetColorspaceCTL(const stm_mode_line_t *pModeLine) const;

  ULONG ReadMixReg(ULONG reg) const { return g_pIOS->ReadRegister(m_pGammaReg + ((m_ulRegOffset+reg)>>2)); }
  void  WriteMixReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pGammaReg + ((m_ulRegOffset+reg)>>2), val); }

private:
  CGammaMixer(const CGammaMixer&);
  CGammaMixer& operator=(const CGammaMixer&);

};

#endif /* _GAMMA_MIXER_H */
