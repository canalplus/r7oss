/***********************************************************************
 *
 * File: STMCommon/stmawg.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>
#include <Generic/DisplayDevice.h>
#include <Generic/Output.h>

#include <Generic/IDebug.h>
#include "stmawg.h"


#define FIRMWARE_BUNDLE_VERSION   1

#define FIRMWARE_NAME_MAX 30  // same as in linux/firmware.h



CAWG::CAWG (CDisplayDevice *pDev,ULONG ram_offset, ULONG ram_size, const char *prefix)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_pAWGRam = (ULONG*)pDev->GetCtrlRegisterBase()+(ram_offset>>2);

  m_bIsSuspended = false;
  m_bIsFWLoaded  = false;
  m_bIsDisabled  = false;

  m_maxTimingSize = ram_size;
  m_AWGPrefix     = prefix;

  m_FWbundle = 0;
}


CAWG::~CAWG (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  if (LIKELY (m_FWbundle))
    g_pIOS->ReleaseFirmware (m_FWbundle);
}


bool CAWG::FindFWStartInBundle (const stm_mode_line_t  * const pMode,
                                const UCHAR           **start,
                                ULONG                  *size)
{
  /* find start and size of firmware data in blob for a particular mode */
  bool         success = false;
  UCHAR        n_entries;
  const UCHAR *p;
  ULONG        s;

  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  ASSERTF ((m_FWbundle != 0), ("no firmware?\n"));
  ASSERTF ((m_FWbundle->pData != 0), ("no firmware2?\n"));
  ASSERTF ((m_FWbundle->ulDataSize != 0), ("no firmware3?\n"));

  const char *suffix = 0;
  if (UNLIKELY (pMode->ModeParams.OutputStandards & (STM_OUTPUT_STD_SMPTE240M
                                                     | STM_OUTPUT_STD_SMPTE295M)))
    suffix = "o";
  else if (UNLIKELY (pMode->ModeParams.OutputStandards & STM_OUTPUT_STD_AS4933))
    {
      switch (pMode->Mode)
        {
        case STVTG_TIMING_MODE_1080I50000_72000:
          suffix = "iLH";
          break;

        case STVTG_TIMING_MODE_1152I50000_48000:
          suffix = "iSH";
          break;

        default:
          suffix = "??";
          break;
        }
    }

  DEBUGF2 (2, ("%s finding firmware for '%lu%c%lu@%lu flag: %s'\n",
               __PRETTY_FUNCTION__,
               pMode->ModeParams.ActiveAreaHeight,
               "xpi"[pMode->ModeParams.ScanType],
               pMode->ModeParams.FrameRate,
               pMode->TimingParams.ulPixelClock / 1000,
               suffix ? suffix : "(none)"));

  n_entries =  m_FWbundle->pData[1];
  p         = &m_FWbundle->pData[2];
  for (int i = 0;
       i < n_entries;
       ++i, p += s + 14)
    {
      s = (p[12] << 8 | p[13]) * 4;

      if ((pMode->ModeParams.ScanType == SCAN_I && !p[0])
          || (pMode->ModeParams.ScanType == SCAN_P && p[0]))
        {
          /* scan type doesn't match */
          DEBUGF2 (9, ("scan type doesn't match @ %d\n", i));
          continue;
        }

      if ((suffix && !p[1])
          || (!suffix && p[1]))
        {
          /* flags don't match */
          DEBUGF2 (9, ("flags don't match @ %x\n", i));
          continue;
        }

      if (pMode->ModeParams.ActiveAreaHeight != (ULONG) (p[2] << 8 | p[3]))
        {
          /* active area doesn't match */
          DEBUGF2 (9, ("active area doesn't match @ %d\n", i));
          continue;
        }

      if (pMode->ModeParams.FrameRate != (ULONG) (p[4] << 24
                                                  | p[5] << 16
                                                  | p[6] <<  8
                                                  | p[7] <<  0))
        {
          /* frame rate doesn't match */
          DEBUGF2 (9, ("frame rate doesn't match @ %d\n", i));
          continue;
        }

      if ((pMode->TimingParams.ulPixelClock / 1000) != (ULONG) (p[8] << 24
                                                                | p[ 9] << 16
                                                                | p[10] <<  8
                                                                | p[11] <<  0))
        {
          /* pixel clock doesn't match */
          DEBUGF2 (9, ("pixel clock doesn't match @ %d\n", i));
          continue;
        }

      DEBUGF2 (3, ("%s: found a match @ %d, size %lu\n",
                   __PRETTY_FUNCTION__, i, s));

      *start = &p[14];
      *size  = s;

      success = true;
      break;
    }

  return success;
}


