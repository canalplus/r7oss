/***********************************************************************
 * 
 * File: Gamma/GammaBlitter.cpp
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Generic/DisplayDevice.h>

#include "GenericGammaDefs.h"
#include "GenericGammaDevice.h"
#include "GammaBlitter.h"

// If you want this file to produce debug, just set the following define to 1
//#define DEBUG_GAMMA_BLT

#ifdef DEBUG_GAMMA_BLT 

#define DEBUGBLITNODE(a) DebugBlitNode(a)

void DebugBlitNode(GAMMA_BLIT_NODE *pBlitNode)
{
    DEBUGF(("INS :0x%08X. USR :0x%08X. NIP :0x%08X.\n",
                pBlitNode->BLT_INS, 
                pBlitNode->BLT_USR, 
                pBlitNode->BLT_NIP));
    DEBUGF(("S1BA:0x%08X. S1TY:0x%08X. S1XY:0x%08X. S1SZ:0x%08X. S1CF:0x%08X.\n",
                pBlitNode->BLT_S1BA,
                pBlitNode->BLT_S1TY, 
                pBlitNode->BLT_S1XY, 
                pBlitNode->BLT_S1SZ, 
                pBlitNode->BLT_S1CF));
    DEBUGF(("S2BA:0x%08X. S2TY:0x%08X. S2XY:0x%08X. S2SZ:0x%08X. S2CF:0x%08X.\n",
                pBlitNode->BLT_S2BA,
                pBlitNode->BLT_S2TY, 
                pBlitNode->BLT_S2XY, 
                pBlitNode->BLT_S2SZ, 
                pBlitNode->BLT_S2CF));
    DEBUGF(("TBA :0x%08X. TTY :0x%08X. TXY :0x%08X\n",
                pBlitNode->BLT_TBA,
                pBlitNode->BLT_TTY, 
                pBlitNode->BLT_TXY));
    DEBUGF(("CWO :0x%08X. CWS :0x%08X. CCO :0x%08X. CML :0x%08X.\n",
                pBlitNode->BLT_CWO,
                pBlitNode->BLT_CWS, 
                pBlitNode->BLT_CCO, 
                pBlitNode->BLT_CML));
    DEBUGF(("RZC :0x%08X. HFP :0x%08X. VFP :0x%08X. RSF :0x%08X.\n",
                pBlitNode->BLT_RZC,
                pBlitNode->BLT_HFP, 
                pBlitNode->BLT_VFP, 
                pBlitNode->BLT_RSF));
    DEBUGF(("FF0 :0x%08X. FF1 :0x%08X. FF2 :0x%08X. FF3 :0x%08X.\n",
                pBlitNode->BLT_FF0,
                pBlitNode->BLT_FF1, 
                pBlitNode->BLT_FF2, 
                pBlitNode->BLT_FF3));
    DEBUGF(("ACK :0x%08X. KEY1:0x%08X. KEY2:0x%08X. PMK :0x%08X.\n",
                pBlitNode->BLT_ACK,
                pBlitNode->BLT_KEY1, 
                pBlitNode->BLT_KEY2, 
                pBlitNode->BLT_PMK));
    DEBUGF(("RST :0x%08X. XYL :0x%08X. XYP :0x%08X. JUNK:0x%08X.\n",
                pBlitNode->BLT_RST,
                pBlitNode->BLT_XYL, 
                pBlitNode->BLT_XYP, 
                pBlitNode->BLT_JUNK));
}

#else
#define DEBUGBLITNODE(a) 
#endif

static const ULONG CTLOFFSET           = 0x00;
static const ULONG STA1OFFSET          = 0x04;
static const ULONG STA2OFFSET          = 0x08;
static const ULONG STA3OFFSET          = 0x0C;
static const ULONG NIPOFFSET           = 0x10;
static const ULONG USROFFSET           = 0x14;
static const ULONG REGISTER_MAP_SIZE   = 0x00000100;
static const ULONG BLIT_NODE_SIZE      = sizeof(GAMMA_BLIT_NODE);
static const ULONG INSOFFSET           = 0x18;

static const ULONG BLITTER_IDLE        = 0x01;
static const ULONG BLITTER_COMPLETED   = 0x02;
static const ULONG BLITTER_READY_START = 0x04;
static const ULONG BLITTER_SUSPEND     = 0x08;

static const ULONG BLIT_NODE_BUFFER_SZ = 128;

static const ULONG STGGAL_VALID_DRAW_OPS =  STM_BLITTER_FLAGS_GLOBAL_ALPHA            |
                                            STM_BLITTER_FLAGS_BLEND_SRC_ALPHA         |
                                            STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT |
                                            STM_BLITTER_FLAGS_BLEND_DST_COLOR         |
                                            STM_BLITTER_FLAGS_BLEND_DST_MEMORY        |
                                            STM_BLITTER_FLAGS_BLEND_DST_ZERO          |
                                            STM_BLITTER_FLAGS_PLANE_MASK              |
                                            STM_BLITTER_FLAGS_DST_COLOR_KEY;

static const ULONG STGGAL_VALID_COPY_OPS =  STM_BLITTER_FLAGS_DST_COLOR_KEY           |
                                            STM_BLITTER_FLAGS_SRC_COLOR_KEY           |
                                            STM_BLITTER_FLAGS_GLOBAL_ALPHA            |
                                            STM_BLITTER_FLAGS_BLEND_SRC_ALPHA         |
                                            STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT |
                                            STM_BLITTER_FLAGS_BLEND_DST_COLOR         |
                                            STM_BLITTER_FLAGS_BLEND_DST_MEMORY        |
                                            STM_BLITTER_FLAGS_BLEND_DST_ZERO          |
                                            STM_BLITTER_FLAGS_PLANE_MASK              |
                                            STM_BLITTER_FLAGS_CLUT_ENABLE             |
                                            STM_BLITTER_FLAGS_FLICKERFILTER;
                                      
                                                

////////////////////////////////////////////////////////////////////////////
// CGammaBlitter construction, creation and destruction
//
CGammaBlitter::CGammaBlitter(ULONG *pBlitterBaseAddr): CSTmBlitter(pBlitterBaseAddr)
{
}


bool CGammaBlitter::Create()
{     
  DEBUGF2(2, ("CGammaBlitter::Create()\n"));  

  if(!CSTmBlitter::Create())
    return false;

  m_ulBufferNodes = BLIT_NODE_BUFFER_SZ;

  g_pIOS->AllocateDMAArea(&m_BlitNodeBuffer, m_ulBufferNodes*BLIT_NODE_SIZE, 16, SDAAF_NONE);
  if(!m_BlitNodeBuffer.pMemory)
    return false;

  g_pIOS->MemsetDMAArea(&m_BlitNodeBuffer, 0, 0, m_ulBufferNodes*BLIT_NODE_SIZE);

  ResetBlitter();

  return true;
}


CGammaBlitter::~CGammaBlitter()
{
  DEBUGF2(2, ("CGammaBlitter::~CGammaBlitter() called.\n"));

  g_pIOS->FreeDMAArea(&m_BlitNodeBuffer);

  DEBUGF2(2, ("CGammaBlitter::~CGammaBlitter() completed.\n"));
}


////////////////////////////////////////////////////////////////////////////
// Colour conversion and ALU setup helpers
//
void CGammaBlitter::YUVRGBConversions(GAMMA_BLIT_NODE* pBlitNode)
{
  /*
   * OK, at the moment we only need to deal with two cases,
   * i.e. when src = rgb & dst = yuv or src = yuv & dst = rgb
   * as src1 and the dstination must be the same as dst (for blending
   * or it isn't used. We always assume YUV is in 601 colourspace,
   * has full range (not limited to video levels) and conversion between 601
   * and 709 is not supported.
   * 
   * This basically covers what we can represent in DirectFB.
   */
  ULONG targettype = pBlitNode->BLT_TTY  & BLIT_COLOR_FORM_TYPE_MASK;
  ULONG src2type   = pBlitNode->BLT_S2TY & BLIT_COLOR_FORM_TYPE_MASK;

  if(targettype == BLIT_COLOR_FORM_IS_YUV && src2type != BLIT_COLOR_FORM_IS_YUV)
  {
    DEBUGF2(2, ("CGammaBlitter::YUVRGBConversions() RGB->YUV\n"));
    pBlitNode->BLT_INS |= BLIT_INS_ENABLE_ICSC; 
    pBlitNode->BLT_CCO |= BLIT_CCO_INDIR_RGB2YUV;
  }
  else if (targettype != BLIT_COLOR_FORM_IS_YUV && src2type == BLIT_COLOR_FORM_IS_YUV)
  {
    DEBUGF2(2, ("CGammaBlitter::YUVRGBConversions() YUV->RGB\n"));
    pBlitNode->BLT_INS |= BLIT_INS_ENABLE_ICSC;
  }
}


