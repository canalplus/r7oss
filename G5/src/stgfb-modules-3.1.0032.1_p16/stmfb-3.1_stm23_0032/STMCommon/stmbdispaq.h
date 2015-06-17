/***********************************************************************
 *
 * File: STMCommon/stmbdispaq.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_BDISP_AQ_H
#define _STM_BDISP_AQ_H

#include "stmblitter.h"
#include "stmbdispregs.h"

#define BDISP_VALID_DRAW_OPS   (0                                            \
                                | STM_BLITTER_FLAGS_GLOBAL_ALPHA             \
                                | STM_BLITTER_FLAGS_BLEND_SRC_ALPHA          \
                                | STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT  \
                                | STM_BLITTER_FLAGS_XOR                      \
                                | STM_BLITTER_FLAGS_BLEND_DST_COLOR          \
                                | STM_BLITTER_FLAGS_BLEND_DST_MEMORY         \
                                | STM_BLITTER_FLAGS_BLEND_DST_ZERO           \
                                | STM_BLITTER_FLAGS_PLANE_MASK               \
                                | STM_BLITTER_FLAGS_DST_COLOR_KEY            \
                               )

#define BDISP_VALID_COPY_OPS   (0                                            \
                                | STM_BLITTER_FLAGS_DST_COLOR_KEY            \
                                | STM_BLITTER_FLAGS_SRC_COLOR_KEY            \
                                | STM_BLITTER_FLAGS_GLOBAL_ALPHA             \
                                | STM_BLITTER_FLAGS_PREMULT_COLOUR_ALPHA     \
                                | STM_BLITTER_FLAGS_COLORIZE                 \
                                | STM_BLITTER_FLAGS_XOR                      \
                                | STM_BLITTER_FLAGS_BLEND_SRC_ALPHA          \
                                | STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT  \
                                | STM_BLITTER_FLAGS_BLEND_DST_COLOR          \
                                | STM_BLITTER_FLAGS_BLEND_DST_MEMORY         \
                                | STM_BLITTER_FLAGS_BLEND_DST_ZERO           \
                                | STM_BLITTER_FLAGS_PLANE_MASK               \
                                | STM_BLITTER_FLAGS_CLUT_ENABLE              \
                                | STM_BLITTER_FLAGS_FLICKERFILTER            \
                                | STM_BLITTER_FLAGS_3BUFFER_SOURCE           \
                               )


struct stm_bdisp_aq_fast_blit_node
{
  BDISP_GROUP_0;
  BDISP_GROUP_1;
  BDISP_GROUP_2;
  BDISP_GROUP_3;
};


struct stm_bdisp_aq_full_blit_node
{
  stm_bdisp_aq_fast_blit_node common;

  BDISP_GROUP_4;
  BDISP_GROUP_7;
  BDISP_GROUP_8;
  BDISP_GROUP_9;
  BDISP_GROUP_11;
  BDISP_GROUP_12;
  BDISP_GROUP_15;
  BDISP_GROUP_16;

  ULONG PADDING[6];
};


struct stm_bdisp_aq_fullfill_node
{
  stm_bdisp_aq_fast_blit_node common;

  BDISP_GROUP_4;
  BDISP_GROUP_8;
  BDISP_GROUP_12;
  BDISP_GROUP_15;
  BDISP_GROUP_16;
};

struct stm_bdisp_aq_planar_node
{
  BDISP_GROUP_0; /* general */
  BDISP_GROUP_1; /* target */
  BDISP_GROUP_3; /* source 1 */
  BDISP_GROUP_4; /* source 2 */
  BDISP_GROUP_5; /* source 3 */
  BDISP_GROUP_8; /* filter control */
  BDISP_GROUP_9; /* chroma filter/scale */
  BDISP_GROUP_10; /* luma filter/scale */
  BDISP_GROUP_15; /* ivmx */
};


class CDisplayDevice;

class CSTmBDispAQ : public CSTmBlitter
{
public:
  CSTmBDispAQ(ULONG                   *pBlitterBaseAddr,
              ULONG                    AQOffset,
              int                      qNumber,
              int                      qPriority,
              ULONG                    drawOps,
              ULONG                    copyOps,
              stm_blitter_device_priv  eDevice,
              ULONG                    nLineBufferLength);
  ~CSTmBDispAQ();

  bool Create(void);

  int  SyncChip(bool WaitNextOnly);
  bool IsEngineBusy(void);

  bool HandleBlitterInterrupt(void);

  STMFBBDispSharedAreaPriv *GetSharedArea (void) { m_bUserspaceAQ = true; return m_Shared; }

