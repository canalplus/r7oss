/***********************************************************************
 *
 * File: display/ip/vdp/VdpPlane.cpp
 * Copyright (c) 2014 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public License.
 * See the file COPYING in the main directory of this archive formore details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/Output.h>

#include <display/ip/stmfilter.h>
#include <display/ip/vdp/STRefVdpFilters.h>
#include <display/ip/vdp/VdpReg.h>
#include <display/ip/vdp/VdpPlane.h>

// Max scaling factor for switching to decimate picture usage
#define MAX_SCALING_FACTOR 4

// Max STBUSBandWidth for VDP plane ( in MB/s )
#define MAX_STBUS_BANDWIDTH 250


// Size of STBUS word read made by the video pipeline ( in Bytes )
#define STBUS_NBR_OF_BYTES_PER_WORD 8



// A set of useful named constants to make the filter setup code more readable
static const int fixedpointONE = 1 << 13; // 0x2000 (8k)
static const int fixedpointHALF = fixedpointONE / 2; // 0x1000 (4k)
static const int fixedpointQUARTER = fixedpointONE / 4; // 0x0800 (2k)

static const stm_pixel_format_t g_surfaceFormats[] = {
  SURF_YCBCR422R,
  SURF_YUYV,
  SURF_YCBCR420MB,
  SURF_YUV420,
  SURF_YVU420,
  SURF_YUV422P,
  SURF_YCbCr420R2B,
  SURF_YCbCr422R2B,
};

static const stm_plane_feature_t g_planeFeatures[] = {
  PLANE_FEAT_VIDEO_SCALING,
  PLANE_FEAT_WINDOW_MODE,
  PLANE_FEAT_SRC_COLOR_KEY,
  PLANE_FEAT_TRANSPARENCY,
  PLANE_FEAT_GLOBAL_COLOR
};

static inline uint32_t packRegister( uint16_t high, uint16_t low )
{
  return(( high << 16 ) | low );
}

// Helper functions for the memory addressing methods.
static inline void setProgressiveAddress( VdpSetup *pSetup,
                                          uint32_t luma,
                                          uint32_t chroma )
{
  TRC( TRC_ID_VDP, "luma : 0x%x, chroma : 0x%x", luma, chroma );
  pSetup->topField.luma_ba = luma;
  pSetup->topField.chroma_ba = chroma;
  pSetup->botField.luma_ba = luma;
  pSetup->botField.chroma_ba = chroma;
  pSetup->altTopField.luma_ba = luma;
  pSetup->altTopField.chroma_ba = chroma;
  pSetup->altBotField.luma_ba = luma;
  pSetup->altBotField.chroma_ba = chroma;
}

static inline void setInterlacedAddress( VdpSetup *pSetup,
                                         uint32_t lumaTop,
                                         uint32_t chromaTop,
                                         uint32_t lumaBot,
                                         uint32_t chromaBot )
{
  TRC( TRC_ID_VDP, "lumaTop : 0x%x, chromaTop : 0x%x, lumaBot : 0x%x, chromaBot : 0x%x", lumaTop, chromaTop, lumaBot, chromaBot );
  pSetup->topField.luma_ba = lumaTop;
  pSetup->topField.chroma_ba = chromaTop;
  pSetup->botField.luma_ba = lumaBot;
  pSetup->botField.chroma_ba = chromaBot;
  pSetup->altTopField.luma_ba = lumaBot;
  pSetup->altTopField.chroma_ba = chromaBot;
  pSetup->altBotField.luma_ba = lumaTop;
  pSetup->altBotField.chroma_ba = chromaTop;
}

static inline void setRasterT3IBusAccesses( VdpSetup *pSetup,
                                            int opcodeSize,
                                            int chunkSize )
{
  TRC( TRC_ID_VDP, "opcodeSize : %d, chunkSize : 0x%x", opcodeSize, chunkSize );

  switch ( opcodeSize )
    {
    case 32:
      // Make 4 read of 1 Word ( 1 Word = 8 Bytes ) = > Granulatity of 4 Words
      pSetup->dei_t3i_ctl = DEI_T3I_CTL_OPCODE_SIZE_32;
      break;

    case 16:
      // Make 2 read of 1 Word = > Granulatity of 2 Words
      pSetup->dei_t3i_ctl = DEI_T3I_CTL_OPCODE_SIZE_16;
      break;

    case 8:
    default:
      // Make 1 read of 1 Word = > Granulatity of 1 Word
      pSetup->dei_t3i_ctl = DEI_T3I_CTL_OPCODE_SIZE_8;
      break;
    }

  pSetup->dei_t3i_ctl |= chunkSize;
}


CVdpPlane::CVdpPlane( const char * name,
                      uint32_t id,
                      const CDisplayDevice * pDev,
                      const stm_plane_capabilities_t caps,
                      CVideoPlug * pVideoPlug,
                      uint32_t vdpBaseAddr
                      ) : CDisplayPlane( name, id, pDev, caps )
{
  m_baseAddress = ( uint32_t * )(( uint8_t * )pDev->GetCtrlRegisterBase() + vdpBaseAddr );
  m_videoPlug = pVideoPlug;

  m_fixedpointONE = 1 << 13; // n.13 fixed point for DISP rescaling - 0x2000
  m_ulMaxHSrcInc = MAX_SCALING_FACTOR * m_fixedpointONE; // downscale 4x - 0x8000
  m_ulMinHSrcInc = m_fixedpointONE / 32; // upscale 32x - 0x0100
  m_ulMaxVSrcInc = MAX_SCALING_FACTOR * m_fixedpointONE;
  m_ulMinVSrcInc = m_fixedpointONE / 32;

  vibe_os_zero_memory( &m_HFilter[0], sizeof( DMA_Area ));
  vibe_os_zero_memory( &m_HFilter[1], sizeof( DMA_Area ));
  vibe_os_zero_memory( &m_VFilter[0], sizeof( DMA_Area ));
  vibe_os_zero_memory( &m_VFilter[1], sizeof( DMA_Area ));
  m_idxFilter = 0;

  m_pSurfaceFormats = g_surfaceFormats;
  m_nFormats = N_ELEMENTS( g_surfaceFormats );

  m_pFeatures = g_planeFeatures;
  m_nFeatures = N_ELEMENTS( g_planeFeatures );

  // Control registers, everything disabled
  WriteRegister( DEI_CTL, DEI_CTL_INACTIVE );
  WriteRegister( VHSRC_CTL, 0 );

  // m_CrcData initialization for displaytest module
  vibe_os_zero_memory( &m_CrcData, sizeof( m_CrcData ));

  ResetEveryUseCases();
  m_MBChunkSize = 0;
  m_PlanarChunkSize = 0;
  m_RasterChunkSize = 0;
  m_RasterOpcodeSize = 0;
  m_pCurrNode = 0;
  m_isPictureRepeated = false;
  m_isTopFieldOnDisplay = false;
  m_pSetup = 0;

  m_vdpDisplayInfo.Reset();
}

CVdpPlane::~CVdpPlane( void )
{
  vibe_os_free_dma_area( &m_HFilter[0] );
  vibe_os_free_dma_area( &m_HFilter[1] );
  vibe_os_free_dma_area( &m_VFilter[0] );
  vibe_os_free_dma_area( &m_VFilter[1] );
}

bool CVdpPlane::Create( void )
{
  if ( not CDisplayPlane::Create() )
    {
      TRC( TRC_ID_ERROR, "CDisplayPlane::Create returns error" );
      return false;
    }

  // Each filter table contains two complete sets of filters, one for Luma and one for Chroma.
  vibe_os_allocate_dma_area( &m_HFilter[0], HFC_NB_COEFS * 2, 16, SDAAF_NONE );

  if ( not m_HFilter[0].pMemory )
    {
      TRC( TRC_ID_ERROR, "out of memory" );
      return false;
    }

  TRC( TRC_ID_MAIN_INFO, "CVdpPlane::Create &m_HFilter = %p pMemory = %p pData = %p phys = %08x", &m_HFilter[0], m_HFilter[0].pMemory, m_HFilter[0].pData, m_HFilter[0].ulPhysical );

  vibe_os_allocate_dma_area( &m_HFilter[1], HFC_NB_COEFS * 2, 16, SDAAF_NONE );

  if ( not m_HFilter[1].pMemory )
    {
      TRC( TRC_ID_ERROR, "out of memory" );
      return false;
    }

  TRC( TRC_ID_MAIN_INFO, "CVdpPlane::Create &m_HFilter = %p pMemory = %p pData = %p phys = %08x", &m_HFilter[1], m_HFilter[1].pMemory, m_HFilter[1].pData, m_HFilter[1].ulPhysical );

  vibe_os_allocate_dma_area( &m_VFilter[0], VFC_NB_COEFS * 2, 16, SDAAF_NONE );

  if ( not m_VFilter[0].pMemory )
    {
      TRC( TRC_ID_ERROR, "out of memory" );
      return false;
    }

  TRC( TRC_ID_MAIN_INFO, "CVdpPlane::Create &m_VFilter = %p pMemory = %p pData = %p phys = %08x", &m_VFilter[0], m_VFilter[0].pMemory, m_VFilter[0].pData, m_VFilter[0].ulPhysical );

  vibe_os_allocate_dma_area( &m_VFilter[1], VFC_NB_COEFS * 2, 16, SDAAF_NONE );

  if ( not m_VFilter[1].pMemory )
    {
      TRC( TRC_ID_ERROR, "out of memory" );
      return false;
    }

  TRC( TRC_ID_MAIN_INFO, "CVdpPlane::Create &m_VFilter = %p pMemory = %p pData = %p phys = %08x", &m_VFilter[1], m_VFilter[1].pMemory, m_VFilter[1].pData, m_VFilter[1].ulPhysical );

  if ( not m_Filter.Create())
    {
      TRC( TRC_ID_ERROR, "m_Filter.Create returns error" );
      return false;
    }

  return true;
}

DisplayPlaneResults CVdpPlane::GetControl( stm_display_plane_control_t control,
                                           uint32_t * value ) const
{
  DisplayPlaneResults retval = STM_PLANE_OK;

  switch ( control )
    {
    case PLANE_CTRL_HIDE_MODE_POLICY:
      *value = m_eHideMode;
      break;

    case PLANE_CTRL_FILTER_SET :
      if ( not m_Filter.GetFilterSet( (stm_plane_filter_set_t *)value ))
        {
          retval = STM_PLANE_INVALID_VALUE;
        }

      TRC( TRC_ID_ERROR, "PLANE_CTRL_FILTER_SET : %d", *value );
      break;

    default:
      if ( not m_videoPlug->GetControl( control, value ))
        retval = CDisplayPlane::GetControl( control, value );

      break;
    }

  return retval;
}

DisplayPlaneResults CVdpPlane::SetControl( stm_display_plane_control_t control,
                                           uint32_t value )
{
  DisplayPlaneResults retval = STM_PLANE_OK;

  switch ( control )
    {
    case PLANE_CTRL_HIDE_MODE_POLICY:
      switch ( static_cast < stm_plane_ctrl_hide_mode_policy_t > ( value ))
        {
        case PLANE_HIDE_BY_MASKING:
        case PLANE_HIDE_BY_DISABLING:
          if(m_pDisplayDevice->VSyncLock() != 0)
            return STM_PLANE_EINTR;
          m_eHideMode = static_cast< stm_plane_ctrl_hide_mode_policy_t > ( value );
          m_pDisplayDevice->VSyncUnlock();
          TRC( TRC_ID_MAIN_INFO, "New m_eHideMode=%u",  m_eHideMode );
          break;

        default:
          retval = STM_PLANE_INVALID_VALUE;
          break;
        }
      break;

    case PLANE_CTRL_FILTER_SET :
      TRC( TRC_ID_ERROR, "PLANE_CTRL_FILTER_SET : %d", value );
      if ( not m_Filter.SelectFilterSet( (stm_plane_filter_set_t)value ))
        {
          retval = STM_PLANE_NO_MEMORY;
        }

      if ( m_pDisplayDevice->VSyncLock() != 0 )
      {
        return STM_PLANE_EINTR;
      }
      m_ContextChanged = true; // to force context change
      TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new filter set)", GetName() );
      m_pDisplayDevice->VSyncUnlock();
      break;

    default:
      if ( not m_videoPlug->SetControl( control, value ))
        retval = CDisplayPlane::SetControl( control, value );

      break;
    }

  return retval;
}


bool CVdpPlane::GetControlRange( stm_display_plane_control_t selector,
                                 stm_display_plane_control_range_t * range )
{
  switch ( selector )
    {
    case PLANE_CTRL_FILTER_SET :
      TRC( TRC_ID_ERROR, "selector PLANE_CTRL_FILTER_SET" );
      range->min_val = FILTER_SET_LEGACY;
      range->max_val = FILTER_SET_SMOOTH;
      range->default_val = FILTER_SET_MEDIUM;
      range->step = 1;
      return true;

    default :
      return CDisplayPlane::GetControlRange( selector, range );
    }
}


bool CVdpPlane::GetCompoundControlRange( stm_display_plane_control_t selector,
                                         stm_compound_control_range_t * range )
{
  range->min_val.x = 0;
  range->min_val.y = 0;
  range->min_val.width = 32;
  range->min_val.height = 24;
  range->max_val.x = 0;
  range->max_val.y = 0;
  range->max_val.width = 1920;
  range->max_val.height = 1088;
  range->default_val.x = 0;
  range->default_val.y = 0;
  range->default_val.width = 0;
  range->default_val.height = 0;
  range->step.x = 1;
  range->step.y = 1;
  range->step.width = 1;
  range->step.height = 1;
  return true;
}

void CVdpPlane::DisableHW( void )
{
  if ( m_isEnabled )
    {
      // Disable video display processor
      WriteRegister( DEI_CTL, DEI_CTL_INACTIVE );
      WriteRegister( VHSRC_CTL, 0 );
      CDisplayPlane::DisableHW();
    }
}

TuningResults CVdpPlane::SetTuning( uint16_t service,
                                    void * inputList,
                                    uint32_t inputListSize,
                                    void * outputList,
                                    uint32_t outputListSize )
{
  TuningResults res = TUNING_INVALID_PARAMETER;
  tuning_service_type_t ServiceType = ( tuning_service_type_t )service;

  switch( ServiceType )
    {
    case CRC_CAPABILITY:
      {
        if (outputList != NULL)
        {
          char * string = ( char * )outputList;
          outputListSize = vibe_os_snprintf( string, outputListSize, "VDP, LastLuma, LastChroma" );
          res = TUNING_OK;
        }
        else
        {
          TRC( TRC_ID_ERROR, "Invalid outputList!");
        }
      }
      break;

    case CRC_COLLECT:
      {
        SetTuningOutputData_t * output = ( SetTuningOutputData_t * )outputList;

        if (m_CrcData.Status)
        {
          if ( output != NULL )
          {
            if ( m_pDisplayDevice->VSyncLock() != 0)
                return TUNING_EINTR;

            output->Data.Crc = m_CrcData;
            // clear m_CrcData for next Vsync
            vibe_os_zero_memory( &m_CrcData, sizeof( m_CrcData ));
            TRC( TRC_ID_UNCLASSIFIED, "VSync %lld PTS : %lld, crc0 : %x, crc1 : %x", output->Data.Crc.LastVsyncTime, output->Data.Crc.PTS, output->Data.Crc.CrcValue[0], output->Data.Crc.CrcValue[1] );
            res = TUNING_OK;

            m_pDisplayDevice->VSyncUnlock();
          }
          else
          {
            TRC( TRC_ID_ERROR, "output not valid 0x%p 0x%p", output, m_pOutput);
          }
        }
        else
        {
          res = TUNING_NO_DATA_AVAILABLE;
        }
      }
      break;

    default:
      res = CDisplayPlane::SetTuning( service, inputList, inputListSize, outputList, outputListSize );
      break;
    }

  return res;
}

void CVdpPlane::ProcessLastVsyncStatus( const stm_time64_t &vsyncTime, CDisplayNode * pNodeDisplayed )
{
  uint32_t status_reg;

  if ( not pNodeDisplayed )
  {
    // No picture was displayed during the last VSync period so there is no status to collect.
    TRC(TRC_ID_UNCLASSIFIED, "No status to collect");
    m_CrcData.Status = false;
    return;
  }

  // Save the CRC data collected and status at last VSync
  m_CrcData.LastVsyncTime  = vsyncTime;
  m_CrcData.PictureID = pNodeDisplayed->m_pictureId;
  m_CrcData.PTS = pNodeDisplayed->m_bufferDesc.info.PTS;
  m_CrcData.Status = true;
  m_Statistics.CurDispPicPTS = pNodeDisplayed->m_bufferDesc.info.PTS;

  switch( pNodeDisplayed->m_srcPictureType )
    {
    case GNODE_BOTTOM_FIELD:
      m_CrcData.PictureType = 'B';
      break;

    case GNODE_TOP_FIELD:
      m_CrcData.PictureType = 'T';
      break;

    case GNODE_PROGRESSIVE:
      m_CrcData.PictureType = 'F';
      break;
    }

  m_CrcData.CrcValue[0] = ReadRegister( Y_CRC_CHECK_SUM );
  m_CrcData.CrcValue[1] = ReadRegister( UV_CRC_CHECK_SUM );

  status_reg = ReadRegister(P2I_PXF_IT_STATUS);

  /* In normal conditions, IT_FIFO_EMPTY should be equal to '0' at the end of a processing */
  if (status_reg & P2I_STATUS_IT_FIFO_EMPTY )
  {
    TRC( TRC_ID_ERROR, "FIFO_EMPTY error!" );
    pNodeDisplayed->m_bufferDesc.info.stats.status |= STM_STATUS_BUF_HW_ERROR;
  }

  /* In normal conditions, END_PROCESSING should be equal to '1' at the end of a processing */
  if( (status_reg & P2I_STATUS_END_PROCESSING ) == 0)
  {
    TRC( TRC_ID_ERROR, "Last cmd not processed correctly!" );
    pNodeDisplayed->m_bufferDesc.info.stats.status |= STM_STATUS_BUF_HW_ERROR;
  }

  // Write in P2I_PXF_IT_STATUS register to reset the status
  WriteRegister( P2I_PXF_IT_STATUS, ( P2I_STATUS_END_PROCESSING | P2I_STATUS_IT_FIFO_EMPTY ));
  TRC( TRC_ID_VDP, "PTS : %lld, crc0 : %x, crc1 : %x", pNodeDisplayed->m_bufferDesc.info.PTS, m_CrcData.CrcValue[0], m_CrcData.CrcValue[1] );

}

