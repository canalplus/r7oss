/***********************************************************************
 *
 * File: STMCommon/stmsdvtg.cpp
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
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

#include "stmsdvtg.h"
#include "stmfsynth.h"

/* VTG offset registers ---------------------------------------------------*/
#define VTGn_VTGMOD           0x0024
#define VTGn_CLKLN            0x0028
#define VTGn_VID_TFO          0x0004
#define VTGn_VID_TFS          0x0008
#define VTGn_VID_BFO          0x000C
#define VTGn_VID_BFS          0x0010
#define VTGn_H_HD_1           0x002C
#define VTGn_TOP_V_VD_1       0x0030
#define VTGn_BOT_V_VD_1       0x0034
#define VTGn_TOP_V_HD_1       0x0038
#define VTGn_BOT_V_HD_1       0x003C
#define VTGn_H_HD_2           0x005C
#define VTGn_TOP_V_VD_2       0x0060
#define VTGn_BOT_V_VD_2       0x0064
#define VTGn_TOP_V_HD_2       0x0068
#define VTGn_BOT_V_HD_2       0x006C
#define VTGn_H_HD_3           0x007C
#define VTGn_TOP_V_VD_3       0x0080
#define VTGn_BOT_V_VD_3       0x0084
#define VTGn_TOP_V_HD_3       0x0088
#define VTGn_BOT_V_HD_3       0x008C
#define VTGn_HLFLN            0x0040
#define VTGn_DRST             0x0048
#define VTGn_ITM              0x0098
#define VTGn_ITS              0x009C
#define VTGn_STA              0x00A0
#define VTGn_LN_STAT          0x00A4
#define VTGn_LN_INT           0x00A8

// Common VTG Mode Register Definitions
#define VTG_MOD_MASTER_MODE            (0x0L)
#define VTG_MOD_SLAVE_EXT0             (0x1L)
#define VTG_MOD_SLAVE_EXT1             (0x2L)
#define VTG_MOD_MODE_MASK              (0x3L)
#define VTG_MOD_SLAVE_ODDEVEN_NVSYNC   (1L<<4)
#define VTG_MOD_SLAVE_ODDEVEN_HFREERUN (1L<<5)
#define VTG_MOD_FORCE_INTERLACED       (1L<<6)
#define VTG_MOD_HSYNC_PAD_IN_NOUT      (1L<<7)
#define VTG_MOD_VSYNC_PAD_IN_NOUT      (1L<<8)
#define VTG_MOD_DISABLE                (1L<<9)
#define VTG_MOD_BNOTT_INV_POLARITY     (1L<<10)
#define VTG_MOD_HREF_INV_POLARITY      (1L<<11)
#define VTG_MOD_VREF_INV_POLARITY      (1L<<12)
#define VTG_MOD_HINPUT_INV_POLARITY    (1L<<13)
#define VTG_MOD_VINPUT_INV_POLARITY    (1L<<14)
#define VTG_MOD_EXT_BNOTTREF_INV       (1L<<15)

#define VTG_ITS_VSB (1L<<3)
#define VTG_ITS_VST (1L<<4)
#define VTG_ITS_LNB (1L<<6)
#define VTG_ITS_LNT (1L<<7)

/*
 * Unfortunately, the VTG registers aren't aligned continuously, so we access
 * them through this.
 */
struct _vtg_reg_offsets {
  ULONG h_hd;
  ULONG top_v_vd;
  ULONG bot_v_vd;
  ULONG top_v_hd;
  ULONG bot_v_hd;
};
static const struct _vtg_reg_offsets vtg_regs[3] = {
  { VTGn_H_HD_1, VTGn_TOP_V_VD_1, VTGn_BOT_V_VD_1, VTGn_TOP_V_HD_1, VTGn_BOT_V_HD_1 },
  { VTGn_H_HD_2, VTGn_TOP_V_VD_2, VTGn_BOT_V_VD_2, VTGn_TOP_V_HD_2, VTGn_BOT_V_HD_2 },
  { VTGn_H_HD_3, VTGn_TOP_V_VD_3, VTGn_BOT_V_VD_3, VTGn_TOP_V_HD_3, VTGn_BOT_V_HD_3 }
};

/* Capture VTG hardware addresses */
static unsigned VtgCount=0;
static unsigned long *VtgRegBases[STMVTG_MAX_SYNC_OUTPUTS];