  ULONG GetBlitLoad (void) { ULONG busy;
                             /* we don't do any locking here - this all is
                                for statistics only anyway... */
                             if (m_Shared->bdisp_ops_start_time)
                               {
                                 /* normally, we calculate the busy ticks upon
                                    a node interrupt. But if GetBlitLoad() is
                                    called more often than interrupts occur,
                                    it will return zero for a very long time,
                                    and then all of a sudden a huge(!) value
                                    for busy ticks. To prevent this from
                                    happening, we calculate the busy ticks
                                    here as well. */
                                 unsigned long long nowtime
                                   = g_pIOS->GetSystemTime ();
                                 m_Shared->ticks_busy
                                   += (nowtime - m_Shared->bdisp_ops_start_time);
                                 m_Shared->bdisp_ops_start_time = nowtime;
                               }
                             busy = m_Shared->ticks_busy;
                             m_Shared->ticks_busy = 0;
                             return busy; }

  // Drawing functions
  bool FillRect       (const stm_blitter_operation_t&, const stm_rect_t&);
  bool DrawRect       (const stm_blitter_operation_t&, const stm_rect_t&);
  bool CopyRect       (const stm_blitter_operation_t&, const stm_rect_t&, const stm_point_t&);
  bool CopyRectComplex(const stm_blitter_operation_t&, const stm_rect_t&, const stm_rect_t&);

protected:
  ULONG  m_drawOps;
  ULONG  m_copyOps;
  stm_blitter_device_priv m_eDevice;
  ULONG  m_lineBufferLength;

private:
  bool ALUOperation     (stm_bdisp_aq_fast_blit_node *pBlitNodeCommon, const stm_blitter_operation_t &op);
  bool ColourKey        (stm_bdisp_aq_fast_blit_node *pBlitNodeCommon, const stm_blitter_operation_t &op, ULONG *keys, ULONG cicupdate);
  bool MatrixConversions(stm_bdisp_aq_fast_blit_node *pBlitNodeCommon, const stm_blitter_operation_t &op, ULONG s2ty, ULONG *ivmx, ULONG *ovmx, ULONG cicupdate);
  bool SetPlaneMask     (stm_bdisp_aq_fast_blit_node *pBlitNodeCommon, const stm_blitter_operation_t &op, ULONG &pmk, ULONG cicupdate);

  void *GetNewNode(void);
  void CommitBlitOperation(void);
  void QueueBlitOperation(void *blitNode, int size);
  void QueueBlitOperation_locked(void *blitNode, int size);

  struct _BDISP_GROUP_9 {
    BDISP_GROUP_9;
  };

  struct _BDISP_GROUP_10 {
    BDISP_GROUP_10;
  };

  union _BltNodeGroup0910 {
    struct _BDISP_GROUP_9  *c;
    struct _BDISP_GROUP_10 *l;
  };

  void FilteringSetup (int                      hsrcinc,
                       int                      vsrcinc,
                       union _BltNodeGroup0910 &filter,
                       ULONG                    h_init,
                       ULONG                    v_init);
  bool CopyRectSetupHelper(const stm_blitter_operation_t &op,
                           const stm_rect_t              &dst,
                           const stm_rect_t              &src,
                           stm_bdisp_aq_full_blit_node   *pBlitNode,
                           bool                          &bRequiresSpans,
                           bool                          &bCopyDirReversed);

  bool CopyRectFromRLE    (const stm_blitter_operation_t&, const stm_rect_t&);

  bool YCbCrNodePrepare (const stm_blitter_operation_t &op,
                         const stm_rect_t              & __restrict src,
                         const stm_rect_t              & __restrict dst,
                         stm_bdisp_aq_planar_node      &node,
                         bool                          &bRequiresSpans);
  bool YCbCrBlitUsingSpans (const stm_blitter_operation_t &op,
                            const stm_rect_t              & __restrict src,
                            const stm_rect_t              & __restrict dst,
                            stm_bdisp_aq_planar_node      &blitNode);
  bool CopyRectFromMB     (const stm_blitter_operation_t&, const stm_rect_t&, const stm_rect_t&);
  bool CopyRectFromPlanar (const stm_blitter_operation_t&, const stm_rect_t&, const stm_rect_t&);

#ifdef DEBUG
  /* previous macroblock blit source format - YCbCr420MB, YCbCr422MB,
     SURF_YUV420, SURF_YVU420, or SURF_YUV422P */
  SURF_FMT m_LastPlanarformat;
#endif

  ULONG *m_pAQReg;
  ULONG  m_nQueueID;
  ULONG  m_nQueuePriority;
  ULONG  m_lnaInt;
  ULONG  m_nodeInt;

  DMA_Area                  m_SharedAreaDMA;
  STMFBBDispSharedAreaPriv *m_Shared;
  bool                      m_bUserspaceAQ;
  ULONG                     m_NodeOffset;

  void  WriteAQReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pAQReg + (reg>>2), val); }
  ULONG ReadAQReg(ULONG reg)             { return g_pIOS->ReadRegister(m_pAQReg + (reg>>2)); }

  CSTmBDispAQ(const CSTmBDispAQ&);
  CSTmBDispAQ& operator=(const CSTmBDispAQ&);
};

#endif // _STM_BDISP_AQ_H
