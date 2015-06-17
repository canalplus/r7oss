/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc8vtg.cpp
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

#include <display/ip/stmviewport.h>

#include "stmc8vtg.h"

CSTmC8VTG::CSTmC8VTG(CDisplayDevice      *pDev,
                     uint32_t             uRegOffset,
                     uint32_t             uNumSyncOutputs,
                     CSTmFSynth          *pFSynth,
                     bool                 bDoubleClocked,
                     stm_vtg_sync_type_t  refpol,
                     bool                 bDisableSyncsOnStop,
                     bool                 bUseSlaveInterrupts): CSTmVTG(pDev,
                                                                        pFSynth,
                                                                        bDoubleClocked,
                                                                        uNumSyncOutputs)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  ASSERTF((uNumSyncOutputs<=STM_C8VTG_MAX_SYNC_OUTPUTS),(""));

  m_pDevRegs     = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_uVTGOffset   = uRegOffset;

  /*
   * The required basic VTG mode settings (master/slave) that will be used
   * by ProgramVTGTimings to construct the full mode register configuration.
   *
   * This will be configured by the Start() methods.
   */
  m_uVTGStartModeConfig = VTG_MOD_DISABLE;

  /*
   * The default behaviour of the VTG class is to disable the sync generation
   * when Stop() is called. This can be overridden by sub-classes to manage
   * different usage senarios.
   */
  m_bDisableSyncsOnStop = bDisableSyncsOnStop;

  /*
   * When configured to slave from an odd/even signal the general design
   * recommendation is to leave the hsync freerunning. This is in fact the
   * only configuration where slaving works on 7xxx series SoCs when
   * implementing the Main->DENC use case. However we may need to override this
   * for other use cases.
   */
  m_bUseOddEvenHSyncFreerun = true;

  m_bDoSoftResetInInterrupt = false;

  m_bUseSlaveInterrupts = bUseSlaveInterrupts;

  SetSyncType(STM_SYNC_SEL_REF, refpol);

  m_syncRegOffsets = 0;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmC8VTG::~CSTmC8VTG() {}

