/***********************************************************************
 *
 * File: display/ip/GdpPlane.cpp
 * Copyright (c) 2004-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/Output.h>

#include "display/ip/gdp/GdpReg.h"
#include "display/ip/gdp/GdpDefs.h"
#include "display/ip/gdp/STRefGDPFilters.h"

#include "display/ip/gdp/GdpPlane.h"

static const stm_pixel_format_t g_surfaceFormats[] = {
  SURF_RGB565,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_ARGB8888,
  SURF_BGRA8888,
  SURF_RGB888,
  SURF_ARGB8565,
  SURF_YCBCR422R,
  SURF_CRYCB888,
  SURF_ACRYCB8888
};


static const stm_pixel_format_t g_surfaceFormatsWithClut[] = {
  SURF_RGB565,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_ARGB8888,
  SURF_BGRA8888,
  SURF_RGB888,
  SURF_ARGB8565,
  SURF_YCBCR422R,
  SURF_CRYCB888,
  SURF_ACRYCB8888,
  SURF_CLUT8,
  SURF_ACLUT88
};

static const stm_plane_feature_t g_gdpPlaneFeatures[] = {
   PLANE_FEAT_VIDEO_SCALING,
   PLANE_FEAT_SRC_COLOR_KEY,
   PLANE_FEAT_TRANSPARENCY,
   PLANE_FEAT_WINDOW_MODE
};

static const uint32_t GDP_AGC_FULL_RANGE  = 128<<GDP_AGC_GAIN_SHIFT;
/*
 * Map 0-255 to 16-235, 6.25% offset and 85.88% gain, approximately.
 */
static const uint32_t GDP_AGC_VIDEO_RANGE = (64 << GDP_AGC_CONSTANT_SHIFT) |
                                            (110<< GDP_AGC_GAIN_SHIFT);

#define GDP_HFILTER_TABLE_SIZE (NB_HSRC_FILTERS * (NB_HSRC_COEFFS + HFILTERS_ALIGN))
#define GDP_VFILTER_TABLE_SIZE (NB_VSRC_FILTERS * (NB_VSRC_COEFFS + VFILTERS_ALIGN))
#define GDP_FFILTER_TABLE_SIZE (sizeof(uint32_t)*6)
#define GDP_CLUT_SIZE          (256*sizeof(uint32_t))

