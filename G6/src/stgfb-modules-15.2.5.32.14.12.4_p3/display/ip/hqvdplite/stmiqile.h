/***********************************************************************
 *
 * File: display/ip/stmiqile.h
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_IQI_LE_H
#define _STM_IQI_LE_H

#ifdef __cplusplus

#include <stm_display_plane.h>
#include <vibe_os.h>

#include <display/generic/DisplayInfo.h>
#include <display/generic/DisplayNode.h>
#include <display/ip/hqvdplite/lld/c8fvp3_stddefs.h>
#include <display/ip/hqvdplite/lld/c8fvp3_hqvdplite_api_IQI.h>


struct IQILeSetup
{
  struct {
    bool is_709_colorspace;
    bool lce_lut_init;
    bool enable_csc;
  } le;
};

class CSTmIQILe
{
public:
  CSTmIQILe (void);
  virtual ~CSTmIQILe (void);

  bool Create (void);

  void CalculateSetup (CDisplayNode * node, struct IQILeSetup * setup);

  void CalculateParams(struct IQILeSetup * setup, HQVDPLITE_IQI_Params_t * params);

  bool SetLeParams ( struct _IQILEConf *  le) __attribute__((nonnull,warn_unused_result));

  void GetLeParams(struct _IQILEConf *  le);

  bool SetLePreset(enum PlaneCtrlIQIConfiguration preset);

  void GetLePreset( enum PlaneCtrlIQIConfiguration * preset) const;

private:
  bool GenerateLeLUT (const struct _IQILEConf * const le, uint16_t *LutToPrg) const;
  bool GenerateFixedCurveTable (const struct _IQILEConfFixedCurveParams * const params, int16_t * const curve) const;

  struct _IQILEConf m_current_config;
  enum PlaneCtrlIQIConfiguration m_preset;
  uint16_t * m_LumaLUT;
};

#endif /* __cplusplus */


#endif /* _STM_IQI_H */
