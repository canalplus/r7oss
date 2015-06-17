/***********************************************************************
 *
 * File: display/ip/tvout/stmvip.cpp
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

#include "stmvip.h"

#define TVO_VIP_CTL_OFFSET                      0x00                    /* VIP Configuration Register */
#define TVO_VIP_REORDER_R_SHIFT                 24
#define TVO_VIP_REORDER_G_SHIFT                 20
#define TVO_VIP_REORDER_B_SHIFT                 16
#define TVO_VIP_REORDER_WIDTH                   2
#define TVO_VIP_REORDER_Y_G_SEL                 0
#define TVO_VIP_REORDER_CB_B_SEL                1
#define TVO_VIP_REORDER_CR_R_SEL                2
#define TVO_VIP_CLIP_SHIFT                      8
#define TVO_VIP_CLIP_WIDTH                      3
#define TVO_VIP_CLIP_DISABLED                   0
#define TVO_VIP_CLIP_EAV_SAV                    1
#define TVO_VIP_CLIP_LIMITED_RANGE_RGB_Y        2
#define TVO_VIP_CLIP_LIMITED_RANGE_CB_CR        3
#define TVO_VIP_CLIP_PROG_RANGE                 4
#define TVO_VIP_RND_SHIFT                       4
#define TVO_VIP_RND_WIDTH                       2
#define TVO_VIP_RND_8BIT_ROUNDED                0
#define TVO_VIP_RND_10BIT_ROUNDED               1
#define TVO_VIP_RND_12BIT_ROUNDED               2

#define TVO_VIP_SEL_INPUT_SHIFT                 0
#define TVO_VIP_SEL_INPUT_WIDTH                 4
#define TVO_VIP_SEL_INPUT_MAIN                  0
#define TVO_VIP_SEL_INPUT_AUX                   8
#define TVO_VIP_SEL_INPUT_CONVERTED             0
#define TVO_VIP_SEL_INPUT_BYPASSED              1
#define TVO_VIP_SEL_INPUT_MAIN_FILTERED_422     2
#define TVO_VIP_SEL_INPUT_DENC_DAC_123          13
#define TVO_VIP_SEL_INPUT_DENC_DAC_456          14
#define TVO_VIP_SEL_INPUT_FORCE_COLOR           15

#define TVO_VIP_FORCE_COLOR_0                   0x04                    /* CB_B and Y_G input Force */
#define TVO_VIP_FORCE_COLOR_1                   0x08                    /* CR_R input Force */
#define TVO_VIP_FORCE_COLOR_MASK                0xFFF                   /* Force color mask */
#define TVO_VIP_FORCE_COLOR_B_CB_SHIFT          0
#define TVO_VIP_FORCE_COLOR_G_Y_SHIFT           16
#define TVO_VIP_FORCE_COLOR_R_CR_SHIFT          0
#define TVO_VIP_FORCE_COLOR_WIDTH               12

#define TVO_VIP_CLIP_VALUE_B_CB                 0x0C                    /* Clip value range in case of B_CB */
#define TVO_VIP_CLIP_VALUE_G_Y                  0x10                    /* Clip value range in case of G_Y */
#define TVO_VIP_CLIP_VALUE_R_CR                 0x14                    /* Clip value range in case of R_CR */
#define TVO_VIP_CLIP_MIN_SHIFT                  0
#define TVO_VIP_CLIP_MAX_SHIFT                  16
#define TVO_VIP_CLIP_VALUE_WIDTH                12


#define TVO_VIP_SYNC_SEL_OFFSET                 0x18
#define TVO_VIP_SYNC_SEL_WIDTH                  5
#define TVO_VIP_SYNC_DENC_SHIFT                 0
#define TVO_VIP_SYNC_SDDCS_SHIFT                8
#define TVO_VIP_SYNC_SDVTG_SHIFT                16
#define TVO_VIP_SYNC_TTXT_SHIFT                 24
#define TVO_VIP_SYNC_HDF_SHIFT                  0
#define TVO_VIP_SYNC_HDDCS_SHIFT                8
#define TVO_VIP_SYNC_HDMI_SHIFT                 0
#define TVO_VIP_SYNC_DVO_SHIFT                  0
#define TVO_VIP_SYNC_SEL_OFFSET_MAIN            0
#define TVO_VIP_SYNC_SEL_OFFSET_AUX             (2 << 3)

