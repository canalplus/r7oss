/***********************************************************************
 *
 * File: pixel_capture/ip/dvp/stmdvpcapture.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display_types.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "dvp_defs.h"
#include "dvp_regs.h"

#include "stmdvpcapture.h"
#include "dvp_filters.h"

#define CAP_HFILTER_TABLE_SIZE (NB_HSRC_FILTERS * (NB_HSRC_COEFFS + HFILTERS_ALIGN))
#define CAP_VFILTER_TABLE_SIZE (NB_VSRC_FILTERS * (NB_VSRC_COEFFS + VFILTERS_ALIGN))
#define CAP_MATRIX_TABLE_SIZE  (NB_MATRIX_CONVERSION * MATRIX_ENTRY_SIZE)

//#define CAPTURE_DEBUG_REGISTERS

/* Don't allocate memory for Matrix programming */
//#define CAPTURE_DVP_USE_STATIC_MATRIX

/*
 * Timout value is calculated as below :
 * timeout = 2 * max Capture duration
 *
 * 2 for interlaced display latancy
 * max Capture duration = 40ms
 */
#define CAPTURE_RELEASE_WAIT_TIMEOUT      1000

/* Timing event mask */
#define CAPTURE_DVP_FIELD_POL_SHIFT       16

/* WA fixing issue with interlaced mode */
//#define HDMIRX_INTERLACE_MODE_WA

static const stm_pixel_capture_input_s stm_pixel_capture_inputs[] = {
  {"stm_input_hdmirx",      MUX_CTL_SOURCE_HDMI_RX    , 0},
  {"stm_input_vxi",         MUX_CTL_SOURCE_VXI        , 0},
};

static const stm_pixel_capture_format_t c_surfaceFormats[] = {
  STM_PIXEL_FORMAT_YUV_8B8B8B_SP,
  STM_PIXEL_FORMAT_RGB_8B8B8B_SP,
  STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP,
  STM_PIXEL_FORMAT_RGB_10B10B10B_SP,
  STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP,
  STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP,
};

static stm_pixel_capture_format_t c_Formats[STM_PIXEL_FORMAT_COUNT];

#if defined(CAPTURE_DEBUG_REGISTERS)
static uint32_t ctx=0;
#endif

CDvpCAP::CDvpCAP(const char     *name,
                                         uint32_t        id,
                                         uint32_t        base_address,
                                         uint32_t        size,
                                         const CPixelCaptureDevice *pDev,
                                         const stm_pixel_capture_capabilities_flags_t caps,
                                         stm_pixel_capture_hw_features_t hw_features):CCapture(name, id, pDev, caps)
{
  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::CDvpCAP %s id = %u", name, id );

  m_pSurfaceFormats = c_surfaceFormats;
  m_nFormats = N_ELEMENTS (c_surfaceFormats);

  m_ulDVPBaseAddress       = base_address;

  m_pCaptureInputs         = stm_pixel_capture_inputs;
  m_pCurrentInput          = m_pCaptureInputs[0];
  m_numInputs              = N_ELEMENTS(stm_pixel_capture_inputs);

  m_MatrixMode             = DVP_MATRIX_INVALID_MODE;
  m_PendingEvent           = 0;
  m_DvpHardwareFeatures    = hw_features;

  m_InputParamsAreValid    = false;

  /*
   * Override scaling capabilities this for DVP Capture hw.
   */
  m_ulMaxHSrcInc = m_fixedpointONE*8; // downscale to 1/8
  m_ulMaxVSrcInc = m_fixedpointONE*8; // downscale to 1/8
  m_ulMinHSrcInc = m_fixedpointONE;   // upscale not supported by compo capture
  m_ulMinVSrcInc = m_fixedpointONE;   // upscale not supported by compo capture

  /* No Skip Line support for Compositor Capture */
  m_ulMaxLineStep = 1;

  m_HFilter      = (DMA_Area) { 0 };
  m_VFilter      = (DMA_Area) { 0 };
  m_CMatrix      = (DMA_Area) { 0 };

  m_pReg = (uint32_t*)vibe_os_map_memory(m_ulDVPBaseAddress, size);
  ASSERTF(m_pReg,("CDvpCAP::CDvpCAP 'unable to map CAPTURE device registers'\n"));

  m_InterruptMask = (MISC_VSYNC_MASK|MISC_EOLOCK_MASK);

  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::CDvpCAP: Register remapping 0x%08x -> 0x%08x", m_ulDVPBaseAddress,(uint32_t)m_pReg );

  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::CDvpCAP out" );
}


