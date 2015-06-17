/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc8vtg_v8_4.cpp
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

#include "stmc8vtg_v8_4.h"


/* VTG offset registers ---------------------------------------------------*/
#define VTGn_VTGMOD           0x0000
#define VTGn_INV_BNOTTOP      0x0004                                    // New register
#define VTGn_CLKLN            0x0008
#define VTGn_VID_TFO          0x0040
#define VTGn_VID_TFS          0x0044
#define VTGn_VID_BFO          0x0048
#define VTGn_VID_BFS          0x004C
#define VTGn_H_HD_1           0x00C0
#define VTGn_TOP_V_VD_1       0x00C4
#define VTGn_BOT_V_VD_1       0x00C8
#define VTGn_TOP_V_HD_1       0x00CC
#define VTGn_BOT_V_HD_1       0x00D0
#define VTGn_H_HD_2           0x00E0
#define VTGn_TOP_V_VD_2       0x00E4
#define VTGn_BOT_V_VD_2       0x00E8
#define VTGn_TOP_V_HD_2       0x00EC
#define VTGn_BOT_V_HD_2       0x00F0
#define VTGn_H_HD_3           0x0100
#define VTGn_TOP_V_VD_3       0x0104
#define VTGn_BOT_V_VD_3       0x0108
#define VTGn_TOP_V_HD_3       0x010C
#define VTGn_BOT_V_HD_3       0x0110
#define VTGn_H_HD_4           0x0120
#define VTGn_TOP_V_VD_4       0x0124
#define VTGn_BOT_V_VD_4       0x0128
#define VTGn_TOP_V_HD_4       0x012C
#define VTGn_BOT_V_HD_4       0x0130
#define VTGn_H_HD_5           0x0140
#define VTGn_TOP_V_VD_5       0x0144
#define VTGn_BOT_V_VD_5       0x0148
#define VTGn_TOP_V_HD_5       0x014C
#define VTGn_BOT_V_HD_5       0x0150
#define VTGn_H_HD_6           0x0160
#define VTGn_TOP_V_VD_6       0x0164
#define VTGn_BOT_V_VD_6       0x0168
#define VTGn_TOP_V_HD_6       0x016C
#define VTGn_BOT_V_HD_6       0x0170

#define VTGn_HLFLN            0x000C
#define VTGn_DRST             0x0010
#define VTGn_LN_INT           0x0014
#define VTGn_LN_STAT          0x0018
#define VTGn_DFV              0x0070
#define VTGn_HOST_STA         0x0074
#define VTGn_HOST_ITS         0x0078
#define VTGn_HOST_ITS_BCLR    0x007C
#define VTGn_HOST_ITS_BSET    0x0080
#define VTGn_HOST_ITM         0x0084
#define VTGn_HOST_ITM_BCLR    0x0088
#define VTGn_HOST_ITM_BSET    0x008C
#define VTGn_RT_STA           0x00A4
#define VTGn_RT_ITS           0x00A8
#define VTGn_RT_ITS_BCLR      0x00AC
#define VTGn_RT_ITS_BSET      0x00B0
#define VTGn_RT_ITM           0x00B4
#define VTGn_RT_ITM_BCLR      0x00B8
#define VTGn_RT_ITM_BSET      0x00BC

