/***********************************************************************
 *
 * File: Generic/Output.cpp
 * Copyright (c) 2000-2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include "IOS.h"
#include "IDebug.h"
#include "Output.h"
#include "DisplayPlane.h"

/* {Mode},
 * {FrameRate, ScanType, ActiveAreaWidth, ActiveAreaHeight, ActiveAreaXStart, FullVBIHeight, OutputStandards, SquarePixel, HDMIVideoCode},
 * {PixelsPerLine, LinesByFrame, PixelClock, HSyncPolarity, HSyncPulseWidth, VSyncPolarity, VSyncPulseWidth}
 *
 * Note that ActiveAreaHeight and FullVBIHeight are given in frame lines
 * even for interlaced modes, so the actual number of lines per interlaced field
 * is half the given numbers.
 *
 * VSyncPulseWidth is specified in the number of lines to be generated per
 * progressive frame or interlaced field. The SD lengths are for digital
 * blanking, the DENC generates the correct VBI signals for analogue signals.
 * The 576i (625line) timing uses the 3 line system described in CEA-861-C,
 * rather than 2.5 lines.
 *
 * SD Progressive modes describe a SMPTE293M system.
 *
 * The VBI length (hence the vertical video position) for non-square-pixel
 * 480i (525line) modes is based on the CEA-861-C definition of 480i for digital
 * interfaces and therefore the video starts one field line early compared with
 * the original 525line analogue standards. Note that this does not effect line
 * 20/21 data services on analogue outputs.
 *
 * The horizontal blanking for non-square-pixel 480i modes is also based on
 * the digital CEA-861-C definition and as a result the analogue output of
 * these modes will have the image shifted by ~4pixels.
 *
 * Interestingly the definitions of the analogue and digital versions of
 * 576i (625line) modes are identical. Go figure?
 *
 */

#define PIX_AR_NULL   {0,0}
#define PIX_AR_SQUARE {1,1}
#define PIX_AR_8_9    {8,9}
#define PIX_AR_32_27  {32,27}
#define PIX_AR_16_15  {16,15}
#define PIX_AR_64_45  {64,45}

#define STM_480I_5994_STANDARDS (STM_OUTPUT_STD_NTSC_M   | \
                                 STM_OUTPUT_STD_NTSC_443 | \
                                 STM_OUTPUT_STD_NTSC_J   | \
                                 STM_OUTPUT_STD_PAL_M    | \
                                 STM_OUTPUT_STD_PAL_60)

#define STM_576I_50_STANDARDS   (STM_OUTPUT_STD_PAL_BDGHI | \
                                 STM_OUTPUT_STD_PAL_N     | \
                                 STM_OUTPUT_STD_PAL_Nc    | \
                                 STM_OUTPUT_STD_SECAM)