CDvpCAP::~CDvpCAP(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  this->Stop();

  vibe_os_free_dma_area(&m_HFilter);
  vibe_os_free_dma_area(&m_VFilter);
  vibe_os_free_dma_area(&m_CMatrix);

  vibe_os_unmap_memory(m_pReg);
  m_pReg = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CDvpCAP::SetPlugParams(bool LumaNotChroma,
                          uint8_t PageSize,
                          uint8_t MaxOpcodeSize,
                          uint8_t MaxChunkSize,
                          uint8_t MaxMessageSize,
                          uint8_t WritePosting,
                          uint8_t MinSpaceBetweenReq)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if (LumaNotChroma) {
    WriteCaptureReg(DVP_CAPn_PAS,PageSize);
    WriteCaptureReg(DVP_CAPn_MAOS,MaxOpcodeSize);
    WriteCaptureReg(DVP_CAPn_MACS,MaxChunkSize);
    WriteCaptureReg(DVP_CAPn_MAMS,
                  (MaxMessageSize     << CAP_MAMS_MAX_MESSAGE_SIZE_SHIFT) |
                  (WritePosting       << CAP_MAMS_WRITE_POSTING_SHIFT)|
                  (MinSpaceBetweenReq << CAP_MAMS_MIN_SPACE_BW_REQ_SHIFT));
  }
  else
  {
    WriteCaptureReg(DVP_CAPn_PAS2,PageSize);
    WriteCaptureReg(DVP_CAPn_MAOS2,MaxOpcodeSize);
    WriteCaptureReg(DVP_CAPn_MACS2,MaxChunkSize);
    WriteCaptureReg(DVP_CAPn_MAMS2,
                  (MaxMessageSize     << CAP_MAMS_MAX_MESSAGE_SIZE_SHIFT) |
                  (WritePosting       << CAP_MAMS_WRITE_POSTING_SHIFT)|
                  (MinSpaceBetweenReq << CAP_MAMS_MIN_SPACE_BW_REQ_SHIFT));
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CDvpCAP::SetMatrix(uint8_t inputisrgb,
                            uint8_t outputisrgb)
{
  int inmode=inputisrgb?1:0;
  int outmode=outputisrgb?1:0;
  int matrixmode=(inmode<<1)|(outmode<<0);

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(m_MatrixMode != (DvpMatrixMode_t)matrixmode)
  {
    TRC( TRC_ID_MAIN_INFO, "'new matrixmode \t = 0x%02x'", matrixmode );

#ifdef CAPTURE_DVP_USE_STATIC_MATRIX
    int16_t coefs[9]    = {4096,0,0,0, 4096,0,0,0, 4096};
    int16_t offset[3]   = {0, 0, 0};
    int16_t inoffset[3] = {0, 0, 0};

    // Values for RGB to YUV
    //int16_t coefs[9]    = {871,2929,296,-480,-1615,2095,2095,-1753,-341} ;
    //int16_t offset[3]   = {0,8192,8192};

    static uint8_t LutabM[4][9]={{8,6,7,2,0,1,5,3,4}, // 0 = YUV2YUV
                                 {2,0,1,5,3,4,8,6,7}, // 1 = YUV2RGB
                                 {6,7,8,0,1,2,3,4,5}, // 2 = RGB2YUV
                                 {0,1,2,3,4,5,6,7,8}};// 3 = RGB2RGB

    static uint8_t LutabO[2][3]={{2,0,1},  // YUV
                                 {0,1,2}}; // RGB

    uint32_t regval  = 0;
    uint32_t i=0,Add=MATRIX_MX0_OFFSET;

    WriteMatrixReg(MATRIX_MX0_OFFSET,
                  ((coefs[LutabM[matrixmode][0]]&0xffff)<<MATRIX_MX0_C11_SHIFT)|
                  ((coefs[LutabM[matrixmode][1]]&0xffff)<<MATRIX_MX0_C12_SHIFT));
    WriteMatrixReg(MATRIX_MX1_OFFSET,
                  ((coefs[LutabM[matrixmode][2]]&0xffff)<<MATRIX_MX1_C13_SHIFT)|
                  ((coefs[LutabM[matrixmode][3]]&0xffff)<<MATRIX_MX1_C21_SHIFT));
    WriteMatrixReg(MATRIX_MX2_OFFSET,
                  ((coefs[LutabM[matrixmode][4]]&0xffff)<<MATRIX_MX2_C22_SHIFT)|
                  ((coefs[LutabM[matrixmode][5]]&0xffff)<<MATRIX_MX2_C23_SHIFT));
    WriteMatrixReg(MATRIX_MX3_OFFSET,
                  ((coefs[LutabM[matrixmode][6]]&0xffff)<<MATRIX_MX3_C31_SHIFT)|
                  ((coefs[LutabM[matrixmode][7]]&0xffff)<<MATRIX_MX3_C32_SHIFT));
    WriteMatrixReg(MATRIX_MX4_OFFSET,
                  ((coefs[LutabM[matrixmode][8]]&0xffff)<<MATRIX_MX4_C33_SHIFT));

    WriteMatrixReg(MATRIX_MX5_OFFSET,
                  ((offset[LutabO[outmode][0]]&0xffff)<<MATRIX_MX5_Offset1_SHIFT)|
                  ((offset[LutabO[outmode][1]]&0xffff)<<MATRIX_MX5_Offset2_SHIFT));
    WriteMatrixReg(MATRIX_MX6_OFFSET,
                  ((offset[LutabO[outmode][2]]&0xffff)<<MATRIX_MX6_Offset3_SHIFT)|
                  ((inoffset[LutabO[inmode][0]]&0xffff)<<MATRIX_MX6_inOff1_SHIFT));
    WriteMatrixReg(MATRIX_MX7_OFFSET,
                  ((inoffset[LutabO[inmode][1]]&0xffff)<<MATRIX_MX7_inOff2_SHIFT)|
                  ((inoffset[LutabO[inmode][2]]&0xffff)<<MATRIX_MX7_inOff3_SHIFT));

    for(i=0; i<(MATRIX_ENTRY_SIZE/4); i++, Add+=4)
    {
      regval = ReadMatrixReg(Add);
      TRCIN( TRC_ID_UNCLASSIFIED, "%s 'DVP_MATn_MX%d \t = 0x%08x'\n",__FUNCTION__, i, regval);
    }
#else /* !CAPTURE_DVP_USE_STATIC_MATRIX */

    uint32_t *regval  = (uint32_t *)((uint32_t )m_CMatrix.pData + ((int)matrixmode*MATRIX_ENTRY_SIZE));
    uint32_t  i=0,Add=MATRIX_MX0_OFFSET;

    if(regval)
    {
      for( i=0; i<(MATRIX_ENTRY_SIZE/4); i++, Add+=4, regval+=1)
      {
        TRC( TRC_ID_UNCLASSIFIED, "'DVP_MATn_MX%d \t = 0x%08x'", i, *regval );
        WriteMatrixReg(Add,*regval);
      }
    }
#endif /* CAPTURE_DVP_USE_STATIC_MATRIX */

    /* Update the matrix mode */
    m_MatrixMode = (DvpMatrixMode_t)matrixmode;
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}


void CDvpCAP::ProgramSynchroConfiguration(void)
{
  uint32_t synchro_ctl   = SYNC_FIELD_DETECT_MASK;
  uint32_t active_height = m_InputParams.active_window.height;
  uint32_t total_height  = m_InputParams.vtotal;
  uint32_t y_pos         = m_InputParams.active_window.y;

  bool isInputInterlaced = !!((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);

  uint32_t is_locked = ReadMiscReg(DVP_MISCn_LCK) & 0x1;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(isInputInterlaced)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Input is INTERLACED !" );
    synchro_ctl |= SYNC_INTERLACED_MASK;
#ifndef HDMIRX_INTERLACE_MODE_WA
    active_height /= 2;
    total_height  /= 2;
    y_pos         /= 2;
#endif
  }

  if(m_InputParams.vsync_polarity == STM_PIXEL_CAPTURE_FIELD_POLARITY_LOW)
    synchro_ctl |= SYNC_VER_POLARITY_MASK;

  if(m_InputParams.hsync_polarity == STM_PIXEL_CAPTURE_FIELD_POLARITY_LOW)
    synchro_ctl |= SYNC_HOR_POLARITY_MASK;

  WriteSynchroReg(DVP_SYNCn_CTL,  synchro_ctl);
  WriteSynchroReg(DVP_SYNCn_HTOT,  m_InputParams.htotal);
  WriteSynchroReg(DVP_SYNCn_VTOT, ((total_height << 16) | total_height));
  WriteSynchroReg(DVP_SYNCn_HACT, m_InputParams.active_window.width);
  WriteSynchroReg(DVP_SYNCn_VACT, ((active_height << 16) | active_height));
  WriteSynchroReg(DVP_SYNCn_HBLK, m_InputParams.active_window.x);
  WriteSynchroReg(DVP_SYNCn_VBLK, ((y_pos << 16) | y_pos));

  if(!is_locked)
  {
    /* lock CAPTURE before continuing */
    WriteMiscReg(DVP_MISCn_LCK, !is_locked);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CDvpCAP::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Each filter table contains _two_ complete sets of filters, one for
   * Luma and one for Chroma.
   */
  vibe_os_allocate_dma_area(&m_HFilter, CAP_HFILTER_TABLE_SIZE, 16, SDAAF_NONE);
  if(!m_HFilter.pMemory)
  {
    TRC( TRC_ID_ERROR, "CDvpCAP::Create 'out of memory'" );
    return false;
  }
  vibe_os_memcpy_to_dma_area(&m_HFilter,0,&capture_HSRC_Coeffs,sizeof(capture_HSRC_Coeffs));

  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::Create &m_HFilter = %p pMemory = %p pData = %p phys = %08x", &m_HFilter,m_HFilter.pMemory,m_HFilter.pData,m_HFilter.ulPhysical );

  vibe_os_allocate_dma_area(&m_VFilter, CAP_VFILTER_TABLE_SIZE, 16, SDAAF_NONE);
  if(!m_VFilter.pMemory)
  {
    TRC( TRC_ID_ERROR, "CDvpCAP::Create 'out of memory'" );
    goto exit_error;
  }
  vibe_os_memcpy_to_dma_area(&m_VFilter,0,&capture_VSRC_Coeffs,sizeof(capture_VSRC_Coeffs));

  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::Create &m_VFilter = %p pMemory = %p pData = %p phys = %08x", &m_VFilter,m_VFilter.pMemory,m_VFilter.pData,m_VFilter.ulPhysical );

  vibe_os_allocate_dma_area(&m_CMatrix, CAP_MATRIX_TABLE_SIZE, 32, SDAAF_NONE);
  if(!m_CMatrix.pMemory)
  {
    TRC( TRC_ID_ERROR, "CDvpCAP::Create 'out of memory'" );
    goto exit_error;
  }
  vibe_os_memcpy_to_dma_area(&m_CMatrix,0,&capture_MATRIX_Coeffs,sizeof(capture_MATRIX_Coeffs));

  TRC( TRC_ID_MAIN_INFO, "CDvpCAP::Create &m_CMatrix = %p pMemory = %p pData = %p phys = %08x", &m_CMatrix,m_CMatrix.pMemory,m_CMatrix.pData,m_CMatrix.ulPhysical );

  SetPlugParams(false, 0, 6, 0, 0, 1, 0);
  SetPlugParams(true, 0, 6, 0, 0, 1, 0);

  /*
   * Ensure the DVP is initially disabled.
   */
  WriteMiscReg(DVP_MISCn_ITM_BCLR, MISC_ALL_MASK);

  /*
   * Only enable VSync and End Of Lock events
   */
  WriteMiscReg(DVP_MISCn_ITM_BSET, m_InterruptMask);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

exit_error:
  if(m_HFilter.pMemory)
      vibe_os_free_dma_area(&m_HFilter);

  if(m_VFilter.pMemory)
      vibe_os_free_dma_area(&m_VFilter);

  if(m_CMatrix.pMemory)
      vibe_os_free_dma_area(&m_CMatrix);

  return false;
}

/* For DVP Capture : Input Cropping is not allowed! */
bool CDvpCAP::CheckInputWindow(stm_pixel_capture_rect_t input_window)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((input_window.width != m_InputParams.active_window.width)
  || (input_window.height != m_InputParams.active_window.height)
  || (input_window.x != m_InputWindow.x)
  || (input_window.y != m_InputWindow.y))
    return false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


int CDvpCAP::SetInputParams(stm_pixel_capture_input_params_t params)
{
  int retval = -EINVAL;
  unsigned int regval=0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((retval = CCapture::SetInputParams(params)) == 0)
  {
    m_InputParamsAreValid = true;

    /*
     * Check if capture is already unlocked or not.
     */
    regval = ReadMiscReg(DVP_MISCn_LCK);
    if(!regval)
    {
      /*
       * If capture was already unlocked then we need to program the new SYNCHRO
       * configuration so we can be receiving events once we start the capture
       * and enable interrupts.
       */
      TRC( TRC_ID_MAIN_INFO, "Capture is already unlocked!!!" );
      ProgramSynchroConfiguration();
    }
    else
    {
      /*
       * If capture was locked then postpone the SYNCHRO programming later when
       * we start the capture and enable interrupts.
       */
      m_AreInputParamsChanged = true;

      /* Now we can unlock the capture */
      WriteMiscReg(DVP_MISCn_LCK, 0);
    }
  }
  else
  {
    /*
     * Wrong input params provide, discard comming start.
     */
    m_InputParamsAreValid = false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return retval;
}


int CDvpCAP::GetFormats(const stm_pixel_capture_format_t** pFormats) const
{
  int num_formats = 0;

  /*
   * This function should return the supported buffer formats according
   * to the current input setting.
   */
  if((m_InputParams.active_window.width != 0) && (m_InputParams.active_window.height != 0)
  && (m_InputParams.src_frame_rate != 0))
  {
    uint32_t i;
    bool isInputRGB = ((m_InputParams.color_space == STM_PIXEL_CAPTURE_RGB) || (m_InputParams.color_space == STM_PIXEL_CAPTURE_RGB_VIDEORANGE));

    vibe_os_zero_memory(c_Formats, (STM_PIXEL_FORMAT_COUNT*sizeof(stm_pixel_capture_format_t)));
    for(i=0; i<m_nFormats;i++)
    {
      if(((c_surfaceFormats[i] == STM_PIXEL_FORMAT_RGB565) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_RGB888) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_ARGB1555) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_ARGB4444) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_ARGB8565) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_ARGB8888) ||
          (c_surfaceFormats[i] == STM_PIXEL_FORMAT_RGB_10B10B10B_SP)) &&
         (!isInputRGB))
      {
        /* DVP doesn't support YCbCr ==> RGB CS conversion */
        continue;
      }
      else
      {
        c_Formats[num_formats] = c_surfaceFormats[i];
        num_formats++;
      }
    }
    *pFormats = c_Formats;
  }
  else
  {
    /*
     * We don't yet set any input params so likely we can be supporting
     * any format.
     */
    *pFormats = m_pSurfaceFormats;
    num_formats = m_nFormats;
  }

  return num_formats;
}


