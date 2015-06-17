/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407vdp.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STIH407_VDP_H
#define _STIH407_VDP_H

#include <vibe_debug.h>
#include <display/ip/vdp/VdpPlane.h>

class CVideoPlug;

class CSTiH407VDP: public CVdpPlane
{
public:
  CSTiH407VDP(const CDisplayDevice *pDev,
              CVideoPlug *pVideoPlug): CVdpPlane("Main-PIP",
                                                 STiH407_PLANE_IDX_VID_PIP,
                                                 pDev,
                                                 (stm_plane_capabilities_t)(PLANE_CAPS_VIDEO|PLANE_CAPS_PRIMARY_OUTPUT|PLANE_CAPS_SECONDARY_OUTPUT),
                                                 pVideoPlug,
                                                 STiH407_VDP_BASE)
  {
    m_pDevReg = (uint32_t*)pDev->GetCtrlRegisterBase();
    m_MBChunkSize = 3;
    m_PlanarChunkSize = 3;
    m_RasterChunkSize = 3;
    m_RasterOpcodeSize = 32;
  }

  // The SYS CONF register SYSTEM_CONFIG5073 allows to bring the Main VTG synchros or the Aux VTG synchros to the VDP plane.
  // It should be programmed every times the connections between VDP and Output change
  //
  // WARNING: "SYSTEM_CONFIG5073" is NOT double buffered on VSync. It has an immediate effect
  DisplayPlaneResults ConnectToOutput(COutput* pOutput)
  {
    DisplayPlaneResults res;
    uint32_t value;

    // First, perfom the connection
    res = CVdpPlane::ConnectToOutput(pOutput);

    // If the connection was successful, program SYSTEM_CONFIG5073
    if (res == STM_PLANE_OK)
    {
        // Retrieve info from the output to know which VTG is used by this output
        if(pOutput->GetControl(OUTPUT_CTRL_VIDEO_SOURCE_SELECT, &value) == STM_OUT_OK)
        {
            if((value == STM_VIDEO_SOURCE_MAIN_COMPOSITOR)||(value == STM_VIDEO_SOURCE_MAIN_COMPOSITOR_BYPASS))
            {
                // VDP connected to Main Mixer. Bring synchros from Main VTG
                TRC(TRC_ID_MAIN_INFO, "VDP receives synchro from Main VTG");
                uint32_t val = ReadSysCfgReg(SYS_CFG5073);
                val |= SYS_CFG5073_DUAL_SCART_EN;
                WriteSysCfgReg(SYS_CFG5073, val);
            }
            else
            {
                // VDP connected to Aux Mixer. Bring synchros from Aux VTG
                TRC(TRC_ID_MAIN_INFO, "VDP receives synchro from Aux VTG");
                uint32_t val = ReadSysCfgReg(SYS_CFG5073);
                val &= ~SYS_CFG5073_DUAL_SCART_EN;
                WriteSysCfgReg(SYS_CFG5073, val);
            }
        }
        else
        {
            TRC( TRC_ID_ERROR, "Failed to retrieve the used VTG!" );
            // Synchro is not updated in that case

            // Cancel what has already been done
            CVdpPlane::DisconnectFromOutput(pOutput);

            res = STM_PLANE_CANNOT_CONNECT;
        }
    }

    return res;
  }

private:
  uint32_t *m_pDevReg;
  void     WriteSysCfgReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevReg, (STiH407_SYSCFG_CORE + reg), val); }
  uint32_t ReadSysCfgReg(uint32_t reg)                { return vibe_os_read_register(m_pDevReg, (STiH407_SYSCFG_CORE +reg)); }

  CSTiH407VDP (const CSTiH407VDP &);
  CSTiH407VDP& operator= (const CSTiH407VDP &);
};

#endif // _STIH407_VDP_H
