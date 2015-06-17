/***********************************************************************
 *
 * File: display/ip/panel/stmpaneloutput.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/DisplayMixer.h>

#include <display/ip/displaytiming/stmvtg.h>
#include "stmpaneloutput.h"
#include "stmodp.h"
#include "stmodpdtg.h"
#include "stmdptx.h"

static stm_display_mode_t CustomMode =
{
    STM_TIMING_MODE_CUSTOM,
    { 60000, STM_PROGRESSIVE_SCAN, 1920, 1080, 112, 20,
    (STM_OUTPUT_STD_UNDEFINED), {{0,0},{0,0}}, {0,0}, STM_MODE_FLAGS_NONE },
    { 2200, 1125, 148500000, STM_SYNC_NEGATIVE, 8, STM_SYNC_NEGATIVE, 6}
};

CSTmPanelOutput::CSTmPanelOutput(
    const char                  *name,
    uint32_t                    id,
    CDisplayDevice              *pDev,
    CSTmODP                     *pODP,
    CSTmDTGOdp                     *pVTG1,
    CSTmDPTx                    *pDPTx,
    CDisplayMixer               *pMixer): CSTmMasterOutput(name, id, pDev, 0, pVTG1, pMixer)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    m_ulCapabilities |= (OUTPUT_CAPS_DISPLAY_TIMING_MASTER);

    m_pODP = pODP;
    m_pDPTx = pDPTx;
    m_pDTG = pVTG1;

    TRC( TRC_ID_MAIN_INFO, "- m_pVTG          = %p", m_pVTG );
    TRC( TRC_ID_MAIN_INFO, "- m_pMixer        = %p", m_pMixer );
    TRC( TRC_ID_MAIN_INFO, "- m_pDPTx        = %p", m_pDPTx );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CSTmPanelOutput::~CSTmPanelOutput() {}

const stm_display_mode_t* CSTmPanelOutput::SupportedMode(const stm_display_mode_t *mode) const
{
    switch(mode->mode_id)
    {
        case STM_TIMING_MODE_CUSTOM:
                return mode;
        break;

        default:
        break;
    }

    return 0;
}

bool CSTmPanelOutput::SetOutputFormat(uint32_t format)
{
    TRC( TRC_ID_MAIN_INFO, "- format = 0x%x", format );

    if(!m_bIsStarted)
    {
        TRC( TRC_ID_MAIN_INFO, "- No display mode, nothing to do" );
        return true;
    }

    if((format & (STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_YC)) && (!m_bUsingDENC))
    {
        TRC( TRC_ID_ERROR, "DENC is not used by this Output !!" );
        return false;
    }

    return true;
}

uint32_t CSTmPanelOutput::SetControl(stm_output_control_t ctrl, uint32_t val)
{
    TRC( TRC_ID_MAIN_INFO, "ctrl = %d val = %u", ctrl, val );

    return STM_OUT_OK;
}


uint32_t CSTmPanelOutput::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
    CSTmMasterOutput::GetControl(ctrl, val);

    return STM_OUT_OK;
}


OutputResults CSTmPanelOutput::Start(const stm_display_mode_t *mode)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if(mode==NULL)
        return STM_OUT_INVALID_VALUE;

    CustomMode = *(stm_display_mode_t*)mode;

    CustomMode.mode_id = STM_TIMING_MODE_CUSTOM;

    /* STM_TIMING_MODE_RESERVED is to save CustomMode table */
    if(mode->mode_id != STM_TIMING_MODE_RESERVED)
    {
        OutputResults retval=CSTmMasterOutput::Start(mode);

        if(retval != STM_OUT_OK)
        {
            TRC( TRC_ID_ERROR, "- failed to start master output" );
            return retval;
        }

        if(m_ulCapabilities & OUTPUT_CAPS_DISPLAYPORT)
           m_pDPTx->Start(mode);
    }
    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return STM_OUT_OK;
}


bool CSTmPanelOutput::Stop(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /*
    * Lock out interrupt processing while we test and shutdown the mixer, to
    * prevent a plane's vsync update processing enabling itself on
    * the mixer between the test and the Stop.
    */
    vibe_os_lock_resource(m_lock);

    if(m_pMixer->HasEnabledPlanes())
    {
        TRC( TRC_ID_ERROR, "mixer has enabled planes" );
        vibe_os_unlock_resource(m_lock);
        return false;
    }

    /* Disable all planes on the mixer/ */
    m_pMixer->Stop();

    vibe_os_unlock_resource(m_lock);

    if(m_bIsStarted)
    {
        m_pVTG->Stop();

        vibe_os_lock_resource(m_lock);
        {
            m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
            COutput::Stop();
        }
        vibe_os_unlock_resource(m_lock);
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;
}

const stm_display_mode_t* CSTmPanelOutput::FindMode(uint32_t uXRes, uint32_t uYRes,
uint32_t uMinLines, uint32_t uMinPixels, uint32_t uPixClock, stm_scan_type_t scanType) const
{
    /* Supporting only custom mode for panel */
    return &CustomMode;
}

const stm_display_mode_t* CSTmPanelOutput::GetModeParamsLine (
    stm_display_mode_id_t mode) const
{
    /* Supporting only custom mode for panel */
    return &CustomMode;
}

uint32_t CSTmPanelOutput::SetCompoundControl(
    stm_output_control_t ctrl, void *newVal)
{
    switch (ctrl)
    {
        case OUTPUT_CTRL_PANEL_CONFIGURE:
        {
            /*
                ODP configuration:
                 color_config, dither_type, panel power timing
                DTG configuration:
                 color_config, dither_type, panel power timing
                misc_timing_config
                LVDS configuration:
                 Spread spectrum, lvds config, secondary stream
             */
            stm_display_panel_config_t display_config;
            display_config = *(stm_display_panel_config_t*)newVal;

            if(display_config.panel_power_up_timing.pwr_to_de_delay_during_power_on || \
            display_config.panel_power_up_timing.de_to_bklt_on_delay_during_power_on || \
            display_config.panel_power_up_timing.bklt_to_de_off_delay_during_power_off || \
            display_config.panel_power_up_timing.de_to_pwr_delay_during_power_off)
            {
                m_pODP->SetPowerTiming(&display_config.panel_power_up_timing);
            }
            m_pDTG->SetInputLockScheme(display_config.misc_timing_config);
        }
        break;

        default:
            TRC( TRC_ID_ERROR, "CSTmPanelOutput::SetCompoundControl Invalid Control Option %d", ctrl );
        return STM_OUT_NO_CTRL;
    }

    return STM_OUT_OK;
}
