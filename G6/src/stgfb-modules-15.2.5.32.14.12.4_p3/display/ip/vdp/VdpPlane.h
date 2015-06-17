/***********************************************************************
 *
 * File: display/ip/vdp/VdpPlane.h
 * Copyright (c) 2014 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public License.
 * See the file COPYING in the main directory of this archive formore details.
 *
\***********************************************************************/

#ifndef _VDP_PLANE_H_
#define _VDP_PLANE_H_

#include <display/generic/DisplayPlane.h>
#include <display/ip/videoPlug/VideoPlug.h>
#include <display/ip/vdp/VdpFilter.h>
#include <display/ip/vdp/VdpDisplayInfo.h>

struct HVSRCState
{
  uint32_t lumaHInc;
  uint32_t lumaVInc;
  int32_t lumaHPhase;
  int32_t lumaVPhase;
  int32_t chromaHPhase;
  int32_t chromaVPhase;
  uint32_t vc1Flag;
  uint32_t vc1LumaType;
  uint32_t vc1ChromaType;
};

struct VdpFieldSetup
{
  uint32_t dei_ctl;
  uint32_t luma_ba;
  uint32_t chroma_ba;
  uint32_t luma_xy;
  uint32_t luma_vsrc;
  uint32_t chroma_vsrc;
  uint32_t vp_size;
  bool bFieldSeen;
};

struct VdpSetup
{
  uint32_t dei_t3i_ctl;
  uint32_t vhsrc_ctl;
  uint32_t mb_stride;
  uint32_t luma_format;
  uint32_t luma_line_stack[5];
  uint32_t luma_pixel_stack[3];
  uint32_t chroma_format;
  uint32_t chroma_line_stack[5];
  uint32_t chroma_pixel_stack[3];
  uint32_t luma_size;
  uint32_t chroma_size;
  uint32_t luma_hsrc;
  uint32_t chroma_hsrc;
  uint32_t target_size;
  // Specific setup for each field
  VdpFieldSetup * pFieldSetup;
  VdpFieldSetup topField; // top field displayed on top vsync
  VdpFieldSetup botField; // bottom field displayed on bottom vsync
  VdpFieldSetup altTopField; // bottom field displayed on top vsync
  VdpFieldSetup altBotField; // top field displayed on bottom vsync
  VideoPlugSetup vidPlugSetup;
  // Filter coefficients
  const uint32_t * hfluma;
  const uint32_t * hfchroma;
  const uint32_t * vfluma;
  const uint32_t * vfchroma;
};

typedef struct VdpUseCase_s
{
  bool isValid;
  VdpSetup setup;
} VdpUseCase_t;

class CVdpPlane: public CDisplayPlane
{
 public:

  CVdpPlane( const char * name,
             uint32_t id,
             const CDisplayDevice * pDev,
             const stm_plane_capabilities_t caps,
             CVideoPlug * pVideoPlug,
             uint32_t baseAddr );
  ~CVdpPlane( void );
  bool Create( void );

  DisplayPlaneResults GetControl( stm_display_plane_control_t control,
                                  uint32_t * currentVal ) const;
  DisplayPlaneResults SetControl( stm_display_plane_control_t control,
                                  uint32_t newVal );
  bool GetControlRange( stm_display_plane_control_t selector,
                        stm_display_plane_control_range_t * range );
  bool GetCompoundControlRange( stm_display_plane_control_t selector,
                                stm_compound_control_range_t * range );
  void DisableHW( void );
  TuningResults SetTuning( uint16_t service,
                           void * inputList,
                           uint32_t inputListSize,
                           void * outputList,
                           uint32_t outputListSize );
  void ProcessLastVsyncStatus( const stm_time64_t &vsyncTime,
                               CDisplayNode * pNodeDisplayed );
  void PresentDisplayNode( CDisplayNode *pPrevNode,
                           CDisplayNode *pCurrNode,
                           CDisplayNode *pNextNode,
                           bool isPictureRepeated,
                           bool isDisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const stm_time64_t &vsyncTime );
  bool NothingToDisplay(void);