CGdpPlane::CGdpPlane(const char     *name,
                     uint32_t        id,
                     const CDisplayDevice *pDev,
                     const stm_plane_capabilities_t caps,
                     uint32_t        baseAddr,
                     bool            bHasClut
                     ):CDisplayPlane(name, id, pDev, caps)
{
  TRC( TRC_ID_GDP_PLANE, "CGdpPlane::CGdpPlane %s id = %u", name, id );

  m_GDPBaseAddr = (uint32_t *)((uint8_t *)pDev->GetCtrlRegisterBase() + baseAddr);

  InitializeState(bHasClut);
  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


CGdpPlane::CGdpPlane(const char          *name,
                     uint32_t             id,
                     const stm_plane_capabilities_t caps,
                     CGdpPlane *linktogdp): CDisplayPlane(name, id, linktogdp->GetParentDevice(), caps)
{
  TRC( TRC_ID_GDP_PLANE, "CGDpPlane::CGDpPlane %s id = %u linked to %s", name, id, linktogdp->GetName() );

  m_GDPBaseAddr = linktogdp->m_GDPBaseAddr;

  InitializeState(linktogdp->m_bHasClut);

  /*
   * We need to override our configuration with the values from the GDP we
   * are linking with, which itself may have been overridden by an SoC
   * specific class. This avoids us needing to create a SoC specific version
   * of this constructor.
   */
  m_bHasVFilter        = linktogdp->m_bHasVFilter;
  m_bHasFlickerFilter  = linktogdp->m_bHasFlickerFilter;
  m_bHas4_13_precision = linktogdp->m_bHas4_13_precision;

  m_ulMaxHSrcInc   = linktogdp->m_ulMaxHSrcInc;
  m_ulMinHSrcInc   = linktogdp->m_ulMinHSrcInc;
  m_ulMaxVSrcInc   = linktogdp->m_ulMaxVSrcInc;
  m_ulMinVSrcInc   = linktogdp->m_ulMinVSrcInc;


  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


void CGdpPlane::InitializeState(bool bHasClut)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  m_bHasClut    = bHasClut;
  if(!m_bHasClut)
  {
    m_pSurfaceFormats = g_surfaceFormats;
    m_nFormats = N_ELEMENTS (g_surfaceFormats);
  }
  else
  {
    m_pSurfaceFormats = g_surfaceFormatsWithClut;
    m_nFormats = N_ELEMENTS (g_surfaceFormatsWithClut);
  }

  m_pFeatures = g_gdpPlaneFeatures;
  m_nFeatures = N_ELEMENTS (g_gdpPlaneFeatures);

  vibe_os_zero_memory(&m_HFilter,       sizeof(DMA_Area));
  vibe_os_zero_memory(&m_VFilter,       sizeof(DMA_Area));
  vibe_os_zero_memory(&m_FlickerFilter, sizeof(DMA_Area));
  vibe_os_zero_memory(&m_Registers,     sizeof(DMA_Area));
  vibe_os_zero_memory(&m_DummyBuffer,   sizeof(DMA_Area));

  m_FlickerFilterState = PLANE_FLICKER_FILTER_DISABLE;
  m_FlickerFilterMode  = PLANE_FLICKER_FILTER_SIMPLE;

  ResetGdpSetup();
  m_CurrentGdpSetup = m_NextGdpSetup;

  m_bHasVFilter            = false;
  m_bHasFlickerFilter      = false;
  m_bHas4_13_precision     = false;

  m_IsColorKeyChanged      = true;
  m_IsGainChanged          = true;
  m_IsTransparencyChanged  = true;

  /*
   * The GDP sample rate converters have an n.8 fixed point format,
   * but to get better registration with video planes we do the maths in n.13
   * and then round it before use to reduce the fixed point error between the
   * two. Not doing this is particularly noticeable with DVD menu highlights
   * when upscaling to HD output.
   */
  m_fixedpointONE     = 1<<13;

  /*
   * Do not assume scaling is available, SoC specific subclasses will
   * override this in their constructors.
   */
  m_ulMaxHSrcInc   = m_fixedpointONE;
  m_ulMinHSrcInc   = m_fixedpointONE;
  m_ulMaxVSrcInc   = m_fixedpointONE;
  m_ulMinVSrcInc   = m_fixedpointONE;

  m_ulGain           = 255;
  m_ulAlphaRamp      = 0x0000ff00;
  m_ulStaticAlpha[0] = 0;
  m_ulStaticAlpha[1] = 0x80;

  m_ColorKeyConfig.flags  = static_cast<enum StmColorKeyConfigFlags>(SCKCF_ENABLE | SCKCF_FORMAT | SCKCF_R_INFO | SCKCF_G_INFO | SCKCF_B_INFO);
  m_ColorKeyConfig.enable = 0;
  m_ColorKeyConfig.format = SCKCVF_RGB;
  m_ColorKeyConfig.r_info = SCKCCM_DISABLED;
  m_ColorKeyConfig.g_info = SCKCCM_DISABLED;
  m_ColorKeyConfig.b_info = SCKCCM_DISABLED;

  m_FirstGDPNodeOwner = 0;
  m_NextGDPNodeOwner  = 0;

  m_ulDirectBaseAddress = 0;
  m_lastBufferAddr      = 0;

  /* The maximum line step depends on the source pitch and width. The maximum
     pitch the hardware will accept is 65535, whereas the maximum source size
     is 2047. Assuming a source width of 1 pixel in CLUT8 format, we get 1024
     as maximum useful line step; for any other format this will be different.
     Therefore we do additional checks when a buffer is queued, and we adjust
     m_ulMaxLineStep dynamically, as other code uses that as basis for some
     calculation/checks ... */
  m_ulMaxLineStep = 1024;

  /*
   * Note: the scaling capabilities are calculated in the Create method as
   *       the sample rate limits may be overriden.
   */

  /*
   *  Default Input and Output Window Modes.
   */
  m_InputWindowMode  = MANUAL_MODE;
  m_OutputWindowMode = MANUAL_MODE;

  /*
   *  Default AR Conversion Mode is 'IGNORE' for GDP plane.
   */
  m_AspectRatioConversionMode = ASPECT_RATIO_CONV_IGNORE;

  m_wasEnabled = false;
  m_b4k2k      = false;

  m_gdpDisplayInfo.Reset();
  m_pNodeToDisplay = 0;

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


CGdpPlane::~CGdpPlane(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  vibe_os_free_dma_area(&m_HFilter);
  vibe_os_free_dma_area(&m_VFilter);
  vibe_os_free_dma_area(&m_FlickerFilter);
  vibe_os_free_dma_area(&m_Registers[0]);
  vibe_os_free_dma_area(&m_Registers[1]);
  vibe_os_free_dma_area(&m_DummyBuffer);

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


bool CGdpPlane::GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range)
{
  range->min_val.x = 0;
  range->min_val.y = 0;
  range->min_val.width = 1;
  range->min_val.height = 24;

  range->max_val.x = 0;
  range->max_val.y = 0;
  if(m_b4k2k == false)
  {
    range->max_val.width = 1920;
    range->max_val.height = 2047;
  }
  else
  {
    range->max_val.width = 65535;
    range->max_val.height = 65535;
  }

  range->default_val.x = 0;
  range->default_val.y = 0;
  range->default_val.width = 0;
  range->default_val.height = 0;

  range->step.x = 1;
  range->step.y = 1;
  range->step.width = 1;
  range->step.height = 1;

  if(m_pOutput && (selector == PLANE_CTRL_OUTPUT_WINDOW_VALUE))
  {
    const stm_display_mode_t* pCurrentMode = m_pOutput->GetCurrentDisplayMode();

    if(pCurrentMode)
    {
      if( (m_b4k2k == true)
       || ((pCurrentMode->mode_id != STM_TIMING_MODE_4K2K29970_296703)
        && (pCurrentMode->mode_id != STM_TIMING_MODE_4K2K25000_297000)
        && (pCurrentMode->mode_id != STM_TIMING_MODE_4K2K24000_297000)
        && (pCurrentMode->mode_id != STM_TIMING_MODE_4K2K24000_297000_WIDE)
        && (pCurrentMode->mode_id != STM_TIMING_MODE_4K2K23980_296703)))
      {
        range->max_val.width  = pCurrentMode->mode_params.active_area_width - range->min_val.x;
        range->max_val.height = pCurrentMode->mode_params.active_area_height - range->min_val.y;
      }
    }
  }

  return true;
}


bool CGdpPlane::IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const
{
  bool is_supported = false;
  switch(feature)
  {
    case PLANE_FEAT_FLICKER_FILTER:
        is_supported = m_bHasFlickerFilter;
        if(applicable)
            *applicable = m_bHasFlickerFilter;
    break;

    case PLANE_FEAT_WINDOW_MODE:
        is_supported = m_bHasVFilter;
        if(applicable)
            *applicable = m_bHasVFilter;
    break;

    default:
        is_supported =  CDisplayPlane::IsFeatureApplicable(feature, applicable);
  }
  return is_supported;
}


bool CGdpPlane::Create(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  if(!CDisplayPlane::Create())
    return false;

  SetScalingCapabilities(&m_rescale_caps);

  vibe_os_allocate_dma_area(&m_HFilter, GDP_HFILTER_TABLE_SIZE, 16, SDAAF_NONE);
  if(!m_HFilter.pMemory)
  {
    TRC( TRC_ID_ERROR, "failed to allocate HFilter memory" );
    return false;
  }

  vibe_os_memcpy_to_dma_area(&m_HFilter,0,&stlayer_HSRC_Coeffs,sizeof(stlayer_HSRC_Coeffs));

  if(m_bHasVFilter)
  {
#ifdef GDP_USE_REFERENCE_FILTERS
    vibe_os_allocate_dma_area(&m_VFilter, GDP_VFILTER_TABLE_SIZE, 16, SDAAF_NONE);
#else
    vibe_os_allocate_dma_area(&m_VFilter,
                            (N_ELEMENTS (gdp_3x8_filters)
                             * GDP_FILTER_3X8_TABLE_HW_SIZE),
                            16, SDAAF_NONE);
#endif
    if(!m_VFilter.pMemory)
    {
      TRC( TRC_ID_ERROR, "failed to allocate VFilter memory" );
      return false;
    }

#ifdef GDP_USE_REFERENCE_FILTERS
    vibe_os_memcpy_to_dma_area(&m_VFilter,0,&stlayer_VSRC_Coeffs,sizeof(stlayer_VSRC_Coeffs));
#else
    unsigned int i;
    for(i = 0; i < N_ELEMENTS (gdp_3x8_filters); ++i)
      vibe_os_memcpy_to_dma_area (&m_VFilter,
                               GDP_FILTER_3X8_TABLE_HW_SIZE * i,
                               &gdp_3x8_filters[i].filter_coeffs,
                               GDP_FILTER_3X8_TABLE_SIZE);
#endif
  }

  if(m_bHasFlickerFilter)
  {
    vibe_os_allocate_dma_area(&m_FlickerFilter, GDP_FFILTER_TABLE_SIZE, 16, SDAAF_NONE);
    if(!m_FlickerFilter.pMemory)
    {
      TRC( TRC_ID_ERROR, "failed to allocate VFilter memory" );
      return false;
    }

    static uint32_t filter[4] = {
      /*
       * The flicker filter set is identical to that found in the Gamma blitter.
       */
      0x04008000, // Threshold1 = 4, Taps = 0%   ,100%,0%
      0x06106010, // Threshold2 = 6, Taps = 12.5%,75% ,12.5%
      0x08185018, // Threshold3 = 8, Taps = 19%  ,62% ,19%
      0x00204020 //                 Taps = 25%  ,50% ,25%
    };
    vibe_os_memcpy_to_dma_area(&m_FlickerFilter,0,&filter,sizeof(filter));
  }

  vibe_os_allocate_dma_area(&m_Registers[0], sizeof(GENERIC_GDP_LLU_NODE), 16, SDAAF_NONE);
  if(!m_Registers[0].pMemory)
  {
    TRC( TRC_ID_ERROR, "failed to allocate register memory" );
    return false;
  }

  vibe_os_allocate_dma_area(&m_Registers[1], sizeof(GENERIC_GDP_LLU_NODE), 16, SDAAF_NONE);
  if(!m_Registers[1].pMemory)
  {
    TRC( TRC_ID_ERROR, "failed to allocate register memory" );
    return false;
  }

  vibe_os_allocate_dma_area(&m_DummyBuffer, sizeof(uint32_t), 16, SDAAF_NONE);
  if(!m_DummyBuffer.pMemory)
  {
    TRC( TRC_ID_ERROR, "CGDpPlane::Create 'failed to allocate dummy buffer'" );
    return false;
  }

  TRC( TRC_ID_GDP_PLANE, "CGDpPlane::Create 'dummy buffer = 0x%x'",  m_DummyBuffer.ulPhysical );

  /*
   * We have to start with a valid node in memory that will safely display
   * nothing, in case we are part of bigger node list which becomes active
   * due to one of the other node owners in the list becoming active.
   */
  createDummyNode((GENERIC_GDP_LLU_NODE *)m_Registers[0].pData);
  vibe_os_flush_dma_area(&m_Registers[0], 0, sizeof(GENERIC_GDP_LLU_NODE));
  createDummyNode((GENERIC_GDP_LLU_NODE *)m_Registers[1].pData);
  vibe_os_flush_dma_area(&m_Registers[1], 0, sizeof(GENERIC_GDP_LLU_NODE));

  /*
   * This sets us as the head of the node list, which by default contains
   * a single node that will be pointed back to itself. If a subclass sets
   * m_NextGDPNodeOwner then the chain is traversed until the last node and
   * it gets pointed back to this object's node.
   */
  SetFirstGDPNodeOwner(this);


  TRCOUT( TRC_ID_GDP_PLANE, "" );
  return true;
}


bool CGdpPlane::SetFirstGDPNodeOwner(CGdpPlane *gdp)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  if(m_NextGDPNodeOwner)
  {
    if(!m_NextGDPNodeOwner->SetFirstGDPNodeOwner(gdp))
      return false;

    /*
     * In the middle of the list we link to the next in the chain.
     */
    uint32_t linkaddr = m_NextGDPNodeOwner->GetGDPRegisters(0)->ulPhysical;
    vibe_os_memcpy_to_dma_area(&m_Registers[0],
                            OFFSETOF(GENERIC_GDP_LLU_NODE,GDPn_NVN),
                            &linkaddr,
                            sizeof(linkaddr));

    linkaddr = m_NextGDPNodeOwner->GetGDPRegisters(1)->ulPhysical;
    vibe_os_memcpy_to_dma_area(&m_Registers[1],
                            OFFSETOF(GENERIC_GDP_LLU_NODE,GDPn_NVN),
                            &linkaddr,
                            sizeof(linkaddr));
  }
  else
  {
    /*
     * At the end of the list we cross link the two node chains.
     */
    uint32_t linkaddr = gdp->GetGDPRegisters(0)->ulPhysical;
    vibe_os_memcpy_to_dma_area(&m_Registers[1],
                            OFFSETOF(GENERIC_GDP_LLU_NODE,GDPn_NVN),
                            &linkaddr,
                            sizeof(linkaddr));

    TRC( TRC_ID_GDP_PLANE, "reg[1].nvn %p = %x",m_Registers[1].pData,((GENERIC_GDP_LLU_NODE *)m_Registers[1].pData)->GDPn_NVN );

    linkaddr = gdp->GetGDPRegisters(1)->ulPhysical;
    vibe_os_memcpy_to_dma_area(&m_Registers[0],
                            OFFSETOF(GENERIC_GDP_LLU_NODE,GDPn_NVN),
                            &linkaddr,
                            sizeof(linkaddr));

    TRC( TRC_ID_GDP_PLANE, "reg[0].nvn %p = %x",m_Registers[0].pData,((GENERIC_GDP_LLU_NODE *)m_Registers[0].pData)->GDPn_NVN );

  }

  m_FirstGDPNodeOwner = gdp;
  TRCOUT( TRC_ID_GDP_PLANE, "" );
  return true;
}

static inline uint32_t round_src(uint32_t val)
{
  /*
   * val is a sample rate converter step in n.13 fixed point. We want to round
   * it as if it were n.8, but leave it in n.13 format for now.
   */
  val += 1L<<4; // change bit 6, this will be the LSB in n.8
  val &= ~0x1f; // zero the bottom 5 bits that will be lost in n.8
  return val;
}


void CGdpPlane::CalculateHorizontalScaling()
{
  /*
   * Calculate the scaling factors, with one extra bit of precision so we can
   * round the result.
   */
  m_gdpDisplayInfo.m_hsrcinc = (m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.width * m_fixedpointONE * 2) / m_gdpDisplayInfo.m_dstFrameRect.width;

  if(m_gdpDisplayInfo.m_selectedPicture.isSrc420 || m_gdpDisplayInfo.m_selectedPicture.isSrc422)
  {
    /*
     * For formats with half chroma, we have to round up or down to an even
     * number, so that the chroma value which is half this value cannot lose
     * precision.
     */
    m_gdpDisplayInfo.m_hsrcinc += 1L<<1;
    m_gdpDisplayInfo.m_hsrcinc &= ~0x3;
    m_gdpDisplayInfo.m_hsrcinc >>= 1;
  }
  else
  {
    /*
     * As chroma is not an issue here just round the result and convert to
     * the correct fixed point format.
     */
    m_gdpDisplayInfo.m_hsrcinc += 1;
    m_gdpDisplayInfo.m_hsrcinc >>= 1;
  }

  bool bRecalculateDstWidth = false;

  if(m_gdpDisplayInfo.m_hsrcinc < m_ulMinHSrcInc)
  {
      m_gdpDisplayInfo.m_hsrcinc = m_ulMinHSrcInc;
      bRecalculateDstWidth = true;
  }

  if(m_gdpDisplayInfo.m_hsrcinc > m_ulMaxHSrcInc)
  {
      m_gdpDisplayInfo.m_hsrcinc = m_ulMaxHSrcInc;
      bRecalculateDstWidth = true;
  }

  if(bRecalculateDstWidth)
    m_gdpDisplayInfo.m_dstFrameRect.width  = (m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.width  * m_fixedpointONE) / m_gdpDisplayInfo.m_hsrcinc;

}


static inline uint32_t
__attribute__((const))
_MIN (uint32_t x, uint32_t y)
{
  return (x < y) ? x : y;
}

bool CGdpPlane::AdjustIOWindowsForHWConstraints(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const
{
  if(m_b4k2k == false)
  {
    /* Adjust the source rectangle */
    if(pDisplayInfo->m_primarySrcFrameRect.width > 1920)
      pDisplayInfo->m_primarySrcFrameRect.width = 1920;
    if(pDisplayInfo->m_primarySrcFrameRect.height > 2047)
      pDisplayInfo->m_primarySrcFrameRect.height = 2047;
  }

  if((m_capabilities & PLANE_CAPS_GRAPHICS_BEST_QUALITY) == 0)
  {
    /* Get minimal value of input and output rectangle */
    uint32_t width, height;

    width  = _MIN(pDisplayInfo->m_primarySrcFrameRect.width, pDisplayInfo->m_dstFrameRect.width);
    height = _MIN(pDisplayInfo->m_primarySrcFrameRect.height, pDisplayInfo->m_dstFrameRect.height);

    /* Truncate input and output to min value */
    pDisplayInfo->m_primarySrcFrameRect.width  = pDisplayInfo->m_dstFrameRect.width  = width;
    pDisplayInfo->m_primarySrcFrameRect.height = pDisplayInfo->m_dstFrameRect.height = height;
  }
    return true;
}


void CGdpPlane::CalculateVerticalScaling()
{
  unsigned long srcHeight;

  m_gdpDisplayInfo.m_line_step = 1;
  m_gdpDisplayInfo.m_verticalFilterOutputSamples = m_gdpDisplayInfo.m_dstHeight;

restart:
  srcHeight = m_gdpDisplayInfo.m_selectedPicture.srcHeight / m_gdpDisplayInfo.m_line_step;
  m_gdpDisplayInfo.m_verticalFilterInputSamples  = srcHeight;

  /*
   * Calculate the scaling factors, with one extra bit of precision so we can
   * round the result.
   */
  m_gdpDisplayInfo.m_vsrcinc = (m_gdpDisplayInfo.m_verticalFilterInputSamples * m_fixedpointONE * 2) / m_gdpDisplayInfo.m_verticalFilterOutputSamples;

  if(m_gdpDisplayInfo.m_selectedPicture.isSrc420)
  {
    /*
     * For formats with half vertical chroma, we have to round up or down to
     * an even number, so that the chroma value which is half this value
     * cannot lose precision.
     */
    m_gdpDisplayInfo.m_vsrcinc += 1L<<1;
    m_gdpDisplayInfo.m_vsrcinc &= ~0x3;
    m_gdpDisplayInfo.m_vsrcinc >>= 1;
  }
  else
  {
    /*
     * As chroma is not an issue here just round the result and convert to
     * the correct fixed point format.
     */
    m_gdpDisplayInfo.m_vsrcinc += 1;
    m_gdpDisplayInfo.m_vsrcinc >>= 1;
  }

  bool bRecalculateDstHeight = false;

  if(m_gdpDisplayInfo.m_vsrcinc < m_ulMinVSrcInc)
  {
    m_gdpDisplayInfo.m_vsrcinc = m_ulMinVSrcInc;
    bRecalculateDstHeight = true;
  }

  if(m_gdpDisplayInfo.m_vsrcinc > m_ulMaxVSrcInc)
  {
    if(m_gdpDisplayInfo.m_line_step < m_ulMaxLineStep)
    {
      ++m_gdpDisplayInfo.m_line_step;
      goto restart;
    }
    else
    {
      m_gdpDisplayInfo.m_vsrcinc = m_ulMaxVSrcInc;
      bRecalculateDstHeight = true;
    }
  }

  if(bRecalculateDstHeight)
  {
    m_gdpDisplayInfo.m_dstHeight = (m_gdpDisplayInfo.m_verticalFilterInputSamples * m_fixedpointONE) / m_gdpDisplayInfo.m_vsrcinc;
  }
}


void CGdpPlane::CalculateViewport()
{
  /*
   * Now we know the destination viewport extents for the
   * compositor/mixer, which may get clipped by the active video area of
   * the display mode.
   */
  m_gdpDisplayInfo.m_viewport.startPixel = STCalculateViewportPixel(m_gdpDisplayInfo.m_pCurrentMode, m_gdpDisplayInfo.m_dstFrameRect.x);
  m_gdpDisplayInfo.m_viewport.stopPixel  = STCalculateViewportPixel(m_gdpDisplayInfo.m_pCurrentMode, m_gdpDisplayInfo.m_dstFrameRect.x + m_gdpDisplayInfo.m_dstFrameRect.width - 1);
  /*
   * We need to limit the number of output samples generated to the
   * (possibly clipped) viewport width.
   */
  m_gdpDisplayInfo.m_horizontalFilterOutputSamples = (m_gdpDisplayInfo.m_viewport.stopPixel - m_gdpDisplayInfo.m_viewport.startPixel + 1);

  m_gdpDisplayInfo.m_viewport.startLine  = STCalculateViewportLine(m_gdpDisplayInfo.m_pCurrentMode,  m_gdpDisplayInfo.m_dstFrameRect.y);

  TRC( TRC_ID_GDP_PLANE, "samples = %u startpixel = %u stoppixel = %u",m_gdpDisplayInfo.m_horizontalFilterOutputSamples, m_gdpDisplayInfo.m_viewport.startPixel, m_gdpDisplayInfo.m_viewport.stopPixel );

  /*
   * The viewport line numbering is always frame based, even on
   * an interlaced display
   */
  m_gdpDisplayInfo.m_viewport.stopLine = STCalculateViewportLine(m_gdpDisplayInfo.m_pCurrentMode, m_gdpDisplayInfo.m_dstFrameRect.y + m_gdpDisplayInfo.m_dstFrameRect.height - 1);

  TRC( TRC_ID_GDP_PLANE, "startline = %u stopline = %u",m_gdpDisplayInfo.m_viewport.startLine,m_gdpDisplayInfo.m_viewport.stopLine );
}


void CGdpPlane::AdjustBufferInfoForScaling(void)
{

  if(!m_gdpDisplayInfo.m_isSrcInterlaced && m_gdpDisplayInfo.m_isDisplayInterlaced)
  {
    if (m_ulMaxLineStep > 1)
    {
      TRC( TRC_ID_GDP_PLANE, "using line skip (max %u)\n", m_ulMaxLineStep );
    }
    else
    {
      /*
       * we have to convert to interlaced using a
       * 2x downscale. But, if the hardware cannot do that or the overall
       * scale then goes outside the hardware capabilities, we treat the
       * source as interlaced instead.
       */

      // We convert a progressive Frame into an Interlaced Field so we should consider
      // the Src Frame Height and the dst Field Height
      bool convert_to_interlaced = (m_gdpDisplayInfo.m_dstHeight < (m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.height * m_fixedpointONE / m_ulMaxVSrcInc));

      if(convert_to_interlaced)
      {
        TRC( TRC_ID_GDP_PLANE, " converting source to interlaced for downscaling \n " );
        m_gdpDisplayInfo.m_isSrcInterlaced           = true;
        m_gdpDisplayInfo.m_selectedPicture.srcHeight = m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.height/2;
        m_gdpDisplayInfo.m_firstFieldType            = GNODE_TOP_FIELD;
      }
    }
  }

  if(m_gdpDisplayInfo.m_isSrcInterlaced)
  {
    /*
     * Change the vertical start position from frame to field coordinates
     *
     * Remember that this value is in the fixed point format.
     */
    m_gdpDisplayInfo.m_srcFrameRectFixedPointY /= 2;
  }

  CalculateHorizontalScaling();
  CalculateVerticalScaling();

  /*
   * Now adjust the source coordinate system to take into account line skipping
   */
  m_gdpDisplayInfo.m_srcFrameRectFixedPointY /= m_gdpDisplayInfo.m_line_step;

  /*
   * Define the Y coordinate limit in the source image, used to ensure we
   * do not go outside of the required source image crop when the Y position
   * is adjusted for re-scale purposes.
   */
  m_gdpDisplayInfo.m_maxYCoordinate = ((m_gdpDisplayInfo.m_srcFrameRectFixedPointY / m_fixedpointONE)
                           + m_gdpDisplayInfo.m_verticalFilterInputSamples - 1);

  CalculateViewport();
}

/* Called on Vsync */
void CGdpPlane::PresentDisplayNode( CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isDisplayInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime )
{
  bool isAddrChanged = false;

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  if (!pCurrNode || !m_outputInfo.isOutputStarted)
  {
    /*Nothing to display*/
    return;
  }

  /*If a buffer has been queued to just do a flip*/
  if ( m_lastBufferAddr != (pCurrNode->m_bufferDesc.src.primary_picture.video_buffer_addr) )
  {
    isAddrChanged = true;
    m_ContextChanged = true;
    TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new video_buffer_addr)", GetName() );
  }

  m_pNodeToDisplay = pCurrNode;

  if (!isPictureRepeated || isAddrChanged)
  {
    TRC( TRC_ID_GDP_PLANE, "presentation_time : %lld, PTS : %lld, nfields : %d",
         pCurrNode->m_bufferDesc.info.presentation_time, pCurrNode->m_bufferDesc.info.PTS, pCurrNode->m_bufferDesc.info.nfields );
    m_Statistics.PicDisplayed++;
  }
  else
  {
    m_Statistics.PicRepeated++;
  }

  if (FillDisplayInfo())
  {
    /* Only called if we have new changes */
    if(IsContextChanged(pCurrNode, isPictureRepeated))
    {
      m_lastBufferAddr = pCurrNode->m_bufferDesc.src.primary_picture.video_buffer_addr;
      ClearContextFlags();
      ResetGdpSetup();
      if(!PrepareGdpSetup())
      {
        m_NextGdpSetup.isValid = false;
        TRC( TRC_ID_GDP_PLANE ,"CGdpPlane: PrepareGdpsetup() failed !!\n");
      }
    }

    if(m_NextGdpSetup.isValid)
    {
      if(!SetupDynamicGdpSetup())
      {
        m_NextGdpSetup.isValid = false;
        TRC( TRC_ID_GDP_PLANE ,"CGdpPlane: SetupDynamicGdpsetup() failed !!\n");
      }
      TRC( TRC_ID_GDP_PLANE, "------------------------- Top Node -----------------------" );
      DEBUGGDP(&m_NextGdpSetup.topNode);
      TRC( TRC_ID_GDP_PLANE, "---------------------- Bottom Node -----------------------" );
      DEBUGGDP(&m_NextGdpSetup.botNode);
      TRC( TRC_ID_GDP_PLANE, "----------------------------------------------------------" );
    }
  }

  /* Should be called every OutputVsync */
  ApplyGdpSetup(isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);
}

void CGdpPlane::ClearContextFlags(void)
{
    /* Trigger dynamic gdp setup*/
    m_IsGainChanged                     = true;
    m_IsColorKeyChanged                 = true;
    m_IsTransparencyChanged             = true;
}

bool CGdpPlane::FillDisplayInfo()
{

  if(!m_pOutput)
    return false;

  m_gdpDisplayInfo.Reset();

  CDisplayPlane::FillDisplayInfo(m_pNodeToDisplay, &m_gdpDisplayInfo);

  m_gdpDisplayInfo.m_pCurrentMode = m_pOutput->GetCurrentDisplayMode();
  if(!m_gdpDisplayInfo.m_pCurrentMode)
    return false;

  if(m_gdpDisplayInfo.m_pCurrentMode->mode_id == STM_TIMING_MODE_RESERVED)
    return false;

  m_gdpDisplayInfo.m_isDisplayInterlaced = (m_gdpDisplayInfo.m_pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN);
  m_gdpDisplayInfo.m_firstFieldType = m_pNodeToDisplay->m_firstFieldType;

  return true;
}

bool CGdpPlane::SetupProgressiveNode(void)
{

  m_NextGdpSetup.info              = m_pNodeToDisplay->m_bufferDesc.info;
  m_NextGdpSetup.nodeType          = GNODE_PROGRESSIVE;

  m_NextGdpSetup.isValid = true;
  return true;
}

bool CGdpPlane::SetupSimpleInterlacedNode(void)

{
  /*
   * This method queues either a pair of fields in an interlaced buffer or
   * a single field in that buffer using a single display node. This covers
   * a number of different use cases:
   *
   * 1. An interlaced field pair on an interlaced display, being displayed at
   *    normal speed without any intra-field interpolation. The fields will
   *    be displayed alternately for as long as the display node is valid.
   *
   * 2. A single interlaced field on an interlaced display. The specified
   *    field will get displayed on the first valid display field, usually for
   *    just one field (it is expected that another node with the next field
   *    will be queued behind it during normal speed playback). If the display
   *    node is maintained on the display for more than one field then the
   *    behaviour depends on the interpolateFields flag. If true (i.e. for
   *    slowmotion) then an interpolated other field is displayed, otherwise
   *    the data from the other field in the buffer is displayed (this is the
   *    best choice of a bad lot, because this should only happen if the
   *    queue has starved or some correction for AV sync is being applied).
   *    This use case is intented to support per-field pan and scan vectors,
   *    where the hardware setup needs to be different for each field,
   *    including the repeated first field when doing 3/2 pulldown (eeek).
   *
   * 3. A single interlaced field on a progressive display. The specified
   *    field will be displayed for as long as the display node is valid.
   *
   */
  m_NextGdpSetup.info              = m_pNodeToDisplay->m_bufferDesc.info;
  m_NextGdpSetup.nodeType          = m_gdpDisplayInfo.m_firstFieldType;

  if(m_gdpDisplayInfo.m_repeatFirstField && !m_gdpDisplayInfo.m_firstFieldOnly)
      m_pNodeToDisplay->m_bufferDesc.info.nfields++;

  m_NextGdpSetup.isValid = true;

  return true;
}

bool CGdpPlane::PrepareGdpSetup(void)
{
  GENERIC_GDP_LLU_NODE    &topNode  = m_NextGdpSetup.topNode;
  GENERIC_GDP_LLU_NODE    &botNode  = m_NextGdpSetup.botNode;

  if(!PrepareIOWindows(m_pNodeToDisplay, &m_gdpDisplayInfo))
  {
    TRC( TRC_ID_GDP_PLANE,"CGdpPlane: PrepareIOWindows() failed !!\n");
    return false;
  }

  if(!m_gdpDisplayInfo.m_selectedPicture.pitch)
    return false;
  /*
   * Convert the source origin to fixed point format ready for setting up
   * the resize filters. Note that the incoming coordinates are in
   * multiples of a 16th of a pixel/scanline.
   */
  m_gdpDisplayInfo.m_srcFrameRectFixedPointX = ValToFixedPoint(m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.x, 16);
  m_gdpDisplayInfo.m_srcFrameRectFixedPointY = ValToFixedPoint(m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.y, 16);

  /* it is totally ok to dynamically modify m_ulMaxLineStep here, as the only
     place where it is used to calculate information that can be exported to
     the outside is in Create(), which has happened long ago. The max line
     step needs to correctly reflect the current pitch and is used here
     AdjustBufferInfoForScaling(). */
  m_ulMaxLineStep = 65535 / m_gdpDisplayInfo.m_selectedPicture.pitch;
  if (m_gdpDisplayInfo.m_isSrcInterlaced)
    m_ulMaxLineStep /= 2;

  AdjustBufferInfoForScaling();

  if(!m_bHas4_13_precision)
  {
    m_gdpDisplayInfo.m_hsrcinc        = round_src(m_gdpDisplayInfo.m_hsrcinc);
    m_gdpDisplayInfo.m_vsrcinc        = round_src(m_gdpDisplayInfo.m_vsrcinc);
  }

  TRC( TRC_ID_GDP_PLANE, "one = 0x%x hsrcinc = 0x%x vsrcinc = 0x%x",m_fixedpointONE, m_gdpDisplayInfo.m_hsrcinc, m_gdpDisplayInfo.m_vsrcinc);

  /*
   * Wait for Next VSync before loading the next node if we are the last node
   * in the node list
   */
  topNode.GDPn_CTL = botNode.GDPn_CTL = IsLastGDPNodeOwner()?GDP_CTL_WAIT_NEXT_VSYNC:0;

  /*
   * Note that the resize and filter setup, may set state in the nodes which
   * is inspected by the following calls to setOutputViewport and
   * setMemoryAddressing.
   * same dependecy between setMemoryAddressing and setNodeFlickerFilter
   * so don't reorder these!
   */
  if(!setNodeResizeAndFilters(topNode, botNode))
    return false;


  if(!setMemoryAddressing(topNode, botNode))
    return false;

  if(!setNodeFlickerFilter(topNode, botNode))
    return false;

  if(!setOutputViewport(topNode, botNode))
    return false;

  if(!setNodeColourFmt(topNode, botNode))
    return false;

  if (!m_gdpDisplayInfo.m_isSrcInterlaced)
    return SetupProgressiveNode();
  else
    return SetupSimpleInterlacedNode();
}

bool CGdpPlane::SetupDynamicGdpSetup(void)
{
  GENERIC_GDP_LLU_NODE    &topNode  = m_NextGdpSetup.topNode;
  GENERIC_GDP_LLU_NODE    &botNode  = m_NextGdpSetup.botNode;


  if(m_IsColorKeyChanged)
  {
    m_IsColorKeyChanged = false;
    if(!setNodeColourKeys(topNode, botNode))
      return false;
  }

  if (m_IsTransparencyChanged || m_IsGainChanged)
  {
    m_IsTransparencyChanged = false;
    m_IsGainChanged         = false;
    if(!setNodeAlphaGain(topNode, botNode))
      return false;
  }

  return true;
}

void CGdpPlane::ApplyGdpSetup(bool isDisplayInterlaced,
                              bool isTopFieldOnDisplay,
                              const stm_time64_t &vsyncTime)
{
  GENERIC_GDP_LLU_NODE *nextfieldsetup = 0;
  GENERIC_GDP_LLU_NODE *currentfieldsetup = 0;
  uint32_t              nextflags      = 0;
  uint32_t              currentflags   = 0;


  if(!m_NextGdpSetup.isValid)
  {
    /* Do not update HW */
    DisableHW();
    return;
  }

  if(isDisplayInterlaced)
  {
    if(isTopFieldOnDisplay)
    {
      nextfieldsetup    = &m_NextGdpSetup.botNode;
      currentfieldsetup = &m_CurrentGdpSetup.botNode;
    }
    else
    {
      nextfieldsetup    = &m_NextGdpSetup.topNode;
      currentfieldsetup = &m_CurrentGdpSetup.topNode;
    }


    nextflags = m_NextGdpSetup.info.ulFlags;
    currentflags = m_CurrentGdpSetup.info.ulFlags;

    /*
     * If we are being asked to present graphics on an interlaced display
     * then only allow changes on the top field. This is to prevent visual
     * artifacts when animating vertical movement.
     */
    if(isTopFieldOnDisplay && (m_NextGdpSetup.info.ulFlags & STM_BUFFER_PRESENTATION_GRAPHICS))
    {
      EnableHW();
      if(!m_isEnabled)
      {
        /*
        * Check if something went wrong during hardware enable, if so keep this
        * buffer on the queue to try again later.
        */
        return;
       }

      WriteConfigForNextVsync(nextfieldsetup, nextflags);
      return;
    }

    if((isTopFieldOnDisplay  && m_NextGdpSetup.nodeType == GNODE_TOP_FIELD) ||
       (!isTopFieldOnDisplay && m_NextGdpSetup.nodeType == GNODE_BOTTOM_FIELD))
    {
      TRC( TRC_ID_GDP_PLANE, "Waiting for correct field" );
      TRC( TRC_ID_GDP_PLANE, "isTopFieldOnDisplay = %s", isTopFieldOnDisplay?"true":"false" );
      TRC( TRC_ID_GDP_PLANE, "nodeType            = %s", (m_NextGdpSetup.nodeType==GNODE_TOP_FIELD)?"top":"bottom" );

      if(m_CurrentGdpSetup.isValid)
      {
        EnableHW();
        WriteConfigForNextVsync(currentfieldsetup, currentflags);
        return;
      }
      else
      {
        return;
      }
    }
  }
  else
  {
    if(m_NextGdpSetup.nodeType == GNODE_PROGRESSIVE || m_NextGdpSetup.nodeType == GNODE_TOP_FIELD)
      nextfieldsetup = &m_NextGdpSetup.topNode;
    else
      nextfieldsetup = &m_NextGdpSetup.botNode;

    nextflags = m_NextGdpSetup.info.ulFlags;
  }

  EnableHW();
  if(!m_isEnabled)
  {
      /*
       * Check if something went wrong during hardware enable, if so keep this
       * buffer on the queue to try again later.
       */
      return;
  }

  WriteConfigForNextVsync(nextfieldsetup, nextflags);

  m_CurrentGdpSetup = m_NextGdpSetup;
  return;
}

void CGdpPlane::WriteConfigForNextVsync(GENERIC_GDP_LLU_NODE *nextfieldsetup,
                                        uint32_t              nextflags)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  if(nextfieldsetup)
  {
    writeFieldSetup((GENERIC_GDP_LLU_NODE*)nextfieldsetup, nextflags);
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );
}