static const stm_mode_line_t ModeParamsTable[] =
{
  /* SD/ED modes */
  { STVTG_TIMING_MODE_480I60000_13514,
    { 60000, SCAN_I, 720, 480, 119, 36, STM_OUTPUT_STD_NTSC_M, {PIX_AR_8_9, PIX_AR_32_27}, {0,0} },
    { 858, 525, 13513500, false, 62, false, 3}
  },
  { STVTG_TIMING_MODE_480P60000_27027,
    { 60000, SCAN_P, 720, 480, 122, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_8_9, PIX_AR_32_27}, {2,3} },
    { 858, 525, 27027000, false, 62, false, 6}
  },
  { STVTG_TIMING_MODE_480I59940_13500,
    { 59940, SCAN_I, 720, 480, 119, 36, STM_480I_5994_STANDARDS, {PIX_AR_8_9, PIX_AR_32_27}, {0,0} },
    { 858, 525, 13500000, false, 62, false, 3}
  },
  { STVTG_TIMING_MODE_480P59940_27000,
    { 59940, SCAN_P, 720, 480, 122, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_8_9, PIX_AR_32_27}, {2,3} },
    { 858, 525, 27000000, false, 62, false, 6}
  },
  { STVTG_TIMING_MODE_480I59940_12273,
    { 59940, SCAN_I, 640, 480, 118, 38, (STM_OUTPUT_STD_NTSC_M | STM_OUTPUT_STD_NTSC_J | STM_OUTPUT_STD_PAL_M), {PIX_AR_SQUARE, PIX_AR_NULL}, {0,0} },
    { 780, 525, 12272727, false, 59, false, 3}
  },
  { STVTG_TIMING_MODE_576I50000_13500,
    { 50000, SCAN_I, 720, 576, 132, 44, STM_576I_50_STANDARDS, {PIX_AR_16_15, PIX_AR_64_45}, {0,0} },
    { 864, 625, 13500000, false, 63, false, 3}
  },
  { STVTG_TIMING_MODE_576P50000_27000,
    { 50000, SCAN_P, 720, 576, 132, 44, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE293M), {PIX_AR_16_15, PIX_AR_64_45}, {17,18} },
    { 864, 625, 27000000, false, 64, false, 5}
  },
  { STVTG_TIMING_MODE_576I50000_14750,
    { 50000, SCAN_I, 768, 576, 155, 44, STM_576I_50_STANDARDS, {PIX_AR_SQUARE, PIX_AR_NULL}, {0,0} },
    { 944, 625, 14750000, false, 71, false, 3}
  },

  /*
   * 1080p modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { STVTG_TIMING_MODE_1080P60000_148500,
    { 60000, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,16} },
    { 2200, 1125, 148500000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P59940_148352,
    { 59940, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,16} },
    { 2200, 1125, 148351648, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P50000_148500,
    { 50000, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,31} },
    { 2640, 1125, 148500000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P30000_74250,
    { 30000, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,34} },
    { 2200, 1125,  74250000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P29970_74176,
    { 29970, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,34} },
    { 2200, 1125,  74175824, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P25000_74250,
    { 25000, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,33} },
    { 2640, 1125,  74250000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P24000_74250,
    { 24000, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,32} },
    { 2750, 1125,  74250000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080P23976_74176,
    { 23976, SCAN_P, 1920, 1080, 192, 41, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,32} },
    { 2750, 1125,  74175824, true, 44, true, 5}
  },

  /*
   * 1080i modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { STVTG_TIMING_MODE_1080I60000_74250,
    { 60000, SCAN_I, 1920, 1080, 192, 40, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,5} },
    { 2200, 1125,  74250000, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080I59940_74176,
    { 59940, SCAN_I, 1920, 1080, 192, 40, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,5} },
    { 2200, 1125,  74175824, true, 44, true, 5}
  },
  { STVTG_TIMING_MODE_1080I50000_74250_274M,
    { 50000, SCAN_I, 1920, 1080, 192, 40, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE274M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,20} },
    { 2640, 1125,  74250000, true, 44, true, 5}
  },
  /* Australian 1080i. */
  { STVTG_TIMING_MODE_1080I50000_72000,
    { 50000, SCAN_I, 1920, 1080, 352, 124, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_AS4933), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,39} },
    { 2304, 1250,  72000000, true, 168, false,  5}
  },

  /*
   * 720p modes, SMPTE 296M (analogue) and CEA-861-C (HDMI)
   */
  { STVTG_TIMING_MODE_720P60000_74250,
    { 60000, SCAN_P, 1280,  720, 260, 25, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,4} },
    { 1650,  750,  74250000, true, 40, true, 5}
  },
  { STVTG_TIMING_MODE_720P59940_74176,
    { 59940, SCAN_P, 1280,  720, 260, 25, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,4} },
    { 1650,  750,  74175824, true, 40, true, 5}
  },
  { STVTG_TIMING_MODE_720P50000_74250,
    { 50000, SCAN_P, 1280,  720, 260, 25, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_SMPTE296M), {PIX_AR_NULL, PIX_AR_SQUARE}, {0,19} },
    { 1980,  750,  74250000, true,  40, true, 5}
  },

  /*
   * A 1280x1152@50Hz Australian analogue HD mode.
   */
  { STVTG_TIMING_MODE_1152I50000_48000,
    { 50000, SCAN_I, 1280, 1152, 235, 178, STM_OUTPUT_STD_AS4933, {PIX_AR_NULL, PIX_AR_NULL}, {0,0} },
    { 1536, 1250,  48000000, true, 44, true, 5}
  },

  /*
   * good old VGA, the default fallback mode for HDMI displays, note that
   * the same video code is used for 4x3 and 16x9 displays and it is up to
   * the display to determine how to present it (see CEA-861-C). Also note
   * that CEA specifies the pixel aspect ratio is 1:1 .
   */
  { STVTG_TIMING_MODE_480P59940_25180,
    { 59940, SCAN_P, 640, 480, 144, 35, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_VESA), {PIX_AR_SQUARE, PIX_AR_NULL}, {1,0} },
    { 800, 525, 25174800, false, 96, false, 2}
  },
  { STVTG_TIMING_MODE_480P60000_25200,
    { 60000, SCAN_P, 640, 480, 144, 35, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_VESA), {PIX_AR_SQUARE, PIX_AR_NULL}, {1,0} },
    { 800, 525, 25200000, false, 96, false, 2}
  },

  /*
   * CEA861 pixel repeated modes for SD and ED modes. These can only be set on the
   * HDMI output, to match a particular analogue mode on the master output.
   *
   * Note: that pixel aspect ratios are not given for the 2880 modes as this is
   * variable dependent on the pixel repeat.
   */
  { STVTG_TIMING_MODE_480I59940_27000,
    { 59940, SCAN_I, 1440, 480, 238, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X), {PIX_AR_8_9, PIX_AR_32_27}, {6,7} },
    { 1716, 525, 27000000, false, 124, false, 3}
  },
  { STVTG_TIMING_MODE_576I50000_27000,
    { 50000, SCAN_I, 1440, 576, 264, 44, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X), {PIX_AR_16_15, PIX_AR_64_45}, {21,22} },
    { 1728, 625, 27000000, false, 126, false, 3}
  },
  { STVTG_TIMING_MODE_480I59940_54000,
    { 59940, SCAN_I, 2880, 480, 476, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X |STM_OUTPUT_STD_TMDS_PIXELREP_4X), {PIX_AR_NULL, PIX_AR_NULL}, {10,11} },
    { 3432, 525, 54000000, false, 248, false, 3}
  },
  { STVTG_TIMING_MODE_576I50000_54000,
    { 50000, SCAN_I, 2880, 576, 528, 44, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X |STM_OUTPUT_STD_TMDS_PIXELREP_4X), {PIX_AR_NULL, PIX_AR_NULL}, {25,26} },
    { 3456, 625, 54000000, false, 252, false, 3}
  },
  { STVTG_TIMING_MODE_480P59940_54000,
    { 59940, SCAN_P, 1440, 480, 244, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X), {PIX_AR_8_9, PIX_AR_32_27}, {14,15} },
    { 1716, 525, 54000000, false, 124, false, 6}
  },
  { STVTG_TIMING_MODE_480P60000_54054,
    { 60000, SCAN_P, 1440, 480, 244, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X), {PIX_AR_8_9, PIX_AR_32_27}, {14,15} },
    { 1716, 525, 54054000, false, 124, false, 6}
  },
  { STVTG_TIMING_MODE_576P50000_54000,
    { 50000, SCAN_P, 1440, 576, 264, 44, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X), {PIX_AR_16_15, PIX_AR_64_45}, {29,30} },
    { 1728, 625, 54000000, false, 128, false, 5}
  },
  { STVTG_TIMING_MODE_480P59940_108000,
    { 59940, SCAN_P, 2880, 480, 488, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X | STM_OUTPUT_STD_TMDS_PIXELREP_4X), {PIX_AR_NULL, PIX_AR_NULL}, {35,36} },
    { 3432, 525, 108000000, false, 248, false, 6}
  },
  { STVTG_TIMING_MODE_480P60000_108108,
    { 60000, SCAN_P, 2880, 480, 488, 36, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X | STM_OUTPUT_STD_TMDS_PIXELREP_4X), {PIX_AR_NULL, PIX_AR_NULL}, {35,36} },
    { 3432, 525, 108108000, false, 248, false, 6}
  },
  { STVTG_TIMING_MODE_576P50000_108000,
    { 50000, SCAN_P, 2880, 576, 528, 44, (STM_OUTPUT_STD_CEA861C | STM_OUTPUT_STD_TMDS_PIXELREP_2X | STM_OUTPUT_STD_TMDS_PIXELREP_4X), {PIX_AR_NULL, PIX_AR_NULL}, {37,38} },
    { 3456, 625, 108000000, false, 256, false, 5}
  }
};