extern "C" void vtg_spin_lock_irqsave( void );
extern "C" void vtg_spin_unlock_irqrestore( void );
extern "C" void vtg_delay ( void );

CSTmSDVTG::CSTmSDVTG(CDisplayDevice      *pDev,
                     ULONG                ulRegOffset,
                     CSTmFSynth          *pFSynth,
                     bool                 bDoubleClocked,
                     stm_vtg_sync_type_t  refpol): CSTmVTG(pDev, ulRegOffset, pFSynth, bDoubleClocked)
{
  DENTRY();
  /*
   * By default enable all three sync outputs, can be overridden by a subclass.
   */
  m_bSyncEnabled[0] = true;
  m_bSyncEnabled[1] = true;
  m_bSyncEnabled[2] = true;

  SetSyncType(0,refpol);

  DEXIT();
}


CSTmSDVTG::~CSTmSDVTG() {}


void CSTmSDVTG::GetHSyncPositions(int                        outputid,
                                  const stm_timing_params_t &TimingParams,
                                  ULONG                     &ulSync)
{
  ULONG ulStart,ulStop;

  DENTRY();

  CSTmVTG::GetHSyncPositions(outputid,TimingParams,&ulStart,&ulStop);

  ulSync = ulStart | (ulStop<<16);
  DEXIT();
}


