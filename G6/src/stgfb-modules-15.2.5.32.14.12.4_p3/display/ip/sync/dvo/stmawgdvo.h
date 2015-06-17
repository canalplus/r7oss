/***********************************************************************
 *
 * File: display/ip/sync/dvo/stmawgdvo.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMAWG_DVO_H
#define _STMAWG_DVO_H

#include <stm_display_output.h>

#include <vibe_os.h>
#include <display/ip/sync/stmawg.h>
#include <display/ip/sync/dvo/stmawg_dvotypes.h>
#include <display/ip/sync/dvo/fw_gen/fw_gen.h>

#define AWG_MAX_SIZE 64

class CDisplayDevice;

typedef enum
{
  STM_AWG_MODE_HSYNC_IGNORE     = (1 << 0),
  STM_AWG_MODE_FIELDFRAME       = (1 << 1),

  STM_AWG_MODE_FILTER_HD        = (1 << 2),
  STM_AWG_MODE_FILTER_ED        = (1 << 3),
  STM_AWG_MODE_FILTER_SD        = (1 << 4)
} stm_awg_mode_t;


class CSTmAwgDvo: public CSTmAwg
{
public:
  CSTmAwgDvo (CDisplayDevice* pDev, uint32_t ram_offset);
  virtual ~CSTmAwgDvo (void);

  bool Start ( const stm_display_mode_t * const pMode
             , const uint32_t                   uFormat
             , stm_awg_dvo_parameters_t         AwgCodeParams);
  bool Stop (void);
  void SetAwgCodeParams (stm_awg_dvo_parameters_t AwgCodeParams);

protected:
  stm_awg_dvo_parameters_t m_sAwgCodeParams;

  virtual bool GenerateRamCode (const stm_display_mode_t * const pMode, const uint32_t uFormat);

};

#endif /* _STMAWG_DVO_H */
