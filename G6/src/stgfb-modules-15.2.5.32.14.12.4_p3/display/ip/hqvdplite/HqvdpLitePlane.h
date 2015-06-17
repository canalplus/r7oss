/***********************************************************************
 *
 * File: display/ip/HqvdpLitePlane.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _HQVDPLITE_PLANE_H
#define _HQVDPLITE_PLANE_H

#include <display/ip/videoPlug/VideoPlug.h>
#include <display/generic/DisplayPlane.h>

#include <display/ip/hqvdplite/HqvdpLiteDisplayInfo.h>
#include <display/ip/hqvdplite/stmiqipeaking.h>
#include <display/ip/hqvdplite/stmiqidefaultcolor.h>
#include <display/ip/hqvdplite/stmiqile.h>
#include <display/ip/hqvdplite/stmiqicti.h>
#include <display/ip/hqvdplite/lld/c8fvp3_hqvdplite_api_struct.h>


struct HQVDPLITESetup
{
    // Specific setup for each field
    HQVDPLITE_CMD_t fieldSetup;
    VideoPlugSetup  vidPlugSetup;
};

#define HQVDPLITE_DEI_MOTION_BUFFERS 3

typedef enum {
    HQVDPLITE_DEI_MOTION_OFF,
    HQVDPLITE_DEI_MOTION_INIT,
    HQVDPLITE_DEI_MOTION_LOW_CONF,
    HQVDPLITE_DEI_MOTION_FULL_CONF
} stm_hqvdplite_HQVDPLITE_DEI_MOTION_state;


typedef struct hqvdpLiteCmd_s
{
    bool            isValid;
    HQVDPLITE_CMD_t fieldSetup;
} hqvdpLiteCmd_t;


typedef enum
{
    DEI_MODE_OFF,
    DEI_MODE_VI,        // Currently not used
    DEI_MODE_DI,
    DEI_MODE_MEDIAN,    // Currently not used
    DEI_MODE_3D,
    MAX_DEI_MODES
} stm_display_dei_mode_t;



class CHqvdpLitePlane: public CDisplayPlane
{
public:
    CHqvdpLitePlane(const char                    *name,
                    uint32_t                       id,
                    const CDisplayDevice          *pDev,
                    const stm_plane_capabilities_t caps,
                    CVideoPlug                    *pVideoPlug,
                    uint32_t                       baseAddress);
    virtual ~CHqvdpLitePlane(void);


    virtual bool Create(void);
    virtual void PresentDisplayNode(CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime);
    virtual void SwitchFieldSetup();
    virtual void ProcessLastVsyncStatus(const stm_time64_t &vsyncTime, CDisplayNode *pNodeDisplayed);
    virtual bool NothingToDisplay(void);

    virtual DisplayPlaneResults SetCompoundControl(stm_display_plane_control_t ctrl, void * newVal);
    virtual DisplayPlaneResults SetControl (stm_display_plane_control_t control, uint32_t value);
    virtual DisplayPlaneResults GetControl (stm_display_plane_control_t  control,  uint32_t *value) const;

    virtual bool GetTuningDataRevision(stm_display_plane_tuning_data_control_t ctrl, uint32_t * revision);
    virtual DisplayPlaneResults GetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * currentVal);
    virtual DisplayPlaneResults SetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * newVal);

    virtual bool IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const;
    virtual bool GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range);

    virtual TuningResults SetTuning(uint16_t service,
                                     void *inputList,
                                     uint32_t inputListSize,
                                     void *outputList,
                                     uint32_t outputListSize);

    /*
     * Power Management stuff
     */
    virtual void Resume(void);

protected:
    uint32_t *m_baseAddress;
    uint32_t *m_mbxBaseAddress;

    void  WriteMbxReg(uint32_t addr, uint32_t val) { TRC(TRC_ID_HQVDPLITE_REG, "HQVDPLITE : %08x <-  %08x", addr, val); vibe_os_write_register(m_mbxBaseAddress, addr, val); }
    uint32_t ReadMbxReg (uint32_t addr)            { uint32_t val = vibe_os_read_register(m_mbxBaseAddress, addr); TRC(TRC_ID_HQVDPLITE_REG, " HQVDPLITE : %08x  -> %08x", addr, val); return val; }

    DMA_Area m_pPreviousFromFirmware0;

