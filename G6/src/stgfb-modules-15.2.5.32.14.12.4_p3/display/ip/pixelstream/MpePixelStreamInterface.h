/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2013-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef _MPE_PIXEL_STREAM_INTERFACE_H
#define _MPE_PIXEL_STREAM_INTERFACE_H

#include <display/generic/PixelStreamInterface.h>

class CDisplayDevice;

typedef struct PixelStreamIrqInfo_s
{
    uint32_t EnableRegOffset;
    uint32_t StatusRegOffset;
    uint32_t EnableMask;
    uint32_t StatusMask;
} PixelStreamIrqInfo_t;

typedef enum
{
    PIX_HDMI = 0,
    PIX_DPRX = 1,
    PIX_VXI = 2,
    PIX_AIP = 3,
} PixelStream_Type_t;

typedef enum
{
    PIX_AIP_HD_GFX = 0,
    PIX_AIP_SD = 1,
    PIX_AIP_STG = 2,
} PixelStreamAIP_Instance_t;

///////////////////////////////////////////////////////////////////////////////
// Base class for pixel stream interface

class CMpePixelStreamInterface: public CPixelStreamInterface
{
public:
    CMpePixelStreamInterface ( uint32_t interfaceID, CDisplaySource * pSource, PixelStreamIrqInfo_t * IrqInfo, int32_t hwId, uint16_t sourceType, uint16_t sourceInstance);

    virtual ~CMpePixelStreamInterface();

    virtual bool        Create(void);
    virtual int         Release(stm_display_source_pixelstream_h p);
    virtual int         SetInputParams(stm_display_source_pixelstream_h p,
                                       const stm_display_source_pixelstream_params_t * params);

    virtual uint32_t    GetTimingID(void) const { return (uint32_t)m_interfaceHwId;}

    bool                IsConnectionPossible(CDisplayPlane *pPlane) const;
    int32_t             ConnectSource(CDisplayPlane *pPlane);
    bool                DisconnectSource(CDisplayPlane *pPlane);
    bool                Flush(bool bFlushCurrentNode,
                              const void * const user,
                              bool bCheckUser);
    stm_display_timing_events_t GetInterruptStatus(void);

    void                SourceVSyncUpdateHW (bool                   isInterlaced,
                                             bool                   isTopVSync,
                                             const stm_time64_t    &vsyncTime);

    // Set status of the hardware which is controlled by this interface
    bool                SetInterfaceStatus(SourceInterfaceStatus status);

    void Suspend(void);
    void Resume(void);

protected:
   /* m_display_buffer describes the format of the pixel stream source */
    stm_display_buffer_t      m_display_buffer;
    CDisplayNode              m_displayNode;
    PixelStreamIrqInfo_t     *m_pPixelStreamIrqInfo;

    uint32_t*                 m_pDevRegs;

    void                      EnableInterrupts(void);
    void                      DisableInterrupts(void);

    uint32_t                  ReadInterruptReg(uint32_t addr) const {
                                return vibe_os_read_register(m_pDevRegs, addr);
                              }
    void                      WriteInterruptReg(uint32_t addr, uint32_t val) { vibe_os_write_register(m_pDevRegs, addr, val); }

    void                      SetInterruptReg(uint32_t addr, uint32_t bitToMask) {
                                WriteInterruptReg(addr, (ReadInterruptReg(addr) | bitToMask));
                              }
    void                      ClearInterruptReg(uint32_t addr, uint32_t bitToClear) {
                                WriteInterruptReg(addr, (ReadInterruptReg(addr) & ~bitToClear));
                              }
};

#endif /* _MPE_PIXEL_STREAM_INTERFACE_H */


/* end of file */
