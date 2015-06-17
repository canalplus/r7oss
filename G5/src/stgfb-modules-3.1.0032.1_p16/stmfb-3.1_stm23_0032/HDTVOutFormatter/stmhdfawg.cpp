/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfawg.cpp
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IDebug.h>
#include <Generic/DisplayDevice.h>

#include "stmhdfawg.h"


#define AWG_CTRL_REG         0x0000
#define AWG_CTRL2_REG        0x0010

#define AWG_RAM_REG          0x0300

#define AWG_CTRL_PREFILTER_EN           (1L<<10)
#define AWG_CTRL_SYNC_ON_PRPB           (1L<<11)
#define AWG_CTRL_SEL_MAIN_TO_DENC       (1L<<12)
#define AWG_CTRL_PRPB_SYNC_OFFSET_SHIFT (16)
#define AWG_CTRL_PRPB_SYNC_OFFSET_MASK  (0x7ff<<AWG_CTRL_PRPB_SYNC_OFFSET_SHIFT)
#define AWG_CTRL_SYNC_EN                (1L<<27)
#define AWG_CTRL_SYNC_HREF_IGNORED      (1L<<28)
#define AWG_CTRL_SYNC_FIELD_N_FRAME     (1L<<29)
#define AWG_CTRL_SYNC_FILTER_DEL_SD     (0)
#define AWG_CTRL_SYNC_FILTER_DEL_ED     (1L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_HD     (2L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_BYPASS (3L<<30)
#define AWG_CTRL_SYNC_FILTER_DEL_MASK   (3L<<30)


CSTmHDFormatterAWG::CSTmHDFormatterAWG (CDisplayDevice *pDev,
                                        ULONG           hdf_offset,
                                        ULONG           ram_size): CAWG (pDev,
                                                                         (hdf_offset + AWG_RAM_REG),
                                                                         ram_size)
{
  DEBUGF2 (2, ("%s\n", __PRETTY_FUNCTION__));
  m_pHDFAWG = (ULONG*)pDev->GetCtrlRegisterBase() + (hdf_offset>>2);

  WriteReg(AWG_CTRL2_REG,0);
}


CSTmHDFormatterAWG::~CSTmHDFormatterAWG ()
{
  DEBUGF2 (2, ("%s\n", __PRETTY_FUNCTION__));
}


bool CSTmHDFormatterAWG::Enable (stm_awg_mode_t mode)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  /* Set up sync behavior */
  ULONG awg = ReadReg (AWG_CTRL_REG) & ~(AWG_CTRL_SYNC_FILTER_DEL_MASK
                                         | AWG_CTRL_SYNC_FIELD_N_FRAME
                                         | AWG_CTRL_SYNC_HREF_IGNORED);

  if (mode & STM_AWG_MODE_FILTER_HD)
    awg |= AWG_CTRL_SYNC_FILTER_DEL_HD;
  else if (mode & STM_AWG_MODE_FILTER_ED)
    awg |= AWG_CTRL_SYNC_FILTER_DEL_ED;
  else if (mode & STM_AWG_MODE_FILTER_SD)
    awg |= AWG_CTRL_SYNC_FILTER_DEL_SD;
  else
    awg |= AWG_CTRL_SYNC_FILTER_DEL_BYPASS;

  if (mode & STM_AWG_MODE_FIELDFRAME)
    awg |= AWG_CTRL_SYNC_FIELD_N_FRAME;

  if (mode & STM_AWG_MODE_HSYNC_IGNORE)
    awg |= AWG_CTRL_SYNC_HREF_IGNORED;

  if(!m_bIsDisabled)
    awg |= AWG_CTRL_SYNC_EN;

  DEBUGF2 (2, ("%s: AWG_CTRL = 0x%lx\n", __PRETTY_FUNCTION__,awg));

  WriteReg (AWG_CTRL_REG, awg);

  return true;
}


void CSTmHDFormatterAWG::Enable (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_bIsDisabled = false;

  if(m_bIsFWLoaded)
  {
    /* enable AWG, using previous state */
    ULONG awg = ReadReg (AWG_CTRL_REG) | AWG_CTRL_SYNC_EN;
    DEBUGF2 (2, ("%s: AWG_CTRL_REG: %.8lx\n", __PRETTY_FUNCTION__, awg));
    WriteReg (AWG_CTRL_REG, awg);
  }
}


void CSTmHDFormatterAWG::Disable (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_bIsDisabled = true;

  /* disable AWG, but leave state */
  ULONG awg = ReadReg (AWG_CTRL_REG) & ~(AWG_CTRL_SYNC_EN);
  DEBUGF2 (2, ("%s: AWG_CTRL_REG: %.8lx\n",__PRETTY_FUNCTION__, awg));
  WriteReg (AWG_CTRL_REG, awg);
}
