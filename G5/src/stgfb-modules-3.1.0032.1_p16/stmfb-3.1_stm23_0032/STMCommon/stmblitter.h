/***********************************************************************
 * 
 * File: STMCommon/stmblitter.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_BLITTER_H
#define _STM_BLITTER_H

#include <Generic/GAL.h>

/*
 * Common blitter register defines
 * 
 * Blit instruction control
 */
#define BLIT_INS_SRC1_MODE_DISABLED           0x0
#define BLIT_INS_SRC1_MODE_MEMORY             0x1
#define BLIT_INS_SRC1_MODE_COLOR_FILL         0x3
#define BLIT_INS_SRC1_MODE_DIRECT_COPY        0x4
#define BLIT_INS_SRC1_MODE_DIRECT_FILL        0x7
#define BLIT_INS_SRC1_MODE_MASK               0x7

#define BLIT_INS_SRC2_MODE_DISABLED           0x0
#define BLIT_INS_SRC2_MODE_MEMORY             (0x1<<3)
#define BLIT_INS_SRC2_MODE_COLOR_FILL         (0x3<<3)
#define BLIT_INS_SRC2_MODE_MASK               (0x3<<3)

#define BLIT_INS_SRC3_MODE_DISABLED           0x0
#define BLIT_INS_SRC3_MODE_MEMORY             (0x1<<5)

#define BLIT_INS_ENABLE_ICSC                  (1L<<6)
#define BLIT_INS_ENABLE_CLUT                  (1L<<7)
#define BLIT_INS_ENABLE_2DRESCALE             (1L<<8)
#define BLIT_INS_ENABLE_FLICKERFILTER         (1L<<9)
#define BLIT_INS_ENABLE_RECTCLIP              (1L<<10)
#define BLIT_INS_ENABLE_COLOURKEY             (1L<<11)
#define BLIT_INS_ENABLE_OCSC                  (1L<<12)
#define BLIT_INS_ENABLE_PLANEMASK             (1L<<14)
#define BLIT_INS_ENABLE_XYL                   (1L<<15)

/*
 * ALU and Colour key control
 */
#define BLIT_ACK_BYPASSSOURCE1                0x0
#define BLIT_ACK_ROP                          0x1
#define BLIT_ACK_BLEND_SRC2_N_PREMULT         0x2
#define BLIT_ACK_BLEND_SRC2_PREMULT           0x3
#define BLIT_ACK_BYPASSSOURCE2                0x7
#define BLIT_ACK_MODE_MASK                    0xF

#define BLIT_ACK_ROP_COPY                     3
#define BLIT_ACK_ROP_NOOP                     5
#define BLIT_ACK_ROP_XOR                      6
#define BLIT_ACK_ROP_SHIFT                    8
#define BLIT_ACK_ROP_MASK                     (0xFF << BLIT_ACK_ROP_SHIFT)

#define BLIT_ACK_GLOBAL_ALPHA_SHIFT           8
#define BLIT_ACK_GLOBAL_ALPHA_MASK            (0xFF << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
 
#define BLIT_ACK_BLUE_ENABLE                  (0x01 << 16)
#define BLIT_ACK_INV_BLUE_ENABLE              (0x03 << 16)
#define BLIT_ACK_RGB_ENABLE                   (0x15 << 16)
#define BLIT_ACK_INV_RGB_ENABLE               (0x3F << 16)

#define BLIT_ACK_DEST_COLOUR_KEYING           0x0
#define BLIT_ACK_SRC_COLOUR_KEYING            (1L << 22)
#define BLIT_ACK_DEST_KEY_ZEROS_SRC_ALPHA     (3L << 22)

/*
 * Source/Dest type
 */
#define BLIT_TY_PIXMAP_PITCH_MASK             (0xFFFF)

