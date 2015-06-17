/***********************************************************************
 *
 * File: display/ip/tvout/stmc8fdvo_v2_1.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_C8FDVO_V2_1_H
#define _STM_C8FDVO_V2_1_H

#include <display/ip/stmfdvo.h>
#include <display/ip/tvout/stmvipdvo.h>


class CSTmC8FDVO_V2_1: public CSTmFDVO
{
public:
  CSTmC8FDVO_V2_1(const char     *name,
                  uint32_t        id,
                  CDisplayDevice *pDev,
                  CSTmVIP        *pDVOVIP,
                  uint32_t        uFDVOOffset,
                  COutput        *pMasterOutput);

  virtual ~CSTmC8FDVO_V2_1();

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  OutputResults Start(const stm_display_mode_t* pModeLine);

protected:
  bool SetOutputFormat(uint32_t format, const stm_display_mode_t* pModeLine);
  virtual bool SetVTGSyncs(const stm_display_mode_t* pModeLine)=0;
private:
  CSTmVIP             *m_pVIP;
  CSTmC8FDVO_V2_1(const CSTmC8FDVO_V2_1&);
  CSTmC8FDVO_V2_1& operator=(const CSTmC8FDVO_V2_1&);
};


#endif /* _STM_C8FDVO_V2_1_H */
