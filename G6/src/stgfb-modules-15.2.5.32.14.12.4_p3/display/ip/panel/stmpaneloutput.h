/***********************************************************************
 *
 * File: display/ip/panel/stmpaneloutput.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_PANELOUTPUT_H
#define _STM_PANELOUTPUT_H

#include <display/ip/stmmasteroutput.h>

class CSTmVTG;
class CSTmODP;
class CSTmDPTx;
class CSTmDTGOdp;

class CSTmPanelOutput: public CSTmMasterOutput
{
public:
  CSTmPanelOutput(const char *name,
                   uint32_t id,
                   CDisplayDevice *,
                   CSTmODP *,
                   CSTmDTGOdp *,
                   CSTmDPTx *,
                   CDisplayMixer *);

    virtual ~CSTmPanelOutput();

    bool  Start(const stm_display_mode_t*, uint32_t tvStandard);
    OutputResults Start(const stm_display_mode_t *mode);
    bool Stop(void);
    uint32_t SetCompoundControl(stm_output_control_t, void *newVal);


    const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;

    uint32_t SetControl(stm_output_control_t, uint32_t newVal);
    uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
    const stm_display_mode_t* GetModeParamsLine(stm_display_mode_id_t mode) const;
    const stm_display_mode_t* FindMode(uint32_t uXRes, uint32_t uYRes, uint32_t uMinLines, uint32_t uMinPixels, uint32_t uPixClock, stm_scan_type_t) const;

protected:
    CSTmODP              *m_pODP;
    CSTmDPTx             *m_pDPTx;
    CSTmDTGOdp           *m_pDTG;

    virtual bool SetOutputFormat(uint32_t format);

    virtual void EnableDACs(void)  = 0;
    virtual void DisableDACs(void) = 0;
private:
    CSTmPanelOutput(const CSTmPanelOutput&);
    CSTmPanelOutput& operator=(const CSTmPanelOutput&);
};

#endif //_STM_PANELOUTPUT_H