#define BLIT_TY_COLOR_FORM_IS_RGB(ty)         (((ty) & (0x18 << 16)) == (0x00 << 16))
#  define BLIT_COLOR_FORM_RGB565                (0x00 << 16)
#  define BLIT_COLOR_FORM_RGB888                (0x01 << 16)
#  define BLIT_COLOR_FORM_ARGB8565              (0x04 << 16)
#  define BLIT_COLOR_FORM_ARGB8888              (0x05 << 16)
#  define BLIT_COLOR_FORM_ARGB1555              (0x06 << 16)
#  define BLIT_COLOR_FORM_ARGB4444              (0x07 << 16)
#define BLIT_TY_COLOR_FORM_IS_CLUT(ty)        ((((ty) & (0x18 << 16)) == (0x08 << 16)) && (((ty) & BLIT_TY_COLOR_FORM_MASK) != BLIT_COLOR_FORM_YCBCR42XMBN))
#  define BLIT_COLOR_FORM_CLUT1                 (0x08 << 16)
#  define BLIT_COLOR_FORM_CLUT2                 (0x09 << 16)
#  define BLIT_COLOR_FORM_CLUT4                 (0x0a << 16)
#  define BLIT_COLOR_FORM_CLUT8                 (0x0b << 16)
#  define BLIT_COLOR_FORM_ACLUT44               (0x0c << 16)
#  define BLIT_COLOR_FORM_ACLUT88               (0x0d << 16)
#define BLIT_TY_COLOR_FORM_IS_YUV(ty)         ((((ty) & (0x18 << 16)) == (0x10 << 16)) || (((ty) & (0x0f << 16)) == (0x0e << 16)))
#define BLIT_TY_COLOR_FORM_IS_YUVr(ty)        ((((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR888) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR422R) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_AYCBCR8888))
#  define BLIT_COLOR_FORM_YCBCR888              (0x10 << 16)
#  define BLIT_COLOR_FORM_YCBCR422R             (0x12 << 16)
#  define BLIT_COLOR_FORM_YCBCR42XMB            (0x14 << 16)
#  define BLIT_COLOR_FORM_AYCBCR8888            (0x15 << 16)
#  define BLIT_COLOR_FORM_YCBCR42XR2B           (0x16 << 16)
#  define BLIT_COLOR_FORM_YCBCR42XMBN           (0x0e << 16)
#  define BLIT_COLOR_FORM_YUV444P               (0x1e << 16)
#define BLIT_TY_COLOR_FORM_IS_MISC(ty)        (((ty) & (0x18 << 16)) == (0x18 << 16))
#define BLIT_TY_COLOR_FORM_IS_ALPHA(ty)       (((ty) & (0x1e << 16)) == (0x18 << 16))
#  define BLIT_COLOR_FORM_A1                    (0x18 << 16)
#  define BLIT_COLOR_FORM_A8                    (0x19 << 16)
#  define BLIT_COLOR_FORM_RLD_BD                (0x1a << 16)
#  define BLIT_COLOR_FORM_RLD_H2                (0x1b << 16)
#  define BLIT_COLOR_FORM_RLD_H8                (0x1c << 16)

#define BLIT_COLOR_FORM_IS_RGB                (0x00 << 16)
#define BLIT_COLOR_FORM_IS_CLUT               (0x08 << 16)
#define BLIT_COLOR_FORM_IS_YUV                (0x10 << 16)
#define BLIT_COLOR_FORM_IS_MISC               (0x18 << 16)
#define BLIT_COLOR_FORM_TYPE_MASK             (0x18 << 16)

#define BLIT_TY_FULL_ALPHA_RANGE              (1L << 21)
#define BLIT_TY_COPYDIR_BOTTOMRIGHT           (3L << 24)
/*
 * NOTE: bit 29 is the opposite way around from the original Gamma blitter
 *       documentation, which appears to be incorrect. It is documented
 *       correctly for the BDisp blitter and works correctly on both.
 */
#define BLIT_TY_COLOR_EXPAND_MSB              (0)
#define BLIT_TY_COLOR_EXPAND_ZEROS            (1L << 29)
#define BLIT_TY_BIG_ENDIAN                    (1L << 30)


/*
 * Colourspace conversion and CLUT control
 */
#define BLIT_CCO_INCOL_ITU709                 (1L<<0)
#define BLIT_CCO_INSIGN_TWO_COMP              (1L<<1)
#define BLIT_CCO_INDIR_RGB2YUV                (1L<<2)
#define BLIT_CCO_INMATRIX_VID_NOT_GFX         (1L<<3)
#define BLIT_CCO_OUTCOL_ITU709                (1L<<8)
#define BLIT_CCO_OUTSIGN_TWO_COMP             (1L<<9)
#define BLIT_CCO_OUTMATRIX_VID_NOT_GFX        (1L<<10)
#define BLIT_CCO_CLUT_EXPAND                  (0)
#define BLIT_CCO_CLUT_CORRECT                 (1L<<16)
#define BLIT_CCO_CLUT_REDUCE                  (2L<<16)
#define BLIT_CCO_CLUT_UPDATE_EN               (1L<<18)