bool CDvpCAP::ConfigureCaptureResizeAndFilters(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      DvpCaptureSetup_t                    *pCaptureSetup)
{
  uint32_t ulCtrl = 0;
  uint32_t hw_srcinc;
  unsigned filter_index;

  if(qbinfo.chroma_hsrcinc != (uint32_t)m_fixedpointONE)
  {
    TRC( TRC_ID_UNCLASSIFIED, "H Resize Enabled (HSRC 4.13/4.8: %x/%x)", qbinfo.hsrcinc, qbinfo.hsrcinc >> 5 );

    hw_srcinc = (qbinfo.chroma_hsrcinc >> 5);

    pCaptureSetup->HSRC_C = hw_srcinc;

    ulCtrl |= CAP_CTL_EN_H_RESIZE;

    for(filter_index=0;filter_index<N_ELEMENTS(HSRC_index);filter_index++)
    {
      if(hw_srcinc <= HSRC_index[filter_index])
      {
        TRC( TRC_ID_UNCLASSIFIED, "  -> 5-Tap Polyphase Filtering (idx: %d)",  filter_index );

        pCaptureSetup->HFCn_C = (uint32_t )m_HFilter.pData + (filter_index*HFILTERS_ENTRY_SIZE);

        break;
      }
    }
  }

  if((qbinfo.hsrcinc != (uint32_t)m_fixedpointONE) || (qbinfo.isInput422))
  {
    TRC( TRC_ID_UNCLASSIFIED, "H Resize Enabled (HSRC 4.13/4.8: %x/%x)", qbinfo.hsrcinc, qbinfo.hsrcinc >> 5 );

    hw_srcinc = (qbinfo.hsrcinc >> 5);

    pCaptureSetup->HSRC_L = hw_srcinc;

    ulCtrl |= CAP_CTL_EN_H_RESIZE;

    for(filter_index=0;filter_index<N_ELEMENTS(HSRC_index);filter_index++)
    {
      if(hw_srcinc <= HSRC_index[filter_index])
      {
        TRC( TRC_ID_UNCLASSIFIED, "  -> 5-Tap Polyphase Filtering (idx: %d)",  filter_index );

        pCaptureSetup->HFCn_L = (uint32_t )m_HFilter.pData + (filter_index*HFILTERS_ENTRY_SIZE);

        break;
      }
    }
  }

  if(qbinfo.vsrcinc != (uint32_t)m_fixedpointONE)
  {
    TRC( TRC_ID_UNCLASSIFIED, "V Resize Enabled (VSRC 4.13/4.8: %x/%x)", qbinfo.vsrcinc, qbinfo.vsrcinc >> 5 );

    hw_srcinc = (qbinfo.vsrcinc >> 5);

    pCaptureSetup->VSRC = hw_srcinc;

    ulCtrl |= CAP_CTL_EN_V_RESIZE;

    for(filter_index=0;filter_index<N_ELEMENTS(VSRC_index);filter_index++)
    {
      if(hw_srcinc <= VSRC_index[filter_index])
      {
        TRC( TRC_ID_UNCLASSIFIED, "  -> 3-Tap Polyphase Filtering (idx: %d)",  filter_index );

        pCaptureSetup->VFCn = (uint32_t )m_VFilter.pData + (filter_index*VFILTERS_ENTRY_SIZE);

        break;
      }
    }
  }

  if(qbinfo.isInputInterlaced)
  {
    /*
     * When capturing an interlaced content and putting it on a progressive
     * content we adjust the filter phase of the bottom field to start from
     * an appropriate distance lower in the destination buffer, based on the
     * scaling factor.
     */
    uint32_t bottomfieldinc   = qbinfo.vsrcinc;
    uint32_t integerinc       = bottomfieldinc / m_fixedpointONE;
    uint32_t fractionalinc    = bottomfieldinc - (integerinc
                                                  * m_fixedpointONE);
    uint32_t topfieldphase    = (((fractionalinc * 8) + (m_fixedpointONE / 2))
                                / m_fixedpointONE) % 8;
    /*
     * The DVP Capture is using the initial phase value for the current field
     * type to positionate the captured buffer on memory. As we starting the
     * capture only during TOP field then we should be programing the TOS field
     * not the BOS field.
     */

    pCaptureSetup->VSRC |= (topfieldphase<<CAP_VSRC_TOP_INITIAL_PHASE_SHIFT);
  }

  pCaptureSetup->CTL |= ulCtrl;

  return true;
}


void CDvpCAP::ConfigureCaptureWindowSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      DvpCaptureSetup_t                    *pCaptureSetup)
{
  /* DVP CAPTURE is always capturing the complete source area */
}


void CDvpCAP::ConfigureCaptureBufferSize(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      DvpCaptureSetup_t                    *pCaptureSetup)
{
  uint32_t width       = qbinfo.BufferFrameRect.width;
  uint32_t top_height  = qbinfo.BufferHeight;
  uint32_t bot_height  = CAP_CMW_BOT_EQ_TOP_HEIGHT;
#ifndef HDMIRX_INTERLACE_MODE_WA
  if(qbinfo.isInputInterlaced)
    top_height = top_height/2;
#endif
  pCaptureSetup->CMW = (bot_height | (top_height << 16) | width);
}


bool CDvpCAP::ConfigureCaptureColourFmt(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      DvpCaptureSetup_t                    *pCaptureSetup)
{
  uint32_t ulCtrl = 0;
  uint32_t pitch_scaling = qbinfo.line_step;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(qbinfo.isInputInterlaced && !qbinfo.isBufferInterlaced)
  {
    pitch_scaling = 2 * qbinfo.line_step;
  }
  TRC( TRC_ID_UNCLASSIFIED, "pitch_scaling = %d", pitch_scaling );

  pCaptureSetup->PMP = ((pBuffer->cap_format.stride * pitch_scaling) << 16) | (pBuffer->cap_format.stride * pitch_scaling);

  switch(qbinfo.BufferFormat.format)
  {
    case STM_PIXEL_FORMAT_RGB_10B10B10B_SP:
      ulCtrl = CAP_CTL_RGB_101010;
      /* fall through */
    case STM_PIXEL_FORMAT_RGB_8B8B8B_SP:
      ulCtrl |= CAP_CTL_RGB_888;
      pCaptureSetup->VLP = pBuffer->rgb_address;
      pCaptureSetup->VCP = pBuffer->rgb_address + pBuffer->cap_format.stride;
      break;

    case STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP:
      ulCtrl = CAP_CTL_YCbCr101010;
      /* fall through */
    case STM_PIXEL_FORMAT_YUV_8B8B8B_SP:
      ulCtrl |= CAP_CTL_YCbCr888;
      pCaptureSetup->VLP = pBuffer->luma_address;
      pCaptureSetup->VCP = pBuffer->luma_address + pBuffer->chroma_offset;
      break;

    case STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP:
      ulCtrl = CAP_CTL_YCbCr422RDB_10;
      /* fall through */
    case STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP:
      ulCtrl |= CAP_CTL_YCbCr422RDB_8;
      pCaptureSetup->VLP = pBuffer->luma_address;
      pCaptureSetup->VCP = pBuffer->luma_address + pBuffer->chroma_offset;
      break;

    default:
      TRC( TRC_ID_ERROR, "Unknown colour format." );
      return false;
  }
  ulCtrl = (ulCtrl & CAP_CTL_FORMAT_MASK) << CAP_CTL_FORMAT_OFFSET;

  /*
   * No Color Space management in DVP IP! However we should program the
   * Matrix Color conversion according to the buffer color space config.
   * This will be done when configuring the input.
   */

  pCaptureSetup->CTL |= ulCtrl;

  return true;
}