 protected:
  virtual bool   AdjustIOWindowsForHWConstraints(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const;

  uint8_t m_MBChunkSize;
  uint8_t m_PlanarChunkSize;
  uint8_t m_RasterChunkSize;
  uint8_t m_RasterOpcodeSize;

 private:

  CVdpPlane( const CVdpPlane& );
  CVdpPlane& operator = ( const CVdpPlane& );
  uint32_t ReadRegister( uint32_t reg ) { uint32_t __val = vibe_os_read_register( m_baseAddress, reg ); TRC(TRC_ID_VDP_REG, " VDP : %08x  -> %08x", (reg), __val); return __val; }
  void WriteRegister( uint32_t reg, uint32_t val ) { TRC(TRC_ID_VDP_REG, "VDP : %08x <-  %08x", (reg), val); vibe_os_write_register( m_baseAddress, reg, val ); }
  bool FillDisplayInfo();
  bool PrepareSetup();
  bool SetDynamicDisplayInfo();
  void WriteSetup();
  void SetSrcDisplayInfos();
  void SetHWDisplayInfos();
  void AdjustBufferInfoForScaling();
  void setControlRegister();
  void set420MBMemoryAddressing_staticPart();
  void set420MBMemoryAddressing_dynamicPart();
  void set422RInterleavedMemoryAddressing_staticPart();
  void set422RInterleavedMemoryAddressing_dynamicPart();
  void setPlanarMemoryAddressing_staticPart();
  void setPlanarMemoryAddressing_dynamicPart();
  void setRaster2BufferMemoryAddressing_staticPart();
  void setRaster2BufferMemoryAddressing_dynamicPart();
  void setupProgressiveVerticalScaling();
  void setupDeinterlacedVerticalScaling();
  void setupInterlacedVerticalScaling();
  void setupHorizontalScaling();
  void selectScalingFilters();
  void ResetEveryUseCases(void);
  int ScaleVerticalSamplePosition( int pos );
  void calculateVerticalFilterSetup( VdpFieldSetup &fieldSetup,
                                     int firstSampleOffset,
                                     bool doLumaFilter );
  void CalculateHorizontalScaling();
  void CalculateVerticalScaling();
  void CalculateViewport();
  VdpFieldSetup * SelectFieldSetup();

  bool IsScalingPossible(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo);

  bool IsZoomFactorOk(uint32_t vhsrc_input_width,
                      uint32_t vhsrc_input_height,
                      uint32_t vhsrc_output_width,
                      uint32_t vhsrc_output_height);


  bool IsScalingPossibleByHw(CDisplayNode    *pNodeToDisplay,
                                        CDisplayInfo    *pDisplayInfo,
                                        bool             isSrcInterlaced);

  bool IsSTBusDataRateOk(CDisplayNode           *pNodeToDisplay,
                         CDisplayInfo           *pDisplayInfo,
                         bool                    isSrcInterlaced);

  bool GetNbrBytesPerPixel(CDisplayInfo           *pDisplayInfo,
                           bool                    isSrcInterlaced,
                           stm_rational_t         *pNbrBytesPerPixel);

  bool ComputeSTBusDataRate(CDisplayInfo           *pDisplayInfo,
                            bool                    isSrcInterlaced,
                            uint32_t                src_width,
                            uint32_t                src_frame_height,
                            uint32_t                dst_width,
                            uint32_t                dst_frame_height,
                            bool                    isSrcOn10bits,
                            uint32_t                verticalRefreshRate,
                            uint64_t               *pDataRateInMBperS);

  /* 2 set of buffers are allocated. One is currently in use and one can be used to prepare a new filter set */
  DMA_Area m_HFilter[2];
  DMA_Area m_VFilter[2];
  uint8_t m_idxFilter;  /* Indicates which filter set (0 or 1) is currently in use */
  uint32_t * m_baseAddress;
  CVideoPlug * m_videoPlug;
  Crc_t m_CrcData; // Data collected for CRC verification
  CVdpFilter m_Filter;
  CVdpDisplayInfo m_vdpDisplayInfo;
  VdpUseCase_t m_useCase[MAX_USE_CASES];
  CDisplayNode * m_pCurrNode;
  bool m_isPictureRepeated;
  bool m_isTopFieldOnDisplay;
  VdpSetup * m_pSetup;
};

#endif
