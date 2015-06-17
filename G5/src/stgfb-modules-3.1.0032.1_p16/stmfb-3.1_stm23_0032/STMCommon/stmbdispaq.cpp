/***********************************************************************
 *
 * File: STMCommon/stmbdispaq.cpp
 * Copyright (c) 2007-2008 STMicroelectronics Limited.
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

#include "stmbdispregs.h"
#include "stmbdispaq.h"
#include "stmbdisp.h"


static const ULONG NODELISTPAGES = 45;

static const ULONG BLIT_NODE_SIZE       = sizeof(stm_bdisp_aq_full_blit_node);
static const ULONG BLIT_CIC_OFFSET      = (ULONG)&((stm_bdisp_aq_fast_blit_node*)0)->BLT_CIC;
static const ULONG BLIT_FAST_NODE_CIC   = 0x0000C;
static const ULONG BLIT_FULL_NODE_CIC   = 0x19B9C;
static const ULONG BLIT_FULLFILL_CIC    = 0x1911C;
static const ULONG BLIT_PLANAR_CIC      = (1<<3 | 1<<4 | 1<<5 | 1<<8 | 1<<9 | 1<<10);
static const ULONG BLIT_PMK_CIC         = (1L<<8);
static const ULONG BLIT_RESCALE_CIC     = (1L<<9);
static const ULONG BLIT_FFILT_CIC       = (1L<<11);
static const ULONG BLIT_KEYS_CIC        = (1L<<12);
static const ULONG BLIT_IVMX_CIC        = (1L<<15);
static const ULONG BLIT_OVMX_CIC        = (1L<<16);

static const int fixedpointONE  = 1L<<10;          // 1 in 6.10 fixed point
static const int fixedpointHALF = fixedpointONE/2; // 1/2 in 6.10 fixed point
static const int fractionMASK   = fixedpointONE-1;


/* this define is here because I assumed a noticable performance difference,
   but there isn't - so enable this feature by default... */
#define STM_BDISP_MEASURE_BLITLOAD
#ifdef STM_BDISP_MEASURE_BLITLOAD

  #define blitload_init_ticks() \
    ( { \
      m_Shared->bdisp_ops_start_time = 0; \
      m_Shared->ticks_busy = 0; \
    } )

  #define blitload_commit_blit_operation() \
    ( { \
      if (!m_Shared->bdisp_ops_start_time) \
        m_Shared->bdisp_ops_start_time = g_pIOS->GetSystemTime (); \
    } )

  /* we want some more statistics, so lets remember the number of ticks the
     BDisp was busy executing nodes */
  #define blitload_calc_busy_ticks() \
    ( { \
      m_Shared->ticks_busy += (g_pIOS->GetSystemTime () - m_Shared->bdisp_ops_start_time); \
      if (UNLIKELY (!m_Shared->ticks_busy)) \
        /* this can happen if the BDisp was faster in finishing a Node(list) \
           than it takes us to see a change in 'ticks'. 'Ticks' are in       \
           useconds at the moment, so fast enough; but still. Lets assume    \
           the BDisp was busy, because else the interrupt shouldn't have     \
           been asserted! */                                                 \
        m_Shared->ticks_busy = 1; \
      m_Shared->bdisp_ops_start_time = 0; \
    } )

#else /* STM_BDISP_BLITSTATS */
  #define blitload_init_ticks()            m_Shared->ticks_busy = 0
  #define blitload_commit_blit_operation() ( { } )
  #define blitload_calc_busy_ticks()       ( { } )
#endif /* STM_BDISP_BLITSTATS */


/* locking needs only to be done if access from user space is done,
   otherwise access is locked through a global blitter lock. */
#define LOCK_ATOMIC \
  if (m_bUserspaceAQ) \
  { \
    while (UNLIKELY (g_pIOS->AtomicCmpXchg ((ATOMIC *) &m_Shared->lock, 0, 1) != 0)) \
      g_pIOS->StallExecution (1); \
    if (UNLIKELY (m_Shared->locked_by)) \
      g_pIDebug->IDebugPrintf ("locked an already locked lock by %x\n", m_Shared->locked_by); \
    m_Shared->locked_by = 3; \
  }

#define UNLOCK_ATOMIC \
  if (m_bUserspaceAQ) \
  { \
    if (UNLIKELY (m_Shared->locked_by != 3)) \
      g_pIDebug->IDebugPrintf ("m_Shared->locked_by error (%x)!\n", m_Shared->locked_by); \
    m_Shared->locked_by = 0; \
    g_pIOS->Barrier (); \
    g_pIOS->AtomicSet ((ATOMIC *) &m_Shared->lock, 0); \
  }


#define SetMatrix(dst, src) \
  ( { \
    dst[0] = src[0]; \
    dst[1] = src[1]; \
    dst[2] = src[2]; \
    dst[3] = src[3]; \
  } )

CSTmBDispAQ::CSTmBDispAQ(ULONG                   *pBlitterBaseAddr,
                         ULONG                    AQOffset,
                         int                      qNumber,
                         int                      qPriority,
                         ULONG                    drawOps,
                         ULONG                    copyOps,
                         stm_blitter_device_priv  eDevice,
                         ULONG                    nLineBufferLength): CSTmBlitter(pBlitterBaseAddr)
{
  m_pAQReg         = pBlitterBaseAddr + (AQOffset >>2);
  m_nQueueID       = qNumber - 1;
  m_nQueuePriority = qPriority;

  m_Shared = 0;

  /*
   * Set default available operations. These may be altered for specific chip
   * instantiations of the BDisp IP.
   */
  m_drawOps = drawOps;
  m_copyOps = copyOps;

  m_eDevice = eDevice;

  m_lineBufferLength = nLineBufferLength;

#ifdef DEBUG
  m_LastPlanarformat = SURF_NULL_PAD;
#endif
}


bool CSTmBDispAQ::Create()
{
  DEBUGF2(2, ("%s()\n", __PRETTY_FUNCTION__));

  if(m_nQueueID >= STM_BDISP_MAX_AQs)
    return false;

  if(!CSTmBlitter::Create())
    return false;

  m_bUserspaceAQ = false;

  m_lnaInt  = BDISP_ITS_AQ_LNA_REACHED << (BDISP_ITS_AQ1_SHIFT+(m_nQueueID*4));
  m_nodeInt = BDISP_ITS_AQ_NODE_NOTIFY << (BDISP_ITS_AQ1_SHIFT+(m_nQueueID*4));

  /* for userspace BDisp, we need an area of memory shared with userspace,
     so we can do efficient interrupt handling. It shall be aligned to a
     page boundary, and size shall be an integer multiple of PAGE_SIZE,
     too so the mmap() implementation is easier. Since our shared area
     consists of the actual shared area (shared between IRQ handler and
     userspace) and the nodelist for the blitter, we want to make sure
     it is possible to mmap() the nodelist _un_cached and the shared
     struct _cached_. Therefore, the struct will occupy one complete
     page. */
  ULONG area_size = (_ALIGN_UP (sizeof (STMFBBDispSharedAreaPriv),
                                g_pIOS->OSPageSize)
                     + NODELISTPAGES * g_pIOS->OSPageSize);
  g_pIOS->AllocateDMAArea (&m_SharedAreaDMA,
                           area_size, g_pIOS->OSPageSize, SDAAF_NONE);
  if (!m_SharedAreaDMA.pMemory)
    {
      DEBUGF2(1, ("%s @ %p: failed to allocate BDisp nodelist\n",
                  __PRETTY_FUNCTION__, this));
      return false;
    }
  g_pIOS->MemsetDMAArea (&m_SharedAreaDMA, 0, 0xab, area_size);

  /* initialize shared area */
  m_Shared = reinterpret_cast <STMFBBDispSharedAreaPriv *> (m_SharedAreaDMA.pData);
  m_Shared->version = 2;
  m_Shared->aq_index   = m_nQueueID;
  m_Shared->nodes_size = NODELISTPAGES * g_pIOS->OSPageSize;
  m_Shared->nodes_phys = (m_SharedAreaDMA.ulPhysical
                          + _ALIGN_UP (sizeof (STMFBBDispSharedAreaPriv),
                                       g_pIOS->OSPageSize));

  /* important stuff */
  m_Shared->bdisp_running = m_Shared->updating_lna = 0;
  m_Shared->prev_set_lna = m_Shared->last_lut = 0;
  m_Shared->device = m_eDevice;
  g_pIOS->AtomicSet ((ATOMIC *) &m_Shared->lock, 0);
  m_Shared->locked_by = 0;
  /* stats only */
  m_Shared->num_idle = m_Shared->num_irqs = m_Shared->num_lna_irqs
    = m_Shared->num_node_irqs = m_Shared->num_ops_hi = m_Shared->num_ops_lo
    = m_Shared->num_starts
    = m_Shared->num_wait_idle = m_Shared->num_wait_next
    = 0;

  blitload_init_ticks ();

  DEBUGF2(2, ("%s @ %p: shared area: v/p/s: %p/%.8lx/%lu, nodearea: p/s: %.8lx/%lu\n",
              __PRETTY_FUNCTION__, this,
              m_SharedAreaDMA.pData, m_SharedAreaDMA.ulPhysical,
              m_SharedAreaDMA.ulDataSize, m_Shared->nodes_phys,
              m_Shared->nodes_size));


  m_ulBufferNodes = m_Shared->nodes_size / BLIT_NODE_SIZE;
  m_NodeOffset    = m_Shared->nodes_phys - m_SharedAreaDMA.ulPhysical;

  /*
   * Prelink all the buffer nodes around to make a circular list
   */
  stm_bdisp_aq_fast_blit_node tmp = {BLT_NIP: 0,
                                     BLT_CIC: BLIT_FAST_NODE_CIC,
                                     BLT_INS: BLIT_INS_SRC1_MODE_DISABLED};

  tmp.BLT_NIP = m_Shared->nodes_phys + BLIT_NODE_SIZE;
  for (unsigned int i = 0; i < (m_ulBufferNodes - 1); ++i)
  {
    g_pIOS->MemcpyToDMAArea (&m_SharedAreaDMA, m_NodeOffset + (i * BLIT_NODE_SIZE),
                             &tmp, sizeof (stm_bdisp_aq_fast_blit_node));
    tmp.BLT_NIP += BLIT_NODE_SIZE;
  }

  /*
   * Do the last node, pointing back to the first.
   */
  tmp.BLT_NIP = m_Shared->nodes_phys;
  g_pIOS->MemcpyToDMAArea (&m_SharedAreaDMA,
                           m_NodeOffset + ((m_ulBufferNodes - 1) * BLIT_NODE_SIZE),
                           &tmp, sizeof (stm_bdisp_aq_fast_blit_node));


  m_Shared->next_free = 0;
  m_Shared->last_free = (m_ulBufferNodes - 1) * BLIT_NODE_SIZE;

  WriteAQReg(BDISP_AQ_CTL, m_nQueuePriority | (BDISP_AQ_CTL_QUEUE_EN           |
                                               BDISP_AQ_CTL_EVENT_SUSPEND      |
                                               BDISP_AQ_CTL_NODE_COMPLETED_INT |
                                               BDISP_AQ_CTL_LNA_INT));

  /*
   * Note that no blits should start until the LNA register is written.
   */
  DEBUGF2(2, ("%s() blit node base address = %#08lx\n",__PRETTY_FUNCTION__,m_Shared->nodes_phys));
  WriteAQReg(BDISP_AQ_IP, m_Shared->nodes_phys);

  return true;
}


CSTmBDispAQ::~CSTmBDispAQ()
{
  DEBUGF2(2, ("%s() called.\n", __PRETTY_FUNCTION__));

  WriteAQReg(BDISP_AQ_CTL, 0);

  if (m_Shared)
    {
      g_pIDebug->IDebugPrintf ("%s @ %p: %u starts, %u (%u/%u) interrupts, %u wait_idle, %u wait_next, %u idle\n",
                               __PRETTY_FUNCTION__, this,
                               m_Shared->num_starts, m_Shared->num_irqs, m_Shared->num_node_irqs,
                               m_Shared->num_lna_irqs,
                               m_Shared->num_wait_idle, m_Shared->num_wait_next, m_Shared->num_idle);

      ULONGLONG ops = ((ULONGLONG) (m_Shared->num_ops_hi)) << 32;
      ops += m_Shared->num_ops_lo;
      g_pIDebug->IDebugPrintf ("%s @ %p: %llu ops, %llu ops/start, %llu ops/idle, %u starts/idle\n",
                               __PRETTY_FUNCTION__, this,
                               ops,
                               m_Shared->num_starts ? ops / m_Shared->num_starts : 0,
                               m_Shared->num_idle   ? ops / m_Shared->num_idle   : 0,
                               m_Shared->num_idle   ? m_Shared->num_starts / m_Shared->num_idle : 0);
    }

  g_pIOS->FreeDMAArea(&m_SharedAreaDMA);

  DEBUGF2(2, ("%s() completed.\n", __PRETTY_FUNCTION__));
}


