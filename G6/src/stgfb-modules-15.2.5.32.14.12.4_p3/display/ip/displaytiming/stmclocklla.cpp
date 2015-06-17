/***********************************************************************
 *
 * File: display/ip/displaytiming/stmclocklla.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "stmclocklla.h"

CSTmClockLLA::CSTmClockLLA( struct vibe_clk *clk_src_map, uint32_t clk_src_mapsize
                              , struct vibe_clk *clk_out_map, uint32_t clk_out_mapsize)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t i;

  m_sourceMap   = clk_src_map;
  m_nSrcMapSize = clk_src_mapsize;
  m_outputMap   = clk_out_map;
  m_nOutMapSize = clk_out_mapsize;

  m_bIsSuspended = false;

  ASSERTF(m_sourceMap,("m_sourceMap should be valid at creation time!'\n"));
  ASSERTF(m_outputMap,("m_outputMap should be valid at creation time!'\n"));

  TRC( TRC_ID_MAIN_INFO, "SRC[%p:%d] - OUT[%p:%d].", m_sourceMap, m_nSrcMapSize, m_outputMap, m_nOutMapSize );

  /* Initialize SRC : This will be done only for disabled sources */
  for(i=0; i<m_nSrcMapSize; i++)
  {
    TRC( TRC_ID_MAIN_INFO, "Enabling Clock %s.", m_sourceMap[i].name );
    if(vibe_os_clk_enable(&m_sourceMap[i]) == 0)
    {
      if(m_sourceMap[i].parent)
      {
        TRC( TRC_ID_MAIN_INFO, "Clock %s - src = %s.", m_sourceMap[i].name, m_sourceMap[i].parent->name );
        vibe_os_clk_set_parent(&m_sourceMap[i], m_sourceMap[i].parent);
      }
      if(m_sourceMap[i].rate)
        vibe_os_clk_set_rate(&m_sourceMap[i], m_sourceMap[i].rate);
      else
        m_sourceMap[i].rate = vibe_os_clk_get_rate(&m_sourceMap[i]);
      TRC( TRC_ID_MAIN_INFO, "Clock %s - rate = %d.", m_sourceMap[i].name, m_sourceMap[i].rate );
    }
  }

  /* Initialize OUT */
  for(i=0; i<m_nOutMapSize; i++)
  {
    TRC( TRC_ID_MAIN_INFO, "Enabling Clock %s.", m_outputMap[i].name );
    if(vibe_os_clk_enable(&m_outputMap[i]) == 0)
    {
      if(m_outputMap[i].parent)
      {
        TRC( TRC_ID_MAIN_INFO, "Clock %s - src = %s.", m_outputMap[i].name, m_outputMap[i].parent->name );
        vibe_os_clk_set_parent(&m_outputMap[i], m_outputMap[i].parent);
      }
      if(m_outputMap[i].rate)
        vibe_os_clk_set_rate(&m_outputMap[i], m_outputMap[i].rate);
      else
        m_outputMap[i].rate = vibe_os_clk_get_rate(&m_outputMap[i]);
      TRC( TRC_ID_MAIN_INFO, "Clock %s - rate = %d.", m_outputMap[i].name, m_outputMap[i].rate );
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmClockLLA::~CSTmClockLLA(void)
{
  uint32_t i;

  /*
   * Do not disable clocks when suspended because it was already done
   */
  if(!m_bIsSuspended)
  {
    for(i=0; i<m_nSrcMapSize; i++)
      vibe_os_clk_disable(&m_sourceMap[i]);

    for(i=0; i<m_nOutMapSize; i++)
      vibe_os_clk_disable(&m_outputMap[i]);
  }
}


bool CSTmClockLLA::Enable(stm_clk_divider_output_name_t   name,
                                stm_clk_divider_output_source_t src,
                                stm_clk_divider_output_divide_t div)
{
  uint32_t source, output;
  uint32_t rate;

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "OUT Clock %d NOT FOUND!!", name );
    return false;
  }

  vibe_os_clk_enable(&m_outputMap[output]);

  if(src == STM_CLK_SRC_SPARE)
  {
    TRC( TRC_ID_MAIN_INFO, "Enabling Clock %s", m_outputMap[output].name );
    return true;
  }

  if(!lookupSource(src,&source))
  {
    TRC( TRC_ID_ERROR, "SRC Clock %d NOT FOUND!!", src );
    return false;
  }

  rate =  vibe_os_clk_get_rate(&m_sourceMap[source]) >> div;
  TRC( TRC_ID_MAIN_INFO, "Enabling Clock %s (src is %s, div = %d, rate = %d)", m_outputMap[output].name, m_sourceMap[source].name, (1 << div), rate );

  vibe_os_clk_set_parent(&m_outputMap[output], &m_sourceMap[source]);
  vibe_os_clk_set_rate(&m_outputMap[output], rate);

  return true;
}