void CSTmSDVTG::GetInterlacedVSyncPositions(int                        outputid,
                                            const stm_timing_params_t &TimingParams,
                                            ULONG                      clocksperline,
                                            ULONG                     &ulVSyncLineTop,
                                            ULONG                     &ulVSyncLineBot,
                                            ULONG                     &ulVSyncPixOffTop,
                                            ULONG                     &ulVSyncPixOffBot)
{
  DENTRY();

  ULONG risesync_top, fallsync_top,
        risesync_bot, fallsync_bot;
  ULONG risesync_offs_top, fallsync_offs_top,
        risesync_offs_bot, fallsync_offs_bot;

  LONG  h_offs = m_VSyncHOffsets[outputid];
  ULONG last_top_line_nr = (TimingParams.LinesByFrame / 2) + 1;
  ULONG last_bot_line_nr = (TimingParams.LinesByFrame / 2);

  if (m_bDoubleClocked)
    h_offs *= 2;

  /*
   * Limit the offset to half a line; in reality the offsets are only going to
   * be a handful of clocks so this is being a bit paranoid.
   */
  if(h_offs >= (LONG)(clocksperline/2))
    h_offs = (LONG)(clocksperline/2) - 1L;

  if(h_offs <= -((LONG)(clocksperline/2)))
    h_offs = -((LONG)(clocksperline/2)) + 1L;

  /*
   * This programming is for interlaced modes that have an odd number of
   * lines, which leads to a half line at the end of the top and beginning
   * of the bottom field. This "shared" line is handled by the VTG as follows:
   *
   * (o) the first clocksperline/2 clocks will have a line count equal to the
   *     last line of the top field and the VTG will use the top field registers
   * (o) at a horizontal count of clocksperline/2 the VTG changes from top to
   *     bottom field, resets the _line_ count to zero and it starts to use the
   *     bottom field registers
   * (o) the horizontal counter continues from clocksperline/2 to
   *     clocksperline-1 with the line count equal to zero
   * (o) at the end of the shared line we start the first full line of the
   *     bottom field which has a line count of one, which is the same as the
   *     first full line of a top field.
   */
  if ((m_syncType[outputid] == STVTG_SYNC_POSITIVE) ||
      (m_syncType[outputid] == STVTG_SYNC_TIMING_MODE && TimingParams.VSyncPolarity))
  {
    DEBUGF2(2,("%s: syncid = %d VSync Positive\n",__PRETTY_FUNCTION__,outputid));

    if (h_offs >= 0)
    {
      risesync_top = 1;
      fallsync_top = risesync_top + TimingParams.VSyncPulseWidth;
      risesync_bot = 0;
      fallsync_bot = TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = (ULONG) h_offs;
      fallsync_offs_bot = risesync_offs_bot = (clocksperline / 2) + h_offs;
    }
    else
    {
      /*
       * Because we are now straddling the top<->bottom transition the start and
       * end of each waveform are defined defined partly in the register for
       * one field and partly in the other.
       */

      /*
       * First the waveform for the bottom field vsync, which is starting just
       * before the halfway point of the last half-line of the VTG top field.
       */
      risesync_top = last_top_line_nr;
      fallsync_bot = TimingParams.VSyncPulseWidth;
      risesync_offs_top = fallsync_offs_bot = (clocksperline/2) + h_offs;

      /*
       * Now the waveform for the top field vsync, which is starting just
       * before the end of the last full line of the VTG bottom field
       */
      risesync_bot = last_bot_line_nr;
      fallsync_top = TimingParams.VSyncPulseWidth;
      risesync_offs_bot = fallsync_offs_top = clocksperline + h_offs;
    }
  }
  else if ((m_syncType[outputid] == STVTG_SYNC_NEGATIVE) ||
           (m_syncType[outputid] == STVTG_SYNC_TIMING_MODE && !TimingParams.VSyncPolarity))
  {
    DEBUGF2(2,("%s: syncid = %d VSync Negative\n",__PRETTY_FUNCTION__,outputid));

    if (h_offs >= 0)
    {
      fallsync_top = 1;
      risesync_top = fallsync_top + TimingParams.VSyncPulseWidth;
      fallsync_bot = 0;
      risesync_bot = TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = (ULONG) h_offs;
      fallsync_offs_bot = risesync_offs_bot = (clocksperline / 2) + h_offs;
    }
    else
    {
      /*
       * See previous comments to understand why this is correct.
       */

      /*
       * Bottom vsync waveform.
       */
      fallsync_top = last_top_line_nr;
      risesync_bot = TimingParams.VSyncPulseWidth;
      fallsync_offs_top = risesync_offs_bot = (clocksperline/2) + h_offs;

      /*
       * Top vsync waveform.
       */
      fallsync_bot = last_bot_line_nr;
      risesync_top = TimingParams.VSyncPulseWidth;
      fallsync_offs_bot = risesync_offs_top = clocksperline + h_offs;
    }
  }
  else if (m_syncType[outputid] == STVTG_SYNC_TOP_NOT_BOT)
  {
    DEBUGF2(2,("%s: syncid = %d Top not Bot\n",__PRETTY_FUNCTION__,outputid));
    if(h_offs>=0)
    {
      fallsync_top      = 0xffff;
      risesync_top      = 0x0001;
      fallsync_offs_top = 0xffff;
      risesync_offs_top = h_offs;
      fallsync_bot      = 0x0000;
      risesync_bot      = 0xffff;
      fallsync_offs_bot = (clocksperline/2)+h_offs;
      risesync_offs_bot = 0xffff;
    }
    else
    {
      fallsync_top      = last_top_line_nr;
      risesync_top      = 0xffff;
      fallsync_offs_top = (clocksperline/2)+h_offs;
      risesync_offs_top = 0xffff;
      fallsync_bot      = 0xffff;
      risesync_bot      = last_bot_line_nr;
      fallsync_offs_bot = 0xffff;
      risesync_offs_bot = clocksperline+h_offs;
    }
  }
  else /* BOT_NOT_TOP */
  {
    DEBUGF2(2,("%s: syncid = %d Bot not Top\n",__PRETTY_FUNCTION__,outputid));
    if(h_offs>=0)
    {
      fallsync_top      = 0x0001;
      risesync_top      = 0xffff;
      fallsync_offs_top = h_offs;
      risesync_offs_top = 0xffff;
      fallsync_bot      = 0xffff;
      risesync_bot      = 0x0000;
      fallsync_offs_bot = 0xffff;
      risesync_offs_bot = (clocksperline/2)+h_offs;
    }
    else
    {
      fallsync_top      = 0xffff;
      risesync_top      = last_top_line_nr;
      fallsync_offs_top = 0xffff;
      risesync_offs_top = (clocksperline/2) + h_offs;
      fallsync_bot      = last_bot_line_nr;
      risesync_bot      = 0xffff;
      fallsync_offs_bot = clocksperline + h_offs;
      risesync_offs_bot = 0xffff;
    }
  }

  ulVSyncLineTop = fallsync_top << 16 | risesync_top;
  ulVSyncLineBot = fallsync_bot << 16 | risesync_bot;

  ulVSyncPixOffTop = fallsync_offs_top << 16 | risesync_offs_top;
  ulVSyncPixOffBot = fallsync_offs_bot << 16 | risesync_offs_bot;

  DEBUGF2(2,("%s: syncid = %d top = 0x%08lx bottom = 0x%08lx\n",__PRETTY_FUNCTION__,outputid,ulVSyncLineTop,ulVSyncLineBot));

  DEXIT();
}


