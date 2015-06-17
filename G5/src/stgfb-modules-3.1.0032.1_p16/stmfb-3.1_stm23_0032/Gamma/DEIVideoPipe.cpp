/***********************************************************************
 *
 * File: Gamma/DEIVideoPipe.cpp
 * Copyright (c) 2006-2009 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Generic/Output.h>

#include <Gamma/VDPFilter.h>
#include <Gamma/DEIReg.h>
#include <Gamma/DEIVideoPipe.h>

/*
 * A set of useful named constants to make the filter setup code more readable
 */
static const int fixedpointONE        = 1<<13;
static const int fixedpointHALF       = fixedpointONE/2;
static const int fixedpointQUARTER    = fixedpointONE/4;


static const SURF_FMT g_surfaceFormats[] = {
    SURF_YCBCR422R,
    SURF_YUYV,
    SURF_YCBCR420MB,
    SURF_YUV420,
    SURF_YVU420,
    SURF_YUV422P
};


/* DEI_CTL register */
#define COMMON_CTRLS    (DEI_CTL_BYPASS                                \
                         | DEI_CTL_DD_MODE_DIAG9                       \
                         | DEI_CTL_CLAMP_MODE_DIRECTIONAL_NOT_VERTICAL \
                         | DEI_CTL_DIR_RECURSIVE_ENABLE                \
                         | ( 4 << DEI_CTL_KCORRECTION_SHIFT)           \
                         | (10 << DEI_CTL_T_DETAIL_SHIFT)              \
                         | C1_DEI_CTL_MD_MODE_OFF                      \
                        )



/* these are for STb7109, subclasses need to provide their own */
static FMDSW_FmdInitParams_t g_fmd_init_params = {
  ulH_block_sz  : 80, /*!< Horizontal block size for BBD */
  ulV_block_sz  : 40, /*!< Vertical block size for BBD */

  count_miss    :  2, /*!< Delay for a film mode detection */
  count_still   : 30, /*!< Delay for a still mode detection */
  t_noise       : 10, /*!< Noise threshold */
  k_cfd1        : 21, /*!< Consecutive field difference factor 1 */
  k_cfd2        : 16, /*!< Consecutive field difference factor 2 */
  k_cfd3        :  6, /*!< Consecutive field difference factor 3 */
  t_mov         : 10, /*!< Moving pixel threshold */
  t_num_mov_pix : 35, /*!< Moving block threshold */
  t_repeat      : 70, /*!< Threshold on BBD for a field repetition */
  t_scene       : 15, /*!< Threshold on BBD for a scene change  */
  k_scene       : 25, /*!< Percentage of blocks with BBD > t_scene */
  d_scene       :  1, /*!< Scene change detection delay (1,2,3 or 4) */
};


/* some compatibility so we can build the same file with and without the
   software FMD library without having to sprinkle #ifdef's all around
   the place */
#ifdef USE_FMD

#  define FMD_DEBUGF(x)     DEBUGF(x)
#  define FMD_DEBUGF2(x,y)  DEBUGF2(x,y)
#  define FMD_USED

#else /* !USE_FMD */

#  define FMD_DEBUGF(x)     ( { } )
#  define FMD_DEBUGF2(x,y)  ( { } )
#  define FMD_USED          __attribute__((unused))

#endif /* USE_FMD */

#ifdef FMD_STATUS_DEBUG
#  define DEBUG_ASSIGN(backup, src)    do{ (backup) = (src); }while(0)
#else
#  define DEBUG_ASSIGN(backup, src)    do{ }while(0)
#endif




CDEIVideoPipe::CDEIVideoPipe(
  stm_plane_id_t   planeID,
  CGammaVideoPlug *pVidPlug,
  ULONG            deiBaseAddr): CGammaCompositorVideoPlane (planeID,
                                                             pVidPlug,
                                                             deiBaseAddr)
{
  DEBUGF2 (2, (FENTRY ", self: %p, planeID: %d\n",
               __PRETTY_FUNCTION__, this, planeID));

  m_pSurfaceFormats    = g_surfaceFormats;
  m_nFormats           = N_ELEMENTS (g_surfaceFormats);

  m_eHideMode = SPCHM_MIXER_PULLSDATA;

  /*
   * Can be overriden by subclass to just use the HW as a video pipeline,
   * i.e. where the de-interlacer part of the IP has been removed to save
   * space.
   */
  m_bHaveDeinterlacer = true;

  /*
   * The SoC specific subclass must specify the maximum image size that
   * can be de-interlaced.
   */
  m_nDEIPixelBufferLength = 0;
  m_nDEIPixelBufferHeight = 0;

  /*
   * The following changes the behaviour of the generic buffer queue update
   * in GammaCompositorPlane to hold on to buffers for an extra update
   * period, so that previously displayed buffers can still be used by the
   * de-interlacer.
   */
  m_keepHistoricBufferForDeinterlacing = true;

  m_ulMotionDataFlag   = SDAAF_NONE;
  m_nCurrentMotionData = 0;
  m_MotionState        = DEI_MOTION_INIT;
  m_bUseMotion         = false;
  m_bUseFMD            = false;

  m_nMBChunkSize       = 0x3;
  m_nRasterChunkSize   = 0xf;
  m_nPlanarChunkSize   = 0xf;
  m_nRasterOpcodeSize  = 8;
  m_nMotionOpcodeSize  = 8;

  m_bReportPixelFIFOUnderflow = true;

#ifdef FMD_STATUS_DEBUG
    m_FMD_Film = 0;
    m_FMD_Still = 0;
    g_pIOS->ZeroMemory (&m_FMD_HWthresh,  sizeof (m_FMD_HWthresh));
    g_pIOS->ZeroMemory (&m_FMD_HWwindow,  sizeof (m_FMD_HWwindow));
    g_pIOS->ZeroMemory (&m_FMD_FieldInfo, sizeof (m_FMD_FieldInfo));
#endif


  m_nDEICtlLumaInterpShift   = C1_DEI_CTL_LUMA_INTERP_SHIFT;
  m_nDEICtlChromaInterpShift = C1_DEI_CTL_CHROMA_INTERP_SHIFT;
  m_nDEICtlSetupMotionSD     = COMMON_CTRLS;
  m_nDEICtlSetupMotionHD     = DEI_CTL_BYPASS;
  m_nDEICtlSetupMedian       = COMMON_CTRLS;
  m_nDEICtlMDModeShift       = C1_DEI_CTL_MD_MODE_SHIFT;
  m_nDEICtlMainModeDEI       = C1_DEI_CTL_MODE_DEINTERLACING;

  m_bHaveFMDBlockOffset       = false;
  m_FMDEnable                 = C1_DEI_CTL_FMD_ENABLE;
  m_fmd_init_params           = &g_fmd_init_params;
  m_MaxFMDBlocksPerLine       = 24;
  m_MaxFMDBlocksPerField      = 13;
  m_FMDStatusSceneCountMask   = FMD_STATUS_SCENE_COUNT_MASK;
  m_FMDStatusMoveStatusMask   = FMD_STATUS_MOVE_STATUS_MASK;
  m_FMDStatusRepeatStatusMask = FMD_STATUS_REPEAT_STATUS_MASK;

  // Control registers, everything disabled
  WriteVideoReg (DEI_CTL, DEI_CTL_NOT_ACTIVE);
  WriteVideoReg (VHSRC_CTL,0x00000000);

  for (ULONG i = 0; i < N_ELEMENTS (m_MotionData); ++i)
    g_pIOS->ZeroMemory (&m_MotionData[i], sizeof (DMA_Area));

  DEBUGF2 (4, (FEXIT ", self: %p\n", __PRETTY_FUNCTION__, this));
}


CDEIVideoPipe::~CDEIVideoPipe(void)
{
  DEBUGF2 (2, (FENTRY ", self: %p\n", __PRETTY_FUNCTION__, this));

  if (m_bUseFMD)
    {
      FMDSW_Error_t result = FMDSW_Terminate (kFmdDeviceMain);
      if (result != ST_NO_ERROR)
        FMD_DEBUGF (("FMDSW_Terminate(.) returned !OK\n"));
    }

  for (ULONG i = 0; i < N_ELEMENTS (m_MotionData); ++i)
    g_pIOS->FreeDMAArea (&m_MotionData[i]);

  DEBUGF2 (4, (FEXIT ", self: %p\n", __PRETTY_FUNCTION__, this));
}


bool
CDEIVideoPipe::Create (void)
{
  DEBUGF2 (2, (FENTRY ", self: %p\n", __PRETTY_FUNCTION__, this));

  if (!CGammaCompositorVideoPlane::Create ())
    return false;

  if (!m_VDPFilter.Create ())
    return false;

  m_bUseDeinterlacer = m_bHaveDeinterlacer;

  if (m_bHaveDeinterlacer)
    {
      m_bUseMotion = true;
      for (ULONG i = 0; i < N_ELEMENTS (m_MotionData) && m_bUseMotion; ++i)
        {
          g_pIOS->AllocateDMAArea (&m_MotionData[i],
                                   m_nDEIPixelBufferLength * m_nDEIPixelBufferHeight / 2,
                                   16,
                                   m_ulMotionDataFlag);
          if (!m_MotionData[i].pMemory)
            {
              DEBUGF2 (0, ("%s could not allocate DMA memory for %lu!\n",
                           __PRETTY_FUNCTION__, i));
              m_bUseMotion = false;
            }
        }

      if (!m_bUseMotion)
        DEBUGF2 (0, ("%s motion based deinterlacing disabled!\n",
                     __PRETTY_FUNCTION__));

      FMDSW_Error_t          result;
      FMDSW_FmdHwThreshold_t fmd_hw_thresh;

      result = FMDSW_Init (kFmdDeviceMain,
                           m_fmd_init_params,
                           static_cast<FMDSW_FmdDebugParams_t *> (0));

      if (result == ST_NO_ERROR)
        {
          result = FMDSW_CalculateHwThresh (kFmdDeviceMain,
                                            &fmd_hw_thresh);
          if (result == ST_NO_ERROR)
            {
              m_bUseFMD = true;

              WriteVideoReg (FMD_THRESHOLD_SCD, fmd_hw_thresh.ulT_scene);
              WriteVideoReg (FMD_THRESHOLD_RFD, fmd_hw_thresh.ulT_repeat);
              WriteVideoReg (FMD_THRESHOLD_MOVE,
                             fmd_hw_thresh.ulT_num_mov_pix << 16
                             | fmd_hw_thresh.ulT_mov);
              WriteVideoReg (FMD_THRESHOLD_CFD, fmd_hw_thresh.ulT_noise);

              FMD_DEBUGF2 (1, ("FMD thresholds: %x/%x/%x/%x/%x (%d/%d/%d/%d/%d)\n",
                               fmd_hw_thresh.ulT_scene, fmd_hw_thresh.ulT_repeat,
                               fmd_hw_thresh.ulT_mov, fmd_hw_thresh.ulT_num_mov_pix,
                               fmd_hw_thresh.ulT_noise,
                               fmd_hw_thresh.ulT_scene, fmd_hw_thresh.ulT_repeat,
                               fmd_hw_thresh.ulT_mov, fmd_hw_thresh.ulT_num_mov_pix,
                               fmd_hw_thresh.ulT_noise));

              DEBUG_ASSIGN (m_FMD_HWthresh, fmd_hw_thresh);
            }
          else
            FMD_DEBUGF (("FMDSW_CalculateHwThresh(.) returned !OK\n"));
        }
      else
        FMD_DEBUGF (("FMDSW_Init(.) returned !OK\n"));
    }

  m_VDPFilter.DumpFilters ();

  /*
   * Program the hardware to point to the filter coefficient tables. These
   * pointers don't change, but the contents of the memory pointed to does.
   */
  WriteVideoReg (VHSRC_HFP, m_HFilter.ulPhysical);
  WriteVideoReg (VHSRC_VFP, m_VFilter.ulPhysical);

  /*
   * Fill in the plane capabilities, note we have to do this here rather than
   * the constructor to reflect the deinterlacer control availability.
   */
  m_capabilities.ulCaps = (0
                           | PLANE_CAPS_RESIZE
                           | PLANE_CAPS_SUBPIXEL_POSITION
                           | PLANE_CAPS_SRC_COLOR_KEY
                           | PLANE_CAPS_GLOBAL_ALPHA
                           | PLANE_CAPS_DEINTERLACE
                          );
  m_capabilities.ulControls  = PLANE_CTRL_CAPS_PSI_CONTROLS;
  if (m_bHaveDeinterlacer)
    m_capabilities.ulControls |= PLANE_CTRL_CAPS_DEINTERLACE;
  if (m_bUseFMD)
    m_capabilities.ulControls |= PLANE_CTRL_CAPS_FMD;
  m_capabilities.ulMinFields =    1;
  m_capabilities.ulMinWidth  =   32;
  m_capabilities.ulMinHeight =   24;
  m_capabilities.ulMaxWidth  = 1920;
  m_capabilities.ulMaxHeight = 1088;
  SetScalingCapabilities(&m_capabilities);

  DEBUGF2 (4, (FEXIT ", self: %p\n", __PRETTY_FUNCTION__, this));

  return true;
}


