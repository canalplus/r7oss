/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmiphy.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMIPHY_H
#define _STMHDMIPHY_H


class CSTmHDMIPhy
{
public:
  CSTmHDMIPhy(): m_pPHYConfig(0) {}
  virtual ~CSTmHDMIPhy(void) {};

  virtual bool Start(const stm_display_mode_t* , uint32_t outputFormat) = 0;
  virtual void Stop(void)    = 0;

  void SetPhyConfig(stm_display_hdmi_phy_config_t *config) { m_pPHYConfig = config; }

protected:
  stm_display_hdmi_phy_config_t *m_pPHYConfig;

private:
  CSTmHDMIPhy(const CSTmHDMIPhy&);
  CSTmHDMIPhy& operator=(const CSTmHDMIPhy&);

};

#endif //_STMHDMIPHY_H