void CSTmSDVTG::GetProgressiveVSyncPositions(int                        outputid,
                                             const stm_timing_params_t &TimingParams,
                                             ULONG                      clocksperline,
                                             ULONG                     &ulVSyncLineTop,
                                             ULONG                     &ulVSyncPixOffTop)
{
  DENTRY();

  ULONG risesync_top = 0, fallsync_top = 0;
  ULONG risesync_offs_top = 0, fallsync_offs_top = 0;
  LONG  h_offs = m_HSyncOffsets[outputid];

  if (m_bDoubleClocked)
    h_offs *= 2;

  /*
   * The VSync programming is in lines (which are odd for top field and
   * even for bottom field).
   */
  if ((m_syncType[outputid] == STVTG_SYNC_POSITIVE) ||
      (m_syncType[outputid] == STVTG_SYNC_TIMING_MODE && TimingParams.VSyncPolarity))
  { // VSync positive
    DEBUGF2(2,("%s: syncid = %d positive\n",__PRETTY_FUNCTION__,outputid));

    if (h_offs >= 0)
    {
      risesync_top = 1;
      fallsync_top = risesync_top + TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = (unsigned long) h_offs;
    }
    else
    {
      risesync_top = TimingParams.LinesByFrame;
      fallsync_top = TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = clocksperline + h_offs;
    }

  }
  else if ((m_syncType[outputid] == STVTG_SYNC_NEGATIVE) ||
      (m_syncType[outputid] == STVTG_SYNC_TIMING_MODE && !TimingParams.VSyncPolarity))

  { // VSync negative
    DEBUGF2(2,("%s: syncid = %d negative\n",__PRETTY_FUNCTION__,outputid));

    if (h_offs >= 0)
    {
      fallsync_top = 1;
      risesync_top = fallsync_top + TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = (unsigned long) h_offs;
    }
    else
    {
      fallsync_top = TimingParams.LinesByFrame;
      risesync_top = TimingParams.VSyncPulseWidth;

      fallsync_offs_top = risesync_offs_top = clocksperline + h_offs;
    }
  }
  else
  {
    DEBUGF2(2,("%s: syncid = %d TnB/BnT meaningless in progressive modes\n",__PRETTY_FUNCTION__,outputid));
  }

  ulVSyncLineTop   = (fallsync_top << 16)      | risesync_top;
  ulVSyncPixOffTop = (fallsync_offs_top << 16) | risesync_offs_top;

  DEBUGF2(2,("%s: syncid = %d top = 0x%08lx\n",__PRETTY_FUNCTION__,outputid,ulVSyncLineTop));

  DEXIT();
}


