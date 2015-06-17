/***********************************************************************
 *
 * File: Gamma/CDEIVideoPipeV2.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "DEIVideoPipeV2.h"
#include "DEIReg.h"

#define IQI_OFFSET 0x200   // offset from DEI

/* DEI_CTL register */
#if 0
/* our setup */
#define COMMON_CTRLS    (DEI_CTL_BYPASS\
                         | DEI_CTL_DD_MODE_DIAG9                       \
                         | DEI_CTL_CLAMP_MODE_DIRECTIONAL_NOT_VERTICAL \
                         | DEI_CTL_DIR_RECURSIVE_ENABLE                \
                         | (0x6 << DEI_CTL_KCORRECTION_SHIFT)          \
                         | (0xa << DEI_CTL_T_DETAIL_SHIFT)             \
                         | C2_DEI_CTL_KMOV_FACTOR_14                   \
                         | C2_DEI_CTL_MD_MODE_OFF                      \
                        )
#endif
#if 1
/* from ST1000 */
#define COMMON_CTRLS    (DEI_CTL_BYPASS\
                         | DEI_CTL_DD_MODE_DIAG9                       \
                         | DEI_CTL_CLAMP_MODE_DIRECTIONAL_NOT_VERTICAL \
                         | DEI_CTL_DIR_RECURSIVE_ENABLE                \
                         | (0x7 << DEI_CTL_KCORRECTION_SHIFT)          \
                         | (0x8 << DEI_CTL_T_DETAIL_SHIFT)             \
                         | C2_DEI_CTL_KMOV_FACTOR_14                   \
                         | C2_DEI_CTL_MD_MODE_OFF                      \
                        )
#endif
#if 0
/* from validation */
#define COMMON_CTRLS    (DEI_CTL_BYPASS\
                         | DEI_CTL_DD_MODE_DIAG9                       \
                         | DEI_CTL_CLAMP_MODE_DIRECTIONAL_NOT_VERTICAL \
                         | DEI_CTL_DIR_RECURSIVE_ENABLE                \
                         | DEI_CTL_DIR3_ENABLE                         \
                         | (0x4 << DEI_CTL_KCORRECTION_SHIFT)          \
                         | (0xc << DEI_CTL_T_DETAIL_SHIFT)             \
                         | C2_DEI_CTL_KMOV_FACTOR_14                   \
                         | C2_DEI_CTL_MD_MODE_OFF                      \
                         | C2_DEI_CTL_REDUCE_MOTION                    \
                        )
#endif



static FMDSW_FmdInitParams_t g_fmd_init_params = {
  ulH_block_sz  : 40, /*!< Horizontal block size for BBD */
  ulV_block_sz  : 20, /*!< Vertical block size for BBD */

  count_miss    :  2, /*!< Delay for a film mode detection */
  count_still   : 30, /*!< Delay for a still mode detection */
  t_noise       : 10, /*!< Noise threshold */
  k_cfd1        : 21, /*!< Consecutive field difference factor 1 */
  k_cfd2        : 16, /*!< Consecutive field difference factor 2 */
  k_cfd3        :  6, /*!< Consecutive field difference factor 3 */
  t_mov         : 10, /*!< Moving pixel threshold */
  t_num_mov_pix :  9, /*!< Moving block threshold */
  t_repeat      : 70, /*!< Threshold on BBD for a field repetition */
  t_scene       : 15, /*!< Threshold on BBD for a scene change  */
  k_scene       : 25, /*!< Percentage of blocks with BBD > t_scene */
  d_scene       :  1, /*!< Scene change detection delay (1,2,3 or 4) */
};


CDEIVideoPipeV2::CDEIVideoPipeV2 (stm_plane_id_t   planeID,
                                  CGammaVideoPlug *plug,
                                  ULONG            dei_base): CDEIVideoPipe (planeID,
                                                                             plug,
                                                                             dei_base)
{
  DEBUGF2 (4, (FENTRY ", planeID: %d, dei_base: %lx, IQI base: %lx\n",
               __PRETTY_FUNCTION__,
               planeID, dei_base, dei_base + IQI_OFFSET));

  m_IQI = 0;

  /*
   * Supports 1080i de-interlacing. Note the use of 1088 lines as this is
   * a multiple of the size of a macroblock. Although in principal this version
   * of the hardware could be instantiated on a chip with SD sized line buffers,
   * it is pretty unlikely in any chip we will support in the near future as
   * they are all aimed at the HD market.
   */
  m_nDEIPixelBufferLength = 1920;
  m_nDEIPixelBufferHeight = 1088;

  /*
   * Override the motion control setup for this version of the IP.
   */
  m_nDEICtlLumaInterpShift   = C2_DEI_CTL_LUMA_INTERP_SHIFT;
  m_nDEICtlChromaInterpShift = C2_DEI_CTL_CHROMA_INTERP_SHIFT;
  m_nDEICtlSetupMotionSD     = COMMON_CTRLS;
  m_nDEICtlSetupMotionHD     = COMMON_CTRLS | C2_DEI_CTL_REDUCE_MOTION;
  m_nDEICtlSetupMedian       = COMMON_CTRLS;
  m_nDEICtlMDModeShift       = C2_DEI_CTL_MD_MODE_SHIFT;
  m_nDEICtlMainModeDEI       = 0;

  /* has the P2I block */
  m_bHasProgressive2InterlacedHW = true;

  /* has a new FMD */
  m_bHaveFMDBlockOffset  = true;
  m_FMDEnable            = C2_DEI_CTL_FMD_ENABLE;
  m_fmd_init_params      = &g_fmd_init_params;
  m_MaxFMDBlocksPerLine  = 48;
  m_MaxFMDBlocksPerField = 27;
  m_FMDStatusSceneCountMask   = FMD_STATUS_C2_SCENE_COUNT_MASK;
  m_FMDStatusMoveStatusMask   = FMD_STATUS_C2_MOVE_STATUS_MASK;
  m_FMDStatusRepeatStatusMask = FMD_STATUS_C2_REPEAT_STATUS_MASK;

  DEXITn (4);
}