COutput::COutput(CDisplayDevice* pDev, ULONG timingID)
{
  m_pDisplayDevice = pDev;
  m_pCurrentMode   = 0;
  m_ulTVStandard   = 0;
  m_ulInputSource  = 0;
  m_ulOutputFormat = 0;
  m_ulMaxPixClock  = 0;
  m_ulTimingID     = timingID;
  m_LastVTGSync    = STM_UNKNOWN_FIELD;

  m_LastVSyncTime      = (TIME64)0;
  m_fieldframeDuration = (TIME64)0;
  m_displayStatus      = STM_DISPLAY_DISCONNECTED;
  m_bIsSuspended       = false;

  m_signalRange    = STM_SIGNAL_FILTER_SAV_EAV;
}

COutput::~COutput(void) {}


const stm_mode_line_t* COutput::GetModeParamsLine(stm_display_mode_t mode) const
{
  ULONG i;

  for (i=0; i < (ULONG)STVTG_TIMING_MODE_COUNT; i++)
  {
    if (ModeParamsTable[i].Mode == mode)
    {
      if((m_ulMaxPixClock != 0) &&
         (ModeParamsTable[i].TimingParams.ulPixelClock > m_ulMaxPixClock))
      {
        return 0;
      }

      DEBUGF2(3,("COutput::GetModeParamsLine mode found.\n"));
      return SupportedMode(&ModeParamsTable[i]);
    }
  }

  DEBUGF2(3,("COutput::GetModeParamsLine no mode found.\n"));
  return 0;
}