void CSTmSDVTG::ProgramVTGTimings(const stm_mode_line_t* pModeLine)
{
  DENTRY();

  stm_timing_params_t TimingParams;

  // Get corresponding register parameters in that line
  TimingParams = pModeLine->TimingParams;

  ULONG ulMOD = ReadVTGReg(VTGn_VTGMOD);
  ULONG clocksperline;

  if(m_bDoubleClocked)
    clocksperline = TimingParams.PixelsPerLine*2;
  else
    clocksperline = TimingParams.PixelsPerLine;

  // Set the number of clock cycles per line
  WriteVTGReg(VTGn_CLKLN, clocksperline);

  // Set Half Line Per Field
  if(pModeLine->ModeParams.ScanType == SCAN_P)
  { // Progressive
    WriteVTGReg(VTGn_HLFLN, 2*TimingParams.LinesByFrame);
  }
  else
  { // Interlaced
    WriteVTGReg(VTGn_HLFLN, TimingParams.LinesByFrame);

    /*
     * VTG normally determines an interlaced mode because HLFLN is ODD
     * but there are some interlaced HD modes where the lines per frame is EVEN
     * so we need to force the VTG to be interlaced in these modes.
     */
    if((TimingParams.LinesByFrame & 0x1) == 0)
    {
      DEBUGF2(2,("CSTmSDVTG::ProgramVTGTimings Forced Interlaced Timing\n"));
      ulMOD |= VTG_MOD_FORCE_INTERLACED;
    }
  }

  /*
   * Set the HRef pulse programming.
   */
  if ((m_syncType[0] == STVTG_SYNC_NEGATIVE) ||
      (m_syncType[0] == STVTG_SYNC_TIMING_MODE && !TimingParams.HSyncPolarity))
  { // HRef is requested to be negative
      DEBUGF2(2,("CSTmSDVTG::ProgramVTGTimings Inverting HRef polarity\n"));
      ulMOD |= VTG_MOD_HREF_INV_POLARITY;
  }

  ULONG ulHSync,
        ulVSyncLineTop, ulVSyncLineBot,
        ulVSyncOffTop,  ulVSyncOffBot;
  bool  isInterlaced   = (pModeLine->ModeParams.ScanType == SCAN_I) ? true
                                                                    : false;

  /*
   * Set VRef pulse programming
   */
  if ((m_syncType[0] == STVTG_SYNC_NEGATIVE) ||
      (m_syncType[0] == STVTG_SYNC_TIMING_MODE && !TimingParams.VSyncPolarity))
  { // VRef is requested to be negative
    DEBUGF2(2,("CSTmSDVTG::ProgramVTGTimings Inverting VRef polarity\n"));
    ulMOD |= VTG_MOD_VREF_INV_POLARITY;
  }

  for (int i = 0; i < STMVTG_MAX_SYNC_OUTPUTS; ++i)
  {
    if(m_bSyncEnabled[i])
    {
      GetHSyncPositions(i+1, TimingParams, ulHSync);
      if(isInterlaced && ((TimingParams.LinesByFrame & 0x1) == 1))
      {
        GetInterlacedVSyncPositions(i+1, TimingParams, clocksperline,
                                    ulVSyncLineTop, ulVSyncLineBot,
                                    ulVSyncOffTop, ulVSyncOffBot);
      }
      else
      {
        GetProgressiveVSyncPositions(i+1, TimingParams, clocksperline,
                                    ulVSyncLineTop, ulVSyncOffTop);
        ulVSyncLineBot = ulVSyncLineTop;
        ulVSyncOffBot  = ulVSyncOffTop;
      }

      WriteVTGReg(vtg_regs[i].h_hd, ulHSync);           //     HD_HS |     HD_HO
      WriteVTGReg(vtg_regs[i].top_v_vd,ulVSyncLineTop); // VD_TOP_VS | VD_TOP_VO
      WriteVTGReg(vtg_regs[i].bot_v_vd,ulVSyncLineBot); // VD_BOT_VS | VD_BOT_VO
      WriteVTGReg(vtg_regs[i].top_v_hd,ulVSyncOffTop);  // VD_TOP_HS | VD_TOP_HO
      WriteVTGReg(vtg_regs[i].bot_v_hd,ulVSyncOffBot);  // VD_BOT_HS | VD_BOT_HO

      if (i == 1)
      {
        DEBUGF2(4,("%s: ulHSync strt/stp : %lu/%lu\n", __PRETTY_FUNCTION__, ulHSync & 0xffff, ulHSync >> 16));
        DEBUGF2(4,("%s: ulVSyncLineTop   : %lu/%lu\n", __PRETTY_FUNCTION__, ulVSyncLineTop & 0xffff, ulVSyncLineTop >> 16));
        DEBUGF2(4,("%s: ulVSyncLineBot   : %lu/%lu\n", __PRETTY_FUNCTION__, ulVSyncLineBot & 0xffff, ulVSyncLineBot >> 16));
        DEBUGF2(4,("%s: ulVSyncOffTop    : %lu/%lu\n", __PRETTY_FUNCTION__, ulVSyncOffTop & 0xffff, ulVSyncOffTop >> 16));
        DEBUGF2(4,("%s: ulVSyncOffBot    : %lu/%lu\n", __PRETTY_FUNCTION__, ulVSyncOffBot & 0xffff, ulVSyncOffBot >> 16));
        DEBUGF2(4,("%s: clocksperline    : %lu\n", __PRETTY_FUNCTION__, clocksperline));
      }
    }
  }

  WriteVTGReg(VTGn_VTGMOD, ulMOD);

  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_VTGMOD     %#.8lx\n", ReadVTGReg(VTGn_VTGMOD)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_CLKLN      %#.8lx\n", ReadVTGReg(VTGn_CLKLN)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_HLFLN      %#.8lx\n", ReadVTGReg(VTGn_HLFLN)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_H_HD_1     %#.8lx\n", ReadVTGReg(VTGn_H_HD_1)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_TOP_V_VD_1 %#.8lx\n", ReadVTGReg(VTGn_TOP_V_VD_1)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_BOT_V_VD_1 %#.8lx\n", ReadVTGReg(VTGn_BOT_V_VD_1)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_TOP_V_HD_1 %#.8lx\n", ReadVTGReg(VTGn_TOP_V_HD_1)));
  DEBUGF2(2, ("CSTmSDVTG::ProgramVTGTimings VTG_BOT_V_HD_1 %#.8lx\n", ReadVTGReg(VTGn_BOT_V_HD_1)));

  m_pCurrentMode = pModeLine;

  DEXIT();
}

