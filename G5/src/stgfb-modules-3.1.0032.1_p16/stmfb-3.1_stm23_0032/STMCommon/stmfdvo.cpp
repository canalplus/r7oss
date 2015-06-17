/***********************************************************************
 *
 * File: STMCommon/stmfdvo.cpp
 * Copyright (c) 2008-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>

#include "stmfdvo.h"

static const int FDVO_DOF_CFG       = 0x4;
static const int FDVO_LUT_PROG_LOW  = 0x8;
static const int FDVO_LUT_PROG_MID  = 0xC;
static const int FDVO_LUT_PROG_HIGH = 0x10;

#define FDVO_DOF_EN_LOWBYTE        (1L<<0)
#define FDVO_DOF_EN_MIDBYTE        (1L<<1)
#define FDVO_DOF_EN_HIGHBYTE       (1L<<2)
#define FDVO_DOF_EN_CHROMA_FILTER  (1L<<3)
#define FDVO_DOF_INV_CLOCKOUT      (1L<<4)
#define FDVO_DOF_DOUBLE_CLOCK_DATA (1L<<5)
#define FDVO_DOF_EN                (1L<<6)
#define FDVO_DOF_DROP_SEL          (1L<<7)
#define FDVO_DOF_MOD_COUNT_SHIFT   (8)
#define FDVO_DOF_MOD_COUNT_MASK    (7L<<FDVO_DOF_MOD_COUNT_SHIFT)

#define FDVO_LUT_ZERO              (0)
#define FDVO_LUT_Y_G               (1)
#define FDVO_LUT_Y_G_DEL           (2)
#define FDVO_LUT_CB_B              (3)
#define FDVO_LUT_CB_B_DEL          (4)
#define FDVO_LUT_CR_R              (5)
#define FDVO_LUT_CR_R_DEL          (6)
#define FDVO_LUT_HOLD              (7)


struct fdvo_config
{
  ULONG flags;
  ULONG lowbyte;
  ULONG midbyte;
  ULONG highbyte;
};


static fdvo_config rgb_24bit_config =
{
  ((1L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE            |
   FDVO_DOF_EN_HIGHBYTE),
  FDVO_LUT_CB_B,
  FDVO_LUT_Y_G,
  FDVO_LUT_CR_R
};

static fdvo_config yuv_444_24bit_config =
{
  ((1L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE            |
  FDVO_DOF_EN_HIGHBYTE),
  FDVO_LUT_CB_B,
  FDVO_LUT_Y_G,
  FDVO_LUT_CR_R
};

static fdvo_config yuv_444_16bit_config =
{ /* Note the FDVO output clock must be twice the pixel clock in this mode */
  ((2L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE),
  (FDVO_LUT_Y_G  | (FDVO_LUT_Y_G<<3)),
  (FDVO_LUT_CB_B | (FDVO_LUT_CR_R<<3)),
  0
};

static fdvo_config yuv_422_16bit_config =
{
  ((2L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE),
  (FDVO_LUT_Y_G  | (FDVO_LUT_Y_G<<3)),
  (FDVO_LUT_CB_B | (FDVO_LUT_CR_R_DEL<<3)),
  0
};

static fdvo_config yuv_422_8bit_config =
{ /* Note the FDVO output clock must be twice the pixel clock in this mode */
  ((4L<<FDVO_DOF_MOD_COUNT_SHIFT) |
   FDVO_DOF_EN_LOWBYTE            |
   FDVO_DOF_EN_MIDBYTE),
   ( FDVO_LUT_CB_B           | (FDVO_LUT_Y_G << 3) |
    (FDVO_LUT_CR_R_DEL << 6) | (FDVO_LUT_Y_G << 9)),
   0,
   0
};


CSTmFDVO::CSTmFDVO(CDisplayDevice *pDev,
                   ULONG           ulFDVOOffset,
                   COutput        *pMasterOutput): COutput(pDev,0), m_AWG(pDev, ulFDVOOffset)
{
  DEBUGF2(2,("CSTmFDVO::CSTmFDVO pDev = %p  master output = %p\n",pDev,pMasterOutput));
  m_pMasterOutput = pMasterOutput;
  m_ulTimingID    = pMasterOutput->GetTimingID();

  m_pFDVORegs     = (ULONG*)pDev->GetCtrlRegisterBase() + (ulFDVOOffset>>2);

  m_ulOutputFormat = STM_VIDEO_OUT_422;
  m_bEnable422ChromaFilter = false;
  m_bInvertDataClock = false;

  DEBUGF2(2,("CSTmFDVO::CSTmFDVO m_pFDVORegs = %p\n",m_pFDVORegs));
}


CSTmFDVO::~CSTmFDVO()
{
  CSTmFDVO::Stop();
}


bool CSTmFDVO::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  DENTRY();

  // The DVO is slaved off the timings of a main output, which must have
  // already been started with the mode requested here.
  if(m_pMasterOutput->GetCurrentDisplayMode() == 0         ||
     m_pMasterOutput->GetCurrentDisplayMode() != pModeLine ||
     m_pMasterOutput->GetCurrentTVStandard()  != tvStandard)
  {
    DERROR("Display mode or standard invalid\n");
    return false;
  }

  Stop();

  if(!SetOutputFormat(m_ulOutputFormat,pModeLine))
  {
    DERROR("Invalid output format\n");
    Stop();
    return false;
  }

  /*
   * Start EAV/SAV insertion.
   * Note: if no EAV/SAV generation firmware is available for the display mode
   *       then the FDVO will still output the video, syncs and clock signals.
   */
  m_AWG.Start(pModeLine);

  COutput::Start(pModeLine, tvStandard);

  DEXIT();

  return true;
}