void CGdpPlane::writeFieldSetup(GENERIC_GDP_LLU_NODE *fieldsetup, uint32_t flags)
{

  if((flags & STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR) && (m_ulDirectBaseAddress == 0))
  {
    TRC( TRC_ID_GDP_PLANE, "Direct buffer address is NULL" );
    return;
  }

  /*
   * We need to work out which of the two register banks we should update
   * such that it will be loaded on the next frame/field. We cannot simply
   * keep a "count" because it will go wrong if we miss a vsync interrupt and
   * may go wrong resuming from a suspend. This is particularly bad in
   * interlaced display modes because you can end up with the the fields
   * reversed. All of this was why we originally only had a single node linked
   * to itself. But if we want to do updates safely when we have multiple
   * GDP windows hence a node list with multiple nodes for the same frame/field,
   * then we have to maintain two lists.
   *
   * Ideally there would be a spare bit somewhere in the GDP node that we could
   * use to mark nodes in bank 1, but there isn't, at least not one that is
   * benign, not otherwise used by us and available on all versions of the GDP
   * hardware in all the SoCs we support. So we have to do this the hard way
   * and search a chain for a node that contains the current NVN register value.
   *
   * At least the search isn't long, mostly it is a chain of one node, with VBI
   * output linked to a GDP it will be a chain of two entries.
   */
  int index = isNVNInNodeList(GetHWNVN(),0)?1:0;

  /*
   * Patch the new node into the linked list in the right place, it is OK to
   * change the NVN value in fieldsetup at this point.
   */
  GENERIC_GDP_LLU_NODE *nextnode = (GENERIC_GDP_LLU_NODE *)m_Registers[index].pData;
  fieldsetup->GDPn_NVN = nextnode->GDPn_NVN;

  uint32_t savedbaseaddr = fieldsetup->GDPn_PML;
  if(flags & STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR)
  {
    fieldsetup->GDPn_PML += m_ulDirectBaseAddress;
  }

  /* Make new node visible */
  fieldsetup->GDPn_PPT   &= ~(GDP_PPT_IGNORE_ON_MIX1 |
                              GDP_PPT_IGNORE_ON_MIX2);

  /* Write the main registers */
  vibe_os_memcpy_to_dma_area(&m_Registers[index], 0, fieldsetup, sizeof(GENERIC_GDP_LLU_NODE));
  fieldsetup->GDPn_PML = savedbaseaddr;

}

