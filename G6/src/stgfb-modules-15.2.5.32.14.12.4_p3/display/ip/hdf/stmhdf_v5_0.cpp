/***********************************************************************
 *
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

#include "stmhdf_v5_0.h"

/*
 * Specific HDF registers
 */
#define HDF_ANA_CFG                         0x000
#define HDF_ANA_Y_SCALE                     0x004
#define HDF_ANA_CB_SCALE                    0x008
#define HDF_ANA_CR_SCALE                    0x00C

/*
 * AWG Settings
 */
#define HDF_PBPR_SYNC_OFFSET_SHIFT         16
#define HDF_PBPR_SYNC_OFFSET_MASK          (0x7FF << HDF_PBPR_SYNC_OFFSET_SHIFT)
#define HDF_ANA_PREFILTER_EN               (0x1 << 9)
#define HDF_ANA_SYNC_ON_PBPR               (0x1 << 8)
#define HDF_AWG_ASYNC_FILTER_MODE_SHIFT    4
#define HDF_AWG_ASYNC_FILTER_MODE_MASK     (0x7 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_ANA_CFG_SYNC_FILTER_DEL_SD     (0 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_ANA_CFG_SYNC_FILTER_DEL_ED     (1 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_ANA_CFG_SYNC_FILTER_DEL_HD     (2 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_ANA_CFG_SYNC_FILTER_DEL_BYPASS (3 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_ANA_CFG_SYNC_FILTER_DEL_4TAP   (4 << HDF_AWG_ASYNC_FILTER_MODE_SHIFT)
#define HDF_AWG_SYNC_DEL                   (1L << 3)
#define HDF_AWG_ASYNC_VSYNC_FIELD_FRAME    (1L << 2)
#define HDF_AWG_ASYNC_HSYNC_MTD            (1L << 1)
#define HDF_AWG_ASYNC_EN                   (1L << 0)

/*
 * Specific TVO registers
 */
#define TVO_LUMA_SRC_CFG                    0x340
#define TVO_LUMA_COEFF_P1_T123              0x344
#define TVO_LUMA_COEFF_P1_T456              0x348
#define TVO_LUMA_COEFF_P2_T123              0x34C
#define TVO_LUMA_COEFF_P2_T456              0x350
#define TVO_LUMA_COEFF_P3_T123              0x354
#define TVO_LUMA_COEFF_P3_T456              0x358
#define TVO_LUMA_COEFF_P4_T123              0x35C
#define TVO_LUMA_COEFF_P4_T456              0x360

#define TVO_CHROMA_SRC_CFG                  0x364
#define TVO_CHROMA_COEFF_P1_T123            0x368
#define TVO_CHROMA_COEFF_P1_T456            0x36C
#define TVO_CHROMA_COEFF_P2_T123            0x370
#define TVO_CHROMA_COEFF_P2_T456            0x374
#define TVO_CHROMA_COEFF_P3_T123            0x378
#define TVO_CHROMA_COEFF_P3_T456            0x37C
#define TVO_CHROMA_COEFF_P4_T123            0x380
#define TVO_CHROMA_COEFF_P4_T456            0x384


#define TVO_ANA_SRC_CFG_2X                  (0L)
#define TVO_ANA_SRC_CFG_4X                  (1L)
#define TVO_ANA_SRC_CFG_8X                  (2L)
#define TVO_ANA_SRC_CFG_DISABLE             (3L)

#define TVO_ANA_SRC_CFG_DIV_SHIFT           2
#define TVO_ANA_SRC_CFG_DIV_256             (0L<<TVO_ANA_SRC_CFG_DIV_SHIFT)
#define TVO_ANA_SRC_CFG_DIV_512             (1L<<TVO_ANA_SRC_CFG_DIV_SHIFT)
#define TVO_ANA_SRC_CFG_DIV_1024            (2L<<TVO_ANA_SRC_CFG_DIV_SHIFT)
#define TVO_ANA_SRC_CFG_DIV_2048            (3L<<TVO_ANA_SRC_CFG_DIV_SHIFT)
#define TVO_ANA_SRC_CFG_DIV_MASK            (3L<<TVO_ANA_SRC_CFG_DIV_SHIFT)