#if defined(CONFIG_STM_HOST_CPU)
static const int VTGn_STA       =       VTGn_HOST_STA;       // HOST
static const int VTGn_ITM       =       VTGn_HOST_ITM;       // HOST
static const int VTGn_ITM_BCLR  =       VTGn_HOST_ITM_BCLR;  // HOST
static const int VTGn_ITM_BSET  =       VTGn_HOST_ITM_BSET;  // HOST
static const int VTGn_ITS       =       VTGn_HOST_ITS;       // HOST
static const int VTGn_ITS_BCLR  =       VTGn_HOST_ITS_BCLR;  // HOST
static const int VTGn_ITS_BSET  =       VTGn_HOST_ITS_BSET;  // HOST
#else
static const int VTGn_STA       =       VTGn_RT_STA;         // RT
static const int VTGn_ITM       =       VTGn_RT_ITM;         // RT
static const int VTGn_ITM_BCLR  =       VTGn_RT_ITM_BCLR;    // RT
static const int VTGn_ITM_BSET  =       VTGn_RT_ITM_BSET;    // RT
static const int VTGn_ITS       =       VTGn_RT_ITS;         // RT
static const int VTGn_ITS_BCLR  =       VTGn_RT_ITS_BCLR;    // RT
static const int VTGn_ITS_BSET  =       VTGn_RT_ITS_BSET;    // RT
#endif

#define VTG_ITS_VSB (1L<<0)
#define VTG_ITS_VST (1L<<1)
#define VTG_ITS_LNB (1L<<4)
#define VTG_ITS_LNT (1L<<5)

#define STM_C8VTG_MAX_SYNC_OUTPUTS 6


static const stm_vtg_reg_offsets_t vtg_regs[STM_C8VTG_MAX_SYNC_OUTPUTS] = {
  { VTGn_H_HD_1, VTGn_TOP_V_VD_1, VTGn_BOT_V_VD_1, VTGn_TOP_V_HD_1, VTGn_BOT_V_HD_1 },
  { VTGn_H_HD_2, VTGn_TOP_V_VD_2, VTGn_BOT_V_VD_2, VTGn_TOP_V_HD_2, VTGn_BOT_V_HD_2 },
  { VTGn_H_HD_3, VTGn_TOP_V_VD_3, VTGn_BOT_V_VD_3, VTGn_TOP_V_HD_3, VTGn_BOT_V_HD_3 },
  { VTGn_H_HD_4, VTGn_TOP_V_VD_4, VTGn_BOT_V_VD_4, VTGn_TOP_V_HD_4, VTGn_BOT_V_HD_4 },
  { VTGn_H_HD_5, VTGn_TOP_V_VD_5, VTGn_BOT_V_VD_5, VTGn_TOP_V_HD_5, VTGn_BOT_V_HD_5 },
  { VTGn_H_HD_6, VTGn_TOP_V_VD_6, VTGn_BOT_V_VD_6, VTGn_TOP_V_HD_6, VTGn_BOT_V_HD_6 }
};


CSTmC8VTG_V8_4::CSTmC8VTG_V8_4(
    CDisplayDevice      *pDev,
    uint32_t             uRegOffset,
    uint32_t             uNumSyncOutputs,
    CSTmFSynth          *pFSynth,
    bool                 bDoubleClocked,
    stm_vtg_sync_type_t  refpol,
    bool                 bDisableSyncsOnStop,
    bool                 bUseSlaveInterrupts): CSTmC8VTG(pDev,
                                                         uRegOffset,
                                                         uNumSyncOutputs,
                                                         pFSynth,
                                                         bDoubleClocked,
                                                         refpol,
                                                         bDisableSyncsOnStop,
                                                         bUseSlaveInterrupts)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_syncRegOffsets = vtg_regs;

  /*
   * This is a temporary override of the default behaviour, for the current
   * dual die configuration of the V8.4 VTG used on the STiH416/STiH407.
   */
  m_bUseOddEvenHSyncFreerun = false;

  m_dbg_count = 0;
  m_mismatched_sync_dbg_count = 5;

  /*
   * Set interrupt configuration
   */
  m_InterruptMask = (VTG_ITS_VST | VTG_ITS_VSB);

  /*
   * Ensure the VTG is initially disabled, for instance in case this driver
   * is being loaded after a boot loader has brought up a splash screen.
   *
   * Note: This is slight overkill, at least for current use on STiH416/STiH407, as
   *       the requirements of the dual VTG slaving means we have to do
   *       a hard reset of the blocks in the device class to put the VTGs back
   *       into a "clean" state to cope with the splash screen use case.
   */
  WriteVTGReg(VTGn_ITM_BCLR, (VTG_ITS_VST | VTG_ITS_VSB | VTG_ITS_LNT | VTG_ITS_LNB));

  /*
   * Enable the interrupt mask by default, this allows us to monitor the
   * interrupt status on both master and slave to see if they are in sync even
   * though the operating system will only register a handler for one of them.
   */
  WriteVTGReg(VTGn_ITM_BSET, m_InterruptMask);

  CSTmC8VTG_V8_4::DisableSyncs(); // Explicit call as this is virtual and we are in a constructor

  m_lastvsync = 0ULL;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmC8VTG_V8_4::~CSTmC8VTG_V8_4() {}