bool CGdpPlane::setNodeFlickerFilter(GENERIC_GDP_LLU_NODE       &topNode,
                                     GENERIC_GDP_LLU_NODE       &botNode)
{
  uint32_t vsrcinc  = (topNode.GDPn_VSRC & GDP_VSRC_VSRC_INCREMENT_MASK) << 5;
  uint32_t ctl      = topNode.GDPn_CTL;

  if(m_FlickerFilterState
     && m_bHasFlickerFilter
     && m_gdpDisplayInfo.m_isDisplayInterlaced)
  {
    uint32_t isVResizeEnabled = ctl & GDP_CTL_EN_V_RESIZE;
    uint32_t isYCbCrSurf = ctl & 0x10;

    /* When applying anti flicker filter with interlaced source. We need
     * to divide the pitch by 2 and multiply the height by 2.
     */
    if (!isVResizeEnabled && m_gdpDisplayInfo.m_isSrcInterlaced)
    {
      uint32_t pmp  = topNode.GDPn_PMP;
      uint32_t size = topNode.GDPn_SIZE;
      pmp = ((pmp & ~GDP_PMP_PITCH_VALUE_MASK)
             | ((pmp & GDP_PMP_PITCH_VALUE_MASK)/2));

      size = ((size & ~GDP_SIZE_HEIGHT_MASK)
              | ((size & GDP_SIZE_HEIGHT_MASK)*2));
      topNode.GDPn_PMP = pmp;
      topNode.GDPn_SIZE = size;

      pmp  = botNode.GDPn_PMP;
      size = botNode.GDPn_SIZE;

      pmp = ((pmp & ~GDP_PMP_PITCH_VALUE_MASK)
             | ((pmp & GDP_PMP_PITCH_VALUE_MASK)/2));

      size = ((size & ~GDP_SIZE_HEIGHT_MASK)
              | ((size & GDP_SIZE_HEIGHT_MASK)*2));
      botNode.GDPn_PMP = pmp;
      botNode.GDPn_SIZE = size;
    }

    /* Anti flicker filter should be enabled in two cases:
     * 1. No vertical resize is enabled.
     * 2. Progressive source with vsrcinc = m_fixedpointONE*2
     * In fact, resize is always enabled with progressive source and intelaced display.
     */
    if (!isVResizeEnabled
        || ((isVResizeEnabled && !m_gdpDisplayInfo.m_isSrcInterlaced)
            && (vsrcinc == (uint32_t)m_fixedpointONE*2)))
    {
      /* Set anti flicker filter coefficients */
      topNode.GDPn_VFP = m_FlickerFilter.ulPhysical;
      botNode.GDPn_VFP = m_FlickerFilter.ulPhysical;

      if((m_FlickerFilterMode == PLANE_FLICKER_FILTER_ADAPTIVE)
         && !isYCbCrSurf)
      {

        /* Enable Adaptive anti flicker filter */
        topNode.GDPn_VSRC |= GDP_VSRC_ADAPTIVE_FLICKERFIL;
        botNode.GDPn_VSRC |= GDP_VSRC_ADAPTIVE_FLICKERFIL;
      }

      /* The resize should be disabled for progressive source before enabling
       * the anti flicker filter.
       */
      if (isVResizeEnabled)
        ctl &= ~GDP_CTL_EN_V_RESIZE;

      /* Enable the anti flicker filter:
       * GDP_CTL_EN_VFILTER_UPD: update anti flicker filter coefficients from memory
       * GDP_CTL_EN_FLICKERFIL : enable the anti flicker filter block
       */
      ctl |= (GDP_CTL_EN_FLICKERFIL | GDP_CTL_EN_VFILTER_UPD);

      topNode.GDPn_CTL = ctl;
      botNode.GDPn_CTL = ctl;
      /* Remove the last pixel, corrupted due to anti flicker filter application.
       * Its an expected corruption. In fact, when applying the anti flicker filter
       * for the last line, we will compute its result by using the values of line+1
       * which is a random value.
       */
      uint32_t vps = topNode.GDPn_VPS;
      vps = ((vps & ~(GDP_VPS_YDS_MASK))
             | (((((vps & GDP_VPS_YDS_MASK)>>16)-1)<<16)
                & GDP_VPS_YDS_MASK));
      topNode.GDPn_VPS = vps;
      vps = botNode.GDPn_VPS;
      vps = ((vps & ~(GDP_VPS_YDS_MASK))
             | (((((vps & GDP_VPS_YDS_MASK)>>16)-1)<<16)
                & GDP_VPS_YDS_MASK));
      botNode.GDPn_VPS = vps;
    }
  }
  return true;
}