const stm_mode_line_t* COutput::FindMode(ULONG ulXRes, ULONG ulYRes, ULONG ulMinLines, ULONG ulMinPixels, ULONG ulpixclock, stm_scan_type_t scanType) const
{
  int i;

  DEBUGF2(3,("COutput::FindMode x=%ld y=%ld pixclk=%ld interlaced=%s.\n",ulXRes,ulYRes,ulpixclock,(scanType==SCAN_I)?"true":"false"));

  for (i=0; i < STVTG_TIMING_MODE_COUNT; i++)
  {
    if(ModeParamsTable[i].ModeParams.ActiveAreaWidth != ulXRes)
      continue;

    if(ModeParamsTable[i].ModeParams.ActiveAreaHeight != ulYRes)
      continue;

    // Allow for some maths rounding errors in the pixel clock comparison
    static const int clockMargin = 6000;
    if(ulpixclock < ModeParamsTable[i].TimingParams.ulPixelClock-clockMargin ||
       ulpixclock > ModeParamsTable[i].TimingParams.ulPixelClock+clockMargin )
      continue;

    if((m_ulMaxPixClock != 0) && (ulpixclock > m_ulMaxPixClock+clockMargin))
      continue;

    if(ModeParamsTable[i].ModeParams.ScanType != scanType)
      continue;

    /*
     * Unfortunately 1080i@59.94Hz and 1080i@50Hz will both pass all the above
     * tests; so we need to check the total lines. To make this a bit easier
     * the input parameter is the minimum number of lines required to match the
     * mode. This allows someone to pass is 0, to match the first mode that
     * matches all the other more standard parameters.
     */
    if(ModeParamsTable[i].TimingParams.LinesByFrame < ulMinLines)
      continue;

    /*
     * Same idea as above but for the total number of pixels per line
     * this lets us distinguish between 720p@50Hz and 720p@60Hz
     */
    if(ModeParamsTable[i].TimingParams.PixelsPerLine < ulMinPixels)
      continue;

    DEBUGF2(3,("COutput::FindMode mode found.\n"));
    return SupportedMode(&ModeParamsTable[i]);
  }

  DEBUGF2(3,("COutput::FindMode no mode found.\n"));
  return 0;
}


const stm_mode_line_t* COutput::SupportedMode(const stm_mode_line_t *) const
{
  // Default for slaved outputs which do not support mode change
  return 0;
}