/* Compare VTG hardware address and and store if not previously registered */
static void AddVtgAddress( unsigned long *addr)
{
  unsigned n;

  if ( VtgCount == STMVTG_MAX_SYNC_OUTPUTS )
    return;

  for(n=0;n<VtgCount; ++n)
  {
    if ( VtgRegBases[n] == addr )
      return;
  }
  
  VtgRegBases[VtgCount] = addr;
  ++VtgCount;
}


bool CSTmSDVTG::Start(const stm_mode_line_t* pModeLine)
{
  DENTRY();

  if(m_pFSynth)
    m_pFSynth->Start(pModeLine->TimingParams.ulPixelClock);

  AddVtgAddress(m_pDevRegs + (m_ulVTGOffset>>2) );
 
  WriteVTGReg(VTGn_VTGMOD,(VTG_MOD_MASTER_MODE | VTG_MOD_DISABLE));
  m_bDisabled = true;

  ProgramVTGTimings(pModeLine);

  // Enable interrupts on VTG. First read to clear any pending
  (void)ReadVTGReg(VTGn_ITS);
  // Enable field interrupts
  WriteVTGReg(VTGn_ITM, VTG_ITS_VST|VTG_ITS_VSB);

  RestoreSyncs();

  DEXIT();
  return true;
}


bool CSTmSDVTG::Start(const stm_mode_line_t* pModeLine, int externalid)
{
  DENTRY();

  /*
   * Set the basic slave mode parameters.
   */
  ULONG mode = VTG_MOD_DISABLE;
  m_bDisabled = true;

  AddVtgAddress(m_pDevRegs + (m_ulVTGOffset>>2) );

  mode |= (externalid==0)?VTG_MOD_SLAVE_EXT0:VTG_MOD_SLAVE_EXT1;

  if((m_syncType[0] == STVTG_SYNC_TOP_NOT_BOT) ||
     (m_syncType[0] == STVTG_SYNC_BOT_NOT_TOP))
  {
    /*
     * We are letting the hsync freerun, rather than sync to the incoming
     * signals because it produces a much more reliable startup of the
     * slave VTG; we do not fully understand this particularly as our input
     * syncs are only ever from VTG1 (in reality) so they should be in perfect
     * step.
     */
    mode |= VTG_MOD_SLAVE_ODDEVEN_HFREERUN;
    if(m_syncType[0] == STVTG_SYNC_TOP_NOT_BOT)
      mode |= VTG_MOD_VINPUT_INV_POLARITY;
  }

  if ((m_syncType[0] == STVTG_SYNC_NEGATIVE) ||
      (m_syncType[0] == STVTG_SYNC_TIMING_MODE && !pModeLine->TimingParams.HSyncPolarity))
  {
    mode |= VTG_MOD_HINPUT_INV_POLARITY;
  }

  if ((m_syncType[0] == STVTG_SYNC_NEGATIVE) ||
      (m_syncType[0] == STVTG_SYNC_TIMING_MODE && !pModeLine->TimingParams.VSyncPolarity))
  {
    mode |= VTG_MOD_VINPUT_INV_POLARITY;
  }

  WriteVTGReg(VTGn_VTGMOD, mode);

  ProgramVTGTimings(pModeLine);

  DEBUGF2(2, ("CSTmSDVTG::Start(slave) VTGn_MODE     %#.8lx\n", ReadVTGReg(VTGn_VTGMOD)));

  /*
   * We do not enable interrupts in slave mode, software uses VTG1, just
   * kick the thing info life.
   */
  RestoreSyncs();

  DEXIT();
  return true;
}