void CGdpPlane::updateBaseAddress(void)
{
  GENERIC_GDP_LLU_NODE *fieldsetup;
  const stm_display_mode_t*   pCurrentMode;

  if(!m_pOutput
     || !m_NextGdpSetup.isValid
     || !(m_NextGdpSetup.info.ulFlags & STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR))
  {
    return;
  }

  pCurrentMode = m_pOutput->GetCurrentDisplayMode();

  if(!pCurrentMode)
  {
      // Output not started
      return;
  }

  if((pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN) &&
     (m_pOutput->GetCurrentVTGEvent() & STM_TIMING_EVENT_TOP_FIELD))
  {
    fieldsetup = &m_NextGdpSetup.botNode;
  }
  else
  {
    fieldsetup = &m_NextGdpSetup.topNode;
  }

  uint32_t baseaddr = fieldsetup->GDPn_PML + m_ulDirectBaseAddress;

  int index = isNVNInNodeList(GetHWNVN(),0)?1:0;

  TRC( TRC_ID_GDP_PLANE, "Direct buffer address is 0x%x",baseaddr );

  vibe_os_memcpy_to_dma_area(&m_Registers[index],
                          OFFSETOF(GENERIC_GDP_LLU_NODE, GDPn_PML),
                          &baseaddr,
                          sizeof(uint32_t));
}


bool CGdpPlane::isNVNInNodeList(uint32_t nvn, int bank)
{
  CGdpPlane *list = GetFirstGDPNodeOwner();
  while(list != 0)
  {
    GENERIC_GDP_LLU_NODE *node = (GENERIC_GDP_LLU_NODE *)(list->GetGDPRegisters(bank)->pData);
    if(node->GDPn_NVN == nvn)
      return true;

    list = list->GetNextGDPNodeOwner();
  }
  return false;
}