void CAWG::CacheFirmwares (COutput *output)
{
  char name[FIRMWARE_NAME_MAX];

  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  ASSERTF (!m_FWbundle, ("%s called twice?!\n", __PRETTY_FUNCTION__));

  if (g_pIOS->snprintf (name, sizeof (name),
                        "%s.fw", m_AWGPrefix) < 0)
    return;

  DEBUGF2 (2, ("%s: caching '%s'\n", __PRETTY_FUNCTION__, name));

  if (LIKELY (!g_pIOS->RequestFirmware (&m_FWbundle, name)))
    {
      const UCHAR  *p;
      ULONG         total_size;

      /* do some basic error checks of the firmware data here */
      ASSERTF (m_FWbundle, ("m_FWBundle == NULL?!\n"));
      ASSERTF (m_FWbundle->pData, ("m_FWbundle->pData == NULL?!\n"));
      ASSERTF (m_FWbundle->ulDataSize > 2, ("m_FWBundle->ulDataSize <= 2?!\n"));

      if (UNLIKELY (m_FWbundle->pData[0] != FIRMWARE_BUNDLE_VERSION))
        {
          DEBUGF (("%s: wrong AWG FW bundle version %d, want %d\n",
                   __PRETTY_FUNCTION__,
                   m_FWbundle->pData[0], FIRMWARE_BUNDLE_VERSION));
          goto error;
        }

      if (UNLIKELY (!m_FWbundle->pData[1]))
        {
          DEBUGF (("%s: empty AWG FW bundle?!\n", __PRETTY_FUNCTION__));
          /* an empty file is useless for us... */
          goto error;
        }

      total_size = 2;

      p = &m_FWbundle->pData[2];
      for (int i = 0; i < m_FWbundle->pData[1]; i++)
        {
          USHORT size = (p[12] << 8 | p[13]);

          if (UNLIKELY (size > m_maxTimingSize))
            {
              DEBUGF (("%s: AWG FW bundle corrupted (1) (%hu > %d)!\n",
                       __PRETTY_FUNCTION__, size, m_maxTimingSize));
              goto error;
            }

          size *= 4;
          if (UNLIKELY (m_FWbundle->ulDataSize < total_size + size + 14))
            {
              DEBUGF (("%s: AWG FW bundle corrupted (2)!\n", __PRETTY_FUNCTION__));
              goto error;
            }

          DEBUGF2 (3, ("%s: at %x: %lu%c%lu@%lu flag: %c\n",
                       __PRETTY_FUNCTION__,
                       p - m_FWbundle->pData,
                       (ULONG) (p[2] << 8 | p[3]),
                       p[0] ? 'i' : 'p',
                       (ULONG) (p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7] << 0),
                       (ULONG) (p[8] << 24 | p[9] << 16 | p[10] << 8 | p[11] << 0),
                       p[1] ? 'y' : 'n'));


          total_size += size + 14;
          p += size + 14;
        }

      if (UNLIKELY (m_FWbundle->ulDataSize != total_size))
        {
          DEBUGF (("%s: AWG FW bundle corrupted (3)!\n", __PRETTY_FUNCTION__));
          goto error;
        }

#ifdef DEBUG
      int n_timings_found = 0;
      for (int mode = 0; mode < STVTG_TIMING_MODE_COUNT; ++mode)
        {
          const UCHAR *start;
          ULONG        size;

          const stm_mode_line_t * const pMode =
            output->GetModeParamsLine ((stm_display_mode_t) mode);

          if (LIKELY (pMode))
            if (FindFWStartInBundle (pMode, &start, &size))
              ++n_timings_found;
        }
      DEBUGF2 (2, ("%s: found timings for %d modes\n",
                   __PRETTY_FUNCTION__, n_timings_found));
#endif
    }

  return;

error:
  g_pIOS->ReleaseFirmware (m_FWbundle);
  m_FWbundle = 0;
}


