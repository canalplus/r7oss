/***********************************************************************
 *
 * File: display/ip/hdf/stmhdf_v4_3.cpp
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>

#include <display/ip/hdf/stmhdf.h>

#include "stmhdf_v4_3.h"

/*
 * Specific HDF registers
 */
#define HDF_ANA_CFG              0x000
#define HDF_ANA_YC_DELAY         0x004
#define HDF_ANA_Y_SCALE          0x008
#define HDF_ANA_C_SCALE          0x00C // Just CB in some chips
#define HDF_ANA_CB_SCALE         HDF_ANA_C_SCALE
#define HDF_ANA_CR_SCALE         0x038

/*
 * Note: the first set of definitions are also used for the Digital config
 */
#define ANA_CFG_INPUT_YCBCR            (0L)
#define ANA_CFG_INPUT_RGB              (1L)
#define ANA_CFG_INPUT_AUX              (2L)
#define ANA_CFG_INPUT_MASK             (3L)
#define ANA_CFG_RCTRL_G                (0L)
#define ANA_CFG_RCTRL_B                (1L)
#define ANA_CFG_RCTRL_R                (2L)
#define ANA_CFG_RCTRL_MASK             (3L)
#define ANA_CFG_REORDER_RSHIFT         (2L)
#define ANA_CFG_REORDER_GSHIFT         (4L)
#define ANA_CFG_REORDER_BSHIFT         (6L)
#define ANA_CFG_CLIP_EN                (1L<<8)
#define ANA_CFG_CLIP_GB_N_CRCB         (1L<<9)
/*
 * The rest are for analogue only
 */
#define ANA_CFG_PREFILTER_EN           (1L<<10)
#define ANA_CFG_SYNC_ON_PRPB           (1L<<11)
#define ANA_CFG_SEL_MAIN_TO_DENC       (1L<<12)
#define ANA_CFG_4TAP_SYNC_FILTER       (1L<<15)
#define ANA_CFG_PRPB_SYNC_OFFSET_SHIFT (16)
#define ANA_CFG_PRPB_SYNC_OFFSET_MASK  (0x7ff<<ANA_CFG_PRPB_SYNC_OFFSET_SHIFT)
#define ANA_CFG_SYNC_EN                (1L<<27)
#define ANA_CFG_SYNC_HREF_IGNORED      (1L<<28)
#define ANA_CFG_SYNC_FIELD_N_FRAME     (1L<<29)
#define ANA_CFG_SYNC_FILTER_DEL_SD     (0)
#define ANA_CFG_SYNC_FILTER_DEL_ED     (1L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_HD     (2L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_BYPASS (3L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_MASK   (3L<<30)

/*
 * AWG Settings
 */
#define AWG_CTRL_PREFILTER_EN           (1L<<10)
#define AWG_CTRL_SYNC_ON_PRPB           (1L<<11)
#define AWG_CTRL_SEL_MAIN_TO_DENC       (1L<<12)
#define AWG_CTRL_PRPB_SYNC_OFFSET_SHIFT (16)
#define AWG_CTRL_PRPB_SYNC_OFFSET_MASK  (0x7ff<<AWG_CTRL_PRPB_SYNC_OFFSET_SHIFT)
#define AWG_CTRL_SYNC_EN                (1L<<27)
#define AWG_CTRL_SYNC_HREF_IGNORED      (1L<<28)
#define AWG_CTRL_SYNC_FIELD_N_FRAME     (1L<<29)
#define AWG_CTRL_SYNC_FILTER_DEL_SD     (0)
#define AWG_CTRL_SYNC_FILTER_DEL_ED     (1L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_HD     (2L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_BYPASS (3L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_MASK   (3L<<30)


/*
 * Default Analogue HD formatter SRC coefficients.
 */
static stm_display_hdf_filter_setup_t default_2x_filter = {
  STM_FLT_DIV_256,
  {
    0x00FE83FB,
    0x1F900401,
    0x0113,
    0x00000000,
    0x00000000,
    0x00F408F9,
    0x055F7C25,
    0x00000000,
    0x00000000
  }
};


static stm_display_hdf_filter_setup_t default_alt_2x_luma_filter = {
  STM_FLT_DIV_512,
  {
    0x00FD7BFD,
    0x03A88613,
    0x08b,
    0x00000000,
    0x00000000,
    0x0001FBFA,
    0x0428BC29,
    0x00000000,
    0x00000000
  }
};