void CVdpPlane::PresentDisplayNode( CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isDisplayInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime )
{
  stm_display_use_cases_t currentUseCase;

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  if ( not pCurrNode )
    {
      TRC( TRC_ID_ERROR, "pCurrNode is a NULL pointer!" );
      return;
    }

  TRC( TRC_ID_VDP, "isPictureRepeated : %d, isDisplayInterlaced : %d, isTopFieldOnDisplay : %d, vsyncTime : %lld", isPictureRepeated, isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime );
  TRC( TRC_ID_PICT_QUEUE_RELEASE, "%d%c (%p)",  pCurrNode->m_pictureId, pCurrNode->m_srcPictureTypeChar, pCurrNode);

  m_pCurrNode = pCurrNode;
  m_isPictureRepeated = isPictureRepeated;
  m_isTopFieldOnDisplay = isTopFieldOnDisplay;

  // check whether the context is changed
  // if yes, reset and recompute everything
  if ( IsContextChanged( m_pCurrNode, m_isPictureRepeated ))
    {
      TRC( TRC_ID_VDP, "Display context has changed" );

      // Reset all saved Cmds
      m_isPictureRepeated = false;
      ResetEveryUseCases();

      // Recompute the DisplayInfo
      if ( not FillDisplayInfo())
        {
          TRC( TRC_ID_ERROR, "Failed to create the Vdp DisplayInfo!" );
          goto nothing_to_display;
        }
    }

  // get the selected video adresses
  if ( m_vdpDisplayInfo.m_isSecondaryPictureSelected )
    {
      m_vdpDisplayInfo.m_video_buffer_addr = m_pCurrNode->m_bufferDesc.src.secondary_picture.video_buffer_addr;
      m_vdpDisplayInfo.m_chroma_buffer_offset = m_pCurrNode->m_bufferDesc.src.secondary_picture.chroma_buffer_offset;
    }
  else
    {
      m_vdpDisplayInfo.m_video_buffer_addr = m_pCurrNode->m_bufferDesc.src.primary_picture.video_buffer_addr;
      m_vdpDisplayInfo.m_chroma_buffer_offset = m_pCurrNode->m_bufferDesc.src.primary_picture.chroma_buffer_offset;
    }

  // get the currrent use case ( this cannot fail )
  currentUseCase = GetCurrentUseCase( m_pCurrNode->m_srcPictureType, m_outputInfo.isDisplayInterlaced, m_isTopFieldOnDisplay );

  m_pSetup = &m_useCase[currentUseCase].setup;

  // Check whether a valid setup is available for this use case. If not, compute it
  if ( not m_useCase[currentUseCase].isValid )
    {
      TRC( TRC_ID_VDP, "No valid setup available for this use case : Recompute the setup" );

      // prepare all parameters for each setup
      if ( not PrepareSetup())
        {
          TRC( TRC_ID_ERROR, "Failed to prepare the setup !" );
          goto nothing_to_display;
        }

      m_useCase[currentUseCase].isValid = true;
    }

  // Customize the fields changing all the time ( pictureBaseAddresses, CSDI config... )
  if ( not SetDynamicDisplayInfo())
    {
      TRC( TRC_ID_ERROR, "Failed to prepare the DisplayInfo changing all the time!" );
      goto nothing_to_display;
    }

  WriteSetup();

  UpdatePictureDisplayedStatistics(pCurrNode, isPictureRepeated, isDisplayInterlaced, isTopFieldOnDisplay);

  return;

nothing_to_display:
  NothingToDisplay();
}


// "m_vsyncLock" should be taken when NothingToDisplay() is called
bool CVdpPlane::NothingToDisplay()
{
  bool res;

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  if(m_isPlaneActive)
  {
      TRC(TRC_ID_MAIN_INFO, "%s: Nothing to display. VDP HW disabled", GetName());
      m_isPlaneActive = false;
  }

  // Perform the stop actions specific to HQVDP plane
  WriteRegister( DEI_CTL, DEI_CTL_INACTIVE );
  WriteRegister( VHSRC_CTL, 0 );

  vibe_os_zero_memory( &m_CrcData, sizeof( m_CrcData ));

  // Perform the generic stop actions
  res = CDisplayPlane::NothingToDisplay();

  return res;
}

bool CVdpPlane::FillDisplayInfo()
{
  uint32_t transparency = STM_PLANE_TRANSPARENCY_OPAQUE;

  // Reset the DisplayInfo before recomputing them
  m_vdpDisplayInfo.Reset();

  if ( m_pOutput )
    {
      m_vdpDisplayInfo.m_pCurrentMode = m_pOutput->GetCurrentDisplayMode();
    }

  if ( not CDisplayPlane::FillDisplayInfo( m_pCurrNode, &m_vdpDisplayInfo ))
    {
      TRC( TRC_ID_ERROR, "CDisplayPlane::FillDisplayInfo failed !!" );
      return false;
    }

  if ( not PrepareIOWindows( m_pCurrNode, &m_vdpDisplayInfo ))
    {
      TRC( TRC_ID_ERROR, "PrepareIOWindows() failed !!" );
      return false;
    }

  TRC( TRC_ID_VDP, "Plane %s new IO Windows, InWin : %d %d %d %d, OutWin : %d %d %d %d", GetName(),
       m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height,
       m_vdpDisplayInfo.m_dstFrameRect.x, m_vdpDisplayInfo.m_dstFrameRect.y, m_vdpDisplayInfo.m_dstFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.height );

  // set display hw information
  SetHWDisplayInfos();

  // set display source information
  SetSrcDisplayInfos();

  AdjustBufferInfoForScaling();

  if (m_TransparencyState==CONTROL_ON)
    {
      transparency = m_Transparency;
    }
  else
    {
      if(m_pCurrNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CONST_ALPHA )
        {
          transparency = m_pCurrNode->m_bufferDesc.src.ulConstAlpha;
        }
    }
  if ( not m_videoPlug->CreatePlugSetup( m_vdpDisplayInfo.m_videoPlugSetup,
                                         &m_pCurrNode->m_bufferDesc,
                                         m_vdpDisplayInfo.m_viewport,
                                         m_vdpDisplayInfo.m_selectedPicture.colorFmt,
                                         transparency))
    {
      TRC( TRC_ID_ERROR, "Failed to create the PlugSetup!" );
      return false;
    }

  return true;
}