bool CGdpPlane::setNodeColourFmt(GENERIC_GDP_LLU_NODE       &topNode,
                                 GENERIC_GDP_LLU_NODE       &botNode)
{
  uint32_t ulCtrl = 0;
  uint32_t alphaRange = (m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_LIMITED_RANGE_ALPHA)?0:GDP_CTL_ALPHA_RANGE;

  switch(m_gdpDisplayInfo.m_selectedPicture.colorFmt)
  {
    case SURF_RGB565:
      ulCtrl = GDP_CTL_RGB_565;
      break;

    case SURF_RGB888:
      ulCtrl = GDP_CTL_RGB_888;
      break;

    case SURF_ARGB8565:
      ulCtrl = GDP_CTL_ARGB_8565 | alphaRange;
      break;

    case SURF_ARGB8888:
      ulCtrl = GDP_CTL_ARGB_8888 | alphaRange;
      break;

    case SURF_ARGB1555:
      ulCtrl = GDP_CTL_ARGB_1555;
      break;

    case SURF_ARGB4444:
      ulCtrl = GDP_CTL_ARGB_4444;
      break;

    case SURF_YCBCR422R:
      ulCtrl = GDP_CTL_YCbCr422R;

      if(m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_COLORSPACE_709)
        ulCtrl |= GDP_CTL_709_SELECT;

      break;

    case SURF_CRYCB888:
      ulCtrl = GDP_CTL_YCbCr888;

      if(m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_COLORSPACE_709)
        ulCtrl |= GDP_CTL_709_SELECT;

      break;

    case SURF_ACRYCB8888:
      ulCtrl = GDP_CTL_AYCbCr8888 | alphaRange;

      if(m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_COLORSPACE_709)
        ulCtrl |= GDP_CTL_709_SELECT;

      break;

    case SURF_CLUT8:
      if(m_bHasClut)
      {
        ulCtrl |= GDP_CTL_CLUT8 | GDP_CTL_EN_CLUT_UPDATE;
      }
      else
      {
        TRC( TRC_ID_ERROR, "CGDpPlane::setNodeColourFmt - Clut not supported on hardware." );
        return false;
      }
      break;

    case SURF_ACLUT88:
      if(m_bHasClut)
      {
        ulCtrl |= GDP_CTL_ACLUT88 | GDP_CTL_EN_CLUT_UPDATE;
      }
      else
      {
        TRC( TRC_ID_ERROR, "CGDpPlane::setNodeColourFmt - Clut not supported on hardware." );
        return false;
      }
      break;

    case SURF_BGRA8888:
      ulCtrl = GDP_CTL_ARGB_8888 | GDP_CTL_BIGENDIAN | alphaRange;
      break;

    default:
      TRC( TRC_ID_ERROR, "Unknown colour format." );
      return false;
  }

  if(m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_PREMULTIPLIED_ALPHA)
  {
    TRC( TRC_ID_GDP_PLANE, "CGDpPlane::setNodeColourFmt - Setting Premultiplied Alpha." );
    ulCtrl |= GDP_CTL_PREMULT_FORMAT;
  }

  topNode.GDPn_CTL |= ulCtrl;
  botNode.GDPn_CTL |= ulCtrl;

  TRC( TRC_ID_GDP_PLANE, "CGDpPlane::setNodeColourFmt - Setting Clut Address = 0x%08x.", m_pNodeToDisplay->m_bufferDesc.src.clut_bus_address);
  topNode.GDPn_CML  = botNode.GDPn_CML = m_pNodeToDisplay->m_bufferDesc.src.clut_bus_address;

  return true;
}


void CGdpPlane::updateColorKeyState (stm_color_key_config_t * const dst,
                               const stm_color_key_config_t * const src) const
{
  /* argh, this should be possible in a nicer way... */
  dst->flags = static_cast<StmColorKeyConfigFlags> (src->flags | dst->flags);
  if (src->flags & SCKCF_ENABLE)
    dst->enable = src->enable;
  if (src->flags & SCKCF_FORMAT)
    dst->format = src->format;
  if (src->flags & SCKCF_R_INFO)
    dst->r_info = src->r_info;
  if (src->flags & SCKCF_G_INFO)
    dst->g_info = src->g_info;
  if (src->flags & SCKCF_B_INFO)
    dst->b_info = src->b_info;
  if (src->flags & SCKCF_MINVAL)
    dst->minval = src->minval;
  if (src->flags & SCKCF_MAXVAL)
    dst->maxval = src->maxval;
}

bool CGdpPlane::setNodeColourKeys(GENERIC_GDP_LLU_NODE       &topNode,
                                  GENERIC_GDP_LLU_NODE       &botNode)
{
  uint8_t ucRCR = 0;
  uint8_t ucGY  = 0;
  uint8_t ucBCB = 0;

  // GDPs do not support destination colour keying
  if((m_pNodeToDisplay->m_bufferDesc.dst.ulFlags & (STM_BUFFER_DST_COLOR_KEY | STM_BUFFER_DST_COLOR_KEY_INV)) != 0)
    return false;

  updateColorKeyState (&m_ColorKeyConfig, &m_pNodeToDisplay->m_bufferDesc.src.ColorKey);

  // If colour keying not required, do nothing.
  if (m_ColorKeyConfig.enable == 0)
    return true;

  //Get Min Key value
  if (!(m_ColorKeyConfig.flags & SCKCF_MINVAL)
      || !GetRGBYCbCrKey (ucRCR, ucGY, ucBCB, m_ColorKeyConfig.minval,
                          m_gdpDisplayInfo.m_selectedPicture.colorFmt,
                          m_ColorKeyConfig.format == SCKCVF_RGB))
  {
    TRC( TRC_ID_ERROR, "Min key value not obtained." );
    return false;
  }
  botNode.GDPn_KEY1 = topNode.GDPn_KEY1 = (ucBCB | (ucGY<<8) | (ucRCR<<16));

  //Get Max Key value
  if (!(m_ColorKeyConfig.flags & SCKCF_MAXVAL)
      || !GetRGBYCbCrKey (ucRCR, ucGY, ucBCB, m_ColorKeyConfig.maxval,
                          m_gdpDisplayInfo.m_selectedPicture.colorFmt,
                          m_ColorKeyConfig.format == SCKCVF_RGB))
  {
    TRC( TRC_ID_ERROR, "Max key value not obtained" );
    return false;
  }
  botNode.GDPn_KEY2 = topNode.GDPn_KEY2 = (ucBCB | (ucGY<<8) | (ucRCR<<16));

  uint32_t ulCtrl = GDP_CTL_EN_COLOR_KEY;

  switch (m_ColorKeyConfig.r_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= GDP_CTL_RCR_COL_KEY_1; break;
  case SCKCCM_INVERSE: ulCtrl |= GDP_CTL_RCR_COL_KEY_3; break;
  default: return false;
  }

  switch (m_ColorKeyConfig.g_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= GDP_CTL_GY_COL_KEY_1; break;
  case SCKCCM_INVERSE: ulCtrl |= GDP_CTL_GY_COL_KEY_3; break;
  default: return false;
  }

  switch (m_ColorKeyConfig.b_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= GDP_CTL_BCB_COL_KEY_1; break;
  case SCKCCM_INVERSE: ulCtrl |= GDP_CTL_BCB_COL_KEY_3; break;
  default: return false;
  }

  topNode.GDPn_CTL |= ulCtrl;
  botNode.GDPn_CTL |= ulCtrl;

  return true;
}


int ScaleVerticalSamplePosition(int pos, CDisplayInfo DisplayInfo)
{
  /*
   * This function scales a source vertical sample position by the true
   * vertical scale factor between source and destination, ignoring any
   * scaling to convert between interlaced and progressive sources and
   * destinations. It is used to work out the starting sample position for
   * the first line on the destination, particularly for the start of a
   * bottom display field.
   */
  return (pos * DisplayInfo.m_selectedPicture.srcFrameRect.height) / DisplayInfo.m_dstFrameRect.height;
}

bool CGdpPlane::setNodeResizeAndFilters(GENERIC_GDP_LLU_NODE       &topNode,
                                        GENERIC_GDP_LLU_NODE       &botNode)
{
  uint32_t ulCtrl = 0;
  uint32_t hw_srcinc;
  uint32_t hw_srcinc_ext;
  unsigned filter_index;

  if(m_gdpDisplayInfo.m_hsrcinc != (uint32_t)m_fixedpointONE)
  {
    TRC( TRC_ID_GDP_PLANE, "H Resize Enabled (HSRC 4.13/4.8: %x/%x)", m_gdpDisplayInfo.m_hsrcinc, m_gdpDisplayInfo.m_hsrcinc>> 5 );

    hw_srcinc = (m_gdpDisplayInfo.m_hsrcinc >> 5);
    hw_srcinc_ext = (m_gdpDisplayInfo.m_hsrcinc & 0x1f) << GDP_HSRC_HSRC_INC_XTN_SHIFT;

    topNode.GDPn_HSRC
      = botNode.GDPn_HSRC
      = hw_srcinc | ((m_bHas4_13_precision) ? hw_srcinc_ext : 0);

    ulCtrl |= GDP_CTL_EN_H_RESIZE;

    for(filter_index=0;filter_index<N_ELEMENTS(HSRC_index);filter_index++)
    {
      if(hw_srcinc <= HSRC_index[filter_index])
      {
        TRC( TRC_ID_GDP_PLANE, "  -> 5-Tap Polyphase Filtering (idx: %d)",  filter_index );

        topNode.GDPn_HFP
          = botNode.GDPn_HFP
          = m_HFilter.ulPhysical + (filter_index*HFILTERS_ENTRY_SIZE);

        topNode.GDPn_HSRC |= GDP_HSRC_FILTER_EN;
        botNode.GDPn_HSRC |= GDP_HSRC_FILTER_EN;

        ulCtrl |= GDP_CTL_EN_HFILTER_UPD;
        break;
      }
    }
  }

  if((m_gdpDisplayInfo.m_vsrcinc != (uint32_t)m_fixedpointONE)
     || (!m_gdpDisplayInfo.m_isSrcInterlaced &&  m_gdpDisplayInfo.m_isDisplayInterlaced))
  {
    TRC( TRC_ID_GDP_PLANE, "V Resize Enabled (VSRC 4.13/4.8: %x/%x)", m_gdpDisplayInfo.m_vsrcinc,m_gdpDisplayInfo.m_vsrcinc >> 5 );
    hw_srcinc = (m_gdpDisplayInfo.m_vsrcinc >> 5);
    hw_srcinc_ext = (m_gdpDisplayInfo.m_vsrcinc & 0x1f) << GDP_VSRC_VSRC_INC_XTN_SHIFT;

    topNode.GDPn_VSRC
      = botNode.GDPn_VSRC
      = hw_srcinc | ((m_bHas4_13_precision) ? hw_srcinc_ext : 0);

    ulCtrl |= GDP_CTL_EN_V_RESIZE;

    if(m_bHasVFilter)
    {
#ifdef GDP_USE_REFERENCE_FILTERS
      for (filter_index = 0;
           filter_index < N_ELEMENTS (VSRC_index);
           ++filter_index)
      {
        if(hw_srcinc <= VSRC_index[filter_index])
        {
          TRC( TRC_ID_GDP_PLANE, "  -> 3-Tap Polyphase Filtering (idx: %d)",  filter_index );

          topNode.GDPn_VFP
            = botNode.GDPn_VFP
            = m_VFilter.ulPhysical + (filter_index*VFILTERS_ENTRY_SIZE);

          topNode.GDPn_VSRC |= GDP_VSRC_FILTER_EN;
          botNode.GDPn_VSRC |= GDP_VSRC_FILTER_EN;

          ulCtrl |= GDP_CTL_EN_VFILTER_UPD;
          break;
        }
      }
#else
      for (filter_index = 0;
           filter_index < N_ELEMENTS (gdp_3x8_filters);
           ++filter_index)
      {
        if (hw_srcinc <= gdp_3x8_filters[filter_index].range.scale_max)
        {
          TRC( TRC_ID_GDP_PLANE, "  -> 3-Tap Polyphase Filtering (idx: %d)",  filter_index );

          topNode.GDPn_VFP
            = botNode.GDPn_VFP
            = m_VFilter.ulPhysical + (filter_index
                                      * GDP_FILTER_3X8_TABLE_HW_SIZE);

          topNode.GDPn_VSRC |= GDP_VSRC_FILTER_EN;
          botNode.GDPn_VSRC |= GDP_VSRC_FILTER_EN;

          ulCtrl |= GDP_CTL_EN_VFILTER_UPD;
          break;
        }
      }
#endif
    }

     if(m_gdpDisplayInfo.m_isDisplayInterlaced && !m_gdpDisplayInfo.m_isSrcInterlaced)
     {
       /*
        * When putting progressive content on an interlaced display
        * we adjust the filter phase of the bottom field to start
        * an appropriate distance lower in the source bitmap, based on the
        * scaling factor. If the scale means that the bottom field
        * starts >1 source bitmap line lower then this will get dealt
        * with in the memory setup by adjusting the source bitmap address.
        */
       uint32_t bottomfieldinc   = ScaleVerticalSamplePosition(m_fixedpointONE,
                                                               m_gdpDisplayInfo);
       uint32_t integerinc       = bottomfieldinc / m_fixedpointONE;
       uint32_t fractionalinc    = bottomfieldinc - (integerinc
                                                     * m_fixedpointONE);
       uint32_t bottomfieldphase = (((fractionalinc * 8) + (m_fixedpointONE / 2))
                                    / m_fixedpointONE) % 8;

       botNode.GDPn_VSRC |= (bottomfieldphase<<GDP_VSRC_INITIAL_PHASE_SHIFT);
     }
   }

  topNode.GDPn_CTL |= ulCtrl;
  botNode.GDPn_CTL |= ulCtrl;

  return true;
}


