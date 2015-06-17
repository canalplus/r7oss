/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407hdmi.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/ip/displaytiming/stmclocklla.h>
#include <display/ip/displaytiming/stmvtg.h>

#include <display/ip/hdmi/stmhdmiregs.h>
#include <display/ip/hdmi/stmv29iframes.h>

#include <display/ip/tvout/stmvip.h>

#include "stiH407reg.h"
#include "stiH407device.h"
#include "stiH407hdmi.h"
#include "stiH407hdmiphy.h"

#define UNIPLAYER_SOFT_RST      0x0

#define UNIPLAYER_CTRL                 0x44
#define UNIPLAYER_CTRL_OPMASK          (0x7<<0)
#define UNIPLAYER_CTRL_OFF             (0)
#define UNIPLAYER_CTRL_MUTE_PCM        (0x1<<0)
#define UNIPLAYER_CTRL_MUTE_PAUSEBURST (0x2<<0)
#define UNIPLAYER_CTRL_AUDIO_DATA      (0x3<<0)
#define UNIPLAYER_CTRL_ENCODED         (0x4<<0)
#define UNIPLAYER_CTRL_CD              (0x5<<0)
#define UNIPLAYER_CTRL_DIV_ONE         (0x1<<5)
#define UNIPLAYER_CTRL_EN_SPDIF_FMT    (0x1<<17)

#define UNIPLAYER_FMT                  0x48
#define UNIPLAYER_FMT_NUM_CH_MASK      (0x7<<9)
#define UNIPLAYER_FMT_2CH              (0x1<<9)

#define TVO_HDMI_FORCE_COLOR_R_CR_MASK  (0xFF0000)
#define TVO_HDMI_FORCE_COLOR_G_Y_MASK   (0xFF00)
#define TVO_HDMI_FORCE_COLOR_B_CB_MASK  (0xFF)
#define TVO_HDMI_FORCE_COLOR_R_CR_SHIFT (16)  /* Force color R/Cr shift*/
#define TVO_HDMI_FORCE_COLOR_G_Y_SHIFT  (8)   /* Force color G/Y shift */
#define TVO_HDMI_FORCE_COLOR_B_CB_SHIFT (0)   /* Force color B/Cb shift*/

#define TVO_HDMI_FORCE_COLOR(x) ((x<<4)| (((x & 0xF0))>>4))

#define VTG_HDMI_SYNC_ID m_pTVOSyncMap[TVO_VIP_SYNC_HDMI_IDX]

#define STM_HDMI_AUD_CFG_CLK_128FS (1L<<1)
#define PCM0_DEFAULT_CLOCK_FREQUENCY    (48000*128)

static const int HDMI_DELAY     = 5;
static const int HDMI_422_FILTER_DELAY = 23; // Assuming the 23-tap filter is active

////////////////////////////////////////////////////////////////////////////////
//

CSTiH407HDMI::CSTiH407HDMI(
  CDisplayDevice            *pDev,
  COutput                   *pMainOutput,
  CSTmVTG                   *pVTG,
  CSTmVIP                   *pHDMIVIP,
  CSTmClockLLA              *pClkDivider,
const stm_display_sync_id_t *pSyncMap): CSTmHDMI("hdmi0",
                                                 STiH407_OUTPUT_IDX_HDMI,
                                                 pDev,
                                                 (stm_hdmi_hardware_features_t )(STM_HDMI_DEEP_COLOR | STM_HDMI_RX_SENSE),
                                                 STiH407_HDMI_BASE,
                                                 pMainOutput)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p: pDev = %p  main output = %p", this, pDev, pMainOutput );

  m_pVTG        = pVTG;
  m_pClkDivider = pClkDivider;
  m_pVIP        = pHDMIVIP;
  m_pTVOSyncMap = pSyncMap;

  m_uAudioClkOffset      = STiH407_CLKGEN_D_10;
  m_uUniplayerHDMIOffset = STiH407_UNIPLAYER_HDMI_BASE;

  /* Support of Ultra High Definition modes */
  m_ulCapabilities |= (OUTPUT_CAPS_UHD_DIGITAL);

  /* Disable hdmi internal decimation */
  m_hdmi_decimation_bypass=true;

  TRCOUT( TRC_ID_MAIN_INFO, "%p", this );
}


