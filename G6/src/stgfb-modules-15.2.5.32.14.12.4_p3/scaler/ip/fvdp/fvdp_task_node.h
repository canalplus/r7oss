/***********************************************************************
 *
 * File: scaler/ip/fvdp/fvdp_task_node.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_TASK_NODE_H
#define FVDP_TASK_NODE_H

#ifdef __cplusplus

#include <stm_scaler.h>
#include "scaling_task_node.h"
#include "fvdp.h"

class CFvdpTaskNode : public CScalingTaskNode
{
public:
    CFvdpTaskNode() : CScalingTaskNode()
      {
        m_TnrState = FVDP_PSI_OFF;
        vibe_os_zero_memory( &m_InputVideoInfo, sizeof( m_InputVideoInfo ));
        vibe_os_zero_memory( &m_InputCropWindow, sizeof( m_InputCropWindow ));
        vibe_os_zero_memory( &m_OutputVideoInfo, sizeof( m_OutputVideoInfo ));
        vibe_os_zero_memory( &m_OutputActiveWindow, sizeof( m_OutputActiveWindow ));
        m_InputLumaAddr = 0;
        m_InputChromaOffset = 0;
        m_OutputLumaAddr = 0;
        m_OutputChromaOffset = 0;
      }

    virtual bool        PrepareAndCheckScaling(const stm_scaler_task_descr_t *task_descr);

    FVDP_PSIState_t     m_TnrState;           // TNR On/Off state

    FVDP_VideoInfo_t    m_InputVideoInfo;     // Input video info      (input frame features)
    FVDP_CropWindow_t   m_InputCropWindow;    // Input cropping window

    FVDP_VideoInfo_t    m_OutputVideoInfo;    // Ouput video info      (ouput frame features)
    FVDP_OutputWindow_t m_OutputActiveWindow; // Output active window  (black border around)

    uint32_t            m_InputLumaAddr;      // input luma address
    uint32_t            m_InputChromaOffset;  // input chroma offset
    uint32_t            m_OutputLumaAddr;     // output luma address
    uint32_t            m_OutputChromaOffset; // output chroma offset

protected:
    bool                PrepareAndCheckScalerFlags(const stm_scaler_task_descr_t *task_descr);
    bool                PrepareAndCheckInputVideoInfo(const stm_scaler_task_descr_t *task_descr);
    bool                PrepareAndCheckInputCroppingWindow(const stm_scaler_task_descr_t *task_descr,
                                                           area_config_t                 *area_config);
    bool                AdjustInputCropWindowForHWConstraints(void);

    bool                PrepareAndCheckOutputVideoInfo(const stm_scaler_task_descr_t *task_descr);
    bool                PrepareAndCheckOutputActiveWindow(const stm_scaler_task_descr_t *task_descr,
                                                          area_config_t                 *area_config);
    bool                AdjustOutputActiveWindowForHWConstraints(void);

}; /* CFvdpTaskNode */

#endif /* __cplusplus */

#endif /* FVDP_TASK_NODE_H */