void CDvpCAP::ConfigureCaptureInput(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                      CaptureQueueBufferInfo               &qbinfo,
                                                      DvpCaptureSetup_t                    *pCaptureSetup)
{
  uint32_t   ulCtrl          = 0;

  if(qbinfo.isInputRGB)
  {
    TRC( TRC_ID_UNCLASSIFIED, "RGB input selected" );
    ulCtrl  = (CAP_CTL_SOURCE_RGB888 & CAP_CTL_SEL_SOURCE_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }
  else if(qbinfo.isInput422)
  {
    TRC( TRC_ID_UNCLASSIFIED, "YCbCr 422 input selected" );
    ulCtrl  = (CAP_CTL_SOURCE_YCBCR422_8BITS & CAP_CTL_SEL_SOURCE_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
    if((m_DvpHardwareFeatures.chroma_hsrc) &&
       ((qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP) ||
        (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP)))
    {
      /* Bypass 444 to 422 conversion */
      ulCtrl  |= CAP_CTL_BYPASS_422;
    }
  }
  else
  {
    TRC( TRC_ID_UNCLASSIFIED, "YCbCr input selected" );
    ulCtrl  = (CAP_CTL_SOURCE_YCBCR888 & CAP_CTL_SEL_SOURCE_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }

  if((m_InputParams.pixel_format == STM_PIXEL_FORMAT_RGB_10B10B10B_SP) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP))
  {
    TRC( TRC_ID_UNCLASSIFIED, "RGB/YCbCr (SB) 10-bits input selected" );
    ulCtrl  |= (CAP_CTL_SOURCE_SB_10BITS_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }
  else if(m_InputParams.pixel_format == STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP)
  {
    TRC( TRC_ID_UNCLASSIFIED, "YCbCr (DB) 10-bits input selected" );
    ulCtrl  |= (CAP_CTL_SOURCE_DB_10BITS_MASK) << CAP_CTL_SEL_SOURCE_OFFSET;
  }
  pCaptureSetup->CTL |= ulCtrl;

  /*
   * The Full Programmable Matrix is always enable (no bypass).
   * It performs following color conversion (X = 10 or 8) :
   * * RGBXXX => RGBXXX
   * * YCbCrXXX => YCbCrXXX
   * * RGBXXX => YCbCrXXX
   *
   * All the other conversions are forbidden.
   */
  /* Set Color Conversion Matrix coeffs */
  if((qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_RGB565) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_RGB_8B8B8B_SP) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB1555) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB8565) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB8888) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_ARGB4444) ||
     (qbinfo.BufferFormat.format == STM_PIXEL_FORMAT_RGB_10B10B10B_SP))
  {
    /* Output buffer is RGB */
    SetMatrix(qbinfo.isInputRGB, true);
  }
  else
  {
    /* Output buffer is YCbCr */
    SetMatrix(qbinfo.isInputRGB, false);
  }
}


DvpCaptureSetup_t * CDvpCAP::PrepareCaptureSetup(const stm_pixel_capture_buffer_descr_t * const   pBuffer,
                                                          CaptureQueueBufferInfo               &qbinfo)
{
  DvpCaptureSetup_t       *pCaptureSetup;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  // Allocate a buffer for Capture data. This buffer will be freed by CQueue::ReleaseNode
  pCaptureSetup = (DvpCaptureSetup_t *) vibe_os_allocate_memory (sizeof(DvpCaptureSetup_t));
  if(!pCaptureSetup)
  {
    TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
    return 0;
  }
  vibe_os_zero_memory(pCaptureSetup, sizeof(DvpCaptureSetup_t));

  pCaptureSetup->buffer_address = (uint32_t)pBuffer;

  pCaptureSetup->CTL = CAP_CTL_CAPTURE_ENA;

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return pCaptureSetup;
}


bool CDvpCAP::PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer, CaptureQueueBufferInfo &qbinfo)
{
  capture_node_h              capNode   = {0};
  DvpCaptureSetup_t          *pCaptureSetup = (DvpCaptureSetup_t *)NULL;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  // Prepare capture
  pCaptureSetup = PrepareCaptureSetup(pBuffer, qbinfo);
  if(!pCaptureSetup)
  {
    TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
    return false;
  }

  capNode = m_CaptureQueue->GetFreeNode();
  if(!capNode)
  {
    /* should wait until for a free node */
    vibe_os_free_memory(pCaptureSetup);
    return false;
  }

  if(!pBuffer->cap_format.stride)
  {
    vibe_os_free_memory(pCaptureSetup);
    m_CaptureQueue->ReleaseNode(capNode);
    return false;
  }

  AdjustBufferInfoForScaling(pBuffer, qbinfo);

  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x",m_fixedpointONE, qbinfo.hsrcinc,qbinfo.chroma_hsrcinc );
  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x",m_fixedpointONE, qbinfo.vsrcinc,qbinfo.chroma_vsrcinc );

  /*
   * Configure the capture Colour Format.
   */
  if(!ConfigureCaptureColourFmt(pBuffer, qbinfo, pCaptureSetup))
  {
    TRC( TRC_ID_ERROR, "failed to configure capture buffer format" );
    vibe_os_free_memory(pCaptureSetup);
    m_CaptureQueue->ReleaseNode(capNode);
    return false;
  }

  /*
   * Configure the capture Resize and Filter.
   */
  if(!ConfigureCaptureResizeAndFilters(pBuffer, qbinfo, pCaptureSetup))
  {
    TRC( TRC_ID_ERROR, "failed to configure capture resize and filters" );
    vibe_os_free_memory(pCaptureSetup);
    m_CaptureQueue->ReleaseNode(capNode);
    return false;
  }

  /* Configure the capture source size */
  ConfigureCaptureWindowSize(pBuffer, qbinfo, pCaptureSetup);

  /* Configure the capture buffer (memory) size */
  ConfigureCaptureBufferSize(pBuffer, qbinfo, pCaptureSetup);

  /* Configure the capture input */
  ConfigureCaptureInput(pBuffer, qbinfo, pCaptureSetup);

  /* Set the new node data */
  m_CaptureQueue->SetNodeData(capNode,pCaptureSetup);

  pCaptureSetup = 0;  // This buffer is now under the control of queue_node

  /* Finally queue the new capture node */
  if(!m_CaptureQueue->QueueNode(capNode))
  {
    m_CaptureQueue->ReleaseNode(capNode);
    return false;
  }

  /*
   * Until now the capture is started and a buffer will need to be
   * release later.
   */
  m_hasBuffersToRelease = true;

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return true;
}


void CDvpCAP::Flush(bool bFlushCurrentNode)
{
  uint32_t          nodeReleasedNb = 0;
  capture_node_h    cut_node;           // The Queue will be cut just before this node
  capture_node_h    tail = 0;           // Remaining part of the queue after the cut

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if (bFlushCurrentNode)
  {
    TRC( TRC_ID_MAIN_INFO, "Capture Flush ALL nodes" );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Capture Flush nodes not on processing" );
  }

  ///////////////////////////////////////    Critical section      ///////////////////////////////////////////

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if (vibe_os_down_semaphore(m_QueueLock) != 0 )
    return;

  if (bFlushCurrentNode)
  {
    // Flush ALL nodes

    // Cut the Queue at its head
    cut_node = m_CaptureQueue->GetNextNode(0);

    if (cut_node)
    {
      if (m_CaptureQueue->CutTail(cut_node))
      {
        tail = cut_node;
      }
    }

    // References to the previous, pending and current nodes are reset
    m_pendingNode  = 0;
    m_currentNode  = 0;

  }
  else
  {
    // Flush the nodes not on processing

    // Cut the Queue after the pending node
    cut_node = m_CaptureQueue->GetNextNode(m_pendingNode);

    if (cut_node)
    {
      if (m_CaptureQueue->CutTail(cut_node))
      {
        tail = cut_node;
      }
    }
  }

  vibe_os_up_semaphore(m_QueueLock);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////

  // It is now safe to release all the nodes in "tail"

  while(tail != 0)
  {
    capture_node_h nodeToRelease = tail;

    // Get the next node BEFORE releasing the current node (otherwise we lose the reference)
    tail = m_CaptureQueue->GetNextNode(tail);

    // Flush pused buffer before releasing the node
    if(IsAttached() && m_PushGetInterface.push_buffer)
    {
      if(PushCapturedNode(nodeToRelease, 0, 0, true) == 0)
      {
        nodeReleasedNb++;
      }
    }
    else
    {
      RevokeCaptureBufferAccess(nodeToRelease);
      m_CaptureQueue->ReleaseNode(nodeToRelease);
      nodeReleasedNb++;
      m_Statistics.PicReleased++;
    }
  }

  if(nodeReleasedNb == m_NumberOfQueuedBuffer)
  {
    m_NumberOfQueuedBuffer = 0;
    m_hasBuffersToRelease = false;
  }

  TRC( TRC_ID_MAIN_INFO, "%d nodes released.", nodeReleasedNb );

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}


