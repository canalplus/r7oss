/***********************************************************************
 *
 * File: display/ip/GdpPlane.h
 * Copyright (c) 2004, 2005, 2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GDP_PLANE_H
#define _GDP_PLANE_H

#include <display/generic/DisplayPlane.h>
#include <display/ip/gdp/GdpReg.h>
#include <display/ip/gdp/GdpDisplayInfo.h>

struct GENERIC_GDP_LLU_NODE
{
	uint32_t GDPn_CTL;   /* 0x00 */
	uint32_t GDPn_AGC;   /* 0x04 */
	uint32_t GDPn_HSRC;  /* 0x08 */
	uint32_t GDPn_VPO;   /* 0x0C */
/***** Next 128 bit *************/
	uint32_t GDPn_VPS;   /* 0x10 */
	uint32_t GDPn_PML;   /* 0x14 */
	uint32_t GDPn_PMP;   /* 0x18 */
	uint32_t GDPn_SIZE;  /* 0x1C */
/***** Next 128 bit *************/
	uint32_t GDPn_VSRC;  /* 0x20 */
	uint32_t GDPn_NVN;   /* 0x24 */
	uint32_t GDPn_KEY1;  /* 0x28 */
	uint32_t GDPn_KEY2;  /* 0x2C */
/***** Next 128 bit *************/
	uint32_t GDPn_HFP;   /* 0x30 */
	uint32_t GDPn_PPT;   /* 0x34 */
	uint32_t GDPn_VFP;   /* 0x38 7109Cut3 Vertical filter pointer */
	uint32_t GDPn_CML;   /* 0x3C 7109Cut3 Clut pointer            */
};

#if !defined(DEBUG_GDPNODE_LEVEL)
#define DEBUG_GDPNODE_LEVEL 3
#endif
#define DEBUGGDP(pGDP)\
{   \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_CTL  = %#.8x  GDPn_AGC  = %#.8x", (pGDP)->GDPn_CTL,  (pGDP)->GDPn_AGC ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_HSRC = %#.8x  GDPn_VPO  = %#.8x", (pGDP)->GDPn_HSRC, (pGDP)->GDPn_VPO ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_VPS  = %#.8x  GDPn_PML  = %#.8x", (pGDP)->GDPn_VPS,  (pGDP)->GDPn_PML ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_PMP  = %#.8x  GDPn_SIZE = %#.8x", (pGDP)->GDPn_PMP,  (pGDP)->GDPn_SIZE ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_VSRC = %#.8x  GDPn_NVN  = %#.8x", (pGDP)->GDPn_VSRC, (pGDP)->GDPn_NVN  ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_KEY1 = %#.8x  GDPn_KEY2 = %#.8x", (pGDP)->GDPn_KEY1, (pGDP)->GDPn_KEY2 ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_HFP  = %#.8x  GDPn_PPT  = %#.8x", (pGDP)->GDPn_HFP,  (pGDP)->GDPn_PPT ); \
  TRC( TRC_ID_UNCLASSIFIED, "  GDPn_VFP  = %#.8x  GDPn_CML  = %#.8x", (pGDP)->GDPn_VFP,  (pGDP)->GDPn_CML ); \
}

typedef struct
{
  bool                      isValid;
  GENERIC_GDP_LLU_NODE      topNode;
  GENERIC_GDP_LLU_NODE      botNode;
  stm_buffer_presentation_t info;
  BufferNodeType            nodeType;
  uint32_t                  pictureId;
  bool                      areIOWindowsValid;
} GdpSetup_t;

class CGdpPlane: public CDisplayPlane
{
  friend class CVBIPlane;
  public:
    CGdpPlane(const char     *name,
              uint32_t        id,
              const CDisplayDevice *pDev,
              const stm_plane_capabilities_t caps,
              uint32_t        baseAddr,
              bool            bHasClut = true);

    ~CGdpPlane();

