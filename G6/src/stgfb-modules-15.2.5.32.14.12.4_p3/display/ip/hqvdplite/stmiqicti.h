/***********************************************************************
 *
 * File: display/ip/stmiqicti.h
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_IQI_CTI_H
#define _STM_IQI_CTI_H

#ifdef __cplusplus

#include <stm_display_plane.h>
#include <vibe_os.h>

#include <display/generic/DisplayInfo.h>
#include <display/ip/hqvdplite/lld/c8fvp3_stddefs.h>
#include <display/ip/hqvdplite/lld/c8fvp3_hqvdplite_api_IQI.h>


class CSTmIQICti
{
public:
  CSTmIQICti (void);
  virtual ~CSTmIQICti (void);

  bool Create (void);

  void CalculateParams(HQVDPLITE_IQI_Params_t * params);

  bool SetCtiParams ( struct _IQICtiConf *  peaking) __attribute__((nonnull,warn_unused_result));

  void GetCtiParams(struct _IQICtiConf *  peaking);

  bool SetCtiPreset(enum PlaneCtrlIQIConfiguration preset);

  void GetCtiPreset( enum PlaneCtrlIQIConfiguration * preset) const;

private:


  struct _IQICtiConf m_current_config;
  enum PlaneCtrlIQIConfiguration m_preset;

};

#endif /* __cplusplus */


#endif /* _STM_IQI_H */