bool CGdpPlane::setOutputViewport(GENERIC_GDP_LLU_NODE    &topNode,
                                  GENERIC_GDP_LLU_NODE    &botNode)
{
  /*
   * Set the destination viewport, which is in terms of frame line numbering,
   * regardless of interlaced/progressive.
   */
  topNode.GDPn_VPO = PackRegister(m_gdpDisplayInfo.m_viewport.startLine, m_gdpDisplayInfo.m_viewport.startPixel);
  botNode.GDPn_VPO = topNode.GDPn_VPO;

  topNode.GDPn_VPS = PackRegister(m_gdpDisplayInfo.m_viewport.stopLine, m_gdpDisplayInfo.m_viewport.stopPixel);
  botNode.GDPn_VPS = topNode.GDPn_VPS;

  /*
   * The number of pixels per line to read is recalculated from the number of
   * output samples required to fill the viewport, which may have been clipped
   * by the active video area.
   */
  uint32_t horizontalInputSamples = (m_gdpDisplayInfo.m_horizontalFilterOutputSamples * m_gdpDisplayInfo.m_hsrcinc) / m_fixedpointONE;
  if(horizontalInputSamples > (uint32_t)m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.width)
    horizontalInputSamples = m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.width;

  TRC( TRC_ID_GDP_PLANE, "H input samples = %u V input samples = %u",horizontalInputSamples, m_gdpDisplayInfo.m_verticalFilterInputSamples );

  topNode.GDPn_SIZE = PackRegister(m_gdpDisplayInfo.m_verticalFilterInputSamples, horizontalInputSamples);
  botNode.GDPn_SIZE = topNode.GDPn_SIZE;

  return true;
}


bool CGdpPlane::setMemoryAddressing(GENERIC_GDP_LLU_NODE       &topNode,
                                    GENERIC_GDP_LLU_NODE       &botNode)
{
  uint32_t pitch = m_gdpDisplayInfo.m_selectedPicture.pitch;

  TRC( TRC_ID_GDP_PLANE, "(line step %u)", m_gdpDisplayInfo.m_line_step );

  topNode.GDPn_PMP = pitch * m_gdpDisplayInfo.m_line_step;
  if(m_gdpDisplayInfo.m_isSrcInterlaced)
  {
    TRC( TRC_ID_GDP_PLANE, "  -> interlaced source" );
    topNode.GDPn_PMP *= 2;
  }
  else
    TRC( TRC_ID_GDP_PLANE, "  -> progressive source" );

  botNode.GDPn_PMP = topNode.GDPn_PMP;

  // Set up the pixmap memory
  uint32_t ulXStart        = m_gdpDisplayInfo.m_srcFrameRectFixedPointX/m_fixedpointONE;
  uint32_t ulYStart        = m_gdpDisplayInfo.m_srcFrameRectFixedPointY/m_fixedpointONE;
  uint32_t ulBytesPerPixel = m_gdpDisplayInfo.m_selectedPicture.pixelDepth>>3;
  uint32_t ulScanLine      = topNode.GDPn_PMP * ulYStart;
  uint32_t bufferPhysAddr  = (m_pNodeToDisplay->m_bufferDesc.info.ulFlags & STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR)?0:m_pNodeToDisplay->m_bufferDesc.src.primary_picture.video_buffer_addr;

  topNode.GDPn_PML = bufferPhysAddr + ulScanLine + (ulBytesPerPixel * ulXStart);

  if(m_gdpDisplayInfo.m_isSrcInterlaced)
  {
    /*
     * When accessing the buffer content as interlaced fields the bottom
     * field start pointer must be one line down the buffer.
     */
    botNode.GDPn_PML = topNode.GDPn_PML + pitch * m_gdpDisplayInfo.m_line_step;
  }
  else if(m_gdpDisplayInfo.m_isDisplayInterlaced)
  {
    /*
     * Progressive on interlaced display, the start position of the
     * bottom field depends on the scaling factor involved. Note that
     * we round to the next line if the sample position is in the last 1/16th.
     * The filter phase (which has 8 positions) will have been rounded up
     * and then wrapped to 0 in the filter setup.
     *
     * Note: this could potentially cause us to read beyond the end of the
     * image buffer.
     */
    TRC( TRC_ID_GDP_PLANE, "  -> interlaced output (height src %u -> dst %u)", m_gdpDisplayInfo.m_selectedPicture.srcFrameRect.height, m_gdpDisplayInfo.m_dstHeight);
    uint32_t linestobottomfield = ((ScaleVerticalSamplePosition(m_fixedpointONE,
                                                                m_gdpDisplayInfo)
                                    + (m_fixedpointONE / 16))
                                    / m_fixedpointONE);
    TRC( TRC_ID_GDP_PLANE, "  -> linestobottomfield %u", linestobottomfield );
    botNode.GDPn_PML = topNode.GDPn_PML + (pitch*linestobottomfield);
  }
  else
  {
     /*
       * Progressive content on a progressive display has the same start pointer
       * for both nodes, i.e. the content is repeated exactly.
       */
      TRC( TRC_ID_GDP_PLANE, "  -> progressive output" );
      botNode.GDPn_PML = topNode.GDPn_PML;
  }

  TRC( TRC_ID_GDP_PLANE, "  -> top PML = %#08x bot PML = %#08x.",  topNode.GDPn_PML, botNode.GDPn_PML );

  return true;
}


#define AGC_FULL_RANGE_GAIN       128
/*
 * Map 0-255 to 16-235, 6.25% offset and 85.88% gain, approximately.
 */
#define AGC_CONSTATNT_BLACK_LEVEL 64
#define AGC_VIDEO_GAIN     110

bool CGdpPlane::setNodeAlphaGain(GENERIC_GDP_LLU_NODE       &topNode,
                                 GENERIC_GDP_LLU_NODE       &botNode)
{
  uint32_t transparency = STM_PLANE_TRANSPARENCY_OPAQUE;
  uint32_t ulAlpha      = ((m_Transparency + 1) >> 1) & 0xFF;
  uint32_t staticgain   = (m_ulGain+1)/2; // convert 0-255 to 0-128 (note not 127)
  uint32_t ulGain       = 0;
  uint32_t ulAGC        = 0;

  if (m_TransparencyState==CONTROL_ON)
  {
    transparency = m_Transparency;
  }
  else
  {
    if(m_pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_CONST_ALPHA )
    {
      transparency = m_pNodeToDisplay->m_bufferDesc.src.ulConstAlpha;
    }
  }

  // The input range is 0-255, this must be scaled to 0-128 for the hardware
  ulAlpha = ((transparency + 1) >> 1) & 0xFF;
  if(m_gdpDisplayInfo.m_selectedPicture.colorFmt == SURF_ARGB1555)
  {
    /*
     * Scale the alpha ramp map by the requested buffer alpha
     */
    ulAGC |= (m_ulStaticAlpha[0] * ulAlpha)/128;
    ulAGC |= ((m_ulStaticAlpha[1] * ulAlpha)/128)<<8;
  }
  else
  {
    ulAGC |= ulAlpha;
  }

  if (m_pNodeToDisplay->m_bufferDesc.dst.ulFlags & STM_BUFFER_DST_RESCALE_TO_VIDEO_RANGE)
  {
    ulGain  = (AGC_VIDEO_GAIN * staticgain) / 128;
    ulAGC  |= (AGC_CONSTATNT_BLACK_LEVEL << GDP_AGC_CONSTANT_SHIFT);
  }
  else
  {
    ulGain = (AGC_FULL_RANGE_GAIN * staticgain) / 128;
  }

  ulAGC |= (ulGain << GDP_AGC_GAIN_SHIFT);

  topNode.GDPn_AGC = botNode.GDPn_AGC = ulAGC;

  return true;
}


