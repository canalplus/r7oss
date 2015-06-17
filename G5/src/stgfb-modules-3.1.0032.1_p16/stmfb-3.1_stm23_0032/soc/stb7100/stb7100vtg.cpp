/***********************************************************************
 *
 * File: soc/stb7100/stb7100vtg.cpp
 * Copyright (c) 2005 STMicroelectronics Limited.
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
#include <STMCommon/stmfsynth.h>

#include "stb7100reg.h"
#include "stb7100vtg.h"

// VTG offsets from the start of the VOS register block.
#define VTG1_OFFSET 0x00
#define VTG2_OFFSET 0x34

/*
 * first register in block, belongs to VTG1 but controls the external sync
 * outputs, which can come from VTG2 as well as VTG1.
 */
#define VTG_HDRIVE            0x0000

#define VTGn_CLKLN            0x0004
#define VTGn_HDO              0x0008
#define VTGn_HDS              0x000C
#define VTGn_HLFLN            0x0010
#define VTGn_VDO              0x0014
#define VTGn_VDS              0x0018
#define VTGn_MODE             0x001C
#define VTGn_VTIMER           0x0020
#define VTGn_DRST             0x0024

#define VTG1_ITM              0x0028
#define VTG1_ITS              0x002C
#define VTG1_STA              0x0030

// Oh why Oh why Oh why did they put R1 and R2 in the middle of the registers
// for VTG2. Do hardware designers actually have a brain?
#define VTG2_R1               0x0028
#define VTG2_R2               0x002C
#define VTG2_ITM              0x0030
#define VTG2_ITS              0x0034
#define VTG2_STA              0x0038

/*
 * Extra VTG1 registers added in Cut2/3 of STb7100
 */
#define VTG1_AWGHDO           0x0130
#define VTG1_AWGVDO           0x0134
#define VTG1_DACHDO           0x0138
#define VTG1_DACHDS           0x013C
#define VTG1_DACVDO           0x0140
#define VTG1_DACVDS           0x0144


#define VTG_HDRIVE_ALT_PIO3_ZERO      (0L)
#define VTG_HDRIVE_ALT_PIO3_MAIN_SYNC (1L)
#define VTG_HDRIVE_ALT_PIO3_DAC_SYNC  (2L)
#define VTG_HDRIVE_ALT_PIO3_MAIN_REF  (3L)
#define VTG_HDRIVE_ALT_PIO3_AUX_SYNC  (4L)
#define VTG_HDRIVE_ALT_PIO3_AUX_REF   (5L)
#define VTG_HDRIVE_ALT_PIO3_MASK      (7L)
#define VTG_HDRIVE_AVSYNC_MAIN_VREF   (0)
#define VTG_HDRIVE_AVSYNC_AUX_VREF    (1L<<8)
#define VTG_HDRIVE_DVOSYNC_ZERO       (0)
#define VTG_HDRIVE_DVOSYNC_MAIN_SYNC  (1L<<16)
#define VTG_HDRIVE_DVOSYNC_DAC_SYNC   (2L<<16)
#define VTG_HDRIVE_DVOSYNC_MAIN_REF   (3L<<16)
#define VTG_HDRIVE_DVOSYNC_AUX_SYNC   (4L<<16)
#define VTG_HDRIVE_DVOSYNC_AUX_REF    (5L<<16)
#define VTG_HDRIVE_DVOSYNC_MASK       (7L<<16)

#define VTG_MODE_MASTER           (0x00)
#define VTG_MODE_SLAVE_HV         (0x01)
#define VTG_MODE_FORCE_INTERLACED (1L<<2)

#define VTG_DRST_RESET (1L<<0)

#define VTG_VDO_VPOL_FALLING_EDGE     (1L<<28)
#define VTG_VDO_VPOL_RISING_EDGE      (0)
#define VTG_VDO_VPOS_BEFORE_VREF      (1L<<29)
#define VTG_VDO_VPOS_AFTER_VREF       (0)
#define VTG_VDO_VHD_SHIFT             (16)
#define VTG_VDO_VHD_MASK              (0xfff<<VTG_VDO_VHD_SHIFT)

#define VTG_ITS_VSB (1L<<1)
#define VTG_ITS_VST (1L<<2)
#define VTG_ITS_PDVS (1L<<3)