bool CSTmFDVO::Stop(void)
{
  DENTRY();

  ULONG val = ReadFDVOReg(FDVO_DOF_CFG) & ~FDVO_DOF_EN;
  WriteFDVOReg(FDVO_DOF_CFG, val);

  m_AWG.Stop();
  COutput::Stop();

  DEXIT();
  return true;
}


ULONG CSTmFDVO::GetCapabilities(void) const
{
  return (STM_OUTPUT_CAPS_DVO_656 |
          STM_OUTPUT_CAPS_DVO_HD  |
          STM_OUTPUT_CAPS_DVO_24BIT);
}

ULONG CSTmFDVO::SupportedControls(void) const
{
  return (STM_CTRL_CAPS_VIDEO_OUT_SELECT |
          STM_CTRL_CAPS_SIGNAL_RANGE     |
          STM_CTRL_CAPS_FDVO_CONTROL     |
          STM_CTRL_CAPS_422_CHROMA_FILTER);
}


void CSTmFDVO::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch(ctrl)
  {
    case STM_CTRL_VIDEO_OUT_SELECT:
    {
      if(SetOutputFormat(ulNewVal,m_pCurrentMode))
        m_ulOutputFormat = ulNewVal;
      break;
    }
    case STM_CTRL_422_CHROMA_FILTER:
    {
      m_bEnable422ChromaFilter = (ulNewVal!=0);
      /*
       * Just call SetOutputFormat again with the current format and mode
       * to update the hardware filter state, if the output is running.
       */
      SetOutputFormat(m_ulOutputFormat,m_pCurrentMode);
      break;
    }
    case STM_CTRL_FDVO_INVERT_DATA_CLOCK:
    {
      m_bInvertDataClock = (ulNewVal!=0);
      SetOutputFormat(m_ulOutputFormat,m_pCurrentMode);
      break;
    }
    default:
      break;
  }
}


ULONG CSTmFDVO::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_VIDEO_OUT_SELECT:
      return m_ulOutputFormat;
    case STM_CTRL_SIGNAL_RANGE:
      return m_signalRange;
    case STM_CTRL_422_CHROMA_FILTER:
      return m_bEnable422ChromaFilter?1:0;
    case STM_CTRL_FDVO_INVERT_DATA_CLOCK:
      return m_bInvertDataClock?1:0;
    default:
      return 0;
  }
}