bool
CDEIVideoPipe::SetControl (stm_plane_ctrl_t control,
                           ULONG            value)
{
  bool retval;

  DEBUGF2 (8, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_DEI_FMD_ENABLE:
      m_bUseFMD = value ? true : false;
      retval = true;
      break;

    case PLANE_CTRL_DEI_MODE:
      retval = true;
      switch (static_cast<enum PlaneCtrlDEIConfiguration>(value))
        {
        case PCDEIC_DISABLED:
          m_bUseDeinterlacer = false;
          break;

        case PCDEIC_3DMOTION:
        case PCDEIC_MEDIAN:
          /* if we have got a deinterlacer... */
          if (m_bHaveDeinterlacer)
          {
              /* enable its use... */
              m_bUseDeinterlacer = true;
              /* and check which mode to use: */
              if (value == PCDEIC_MEDIAN)
                  /* median is easy, as we can support it always, */
                  m_bUseMotion = false;
              /* but availability of motion based DEI is tricky to check
                 for, so we do this only if it wasn't the active mode before
                 switching off the DEI. */
              else if (!m_bUseMotion)
              {
                  /* we are not using motion data at the moment, but are
                     told to do so now. We have to check if we have
                     got the memory to store motion data. */
                  for (ULONG i = 0; i < N_ELEMENTS (m_MotionData); ++i)
                      if (!m_MotionData[i].pMemory)
                      {
                          /* DEI requested but not enough memory, so we fail.
                             Activation of the DEI itself will still have
                             worked ok, it's just that it is working in
                             median mode, although 3D was requested. */
                          retval = false;
                          break;
                      }
                  m_bUseMotion = true;
              }
          }
          else
              /* DEI requested but hardware can not do this, so we fail. */
              retval = false;
          break;

        default:
          retval = false;
          break;
        }
      break;

    case PLANE_CTRL_DEI_CTRLREG:
      /* FIXME: remove, as it's only a hack for easier debugging. */
      m_nDEICtlSetupMotionSD = value;
      retval = true;
      break;

    case PLANE_CTRL_DEI_FMD_THRESHS:
      {
      FMDSW_Error_t           result;
      FMDSW_FmdHwThreshold_t  fmd_hw_thresh;
      bool                    orig_bUseFMD = m_bUseFMD;
      FMDSW_FmdInitParams_t  *p = reinterpret_cast<FMDSW_FmdInitParams_t *>(value);

      if (!p)
        {
          retval = false;
          break;
        }

      /* disable before doing anything else */
      m_bUseFMD = false;

      result = FMDSW_Reset (kFmdDeviceMain);
      if (result != ST_NO_ERROR)
        FMD_DEBUGF (("FMDSW_Reset (kFmdDeviceMain) returned an error: %d\n",
                     result));
      result = FMDSW_Terminate (kFmdDeviceMain);
      if (result != ST_NO_ERROR)
        FMD_DEBUGF (("FMDSW_Terminate(.) returned !OK\n"));

      p->ulH_block_sz = m_fmd_init_params->ulH_block_sz;
      p->ulV_block_sz = m_fmd_init_params->ulV_block_sz;

      *m_fmd_init_params = *p;

      result = FMDSW_Init (kFmdDeviceMain,
                           p,
                           static_cast<FMDSW_FmdDebugParams_t *> (0));
      result = FMDSW_CalculateHwThresh (kFmdDeviceMain,
                                        &fmd_hw_thresh);
      if (result == ST_NO_ERROR)
        {
          WriteVideoReg (FMD_THRESHOLD_SCD, fmd_hw_thresh.ulT_scene);
          WriteVideoReg (FMD_THRESHOLD_RFD, fmd_hw_thresh.ulT_repeat);
          WriteVideoReg (FMD_THRESHOLD_MOVE,
                         fmd_hw_thresh.ulT_num_mov_pix << 16
                         | fmd_hw_thresh.ulT_mov);
          WriteVideoReg (FMD_THRESHOLD_CFD, fmd_hw_thresh.ulT_noise);

          FMD_DEBUGF2 (1, ("FMD thresholds: %x/%x/%x/%x/%x (%d/%d/%d/%d/%d)\n",
                           fmd_hw_thresh.ulT_scene, fmd_hw_thresh.ulT_repeat,
                           fmd_hw_thresh.ulT_mov, fmd_hw_thresh.ulT_num_mov_pix,
                           fmd_hw_thresh.ulT_noise,
                           fmd_hw_thresh.ulT_scene, fmd_hw_thresh.ulT_repeat,
                           fmd_hw_thresh.ulT_mov, fmd_hw_thresh.ulT_num_mov_pix,
                           fmd_hw_thresh.ulT_noise));

          DEBUG_ASSIGN (m_FMD_HWthresh, fmd_hw_thresh);

          m_bUseFMD = orig_bUseFMD;
        }
      else
        FMD_DEBUGF (("FMDSW_CalculateHwThresh(.) returned !OK\n"));

      retval = (result == ST_NO_ERROR);
      }
      break;

    case PLANE_CTRL_HIDE_MODE:
      switch (static_cast<enum StmPlaneCtrlHideMode>(value))
        {
        case SPCHM_MIXER_PULLSDATA:
        case SPCHM_MIXER_DISABLE:
          m_eHideMode = static_cast<enum StmPlaneCtrlHideMode>(value);
          retval = true;
          break;

        default:
          retval = false;
          break;
        }
      break;

    default:
      retval = CGammaCompositorVideoPlane::SetControl (control, value);
      break;
    }

  return retval;
}


#ifndef USE_FMD
#  define m_bUseFMD         false
#endif

bool
CDEIVideoPipe::GetControl (stm_plane_ctrl_t  control,
                           ULONG            *value) const
{
  bool retval;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_DEI_FMD_ENABLE:
      *value = m_bUseFMD;
      retval = true;
      break;

    case PLANE_CTRL_DEI_MODE:
      if (!m_bUseDeinterlacer)
        *value = PCDEIC_DISABLED;
      else if (m_bUseMotion)
        *value = PCDEIC_3DMOTION;
      else
        *value = PCDEIC_MEDIAN;
      retval = true;
      break;

    case PLANE_CTRL_DEI_CTRLREG:
      *value = m_nDEICtlSetupMotionSD;
      retval = true;
      break;

    case PLANE_CTRL_DEI_FMD_THRESHS:
      {
      FMDSW_FmdInitParams_t *p = reinterpret_cast<FMDSW_FmdInitParams_t *>(value);
      if (p)
        {
          *p = *m_fmd_init_params;
          retval = true;
        }
      else
        retval = false;
      }
      break;

    case PLANE_CTRL_HIDE_MODE:
      *value = m_eHideMode;
      retval = true;
      break;

    default:
      retval = CGammaCompositorVideoPlane::GetControl (control, value);
      break;
    }

  return retval;
}


bool CDEIVideoPipe::getQueueBufferInfo(const stm_display_buffer_t * const pFrame,
                                     const void                 * const user,
                                     GAMMA_QUEUE_BUFFER_INFO    &qbinfo) const
{
    DEBUGF2 (8, (FENTRY "\n", __PRETTY_FUNCTION__));

    if(!CGammaCompositorPlane::GetQueueBufferInfo(pFrame, user, qbinfo))
      return false;

    if(qbinfo.isSourceInterlaced)
    {
      bool deinterlaceAllowedForRescale;
      int maxpixelsperline = pFrame->src.ulStride/(pFrame->src.ulPixelDepth>>3);

      if(qbinfo.isDisplayInterlaced && !m_bHasProgressive2InterlacedHW)
      {
        /*
         * If we are on an interlaced display and do not have a P2I then we have
         * to check the scaling (without line skipping) will stay in range,
         * because a 2x downscale is now required to generate the fields.
         *
         * Note: this will allow the de-interlacer to be enabled when the
         * source and destination are the same size, this gives better
         * results for trick modes but may introduce some blur in the normal
         * playback case. However the application can turn the
         * de-interlacer off explicitly, with the plane controls, if it wants
         * to.
         */
        int maxverticaldownscale = m_ulMaxVSrcInc/(m_fixedpointONE*2);

        deinterlaceAllowedForRescale = (qbinfo.dst.height >= (qbinfo.src.height/maxverticaldownscale));
      }
      else
      {
        /*
         * With a P2I, the default is to always allow de-interlacing regardless
         * of the scale factor. In the 1:1 case we do not have to use the
         * rescale and filtering to get back to interfaced fields, so we can now
         * use the de-interlacer to give higher quality slow-motion and pause.
         *
         * The penalty is higher memory bandwidth usage, but the de-interlacing
         * mode can be tuned or turned off altogether using the plane controls
         * if the application wishes to alter this behaviour.
         */
        deinterlaceAllowedForRescale = true;
      }

      if(m_bUseDeinterlacer                              &&
         deinterlaceAllowedForRescale                    &&
         (maxpixelsperline  <= m_nDEIPixelBufferLength)  &&
         (qbinfo.src.height <= m_nDEIPixelBufferHeight))
      {
          DEBUGF2(3,("%s: enable HW de-interlacing\n", __PRETTY_FUNCTION__));
          qbinfo.isHWDeinterlacing = true;
          /*
           * As we are going to the trouble of hardware de-interlacing, we also
           * use the interpolated fields for extending the duration of a frame
           * for AV sync or when we "starve", eliminating motion artifacts.
           * Because we are doing a proper de-interlace we don't get the
           * vertical "bobbing" on high contrast edges that you get with
           * simple vertical interpolation between fields - the reason this
           * is not done unless the application specifies it (for slow motion)
           * for the case where we cannot use the de-interlacer.
           */
          qbinfo.interpolateFields = true;

          if(qbinfo.isDisplayInterlaced && m_bHasProgressive2InterlacedHW)
          {
              DEBUGF2(3,("%s: using P2I\n", __PRETTY_FUNCTION__));
              qbinfo.isUsingProgressive2InterlacedHW = true;
          }
      }
    }

    /*
     * Change the source and destination rectangles ready for
     * scaling and interlaced<->progressive conversions. Also
     * calculates the sample rate converter increments for
     * the scaling hardware.
     */
    CGammaCompositorPlane::AdjustBufferInfoForScaling(pFrame, qbinfo);

    DEBUGF2 (8, (FEXIT "\n", __PRETTY_FUNCTION__));

    return true;
}


bool CDEIVideoPipe::PrepareNodeForQueueing(const stm_display_buffer_t * const pFrame,
                                         const void                 * const user,
                                         DEISetup                   &displayNode,
                                         GAMMA_QUEUE_BUFFER_INFO    &qbinfo)
{
    DEBUGF2 (9, ("dei queue src addr/size/flg: %.8lx/%lu/%lx  @  %llu\n",
                 pFrame->src.ulVideoBufferAddr, pFrame->src.ulVideoBufferSize,
                 pFrame->src.ulFlags, pFrame->info.presentationTime));

    if(!getQueueBufferInfo(pFrame, user, qbinfo))
        return false;

    /*
     * Basic control setup for directional de-interlacing, but with
     * the de-interlacer bypassed by default.
     */
    ULONG ctlval;
    if(m_bUseMotion)
    {
      ctlval = (qbinfo.src.width > 960) ? m_nDEICtlSetupMotionHD : m_nDEICtlSetupMotionSD;

      ctlval |= (DEI_CTL_INTERP_3D << m_nDEICtlLumaInterpShift)
             |  (DEI_CTL_INTERP_3D << m_nDEICtlChromaInterpShift);
    }
    else
    {
      ctlval = m_nDEICtlSetupMedian
             | (DEI_CTL_INTERP_MEDIAN << m_nDEICtlLumaInterpShift)
             | (DEI_CTL_INTERP_MEDIAN << m_nDEICtlChromaInterpShift);
    }

    if (m_bUseFMD)
      ctlval |= m_FMDEnable;

    displayNode.topField.CTL
      = displayNode.botField.CTL
      = displayNode.altTopField.CTL
      = displayNode.altBotField.CTL
      = ctlval;

    displayNode.topField.bFieldSeen
      = displayNode.botField.bFieldSeen
      = displayNode.altTopField.bFieldSeen
      = displayNode.altBotField.bFieldSeen
      = false;

    displayNode.SRC_CTL = ( VHSRC_CTL__OUTPUT_ENABLE
                          | VHSRC_CTL__COEF_LOAD_LINE_NUM(3)
                          | VHSRC_CTL__PIX_LOAD_LINE_NUM(4) );

    if(!m_videoPlug->CreatePlugSetup(displayNode.vidPlugSetup, pFrame, qbinfo))
        return false;

    displayNode.TARGET_SIZE = PackRegister(qbinfo.verticalFilterOutputSamples,
                                           qbinfo.horizontalFilterOutputSamples);

    if(qbinfo.isUsingProgressive2InterlacedHW)
        displayNode.P2I_PXF_CFG = (qbinfo.verticalFilterOutputSamples/2)<<4;
    else
        displayNode.P2I_PXF_CFG = P2I_CONF_BYPASS_EN;

    const ULONG pixelsperline = pFrame->src.ulStride
                                / (pFrame->src.ulPixelDepth >> 3);

    const ULONG wordsperline  = (((pixelsperline + (pixelsperline % m_nMotionOpcodeSize)) / m_nMotionOpcodeSize)
                                 * (m_nMotionOpcodeSize/8));

    const ULONG nlines        = (qbinfo.src.y / m_fixedpointONE)
                                + qbinfo.src.height;

    displayNode.MOTION_PIXEL_STACK = DEI_STACK (1, wordsperline);
    displayNode.MOTION_LINE_STACK  = DEI_STACK (wordsperline, nlines);

    switch (pFrame->src.ulColorFmt)
    {
        case SURF_YCBCR420MB:
            set420MBMemoryAddressing (displayNode, pFrame, qbinfo);
            break;

        case SURF_YCBCR422R:
        case SURF_YUYV:
            set422RInterleavedMemoryAddressing (displayNode, pFrame, qbinfo);
            break;

        case SURF_YUV420:
        case SURF_YVU420:
        case SURF_YUV422P:
            setPlanarMemoryAddressing (displayNode, pFrame, qbinfo);
            break;

        default:
            return false;
    }

    setupHorizontalScaling(displayNode, qbinfo);

    if(!qbinfo.isSourceInterlaced)
    {
        /*
         * Progressive content on either a progressive or interlaced display
         */
        setupProgressiveVerticalScaling(displayNode, qbinfo);
    }
    else if(qbinfo.isHWDeinterlacing)
    {
        setupHWDeInterlacedVerticalScaling(displayNode, qbinfo);
    }
    else if(!qbinfo.isDisplayInterlaced)
    {
        setupDeInterlacedVerticalScaling(displayNode, qbinfo);
    }
    else
    {
        setupInterlacedVerticalScaling(displayNode, qbinfo);
    }


    /*
     * Must be called after the scaling setup as the subpixel positions are
     * needed to determine the filter choice.
     */
    selectScalingFilters(displayNode, pFrame, qbinfo);

    return true;
}


