/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
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

#ifndef _PIXELSTREAMINTERFACE_H
#define _PIXELSTREAMINTERFACE_H

#include "DisplayDevice.h"
#include "DisplayPlane.h"
#include "DisplaySource.h"
#include "SourceInterface.h"


class CPixelStreamInterface: public CSourceInterface
{
public:
    // Construction/Destruction
    CPixelStreamInterface ( uint32_t interfaceID, CDisplaySource * pSource );

    virtual ~CPixelStreamInterface(void);
    bool     Create(void);

    virtual uint32_t                    GetTimingID(void) const = 0;

    virtual void                        UpdateOutputInfo(CDisplayPlane *pPlane) {};

    // PixelStream Interface
    virtual int     Release(stm_display_source_pixelstream_h p) = 0;
    virtual int     SetInputParams(stm_display_source_pixelstream_h p,const stm_display_source_pixelstream_params_t * params) = 0;
    virtual int32_t ConnectSource(CDisplayPlane *pPlane) = 0;
    virtual bool    DisconnectSource(CDisplayPlane *pPlane) = 0;
    int             GetParams(stm_display_pixel_stream_params_t* param) const;

    // Interface Pixel Stream Identification
    uint32_t                            GetStatus(void)             const { return 0; }
    stm_display_source_interfaces_t     GetInterfaceType(void)      const { return STM_SOURCE_PIXELSTREAM_IFACE; }

    // Base class interface
    virtual bool     IsConnectionPossible(CDisplayPlane *pPlane)    const = 0;

    virtual uint32_t GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const {return STM_SRC_NO_CTRL;}
    virtual uint32_t SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val) {return STM_SRC_NO_CTRL;}

protected:
    stm_display_pixel_stream_params_t m_pixel_stream_params;
};

#endif /* _PIXELSTREAMINTERFACE_H */
