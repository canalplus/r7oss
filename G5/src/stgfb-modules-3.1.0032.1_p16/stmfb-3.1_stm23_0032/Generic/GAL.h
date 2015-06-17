/***********************************************************************
 * 
 * File: Generic/GAL.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GAL_H
#define GAL_H

#include "stmdisplay.h"

class CGAL
{
public:
  CGAL(void) {}
  virtual ~CGAL() {}

  // Chip status information
  virtual bool IsEngineBusy(void) = 0;
  // Wait until chip appears idle
  virtual int  SyncChip(bool WaitNextOnly) = 0;
      
  virtual bool HandleBlitterInterrupt(void) = 0;

  virtual STMFBBDispSharedAreaPriv *GetSharedArea (void) = 0;

  virtual ULONG GetBlitLoad (void) = 0;

  ////////////////////////////////////////////////////////////
  // Drawing interfaces
  virtual bool FillRect          (const  stm_blitter_operation_t&, const stm_rect_t&) = 0;
  virtual bool DrawRect          (const  stm_blitter_operation_t&, const stm_rect_t&) = 0;
  virtual bool CopyRect          (const  stm_blitter_operation_t&, const stm_rect_t&, const stm_point_t&) = 0;
  virtual bool CopyRectComplex   (const  stm_blitter_operation_t&, const stm_rect_t&, const stm_rect_t&)  = 0;
};

#endif