bool CSTmC8VTG::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Note: The current driver implementation does not support the correct
   * placement of vref and the start of the vsync waveforms on the bottom field
   * for interlaced modes with an even total of lines, e.g. 1250 line
   * AS4933.1/CEA-861 mode 39.
   *
   * It isn't clear if this now ever needs to be supported, or precisely how to
   * program it, because to get vref in the correct place it looks like you have
   * to reprogram the HLFLN register differently for top and bottom fields.
   * Something that is currently understood to be not allowed while the VTGs
   * are active, i.e. you can only safely re-program during a bottom field.
   */
  if((pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN) &&
     (pModeLine->mode_timing.lines_per_frame & 0x1) == 0)
  {
    return false;
  }

  if(!CSTmVTG::Start(pModeLine))
  {
    TRC( TRC_ID_ERROR, "Base VTG implementation failed to start" );
    return false;
  }

  m_uVTGStartModeConfig = VTG_MOD_MASTER_MODE;

  if(m_bDisabled)
  {
    ProgramVTGTimings(pModeLine);
    RestoreSyncs();
  }
  else
  {
    vibe_os_lock_resource(m_lock);
    m_PendingMode = *pModeLine;
    m_bUpdateOnlyClockFrequency = false; // Should be false already, but make sure
    vibe_os_unlock_resource(m_lock);
  }

  /*
   * This is a temporary solution to setting the MISR calculation window to the
   * active video until the MISR code can be updated to use
   * SetOutputWindowRectangle correctly.
   */
  SetOutputWindowRectangle(STCalculateViewportPixel(pModeLine,0),
                           STCalculateViewportLine(pModeLine,0),
                           pModeLine->mode_params.active_area_width,
                           pModeLine->mode_params.active_area_height);
  if(m_pSlavedVTG)
    m_pSlavedVTG->SetOutputWindowRectangle(STCalculateViewportPixel(pModeLine,0),
                                           STCalculateViewportLine(pModeLine,0),
                                           pModeLine->mode_params.active_area_width,
                                           pModeLine->mode_params.active_area_height);

  if(!m_bIsSlaveVTG && !m_bUseSlaveInterrupts)
    EnableInterrupts();

  /*
   * We need to block until deferred programming in interrupt has actually
   * happened.
   */
  while(m_PendingMode.mode_id != STM_TIMING_MODE_RESERVED)
    vibe_os_stall_execution(5000); // Wait in 5ms chunks.

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmC8VTG::Start(const stm_display_mode_t* pModeLine, int externalid)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Set the basic slave mode parameters.
   */
  uint32_t mode = 0;

  mode |= (externalid==0)?VTG_MOD_SLAVE_EXT0:VTG_MOD_SLAVE_EXT1;

  if((m_SlavedVTGInputSyncType == STVTG_SYNC_TOP_NOT_BOT) ||
     (m_SlavedVTGInputSyncType == STVTG_SYNC_BOT_NOT_TOP))
  {
    mode |= VTG_MOD_SLAVE_ODDEVEN_NVSYNC;
    if(m_bUseOddEvenHSyncFreerun)
      mode |= VTG_MOD_SLAVE_ODDEVEN_HFREERUN;
  }

  if ((m_SlavedVTGInputSyncType == STVTG_SYNC_NEGATIVE) ||
      (m_SlavedVTGInputSyncType == STVTG_SYNC_TIMING_MODE && !pModeLine->mode_timing.hsync_polarity))
  {
    mode |= VTG_MOD_HINPUT_INV_POLARITY;
  }

  if ((m_SlavedVTGInputSyncType == STVTG_SYNC_NEGATIVE) ||
      (m_SlavedVTGInputSyncType == STVTG_SYNC_TOP_NOT_BOT) ||
      (m_SlavedVTGInputSyncType == STVTG_SYNC_TIMING_MODE && !pModeLine->mode_timing.vsync_polarity))
  {
    mode |= VTG_MOD_VINPUT_INV_POLARITY;
  }

  m_uVTGStartModeConfig = mode;

  /*
   * Only re-program the hardware now if we are disabled, otherwise it will
   * get triggered by the master in its interrupt handling.
   */
  if(m_bDisabled)
    ProgramVTGTimings(pModeLine);

  if(m_bIsSlaveVTG && m_bUseSlaveInterrupts)
    EnableInterrupts();

  /*
   * We defer the enabling of the syncs to the master RestoreSyncs().
   */

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmC8VTG::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bDisableSyncsOnStop)
    DisableSyncs();

  if((!m_bIsSlaveVTG && !m_bUseSlaveInterrupts) ||
     (m_bIsSlaveVTG  && m_bUseSlaveInterrupts))
  {
    DisableInterrupts();
  }

  /*
   * Cancel any pending soft reset, it may be actively harmful if it now
   * happened when we next restore interrupt processing.
   */
  m_bDoSoftResetInInterrupt = false;

  CSTmVTG::Stop();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG::RestoreSyncs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Note: We try and restore the syncs on the slave class first; it may or may
   *       not do anything depending on the exact underlying hardware version.
   */
  if(m_pSlavedVTG)
    m_pSlavedVTG->RestoreSyncs();

  if(IsStarted())
  {
    ResetCounters();
    EnableSyncs();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG::GetHSyncPositions(int                             outputid,
                            const stm_display_mode_timing_t      &TimingParams,
                                  uint32_t                       &HSync)
{
  uint32_t start,stop;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  CSTmVTG::GetHSyncPositions(outputid,TimingParams,&start,&stop);

  HSync = start | (stop<<16);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG::GetInterlacedVSyncPositions(int                       outputid,
                                      const stm_display_mode_timing_t &TimingParams,
                                            uint32_t                   clocksperline,
                                            uint32_t                  &VSyncLineTop,
                                            uint32_t                  &VSyncLineBot,
                                            uint32_t                  &VSyncPixOffTop,
                                            uint32_t                  &VSyncPixOffBot)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t risesync_top, fallsync_top,
        risesync_bot, fallsync_bot;
  uint32_t risesync_offs_top, fallsync_offs_top,
        risesync_offs_bot, fallsync_offs_bot;

  int32_t  h_offs = m_syncParams[outputid].m_VSyncHOffsets;
  uint32_t last_top_line_nr = (TimingParams.lines_per_frame / 2) + 1;
  uint32_t last_bot_line_nr = (TimingParams.lines_per_frame / 2);

  if (m_bDoubleClocked)
    h_offs *= 2;

  /*
   * Limit the offset to half a line; in reality the offsets are only going to
   * be a handful of clocks so this is being a bit paranoid.
   */
  if(h_offs >= (int32_t)(clocksperline/2))
    h_offs = (int32_t)(clocksperline/2) - 1L;

  if(h_offs <= -((int32_t)(clocksperline/2)))
    h_offs = -((int32_t)(clocksperline/2)) + 1L;

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
  if ((m_syncParams[outputid].m_syncType == STVTG_SYNC_POSITIVE) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_TIMING_MODE && TimingParams.vsync_polarity) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_INVERSE_TIMING_MODE && !TimingParams.vsync_polarity))
  {
    TRC( TRC_ID_MAIN_INFO, "syncid = %d VSync Positive",outputid );

    if (h_offs >= 0)
    {
      risesync_top = 1;
      fallsync_top = risesync_top + TimingParams.vsync_width;
      risesync_bot = 0;
      fallsync_bot = TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = (uint32_t) h_offs;
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
      fallsync_bot = TimingParams.vsync_width;
      risesync_offs_top = fallsync_offs_bot = (clocksperline/2) + h_offs;

      /*
       * Now the waveform for the top field vsync, which is starting just
       * before the end of the last full line of the VTG bottom field
       */
      risesync_bot = last_bot_line_nr;
      fallsync_top = TimingParams.vsync_width;
      risesync_offs_bot = fallsync_offs_top = clocksperline + h_offs;
    }
  }
  else if ((m_syncParams[outputid].m_syncType == STVTG_SYNC_NEGATIVE) ||
           (m_syncParams[outputid].m_syncType == STVTG_SYNC_TIMING_MODE && !TimingParams.vsync_polarity) ||
           (m_syncParams[outputid].m_syncType == STVTG_SYNC_INVERSE_TIMING_MODE && TimingParams.vsync_polarity))
  {
    TRC( TRC_ID_MAIN_INFO, "syncid = %d VSync Negative",outputid );

    if (h_offs >= 0)
    {
      fallsync_top = 1;
      risesync_top = fallsync_top + TimingParams.vsync_width;
      fallsync_bot = 0;
      risesync_bot = TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = (uint32_t) h_offs;
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
      risesync_bot = TimingParams.vsync_width;
      fallsync_offs_top = risesync_offs_bot = (clocksperline/2) + h_offs;

      /*
       * Top vsync waveform.
       */
      fallsync_bot = last_bot_line_nr;
      risesync_top = TimingParams.vsync_width;
      fallsync_offs_bot = risesync_offs_top = clocksperline + h_offs;
    }
  }
  else if (m_syncParams[outputid].m_syncType == STVTG_SYNC_TOP_NOT_BOT)
  {
    TRC( TRC_ID_MAIN_INFO, "syncid = %d Top not Bot",outputid );
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
    TRC( TRC_ID_MAIN_INFO, "syncid = %d Bot not Top",outputid );
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

  VSyncLineTop = fallsync_top << 16 | risesync_top;
  VSyncLineBot = fallsync_bot << 16 | risesync_bot;

  VSyncPixOffTop = fallsync_offs_top << 16 | risesync_offs_top;
  VSyncPixOffBot = fallsync_offs_bot << 16 | risesync_offs_bot;

  TRC( TRC_ID_MAIN_INFO, "syncid = %d top = 0x%08x bottom = 0x%08x",outputid,VSyncLineTop,VSyncLineBot );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG::GetProgressiveVSyncPositions(int                        outputid,
                                       const stm_display_mode_timing_t &TimingParams,
                                             uint32_t                   clocksperline,
                                             uint32_t                  &VSyncLineTop,
                                             uint32_t                  &VSyncPixOffTop)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t risesync_top = 0, fallsync_top = 0;
  uint32_t risesync_offs_top = 0, fallsync_offs_top = 0;
  int32_t  h_offs = m_syncParams[outputid].m_VSyncHOffsets;

  if (m_bDoubleClocked)
    h_offs *= 2;

  /*
   * The VSync programming is in lines (which are odd for top field and
   * even for bottom field).
   */
  if ((m_syncParams[outputid].m_syncType == STVTG_SYNC_POSITIVE) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_TIMING_MODE && TimingParams.vsync_polarity) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_INVERSE_TIMING_MODE && !TimingParams.vsync_polarity))
  { // VSync positive
    TRC( TRC_ID_MAIN_INFO, "syncid = %d positive",outputid );

    if (h_offs >= 0)
    {
      risesync_top = 1;
      fallsync_top = risesync_top + TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = (unsigned long) h_offs;
    }
    else
    {
      risesync_top = TimingParams.lines_per_frame;
      fallsync_top = TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = clocksperline + h_offs;
    }

  }
  else if ((m_syncParams[outputid].m_syncType == STVTG_SYNC_NEGATIVE) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_TIMING_MODE && !TimingParams.vsync_polarity) ||
      (m_syncParams[outputid].m_syncType == STVTG_SYNC_INVERSE_TIMING_MODE && TimingParams.vsync_polarity))

  { // VSync negative
    TRC( TRC_ID_MAIN_INFO, "syncid = %d negative",outputid );

    if (h_offs >= 0)
    {
      fallsync_top = 1;
      risesync_top = fallsync_top + TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = (unsigned long) h_offs;
    }
    else
    {
      fallsync_top = TimingParams.lines_per_frame;
      risesync_top = TimingParams.vsync_width;

      fallsync_offs_top = risesync_offs_top = clocksperline + h_offs;
    }
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "syncid = %d TnB/BnT meaningless in progressive modes",outputid );
  }

  VSyncLineTop   = (fallsync_top << 16)      | risesync_top;
  VSyncPixOffTop = (fallsync_offs_top << 16) | risesync_offs_top;

  TRC( TRC_ID_MAIN_INFO, "syncid = %d top = 0x%08x",outputid,VSyncLineTop );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG::ProgramSyncOutputs(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  const stm_display_mode_timing_t &TimingParams = pModeLine->mode_timing;
  uint32_t clocksperline  = TimingParams.pixels_per_line;
  bool     isInterlaced   = (pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN);
  uint32_t HSync;
  uint32_t VSyncLineTop, VSyncLineBot;
  uint32_t VSyncOffTop,  VSyncOffBot;

  if(m_bDoubleClocked)
    clocksperline *= 2;

  for (int i = 0; i < m_max_sync_outputs; ++i)
  {
    if ((m_pendingSync & (1LU<<i)) == (1LU<<i))
    {
      GetHSyncPositions(i+1, TimingParams, HSync);
      if(isInterlaced && ((TimingParams.lines_per_frame & 0x1) == 1))
      {
        GetInterlacedVSyncPositions(i+1, TimingParams, clocksperline,
                                    VSyncLineTop, VSyncLineBot,
                                    VSyncOffTop, VSyncOffBot);
      }
      else
      {
        GetProgressiveVSyncPositions(i+1, TimingParams, clocksperline,
                                    VSyncLineTop, VSyncOffTop);
        VSyncLineBot = VSyncLineTop;
        VSyncOffBot  = VSyncOffTop;
      }

      WriteVTGReg(m_syncRegOffsets[i].h_hd,     HSync);        //     HD_HS |     HD_HO
      WriteVTGReg(m_syncRegOffsets[i].top_v_vd, VSyncLineTop); // VD_TOP_VS | VD_TOP_VO
      WriteVTGReg(m_syncRegOffsets[i].bot_v_vd, VSyncLineBot); // VD_BOT_VS | VD_BOT_VO
      WriteVTGReg(m_syncRegOffsets[i].top_v_hd, VSyncOffTop);  // VD_TOP_HS | VD_TOP_HO
      WriteVTGReg(m_syncRegOffsets[i].bot_v_hd, VSyncOffBot);  // VD_BOT_HS | VD_BOT_HO

      if (i == 1)
      {
        TRC( TRC_ID_UNCLASSIFIED, "ulHSync strt/stp : %u/%u", HSync & 0xffff, HSync >> 16 );
        TRC( TRC_ID_UNCLASSIFIED, "ulVSyncLineTop   : %u/%u", VSyncLineTop & 0xffff, VSyncLineTop >> 16 );
        TRC( TRC_ID_UNCLASSIFIED, "ulVSyncLineBot   : %u/%u", VSyncLineBot & 0xffff, VSyncLineBot >> 16 );
        TRC( TRC_ID_UNCLASSIFIED, "ulVSyncOffTop    : %u/%u", VSyncOffTop & 0xffff, VSyncOffTop >> 16 );
        TRC( TRC_ID_UNCLASSIFIED, "ulVSyncOffBot    : %u/%u", VSyncOffBot & 0xffff, VSyncOffBot >> 16 );
      }

      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG::ProgramSyncOutputs(this=%p): VTG_H_HD_%d     %#.8x", this, i+1, ReadVTGReg(m_syncRegOffsets[i].h_hd) );
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG::ProgramSyncOutputs(this=%p): VTG_TOP_V_VD_%d %#.8x", this, i+1, ReadVTGReg(m_syncRegOffsets[i].top_v_vd) );
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG::ProgramSyncOutputs(this=%p): VTG_BOT_V_VD_%d %#.8x", this, i+1, ReadVTGReg(m_syncRegOffsets[i].bot_v_vd) );
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG::ProgramSyncOutputs(this=%p): VTG_TOP_V_HD_%d %#.8x", this, i+1, ReadVTGReg(m_syncRegOffsets[i].top_v_hd) );
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG::ProgramSyncOutputs(this=%p): VTG_BOT_V_HD_%d %#.8x", this, i+1, ReadVTGReg(m_syncRegOffsets[i].bot_v_hd) );
    }
  }
  m_pendingSync=0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