bool CDEIVideoPipe::QueueNode(const stm_display_buffer_t    * const pFrame,
                            const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                            DEISetup                      &displayNode,
                            ULONG                          node_size)
{
    if(!qbinfo.isSourceInterlaced)
        return QueueProgressiveContent(&displayNode, node_size, qbinfo);

    if(qbinfo.firstFieldOnly || (qbinfo.isDisplayInterlaced && !qbinfo.interpolateFields))
        return QueueSimpleInterlacedContent(&displayNode, node_size, qbinfo);
    else
        return QueueInterpolatedInterlacedContent(&displayNode, node_size, qbinfo);
}


bool CDEIVideoPipe::QueueBuffer(const stm_display_buffer_t * const pFrame,
                              const void                 * const user)
{
    DEISetup displayNode = {0};
    GAMMA_QUEUE_BUFFER_INFO qbinfo = {0};

    DEBUGF2 (8, ("%s\n", __PRETTY_FUNCTION__));

    if (!PrepareNodeForQueueing(pFrame, user, displayNode, qbinfo))
        return false;

    return QueueNode (pFrame, qbinfo, displayNode, sizeof (displayNode));
}


void CDEIVideoPipe::DisableHW()
{
    if(m_isEnabled)
    {
        // Disable video display processor
        WriteVideoReg(DEI_CTL, DEI_CTL_NOT_ACTIVE);
        WriteVideoReg(VHSRC_CTL, 0);

        if (m_bUseFMD
            && FMDSW_Reset (kFmdDeviceMain) != ST_NO_ERROR)
            FMD_DEBUGF (("FMDSW_Reset(.)3 returned !OK\n"));

        CGammaCompositorVideoPlane::DisableHW();
    }
}


void CDEIVideoPipe::UpdateHW(bool isDisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const TIME64 &vsyncTime)
{
    DEBUGF2 (9, (FENTRY "\n", __PRETTY_FUNCTION__));

    /*
     * This is only called as part of the display interrupt processing
     * bottom half and is not re-entrant. Given limitations on stack usage in
     * interrupt context on certain systems, we keep as much off the stack
     * as possible.
     */
    static stm_plane_node displayNode;
    static DEISetup      *nextfieldowner;
    static DEIFieldSetup *nextfieldsetup;

    /*
     * PART0
     *
     * Get and update the CRC values.
     */
    if (m_currentNode.isValid)
    {
        ULONG ycrc  = ReadVideoReg (Y_CRC_CHECK_SUM);
        ULONG uvcrc = ReadVideoReg (UV_CRC_CHECK_SUM);
        m_currentNode.info.stats.ulStatus |= STM_PLANE_STATUS_BUF_CRC_VALID;

        if (isDisplayInterlaced && !isTopFieldOnDisplay)
        {
            m_currentNode.info.stats.ulYCRC[1]  = ycrc;
            m_currentNode.info.stats.ulUVCRC[1] = uvcrc;
        }
        else
        {
            m_currentNode.info.stats.ulYCRC[0]  = ycrc;
            m_currentNode.info.stats.ulUVCRC[0] = uvcrc;
        }

        if(ReadVideoReg(P2I_PXF_IT_STATUS) & P2I_STATUS_FIFO_EMPTY)
        {
          m_currentNode.info.stats.ulStatus |= STM_PLANE_STATUS_BUF_HW_ERROR;
          if(m_bReportPixelFIFOUnderflow)
          {
            /*
             * Reporting this once is sufficient as the likely cause of the
             * condition is persistent, so once we are in this state we never
             * get out of it.
             */
            g_pIDebug->IDebugPrintf("CDEIVideoPipe::UpdateHW(): Pixel FIFO underflow");
            m_bReportPixelFIFOUnderflow = false;
          }
        }

        WriteVideoReg(P2I_PXF_IT_STATUS, (P2I_STATUS_END_PROCESSING|P2I_STATUS_FIFO_EMPTY));
    }

    /*
     * PART1
     *
     * Update the state with what is currently on the display, i.e. is
     * the current node still there because it was to be displayed for
     * several fields, or has it finished and is a new node being displayed
     */
    UpdateCurrentNode(vsyncTime);


    /*
     * PART2.
     *
     * After the update we need to check the current node status again.
     *
     * Firstly, if we are on an interlaced display and we still have a valid
     * node, AND we are NOT paused, we need to update the field specific
     * hardware setup for the next field. This copes with the trick mode case
     * and the case where there is nothing left to display but the current node
     * is persistent. It doesn't matter if we do this and then PART3 programs
     * a completely new node instead.
     */
    nextfieldsetup = 0;

    if(m_currentNode.isValid && isDisplayInterlaced)
    {
        nextfieldsetup = SelectFieldSetup(isTopFieldOnDisplay, m_currentNode);
        nextfieldowner = (DEISetup *)m_currentNode.dma_area.pData;
    }

    if(m_isPaused || (m_currentNode.isValid && m_currentNode.info.nfields > 1))
    {
        /*
         * We have got a node on display that is going to be there for more than
         * one frame/field, so we do not get another from the queue yet.
         */
        goto write_next_field;
    }

    /*
     * PART3.
     *
     * For whatever reason we now want to try and queue a new node to be
     * displayed on the NEXT field/frame interrupt.
     */
    if(PeekNextNodeFromDisplayList(vsyncTime, displayNode))
    {
        DEBUGF2(9,("%s: next @ %llu\n", __PRETTY_FUNCTION__, displayNode.info.presentationTime));
        if(isDisplayInterlaced)
        {
            /*
             * If we are being asked to present graphics on an interlaced display
             * then only allow changes on the top field. This is to prevent visual
             * artifacts when animating vertical movement.
             */
            if(isTopFieldOnDisplay && (displayNode.info.ulFlags & STM_PLANE_PRESENTATION_GRAPHICS))
                goto write_next_field;

            /*
             * On an interlaced display, we need to deal with the situation
             * where the node we have dequeued is marked as belonging to one
             * field but we are updating for a different field. There are two
             * situations where this can happen, firstly some error has caused
             * us to miss processing a field interrupt so we are out of
             * sequence. Secondly we are in a startup condition, where the
             * stream is starting on the opposite field to the current hardware
             * state. Progressive nodes can be displayed on any field.
             *
             * In both cases we keep the existing node (if any) on the display
             * for another field.
             */
            if(!displayNode.useAltFields)
            {
                if((isTopFieldOnDisplay  && displayNode.nodeType == GNODE_TOP_FIELD) ||
                   (!isTopFieldOnDisplay && displayNode.nodeType == GNODE_BOTTOM_FIELD))
                {
                    DEBUGF2(3,("%s Waiting for correct field\n", __PRETTY_FUNCTION__));
                    DEBUGF2(3,("%s isTopFieldOnDisplay = %s\n", __PRETTY_FUNCTION__, isTopFieldOnDisplay?"true":"false"));
                    DEBUGF2(3,("%s nodeType            = %s\n", __PRETTY_FUNCTION__, (displayNode.nodeType==GNODE_TOP_FIELD)?"top":"bottom"));

                    goto write_next_field;
                }
            }

            nextfieldsetup = SelectFieldSetup(isTopFieldOnDisplay,displayNode);
        }
        else
        {
            if(displayNode.nodeType == GNODE_PROGRESSIVE || displayNode.nodeType == GNODE_TOP_FIELD)
                nextfieldsetup = &((DEISetup*)displayNode.dma_area.pData)->topField;
            else
                nextfieldsetup = &((DEISetup*)displayNode.dma_area.pData)->botField;
        }

        nextfieldowner = (DEISetup*)displayNode.dma_area.pData;
        WriteCommonSetup(nextfieldowner);

        if (!m_isEnabled)
        {
          EnableHW();
          /*
           * Check if something went wrong during hardware enable, if so clear
           * the display pipe hardware control register again and keep this
           * buffer on the queue to try again later.
           */
          if(!m_isEnabled)
          {
            WriteVideoReg(DEI_CTL, DEI_CTL_NOT_ACTIVE);
            WriteVideoReg(VHSRC_CTL, 0);
            return;
          }
          m_videoPlug->Show();
        }

        SetPendingNode(displayNode);
        PopNextNodeFromDisplayList();
    }
    else
      DEBUGF2(9,("dei no next\n"));

    /*
     * Finally write the next field setup if required. This ensures we only do
     * it once in the case where a persistent frame is on display but we now
     * have a new node.
     */
write_next_field:
    if(nextfieldsetup)
    {
        UpdateDEIStateMachine(nextfieldsetup,nextfieldowner);
        if(m_bHasProgressive2InterlacedHW)
        {
            if(isTopFieldOnDisplay)
                WriteVideoReg(P2I_PXF_CONF, nextfieldowner->P2I_PXF_CFG);
            else
                WriteVideoReg(P2I_PXF_CONF, nextfieldowner->P2I_PXF_CFG | P2I_CONF_TOP_NOT_BOT);
        }
        WriteFieldSetup(nextfieldsetup);
    }

    DEBUGF2 (9, (FEXIT "\n", __PRETTY_FUNCTION__));
}


bool CDEIVideoPipe::Hide(void)
{
  switch (m_eHideMode)
    {
    case SPCHM_MIXER_PULLSDATA:
      if(m_pOutput && m_isEnabled)
      {
        m_videoPlug->Hide();
        g_pIOS->LockResource(m_lock);
        m_ulStatus &= ~STM_PLANE_STATUS_VISIBLE;
        g_pIOS->UnlockResource(m_lock);
        return true;
      }
      break;

    case SPCHM_MIXER_DISABLE:
      return CGammaCompositorVideoPlane::Hide();
    }

  return false;
}