bool CSTmFDVO::SetOutputFormat(ULONG format,const stm_mode_line_t* pModeLine)
{
  fdvo_config *config;

  DENTRY();

  switch(format)
  {
    case STM_VIDEO_OUT_RGB:
      DEBUGF2(2,("%s: 24bit RGB\n",__PRETTY_FUNCTION__));
      config = &rgb_24bit_config;
      break;
    case STM_VIDEO_OUT_YUV:
      DEBUGF2(2,("%s: 16bit YUV 4:4:4 (double clocked)\n",__PRETTY_FUNCTION__));
      config = &yuv_444_16bit_config;
      break;
    case STM_VIDEO_OUT_422:
      DEBUGF2(2,("%s: 16bit YUV 4:2:2\n",__PRETTY_FUNCTION__));
      config = &yuv_422_16bit_config;
      break;
    case STM_VIDEO_OUT_ITUR656:
      DEBUGF2(2,("%s: 8bit YUV 4:2:2 (ITUR656)\n",__PRETTY_FUNCTION__));
      config = &yuv_422_8bit_config;
      break;
    case STM_VIDEO_OUT_YUV_24BIT:
      DEBUGF2(2,("%s: 24bit YUV 4:4:4\n",__PRETTY_FUNCTION__));
      config = &yuv_444_24bit_config;
      break;
    default:
      return false;
  }

  if(pModeLine)
  {
    if(format & STM_VIDEO_OUT_ITUR656)
    {
      /*
       * 8bit ITUR656 is not defined HD modes
       */
      if((pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) != 0)
      {
        DERROR("8bit DVO not supported in HD modes\n");
        return false;
      }
    }

    WriteFDVOReg(FDVO_LUT_PROG_LOW,  config->lowbyte);
    WriteFDVOReg(FDVO_LUT_PROG_MID,  config->midbyte);
    WriteFDVOReg(FDVO_LUT_PROG_HIGH, config->highbyte);

    ULONG cfg = (config->flags | FDVO_DOF_EN);

    if(m_bEnable422ChromaFilter)
    {
      DEBUGF2(2,("%s: 422 Chroma filter enabled\n",__PRETTY_FUNCTION__));
      cfg |= FDVO_DOF_EN_CHROMA_FILTER;
    }

    if(m_bInvertDataClock)
    {
      DEBUGF2(2,("%s: Output data clock inverted\n",__PRETTY_FUNCTION__));
      cfg |= FDVO_DOF_INV_CLOCKOUT;
    }

    WriteFDVOReg(FDVO_DOF_CFG, cfg);
  }

  DEXIT();
  return true;
}

// No interrupts on DVO
bool CSTmFDVO::HandleInterrupts(void) { return false; }

// DVO is a slave output and doesn't explicitly manage plane composition.
bool CSTmFDVO::CanShowPlane(stm_plane_id_t planeID) { return false; }
bool CSTmFDVO::ShowPlane   (stm_plane_id_t planeID) { return false; }
void CSTmFDVO::HidePlane   (stm_plane_id_t planeID) {}
bool CSTmFDVO::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) { return false; }
bool CSTmFDVO::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const { return false; }


///////////////////////////////////////////////////////////////////////////////
//

static const int AWG_RAM_REG  = 0x100;
static const int AWG_CTRL_REG = 0x0;

static const int AWG_RAM_SIZE = 60; /* Taken from HDTVOut functional spec */

#define AWG_CTRL_EN                 (1L<<0)
#define AWG_CTRL_SYNC_FIELD_N_FRAME (1L<<1)
#define AWG_CTRL_SYNC_HREF_IGNORED  (1L<<2)


CSTmFDVOAWG::CSTmFDVOAWG(CDisplayDevice* pDev, ULONG ulRegOffset):
                        CAWG (pDev,(ulRegOffset+AWG_RAM_REG),AWG_RAM_SIZE,"fdvo_sync_insertion")
{
  DENTRY();

  m_pFDVOAWG = (ULONG*)pDev->GetCtrlRegisterBase() + (ulRegOffset>>2);

  DEXIT();
}


CSTmFDVOAWG::~CSTmFDVOAWG () {}


bool CSTmFDVOAWG::Enable (stm_awg_mode_t mode)
{
  DENTRY();

  /* enable AWG and set up sync behavior */
  ULONG awg = 0;

  if (mode & STM_AWG_MODE_FIELDFRAME)
    awg |= AWG_CTRL_SYNC_FIELD_N_FRAME;

  if (mode & STM_AWG_MODE_HSYNC_IGNORE)
    awg |= AWG_CTRL_SYNC_HREF_IGNORED;

  if(!m_bIsDisabled)
    awg |= AWG_CTRL_EN;

  WriteReg (AWG_CTRL_REG, awg);

  DEXIT();
  return true;
}


void CSTmFDVOAWG::Enable (void)
{
  DENTRY();

  m_bIsDisabled = false;

  if(m_bIsFWLoaded)
  {
    /* enable AWG, using previous state */
    ULONG awg = ReadReg (AWG_CTRL_REG) | AWG_CTRL_EN;
    WriteReg (AWG_CTRL_REG, awg);
  }

  DEXIT();
}


void CSTmFDVOAWG::Disable (void)
{
  DENTRY();

  m_bIsDisabled = true;
  /* disable AWG, but leave state */
  ULONG awg = ReadReg (AWG_CTRL_REG) & ~(AWG_CTRL_EN);
  WriteReg (AWG_CTRL_REG, awg);

  DEXIT();
}
