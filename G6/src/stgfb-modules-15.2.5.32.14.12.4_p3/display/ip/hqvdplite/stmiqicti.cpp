/***********************************************************************
 *
 * File: display/ip/stmiqicti.cpp
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>


#include "stmiqicti.h"


/* revision 2 CTI */

#define CTIr2_DEFAULT_COR                  (0x01) /* VDP_IQI_CTI_DEFAULT_COR */
#define CTIr2_DEFAULT_MED                  (0x1C) /* VDP_IQI_CTI_DEFAULT_MED */
#define CTIr2_DEFAULT_MON                  (0x04) /* VDP_IQI_CTI_DEFAULT_MON */


struct _IQICtiInitParameters {
  struct _CTI {
    /* CTI */
    uint8_t tmon;   /* value of cti used in monotonicity checker */
    uint8_t tmed;   /* value of the cti threshold used in 3 points median filter */
    uint8_t coring; /* value of cti coring used in the peaking filter */
    uint8_t extended_cti;
  } cti;
};



static const struct _IQICtiInitParameters IQI_CTI_initDefaults = {
  cti     : { tmon : CTIr2_DEFAULT_MON,
              tmed : CTIr2_DEFAULT_MED,
              coring : CTIr2_DEFAULT_COR,
              extended_cti : 0
            }
};


static const struct _IQICtiConf IQI_CTI_ConfigParams[PCIQIC_COUNT] = {
  /* disable */
  {             strength1 : IQICS_NONE,
                strength2 : IQICS_NONE
  },
  /* soft */
  {
                strength1 : IQICS_NONE,
                strength2 : IQICS_MIN
  },

  /* medium */
  {
                strength1 : IQICS_NONE,
                strength2 : IQICS_STRONG
  },

  /* strong */
  {             strength1 : IQICS_MIN,
                strength2 : IQICS_MEDIUM
  }
};




CSTmIQICti::CSTmIQICti (void)
{
  m_preset = PCIQIC_FIRST;
  m_current_config = IQI_CTI_ConfigParams[PCIQIC_FIRST];;
}


CSTmIQICti::~CSTmIQICti (void)
{

}


bool
CSTmIQICti::Create (void)
{

  m_current_config = IQI_CTI_ConfigParams[PCIQIC_FIRST];
  m_preset = PCIQIC_FIRST;

  return true;
}


void CSTmIQICti::CalculateParams(HQVDPLITE_IQI_Params_t * params)
{
    params->CtiConfig = ((IQI_CTI_initDefaults.cti.tmon << IQI_CTI_CONFIG_CTI_T_MON_SHIFT ) &  IQI_CTI_CONFIG_CTI_T_MON_MASK) |
                        ((IQI_CTI_initDefaults.cti.tmed << IQI_CTI_CONFIG_CTI_T_MED_SHIFT ) &  IQI_CTI_CONFIG_CTI_T_MED_MASK) |
                        (( m_current_config.strength1 << IQI_CTI_CONFIG_CTI_STRENGTH1_SHIFT ) &  IQI_CTI_CONFIG_CTI_STRENGTH1_MASK) |
                        (( m_current_config.strength2 << IQI_CTI_CONFIG_CTI_STRENGTH2_SHIFT ) &  IQI_CTI_CONFIG_CTI_STRENGTH2_MASK) |
                        ((IQI_CTI_initDefaults.cti.extended_cti << IQI_CTI_CONFIG_EXTENDED_CTI_SHIFT ) &  IQI_CTI_CONFIG_EXTENDED_CTI_MASK) |
                        ((IQI_CTI_initDefaults.cti.coring << IQI_CTI_CONFIG_CTI_COR_SHIFT ) &  IQI_CTI_CONFIG_CTI_COR_MASK) ;

   TRC( TRC_ID_HQVDPLITE, "CTI Config register content : 0x%x", params->CtiConfig);
}


/* to be called on SetControl */
bool
CSTmIQICti::SetCtiParams ( struct _IQICtiConf *  cti)
{

  if (!cti
      || cti->strength1 >= IQICS_COUNT
      || cti->strength2 >= IQICS_COUNT)
    {
      TRC( TRC_ID_ERROR," invalid IQI CTI parameters");
      return false;
    }

    /* copy in m_current config */
    m_current_config = *cti;

    TRC( TRC_ID_MAIN_INFO, "cti->strength1 = %d", cti->strength1 );
    TRC( TRC_ID_MAIN_INFO, "cti->strength2 = %d", cti->strength2 );

    return true;

}

void CSTmIQICti::GetCtiParams(struct _IQICtiConf *  cti)
{
    *cti = m_current_config;
}



bool CSTmIQICti::SetCtiPreset(PlaneCtrlIQIConfiguration preset)
{

    if (preset >=  PCIQIC_COUNT)
    {
      TRC( TRC_ID_ERROR, "invalid IQI cti preset" );
      return false;
    }
    m_current_config =  IQI_CTI_ConfigParams[preset];
    m_preset = preset;
    TRC( TRC_ID_HQVDPLITE, "CTI Preset = %d", preset );
    return true;
}

void CSTmIQICti::GetCtiPreset( enum PlaneCtrlIQIConfiguration * preset) const
{
    *preset = m_preset;

}

