/***********************************************************************
 *
 * File: Gamma/DEIVideoPipe.h
 * Copyright (c) 2006-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _DEI_VIDEO_PIPE_H
#define _DEI_VIDEO_PIPE_H

#include <Gamma/GammaCompositorVideoPlane.h>
#include <Gamma/VDPFilter.h>
#include <STMCommon/fmdsw.h>
#define FMD_STATUS_DEBUG


struct DEIFieldSetup
{
    ULONG CTL;
    ULONG LUMA_BA;
    ULONG CHROMA_BA;
    ULONG PREV_LUMA_BA;
    ULONG PREV_CHROMA_BA;
    ULONG NEXT_LUMA_BA;
    ULONG NEXT_CHROMA_BA;

    ULONG LUMA_XY;
    ULONG LUMA_VSRC;
    ULONG CHR_VSRC;
    ULONG DEIVP_SIZE;

    ULONG CURRENT_MOTION;
    ULONG NEXT_MOTION;
    ULONG PREVIOUS_MOTION;

    bool bFieldSeen;
    bool bSwapPreviousNextAddr;
};


struct DEISetup
{
    ULONG T3I_CTL;
    ULONG SRC_CTL;
    ULONG MB_STRIDE;

    ULONG LUMA_FORMAT;
    ULONG LUMA_LINE_STACK[5];
    ULONG LUMA_PIXEL_STACK[3];

    ULONG CHROMA_FORMAT;
    ULONG CHROMA_LINE_STACK[5];
    ULONG CHROMA_PIXEL_STACK[3];

    ULONG MOTION_LINE_STACK;
    ULONG MOTION_PIXEL_STACK;

    ULONG LUMA_SIZE;
    ULONG CHR_SIZE;
    ULONG LUMA_HSRC;
    ULONG CHR_HSRC;

    ULONG PDELTA;
    ULONG NLZZD; // As output is always 4:4:4 this is used for Luma and Chroma


    ULONG TARGET_SIZE;
    ULONG BLOCK_NUMBER;

    ULONG P2I_PXF_CFG;

    // Specific setup for each field
    DEIFieldSetup topField;
    DEIFieldSetup botField;
    DEIFieldSetup altTopField;
    DEIFieldSetup altBotField;

    VideoPlugSetup vidPlugSetup;

    // Filter coefficients
    const ULONG *hfluma;
    const ULONG *hfchroma;
    const ULONG *vfluma;
    const ULONG *vfchroma;
};


typedef enum {
    DEI_MOTION_OFF,
    DEI_MOTION_INIT,
    DEI_MOTION_LOW_CONF,
    DEI_MOTION_FULL_CONF
} stm_dei_motion_state;


#define DEI_MOTION_BUFFERS 3

class CDEIVideoPipe: public CGammaCompositorVideoPlane
{
friend class CSTi7200FlexVP;
public:
    CDEIVideoPipe(stm_plane_id_t GDPid, CGammaVideoPlug *, ULONG deiBaseAddr);
    virtual ~CDEIVideoPipe(void);

    virtual bool QueueBuffer(const stm_display_buffer_t * const pFrame,
                             const void                 * const user);
    virtual void UpdateHW(bool isDisplayInterlaced,
                          bool isTopFieldOnDisplay,
                          const TIME64 &vsyncTime);

    virtual bool SetControl (stm_plane_ctrl_t control,
                             ULONG            value);
    virtual bool GetControl (stm_plane_ctrl_t  control,
                             ULONG            *value) const;

    virtual bool Create(void);

    virtual bool Hide(void);
    virtual bool Show(void);

protected:
    bool                     m_bHaveDeinterlacer;

    int                      m_nMBChunkSize;
    int                      m_nRasterChunkSize;
    int                      m_nPlanarChunkSize;
    int                      m_nMotionOpcodeSize;
    int                      m_nRasterOpcodeSize;

    STM_DMA_AREA_ALLOC_FLAGS m_ulMotionDataFlag;
    /* max DEI resolution (to be setup by derived classes) */
    int                      m_nDEIPixelBufferLength;
    int                      m_nDEIPixelBufferHeight;
    /* DEI_CTL (to be setup by derived classes) */
    ULONG                    m_nDEICtlSetupMotionSD;
    ULONG                    m_nDEICtlSetupMotionHD;
    ULONG                    m_nDEICtlSetupMedian;
    ULONG                    m_nDEICtlMDModeShift;
    ULONG                    m_nDEICtlLumaInterpShift;
    ULONG                    m_nDEICtlChromaInterpShift;
    ULONG                    m_nDEICtlMainModeDEI; /* as opposed to field merging */

    /* FMD properties (to be setup by derived classes) */
    FMDSW_FmdInitParams_t       *m_fmd_init_params;
    bool                         m_bHaveFMDBlockOffset;
    ULONG                        m_FMDEnable;
    UCHAR                        m_MaxFMDBlocksPerLine;
    UCHAR                        m_MaxFMDBlocksPerField;
    ULONG                        m_FMDStatusSceneCountMask;
    ULONG                        m_FMDStatusMoveStatusMask;
    ULONG                        m_FMDStatusRepeatStatusMask;

    void DisableHW(void);

    bool PrepareNodeForQueueing(const stm_display_buffer_t * const pFrame,
                                const void                 * const user,
                                DEISetup                   &displayNode,
                                GAMMA_QUEUE_BUFFER_INFO    &qbinfo);
    bool QueueNode(const stm_display_buffer_t    * const pFrame,
                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                   DEISetup                      &displayNode,
                   ULONG                          node_size);

