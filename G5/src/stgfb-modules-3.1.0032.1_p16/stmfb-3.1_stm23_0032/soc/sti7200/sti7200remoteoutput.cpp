/***********************************************************************
 *
 * File: soc/sti7200/sti7200remoteoutput.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <STMCommon/stmhdmiregs.h>
#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include "sti7200reg.h"
#include "sti7200cut1tvoutreg.h"
#include "sti7200cut2remotedevice.h"
#include "sti7200remoteoutput.h"
#include "sti7200mixer.h"
#include "sti7200denc.h"

/*
 * Remote TVOut specific config registers definitions
 */
#define TVOUT_PADS_DAC_POFF (1L<<10)

#define TVOUT_CLK_SEL_PIX1X_SHIFT (0)
#define TVOUT_CLK_SEL_PIX1X_MASK  (3L<<TVOUT_CLK_SEL_PIX1X_SHIFT)
#define TVOUT_CLK_SEL_DENC_SHIFT  (2)
#define TVOUT_CLK_SEL_DENC_MASK   (7L<<TVOUT_CLK_SEL_DENC_SHIFT)
#define TVOUT_CLK_SEL_DENC_STOP   (7L)
#define TVOUT_CLK_SEL_FDVO_SHIFT  (5)
#define TVOUT_CLK_SEL_FDVO_MASK   (3L<<TVOUT_CLK_SEL_FDVO_SHIFT)

#define TVOUT_SYNC_SEL_DENC_SHIFT (0)
#define TVOUT_SYNC_SEL_DENC_MASK  (7L<<TVOUT_SYNC_SEL_DENC_SHIFT)
#define TVOUT_SYNC_SEL_DENC_PADS  (7L)
#define TVOUT_SYNC_SEL_FDVO_SHIFT (3)
#define TVOUT_SYNC_SEL_FDVO_MASK  (3L<<TVOUT_SYNC_SEL_FDVO_SHIFT)
#define TVOUT_SYNC_SEL_AWG_SHIFT  (5)
#define TVOUT_SYNC_SEL_AWG_MASK   (3L<<TVOUT_SYNC_SEL_AWG_SHIFT)

#define DENC_DELAY (-4)

CSTi7200RemoteOutput::CSTi7200RemoteOutput(
    CSTi7200Device       *pDev,
    ULONG                 ulTVOutOffset,
    CSTmSDVTG            *pVTG,
    CSTi7200DENC         *pDENC,
    CGammaMixer          *pMixer,
    CSTmFSynthType2      *pFSynth): CSTmMasterOutput(pDev, pDENC, pVTG, pMixer, pFSynth)
{
  DEBUGF2(2,("CSTi7200RemoteOutput::CSTi7200RemoteOutput - m_pVTG   = %p\n", m_pVTG));
  DEBUGF2(2,("CSTi7200RemoteOutput::CSTi7200RemoteOutput - m_pDENC  = %p\n", m_pDENC));
  DEBUGF2(2,("CSTi7200RemoteOutput::CSTi7200RemoteOutput - m_pMixer = %p\n", m_pMixer));

  m_ulTVOutOffset = ulTVOutOffset;

  /*
   * The clock tree has a fixed /8 after the fsynth, so for a 27MHz input clock
   * we need to multiply the pixelclock by 16 to get the required FSynth
   * frequency.
   */
  m_pFSynth->SetDivider(16);

  m_ulInputSource = STM_AV_SOURCE_MAIN_INPUT;
  CSTi7200RemoteOutput::SetControl(STM_CTRL_VIDEO_OUT_SELECT,(STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS));
  CSTi7200RemoteOutput::DisableDACs();

  /*
   * Fixed setup of syncs and clock divides to subblocks as we are only
   * supporting SD TV modes, we might as well do it once here.
   *
   * Sync1  - Positive H sync, V sync is actually a top not bot, needed by the
   *          DENC. Yes you might think it might use the ref bnott signal but it
   *          doesn't.
   * Sync2  - External syncs, polarity as specified in standards, could be moved
   *          independently to Sync1 to compensate for signal delays.
   * Sync3  - Internal signal generator sync, again movable relative to Sync1
   *          to allow for delays in the generator block.
   */
  WriteTVOutReg(TVOUT_SYNC_SEL, ((TVOUT_SYNC_SEL_SYNC1 << TVOUT_SYNC_SEL_DENC_SHIFT) |
                                 (TVOUT_SYNC_SEL_REF   << TVOUT_SYNC_SEL_FDVO_SHIFT) |
                                 (TVOUT_SYNC_SEL_SYNC3 << TVOUT_SYNC_SEL_AWG_SHIFT)));

  m_pVTG->SetSyncType(1, STVTG_SYNC_TOP_NOT_BOT);
  m_pVTG->SetSyncType(2, STVTG_SYNC_TIMING_MODE);
  m_pVTG->SetSyncType(3, STVTG_SYNC_POSITIVE);

  m_pVTG->SetHSyncOffset(1, DENC_DELAY);
  m_pVTG->SetVSyncHOffset(1, DENC_DELAY);

  /*
   * Default pad configuration
   */
  WriteTVOutReg(TVOUT_PADS_CTL, TVOUT_PADS_HSYNC_HSYNC2 |
                                TVOUT_PADS_VSYNC_VSYNC2 |
                                TVOUT_PADS_DAC_POFF);
  /*
   * Set internal clock divides, we expect 27MHz to be on the input clock
   */
  WriteTVOutReg(TVOUT_CLK_SEL, ((TVOUT_CLK_BYPASS << TVOUT_CLK_SEL_DENC_SHIFT) |
                                (TVOUT_CLK_DIV_2  << TVOUT_CLK_SEL_PIX1X_SHIFT)|
                                (TVOUT_CLK_BYPASS << TVOUT_CLK_SEL_FDVO_SHIFT)));

  DEXIT();
}