/*
 * This function commits the node offset at m_Shared->next_free to the
 * blitter for execution.
 */
void CSTmBDispAQ::CommitBlitOperation(void)
{
  unsigned long new_last_valid = m_Shared->nodes_phys + m_Shared->next_free;

  DEBUGF2(3,("%s node offset = 0x%08lx\n",__PRETTY_FUNCTION__,m_Shared->next_free));

  /* the following code serves two purposes - a) we need a memory barrier,
     which is accomplished by reading back the memory just written to, and
     b) we need the value read back anyway, to update next_free. */
  /*
   * Try a memory barrier here, to ensure the blitnode has made it to
   * memory, before we reprogram the hardware and it reads the node.
   * This requires that the caller has purged the cache on this node's
   * memory before calling this function.
   */
  volatile struct stm_bdisp_aq_fast_blit_node * const node
    = (volatile struct stm_bdisp_aq_fast_blit_node *)(m_SharedAreaDMA.pData
                                                      + m_NodeOffset
                                                      + m_Shared->next_free);
  /*
   * Force cache refill of first word of node and hope the compiler doesn't
   * optimise it out.
   */
  unsigned long nip = node->BLT_NIP;

  DEBUGF2(3,("%s nip = 0x%08lx\n",__PRETTY_FUNCTION__,nip));

  if (UNLIKELY (! ++m_Shared->num_ops_lo))
    ++m_Shared->num_ops_hi;

  /* signal to irq handler that we are in the process of updating
     BDISP_AQ_LNA and want bdisp_running to be left untouched, which is
     important in case we are interrupted between setting bdisp_running
     = true and the actual writing to the register. */
  m_Shared->updating_lna = true;
  m_Shared->prev_set_lna = new_last_valid;

  /* Make sure updating_lna and prev_set_lna are set before
     bdisp_running! */
  g_pIOS->Barrier ();

  blitload_commit_blit_operation ();

  m_Shared->bdisp_running = true;

  /* The idea of this second barrier here is to make sure bdisp_running is
     set before the register, just to make sure the IRQ is not fired for
     this new command while bdisp_running gets set to true only later,
     because of instruction scheduling. The probability of such a
     situation happening should be around zero, but what the heck... */
  g_pIOS->Barrier ();

  /* kick */
  WriteAQReg(BDISP_AQ_LNA, new_last_valid);

  /* we're done */
  m_Shared->updating_lna = false;

  ++m_Shared->num_starts;

  m_Shared->next_free = nip - m_Shared->nodes_phys;
}