bool CSTmClockLLA::Disable(stm_clk_divider_output_name_t name)
{
  uint32_t output;

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "Clock %d NOT FOUND!!", name );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Disabling Clock %s", m_outputMap[output].name );
  vibe_os_clk_disable(&m_outputMap[output]);

  return true;
}


bool CSTmClockLLA::SetRate(stm_clk_divider_output_name_t name, uint32_t rate)
{
  uint32_t output;

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "OUT Clock %d NOT FOUND!!", name );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Setting Clock %s rate to %d", m_outputMap[output].name, rate );

  vibe_os_clk_set_rate(&m_outputMap[output], rate);

  return true;
}


uint32_t CSTmClockLLA::GetRate(stm_clk_divider_output_name_t name)
{
  uint32_t output;

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "OUT Clock %d NOT FOUND!!", name );
    return 0;
  }

  uint32_t rate = vibe_os_clk_get_rate(&m_outputMap[output]);

  TRC( TRC_ID_MAIN_INFO, "Clock %s rate is %d", m_outputMap[output].name, rate );

  return rate;
}


bool CSTmClockLLA::SetParent(stm_clk_divider_output_name_t name, stm_clk_divider_output_source_t src)
{
  uint32_t source, output;

  if(!lookupSource(src,&source))
  {
    TRC( TRC_ID_ERROR, "SRC Clock %d NOT FOUND!!", src );
    return false;
  }

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "OUT Clock %d NOT FOUND!!", name );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Setting Clock %s parent to %s", m_outputMap[output].name, m_sourceMap[source].name );

  vibe_os_clk_set_parent(&m_outputMap[output], &m_sourceMap[source]);

  return true;
}


stm_clk_divider_output_source_t CSTmClockLLA::GetParent(stm_clk_divider_output_name_t name)
{
  uint32_t output;

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "OUT Clock %d NOT FOUND!!", name );
    return STM_CLK_SRC_SPARE;
  }

  struct vibe_clk *parent = vibe_os_clk_get_parent(&m_outputMap[output]);
  TRC( TRC_ID_MAIN_INFO, "Clock %s parent is %s", m_outputMap[output].name, parent->name );

  return static_cast<stm_clk_divider_output_source_t>(parent->id);
}


bool CSTmClockLLA::isEnabled(stm_clk_divider_output_name_t name) const
{
  uint32_t output;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "Clock %d NOT FOUND!!", name );
    return false;
  }

  bool enabled = m_outputMap[output].enabled == 0 ? false : true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return enabled;
}


bool CSTmClockLLA::getDivide(stm_clk_divider_output_name_t    name,
                               stm_clk_divider_output_divide_t *div) const
{
  uint32_t output;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "Clock %d NOT FOUND!!", name );
    return false;
  }

  struct vibe_clk *parent = vibe_os_clk_get_parent(&m_outputMap[output]);
  uint32_t d = vibe_os_clk_get_rate(parent)/vibe_os_clk_get_rate(&m_outputMap[output]);
  
  switch(d)
    {
    case 1:
      *div = STM_CLK_DIV_1;
      break;
    case 2:
      *div = STM_CLK_DIV_2;
      break;
    case 4:
      *div = STM_CLK_DIV_4;
      break;
    case 8:
      *div = STM_CLK_DIV_8;
      break;
    default:
      return false; // Impossible but keep the compiler quiet.
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmClockLLA::getSource(stm_clk_divider_output_name_t    name,
                               stm_clk_divider_output_source_t *src) const
{
  uint32_t output;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "Clock %d NOT FOUND!!", name );
    return false;
  }

  struct vibe_clk *parent = vibe_os_clk_get_parent(&m_outputMap[output]);

  *src = static_cast<stm_clk_divider_output_source_t>(parent->id);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


int CSTmClockLLA::Suspend(void)
{
  uint32_t i;

  if(m_bIsSuspended)
    return 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* Suspend OUT */
  for(i=0; i<m_nOutMapSize; i++)
    vibe_os_clk_suspend(&m_outputMap[i]);

  /* Suspend SRC : This will be done only for disabled sources */
  for(i=0; i<m_nSrcMapSize; i++)
    vibe_os_clk_suspend(&m_sourceMap[i]);

  m_bIsSuspended = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return 0;
}


int CSTmClockLLA::Resume(void)
{
  uint32_t i;

  if(!m_bIsSuspended)
    return 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* Resume SRC : This will be done only for disabled sources */
  for(i=0; i<m_nSrcMapSize; i++)
    vibe_os_clk_resume(&m_sourceMap[i]);

  /* Resume OUT */
  for(i=0; i<m_nOutMapSize; i++)
    vibe_os_clk_resume(&m_outputMap[i]);

  m_bIsSuspended = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return 0;
}