#define TVO_VIP_DFV_OFFSET                      0x50                    /* Soft Reset */
#define TVO_VIP_SOFT_RESET                      (1L << 0)


int tvo_vip_sync_shift [TVO_VIP_SYNC_NBR] =
{
    TVO_VIP_SYNC_DENC_SHIFT             /* TVO_VIP_SYNC_DENC_IDX */
  , TVO_VIP_SYNC_SDDCS_SHIFT            /* TVO_VIP_SYNC_SDDCS_IDX */
  , TVO_VIP_SYNC_SDVTG_SHIFT            /* TVO_VIP_SYNC_SDVTG_IDX */
  , TVO_VIP_SYNC_TTXT_SHIFT             /* TVO_VIP_SYNC_TTXT_IDX */
  , TVO_VIP_SYNC_HDF_SHIFT              /* TVO_VIP_SYNC_HDF_IDX */
  , TVO_VIP_SYNC_HDDCS_SHIFT            /* TVO_VIP_SYNC_HDDCS_IDX */
  , TVO_VIP_SYNC_HDMI_SHIFT             /* TVO_VIP_SYNC_HDMI_IDX */
  , TVO_VIP_SYNC_DVO_SHIFT              /* TVO_VIP_SYNC_DVO_IDX */
};


CSTmVIP::CSTmVIP(CDisplayDevice                *pDev,
                 uint32_t                       ulVIPRegs,
                 uint32_t                       ulSyncType,
                 const stm_display_sync_id_t   *sync_sel_map,
                 uint32_t                       capabilities)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmVIP::CSTmVIP: pDev=%p - ulVIPRegs=0x%x", pDev, ulVIPRegs );

  m_pDevRegs       = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_ulVIPRegOffset = ulVIPRegs;

  m_ulSyncType   = ulSyncType;
  m_sync_sel_map = sync_sel_map;

  /*
   * Default channel reorder, no swaps. This can be overridden by calling
   * SetColorChannelOrder
   */
  m_ulRChannel = TVO_VIP_REORDER_CR_R_SEL;
  m_ulGChannel = TVO_VIP_REORDER_Y_G_SEL;
  m_ulBChannel = TVO_VIP_REORDER_CB_B_SEL;

  m_ulCapabilities = capabilities;

  /* Reset VIP Registers */
  m_ulVipCfg = 0;
  WriteReg(TVO_VIP_CTL_OFFSET, m_ulVipCfg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmVIP::~CSTmVIP() {}


void CSTmVIP::SoftReset(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteReg(TVO_VIP_DFV_OFFSET, TVO_VIP_SOFT_RESET);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmVIP::SetColorChannelOrder(tvo_vip_input_color_channel_t red_output,
                                   tvo_vip_input_color_channel_t green_output,
                                   tvo_vip_input_color_channel_t blue_output)
{
  m_ulRChannel = red_output;
  m_ulGChannel = green_output;
  m_ulBChannel = blue_output;
}

const char *CSTmVIP::source_names[] = {
        "MAIN"
      , "AUX"
      , "DENC123"
      , "DENC456"
      , "MAIN_FILTERED_422"
    };

bool CSTmVIP::SetInputParams(const tvo_vip_video_source_t source, uint32_t format, const stm_display_signal_range_t clipping)
{
  TRC( TRC_ID_MAIN_INFO, "- VIP Input is %s, format = 0x%x", source_names[source], format );

  uint32_t InputSel = 0;
  uint32_t Rounding = 0;
  uint32_t Clipping = 0;

  m_ulVipCfg = ReadReg(TVO_VIP_CTL_OFFSET);

  SetBitField(m_ulVipCfg, TVO_VIP_REORDER_R_SHIFT, TVO_VIP_REORDER_WIDTH, m_ulRChannel);
  SetBitField(m_ulVipCfg, TVO_VIP_REORDER_G_SHIFT, TVO_VIP_REORDER_WIDTH, m_ulGChannel);
  SetBitField(m_ulVipCfg, TVO_VIP_REORDER_B_SHIFT, TVO_VIP_REORDER_WIDTH, m_ulBChannel);

  switch(clipping)
  {
    case STM_SIGNAL_FULL_RANGE:
      TRC( TRC_ID_MAIN_INFO, "full range output (no clipping)" );
      Clipping = TVO_VIP_CLIP_DISABLED;
      break;
    case STM_SIGNAL_FILTER_SAV_EAV:
      TRC( TRC_ID_MAIN_INFO, "clip EAV/SAV" );
      Clipping = TVO_VIP_CLIP_EAV_SAV;
      break;
    case STM_SIGNAL_VIDEO_RANGE:
      if(format & STM_VIDEO_OUT_RGB)
      {
        TRC( TRC_ID_MAIN_INFO, "clip RGB/Y range" );
        Clipping = TVO_VIP_CLIP_LIMITED_RANGE_RGB_Y;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "clip CB/CR range" );
        Clipping = TVO_VIP_CLIP_LIMITED_RANGE_CB_CR;
      }
      break;
  }

  SetBitField(m_ulVipCfg, TVO_VIP_CLIP_SHIFT, TVO_VIP_CLIP_WIDTH, Clipping);

  switch (format & STM_VIDEO_OUT_DEPTH_MASK)
  {
    case STM_VIDEO_OUT_24BIT:
      TRC( TRC_ID_MAIN_INFO, "24bit color, rounding input to 8bit" );
      Rounding = TVO_VIP_RND_8BIT_ROUNDED;
      break;
    case STM_VIDEO_OUT_30BIT:
      TRC( TRC_ID_MAIN_INFO, "30bit color, rounding input to 10bit" );
      Rounding = TVO_VIP_RND_10BIT_ROUNDED;
      break;
    case STM_VIDEO_OUT_36BIT:
    case STM_VIDEO_OUT_48BIT:
      TRC( TRC_ID_MAIN_INFO, "36/48bit color, rounding input to 12bit" );
      Rounding = TVO_VIP_RND_12BIT_ROUNDED;
      break;
    default:
      TRC( TRC_ID_MAIN_INFO, "Unsupported colordepth rounding input to 8bit" );
      Rounding = TVO_VIP_RND_8BIT_ROUNDED;
      break;
  }
  SetBitField(m_ulVipCfg, TVO_VIP_RND_SHIFT      , TVO_VIP_RND_WIDTH    , Rounding);

  switch(source)
  {
    case TVO_VIP_MAIN_VIDEO:
      InputSel |= TVO_VIP_SEL_INPUT_MAIN;

      if(  ( (m_ulCapabilities & TVO_VIP_BYPASS_INPUT_IS_RGB) && (format & STM_VIDEO_OUT_RGB))
        || (!(m_ulCapabilities & TVO_VIP_BYPASS_INPUT_IS_RGB) && (format & STM_VIDEO_OUT_YUV)) )
      {
        if(m_ulCapabilities & TVO_VIP_INPUT_IS_INVERTED)
          InputSel &= ~TVO_VIP_SEL_INPUT_BYPASSED;
        else
          InputSel |= TVO_VIP_SEL_INPUT_BYPASSED;
      }
      else
      {
        if(m_ulCapabilities & TVO_VIP_INPUT_IS_INVERTED)
          InputSel |= TVO_VIP_SEL_INPUT_BYPASSED;
        else
          InputSel &= ~TVO_VIP_SEL_INPUT_BYPASSED;
      }

      SelectSync(TVO_VIP_MAIN_SYNCS);

      break;
    case TVO_VIP_AUX_VIDEO:
      InputSel |= TVO_VIP_SEL_INPUT_AUX;

      if(  ( (m_ulCapabilities & TVO_VIP_BYPASS_INPUT_IS_RGB) && (format & STM_VIDEO_OUT_RGB))
        || (!(m_ulCapabilities & TVO_VIP_BYPASS_INPUT_IS_RGB) && (format & STM_VIDEO_OUT_YUV)) )
      {
        if(m_ulCapabilities & TVO_VIP_INPUT_IS_INVERTED)
          InputSel &= ~TVO_VIP_SEL_INPUT_BYPASSED;
        else
          InputSel |= TVO_VIP_SEL_INPUT_BYPASSED;
      }
      else
      {
        if(m_ulCapabilities & TVO_VIP_INPUT_IS_INVERTED)
          InputSel |= TVO_VIP_SEL_INPUT_BYPASSED;
        else
          InputSel &= ~TVO_VIP_SEL_INPUT_BYPASSED;
      }

      SelectSync(TVO_VIP_AUX_SYNCS);

      break;
    case TVO_VIP_DENC123:
      InputSel |= TVO_VIP_SEL_INPUT_DENC_DAC_123;
      /*
       * Note the DENC output is already in the required YCbCr or RGB format
       * The syncs will be set by the caller in this case because they may be
       * either Main or Aux depending on the DENC usage.
       */
      break;
    case TVO_VIP_DENC456:
      InputSel |= TVO_VIP_SEL_INPUT_DENC_DAC_456;
      break;
    case TVO_VIP_MAIN_FILTERED_422:
      if(!(m_ulCapabilities & TVO_VIP_HAS_MAIN_FILTERED_422_INPUT) || !(format & STM_VIDEO_OUT_422))
        return false;

      InputSel |= TVO_VIP_SEL_INPUT_MAIN_FILTERED_422;
      SelectSync(TVO_VIP_MAIN_SYNCS);
      break;
  }

  SetBitField(m_ulVipCfg, TVO_VIP_SEL_INPUT_SHIFT, TVO_VIP_SEL_INPUT_WIDTH, InputSel);

  TRC( TRC_ID_MAIN_INFO, "- VipCfg = 0x%x", m_ulVipCfg );

  WriteReg(TVO_VIP_CTL_OFFSET, m_ulVipCfg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmVIP::SelectSync(const tvo_vip_sync_source_t sync_source)
{
  TRC( TRC_ID_MAIN_INFO, "- Input is %s", ((sync_source == TVO_VIP_MAIN_SYNCS)? "Main":"Aux") );

  int      idx = 0;
  uint32_t SyncSel;
  uint32_t SyncSelMainOrAux;

  SyncSel = ReadReg(TVO_VIP_SYNC_SEL_OFFSET);

  SyncSelMainOrAux = (sync_source == TVO_VIP_MAIN_SYNCS)?TVO_VIP_SYNC_SEL_OFFSET_MAIN:TVO_VIP_SYNC_SEL_OFFSET_AUX;

  for(idx=0; idx < TVO_VIP_SYNC_NBR; idx++)
  {
    if(m_ulSyncType & (1 << idx))
      SetBitField(SyncSel, tvo_vip_sync_shift[idx], TVO_VIP_SYNC_SEL_WIDTH, (SyncSelMainOrAux | m_sync_sel_map[idx]));
  }

  TRC( TRC_ID_MAIN_INFO, "- SyncSel = 0x%x", SyncSel );

  WriteReg(TVO_VIP_SYNC_SEL_OFFSET, SyncSel);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmVIP::SetForceColor(const bool EnableFC, uint32_t R_Cr, uint32_t G_Y, uint32_t B_Cb)
{
  TRC( TRC_ID_MAIN_INFO, "- HDF ForceColor is %s, R_Cr=0x%x G_Y=0x%x B_Cb=0x%x", (EnableFC? "Enabled":"Disabled"), R_Cr, G_Y, B_Cb );

  uint32_t VipCfg;
  uint32_t force_color_0 = 0;
  uint32_t force_color_1 = 0;

  VipCfg = m_ulVipCfg;

  if(EnableFC)
  {
    SetBitField(force_color_1, TVO_VIP_FORCE_COLOR_R_CR_SHIFT, TVO_VIP_FORCE_COLOR_WIDTH  , R_Cr);
    SetBitField(force_color_0, TVO_VIP_FORCE_COLOR_G_Y_SHIFT , TVO_VIP_FORCE_COLOR_WIDTH  , G_Y );
    SetBitField(force_color_0, TVO_VIP_FORCE_COLOR_B_CB_SHIFT, TVO_VIP_FORCE_COLOR_WIDTH  , B_Cb);
    SetBitField(VipCfg       , TVO_VIP_SEL_INPUT_SHIFT   , TVO_VIP_SEL_INPUT_WIDTH, TVO_VIP_SEL_INPUT_FORCE_COLOR);
  }

  WriteReg(TVO_VIP_CTL_OFFSET      , VipCfg);
  WriteReg(TVO_VIP_FORCE_COLOR_0, force_color_0);
  WriteReg(TVO_VIP_FORCE_COLOR_1, force_color_1);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
