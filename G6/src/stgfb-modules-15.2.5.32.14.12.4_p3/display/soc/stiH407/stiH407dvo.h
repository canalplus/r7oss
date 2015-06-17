/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407dvo.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407DVO_H
#define _STIH407DVO_H

#include <display/ip/tvout/stmc8fdvo_v2_1.h>

#define TVOUT_DVO_EMB_DELAY_HD       (0)
#define TVOUT_DVO_EMB_DELAY_ED       (1)
#define TVOUT_DVO_EMB_DELAY_SD       (2)

#define TVOUT_DVO_EXT_DELAY_HD       (6)
#define TVOUT_DVO_EXT_DELAY_ED       (5)
#define TVOUT_DVO_EXT_DELAY_SD       (4)


class CSTmTVOutClkDivider;
class CSTmVTG;


class CSTiH407DVO: public CSTmC8FDVO_V2_1
{
public:
  CSTiH407DVO(CDisplayDevice              *pDev,
              COutput                     *pMasterOutput,
              CSTmVTG                     *pVTG,
              CSTmClockLLA                *pClkDivider,
              CSTmVIP                     *pDVOVIP,
              const stm_display_sync_id_t *pSyncMap,
              uint32_t                     uDVOBase);

  bool Create(void);

private:
  CSTmClockLLA *m_pClkDivider;
  CSTmVTG      *m_pVTG;

  const stm_display_sync_id_t *m_pTVOSyncMap;

  bool SetOutputFormat(uint32_t format, const stm_display_mode_t* pModeLine);
  void DisableClocks(void);

  virtual bool SetVTGSyncs(const stm_display_mode_t* pModeLine);

  CSTiH407DVO(const CSTiH407DVO&);
  CSTiH407DVO& operator=(const CSTiH407DVO&);
};


#endif /* _STIH407DVO_H */