private:
    DMA_Area             m_MotionData[DEI_MOTION_BUFFERS];
    int                  m_nCurrentMotionData;
    stm_dei_motion_state m_MotionState;

    bool                 m_bUseDeinterlacer;
    bool                 m_bUseMotion; /* if not, use median */
    bool                 m_bUseFMD;

    StmPlaneCtrlHideMode m_eHideMode;

    bool                 m_bReportPixelFIFOUnderflow;

    VDPFilter            m_VDPFilter;

#ifdef FMD_STATUS_DEBUG
    bool                   m_FMD_Film;
    unsigned char          m_FMD_pattern;
    int                    m_FMD_Still;
    FMDSW_FmdHwThreshold_t m_FMD_HWthresh;
    FMDSW_FmdHwWindow_t    m_FMD_HWwindow;
    FMDSW_FmdFieldInfo_t   m_FMD_FieldInfo;
#endif

    bool getQueueBufferInfo(const stm_display_buffer_t * const pFrame,
                            const void                 * const user,
                            GAMMA_QUEUE_BUFFER_INFO    &qbinfo) const;

    void selectScalingFilters(DEISetup                      &displayNode,
                              const stm_display_buffer_t    * const pFrame,
                              const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    void setupHorizontalScaling(DEISetup                      &displayNode,
                                const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void calculateVerticalFilterSetup(DEIFieldSetup           &field,
                                const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                                      int                      adjustment,
                                      bool                     isLumaFilter) const;

    void setupProgressiveVerticalScaling(DEISetup                       &displayNode,
                                         const GAMMA_QUEUE_BUFFER_INFO  &qbinfo) const;


    void setupDeInterlacedVerticalScaling(DEISetup                      &displayNode,
                                          const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void setupHWDeInterlacedVerticalScaling(DEISetup                      &displayNode,
                                            const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    void allocateMotionBuffers(DEIFieldSetup &field, DEIFieldSetup &altfield);

    void setupInterlacedVerticalScaling(DEISetup                        &displayNode,
                                        const GAMMA_QUEUE_BUFFER_INFO   &qbinfo) const;

    void set420MBMemoryAddressing(DEISetup                      &displayNode,
                                  const stm_display_buffer_t    * const pFrame,
                                  const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void set422RInterleavedMemoryAddressing(DEISetup                      &displayNode,
                                            const stm_display_buffer_t    * const pFrame,
                                            const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void setPlanarMemoryAddressing(DEISetup                      &displayNode,
                                   const stm_display_buffer_t    * const pFrame,
                                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    DEIFieldSetup *SelectFieldSetup(bool            isTopFieldOnDisplay,
                                    stm_plane_node &displayNode) const;

    void NextDEIFieldSetup    (DEISetup *owner, bool isTopNode, bool streamChanged);
    void PreviousDEIFieldSetup(DEISetup *owner, bool isTopNode, bool streamChanged) const;
    void UpdateDEIStateMachine(const DEIFieldSetup *field, DEISetup *owner);

    void WriteCommonSetup (const DEISetup      * const setup);
    void WriteFieldSetup  (const DEIFieldSetup * const field);

private:
    CDEIVideoPipe(const CDEIVideoPipe&);
    CDEIVideoPipe& operator=(const CDEIVideoPipe&);
};

#endif // _DEI_VIDEO_PIPE_H