bool CDEIVideoPipe::Show(void)
{
  /* we always have to tell the mixer to pull data again: in case somebody
     had it (completely) disabled first (while in SPCHM_MIXER_DISABLE mode),
     then switched into SPCHM_MIXER_PULLSDATA, and now is calling Show().
     The same applies for the plug: If somebody called Hide() while in
     SPCHM_MIXER_PULLSDATA mode and now calls show after having switched into
     SPCHM_MIXER_DISABLE mode, we still want to make sure the imagedata is
     not ignored by the mixer! */
  if (CGammaCompositorVideoPlane::Show())
  {
    m_videoPlug->Show();
    m_MotionState = DEI_MOTION_OFF;
    (void) FMDSW_Reset (kFmdDeviceMain);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////
// Video Display Processor private implementation
DEIFieldSetup *
CDEIVideoPipe::SelectFieldSetup (bool           isTopFieldOnDisplay,
                               stm_plane_node &displayNode) const
{
  DEISetup * const setup = (DEISetup *) displayNode.dma_area.pData;

  DEBUGF2 (4, ("%s Updating for %s field\n",
               __PRETTY_FUNCTION__, isTopFieldOnDisplay ? "bottom" : "top"));

  if (!isTopFieldOnDisplay)
    {
      /* bottom field on display */
      if (m_isPaused || displayNode.useAltFields)
        return (displayNode.nodeType == GNODE_TOP_FIELD)
               ? &setup->topField
               : &setup->altTopField;

      return &setup->topField;

    }

  /* top field on display */
  if (m_isPaused || displayNode.useAltFields)
    return (displayNode.nodeType == GNODE_BOTTOM_FIELD)
           ? &setup->botField
           : &setup->altBotField;

  return &setup->botField;
}


static inline bool
hasStreamChanged (const DEISetup * const node1,
               const DEISetup * const node2)
{
    if (node1->T3I_CTL != node2->T3I_CTL
        || node1->LUMA_LINE_STACK[0] != node2->LUMA_LINE_STACK[0]
        || node1->LUMA_PIXEL_STACK[0] != node2->LUMA_PIXEL_STACK[0])
    {
        DEBUGF2 (4, ("stream changed from %.8lx/%.8lx/%.8lx -> %.8lx/%.8lx/%.8lx\n",
                     node1->T3I_CTL, node1->LUMA_LINE_STACK[0], node1->LUMA_PIXEL_STACK[0],
                     node2->T3I_CTL, node2->LUMA_LINE_STACK[0], node2->LUMA_PIXEL_STACK[0]));

        return true;
    }

    return false;
}


static inline ULONG
__attribute__((const))
_MIN (ULONG x, ULONG y)
{
  return (x < y) ? x : y;
}


void
CDEIVideoPipe::NextDEIFieldSetup (DEISetup * const setup,
                                bool             isTopNode,
                                bool             streamChanged)
{
    DEIFieldSetup * const field = isTopNode ? &setup->topField
                                            : &setup->botField;
    DEIFieldSetup * const altfield = isTopNode ? &setup->altBotField
                                               : &setup->altTopField;

    DEISetup *nextnode = 0;
    if (m_pNodeList[m_readerPos].isValid)
        nextnode = reinterpret_cast <DEISetup *> (m_pNodeList[m_readerPos].dma_area.pData);

    if (!field->NEXT_LUMA_BA)
    {
        bool streamChanging = true;
        if(nextnode)
            streamChanging = hasStreamChanged (setup, nextnode);

        if (streamChanging)
          {
            DEBUG_ASSIGN (m_FMD_Film, false);
            DEBUG_ASSIGN (m_FMD_Still, 0);
          }

        if (streamChanging
            || !m_previousNode.isValid)
        {
            if (streamChanging)
                DEBUGF2 (3,("%s: streamChanging on next node\n", __PRETTY_FUNCTION__));
            else
                DEBUGF2 (3,("%s: previous node is not valid!\n", __PRETTY_FUNCTION__));

            if (m_bUseFMD)
              {
                DEISetup * const tmp = nextnode ? nextnode : setup;
                FMD_DEBUGF (("DEI_VIEWPORT_SIZE: %.8lx\n", tmp->topField.DEIVP_SIZE));
                FMD_DEBUGF (("LUMA_XY: %.8lx\n", tmp->topField.LUMA_XY));

                /* calculate some FMD parameters */
                FMDSW_Error_t              result;
                FMDSW_FmdHwWindow_t        fmd_hw_window;
                const FMDSW_SourceParams_t fmd_source_params FMD_USED = {
                  ulOffsetX      : 0,
                  ulOffsetY      : 0,
                  ulWidth        : (field->DEIVP_SIZE >>  0) & 0x7ff,
                  ulHeight       : (field->DEIVP_SIZE >> 16) & 0x7ff,
                  eFmdSourceFreq : k50Hz /* not used */
                };

                result = FMDSW_Reset (kFmdDeviceMain);
                if (result != ST_NO_ERROR)
                  FMD_DEBUGF (("FMDSW_Reset (kFmdDeviceMain) returned an error: %d\n",
                               result));
                FMDSW_CalculateHwWindow (kFmdDeviceMain,
                                         &fmd_source_params,
                                         &fmd_hw_window);
                if (result != ST_NO_ERROR)
                  FMD_DEBUGF (("FMDSW_CalculateHwWindow (kFmdDeviceMain, ...) "
                               "returned an error: %d\n", result));
                result = FMDSW_SetSource (kFmdDeviceMain,
                                          &fmd_hw_window);
                if (result != ST_NO_ERROR)
                  FMD_DEBUGF (("FMDSW_SetSource (kFmdDeviceMain, ...) returned "
                               "an error: %d\n", result));

                FMD_DEBUGF (("new1 params: x/y BBDwin %u/%u block-x %u*%u block-y %u*%u "
                             "offs x/y %u/%u w/h %u/%u\n",
                             fmd_hw_window.ulFmdvp_start,
                             fmd_hw_window.ulFmdvl_start,
                             fmd_hw_window.ulH_nb,
                             fmd_hw_window.ulH_block_sz,
                             fmd_hw_window.ulV_nb,
                             fmd_hw_window.ulV_block_sz,
                             fmd_hw_window.ulOffsetX,
                             fmd_hw_window.ulOffsetY,
                             fmd_hw_window.ulWidth,
                             fmd_hw_window.ulHeight
                           ));

                DEBUG_ASSIGN (m_FMD_HWwindow, fmd_hw_window);

                if (m_bHaveFMDBlockOffset)
                  {
                    FMD_DEBUGF2 (9, ("original fmd_block_number::v/hoffset: %#.2lx/%#.2lx\n",
                                     tmp->BLOCK_NUMBER >> 24,
                                     (tmp->BLOCK_NUMBER >> 16) & 0x3f));

                    tmp->BLOCK_NUMBER |= (fmd_hw_window.ulFmdvl_start << 24);
                    tmp->BLOCK_NUMBER |= (fmd_hw_window.ulFmdvp_start << 16);

                    FMD_DEBUGF2 (9, ("changed fmd_block_number::v/hoffset: %#.2lx/%#.2lx\n",
                                     tmp->BLOCK_NUMBER >> 24,
                                     (tmp->BLOCK_NUMBER >> 16) & 0x3f));
                  }

                /* FIXME: writing this register should be optimized in that it
                   only needs to be done at a stream change, not for every
                   VSync! */
                WriteVideoReg (FMD_BLOCK_NUMBER, tmp->BLOCK_NUMBER);
              }
          }

        if (nextnode && !streamChanging)
        {
            if (isTopNode)
            {
                field->NEXT_LUMA_BA   = nextnode->botField.LUMA_BA;
                field->NEXT_CHROMA_BA = nextnode->botField.CHROMA_BA;
            }
            else
            {
                field->NEXT_LUMA_BA   = nextnode->topField.LUMA_BA;
                field->NEXT_CHROMA_BA = nextnode->topField.CHROMA_BA;
            }
        }
        else if (field->PREV_LUMA_BA)
        {
            field->NEXT_LUMA_BA   = field->PREV_LUMA_BA;
            field->NEXT_CHROMA_BA = field->PREV_CHROMA_BA;
        }
        else if (m_currentNode.isValid && !streamChanged)
        {
            const DEIFieldSetup * const tmp =
              isTopNode ? &((DEISetup *) m_currentNode.dma_area.pData)->botField
                        : &((DEISetup *) m_currentNode.dma_area.pData)->topField;

            field->NEXT_LUMA_BA   = tmp->LUMA_BA;
            field->NEXT_CHROMA_BA = tmp->CHROMA_BA;
        }
        else
        {
            if (isTopNode)
            {
                field->NEXT_LUMA_BA   = setup->botField.LUMA_BA;
                field->NEXT_CHROMA_BA = setup->botField.CHROMA_BA;
            }
            else
            {
                field->NEXT_LUMA_BA   = setup->topField.LUMA_BA;
                field->NEXT_CHROMA_BA = setup->topField.CHROMA_BA;
            }
        }
    }

    altfield->NEXT_LUMA_BA   = field->NEXT_LUMA_BA;
    altfield->NEXT_CHROMA_BA = field->NEXT_CHROMA_BA;
}


void
CDEIVideoPipe::PreviousDEIFieldSetup (DEISetup * const owner,
                                    bool      isTopNode,
                                    bool      streamChanged) const
{
    DEIFieldSetup * const field = isTopNode ? &owner->topField
                                            : &owner->botField;
    DEIFieldSetup * const altfield = isTopNode ? &owner->altBotField
                                               : &owner->altTopField;

    if (!field->PREV_LUMA_BA)
    {
        if (m_currentNode.isValid && !streamChanged)
        {
            const DEIFieldSetup * const tmp =
              isTopNode ? &((DEISetup *) m_currentNode.dma_area.pData)->botField
                        : &((DEISetup *) m_currentNode.dma_area.pData)->topField;

            field->PREV_LUMA_BA   = tmp->LUMA_BA;
            field->PREV_CHROMA_BA = tmp->CHROMA_BA;
        }
        else
        {
            /*
             * Just pick the next field as the previous, which is easy
             * as we just spent a load of logic working out the next field.
             */
            field->PREV_LUMA_BA   = field->NEXT_LUMA_BA;
            field->PREV_CHROMA_BA = field->NEXT_CHROMA_BA;
        }
    }

    altfield->PREV_LUMA_BA   = field->PREV_LUMA_BA;
    altfield->PREV_CHROMA_BA = field->PREV_CHROMA_BA;
}


void
CDEIVideoPipe::UpdateDEIStateMachine (const DEIFieldSetup * const fieldsetup,
                                    DEISetup            * const setup)
{
    ULONG dei_ctl = fieldsetup->CTL;

    /*
     * We only update a field's setup and the de-interlacing state
     * if the de-interlacer is in use and this is the first time we
     * have seen this field (or the alternate field setup using the
     * same image data).
     */
    if ((dei_ctl & DEI_CTL_BYPASS) == 0 && !fieldsetup->bFieldSeen)
    {
        bool isTopNode;
        switch (dei_ctl & DEI_CTL_CVF_TYPE_MASK)
        {
            case DEI_CTL_CVF_TYPE_420_ODD:
            case DEI_CTL_CVF_TYPE_422_ODD:
                isTopNode = true;
                break;

            default:
                isTopNode = false;
                break;
        }

        /* set some defaults for FMD */
        FMDSW_FmdFieldInfo_t fmd_field_info = {
          ucFieldIndex : 0,
          ucPattern    : 0,
          bPhase       : false,
          bFilmMode    : false,
          bStillMode   : false,
          bSceneChange : false
        };

        if (m_previousNode.isValid
            && m_bUseFMD)
          {
            /* we're going to process the FMD/DEI statistics and also do the
               software FMD stuff. m_previousNode is the Node which just
               became the one being displayed during this VSync (before
               CDEIVideoPipe::UpdateHW() updated it to be m_currentNode now) ,
               which we are still in. The statistics have been calculated
               ahead, i.e. exactly for this field. So we should set up the DEI
               hardware now. The problems is though, that the DEI registers
               are double buffered and as such the DEI will read the new
               values only during the _next_ vsync. This is a bit late, but
               there is no way around this :( */

            ULONG fmd_status = ReadVideoReg (FMD_STATUS); /* 0xac */
            const FMDSW_FmdHwStatus_t fmd_hw_stats FMD_USED = {
              ulCfd_sum       : ReadVideoReg (FMD_CFD_SUM)   & 0x0fffffff, /* 0xa4 */
              ulField_sum     : ReadVideoReg (FMD_FIELD_SUM) & 0x0fffffff, /* 0xa8 */
              ulPrev_bd_count : 0, /* not used */
              ulScene_count   : fmd_status & m_FMDStatusSceneCountMask,
              bMove_status    : (fmd_status & m_FMDStatusMoveStatusMask) ? true : false,
              bRepeat_status  : (fmd_status & m_FMDStatusRepeatStatusMask) ? true : false,
              ulH_nb          : 0, /* not used */
              ulV_nb          : 0  /* not used */
            };
            FMD_DEBUGF2 (8, ("params for proc: %.5x/%.5x/%.2x/Move: %s/Repeat: %s\n",
                             fmd_hw_stats.ulCfd_sum,
                             fmd_hw_stats.ulField_sum,
                             fmd_hw_stats.ulScene_count,
                             fmd_hw_stats.bMove_status ? "yes" : "no",
                             fmd_hw_stats.bRepeat_status ? "yes" : "no"));
            /* FIXME: how do we find out if a field was skipped? (Last
               parameter to FMDSW_Process()) */
            FMDSW_Process (kFmdDeviceMain,
                           &fmd_hw_stats,
                           &fmd_field_info,
                           false);
#ifdef FMD_STATUS_DEBUG
            if (m_FMD_FieldInfo.bFilmMode)
              {
                m_FMD_Film = true;
                if (m_FMD_pattern != fmd_field_info.ucPattern)
                  {
                    static const char *pat_tbl[] = {
                      "2:2", "3:2", "2:2:2:4", "2:3:3:2", "5:5", "6:4",
                      "3:2:3:2:2", "8:7", "4:4", "2^11:3", "2^23:4",
                      "2:2:2:2:3", "3:4:4:4", "3:3:3:3:3:3:3:4", "3:3:4",
                      "3:3"
                    };

                    g_pIDebug->IDebugPrintf ("Film mode -> pat: %.2x (%s)\n",
                                             fmd_field_info.ucPattern,
                                             ((fmd_field_info.ucPattern < N_ELEMENTS (pat_tbl))
                                              ? pat_tbl[fmd_field_info.ucPattern]
                                              : "???"));
                    m_FMD_pattern = fmd_field_info.ucPattern;
                  }
              }
            else if (m_FMD_Film)
              {
                m_FMD_Film = false;
                m_FMD_pattern = -1;
                g_pIDebug->IDebugPrintf ("left film mode\n");
              }

            if (m_FMD_Still != fmd_field_info.bStillMode)
              {
                m_FMD_Still = fmd_field_info.bStillMode;
                g_pIDebug->IDebugPrintf ("Still mode: %s\n",
                                         m_FMD_Still ? "yes" : "no");
              }
            if (fmd_field_info.bSceneChange)
              g_pIDebug->IDebugPrintf ("SceneChange!\n");
#endif
          }
        else if (m_bUseFMD)
          FMD_DEBUGF (("previous node is INVALID!!!!!!!!\n"));

        bool streamChange = true;
        if (m_currentNode.isValid)
            streamChange = hasStreamChanged ((DEISetup *) m_currentNode.dma_area.pData, setup);

        NextDEIFieldSetup (setup, isTopNode, streamChange);
        PreviousDEIFieldSetup (setup, isTopNode, streamChange);

        if (streamChange)
        {
            m_MotionState = DEI_MOTION_OFF;
            DEBUG_ASSIGN (m_FMD_Film, false);
            DEBUG_ASSIGN (m_FMD_Still, 0);
        }
        else if (fmd_field_info.bSceneChange)
        {
            /*
             * actually, we want DEI_MOTION_OFF here as well, but since we are
             * one field late we set it to init because there will be a valid
             * previous field.
             */
            m_MotionState = DEI_MOTION_INIT;
        }

        if (((dei_ctl >> m_nDEICtlLumaInterpShift) & DEI_CTL_INTERP_3D) == DEI_CTL_INTERP_3D)
        {
            dei_ctl |= m_MotionState << m_nDEICtlMDModeShift;

            if((m_MotionState == DEI_MOTION_OFF) ||
               (m_MotionState == DEI_MOTION_INIT))
            {
                /*
                 * While we are initializing the motion buffers we must not use
                 * 3D interpolation, so fall back to median.
                 */
                dei_ctl &= ~(DEI_CTL_INTERP_MASK << m_nDEICtlLumaInterpShift
                           | DEI_CTL_INTERP_MASK << m_nDEICtlChromaInterpShift);

                dei_ctl |= (DEI_CTL_INTERP_MEDIAN << m_nDEICtlLumaInterpShift
                          | DEI_CTL_INTERP_MEDIAN << m_nDEICtlChromaInterpShift);
            }

            switch (m_MotionState)
            {
                case DEI_MOTION_OFF:       m_MotionState = DEI_MOTION_INIT;      break;
                case DEI_MOTION_INIT:      m_MotionState = DEI_MOTION_LOW_CONF;  break;
                case DEI_MOTION_LOW_CONF:  m_MotionState = DEI_MOTION_FULL_CONF; break;
                case DEI_MOTION_FULL_CONF: /* nothing */                         break;
            }
        }
        else
        {
            /*
             * Make sure the motion state machine is reset to off when we switch
             * the de-interlacing mode to something other than 3D interpolation,
             * that way it is ready to start again if we switch it back on.
             */
            m_MotionState = DEI_MOTION_OFF;
        }

        /* Deinterlacing modes are determined by the input sequence type:
           - interlaced sequences (video) are deinterlaced with a spatial or
             3D algorithm
           - progressive sequences (for example movie broadcast in PAL/50
             Hz) are deinterlaced with the field merging algorithm */
        /* if we are in a movement phase in Film mode, we have to
           a) swap field addresses of previous and next and
           b) tell the DEI hardware that they are. This is because normally
           the DEI_NYF_BA register is not used when the deinterlacer is
           performing field merging (or only intra-field calculations). But
           the DEI_PYF_BA register is used when field merging. So, to not
           merge the wrong fields, we put the address of the _next_ field
           into the previous register. Seems awkward, but works.
           if in stillmode we just tell the hardware that fields are swapped
           but we don't actually swap them.*/
        bool bDoSwapFields = false;
        if (fmd_field_info.bFilmMode
            || fmd_field_info.bStillMode
           )
          {
            dei_ctl |= C1_DEI_CTL_MODE_FIELD_MERGING; /* this bit is the same
                                                         in all revisions of
                                                         the DEI (till now) */
            if (fmd_field_info.bPhase)
              {
                dei_ctl |= DEI_CTL_FMD_FIELD_SWAP;
                bDoSwapFields = true;
              }
            if (fmd_field_info.bStillMode)
              {
                if (dei_ctl & DEI_CTL_FMD_FIELD_SWAP)
                  FMD_DEBUGF (("swap was on, disabling due to still mode!\n"));
                bDoSwapFields = false;
              }
          }
        else
          dei_ctl |= m_nDEICtlMainModeDEI; // want MLD here

        DEBUG_ASSIGN (m_FMD_FieldInfo, fmd_field_info);

        /* Set the full DEI control state for both the real field and
           alternate that uses the same image data but with a different filter
           setup. This is needed when de-interlacing for resize on an
           interlaced display. */
        if (isTopNode)
        {
            setup->topField.bFieldSeen
              = setup->altBotField.bFieldSeen
              = true;

            setup->topField.CTL    = dei_ctl;
            setup->altBotField.CTL = dei_ctl;
            setup->topField.bSwapPreviousNextAddr
              = setup->altBotField.bSwapPreviousNextAddr = bDoSwapFields;
        }
        else
        {
            setup->botField.bFieldSeen
              = setup->altTopField.bFieldSeen
              = true;

            setup->botField.CTL    = dei_ctl;
            setup->altTopField.CTL = dei_ctl;
            setup->botField.bSwapPreviousNextAddr
              = setup->altTopField.bSwapPreviousNextAddr = bDoSwapFields;
        }

        DEBUGF2 (4, ("dei_ctl: %#.8lx CY: %#.8lx PY: %#.8lx NY: %#.8lx\n",
                     fieldsetup->CTL, fieldsetup->LUMA_BA,
                     fieldsetup->PREV_LUMA_BA, fieldsetup->NEXT_LUMA_BA));
        DEBUGF2 (4, ("dei_ctl: %#.8lx CC: %#.8lx PC: %#.8lx NC: %#.8lx\n",
                     fieldsetup->CTL, fieldsetup->CHROMA_BA,
                     fieldsetup->PREV_CHROMA_BA, fieldsetup->NEXT_CHROMA_BA));
        DEBUGF2 (4, ("dei_ctl: %#.8lx CM: %#.8lx PM: %#.8lx NM: %#.8lx\n",
                     fieldsetup->CTL, fieldsetup->CURRENT_MOTION,
                     fieldsetup->PREVIOUS_MOTION, fieldsetup->NEXT_MOTION));
    }
}


void
CDEIVideoPipe::WriteFieldSetup (const DEIFieldSetup * const field)
{
    DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * All of these registers are double buffered and do not take effect until
     * the next vsync. If our vsync handling has been badly delayed and a new
     * sync happens while we are in the middle of this update, we could crash
     * the display pipe with an inconsistent setup. So the first thing we do is
     * write the disable value to the control register so if that does happen
     * the DEI will get turned off for a frame. The display will glitch, FMD
     * cadence detection and motion buffers will be wrong but everything should
     * eventually recover.
     */
    WriteVideoReg (DEI_CTL,         DEI_CTL_NOT_ACTIVE);

    WriteVideoReg (DEI_VP_ORIGIN,   field->LUMA_XY);
    WriteVideoReg (DEI_VP_SIZE,     field->DEIVP_SIZE);

    /* current luma & chroma */
    WriteVideoReg (DEI_CYF_BA,      field->LUMA_BA);
    WriteVideoReg (DEI_CCF_BA,      field->CHROMA_BA);

    /* write previous and next luma & chroma depending on the movement phase
       */
    if (field->CTL & DEI_CTL_FMD_FIELD_SWAP
        && field->bSwapPreviousNextAddr)
      {
        WriteVideoReg (DEI_PYF_BA,      field->NEXT_LUMA_BA);
        WriteVideoReg (DEI_NYF_BA,      field->PREV_LUMA_BA);

        WriteVideoReg (DEI_PCF_BA,      field->NEXT_CHROMA_BA);
        WriteVideoReg (DEI_NCF_BA,      field->PREV_CHROMA_BA);
      }
    else
      {
        WriteVideoReg (DEI_PYF_BA,      field->PREV_LUMA_BA);
        WriteVideoReg (DEI_NYF_BA,      field->NEXT_LUMA_BA);

        WriteVideoReg (DEI_PCF_BA,      field->PREV_CHROMA_BA);
        WriteVideoReg (DEI_NCF_BA,      field->NEXT_CHROMA_BA);
      }

    WriteVideoReg (DEI_PMF_BA,      field->PREVIOUS_MOTION);
    WriteVideoReg (DEI_CMF_BA,      field->CURRENT_MOTION);
    WriteVideoReg (DEI_NMF_BA,      field->NEXT_MOTION);

    WriteVideoReg (VHSRC_LUMA_VSRC, field->LUMA_VSRC);
    WriteVideoReg (VHSRC_CHR_VSRC,  field->CHR_VSRC);

    /*
     * Make the real control register value the very last thing we write.
     */
    WriteVideoReg (DEI_CTL,         field->CTL);

    DEBUGF2 (4, ("DEI_CTL         0x%lx\n", field->CTL));
    DEBUGF2 (4, ("DEI_CYF_BA      0x%lx\n", field->LUMA_BA));
    DEBUGF2 (4, ("DEI_CCF_BA      0x%lx\n", field->CHROMA_BA));
    DEBUGF2 (4, ("DEI_VP_ORIGIN   0x%lx\n", field->LUMA_XY));
    DEBUGF2 (4, ("DEI_VP_SIZE     0x%lx\n", field->DEIVP_SIZE));
    DEBUGF2 (4, ("VHSRC_LUMA_VSRC 0x%lx\n", field->LUMA_VSRC));
    DEBUGF2 (4, ("VHSRC_CHR_VSRC  0x%lx\n", field->CHR_VSRC));

    DEBUGF2 (9, ("dei: ba luma ctl/next/cur/prev: %.8lx/%.8lx/%.8lx/%.8lx\n",
                 field->CTL, field->NEXT_LUMA_BA, field->LUMA_BA,
                 field->PREV_LUMA_BA));
}


void
CDEIVideoPipe::WriteCommonSetup (const DEISetup * const setup)
{
    DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * As we are starting a new buffer, disable the pipe (see comment in
     * WriteFieldSetup) until the field part has been written.
     */
    WriteVideoReg (DEI_CTL,           DEI_CTL_NOT_ACTIVE);

    WriteVideoReg (DEI_T3I_CTL,       setup->T3I_CTL);
    WriteVideoReg (DEI_VF_SIZE,       setup->MB_STRIDE);
    WriteVideoReg (DEI_YF_FORMAT,     setup->LUMA_FORMAT);
    WriteVideoReg (DEI_CF_FORMAT,     setup->CHROMA_FORMAT);

    WriteVideoReg (VHSRC_CTL,         setup->SRC_CTL);
    WriteVideoReg (VHSRC_TARGET_SIZE, setup->TARGET_SIZE);
    WriteVideoReg (VHSRC_LUMA_SIZE,   setup->LUMA_SIZE);
    WriteVideoReg (VHSRC_CHR_SIZE,    setup->CHR_SIZE);
    WriteVideoReg (VHSRC_LUMA_HSRC,   setup->LUMA_HSRC);
    WriteVideoReg (VHSRC_CHR_HSRC,    setup->CHR_HSRC);
    WriteVideoReg (VHSRC_NLZZD_Y,     setup->NLZZD);
    WriteVideoReg (VHSRC_NLZZD_C,     setup->NLZZD);
    WriteVideoReg (VHSRC_PDELTA,      setup->PDELTA);

    WriteVideoReg (DEI_MF_STACK_L0,   setup->MOTION_LINE_STACK);
    WriteVideoReg (DEI_MF_STACK_P0,   setup->MOTION_PIXEL_STACK);
    /* FIXME: writing this register should be optimized in that it only needs to
       be done at a stream change, not for every VSync! */
    WriteVideoReg (FMD_BLOCK_NUMBER,  setup->BLOCK_NUMBER);

    ASSERTF (N_ELEMENTS (setup->LUMA_LINE_STACK) == N_ELEMENTS (setup->CHROMA_LINE_STACK),
             ("%s: LUMA_LINE_STACK & CHROMA_LINE_STACK differ\n!?",
              __PRETTY_FUNCTION__));
    ASSERTF (N_ELEMENTS (setup->LUMA_PIXEL_STACK) == N_ELEMENTS (setup->CHROMA_PIXEL_STACK),
             ("%s: LUMA_PIXEL_STACK & CHROMA_PIXEL_STACK differ\n!?",
              __PRETTY_FUNCTION__));
    for (ULONG i = 0; i < N_ELEMENTS (setup->LUMA_LINE_STACK); ++i)
    {
        WriteVideoReg (DEI_YF_STACK_L0 + (i * sizeof (ULONG)),
                       setup->LUMA_LINE_STACK[i]);
        WriteVideoReg (DEI_CF_STACK_L0 + (i * sizeof (ULONG)),
                       setup->CHROMA_LINE_STACK[i]);
    }

    for (ULONG i = 0; i < N_ELEMENTS (setup->LUMA_PIXEL_STACK); ++i)
    {
        WriteVideoReg (DEI_YF_STACK_P0 + (i * sizeof (ULONG)),
                       setup->LUMA_PIXEL_STACK[i]);
        WriteVideoReg (DEI_CF_STACK_P0 + (i * sizeof (ULONG)),
                       setup->CHROMA_PIXEL_STACK[i]);
    }

    if (setup->hfluma)
    {
        // Copy the required filter coefficients to the hardware filter tables
        g_pIOS->MemcpyToDMAArea (&m_HFilter, 0,
                                 setup->hfluma, HFILTER_SIZE);
        g_pIOS->MemcpyToDMAArea (&m_HFilter, HFILTER_SIZE,
                                 setup->hfchroma, HFILTER_SIZE);

        g_pIOS->MemcpyToDMAArea (&m_VFilter, 0,
                                 setup->vfluma, VFILTER_SIZE);
        g_pIOS->MemcpyToDMAArea (&m_VFilter, VFILTER_SIZE,
                                 setup->vfchroma, VFILTER_SIZE);

    }

    m_videoPlug->WritePlugSetup (setup->vidPlugSetup);
}


/*
 * Resize filter setup for the video display pipeline used by QueueBuffer.
 *
 * Be afraid ... be very afraid!
 */
void
CDEIVideoPipe::selectScalingFilters (DEISetup                      &displayNode,
                                   const stm_display_buffer_t    * const pFrame,
                                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
    DEBUGF2(3,("%s\n", __PRETTY_FUNCTION__));

    const LONG lumaHphase   = (displayNode.LUMA_HSRC >> 16) & 0x1fff;
    const LONG chromaHphase = (displayNode.CHR_HSRC  >> 16) & 0x1fff;

    /*
     * Warning, there is a corner case where the bottom field would pick a
     * different vertical filter to the top field.
     *
     * The scale factor would need to be 1x and one field would have a phase
     * of 1/2 and the other would not have a phase of 1/2.
     *
     * This need thinking about.
     */
    const LONG lumaVphase   = (displayNode.topField.LUMA_VSRC >> 16) & 0x1fff;
    const LONG chromaVphase = (displayNode.topField.CHR_VSRC  >> 16) & 0x1fff;

    DEBUGF2 (3, ("%s src.width: %ld dst.width: %ld\n",
                 __PRETTY_FUNCTION__, qbinfo.src.width, qbinfo.dst.width));
    DEBUGF2 (3, ("%s src.height: %ld dst.height: %ld\n",
                 __PRETTY_FUNCTION__, qbinfo.src.height, qbinfo.dst.height));
    DEBUGF2 (3, ("%s hsrcinc: %lu vsrcinc: %lu\n",
                 __PRETTY_FUNCTION__, qbinfo.hsrcinc, qbinfo.vsrcinc));

    /*
     * We only want to update the filters when the phase changes from or to
     * the middle of the filter, phase 0 for the horizontal filter and phase 1/2
     * for the vertical filter. Otherwise we would be changing the filter all
     * the time during subpixel pan and scan when it wasn't needed.
     */
    register bool bUpdate = (
         (m_HVSRCState.LumaHphase   == 0 && lumaHphase != 0)
      || (m_HVSRCState.LumaHphase   != 0 && lumaHphase == 0)
      || (m_HVSRCState.ChromaHphase == 0 && chromaHphase != 0)
      || (m_HVSRCState.ChromaHphase != 0 && chromaHphase == 0)
      || (m_HVSRCState.LumaVphase   == fixedpointHALF && lumaVphase   != fixedpointHALF)
      || (m_HVSRCState.LumaVphase   != fixedpointHALF && lumaVphase   == fixedpointHALF)
      || (m_HVSRCState.ChromaVphase == fixedpointHALF && chromaVphase != fixedpointHALF)
      || (m_HVSRCState.ChromaVphase != fixedpointHALF && chromaVphase == fixedpointHALF));

    const ULONG VC1flag = (pFrame->src.ulFlags & ( STM_PLANE_SRC_VC1_POSTPROCESS_LUMA
                                                 | STM_PLANE_SRC_VC1_POSTPROCESS_CHROMA));

    bUpdate = bUpdate || (m_HVSRCState.VC1flag != VC1flag);

    if (  (VC1flag != 0)
       && (  (m_HVSRCState.VC1LumaType   != pFrame->src.ulPostProcessLumaType)
          || (m_HVSRCState.VC1ChromaType != pFrame->src.ulPostProcessChromaType)))
    {
        bUpdate = true;
    }

    DEBUGF2 (3, ("%s @ %p: bUpdate = %s LumaHInc = 0x%lx hsrcinc = 0x%lx LumaVInc = 0x%lx vsrcinc = 0x%lx\n",
                 __PRETTY_FUNCTION__, this, bUpdate?"true":"false", m_HVSRCState.LumaHInc, qbinfo.hsrcinc, m_HVSRCState.LumaVInc, qbinfo.vsrcinc));

    if (  bUpdate
       || (m_HVSRCState.LumaHInc != qbinfo.hsrcinc)
       || (m_HVSRCState.LumaVInc != qbinfo.vsrcinc))
    {
        /*
         * We put the filter change on both the top and bottom nodes because
         * we don't know which will get displayed first.
         */
        displayNode.SRC_CTL |= (VHSRC_CTL__LOAD_HFILTER_COEFFS
                                | VHSRC_CTL__LOAD_VFILTER_COEFFS);

        /*
         * Note: we force the phase to be non zero in the horizontal filter
         * selection code when we have a non-linear zoom, to catch the corner
         * case where the central area zoom (hsrcinc) is 1:1; this ensures we
         * do get a valid filter selected.
         */
        displayNode.hfluma   = m_VDPFilter.SelectHorizontalLumaFilter (qbinfo.hsrcinc,
                                                                       (qbinfo.pdeltaLuma == 0)?lumaHphase:1);
        displayNode.hfchroma = m_VDPFilter.SelectHorizontalChromaFilter (qbinfo.chroma_hsrcinc,
                                                                         (qbinfo.pdeltaLuma == 0)?chromaHphase:1);

        if (VC1flag & STM_PLANE_SRC_VC1_POSTPROCESS_LUMA)
          displayNode.vfluma = m_VDPFilter.SelectVC1VerticalLumaFilter (qbinfo.vsrcinc,
                                                                        lumaVphase,
                                                                        pFrame->src.ulPostProcessLumaType);
        else
          displayNode.vfluma = m_VDPFilter.SelectVerticalLumaFilter (qbinfo.vsrcinc,
                                                                     lumaVphase);

        if (VC1flag & STM_PLANE_SRC_VC1_POSTPROCESS_CHROMA)
          displayNode.vfchroma = m_VDPFilter.SelectVC1VerticalChromaFilter (qbinfo.chroma_vsrcinc,
                                                                            chromaVphase,
                                                                            pFrame->src.ulPostProcessChromaType);
        else
          displayNode.vfchroma = m_VDPFilter.SelectVerticalChromaFilter (qbinfo.chroma_vsrcinc,
                                                                         chromaVphase);

        DEBUGF2 (3, ("%s @ %p: hfluma = 0x%p hfchroma = 0x%p vfluma = 0x%p vfchroma = 0x%p\n",
                     __PRETTY_FUNCTION__, this, displayNode.hfluma, displayNode.hfchroma, displayNode.vfluma, displayNode.vfchroma));

#if defined(DEBUG)
        m_VDPFilter.DumpHorizontalFilter (displayNode.hfluma);
        m_VDPFilter.DumpHorizontalFilter (displayNode.hfchroma);
        m_VDPFilter.DumpVerticalFilter (displayNode.vfluma);
        m_VDPFilter.DumpVerticalFilter (displayNode.vfchroma);
#endif

        m_HVSRCState.LumaHInc      = qbinfo.hsrcinc;
        m_HVSRCState.LumaVInc      = qbinfo.vsrcinc;
        m_HVSRCState.LumaHphase    = lumaHphase;
        m_HVSRCState.LumaVphase    = lumaVphase;
        m_HVSRCState.ChromaHphase  = chromaHphase;
        m_HVSRCState.ChromaVphase  = chromaVphase;
        m_HVSRCState.VC1flag       = VC1flag;
        m_HVSRCState.VC1LumaType   = pFrame->src.ulPostProcessLumaType;
        m_HVSRCState.VC1ChromaType = pFrame->src.ulPostProcessChromaType;
    }

    DEXITn(3);
}


void
CDEIVideoPipe::setupHorizontalScaling (DEISetup                      &displayNode,
                                     const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    int x, width, chroma_width;
    int subpixelpos, repeat;

    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * Horizontal filter setup is reasonably simple and is the same for both
     * fields when dealing with the source as two interlaced fields.
     *
     * First of all get the start position in fixed point format, this is used
     * as the basis for both the luma and chroma filter setup.
     */

    width        = qbinfo.src.width;
    chroma_width = width / 2;
    repeat       = 3;

    x = FixedPointToInteger(qbinfo.src.x, &subpixelpos);

    /*
     * Here is how we deal with luma starting on an odd pixel. We adjust so
     * the DEI memory interface actually starts with the pixel before it;
     * this is so we can get the correct associated first chroma sample into
     * the chroma filter pipeline. We correct the luma by reducing the
     * repeat value in the filter tap setup, causing the original pixel sample
     * we wanted to be in the central position when the filter FSM starts.
     */
    if ((x % 2) == 1)
    {
      repeat--;
      x--;
      width++;
    }

    displayNode.LUMA_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                   // Hi 16 bits
                                         (qbinfo.pdeltaLuma == 0)?qbinfo.hsrcinc:qbinfo.nlLumaStartInc); // Lo 16 bits

    displayNode.LUMA_SIZE = (width & 0x7ff);

    /*
     * For the horizontal filter, with no scaling (i.e. a src increment of 1),
     * no non-linear zoom and no subpixel pan/scan we must use the internal
     * coefficients.
     */
    if ((qbinfo.hsrcinc != (ULONG) m_fixedpointONE) || (subpixelpos != 0) || (qbinfo.pdeltaLuma != 0))
        displayNode.SRC_CTL |= VHSRC_CTL__LUMA_HFILTER_ENABLE;


    DEBUGF2 (3, ("%s width: %d LUMA_SIZE: 0x%lx\n",
                 __PRETTY_FUNCTION__, width, displayNode.LUMA_SIZE));

    /*
     * We appear to need to round up x+width to be a multiple of 8 pixels,
     * otherwise certain combinations cause the display to break as if the
     * stride was wrong. We think this is an interaction between the chroma
     * HSRC and the T3I interface's state machines; related to the fact we
     * effectively pull image data in chunks of 8 pixels from memory.
     */
    const int tmp = (x+width)%8;
    if (tmp !=0)
        width += 8-tmp;

    /*
     * This initializes the DEI viewport register values; this routine must
     * be called before setting up the vertical scaling, which modifies the top
     * 16bits of these values for y position and height.
     *
     * The horizontal parameters do not change for top and bottom fields.
     */
    displayNode.topField.DEIVP_SIZE
      = displayNode.botField.DEIVP_SIZE
      = displayNode.altTopField.DEIVP_SIZE
      = displayNode.altBotField.DEIVP_SIZE
      = (width & 0xffff);

    displayNode.topField.LUMA_XY
      = displayNode.botField.LUMA_XY
      = displayNode.altTopField.LUMA_XY
      = displayNode.altBotField.LUMA_XY
      = (x & 0xffff);

    /*
     * Now for the chroma, which always has half the number of samples
     * horizontally than the luma. Chroma cannot end up starting on an odd
     * sample, so the filter repeat is reset to 3.
     */
    repeat = 3;

    x = FixedPointToInteger((qbinfo.src.x/2), &subpixelpos);

    displayNode.CHR_HSRC = PackRegister((subpixelpos | (repeat<<13)),                                            // Hi 16 bits
                                        (qbinfo.pdeltaLuma == 0)?qbinfo.chroma_hsrcinc:qbinfo.nlChromaStartInc); // Lo 16 bits

    displayNode.CHR_SIZE  = chroma_width & 0x7ff;

    if ((qbinfo.chroma_hsrcinc != (ULONG)m_fixedpointONE) || (subpixelpos != 0) || (qbinfo.pdeltaLuma != 0))
        displayNode.SRC_CTL |= VHSRC_CTL__CHROMA_HFILTER_ENABLE;

    displayNode.PDELTA = PackRegister(qbinfo.pdeltaLuma, qbinfo.pdeltaLuma/2);
    displayNode.NLZZD  = PackRegister(qbinfo.nlZone1, qbinfo.nlZone2);

    DEBUGF2 (3, ("%s - out\n", __PRETTY_FUNCTION__));
}


void
CDEIVideoPipe::calculateVerticalFilterSetup (DEIFieldSetup           &field,
                                     const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                                           int                      firstSampleOffset,
                                           bool                     doLumaFilter) const

{
    int y,phase,repeat,height;

    CGammaCompositorPlane::CalculateVerticalFilterSetup(qbinfo,
                                                        firstSampleOffset,
                                                        doLumaFilter,
                                                        y,
                                                        phase,
                                                        repeat,
                                                        height);

    /*
     * For interlaced content, not being de-interlaced, we need to convert the
     * y and number of input lines which are currently in field lines to
     * frame lines for the T3 memory interface. The interface, which is told
     * the content is interlaced then just goes and effectively divides them by
     * 2 again! This is used for all image formats on the DEI, unlike the
     * "CGammaCompositorDisp" video plane which only uses line addressing for
     * 420 macroblock format. However we don't use odd addresses for bottom
     * fields, that is done by offsetting the base address of the image
     * in the memory addressing setup.
     */
    if (!qbinfo.isHWDeinterlacing && qbinfo.isSourceInterlaced)
    {
       height *= 2;
       y *= 2;
    }

    if (doLumaFilter)
    {
        field.LUMA_XY    |= (y << 16);
        field.DEIVP_SIZE |= (height << 16);
        field.LUMA_VSRC  =  (repeat << 29) | (phase << 16) | qbinfo.vsrcinc;

        DEBUGF2 (3, ("luma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x\n",
                     y, phase, height, repeat, (int)qbinfo.vsrcinc));
    }
    else
    {
        field.CHR_VSRC = (repeat << 29) | (phase << 16) | qbinfo.chroma_vsrcinc;

        DEBUGF2 (3, ("chroma_filter: y: %d phase: 0x%08x height: %d repeat: %d srcinc: 0x%08x\n",
                     y, phase, height, repeat, (int)qbinfo.chroma_vsrcinc));
    }

}


void
CDEIVideoPipe::setupProgressiveVerticalScaling (DEISetup                      &displayNode,
                                              const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * For progressive content we only need a single node, even on an interlaced
     * display, as we use the top and bottom field setup in the node. Note that
     * the bottom field values will not be used when displaying on a
     * progressive display.
     *
     * If we are using a P2I then the top and bottom field filter setup is
     * identical, the P2I field selection is setup on the fly. Otherwise
     * we need to adjust the start position by a source line.
     */
    int bottomFieldAdjustment = ScaleVerticalSamplePosition(qbinfo.isUsingProgressive2InterlacedHW?0:fixedpointONE, qbinfo);

    /*
     * First Luma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, 0, true);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, bottomFieldAdjustment, true);

    /*
     * Now Chroma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, 0, false);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, bottomFieldAdjustment, false);

    /*
     * The alternate field setups (for inter-field interpolation) are identical
     * to the main ones for progressive content. This keeps the presentation
     * logic simpler.
     */
    displayNode.altTopField = displayNode.topField;
    displayNode.altBotField = displayNode.botField;
}


void
CDEIVideoPipe::setupDeInterlacedVerticalScaling (DEISetup                      &displayNode,
                                               const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * We de-interlace, without having a real hardware de-interlacer,
     * by scaling up each field to produce a whole frame. To make consecutive
     * frames appear to be in the same place we move the source start position
     * by +/-1/4 of the distance between two source lines in the SAME field.
     *
     */
    int topSamplePosition =  fixedpointQUARTER;
    int botSamplePosition = -fixedpointQUARTER;

    /*
     * Luma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, topSamplePosition, true);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, true);

    /*
     * Chroma
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, topSamplePosition, false);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, false);

    /*
     * As this must be presented on a progressive display the alternate field
     * setups are never needed.
     */
}


void
CDEIVideoPipe::setupHWDeInterlacedVerticalScaling (DEISetup                      &displayNode,
                                                 const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    if (qbinfo.isDisplayInterlaced && !qbinfo.isUsingProgressive2InterlacedHW)
    {
        /*
         * Setup rescale when de-interlacing and then presenting back to an
         * interlaced display without the help of a P2I block. We need to
         * calculate the sample position, in the de-interlaced source, that
         * will appear on the first line of the bottom field. This is why
         * the offset is 1 not 1/2.
         *
         */
        int bottomFieldSampleOffset = ScaleVerticalSamplePosition(fixedpointONE,qbinfo);

        /*
         * Note: Don't be tempted to optimize any of this with straight
         * assignments, you will break the memory base addresses in the
         * alternate fields.
         */
        calculateVerticalFilterSetup(displayNode.topField,qbinfo,0,true);
        calculateVerticalFilterSetup(displayNode.altTopField,qbinfo,0,true);

        calculateVerticalFilterSetup(displayNode.botField, qbinfo,
                                     bottomFieldSampleOffset, true);
        calculateVerticalFilterSetup(displayNode.altBotField, qbinfo,
                                     bottomFieldSampleOffset, true);

        calculateVerticalFilterSetup(displayNode.topField,qbinfo,0,false);
        calculateVerticalFilterSetup(displayNode.altTopField,qbinfo,0,false);

        calculateVerticalFilterSetup(displayNode.botField, qbinfo,
                                     bottomFieldSampleOffset, false);
        calculateVerticalFilterSetup(displayNode.altBotField, qbinfo,
                                     bottomFieldSampleOffset, false);
    }
    else
    {
        /*
         * Real de-interlacing to a progressive display or to an interlaced
         * display using a P2I HW block.
         *
         * Note: The alternate field setup could be optimized in this case
         * as the base addresses would still be correct, but we don't
         * to keep the code consistent.
         */
        calculateVerticalFilterSetup(displayNode.topField,qbinfo,0,true);
        calculateVerticalFilterSetup(displayNode.altTopField,qbinfo,0,true);
        calculateVerticalFilterSetup(displayNode.botField,qbinfo,0,true);
        calculateVerticalFilterSetup(displayNode.altBotField,qbinfo,0,true);

        calculateVerticalFilterSetup(displayNode.topField,qbinfo,0,false);
        calculateVerticalFilterSetup(displayNode.altTopField,qbinfo,0,false);
        calculateVerticalFilterSetup(displayNode.botField,qbinfo,0,false);
        calculateVerticalFilterSetup(displayNode.altBotField,qbinfo,0,false);
    }

    displayNode.topField.CTL    &= ~DEI_CTL_BYPASS;
    displayNode.botField.CTL    &= ~DEI_CTL_BYPASS;
    displayNode.altTopField.CTL &= ~DEI_CTL_BYPASS;
    displayNode.altBotField.CTL &= ~DEI_CTL_BYPASS;

    /* DEIVP_SIZE is the full frame size, e.g. 720x480 for 480i contents,
       therefore, to calculate the number of blocks we have to divide the
       height by two */
    ULONG fmdblocksperline  =  ((displayNode.topField.DEIVP_SIZE >>  0)
                                & 0x7ff)
                               / m_fmd_init_params->ulH_block_sz;
    ULONG fmdblocksperfield = (((displayNode.topField.DEIVP_SIZE >> 16)
                                & 0x7ff) / 2)
                              / m_fmd_init_params->ulV_block_sz;
    /* just to be on the safe side */
    fmdblocksperline  = _MIN (fmdblocksperline,  m_MaxFMDBlocksPerLine);
    fmdblocksperfield = _MIN (fmdblocksperfield, m_MaxFMDBlocksPerField);

    displayNode.BLOCK_NUMBER = (fmdblocksperfield << 8) | fmdblocksperline;

    /*
     * Fix up the forward and backward reference fields for the de-interlacer,
     * where they are contained in this image. This makes the logic in the
     * hardware update easier, because any "PREV" addresses that are NULL must
     * be got from the node currently on display (if any) and any "NEXT"
     * addresses that are NULL must be "peeked" from the next node in the list
     * if it exists.
     *
     * Also assign the motion field addresses, which we can do statically,
     * although the motion detection mode and hence the actual buffer usage
     * will get determined in the HW update.
     */
    if (!qbinfo.firstFieldOnly)
    {
      if (qbinfo.firstFieldType == GNODE_TOP_FIELD)
      {
        displayNode.topField.NEXT_LUMA_BA      = displayNode.botField.LUMA_BA;
        displayNode.altBotField.NEXT_LUMA_BA   = displayNode.botField.LUMA_BA;
        displayNode.botField.PREV_LUMA_BA      = displayNode.topField.LUMA_BA;
        displayNode.altTopField.PREV_LUMA_BA   = displayNode.topField.LUMA_BA;

        displayNode.topField.NEXT_CHROMA_BA    = displayNode.botField.CHROMA_BA;
        displayNode.altBotField.NEXT_CHROMA_BA = displayNode.botField.CHROMA_BA;
        displayNode.botField.PREV_CHROMA_BA    = displayNode.topField.CHROMA_BA;
        displayNode.altTopField.PREV_CHROMA_BA = displayNode.topField.CHROMA_BA;

        allocateMotionBuffers (displayNode.topField, displayNode.altBotField);
        allocateMotionBuffers (displayNode.botField, displayNode.altTopField);
      }
      else
      {
        displayNode.topField.PREV_LUMA_BA      = displayNode.botField.LUMA_BA;
        displayNode.altBotField.PREV_LUMA_BA   = displayNode.botField.LUMA_BA;
        displayNode.botField.NEXT_LUMA_BA      = displayNode.topField.LUMA_BA;
        displayNode.altTopField.NEXT_LUMA_BA   = displayNode.topField.LUMA_BA;

        displayNode.topField.PREV_CHROMA_BA    = displayNode.botField.CHROMA_BA;
        displayNode.altBotField.PREV_CHROMA_BA = displayNode.botField.CHROMA_BA;
        displayNode.botField.NEXT_CHROMA_BA    = displayNode.topField.CHROMA_BA;
        displayNode.altTopField.NEXT_CHROMA_BA = displayNode.topField.CHROMA_BA;

        allocateMotionBuffers (displayNode.botField, displayNode.altTopField);
        allocateMotionBuffers (displayNode.topField, displayNode.altBotField);
      }
    }
    else
    {
      if (qbinfo.firstFieldType == GNODE_TOP_FIELD)
        allocateMotionBuffers (displayNode.topField, displayNode.altBotField);
      else
        allocateMotionBuffers (displayNode.botField, displayNode.altTopField);
    }
}


void
CDEIVideoPipe::allocateMotionBuffers (DEIFieldSetup &field,
                                    DEIFieldSetup &altfield)
{
    /* current motion */
    register ULONG addr = m_MotionData[m_nCurrentMotionData].ulPhysical;
    field.CURRENT_MOTION    = addr;
    altfield.CURRENT_MOTION = addr;

    /* next motion */
    addr = m_MotionData[(m_nCurrentMotionData+1)%DEI_MOTION_BUFFERS].ulPhysical;
    field.NEXT_MOTION    = addr;
    altfield.NEXT_MOTION = addr;

    /* previous motion */
    addr = (!m_nCurrentMotionData) ? m_MotionData[DEI_MOTION_BUFFERS-1].ulPhysical
                                   : m_MotionData[m_nCurrentMotionData-1].ulPhysical;
    field.PREVIOUS_MOTION    = addr;
    altfield.PREVIOUS_MOTION = addr;

    m_nCurrentMotionData = (m_nCurrentMotionData + 1) % DEI_MOTION_BUFFERS;
}


void
CDEIVideoPipe::setupInterlacedVerticalScaling (DEISetup                      &displayNode,
                                             const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    /*
     * The first sample position to interpolate a top field from the bottom
     * field source data is just a coordinate translation between the top and
     * bottom source fields.
     */
    int altTopSamplePosition = -fixedpointHALF;

    /*
     * The first sample position for the _display's_ bottom field, interpolated
     * from the top field data, is +1/2 (but scaled this time). This is because
     * the distance between the source samples located on two consecutive top
     * field display lines has changed; the bottom field display line sample
     * must lie half way between them in the source coordinate system.
     */
    int altBotSamplePosition = ScaleVerticalSamplePosition(fixedpointHALF, qbinfo);

    /*
     * You might think that the sample position for the bottom field generated
     * from real bottom field data should always be 0. But this is quite
     * complicated, because as we have seen above when the image is scaled the
     * mapping of the bottom field's first display line back to the source
     * image changes. Plus we have a source coordinate system change because
     * the first sample of the bottom field data is referenced as 0 just as the
     * first sample of the top field data is.
     *
     * So the start position is the source sample position, of the display's
     * bottom field in the top field's coordinate system, translated to the
     * bottom field's coordinate system.
     */
    int botSamplePosition = altBotSamplePosition - fixedpointHALF;

    /*
     * Work out the real top and bottom fields
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, 0, true);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, true);

    /*
     * Now interpolate a bottom field from the top field contents in
     * order to do slow motion and pause without motion jitter.
     */
    calculateVerticalFilterSetup (displayNode.altBotField, qbinfo,altBotSamplePosition, true);

    /*
     * Now interpolate a top field from the bottom field contents.
     */
    calculateVerticalFilterSetup (displayNode.altTopField, qbinfo, altTopSamplePosition, true);

    /*
     * Now do exactly the same again for the chroma.
     */
    calculateVerticalFilterSetup (displayNode.topField, qbinfo, 0, false);
    calculateVerticalFilterSetup (displayNode.botField, qbinfo, botSamplePosition, false);

    calculateVerticalFilterSetup (displayNode.altBotField, qbinfo, altBotSamplePosition, false);
    calculateVerticalFilterSetup (displayNode.altTopField, qbinfo, altTopSamplePosition, false);
}


/*
 * This is the magic setup for the DEI's memory access. Don't ask,
 * most of this is verbatim from documentation and reference drivers
 * without any real explanation (particularly for 420MB format).
 *
 * Even more scary than the filter setup
 */
void
CDEIVideoPipe::set420MBMemoryAddressing (DEISetup                      &displayNode,
                                       const stm_display_buffer_t    * const pFrame,
                                       const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    const ULONG baseAddress = pFrame->src.ulVideoBufferAddr;
    const ULONG chromaBaseAddress = baseAddress + pFrame->src.chromaBufferOffset;

    displayNode.T3I_CTL = (DEI_T3I_CTL_OPCODE_SIZE_8 |
                           DEI_T3I_CTL_MACRO_BLOCK_ENABLE | m_nMBChunkSize);

    displayNode.MB_STRIDE  = ((pFrame->src.ulStride + 15) & ~15);

    ULONG mb_width      = ((pFrame->src.ulStride + 31) & ~31) / 32;
    ULONG mb_half_width = displayNode.MB_STRIDE / 16;
    ULONG nlines        = (qbinfo.src.y / m_fixedpointONE) + qbinfo.src.height;

    displayNode.LUMA_FORMAT         = DEI_FORMAT_REVERSE;
    displayNode.LUMA_PIXEL_STACK[0] = DEI_STACK (56, (2 * mb_width));
    displayNode.LUMA_PIXEL_STACK[1] = DEI_STACK ( 8, 2);

    displayNode.CHROMA_FORMAT         = (DEI_FORMAT_REVERSE
                                         | DEI_REORDER_15263748);
    displayNode.CHROMA_PIXEL_STACK[0] = DEI_STACK ( 44, mb_width);
    displayNode.CHROMA_PIXEL_STACK[1] = DEI_STACK ( 12, 2);
    displayNode.CHROMA_PIXEL_STACK[2] = DEI_STACK (  4, 2);

    if (qbinfo.isSourceInterlaced)
    {
        const ULONG mb_height = ((nlines * 2 + 31) & ~31) / 32;

        displayNode.topField.LUMA_BA      = baseAddress +   8;
        displayNode.botField.LUMA_BA      = baseAddress + 136;
        displayNode.altTopField.LUMA_BA   = baseAddress + 136;
        displayNode.altBotField.LUMA_BA   = baseAddress +   8;

        displayNode.topField.CHROMA_BA    = chromaBaseAddress +  8;
        displayNode.botField.CHROMA_BA    = chromaBaseAddress + 72;
        displayNode.altTopField.CHROMA_BA = chromaBaseAddress + 72;
        displayNode.altBotField.CHROMA_BA = chromaBaseAddress +  8;

        displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_420_ODD;
        displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_420_EVEN;
        displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_420_EVEN;
        displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_420_ODD;

        displayNode.LUMA_LINE_STACK[0] = DEI_STACK ((64 * mb_half_width) - 37,
                                                    mb_height);
        displayNode.LUMA_LINE_STACK[1] = DEI_STACK ( 27, 2);
        displayNode.LUMA_LINE_STACK[2] = DEI_STACK (  3, 4);
        displayNode.LUMA_LINE_STACK[3] = DEI_STACK (- 1, 2);

        displayNode.CHROMA_LINE_STACK[0] = DEI_STACK ((32 * mb_half_width) - 33,
                                                      mb_height);
        displayNode.CHROMA_LINE_STACK[1] = DEI_STACK ( 31, 2);
        displayNode.CHROMA_LINE_STACK[2] = DEI_STACK (  3, 2);
        displayNode.CHROMA_LINE_STACK[3] = DEI_STACK (- 1, 2);
    }
    else
    {
        const ULONG mb_height = ((nlines + 31) & ~31) / 32;

        displayNode.topField.LUMA_BA    = baseAddress + 8;
        displayNode.botField.LUMA_BA    = baseAddress + 8;
        displayNode.altTopField.LUMA_BA = baseAddress + 8;
        displayNode.altBotField.LUMA_BA = baseAddress + 8;

        displayNode.topField.CHROMA_BA    = chromaBaseAddress + 8;
        displayNode.botField.CHROMA_BA    = chromaBaseAddress + 8;
        displayNode.altTopField.CHROMA_BA = chromaBaseAddress + 8;
        displayNode.altBotField.CHROMA_BA = chromaBaseAddress + 8;

        displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
        displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
        displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
        displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;

        displayNode.LUMA_LINE_STACK[0] = DEI_STACK ((64 * mb_half_width) - 53,
                                                    mb_height);
        displayNode.LUMA_LINE_STACK[1] = DEI_STACK ( 11, 2);
        displayNode.LUMA_LINE_STACK[2] = DEI_STACK (-13, 4);
        displayNode.LUMA_LINE_STACK[3] = DEI_STACK (-17, 2);
        displayNode.LUMA_LINE_STACK[4] = DEI_STACK ( 16, 2);

        displayNode.CHROMA_LINE_STACK[0] = DEI_STACK ((32 * mb_half_width) - 41,
                                                      mb_height);
        displayNode.CHROMA_LINE_STACK[1] = DEI_STACK ( 23, 2);
        displayNode.CHROMA_LINE_STACK[2] = DEI_STACK (- 5, 2);
        displayNode.CHROMA_LINE_STACK[3] = DEI_STACK (- 9, 2);
        displayNode.CHROMA_LINE_STACK[4] = DEI_STACK (  8, 2);
    }
}


void
CDEIVideoPipe::set422RInterleavedMemoryAddressing (DEISetup                      &displayNode,
                                                 const stm_display_buffer_t    * const pFrame,
                                                 const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    const ULONG baseAddress = pFrame->src.ulVideoBufferAddr;
    const ULONG nlines = (qbinfo.src.y / m_fixedpointONE) + qbinfo.src.height;

    /*
     * Calculating the pixels per line back from the stride
     * is ok because in general we do not end up with
     * large mismatches between stride and number of pixels in
     * our applications.
     */
    const ULONG pixelsperline = (  pFrame->src.ulStride
                                / (pFrame->src.ulPixelDepth >> 3));

    /*
     * This is the number of STBus 8byte words in a line.
     */
    const ULONG wordsperline  = (((pixelsperline + (pixelsperline % m_nRasterOpcodeSize)) / m_nRasterOpcodeSize)
                                 * (m_nRasterOpcodeSize/8));

    switch(m_nRasterOpcodeSize)
    {
      case 32:
        displayNode.T3I_CTL = DEI_T3I_CTL_OPCODE_SIZE_32;
        break;
      case 16:
        displayNode.T3I_CTL = DEI_T3I_CTL_OPCODE_SIZE_16;
        break;
      case 8:
      default:
        displayNode.T3I_CTL = DEI_T3I_CTL_OPCODE_SIZE_8;
        break;
    }

    displayNode.T3I_CTL            |= DEI_T3I_CTL_422R_ENABLE;
    displayNode.T3I_CTL            |= m_nRasterChunkSize;
    if (pFrame->src.ulColorFmt == SURF_YUYV)
        displayNode.T3I_CTL |= DEI_T3I_CTL_422R_YUYV_NOT_UYVY;

    displayNode.LUMA_FORMAT   = DEI_FORMAT_IDENTITY;
    displayNode.CHROMA_FORMAT = (DEI_FORMAT_IDENTITY | DEI_REORDER_IDENTITY);

    displayNode.LUMA_PIXEL_STACK[0] = DEI_STACK (1, wordsperline);

    if (qbinfo.isSourceInterlaced)
    {
        const ULONG secondLineAddress = baseAddress + pFrame->src.ulStride;

        displayNode.topField.LUMA_BA    = baseAddress;
        displayNode.botField.LUMA_BA    = secondLineAddress;
        displayNode.altTopField.LUMA_BA = secondLineAddress;
        displayNode.altBotField.LUMA_BA = baseAddress;

        displayNode.LUMA_LINE_STACK[0]  = DEI_STACK (wordsperline * 2,
                                                     nlines);

        displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_422_ODD;
        displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_422_EVEN;
        displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_422_EVEN;
        displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_422_ODD;
    }
    else
    {
        displayNode.topField.LUMA_BA    = baseAddress;
        displayNode.botField.LUMA_BA    = baseAddress;
        displayNode.altTopField.LUMA_BA = baseAddress;
        displayNode.altBotField.LUMA_BA = baseAddress;

        /*
         * We need to split into two loops to support 1080p, as each loop only
         * supports 1024 iterations.
         */
        displayNode.LUMA_LINE_STACK[0]  = DEI_STACK (wordsperline, 2);
        displayNode.LUMA_LINE_STACK[1]  = DEI_STACK (wordsperline,
                                                     (nlines + 1) / 2);

        displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
        displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
        displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
        displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
    }
}


void
CDEIVideoPipe::setPlanarMemoryAddressing (DEISetup                      &displayNode,
                                        const stm_display_buffer_t    * const pFrame,
                                        const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
{
    DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

    const ULONG nlines = (qbinfo.src.y / m_fixedpointONE) + qbinfo.src.height;
    const ULONG pixelsperline = pFrame->src.ulStride / (pFrame->src.ulPixelDepth >> 3);
    const ULONG wordsperline = (pixelsperline + (pixelsperline % 8)) / 8;
    const ULONG wordpairsperline = (pixelsperline + (pixelsperline % 16)) / 16;

    const ULONG baseAddress = pFrame->src.ulVideoBufferAddr;
    ULONG chromaBaseAddress = baseAddress + pFrame->src.chromaBufferOffset;
    const ULONG uvoffset = ( (wordsperline / 2)
                           * (pFrame->src.ulTotalLines / (qbinfo.isSrc420 ? 2 : 1)));

    displayNode.T3I_CTL = DEI_T3I_CTL_OPCODE_SIZE_8 | m_nPlanarChunkSize;
    displayNode.LUMA_FORMAT  = DEI_FORMAT_IDENTITY;

    /*
     * No chroma endianess swap, however two consecutive 8byte words are
     * "shuffled" together to produce CbCrCbCr... from one word of 8*Cb and one
     * word of 8*Cr.
     */
    displayNode.CHROMA_FORMAT = (DEI_FORMAT_IDENTITY
                                 | DEI_REORDER_WORD_INTERLEAVE);

    displayNode.LUMA_PIXEL_STACK[0] = DEI_STACK (1,wordsperline);

    if (pFrame->src.ulColorFmt == SURF_YVU420)
    {
        /*
         * Cr followed by Cb, we cannot use the chroma reordering/endian
         * conversion to get the bytes in the right order so we have to
         * arrange to read Cb first.
         */
        chromaBaseAddress += (uvoffset*8);
        displayNode.CHROMA_PIXEL_STACK[0] = DEI_STACK ((uvoffset + 1),
                                                       wordpairsperline);
        displayNode.CHROMA_PIXEL_STACK[1] = DEI_STACK (-uvoffset, 2);
    }
    else
    {
        /*
         * Easy case Cb followed by Cr
         */
        displayNode.CHROMA_PIXEL_STACK[0] = DEI_STACK ((1 - uvoffset),
                                                       wordpairsperline);
        displayNode.CHROMA_PIXEL_STACK[1] = DEI_STACK (uvoffset, 2);
    }

    if (qbinfo.isSourceInterlaced)
    {
        const ULONG secondLineAddress       = baseAddress + pFrame->src.ulStride;
        const ULONG chromaSecondLineAddress = ( chromaBaseAddress
                                              + (pFrame->src.ulStride / 2));

        displayNode.topField.LUMA_BA    = baseAddress;
        displayNode.botField.LUMA_BA    = secondLineAddress;
        displayNode.altTopField.LUMA_BA = secondLineAddress;
        displayNode.altBotField.LUMA_BA = baseAddress;

        displayNode.topField.CHROMA_BA    = chromaBaseAddress;
        displayNode.botField.CHROMA_BA    = chromaSecondLineAddress;
        displayNode.altTopField.CHROMA_BA = chromaSecondLineAddress;
        displayNode.altBotField.CHROMA_BA = chromaBaseAddress;

        if (qbinfo.isSrc420)
        {
            displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_420_ODD;
            displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_420_EVEN;
            displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_420_EVEN;
            displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_420_ODD;

            displayNode.CHROMA_LINE_STACK[0] = DEI_STACK (wordsperline,
                                                          nlines / 2);
        }
        else
        {
            displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_422_ODD;
            displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_422_EVEN;
            displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_422_EVEN;
            displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_422_ODD;

            displayNode.CHROMA_LINE_STACK[0] = DEI_STACK (wordsperline, nlines);
        }

        displayNode.LUMA_LINE_STACK[0] = DEI_STACK (wordsperline * 2, nlines);
    }
    else
    {
        const ULONG halfheight = (nlines+1)/2;

        displayNode.topField.LUMA_BA      = baseAddress;
        displayNode.botField.LUMA_BA      = baseAddress;
        displayNode.altTopField.LUMA_BA   = baseAddress;
        displayNode.altBotField.LUMA_BA   = baseAddress;

        displayNode.topField.CHROMA_BA    = chromaBaseAddress;
        displayNode.botField.CHROMA_BA    = chromaBaseAddress;
        displayNode.altTopField.CHROMA_BA = chromaBaseAddress;
        displayNode.altBotField.CHROMA_BA = chromaBaseAddress;

        if (qbinfo.isSrc420)
        {
            displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
            displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
            displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;
            displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_420_PROGRESSIVE;

            displayNode.CHROMA_LINE_STACK[0] = DEI_STACK (wordsperline / 2,
                                                          nlines / 2);
        }
        else
        {
            displayNode.topField.CTL    |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
            displayNode.botField.CTL    |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
            displayNode.altTopField.CTL |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;
            displayNode.altBotField.CTL |= DEI_CTL_CVF_TYPE_422_PROGRESSIVE;

            /*
             * We need to split into two loops to support 1080p,
             * each loop only supports 1024 iterations.
             */

            displayNode.CHROMA_LINE_STACK[0] = DEI_STACK (wordsperline / 2, 2);
            displayNode.CHROMA_LINE_STACK[1] = DEI_STACK (wordsperline / 2,
                                                          halfheight);
        }

        displayNode.LUMA_LINE_STACK[0] = DEI_STACK (wordsperline, 2);
        displayNode.LUMA_LINE_STACK[1] = DEI_STACK (wordsperline, halfheight);
    }
}