bool CAWG::Start (const stm_mode_line_t * const pMode)
{
  const UCHAR *start;
  ULONG        size;

  DEBUGF2 (3, ("%s for %d\n", __PRETTY_FUNCTION__, pMode->Mode));

  /* if we have a firmware for that mode then use it */

  if (UNLIKELY (m_bIsFWLoaded))
    {
      /* somebody should have called Stop() so as to avoid a HW bug on
         STi7200 cut 1 from surfacing. The sequence is
         Stop AWG
         wait for VSync
         write AWG RAM
         enable AWG */
      /* disable AWG as desperate measure */
      DEBUGF (("%s: somebody should have called STOP() earlier, AWG disabled!\n",
               __PRETTY_FUNCTION__));
      Stop ();
      return false;
    }

  if (LIKELY (m_FWbundle
              && FindFWStartInBundle (pMode, &start, &size)
              && UploadTiming (start, size)))
    {
      /* enable AWG again */
      int mode = 0; /* stm_awg_mode_t */

      if (pMode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK)
        mode |= STM_AWG_MODE_FILTER_HD;
      else if (pMode->ModeParams.OutputStandards & STM_OUTPUT_STD_ED_MASK)
        mode |= STM_AWG_MODE_FILTER_ED;
      else if (pMode->ModeParams.OutputStandards & STM_OUTPUT_STD_SD_MASK)
        mode |= STM_AWG_MODE_FILTER_SD;
      // else bypass

      switch (pMode->Mode)
        {
        case STVTG_TIMING_MODE_1080I60000_74250:
        case STVTG_TIMING_MODE_1080I59940_74176:
        case STVTG_TIMING_MODE_1080I50000_74250_274M:
          // might be needed, but not at the moment... need some quality tests.
          //mode |= STM_AWG_MODE_HSYNC_IGNORE;
          break;

        default:
          // nothing, but prevent compiler warning
          break;
        }

      if (pMode->ModeParams.ScanType == SCAN_P)
        // when using field based vertical sync in the AWG cell, the
        // instruction address generator is reset every field, that
        // is upon every pulse of Vsync_redge.
        // for frame based vertical sync, the address generator is
        // reset at every frame, that is upon pulses of Vsync_redge
        // which occur when BnotTsync_awg is low.
        // For progressive modes we want the address generator to be
        // reset on every Vsync, for interlaced modes on every second
        // Vsync.
        mode |= STM_AWG_MODE_FIELDFRAME;

      /*
       * Program mode, but only really enable the AWG if not in a
       * disabled state.
       */
      Enable (static_cast<stm_awg_mode_t> (mode));
      m_bIsFWLoaded = true;
    }
  else
    {
      /* have no timings for that mode */
      DEBUGF2 (1, ("%s missing firmware for %lu%c%lu@%lu flags: %c'\n",
                   __PRETTY_FUNCTION__,
                   pMode->ModeParams.ActiveAreaHeight,
                   "xpi"[pMode->ModeParams.ScanType],
                   pMode->ModeParams.FrameRate,
                   pMode->TimingParams.ulPixelClock / 1000,
                   '?'));
    }

  return m_bIsFWLoaded;
}


bool CAWG::Stop (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  Disable ();
  m_bIsFWLoaded = false;

  return true;
}


void CAWG::Suspend (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  Disable();
  m_bIsSuspended = true;
}


void CAWG::Resume (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_bIsSuspended = false;
  Enable();
}


bool CAWG::UploadTiming (const UCHAR * const data,
                         ULONG               size)
{
  DEBUGF2 (3, ("%s: uploading AWG timing (%lu bytes)\n",
               __PRETTY_FUNCTION__, size / 4));

  if (UNLIKELY (size/4 > m_maxTimingSize))
    {
      DEBUGF (("%s: size (%lu) > m_maxTimingSize (%d)\n",
               __PRETTY_FUNCTION__, size/4, m_maxTimingSize));
      return false;
    }

  /* write AWG firmware */
  for (USHORT i = 0; i < size; i+=4)
    {
      ULONG reg = data[i+0]   << 24
                  | data[i+1] << 16
                  | data[i+2] <<  8
                  | data[i+3] <<  0;
      WriteRam (i, reg);
    }

#ifdef DEBUG
  // dump AWG firmware
  for (UCHAR i = 0; i < m_maxTimingSize; ++i)
    {
      ULONG reg = ReadRam (i*4);
      DEBUGF2 (3, ("read RAM(%.2d): %.8lx\n", i, reg));
    }
#endif

  return true;
}
