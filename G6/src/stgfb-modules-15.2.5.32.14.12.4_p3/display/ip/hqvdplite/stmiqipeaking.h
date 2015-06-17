/***********************************************************************
 *
 * File: display/ip/stmiqi.h
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_IQI_PEAKING_H
#define _STM_IQI_PEAKING_H


enum IQICellRevision
{
  IQI_CELL_REV1, /* not supported */
  IQI_CELL_REV2
};



#ifdef __cplusplus

#include <stm_display_plane.h>
#include <vibe_os.h>

#include <display/generic/DisplayInfo.h>
#include <display/ip/hqvdplite/lld/c8fvp3_stddefs.h>
#include <display/ip/hqvdplite/lld/c8fvp3_hqvdplite_api_IQI.h>





struct IQIPeakingSetup
{
  struct {
    bool env_detect_3_lines;
    bool vertical_peaking_allowed;
    bool extended_size;
    enum IQIPeakingFilterFrequency highpass_filter_cutoff_freq;
    enum IQIPeakingFilterFrequency bandpass_filter_center_freq;
    uint32_t vertical_gain;
    uint32_t horizontal_gain;
    uint32_t coeff0_;
    uint32_t coeff1_;
    uint32_t coeff2_;
    uint32_t coeff3_;
    uint32_t coeff4_;
  } peaking;
};

class CSTmIQIPeaking
{
public:
  CSTmIQIPeaking (void);
  virtual ~CSTmIQIPeaking (void);

  bool Create (void);




  void CalculateSetup ( CDisplayInfo        * pqbi,
                        bool isDisplayInterlaced,
                       struct IQIPeakingSetup            *  setup)  __attribute__((nonnull(1)));

  void CalculateParams(struct IQIPeakingSetup * setup, HQVDPLITE_IQI_Params_t * params);

  bool SetPeakingParams ( struct _IQIPeakingConf *  peaking) __attribute__((nonnull,warn_unused_result));

  void GetPeakingParams(struct _IQIPeakingConf *  peaking);

  bool SetPeakingPreset(enum PlaneCtrlIQIConfiguration preset);

  void GetPeakingPreset( enum PlaneCtrlIQIConfiguration * preset) const;

private:
  enum IQICellRevision m_Revision;

  struct _IQIPeakingConf m_current_config;
  enum PlaneCtrlIQIConfiguration m_preset;

  void PeakingGainSet (struct IQIPeakingSetup *  setup,

                       enum IQIPeakingHorVertGain     gain_highpass,
                       enum IQIPeakingHorVertGain     gain_bandpass,
                       enum IQIPeakingFilterFrequency cutoff_freq,
                       enum IQIPeakingFilterFrequency center_freq);


  IQIPeakingFilterFrequency
    GetZoomCorrectedFrequency ( CDisplayInfo             * pqbi,
                               enum IQIPeakingFilterFrequency  PeakingFrequency)  __attribute__((nonnull(1)));



};

#endif /* __cplusplus */


#endif /* _STM_IQI_H */
