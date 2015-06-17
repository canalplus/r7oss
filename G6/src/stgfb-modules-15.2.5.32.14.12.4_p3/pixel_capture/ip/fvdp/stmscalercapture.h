/***********************************************************************
 *
 * File: pixel_capture/ip/fvdp/stmscalercapture.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _FVDP_SCALER_CAPTURE_H
#define _FVDP_SCALER_CAPTURE_H


#include <pixel_capture/generic/CaptureDevice.h>
#include <pixel_capture/generic/Capture.h>

///////////////////////////////////////////////////////////////////////////////
// Base class for fvdp scaler hardware capture


class CFvdpScalerCAP: public CCapture
{
  public:
    CFvdpScalerCAP(const char     *name,
                        uint32_t        id,
                        const CPixelCaptureDevice *pDev,
                        const stm_pixel_capture_capabilities_flags_t caps);

    ~CFvdpScalerCAP();

    bool Create(void);
    virtual bool  PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer,
                                               CaptureQueueBufferInfo             &qbinfo);

    int  ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer);
    void CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);
    void ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);
    void Flush(bool bFlushCurrentNode);

  private:
    CFvdpScalerCAP(const CFvdpScalerCAP&);
    CFvdpScalerCAP& operator=(const CFvdpScalerCAP&);
};


#endif // _FVDP_SCALER_CAPTURE_H