void CGammaBlitter::BlendOperation(GAMMA_BLIT_NODE* pBlitNode, const stm_blitter_operation_t &op)
{
  unsigned long globalAlpha = 128; // Default fully opaque value
    
  if(op.ulFlags & STM_BLITTER_FLAGS_GLOBAL_ALPHA)
  {
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting global alpha 0x%lx\n",op.ulGlobalAlpha));
    globalAlpha = (op.ulGlobalAlpha+1) / 2; // Scale 0-255 to 0-128 (note not 127!)
  }
      
  pBlitNode->BLT_ACK |= ((globalAlpha & 0xff) << 8);
  
  // Note the the following blending options are mutually exclusive
  if (op.ulFlags & STM_BLITTER_FLAGS_BLEND_SRC_ALPHA)
  {
    // RGB of SRC2 is blended with SRC1 based on SRC2 Alpha * Global Alpha
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting src as NOT pre-multiplied\n"));
    pBlitNode->BLT_ACK &= ~BLIT_ACK_MODE_MASK;
    pBlitNode->BLT_ACK |= BLIT_ACK_BLEND_SRC2_N_PREMULT;
  }
  else if (op.ulFlags & STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT)
  {
    // RGB of SRC2 is blended with SRC1 based on Global Alpha, the SRC2 RGB
    // is assumed to have been pre-multiplied by SRC2 Alpha.
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting src as pre-multiplied\n"));
    pBlitNode->BLT_ACK &= ~BLIT_ACK_MODE_MASK;
    pBlitNode->BLT_ACK |= BLIT_ACK_BLEND_SRC2_PREMULT;
  }

  if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_COLOR)
  {
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting dst colour\n"));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_COLOR_FILL;
    pBlitNode->BLT_S1CF = op.ulColour;
  }
  else if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_ZERO)
  {
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting dst colour to zero\n"));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_COLOR_FILL;
    pBlitNode->BLT_S1CF = 0;
  }
  else if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_MEMORY)
  {
    DEBUGF2(2, ("CGammaBlitter::BlendOperation() setting dst memory\n"));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
  }
}