#define VTG1_HDO_HDMIHDO_SHIFT        (24)
#define VTG1_HDO_HDMIHDO_MASK         (0x7<<VTG1_HDO_HDMIHDO_SHIFT)


CSTb7100VTG::CSTb7100VTG(CDisplayDevice *pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth,
			 ULONG its, ULONG itm, ULONG sta)
: CSTmVTG(pDev, ulRegOffset, pFSynth), m_vtg_its(its), m_vtg_itm(itm), m_vtg_sta(sta)
{
}


CSTb7100VTG::~CSTb7100VTG() {}


void CSTb7100VTG::GetVSyncPositions(int outputid, const stm_timing_params_t &TimingParams, ULONG *ulVDO, ULONG *ulVDS)
{
  LONG h_offs = m_VSyncHOffsets[outputid];
  LONG v_offs = 0;
  LONG halfline = TimingParams.PixelsPerLine/2;

  DENTRY();

  /*
   * Limit the offset to half a line; in reality the offsets are only going to
   * be a handful of clocks so this is being a bit paranoid.
   */
  if(h_offs >= halfline)
    h_offs = halfline - 1L;

  if(h_offs <= -halfline)
    h_offs = -halfline + 1L;

  ULONG vdo,vds;
  ULONG flags = VTG_VDO_VPOS_AFTER_VREF;
  if(h_offs < 0)
  {
    /*
     * The vsync horizontal offset count wraps at the halfline position.
     */
    h_offs += halfline;
    v_offs = -1L;
    flags = VTG_VDO_VPOS_BEFORE_VREF;
  }

  if (((m_syncType[outputid] == STVTG_SYNC_TIMING_MODE) && TimingParams.VSyncPolarity) || (m_syncType[outputid] == STVTG_SYNC_POSITIVE))
  {
    /*
     * VSync positive, programmed in 1/2 lines _not_ lines. This is different
     * to the 8K/5528 VTG.
     */
    if(v_offs == -1)
      vdo = TimingParams.LinesByFrame - 1L;
    else
      vdo = 0;

    vds = (TimingParams.VSyncPulseWidth*2) + v_offs;
    flags |= VTG_VDO_VPOL_RISING_EDGE;
  }
  else
  {
    // VSync negative, programmed in 1/2 lines
    vdo = (TimingParams.VSyncPulseWidth*2) + v_offs;

    if(v_offs == -1L)
      vds = TimingParams.LinesByFrame -1L;
    else
      vds = 0;

    flags |= VTG_VDO_VPOL_FALLING_EDGE;
  }

  *ulVDO = vdo | flags | (h_offs<<VTG_VDO_VHD_SHIFT);
  *ulVDS = vds;

  DEXIT();
}