CSTi7200RemoteOutput::~CSTi7200RemoteOutput() {}


const stm_mode_line_t* CSTi7200RemoteOutput::SupportedMode(const stm_mode_line_t *mode) const
{
  /*
   * The Remote device can support ED/HD output on its associated FlexDVO,
   * but we are not supporting this functionality.
   */
  if((mode->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK) == 0)
    return 0;

  if(mode->ModeParams.ScanType == SCAN_P)
    return 0;

  if(mode->TimingParams.ulPixelClock == 13500000)
    return mode;

  return 0;
}


ULONG CSTi7200RemoteOutput::GetCapabilities(void) const
{
  ULONG caps = (STM_OUTPUT_CAPS_SD_ANALOGUE       |
                STM_OUTPUT_CAPS_PLANE_MIXER       |
                STM_OUTPUT_CAPS_MODE_CHANGE       |
                STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
                STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE   );

  return caps;
}


bool CSTi7200RemoteOutput::SetOutputFormat(ULONG format)
{
  return m_pDENC->SetMainOutputFormat(format);
}


void CSTi7200RemoteOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG1);

  if(m_ulOutputFormat & STM_VIDEO_OUT_YUV)
  {
    DEBUGF2(2,("%s: Enabling Remote YPrPb DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG1_DAC_SD1_HZU_DISABLE |
             SYS_CFG1_DAC_SD1_HZV_DISABLE |
             SYS_CFG1_DAC_SD1_HZW_DISABLE);
  }
  else
  {
    if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
    {
      DEBUGF2(2,("%s: Enabling CVBS DAC\n",__PRETTY_FUNCTION__));
      val &= ~SYS_CFG1_DAC_SD1_HZU_DISABLE;
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
      val |=  SYS_CFG1_DAC_SD1_HZU_DISABLE;
    }

    if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
    {
      DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
      val &= ~(SYS_CFG1_DAC_SD1_HZV_DISABLE |
               SYS_CFG1_DAC_SD1_HZW_DISABLE);
    }
    else
    {
      DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
      val |=  (SYS_CFG1_DAC_SD1_HZV_DISABLE |
               SYS_CFG1_DAC_SD1_HZW_DISABLE);
    }
  }

  WriteSysCfgReg(SYS_CFG1,val);

  val = ReadTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_PADS_DAC_POFF;
  WriteTVOutReg(TVOUT_PADS_CTL, val);

  DEXIT();
}


void CSTi7200RemoteOutput::DisableDACs(void)
{
  DENTRY();

  ULONG val = ReadTVOutReg(TVOUT_PADS_CTL) | TVOUT_PADS_DAC_POFF;
  WriteTVOutReg(TVOUT_PADS_CTL, val);

  DEXIT();
}


////////////////////////////////////////////////////////////////////////////////
//
CSTi7200Cut2RemoteOutput::CSTi7200Cut2RemoteOutput(
    CSTi7200Cut2RemoteDevice *pDev,
    CSTmSDVTG                *pVTG,
    CSTi7200DENC             *pDENC,
    CSTi7200Cut2RemoteMixer  *pMixer,
    CSTmFSynthType2          *pFSynth): CSTi7200RemoteOutput(pDev, STi7200C2_REMOTE_TVOUT_BASE, pVTG, pDENC, pMixer, pFSynth)
{
  DENTRY();

  ULONG tmp = ReadSysCfgReg(SYS_CFG49) & ~(SYS_CFG49_REMOTE_PIX_HD |
                                           SYS_CFG49_REMOTE_SDTVO_HD);

  WriteSysCfgReg(SYS_CFG49, tmp);

  /*
   * Set the Remote SDTVOut clock to be the same as Cut1 (27MHz). There is no
   * need to use the 108MHz alternative in this case, unlike the local SDTVOut.
   */
  tmp = ReadClkReg(CLKB_OUT_MUX_CFG) & ~(CLKBC2_OUT_MUX_SD1_DIV2|CLKBC2_OUT_MUX_SD1_EXT);
  WriteClkReg(CLKB_OUT_MUX_CFG, tmp);

  DEXIT();
}


CSTi7200Cut2RemoteOutput::~CSTi7200Cut2RemoteOutput(void) {}