bool CVdpPlane::IsScalingPossible( CDisplayNode  *pNodeToDisplay,
                                   CDisplayInfo  *pDisplayInfo)
{
    bool isSrcInterlaced = pDisplayInfo->m_isSrcInterlaced;


    TRCBL(TRC_ID_MAIN_INFO);
    TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Primary picture");
    pDisplayInfo->m_isSecondaryPictureSelected = false;
    FillSelectedPictureDisplayInfo(pNodeToDisplay, pDisplayInfo);

    if(IsScalingPossibleByHw(pNodeToDisplay, pDisplayInfo, isSrcInterlaced))
    {
        // Scaling possible with the Primary picture
        return true;
    }

    /* Is decimation available? */
    if( (pNodeToDisplay->m_bufferDesc.src.horizontal_decimation_factor != STM_NO_DECIMATION) ||
        (pNodeToDisplay->m_bufferDesc.src.vertical_decimation_factor   != STM_NO_DECIMATION) )
    {
        TRCBL(TRC_ID_MAIN_INFO);
        TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Secondary picture");
        pDisplayInfo->m_isSecondaryPictureSelected = true;
        FillSelectedPictureDisplayInfo(pNodeToDisplay, pDisplayInfo);

        if(IsScalingPossibleByHw(pNodeToDisplay, pDisplayInfo, isSrcInterlaced))
        {
            // Scaling possible with the Secondary picture (aka Decimated)
            return true;
        }


        // In case of Progressive source a bigger dowscaling can be achieved by forcing the source to be treated as Interlaced
        if(!pDisplayInfo->m_isSrcInterlaced)
        {
            TRCBL(TRC_ID_MAIN_INFO);
            TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Secondary picture forced as Interlaced");
            isSrcInterlaced = true;

            if(IsScalingPossibleByHw(pNodeToDisplay, pDisplayInfo, isSrcInterlaced))
            {
                // Scaling possible thanks to this trick. The source is now considered as Interlaced
                pDisplayInfo->m_isSrcInterlaced = true;
                TRC( TRC_ID_MAIN_INFO, "Src now considered as Interlaced" );
                return true;
            }
        }
    }
    else
    {
        // No decimation available

        // In case of Progressive source a bigger dowscaling can be achieved by forcing the source to be treated as Interlaced
        if(!pDisplayInfo->m_isSrcInterlaced)
        {
            TRCBL(TRC_ID_MAIN_INFO);
            TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Primary picture forced as Interlaced");
            isSrcInterlaced = true;

            if(IsScalingPossibleByHw(pNodeToDisplay, pDisplayInfo, isSrcInterlaced))
            {
                // Scaling possible thanks to this trick. The source is now considered as Interlaced
                pDisplayInfo->m_isSrcInterlaced = true;
                TRC( TRC_ID_MAIN_INFO, "Src now considered as Interlaced" );
                return true;
            }
        }
    }


    TRC( TRC_ID_ERROR, "!!! Scaling NOT possible !!!" );
    return false;
}

bool CVdpPlane::IsZoomFactorOk(uint32_t vhsrc_input_width,
                               uint32_t vhsrc_input_height,
                               uint32_t vhsrc_output_width,
                               uint32_t vhsrc_output_height)
{
  TRC( TRC_ID_MAIN_INFO, "vhsrc_input_width = %d, vhsrc_input_height = %d, vhsrc_output_width = %d, vhsrc_output_height=%d",
            vhsrc_input_width,
            vhsrc_input_height,
            vhsrc_output_width,
            vhsrc_output_height);

  if( (vhsrc_input_width  <= vhsrc_output_width  * MAX_SCALING_FACTOR) &&
      (vhsrc_input_height <= vhsrc_output_height * MAX_SCALING_FACTOR) )
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool CVdpPlane::PrepareSetup()
{
  TRC( TRC_ID_VDP, "");

  // The first step is to configure the majority of the VDP control register.
  setControlRegister();

  m_pSetup->vhsrc_ctl = ( VHSRC_CTL_ENA_VHSRC |
                          VHSRC_CTL_COEF_LOAD_LINE( 3 ) |
                          VHSRC_CTL_PIX_LOAD_LINE( 4 ));


  m_pSetup->target_size = packRegister( m_vdpDisplayInfo.m_verticalFilterOutputSamples,
                                        m_vdpDisplayInfo.m_horizontalFilterOutputSamples );

  switch ( m_vdpDisplayInfo.m_selectedPicture.colorFmt )
    {
    case SURF_YCBCR420MB:
      set420MBMemoryAddressing_staticPart();
      break;

    case SURF_YCBCR422R:
    case SURF_YUYV:
      set422RInterleavedMemoryAddressing_staticPart();
      break;

    case SURF_YUV420:
    case SURF_YVU420:
    case SURF_YUV422P:
      setPlanarMemoryAddressing_staticPart();
      break;

    case SURF_YCbCr420R2B:
    case SURF_YCbCr422R2B:
      setRaster2BufferMemoryAddressing_staticPart();
      break;

    default:
      return false;
    }

  setupHorizontalScaling();

  if ( not m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      // Progressive content on either a progressive or interlaced display
      setupProgressiveVerticalScaling();
    }
  else if ( not m_outputInfo.isDisplayInterlaced )
    {
      setupDeinterlacedVerticalScaling();
    }
  else
    {
      setupInterlacedVerticalScaling();
    }

  // Must be called after the scaling setup as the subpixel positions are needed to determine the filter choice.
  selectScalingFilters();

  return true;
}

bool CVdpPlane::SetDynamicDisplayInfo()
{
  TRC( TRC_ID_VDP, "");

  switch ( m_vdpDisplayInfo.m_selectedPicture.colorFmt )
    {
    case SURF_YCBCR420MB:
      set420MBMemoryAddressing_dynamicPart();
      break;

    case SURF_YCBCR422R:
    case SURF_YUYV:
      set422RInterleavedMemoryAddressing_dynamicPart();
      break;

    case SURF_YUV420:
    case SURF_YVU420:
    case SURF_YUV422P:
      setPlanarMemoryAddressing_dynamicPart();
      break;

    case SURF_YCbCr420R2B:
    case SURF_YCbCr422R2B:
      setRaster2BufferMemoryAddressing_dynamicPart();
      break;

    default:
      return false;
    }

  m_pSetup->pFieldSetup = SelectFieldSetup();

  uint32_t dei_ctl = m_pSetup->pFieldSetup->dei_ctl;

  // We only update a field's setup and the deinterlacing state if the deinterlacer is in use
  // and this is the first time we have seen this field ( or the alternate field setup using the same image data ).
  if ( not m_pSetup->pFieldSetup->bFieldSeen )
    {
      bool isTopNode;

      switch ( dei_ctl & DEI_CTL_CVF_TYPE_MASK )
        {
        case DEI_CTL_CVF_TYPE_420_ODD:
        case DEI_CTL_CVF_TYPE_422_ODD:
          isTopNode = true;
          break;

        default:
          isTopNode = false;
          break;
        }

      // Set the full VDP control state for both the real field and alternate that uses the same image data but with a different filter setup
      // This is needed when deinterlacing for resize on an interlaced display.
      if ( isTopNode )
        {
          m_pSetup->topField.bFieldSeen = m_pSetup->altBotField.bFieldSeen = true;
          m_pSetup->topField.dei_ctl = dei_ctl;
          m_pSetup->altBotField.dei_ctl = dei_ctl;
        }
      else
        {
          m_pSetup->botField.bFieldSeen = m_pSetup->altTopField.bFieldSeen = true;
          m_pSetup->botField.dei_ctl = dei_ctl;
          m_pSetup->altTopField.dei_ctl = dei_ctl;
        }

      TRC( TRC_ID_UNCLASSIFIED, "dei_ctl: %#.8x CY: %#.8x CC: %#.8x",
           m_pSetup->pFieldSetup->dei_ctl, m_pSetup->pFieldSetup->luma_ba, m_pSetup->pFieldSetup->chroma_ba );
    }

  return true;
}

void CVdpPlane::WriteSetup()
{
  TRC( TRC_ID_VDP, "");

  if ( m_vdpDisplayInfo.m_areIOWindowsValid )
    {
      m_videoPlug->WritePlugSetup( m_vdpDisplayInfo.m_videoPlugSetup, isVisible() );
      EnableHW();
      m_isPlaneActive = true;

      // As we are starting a new buffer, disable the pipe until the field part has been written.
      WriteRegister( DEI_CTL, DEI_CTL_INACTIVE );

      if ( not m_isPictureRepeated )
        {
          WriteRegister( DEI_T3I_CTL, m_pSetup->dei_t3i_ctl );
          WriteRegister( DEI_VF_SIZE, m_pSetup->mb_stride );
          WriteRegister( DEI_YF_FORMAT, m_pSetup->luma_format );
          WriteRegister( DEI_CF_FORMAT, m_pSetup->chroma_format );

          WriteRegister( VHSRC_CTL, m_pSetup->vhsrc_ctl );
          WriteRegister( VHSRC_TARGET_SIZE, m_pSetup->target_size );
          WriteRegister( VHSRC_Y_SIZE, m_pSetup->luma_size );
          WriteRegister( VHSRC_C_SIZE, m_pSetup->chroma_size );
          WriteRegister( VHSRC_Y_HSRC, m_pSetup->luma_hsrc );
          WriteRegister( VHSRC_C_HSRC, m_pSetup->chroma_hsrc );
          WriteRegister( VHSRC_NLZZD_Y, 0 );
          WriteRegister( VHSRC_NLZZD_C, 0 );
          WriteRegister( VHSRC_PDELTA, 0 );

          for ( uint32_t i = 0; i < N_ELEMENTS( m_pSetup->luma_line_stack ); i++ )
            {
              WriteRegister( DEI_YF_STACK_L0 + ( i * sizeof( uint32_t )), m_pSetup->luma_line_stack[i] );
              WriteRegister( DEI_CF_STACK_L0 + ( i * sizeof( uint32_t )), m_pSetup->chroma_line_stack[i] );
            }

          for ( uint32_t i = 0; i < N_ELEMENTS( m_pSetup->luma_pixel_stack ); i++ )
            {
              WriteRegister( DEI_YF_STACK_P0 + ( i * sizeof( uint32_t )), m_pSetup->luma_pixel_stack[i] );
              WriteRegister( DEI_CF_STACK_P0 + ( i * sizeof( uint32_t )), m_pSetup->chroma_pixel_stack[i] );
            }
        }

      // All of these registers are double buffered and do not take effect until the next vsync.
      // If our vsync handling has been badly delayed and a new sync happens while we are in the middle of this update,
      // we could crash the display pipe with an inconsistent setup.
      // So the first thing we do is write the disable value to the control register so if that does happen
      // the VDP will get turned off for a frame. The display will glitch.
      WriteRegister( DEI_VIEWPORT_ORIG, m_pSetup->pFieldSetup->luma_xy );
      WriteRegister( DEI_VIEWPORT_SIZE, m_pSetup->pFieldSetup->vp_size );

      // current luma & chroma
      WriteRegister( DEI_CYF_BA, m_pSetup->pFieldSetup->luma_ba );
      WriteRegister( DEI_CCF_BA, m_pSetup->pFieldSetup->chroma_ba );
      WriteRegister( VHSRC_Y_VSRC, m_pSetup->pFieldSetup->luma_vsrc );
      WriteRegister( VHSRC_C_VSRC, m_pSetup->pFieldSetup->chroma_vsrc );

      // Make the real control register value the very last thing we write.
      WriteRegister( DEI_CTL, m_pSetup->pFieldSetup->dei_ctl );
    }
  else
    {
      TRC(TRC_ID_MAIN_INFO, "IOWindows not valid");
      NothingToDisplay();
    }
}

void CVdpPlane::SetSrcDisplayInfos()
{
  TRC( TRC_ID_VDP, "");

  // pixelsPerLine
  m_vdpDisplayInfo.m_pixelsPerLine = m_vdpDisplayInfo.m_selectedPicture.pitch / ( m_vdpDisplayInfo.m_selectedPicture.pixelDepth >> 3 );

  // Convert the source origin to fixed point format ready for setting up the resize filters.
  // Note that the incoming coordinates are in multiples of a 16th of a pixel/scanline.
  m_vdpDisplayInfo.m_srcFrameRectFixedPointX = ValToFixedPoint( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, 16 );
  m_vdpDisplayInfo.m_srcFrameRectFixedPointY = ValToFixedPoint( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, 16 );

}

void CVdpPlane::SetHWDisplayInfos()
{
  TRC( TRC_ID_VDP, "");

  // it is totally ok to dynamically modify m_ulMaxLineStep here,
  // as the only place where it is used to calculate information that can be exported to the outside is in Create(),
  // which has happened long ago.
  // The max line step needs to correctly reflect the current pitch and is used in AdjustBufferInfoForScaling().
  if ( m_vdpDisplayInfo.m_selectedPicture.pitch == 0 )
    return;

  m_ulMaxLineStep = 65535 / m_vdpDisplayInfo.m_selectedPicture.pitch;

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    m_ulMaxLineStep /= 2;
}

void CVdpPlane::AdjustBufferInfoForScaling()
{
  TRC( TRC_ID_VDP, "InWin : %d %d %d %d, OutWin : %d %d %d %d",
       m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height,
       m_vdpDisplayInfo.m_dstFrameRect.x, m_vdpDisplayInfo.m_dstFrameRect.y, m_vdpDisplayInfo.m_dstFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.height );

  if ( not m_vdpDisplayInfo.m_isSrcInterlaced and m_outputInfo.isDisplayInterlaced )
    {
      if ( m_ulMaxLineStep > 1 )
        {
          TRC( TRC_ID_MAIN_INFO, "using line skip ( max %u )", m_ulMaxLineStep );
        }
      else
        {
          // without a P2I block we have to convert to interlaced using a
          // 2x downscale. But, if the hardware cannot do that or the overall
          // scale then goes outside the hardware capabilities, we treat the
          // source as interlaced instead.
          // We convert a progressive Frame into an Interlaced Field so we should consider
          // the Src Frame Height and the dst Field Height
          bool convert_to_interlaced = ( m_vdpDisplayInfo.m_dstHeight < ( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height * m_fixedpointONE / m_ulMaxVSrcInc ));

          if ( convert_to_interlaced )
            {
              TRC( TRC_ID_MAIN_INFO, "converting source to interlaced for downscaling" );
              m_vdpDisplayInfo.m_isSrcInterlaced = true;
              m_vdpDisplayInfo.m_selectedPicture.srcHeight = m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height/2;
              m_pCurrNode->m_firstFieldType = GNODE_TOP_FIELD;
            }
        }
    }

  // Skip hardware buffer configuration if IO windows are invalid
  if ( not m_vdpDisplayInfo.m_areIOWindowsValid )
    {
      return;
    }

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      // Also change the vertical start position from frame to field coordinates,
      // unless we are using the hardware de-interlacer in which case we keep it in frame coordinates.
      // Remember that this value is in the fixed point format.
      m_vdpDisplayInfo.m_srcFrameRectFixedPointY /= 2;
    }

  CalculateHorizontalScaling();
  CalculateVerticalScaling();

  // Now adjust the source coordinate system to take into account line skipping
  m_vdpDisplayInfo.m_srcFrameRectFixedPointY /= m_vdpDisplayInfo.m_line_step;

  // Define the Y coordinate limit in the source image,
  // used to ensure we do not go outside of the required source image crop when the Y position is adjusted for re-scale purposes.
  m_vdpDisplayInfo.m_maxYCoordinate = (( m_vdpDisplayInfo.m_srcFrameRectFixedPointY / m_fixedpointONE )
                                    + m_vdpDisplayInfo.m_verticalFilterInputSamples - 1 );

  CalculateViewport();
}

