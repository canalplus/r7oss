/***********************************************************************
 *
 * File: pixel_capture/ip/gamma/stmgammacapture.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_COMPOSITOR_CAPTURE_H
#define _GAMMA_COMPOSITOR_CAPTURE_H


#include <pixel_capture/generic/CaptureDevice.h>
#include <pixel_capture/generic/Capture.h>
#include <pixel_capture/generic/Queue.h>

///////////////////////////////////////////////////////////////////////////////
// Base class for compositor hardware capture


typedef struct GammaCaptureSetup_s
{
  uint32_t CTL;
  uint32_t CWO;
  uint32_t CWS;
  uint32_t VTP;
  uint32_t VBP;
  uint32_t PMP;
  uint32_t CMW;

  /* Resize setup */
  uint32_t HSRC;
  uint32_t VSRC;

  /* Filters setup */
  uint32_t HFCn;
  uint32_t VFCn;

  /* Plugs setup */
  uint32_t PAS;
  uint32_t MAOS;
  uint32_t MACS;
  uint32_t MAMS;

  /* internal buffer address to be used for notification */
  uint32_t buffer_address;
  volatile uint32_t buffer_processed;
} GammaCaptureSetup_t;


class CGammaCompositorCAP: public CCapture
{
  public:
    CGammaCompositorCAP(const char     *name,
                        uint32_t        id,
                        uint32_t        base_address,
                        uint32_t        size,
                        const CPixelCaptureDevice *pDev,
                        const stm_pixel_capture_capabilities_flags_t caps);

    ~CGammaCompositorCAP();

    virtual bool  Create(void);

    virtual int   ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer);

    virtual void  CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);

    virtual bool  PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer,
                                               CaptureQueueBufferInfo             &qbinfo);

    int           Start(void);
    int           Stop(void);

    void          RevokeCaptureBufferAccess(capture_node_h pNode);
  protected:
    uint32_t            m_ulCAPBaseAddress;

    DMA_Area m_HFilter;
    DMA_Area m_VFilter;

    void ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);
    void writeFieldSetup(const GammaCaptureSetup_t *setup, bool isTopField);

    GammaCaptureSetup_t * PrepareCaptureSetup(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo);
    void                  ConfigureCaptureWindowSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          GammaCaptureSetup_t                  *pCaptureSetup);
    void                  ConfigureCaptureBufferSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          GammaCaptureSetup_t                  *pCaptureSetup);
    void                  ConfigureCaptureInput(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          GammaCaptureSetup_t                  *pCaptureSetup);
    bool                  ConfigureCaptureResizeAndFilters(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          GammaCaptureSetup_t                  *pCaptureSetup);
    bool                  ConfigureCaptureColourFmt(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          GammaCaptureSetup_t                  *pCaptureSetup);
    void                  Flush(bool bFlushCurrentNode);

    int                   NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime);

  private:
    void WriteCaptureReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, reg, val); }
    uint32_t ReadCaptureReg(uint32_t reg) { return vibe_os_read_register(m_pReg, reg); }

    CGammaCompositorCAP(const CGammaCompositorCAP&);
    CGammaCompositorCAP& operator=(const CGammaCompositorCAP&);
};

/*
 * The following routine is generic helper for ST Gamma Capture devices.
 * It calculate a viewport line number , suitable for programming the hardware;
 * taking into account the video mode's VBI region and horizontal front porch.
 * This is needed by a number of different hardware blocks spanning various
 * devices, so we want the calculation in one place so everything is consistent
 * and the code is more readable.
 *
 * The value is clamped to sensible maximum value based on the input display
 *  mode.
 */

static inline uint32_t CaptureCalculateWindowLine(const stm_pixel_capture_input_params_t &InputParams, int y)
{
    /*
     * Video frame line numbers start at 1, y starts at 0 as in a standard
     * graphics coordinate system. In interlaced modes the start line is the
     * field line number of the odd field, but y is still defined as a
     * progressive frame.
     *
     * Note that y can be negative to place a viewport before
     * the active video area (i.e. in the VBI).
     */
    int adjust  = (InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED)?2:1;

    int line    = (InputParams.active_window.y*adjust) + y;
    int maxline = (InputParams.active_window.y*adjust) + (InputParams.active_window.height - 1);

    if(line < 1)       line = adjust;
    if(line > maxline) line = maxline;

    return line;
}

#endif // _GAMMA_COMPOSITOR_CAP_H
