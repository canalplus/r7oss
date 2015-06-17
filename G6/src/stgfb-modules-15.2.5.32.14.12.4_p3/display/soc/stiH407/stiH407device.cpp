/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407device.cpp
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
#include <display/generic/DisplaySource.h>

#include <display/ip/stmvtac.h>
#include <display/ip/displaytiming/stmfsynthlla.h>
#include <display/ip/displaytiming/stmclocklla.h>
#include <display/ip/displaytiming/stmc8vtg_v8_4.h>

#include <display/ip/tvout/stmvip.h>
#include <display/ip/tvout/stmtvoutteletext.h>
#include <display/ip/hdf/stmhdf_v5_0.h>

#include <display/ip/queuebufferinterface/PureSwQueueBufferInterface.h>

#include <display/ip/gdp/VBIPlane.h>

#include "stiH407reg.h"
#include "stiH407device.h"
#include "stiH407mainoutput.h"
#include "stiH407auxoutput.h"
#include "stiH407denc.h"
#include "stiH407hdmi.h"
#include "stiH407mixer.h"
#include "stiH407gdp.h"
#include "stiH407dvo.h"
#include "stiH407vdp.h"
#include "stiH407hqvdpliteplane.h"

static const bool STM_SYSTEM_RESUME_ON_HDMI_EVENTS = true;

static const int STM_C8VTG_NUM_SYNC_OUTPUTS = 6;

static const stm_display_sync_id_t  tvo_vip_sync_sel_map[] =
{
    STM_SYNC_SEL_1          /* TVO_VIP_SYNC_DENC_IDX */
  , STM_SYNC_SEL_2          /* TVO_VIP_SYNC_SDDCS_IDX */
  , STM_SYNC_SEL_REF        /* TVO_VIP_SYNC_SDVTG_IDX */
  , STM_SYNC_SEL_REF        /* TVO_VIP_SYNC_TTXT_IDX */
  , STM_SYNC_SEL_3          /* TVO_VIP_SYNC_HDF_IDX */
  , STM_SYNC_SEL_4          /* TVO_VIP_SYNC_HDDCS_IDX */
  , STM_SYNC_SEL_5          /* TVO_VIP_SYNC_HDMI_IDX */
  , STM_SYNC_SEL_6          /* TVO_VIP_SYNC_DVO_IDX */
};

static const stm_display_sync_id_t  tvo_vip_dvo_sync_sel_map[] =
{
    STM_SYNC_SEL_6          /* TVO_VIP_DVO_SYNC_DVO_IDX */
  , STM_SYNC_SEL_6          /* TVO_VIP_DVO_SYNC_PAD_HSYNC_IDX */
  , STM_SYNC_SEL_6          /* TVO_VIP_DVO_SYNC_PAD_VSYNCIDX */
  , STM_SYNC_SEL_6          /* TVO_VIP_DVO_SYNC_PAD_BNOT_IDX */
  , STM_SYNC_SEL_REF        /* TVO_VIP_DVO_SYNC_VGA_HSYNC_IDX */
  , STM_SYNC_SEL_REF        /* TVO_VIP_DVO_SYNC_VGA_VSYNC_IDX */
};

static const stm_display_sync_delay_t  tvo_vip_dvo_sync_delay_map[] =
{
    STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_IDX */
  , STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_HSYNC_IDX */
  , STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_VSYNC_IDX */
  , STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_BNOT_IDX */
  , STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_BNOT_IDX */
  , STM_SYNC_DELAY_NONE     /* TVO_VIP_SYNC_DVO_BNOT_IDX */
};


/* Clockgen D2 (Video display and output stage) */
static struct vibe_clk stih407_clock_fs[] = {
  { STM_CLK_SRC_HD,           "CLK_D2_FS0",       0, 0, 0, 297000000 },
  { STM_CLK_SRC_SD,           "CLK_D2_FS1",       0, 0, 0, 108000000 },
};