CDEIVideoPipeV2::~CDEIVideoPipeV2 (void)
{
  DENTRYn (4);

  delete (m_IQI);

  DEXITn (4);
}


bool
CDEIVideoPipeV2::Create (void)
{
  DENTRYn (4);

  if (!CDEIVideoPipe::Create ())
    return false;

  m_IQI = new CSTmIQI (IQI_CELL_REV2, m_baseAddress + IQI_OFFSET);
  if (!m_IQI)
    {
      DEBUGF (("%s - failed to create IQI\n", __PRETTY_FUNCTION__));
      return false;
    }
  m_IQI->Create ();

  DEXITn (4);

  return true;
}


bool
CDEIVideoPipeV2::QueuePreparedNode (const stm_display_buffer_t    * const pFrame,
                                    const GAMMA_QUEUE_BUFFER_INFO &qbi,
                                    struct DEIVideoPipeV2Setup    *node)
{
  DENTRYn (8);

  m_IQI->CalculateSetup (pFrame,
                         qbi.isDisplayInterlaced,
                         qbi.isSourceInterlaced,
                         &node->iqi_setup);

  return QueueNode (pFrame,
                    qbi,
                    node->dei_setup, sizeof (struct DEIVideoPipeV2Setup));
}


bool
CDEIVideoPipeV2::QueueBuffer (const stm_display_buffer_t * const pFrame,
                              const void                 * const user)
{
  DENTRYn (8);

  struct DEIVideoPipeV2Setup node = { dei_setup : { 0 } };
  struct GAMMA_QUEUE_BUFFER_INFO   qbi;

  if (UNLIKELY (!PrepareNodeForQueueing (pFrame,
                                         user,
                                         node.dei_setup,
                                         qbi)))
    return false;

  return QueuePreparedNode (pFrame, qbi, &node);
}


void
CDEIVideoPipeV2::UpdateHW (bool          isDisplayInterlaced,
                           bool          isTopFieldOnDisplay,
                           const TIME64 &vsyncTime)
{
  DENTRYn (9);

  CDEIVideoPipe::UpdateHW (isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);

  ULONG width;
  const struct IQISetup *iqi_setup;
  if (LIKELY (m_pendingNode.isValid))
    {
      const struct DEIVideoPipeV2Setup * const nextMVPnode
        = reinterpret_cast<struct DEIVideoPipeV2Setup *> (m_pendingNode.dma_area.pData);
      iqi_setup = &nextMVPnode->iqi_setup;

      width     = (nextMVPnode->dei_setup.TARGET_SIZE >> 0) & 0x7ff;
    }
  else
    iqi_setup = 0, width = 0;

  m_IQI->UpdateHW (iqi_setup, width);

  DEXITn (9);
}


stm_plane_caps_t
CDEIVideoPipeV2::GetCapabilities (void) const
{
  DENTRYn (4);

  stm_plane_caps_t caps = CDEIVideoPipe::GetCapabilities ();
  caps.ulControls |= m_IQI->GetCapabilities ();

  return caps;
}

bool
CDEIVideoPipeV2::SetControl (stm_plane_ctrl_t control,
                             ULONG            value)
{
  bool retval;

  DENTRYn (8);

  switch (control)
    {
    case PLANE_CTRL_IQI_FIRST ... PLANE_CTRL_IQI_LAST:
      retval = m_IQI->SetControl (control, value);
      break;

    case PLANE_CTRL_DEI_CTRLREG:
      /* FIXME: remove, as it's only a hack for easier debugging. */
      m_nDEICtlSetupMotionSD = value;
      m_nDEICtlSetupMotionHD = value | C2_DEI_CTL_REDUCE_MOTION;
      retval = true;
      break;

    case PLANE_CTRL_PSI_SATURATION:
      /* if the IQI implements PSI controls (why wouldn't it, though), use it
         instead of the video plug for saturation, to prevent artefacts
         occuring on the latter! */
      if (m_IQI->GetCapabilities() & PLANE_CTRL_CAPS_PSI_CONTROLS)
        {
          retval = m_IQI->SetControl (control, value);
          break;
        }
      /* otherwise fall through */

    default:
      retval = CDEIVideoPipe::SetControl (control, value);
      break;
    }

  return retval;
}


bool
CDEIVideoPipeV2::GetControl (stm_plane_ctrl_t  control,
                             ULONG            *value) const
{
  bool retval;

  DENTRYn (4);

  switch (control)
    {
    case PLANE_CTRL_IQI_FIRST ... PLANE_CTRL_IQI_LAST:
      retval = m_IQI->GetControl (control, value);
      break;

    case PLANE_CTRL_PSI_SATURATION:
      /* if the IQI implements PSI controls (why wouldn't it, though), use it
         instead of the video plug for saturation, to prevent artefacts
         occuring on the latter! */
      if (m_IQI->GetCapabilities() & PLANE_CTRL_CAPS_PSI_CONTROLS)
        {
          retval = m_IQI->GetControl (control, value);
          break;
        }
      /* otherwise fall through */

    default:
      retval = CDEIVideoPipe::GetControl (control, value);
      break;
    }

  return retval;
}