void CVdpPlane::setControlRegister()
{
  TRC( TRC_ID_VDP, "");

  // Basic control setup for directional deinterlacing, but with the deinterlacer bypassed by default.
  // If necessary the deinterlacer will be enabled later.
  m_pSetup->topField.dei_ctl
    = m_pSetup->botField.dei_ctl
    = m_pSetup->altTopField.dei_ctl
    = m_pSetup->altBotField.dei_ctl
    = 0;

  // Now set the field / frame specific control flags.
  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 )
        {
          m_pSetup->topField.dei_ctl |= DEI_CTL_CVF_TYPE_420_ODD;
          m_pSetup->botField.dei_ctl |= DEI_CTL_CVF_TYPE_420_EVEN;
          m_pSetup->altTopField.dei_ctl |= DEI_CTL_CVF_TYPE_420_EVEN;
          m_pSetup->altBotField.dei_ctl |= DEI_CTL_CVF_TYPE_420_ODD;
        }
      else
        {
          m_pSetup->topField.dei_ctl |= DEI_CTL_CVF_TYPE_422_ODD;
          m_pSetup->botField.dei_ctl |= DEI_CTL_CVF_TYPE_422_EVEN;
          m_pSetup->altTopField.dei_ctl |= DEI_CTL_CVF_TYPE_422_EVEN;
          m_pSetup->altBotField.dei_ctl |= DEI_CTL_CVF_TYPE_422_ODD;
        }
    }
  else
    {
      if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 )
        {
          m_pSetup->topField.dei_ctl |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
          m_pSetup->botField.dei_ctl |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
          m_pSetup->altTopField.dei_ctl |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
          m_pSetup->altBotField.dei_ctl |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
        }
      else
        {
          m_pSetup->topField.dei_ctl |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
          m_pSetup->botField.dei_ctl |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
          m_pSetup->altTopField.dei_ctl |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
          m_pSetup->altBotField.dei_ctl |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
        }
    }
}

// This is the magic setup for the VDP's memory access.
// Don't ask, most of this is verbatim from documentation and reference drivers
// without any real explanation ( particularly for 420MB format ).
// Even more scary than the filter setup
void CVdpPlane::set420MBMemoryAddressing_staticPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t mb_height = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.height, 32 );
  const uint32_t mb_width = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.pitch, 32 );
  const uint32_t mb_half_width = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.pitch, 16 );

  m_pSetup->dei_t3i_ctl = ( DEI_T3I_CTL_OPCODE_SIZE_8 |
                            DEI_T3I_CTL_MACRO_BLOCK_ENABLE | m_MBChunkSize );

  m_pSetup->mb_stride = _ALIGN_UP( m_vdpDisplayInfo.m_selectedPicture.pitch, 16 );

  m_pSetup->luma_format = DEI_FORMAT_REVERSE;
  m_pSetup->luma_pixel_stack[0] = DEI_STACK( 56, ( 2 * mb_width ));
  m_pSetup->luma_pixel_stack[1] = DEI_STACK( 8, 2 );

  m_pSetup->chroma_format = ( DEI_FORMAT_REVERSE | DEI_REORDER_15263748 );
  m_pSetup->chroma_pixel_stack[0] = DEI_STACK( 44, mb_width );
  m_pSetup->chroma_pixel_stack[1] = DEI_STACK( 12, 2 );
  m_pSetup->chroma_pixel_stack[2] = DEI_STACK( 4, 2 );

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      m_pSetup->luma_line_stack[0] = DEI_STACK(( 64 * mb_half_width ) - 37, mb_height );
      m_pSetup->luma_line_stack[1] = DEI_STACK( 27, 2 );
      m_pSetup->luma_line_stack[2] = DEI_STACK( 3, 4 );
      m_pSetup->luma_line_stack[3] = DEI_STACK( -1, 2 );

      m_pSetup->chroma_line_stack[0] = DEI_STACK(( 32 * mb_half_width ) - 33, mb_height );
      m_pSetup->chroma_line_stack[1] = DEI_STACK( 31, 2 );
      m_pSetup->chroma_line_stack[2] = DEI_STACK( 3, 2 );
      m_pSetup->chroma_line_stack[3] = DEI_STACK( -1, 2 );
    }
  else
    {
      m_pSetup->luma_line_stack[0] = DEI_STACK(( 64 * mb_half_width ) - 53, mb_height );
      m_pSetup->luma_line_stack[1] = DEI_STACK( 11, 2 );
      m_pSetup->luma_line_stack[2] = DEI_STACK( -13, 4 );
      m_pSetup->luma_line_stack[3] = DEI_STACK( -17, 2 );
      m_pSetup->luma_line_stack[4] = DEI_STACK( 16, 2 );

      m_pSetup->chroma_line_stack[0] = DEI_STACK(( 32 * mb_half_width ) - 41, mb_height );
      m_pSetup->chroma_line_stack[1] = DEI_STACK( 23, 2 );
      m_pSetup->chroma_line_stack[2] = DEI_STACK( -5, 2 );
      m_pSetup->chroma_line_stack[3] = DEI_STACK( -9, 2 );
      m_pSetup->chroma_line_stack[4] = DEI_STACK( 8, 2 );
    }
}

void CVdpPlane::set420MBMemoryAddressing_dynamicPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t baseAddress = m_vdpDisplayInfo.m_video_buffer_addr;
  const uint32_t chromaBaseAddress = baseAddress + m_vdpDisplayInfo.m_chroma_buffer_offset;
  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      setInterlacedAddress( m_pSetup,
                            ( baseAddress + 8 ), ( chromaBaseAddress + 8 ),
                            ( baseAddress + 136 ), ( chromaBaseAddress + 72 ));
    }
  else
    {
      setProgressiveAddress( m_pSetup, ( baseAddress + 8 ), ( chromaBaseAddress + 8 ));
    }
}

void CVdpPlane::set422RInterleavedMemoryAddressing_staticPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t nlines = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.height, ( m_vdpDisplayInfo.m_isSrcInterlaced ? 2 : 1 ));

  // This is the number of STBus 8byte words in a line rounded up to also be a multiple of 2 * the bus opcode size.
  // The programming is mostly done as if we were only reading 8bit luma data.
  // But we also have to take into account chroma is interleaved and pulled at the same time
  // and this puts an extra constraint on the number of whole STBus words read per line.
  // Note that this is done to make sure we do not get any pixel pipeline underflows,
  // but it does not guarantee that the image will render correctly if its stride is not already this multiple.
  const uint32_t busTransactionsPerLine = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_pixelsPerLine, ( m_RasterOpcodeSize * 2 ));
  const uint32_t bytesReadPerLine = busTransactionsPerLine * ( m_RasterOpcodeSize * 2 );
  const uint32_t wordsPerLine = bytesReadPerLine / STBUS_NBR_OF_BYTES_PER_WORD;

  setRasterT3IBusAccesses( m_pSetup, m_RasterOpcodeSize, m_RasterChunkSize );

  m_pSetup->dei_t3i_ctl |= DEI_T3I_CTL_LUMA_CHROMA_BUFFERS;

  if ( m_vdpDisplayInfo.m_selectedPicture.colorFmt == SURF_YUYV )
    m_pSetup->dei_t3i_ctl |= DEI_T3I_CTL_LUMA_CHROMA_ENDIANESS;

  m_pSetup->luma_format = DEI_FORMAT_IDENTITY;
  m_pSetup->chroma_format = ( DEI_FORMAT_IDENTITY | DEI_REORDER_IDENTITY );

  m_pSetup->luma_pixel_stack[0] = DEI_STACK( 1, wordsPerLine );

  // This is the number of words to move the read address by between each line in the image.
  // This is again programmed as if we only had 8bit Luma data and the hardware sorts out the interleaved chroma.
  // The size of the line needs to be a multiple of the STBUS word size.
  uint32_t lineOffset = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_pixelsPerLine, STBUS_NBR_OF_BYTES_PER_WORD );

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      lineOffset *= 2;
    }

  // We need to split line reading into two loops to support 1080p, as each loop only supports 1024 iterations.
  // But it doesn't harm to always do it and simplify the logic.
  m_pSetup->luma_line_stack[0] = DEI_STACK( lineOffset, 2 );
  m_pSetup->luma_line_stack[1] = DEI_STACK( lineOffset, DIV_ROUNDED_UP( nlines, 2 ));
}