#define TVO_ANA_SRC_CFG_BYPASS              (1L<<4)


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


static stm_display_hdf_filter_setup_t default_alt_2x_chroma_filter = {
  STM_FLT_DIV_512,
  {
    0x001305F7,
    0x05274BD0,
    0x0175,
    0x00000000,
    0x00000000,
    0x0004907C,
    0x09C80B9D,
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


CSTmHDF_V5_0::CSTmHDF_V5_0(
    CDisplayDevice      *pDev,
    uint32_t             TVOReg,
    uint32_t             TVFmt,
    uint32_t             AWGRam,
    uint32_t             AWGRamSize): CSTmHDFormatter(pDev, TVFmt)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_TVOReg                      = TVOReg;
  m_AWGRam                      = AWGRam;
  m_AWGRamSize                  = AWGRamSize;

  TRC( TRC_ID_MAIN_INFO, "CSTmHDF_V4_3::CSTmHDF_V4_3 - m_TVFmt     = 0x%x", m_TVFmt );

  m_bHasSeparateCbCrRescale     = true;
  m_bHas4TapSyncFilter          = true;
  /*
   * TVOUT Design Team : Use the alternative 2X Filters for New
   * HDFormatter IP versions. This will fix the multiburst attenuation
   * issue on Orly platform.
   */
  m_bUseAlternate2XFilter       = true;

  m_filters[HDF_COEFF_2X_LUMA]       = default_2x_filter;
  m_filters[HDF_COEFF_2X_CHROMA]     = default_2x_filter;
  m_filters[HDF_COEFF_2X_ALT_LUMA]   = default_alt_2x_luma_filter;
  m_filters[HDF_COEFF_2X_ALT_CHROMA] = default_alt_2x_chroma_filter;
  m_filters[HDF_COEFF_4X_LUMA]       = default_4x_filter;
  m_filters[HDF_COEFF_4X_CHROMA]     = default_4x_filter;

  /*
   * SDDACs will always run @108MHz so require a 4x upsample
   * (from 27MHz DENC output)
   */
  SetSDDACUpsampler(4);

  /*
   * Ensure the AWG is initially disabled.
   */
  uint32_t cfgana  = ReadHDFReg(HDF_ANA_CFG);

  cfgana &= ~( HDF_PBPR_SYNC_OFFSET_MASK | HDF_ANA_PREFILTER_EN | HDF_ANA_SYNC_ON_PBPR
             | HDF_AWG_ASYNC_FILTER_MODE_MASK | HDF_AWG_SYNC_DEL | HDF_AWG_ASYNC_VSYNC_FIELD_FRAME
             | HDF_AWG_ASYNC_HSYNC_MTD        | HDF_AWG_ASYNC_EN );

  WriteHDFReg(HDF_ANA_CFG,cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDF_V5_0::~CSTmHDF_V5_0() { }


void CSTmHDF_V5_0::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * SDDACs will always run @108MHz so require a 4x upsample
   * (from 27MHz DENC output)
   */
  SetSDDACUpsampler(4);

  /*
   * Ensure the AWG is initially disabled.
   */
  uint32_t cfgana  = ReadHDFReg(HDF_ANA_CFG);

  cfgana &= ~( HDF_PBPR_SYNC_OFFSET_MASK | HDF_ANA_PREFILTER_EN | HDF_ANA_SYNC_ON_PBPR
             | HDF_AWG_ASYNC_FILTER_MODE_MASK | HDF_AWG_SYNC_DEL | HDF_AWG_ASYNC_VSYNC_FIELD_FRAME
             | HDF_AWG_ASYNC_HSYNC_MTD        | HDF_AWG_ASYNC_EN );

  WriteHDFReg(HDF_ANA_CFG,cfgana);

  CSTmHDFormatter::Resume();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDF_V5_0::SetInputParams(const bool IsMainInput, uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  uint32_t cfgana;

  if(!IsStarted())
  {
    TRC( TRC_ID_ERROR, "HDFormatter Not yet Started !!" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "- HDF Input is %s, format = 0x%x", (IsMainInput? "Main":"Aux"), format );

  m_bMainInput     = IsMainInput;
  m_ulInputFormat  = format;

  cfgana  = ReadHDFReg(HDF_ANA_CFG);

  cfgana &= ~( HDF_PBPR_SYNC_OFFSET_MASK | HDF_ANA_PREFILTER_EN | HDF_ANA_SYNC_ON_PBPR
             | HDF_AWG_ASYNC_FILTER_MODE_MASK | HDF_AWG_SYNC_DEL | HDF_AWG_ASYNC_VSYNC_FIELD_FRAME
             | HDF_AWG_ASYNC_HSYNC_MTD        | HDF_AWG_ASYNC_EN );

  cfgana |= ((m_AWG_Y_C_Offset  << HDF_PBPR_SYNC_OFFSET_SHIFT) & HDF_PBPR_SYNC_OFFSET_MASK);

  if(m_CurrentMode.mode_params.scan_type == STM_PROGRESSIVE_SCAN)
    cfgana |=  HDF_AWG_ASYNC_VSYNC_FIELD_FRAME;

  if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_HD_MASK)
    cfgana |= HDF_ANA_CFG_SYNC_FILTER_DEL_HD;
  else if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_ED_MASK)
    cfgana |= HDF_ANA_CFG_SYNC_FILTER_DEL_ED;
  else if(m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
    cfgana |= HDF_ANA_CFG_SYNC_FILTER_DEL_SD;

  if(  (m_CurrentMode.mode_params.output_standards & STM_OUTPUT_STD_SMPTE293M)
    && (m_bHas4TapSyncFilter && m_CurrentMode.mode_timing.lines_per_frame == 525) )
    cfgana |= HDF_ANA_CFG_SYNC_FILTER_DEL_4TAP;

  WriteHDFReg(HDF_ANA_CFG, cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDF_V5_0::EnableAWG (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t     Cnt;

  TRC( TRC_ID_MAIN_INFO, "Loading AWG Ram" );
  for (Cnt=0; (Cnt < m_AWGRamSize); Cnt++)
    WriteAWGReg((Cnt*4), m_pHDFParams.ANARAMRegisters[Cnt]);

  uint32_t cfgana  = ReadHDFReg(HDF_ANA_CFG) | HDF_AWG_ASYNC_EN;
  TRC( TRC_ID_MAIN_INFO, "HDF_ANA_CFG: %.8x", cfgana );
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


void CSTmHDF_V5_0::DisableAWG (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* disable AWG, but leave state */
  uint32_t cfgana = ReadHDFReg (HDF_ANA_CFG) & ~(HDF_AWG_ASYNC_EN);
  WriteHDFReg (HDF_ANA_CFG, cfgana);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDF_V5_0::SetYCbCrReScalers(void)
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


void CSTmHDF_V5_0::SetSDDACUpsampler(const int multiple)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t val,srccfg;
  stm_filter_coeff_types_t luma,chroma;
  bool isRGB = (m_ulInputFormat & STM_VIDEO_OUT_RGB) != 0;

  switch(multiple)
  {
    case 2:
      if(!m_bUseAlternate2XFilter)
      {
        TRC( TRC_ID_MAIN_INFO, "2X" );
        luma   = HDF_COEFF_2X_LUMA;
        chroma = isRGB?HDF_COEFF_2X_LUMA:HDF_COEFF_2X_CHROMA;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "2X (ALT)" );
        luma   = HDF_COEFF_2X_ALT_LUMA;
        chroma = isRGB?HDF_COEFF_2X_ALT_LUMA:HDF_COEFF_2X_ALT_CHROMA;
      }
      srccfg = TVO_ANA_SRC_CFG_2X;
      break;
    case 4:
      TRC( TRC_ID_MAIN_INFO, "4X" );
      luma   = HDF_COEFF_4X_LUMA;
      chroma = isRGB?HDF_COEFF_4X_LUMA:HDF_COEFF_4X_CHROMA;
      srccfg = TVO_ANA_SRC_CFG_4X;
      break;
    case 1:
    default:
      TRC( TRC_ID_MAIN_INFO, "disabled/bypass" );
      WriteTVOReg(TVO_LUMA_SRC_CFG, (TVO_ANA_SRC_CFG_DISABLE|TVO_ANA_SRC_CFG_DIV_512|TVO_ANA_SRC_CFG_BYPASS));
      WriteTVOReg(TVO_CHROMA_SRC_CFG, TVO_ANA_SRC_CFG_DIV_512);
      return;
  }

  TRC( TRC_ID_MAIN_INFO, "luma filter index = %d chroma filter index = %d", luma, chroma );

  val = srccfg | (m_filters[luma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[luma].div << TVO_ANA_SRC_CFG_DIV_SHIFT);

  WriteTVOReg(TVO_LUMA_SRC_CFG, val);

  val = srccfg | (m_filters[chroma].coeff[HDF_COEFF_PHASE1_7] << 16) |
                 (m_filters[chroma].div << TVO_ANA_SRC_CFG_DIV_SHIFT);

  WriteTVOReg(TVO_CHROMA_SRC_CFG, val);

  WriteTVOReg(TVO_LUMA_COEFF_P1_T123, m_filters[luma].coeff[HDF_COEFF_PHASE1_123]);
  WriteTVOReg(TVO_LUMA_COEFF_P1_T456, m_filters[luma].coeff[HDF_COEFF_PHASE1_456]);
  WriteTVOReg(TVO_LUMA_COEFF_P2_T123, m_filters[luma].coeff[HDF_COEFF_PHASE2_123]);
  WriteTVOReg(TVO_LUMA_COEFF_P2_T456, m_filters[luma].coeff[HDF_COEFF_PHASE2_456]);
  WriteTVOReg(TVO_LUMA_COEFF_P3_T123, m_filters[luma].coeff[HDF_COEFF_PHASE3_123]);
  WriteTVOReg(TVO_LUMA_COEFF_P3_T456, m_filters[luma].coeff[HDF_COEFF_PHASE3_456]);
  WriteTVOReg(TVO_LUMA_COEFF_P4_T123, m_filters[luma].coeff[HDF_COEFF_PHASE4_123]);
  WriteTVOReg(TVO_LUMA_COEFF_P4_T456, m_filters[luma].coeff[HDF_COEFF_PHASE4_456]);

  WriteTVOReg(TVO_CHROMA_COEFF_P1_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE1_123]);
  WriteTVOReg(TVO_CHROMA_COEFF_P1_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE1_456]);
  WriteTVOReg(TVO_CHROMA_COEFF_P2_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE2_123]);
  WriteTVOReg(TVO_CHROMA_COEFF_P2_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE2_456]);
  WriteTVOReg(TVO_CHROMA_COEFF_P3_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE3_123]);
  WriteTVOReg(TVO_CHROMA_COEFF_P3_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE3_456]);
  WriteTVOReg(TVO_CHROMA_COEFF_P4_T123, m_filters[chroma].coeff[HDF_COEFF_PHASE4_123]);
  WriteTVOReg(TVO_CHROMA_COEFF_P4_T456, m_filters[chroma].coeff[HDF_COEFF_PHASE4_456]);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