int CDvpCAP::ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(m_hasBuffersToRelease && m_CaptureQueue)
  {
    capture_node_h       pNode = 0;
    DvpCaptureSetup_t   *setup = (DvpCaptureSetup_t *)NULL;

    TRC( TRC_ID_UNCLASSIFIED, "capture = %u releasing previous node", GetID() );

    // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
    if (vibe_os_down_semaphore(m_QueueLock) != 0 )
      return -EINTR;

    pNode = m_CaptureQueue->GetNextNode(0);

    vibe_os_up_semaphore(m_QueueLock);

    while(pNode != 0)
    {
      /* Check if it is the requested node for release */
      setup = (DvpCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
      if(setup && setup->buffer_address == (uint32_t)pBuffer)
        break;
      pNode = m_CaptureQueue->GetNextNode(pNode);
    }

    if(!setup || !pNode) // nothing to release!
    {
      TRC( TRC_ID_ERROR, "CDvpCAP::ReleaseBuffer 'nothing to release?'" );
      return 0;
    }

    TRC( TRC_ID_UNCLASSIFIED, "CDvpCAP::ReleaseBuffer 'got node 0x%p for release'", pNode );

    /* Check if the node is already processed by the hw before releasing it */
    vibe_os_wait_queue_event(m_WaitQueueEvent, &setup->buffer_processed, 1, STMIOS_WAIT_COND_EQUAL, CAPTURE_RELEASE_WAIT_TIMEOUT);
    TRC( TRC_ID_UNCLASSIFIED, "CDvpCAP::ReleaseBuffer 'releasing Node %p (buffer = 0x%x | processed? (%d) )'", pNode, setup->buffer_address, setup->buffer_processed );

    // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we release the node
    if (vibe_os_down_semaphore(m_QueueLock) != 0 )
      return -EINTR;

    m_CaptureQueue->ReleaseNode(pNode);
    m_NumberOfQueuedBuffer--;

    m_Statistics.PicReleased++;

    if(m_NumberOfQueuedBuffer == 0)
      m_hasBuffersToRelease = false;

    vibe_os_up_semaphore(m_QueueLock);
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return 0;
}


int CDvpCAP::NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime)
{
  int res=0;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(pNode && m_CaptureQueue->IsValidNode(pNode))
  {
    DvpCaptureSetup_t   *setup = (DvpCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
    if(setup)
    {
      /* Mark this buffer as processed */
      setup->buffer_processed = 1;

      /* Set back the new node data */
      m_CaptureQueue->SetNodeData(pNode,setup);

      m_Statistics.PicCaptured++;

      /* Notify captured buffer */
      res = CCapture::NotifyEvent(pNode, vsyncTime);

      TRC( TRC_ID_UNCLASSIFIED, "'Notify Node %p (buffer = 0x%x | processed? (%d) )'", pNode, setup->buffer_address,setup->buffer_processed );
    }
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return res;
}


void CDvpCAP::writeFieldSetup(const DvpCaptureSetup_t *setup, bool isTopField)
{
  uint32_t  *regval;
  uint32_t   i,Add;

  TRC( TRC_ID_UNCLASSIFIED, "%s field !", isTopField ? "TOP" : "BOT" );

  WriteCaptureReg(DVP_CAPn_PMP , setup->PMP);
  WriteCaptureReg(DVP_CAPn_CMW , setup->CMW);

  /*
   * Note that the 'isTopField' information is for the next field to be
   * captured.
   * For new we are going to push a new capture setting that will be executed
   * during the next field.
   */
  if(isTopField)
  {
    /* The next field to be captured is TOP */
    WriteCaptureReg(DVP_CAPn_VLP , setup->VLP);
    WriteCaptureReg(DVP_CAPn_VCP , setup->VCP);
  }
  else
  {
    /*
     * The next field to be captured is BOTTOM.
     */
    uint32_t stride                = (setup->PMP & CAP_PMP_LUMA_MASK) >> 1;
    uint32_t luma_buffer_address   = setup->VLP;
    uint32_t chroma_buffer_address = setup->VCP;

    /* Update memory address for BOTTOM field */
    luma_buffer_address   += stride;
    chroma_buffer_address += stride;
    WriteCaptureReg(DVP_CAPn_VLP , luma_buffer_address);
    WriteCaptureReg(DVP_CAPn_VCP , chroma_buffer_address);
  }

  /* Set horizontal filter coeffs if resize is enabled */
  if(setup->CTL & CAP_CTL_EN_H_RESIZE)
  {
    /* Luma part */
    Add = DVP_CAPn_HFC0_L;

    regval = (uint32_t *)setup->HFCn_L;
    if(regval)
    {
      for( i=0; i<(NB_HSRC_COEFFS/4); i++, Add+=4, regval+=1)
      {
        TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_HFC%d \t = 0x%08x'", i, *regval );
        WriteCaptureReg(Add,*regval);
      }
    }
    WriteCaptureReg(DVP_CAPn_HSRC_L , setup->HSRC_L);

    /* Chroma part */
    if(m_DvpHardwareFeatures.chroma_hsrc)
    {
      Add = DVP_CAPn_HFC0_C;

      regval = (uint32_t *)setup->HFCn_C;
      if(regval)
      {
        for( i=0; i<(NB_HSRC_COEFFS/4); i++, Add+=4, regval+=1)
        {
          TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_HFC%d \t = 0x%08x'", i, *regval );
          WriteCaptureReg(Add,*regval);
        }
      }
      WriteCaptureReg(DVP_CAPn_HSRC_C , setup->HSRC_C);
    }
  }

  /* Set vertical filter coeffs if resize is enabled */
  if(setup->CTL & CAP_CTL_EN_V_RESIZE)
  {
    uint32_t vsrc_inc   = setup->VSRC;
    if(m_DvpHardwareFeatures.chroma_hsrc)
      Add = DVP_CAPn_VFC0_HW_V1_6;
    else
      Add = DVP_CAPn_VFC0_HW_V1_4;
    regval = (uint32_t *)setup->VFCn;
    if(regval)
    {
      for( i=0; i<(NB_VSRC_COEFFS/4); i++, Add+=4, regval+=1)
      {
        TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_HFC%d \t = 0x%08x'", i, *regval );
        WriteCaptureReg(Add,*regval);
      }
    }

    /* Write VSRC register */
    if(!isTopField)
    {
      uint32_t fieldphase = (setup->VSRC >> CAP_VSRC_TOP_INITIAL_PHASE_SHIFT) & CAP_VSRC_INITIAL_PHASE_MASK;

      /* Clear the fieldphase from capture setup before going on */
      vsrc_inc &= ~(CAP_VSRC_INITIAL_PHASE_MASK << CAP_VSRC_TOP_INITIAL_PHASE_SHIFT);

      /* Set the inital phase for the BOTTOM field (next field to be captured) */
      vsrc_inc |= (fieldphase<<CAP_VSRC_BOT_INITIAL_PHASE_SHIFT);
    }
    if(m_DvpHardwareFeatures.chroma_hsrc)
      WriteCaptureReg(DVP_CAPn_VSRC_HW_V1_6 , vsrc_inc);
    else
      WriteCaptureReg(DVP_CAPn_VSRC_HW_V1_4 , vsrc_inc);
  }

  /* Program the control register only during TOP (current) field */
  if(isTopField)
  {
    /* We should not program the control register for each nodes update */
    if(m_ulCaptureCTL != setup->CTL)
    {
      m_ulCaptureCTL = setup->CTL;
      WriteCaptureReg(DVP_CAPn_CTL , setup->CTL);
    }
  }

  m_Statistics.PicInjected++;

#if defined(CAPTURE_DEBUG_REGISTERS)
  if((ctx == 1000) || (ctx == 1001))
  {
    TRC( TRC_ID_UNCLASSIFIED, "'%s Node'\n", isTopField ? "TOP" : "BOT" );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_CTL \t = 0x%08x'",  ReadCaptureReg(DVP_CAPn_CTL) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_VLP \t = 0x%08x'",  ReadCaptureReg(DVP_CAPn_VLP) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_VCP \t = 0x%08x'",  ReadCaptureReg(DVP_CAPn_VCP) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_PMP \t = 0x%08x'",  ReadCaptureReg(DVP_CAPn_PMP) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_CMW \t = 0x%08x'",  ReadCaptureReg(DVP_CAPn_CMW) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_HSRC \t = 0x%08x'", ReadCaptureReg(DVP_CAPn_HSRC) );
    TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_VSRC \t = 0x%08x'", ReadCaptureReg(DVP_CAPn_VSRC) );

    for(i=0; i < NB_HSRC_COEFFS/4; i++)
      TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_HFC%d \t = 0x%08x'", i, ReadCaptureReg((DVP_CAPn_HFC0 + (4*i))) );

    for(i=0; i < NB_VSRC_COEFFS/4; i++)
      TRC( TRC_ID_UNCLASSIFIED, "'DVP_CAPn_VFC%d \t = 0x%08x'", i, ReadCaptureReg((DVP_CAPn_VFC0 + (4*i))) );

    for(i=0; i < MATRIX_ENTRY_SIZE/4; i++)
      TRC( TRC_ID_UNCLASSIFIED, "'DVP_MATn_MX%d \t = 0x%08x'", i, ReadMatrixReg(((4*i))) );

    if(ctx == 1001)
      ctx = 0;
  }
  ctx++;
#endif
}


int CDvpCAP::Start(void)
{
  int res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_isStarted)
  {
    /* Start DVP only if there are valid inputs parameters */
    if(m_InputParamsAreValid)
    {
      /* If not locked call SetInputParams to make sure inputs are set
       * and lock is set so HW start capturing */
      if(!(ReadMiscReg(DVP_MISCn_LCK) & 0x1))
        SetInputParams(m_InputParams);

      if(EnableInterrupts())
      {
        TRC( TRC_ID_ERROR, "failed to enable capture interrupts" );
      }
      CCapture::Start();
    }
    else
    {
      TRC( TRC_ID_ERROR, "Input params not well configured" );
      res = -EINVAL;
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


int CDvpCAP::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_isStarted)
  {
    if(DisableInterrupts())
    {
      TRC( TRC_ID_ERROR, "failed to disable capture interrupts" );
    }

    /* Unlock the capture */
    WriteMiscReg(DVP_MISCn_LCK, 0);

    uint32_t ulCtrl = ReadCaptureReg(DVP_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(DVP_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;

    CCapture::Stop();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


uint32_t CDvpCAP::GetInterruptStatus(void)
{
  uint32_t intStatus;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  intStatus =  ReadMiscReg(DVP_MISCn_ITS);

  if(intStatus != 0)
  {
    TRC( TRC_ID_UNCLASSIFIED, "intStatus = 0x%02x !", intStatus );
  }

  /* Always clear the status */
  WriteMiscReg(DVP_MISCn_ITS_BCLR, intStatus);
  (void)ReadMiscReg(DVP_MISCn_ITS); // sync bus write

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return intStatus;
}


int CDvpCAP::EnableInterrupts(void)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  /* Clear any old EOCAP pending status bit first. */
  WriteMiscReg(DVP_MISCn_ITS_BCLR, MISC_ALL_MASK);

  /* Enable EOFCAP (all) interrupts on MISC */
  WriteMiscReg(DVP_MISCn_ITM_BSET, MISC_ALL_MASK);

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return 0;
}


int CDvpCAP::DisableInterrupts(void)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  /* Clear all pending events */
  WriteMiscReg(DVP_MISCn_ITS_BCLR, MISC_ALL_MASK);

  /* Mask all events */
  WriteMiscReg(DVP_MISCn_ITM_BCLR, m_InterruptMask);

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return 0;
}


int CDvpCAP::HandleInterrupts(uint32_t *TimingEvent)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(!TimingEvent)
  {
    TRC( TRC_ID_ERROR, "invalid timing event argument!" );
    return -EINVAL;
  }

  vibe_os_lock_resource(m_lock);

  /* Read ITS */
  *TimingEvent =  GetInterruptStatus();

  /* Get field polarity information */
  *TimingEvent |= ((ReadSynchroReg(DVP_SYNCn_STA) & SYNC_STA_FIELD_POL_MASK) << CAPTURE_DVP_FIELD_POL_SHIFT);

  if(IsAttached())
  {
    /*
     * This part is begin implemented in order to optimize the update hardware
     * process in the way that it only catch end of capture events and not other
     * sync events to push new capture nodes and release captured buffers.
     *
     * To do so we are going to check if we have received the first vsync event
     * and capture is already locked. If so we will be masking all events and
     * keep only EOCAP one.
     */
    uint32_t is_locked = ReadMiscReg(DVP_MISCn_LCK) & 0x1;

    if(((*TimingEvent) & MISC_VSYNC_MASK) && is_locked)
    {
      /* Clear All DVP event masks */
      WriteMiscReg(DVP_MISCn_ITM_BCLR, MISC_ALL_MASK);

      /* Mask only VSYNC DVP event */
      WriteMiscReg(DVP_MISCn_ITM_BSET, MISC_VSYNC_MASK);

      *TimingEvent &= ~MISC_VSYNC_MASK;
      m_PendingEvent |= MISC_VSYNC_MASK;
    }
    else
    {
      /* Unmask all DVP irqs (keep waiting for the first vsync event ...) */
      WriteMiscReg(DVP_MISCn_ITM_BSET, MISC_ALL_MASK);
    }
  }
  else
  {
    /* Unmask all DVP irqs (keep waiting for the first vsync event ...) */
    WriteMiscReg(DVP_MISCn_ITM_BSET, MISC_ALL_MASK);
  }

  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return 0;
}


void CDvpCAP::ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  capture_node_h       nodeCandidateForCapture = 0;
  uint32_t             intStatus=0;
  bool                 isTopField=true;
  bool                 isInputInterlaced=false;

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if (vibe_os_down_semaphore(m_QueueLock) != 0 )
    goto nothing_to_process_in_capture;

  intStatus         = timingevent & MISC_ALL_MASK;
  isTopField        = !!((timingevent >> CAPTURE_DVP_FIELD_POL_SHIFT) & SYNC_STA_FIELD_POL_MASK);
  isInputInterlaced = !!((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);

  TRC( TRC_ID_UNCLASSIFIED, "intStatus = %d | isTopField = %d  | isInputInterlaced = %d", intStatus, (int)isTopField, (int)isInputInterlaced);

  if(intStatus & MISC_EOLOCK_MASK)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOLOCK_MASK !" );

    /*
     * This is meaning that the input signal is no more stable or available.
     * Someone could be setting new input parameters and unlocking the capture.
     * If this is the case then we should ignore this event and go on otherwise
     * we should be stopping capture process and not programming new nodes as
     * this would be crashing the capture pipe.
     */
    if(!m_AreInputParamsChanged)
    {
      /* No new input params begin submitter */
      goto nothing_to_process_in_capture;
    }
    intStatus &= ~MISC_EOLOCK_MASK;
  }

  /*
   * We may have a valid capture buffer to process. However at this time we
   * can't receive a VSYNC event yet as SYNCHRO is not yet configured!
   * So proceed to the SYNCHRO programming to enable VSYNC events which will
   * allow us to apply the new capture node and start the capture process.
   */
  if(m_AreInputParamsChanged)
  {
    ProgramSynchroConfiguration();

    /* unvalidate previous input params */
    m_AreInputParamsChanged = false;
  }

  if(!m_isStarted)
  {
    /* Do not program Capture Configuration */
    goto nothing_to_process_in_capture;
  }

  /*
   * For the moment we don't treat field event.
   */
  if(intStatus & MISC_EOFIELD_MASK)
  {
    if((intStatus == MISC_EOFIELD_MASK) && (m_PendingEvent == 0))
    {
      TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOFIELD_MASK with no PendingEvent!" );
      vibe_os_up_semaphore(m_QueueLock);
      return;
    }

    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOFIELD_MASK !" );
    intStatus &= ~MISC_EOFIELD_MASK;
  }

  /*
   * Invalidate Nodes when VSync signal occurs.
   */
  if((intStatus & MISC_VSYNC_MASK) || (m_PendingEvent & MISC_VSYNC_MASK))
  {
    if(((intStatus & MISC_VSYNC_MASK) == MISC_VSYNC_MASK) && (m_PendingEvent == 0))
    {
      TRC( TRC_ID_UNCLASSIFIED, "VSync Event Ignored!!!" );
      goto skip_capture_event;
    }
    else
    {
      m_Statistics.PicVSyncNb++;
    }
  }

  /*
   * Make sure to push back all remainig nodes that were queued before the
   * actual marked previous node.
   */
  if(!isTopField || !isInputInterlaced)
  {
    if((m_previousNode) && m_CaptureQueue->IsValidNode(m_previousNode))
    {
      capture_node_h first_node = m_CaptureQueue->GetNextNode(0);
      while ((first_node)
          && (first_node != m_currentNode)
          && (m_CaptureQueue->IsValidNode(first_node)))
      {
        if(PushCapturedNode(first_node, vsyncTime, 0, true) < 0)
        {
          TRC( TRC_ID_ERROR, "Push Captured Node failed!!!" );
        }
        first_node = m_CaptureQueue->GetNextNode(0);
        m_Statistics.PicSkipped++;
      }
      m_previousNode = 0;
    }
  }

  if(((intStatus & MISC_EOCAP_MASK) == MISC_EOCAP_MASK) || (m_PendingEvent & MISC_VSYNC_MASK))
  {
    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_VSYNC_MASK !" );

    if (m_previousNode && (!m_CaptureQueue->IsValidNode(m_previousNode)) )
    {
      TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_previousNode 0x%p!",m_previousNode );
      m_previousNode = 0;
    }

    if (m_currentNode && (!m_CaptureQueue->IsValidNode(m_currentNode)) )
    {
      TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_currentNode 0x%p!",m_currentNode );
      m_currentNode = 0;
    }

    if (m_pendingNode && (!m_CaptureQueue->IsValidNode(m_pendingNode)) )
    {
      TRC( TRC_ID_UNCLASSIFIED, "Error invalid m_pendingNode 0x%p!",m_pendingNode);
      m_pendingNode = 0;
    }

    // A new TOP VSync has happen so:
    //  - the PendingNode is going to become the CurrentNode
    //  - the CurrentNode is going to be released (if the pending picture is a new one)
    //  - a new node is going to be prepared for next VSync and will become the PendingNode
    CCapture::UpdateCurrentNode(vsyncTime);

    /* If the input is interlaced then we update nodes values only during the BOTTOM field */
    if(!isTopField || !isInputInterlaced)
    {
      // The current node become the previous node so it will notified later
      m_previousNode = m_currentNode;

      // The pending node become the current node
      m_currentNode  = m_pendingNode;

      // The pending node become the current node
      m_pendingNode = 0;

      m_PendingEvent = 0;

      if(intStatus & MISC_EOCAP_MASK)
        m_Statistics.PicCaptured++;
    }

    /*
     * Process new vsync event (eventuall we could have capture event there).
     */
    if(((intStatus & MISC_ALL_MASK) == MISC_EOCAP_MASK)
      || (m_PendingEvent & MISC_EOCAP_MASK)
      || (m_PendingEvent & MISC_VSYNC_MASK))
    {
      TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOCAP_MASK !" );

      /*
       * If the input is interlaced then we release the buffer only during the TOP
       * field as this event is always coming during the current field.
       */
      if(!isTopField || !isInputInterlaced)
      {
        if((m_previousNode) && m_CaptureQueue->IsValidNode(m_previousNode))
        {
          // Push back the previous node if it was not yet done during capture
          // event.
          if(PushCapturedNode(m_previousNode, vsyncTime, 1, true) < 0)
          {
            /* Skip this event */
            TRC( TRC_ID_ERROR, "Push Captured Node failed!!!" );
            goto skip_capture_event;
          }
          m_previousNode = 0;
          m_PendingEvent &= ~(MISC_EOCAP_MASK|MISC_VSYNC_MASK);
        }
        else
        {
          TRC( TRC_ID_UNCLASSIFIED, "Invalid previous node !!!" );
        }
      }
    }

    if(!isTopField || !isInputInterlaced)
    {
      // Now find a candidate for next VSync
      if (m_currentNode == 0)
      {
        // No picture was currently captured: Get the first picture of the queue
        nodeCandidateForCapture = m_CaptureQueue->GetNextNode(0);

        if (nodeCandidateForCapture)
          TRC( TRC_ID_UNCLASSIFIED, "No buffer currently queued. Use the 1st node of the queue: %p", nodeCandidateForCapture );
      }
      else
      {
        // Get the next buffer in the queue
        nodeCandidateForCapture = m_CaptureQueue->GetNextNode(m_currentNode);

        if (nodeCandidateForCapture)
          TRC( TRC_ID_UNCLASSIFIED, "Move to next node: %p", nodeCandidateForCapture );
      }

      if (!nodeCandidateForCapture)
      {
        if (GetCaptureNode(nodeCandidateForCapture, true) < 0)
        {
          /* Skip this event */
          TRC( TRC_ID_ERROR, "Get Capture Node failed!!!" );
          goto skip_capture_event;
        }
      }
    }
    else
    {
      if (m_pendingNode)
      {
        nodeCandidateForCapture = m_pendingNode;
        TRC( TRC_ID_UNCLASSIFIED, "Keep using current node %p for BOTTOM field capture", nodeCandidateForCapture );
      }
    }

    if((nodeCandidateForCapture) && m_CaptureQueue->IsValidNode(nodeCandidateForCapture))
    {
      stm_i_push_get_sink_get_desc_t   *pGetBufferDesc = (stm_i_push_get_sink_get_desc_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
      stm_pixel_capture_buffer_descr_t  CaptureBuffer = { 0 };
      CaptureQueueBufferInfo            qbinfo        = { 0 };
      DvpCaptureSetup_t                *pCaptureSetup = (DvpCaptureSetup_t *)NULL;

      if(pGetBufferDesc)
      {
        TRC( TRC_ID_UNCLASSIFIED, "'Capture will be processing Node (%p) and Buffer (%p) for the new VSync'", nodeCandidateForCapture, (void *)pGetBufferDesc);

        if(pGetBufferDesc->video_buffer_addr == 0)
        {
          TRC( TRC_ID_UNCLASSIFIED, "Got Node with invalid buffer address (%p)\n", m_Sink);
          /* Release the node */
          m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
          m_Statistics.PicReleased++;

          if (GetCaptureNode(nodeCandidateForCapture, true) < 0)
          {
            TRC( TRC_ID_ERROR, "Error in GetCapturedNode(%p)\n", m_Sink);
            // Nothing to process! Skip this event.
            goto skip_capture_event;
          }
          pGetBufferDesc = (stm_i_push_get_sink_get_desc_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
        }

        /*
         * Setup the capture buffer format and descriptor according to new push
         * buffer descriptor data.
         */
        if(pGetBufferDesc)
        {
          CaptureBuffer.cap_format     = m_CaptureFormat;
          CaptureBuffer.bytesused      = pGetBufferDesc->height * pGetBufferDesc->pitch;
          CaptureBuffer.length         = pGetBufferDesc->height * pGetBufferDesc->pitch;
          if((CaptureBuffer.cap_format.format == STM_PIXEL_FORMAT_RGB_8B8B8B_SP) ||
             (CaptureBuffer.cap_format.format == STM_PIXEL_FORMAT_RGB_10B10B10B_SP))
          {
            CaptureBuffer.rgb_address   = pGetBufferDesc->video_buffer_addr;
          }
          else
          {
            CaptureBuffer.luma_address   = pGetBufferDesc->video_buffer_addr;
            CaptureBuffer.chroma_offset  = pGetBufferDesc->chroma_buffer_offset;
          }
          CaptureBuffer.captured_time  = vsyncTime + m_FrameDuration;

          if(!GetQueueBufferInfo(&CaptureBuffer, qbinfo))
          {
            TRC( TRC_ID_UNCLASSIFIED, "failed to get new buffer queue info data");
            goto skip_capture_event;
          }

          AdjustBufferInfoForScaling(&CaptureBuffer, qbinfo);

          TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x",m_fixedpointONE, qbinfo.hsrcinc,qbinfo.chroma_hsrcinc );
          TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x",m_fixedpointONE, qbinfo.vsrcinc,qbinfo.chroma_vsrcinc );

          // Prepare capture
          pCaptureSetup = PrepareCaptureSetup(&CaptureBuffer, qbinfo);
          if(!pCaptureSetup)
          {
            TRC( TRC_ID_ERROR, "failed to allocate pCaptureSetup" );
            goto skip_capture_event;
          }

          /*
           * Configure the capture Colour Format.
           */
          if(!ConfigureCaptureColourFmt(&CaptureBuffer, qbinfo, pCaptureSetup))
          {
            TRC( TRC_ID_ERROR, "failed to configure capture buffer format" );
            vibe_os_free_memory(pCaptureSetup);
            m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
            goto skip_capture_event;
          }

          /*
           * Configure the capture Resize and Filter.
           */
          if(!ConfigureCaptureResizeAndFilters(&CaptureBuffer, qbinfo, pCaptureSetup))
          {
            TRC( TRC_ID_ERROR, "failed to configure capture resize and filters" );
            vibe_os_free_memory(pCaptureSetup);
            m_CaptureQueue->ReleaseNode(nodeCandidateForCapture);
            goto skip_capture_event;
          }

          /* Configure the capture source size */
          ConfigureCaptureWindowSize(&CaptureBuffer, qbinfo, pCaptureSetup);

          /* Configure the capture buffer (memory) size */
          ConfigureCaptureBufferSize(&CaptureBuffer, qbinfo, pCaptureSetup);

          /* Configure the capture input */
          ConfigureCaptureInput(&CaptureBuffer, qbinfo, pCaptureSetup);

          /* Write capture cfg */
          writeFieldSetup(pCaptureSetup, (!isTopField || !isInputInterlaced) ? true:false);

          /* Setup is no more needed */
          vibe_os_free_memory(pCaptureSetup);

          if(!isTopField || !isInputInterlaced)
          {
            SetPendingNode(nodeCandidateForCapture);
          }
        }
      }
    }
  }

  vibe_os_up_semaphore(m_QueueLock);

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  return;

skip_capture_event:
  m_Statistics.PicSkipped++;
  vibe_os_up_semaphore(m_QueueLock);

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  return;

nothing_to_process_in_capture:
  {
    // The Capture is disabled to avoid writing an invalid buffer
    uint32_t ulCtrl = ReadCaptureReg(DVP_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(DVP_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;
  }

  vibe_os_up_semaphore(m_QueueLock);

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);
}


void CDvpCAP::CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  capture_node_h       nodeCandidateForCapture = 0;
  DvpCaptureSetup_t   *setup                   = (DvpCaptureSetup_t *)NULL;
  uint32_t             intStatus=0;
  bool                 isTopField=true;
  bool                 isInputInterlaced=false;

  TRC( TRC_ID_UNCLASSIFIED, "CDvpCAP::CaptureUpdateHW 'vsyncTime = %llu \t - timingevent = 0x%08x'", vsyncTime, timingevent );
  m_Statistics.PicVSyncNb++;

  if ((!m_CaptureQueue))
  {
    // The queue is not created yet
    TRC( TRC_ID_ERROR, "The queue is not created yet!" );
    goto nothing_to_inject_in_capture;
  }

  // Take the lock to ensure that CaptureUpdateHW() cannot be executed while we update the Queue
  if (vibe_os_down_semaphore(m_QueueLock) != 0 )
    goto nothing_to_inject_in_capture;

  intStatus         = timingevent & MISC_ALL_MASK;
  isTopField        = !!((timingevent >> CAPTURE_DVP_FIELD_POL_SHIFT) & SYNC_STA_FIELD_POL_MASK);
  isInputInterlaced = !!((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);

  vibe_os_up_semaphore(m_QueueLock);

  if(intStatus & MISC_EOLOCK_MASK)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOLOCK_MASK !" );

    /*
     * This is meaning that the input signal is no more stable or available.
     * Someone could be setting new input parameters and unlocking the capture.
     * If this is the case then we should ignore this event and go on otherwise
     * we should be stopping capture process and not programming new nodes as
     * this would be crashing the capture pipe.
     */
    if(!m_AreInputParamsChanged)
    {
      /* No new input params begin submitter */
      goto nothing_to_inject_in_capture;
    }
  }

  /*
   * We may have a valid capture buffer to process. However at this time we
   * can't receive a VSYNC event yet as SYNCHRO is not yet configured!
   * So proceed to the SYNCHRO programming to enable VSYNC events which will
   * allow us to apply the new capture node and start the capture process.
   */
  if(m_AreInputParamsChanged)
  {
    ProgramSynchroConfiguration();

    /* unvalidate previous input params */
    m_AreInputParamsChanged = false;
  }

  if(!m_isStarted)
  {
    /* Do not program Capture Configuration */
    goto nothing_to_inject_in_capture;
  }

  if(intStatus & MISC_EOCAP_MASK)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOCAP_MASK !" );

    // Check node validity
    if (m_previousNode && (!m_CaptureQueue->IsValidNode(m_previousNode)) )
    {
      TRC( TRC_ID_MAIN_INFO, "Error invalid m_previousNode 0x%p!",m_previousNode );
      m_previousNode = 0;
    }

    // Check node validity
    if (m_currentNode && (!m_CaptureQueue->IsValidNode(m_currentNode)) )
    {
      TRC( TRC_ID_MAIN_INFO, "Error invalid m_currentNode 0x%p!",m_currentNode );
      m_currentNode = 0;
    }

    if (m_pendingNode && (!m_CaptureQueue->IsValidNode(m_pendingNode)) )
    {
      TRC( TRC_ID_MAIN_INFO, "Error invalid m_pendingNode 0x%p!",m_pendingNode );
      m_pendingNode = 0;
    }

    // A new VSync has happen so:
    //  - the pendingNode is going to become the currentNode
    //  - the currentNode is going to be notfied within next VSync
    //  - a new node is going to be prepared for next VSync and will become the pendingNode

    CCapture::UpdateCurrentNode(vsyncTime);

    /* If the input is interlaced then we update nodes values only during the BOTTOM field */
    if(!isTopField || !isInputInterlaced)
    {
      // The current node become the previous node so it will notified later
      m_previousNode = m_currentNode;

      // The pending node become the current node
      m_currentNode  = m_pendingNode;
      m_pendingNode  = 0;
    }

    /* If the input is interlaced then we release the buffer only during the TOP field */
    if(isTopField || !isInputInterlaced)
    {
      if(m_previousNode)
      {
        NotifyEvent(m_previousNode, vsyncTime);
        m_previousNode = 0;
      }
    }
  }

  if(intStatus & MISC_EOFIELD_MASK)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Received MISC_EOFIELD_MASK !" );
  }

  if(intStatus & (MISC_VSYNC_MASK | MISC_EOCAP_MASK))
  {
    // Now find a candidate for next VSync
    if (m_currentNode == 0)
    {
      // No picture was currently captured: Get the first picture of the queue
      nodeCandidateForCapture = m_CaptureQueue->GetNextNode(0);

      if (nodeCandidateForCapture)
        TRC( TRC_ID_UNCLASSIFIED, "No buffer currently queued. Use the 1st node of the queue: %p", nodeCandidateForCapture );
    }
    else
    {
      if(isTopField || !isInputInterlaced)
      {
        // Get the next buffer in the queue
        nodeCandidateForCapture = m_CaptureQueue->GetNextNode(m_currentNode);
      }

      if (nodeCandidateForCapture)
        TRC( TRC_ID_UNCLASSIFIED, "Move to next node: %p", nodeCandidateForCapture );
    }

    if (!nodeCandidateForCapture)
    {
      // No valid candidate was found. If we had a currentNode, continue to process it
      if (m_currentNode)
      {
        nodeCandidateForCapture = m_currentNode;
        m_Statistics.PicRepeated++;
        TRC( TRC_ID_UNCLASSIFIED, "No valid candidate found. Keep processing node %p", nodeCandidateForCapture );
      }
      else
      {
        // Nothing to process!
        m_Statistics.PicSkipped++;
        goto nothing_to_inject_in_capture;
      }
    }

    if(nodeCandidateForCapture)
    {
      setup = (DvpCaptureSetup_t *)m_CaptureQueue->GetNodeData(nodeCandidateForCapture);
      if(setup)
      {
        TRC( TRC_ID_UNCLASSIFIED, "'Capture will be processing Node (%p) and Buffer (%p) for the new VSync'", nodeCandidateForCapture, (void *)setup->buffer_address);

        /* update capture time */
        stm_pixel_capture_buffer_descr_t *pBuffer = (stm_pixel_capture_buffer_descr_t *)setup->buffer_address;
        if(pBuffer)
        {
          pBuffer->captured_time = vsyncTime + m_FrameDuration;
        }

        /* Write capture cfg */
        writeFieldSetup(setup, (!isTopField || !isInputInterlaced) ? true:false);
      }

      if(isTopField || !isInputInterlaced)
      {
        SetPendingNode(nodeCandidateForCapture);
      }
    }
  }

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);

  return;

nothing_to_inject_in_capture:
  {
    // The Capture is disabled to avoid writing an invalid buffer
    uint32_t ulCtrl = ReadCaptureReg(DVP_CAPn_CTL);
    ulCtrl &= ~CAP_CTL_CAPTURE_ENA;
    WriteCaptureReg(DVP_CAPn_CTL , ulCtrl);

    /* Invalidate previous CTL value */
    m_ulCaptureCTL = CAPTURE_HW_INVALID_CTL;
  }

  /* unblock waiting dequeue calls */
  vibe_os_wake_up_queue_event(m_WaitQueueEvent);
}


