/***********************************************************************
 *
 * File: pixel_capture/ip/fvdp/stmscalercapture.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display_types.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <pixel_capture/generic/Capture.h>

#include "scaler_defs.h"
#include "scaler_regs.h"

#include "stmscalercapture.h"


static const stm_pixel_capture_input_s stm_pixel_capture_inputs[] = {
  {"stm_input_unknown",      0, 0},
};

static const stm_pixel_capture_format_t c_surfaceFormats[] = {
  STM_PIXEL_FORMAT_RGB888,
  STM_PIXEL_FORMAT_YUV_NV12,
  STM_PIXEL_FORMAT_YUV_NV16,
};

CFvdpScalerCAP::CFvdpScalerCAP(const char     *name,
                                         uint32_t        id,
                                         const CPixelCaptureDevice *pDev,
                                         const stm_pixel_capture_capabilities_flags_t caps):CCapture(name, id, pDev, caps)
{
  TRC( TRC_ID_MAIN_INFO, "CFvdpScalerCAP::CFvdpScalerCAP %s id = %u", name, id );

  m_pSurfaceFormats = c_surfaceFormats;
  m_nFormats = N_ELEMENTS (c_surfaceFormats);

  m_pCaptureInputs         = stm_pixel_capture_inputs;
  m_pCurrentInput          = m_pCaptureInputs[0];
  m_numInputs              = N_ELEMENTS(stm_pixel_capture_inputs);

  TRC( TRC_ID_MAIN_INFO, "CFvdpScalerCAP::CFvdpScalerCAP out" );
}


CFvdpScalerCAP::~CFvdpScalerCAP(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCIN( TRC_ID_MAIN_INFO, "" );
}


bool CFvdpScalerCAP::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CCapture::Create())
    return false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


int CFvdpScalerCAP::ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}

void CFvdpScalerCAP::CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CFvdpScalerCAP::Flush(bool bFlushCurrentNode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CFvdpScalerCAP::ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CFvdpScalerCAP::PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer, CaptureQueueBufferInfo &qbinfo)
{

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return true;
}
