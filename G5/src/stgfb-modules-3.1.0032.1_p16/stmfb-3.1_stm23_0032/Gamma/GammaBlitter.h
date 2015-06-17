/***********************************************************************
 * 
 * File: Gamma/GammaBlitter.h
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_BLITTER_H
#define _GAMMA_BLITTER_H

#include <STMCommon/stmblitter.h>

struct GAMMA_BLIT_NODE
{
  ULONG BLT_NIP;  /* Next Instruction Pointer */
  ULONG BLT_USR;  /* User specific  */
  ULONG BLT_INS;  /* Instruction  */
  ULONG BLT_S1BA; /* Source1 Base Address */

  ULONG BLT_S1TY; /* Source1 Type */
  ULONG BLT_S1XY; /* Source1 XY Location */
  ULONG BLT_S1SZ; /* Source1 Size */
  ULONG BLT_S1CF; /* Source1 Color Fill */

  ULONG BLT_S2BA; /* Source2 Base Address */
  ULONG BLT_S2TY; /* Source2 Type */
  ULONG BLT_S2XY; /* Source2 XY Location */
  ULONG BLT_S2SZ; /* Source2 Size */

  ULONG BLT_S2CF; /* Source2 Color Fill */
  ULONG BLT_TBA;  /* Target Base Address */
  ULONG BLT_TTY;  /* Target Type */
  ULONG BLT_TXY;  /* Target XY Location */

  ULONG BLT_CWO;  /* Clipping Window Offset */
  ULONG BLT_CWS;  /* Clipping Window Stop */
  ULONG BLT_CCO;  /* Color Conversion Operators */
  ULONG BLT_CML;  /* CLUT Memory Location */

  ULONG BLT_RZC;  /* 2D Resize Control */
  ULONG BLT_HFP;  /* H Filter Coefficients Pointer */
  ULONG BLT_VFP;  /* V Filter Coefficients Pointer */
  ULONG BLT_RSF;  /* Resize Scaling Factor */

  ULONG BLT_FF0;  /* Flicker Filter 0 */
  ULONG BLT_FF1;  /* Flicker Filter 1 */
  ULONG BLT_FF2;  /* Flicker Filter 2 */
  ULONG BLT_FF3;  /* Flicker Filter 3 */

  ULONG BLT_ACK;  /* ALU and Color Key Control */
  ULONG BLT_KEY1; /* Color Key 1 */
  ULONG BLT_KEY2; /* Color Key 2 */
  ULONG BLT_PMK;  /* Plane Mask */

  ULONG BLT_RST;  /* Raster Scan Trigger */
  ULONG BLT_XYL;  /* XYL */
  ULONG BLT_XYP;  /* XY Pointer */
  ULONG BLT_JUNK; /* Dummy reg to make node a multiple of 128 bits*/
};

class CDisplayDevice;

class CGammaBlitter : public CSTmBlitter
{
public:
  CGammaBlitter(ULONG *pBlitterBaseAddr);
  ~CGammaBlitter();

  bool Create(void);
  
  bool IsEngineBusy(void);
  int  SyncChip(bool WaitNextOnly);
  
  bool HandleBlitterInterrupt(void);

  STMFBBDispSharedAreaPriv *GetSharedArea (void) { return 0; };

  ULONG GetBlitLoad (void) { return 0; };

  // Drawing functions
  bool FillRect       (const stm_blitter_operation_t&, const stm_rect_t&);
  bool DrawRect       (const stm_blitter_operation_t&, const stm_rect_t&);
  bool CopyRect       (const stm_blitter_operation_t&, const stm_rect_t&, const stm_point_t&);
  bool CopyRectComplex(const stm_blitter_operation_t&, const stm_rect_t&, const stm_rect_t&);
  
private:
  void StartBlitter(unsigned long);
  void ResetBlitter(void);

  void BlendOperation   (GAMMA_BLIT_NODE* pBlitNode, const stm_blitter_operation_t &pOP);
  bool ColourKey        (GAMMA_BLIT_NODE* pBlitNode, const stm_blitter_operation_t &pOP);
  void YUVRGBConversions(GAMMA_BLIT_NODE* pBlitNode);

  void SetPlaneMask(const stm_blitter_operation_t &op, ULONG &ins, ULONG &pmk) const
  {
    if(op.ulFlags & STM_BLITTER_FLAGS_PLANE_MASK)
    {
      ins |= BLIT_INS_ENABLE_PLANEMASK;
      pmk  = op.ulPlanemask;
    }
  }

  void QueueBlitOperation(GAMMA_BLIT_NODE &blitNode);  
};

#endif // _GAMMA_BLITTER_H