void CVdpPlane::set422RInterleavedMemoryAddressing_dynamicPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t baseAddress = m_vdpDisplayInfo.m_video_buffer_addr;

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      setInterlacedAddress( m_pSetup,
                            baseAddress, 0,
                            ( baseAddress + m_vdpDisplayInfo.m_selectedPicture.pitch ), 0 );
    }
  else
    {
      setProgressiveAddress( m_pSetup, baseAddress, 0 );
    }
}

void CVdpPlane::setPlanarMemoryAddressing_staticPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t nlines = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.height, ( m_vdpDisplayInfo.m_isSrcInterlaced ? 2 : 1 ));
  const uint32_t chromalines = DIV_ROUNDED_UP( nlines, ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 ? 2 : 1 ));
  const uint32_t wordsperline = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_pixelsPerLine, STBUS_NBR_OF_BYTES_PER_WORD );
  const uint32_t wordpairsperline = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_pixelsPerLine, ( STBUS_NBR_OF_BYTES_PER_WORD * 2 ));
  const uint32_t uvoffset = (( wordsperline / 2 )
                             * ( m_vdpDisplayInfo.m_selectedPicture.height / ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 ? 2 : 1 )) );

  m_pSetup->dei_t3i_ctl = DEI_T3I_CTL_OPCODE_SIZE_8 | m_PlanarChunkSize;
  m_pSetup->luma_format = DEI_FORMAT_IDENTITY;

  // No chroma endianess swap, however two consecutive 8byte words are "shuffled" together to produce CbCrCbCr...
  // from one word of 8 * Cb and one word of 8 * Cr.
  m_pSetup->chroma_format = ( DEI_FORMAT_IDENTITY | DEI_REORDER_WORD_INTERLEAVE );

  m_pSetup->luma_pixel_stack[0] = DEI_STACK( 1, wordsperline );

  if ( m_vdpDisplayInfo.m_selectedPicture.colorFmt == SURF_YVU420 )
    {
      // Cr followed by Cb, we cannot use the chroma reordering/endian conversion to get the bytes in the right order
      // so we have to arrange to read Cb first.
      m_pSetup->chroma_pixel_stack[0] = DEI_STACK(( uvoffset + 1 ), wordpairsperline );
      m_pSetup->chroma_pixel_stack[1] = DEI_STACK( -uvoffset, 2 );
    }
  else
    {
      // Easy case, Cb followed by Cr
      m_pSetup->chroma_pixel_stack[0] = DEI_STACK(( 1 - uvoffset ), wordpairsperline );
      m_pSetup->chroma_pixel_stack[1] = DEI_STACK( uvoffset, 2 );
    }

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      m_pSetup->chroma_line_stack[0] = DEI_STACK( wordsperline, chromalines );
      m_pSetup->luma_line_stack[0] = DEI_STACK( wordsperline * 2, nlines );
    }
  else
    {
      // We need to split into two loops to support 1080p, each loop only supports 1024 iterations.
      m_pSetup->chroma_line_stack[0] = DEI_STACK( wordsperline / 2, 2 );
      m_pSetup->chroma_line_stack[1] = DEI_STACK( wordsperline / 2,
                                                  DIV_ROUNDED_UP( chromalines, 2 ));

      m_pSetup->luma_line_stack[0] = DEI_STACK( wordsperline, 2 );
      m_pSetup->luma_line_stack[1] = DEI_STACK( wordsperline,
                                                DIV_ROUNDED_UP( nlines, 2 ));
    }
}

void CVdpPlane::setPlanarMemoryAddressing_dynamicPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t wordsperline = DIV_ROUNDED_UP( m_vdpDisplayInfo.m_pixelsPerLine, STBUS_NBR_OF_BYTES_PER_WORD );
  const uint32_t baseAddress = m_vdpDisplayInfo.m_video_buffer_addr;
  uint32_t chromaBaseAddress = baseAddress + m_vdpDisplayInfo.m_chroma_buffer_offset;
  const uint32_t uvoffset = (( wordsperline / 2 )
                             * ( m_vdpDisplayInfo.m_selectedPicture.height / ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 ? 2 : 1 )) );

  if ( m_vdpDisplayInfo.m_selectedPicture.colorFmt == SURF_YVU420 )
    {
      // Cr followed by Cb, we cannot use the chroma reordering/endian conversion to get the bytes in the right order
      // so we have to arrange to read Cb first.
      chromaBaseAddress += ( uvoffset * 8 );
    }

  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      const uint32_t secondLineAddress = baseAddress + m_vdpDisplayInfo.m_selectedPicture.pitch;
      const uint32_t chromaSecondLineAddress = ( chromaBaseAddress
                                                 + ( m_vdpDisplayInfo.m_selectedPicture.pitch / 2 ));

      setInterlacedAddress( m_pSetup,
                            baseAddress, chromaBaseAddress,
                            secondLineAddress, chromaSecondLineAddress );
    }
  else
    {
      setProgressiveAddress( m_pSetup, baseAddress, chromaBaseAddress );
    }
}

void CVdpPlane::setRaster2BufferMemoryAddressing_staticPart()
{
  TRC( TRC_ID_VDP, "");

  unsigned int granularity;
  unsigned int pixel_ceiling, pixel_offset;
  unsigned int luma_line_ceiling_L0, chroma_line_ceiling_L0;
  unsigned int luma_line_ceiling_L1, chroma_line_ceiling_L1;
  unsigned int line_offset;
  const uint32_t nlines = m_vdpDisplayInfo.m_isSrcInterlaced ? DIV_ROUNDED_UP( m_vdpDisplayInfo.m_selectedPicture.height, 2 ) : m_vdpDisplayInfo.m_selectedPicture.height;

  setRasterT3IBusAccesses( m_pSetup, m_RasterOpcodeSize, m_RasterChunkSize );

  m_pSetup->luma_format = DEI_FORMAT_IDENTITY;
  m_pSetup->chroma_format = DEI_FORMAT_IDENTITY;

  // When reading memory, the smallest "granularity" depends of the OpcodeSize and the STBUS Width ( = 8 Bytes )
  // For example, with DEI_T3I_CTL_OPCODE_SIZE_32, there is a granularity of 4 Words
  granularity = m_RasterOpcodeSize / STBUS_NBR_OF_BYTES_PER_WORD;

  // Configuration of the pixel Stacks ( ie horizontal access )

  // The line ceiling corresponds to the number of STBUS operations needed to read one video line BUT there is a HW constraint:
  // It should be rounded up to the closest multiple of the granularity.
  // Example with ( pixelsperline = 720 ) and ( OpcodeSize = 32 ):
  // -There is a granularity of 4 Words ( = 32 Bytes )
  // -There will have 720 / 8 = 90 read of 8 Bytes. 90 is not a multiple of 4 so it is rounded up to 92
  pixel_ceiling = m_vdpDisplayInfo.m_pixelsPerLine / STBUS_NBR_OF_BYTES_PER_WORD;
  pixel_ceiling = DIV_ROUNDED_UP( pixel_ceiling, granularity ) * granularity;

  pixel_offset = 1; // For each pixel, we progress of 1 Bytes

  m_pSetup->luma_pixel_stack[0] = DEI_STACK( pixel_offset, pixel_ceiling );
  m_pSetup->chroma_pixel_stack[0] = DEI_STACK( pixel_offset, pixel_ceiling ); // Horizontally there is as much luma pixels as chroma pixels so the configuration is the same

  // Configuration of the line Stacks ( ie vertical access )

  // The line_offset is the same for luma and chroma
  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      // In case of Interlaced picture, the Stride should be double to skip alternate fields
      line_offset = ( m_vdpDisplayInfo.m_selectedPicture.pitch * 2 ) / STBUS_NBR_OF_BYTES_PER_WORD;
    }
  else
    {
      line_offset = m_vdpDisplayInfo.m_selectedPicture.pitch / STBUS_NBR_OF_BYTES_PER_WORD;
    }

  if ( line_offset % granularity != 0 )
    {
      TRC( TRC_ID_ERROR, "IMPOSSIBLE to display this 4:2:0 R2B picture ! A padding is necessary ! " );
    }

  // Two line stacks are used to read the image,
  // for example, with a 1080P picture we will get luma_line_ceiling_L0 = 2 and luma_line_ceiling_L1 = 540.
  // This is only necessary if the number of lines exceeds 1024, but it does no harm to do it all the time and it simplifies the logic.
  luma_line_ceiling_L0 = 2;
  luma_line_ceiling_L1 = DIV_ROUNDED_UP( nlines, 2 );

  // Also do the same with the chroma even if it is 420 and has half the number of lines anyway.
  chroma_line_ceiling_L0 = 2;
  chroma_line_ceiling_L1 = m_vdpDisplayInfo.m_selectedPicture.isSrc420 ? DIV_ROUNDED_UP( luma_line_ceiling_L1 , 2 ) : luma_line_ceiling_L1;

  m_pSetup->luma_line_stack[0] = DEI_STACK( line_offset, luma_line_ceiling_L0 );
  m_pSetup->chroma_line_stack[0] = DEI_STACK( line_offset, chroma_line_ceiling_L0 );

  m_pSetup->luma_line_stack[1] = DEI_STACK( line_offset, luma_line_ceiling_L1 );
  m_pSetup->chroma_line_stack[1] = DEI_STACK( line_offset, chroma_line_ceiling_L1 );
}

void CVdpPlane::setRaster2BufferMemoryAddressing_dynamicPart()
{
  TRC( TRC_ID_VDP, "");

  const uint32_t baseAddress = m_vdpDisplayInfo.m_video_buffer_addr;
  const uint32_t chromaBaseAddress = baseAddress + m_vdpDisplayInfo.m_chroma_buffer_offset;

  // Configuration of the Base Addresses
  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      // To get to the second field we add the original stride to the base addresses.
      const uint32_t secondLumaLineAddress = baseAddress + m_vdpDisplayInfo.m_selectedPicture.pitch;
      const uint32_t secondChromaLineAddress = chromaBaseAddress + m_vdpDisplayInfo.m_selectedPicture.pitch;

      setInterlacedAddress( m_pSetup,
                            baseAddress, chromaBaseAddress,
                            secondLumaLineAddress, secondChromaLineAddress );
    }
  else
    {
      setProgressiveAddress( m_pSetup, baseAddress, chromaBaseAddress );
    }
}

void CVdpPlane::setupProgressiveVerticalScaling()
{
  TRC( TRC_ID_VDP, "");

  // For progressive content we only need a single node, even on an interlaced display, as we use the top and bottom field setup in the node.
  // Note that the bottom field values will not be used when displaying on a progressive display.
  // If we are using a P2I then the top and bottom field filter setup is identical, the P2I field selection is setup on the fly.
  // Otherwise we need to adjust the start position by a source line.
  int bottomFieldAdjustment = ScaleVerticalSamplePosition( fixedpointONE );

  // First Luma
  calculateVerticalFilterSetup( m_pSetup->topField, 0, true );
  calculateVerticalFilterSetup( m_pSetup->botField, bottomFieldAdjustment, true );

  // Now Chroma
  calculateVerticalFilterSetup( m_pSetup->topField, 0, false );
  calculateVerticalFilterSetup( m_pSetup->botField, bottomFieldAdjustment, false );

  // The alternate field setups ( for inter-field interpolation ) are identical to the main ones for progressive content.
  // This keeps the presentation logic simpler.
  m_pSetup->altTopField = m_pSetup->topField;
  m_pSetup->altBotField = m_pSetup->botField;
}