void CDvpCAP::AdjustBufferInfoForScaling(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                                       CaptureQueueBufferInfo              &qbinfo) const
{
#ifdef HDMIRX_INTERLACE_MODE_WA
  if(qbinfo.isInputInterlaced && !qbinfo.isBufferInterlaced)
  {
    TRC( TRC_ID_MAIN_INFO, "Input Interlaced & Buffer Progressive" );
    qbinfo.BufferHeight /= 2;

    /*
     * We know that hdmirx is already giving resolution for each field
     * i.e for 1920x1080-50 mode it is giving 1920x540 as resolution.
     * This should be fixed in the hdmirx and here we are putting this WA
     * until having the proper fix (See Bugzilla #41167).
     */
    qbinfo.InputVisibleArea.y       *= 2;
    qbinfo.InputVisibleArea.height  *= 2;
  }
#endif

  CCapture::AdjustBufferInfoForScaling(pBuffer, qbinfo);
}

void CDvpCAP::RevokeCaptureBufferAccess(capture_node_h pNode)
{
  if(pNode && m_CaptureQueue->IsValidNode(pNode))
  {
    if(!IsAttached())
    {
      DvpCaptureSetup_t *setup = (DvpCaptureSetup_t *)m_CaptureQueue->GetNodeData(pNode);
      stm_pixel_capture_buffer_descr_t *bufferToRelease;

      if(setup)
      {
        bufferToRelease = (stm_pixel_capture_buffer_descr_t *)setup->buffer_address;
        CCapture::RevokeCaptureBufferAccess(bufferToRelease);
      }
    }
  }
}