DisplayPlaneResults CGdpPlane::SetControl(stm_display_plane_control_t control, uint32_t value)
{
  DisplayPlaneResults result = STM_PLANE_OK;

  TRCIN( TRC_ID_GDP_PLANE, "control = %d ulNewVal = %u (0x%08x)",(int)control,value,value );

  switch(control)
  {
    case PLANE_CTRL_FLICKER_FILTER_STATE:
      if(value != PLANE_FLICKER_FILTER_DISABLE && value != PLANE_FLICKER_FILTER_ENABLE)
        return STM_PLANE_INVALID_VALUE;
      if ( m_pDisplayDevice->VSyncLock() != 0 )
        return STM_PLANE_EINTR;
      if (m_FlickerFilterState != value)
      {
        m_FlickerFilterState = (stm_plane_ff_state_t) value;
        m_ContextChanged = true;
        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new FLICKER_FILTER_STATE)", GetName() );
      }
      m_pDisplayDevice->VSyncUnlock();
      break;

    case PLANE_CTRL_FLICKER_FILTER_MODE:
      if(value != PLANE_FLICKER_FILTER_SIMPLE && value != PLANE_FLICKER_FILTER_ADAPTIVE)
        return STM_PLANE_INVALID_VALUE;
      if ( m_pDisplayDevice->VSyncLock() != 0)
        return STM_PLANE_EINTR;
      if(m_FlickerFilterMode != value)
      {
        m_FlickerFilterMode = (stm_plane_ff_mode_t) value;
        m_ContextChanged = true;
        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new FLICKER_FILTER_MODE)", GetName() );
      }
      m_pDisplayDevice->VSyncUnlock();
      break;

    case PLANE_CTRL_GLOBAL_GAIN_VALUE:
      if(value>255)
        return STM_PLANE_INVALID_VALUE;
      if ( m_pDisplayDevice->VSyncLock() != 0)
        return STM_PLANE_EINTR;
      m_ulGain = value;
      m_IsGainChanged = true;
      m_pDisplayDevice->VSyncUnlock();
      break;

    case PLANE_CTRL_ALPHA_RAMP:
      if ( m_pDisplayDevice->VSyncLock() != 0)
        return STM_PLANE_EINTR;
      m_ulAlphaRamp = value;
      m_ulStaticAlpha[0] = ((m_ulAlphaRamp & 0xff)+1)/2;
      m_ulStaticAlpha[1] = (((m_ulAlphaRamp>>8) & 0xff)+1)/2;
      m_IsTransparencyChanged = true;
      m_pDisplayDevice->VSyncUnlock();
      break;

    case PLANE_CTRL_COLOR_KEY:
      {
        const stm_color_key_config_t * const config = reinterpret_cast<stm_color_key_config_t *> (value);
        if (!config)
          return STM_PLANE_INVALID_VALUE;
        updateColorKeyState (&m_ColorKeyConfig, config);
        m_IsColorKeyChanged = true;
      }
      break;

    case PLANE_CTRL_BUFFER_ADDRESS:
      if ( m_pDisplayDevice->VSyncLock() != 0)
        return STM_PLANE_EINTR;
      m_ulDirectBaseAddress = value;
      updateBaseAddress();
      m_ContextChanged = true;
      TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new PLANE_CTRL_BUFFER_ADDRESS)", GetName() );
      m_pDisplayDevice->VSyncUnlock();
      break;

    default:
      return CDisplayPlane::SetControl(control,value);
  }

  return result;
}


DisplayPlaneResults CGdpPlane::GetControl(stm_display_plane_control_t control, uint32_t *value) const
{
  TRCIN( TRC_ID_GDP_PLANE, "control = %d",(int)control );

  switch(control)
  {
    case PLANE_CTRL_TRANSPARENCY_VALUE:
      *value = m_Transparency;
      break;
    case PLANE_CTRL_GLOBAL_GAIN_VALUE:
      *value = m_ulGain;
      break;

    case PLANE_CTRL_FLICKER_FILTER_STATE:
      *value = m_FlickerFilterState;
      break;
    case PLANE_CTRL_FLICKER_FILTER_MODE:
      *value = m_FlickerFilterMode;
      break;

    case PLANE_CTRL_ALPHA_RAMP:
      *value = m_ulAlphaRamp;
      break;

    case PLANE_CTRL_COLOR_KEY:
      {
        stm_color_key_config_t * const config = reinterpret_cast<stm_color_key_config_t *> (value);
        if (!config)
          return STM_PLANE_INVALID_VALUE;
        config->flags = SCKCF_NONE;
        updateColorKeyState (config, &m_ColorKeyConfig);
      }
      break;
    case PLANE_CTRL_BUFFER_ADDRESS:
      *value = m_ulDirectBaseAddress;
      break;

    default:
      return CDisplayPlane::GetControl(control,value);
  }

  return STM_PLANE_OK;
}

void CGdpPlane::EnableHW(void)
{

  if(!m_isEnabled && m_pOutput && m_outputInfo.isOutputStarted)
  {
    if(!m_FirstGDPNodeOwner->isEnabled())
    {
      /* Setup the link with Next Node */
      SetFirstGDPNodeOwner(this);

      TRC( TRC_ID_GDP_PLANE, "CGDpPlane::EnableHW 'START GDP id = %u NVN = %#08x'", GetID(),GetGDPRegisters(0)->ulPhysical );
      vibe_os_write_register(m_GDPBaseAddr, GDPn_NVN_OFFSET,GetGDPRegisters(0)->ulPhysical);
    }

    ((GENERIC_GDP_LLU_NODE *)m_Registers[0].pData)->GDPn_PPT &= ~(GDP_PPT_IGNORE_ON_MIX1 | GDP_PPT_IGNORE_ON_MIX2);
    vibe_os_flush_dma_area(&m_Registers[0], OFFSETOF(GENERIC_GDP_LLU_NODE, GDPn_PPT), sizeof(uint32_t));
    ((GENERIC_GDP_LLU_NODE *)m_Registers[1].pData)->GDPn_PPT &= ~(GDP_PPT_IGNORE_ON_MIX1 | GDP_PPT_IGNORE_ON_MIX2);
    vibe_os_flush_dma_area(&m_Registers[1], OFFSETOF(GENERIC_GDP_LLU_NODE, GDPn_PPT), sizeof(uint32_t));

    CDisplayPlane::EnableHW();
  }

}


void CGdpPlane::DisableHW(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  CDisplayPlane::DisableHW();

  /*
   * If we are part of a node list with other active nodes, then the GDP
   * hardware will not actually be disabled on the mixer. The best we can do
   * in that case is to leave the last node in tact, but with the mixer
   * ignore bits set in the node. We cannot put the dummy node back because
   * this might be called as part of flush and we cannot update the node
   * atomically with regard to the hardware reading the node. The other option
   * is to start trying to change the node linkage, but there lies dragons so
   * we will try this for now.
   */
  ((GENERIC_GDP_LLU_NODE *)m_Registers[0].pData)->GDPn_PPT |=  (GDP_PPT_IGNORE_ON_MIX1 | GDP_PPT_IGNORE_ON_MIX2);
  ((GENERIC_GDP_LLU_NODE *)m_Registers[0].pData)->GDPn_PPT &= ~(GDP_PPT_FORCE_ON_MIX1  | GDP_PPT_FORCE_ON_MIX2);
  vibe_os_flush_dma_area(&m_Registers[0], OFFSETOF(GENERIC_GDP_LLU_NODE, GDPn_PPT), sizeof(uint32_t));

  ((GENERIC_GDP_LLU_NODE *)m_Registers[1].pData)->GDPn_PPT |= (GDP_PPT_IGNORE_ON_MIX1 | GDP_PPT_IGNORE_ON_MIX2);
  ((GENERIC_GDP_LLU_NODE *)m_Registers[1].pData)->GDPn_PPT &= ~(GDP_PPT_FORCE_ON_MIX1  | GDP_PPT_FORCE_ON_MIX2);
  vibe_os_flush_dma_area(&m_Registers[1], OFFSETOF(GENERIC_GDP_LLU_NODE, GDPn_PPT), sizeof(uint32_t));

  /* reset currentGdpSetup */
  m_CurrentGdpSetup.isValid = false;

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}

void CGdpPlane::ResetGdpSetup(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  m_NextGdpSetup.nodeType          = GNODE_PROGRESSIVE;
  m_NextGdpSetup.pictureId         = 0;
  m_NextGdpSetup.isValid           = false;

  vibe_os_zero_memory(&m_NextGdpSetup.topNode, sizeof(GENERIC_GDP_LLU_NODE));
  vibe_os_zero_memory(&m_NextGdpSetup.botNode, sizeof(GENERIC_GDP_LLU_NODE));

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}

void CGdpPlane::createDummyNode(GENERIC_GDP_LLU_NODE *node)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  vibe_os_zero_memory(node, sizeof(GENERIC_GDP_LLU_NODE));
  node->GDPn_CTL = GDP_CTL_ARGB_8888;
  if(IsLastGDPNodeOwner())
    node->GDPn_CTL |= GDP_CTL_WAIT_NEXT_VSYNC;

  node->GDPn_PPT = (GDP_PPT_IGNORE_ON_MIX1 | GDP_PPT_IGNORE_ON_MIX2);

  node->GDPn_PML  = m_DummyBuffer.ulPhysical;
  node->GDPn_PMP  = sizeof(uint32_t);
  node->GDPn_SIZE = 0x00020001;
  /*
   * It appears that the minimum working horizontal viewport start x is 0xd,
   * less than this and the GDP appears not to process the node and gets stuck.
   * Use 0x10 for a degree of safety because there is no explanation for this.
   *
   * Set the vertical position to be in the active video region of all the video
   * standards.
   */
  node->GDPn_VPO  = 0x00430010;
  node->GDPn_VPS  = 0x00440010;

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


void CGdpPlane::Freeze(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  /* Backup current visibility state */
  m_wasEnabled = m_isEnabled;

  if(m_wasEnabled)
  {
    /*
     * For GFX planes we only need to disable HW and keep
     * previous node on the list so we will have previous
     * picture displayed on the screen when resuming.
     */
    DisableHW();
  }

  CDisplayPlane::Freeze();

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}


void CGdpPlane::Resume(void)
{
  TRCIN( TRC_ID_GDP_PLANE, "" );

  if(m_wasEnabled && m_bIsFrozen)
  {
    EnableHW();
  }

  CDisplayPlane::Resume();

  TRCOUT( TRC_ID_GDP_PLANE, "" );
}