void CVdpPlane::setupDeinterlacedVerticalScaling()
{
  TRC( TRC_ID_VDP, "");

  // We deinterlace, without having a real hardware deinterlacer, by scaling up each field to produce a whole frame.
  // To make consecutive frames appear to be in the same place
  // we move the source start position by + / - 1 / 4 of the distance between two source lines in the SAME field.
  int topSamplePosition = fixedpointQUARTER;
  int botSamplePosition = -fixedpointQUARTER;

  // Luma
  calculateVerticalFilterSetup( m_pSetup->topField, topSamplePosition, true );
  calculateVerticalFilterSetup( m_pSetup->botField, botSamplePosition, true );

  // Chroma
  calculateVerticalFilterSetup( m_pSetup->topField, topSamplePosition, false );
  calculateVerticalFilterSetup( m_pSetup->botField, botSamplePosition, false );

  // As this must be presented on a progressive display the alternate field setups are never needed.
}

void CVdpPlane::setupInterlacedVerticalScaling()
{
  TRC( TRC_ID_VDP, "");

  // The first sample position to interpolate a top field from the bottom field source data
  // is just a coordinate translation between the top and bottom source fields.
  int altTopSamplePosition = -fixedpointHALF;

  // The first sample position for the _display's_ bottom field, interpolated from the top field data, is + 1 / 2 ( but scaled this time ).
  // This is because the distance between the source samples located on two consecutive top field display lines has changed;
  // the bottom field display line sample must lie half way between them in the source coordinate system.
  int altBotSamplePosition = ScaleVerticalSamplePosition( fixedpointHALF );

  // You might think that the sample position for the bottom field generated from real bottom field data should always be 0.
  // But this is quite complicated, because as we have seen above when the image is scaled
  // the mapping of the bottom field's first display line back to the source image changes.
  // Plus we have a source coordinate system change because the first sample of the bottom field data is referenced as 0
  // just as the first sample of the top field data is.
  // So the start position is the source sample position, of the display's bottom field in the top field's coordinate system,
  // translated to the bottom field's coordinate system.
  int botSamplePosition = altBotSamplePosition - fixedpointHALF;

  // Work out the real top and bottom fields
  calculateVerticalFilterSetup( m_pSetup->topField, 0, true );
  calculateVerticalFilterSetup( m_pSetup->botField, botSamplePosition, true );

  // Now interpolate a bottom field from the top field contents in order to do slow motion and pause without motion jitter.
  calculateVerticalFilterSetup( m_pSetup->altBotField, altBotSamplePosition, true );

  // Now interpolate a top field from the bottom field contents.
  calculateVerticalFilterSetup( m_pSetup->altTopField, altTopSamplePosition, true );

  // Now do exactly the same again for the chroma.
  calculateVerticalFilterSetup( m_pSetup->topField, 0, false );
  calculateVerticalFilterSetup( m_pSetup->botField, botSamplePosition, false );

  calculateVerticalFilterSetup( m_pSetup->altBotField, altBotSamplePosition, false );
  calculateVerticalFilterSetup( m_pSetup->altTopField, altTopSamplePosition, false );
}

void CVdpPlane::setupHorizontalScaling()
{
  TRC( TRC_ID_VDP, "");

  int x, width, chroma_width;
  int subpixelpos, repeat;

  // Horizontal filter setup is reasonably simple and is the same for both fields when dealing with the source as two interlaced fields.
  // First of all get the start position in fixed point format, this is used as the basis for both the luma and chroma filter setup.

  width = m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width;
  chroma_width = width / 2;
  repeat = 3;

  x = FixedPointToInteger( m_vdpDisplayInfo.m_srcFrameRectFixedPointX, &subpixelpos );

  // Here is how we deal with luma starting on an odd pixel.
  // We adjust so the VDP memory interface actually starts with the pixel before it;
  // this is so we can get the correct associated first chroma sample into the chroma filter pipeline.
  // We correct the luma by reducing the repeat value in the filter tap setup,
  // causing the original pixel sample we wanted to be in the central position when the filter FSM starts.
  if (( x % 2 ) == 1 )
    {
      repeat--;
      x--;
      width++;
    }

  m_pSetup->luma_hsrc = packRegister(( subpixelpos | ( repeat << 13 )), // Hi 16 bits
                                     m_vdpDisplayInfo.m_hsrcinc ); // Lo 16 bits

  m_pSetup->luma_size = ( width & 0x7ff );

  // For the horizontal filter, with no scaling ( i.e. a src increment of 1 ),
  // no non-linear zoom and no subpixel pan / scan we must use the internal coefficients.
  if (( m_vdpDisplayInfo.m_hsrcinc != ( uint32_t ) m_fixedpointONE ) or ( subpixelpos != 0 ))
    m_pSetup->vhsrc_ctl |= VHSRC_CTL_ENA_YHF;

  TRC( TRC_ID_MAIN_INFO, "width: %d luma_size: 0x%x", width, m_pSetup->luma_size );

  // We appear to need to round up x + width to be a multiple of 8 pixels,
  // otherwise certain combinations cause the display to break as if the stride was wrong.
  // We think this is an interaction between the chroma HSRC and the T3I interface's state machines;
  // related to the fact we effectively pull image data in chunks of 8 pixels from memory.
  const int tmp = ( x + width ) % 8;

  if ( tmp != 0 )
    width += 8 - tmp;

  // This initializes the VDP viewport register values;
  // this routine must be called before setting up the vertical scaling, which modifies the top 16bits of these values for y position and height.
  // The horizontal parameters do not change for top and bottom fields.
  m_pSetup->topField.vp_size
    = m_pSetup->botField.vp_size
    = m_pSetup->altTopField.vp_size
    = m_pSetup->altBotField.vp_size
    = ( width & 0xffff );

  m_pSetup->topField.luma_xy
    = m_pSetup->botField.luma_xy
    = m_pSetup->altTopField.luma_xy
    = m_pSetup->altBotField.luma_xy
    = ( x & 0xffff );

  // Now for the chroma, which always has half the number of samples horizontally than the luma.
  // Chroma cannot end up starting on an odd sample, so the filter repeat is reset to 3.
  repeat = 3;

  x = FixedPointToInteger(( m_vdpDisplayInfo.m_srcFrameRectFixedPointX / 2 ), &subpixelpos );

  m_pSetup->chroma_hsrc = packRegister(( subpixelpos | ( repeat << 13 )), // Hi 16 bits
                                       m_vdpDisplayInfo.m_chroma_hsrcinc ); // Lo 16 bits

  m_pSetup->chroma_size = chroma_width & 0x7ff;

  if (( m_vdpDisplayInfo.m_chroma_hsrcinc != ( uint32_t )m_fixedpointONE ) or ( subpixelpos != 0 ))
    m_pSetup->vhsrc_ctl |= VHSRC_CTL_ENA_CHF;
}

// Resize filter setup for the video display pipeline used by QueueBuffer.
// Be afraid ... be very afraid !
void CVdpPlane::selectScalingFilters()
{
  TRC( TRC_ID_VDP, "");

  const int32_t lumaHphase = ( m_pSetup->luma_hsrc >> 16 ) & 0x1fff;
  const int32_t chromaHphase = ( m_pSetup->chroma_hsrc >> 16 ) & 0x1fff;

  // Warning, there is a corner case where the bottom field would pick a different vertical filter to the top field.
  // The scale factor would need to be 1x and one field would have a phase of 1 / 2 and the other would not have a phase of 1 / 2.
  // This need thinking about.
  const int32_t lumaVphase = ( m_pSetup->topField.luma_vsrc >> 16 ) & 0x1fff;
  const int32_t chromaVphase = ( m_pSetup->topField.chroma_vsrc >> 16 ) & 0x1fff;

  TRC( TRC_ID_MAIN_INFO, "srcFrameRect.width: %d dstFrameRect.width: %d", m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.width );
  TRC( TRC_ID_MAIN_INFO, "srcHeight: %d dstHeight: %d", m_vdpDisplayInfo.m_selectedPicture.srcHeight, m_vdpDisplayInfo.m_dstHeight );
  TRC( TRC_ID_MAIN_INFO, "hsrcinc: 0x%x vsrcinc: 0x%x", m_vdpDisplayInfo.m_hsrcinc, m_vdpDisplayInfo.m_vsrcinc );

  const uint32_t vc1Flag = ( m_pCurrNode->m_bufferDesc.src.flags & ( STM_BUFFER_SRC_VC1_POSTPROCESS_LUMA
                                                                          | STM_BUFFER_SRC_VC1_POSTPROCESS_CHROMA ));

  // We put the filter change on both the top and bottom nodes because we don't know which will get displayed first.
  m_pSetup->vhsrc_ctl |= ( VHSRC_CTL_ENA_VFILTER_UPDATE | VHSRC_CTL_ENA_HFILTER_UPDATE );

  // Note: we force the phase to be non zero in the horizontal filter selection code when we have a non-linear zoom,
  // to catch the corner case where the central area zoom ( hsrcinc ) is 1:1;
  // this ensures we do get a valid filter selected.
  m_pSetup->hfluma = m_Filter.SelectHorizontalLumaFilter( m_vdpDisplayInfo.m_hsrcinc, lumaHphase );
  m_pSetup->hfchroma = m_Filter.SelectHorizontalChromaFilter( m_vdpDisplayInfo.m_chroma_hsrcinc, chromaHphase );

  if ( vc1Flag & STM_BUFFER_SRC_VC1_POSTPROCESS_LUMA )
  {
    TRC( TRC_ID_MAIN_INFO, "Use Luma filter for VC1");
    m_pSetup->vfluma = m_Filter.SelectVC1VerticalLumaFilter( m_vdpDisplayInfo.m_vsrcinc,
                                                             lumaVphase,
                                                             m_pCurrNode->m_bufferDesc.src.post_process_luma_type );
  }
  else
  {
    m_pSetup->vfluma = m_Filter.SelectVerticalLumaFilter( m_vdpDisplayInfo.m_vsrcinc,
                                                          lumaVphase );
  }

  if ( vc1Flag & STM_BUFFER_SRC_VC1_POSTPROCESS_CHROMA )
  {
    TRC( TRC_ID_MAIN_INFO, "Use Chroma filter for VC1");
    m_pSetup->vfchroma = m_Filter.SelectVC1VerticalChromaFilter( m_vdpDisplayInfo.m_chroma_vsrcinc,
                                                                 chromaVphase,
                                                                 m_pCurrNode->m_bufferDesc.src.post_process_chroma_type );
  }
  else
  {
    m_pSetup->vfchroma = m_Filter.SelectVerticalChromaFilter( m_vdpDisplayInfo.m_chroma_vsrcinc,
                                                              chromaVphase );
  }

  // flip-flop tables
  m_idxFilter = ( m_idxFilter + 1 ) % 2;
  TRC(TRC_ID_MAIN_INFO, "Toggle to a new filter buffer (%d)", m_idxFilter);

  // Copy the required filter coefficients to the hardware filter tables
  vibe_os_memcpy_to_dma_area( &m_HFilter[m_idxFilter], 0, m_pSetup->hfluma, HFC_NB_COEFS );
  vibe_os_memcpy_to_dma_area( &m_HFilter[m_idxFilter], HFC_NB_COEFS, m_pSetup->hfchroma, HFC_NB_COEFS );
  vibe_os_memcpy_to_dma_area( &m_VFilter[m_idxFilter], 0, m_pSetup->vfluma, VFC_NB_COEFS );
  vibe_os_memcpy_to_dma_area( &m_VFilter[m_idxFilter], VFC_NB_COEFS, m_pSetup->vfchroma, VFC_NB_COEFS );

  // Program the hardware to point to the filter coefficient tables.
  // These pointers don't change, but the contents of the memory pointed to does.
  WriteRegister( VHSRC_HCOEF_BA, m_HFilter[m_idxFilter].ulPhysical );
  WriteRegister( VHSRC_VCOEF_BA, m_VFilter[m_idxFilter].ulPhysical );
}

