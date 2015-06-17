/***********************************************************************
 *
 * File: pixel_capture/ip/dvp/stmdvpcapture.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _DVP_IP_CAPTURE_H
#define _DVP_IP_CAPTURE_H


#include <pixel_capture/generic/CaptureDevice.h>
#include <pixel_capture/generic/Capture.h>
#include <pixel_capture/generic/Queue.h>

///////////////////////////////////////////////////////////////////////////////
// Base class for dvp hardware capture

#define STM_DVP_DLL_OFFSET         0x0                                      // OK
#define STM_DVP_MATRIX_OFFSET      0x100                                    // OK
#define STM_DVP_CAPTURE_OFFSET     0x200                                    // OK
#define STM_DVP_MISC_OFFSET        0x300                                    // OK
#define STM_DVP_SYNCHRO_OFFSET     0x400                                    // OK

typedef enum DvpMatrixMode_e
{
  DVP_MATRIX_YUV_TO_YUV,
  DVP_MATRIX_YUV_TO_RGB,
  DVP_MATRIX_RGB_TO_YUV,
  DVP_MATRIX_RGB_TO_RGB,

  /* TODO : support reduced and full range? */

  DVP_MATRIX_INVALID_MODE,  // keeps this last one
} DvpMatrixMode_t;

typedef struct DvpCaptureSetup_s
{
  uint32_t CTL;
  uint32_t VLP;
  uint32_t VCP;
  uint32_t PMP;
  uint32_t CMW;

  /* Resize setup */
  uint32_t HSRC_L;
  uint32_t HSRC_C;
  uint32_t VSRC;

  /* Filters setup */
  uint32_t HFCn_L;
  uint32_t HFCn_C;
  uint32_t VFCn;

  /* Plugs setup for Chroma */
  uint32_t PAS2;
  uint32_t MAOS2;
  uint32_t MACS2;
  uint32_t MAMS2;

  /* Plugs setup for Luma */
  uint32_t PAS;
  uint32_t MAOS;
  uint32_t MACS;
  uint32_t MAMS;

  /* internal buffer address to be used for notification */
  uint32_t buffer_address;
  volatile uint32_t buffer_processed;
} DvpCaptureSetup_t;


class CDvpCAP: public CCapture
{
  public:
    CDvpCAP(const char     *name,
                        uint32_t        id,
                        uint32_t        base_address,
                        uint32_t        size,
                        const CPixelCaptureDevice *pDev,
                        const stm_pixel_capture_capabilities_flags_t caps,
                        stm_pixel_capture_hw_features_t hw_features);

    ~CDvpCAP();

    virtual bool  Create(void);
    virtual int   GetFormats(const stm_pixel_capture_format_t** pFormats) const;
    virtual bool  CheckInputWindow(stm_pixel_capture_rect_t input_window);
    virtual int   SetInputParams(stm_pixel_capture_input_params_t params);

    virtual int   ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer);

    virtual void  CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);

    virtual bool  PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer,
                                               CaptureQueueBufferInfo             &qbinfo);

    void          AdjustBufferInfoForScaling(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                               CaptureQueueBufferInfo              &qbinfo) const;

    int           Start(void);
    int           Stop(void);

    int           HandleInterrupts(uint32_t *TimingEvent);

    void          RevokeCaptureBufferAccess(capture_node_h pNode);
  protected:
    uint32_t            m_ulDVPBaseAddress;
    uint32_t            m_InterruptMask;
    bool                m_InputParamsAreValid;

    DMA_Area m_HFilter;
    DMA_Area m_VFilter;
    DMA_Area m_CMatrix;

    void ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);

    void writeFieldSetup(const DvpCaptureSetup_t *setup, bool isTopField);

    DvpCaptureSetup_t   * PrepareCaptureSetup(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo);
    void                  ConfigureCaptureWindowSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          DvpCaptureSetup_t                    *pCaptureSetup);
    void                  ConfigureCaptureBufferSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          DvpCaptureSetup_t                    *pCaptureSetup);
    void                  ConfigureCaptureInput(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          DvpCaptureSetup_t                    *pCaptureSetup);
    bool                  ConfigureCaptureResizeAndFilters(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          DvpCaptureSetup_t                    *pCaptureSetup);
    bool                  ConfigureCaptureColourFmt(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo,
                                                          DvpCaptureSetup_t                    *pCaptureSetup);
    void                  Flush(bool bFlushCurrentNode);

    int                   NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime);

    int                   EnableInterrupts(void);
    int                   DisableInterrupts(void);
    uint32_t              GetInterruptStatus(void);

  private:
    /* Setup MATRIX registers */
    DvpMatrixMode_t       m_MatrixMode;             // If chnaged then Matrix regs will need to be updated.
    uint32_t              m_PendingEvent;           // Used to catch EOCAP event during the current VSync event

    stm_pixel_capture_hw_features_t m_DvpHardwareFeatures;  // DVP hardware features used to know about capabilities

    void                  SetMatrix(uint8_t inputisrgb,
                                          uint8_t outputisrgb);

  /* Program DVP Capture PLUGS */
    void                  SetPlugParams(bool LumaNotChroma,
                                          uint8_t PageSize,
                                          uint8_t MaxOpcodeSize,
                                          uint8_t MaxChunkSize,
                                          uint8_t MaxMessageSize,
                                          uint8_t WritePosting,
                                          uint8_t MinSpaceBetweenReq);

    void                  ProgramSynchroConfiguration(void);

    /* Read/Write Macro for DVP CAPTURE block */
    void WriteCaptureReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, (reg+STM_DVP_CAPTURE_OFFSET), val); }
    uint32_t ReadCaptureReg(uint32_t reg) { return vibe_os_read_register(m_pReg, (reg+STM_DVP_CAPTURE_OFFSET)); }

    /* Read/Write Macro for DVP MISC block */
    void WriteMiscReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, (reg+STM_DVP_MISC_OFFSET), val); }
    uint32_t ReadMiscReg(uint32_t reg) { return vibe_os_read_register(m_pReg, (reg+STM_DVP_MISC_OFFSET)); }

    /* Read/Write Macro for DVP SYNCHRO block */
    void WriteSynchroReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, (reg+STM_DVP_SYNCHRO_OFFSET), val); }
    uint32_t ReadSynchroReg(uint32_t reg) { return vibe_os_read_register(m_pReg, (reg+STM_DVP_SYNCHRO_OFFSET)); }

    /* Read Macro for DVP DLL block */
    uint32_t ReadDLLReg(uint32_t reg) { return vibe_os_read_register(m_pReg, (reg+STM_DVP_DLL_OFFSET)); }

    /* Write Macro for DVP MATRIX block */
    void WriteMatrixReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, (reg+STM_DVP_MATRIX_OFFSET), val); }
    uint32_t ReadMatrixReg(uint32_t reg) { return vibe_os_read_register(m_pReg, (reg+STM_DVP_MATRIX_OFFSET)); }

    CDvpCAP(const CDvpCAP&);
    CDvpCAP& operator=(const CDvpCAP&);
};


#endif // _DVP_IP_CAPTURE_H