bool CGammaBlitter::ColourKey(GAMMA_BLIT_NODE* pBlitNode, const stm_blitter_operation_t &op)
{

  if (op.ulFlags & STM_BLITTER_FLAGS_SRC_COLOR_KEY)
  {
    DEBUGF2(2, ("CGammaBlitter::ColourKey() setting src keying\n"));
    pBlitNode->BLT_INS |= BLIT_INS_ENABLE_COLOURKEY;
    pBlitNode->BLT_ACK |= BLIT_ACK_RGB_ENABLE | BLIT_ACK_SRC_COLOUR_KEYING;
    SetKeyColour(op.srcSurface, op.ulColorKeyLow,  &(pBlitNode->BLT_KEY1));
    SetKeyColour(op.srcSurface, op.ulColorKeyHigh, &(pBlitNode->BLT_KEY2));
  }

  if (op.ulFlags & STM_BLITTER_FLAGS_DST_COLOR_KEY)
  {
    if (op.ulFlags & (STM_BLITTER_FLAGS_BLEND_DST_COLOR | STM_BLITTER_FLAGS_BLEND_DST_ZERO))
    {
      DEBUGF2(2, ("CGammaBlitter::ColourKey() cannot destination colour key and blend against a solid colour\n"));
      return false;
    }
    
    DEBUGF2(2, ("CGammaBlitter::ColourKey() setting dst keying\n"));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_ENABLE_COLOURKEY | BLIT_INS_SRC1_MODE_MEMORY;
    pBlitNode->BLT_ACK |= BLIT_ACK_RGB_ENABLE | BLIT_ACK_DEST_COLOUR_KEYING;
    SetKeyColour(op.dstSurface, op.ulColorKeyLow,  &(pBlitNode->BLT_KEY1));
    SetKeyColour(op.dstSurface, op.ulColorKeyHigh, &(pBlitNode->BLT_KEY2));
  }
  
  return true;
}