/*
 * Common resize and flicker filter control
 */
#define BLIT_RZC_2DHF_MODE_DISABLED           (0)
#define BLIT_RZC_2DHF_MODE_RESIZE_ONLY        (4L<<0)
#define BLIT_RZC_2DHF_MODE_FILTER_COLOUR      (5L<<0)
#define BLIT_RZC_2DHF_MODE_FILTER_ALPHA       (6L<<0)
#define BLIT_RZC_2DHF_MODE_FILTER_BOTH        (7L<<0)
#define BLIT_RZC_2DVF_MODE_DISABLED           (0)
#define BLIT_RZC_2DVF_MODE_RESIZE_ONLY        (4L<<4)
#define BLIT_RZC_2DVF_MODE_FILTER_COLOUR      (5L<<4)
#define BLIT_RZC_2DVF_MODE_FILTER_ALPHA       (6L<<4)
#define BLIT_RZC_2DVF_MODE_FILTER_BOTH        (7L<<4)
#define BLIT_RZC_FF_MODE_FILTER0              (0)
#define BLIT_RZC_FF_MODE_ADAPTIVE             (1L<<8)

#define BLIT_RZC_Y_2DHF_MODE_DISABLED         (0)
#define BLIT_RZC_Y_2DHF_MODE_RESIZE_ONLY      (2L<<28)
#define BLIT_RZC_Y_2DHF_MODE_FILTER_BOTH      (3L<<28)
#define BLIT_RZC_Y_2DVF_MODE_DISABLED         (0)
#define BLIT_RZC_Y_2DVF_MODE_RESIZE_ONLY      (2L<<24)
#define BLIT_RZC_Y_2DVF_MODE_FILTER_BOTH      (3L<<24)


/*
 * 5x8 and 8x8 Filter coefficients for both H & V resize.
 */
#define STM_BLIT_FILTER_TABLE_SIZE   64

struct stm_blit_filter_coeffs
{
    int   srcMin;
    int   srcMax;
    UCHAR filter_coeffs[STM_BLIT_FILTER_TABLE_SIZE];
};

/*
 * Fixed flicker filter coefficients for simple 3-tap filter
 */
extern const ULONG stm_flicker_filter_coeffs[4];


class CDisplayDevice;

class CSTmBlitter : public CGAL
{
public:
  CSTmBlitter(ULONG *pBlitterBaseAddr);
  virtual ~CSTmBlitter();

  virtual bool Create(void);
  virtual int  SyncChip(bool WaitNextOnly);

  static inline void SetXY(short X, short Y, ULONG* pReg)
  {
    *pReg = X | (Y << 16);
  }

  static bool SetBufferType(const stm_blitter_surface_t &Surf, ULONG* pReg);
  
protected:
  void SetKeyColour   (const stm_blitter_surface_t &Surf, ULONG  keyColour, ULONG* pReg) const;

  void SetCLUTOperation(ULONG srctype, ULONG &ins, ULONG &cco) const
  {
    ins |= BLIT_INS_ENABLE_CLUT;
    cco |= BLIT_CCO_CLUT_UPDATE_EN;

    switch(srctype)
    {
      case BLIT_COLOR_FORM_IS_CLUT:
        cco |= BLIT_CCO_CLUT_EXPAND;
        break;
      default: 
        cco |= BLIT_CCO_CLUT_CORRECT;
        break;
    }
  }

  int  ChooseBlitter5x8FilterCoeffs(int src);
  int  ChooseBlitter8x8FilterCoeffs(int src);
  
  ULONG          *m_pBlitterReg;

  DMA_Area        m_BlitNodeBuffer; // "Hardware" queue

  ULONG           m_ulBufferNodes;
  ULONG           m_ulBufferHeadIndex;
  ULONG           m_ulBufferTailIndex;
  
  volatile ULONG  m_nodesWaiting; // volatile as shared between thread and interrupt code

  DMA_Area        m_8x8FilterTables;
  DMA_Area        m_5x8FilterTables;

  ULONG           m_preemptionLock;
  
  void WriteDevReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pBlitterReg + (reg>>2), val); }
  ULONG ReadDevReg(ULONG reg)            { return g_pIOS->ReadRegister(m_pBlitterReg + (reg>>2)); }
  
private:
  CSTmBlitter(const CSTmBlitter&);
  CSTmBlitter& operator=(const CSTmBlitter&);
};

#endif // _STM_BLITTER_H