CSTiH407HDMI::~CSTiH407HDMI()
{
}


bool CSTiH407HDMI::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pIFrameManager = new CSTmV29IFrames(m_pDisplayDevice,m_uHDMIOffset);
  if(!m_pIFrameManager || !m_pIFrameManager->Create(this,m_pMasterOutput))
  {
    TRC( TRC_ID_MAIN_INFO, "Unable to create HDMI v2.9 Info Frame manager" );
    delete m_pIFrameManager;
    m_pIFrameManager = 0;
    return false;
  }

  m_pPHY = new CSTiH407HDMIPhy(m_pDisplayDevice);
  if(!m_pPHY)
    return false;

  if(!CSTmHDMI::Create())
    return false;

  /*
   * Do this last now we have a lock allocated.
   */
  SetAudioSource(STM_AUDIO_SOURCE_2CH_I2S);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407HDMI::SetVTGSyncs(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_pTVOSyncMap)
  {
    TRC( TRC_ID_ERROR, "- failed : Undefined Syncs mapping !!" );
    return false;
  }

  /*
   * VTG sync setup;
   */
  m_pVTG->SetSyncType(VTG_HDMI_SYNC_ID, STVTG_SYNC_POSITIVE);
  m_pVTG->SetBnottopType(VTG_HDMI_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);

  if((m_pVIP->GetCapabilities()&TVO_VIP_HAS_MAIN_FILTERED_422_INPUT) && (GetOutputFormat() & STM_VIDEO_OUT_422))
  {
    /*
     * Adaptative Decimation filter is active, 23-tap added for YUV422 output
     */
      m_pVTG->SetHSyncOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY+HDMI_422_FILTER_DELAY);
      m_pVTG->SetVSyncHOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY+HDMI_422_FILTER_DELAY);
  }
  else
  {
    /*
     * Default Delay
     */
    m_pVTG->SetHSyncOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY);
    m_pVTG->SetVSyncHOffset(VTG_HDMI_SYNC_ID, HDMI_DELAY);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;

}