void CSTmSDVTG::Stop(void)
{
  DENTRY();
  // Disable interrupts on VTG
  WriteVTGReg(VTGn_ITM, 0);

  WriteVTGReg(VTGn_VTGMOD, VTG_MOD_DISABLE);

  m_pCurrentMode = 0;
  m_pPendingMode = 0;
  m_bUpdateClockFrequency = false;

  DEXIT();
}


void CSTmSDVTG::ResetCounters(void)
{
  DENTRY();
  WriteVTGReg(VTGn_DRST, 1);
  DEXIT();
}


void CSTmSDVTG::DisableSyncs(void)
{
  DENTRY();

  if(m_pCurrentMode && !m_bDisabled)
  {
    ULONG var = ReadVTGReg(VTGn_VTGMOD) | VTG_MOD_DISABLE;
    WriteVTGReg(VTGn_VTGMOD, var);
    m_bDisabled = true;
  }

  DEXIT();
}


void CSTmSDVTG::RestoreSyncs(void)
{
  DENTRY();

  if(m_pCurrentMode && m_bDisabled)
  {
    /* This function has been modified to reset all VTGS initialised so far.
       It makes the following assumptions:
       - The first VTG registered is the master
       - The reset order appears to be; stop VTG, restart VTG, reset counters
       - A delay in uS specified at module load is used to synchronise the clocks
    */
    unsigned n;
    unsigned long flags;
 
    /* Critical code section */
    vtg_spin_lock_irqsave( );

    /* Stop VTGs */
    for(n=0;n<VtgCount;++n)
    {
      LONG var = g_pIOS->ReadRegister(VtgRegBases[n] +(VTGn_VTGMOD >> 2));
      var |= VTG_MOD_DISABLE;

      g_pIOS->WriteRegister(VtgRegBases[n] +(VTGn_VTGMOD >> 2), var);
    }

    /* Start VTGs and reset VTG counters */
    for(n=0;n<VtgCount;++n)
    {
      LONG var = g_pIOS->ReadRegister(VtgRegBases[n] +(VTGn_VTGMOD >> 2));
      var &= ~VTG_MOD_DISABLE;
      g_pIOS->WriteRegister(VtgRegBases[n] +(VTGn_VTGMOD >> 2), var);

      g_pIOS->WriteRegister(VtgRegBases[n] +(VTGn_DRST >> 2), 1);

      if (n != (VtgCount-1))
        vtg_delay( );
    }

    vtg_spin_unlock_irqrestore( );
 
    /* Critical code section ends */

    m_bDisabled = false;
  }

  DEXIT();
}


stm_field_t CSTmSDVTG::GetInterruptStatus(void)
{
  ULONG ulIntStatus =  ReadVTGReg(VTGn_ITS);

  /*
   * Some chips need to write back to clear the interrupts
   */
  WriteVTGReg(VTGn_ITS,ulIntStatus);

  (void)ReadVTGReg(VTGn_ITS); // sync bus write

  if((ulIntStatus & (VTG_ITS_VST | VTG_ITS_VSB)) == (VTG_ITS_VST | VTG_ITS_VSB))
  {
    if (!m_bSuppressMissedInterruptMessage)
    {
      m_bSuppressMissedInterruptMessage = true;
      DEBUGF2(3,("CSTmSDVTG::GetInterruptStatus TOP and BOTTOM Fields Asserted\n"));
      DEBUGF2(3,("CSTmSDVTG::GetInterruptStatus guessing BOTTOM field\n"));
    }

    return STM_BOTTOM_FIELD;
  }

  m_bSuppressMissedInterruptMessage = false;

  if(ulIntStatus & VTG_ITS_VST)
  {
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

  if(ulIntStatus & VTG_ITS_VSB)
    return STM_BOTTOM_FIELD;

  return STM_UNKNOWN_FIELD;
}