int CVdpPlane::ScaleVerticalSamplePosition( int pos )
{
  TRC( TRC_ID_VDP, "");

  // This function scales a source vertical sample position by the true vertical scale factor between source and destination,
  // ignoring any scaling to convert between interlaced and progressive sources and destinations.
  // It is used to work out the starting sample position for the first line on the destination,
  // particularly for the start of a bottom display field.
  return( pos * m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height ) / m_vdpDisplayInfo.m_dstFrameRect.height;
}

void CVdpPlane::calculateVerticalFilterSetup( VdpFieldSetup &fieldSetup,
                                              int firstSampleOffset,
                                              bool doLumaFilter )

{
  int y, phase, repeat, height;

  int maxy = m_vdpDisplayInfo.m_maxYCoordinate;
  height = m_vdpDisplayInfo.m_verticalFilterInputSamples;
  repeat = 0;

  // We have been given the first sample's offset from src.y in the original field or frame coordinate system.
  // We now have to convert that to the actual coordinate system taking into account line skipping.
  // We could have done this in the caller, but doing it here makes the code marginally more readable.
  // Note that m_vdpDisplayInfo.m_src.y is already in the correct coordinate system,
  // having been adjusted when any required line skipping was calculated.
  int subpixelpos = m_vdpDisplayInfo.m_srcFrameRectFixedPointY + ( firstSampleOffset / m_vdpDisplayInfo.m_line_step );

  TRC( TRC_ID_MAIN_INFO, " subpixelpos: 0x%08x height: %d dstheight: %d",
       subpixelpos, height, m_vdpDisplayInfo.m_dstHeight );

  if ( not doLumaFilter and m_vdpDisplayInfo.m_selectedPicture.isSrc420 )
    {
      // Special case for 420 Chroma filter setup when not using a deinterlacer,
      // which will upsample the chroma before the vertical filter.
      // Adjust the start position by-1 / 2 because 420 chroma samples are taken half way between two field lines.
      // As we only have half the number of chroma samples, divide the start position and height by 2.
      subpixelpos = ( subpixelpos - ( m_fixedpointONE / 2 )) / 2;
      height = height / 2;
      maxy = maxy / 2;
    }

  Get5TapFilterSetup( subpixelpos, height, repeat,
                      doLumaFilter ? m_vdpDisplayInfo.m_vsrcinc : m_vdpDisplayInfo.m_chroma_vsrcinc,
                      m_fixedpointONE );

  y = FixedPointToInteger( subpixelpos, &phase );

  height = LimitSizeToRegion( y, maxy, height );

  // For interlaced content, not being deinterlaced, we need to convert the y and number of input lines
  // which are currently in field lines to frame lines for the T3 memory interface.
  // The interface, which is told the content is interlaced then just goes and effectively divides them by 2 again !
  // However we don't use odd addresses for bottom fields, that is done by offsetting the base address of the image in the memory addressing setup.
  if ( m_vdpDisplayInfo.m_isSrcInterlaced )
    {
      height *= 2;
      y *= 2;
    }

  if ( doLumaFilter )
    {
      fieldSetup.luma_xy |= ( y << 16 );
      fieldSetup.vp_size |= ( height << 16 );
      fieldSetup.luma_vsrc = ( repeat << 29 ) | ( phase << 16 ) | m_vdpDisplayInfo.m_vsrcinc;
      TRC( TRC_ID_MAIN_INFO, "luma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x",
           y, phase, height, repeat, ( int )m_vdpDisplayInfo.m_vsrcinc );
    }
  else
    {
      fieldSetup.chroma_vsrc = ( repeat << 29 ) | ( phase << 16 ) | m_vdpDisplayInfo.m_chroma_vsrcinc;
      TRC( TRC_ID_MAIN_INFO, "chroma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x",
           y, phase, height, repeat, ( int )m_vdpDisplayInfo.m_chroma_vsrcinc );
    }
}

void CVdpPlane::CalculateHorizontalScaling()
{
  TRC( TRC_ID_VDP, "InWin : %d %d %d %d, OutWin : %d %d %d %d",
       m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height,
       m_vdpDisplayInfo.m_dstFrameRect.x, m_vdpDisplayInfo.m_dstFrameRect.y, m_vdpDisplayInfo.m_dstFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.height );

  // Calculate the scaling factors, with one extra bit of precision so we can round the result.
  m_vdpDisplayInfo.m_hsrcinc = ( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width * m_fixedpointONE * 2 ) / m_vdpDisplayInfo.m_dstFrameRect.width;

  if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 or m_vdpDisplayInfo.m_selectedPicture.isSrc422 )
    {
      // For formats with half chroma, we have to round up or down to an even number,
      // so that the chroma value which is half this value cannot lose precision.
      m_vdpDisplayInfo.m_hsrcinc += 1L<<1;
      m_vdpDisplayInfo.m_hsrcinc &= ~0x3;
      m_vdpDisplayInfo.m_hsrcinc >>= 1;
    }
  else
    {
      // As chroma is not an issue here just round the result and convert to the correct fixed point format.
      m_vdpDisplayInfo.m_hsrcinc += 1;
      m_vdpDisplayInfo.m_hsrcinc >>= 1;
    }

  TRC(TRC_ID_MAIN_INFO, "m_hsrcinc : %d, m_ulMinHSrcInc : %d, m_ulMaxHSrcInc : %d, dstFrameRect.width : %d", m_vdpDisplayInfo.m_hsrcinc, m_ulMinHSrcInc, m_ulMaxHSrcInc, m_vdpDisplayInfo.m_dstFrameRect.width);
  if ( m_vdpDisplayInfo.m_hsrcinc < m_ulMinHSrcInc )
    {
      m_vdpDisplayInfo.m_hsrcinc = m_ulMinHSrcInc;
      m_vdpDisplayInfo.m_dstFrameRect.width = ( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width * m_fixedpointONE ) / m_vdpDisplayInfo.m_hsrcinc;
    }

  if ( m_vdpDisplayInfo.m_hsrcinc > m_ulMaxHSrcInc )
    {
      m_vdpDisplayInfo.m_hsrcinc = m_ulMaxHSrcInc;
      m_vdpDisplayInfo.m_dstFrameRect.width = ( m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width * m_fixedpointONE ) / m_vdpDisplayInfo.m_hsrcinc;
    }

  // Chroma src used only for YUV formats on planes that support separate chroma filtering.
  if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 or m_vdpDisplayInfo.m_selectedPicture.isSrc422 )
    m_vdpDisplayInfo.m_chroma_hsrcinc = m_vdpDisplayInfo.m_hsrcinc / 2;
  else
    m_vdpDisplayInfo.m_chroma_hsrcinc = m_vdpDisplayInfo.m_hsrcinc;

  TRC( TRC_ID_MAIN_INFO, "m_hsrcinc : %d, dstFrameRect.width : %d", m_vdpDisplayInfo.m_hsrcinc, m_vdpDisplayInfo.m_dstFrameRect.width );
  TRC( TRC_ID_MAIN_INFO, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x", m_fixedpointONE, m_vdpDisplayInfo.m_hsrcinc,m_vdpDisplayInfo.m_chroma_hsrcinc );
}


void CVdpPlane::CalculateVerticalScaling()
{
  TRC( TRC_ID_VDP, "InWin : %d %d %d %d, OutWin : %d %d %d %d",
       m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height,
       m_vdpDisplayInfo.m_dstFrameRect.x, m_vdpDisplayInfo.m_dstFrameRect.y, m_vdpDisplayInfo.m_dstFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.height );

  unsigned long srcHeight;

  m_vdpDisplayInfo.m_line_step = 1;

  // If we are hardware de-interlacing then the source image is vertically upsampled before it gets to the resize filter.
  // If we are using the P2I block then the required number of samples out of the filter is twice the destination height ( which is now in field lines ).
  m_vdpDisplayInfo.m_verticalFilterOutputSamples = m_vdpDisplayInfo.m_dstHeight;

 restart:

  srcHeight = m_vdpDisplayInfo.m_selectedPicture.srcHeight / m_vdpDisplayInfo.m_line_step;
  m_vdpDisplayInfo.m_verticalFilterInputSamples = srcHeight;

  // Calculate the scaling factors, with one extra bit of precision so we can round the result.
  m_vdpDisplayInfo.m_vsrcinc = ( m_vdpDisplayInfo.m_verticalFilterInputSamples * m_fixedpointONE * 2 ) / m_vdpDisplayInfo.m_verticalFilterOutputSamples;

  if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 )
    {
      // For formats with half vertical chroma, we have to round up or down to an even number,
      // so that the chroma value which is half this value cannot lose precision.
      // When de-interlacing the hardware upsamples the chroma before the resize so it isn't an issue.
      m_vdpDisplayInfo.m_vsrcinc += 1L<<1;
      m_vdpDisplayInfo.m_vsrcinc &= ~0x3;
      m_vdpDisplayInfo.m_vsrcinc >>= 1;
    }
  else
    {
      // As chroma is not an issue here just round the result and convert to the correct fixed point format.
      m_vdpDisplayInfo.m_vsrcinc += 1;
      m_vdpDisplayInfo.m_vsrcinc >>= 1;
    }

  bool bRecalculateDstHeight = false;

  if ( m_vdpDisplayInfo.m_vsrcinc < m_ulMinVSrcInc )
    {
      m_vdpDisplayInfo.m_vsrcinc = m_ulMinVSrcInc;
      bRecalculateDstHeight = true;
    }

  if ( m_vdpDisplayInfo.m_vsrcinc > m_ulMaxVSrcInc )
    {
      if ( m_vdpDisplayInfo.m_line_step < m_ulMaxLineStep )
        {
          m_vdpDisplayInfo.m_line_step++;
          goto restart;
        }
      else
        {
          m_vdpDisplayInfo.m_vsrcinc = m_ulMaxVSrcInc;
          bRecalculateDstHeight = true;
        }
    }

  if ( m_vdpDisplayInfo.m_selectedPicture.isSrc420 )
    m_vdpDisplayInfo.m_chroma_vsrcinc = m_vdpDisplayInfo.m_vsrcinc / 2;
  else
    m_vdpDisplayInfo.m_chroma_vsrcinc = m_vdpDisplayInfo.m_vsrcinc;

  TRC( TRC_ID_MAIN_INFO, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x", m_fixedpointONE, m_vdpDisplayInfo.m_vsrcinc,m_vdpDisplayInfo.m_chroma_vsrcinc );

  if ( bRecalculateDstHeight )
    {
      m_vdpDisplayInfo.m_dstHeight = ( m_vdpDisplayInfo.m_verticalFilterInputSamples * m_fixedpointONE ) / m_vdpDisplayInfo.m_vsrcinc;
    }
}


void CVdpPlane::CalculateViewport()
{
  TRC( TRC_ID_VDP, "InWin : %d %d %d %d, OutWin : %d %d %d %d",
       m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.x, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.y, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.width, m_vdpDisplayInfo.m_selectedPicture.srcFrameRect.height,
       m_vdpDisplayInfo.m_dstFrameRect.x, m_vdpDisplayInfo.m_dstFrameRect.y, m_vdpDisplayInfo.m_dstFrameRect.width, m_vdpDisplayInfo.m_dstFrameRect.height );
  TRC( TRC_ID_VDP, "viewport - type : %d, win : %d %d %d %d", m_vdpDisplayInfo.m_pCurrentMode->mode_params.scan_type,
       m_vdpDisplayInfo.m_pCurrentMode->mode_params.active_area_start_pixel, m_vdpDisplayInfo.m_pCurrentMode->mode_params.active_area_start_line,
       m_vdpDisplayInfo.m_pCurrentMode->mode_params.active_area_width, m_vdpDisplayInfo.m_pCurrentMode->mode_params.active_area_height );

  // Now we know the destination viewport extents for the compositor/mixer,
  // which may get clipped by the active video area of the display mode.
  m_vdpDisplayInfo.m_viewport.startPixel = STCalculateViewportPixel( m_vdpDisplayInfo.m_pCurrentMode, m_vdpDisplayInfo.m_dstFrameRect.x );
  m_vdpDisplayInfo.m_viewport.stopPixel = STCalculateViewportPixel( m_vdpDisplayInfo.m_pCurrentMode, m_vdpDisplayInfo.m_dstFrameRect.x + m_vdpDisplayInfo.m_dstFrameRect.width - 1 );

  // We need to limit the number of output samples generated to the ( possibly clipped ) viewport width.
  m_vdpDisplayInfo.m_horizontalFilterOutputSamples = ( m_vdpDisplayInfo.m_viewport.stopPixel - m_vdpDisplayInfo.m_viewport.startPixel + 1 );
  TRC( TRC_ID_VDP, "samples = %u startpixel = %u stoppixel = %u", m_vdpDisplayInfo.m_horizontalFilterOutputSamples, m_vdpDisplayInfo.m_viewport.startPixel, m_vdpDisplayInfo.m_viewport.stopPixel );

  // The viewport line numbering is always frame based, even on an interlaced display
  m_vdpDisplayInfo.m_viewport.startLine = STCalculateViewportLine( m_vdpDisplayInfo.m_pCurrentMode, m_vdpDisplayInfo.m_dstFrameRect.y );
  m_vdpDisplayInfo.m_viewport.stopLine = STCalculateViewportLine( m_vdpDisplayInfo.m_pCurrentMode, m_vdpDisplayInfo.m_dstFrameRect.y + m_vdpDisplayInfo.m_dstFrameRect.height - 1 );
  TRC( TRC_ID_VDP, "startline = %u stopline = %u", m_vdpDisplayInfo.m_viewport.startLine,m_vdpDisplayInfo.m_viewport.stopLine );
}

