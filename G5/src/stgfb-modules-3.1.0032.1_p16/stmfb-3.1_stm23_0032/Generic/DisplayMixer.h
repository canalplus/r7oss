/***********************************************************************
 *
 * File: Generic/DisplayMixer.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _DISPLAY_MIXER_H
#define _DISPLAY_MIXER_H

class CDisplayDevice;

typedef enum
{
  /*
   * To be interpreted as an or'd bit field, hence the explicit values
   */
  STM_MA_NONE     = 0,
  STM_MA_GRAPHICS = 1,
  STM_MA_VIDEO    = 2,
  STM_MA_ALL      = 3
} stm_mixer_activation_t;


class CDisplayMixer
{
public:
  CDisplayMixer(CDisplayDevice* pDev): m_pDev(pDev),
                                       m_validPlanes(0),
                                       m_planeEnables(0),
                                       m_planeActive(0),
                                       m_ulBackground(0),
                                       m_pCurrentMode(0) {}

  virtual ~CDisplayMixer() {};

  virtual bool Start(const stm_mode_line_t *) = 0;
  virtual void Stop(void) = 0;

  virtual ULONG SupportedControls(void) const = 0;
  virtual void  SetControl(stm_output_control_t, ULONG ulNewVal) = 0;
  virtual ULONG GetControl(stm_output_control_t) const = 0;

  virtual bool EnablePlane(stm_plane_id_t planeID, stm_mixer_activation_t act = STM_MA_NONE) = 0;
  virtual bool DisablePlane(stm_plane_id_t planeID) = 0;

  virtual bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) = 0;
  virtual bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const = 0;

  bool PlaneValid(stm_plane_id_t planeID) const { return (m_validPlanes & planeID) != 0; }
  ULONG GetActivePlanes(void) const { return m_planeEnables; }

protected:
  CDisplayDevice* m_pDev;         // Display Device pointer
  ULONG           m_validPlanes;  // OR of planes in specific mixer
  volatile ULONG  m_planeEnables; // Current enabled planes
  volatile ULONG  m_planeActive;  // Active content, graphics or video
  ULONG           m_ulBackground; // Background colour

  const stm_mode_line_t *m_pCurrentMode;

private:
  CDisplayMixer(const CDisplayMixer&);
  CDisplayMixer& operator=(const CDisplayMixer&);

};

#endif /* _DISPLAY_MIXER_H */
