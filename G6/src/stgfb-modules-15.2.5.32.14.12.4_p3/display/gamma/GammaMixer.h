/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef _GAMMA_MIXER_H
#define _GAMMA_MIXER_H

#include <display/generic/DisplayMixer.h>

typedef enum
{
  /*
   * ST hardware planes, the defines deliberately match the plane enable bit in
   * the hardware compositor mixer control.
   */
  MIXER_ID_NONE   = 0,         /*!< No active planes                            */
  MIXER_ID_BKG    = (1L<<0),   /*!< Mixer background colour                     */
  MIXER_ID_VID1   = (1L<<1),   /*!< Video plane                                 */
  MIXER_ID_VID2   = (1L<<2),   /*!< Video plane                                 */
  MIXER_ID_GDP1   = (1L<<3),   /*!< Graphics plane                              */
  MIXER_ID_GDP2   = (1L<<4),   /*!< Graphics plane                              */
  MIXER_ID_GDP3   = (1L<<5),   /*!< Graphics plane                              */
  MIXER_ID_GDP4   = (1L<<6),   /*!< Graphics plane                              */
  MIXER_ID_CUR    = (1L<<9),   /*!< Cursor plane                                */
  MIXER_ID_ALP    = (1L<<15),  /*!< Alpha mask plane, not currently supported   */

  MIXER_ID_VBI    = (1L<<16),  /*!< A virtual plane linked to a GDP for VBI waveforms */

} stm_mixer_id_t;


class CGammaMixer: public CDisplayMixer
{
public:
  CGammaMixer(CDisplayDevice *pDev,
              uint32_t        ulRegOffset,
        const stm_mixer_id_t *planeMap,
              stm_mixer_id_t  ulVBILinkedPlane = MIXER_ID_NONE);

  ~CGammaMixer();

  bool Start(const stm_display_mode_t *);
  void Stop(void);

  void Suspend(void);
  void Resume(void);

  stm_display_mode_t GetCurrentMode(void) const { return m_CurrentMode; }
  bool IsStarted(void) { return m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED; }

  bool PlaneValid(const CDisplayPlane *) const;

  uint32_t GetCapabilities(void) const { return m_ulCapabilities; }

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;

  bool EnablePlane(const CDisplayPlane *, stm_mixer_activation_t act = STM_MA_NONE);
  bool DisablePlane(const CDisplayPlane *);

  bool UpdateFromOutput(COutput* pOutput, stm_plane_update_reason_t update_reason);

   const CDisplayPlane ** m_pEnabledPlanes;

  bool HasEnabledPlanes(void) const;

  bool SetPlaneDepth(const CDisplayPlane *, int depth, bool activate);
  bool GetPlaneDepth(const CDisplayPlane *, int *depth) const;

protected:
  CDisplayDevice       * m_pDev;                 // Display Device pointer
  uint32_t             * m_pGammaReg;            // Memory mapped registers
  uint32_t               m_ulRegOffset;          // Mixer address register offset
  void                 * m_lock;                 // Resource lock
  uint32_t               m_ulCapabilities;       // Output capabilities provided by this mixer

  const stm_mixer_id_t * m_planeMap;          // Map of plane ids to mixer ids
  uint32_t               m_validPlanes;          // OR'd mixer IDs supported on this mixer
  volatile uint32_t      m_planeEnables;         // Current enabled planes
  volatile uint32_t      m_planeActive;          // Active content, graphics or video
  uint32_t               m_ulBackground;         // Background colour
  bool                   m_bBackgroundVisible;   // Background visibility

  stm_display_mode_t     m_CurrentMode;          // Current display mode
  bool                   m_bIsSuspended;

  int                    m_crossbarSize;     // The number of plane entries in the crossbar
  bool                   m_bHasHwYCbCrMatrix;// Does the hardware include an in-built selectable RGB->YCbCr conversion matrix
  stm_ycbcr_colorspace_t m_colorspaceMode;   // Required output colorspace

  stm_mixer_id_t         m_ulVBILinkedPlane; // The real plane ID for VBI


  virtual bool CanEnablePlane(const CDisplayPlane *);
  virtual void SetMixerForPlanes(stm_mixer_id_t planeID, bool isEnabled, stm_mixer_activation_t act);

  uint32_t GetColorspaceCTL(const stm_display_mode_t *pModeLine) const;
  virtual void UpdateColorspace(const stm_display_mode_t *pModeLine);

  uint32_t ReadMixReg(uint32_t reg) const { return vibe_os_read_register(m_pGammaReg, (m_ulRegOffset+reg)); }
  void  WriteMixReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pGammaReg, (m_ulRegOffset+reg), val); }

private:
  /*
   * Power management suspend saved state
   */
  uint32_t m_SavedCrossbarRegister;

  CGammaMixer(const CGammaMixer&);
  CGammaMixer& operator=(const CGammaMixer&);

  bool HasBackground(void) const { return ((m_validPlanes & MIXER_ID_BKG) != 0); }

};

#endif /* _GAMMA_MIXER_H */