static stm_display_hdf_filter_setup_t default_4x_filter = {
  STM_FLT_DIV_512,
  {
    0x00fc827f,
    0x008fe20b,
    0x01ed,
    0x00f684fc,
    0x050f7c24,
    0x00f4857c,
    0x0a1f402e,
    0x00fa027f,
    0x0e076e1d
  }
};


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTmHDF_V4_3::CSTmHDF_V4_3(
    CDisplayDevice      *pDev,
    uint32_t             TVFmt,
    uint32_t             AWGRam,
    uint32_t             AWGRamSize,
    bool                 bHasSeparateCbCrRescale,
    bool                 bHas4TapSyncFilter): CSTmHDFormatter(pDev, TVFmt)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_AWGRam          = AWGRam;
  m_AWGRamSize      = AWGRamSize;

  TRC( TRC_ID_MAIN_INFO, "CSTmHDF_V4_3::CSTmHDF_V4_3 - m_TVFmt     = 0x%x", m_TVFmt );

  m_bHasSeparateCbCrRescale     = bHasSeparateCbCrRescale;
  m_bHas4TapSyncFilter          = bHas4TapSyncFilter;
  m_bUseAlternate2XFilter       = false;

  m_filters[HDF_COEFF_2X_LUMA]       = default_2x_filter;
  m_filters[HDF_COEFF_2X_CHROMA]     = default_2x_filter;
  m_filters[HDF_COEFF_2X_ALT_LUMA]   = default_alt_2x_luma_filter;
  m_filters[HDF_COEFF_2X_ALT_CHROMA] = default_2x_filter;
  m_filters[HDF_COEFF_4X_LUMA]       = default_4x_filter;
  m_filters[HDF_COEFF_4X_CHROMA]     = default_4x_filter;

  SetupDefaultDACRouting();

  /*
   * Ensure the AWG is initially disabled.
   */
  uint32_t val = ReadHDFReg (HDF_ANA_CFG) & ~(AWG_CTRL_SYNC_FILTER_DEL_MASK
                                          | AWG_CTRL_SYNC_FIELD_N_FRAME
                                          | AWG_CTRL_SYNC_HREF_IGNORED
                                          | AWG_CTRL_SYNC_EN);
  WriteHDFReg(HDF_ANA_CFG,val);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V4_3::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  SetupDefaultDACRouting();

  /*
   * Ensure the AWG is initially disabled.
   */
  uint32_t val = ReadHDFReg (HDF_ANA_CFG) & ~(AWG_CTRL_SYNC_FILTER_DEL_MASK
                                          | AWG_CTRL_SYNC_FIELD_N_FRAME
                                          | AWG_CTRL_SYNC_HREF_IGNORED
                                          | AWG_CTRL_SYNC_EN);
  WriteHDFReg(HDF_ANA_CFG,val);

  CSTmHDFormatter::Resume();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDF_V4_3::~CSTmHDF_V4_3() { }