    virtual bool GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range);
    virtual bool IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const;

    virtual bool Create(void);

    virtual DisplayPlaneResults SetControl(stm_display_plane_control_t control, uint32_t value);
    virtual DisplayPlaneResults GetControl(stm_display_plane_control_t control, uint32_t *value) const;

    virtual void ClearContextFlags                   (void);
    virtual void PresentDisplayNode(CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime);


    bool SetFirstGDPNodeOwner(CGdpPlane *);
    CGdpPlane *GetFirstGDPNodeOwner(void) const { return m_FirstGDPNodeOwner; }
    CGdpPlane *GetNextGDPNodeOwner(void) const { return m_NextGDPNodeOwner; }
    bool IsFirstGDPNodeOwner(void) const { return (m_FirstGDPNodeOwner == this); }
    bool IsLastGDPNodeOwner(void) const { return (m_NextGDPNodeOwner == 0); }

    bool isNVNInNodeList(uint32_t nvn, int list);

    uint32_t GetHWNVN(void) { return vibe_os_read_register(m_GDPBaseAddr, GDPn_NVN_OFFSET); }

    /*
     * Power Managment stuff
     */
    virtual void Freeze(void);
    virtual void Resume(void);

  protected:
    CGdpPlane(
        const char          *name,
        uint32_t             id,
        const stm_plane_capabilities_t caps,
        CGdpPlane *linktogdp);

    bool setNodeColourFmt(GENERIC_GDP_LLU_NODE       &topNode,
                          GENERIC_GDP_LLU_NODE       &botNode);

    virtual bool setNodeColourKeys(GENERIC_GDP_LLU_NODE       &topNode,
                                   GENERIC_GDP_LLU_NODE       &botNode);

    bool setNodeAlphaGain(GENERIC_GDP_LLU_NODE       &topNode,
                          GENERIC_GDP_LLU_NODE       &botNode);

    virtual bool setNodeResizeAndFilters(GENERIC_GDP_LLU_NODE     &topNode,
                                         GENERIC_GDP_LLU_NODE     &botNode);

    virtual bool setOutputViewport(GENERIC_GDP_LLU_NODE           &topNode,
                           GENERIC_GDP_LLU_NODE                   &botNode);

    bool setMemoryAddressing(GENERIC_GDP_LLU_NODE                 &topNode,
                             GENERIC_GDP_LLU_NODE                 &botNode);

    bool  setNodeFlickerFilter(GENERIC_GDP_LLU_NODE               &topNode,
                              GENERIC_GDP_LLU_NODE                &botNode);

    void WriteConfigForNextVsync(GENERIC_GDP_LLU_NODE *nextfieldsetup,
                                 uint32_t              nextflags);
    void writeFieldSetup(GENERIC_GDP_LLU_NODE *fieldsetup, uint32_t flags);

    void updateBaseAddress(void);

    void updateColorKeyState (stm_color_key_config_t       * const dst,
                              const stm_color_key_config_t * const src) const;

    void CalculateViewport(void);

    void CalculateHorizontalScaling(void);

    void CalculateVerticalScaling(void);

    void AdjustBufferInfoForScaling(void);
    virtual bool   AdjustIOWindowsForHWConstraints      (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const;

    bool FillDisplayInfo(void);

    void ResetGdpSetup(void);

    bool PrepareGdpSetup(void);
    bool SetupDynamicGdpSetup(void);

    void ApplyGdpSetup(bool isDisplayInterlaced,
                       bool isTopFieldOnDisplay,
                 const stm_time64_t &vsyncTime);

    bool SetupProgressiveNode(void);

    bool SetupSimpleInterlacedNode(void);

    virtual void createDummyNode(GENERIC_GDP_LLU_NODE *);
    //virtual bool RegisterStatistics(void);
    uint32_t PackRegister(uint16_t hi,uint16_t lo) const { return (hi<<16) | lo; }

    CGdpPlane *m_FirstGDPNodeOwner;
    CGdpPlane *m_NextGDPNodeOwner;

    DMA_Area m_HFilter;
    DMA_Area m_VFilter;
    DMA_Area m_FlickerFilter;
    DMA_Area m_Registers[2];
    DMA_Area m_DummyBuffer;

    stm_plane_ff_state_t m_FlickerFilterState;
    stm_plane_ff_mode_t  m_FlickerFilterMode;

    bool  m_bHasVFilter;
    bool  m_bHasFlickerFilter;
    bool  m_bHasClut;
    bool  m_bHas4_13_precision; /* otherwise 4.8 */
    bool  m_b4k2k;
    uint32_t *m_GDPBaseAddr;

    uint32_t m_ulGain;
    uint32_t m_ulAlphaRamp;
    uint32_t m_ulStaticAlpha[2];

    uint32_t m_ulDirectBaseAddress;
    uint32_t m_lastBufferAddr;
    stm_color_key_config_t m_ColorKeyConfig;

    /* Retreive previous plane visibility state when Plane is frozen */
    bool            m_wasEnabled;

    CDisplayNode   *m_pNodeToDisplay;
    CGdpDisplayInfo m_gdpDisplayInfo;

    GdpSetup_t      m_NextGdpSetup;
    GdpSetup_t      m_CurrentGdpSetup;

    bool  m_IsTransparencyChanged;
    bool  m_IsGainChanged;
    bool  m_IsColorKeyChanged;

  private:
    CGdpPlane(const CGdpPlane&);
    CGdpPlane& operator=(const CGdpPlane&);
    void EnableHW(void);
    void DisableHW(void);

    DMA_Area *GetGDPRegisters(unsigned int index) { return (index<2)?&m_Registers[index]:0; }
    void InitializeState(bool bHasClut);
};


#endif // _GDP_PLANE_H