void CSTiH407HDMI::DisableClocks(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Disabling HDMI Clocks" );
  m_pClkDivider->Disable(STM_CLK_PIX_HDMI);
  m_pClkDivider->Disable(STM_CLK_HDMI_PHY);
  m_pClkDivider->Disable(STM_CLK_TMDS_HDMI);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH407HDMI::PreConfiguration(const stm_display_mode_t *mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p", this );

  stm_clk_divider_output_divide_t divide;
  stm_clk_divider_output_source_t source;
  stm_clk_divider_output_name_t   pixelclock = (m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?STM_CLK_PIX_MAIN:STM_CLK_PIX_AUX;

  m_pClkDivider->getDivide(pixelclock,&divide);
  m_pClkDivider->getSource(pixelclock,&source);

  /*
   * HDMI pixel clock must be the same as main pixel clock.
   */
  m_pClkDivider->Enable(STM_CLK_TMDS_HDMI);
  m_pClkDivider->Enable(STM_CLK_PIX_HDMI, source, divide);
  m_pClkDivider->Enable(STM_CLK_HDMI_PHY, source, divide);

  /*
   * Configure VIP syncs up front.
   */
  m_pVIP->SelectSync(TVO_VIP_MAIN_SYNCS);
  return true;
}


bool CSTiH407HDMI::PostConfiguration(const stm_display_mode_t*)
{
  return true;
}


int CSTiH407HDMI::GetAudioFrequency(void)
{
  uint32_t  clock_rate = 0;
#ifdef CONFIG_ARCH_STI
  struct vibe_clk pcm0_clock = { 0, "CLK_PCM_HDMI", 0,0,0,0 };
#else
  struct vibe_clk pcm0_clock = { 0, "CLK_PCM_0", 0,0,0,0 };
#endif
  clock_rate = vibe_os_clk_get_rate(&pcm0_clock);
  if(!clock_rate)
    clock_rate = PCM0_DEFAULT_CLOCK_FREQUENCY;
  TRC( TRC_ID_HDMI, "PCM0 clock rate is %dKHz!", clock_rate );
  return (clock_rate);
}


void CSTiH407HDMI::GetAudioHWState(stm_hdmi_audio_state_t *state)
{
  uint32_t tmp = ReadUniplayerReg(UNIPLAYER_CTRL);
  switch(tmp & UNIPLAYER_CTRL_OPMASK)
  {
    case UNIPLAYER_CTRL_MUTE_PAUSEBURST:
    case UNIPLAYER_CTRL_AUDIO_DATA:
    case UNIPLAYER_CTRL_ENCODED:
      state->status = STM_HDMI_AUDIO_RUNNING;
      break;
    case UNIPLAYER_CTRL_MUTE_PCM: /* Stop on PCM mute */
    case UNIPLAYER_CTRL_OFF:
    default:
      state->status = STM_HDMI_AUDIO_STOPPED;
  }

  state->clock_divide = 2;
  state->clock_cts_divide = 1;
}


bool CSTiH407HDMI::SetOutputFormat(uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if ((m_pVIP->GetCapabilities()&TVO_VIP_HAS_MAIN_FILTERED_422_INPUT) && (format & STM_VIDEO_OUT_422 ))
  {
    m_pVIP->SetInputParams(TVO_VIP_MAIN_FILTERED_422, format, m_signalRange);
  }
  else
  {
    m_pVIP->SetInputParams(TVO_VIP_MAIN_VIDEO, format, m_signalRange);
  }

  /*
   * TODO: Get this out of here and out of the H415 and H416 HDMI classes and
   *       into the VIP class itself.
   *
   *       (a) we cannot have it copied every time there is a new SoC
   *       (b) it should be available on other output types, particularly DVO
   */
  if (m_bForceColor)
  {
    uint32_t R, ucRCr;
    uint32_t G, ucGY;
    uint32_t B, ucBCb;
    uint32_t tmp;
    stm_ycbcr_colorspace_t      m_colorspaceMode;

    R = (( m_uForcedOutputColor & TVO_HDMI_FORCE_COLOR_R_CR_MASK) >> TVO_HDMI_FORCE_COLOR_R_CR_SHIFT);
    G = (( m_uForcedOutputColor & TVO_HDMI_FORCE_COLOR_G_Y_MASK)  >> TVO_HDMI_FORCE_COLOR_G_Y_SHIFT );
    B = (( m_uForcedOutputColor & TVO_HDMI_FORCE_COLOR_B_CB_MASK) >> TVO_HDMI_FORCE_COLOR_B_CB_SHIFT);

    if ( (format & STM_VIDEO_OUT_YUV )== STM_VIDEO_OUT_YUV)
    {
      m_pMasterOutput->GetControl(OUTPUT_CTRL_YCBCR_COLORSPACE, &tmp);
      m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(tmp);

      if (m_colorspaceMode == STM_YCBCR_COLORSPACE_601)
      {
        ucGY =  (uint32_t)((299 * R + 587 * G + 114 * B)/1000);
        ucBCb = (uint32_t)(128 + (((5000 * B )/10000)-((1687 * R)/10000)-((3313 * G)/10000)));
        ucRCr = (uint32_t)(128 + (((5000 * R)/10000)-((4187 * G)/10000)-((813 * B)/10000)));
      }
      else
      {

        ucGY =  (uint32_t)((212 * R + 715 * G + 72 * B)/1000);
        ucBCb = (uint32_t)(128 + (((5000 * B )/10000)-((1145 * R)/10000)-((3854 * G)/10000)));
        ucRCr = (uint32_t)(128 + (((5000 * R)/10000)-((4541 * G)/10000)-((458 * B)/10000)));

      }
      m_pVIP->SetForceColor(true, TVO_HDMI_FORCE_COLOR(ucRCr), TVO_HDMI_FORCE_COLOR(ucGY), TVO_HDMI_FORCE_COLOR(ucBCb));
    }
    else
    {
      m_pVIP->SetForceColor(true, TVO_HDMI_FORCE_COLOR(R), TVO_HDMI_FORCE_COLOR(G), TVO_HDMI_FORCE_COLOR(B));
    }
  }
  else
  {
    m_pVIP->SetForceColor(false, 0, 0, 0);
  }

  if(!CSTmHDMI::SetOutputFormat(format))
    return false;

  return SetVTGSyncs(GetCurrentDisplayMode());
}


void CSTiH407HDMI::SetSignalRangeClipping(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * We need to defer to SetOutputFormat() rather than setting the VIP
   * directly, in order to maintain the forced color output if it is
   * currently enabled.
   */
  SetOutputFormat(m_ulOutputFormat);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


uint32_t CSTiH407HDMI::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  switch (ctrl)
  {
    case OUTPUT_CTRL_VIDEO_SOURCE_SELECT:
    {
      if(newVal != STM_VIDEO_SOURCE_MAIN_COMPOSITOR)
        return STM_OUT_INVALID_VALUE;

      break;
    }
    case OUTPUT_CTRL_FORCE_COLOR:
    {
      m_bForceColor = (newVal != 0);
      SetOutputFormat(m_ulOutputFormat);
      break;
    }
    case OUTPUT_CTRL_FORCED_RGB_VALUE:
    {
      m_uForcedOutputColor = newVal;
      SetOutputFormat(m_ulOutputFormat);
      break;
    }
    default:
      return CSTmHDMI::SetControl(ctrl, newVal);
  }

  return STM_OUT_OK;
}


bool CSTiH407HDMI::SetAudioSource(stm_display_output_audio_source_t source)
{
  uint32_t val;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Lock the updating of the audio config registers against the
   * HDMI interrupt audio update handling.
   */
  vibe_os_lock_resource(m_statusLock);

  if(m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR)
  {
    vibe_os_unlock_resource(m_statusLock);
    return false;
  }

  if(m_AudioSource == STM_AUDIO_SOURCE_SPDIF)
  {
    vibe_os_unlock_resource(m_statusLock);
    return false;
  }

  if(m_AudioSource != source)
  {
    InvalidateAudioPackets();

    val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~STM_HDMI_AUD_CFG_FIFO_MASK;

    switch(source)
    {
      case STM_AUDIO_SOURCE_2CH_I2S:
      {
        val &= ~STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        val |= STM_HDMI_AUD_CFG_FIFO_0_VALID;
        val |= STM_HDMI_AUD_CFG_FOUR_SP_PER_TRANSFER;
        val |= STM_HDMI_AUD_CFG_CLK_128FS;
        TRC( TRC_ID_MAIN_INFO, "- Set 2CH I2S Input" );
        break;
      }
      case STM_AUDIO_SOURCE_4CH_I2S:
      {
        val |= STM_HDMI_AUD_CFG_FIFO_01_VALID;
        val |= STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        val |= STM_HDMI_AUD_CFG_CLK_128FS;
        TRC( TRC_ID_MAIN_INFO, "- Set 4CH I2S Multichannel Input" );
        break;
      }
      case STM_AUDIO_SOURCE_6CH_I2S:
      {
        val |= STM_HDMI_AUD_CFG_FIFO_012_VALID;
        val |= STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        val |= STM_HDMI_AUD_CFG_CLK_128FS;
        TRC( TRC_ID_MAIN_INFO, "- Set 6CH I2S Multichannel Input" );
        break;
      }
      case STM_AUDIO_SOURCE_8CH_I2S:
      {
        val |= STM_HDMI_AUD_CFG_FIFO_0123_VALID;
        val |= STM_HDMI_AUD_CFG_8CH_NOT_2CH;
        val |= STM_HDMI_AUD_CFG_CLK_128FS;
        TRC( TRC_ID_MAIN_INFO, "- Set 8CH I2S Multichannel Input" );
        break;
      }
      default:
      {
        m_audioState.status = STM_HDMI_AUDIO_DISABLED;

        TRC( TRC_ID_MAIN_INFO, "- Audio Disabled" );

        vibe_os_unlock_resource(m_statusLock);
        return true;
      }
    }

    WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);

    m_audioState.status = STM_HDMI_AUDIO_STOPPED;
    m_AudioSource = source;
  }

  vibe_os_unlock_resource(m_statusLock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