static struct vibe_clk stih407_clock_out[] = {
  /* SYS / SPARE */
  { STM_CLK_SPARE,              "CLK_MAIN_DISP",          0, 0, 0, 0 },
  { STM_CLK_SPARE,              "CLK_AUX_DISP",           0, 0, 0, 0 },
  { STM_CLK_SPARE,              "CLK_COMPO_DVP",          0, 0, 0, 0 },
  { STM_CLK_TMDS_HDMI,          "CLK_TMDS_HDMI",          0, 0, 0, 0 },
  /* TVOUT */
  { STM_CLK_PIX_MAIN,           "CLK_PIX_MAIN_DISP",      0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],      74250000 },
  { STM_CLK_PIX_HDDACS,         "CLK_PIX_HDDAC",          0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],      74250000 },
  { STM_CLK_OUT_HDDACS,         "CLK_HDDAC",              0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],     148500000 },
  { STM_CLK_PIX_AUX,            "CLK_PIX_AUX_DISP",       0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],      13500000 },
  { STM_CLK_DENC,               "CLK_DENC",               0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],      27000000 },
  { STM_CLK_OUT_SDDACS,         "CLK_SDDAC",              0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],     108000000 },
  /* HDMI */
  { STM_CLK_PIX_HDMI,           "CLK_PIX_HDMI",           0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],      74250000 },
  { STM_CLK_HDMI_PHY,           "CLK_REF_HDMIPHY",        0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],     148500000 },
  /* DVO */
  { STM_CLK_PIX_DVO,            "CLK_PIX_DVO",            0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],      74250000 },
  { STM_CLK_OUT_DVO,            "CLK_DVO",                0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],      74250000 },
  /* COMPO */
  { STM_CLK_PIX_GDP1,           "CLK_PIX_GDP1",           0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],     148500000 },
  { STM_CLK_PIX_GDP2,           "CLK_PIX_GDP2",           0, 0, &stih407_clock_fs[STM_CLK_SRC_HD],     148500000 },
  { STM_CLK_PIX_GDP3,           "CLK_PIX_GDP3",           0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],     108000000 },
  { STM_CLK_PIX_GDP4,           "CLK_PIX_GDP4",           0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],     108000000 },
  { STM_CLK_PIX_PIP,            "CLK_PIX_PIP",            0, 0, &stih407_clock_fs[STM_CLK_SRC_SD],     108000000 },
};

/*
 * If you change this you also have to change the mixer map and main mixer class
 */
static const int STiH407_PLANE_IDX_VBI_LINKED_GDP = STiH407_PLANE_IDX_GDP3;

/*
 * NOTE: Although the mixers are almost full crossbars on STiH407 and GDP3 is
 * available on the Aux mixer, for practical reasons, including the fact we need
 * a guaranteed GDP to link the ED/HD VBI plane to, we are going to limit it to
 * the main mixer only. All the other GDPs will remain flexible.
 *
 * Although this compositor version allows the HQVDP/VideoPlug1 to be used on
 * both mixers, the video clock routing in STiH407 doesn't allow the HQVDP
 * to be configured on the aux mixer; this is  because the HQVDP uses the TVOut
 * main pixel clock channel and that cannot be changed.
 */

static const stm_mixer_id_t mainMixerMap[STiH407_PLANE_COUNT] = {
/* [STiH407_PLANE_IDX_GDP1_MAIN]= */  MIXER_ID_GDP3,
/* [STiH407_PLANE_IDX_GDP2_MAIN]= */  MIXER_ID_GDP4,
/* [STiH407_PLANE_IDX_GDP1_AUX] = */  MIXER_ID_GDP1,
/* [STiH407_PLANE_IDX_GDP2_AUX] = */  MIXER_ID_GDP2,
/* [STiH407_PLANE_IDX_VBI_MAIN] = */  MIXER_ID_VBI,
/* [STiH407_PLANE_IDX_VID_MAIN] = */  MIXER_ID_VID1,
/* [STiH407_PLANE_IDX_VID_PIP]  = */  MIXER_ID_VID2,
};