bool COutput::Start(const stm_mode_line_t* pMode, ULONG tvStandard)
{
  DENTRY();

  m_pCurrentMode       = pMode;
  m_ulTVStandard       = tvStandard;
  DEBUGF2(1,("%s: mode %d @ %p\n",__PRETTY_FUNCTION__,m_pCurrentMode->Mode,m_pCurrentMode));

  m_fieldframeDuration = GetFieldOrFrameDurationFromMode(m_pCurrentMode);

  DEBUGF2(2,("COutput::Start m_fieldframeDuration = %lld (us/ticks)\n",m_fieldframeDuration));
  DEXIT();
  return true;
}


bool COutput::Stop(void)
{
  m_pCurrentMode       = 0;
  m_ulTVStandard       = 0;
  m_fieldframeDuration = 0;
  m_LastVTGSync        = STM_UNKNOWN_FIELD;
  m_LastVSyncTime      = (TIME64)0;
  m_bIsSuspended       = false;
  return true;
}


void COutput::Suspend(void)
{
  m_bIsSuspended  = true;
  m_LastVTGSync   = STM_UNKNOWN_FIELD;
  m_LastVSyncTime = (TIME64)0;
}


void COutput::Resume(void)
{
  m_bIsSuspended = false;
}


stm_meta_data_result_t COutput::QueueMetadata(stm_meta_data_t *) { return STM_METADATA_RES_UNSUPPORTED_TYPE; }

void COutput::FlushMetadata(stm_meta_data_type_t) {}

void COutput::UpdateHW() {}

void COutput::SetClockReference(stm_clock_ref_frequency_t refClock, int error_ppm) {}

bool COutput::SetFilterCoefficients(const stm_display_filter_setup_t *) { return false; }

void COutput::SoftReset() {}

TIME64 COutput::GetFieldOrFrameDurationFromMode(const stm_mode_line_t *mode)
{
  /*
   * This looks slightly mad, but it is to get around not having a signed 64bit
   * divide in Linux.
   */
  return ((UTIME64)g_pIOS->GetOneSecond()*1000LL)/(UTIME64)mode->ModeParams.FrameRate;
}

////////////////////////////////////////////////////////////////////////////
// C Display output interface
//