bool CSTb7100VTG::Start(const stm_mode_line_t* pModeLine, stm_vtg_sync_type_t refPolarity)
{
  DEBUGF2(2,("CSTb7100VTG::Start in\n"));
  stm_timing_params_t TimingParams;

  ASSERTF((refPolarity != STVTG_SYNC_TOP_NOT_BOT) && (refPolarity != STVTG_SYNC_BOT_NOT_TOP),("%s: not supported",__PRETTY_FUNCTION__));

  // Get corresponding register parameters in that line
  TimingParams = pModeLine->TimingParams;

  /*
   * Set the number of pixels per line, note this is different to the
   * programming found in SDVTG for the 5528/8000.
   */
  WriteVTGReg(VTGn_CLKLN, TimingParams.PixelsPerLine);

  // Set number of half lines per field/frame
  if(pModeLine->ModeParams.ScanType == SCAN_P)
  {
    DEBUGF2(2,("CSTb7100VTG::Start setting progressive timings\n"));
    WriteVTGReg(VTGn_HLFLN, (2*TimingParams.LinesByFrame));
    WriteVTGReg(VTGn_MODE, VTG_MODE_MASTER);
  }
  else
  {
    ULONG val = VTG_MODE_MASTER;

    DEBUGF2(2,("CSTb7100VTG::Start setting interlaced timings\n"));
    WriteVTGReg(VTGn_HLFLN, TimingParams.LinesByFrame);

    /*
     * VTG normally determines an interlaced mode becuase HLFLN is ODD
     * but there are some interlaced HD modes where the lines per frame is EVEN
     * so we need to force the VTG to be interlaced in these modes.
     */
    if((TimingParams.LinesByFrame & 0x1) == 0)
    {
      DEBUGF2(2,("CSTb7100VTG::Start Forced Interlaced Timing\n"));
      val |= VTG_MODE_FORCE_INTERLACED;
    }

    WriteVTGReg(VTGn_MODE, val);
  }

  ULONG ulHStart,ulHStop;

  CSTmVTG::GetHSyncPositions(VTG_INT_SYNC_ID, TimingParams, &ulHStart, &ulHStop);
  WriteVTGReg(VTGn_HDO, ulHStart);
  WriteVTGReg(VTGn_HDS, ulHStop);

  ULONG ulVDO,ulVDS;

  GetVSyncPositions(VTG_INT_SYNC_ID, TimingParams, &ulVDO, &ulVDS);
  WriteVTGReg(VTGn_VDO, ulVDO);
  WriteVTGReg(VTGn_VDS, ulVDS);

  m_hdo = ReadVTGReg(VTGn_HDO);
  m_hds = ReadVTGReg(VTGn_HDS);
  m_vdo = ReadVTGReg(VTGn_VDO);
  m_vds = ReadVTGReg(VTGn_VDS);

  DEBUGF2(2,("CSTb7100VTG::Start VTG_HDRIVE\t\t%#08lx\n", ReadVTGReg(VTG_HDRIVE)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_CLKLN\t\t%#08lx\n", ReadVTGReg(VTGn_CLKLN)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_HLFLN\t\t%#08lx\n", ReadVTGReg(VTGn_HLFLN)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_HDO\t\t%#08lx\n",   ReadVTGReg(VTGn_HDO)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_HDS\t\t%#08lx\n",   ReadVTGReg(VTGn_HDS)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_VDO\t\t%#08lx\n",   ReadVTGReg(VTGn_VDO)));
  DEBUGF2(2,("CSTb7100VTG::Start VTGn_VDS\t\t%#08lx\n",   ReadVTGReg(VTGn_VDS)));

  DEBUGF2(2,("CSTb7100VTG::Start out\n"));

  m_pCurrentMode = pModeLine;

  m_fpCallback = 0;
  m_pCallbackPointer = 0;
  m_iCallbackOpcode = 0;

  return true;
}


void CSTb7100VTG::Stop(void)
{
  DEBUGF2(2,("CSTb7100VTG::Stop in\n"));

  // There appears to be no way of actually stopping the VTG once it is running?
  WriteVTGReg(VTGn_HDO,   0);
  WriteVTGReg(VTGn_HDS,   0);
  WriteVTGReg(VTGn_VDO,   0);
  WriteVTGReg(VTGn_VDS,   0);
  WriteVTGReg(VTGn_CLKLN, 0);
  WriteVTGReg(VTGn_HLFLN, 0);

  WriteVTGReg(VTGn_DRST, 1);

  m_pCurrentMode = 0;
  m_pPendingMode = 0;
  m_bUpdateClockFrequency = false;

  DEBUGF2(2,("CSTb7100VTG::Stop out\n"));
}


void CSTb7100VTG::ResetCounters(void)
{
  WriteVTGReg(VTGn_DRST, 1);
}


void CSTb7100VTG::DisableSyncs(void)
{
  if(m_pCurrentMode && !m_bDisabled)
  {
    WriteVTGReg(VTGn_HDO,  0);
    WriteVTGReg(VTGn_HDS,  0);
    WriteVTGReg(VTGn_VDO,  0);
    WriteVTGReg(VTGn_VDS,  0);
    WriteVTGReg(VTGn_DRST, 1);

    m_bDisabled = true;
  }
}


void CSTb7100VTG::RestoreSyncs(void)
{
  if(m_pCurrentMode && m_bDisabled)
  {
    WriteVTGReg(VTGn_HDO,  m_hdo);
    WriteVTGReg(VTGn_HDS,  m_hds);
    WriteVTGReg(VTGn_VDO,  m_vdo);
    WriteVTGReg(VTGn_VDS,  m_vds);
    // Do *not* soft reset the VTG at this point - it causes encrypted displays
    // to go wildly out of sync, presumably by issuing an incomplete frame.
    //WriteVTGReg(VTGn_DRST, 1);

    m_bDisabled = false;
  }
}


bool CSTb7100VTG::ArmVsyncTimer(ULONG ulPixels, void (*fpCallback)(void *, int), void *p, int i)
{
  if (m_fpCallback)
    return false;

  m_fpCallback = fpCallback;
  m_pCallbackPointer = p;
  m_iCallbackOpcode = i;

  WriteVTGReg(VTGn_VTIMER, ulPixels);

  // unmask the PDVS interrupt
  WriteVTGReg(m_vtg_itm, ReadVTGReg(m_vtg_itm) | VTG_ITS_PDVS);

  return true;
}


void CSTb7100VTG::WaitForVsync(void)
{
  ULONG its = m_vtg_its;
  do {
    m_ulInterruptStatus |= ReadVTGReg(its);
  } while ((!(m_ulInterruptStatus & VTG_ITS_VSB)) && (!(m_ulInterruptStatus & VTG_ITS_VST)) );
}


stm_field_t CSTb7100VTG::GetInterruptStatus(void)
{
  m_ulInterruptStatus |= ReadVTGReg(m_vtg_its);

  /*
   * The datasheet says this bit in the status register is 0 = bottom, 1 = top
   * but experimental data seems to indicate it is actually a reflection of
   * the VTG's BNOTT signal (bottom_not_top), i.e. top = 0, bottom = 1.
   */
  bool bottomNotTop = (ReadVTGReg(m_vtg_sta) & 0x1) == 0x1;

  m_ulInterruptStatus &= ReadVTGReg(m_vtg_itm);

  if((m_ulInterruptStatus & (VTG_ITS_VST | VTG_ITS_VSB)) == (VTG_ITS_VST | VTG_ITS_VSB))
  {
    if (!m_bSuppressMissedInterruptMessage)
    {
      m_bSuppressMissedInterruptMessage = true;
      DEBUGF2(3,("CSTb7100VTG::GetInterruptStatus Both top and bottom interrupts asserted!\n"));
      DEBUGF2(3,("CSTb7100VTG::GetInterruptStatus using bottomNotTop = %s\n",bottomNotTop?"true":"false"));
    }

    m_ulInterruptStatus &= ~(VTG_ITS_VST | VTG_ITS_VSB);

    return bottomNotTop?STM_BOTTOM_FIELD:STM_TOP_FIELD;
  }

  m_bSuppressMissedInterruptMessage = false;

  if(m_ulInterruptStatus & VTG_ITS_PDVS)
  {
    // perform the callback
    void (*fp)(void *, int) = m_fpCallback;
    if (fp)
    {
      // mask the PDVS interrupt
      WriteVTGReg(m_vtg_itm, ReadVTGReg(m_vtg_itm) & ~VTG_ITS_PDVS);

      // mark the callback as not installed
      m_fpCallback = 0;

      // make the callback (which can safely itself to happen again)
      fp(m_pCallbackPointer, m_iCallbackOpcode);
    }

    m_ulInterruptStatus &= ~VTG_ITS_PDVS;

    // return not-a-field if there is no other work to do
    if (0 == m_ulInterruptStatus)
      return STM_NOT_A_FIELD;
  }

  if(m_ulInterruptStatus & VTG_ITS_VSB)
  {
    m_ulInterruptStatus &= ~VTG_ITS_VSB;

    return STM_BOTTOM_FIELD;
  }


  if(m_ulInterruptStatus & VTG_ITS_VST)
  {
    m_ulInterruptStatus &= ~VTG_ITS_VST;

    if(m_bUpdateClockFrequency)
    {
      m_pFSynth->Start(m_pPendingMode->TimingParams.ulPixelClock);
      m_bUpdateClockFrequency = false;
    }

    if(m_pPendingMode)
    {
      m_pCurrentMode = m_pPendingMode;
      m_pPendingMode = 0;
    }

    return STM_TOP_FIELD;
  }

  return STM_UNKNOWN_FIELD;
}

///////////////////////////////////////////////////////////////////////////
//
CSTb7100VTG1::CSTb7100VTG1(CDisplayDevice* pDev, ULONG ulRegOffset, CSTmFSynth *pFSynth)
: CSTb7100VTG(pDev, ulRegOffset + VTG1_OFFSET, pFSynth, VTG1_ITS, VTG1_ITM, VTG1_STA)
{
  /*
   * Set default digital sync outputs and PCM reader vsync to come
   * from VTG1.
   */
  WriteVTGReg(VTG_HDRIVE, (VTG_HDRIVE_ALT_PIO3_DAC_SYNC    |
                           VTG_HDRIVE_AVSYNC_MAIN_VREF     |
                           VTG_HDRIVE_DVOSYNC_DAC_SYNC));
}


CSTb7100VTG1::~CSTb7100VTG1() {}


bool CSTb7100VTG1::Start(const stm_mode_line_t* pModeLine)
{
  DEBUGF2(2,("CSTb7100VTG1::Start in\n"));

  m_pFSynth->Start(pModeLine->TimingParams.ulPixelClock);

  /*
   * To make HDMI work the main H and V reference sync signals
   * must be positive.
   */
  CSTb7100VTG::Start(pModeLine, STVTG_SYNC_POSITIVE);

  /*
   * We now program the analogue DAC/DVO syncs.
   */
  ULONG ulHStart,ulHStop;

  CSTmVTG::GetHSyncPositions(VTG_DVO_SYNC_ID, pModeLine->TimingParams, &ulHStart, &ulHStop);
  WriteVTGReg(VTG1_DACHDO, ulHStart);
  WriteVTGReg(VTG1_DACHDS, ulHStop);

  ULONG ulVDO,ulVDS;

  GetVSyncPositions(VTG_DVO_SYNC_ID, pModeLine->TimingParams, &ulVDO, &ulVDS);
  WriteVTGReg(VTG1_DACVDO, ulVDO);
  WriteVTGReg(VTG1_DACVDS, ulVDS);

  /*
   * Now the AWG Syncs.
   *
   * Note: The AWG only specifies one edge so it must be "positive"
   */
  SetSyncType(VTG_AWG_SYNC_ID,STVTG_SYNC_POSITIVE);

  CSTmVTG::GetHSyncPositions(VTG_AWG_SYNC_ID, pModeLine->TimingParams, &ulHStart, &ulHStop);
  WriteVTGReg(VTG1_AWGHDO, ulHStart);

  GetVSyncPositions(VTG_AWG_SYNC_ID, pModeLine->TimingParams, &ulVDO, &ulVDS);
  WriteVTGReg(VTG1_AWGVDO, ulVDO);

  DEBUGF2(2, ("CSTb7100VTG1::Start VTGn_MODE\t\t%#08lx\n",     ReadVTGReg(VTGn_MODE)));
  DEBUGF2(2, ("CSTb7100VTG1::Start VTG1_DACHDO\t\t%#08lx\n",   ReadVTGReg(VTG1_DACHDO)));
  DEBUGF2(2, ("CSTb7100VTG1::Start VTG1_DACHDS\t\t%#08lx\n",   ReadVTGReg(VTG1_DACHDS)));
  DEBUGF2(2, ("CSTb7100VTG1::Start VTG1_DACVDO\t\t%#08lx\n",   ReadVTGReg(VTG1_DACVDO)));
  DEBUGF2(2, ("CSTb7100VTG1::Start VTG1_DACVDS\t\t%#08lx\n",   ReadVTGReg(VTG1_DACVDS)));

  // Enable interrupts on VTG. First read to clear any pending
  (void)ReadVTGReg(VTG1_ITS);
  // Enable field interrupts
  WriteVTGReg(VTG1_ITM, VTG_ITS_VST|VTG_ITS_VSB);

  CSTb7100VTG::ResetCounters();

  DEBUGF2(2,("CSTb7100VTG1::Start out\n"));

  return true;
}


bool CSTb7100VTG1::Start(const stm_mode_line_t* pModeLine,int externalid)
{
  return false;
}


void CSTb7100VTG1::Stop(void)
{
  DEBUGF2(2,("CSTb7100VTG1::Stop in\n"));

  // Disable interrupts on VTG
  WriteVTGReg(VTG1_ITM, 0);

  CSTb7100VTG::Stop();

  DEBUGF2(2,("CSTb7100VTG1::Stop out\n"));
}


void CSTb7100VTG1::SetHDMIHSyncDelay(ULONG delay)
{
  DENTRY();

  DEBUGF2(2,("%s: delay = %lu\n",__PRETTY_FUNCTION__,delay));

  ULONG val = ReadVTGReg(VTGn_HDO) & ~VTG1_HDO_HDMIHDO_MASK;

  val |= ((delay << VTG1_HDO_HDMIHDO_SHIFT) & VTG1_HDO_HDMIHDO_MASK);
  WriteVTGReg(VTGn_HDO, val);

  DEXIT();
}


////////////////////////////////////////////////////////////////////////////////
//
//
CSTb7100VTG2::CSTb7100VTG2(CDisplayDevice *pDev,
                           ULONG           ulRegOffset,
                           CSTmFSynth     *pFSynth,
                           CSTb7100VTG1   *pVTG1)
: CSTb7100VTG(pDev, ulRegOffset + VTG2_OFFSET, pFSynth, VTG2_ITS, VTG2_ITM, VTG2_STA)
{
  m_pVTG1 = pVTG1;
}


CSTb7100VTG2::~CSTb7100VTG2() {}


bool CSTb7100VTG2::Start(const stm_mode_line_t* pModeLine)
{
  DEBUGF2(2,("CSTb7100VTG2::Start in\n"));

  if(pModeLine == 0)
  {
    /*
     * This is a request to slave VTG2 to the mode on VTG1, if VTG1 hasn't been
     * started we fail.
     */
    DEBUGF2(2,("CSTb7100VTG2::Start starting in slave mode\n"));

    pModeLine = m_pVTG1->GetCurrentMode();
    if(pModeLine == 0)
      return false;

    /*
     * Set the registers up as if this was a master.
     */
    CSTb7100VTG::Start(pModeLine, STVTG_SYNC_TIMING_MODE);

    /*
     * We need to set R1 and R2 to detect bottom fields, but how? You cannot
     * specify the pixel position of the VSync independently for top and bottom
     * fields unlike the 5528/8K VTG. Help??? Does this VTG magically offset
     * the VSync on the line on a bottom field, if so by how much? The suggestion
     * in the 5528 documentaiton is that it should start at CLKLN/2, so lets
     * try this with a little slack.
     */
    WriteVTGReg(VTG2_R1, pModeLine->TimingParams.PixelsPerLine/2 - 64);
    WriteVTGReg(VTG2_R2, pModeLine->TimingParams.PixelsPerLine/2 + 64);

    // Set the operation mode to be H and V sync slave
    ULONG mode = ReadVTGReg(VTGn_MODE);
    WriteVTGReg(VTGn_MODE, mode | VTG_MODE_SLAVE_HV);

    // We do not enable interrupts in slave mode, software uses VTG1
  }
  else
  {
    // Normal master mode.
    DEBUGF2(2,("CSTb7100VTG2::Start starting in master mode\n"));

    m_pFSynth->Start(pModeLine->TimingParams.ulPixelClock);

    CSTb7100VTG::Start(pModeLine, STVTG_SYNC_TIMING_MODE);
    // Enable interrupts on VTG. First read to clear any pending
    (void)ReadVTGReg(VTG2_ITS);
    // Enable field interrupts
    WriteVTGReg(VTG2_ITM, VTG_ITS_VST|VTG_ITS_VSB);
  }

  DEBUGF2(2, ("CSTb7100VTG2::Start VTGn_MODE     %#08lx\n", ReadVTGReg(VTGn_MODE)));

  CSTb7100VTG::ResetCounters();

  DEBUGF2(2,("CSTb7100VTG2::Start out\n"));

  return true;
}


bool CSTb7100VTG2::Start(const stm_mode_line_t* pModeLine,int externalid)
{
  /*
   * Slave setup by the override in the other Start, we won't do it this way
   * again.
   */
  return false;
}


void CSTb7100VTG2::Stop(void)
{
  DEBUGF2(2,("CSTb7100VTG2::Stop in\n"));

  // Disable interrupts on VTG
  WriteVTGReg(VTG2_ITM, 0);

  CSTb7100VTG::Stop();

  DEBUGF2(2,("CSTb7100VTG2::Stop out\n"));
}