private:
    CVideoPlug *            m_videoPlug;
    uint32_t                m_hqvdpClockFreqInMHz;
    bool                    m_newCmdSendToFirmware;         // TO_DO: Not used for the moment!

    //////////////////////////////////////////////////////////////
    // Information recomputed only when the context is changing

    // Array containing the programming (= the command) saved for each use case. This structure is reset when the context is changing.
    hqvdpLiteCmd_t          m_savedCmd[MAX_USE_CASES];

    CHqvdpLiteDisplayInfo   m_hqvdpLiteDisplayInfo;

    /////////////////////////////////////////////////////////////
    // IQI related
    uint32_t                 m_iqiConfig;
    uint32_t                 m_iqiDefaultColorRGB;
    uint32_t                 m_iqiDefaultColorYCbCr;
    bool                     m_iqiDefaultColorState;
    bool                     m_isIQIChanged;
    CSTmIQIPeaking           m_iqiPeaking;
    IQIPeakingSetup          m_peaking_setup;
    CSTmIQIDefaultColor      m_iqiDC;
    CSTmIQILe                m_iqiLe;
    IQILeSetup               m_le_setup;
    CSTmIQICti               m_iqiCti;


    //////////////////////////////////////////////////////////////

    // Command currently prepared. NB: It should be local to PresentDisplayNode() but it is too big to be on the Stack
    HQVDPLITE_CMD_t         m_CmdPrepared;

    uint32_t                m_FilterSetLuma;
    uint32_t                m_FilterSetChroma;

    // Motion buffers should be allocated in one shot for Secure Data Path
    // (in order to grant acces to this region only once)
    DMA_Area                m_MotionBuffersArea;
    uint32_t                m_MotionBuffersSize;

    uint32_t                m_PreviousMotionBufferPhysAddr;
    uint32_t                m_CurrentMotionBufferPhysAddr;
    uint32_t                m_NextMotionBufferPhysAddr;


    stm_hqvdplite_HQVDPLITE_DEI_MOTION_state m_MotionState;
    uint32_t                m_ulDEIPixelBufferLength;
    uint32_t                m_ulDEIPixelBufferHeight;

    bool                    m_bHasProgressive2InterlacedHW;

    /* Data collected for CRC verification*/
    Crc_t   m_HQVdpLiteCrcData;

    bool    m_bUseFMD;

    DMA_Area   m_SharedArea1;
    DMA_Area   m_SharedArea2;
    DMA_Area * m_SharedAreaPreparedForNextVSync_p;
    DMA_Area * m_SharedAreaUsedByFirmware_p;
    bool       m_bReportPixelFIFOUnderflow;

    virtual bool        IsScalingPossible              (CDisplayNode *pCurrNode, CDisplayInfo *pGenericDisplayInfo);
    virtual void        ResetEveryUseCases             (void);
    virtual bool        AdjustIOWindowsForHWConstraints(CDisplayNode * pCurrNode, CDisplayInfo *pDisplayInfo) const;

    bool IsScalingPossibleByHw(CDisplayNode            *pCurrNode,
                               CHqvdpLiteDisplayInfo   *pDisplayInfo,
                               bool                     isSrcInterlaced);

    bool IsZoomFactorOk(uint32_t        vhsrc_input_width,
                        uint32_t        vhsrc_input_height,
                        uint32_t        vhsrc_output_width,
                        uint32_t        vhsrc_output_height);

    bool IsHwProcessingTimeOk(uint32_t        vhsrc_input_width,
                              uint32_t        vhsrc_input_height,
                              uint32_t        vhsrc_output_width,
                              uint32_t        vhsrc_output_height,
                              stm_display_dei_mode_t deiMode);

    bool IsSTBusDataRateOk(CDisplayNode           *pCurrNode,
                           CHqvdpLiteDisplayInfo  *pDisplayInfo,
                           bool                    isSrcInterlaced,
                           stm_display_dei_mode_t  deiMode);

    bool GetNbrBytesPerPixel(CHqvdpLiteDisplayInfo  *pDisplayInfo,
                             bool                    isSrcInterlaced,
                             stm_display_dei_mode_t  deiMode,
                             stm_rational_t         *pNbrBytesPerPixel);

    bool ComputeSTBusDataRate(CHqvdpLiteDisplayInfo  *pDisplayInfo,
                              bool                    isSrcInterlaced,
                              uint32_t                src_width,
                              uint32_t                src_frame_height,
                              uint32_t                dst_width,
                              uint32_t                dst_frame_height,
                              stm_display_dei_mode_t  deiMode,
                              bool                    isSrcOn10bits,
                              uint32_t                verticalRefreshRate,
                              uint64_t               *pDataRateInMBperS);

    bool FillHqvdpLiteDisplayInfo(CDisplayNode * pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, bool isDisplayInterlaced);
    bool SetDynamicDisplayInfo   (CDisplayNode *pPrevNode, CDisplayNode *pCurrNode, CDisplayNode *pNextNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup, bool isPictureRepeated);

    void SetHWDisplayInfos  (CDisplayNode * pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo);

    bool PrepareCmd             (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, bool isTopFieldOnDisplay, HQVDPLITE_CMD_t *pFieldSetup);
    bool PrepareCmdTopParams    (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_TOP_Params_t * TopParams_p);
    bool PrepareCmdVc1Params    (HQVDPLITE_VC1RE_Params_t * pVC1ReParams);
    bool PrepareCmdFMDParams    (HQVDPLITE_FMD_Params_t *   pFMDParams);

    bool PrepareCmdCSDIParams   (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup);
    bool PrepareCmdHVSRCParams  (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup);
    bool PrepareCmdIQIParams    (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, bool isTopFieldOnDisplay, bool isDisplayInterlaced, HQVDPLITE_IQI_Params_t * IQIParams_p);


    bool SetPictureBaseAddresses        (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, uint32_t* LumaBaseAddress_p, uint32_t* ChromaBaseAddress_p);
    bool SetCsdiParams                  (CDisplayNode *pPrevNode, CDisplayNode *pCurrNode, CDisplayNode *pNextNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup, bool isPictureRepeated);
    void SetNextDEIField                (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup, bool SelectPrimaryPicture);
    void SetPreviousDEIField            (CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup, bool SelectPrimaryPicture);
    void Set3DDEIConfiguration          (uint32_t *DeiCtlRegisterValue);

    bool XP70Init();
    void XP70Term();
    bool InitializeHQVDPLITE();
    bool AllocateSharedDataStructure();
    void DeallocateSharedDataStructure();
    bool LoadPlugs();
    bool SetPlugsConfiguration(uint32_t *ReadPlugBaseAddress, uint32_t *WritePlugBaseAddress);
    bool LoadFirmwareFromHeaderFile();
    void WriteFieldSetup(const HQVDPLITE_CMD_t * const field);

    void SendCmd                (CHqvdpLiteDisplayInfo *pDisplayInfo, HQVDPLITE_CMD_t *pFieldSetup);
    void SendNullCmd            (void);

    void RotateMotionBuffers    (void);
    void UpdateMotionState      (void);
    stm_display_dei_mode_t SelectDEIMode(CHqvdpLiteDisplayInfo *pDisplayInfo, CDisplayNode *pPrevNode, CDisplayNode *pCurNode, CDisplayNode *pNextNode);
    bool IsIQIContextChanged(void);

    CHqvdpLitePlane(const CHqvdpLitePlane&);
    CHqvdpLitePlane& operator=(const CHqvdpLitePlane&);
};

#endif // _HQVDPLITE_PLANE_H

