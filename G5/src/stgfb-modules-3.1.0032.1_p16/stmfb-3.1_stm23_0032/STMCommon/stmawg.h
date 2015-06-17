/***********************************************************************
 *
 * File: STMCommon/stmawg.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMAWG_H
#define _STMAWG_H

#include <stmdisplayoutput.h>

#include <Generic/IOS.h>

/*
 * The AWG introduces a delay of some pixels:
 *   sync to external syncs (VTG): 3 clocks
 *   instruction fetch+execute   : 2 clocks
 *
 * However, the specification for the block appears to contradict itself and
 * also states that the delay from program start to the first pixel entering
 * the filter block is only 4 clocks. This value seems to work better when it
 * comes to getting all the pixels on the screen with a HD TV in exact scan
 * mode, for 1080i and 720p.
 *
 *   filter delay                : 5   clocks for HD
 *                                 4.5 clocks for ED
 *                                 3.5 clocks for SD
 *                                 2   clocks if filter disabled
 *
 */
#define AWG_DELAY                 (4)
#define AWG_FILTER_DELAY_HD       (5)
#define AWG_FILTER_DELAY_ED       (4)
#define AWG_FILTER_DELAY_SD       (3)
#define AWG_FILTER_DELAY_DISABLED (2)

#define AWG_DELAY_HD        (-(AWG_DELAY+AWG_FILTER_DELAY_HD))
#define AWG_DELAY_ED        (-(AWG_DELAY+AWG_FILTER_DELAY_ED))
#define AWG_DELAY_SD        (-(AWG_DELAY+AWG_FILTER_DELAY_SD))
#define AWG_DELAY_NO_FILTER (-(AWG_DELAY+AWG_FILTER_DELAY_DISABLED))


class CDisplayDevice;
class COutput;


typedef enum
{
  STM_AWG_MODE_HSYNC_IGNORE     = (1 << 0),
  STM_AWG_MODE_FIELDFRAME       = (1 << 1),

  STM_AWG_MODE_FILTER_HD        = (1 << 2),
  STM_AWG_MODE_FILTER_ED        = (1 << 3),
  STM_AWG_MODE_FILTER_SD        = (1 << 4)
} stm_awg_mode_t;


class CAWG
{
public:
  CAWG (CDisplayDevice* pDev, ULONG ram_offset, ULONG ram_size, const char *prefix = "component");
  virtual ~CAWG (void);

  void CacheFirmwares (COutput *output);

  bool Start (const stm_mode_line_t * const pMode);
  bool Stop  (void);

  virtual void Disable (void)     = 0;
  virtual void Enable  (void)     = 0;

  virtual void Suspend (void);
  virtual void Resume  (void);

protected:
  // IO mapped pointer to the start of the Gamma register block
  ULONG* m_pAWGRam;
  bool   m_bIsSuspended;
  bool   m_bIsFWLoaded;
  bool   m_bIsDisabled;

  const  STMFirmware *m_FWbundle;

  UCHAR       m_maxTimingSize;
  const char *m_AWGPrefix;

  virtual bool Enable  (stm_awg_mode_t mode) = 0;

  bool FindFWStartInBundle (const stm_mode_line_t  * const pMode,
                            const UCHAR           **start,
                            ULONG                  *size);

  bool UploadTiming (const UCHAR * const data,
                     ULONG               size);

  void  WriteRam (ULONG reg,
                  ULONG value)  { DEBUGF2 (4, ("w %p <- %.8lx\n",m_pAWGRam + (reg>>2),value));
                                  g_pIOS->WriteRegister (m_pAWGRam + (reg>>2), value); }

  ULONG ReadRam  (ULONG reg)    { DEBUGF2 (4, ("r %p -> %.8lx\n", m_pAWGRam + (reg>>2),
                                               g_pIOS->ReadRegister (m_pAWGRam + (reg>>2))));
                                  return g_pIOS->ReadRegister (m_pAWGRam + (reg>>2)); }

};

#endif /* _STMAWG_H */