void CSTmHDF_V4_3::SetSignalRangeClipping(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t cfgana;
  cfgana = ReadHDFReg(HDF_ANA_CFG) & ~ANA_CFG_CLIP_EN;
  if(m_signalRange == STM_SIGNAL_VIDEO_RANGE)
    cfgana |= ANA_CFG_CLIP_EN;
  WriteHDFReg(HDF_ANA_CFG, cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDF_V4_3::SetInputParams(const bool IsMainInput, const uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!IsStarted())
  {
    TRC( TRC_ID_ERROR, "HDFormatter Not yet Started !!" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "- HDF Input is %s, format = 0x%x", (IsMainInput? "Main":"Aux"), format );

  m_bMainInput     = IsMainInput;
  m_ulInputFormat  = format;

  uint32_t cfgana = ReadHDFReg(HDF_ANA_CFG);
  cfgana &= ~( ANA_CFG_INPUT_MASK
             | ANA_CFG_PRPB_SYNC_OFFSET_MASK
             | ANA_CFG_SYNC_EN
             | ANA_CFG_CLIP_EN
             | ANA_CFG_PREFILTER_EN
             | ANA_CFG_4TAP_SYNC_FILTER
             | ANA_CFG_SYNC_ON_PRPB
             | AWG_CTRL_SYNC_FIELD_N_FRAME
             | AWG_CTRL_SYNC_HREF_IGNORED
             | AWG_CTRL_SYNC_FILTER_DEL_MASK );

  if(m_bMainInput)
  {
    switch(m_ulInputFormat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV))
    {
      case STM_VIDEO_OUT_RGB:
      {
        cfgana &= ~ANA_CFG_INPUT_YCBCR;
        cfgana |= ANA_CFG_INPUT_RGB | ANA_CFG_CLIP_GB_N_CRCB;
        break;
      }
      case STM_VIDEO_OUT_YUV:
      {
        cfgana &= ~ANA_CFG_CLIP_GB_N_CRCB;
        cfgana |= ANA_CFG_INPUT_YCBCR;
        if((m_CurrentMode.mode_params.output_standards & (STM_OUTPUT_STD_VESA_MASK | STM_OUTPUT_STD_NTG5)) == 0)
        {
          cfgana |= (m_AWG_Y_C_Offset  << ANA_CFG_PRPB_SYNC_OFFSET_SHIFT) & ANA_CFG_PRPB_SYNC_OFFSET_MASK;
        }
        break;
      }
    }

    if(m_CurrentMode.mode_params.scan_type == STM_PROGRESSIVE_SCAN)
      cfgana |=  AWG_CTRL_SYNC_FIELD_N_FRAME;

    if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_HD_MASK)
      cfgana |= AWG_CTRL_SYNC_FILTER_DEL_HD;
    else if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_ED_MASK)
      cfgana |= AWG_CTRL_SYNC_FILTER_DEL_ED;
    else if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
      cfgana |= AWG_CTRL_SYNC_FILTER_DEL_SD;

    switch (m_CurrentMode.mode_id)
    {
      case STM_TIMING_MODE_1080I60000_74250:
      case STM_TIMING_MODE_1080I59940_74176:
      case STM_TIMING_MODE_1080I50000_74250_274M:
      // might be needed, but not at the moment... need some quality tests.
      //cfgana |= AWG_CTRL_SYNC_HREF_IGNORED;
      break;

      default:
      // nothing, but prevent compiler warning
      break;
    }

    /*
     * For 525line modes use the 4tap filter instead of the standard ED filter
     * if the chip has it in order to meet the correct rise and fall times.
     */
    if(m_bHas4TapSyncFilter && m_CurrentMode.mode_timing.lines_per_frame == 525)
      cfgana |= ANA_CFG_4TAP_SYNC_FILTER;

    if(m_signalRange == STM_SIGNAL_VIDEO_RANGE)
      cfgana |= ANA_CFG_CLIP_EN;

  }
  else
  {
    cfgana |= ANA_CFG_INPUT_AUX;
  }

  WriteHDFReg(HDF_ANA_CFG,cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDF_V4_3::SetDencSource(const bool IsMainInput)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  uint32_t cfgana = ReadHDFReg(HDF_ANA_CFG) & ~ANA_CFG_SEL_MAIN_TO_DENC;

  if(IsMainInput)
  {
    cfgana |= ANA_CFG_SEL_MAIN_TO_DENC;
  }

  WriteHDFReg(HDF_ANA_CFG,cfgana);

  TRC( TRC_ID_ERROR, "- Denc source is %s.", (IsMainInput? "Main":"Aux") );
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V4_3::SetYCbCrReScalers(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteHDFReg(HDF_ANA_Y_SCALE, m_pHDFParams.ANA_SCALE_CTRL_DAC_Y);
  WriteHDFReg(HDF_ANA_CB_SCALE, m_pHDFParams.ANA_SCALE_CTRL_DAC_Cb);
  if(m_bHasSeparateCbCrRescale)
    WriteHDFReg(HDF_ANA_CR_SCALE, m_pHDFParams.ANA_SCALE_CTRL_DAC_Cr);

#if defined (DEBUG_HDF_SYNC)
  TRC( TRC_ID_MAIN_INFO, "HDF_ANA_Y_SCALE  = %.8lx", ReadHDFReg(HDF_ANA_Y_SCALE) );
  TRC( TRC_ID_MAIN_INFO, "HDF_ANA_CB_SCALE = %.8lx", ReadHDFReg(HDF_ANA_CB_SCALE) );
  if(m_bHasSeparateCbCrRescale)
    TRC( TRC_ID_MAIN_INFO, "HDF_ANA_CR_SCALE = %.8lx", ReadHDFReg(HDF_ANA_CR_SCALE) );
#endif

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V4_3::EnableAWG (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t     Cnt;

  TRC( TRC_ID_MAIN_INFO, "Loading AWG Ram\n\t" );
  for (Cnt=0; (Cnt < m_AWGRamSize); Cnt++)
    WriteAWGReg((Cnt*4), m_pHDFParams.ANARAMRegisters[Cnt]);

  uint32_t cfgana  = ReadHDFReg(HDF_ANA_CFG) | ANA_CFG_SYNC_EN;
  TRC( TRC_ID_MAIN_INFO, "HDF_ANA_CFG: %.8lx", cfgana );
  WriteHDFReg(HDF_ANA_CFG, cfgana);

#if defined (DEBUG_HDF_SYNC)
  for (Cnt=0; (Cnt < m_AWGRamSize); Cnt++)
  {
    TRC( TRC_ID_MAIN_INFO, "%.8lx", ReadAWGReg(Cnt*4) );
    if(((((Cnt+1) % 8) == 0) && Cnt > 0) || (Cnt == (m_AWGRamSize-1)))
        TRC( TRC_ID_MAIN_INFO, "" );
    else
        TRC( TRC_ID_MAIN_INFO, ", " );
  }
#endif


  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V4_3::DisableAWG (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* disable AWG, but leave state */
  uint32_t cfgana = ReadHDFReg (HDF_ANA_CFG) & ~(AWG_CTRL_SYNC_EN);
  TRC( TRC_ID_MAIN_INFO, "HDF_ANA_CFG: %.8lx", cfgana );
  WriteHDFReg (HDF_ANA_CFG, cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V4_3::SetupDefaultDACRouting(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Default DAC routing and analogue output config
   */
  WriteHDFReg(HDF_ANA_CFG, (ANA_CFG_RCTRL_R << ANA_CFG_REORDER_RSHIFT) |
                           (ANA_CFG_RCTRL_G << ANA_CFG_REORDER_GSHIFT) |
                           (ANA_CFG_RCTRL_B << ANA_CFG_REORDER_BSHIFT));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