bool CSTmBDispAQ::ALUOperation(stm_bdisp_aq_fast_blit_node *pBlitNode, const stm_blitter_operation_t &op)
{
  unsigned long globalAlpha = 128; // Default fully opaque value

  if ((op.ulFlags & STM_BLITTER_FLAGS_XOR) &&
      (op.ulFlags & (STM_BLITTER_FLAGS_BLEND_SRC_ALPHA | STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT)))
  {
    DEBUGF2(1, ("%s XOR and Blend is mutually exclusive\n",__PRETTY_FUNCTION__));
    return false;
  }

  if(op.ulFlags & STM_BLITTER_FLAGS_GLOBAL_ALPHA)
  {
    DEBUGF2(2, ("%s setting global alpha 0x%lx\n",__PRETTY_FUNCTION__,op.ulGlobalAlpha));
    globalAlpha = (op.ulGlobalAlpha+1) / 2; // Scale 0-255 to 0-128 (note not 127!)
  }

  /*
   * Note the the following blending options are mutually exclusive and
   * we do not write the global alpha into ACK unless we are really
   * blending as we may be using a logical ROP to support destination
   * colour keying, which appears to need the ALU in operation now.
   */
  if (op.ulFlags & STM_BLITTER_FLAGS_BLEND_SRC_ALPHA)
  {
    /*
     * RGB of SRC2 is blended with SRC1 based on SRC2 Alpha * Global Alpha
     */
    DEBUGF2(2, ("%s setting src as NOT pre-multiplied\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_ACK &= ~(BLIT_ACK_MODE_MASK | BLIT_ACK_GLOBAL_ALPHA_MASK);
    pBlitNode->BLT_ACK |= ((globalAlpha & 0xff) << 8) | BLIT_ACK_BLEND_SRC2_N_PREMULT;
  }
  else if (op.ulFlags & STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT)
  {
    /*
     * RGB of SRC2 is blended with SRC1 based on Global Alpha, the SRC2 RGB
     * is assumed to have been pre-multiplied by SRC2 Alpha.
     */
    DEBUGF2(2, ("%s setting src as pre-multiplied\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_ACK &= ~(BLIT_ACK_MODE_MASK | BLIT_ACK_GLOBAL_ALPHA_MASK);
    pBlitNode->BLT_ACK |= ((globalAlpha & 0xff) << 8) | BLIT_ACK_BLEND_SRC2_PREMULT;
  }
  else if (op.ulFlags & STM_BLITTER_FLAGS_XOR)
  {
    DEBUGF2(2, ("%s XOR\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_ACK &= ~(BLIT_ACK_MODE_MASK | BLIT_ACK_ROP_MASK);
    pBlitNode->BLT_ACK |= (BLIT_ACK_ROP_XOR << BLIT_ACK_ROP_SHIFT) | BLIT_ACK_ROP;
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
  }

  if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_COLOR)
  {
    DEBUGF2(2, ("%s setting dst colour\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_COLOR_FILL;
    pBlitNode->BLT_S1CF = op.ulColour;
  }
  else if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_ZERO)
  {
    DEBUGF2(2, ("%s setting dst colour zero\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_COLOR_FILL;
    pBlitNode->BLT_S1CF = 0;
  }
  else if(op.ulFlags & STM_BLITTER_FLAGS_BLEND_DST_MEMORY)
  {
    DEBUGF2(2, ("%s setting dst memory\n",__PRETTY_FUNCTION__));
    pBlitNode->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNode->BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
  }

  return true;
}


static const ULONG ycbcr601_2_rgb[4]      = { 0x2c440000, 0xe9a403aa, 0x0004013f, 0x34f21322 };
static const ULONG ycbcr601_2_rgb_full[4] = { 0x3324a800, 0xe604ab9c, 0x0004a957, 0x32121eeb };
static const ULONG rgb_2_ycbcr601[4]      = { 0x107e4beb, 0x0982581d, 0xfa9ea483, 0x08000080 };
static const ULONG rgb_full_2_ycbcr601[4] = { 0x0e1e8bee, 0x08420419, 0xfb5ed471, 0x08004080 };

static const ULONG ycbcr709_2_rgb[4]      = { 0x31440000, 0xf16403d1, 0x00040145, 0x33b14b18 };
static const ULONG ycbcr709_2_rgb_full[4] = { 0x3964a800, 0xef04abc9, 0x0004a95f, 0x307132df };
static const ULONG rgb_2_ycbcr709[4]      = { 0x107e27f4, 0x06e2dc13, 0xfc5e6c83, 0x08000080 };
static const ULONG rgb_full_2_ycbcr709[4] = { 0x0e3e6bf5, 0x05e27410, 0xfcdea471, 0x08004080 };

static const ULONG ycbcr601_2_ycbcr709[4] = { 0x20c00013, 0xf94403e2, 0x03a00102, 0x3f30abef };

/* FIXME ycbcr709_2_ycbcr601 values are wrong and need the correct values */
static const ULONG ycbcr709_2_ycbcr601[4] = { 0x20c00013, 0xf94403e2, 0x03a00102, 0x3f30abef };


bool CSTmBDispAQ::MatrixConversions(stm_bdisp_aq_fast_blit_node   *pBlitNodeCommon,
                                    const stm_blitter_operation_t &op,
                                    ULONG                          s2ty,
                                    ULONG                         *ivmx,
                                    ULONG                         *ovmx,
                                    ULONG                          cicupdate)
{
  ULONG targettype = pBlitNodeCommon->BLT_TTY & BLIT_COLOR_FORM_TYPE_MASK;
  ULONG src2type   = s2ty & BLIT_COLOR_FORM_TYPE_MASK;

  if((src2type == BLIT_COLOR_FORM_IS_MISC) ||
     (op.ulFlags & (STM_BLITTER_FLAGS_PREMULT_COLOUR_ALPHA | STM_BLITTER_FLAGS_COLORIZE)))
  {
    if((op.ulFlags & (STM_BLITTER_FLAGS_BLEND_DST_COLOR | STM_BLITTER_FLAGS_COLORIZE)) == (STM_BLITTER_FLAGS_BLEND_DST_COLOR | STM_BLITTER_FLAGS_COLORIZE))
    {
      /*
       * We have requested a blend with a destination colour and a colour
       * modulation, but only have one op.ulColour!
       */
      return false;
    }

    /*
     * These two options are RGB source modulations so fail if the source
     * is YUV.
     */
    if(src2type == BLIT_COLOR_FORM_IS_YUV)
      return false;

    /*
     * We can still do this if the target is YUV, by doing the colourspace
     * conversion in the output matrix, but only if the ALU is not being
     * used to blend or xor source and destination. That needs the conversion
     * to be done in the input matrix, but we want to use that matrix to do
     * the RGB modulation, we can't do both!
     */
    if(targettype == BLIT_COLOR_FORM_IS_YUV)
    {
      if(op.ulFlags & (STM_BLITTER_FLAGS_BLEND_SRC_ALPHA         |
                       STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT |
                       STM_BLITTER_FLAGS_XOR))
      {
        return false;
      }

      DEBUGF2(2, ("%s Output RGB->YUV\n",__PRETTY_FUNCTION__));

      SetMatrix (ovmx, rgb_2_ycbcr601);

      pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_OCSC;
      pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_IVMX_CIC | BLIT_OVMX_CIC);
    }

    ULONG red   = 256;
    ULONG green = 256;
    ULONG blue  = 256;

    if(op.ulFlags & STM_BLITTER_FLAGS_COLORIZE)
    {
      DEBUGF2(2, ("%s Colorize col = 0x%lx\n",__PRETTY_FUNCTION__,op.ulColour));
      red   = ((((op.ulColour & 0x00FF0000) >> 16)) * 256) / 255;
      green = ((((op.ulColour & 0x0000FF00) >> 8))  * 256) / 255;
      blue  = (  (op.ulColour & 0x000000FF)         * 256) / 255;
    }

    if(op.ulFlags & STM_BLITTER_FLAGS_PREMULT_COLOUR_ALPHA)
    {
      ULONG alpha = (op.ulGlobalAlpha * 256 ) /255;

      DEBUGF2(2, ("%s Premult Colour Alpha a = %lu\n", __PRETTY_FUNCTION__, alpha));

      red   = (red   * alpha)/256;
      green = (green * alpha)/256;
      blue  = (blue  * alpha)/256;
    }


    if(src2type != BLIT_COLOR_FORM_IS_MISC)
    {
      /*
       * RGB surface types, do a real modulation
       */
      DEBUGF2(2, ("%s Input RGB modulation (%lu,%lu,%lu)\n",__PRETTY_FUNCTION__,red,green,blue));

      ivmx[0] = red   << 21;
      ivmx[1] = green << 10;
      ivmx[2] = blue;
      ivmx[3] = 0;
    }
    else
    {
      /*
       * A8 and A1 surface types, we need to add the calculated colour as
       * the hardware always expands to black with alpha (what use is that?!)
       */
      DEBUGF2(2, ("%s Input Ax RGB replacement (%lu,%lu,%lu)\n",__PRETTY_FUNCTION__,red,green,blue));

      ivmx[0] = 0;
      ivmx[1] = 0;
      ivmx[2] = 0;
      ivmx[3] = (red << 20) | (green << 10) | blue;
    }

    pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_ICSC;
    pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_IVMX_CIC);

  }
  else
  {
    if(targettype == BLIT_COLOR_FORM_IS_YUV && src2type != BLIT_COLOR_FORM_IS_YUV)
    {
      DEBUGF2(2, ("%s Input RGB->YUV\n",__PRETTY_FUNCTION__));

      SetMatrix (ivmx, rgb_2_ycbcr601);

      pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_ICSC;
      pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_IVMX_CIC);
    }
    else if (targettype != BLIT_COLOR_FORM_IS_YUV && src2type == BLIT_COLOR_FORM_IS_YUV)
    {
      DEBUGF2(2, ("%s Input YUV->RGB\n",__PRETTY_FUNCTION__));

      SetMatrix (ivmx, ycbcr601_2_rgb);

      pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_ICSC;
      pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_IVMX_CIC);
    }
  }

  return true;
}


bool CSTmBDispAQ::SetPlaneMask(stm_bdisp_aq_fast_blit_node   *pBlitNodeCommon,
                               const stm_blitter_operation_t &op,
                               ULONG                         &pmk,
                               ULONG                          cicupdate)
{
  if(op.ulFlags & STM_BLITTER_FLAGS_PLANE_MASK)
  {
    if (op.ulFlags & (STM_BLITTER_FLAGS_BLEND_DST_COLOR | STM_BLITTER_FLAGS_BLEND_DST_ZERO))
    {
      DEBUGF2(2, ("%s cannot use plane masking and blend against a solid colour\n",__PRETTY_FUNCTION__));
      return false;
    }

    pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_PMK_CIC);
    pBlitNodeCommon->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_PLANEMASK | BLIT_INS_SRC1_MODE_MEMORY;
    pmk = op.ulPlanemask;
  }

  return true;
}


bool CSTmBDispAQ::ColourKey(stm_bdisp_aq_fast_blit_node   *pBlitNodeCommon,
                            const stm_blitter_operation_t &op,
                            ULONG                         *keys,
                            ULONG                          cicupdate)
{
  if (op.ulFlags & STM_BLITTER_FLAGS_SRC_COLOR_KEY)
  {
    DEBUGF2(2, ("%s setting src keying\n",__PRETTY_FUNCTION__));
    pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_KEYS_CIC);
    pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_COLOURKEY;
    pBlitNodeCommon->BLT_ACK |= BLIT_ACK_RGB_ENABLE | BLIT_ACK_SRC_COLOUR_KEYING;
    SetKeyColour(op.srcSurface, op.ulColorKeyLow,  &keys[0]);
    SetKeyColour(op.srcSurface, op.ulColorKeyHigh, &keys[1]);
  }

  if (op.ulFlags & STM_BLITTER_FLAGS_DST_COLOR_KEY)
  {
    if (op.ulFlags & (STM_BLITTER_FLAGS_BLEND_DST_COLOR | STM_BLITTER_FLAGS_BLEND_DST_ZERO))
    {
      DEBUGF2(2, ("%s cannot destination colour key and blend against a solid colour\n",__PRETTY_FUNCTION__));
      return false;
    }

    DEBUGF2(2, ("%s setting dst keying\n",__PRETTY_FUNCTION__));
    pBlitNodeCommon->BLT_CIC |= (cicupdate | BLIT_KEYS_CIC);
    pBlitNodeCommon->BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;
    pBlitNodeCommon->BLT_INS |= BLIT_INS_ENABLE_COLOURKEY | BLIT_INS_SRC1_MODE_MEMORY;

    if (op.ulFlags & (STM_BLITTER_FLAGS_BLEND_DST_MEMORY))
    {
      pBlitNodeCommon->BLT_ACK |= BLIT_ACK_RGB_ENABLE | BLIT_ACK_DEST_KEY_ZEROS_SRC_ALPHA;
    }
    else
    {
      pBlitNodeCommon->BLT_ACK &= ~(BLIT_ACK_MODE_MASK | BLIT_ACK_ROP_MASK);
      pBlitNodeCommon->BLT_ACK |= (BLIT_ACK_ROP                             |
                                  (BLIT_ACK_ROP_COPY << BLIT_ACK_ROP_SHIFT) |
                                   BLIT_ACK_RGB_ENABLE                      |
                                   BLIT_ACK_DEST_COLOUR_KEYING);
    }

    SetKeyColour(op.dstSurface, op.ulColorKeyLow,  &keys[0]);
    SetKeyColour(op.dstSurface, op.ulColorKeyHigh, &keys[1]);
  }

  return true;
}


void *
CSTmBDispAQ::GetNewNode(void)
{
  LOCK_ATOMIC;

  if(m_Shared->last_free == m_Shared->next_free)
  {
    /* doesn't need to be atomic, it's for statistics only anyway... */
    m_Shared->num_wait_next++;

    g_pIOS->SleepOnQueue(BLIT_NODE_QUEUE,
                         &m_Shared->last_free,m_Shared->next_free,
                         STMIOS_WAIT_COND_NOT_EQUAL);
  }

  return m_SharedAreaDMA.pData + m_NodeOffset + m_Shared->next_free;
}


bool CSTmBDispAQ::FillRect(const stm_blitter_operation_t &op, const stm_rect_t &dst)
{
  bool retval = false;

  DEBUGF2(2, ("%s @ %p: colour %.8lx l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              op.ulColour, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left == dst.right) && (dst.top == dst.bottom))
    return true;

  if (op.ulFlags & ~m_drawOps)
  {
    DEBUGF2(1, ("%s operation flags not handled 0x%lx\n",__PRETTY_FUNCTION__,op.ulFlags));
    return false;
  }

  stm_bdisp_aq_fullfill_node * const pBlitNode
    = static_cast<stm_bdisp_aq_fullfill_node *>(GetNewNode());

  if(!SetBufferType(op.dstSurface,&(pBlitNode->common.BLT_TTY)))
  {
    DEBUGF2(1, ("%s invalid destination type\n",__PRETTY_FUNCTION__));
    goto out;
  }

  pBlitNode->common.BLT_TBA = op.dstSurface.ulMemory;

  SetXY(dst.left, dst.top, &(pBlitNode->common.BLT_TXY));

  pBlitNode->common.BLT_TSZ  = (((dst.bottom - dst.top) & 0x0FFF) << 16) | ((dst.right - dst.left) & 0x0FFF);
  pBlitNode->common.BLT_S1XY = pBlitNode->common.BLT_TXY;

  if (op.ulFlags == STM_BLITTER_FLAGS_NONE
      && (op.colourFormat == op.dstSurface.format))
  {
    /*
     * This is a simple solid fill that can use the 64bit fast path
     * in the blitter. This supports the output of multiple pixels per blitter
     * clock cycle.
     */
    DEBUGF2(2, ("%s fast direct fill\n",__PRETTY_FUNCTION__));
    pBlitNode->common.BLT_S1TY = pBlitNode->common.BLT_TTY & ~BLIT_TY_BIG_ENDIAN;
    pBlitNode->common.BLT_S1CF = op.ulColour;
    pBlitNode->common.BLT_CIC  = BLIT_FAST_NODE_CIC;
    pBlitNode->common.BLT_INS  = BLIT_INS_SRC1_MODE_DIRECT_FILL;
  }
  else
  {
    /*
     * This is a fill which needs to use the source 2 data path for the
     * fill colour. This path is limited to one pixel output per blitter
     * clock cycle.
     */
    stm_blitter_surface_t tmpSurf = {{0}};
    tmpSurf.format = op.colourFormat;

    DEBUGF2(2, ("%s complex fill\n",__PRETTY_FUNCTION__));
    if(!SetBufferType(tmpSurf,&(pBlitNode->BLT_S2TY)))
    {
      DEBUGF2(1, ("%s invalid source colour format\n",__PRETTY_FUNCTION__));
      goto out;
    }

    pBlitNode->BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;
    pBlitNode->BLT_S2XY  = pBlitNode->common.BLT_TXY;
    pBlitNode->BLT_S2SZ  = pBlitNode->common.BLT_TSZ;

    pBlitNode->common.BLT_S2CF = op.ulColour;

    pBlitNode->common.BLT_S1TY = pBlitNode->common.BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;
    pBlitNode->common.BLT_S1BA = pBlitNode->common.BLT_TBA;

    pBlitNode->common.BLT_CIC = BLIT_FULLFILL_CIC & ~(BLIT_PMK_CIC | BLIT_KEYS_CIC | BLIT_IVMX_CIC | BLIT_OVMX_CIC);
    pBlitNode->common.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
    pBlitNode->common.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;

    if(!ALUOperation(&pBlitNode->common, op))
      goto out;

    if(!ColourKey(&pBlitNode->common, op, pBlitNode->BLT_KEYS, BLIT_PMK_CIC)) /* FIXME: BLIT_PMK_CIC seems like a bug?! */
      goto out;

    if(!SetPlaneMask(&pBlitNode->common, op, pBlitNode->BLT_PMK, 0))
      goto out;

    if(!MatrixConversions(&pBlitNode->common, op, pBlitNode->BLT_S2TY, pBlitNode->BLT_IVMX, pBlitNode->BLT_OVMX, (BLIT_PMK_CIC|BLIT_KEYS_CIC)))
      goto out;
  }

  if (!(m_SharedAreaDMA.ulFlags & SDAAF_UNCACHED))
    g_pIOS->FlushCache(pBlitNode, sizeof(stm_bdisp_aq_fullfill_node));
  CommitBlitOperation();

  retval = true;

out:
  UNLOCK_ATOMIC;

  return retval;
}


bool CSTmBDispAQ::DrawRect(const stm_blitter_operation_t &op, const stm_rect_t &dst)
{
  int nodesize;

  DEBUGF2(2, ("%s @ %p: colour %.8lx l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              op.ulColour, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left != dst.right) && (dst.top != dst.bottom))
  {
    stm_bdisp_aq_fullfill_node blitNodes[4];
    int i;

    if (op.ulFlags & ~m_drawOps)
    {
      DEBUGF2(1, ("%s operation flags not handled 0x%lx\n",__PRETTY_FUNCTION__,op.ulFlags));
      return false;
    }

    g_pIOS->ZeroMemory(&(blitNodes[0]), sizeof(blitNodes[0]));

    if(!SetBufferType(op.dstSurface,&(blitNodes[0].common.BLT_TTY)))
    {
      DEBUGF2(1, ("%s invalid destination type\n",__PRETTY_FUNCTION__));
      return false;
    }

    blitNodes[0].common.BLT_TBA = op.dstSurface.ulMemory;

    // First set up the control fields in the first blit node for the operation
    if (op.ulFlags == STM_BLITTER_FLAGS_NONE && (op.colourFormat == op.dstSurface.format))
    {
      // Simple solid rectangle outline.
      blitNodes[0].common.BLT_CIC  = BLIT_FAST_NODE_CIC;
      blitNodes[0].common.BLT_S1TY = blitNodes[0].common.BLT_TTY & ~BLIT_TY_BIG_ENDIAN;
      blitNodes[0].common.BLT_S1CF = op.ulColour;
      blitNodes[0].common.BLT_INS  = BLIT_INS_SRC1_MODE_DIRECT_FILL;
      nodesize = sizeof(stm_bdisp_aq_fast_blit_node);
    }
    else
    {
      /*
       * Operations that require the destination in SRC1 e.g. blending
       * and colourkeying, or colour conversion when drawing to a YUV
       * surface.
       */
      stm_blitter_surface_t tmpSurf = {{0}};

      tmpSurf.format = op.colourFormat;

      if(!SetBufferType(tmpSurf,&(blitNodes[0].BLT_S2TY)))
      {
        DEBUGF2(1, ("%s invalid source colour type\n",__PRETTY_FUNCTION__));
        return false;
      }

      blitNodes[0].BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;

      blitNodes[0].common.BLT_S1TY = blitNodes[0].common.BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;
      blitNodes[0].common.BLT_S1BA = blitNodes[0].common.BLT_TBA;

      blitNodes[0].common.BLT_S2CF = op.ulColour;

      /*
       * Setup default node instruction and ALU operations
       */
      blitNodes[0].common.BLT_CIC = BLIT_FULLFILL_CIC & ~(BLIT_PMK_CIC | BLIT_KEYS_CIC | BLIT_IVMX_CIC | BLIT_OVMX_CIC);
      blitNodes[0].common.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
      blitNodes[0].common.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;

      if(!ALUOperation(&blitNodes[0].common, op))
        return false;

      if(!ColourKey(&blitNodes[0].common, op, blitNodes[0].BLT_KEYS, BLIT_PMK_CIC)) /* FIXME: BLIT_PMK_CIC seems like a bug?!*/
        return false;

      if(!SetPlaneMask(&blitNodes[0].common, op, blitNodes[0].BLT_PMK, 0))
        return false;

      if(!MatrixConversions(&blitNodes[0].common, op, blitNodes[0].BLT_S2TY, blitNodes[0].BLT_IVMX, blitNodes[0].BLT_OVMX, (BLIT_PMK_CIC | BLIT_KEYS_CIC)))
        return false;

      nodesize = sizeof(stm_bdisp_aq_fullfill_node);
    }

    // Copy the first node as a template for the other three
    for(i = 1; i < 4; i++)
    {
      blitNodes[i] = blitNodes[0];
    }

    // Now set up the coordinates and sizes for the surface
    blitNodes[0].common.BLT_TXY = blitNodes[0].common.BLT_S1XY = dst.left        | (dst.top          << 16);
    blitNodes[1].common.BLT_TXY = blitNodes[1].common.BLT_S1XY = (dst.right - 1) | ((dst.top + 1)    << 16);
    blitNodes[2].common.BLT_TXY = blitNodes[2].common.BLT_S1XY = dst.left        | ((dst.bottom - 1) << 16);
    blitNodes[3].common.BLT_TXY = blitNodes[3].common.BLT_S1XY = dst.left        | ((dst.top + 1)    << 16);

    blitNodes[0].common.BLT_TSZ = (0x0001 << 16) | ((dst.right - dst.left) & 0x0FFF);
    blitNodes[1].common.BLT_TSZ = ((((dst.bottom - dst.top) - 2) & 0x0FFF) << 16) | (0x0001);
    blitNodes[2].common.BLT_TSZ = (0x0001 << 16) | ((dst.right - dst.left) & 0x0FFF);
    blitNodes[3].common.BLT_TSZ = ((((dst.bottom - dst.top) - 2) & 0x0FFF) << 16) | (0x0001);

    /*
     * Now make the the source2 x,y and size the same as target otherwise
     * you crash the blitter even when source2 is a solid colour.
     */
    for(i = 0; i < 4; i++)
    {
      blitNodes[i].BLT_S2XY = blitNodes[i].common.BLT_TXY;
      blitNodes[i].BLT_S2SZ = blitNodes[i].common.BLT_TSZ;
    }

    LOCK_ATOMIC;

    QueueBlitOperation_locked(&blitNodes[0], nodesize);
    QueueBlitOperation_locked(&blitNodes[1], nodesize);
    QueueBlitOperation_locked(&blitNodes[2], nodesize);
    QueueBlitOperation_locked(&blitNodes[3], nodesize);

    UNLOCK_ATOMIC;

    DEBUGF2(2, ("%s completed.\n",__PRETTY_FUNCTION__));
  }

  return true;
}


bool CSTmBDispAQ::CopyRect(const stm_blitter_operation_t &op,
                           const stm_rect_t              &dst,
                           const stm_point_t             &src)
{
  bool copyDirReversed;
  bool retval = false;

  DEBUGF2(2, ("%s @ %p: src x/y: %lu/%lu dst l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              src.x, src.y, dst.left, dst.right, dst.top, dst.bottom));

  if ((dst.left == dst.right) && (dst.top == dst.bottom))
    return false;

  stm_bdisp_aq_fast_blit_node * const pBlitNode
    = static_cast<stm_bdisp_aq_fast_blit_node *>(GetNewNode());

  pBlitNode->BLT_CIC = BLIT_FAST_NODE_CIC;

  // Set the fill mode
  pBlitNode->BLT_INS = BLIT_INS_SRC1_MODE_DIRECT_COPY;

  // Set source type
  if(!SetBufferType(op.srcSurface,&(pBlitNode->BLT_S1TY)))
  {
    DEBUGF2(1, ("%s invalid source type\n",__PRETTY_FUNCTION__));
    goto out;
  }

  // Set target type
  if(!SetBufferType(op.dstSurface,&(pBlitNode->BLT_TTY)))
  {
    DEBUGF2(1, ("%s invalid destination type\n",__PRETTY_FUNCTION__));
    goto out;
  }

  /* there is some problem when doing a fast blit of that surface type, just
     change it to RGB16, doesn't matter... */
  if (UNLIKELY (op.dstSurface.format == SURF_YCBCR422R))
  {
    pBlitNode->BLT_TTY &= ~((0x1f << 16) | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
    pBlitNode->BLT_TTY |= BLIT_COLOR_FORM_RGB565;
    pBlitNode->BLT_S1TY &= ~((0x1f << 16) | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
    pBlitNode->BLT_S1TY |= BLIT_COLOR_FORM_RGB565;
  }

  // Work out copy direction
  copyDirReversed = ((op.srcSurface.ulMemory == op.dstSurface.ulMemory)
                     && ((dst.top > src.y)
                         || ((dst.top == src.y) && (dst.left > src.x)))
                    );

  if (!copyDirReversed)
  {
    SetXY(src.x, src.y, &(pBlitNode->BLT_S1XY));
    SetXY(dst.left, dst.top, &(pBlitNode->BLT_TXY));
  }
  else
  {
    pBlitNode->BLT_S1TY |= BLIT_TY_COPYDIR_BOTTOMRIGHT;
    pBlitNode->BLT_TTY  |= BLIT_TY_COPYDIR_BOTTOMRIGHT;

    SetXY(src.x + dst.right - (dst.left + 1),
          src.y + dst.bottom - (dst.top + 1), &(pBlitNode->BLT_S1XY));

    SetXY(dst.right - 1, dst.bottom - 1, &(pBlitNode->BLT_TXY));
  }

  // Set the source base address
  pBlitNode->BLT_S1BA = op.srcSurface.ulMemory;

  // Set the target base address
  pBlitNode->BLT_TBA = op.dstSurface.ulMemory;

  // Set the window dimensions
  pBlitNode->BLT_TSZ = (((dst.bottom - dst.top) & 0x0FFF) << 16) |
                        ((dst.right - dst.left) & 0x0FFF);

  if (!(m_SharedAreaDMA.ulFlags & SDAAF_UNCACHED))
    g_pIOS->FlushCache(pBlitNode, sizeof(stm_bdisp_aq_fast_blit_node));
  CommitBlitOperation();

  DEBUGF2(2, ("%s completed.\n",__PRETTY_FUNCTION__));

  retval = true;

out:
  UNLOCK_ATOMIC;

  return retval;
}


void CSTmBDispAQ::FilteringSetup (int                      hsrcinc,
                                  int                      vsrcinc,
                                  union _BltNodeGroup0910 &filter,
                                  ULONG                    h_init,
                                  ULONG                    v_init)
{
  struct _BDISP_GROUP_9 * __restrict flt = filter.c;
  int                    filterIndex;

  flt->BLT_HFP = 0;
  flt->BLT_VFP = 0;

  filterIndex = ChooseBlitter8x8FilterCoeffs (hsrcinc);
  if (filterIndex >= 0)
    flt->BLT_HFP = (m_8x8FilterTables.ulPhysical
                    + filterIndex * STM_BLIT_FILTER_TABLE_SIZE);

  filterIndex = ChooseBlitter5x8FilterCoeffs (vsrcinc);
  if (filterIndex >= 0)
    flt->BLT_VFP = (m_5x8FilterTables.ulPhysical
                    + filterIndex * STM_BLIT_FILTER_TABLE_SIZE);

  flt->BLT_RSF = (vsrcinc << 16) | (hsrcinc & 0x0000ffff);

  /*
   * Vertical filter setup: repeat the first line twice to pre-fill the
   * filter. This is _not_ the same setup as used for 5 Tap video filtering
   * which sets the filter phase to start at +1/2.
   */
  flt->BLT_RZI = 2 << BLIT_RZI_V_REPEAT_SHIFT;
  flt->BLT_RZI |= ((v_init & fractionMASK) << BLIT_RZI_V_INIT_SHIFT);

  /* Default horizontal filter setup, repeat the first pixel 3 times to
     pre-fill the 8 Tap filter. */
  flt->BLT_RZI |= (3 << BLIT_RZI_H_REPEAT_SHIFT);
  flt->BLT_RZI |= ((h_init & fractionMASK) << BLIT_RZI_H_INIT_SHIFT);
}

bool CSTmBDispAQ::CopyRectSetupHelper(const stm_blitter_operation_t &op,
                                      const stm_rect_t              &dst,
                                      const stm_rect_t              &src,
                                      stm_bdisp_aq_full_blit_node   *pBlitNode,
                                      bool                          &bRequiresSpans,
                                      bool                          &bCopyDirReversed)
{
  stm_point_t SrcPoint;

  /*
   * Setup a complex blit node, with the exception of XY and Size and
   * filter subposition.
   *
   * This forms a template node, potentially used to create multiple nodes,
   * when doing a rescale that requires the operation to be split up into
   * smaller spans.
   */
  SrcPoint.x = src.left;
  SrcPoint.y = src.top;

  DEBUGF2(2, ("%s @ %p: flags: %.8lx src l/r/t/b: %lu/%lu/%lu/%lu -> dst l/r/t/b: %lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this, op.ulFlags,
              src.left, src.right, src.top, src.bottom,
              dst.left, dst.right, dst.top, dst.bottom));

  if (op.ulFlags & ~m_copyOps)
    return false;

  bRequiresSpans = false;

  pBlitNode->common.BLT_CIC = BLIT_FULL_NODE_CIC & ~(BLIT_PMK_CIC     |
                                                     BLIT_RESCALE_CIC |
                                                     BLIT_FFILT_CIC   |
                                                     BLIT_KEYS_CIC    |
                                                     BLIT_IVMX_CIC    |
                                                     BLIT_OVMX_CIC);

  pBlitNode->common.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;
  pBlitNode->common.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_MEMORY;

  ULONG srcwidth  = (src.right  - src.left);
  ULONG srcheight = (src.bottom - src.top);
  ULONG dstwidth  = (dst.right  - dst.left);
  ULONG dstheight = (dst.bottom - dst.top);

  if (op.ulFlags & STM_BLITTER_FLAGS_FLICKERFILTER)
  {
    pBlitNode->common.BLT_INS |= (BLIT_INS_ENABLE_FLICKERFILTER |
                                  BLIT_INS_ENABLE_2DRESCALE);

    pBlitNode->common.BLT_CIC |= (BLIT_PMK_CIC | BLIT_RESCALE_CIC | BLIT_FFILT_CIC);

    // These magic numbers define a good filter
    pBlitNode->BLT_FF0 = stm_flicker_filter_coeffs[0];
    pBlitNode->BLT_FF1 = stm_flicker_filter_coeffs[1];
    pBlitNode->BLT_FF2 = stm_flicker_filter_coeffs[2];
    pBlitNode->BLT_FF3 = stm_flicker_filter_coeffs[3];

    /*
     * We need to turn on the whole filter/resize pipeline to use the
     * flicker filter, even if we are not resizing as well.
     */
    pBlitNode->BLT_F_RZC |= (BLIT_RZC_FF_MODE_ADAPTIVE         |
                             BLIT_RZC_2DHF_MODE_RESIZE_ONLY    |
                             BLIT_RZC_2DVF_MODE_RESIZE_ONLY);

    pBlitNode->BLT_RSF = 0x04000400; // 1:1 scaling by default
    pBlitNode->BLT_RZI = 0;

    /*
     * If the source width is >m_lineBufferLength pixels then we need to split
     * the operation up into <m_lineBufferLength pixel vertical spans. This is
     * because the filter pipeline only has line buffering for up to
     * m_lineBufferLength pixels.
     */
    bRequiresSpans = (srcwidth > m_lineBufferLength);
  }


  if (srcheight != dstheight || srcwidth != dstwidth)
  {
    /*
     * Note that the resize filter setup may override the default 1:1 resize
     * configuration set by the flicker filter; this is fine the vertical and
     * flicker filters can be active at the same time.
     */
    ULONG ulScaleh=0;
    ULONG ulScalew=0;

    bRequiresSpans = srcwidth>m_lineBufferLength;

    ulScaleh  =  ((srcheight * fixedpointONE) + fixedpointHALF) / dstheight;
    ulScalew  =  ((srcwidth  * fixedpointONE) + fixedpointHALF) / dstwidth;

    if ((ulScaleh > ((63 * fixedpointONE) + 0x3ff))
        || (ulScalew > ((63 * fixedpointONE) + 0x3ff))
        || (ulScaleh == 0)
        || (ulScalew == 0))
      return false;

    DEBUGF2(2, ("%s Scaleh = 0x%08lx Scalew = 0x%08lx.\n",__PRETTY_FUNCTION__,ulScaleh,ulScalew));

    /* filters */
    union _BltNodeGroup0910 filters;
    filters.c = reinterpret_cast <struct _BDISP_GROUP_9 *> (&pBlitNode->BLT_RSF);
    FilteringSetup (ulScalew, ulScaleh, filters, 0, 0);

    if (pBlitNode->BLT_HFP)
      pBlitNode->BLT_F_RZC |= BLIT_RZC_2DHF_MODE_FILTER_BOTH;
    else
    {
      DEBUGF2 (1, ("%s Error: No horizontal resize filter found.\n",
                   __PRETTY_FUNCTION__));
      pBlitNode->BLT_F_RZC |= BLIT_RZC_2DHF_MODE_RESIZE_ONLY;
    }

    if (pBlitNode->BLT_VFP)
      pBlitNode->BLT_F_RZC |= BLIT_RZC_2DVF_MODE_FILTER_BOTH;
    else
    {
      DEBUGF2 (1, ("%s Error: No vertical resize filter found.\n",
                   __PRETTY_FUNCTION__));
      pBlitNode->BLT_F_RZC |= BLIT_RZC_2DVF_MODE_RESIZE_ONLY;
    }

    pBlitNode->common.BLT_INS |= BLIT_INS_ENABLE_2DRESCALE;
    pBlitNode->common.BLT_CIC |= (BLIT_PMK_CIC | BLIT_RESCALE_CIC);
  }


  // Set source 2 type
  if(!SetBufferType(op.srcSurface,&(pBlitNode->BLT_S2TY)))
  {
    DEBUGF2(1, ("%s invalid source2 type\n",__PRETTY_FUNCTION__));
    return false;
  }

  pBlitNode->BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;
  pBlitNode->common.BLT_S2CF = 0xffffffff;

  // Set target type
  if(!SetBufferType(op.dstSurface,&(pBlitNode->common.BLT_TTY)))
  {
    DEBUGF2(1, ("%s invalid destination type\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * Set all the base addresses for both sources and destination.
   */
  pBlitNode->BLT_S2BA        = op.srcSurface.ulMemory;
  pBlitNode->common.BLT_TBA  = op.dstSurface.ulMemory;
  pBlitNode->common.BLT_S1BA = pBlitNode->common.BLT_TBA;

  /*
   * We set the CLUT operation after the source 2 type (which the CLUT operates
   * on) in order to determine if we are doing a colour lookup or an RGB
   * LUT correction (e.g. for display gamma).
   */
  if(op.ulFlags & STM_BLITTER_FLAGS_CLUT_ENABLE)
  {
    pBlitNode->BLT_CML = op.ulCLUT;

    SetCLUTOperation((pBlitNode->BLT_S2TY & BLIT_COLOR_FORM_TYPE_MASK),
                      pBlitNode->common.BLT_INS, pBlitNode->BLT_CCO);

    DEBUGF2(3, ("%s CLUT type = %#08lx cco = %#08lx cml = %#08lx\n",__PRETTY_FUNCTION__,(pBlitNode->BLT_S2TY & BLIT_COLOR_FORM_TYPE_MASK), pBlitNode->BLT_CCO, pBlitNode->BLT_CML));
  }

  if(!ALUOperation(&pBlitNode->common, op))
    return false;

  if(!ColourKey(&pBlitNode->common, op, pBlitNode->BLT_KEYS, (BLIT_PMK_CIC | BLIT_RESCALE_CIC | BLIT_FFILT_CIC))) /* FIXME: BLIT_PMK_CIC seems like a bug?! */
    return false;

  if(!SetPlaneMask(&pBlitNode->common, op, pBlitNode->BLT_PMK, 0))
    return false;

  /*
   * Rather like the clut this looks at the src2 and target type setup
   * to determine what colour space conversions are required.
   */
  if(!MatrixConversions(&pBlitNode->common, op, pBlitNode->BLT_S2TY,
                         pBlitNode->BLT_IVMX, pBlitNode->BLT_OVMX,
                        (BLIT_PMK_CIC | BLIT_RESCALE_CIC | BLIT_FFILT_CIC | BLIT_KEYS_CIC)))
  {
    return false;
  }

  // Work out copy direction
  if(op.srcSurface.ulMemory == op.dstSurface.ulMemory)
  {
    bCopyDirReversed = (dst.top > src.top) ||
                      ((dst.top == src.top) && (dst.left > src.left));

  }

  if(bCopyDirReversed)
  {
    pBlitNode->BLT_S2TY        |= BLIT_TY_COPYDIR_BOTTOMRIGHT;
    pBlitNode->common.BLT_TTY  |= BLIT_TY_COPYDIR_BOTTOMRIGHT;
  }

  /*
   * For operations needing the destination to be read we have to setup
   * SRC1 to mirror the setup of the destination, including the copy direction
   * just calculated.
   */
  pBlitNode->common.BLT_S1TY = pBlitNode->common.BLT_TTY | BLIT_TY_COLOR_EXPAND_MSB;

  /*
   * If we can do the operation in a single blit, fill in the remaining bits.
   */
  if(!bRequiresSpans)
  {
    if(!bCopyDirReversed)
    {
      SetXY(src.left, src.top, &(pBlitNode->BLT_S2XY));
      SetXY(dst.left, dst.top, &(pBlitNode->common.BLT_TXY));
    }
    else
    {
      SetXY(src.right - 1, src.bottom - 1, &(pBlitNode->BLT_S2XY));
      SetXY(dst.right - 1, dst.bottom - 1, &(pBlitNode->common.BLT_TXY));
    }

    pBlitNode->common.BLT_S1XY = pBlitNode->common.BLT_TXY;

    pBlitNode->BLT_S2SZ        = ((srcheight & 0x0FFF) << 16) | (srcwidth & 0x0FFF);
    pBlitNode->common.BLT_TSZ  = ((dstheight & 0x0FFF) << 16) | (dstwidth & 0x0FFF);

    /*
     * Default horizontal filter setup, repeat the first pixel 3 times to
     * pre-fill the 8 Tap filter. No initial phase shift is required.
     */
    pBlitNode->BLT_RZI |= (3<<BLIT_RZI_H_REPEAT_SHIFT);
  }
  else
  {
    /*
     * Fill in the vertical sizes as they are constant for all spans and it
     * saves us having to find out the values again.
     */
    pBlitNode->BLT_S2SZ        = ((srcheight & 0x0FFF) << 16);
    pBlitNode->common.BLT_TSZ  = ((dstheight & 0x0FFF) << 16);
  }

  return true;
}


bool CSTmBDispAQ::CopyRectComplex(const stm_blitter_operation_t &op,
                                  const stm_rect_t              &dst,
                                  const stm_rect_t              &src)
{
  if(op.ulFlags & STM_BLITTER_FLAGS_RLE_DECODE)
    return CSTmBDispAQ::CopyRectFromRLE(op, dst);

  if (op.srcSurface.format == SURF_YCBCR420MB
      || op.srcSurface.format == SURF_YCBCR422MB)
    return CSTmBDispAQ::CopyRectFromMB(op,src,dst);

  if (op.srcSurface.format == SURF_YUV420
      || op.srcSurface.format == SURF_YVU420
      || op.srcSurface.format == SURF_YUV422P)
    return CSTmBDispAQ::CopyRectFromPlanar(op,src,dst);

  {
  stm_bdisp_aq_full_blit_node blitNode = {{0}};
  bool bRequiresSpans   = false;
  bool bCopyDirReversed = false;

  if(!CopyRectSetupHelper(op,dst,src,&blitNode, bRequiresSpans, bCopyDirReversed))
    return false;

  if(!bRequiresSpans)
  {
    QueueBlitOperation(&blitNode, sizeof(stm_bdisp_aq_full_blit_node));
    return true;
  }

  ULONG filterstep = (blitNode.BLT_RSF & 0xffff);
  /*
   * The filter delay lines can hold m_lineBufferLength pixels, we want to
   * process slightly less than that because we need up to 4 pixels of overlap
   * between spans to fill the filter taps properly and not end up with
   * stripes down the image at the edges of two adjoining spans.
   */
  ULONG srcstep = (m_lineBufferLength - 8) * fixedpointONE;

  /*
   * Calculate how many destination pixels will get generated from one span of
   * the source image.
   */
  ULONG dststep = srcstep / filterstep;

  /*
   * Work the previous calculation backwards so we don't get rounding errors
   * causing bad filter positioning in the middle of the scaled image. We deal
   * with any left over pixels at the edge of the image separately.
   *
   * Note: this leaves srcstep in 6.10 fixed point.
   */
  srcstep = (dststep * filterstep);
  DEBUGF2(3, ("srcstep = 0x%x\n",(unsigned)srcstep));

  /*
   * Remember that src.right is not inside the rectangle
   */
  ULONG srcwidth = (src.right - src.left);

  ULONG srcx, dstx;

  if(!bCopyDirReversed)
  {
    srcx = src.left * fixedpointONE;
    dstx = dst.left;

    DEBUGF2(3, ("Generating spans left to right\n"));

    while(dststep > 0)
    {
      ULONG adjustedsrcx, srcspanwidth;
      ULONG repeat, subpixelpos;

      if((srcstep & fractionMASK) != 0)
        srcspanwidth = (srcstep + fixedpointONE)/fixedpointONE;
      else
        srcspanwidth = srcstep/fixedpointONE;

      adjustedsrcx = srcx / fixedpointONE;
      subpixelpos = srcx - (adjustedsrcx*fixedpointONE);

      /*
       * Adjust things to fill the first three filter taps either with real
       * data or a repeat of the first pixel.
       */
      ULONG edgedistance = adjustedsrcx - src.left;
      if(edgedistance >= 3)
      {
        repeat        = 0;
        adjustedsrcx -= 3;
        srcspanwidth += 3;
      }
      else
      {
        repeat        = 3 - edgedistance;
        adjustedsrcx  = src.left;
        srcspanwidth += edgedistance;
      }

      /*
       * Now adjust for an extra 4 pixels to be read to fill last four filter
       * taps, or to repeat the last pixel if we have gone outside of the source
       * image by clamping the source width we program in the hardware.
       */
      srcspanwidth += 4;
      if((adjustedsrcx-src.left+srcspanwidth)>srcwidth)
        srcspanwidth = srcwidth-(adjustedsrcx-src.left);

      DEBUGF2(3, ("span: adjsrcx = %lu subpos = %lu srcspanwidth = %lu dstx = %lu dststep = %lu repeat = %lu\n",adjustedsrcx,subpixelpos,srcspanwidth,dstx,dststep,repeat));

      SetXY(adjustedsrcx, src.top, &blitNode.BLT_S2XY);
      SetXY(dstx, dst.top, &blitNode.common.BLT_TXY);

      blitNode.common.BLT_S1XY = blitNode.common.BLT_TXY;

      blitNode.BLT_S2SZ       = (blitNode.BLT_S2SZ       & 0xFFFF0000)       | (srcspanwidth & 0x0FFF);
      blitNode.common.BLT_TSZ = (blitNode.common.BLT_TSZ & 0xFFFF0000)       | (dststep  & 0x0FFF);
      blitNode.BLT_RZI        = (blitNode.BLT_RZI & ~BLIT_RZI_H_REPEAT_MASK) | (repeat      << BLIT_RZI_H_REPEAT_SHIFT);
      blitNode.BLT_RZI        = (blitNode.BLT_RZI & ~BLIT_RZI_H_INIT_MASK)   | (subpixelpos << BLIT_RZI_H_INIT_SHIFT);

      QueueBlitOperation(&blitNode, sizeof(stm_bdisp_aq_full_blit_node));

      srcx += srcstep;
      dstx += dststep;

      /*
       * Remember that dst.right isn't in the destination rectangle either
       */
      if((dst.right - dstx) < dststep)
      {
        dststep = dst.right - dstx;
      }
    }
  }
  else
  {
    DEBUGF2(3, ("Generating spans right to left\n"));

    srcx = (src.right-1) * fixedpointONE;
    dstx = dst.right-1;

    DEBUGF2(3, ("span src left/right: %lu/%lu\n", src.left, src.right));
    DEBUGF2(3, ("span dst left/right: %lu/%lu\n", dst.left, dst.right));

    while(dststep > 0)
    {
      ULONG adjustedsrcx, srcspanwidth;
      ULONG repeat, subpixelpos;

      if((srcstep & fractionMASK) != 0)
        srcspanwidth = (srcstep + fixedpointONE)/fixedpointONE;
      else
        srcspanwidth = srcstep/fixedpointONE;

      adjustedsrcx = srcx / fixedpointONE;
      /*
       * This is not immediately obvious:
       *
       * 1. We need the subpixel position of the start, which is basically where
       *    the filter left off on the previous span.
       *
       * 2. We have to flip around the subpixel position as the filter is being
       *    filled with pixels in reverse.
       *
       * 3. Then if the subpixel position is not zero, we have to move our
       *    first real pixel sample in the middle of the filter to the one on
       *    the right of the current position.
       *
       * This could make the edge distance go negative on the first span if the
       * subpixel position is not zero, but that is ok because we will then end
       * up with an extra repeated pixel to pad the filter taps.
       */
      subpixelpos = (fixedpointONE - (srcx & fractionMASK))%fixedpointONE;
      if(subpixelpos != 0)
      {
        adjustedsrcx++;
        srcspanwidth++;
      }

      ULONG edgedistance = (src.right - adjustedsrcx - 1);
      DEBUGF2(3, ("span edgedistance = %lu\n",edgedistance));
      if(edgedistance >= 3)
      {
        repeat        = 0;
        adjustedsrcx += 3;
        srcspanwidth += 3;
      }
      else
      {
        repeat        = 3 - edgedistance;
        srcspanwidth += edgedistance;
        adjustedsrcx  = src.right-1;
      }

      srcspanwidth += 4;
      if((src.left + srcspanwidth - 1)>adjustedsrcx)
      {
        srcspanwidth = (adjustedsrcx-src.left)+1;
      }

      DEBUGF2(3, ("span adjsrcx = %lu subpixelpos = %lu srcspanwidth = %lu dstx = %lu dststep = %lu repeat = %lu\n",adjustedsrcx,subpixelpos,srcspanwidth,dstx,dststep,repeat));

      SetXY(adjustedsrcx, src.bottom - 1, &blitNode.BLT_S2XY);
      SetXY(dstx,         dst.bottom - 1, &blitNode.common.BLT_TXY);

      blitNode.common.BLT_S1XY = blitNode.common.BLT_TXY;

      blitNode.BLT_S2SZ       = (blitNode.BLT_S2SZ       & 0xFFFF0000)       | (srcspanwidth & 0x0FFF);
      blitNode.common.BLT_TSZ = (blitNode.common.BLT_TSZ & 0xFFFF0000)       | (dststep  & 0x0FFF);
      blitNode.BLT_RZI        = (blitNode.BLT_RZI & ~BLIT_RZI_H_REPEAT_MASK) | (repeat      << BLIT_RZI_H_REPEAT_SHIFT);
      blitNode.BLT_RZI        = (blitNode.BLT_RZI & ~BLIT_RZI_H_INIT_MASK)   | (subpixelpos << BLIT_RZI_H_INIT_SHIFT);

      QueueBlitOperation(&blitNode, sizeof(stm_bdisp_aq_full_blit_node));

      srcx -= srcstep;
      dstx -= dststep;

      /*
       * Alter the span sizes for the "last" span where we have an odd bit
       * left over (i.e. the total destination is not a nice multiple of
       * dststep).
       */
      if(((dstx - dst.left)+1) < dststep)
      {
        dststep = (dstx - dst.left)+1;
      }
    }
  }
  }

  DEBUGF2(2, ("%s completed.\n",__PRETTY_FUNCTION__));
  return true;
}

bool CSTmBDispAQ::CopyRectFromRLE(const stm_blitter_operation_t &op,
                                  const stm_rect_t              &dst)
{
  int retval = false;

  DEBUGF2(2, ("%s @ %p: src height/stride: %lu/%lu -> dst l/r/t/b/stride: %lu/%lu/%lu/%lu/%lu\n",
              __PRETTY_FUNCTION__, this,
              op.srcSurface.ulHeight, op.srcSurface.ulStride,
              dst.left, dst.right, dst.top, dst.bottom, op.dstSurface.ulStride));

  if ((op.ulFlags & ~m_copyOps) || (op.ulFlags & ~STM_BLITTER_FLAGS_RLE_DECODE))
    return false;

  if ((dst.left == dst.right) && (dst.top == dst.bottom))
    return false;

  stm_bdisp_aq_fast_blit_node * const pBlitNode
    = static_cast<stm_bdisp_aq_fast_blit_node *>(GetNewNode());

  pBlitNode->BLT_CIC = BLIT_FAST_NODE_CIC;

  // Set the mode
  pBlitNode->BLT_INS = BLIT_INS_SRC1_MODE_MEMORY;
  pBlitNode->BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  // Set source type
  if(!SetBufferType(op.srcSurface,&(pBlitNode->BLT_S1TY)))
  {
    DEBUGF2(1, ("%s invalid source type\n",__PRETTY_FUNCTION__));
    goto out;
  }

  // Set target type
  if(!SetBufferType(op.dstSurface,&(pBlitNode->BLT_TTY)))
  {
    DEBUGF2(1, ("%s invalid destination type\n",__PRETTY_FUNCTION__));
    goto out;
  }

  /* Src X / Y Reg should store the compressed data length.
   * The source surface is an array of bytes 1 pixel high.
   * The Width of the surface is equivilant to the data length. */
  pBlitNode->BLT_S1XY = op.srcSurface.ulStride;

  SetXY( dst.left, dst.top, &(pBlitNode->BLT_TXY) );

  // Set the source base address
  pBlitNode->BLT_S1BA = op.srcSurface.ulMemory;

  // Set the target base address
  pBlitNode->BLT_TBA = op.dstSurface.ulMemory;

  // Set the window dimensions
  pBlitNode->BLT_TSZ = (((dst.bottom - dst.top) & 0x0FFF) << 16) |
                        ((dst.right - dst.left) & 0x0FFF);


  DEBUGF2(4, ("%s() pBlitNode:\n",__PRETTY_FUNCTION__));
  DEBUGF2(4, ("  BLT_NIP : 0x%8lx  BLT_CIC : 0x%8lx\n"
              "  BLT_INS : 0x%8lx  BLT_ACK : 0x%8lx\n"
              "  BLT_TBA : 0x%8lx  BLT_TTY : 0x%8lx\n"
              "  BLT_TXY : 0x%8lx  BLT_TSZ : 0x%8lx\n"
              "  BLT_S1CF: 0x%8lx  BLT_S2CF: 0x%8lx\n"
              "  BLT_S1BA: 0x%8lx  BLT_S1TY: 0x%8lx\n"
              "  BLT_S1XY: 0x%8lx  BLT_UDF1: 0x%8lx\n",
              pBlitNode->BLT_NIP, pBlitNode->BLT_CIC, pBlitNode->BLT_INS,
              pBlitNode->BLT_ACK,
              pBlitNode->BLT_TBA, pBlitNode->BLT_TTY, pBlitNode->BLT_TXY,
              pBlitNode->BLT_TSZ,
              pBlitNode->BLT_S1CF, pBlitNode->BLT_S2CF,
              pBlitNode->BLT_S1BA, pBlitNode->BLT_S1TY, pBlitNode->BLT_S1XY,
              pBlitNode->BLT_UDF1));

  if (!(m_SharedAreaDMA.ulFlags & SDAAF_UNCACHED))
    g_pIOS->FlushCache(pBlitNode, sizeof(stm_bdisp_aq_fast_blit_node));
  CommitBlitOperation();

  DEBUGF2(2, ("%s completed.\n",__PRETTY_FUNCTION__));

  retval = true;

out:
  UNLOCK_ATOMIC;

  return retval;
}

/* To make things simple, for macroblock we only do colour conversion and
   scaling, ALU operations, and others are just not allowed. Also, ALU
   operations are not even possible for the 3 buffer formats. */
bool CSTmBDispAQ::YCbCrNodePrepare (const stm_blitter_operation_t &op,
                                    const stm_rect_t              & __restrict src,
                                    const stm_rect_t              & __restrict dst,
                                    stm_bdisp_aq_planar_node      &blitNode,
                                    bool                          &bRequiresSpans)
{
  ULONG src_left_f = src.left; /* fixed point */
  ULONG src_top_f = src.top; /* fixed point */
  ULONG src_left_i = src.left; /* int */
  ULONG src_top_i = src.top; /* int */
  if (op.ulFlags & STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT)
  {
    src_left_i /= fixedpointONE;
    src_top_i /= fixedpointONE;
  }
  else
  {
    src_left_f *= fixedpointONE;
    src_top_f *= fixedpointONE;
  }

  /*
   * Remember that src.right is not inside the rectangle
   */
  const ULONG srcwidth  = (src.right  - src.left);
  const ULONG srcheight = (src.bottom - src.top);
  const ULONG dstwidth  = (dst.right  - dst.left);
  const ULONG dstheight = (dst.bottom - dst.top);

  const ULONG srcx_cb_f = src_left_f / 2; /* fixed point */
  const ULONG srcx_cb_i = src_left_i / 2; /* int */
  ULONG srcy_cb_f = src_top_f; /* fixed point */
  ULONG srcy_cb_i; /* int */
  const ULONG srcwidth_cb = (srcwidth + 1) / 2;
  ULONG srcheight_cb = srcheight;

  ULONG ulScaleh_y, ulScalew_y;
  ULONG ulScaleh_chr, ulScalew_chr;

#ifdef DEBUG
  if (op.srcSurface.format != m_LastPlanarformat)
  {
    DEBUGF (("blit planar source format: %d (%s)\n", op.srcSurface.format,
             &"NULL      \0" "RGB565    \0"
              "RGB888    \0" "ARGB8565  \0"
              "ARGB8888  \0" "ARGB1555  \0"
              "ARGB4444  \0" "CRYCB888  \0"
              "YCBCR422R \0" "YCBCR422MB\0"
              "YCBCR420MB\0" "ACRYCB8888\0"
              "CLUT1     \0" "CLUT2     \0"
              "CLUT4     \0" "CLUT8     \0"
              "ACLUT44   \0" "ACLUT88   \0"
              "A1        \0" "A8        \0"
              "BGRA8888  \0" "YUYV      \0"
              "YUV420    \0" "YVU420    \0"
              "YUV422P   \0" "RLD_BD    \0"
              "RLD_H2    \0" "RLD_H8    \0"
              "CLUT8_4444\0" "END       "
             [op.srcSurface.format * 11]));
    m_LastPlanarformat = op.srcSurface.format;
  }
#endif

  bRequiresSpans = (srcwidth > m_lineBufferLength);

  blitNode.BLT_CIC = BLIT_PLANAR_CIC;

  /* we want the data from source two (and three) */
  blitNode.BLT_ACK = BLIT_ACK_BYPASSSOURCE2;
  blitNode.BLT_INS = (BLIT_INS_SRC1_MODE_DISABLED
                      | BLIT_INS_SRC2_MODE_MEMORY
                      | BLIT_INS_SRC3_MODE_MEMORY
                      | BLIT_INS_ENABLE_2DRESCALE
                     );

  blitNode.BLT_F_RZC = 0;
  /* don't care about BLT_PMK, it's not enabled in the instruction */

  ulScaleh_y = (srcheight * fixedpointONE) / dstheight;
  ulScalew_y = (srcwidth  * fixedpointONE) / dstwidth;

  switch (op.srcSurface.format)
  {
  case SURF_YCBCR420MB:
  case SURF_YUV420:
  case SURF_YVU420:
    /* round */
    ++srcheight_cb;
    srcheight_cb /= 2;
    srcy_cb_f /= 2;
    /* for 420 formats, chroma is offset by half a line */
    srcy_cb_f += fixedpointHALF;
    srcy_cb_i = srcy_cb_f / fixedpointONE;
    /* keep precision */
    ulScaleh_chr = (srcheight * fixedpointONE / 2) / dstheight;
    break;

  case SURF_YCBCR422MB:
  case SURF_YUV422P:
    srcy_cb_i = src_top_i;
    ulScaleh_chr = (srcheight * fixedpointONE) / dstheight;
    break;

  default:
    return false;
  }
  SetXY (srcx_cb_i, srcy_cb_i, &blitNode.BLT_S2XY);
  /* keep precision */
  ulScalew_chr = (srcwidth * fixedpointONE / 2) / dstwidth;
  blitNode.BLT_S2SZ = ((((srcheight_cb) & 0x0fff) << 16)
                       | ((srcwidth_cb) & 0x0fff));

  /* setup chroma */
  /* out of range */
  if ((ulScaleh_chr > ((63 * fixedpointONE) + 0x3ff))
      || (ulScalew_chr > ((63 * fixedpointONE) + 0x3ff))
      || (ulScaleh_chr == 0)
      || (ulScalew_chr == 0))
    return false;

  /* filters */
  union _BltNodeGroup0910 filters;
  filters.c = reinterpret_cast <struct _BDISP_GROUP_9 *> (&blitNode.BLT_RSF);
  FilteringSetup (ulScalew_chr, ulScaleh_chr, filters, srcx_cb_f, srcy_cb_f);

  if (blitNode.BLT_HFP)
    blitNode.BLT_F_RZC |= BLIT_RZC_2DHF_MODE_FILTER_BOTH;
  else
  {
    DEBUGF2 (1, ("%s Error: No horizontal chroma resize filter found.\n",
                 __PRETTY_FUNCTION__));
    blitNode.BLT_F_RZC |= BLIT_RZC_2DHF_MODE_RESIZE_ONLY;
  }

  if (blitNode.BLT_VFP)
    blitNode.BLT_F_RZC |= BLIT_RZC_2DVF_MODE_FILTER_BOTH;
  else
  {
    DEBUGF2 (1, ("%s Error: No vertical chroma resize filter found.\n",
                 __PRETTY_FUNCTION__));
    blitNode.BLT_F_RZC |= BLIT_RZC_2DVF_MODE_RESIZE_ONLY;
  }

  /* setup luma */
  /* out of range */
  if ((ulScaleh_y > ((63 * fixedpointONE) + 0x3ff))
      || (ulScalew_y > ((63 * fixedpointONE) + 0x3ff))
      || (ulScaleh_y == 0)
      || (ulScalew_y == 0))
    return false;

  /* filters */
  filters.l = reinterpret_cast <struct _BDISP_GROUP_10 *> (&blitNode.BLT_Y_RSF);
  FilteringSetup (ulScalew_y, ulScaleh_y, filters, src_left_f, src_top_f);

  if (blitNode.BLT_Y_HFP)
    blitNode.BLT_F_RZC |= BLIT_RZC_Y_2DHF_MODE_FILTER_BOTH;
  else
  {
    DEBUGF2 (1, ("%s Error: No horizontal luma resize filter found.\n",
                 __PRETTY_FUNCTION__));
    blitNode.BLT_F_RZC |= BLIT_RZC_Y_2DHF_MODE_RESIZE_ONLY;
  }

  if (blitNode.BLT_Y_VFP)
    blitNode.BLT_F_RZC |= BLIT_RZC_Y_2DVF_MODE_FILTER_BOTH;
  else
  {
    DEBUGF2 (1, ("%s Error: No vertical luma resize filter found.\n",
                 __PRETTY_FUNCTION__));
    blitNode.BLT_F_RZC |= BLIT_RZC_Y_2DVF_MODE_RESIZE_ONLY;
  }

  /* target */
  blitNode.BLT_TBA = op.dstSurface.ulMemory;
  if (UNLIKELY (!SetBufferType (op.dstSurface, &blitNode.BLT_TTY)
                || (!BLIT_TY_COLOR_FORM_IS_RGB (blitNode.BLT_TTY)
                    && !BLIT_TY_COLOR_FORM_IS_YUVr (blitNode.BLT_TTY))))
  {
    DEBUGF2 (1, ("%s unsupported target type\n", __PRETTY_FUNCTION__));
    return false;
  }
  SetXY (dst.left, dst.top, &blitNode.BLT_TXY);
  blitNode.BLT_TSZ = (((dstheight & 0x0fff) << 16)
                      | (dstwidth & 0x0fff));

  /* source 3 */
  blitNode.BLT_S3BA = op.srcSurface.ulMemory;
  if (UNLIKELY (!SetBufferType (op.srcSurface, &blitNode.BLT_S3TY)))
  {
    DEBUGF2 (1, ("%s invalid source3 type\n", __PRETTY_FUNCTION__));
    return false;
  }
  SetXY (src_left_i, src_top_i, &blitNode.BLT_S3XY);
  blitNode.BLT_S3SZ = (((srcheight & 0x0fff) << 16)
                       | (srcwidth & 0x0fff));

  /* source 1 and source 2. source 1 is only needed for the real 3 buffer
     formats and otherwise not enabled in the instruction (but still in
     CIC). */
  switch (op.srcSurface.format)
  {
  case SURF_YUV420:
  case SURF_YVU420:
  case SURF_YUV422P:
    blitNode.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
    blitNode.BLT_S1TY
      = blitNode.BLT_S2TY
      = ((blitNode.BLT_S3TY & ~BLIT_TY_PIXMAP_PITCH_MASK)
         | op.srcSurface.ulStride / 2);
    blitNode.BLT_S1XY = blitNode.BLT_S2XY;
    /* S1SZ is implicitly the same as S2SZ in the hardware for the 3 buffer
       formats. */
    switch (op.srcSurface.format)
    {
    case SURF_YUV420:
      blitNode.BLT_S2BA = op.srcSurface.ulMemory + op.srcSurface.ulChromaOffset;
      blitNode.BLT_S1BA = (blitNode.BLT_S2BA
                           + op.srcSurface.ulStride * op.srcSurface.ulHeight / 4);
      break;

    case SURF_YVU420:
      blitNode.BLT_S1BA = op.srcSurface.ulMemory + op.srcSurface.ulChromaOffset;
      blitNode.BLT_S2BA = (blitNode.BLT_S1BA
                           + op.srcSurface.ulStride * op.srcSurface.ulHeight / 4);
      break;

    case SURF_YUV422P:
      blitNode.BLT_S2BA = op.srcSurface.ulMemory + op.srcSurface.ulChromaOffset;
      blitNode.BLT_S1BA = (blitNode.BLT_S2BA
                           + op.srcSurface.ulStride * op.srcSurface.ulHeight / 2);
      break;

    default:
      /* should not be reached */
      break;
    }

    break;

  case SURF_YCBCR420MB:
  case SURF_YCBCR422MB:
    /* only source 2 is needed, but source1 has to be cleared to not crash
       the BDisp. */
    blitNode.BLT_S1BA
      = blitNode.BLT_S1TY
      = blitNode.BLT_S1XY
      = 0;
    blitNode.BLT_S2BA = op.srcSurface.ulMemory + op.srcSurface.ulChromaOffset;
    /* stride stays the same, as CbCr are interleaved in the plane */
    blitNode.BLT_S2TY
      = ((blitNode.BLT_S3TY & ~BLIT_TY_PIXMAP_PITCH_MASK)
         | op.srcSurface.ulStride / 1);
    break;

  default:
    /* should not be reached */
    break;
  }

  bool src_is_709 = (op.ulFlags & STM_BLITTER_FLAGS_SRC_COLOURSPACE_709) ? true : false;
  bool dst_is_709 = (op.ulFlags & STM_BLITTER_FLAGS_DST_COLOURSPACE_709) ? true : false;

  if (BLIT_TY_COLOR_FORM_IS_RGB (blitNode.BLT_TTY)
      || (src_is_709 != dst_is_709))
  {
    ULONG * __restrict ivmx = blitNode.BLT_IVMX;

    if (BLIT_TY_COLOR_FORM_IS_RGB (blitNode.BLT_TTY))
    {
      if (op.ulFlags & STM_BLITTER_FLAGS_DST_FULLRANGE)
      {
        if (src_is_709)
          SetMatrix (ivmx, ycbcr709_2_rgb_full);
        else
          SetMatrix (ivmx, ycbcr601_2_rgb_full);
      }
      else
      {
        if (src_is_709)
          SetMatrix (ivmx, ycbcr709_2_rgb);
        else
          SetMatrix (ivmx, ycbcr601_2_rgb);
      }
    }
    else
    {
      if (op.ulFlags & STM_BLITTER_FLAGS_SRC_COLOURSPACE_709)
        SetMatrix (ivmx, ycbcr709_2_ycbcr601);
      else
        SetMatrix (ivmx, ycbcr601_2_ycbcr709);
    }

    blitNode.BLT_INS |= BLIT_INS_ENABLE_ICSC;
    blitNode.BLT_CIC |= BLIT_IVMX_CIC;
  }

  return true;
}

bool CSTmBDispAQ::YCbCrBlitUsingSpans (const stm_blitter_operation_t &op,
                                       const stm_rect_t              & __restrict src,
                                       const stm_rect_t              & __restrict dst,
                                       stm_bdisp_aq_planar_node      &blitNode)
{
  ULONG srcx_f
    = src.left * ((op.ulFlags & STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT)
                  ? 1 : fixedpointONE); /* fixed point */
  const ULONG srcx_i
    = src.left / ((op.ulFlags & STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT)
                  ? fixedpointONE : 1); /* int */
  const ULONG srcy_i
    = src.top / ((op.ulFlags & STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT)
                 ? fixedpointONE : 1); /* int */
  const ULONG srcx_cb_i = srcx_i / 2;

  const ULONG srcwidth = (src.right - src.left);
  const ULONG srcwidth_cb = (srcwidth + 1) / 2;

  ULONG dstx = dst.left; /* always int */

  const ULONG hsrcinc = (blitNode.BLT_Y_RSF & 0xffff);
  /*
   * The filter delay lines can hold m_lineBufferLength pixels, we want to
   * process slightly less than that because we need up to 4 pixels of overlap
   * between spans to fill the filter taps properly and not end up with
   * stripes down the image at the edges of two adjoining spans.
   */
  const ULONG max_filterstep = (m_lineBufferLength - 8) * fixedpointONE;

  /*
   * Calculate how many destination pixels will get generated from one span of
   * the source image.
   */
  ULONG dststep = max_filterstep / hsrcinc;

  /*
   * Work the previous calculation backwards so we don't get rounding errors
   * causing bad filter positioning in the middle of the scaled image. We deal
   * with any left over pixels at the edge of the image separately.
   *
   * Note: this puts srcstep in 6.10 fixed point.
   */
  const ULONG srcstep = (dststep * hsrcinc);
  DEBUGF2 (3, ("srcstep = 0x%x\n", (unsigned) srcstep));

  DEBUGF2 (3, ("Generating spans left to right\n"));

  ULONG _srcspanwidth /* int */;
  if ((srcstep & fractionMASK) != 0)
    _srcspanwidth = (srcstep + fixedpointONE) / fixedpointONE;
  else
    _srcspanwidth = srcstep / fixedpointONE;

  ULONG tsz  = blitNode.BLT_TSZ & 0xffff0000;
  ULONG s2xy_y = (blitNode.BLT_S2XY & 0xffff0000) >> 16;
  ULONG s2sz   = blitNode.BLT_S2SZ & 0xffff0000;
  ULONG s3sz = blitNode.BLT_S3SZ & 0xffff0000;
  ULONG rzi  = blitNode.BLT_RZI & ~(BLIT_RZI_H_REPEAT_MASK
                                    | BLIT_RZI_H_INIT_MASK);
  ULONG yrzi = blitNode.BLT_Y_RZI & ~(BLIT_RZI_H_REPEAT_MASK
                                      | BLIT_RZI_H_INIT_MASK);

  while (dststep > 0)
  {
    ULONG adjustedsrcx /* int */, srcspanwidth = _srcspanwidth /* int */;
    ULONG subpixelpos /* fixed point */;
    ULONG edgedistance /* int */;

    adjustedsrcx = srcx_f / fixedpointONE;
    subpixelpos = srcx_f - (adjustedsrcx * fixedpointONE);

    /*
     * Adjust things to fill the first three filter taps either with real
     * data or a repeat of the first pixel.
     */
    edgedistance = adjustedsrcx - srcx_i;

    ULONG repeat;
    if (edgedistance >= 3)
    {
      repeat        = 0;
      adjustedsrcx -= 3;
      srcspanwidth += 3;
    }
    else
    {
      repeat        = 3 - edgedistance;
      adjustedsrcx  = srcx_i;
      srcspanwidth += edgedistance;
    }

    /*
     * Now adjust for an extra 4 pixels to be read to fill last four filter
     * taps, or to repeat the last pixel if we have gone outside of the
     * source image by clamping the source width we program in the hardware.
     */
    srcspanwidth += 4;
    if ((adjustedsrcx - srcx_i + srcspanwidth) > srcwidth)
      srcspanwidth = srcwidth - (adjustedsrcx - srcx_i);

    DEBUGF2 (3, ("span: adjsrcx: %lu subpos: %lu srcspanwidth: %lu dstx: %lu "
                 "dststep: %lu repeat: %lu\n", adjustedsrcx, subpixelpos,
                 srcspanwidth, dstx, dststep, repeat));

    SetXY (dstx, dst.top, &blitNode.BLT_TXY);
    blitNode.BLT_TSZ  = tsz;
    blitNode.BLT_TSZ |= (dststep & 0x0fff);

    SetXY (adjustedsrcx, srcy_i, &blitNode.BLT_S3XY);
    blitNode.BLT_S3SZ  = s3sz;
    blitNode.BLT_S3SZ |= (srcspanwidth & 0x0fff);

    blitNode.BLT_Y_RZI  = yrzi;
    blitNode.BLT_Y_RZI |= (repeat << BLIT_RZI_H_REPEAT_SHIFT);
    blitNode.BLT_Y_RZI |= (subpixelpos << BLIT_RZI_H_INIT_SHIFT);

    { /* chroma needs some different handling */
    ULONG adjustedsrcx_cb /* int */;
    ULONG subpixelpos_cb /* fixed point */;
    ULONG this_srcx_cb_f = srcx_f / 2 /* fixed point */;
    ULONG srcspanwidth_cb = _srcspanwidth / 2 /* int */;
    ULONG edgedistance_cb /* int */;

    adjustedsrcx_cb = this_srcx_cb_f / fixedpointONE;
    subpixelpos_cb = this_srcx_cb_f - (adjustedsrcx_cb * fixedpointONE);

    edgedistance_cb = adjustedsrcx_cb - srcx_cb_i;

    ULONG repeat_cb;
    if (edgedistance_cb >= 3)
    {
      repeat_cb        = 0;
      adjustedsrcx_cb -= 3;
      srcspanwidth_cb += 3;
    }
    else
    {
      repeat_cb        = 3 - edgedistance_cb;
      adjustedsrcx_cb  = srcx_cb_i;
      srcspanwidth_cb += edgedistance_cb;
    }

    srcspanwidth_cb += 4;
    if ((adjustedsrcx_cb - srcx_cb_i + srcspanwidth_cb) > srcwidth_cb)
      srcspanwidth_cb = srcwidth_cb - (adjustedsrcx_cb - srcx_cb_i);

    SetXY (adjustedsrcx_cb, s2xy_y, &blitNode.BLT_S2XY);
    blitNode.BLT_S2SZ  = s2sz;
    blitNode.BLT_S2SZ |= (srcspanwidth_cb & 0x0fff);

    blitNode.BLT_S1XY = blitNode.BLT_S2XY;

    blitNode.BLT_RZI  = rzi;
    blitNode.BLT_RZI |= (repeat_cb << BLIT_RZI_H_REPEAT_SHIFT);
    blitNode.BLT_RZI |= (subpixelpos_cb << BLIT_RZI_H_INIT_SHIFT);
    }

    QueueBlitOperation (&blitNode, sizeof (blitNode));

    srcx_f += srcstep;
    dstx += dststep;

    /* Remember that dst.right isn't in the destination rectangle either */
    if ((dst.right - dstx) < dststep)
      dststep = dst.right - dstx;
  }

  return true;
}

bool CSTmBDispAQ::CopyRectFromMB (const stm_blitter_operation_t &op,
                                  const stm_rect_t              & __restrict src,
                                  const stm_rect_t              & __restrict dst)
{
  stm_bdisp_aq_planar_node blitNode;
  bool                     bRequiresSpans;

  if (!YCbCrNodePrepare (op, src, dst, blitNode, bRequiresSpans))
    return false;

  if (UNLIKELY (!bRequiresSpans))
  {
    QueueBlitOperation (&blitNode, sizeof (blitNode));
    return true;
  }

  return YCbCrBlitUsingSpans (op, src, dst, blitNode);
}

bool CSTmBDispAQ::CopyRectFromPlanar (const stm_blitter_operation_t &op,
                                      const stm_rect_t              & __restrict src,
                                      const stm_rect_t              & __restrict dst)
{
  stm_bdisp_aq_planar_node blitNode;
  bool                     bRequiresSpans;

  if (UNLIKELY (!(m_copyOps & STM_BLITTER_FLAGS_3BUFFER_SOURCE)))
  {
    /* not supported on this platform, fill dst with black */
    stm_blitter_operation_t op_;
    op_.ulFlags = STM_BLITTER_FLAGS_NONE;
    op_.dstSurface = op.dstSurface;
    op_.colourFormat = SURF_ARGB8888;
    op_.ulColour = 0xff101010;
    if (op.ulFlags & STM_BLITTER_FLAGS_DST_FULLRANGE)
      op_.ulColour = 0xff000000;
    FillRect (op_, dst);
    return false;
  }

  if (!YCbCrNodePrepare (op, src, dst, blitNode, bRequiresSpans))
    return false;

  if (UNLIKELY (!bRequiresSpans))
  {
    QueueBlitOperation (&blitNode, sizeof (blitNode));
    return true;
  }

  return YCbCrBlitUsingSpans (op, src, dst, blitNode);
}

bool CSTmBDispAQ::IsEngineBusy(void)
{
  /*
   * As this is only one queue of many on the BDisp hardware,
   * testing the hardware status tells us if there is activity on any
   * of the queues. Hence this call should only return if this queue
   * has activity on it, hence we just return the software status.
   */
  return m_Shared->bdisp_running != 0;
}


bool CSTmBDispAQ::HandleBlitterInterrupt(void)
{
  ULONG intreg = ReadDevReg(BDISP_ITS) & (m_lnaInt | m_nodeInt);

  if(!intreg)
    return false;

  ++m_Shared->num_irqs;

  if (UNLIKELY (!m_Shared->bdisp_running))
    g_pIDebug->IDebugPrintf ("%s @ %p: BDisp not running? BDISP_ITS %.8lx!\n",
                             __PRETTY_FUNCTION__, this, intreg);

  /* the status register holds the address of the last node that generated
     an interrupt */
  m_Shared->last_free = ReadAQReg (BDISP_AQ_STA) - m_Shared->nodes_phys;

  DEBUGF2 (3, ("%s @ %p: ITS: %.8lx: %c%c\n", __PRETTY_FUNCTION__, this,
               intreg,
               (intreg & m_nodeInt) ? 'n' : '_',
               (intreg & m_lnaInt) ?  'q' : '_'));

  if (intreg & m_nodeInt)
  {
    blitload_calc_busy_ticks ();
    m_Shared->bdisp_ops_start_time = g_pIOS->GetSystemTime ();

    ++m_Shared->num_node_irqs;
    if (m_Shared->last_lut && m_Shared->last_free == m_Shared->last_lut)
      m_Shared->last_lut = 0;
  }

  if (intreg & m_lnaInt)
  {
    /* We have processed all currently available nodes in the buffer so
       signal that the blitter is now finished for SyncChip. */
    if (!m_bUserspaceAQ)
    {
      m_Shared->bdisp_running = 0;

      blitload_calc_busy_ticks ();
    }
    else
    {
      /* it can happen that the IRQ is fired while userspace is preparing to
         update the LNA. We have to be careful not to clear bdisp_running in
         that case. */
      if (UNLIKELY (m_Shared->updating_lna
                    && m_Shared->prev_set_lna != ReadAQReg (BDISP_AQ_LNA)))
      {
        /* userspace is about to write the new LNA into the register, but
           didn't manage to do so yet, since it was interrupted. This means
           that the interrupt was meant to signal completion of some
           previous instructions. Userspace might have set bdisp_running
           already, so we leave it alone. Of course, it actually doesn't
           matter whether or not bdisp_running has been set already, it's
           simply important not to reset to zero! */
        DEBUGF2 (3, ("%s @ %p: atomic...: %d/%.8lx/%.8lx\n",
                     __PRETTY_FUNCTION__, this,
                     m_Shared->updating_lna, m_Shared->prev_set_lna,
                     ReadAQReg (BDISP_AQ_LNA)));
      }
      else
      {
        m_Shared->bdisp_running = 0;

        DEBUGF2 (3, ("%s @ %p: done: %d/%.8lx/%.8lx\n",
                     __PRETTY_FUNCTION__, this,
                     m_Shared->updating_lna, m_Shared->prev_set_lna,
                     ReadAQReg (BDISP_AQ_LNA)));

        blitload_calc_busy_ticks ();
      }
    }

    ++m_Shared->num_lna_irqs;
    ++m_Shared->num_idle;
    m_Shared->last_lut = 0;

    g_pIOS->WakeQueue (BLIT_ENGINE_QUEUE);
  }

  /* Signal to sleepers that m_Shared->last_free has changed. */
  g_pIOS->WakeQueue(BLIT_NODE_QUEUE);

  /*
   * Clear interrupts for this queue.
   */
  WriteDevReg(BDISP_ITS,intreg);

  return true;
}


int CSTmBDispAQ::SyncChip(bool WaitNextOnly)
{
  int ret;

  /* We need to deal with possible signal delivery while sleeping, this is
     indicated by a non zero return value.
     The sleep itself is safe from race conditions */
  DEBUGF2(3, ("%p: Waiting for %s.... (%s, last_free/next_free %lu-%lu, "
              "CTL %.8lx, STA %.8lx ITS %.8lx)\n", this,
              WaitNextOnly ? "a bit" : "idle",
              m_Shared->bdisp_running ? "running" : "   idle",
              m_Shared->last_free, m_Shared->next_free,
              ReadAQReg (BDISP_AQ_CTL), ReadAQReg (BDISP_AQ_STA),
              ReadDevReg (BDISP_ITS)));

  if (!WaitNextOnly)
  {
    /* doesn't need to be atomic, it's for statistics only anyway... */
    m_Shared->num_wait_idle++;

    ret = g_pIOS->SleepOnQueue (BLIT_ENGINE_QUEUE,
                                &m_Shared->bdisp_running, 0,
                                STMIOS_WAIT_COND_EQUAL);
  }
  else
  {
    /* doesn't need to be atomic, it's for statistics only anyway... */
    m_Shared->num_wait_next++;

    ret = g_pIOS->SleepOnQueue (BLIT_NODE_QUEUE,
                                &m_Shared->last_free, m_Shared->next_free,
                                STMIOS_WAIT_COND_NOT_EQUAL);
  }

  DEBUGF2(3, ("%p: ...done (%s, last_free/next_free %lu-%lu, "
              "CTL %.8lx, STA %.8lx ITS %.8lx)\n", this,
              m_Shared->bdisp_running ? "running" : "   idle",
              m_Shared->last_free, m_Shared->next_free,
              ReadAQReg (BDISP_AQ_CTL), ReadAQReg (BDISP_AQ_STA),
              ReadDevReg (BDISP_ITS)));

  return ret;
}


#ifdef DEBUG
static void DumpNode (const ULONG *node)
{
  ULONG cic = node[1];

  DEBUGF2 (6, ("BLT_NIP  BLT_CIC  BLT_INS  BLT_ACK : %.8lx %.8lx %.8lx %.8lx\n",
               node[0], node[1], node[2], node[3]));
  DEBUGF2 (6, ("BLT_TBA  BLT_TTY  BLT_TXY  BLT_TSZ : %.8lx %.8lx %.8lx %.8lx\n",
               node[4], node[5], node[6], node[7]));
  node += 8;
  if (cic & (1 << 2)) /* CIC_NODE_COLOR */
  {
    DEBUGF2 (6, ("BLT_S1CF BLT_S2CF                  : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & (1 << 3)) /* CIC_NODE_SOURCE1 */
  {
    DEBUGF2 (6, ("BLT_S1BA BLT_S1TY BLT_S1XY         : %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2]));
    node += 4;
  }
  if (cic & (1 << 4)) /* CIC_NODE_SOURCE2 */
  {
    DEBUGF2 (6, ("BLT_S2BA BLT_S2TY BLT_S2XY BLT_S2SZ: %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & (1 << 5)) /* CIC_NODE_SOURCE3 */
  {
    DEBUGF2 (6, ("BLT_S3BA BLT_S3TY BLT_S3XY BLT_S3SZ: %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & (1 << 6)) /* CIC_NODE_CLIP */
  {
    DEBUGF2 (6, ("BLT_CWO  BLT_CWS                   : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & (1 << 7)) /* CIC_NODE_CLUT */
  {
    DEBUGF2 (6, ("BLT_CCO  BLT_CML                   : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & BLIT_PMK_CIC) /* CIC_NODE_FILTERS */
  {
    DEBUGF2 (6, ("BLT_FCTL BLT_PMK                   : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & BLIT_RESCALE_CIC) /* CIC_NODE_2DFILTERSCHR */
  {
    DEBUGF2 (6, ("BLT_RSF  BLT_RZI  BLT_HFP  BLT_VFP : %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & (1 << 10)) /* CIC_NODE_2DFILTERSLUMA */
  {
    DEBUGF2 (6, ("BLT_YRSF BLT_YRZI BLT_YHFP BLT_YVFP: %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & BLIT_FFILT_CIC) /* CIC_NODE_FLICKER */
  {
    DEBUGF2 (6, ("BLT_FF0  BLT_FF1  BLT_FF2  BLT_FF3 : %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & BLIT_KEYS_CIC) /* CIC_NODE_COLORKEY */
  {
    DEBUGF2 (6, ("BLT_KEY1 BLT_KEY2                  : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & (1 << 14)) /* CIC_NODE_STATIC */
  {
    DEBUGF2 (6, ("BLT_SAR  BLT_USR                   : %.8lx %.8lx\n",
                 node[0], node[1]));
    node += 2;
  }
  if (cic & BLIT_IVMX_CIC) /* CIC_NODE_IVMX */
  {
    DEBUGF2 (6, ("BLT_IVMX BLT_IVMX BLT_IVMX BLT_IVMX: %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & BLIT_OVMX_CIC) /* CIC_NODE_OVMX */
  {
    DEBUGF2 (6, ("BLT_OVMX BLT_OVMX BLT_OVMX BLT_OVMX: %.8lx %.8lx %.8lx %.8lx\n",
                 node[0], node[1], node[2], node[3]));
    node += 4;
  }
  if (cic & (1 << 17)) /* CIC_NODE_VC1R */
  {
    DEBUGF2 (6, ("BLT_VC1R                           : %.8lx\n",
                 node[0]));
    node += 2;
  }
}
#else
static inline void DumpNode(const ULONG *node) { }
#endif

/*
 * Queue a blit node, this copes with all the different sizes
 * of nodes.
 */
void CSTmBDispAQ::QueueBlitOperation_locked(void *blitNode, int size)
{
  DEBUGF2(3,("%s\n",__PRETTY_FUNCTION__));

  if(m_Shared->last_free == m_Shared->next_free)
  {
    /* doesn't need to be atomic, it's for statistics only anyway... */
    m_Shared->num_wait_next++;

    g_pIOS->SleepOnQueue(BLIT_NODE_QUEUE,&m_Shared->last_free,m_Shared->next_free,STMIOS_WAIT_COND_NOT_EQUAL);
  }

  DEBUGF2(3,("%s node offset = 0x%08lx\n",__PRETTY_FUNCTION__,m_Shared->next_free));
  DumpNode (reinterpret_cast<ULONG *> (blitNode));

  g_pIOS->MemcpyToDMAArea(&m_SharedAreaDMA,
                          m_NodeOffset + m_Shared->next_free + BLIT_CIC_OFFSET,
                          (unsigned char *)blitNode + BLIT_CIC_OFFSET,
                          size - BLIT_CIC_OFFSET);

  /*
   * MemcpyToDMAArea does a cache purge.
   */
  CommitBlitOperation();

  DEBUGF2(3,("%s completed.\n",__PRETTY_FUNCTION__));
}

void CSTmBDispAQ::QueueBlitOperation(void *blitNode, int size)
{
  DEBUGF2(3,("%s\n",__PRETTY_FUNCTION__));

  LOCK_ATOMIC;

  QueueBlitOperation_locked(blitNode, size);

  UNLOCK_ATOMIC;

  DEBUGF2(3,("%s completed.\n",__PRETTY_FUNCTION__));
}
