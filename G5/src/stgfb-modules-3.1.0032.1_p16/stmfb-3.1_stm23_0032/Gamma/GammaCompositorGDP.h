/***********************************************************************
 *
 * File: Gamma/GammaCompositorGDP.h
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_COMPOSITOR_GDP_H
#define _GAMMA_COMPOSITOR_GDP_H

#include "GammaCompositorPlane.h"
#include "GenericGammaReg.h"

struct GENERIC_GDP_LLU_NODE
{
	ULONG GDPn_CTL;   /* 0x00 */
	ULONG GDPn_AGC;   /* 0x04 */
	ULONG GDPn_HSRC;  /* 0x08 */
	ULONG GDPn_VPO;   /* 0x0C */
/***** Next 128 bit *************/
	ULONG GDPn_VPS;   /* 0x10 */
	ULONG GDPn_PML;   /* 0x14 */
	ULONG GDPn_PMP;   /* 0x18 */
	ULONG GDPn_SIZE;  /* 0x1C */
/***** Next 128 bit *************/
	ULONG GDPn_VSRC;  /* 0x20 */
	ULONG GDPn_NVN;   /* 0x24 */
	ULONG GDPn_KEY1;  /* 0x28 */
	ULONG GDPn_KEY2;  /* 0x2C */
/***** Next 128 bit *************/
	ULONG GDPn_HFP;   /* 0x30 */
	ULONG GDPn_PPT;   /* 0x34 */
	ULONG GDPn_VFP;   /* 0x38 7109Cut3 Vertical filter pointer */
	ULONG GDPn_CML;   /* 0x3C 7109Cut3 Clut pointer            */
};

#if !defined(DEBUG_GDPNODE_LEVEL)
#define DEBUG_GDPNODE_LEVEL 3
#endif
#define DEBUGGDP(pGDP)\
{   \
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_CTL  = %#.8lx  GDPn_AGC  = %#.8lx\n", (pGDP)->GDPn_CTL,  (pGDP)->GDPn_AGC));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_HSRC = %#.8lx  GDPn_VPO  = %#.8lx\n", (pGDP)->GDPn_HSRC, (pGDP)->GDPn_VPO));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_VPS  = %#.8lx  GDPn_PML  = %#.8lx\n", (pGDP)->GDPn_VPS,  (pGDP)->GDPn_PML));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_PMP  = %#.8lx  GDPn_SIZE = %#.8lx\n", (pGDP)->GDPn_PMP,  (pGDP)->GDPn_SIZE));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_VSRC = %#.8lx  GDPn_NVN  = %#.8lx\n", (pGDP)->GDPn_VSRC, (pGDP)->GDPn_NVN ));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_KEY1 = %#.8lx  GDPn_KEY2 = %#.8lx\n", (pGDP)->GDPn_KEY1, (pGDP)->GDPn_KEY2));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_HFP  = %#.8lx  GDPn_PPT  = %#.8lx\n", (pGDP)->GDPn_HFP,  (pGDP)->GDPn_PPT));\
  DEBUGF2(DEBUG_GDPNODE_LEVEL,("  GDPn_VFP  = %#.8lx  GDPn_CML  = %#.8lx\n", (pGDP)->GDPn_VFP,  (pGDP)->GDPn_CML));\
}

class CGammaCompositorGDP: public CGammaCompositorPlane
{
  friend class CVBIPlane;
  public:
    CGammaCompositorGDP(stm_plane_id_t GDPid, ULONG baseAddr, ULONG PKZVal, bool bHasClut = true);
    ~CGammaCompositorGDP();

    virtual bool Create(void);

    virtual bool QueueBuffer(const stm_display_buffer_t * const pBuffer,
                             const void                 * const user);

    virtual void UpdateHW (bool isDisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const TIME64 &vsyncTime);

    virtual bool SetControl(stm_plane_ctrl_t control, ULONG value);
    virtual bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

    DMA_Area *GetGDPRegisters(unsigned int index) { return (index<2)?&m_Registers[index]:0; }

    void EnableHW(void);
    void DisableHW(void);

    bool SetFirstGDPNodeOwner(CGammaCompositorGDP *);
    CGammaCompositorGDP *GetFirstGDPNodeOwner(void) const { return m_FirstGDPNodeOwner; }
    CGammaCompositorGDP *GetNextGDPNodeOwner(void) const { return m_NextGDPNodeOwner; }
    bool IsFirstGDPNodeOwner(void) const { return (m_FirstGDPNodeOwner == this); }
    bool IsLastGDPNodeOwner(void) const { return (m_NextGDPNodeOwner == 0); }

    bool isNVNInNodeList(ULONG nvn, int list);

    ULONG GetHWNVN(void) { return g_pIOS->ReadRegister((volatile ULONG*)(m_GDPBaseAddr+GDPn_NVN_OFFSET)); }

  protected:
    bool setNodeColourFmt(GENERIC_GDP_LLU_NODE       &topNode,
                          GENERIC_GDP_LLU_NODE       &botNode,
                          const stm_display_buffer_t * const pBuffer);

    virtual bool setNodeColourKeys(GENERIC_GDP_LLU_NODE       &topNode,
                                   GENERIC_GDP_LLU_NODE       &botNode,
                                   const stm_display_buffer_t * const pBuffer);

    bool setNodeAlpha(GENERIC_GDP_LLU_NODE       &topNode,
                      GENERIC_GDP_LLU_NODE       &botNode,
                      const stm_display_buffer_t * const pBuffer);

    virtual bool setNodeResizeAndFilters(GENERIC_GDP_LLU_NODE          &topNode,
                                         GENERIC_GDP_LLU_NODE          &botNode,
                                         const stm_display_buffer_t    * const pBuffer,
                                         const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    virtual bool setOutputViewport(GENERIC_GDP_LLU_NODE          &topNode,
                           GENERIC_GDP_LLU_NODE          &botNode,
                           const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    bool setMemoryAddressing(GENERIC_GDP_LLU_NODE          &topNode,
                             GENERIC_GDP_LLU_NODE          &botNode,
                             const stm_display_buffer_t    * const pBuffer,
                             const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    void writeFieldSetup(GENERIC_GDP_LLU_NODE *fieldsetup, ULONG flags);

    void updateBaseAddress(void);

    void updateColorKeyState (stm_color_key_config_t       * const dst,
                              const stm_color_key_config_t * const src) const;

    virtual void createDummyNode(GENERIC_GDP_LLU_NODE *);

    CGammaCompositorGDP *m_FirstGDPNodeOwner;
    CGammaCompositorGDP *m_NextGDPNodeOwner;

    DMA_Area m_HFilter;
    DMA_Area m_VFilter;
    DMA_Area m_FlickerFilter;
    DMA_Area m_Registers[2];
    DMA_Area m_DummyBuffer;

    bool  m_bHasVFilter;
    bool  m_bHasFlickerFilter;
    bool  m_bHasClut;
    bool  m_bHas4_13_precision; /* otherwise 4.8 */
    ULONG m_GDPBaseAddr;

    ULONG m_ulGain;
    ULONG m_ulAlphaRamp;
    ULONG m_ulStaticAlpha[2];

    ULONG m_ulDirectBaseAddress;

    stm_color_key_config_t m_ColorKeyConfig;

  private:
    CGammaCompositorGDP(const CGammaCompositorGDP&);
    CGammaCompositorGDP& operator=(const CGammaCompositorGDP&);
};


#endif // _GAMMA_COMPOSITOR_GDP_H