static const stm_mixer_id_t auxMixerMap[STiH407_PLANE_COUNT] = {
/* [STiH407_PLANE_IDX_GDP1_MAIN]= */  MIXER_ID_NONE,
/* [STiH407_PLANE_IDX_GDP2_MAIN]= */  MIXER_ID_GDP4,
/* [STiH407_PLANE_IDX_GDP1_AUX] = */  MIXER_ID_GDP1,
/* [STiH407_PLANE_IDX_GDP2_AUX] = */  MIXER_ID_GDP2,
/* [STiH407_PLANE_IDX_VBI_MAIN] = */  MIXER_ID_NONE,
/* [STiH407_PLANE_IDX_VID_MAIN] = */  MIXER_ID_NONE,
/* [STiH407_PLANE_IDX_VID_PIP]  = */  MIXER_ID_VID2,
};

static char sourceNames[STiH407_SOURCE_COUNT][10];


CSTiH407Device::CSTiH407Device(void): CDisplayDevice(0, STiH407_OUTPUT_COUNT, STiH407_PLANE_COUNT, STiH407_SOURCE_COUNT)

{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pReg = (uint32_t*)vibe_os_map_memory(STiH407_REGISTER_BASE, STiH407_REG_ADDR_SIZE);
  ASSERTF(m_pReg,("CSTiH407Device::CSTiH407Device 'unable to map device registers'\n"));

  TRC( TRC_ID_MAIN_INFO, "CSTiH407Device::CSTiH407Device: Register remapping 0x%08x -> 0x%08x", STiH407_REGISTER_BASE,(uint32_t)m_pReg );

  /*
   * Now setup the basic chip config and reset state
   */
  PowerOnSetup();

  /*
   * Global setup of the display device.
   */
  m_pFSynthHD      = 0;
  m_pFSynthSD      = 0;

  m_pClkDivider    = 0;

  m_pMainVTG       = 0;
  m_pAuxVTG        = 0;

  m_pMainMixer     = 0;
  m_pAuxMixer      = 0;

  m_pVideoPlug1    = 0;
  m_pVideoPlug2    = 0;

  m_pHDFormatter   = 0;

  m_pDENCVIP       = 0;
  m_pHDFVIP        = 0;
  m_pHDMIVIP       = 0;
  m_pDVOVIP        = 0;

  m_pDENC          = 0;
  m_pTeletext      = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTiH407Device::~CSTiH407Device()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  RemovePlanes();
  RemoveSources();
  RemoveOutputs();

  delete m_pFSynthHD;
  delete m_pFSynthSD;
  delete m_pClkDivider;

  delete m_pMainVTG;
  delete m_pAuxVTG;

  delete m_pMainMixer;
  delete m_pAuxMixer;

  delete m_pVideoPlug1;
  delete m_pVideoPlug2;

  delete m_pHDFormatter;

  delete m_pDENCVIP;
  delete m_pHDFVIP;
  delete m_pHDMIVIP;
  delete m_pDVOVIP;

  delete m_pDENC;
  if(CONFIG_TVOUT_TELETEXT)
    delete m_pTeletext;

  PowerDown();

  vibe_os_unmap_memory(m_pReg);
  m_pReg = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407Device::PowerOnSetup(void)
{
  uint32_t tmp;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  tmp = ReadDevReg(STiH407_SYSCFG_CORE + SYS_CFG5131);
  tmp |= (SYS_CFG5131_RST_N_HDTVOUT);
  WriteDevReg(STiH407_SYSCFG_CORE + SYS_CFG5131, tmp);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407Device::PowerDown(void)
{
  uint32_t tmp;

  if(m_bIsSuspended)
    return;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Unfortunately we have only one bit controlling the whole
   * power of the HDTVOUT blocks but not seperate bits like Orly2
   * So do not power down HDTVOUT if you still need the HDMI block
   * to wake up system on HPD event
   */
  if(STM_SYSTEM_RESUME_ON_HDMI_EVENTS)
    return;

  tmp = ReadDevReg(STiH407_SYSCFG_CORE + SYS_CFG5131);
  tmp &= ~SYS_CFG5131_RST_N_HDTVOUT;
  WriteDevReg(STiH407_SYSCFG_CORE + SYS_CFG5131, tmp);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH407Device::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CDisplayDevice::Create())
  {
    TRC( TRC_ID_ERROR, "Base class Create failed" );
    return false;
  }

  if(!CreateClocks())
    return false;

  if(!CreateInfrastructure())
    return false;

  if(!CreatePlanes())
    return false;

  if(!CreateSources())
    return false;

  if(!CreateOutputs())
    return false;

  /*
   * Now disable all clocks except SPARE clocks
   */
  for (uint32_t i=0; i<N_ELEMENTS(stih407_clock_out); i++)
  {
    stm_clk_divider_output_name_t clk_name = static_cast<stm_clk_divider_output_name_t>(stih407_clock_out[i].id);
    if(clk_name != STM_CLK_SPARE)
      m_pClkDivider->Disable(clk_name);
  }

  for (uint32_t i=0; i<N_ELEMENTS(stih407_clock_fs); i++)
  {
    stm_clk_divider_output_name_t clk_name = static_cast<stm_clk_divider_output_name_t>(stih407_clock_fs[i].id);
    if(clk_name != STM_CLK_SPARE)
      m_pClkDivider->Disable(clk_name);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


bool CSTiH407Device::CreateClocks(void)
{

  if((m_pClkDivider = new CSTmClockLLA(stih407_clock_fs, N_ELEMENTS(stih407_clock_fs), stih407_clock_out, N_ELEMENTS(stih407_clock_out))) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create ClkDivider" );
    return false;
  }

  if((m_pFSynthHD = new CSTmFSynthLLA(&stih407_clock_fs[STM_CLK_SRC_HD])) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create FSynthHD" );
    return false;
  }

  if((m_pFSynthSD = new CSTmFSynthLLA(&stih407_clock_fs[STM_CLK_SRC_SD])) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create FSynthSD" );
    return false;
  }

  return true;
}


bool CSTiH407Device::CreateInfrastructure(void)
{
  if((m_pMainVTG = new CSTmC8VTG_V8_4(this, STiH407_VTG_MAIN_BASE, STM_C8VTG_NUM_SYNC_OUTPUTS, m_pFSynthHD, false, STVTG_SYNC_POSITIVE, true, false)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create Main VTG" );
    return false;
  }

  if((m_pAuxVTG = new CSTmC8VTG_V8_4(this, STiH407_VTG_AUX_BASE, STM_C8VTG_NUM_SYNC_OUTPUTS, m_pFSynthSD, false, STVTG_SYNC_POSITIVE, true, false)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create Aux VTG" );
    return false;
  }

  if(CONFIG_TVOUT_TELETEXT)
  {
    m_pTeletext = new CSTmTVOutTeletext(this, STiH407_DENC_BASE,
                                              STiH407_TVO_TTXT_BASE,
                                              STiH407_REGISTER_BASE,
                                              SDTP_DENC1_TELETEXT);
    /*
     * Slight difference for Teletext, if we fail to create it then clean up
     * and continue. We may not have been able to claim an FDMA channel and we
     * are not treating that as fatal.
     */
    if(m_pTeletext && !m_pTeletext->Create())
    {
      TRC( TRC_ID_ERROR, "failed to create Teletext" );
      delete m_pTeletext;
      m_pTeletext = 0;
    }
  }

  m_pDENC = new CSTiH407DENC(this, STiH407_DENC_BASE, m_pTeletext);
  if(!m_pDENC || !m_pDENC->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create DENC" );
    return false;
  }

  if((m_pDENCVIP = new CSTmVIP( this, STiH407_TVO_VIP_DENC_BASE
                                    , (TVO_VIP_SYNC_TYPE_DENC | TVO_VIP_SYNC_TYPE_SDDCS | TVO_VIP_SYNC_TYPE_SDVTG | TVO_VIP_SYNC_TYPE_TTXT)
                                    , tvo_vip_sync_sel_map
                                    , TVO_VIP_INPUT_IS_INVERTED | TVO_VIP_BYPASS_INPUT_IS_RGB)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create DENC VIP" );
    return false;
  }

  if((m_pHDFVIP  = new CSTmVIP(this, STiH407_TVO_VIP_HDF_BASE
                                   , (TVO_VIP_SYNC_TYPE_HDF | TVO_VIP_SYNC_TYPE_HDDCS)
                                   , tvo_vip_sync_sel_map
                                   , TVO_VIP_INPUT_IS_INVERTED | TVO_VIP_BYPASS_INPUT_IS_RGB)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create HDF VIP" );
    return false;
  }

  if((m_pHDMIVIP  = new CSTmVIP(this, STiH407_TVO_VIP_HDMI_BASE
                                    , TVO_VIP_SYNC_TYPE_HDMI
                                    , tvo_vip_sync_sel_map
                                    , TVO_VIP_INPUT_IS_INVERTED | TVO_VIP_BYPASS_INPUT_IS_RGB | TVO_VIP_HAS_MAIN_FILTERED_422_INPUT)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create HDMI VIP" );
    return false;
  }
  if((m_pDVOVIP  = new CSTmVIPDVO(this, STiH407_TVO_VIP_DVO_BASE
                                      , (TVO_VIP_DVO_SYNC_TYPE_DVO|TVO_VIP_DVO_SYNC_TYPE_PAD_HSYNC|TVO_VIP_DVO_SYNC_TYPE_PAD_VSYNC|TVO_VIP_DVO_SYNC_TYPE_PAD_BNOT)
                                      , tvo_vip_dvo_sync_sel_map
                                      , tvo_vip_dvo_sync_delay_map
                                      , TVO_VIP_INPUT_IS_INVERTED | TVO_VIP_BYPASS_INPUT_IS_RGB | TVO_VIP_HAS_MAIN_FILTERED_422_INPUT)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create DVO VIP" );
    return false;
  }

  m_pHDFormatter = new CSTmHDF_V5_0(this,
                                    STiH407_TVOUT1_BASE,
                                    STiH407_HD_FORMATTER_BASE,
                                    STiH407_HD_FORMATTER_AWG,
                                    STiH407_HD_FORMATTER_AWG_SIZE);
  if(!m_pHDFormatter || !m_pHDFormatter->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create HDFormatter" );
    return false;
  }

  /*
   * Disable ALT filters for this platform :
   * This has been recommended by validation - See Bug38649
   */
  if(m_pHDFormatter->SetControl(OUTPUT_CTRL_DAC_HD_ALT_FILTER, 0) != STM_OUT_OK)
  {
    TRC( TRC_ID_ERROR, "failed setup default configuration for HDFormatter" );
    return false;
  }

  m_pMainMixer = new CSTiH407MainMixer(this,
                                       mainMixerMap,
                                       N_ELEMENTS(mainMixerMap),
                                       mainMixerMap[STiH407_PLANE_IDX_VBI_LINKED_GDP]);

  if(!m_pMainMixer || !m_pMainMixer->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create main mixer" );
    return false;
  }

  m_pAuxMixer = new CSTiH407AuxMixer(this,
                                     auxMixerMap,
                                     N_ELEMENTS(auxMixerMap));

  if(!m_pAuxMixer || !m_pAuxMixer->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create aux mixer" );
    return false;
  }

  return true;
}


bool CSTiH407Device::CreatePlanes(void)
{
  /*
   * This is the only GDP we are fixing to the main mixer.
   */
  if(!AddPlane(new CSTiH407GDP("Main-GDP1",
                               STiH407_PLANE_IDX_GDP1_MAIN,
                               this,
                               (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_PRIMARY_OUTPUT),
                               STiH407_COMPOSITOR_BASE + STiH407_GDP3_OFFSET)))
  {
    TRC( TRC_ID_ERROR, "failed to create Main-GDP1" );
    return false;
  }

  if(!AddPlane(new CSTiH407GDP("Main-GDP2",
                               STiH407_PLANE_IDX_GDP2_MAIN,
                               this,
                               (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_PRIMARY_OUTPUT|PLANE_CAPS_SECONDARY_OUTPUT),
                               STiH407_COMPOSITOR_BASE + STiH407_GDP4_OFFSET)))
  {
    TRC( TRC_ID_ERROR, "failed to create Main-GDP2" );
    return false;
  }

  if(!AddPlane(new CSTiH407GDPLite("Aux-GDP1",
                               STiH407_PLANE_IDX_GDP1_AUX,
                               this,
                               (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_PRIMARY_OUTPUT|PLANE_CAPS_SECONDARY_OUTPUT),
                               STiH407_COMPOSITOR_BASE + STiH407_GDP1_OFFSET)))
  {
    TRC( TRC_ID_ERROR, "failed to create Aux-GDP1" );
    return false;
  }

  if(!AddPlane(new CSTiH407GDPLite("Aux-GDP2",
                               STiH407_PLANE_IDX_GDP2_AUX,
                               this,
                               (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_PRIMARY_OUTPUT|PLANE_CAPS_SECONDARY_OUTPUT),
                               STiH407_COMPOSITOR_BASE + STiH407_GDP2_OFFSET)))
  {
    TRC( TRC_ID_ERROR, "failed to create Aux-GDP2" );
    return false;
  }

  CDisplayPlane * plane = GetPlane(STiH407_PLANE_IDX_VBI_LINKED_GDP);

  if ( !plane ||
       (!AddPlane(new CVBIPlane("Main-VBI",
                                STiH407_PLANE_IDX_VBI_MAIN,
                                (stm_plane_capabilities_t)(PLANE_CAPS_VBI|PLANE_CAPS_PRIMARY_OUTPUT),
                                static_cast<CGdpPlane*>(plane)))))
  {
    TRC( TRC_ID_ERROR, "failed to create Main-VBI" );
    return false;
  }

  m_pVideoPlug1 = new CVideoPlug(this, STiH407_COMPOSITOR_BASE + STiH407_VID1_OFFSET, true, true);
  if(!m_pVideoPlug1)
  {
    TRC( TRC_ID_ERROR, "failed to create video plug 1" );
    return false;
  }

  m_pVideoPlug2 = new CVideoPlug(this, STiH407_COMPOSITOR_BASE + STiH407_VID2_OFFSET, true, true);
  if(!m_pVideoPlug2)
  {
    TRC( TRC_ID_ERROR, "failed to create video plug 2" );
    return false;
  }

  /*
   * TODO: Confirm video plug assignments, this is currently based on the
   *       clock routing documentation which links the VP1 clock to the HQVDP
   *       and the VP2 clock to the VDP.
   */
  if(!AddPlane(new CSTiH407HqvdpLitePlane(this, m_pVideoPlug1)))
  {
    TRC( TRC_ID_ERROR, "failed to create HQVDP" );
    return false;
  }

  if(!AddPlane(new CSTiH407VDP(this, m_pVideoPlug2)))
  {
    TRC( TRC_ID_ERROR, "failed to create VDP" );
    return false;
  }

  return true;
}



bool CSTiH407Device::CreateSources(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  CDisplaySource                *pSource;
  CPureSwQueueBufferInterface    *pInterface;

  /* Create as many Sources/Interfaces as Decoder */
  /* because the link decoder-source will remains connected */
  /* sizeof(CDisplaySource) = 64, sizeof(CPureSwQueueBufferInterface) = 280 */
  /* so total memory consumption is 50x344=17200 bytes */
  for(int i=0;i<STiH407_SOURCE_COUNT;i++)
  {
      vibe_os_snprintf (sourceNames[i], sizeof(sourceNames[i]),"Source.%d",i);
      pSource = new CDisplaySource(sourceNames[i], i, this, STiH407_PLANE_COUNT);
      if(!AddSource(pSource))
      {
          TRC( TRC_ID_ERROR, "failed to create Source" );
          return false;
      }

      pInterface = new CPureSwQueueBufferInterface(i, pSource);
      if(!pInterface || !pInterface->Create())
      {
          TRC( TRC_ID_ERROR, "failed to create Interface" );
          delete pInterface;
          return false;
      }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407Device::CreateOutputs(void)
{
  CSTiH407MainOutput *pMainOutput = new CSTiH407MainOutput( this,
                                                            m_pMainVTG,
                                                            m_pDENC,
                                                            m_pMainMixer,
                                                            m_pFSynthHD,
                                                            m_pHDFormatter,
                                                            m_pClkDivider,
                                                            m_pHDFVIP,
                                                            m_pDENCVIP,
                                                            tvo_vip_sync_sel_map);
  if(!AddOutput(pMainOutput))
  {
    /*
     * NOTE: AddOutput will destroy the passed object for us if it fails.
     */
    TRC( TRC_ID_ERROR, "failed to create main output" );
    return false;
  }

  CSTiH407AuxOutput *pAuxOutput = new CSTiH407AuxOutput(this, m_pAuxVTG,
                                            m_pDENC,
                                            m_pAuxMixer,
                                            m_pFSynthSD,
                                            m_pHDFormatter,
                                            m_pClkDivider,
                                            m_pHDFVIP,
                                            m_pDENCVIP,
                                            tvo_vip_sync_sel_map);
  if(!AddOutput(pAuxOutput))
  {
    TRC( TRC_ID_ERROR, "failed to create aux output" );
    return false;
  }

  if(!AddOutput(new CSTiH407HDMI(this, pMainOutput,
                                       m_pMainVTG,
                                       m_pHDMIVIP,
                                       m_pClkDivider,
                                       tvo_vip_sync_sel_map)))
  {
    TRC( TRC_ID_ERROR, "failed to allocate HDMI output" );
    return false;
  }

  if (pAuxOutput->GetID() ==(uint32_t)vibe_os_get_master_output_id(STiH407_OUTPUT_IDX_DVO))
  {
    if(!AddOutput(new CSTiH407DVO (this, pAuxOutput,
                                         m_pAuxVTG,
                                         m_pClkDivider,
                                         m_pDVOVIP,
                                         tvo_vip_dvo_sync_sel_map,
                                         STiH407_FLEXDVO_BASE)))
    {
      TRC( TRC_ID_ERROR, "failed to create FDVO output" );
      return false;
    }
  }
  else
  {
    if(!AddOutput(new CSTiH407DVO (this, pMainOutput,
                                         m_pMainVTG,
                                         m_pClkDivider,
                                         m_pDVOVIP,
                                         tvo_vip_dvo_sync_sel_map,
                                         STiH407_FLEXDVO_BASE)))
    {
      TRC( TRC_ID_ERROR, "failed to create FDVO output" );
      return false;
    }
  }

  return true;
}


int CSTiH407Device::Freeze(void)
{
  int res;

  if((res = CDisplayDevice::Freeze()) != 0)
    return res;

  if((res = m_pClkDivider->Freeze()) != 0)
    return res;

  PowerDown();

  return res;
}


int CSTiH407Device::Suspend(void)
{
  int res;

  if((res = CDisplayDevice::Suspend()) != 0)
    return res;

  if((res = m_pClkDivider->Suspend()) != 0)
    return res;

  PowerDown();

  return res;
}


int CSTiH407Device::Resume(void)
{
  int res;

  /* Restore basic chip config and reset state */
  PowerOnSetup();

  if((res = m_pClkDivider->Resume()) != 0)
    return res;

  return CDisplayDevice::Resume();
}


/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 */
CDisplayDevice* AnonymousCreateDevice(unsigned deviceid)
{
  if(deviceid == 0)
    return new CSTiH407Device();

  return 0;
}