extern "C" {

static int get_output_caps(stm_display_output_t *output, ULONG *caps)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  *caps = pOut->GetCapabilities();

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static const stm_mode_line_t* get_mode(stm_display_output_t *output, stm_display_mode_t mode)
{
  COutput *pOut =(COutput *)(output->handle);
  const stm_mode_line_t *mode_line;

  DEBUGF2(3,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return 0;

  mode_line = pOut->GetModeParamsLine(mode);

  g_pIOS->UpSemaphore(output->lock);

  return mode_line;
}


static const stm_mode_line_t* find_mode(stm_display_output_t *output, ULONG xres, ULONG yres, ULONG minlines, ULONG minpixels, ULONG pixclock, stm_scan_type_t scantype)
{
  COutput *pOut =(COutput *)(output->handle);
  const stm_mode_line_t *mode_line;

  DEBUGF2(3,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return 0;

  mode_line = pOut->FindMode(xres, yres, minlines, minpixels, pixclock, scantype);

  g_pIOS->UpSemaphore(output->lock);

  return mode_line;
}


static int start_output(stm_display_output_t *output, const stm_mode_line_t *pModeLine, ULONG ulTVStandard)
{
  COutput *pOut =(COutput *)(output->handle);
  int ret;

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  ret = pOut->Start(pModeLine, ulTVStandard)?0:-1;

  g_pIOS->UpSemaphore(output->lock);

  return ret;
}


static int stop_output(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);
  int ret;

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  ret = pOut->Stop()?0:-1;

  g_pIOS->UpSemaphore(output->lock);

  return ret;
}


static int suspend_output(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->Suspend();

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int resume_output(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->Resume();

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int get_control_capabilities(stm_display_output_t *output, ULONG *ctrlCaps)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  *ctrlCaps = pOut->SupportedControls();

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int set_control(stm_display_output_t *output, stm_output_control_t ctrl, ULONG newVal)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(3,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->SetControl(ctrl, newVal);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int get_control(stm_display_output_t *output, stm_output_control_t ctrl, ULONG *ctrlVal)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  *ctrlVal = pOut->GetControl(ctrl);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int queue_metadata(stm_display_output_t *output, stm_meta_data_t *data, stm_meta_data_result_t *res)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  *res = pOut->QueueMetadata(data);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int flush_metadata(stm_display_output_t *output, stm_meta_data_type_t type)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(3,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->FlushMetadata(type);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int enable_background(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->ShowPlane(OUTPUT_BKG);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int disable_background(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->HidePlane(OUTPUT_BKG);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static const stm_mode_line_t* get_current_display_mode(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);
  const stm_mode_line_t *mode;

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return 0;

  mode = pOut->GetCurrentDisplayMode();

  g_pIOS->UpSemaphore(output->lock);

  return mode;
}


static int get_current_tv_standard(stm_display_output_t *output, ULONG *standard)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  *standard = pOut->GetCurrentTVStandard();

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int set_clock_reference(stm_display_output_t *output, stm_clock_ref_frequency_t refClock, int refClockError)
{
  COutput *pOut =(COutput *)(output->handle);

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  pOut->SetClockReference(refClock,refClockError);

  g_pIOS->UpSemaphore(output->lock);

  return 0;
}


static int set_filter_coefficients(stm_display_output_t *output, const stm_display_filter_setup_t *f)
{
  COutput *pOut =(COutput *)(output->handle);
  int ret;

  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  if(g_pIOS->DownSemaphore(output->lock) != 0)
    return -1;

  ret = pOut->SetFilterCoefficients(f)?0:-1;

  g_pIOS->UpSemaphore(output->lock);

  return ret;
}


static void handle_interrupts(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  pOut->HandleInterrupts();
}


static void soft_reset(stm_display_output_t *output)
{
  COutput *pOut =(COutput *)(output->handle);

  pOut->SoftReset();
}


static void get_last_vsync_info(stm_display_output_t *output, stm_field_t *field, TIME64 *interval)
{
  COutput *pOut =(COutput *)(output->handle);

  /*
   * This may be called from interrupt so don't take the lock.
   * Note there is a clear race condition here, you might get
   * a vsync between reading the field and the time. However
   * this is not very damaging, so we are leaving it for the moment.
   */
  *field    = pOut->GetCurrentVTGVSync();
  *interval = pOut->GetCurrentVSyncTime();
}


static void get_display_status(stm_display_output_t *output, stm_display_status_t *status)
{
  COutput *pOut =(COutput *)(output->handle);

  /*
   * Note that this is called from the primary interrupt handler so we
   * must not take the mutex.
   */
  *status = pOut->GetDisplayStatus();
}


static void set_display_status(stm_display_output_t *output, stm_display_status_t status)
{
  COutput *pOut =(COutput *)(output->handle);

  /*
   * Note that this is called from the primary interrupt handler so we
   * must not take the mutex.
   */
  pOut->SetDisplayStatus(status);
}


static void release_output(stm_display_output_t *output)
{
  DEBUGF2(2,("%s: output = %p\n",__FUNCTION__,output));

  /*
   * Nothing to do for output handles, just release the
   * structure's memory.
   */
  delete output;
}


stm_display_output_ops_t stm_display_output_ops = {
  GetCapabilities        : get_output_caps,

  GetDisplayMode         : get_mode,
  FindDisplayMode        : find_mode,

  Start                  : start_output,
  Stop                   : stop_output,

  Suspend                : suspend_output,
  Resume                 : resume_output,

  GetCurrentDisplayMode  : get_current_display_mode,
  GetCurrentTVStandard   : get_current_tv_standard,

  GetControlCapabilities : get_control_capabilities,
  SetControl             : set_control,
  GetControl             : get_control,

  QueueMetadata          : queue_metadata,
  FlushMetadata          : flush_metadata,

  SetFilterCoefficients  : set_filter_coefficients,

  EnableBackground       : enable_background,
  DisableBackground      : disable_background,

  SetClockReference      : set_clock_reference,

  SoftReset              : soft_reset,

  HandleInterrupts       : handle_interrupts,

  GetLastVSyncInfo       : get_last_vsync_info,
  GetDisplayStatus       : get_display_status,
  SetDisplayStatus       : set_display_status,

  Release                : release_output
};

} // extern "C"