void CSTmC8VTG_V8_4::ProgramOutputWindow(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_UpdateOutputRect)
    return;

  uint32_t video_top_field_start    = ((m_OutputWindowRect.y << 16) | (m_OutputWindowRect.x));
  uint32_t video_top_field_stop     = (((m_OutputWindowRect.y + m_OutputWindowRect.height) << 16) | (m_OutputWindowRect.x + m_OutputWindowRect.width));
  uint32_t video_bottom_field_start = ((m_OutputWindowRect.y << 16) | (m_OutputWindowRect.x));
  uint32_t video_bottom_field_stop  = (((m_OutputWindowRect.y + m_OutputWindowRect.height) << 16) | (m_OutputWindowRect.x + m_OutputWindowRect.width));

  WriteVTGReg(VTGn_VID_TFO, video_top_field_start);
  WriteVTGReg(VTGn_VID_TFS, video_top_field_stop);
  WriteVTGReg(VTGn_VID_BFO, video_bottom_field_start);
  WriteVTGReg(VTGn_VID_BFS, video_bottom_field_stop);

  TRC( TRC_ID_MAIN_INFO, "CSTmC8VTG_V8_4::ProgramOutputWindow VTG_VID_TFO    %#.8x", ReadVTGReg(VTGn_VID_TFO) );
  TRC( TRC_ID_MAIN_INFO, "CSTmC8VTG_V8_4::ProgramOutputWindow VTG_VID_TFS    %#.8x", ReadVTGReg(VTGn_VID_TFS) );
  TRC( TRC_ID_MAIN_INFO, "CSTmC8VTG_V8_4::ProgramOutputWindow VTG_VID_BFO    %#.8x", ReadVTGReg(VTGn_VID_BFO) );
  TRC( TRC_ID_MAIN_INFO, "CSTmC8VTG_V8_4::ProgramOutputWindow VTG_VID_BFS    %#.8x", ReadVTGReg(VTGn_VID_BFS) );

  m_UpdateOutputRect = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::ProgramVTGTimings(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  stm_display_mode_timing_t TimingParams;

  // Get corresponding register parameters in that line
  TimingParams = pModeLine->mode_timing;

  uint32_t ulMOD = m_uVTGStartModeConfig;
  uint32_t clocksperline;

  if(m_bDoubleClocked)
    clocksperline = TimingParams.pixels_per_line*2;
  else
    clocksperline = TimingParams.pixels_per_line;

  // Set the number of clock cycles per line
  WriteVTGReg(VTGn_CLKLN, clocksperline);

  // Set Half Line Per Field
  if(pModeLine->mode_params.scan_type == STM_PROGRESSIVE_SCAN)
  { // Progressive
    WriteVTGReg(VTGn_HLFLN, 2*TimingParams.lines_per_frame);
  }
  else
  { // Interlaced
    WriteVTGReg(VTGn_HLFLN, TimingParams.lines_per_frame);
  }

  /*
   * Set the HRef pulse programming.
   */
  if ((m_syncParams[STM_SYNC_SEL_REF].m_syncType == STVTG_SYNC_NEGATIVE) ||
      (m_syncParams[STM_SYNC_SEL_REF].m_syncType == STVTG_SYNC_TIMING_MODE && !TimingParams.hsync_polarity))
  { // HRef is requested to be negative
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings Inverting HRef polarity" );
      ulMOD |= VTG_MOD_HREF_INV_POLARITY;
  }

  /*
   * Set VRef pulse programming
   */
  if ((m_syncParams[STM_SYNC_SEL_REF].m_syncType == STVTG_SYNC_NEGATIVE) ||
      (m_syncParams[STM_SYNC_SEL_REF].m_syncType == STVTG_SYNC_TIMING_MODE && !TimingParams.vsync_polarity))
  { // VRef is requested to be negative
    TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings Inverting VRef polarity" );
    ulMOD |= VTG_MOD_VREF_INV_POLARITY;
  }

  if (m_CurrentMode.mode_id == STM_TIMING_MODE_RESERVED)
    m_pendingSync = 0xFFFFFFFF;

  ProgramSyncOutputs(pModeLine);

  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTG_VTGMOD     %#.8x", ulMOD );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTG_CLKLN      %#.8x", ReadVTGReg(VTGn_CLKLN) );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTG_HLFLN      %#.8x", ReadVTGReg(VTGn_HLFLN) );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTGn_VID_TFO   %#.8x", ReadVTGReg(VTGn_VID_TFO) );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTGn_VID_TFS   %#.8x", ReadVTGReg(VTGn_VID_TFS) );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTGn_VID_BFO   %#.8x", ReadVTGReg(VTGn_VID_BFO) );
  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::ProgramVTGTimings VTGn_VID_BFS   %#.8x", ReadVTGReg(VTGn_VID_BFS) );

  WriteVTGReg(VTGn_VTGMOD, ulMOD);
  m_CurrentMode = *pModeLine;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::EnableInterrupts(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Enable interrupts on VTG, clear any old pending status bits first.
  WriteVTGReg(VTGn_ITS_BCLR, 0xFF);
  WriteVTGReg(VTGn_ITM_BSET, m_InterruptMask);

  TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::EnableInterrupts VTG_ITS %#.8x", ReadVTGReg(VTGn_ITS) );

  /*
   * Reset interrupt debug to show the first 20 interrupts immediately after the
   * enable.
   */
  m_dbg_count = 20;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::DisableInterrupts(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Disable interrupts on VTG
  vibe_os_lock_resource(m_lock);
  {
    WriteVTGReg(VTGn_ITM_BCLR, m_InterruptMask);
    m_lastvsync = 0ULL;
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::ResetCounters(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Only reset the counters on a master VTG.
   */
  if(!m_bIsSlaveVTG)
  {
    if(m_bDisabled)
      WriteVTGReg(VTGn_DRST, 1);
    else
      m_bDoSoftResetInInterrupt = true;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::DisableSyncs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t var = ReadVTGReg(VTGn_VTGMOD) | VTG_MOD_DISABLE;
  WriteVTGReg(VTGn_VTGMOD, var);
  TRC( TRC_ID_MAIN_INFO, "Disabling syncs expected value = %x VTGMOD = %x", var, ReadVTGReg(VTGn_VTGMOD) );
  m_bDisabled = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmC8VTG_V8_4::EnableSyncs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t var = ReadVTGReg(VTGn_VTGMOD) & ~VTG_MOD_DISABLE;
  WriteVTGReg(VTGn_VTGMOD, var);
  TRC( TRC_ID_MAIN_INFO, "Enabling syncs expected value = %x VTGMOD = %x", var, ReadVTGReg(VTGn_VTGMOD) );
  m_bDisabled = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmC8VTG_V8_4::SetBnottopType(stm_display_sync_id_t sync, stm_vtg_sync_Bnottop_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t val;

  if(sync == STM_SYNC_SEL_REF)
    return false;

  val =  ReadVTGReg(VTGn_INV_BNOTTOP) & ~(1 << (sync - 1));
  if(type == STVTG_SYNC_BNOTTOP_INVERTED)
    val |=  (1 << (sync - 1));
  WriteVTGReg(VTGn_INV_BNOTTOP, val);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTmC8VTG_V8_4::ReadHWInterruptStatus(void)
{
  uint32_t intStatus;

  intStatus =  ReadVTGReg(VTGn_ITS);

  if(intStatus != 0)
  {
    stm_time64_t now = vibe_os_get_system_time();

    WriteVTGReg(VTGn_ITS_BCLR, intStatus);
    (void)ReadVTGReg(VTGn_ITS); // sync bus write

    if(m_dbg_count > 0)
    {
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4 (this=%p): ITS = 0x%x now = %lld elapsed = %lld", this, intStatus,now,(m_lastvsync == 0ULL)?0ULL:(now - m_lastvsync) );
      m_dbg_count--;
    }

    if((intStatus & (VTG_ITS_VST | VTG_ITS_VSB)) == (VTG_ITS_VST | VTG_ITS_VSB))
    {
      if (!m_bSuppressMissedInterruptMessage)
      {
        m_bSuppressMissedInterruptMessage = true;
        TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4 (this=%p): TOP and BOTTOM flags asserted now = %lld elapsed = %lld", this,now,(m_lastvsync == 0ULL)?0ULL:(now - m_lastvsync) );
      }
    }
    else
    {
      m_bSuppressMissedInterruptMessage = false;
    }

    m_lastvsync = now;
  }

  return intStatus;
}


uint32_t CSTmC8VTG_V8_4::GetInterruptStatus(void)
{
  uint32_t intStatus,masterIntStatus,slaveIntStatus=0;
  uint32_t event = STM_TIMING_EVENT_NONE;

  vibe_os_lock_resource(m_lock);

  masterIntStatus =  ReadHWInterruptStatus();
  if(m_pSlavedVTG)
  {
    slaveIntStatus = m_pSlavedVTG->ReadHWInterruptStatus();
    if((slaveIntStatus & (VTG_ITS_VST | VTG_ITS_VSB)) != (masterIntStatus & (VTG_ITS_VST | VTG_ITS_VSB)))
    {
      if(m_mismatched_sync_dbg_count > 0)
      {
        TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4 (this=%p): Master(0x%x)/Slave(0x%x) ITS Mismatch now = %lld", this,masterIntStatus,slaveIntStatus,vibe_os_get_system_time() );

        m_mismatched_sync_dbg_count--;

        if(m_mismatched_sync_dbg_count == 0)
          TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4 (this=%p): Further ITS Mismatch messages suppressed", this );
      }
    }
    else
      m_mismatched_sync_dbg_count = 5; // Reset debug if we recover from error
  }

  intStatus = m_bUseSlaveInterrupts?slaveIntStatus:masterIntStatus;

  DASSERTF2((intStatus != 0),("CSTmC8VTG_V8_4::GetInterruptStatus: NULL interrupt status\n"), m_lock, STM_TIMING_EVENT_NONE);

  if((intStatus & (VTG_ITS_VST | VTG_ITS_VSB)) == (VTG_ITS_VST | VTG_ITS_VSB))
  {
    /*
     * We cannot risk accidentally resetting or re-programming the VTG when
     * we are not absolutely certain which state the VTG is in, otherwise we
     * might break it in an unrecoverable way. So simply return with something
     * appropriate and delay any other work until we have a known good state.
     */
    if(IsStarted())
      event = (m_CurrentMode.mode_params.scan_type == STM_INTERLACED_SCAN)?STM_TIMING_EVENT_BOTTOM_FIELD:STM_TIMING_EVENT_FRAME;
    else
      event = STM_TIMING_EVENT_NONE;

    goto unlock_and_exit;
  }

  /*
   * Defensive coding against catching an interrupt unexpectedly when the
   * VTG is configured to not be disabled when "stopped".
   */
  if(!IsStarted() && !IsPending() )
    goto unlock_and_exit;


  if(intStatus & VTG_ITS_VST)
  {
    if(IsPending() && m_bUpdateOnlyClockFrequency)
    {
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::GetInterruptStatus(this=%p): Updating pending clock frequency = %uHz", this, m_PendingMode.mode_timing.pixel_clock_freq );
      m_pFSynth->Start(m_PendingMode.mode_timing.pixel_clock_freq);
      m_bUpdateOnlyClockFrequency = false;
      m_CurrentMode = m_PendingMode;
      m_PendingMode.mode_id = STM_TIMING_MODE_RESERVED;
    }

    if(IsStarted())
    {
      event |= STM_TIMING_EVENT_FRAME;
      if(m_CurrentMode.mode_params.scan_type == STM_INTERLACED_SCAN)
        event |= STM_TIMING_EVENT_TOP_FIELD;
    }
  }
  else if(intStatus & VTG_ITS_VSB)
  {
    if(IsStarted() && m_UpdateOutputRect)
    {
      ProgramOutputWindow(&m_CurrentMode);
      if(m_pSlavedVTG)
        m_pSlavedVTG->ProgramOutputWindow(&m_CurrentMode);
    }

    if(IsStarted())
    {
      if(m_pSlavedVTG && m_pSlavedVTG->m_pendingSync)
        {
          ((CSTmC8VTG_V8_4*)m_pSlavedVTG)->ProgramSyncOutputs(&m_CurrentMode);
          m_pSlavedVTG->m_pendingSync = 0;
        }
      if(m_pendingSync)
        ProgramSyncOutputs(&m_CurrentMode);
    }

    if(IsPending() && !m_bUpdateOnlyClockFrequency)
    {
      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::GetInterruptStatus(this=%p), Doing Mode Change", this );

      m_pFSynth->Start(m_PendingMode.mode_timing.pixel_clock_freq);

      ProgramOutputWindow(&m_PendingMode);
      if(m_pSlavedVTG)
        m_pSlavedVTG->ProgramOutputWindow(&m_PendingMode);

      if(m_pSlavedVTG)
        m_pSlavedVTG->ProgramVTGTimings(&m_PendingMode);

      ProgramVTGTimings(&m_PendingMode); // Note: this sets m_CurrentMode at the end

      if(ReadHWInterruptStatus() != 0)
        TRC( TRC_ID_ERROR, "CSTmC8VTG_V8_4::GetInterruptStatus(this=%p): Possibly failed to reprogram before vsync", this );

      m_bDoSoftResetInInterrupt   = true; // Force a reset to switch double buffered registers
    }

    if(m_bDoSoftResetInInterrupt)
    {
      m_mismatched_sync_dbg_count = 5; // Reset sync mismatch debug before reset.

      WriteVTGReg(VTGn_DRST, 1);
      m_bDoSoftResetInInterrupt = false;

      TRC( TRC_ID_UNCLASSIFIED, "CSTmC8VTG_V8_4::GetInterruptStatus(this=%p): SoftReset", this );
      /*
       * We have just issued a reset which has immediately raised a new top
       * field/frame, so there is no point in returning the original cause of
       * this interrupt. We are going to be taking a new interrupt as soon as
       * we return.
       */
      event = STM_TIMING_EVENT_NONE;
      goto unlock_and_exit;
    }

    if(IsStarted())
    {
      event |= (m_CurrentMode.mode_params.scan_type == STM_INTERLACED_SCAN)?STM_TIMING_EVENT_BOTTOM_FIELD:STM_TIMING_EVENT_FRAME;
    }
  }

  if(intStatus & (VTG_ITS_LNB | VTG_ITS_LNT))
    event |= STM_TIMING_EVENT_LINE;

unlock_and_exit:
  vibe_os_unlock_resource(m_lock);
  return event;
}
