/***********************************************************************
 *
 * File: STMCommon/stmbdispoutput.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
 * This class implements the generic part of an output that "mixes" virtual
 * display planes using the BDisp blit compositor.
 * 
\***********************************************************************/

#ifndef _STM_BDISP_OUTPUT_H
#define _STM_BDISP_OUTPUT_H

#include <Generic/Output.h>
#include "stmbdispregs.h"

class CDisplayDevice;
class CSTmVirtualPlane;

#define STM_NUM_VIRTUAL_PLANES 15

/*
 * blit node used for nodelist head and virtual mixer background
 * fill operations as well as fast blit (no alpha,no rescale) copies.
 */
struct stm_bdispout_fast_blit_node 
{
  BDISP_GROUP_0;
  BDISP_GROUP_1;
  BDISP_GROUP_2;
  BDISP_GROUP_3;
  BDISP_GROUP_14;
};

struct stm_bdispout_complex_blit_node
{
  BDISP_GROUP_0;
  BDISP_GROUP_1;
  BDISP_GROUP_2;
  BDISP_GROUP_3;
  BDISP_GROUP_4;
  BDISP_GROUP_6;
  BDISP_GROUP_7;
  BDISP_GROUP_8;
  BDISP_GROUP_9;
  BDISP_GROUP_11;
  BDISP_GROUP_14;
  BDISP_GROUP_15;
};


class CSTmBDispOutput: public COutput
{
public:
  CSTmBDispOutput(CDisplayDevice *pDev,
                  COutput        *pMasterOutput,
                  ULONG          *pBDispBaseAddr,
                  ULONG           BDispPhysAddr,
                  ULONG           CQOffset,
                  int             qNumber,
                  int             qPriority);

  virtual ~CSTmBDispOutput();

  virtual bool Create(void);

  ULONG GetCapabilities(void) const;

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  bool CanShowPlane(stm_plane_id_t planeID);
  bool ShowPlane   (stm_plane_id_t planeID);
  void HidePlane   (stm_plane_id_t planeID);
 
  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

  CSTmVirtualPlane *GetVirtualPlane(stm_plane_id_t planeID) const;

  bool HandleInterrupts(void);
  void UpdateHW(void);
  
  bool CreateCQNodes(const stm_display_buffer_t * const pBuffer,
                     DMA_Area                   &dma_area,
                     ULONG                       alphaRamp);

protected:
  ULONG *m_pBDispReg;
  ULONG *m_pCQReg;
  ULONG  m_ulStaticTargetPhysAddr;
  ULONG  m_ulQueueID;
  ULONG  m_ulQueuePriority;
  ULONG  m_ulRetriggerInt;

  ULONG  m_lock;
  ULONG  m_activePlanes;

  COutput *m_pMasterOutput;

  CSTmVirtualPlane *m_pPlanes[STM_NUM_VIRTUAL_PLANES];
  CSTmVirtualPlane *m_ZOrder[STM_NUM_VIRTUAL_PLANES];

  ULONG    m_buffers[2];
  ULONG    m_bufferSize;
  SURF_FMT m_bufferFormat;
  
  ULONG m_background;

  int   m_nFrontBuffer;
  bool  m_bBuffersValid;

  ULONG m_ulFieldOffset;

  DMA_Area m_systemNodes;

  struct stm_bdispout_fast_blit_node    m_fastNodeTemplate;
  struct stm_bdispout_complex_blit_node m_complexNodeTemplate;


  void  WriteBDispReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pBDispReg + (reg>>2), val); }
  ULONG ReadBDispReg(ULONG reg)             { return g_pIOS->ReadRegister(m_pBDispReg + (reg>>2)); }

  void  WriteCQReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pCQReg + (reg>>2), val); }
  ULONG ReadCQReg(ULONG reg)             { return g_pIOS->ReadRegister(m_pCQReg + (reg>>2)); }

private:
  bool CreateFastNode(const stm_display_buffer_t * const pBuffer,
                      DMA_Area                   &dma_area);
  
  
  inline bool isRGB(SURF_FMT fmt);
  inline bool isRasterYUV(SURF_FMT fmt);
  inline bool hasAlpha(SURF_FMT fmt);

  CSTmBDispOutput(const CSTmBDispOutput&);
  CSTmBDispOutput& operator=(const CSTmBDispOutput&);
};


#endif /* _STM_BDISP_OUTPUT_H */