VdpFieldSetup * CVdpPlane::SelectFieldSetup()
{
  if ( m_outputInfo.isDisplayInterlaced )
    {
      if ( not m_isTopFieldOnDisplay )
        {
          // bottom field currently on display - next vsync polarity is top
          if ( m_pCurrNode->m_srcPictureType != GNODE_TOP_FIELD )
            {
              TRC( TRC_ID_VDP, "altTopField");
              return &m_pSetup->altTopField;
            }
          else
            {
              TRC( TRC_ID_VDP, "topField");
              return &m_pSetup->topField;
            }
        }
      else
        {
          // top field currently on display - next vsync polarity is bottom
          if ( m_pCurrNode->m_srcPictureType != GNODE_BOTTOM_FIELD )
            {
              TRC( TRC_ID_VDP, "altBotField");
              return &m_pSetup->altBotField;
            }
          else
            {
              TRC( TRC_ID_VDP, "botField");
              return &m_pSetup->botField;
            }
        }
    }
  else
    {
      if ( m_pCurrNode->m_srcPictureType == GNODE_BOTTOM_FIELD )
        {
          TRC( TRC_ID_VDP, "botField");
          return &m_pSetup->botField;
        }
      else
        {
          TRC( TRC_ID_VDP, "topField");
          return &m_pSetup->topField;
        }
    }
}

bool CVdpPlane::AdjustIOWindowsForHWConstraints(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const
{
    /* VDP output width and height should be even */
    pDisplayInfo->m_dstFrameRect.height &= ~(0x1);
    pDisplayInfo->m_dstFrameRect.width  &= ~(0x1);

    return true;
}

void CVdpPlane::ResetEveryUseCases(void)
{
    vibe_os_zero_memory( &m_useCase, sizeof( m_useCase ));
}

bool CVdpPlane::IsScalingPossibleByHw(CDisplayNode    *pNodeToDisplay,
                                      CDisplayInfo    *pDisplayInfo,
                                      bool             isSrcInterlaced)
{
    uint32_t vhsrc_input_width;
    uint32_t vhsrc_input_height;
    uint32_t vhsrc_output_width;
    uint32_t vhsrc_output_height;

    /*
        This function will check if the scaling from (src_width, src_frame_height) to (dst_width, dst_frame_height)
        is possible according to 3 criteria:
         1) The horizontal and vertical zoom factor should not exceed "MAX_SCALING_FACTOR"
         2) The scaling should be within the HW processing time to perform this scaling (TO DO )
         3) The STBus traffic should not exceed a threshold
    */

    vhsrc_input_width   = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
    vhsrc_input_height  = (isSrcInterlaced)? pDisplayInfo->m_selectedPicture.srcFrameRect.height/2: pDisplayInfo->m_selectedPicture.srcFrameRect.height;
    vhsrc_output_width  = pDisplayInfo->m_dstFrameRect.width;
    vhsrc_output_height = (m_outputInfo.isDisplayInterlaced) ? pDisplayInfo->m_dstFrameRect.height/2: pDisplayInfo->m_dstFrameRect.height;

    TRC( TRC_ID_MAIN_INFO, "Trying: IsSrcInterlaced = %d, vhsrc_input_width = %d, vhsrc_input_height = %d, vhsrc_output_width = %d, vhsrc_output_height=%d",
                isSrcInterlaced,
                vhsrc_input_width,
                vhsrc_input_height,
                vhsrc_output_width,
                vhsrc_output_height);


    if(!IsZoomFactorOk(vhsrc_input_width,
                       vhsrc_input_height,
                       vhsrc_output_width,
                       vhsrc_output_height))
    {
        TRC(TRC_ID_MAIN_INFO, "ZoomFactor NOK!");
        return false;
    }


    if(IsSTBusDataRateOk(pNodeToDisplay, pDisplayInfo, isSrcInterlaced))
    {
        TRC(TRC_ID_MAIN_INFO, "Scaling possible with STBusBandWidth");
        return true;
    }
    TRC(TRC_ID_MAIN_INFO, "STBusDataRate NOK!");
    return false;
}

bool CVdpPlane::IsSTBusDataRateOk(CDisplayNode           *pNodeToDisplay,
                                  CDisplayInfo           *pDisplayInfo,
                                  bool                    isSrcInterlaced)
{
    uint64_t dataRateCurrentUseCaseInMBperS;
    uint64_t threshold;
    uint32_t fullOutputWidth;
    uint32_t fullOutputHeight;  // In Frame coordinates


    fullOutputWidth  = m_outputInfo.currentMode.mode_params.active_area_width;
    fullOutputHeight = m_outputInfo.currentMode.mode_params.active_area_height;


    // Compute the STBus Data Rate for the current use case
    if (!ComputeSTBusDataRate(pDisplayInfo,
                              isSrcInterlaced,
                              pDisplayInfo->m_selectedPicture.srcFrameRect.width,
                              pDisplayInfo->m_selectedPicture.srcFrameRect.height,
                              pDisplayInfo->m_dstFrameRect.width,
                              pDisplayInfo->m_dstFrameRect.height,
                              pDisplayInfo->m_selectedPicture.isSrcOn10bits,
                              m_outputInfo.currentMode.mode_params.vertical_refresh_rate,
                              &dataRateCurrentUseCaseInMBperS))
    {
        TRC(TRC_ID_ERROR, "Failed to compute dataRateCurrentUseCaseInMBperS!");
    }


    // data rate can be BW_RATIO % above the data rate for the full screen use case
    threshold = MAX_STBUS_BANDWIDTH;

    TRC(TRC_ID_MAIN_INFO, "DataRate Current Use Case = %llu MB/s", dataRateCurrentUseCaseInMBperS);
    TRC(TRC_ID_MAIN_INFO, "Threshold = %llu MB/s", threshold);

    if(dataRateCurrentUseCaseInMBperS <= threshold)
    {
        return true;
    }
    else
    {

        return false;
    }
}

bool CVdpPlane::GetNbrBytesPerPixel(CDisplayInfo           *pDisplayInfo,
                                    bool                    isSrcInterlaced,
                                    stm_rational_t         *pNbrBytesPerPixel)
{
    if(isSrcInterlaced)
    {

        if(pDisplayInfo->m_selectedPicture.isSrc422)
        {
            pNbrBytesPerPixel->numerator = 2;
            pNbrBytesPerPixel->denominator = 1;
        }
        else if(pDisplayInfo->m_selectedPicture.isSrc420)
        {
            pNbrBytesPerPixel->numerator = 3;
            pNbrBytesPerPixel->denominator = 2;
        }
        else
        {
            // 4:4:4 source
            TRC(TRC_ID_ERROR, "4:4:4 Interlaced source is not supported!");
            return false;
        }
    }
    else
    {
        /* Progressive source: DEI Bypassed */
        if(pDisplayInfo->m_selectedPicture.isSrc422)
        {
            pNbrBytesPerPixel->numerator = 2;
            pNbrBytesPerPixel->denominator = 1;
        }
        else if(pDisplayInfo->m_selectedPicture.isSrc420)
        {
            pNbrBytesPerPixel->numerator = 3;
            pNbrBytesPerPixel->denominator = 2;
        }
        else
        {
            // 4:4:4 source
            pNbrBytesPerPixel->numerator = 3;
            pNbrBytesPerPixel->denominator = 1;
        }
    }
    return true;
}

bool CVdpPlane::ComputeSTBusDataRate(CDisplayInfo           *pDisplayInfo,
                                     bool                    isSrcInterlaced,
                                     uint32_t                src_width,
                                     uint32_t                src_frame_height,
                                     uint32_t                dst_width,
                                     uint32_t                dst_frame_height,
                                     bool                    isSrcOn10bits,
                                     uint32_t                verticalRefreshRate,
                                     uint64_t               *pDataRateInMBperS)
{
    stm_rational_t nbrBytesPerPixel;
    uint64_t num, den;
    uint32_t OutputTotalHeight;
    uint32_t src_height;

    if(!GetNbrBytesPerPixel(pDisplayInfo, isSrcInterlaced, &nbrBytesPerPixel))
    {
        TRC(TRC_ID_ERROR, "Failed to get the nbr of Bytes per Pixel!");
        return false;
    }

    src_height = (isSrcInterlaced ? src_frame_height / 2 : src_frame_height);

    // If the output is Interlaced, no need to calculate the "field" size for OutputHeight and OutputTotalHeight
    // because the result will be the same when doing "OutputHeight / OutputTotalHeight"
    OutputTotalHeight = m_outputInfo.currentMode.mode_timing.lines_per_frame;

    /*
        STBus data rate estimation:

        STBus data rate = (Size of data read-written in memory) / (time available to perform the operation)

        Size of data read-written in memory = (src_width * src_height) * NbrBytesPerPixel

            * With DEI Spatial or Bypassed, for each pixel, only the Current Field is read by the HW:

                                        4:4:4 input         4:2:2 input         4:2:0 input
                Curr Field              3 Bytes             2 Bytes             1.5 Bytes (1 Byte of Luma and 0.5 Byte of Chroma)
                TOTAL NbrBytesPerPixel  3 Bytes             2 Bytes             1.5 Bytes

        time available to perform the operation = OutputHeight / (VSyncFreq * OutputTotalHeight)

        So we get:
        STBus data rate = NbrBytesPerPixel * (src_width * src_height) / (OutputHeight / (VSyncFreq * OutputTotalHeight))
                        = NbrBytesPerPixel * (src_width * src_height) * (VSyncFreq * OutputTotalHeight) / OutputHeight

        In the info about the current VTG mode, we have the "vertical_refresh_rate".
        "vertical_refresh_rate" is in mHz so it should be divided by 1000 to get the VSyncFreq.

        STBus data rate = NbrBytesPerPixel * (src_width * src_height) * (vertical_refresh_rate * OutputTotalHeight) / (1000 * OutputHeight)

        This will give a data rate in Bytes/s.
        Divide it by 10^6 to get a result in MB/s

        As a conclusion, the formula to use is:
        STBus data rate = NbrBytesPerPixel * (src_width * src_height) * (vertical_refresh_rate * OutputTotalHeight) / (1000 * 1000000 * OutputHeight)
    */

    num = ((uint64_t) nbrBytesPerPixel.numerator) * ((uint64_t) src_width) * ((uint64_t) src_height) * ((uint64_t) verticalRefreshRate) * ((uint64_t) OutputTotalHeight);
    den = ((uint64_t) nbrBytesPerPixel.denominator) * 1000 * 1000000 * ((uint64_t) dst_frame_height);

    *pDataRateInMBperS = vibe_os_div64(num, den);

    /*10 bits sources are not supported by VDP */
    if(isSrcOn10bits)
    {
        return false;
    }

    return true;
}