////////////////////////////////////////////////////////////////////////////
// Drawing operations
//
bool CGammaBlitter::FillRect(const stm_blitter_operation_t &op, const stm_rect_t &dst)
{
  DEBUGF2(2, ("%s @ %p: colour %.8lx l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              op.ulColour, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left != dst.right) && (dst.top != dst.bottom))
  {
    GAMMA_BLIT_NODE  blitNode = {0};

    if (op.ulFlags & ~STGGAL_VALID_DRAW_OPS)
    {
      DEBUGF2(1, ("CGammaBlitter::FillRect() operation flags not handled 0x%lx\n",op.ulFlags));
      return false;
    }

    if(!SetBufferType(op.dstSurface,&(blitNode.BLT_TTY)))
    {
      DEBUGF2(1, ("CGammaBlitter::FillRect() invalid destination type\n"));
      return false;
    }
    
    blitNode.BLT_TBA = op.dstSurface.ulMemory;
    
    SetXY(dst.left, dst.top, &(blitNode.BLT_TXY));

    blitNode.BLT_S1XY = blitNode.BLT_TXY;
    blitNode.BLT_S1SZ = (((dst.bottom - dst.top) & 0x0FFF) << 16) | ((dst.right - dst.left) & 0x0FFF);
      
    if (op.ulFlags == STM_BLITTER_FLAGS_NONE && (op.colourFormat == op.dstSurface.format))
    {
      // Solid Fill
      DEBUGF2(2, ("CGammaBlitter::FillRect() fast direct fill\n"));
      blitNode.BLT_S1TY = blitNode.BLT_TTY & ~BLIT_TY_BIG_ENDIAN;
      blitNode.BLT_S1CF = op.ulColour;
      blitNode.BLT_INS  = BLIT_INS_SRC1_MODE_DIRECT_FILL;
    }
    else
    {
      stm_blitter_surface_t tmpSurf = {{0}};
      
      tmpSurf.format = op.colourFormat;
      
      // Operations that require the destination in SRC1 e.g. blending and colourkeying           
      DEBUGF2(2, ("CGammaBlitter::FillRect() complex fill\n"));
      if(!SetBufferType(tmpSurf,&(blitNode.BLT_S2TY)))
      {
        DEBUGF2(1, ("CGammaBlitter::FillRect() invalid source colour format\n"));
        return false;
      }

      blitNode.BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;
      
      blitNode.BLT_S2XY = blitNode.BLT_TXY;
      blitNode.BLT_S2SZ = blitNode.BLT_S1SZ;
      blitNode.BLT_S2CF = op.ulColour;
      
      blitNode.BLT_S1TY = blitNode.BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;
      blitNode.BLT_S1BA = blitNode.BLT_TBA;
    
      blitNode.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
      blitNode.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;
      
      if(!ColourKey(&blitNode, op))
        return false;

      BlendOperation(&blitNode, op);
      SetPlaneMask  (op, blitNode.BLT_INS, blitNode.BLT_PMK);
      YUVRGBConversions(&blitNode);
    }

    QueueBlitOperation(blitNode);

    DEBUGF2(2, ("CGammaBlitter::FillRect() completed.\n"));
  }

  return true;
}


bool CGammaBlitter::DrawRect(const stm_blitter_operation_t &op, const stm_rect_t &dst)
{
  DEBUGF2(2, ("%s @ %p: colour %.8lx l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              op.ulColour, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left != dst.right) && (dst.top != dst.bottom))
  {
    GAMMA_BLIT_NODE       blitNodes[4];
    int                   i;

    if (op.ulFlags & ~STGGAL_VALID_DRAW_OPS)
    {
      DEBUGF2(1, ("CGammaBlitter::DrawRect() operation flags not handled 0x%lx\n",op.ulFlags));
      return false;
    }
  
    g_pIOS->ZeroMemory(&(blitNodes[0]), sizeof(blitNodes[0]));

    if(!SetBufferType(op.dstSurface,&(blitNodes[0].BLT_TTY)))
    {
      DEBUGF2(1, ("CGammaBlitter::DrawRect() invalid destination type\n"));
      return false;
    }
    
    blitNodes[0].BLT_TBA = op.dstSurface.ulMemory; 
      
    // First set up the control fields in the first blit node for the operation
    if (op.ulFlags == STM_BLITTER_FLAGS_NONE && (op.colourFormat == op.dstSurface.format))
    {
      // Solid Rect
      blitNodes[0].BLT_S1TY = blitNodes[0].BLT_TTY & ~BLIT_TY_BIG_ENDIAN;
      blitNodes[0].BLT_S1CF = op.ulColour;
      blitNodes[0].BLT_INS  = BLIT_INS_SRC1_MODE_DIRECT_FILL;
    }
    else
    {
      stm_blitter_surface_t tmpSurf = {{0}};
      
      tmpSurf.format = op.colourFormat;
      
      // Operations that require the destination in SRC1 e.g. blending and colourkeying           
      if(!SetBufferType(tmpSurf,&(blitNodes[0].BLT_S2TY)))
      {
        DEBUGF2(1, ("CGammaBlitter::DrawRect() invalid source colour type\n"));
        return false;
      }

      blitNodes[0].BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;

      // Operations that require the destination in SRC1 e.g. blending and colourkeying
      blitNodes[0].BLT_S1TY = blitNodes[0].BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;
      blitNodes[0].BLT_S1BA = blitNodes[0].BLT_TBA;
      
      blitNodes[0].BLT_S2CF =  op.ulColour;

      blitNodes[0].BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
      blitNodes[0].BLT_ACK = BLIT_ACK_BYPASSSOURCE2;
      
      if(!ColourKey(&blitNodes[0], op))
        return false;

      BlendOperation(&blitNodes[0], op);
      SetPlaneMask  (op, blitNodes[0].BLT_INS, blitNodes[0].BLT_PMK);
      YUVRGBConversions(&blitNodes[0]);
    }

    // Copy the first node as a template for the other three
    for(i = 1; i < 4; i++)
    {
      blitNodes[i] = blitNodes[0];
    }

    // Now set up the coordinates and sizes for the surface
    blitNodes[0].BLT_TXY = blitNodes[0].BLT_S1XY = dst.left        | (dst.top          << 16);
    blitNodes[1].BLT_TXY = blitNodes[1].BLT_S1XY = (dst.right - 1) | ((dst.top + 1)    << 16);
    blitNodes[2].BLT_TXY = blitNodes[2].BLT_S1XY = dst.left        | ((dst.bottom - 1) << 16);
    blitNodes[3].BLT_TXY = blitNodes[3].BLT_S1XY = dst.left        | ((dst.top + 1)    << 16);

    blitNodes[0].BLT_S1SZ = (0x0001 << 16) | ((dst.right - dst.left) & 0x0FFF);
    blitNodes[1].BLT_S1SZ = ((((dst.bottom - dst.top) - 2) & 0x0FFF) << 16) | (0x0001);
    blitNodes[2].BLT_S1SZ = (0x0001 << 16) | ((dst.right - dst.left) & 0x0FFF);
    blitNodes[3].BLT_S1SZ = ((((dst.bottom - dst.top) - 2) & 0x0FFF) << 16) | (0x0001);      

    /*
     * Now make the the source2 x,y and size the same as source1 otherwise
     * you crash the blitter even when source2 is a solid colour
     */
    for(i = 0; i < 4; i++)
    {
      blitNodes[i].BLT_S2XY = blitNodes[i].BLT_TXY;
      blitNodes[i].BLT_S2SZ = blitNodes[i].BLT_S1SZ;
    }      
    
    // send data to chip;
    DEBUGBLITNODE(&blitNodes[0]);
    QueueBlitOperation(blitNodes[0]);
    QueueBlitOperation(blitNodes[1]);
    QueueBlitOperation(blitNodes[2]);
    QueueBlitOperation(blitNodes[3]);

    DEBUGF2(2, ("CGammaBlitter::DrawRect() completed.\n"));
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////
// Blitting (Copy) operations
//
bool CGammaBlitter::CopyRect(const stm_blitter_operation_t &op,
                             const stm_rect_t              &dst,
                             const stm_point_t             &src)
{
  DEBUGF2(2, ("%s @ %p: src x/y: %lu/%lu dst l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              src.x, src.y, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left != dst.right) && (dst.top != dst.bottom))
  {
    GAMMA_BLIT_NODE  blitNode = {0};

    // Set the fill mode      
    blitNode.BLT_INS = BLIT_INS_SRC1_MODE_DIRECT_COPY;

    // Set source type
    if(!SetBufferType(op.srcSurface,&(blitNode.BLT_S1TY)))
    {
      DEBUGF2(1, ("CGammaBlitter::CopyRect() invalid source type\n"));
      return false;
    }

    // Set target type
    if(!SetBufferType(op.dstSurface,&(blitNode.BLT_TTY)))
    {
      DEBUGF2(1, ("CGammaBlitter::CopyRect() invalid destination type\n"));
      return false;
    }


    // Work out copy direction
    bool copyDirReversed = false;
  
    if(op.srcSurface.ulMemory == op.dstSurface.ulMemory)
    {
      copyDirReversed = (dst.top > src.y) ||    
                        ((dst.top == src.y) && (dst.left > src.x));
    }
  
    if (!copyDirReversed)
    {
      SetXY(src.x, src.y, &(blitNode.BLT_S1XY));
      SetXY(dst.left, dst.top, &(blitNode.BLT_TXY));
    }
    else
    {
      blitNode.BLT_S1TY |= BLIT_TY_COPYDIR_BOTTOMRIGHT;
      blitNode.BLT_TTY  |= BLIT_TY_COPYDIR_BOTTOMRIGHT;

      SetXY(src.x + dst.right - (dst.left + 1),
            src.y + dst.bottom - (dst.top + 1), &(blitNode.BLT_S1XY));
  
      SetXY(dst.right - 1, dst.bottom - 1, &(blitNode.BLT_TXY));
    }

    // Set the source base address
    blitNode.BLT_S1BA = op.srcSurface.ulMemory;
    
    // Set the target base address
    blitNode.BLT_TBA = op.dstSurface.ulMemory;

    // Set the window dimensions
    blitNode.BLT_S1SZ = (((dst.bottom - dst.top) & 0x0FFF) << 16) |
                         ((dst.right - dst.left) & 0x0FFF);   
                         
    QueueBlitOperation(blitNode);	 

    DEBUGF2(2, ("CGammaBlitter::CopyRect() completed.\n"));
  }

  return true;
}


bool CGammaBlitter::CopyRectComplex(const stm_blitter_operation_t &op,
                                    const stm_rect_t              &dst,
                                    const stm_rect_t              &src)
{
  stm_point_t SrcPoint;

  SrcPoint.x = src.left;
  SrcPoint.y = src.top;

  DEBUGF2(2, ("%s @ %p: flags: %.8lx src l/r/t/b: %lu/%lu/%lu/%lu -> dst l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this, op.ulFlags,
              src.left, src.right, src.top, src.bottom,
              dst.left, dst.right, dst.top, dst.bottom));

  if (op.ulFlags & ~STGGAL_VALID_COPY_OPS)
  {
    return false;
  }
  
  GAMMA_BLIT_NODE  blitNode = {0};
        
  blitNode.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;
  blitNode.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_MEMORY;

  if (op.ulFlags & STM_BLITTER_FLAGS_FLICKERFILTER)
  {
    blitNode.BLT_INS |= (BLIT_INS_ENABLE_FLICKERFILTER |
                         BLIT_INS_ENABLE_2DRESCALE);

    blitNode.BLT_FF0  = stm_flicker_filter_coeffs[0];
    blitNode.BLT_FF1  = stm_flicker_filter_coeffs[1];
    blitNode.BLT_FF2  = stm_flicker_filter_coeffs[2];
    blitNode.BLT_FF3  = stm_flicker_filter_coeffs[3];

    blitNode.BLT_RZC  |= (BLIT_RZC_FF_MODE_ADAPTIVE      |
                          BLIT_RZC_2DHF_MODE_RESIZE_ONLY |
                          BLIT_RZC_2DVF_MODE_RESIZE_ONLY);

    blitNode.BLT_RSF  = 0x04000400; // 1:1 scaling by default
  }

  if (((src.bottom - src.top) != (dst.bottom - dst.top)) ||
      ((src.right - src.left) != (dst.right - dst.left)))
  {
    /*
     * Note that the resize filter setup may override the default 1:1 resize
     * configuration set by the flicker filter; this is fine.
     */
    ULONG ulScaleh=0;
    ULONG ulScalew=0;
    
    ulScaleh  =  ((src.bottom - src.top)<<10) / (dst.bottom - dst.top);    
    ulScalew  =  ((src.right - src.left)<<10) / (dst.right - dst.left);
    
    if(ulScaleh > (32<<10) || ulScalew > (32<<10))
      return false;
      
    if(ulScaleh == 0 || ulScalew == 0)
      return false;
      
    /*
     * Fail/fallback to software for >2x downscale.
     * This avoid an apparent hardware bug in all chips containing the
     * gamma blitter. This is under further investigation.
     */
    if(ulScalew > (2<<10))
      return false;
      
    DEBUGF2(2, ("CGammaBlitter::CopyRectComplex() Scaleh = 0x%08lx Scalew = 0x%08lx.\n",ulScaleh,ulScalew));

    int filterIndex;
    if((filterIndex = ChooseBlitter5x8FilterCoeffs((int)ulScalew)) < 0)
    {    
      DEBUGF2(1, ("Error: No horizontal resize filter found.\n"));
      blitNode.BLT_RZC |= BLIT_RZC_2DHF_MODE_RESIZE_ONLY;
      blitNode.BLT_HFP = 0;
    }
    else
    {
      blitNode.BLT_RZC |= BLIT_RZC_2DHF_MODE_FILTER_BOTH;
      blitNode.BLT_HFP = m_5x8FilterTables.ulPhysical + filterIndex*STM_BLIT_FILTER_TABLE_SIZE;
    }

    if((filterIndex = ChooseBlitter5x8FilterCoeffs((int)ulScaleh)) < 0)
    {
      DEBUGF2(1, ("Error: No vertical resize filter found.\n"));
      blitNode.BLT_RZC |= BLIT_RZC_2DVF_MODE_RESIZE_ONLY;
      blitNode.BLT_VFP = 0;
    }
    else
    {   
      blitNode.BLT_RZC |= BLIT_RZC_2DVF_MODE_FILTER_BOTH;
      blitNode.BLT_VFP = m_5x8FilterTables.ulPhysical + filterIndex*STM_BLIT_FILTER_TABLE_SIZE;
    }
    
    blitNode.BLT_INS |= BLIT_INS_ENABLE_2DRESCALE;
    blitNode.BLT_RSF = (ulScaleh << 16) | (ulScalew & 0x0000FFFF);

  }
    
  // Set source 2 type
  if(!SetBufferType(op.srcSurface,&(blitNode.BLT_S2TY)))
  {
    DEBUGF2(1, ("CGammaBlitter::CopyRectComplex() invalid source2 type\n"));
    return false;
  }

  blitNode.BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;

  // Set target type
  if(!SetBufferType(op.dstSurface,&(blitNode.BLT_TTY)))
  {
    DEBUGF2(1, ("CGammaBlitter::CopyRectComplex() invalid destination type\n"));
    return false;
  }

  // Work out copy direction
  bool copyDirReversed = false;
  
  if(op.srcSurface.ulMemory == op.dstSurface.ulMemory)
  {
    copyDirReversed = (dst.top > src.top) ||	
                      ((dst.top == src.top) && (dst.left > src.left));
  }
  
  if (!copyDirReversed)
  {
    SetXY(src.left, src.top, &(blitNode.BLT_S2XY));
    SetXY(dst.left, dst.top, &(blitNode.BLT_TXY));
  }
  else
  {
    blitNode.BLT_S2TY |= BLIT_TY_COPYDIR_BOTTOMRIGHT;
    blitNode.BLT_TTY  |= BLIT_TY_COPYDIR_BOTTOMRIGHT;

    SetXY(src.right - 1, src.bottom - 1, &(blitNode.BLT_S2XY));
    SetXY(dst.right - 1, dst.bottom - 1, &(blitNode.BLT_TXY));
  }

  // Set the source 2 base address and size
  blitNode.BLT_S2BA = op.srcSurface.ulMemory;

  blitNode.BLT_S2SZ = (((src.bottom - src.top) & 0x0FFF) << 16) |
                       ((src.right - src.left) & 0x0FFF);
    
  // Set the target base address
  blitNode.BLT_TBA = op.dstSurface.ulMemory;

  /*
   * Setup SRC1 even if we are not blending or destination colour
   * keying. Why? Because turning off SRC1 completely crashes the blitter.
   * The suspicion is that the ALU is still trying to pull data from the
   * SRC1 path even when SRC1 is disabled in BLT_INS and when the ALU
   * operation is BYPASSSOURCE2. As SRC1 isn't generating data the whole
   * thing deadlocks. Thankfully it only seems to be necessary
   * to set SRC1 up as a solid colour fill, not actually to read memory.
   */
  blitNode.BLT_S1BA = blitNode.BLT_TBA;
  /*
   * Note: do not be tempted to copy BLT_S2TY into BLT_S1TY here,
   * the two surfaces may be different colour formats. SRC1 is the same
   * type as the destination in this case.
   */
  blitNode.BLT_S1TY = blitNode.BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;
  blitNode.BLT_S1XY = blitNode.BLT_TXY;  
  blitNode.BLT_S1SZ = (((dst.bottom - dst.top) & 0x0FFF) << 16) |
                       ((dst.right - dst.left) & 0x0FFF);


  /*
   * We set the CLUT operation after the source 2 type (which the CLUT operates
   * on) in order to determine if we are doing a colour lookup or an RGB
   * LUT correction (e.g. for display gamma).
   */
  if(op.ulFlags & STM_BLITTER_FLAGS_CLUT_ENABLE)
  {
    blitNode.BLT_CML = op.ulCLUT; 
                      
    SetCLUTOperation((blitNode.BLT_S2TY & BLIT_COLOR_FORM_TYPE_MASK),
                      blitNode.BLT_INS, blitNode.BLT_CCO);

    DEBUGF2(2, ("CGammaBlitter::CopyRectComplex() CLUT type = %#08lx cco = %#08lx cml = %#08lx\n",(blitNode.BLT_S2TY & BLIT_COLOR_FORM_TYPE_MASK), blitNode.BLT_CCO, blitNode.BLT_CML));
  }
  
  if(!ColourKey(&blitNode, op))
    return false;

  BlendOperation (&blitNode, op);
  SetPlaneMask   (op, blitNode.BLT_INS, blitNode.BLT_PMK);
  
  /*
   * Rather like the clut this looks at the src2 and target type setup
   * to determine what colour space conversions are required.
   */  
  YUVRGBConversions(&blitNode);


  DEBUGBLITNODE(&blitNode);
  QueueBlitOperation(blitNode);

  DEBUGF2(2, ("CGammaBlitter::CopyRectComplex() completed.\n"));

  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Blitter HW Queue and interrupt management
//
void CGammaBlitter::QueueBlitOperation(GAMMA_BLIT_NODE &blitNode)
{		
    GAMMA_BLIT_NODE *pNextBlitNode;

    DEBUGF2(2, ("CGammaBlitter::QueueBlitOperation\n"));
    pNextBlitNode = (GAMMA_BLIT_NODE*)(m_BlitNodeBuffer.pData+(m_ulBufferHeadIndex*BLIT_NODE_SIZE));

    /* It doesn't matter if we alter blitNode here.
     * Set blit complete on for next blit and make sure the user value
     * is zero until we are ready to enable the node in the queue
     */
    blitNode.BLT_INS |= (1<<18);
    blitNode.BLT_USR  = 0;
       
    /*
     * See if the node buffer is full, if so sleep until the next node has been
     * processed by the hardware and its completion interrupt clears the USR field.
     */    
    if(pNextBlitNode->BLT_USR != 0)
    {
        DEBUGF2(2, ("CGammaBlitter::QueueBlitOperation - queue full, sleeping\n"));
	g_pIOS->SleepOnQueue(BLIT_NODE_QUEUE,&(pNextBlitNode->BLT_USR),0,STMIOS_WAIT_COND_EQUAL);
    }
		
    g_pIOS->MemcpyToDMAArea(&m_BlitNodeBuffer,(m_ulBufferHeadIndex*BLIT_NODE_SIZE),&blitNode,BLIT_NODE_SIZE);
    
    /*
     * We now have to flag the node as ready and if the blitter is stopped,
     * actually start the blit. This means there is a race between this code
     * and the interrupt handler which requires any OS pre-emption and interrupts
     * to be disabled while we do this. The race isn't obvious, but if we get
     * de-scheduled between setting BLT_USE and testing m_nodesWaiting, the
     * blitter interrupt could cause this node to be executed and the blitter
     * finish processing the node before we ever get to the test. This would mean
     * we end up starting the blitter again and executing the node a second
     * time. If the blit operation involves a read/modify/write (i.e. a blend)
     * then this will produce the wrong results on the screen.
     */
    g_pIOS->LockResource(m_preemptionLock);

    /*
     * Set the in use flag in the USR part of the node, it doesn't matter
     * if this doesn't leave the CPU cache, the HW doesn't use it.
     * 
     */
    pNextBlitNode->BLT_USR = 1;
     
    if(m_nodesWaiting == 0)
    {
        DEBUGF2(2, ("CGammaBlitter::QueueBlitOperation - blitter stopped, restarting manually\n"));
        StartBlitter(m_BlitNodeBuffer.ulPhysical+(m_ulBufferHeadIndex*BLIT_NODE_SIZE));
    }

    g_pIOS->UnlockResource(m_preemptionLock);
    
    /*
     * We don't have to lock buffer head index, because it is _only_ used
     * here.
     */ 
    m_ulBufferHeadIndex++;
    if(m_ulBufferHeadIndex == m_ulBufferNodes)
        m_ulBufferHeadIndex = 0;

    DEBUGF2(2, ("CGammaBlitter::QueueBlitOperation - out\n"));
}


bool CGammaBlitter::HandleBlitterInterrupt(void)
{
  ULONG vector = ReadDevReg(STA3OFFSET)&0x0000000E;

  if (!vector)
    return false;

  //clear the interrupt
  WriteDevReg(STA3OFFSET,vector);
	
  switch(vector)
  {
    case BLITTER_SUSPEND:
    case BLITTER_READY_START:
      break;
    case BLITTER_COMPLETED:  
    {
      GAMMA_BLIT_NODE *pBlitNode;

      DEBUGF2(3,("CGammaBlitter::HandleBlitterInterrupt 'Blit Completed'\n"));
      
      pBlitNode = &((GAMMA_BLIT_NODE*)m_BlitNodeBuffer.pData)[m_ulBufferTailIndex];

      /*
       * Mark this node as finished and signal any waiters
       */
      pBlitNode->BLT_USR = 0;
      g_pIOS->WakeQueue(BLIT_NODE_QUEUE);

      /*
       * Move on to the next node in the buffer
       */
      m_ulBufferTailIndex++;
      if(m_ulBufferTailIndex == m_ulBufferNodes)
        m_ulBufferTailIndex = 0;
        		
      pBlitNode = &((GAMMA_BLIT_NODE*)m_BlitNodeBuffer.pData)[m_ulBufferTailIndex];

      if(pBlitNode->BLT_USR != 0)
      {
        /*
         * The next node is marked as valid so restart the blitter.
         */
        StartBlitter(m_BlitNodeBuffer.ulPhysical+(m_ulBufferTailIndex*BLIT_NODE_SIZE));
      }
      else
      {
        /*
         * We have processed all currently available nodes in the buffer
         * so signal that the blitter is now finished and needs a manual
         * restart.
         */
        DEBUGF2(2,("CGammaBlitter::HandleBlitterInterrupt 'Blit Queue Empty'\n"));
        m_nodesWaiting = 0;
        g_pIOS->WakeQueue(BLIT_ENGINE_QUEUE);
      }

      break;
    }
    default:
      DEBUGF2(2,("CGammaBlitter::HandleBlitterInterrupt 'UNKNOWN BLITTER IRQ VECT'\n"));
      break;
  }

  return true;
}


void CGammaBlitter::ResetBlitter(void)
{	
  ULONG ctrlReg =0;

  DEBUGF2(2, ("CGammaBlitter::ResetBlitter() blitter status3 = %#08lx.\n",ReadDevReg(STA3OFFSET)));

  ctrlReg = ReadDevReg(CTLOFFSET); 
  ctrlReg |= 0x00000001; 
  WriteDevReg(CTLOFFSET, ctrlReg);
   
  ctrlReg = ReadDevReg(CTLOFFSET); 
  ctrlReg &= ~0x00000001; 
  WriteDevReg(CTLOFFSET, ctrlReg); 
    
  DEBUGF2(2, ("CGammaBlitter::ResetBlitter() waiting for blitter to become ready.\n"));
    
  while(IsEngineBusy())
  {
    g_pIOS->StallExecution(10);
  }

  DEBUGF2(2, ("CGammaBlitter::ResetBlitter() blitter ready.\n"));
}


void CGammaBlitter::StartBlitter(unsigned long physAddr)
{
  DEBUGF2(2, ("CGammaBlitter::StartBlitter() blitter status3 = %#08lx.\n",ReadDevReg(STA3OFFSET)));
  m_nodesWaiting = 1;

  WriteDevReg(NIPOFFSET,physAddr);

  WriteDevReg(CTLOFFSET,0x02);
  WriteDevReg(CTLOFFSET,0x00); 

  DEBUGF2(2, ("CGammaBlitter::StartBlitter() started status3 = %#08lx.\n",ReadDevReg(STA3OFFSET)));
}


bool CGammaBlitter::IsEngineBusy()
{
  return (ReadDevReg(STA3OFFSET) & 1) ? false:true;
}


int CGammaBlitter::SyncChip(bool WaitNextOnly)
{
  int err;

  /*
   * Do the wait first, which may return an error if interrupted.
   */
  if((err = CSTmBlitter::SyncChip(WaitNextOnly))!= 0)
    return err;

  DEBUGF2(3, ("CGammaBlitter::SyncChip() QueueEmpty\n"));

  /*
   * Is this sanity check needed any more? 
   */
  if(IsEngineBusy())
    ResetBlitter();
    
  return 0;
}

