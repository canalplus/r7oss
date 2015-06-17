/***********************************************************************
 *
 * File: soc/sti7200/sti7200flexvp.cpp
 * Copyright (c) 2007/2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7200flexvp.h"
#include "sti7200xvp.h"
#include "sti7200mainvideopipe.h"

/* VDP */
#define FVP_VDP_T3I_OFFSET    0x00006000
/* DEI registers */
#define FLEXVP_VDP_DEI_OFFSET (FVP_VDP_T3I_OFFSET + 0)
/* IQI */
#define FLEXVP_VDP_IQI_OFFSET (FVP_VDP_T3I_OFFSET + 0x200)
/* AUX Plug register */
#define FVP_AUX_CONF          (FVP_VDP_T3I_OFFSET + 0x500)
#  define VDP_CONF_T3I_EN     (1L << 0)
#  define VDP_CONF_WR_Q_EN    (1L << 1)
#  define VDP_CONF_Q_Y_SIGNED (1L << 2)
#  define VDP_CONF_Q_C_SIGNED (1L << 3)
#  define VDP_CONF_KEYING_EN  (1L << 4)

enum {
  FVP_PICTURE_ONLY      = VDP_CONF_T3I_EN,  /* display only picture (from mem) */
  FVP_GRAIN_ONLY        = VDP_CONF_WR_Q_EN, /* display grain only (from WR queue) */
  FVP_PICTURE_AND_GRAIN = VDP_CONF_T3I_EN | VDP_CONF_WR_Q_EN /* picture and film grain */
};






CSTi7200FlexVP::CSTi7200FlexVP (stm_plane_id_t   GDPid,
                                CGammaVideoPlug *plug,
                                ULONG            baseAddr): CGammaCompositorVideoPlane (GDPid,
                                                                                        plug,
                                                                                        baseAddr + FLEXVP_VDP_DEI_OFFSET)
{
  DEBUGF2 (4, (FENTRY ", planeID: %d, baseAddr: %lx/%lx\n",
               __PRETTY_FUNCTION__, GDPid, m_baseAddress, baseAddr));

  m_FlexVPAddress = baseAddr;

  /*
   * The following changes the behaviour of the generic buffer queue update
   * in GammaCompositorPlane to hold on to buffers for an extra update
   * period, so that previously displayed buffers can still be used by the
   * de-interlacer.
   */
  m_keepHistoricBufferForDeinterlacing = true;

  /*
   * Ensure pixels are read from memory interface
   */
  WriteFlexVPReg (FVP_AUX_CONF, FVP_PICTURE_ONLY);

  m_mainVideo = 0;
  m_xVP       = 0;

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}


CSTi7200FlexVP::~CSTi7200FlexVP (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  delete (m_mainVideo);
  delete (m_xVP);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}


bool
CSTi7200FlexVP::Create (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /*
   * Call the top level base class Create() directly instead of
   * CGammaCompositorVideoPlane::Create(), because we do not want the
   * rescale filter memory allocated for this class.
   */
  if(!CDisplayPlane::Create())
    return false;

  m_xVP = new CSTi7200xVP (this, m_FlexVPAddress);
  if (!m_xVP)
  {
    DEBUGF (("%s - failed to create xVP\n", __PRETTY_FUNCTION__));
    return false;
  }
  m_xVP->Create ();
  DEBUGF2 (4, ("%s: m_xVP is @ %p\n", __PRETTY_FUNCTION__, m_xVP));

  /*
   * We do the DEI second, because it sets up some registers with constant
   * values. But the creation of the xVP results in a reset of the entire block
   * which includes the DEI and that would clear those registers again.
   */
  m_mainVideo = new CSTi7200Cut2MainVideoPipe (m_planeID,
                                               m_videoPlug,
                                               m_FlexVPAddress + FLEXVP_VDP_DEI_OFFSET);
  if (!m_mainVideo || !m_mainVideo->Create ())
    {
      DEBUGF (("%s - failed to create MainVideoPipe\n", __PRETTY_FUNCTION__));
      delete (m_xVP);
      m_xVP = 0;
      delete (m_mainVideo);
      m_mainVideo = 0;
      return false;
    }

  m_nFormats = m_mainVideo->GetFormats (&m_pSurfaceFormats);

  DEBUGF2 (4, ("%s: m_MainVideo is @ %p\n", __PRETTY_FUNCTION__, m_mainVideo));

  return true;
}




bool
CSTi7200FlexVP::QueueBuffer (const stm_display_buffer_t * const pFrame,
                             const void                 * const user)
{
  struct GAMMA_QUEUE_BUFFER_INFO    qbi;
  struct DEIVideoPipeV2Setup        displayNodeMain = { dei_setup : { 0 } };
  struct XVPSetup                   displayNodeFvp;
  bool                              error;
  bool                              xvp_can_handle;

  stm_display_buffer_t *pWorkFrame =
    const_cast <stm_display_buffer_t *> (pFrame);
  /* original params, xVP will change them (if xVP is in use) */
  ULONG orig_src = pFrame->src.ulVideoBufferAddr;
  ULONG orig_flags = pFrame->src.ulFlags;

//g_pIDebug->IDebugPrintf ("sizeof b/q/m/f: %u/%u/%u/%u\n",
//                         sizeof (stm_display_buffer_t),
//                         sizeof (struct GAMMA_QUEUE_BUFFER_INFO),
//                         sizeof (struct STi7xxxMainVideoPipeSetup),
//                         sizeof (struct XVPSetup));

  DEBUGF2 (6, ("+++++++++++++++++++++++++\n"));
  DEBUGF2 (5, ("flex (%p) queue: src addr/size/flg: %.8lx/%lu/%lx  @  %llu\n",
               this,
               pFrame->src.ulVideoBufferAddr, pFrame->src.ulVideoBufferSize,
               pFrame->src.ulFlags, pFrame->info.presentationTime));

  xvp_can_handle = m_xVP->CanHandleBuffer (pWorkFrame, &displayNodeFvp);
  if (xvp_can_handle)
    {
//      typeof (buffer.info.presentationTime) _pres_time = buffer.info.presentationTime;
//      if (LIKELY (_pres_time > 2*m_pOutput->GetFieldOrFrameDuration ()))
//        _pres_time -= (2*m_pOutput->GetFieldOrFrameDuration ());
//      g_pIDebug->IDebugPrintf ("\nqueueing for display @ %llu\n",
//                               _pres_time);
      if (LIKELY (m_xVP->QueueBuffer1 (pWorkFrame, &displayNodeFvp)))
        /* if xVP can handle this buffer, it now changed some attibutes, so
           we will use the new ones. */
        ;
    }

  if (UNLIKELY (!m_mainVideo->PrepareNodeForQueueing (pWorkFrame,
                                                      user,
                                                      displayNodeMain.dei_setup,
                                                      qbi)))
    return false;

  if (xvp_can_handle)
    {
      /* Inventing a new data struct here is a bit ugly, but I don't want the
         xVP code to depend on the DEI... */
      struct XVP_QUEUE_BUFFER_INFO xqbi = {
        tViewPortOrigin  : displayNodeMain.dei_setup.topField.LUMA_XY,
        tViewPortSize    : displayNodeMain.dei_setup.topField.DEIVP_SIZE,
        tVHSRC_LUMA_VSRC : displayNodeMain.dei_setup.topField.LUMA_VSRC,
        bViewPortOrigin  : displayNodeMain.dei_setup.botField.LUMA_XY,
        bViewPortSize    : displayNodeMain.dei_setup.botField.DEIVP_SIZE,
        bVHSRC_LUMA_VSRC : displayNodeMain.dei_setup.botField.LUMA_VSRC,
        atViewPortOrigin : displayNodeMain.dei_setup.altTopField.LUMA_XY,
        atViewPortSize   : displayNodeMain.dei_setup.altTopField.DEIVP_SIZE,
        atVHSRC_LUMA_VSRC : displayNodeMain.dei_setup.altTopField.LUMA_VSRC,
        abViewPortOrigin : displayNodeMain.dei_setup.altBotField.LUMA_XY,
        abViewPortSize   : displayNodeMain.dei_setup.altBotField.DEIVP_SIZE,
        abVHSRC_LUMA_VSRC : displayNodeMain.dei_setup.altBotField.LUMA_VSRC,
        cVHSRC_LUMA_HSRC  : displayNodeMain.dei_setup.LUMA_HSRC
      };
      if (UNLIKELY (!m_xVP->QueueBuffer2 (pWorkFrame,
                                          &displayNodeFvp,
                                          &xqbi)))
        {
          /* FIXME: could do some cleanup here and just continue! But then,
             shouldn't ever really happen. QueueBuffer2() can only fail if
             the xVP's working mode is changed between CanHandleBuffer() and
             here, but since we are globally locked from
             DisplayPlane.cpp:queue_buffer(), it will never do.*/
          return false;
        }
    }

  /* now queue the buffer (for real). */
  error = !m_mainVideo->QueuePreparedNode (pWorkFrame,
                                           qbi,
                                           &displayNodeMain);

  if (UNLIKELY (error))
    DEBUGF2 (5, ("+++++++++++++++++++++++++ FAIL4!!!!!!!!!\n"));

  /* for our own use of the buffer, we explicitly clear the callback data
     because we don't want the callback to be called after having xVP'd
     (TNR/FGT) the frame. Only after really having it on display, which is
     being taken care of by m_mainVideo. */
  qbi.presentation_info.pCompletedCallback = 0;
  qbi.presentation_info.pDisplayCallback   = 0;
  if (LIKELY (qbi.presentation_info.presentationTime > 2*m_pOutput->GetFieldOrFrameDuration ()))
    qbi.presentation_info.presentationTime -= (2*m_pOutput->GetFieldOrFrameDuration ());

  if (!qbi.isSourceInterlaced)
    error |= !QueueProgressiveContent (&displayNodeFvp,
                                       sizeof (displayNodeFvp),
                                       qbi);
  else if (qbi.firstFieldOnly
           || (qbi.isDisplayInterlaced && !qbi.interpolateFields))
    error |= !QueueSimpleInterlacedContent (&displayNodeFvp,
                                            sizeof (displayNodeFvp),
                                            qbi);
  else
    {
      error |= !QueueInterpolatedInterlacedContent (&displayNodeFvp,
                                                    sizeof (displayNodeFvp),
                                                    qbi);
    }

  /* restore settings */
  pWorkFrame->src.ulFlags = orig_flags;
  pWorkFrame->src.ulVideoBufferAddr = orig_src;

  /* something went totally wrong */
  if (UNLIKELY (error))
    return false;

  DEBUGF2 (6, ("+++++++++++++++++++++++++\n"));
  DEBUGF2 (5, ("\n"));

  return !error;
}

void
CSTi7200FlexVP::UpdateHW (bool          isDisplayInterlaced,
                          bool          isTopFieldOnDisplay,
                          const TIME64 &vsyncTime)
{
  struct XVPSetup       *nextfvpsetup = 0;
  struct XVPFieldSetup  *nextfieldsetup = 0;
  static stm_plane_node  nextNode;

  DEBUGF2 (9, ("\n==========================\n"));
  DEBUGF2 (9, (FENTRY " @ %llu\n", __PRETTY_FUNCTION__, vsyncTime));

//  g_pIDebug->IDebugPrintf ("\nVSync before %s @ %llu\n",
//                           isTopFieldOnDisplay ? "bottom" : "top", vsyncTime);

  UpdateCurrentNode (vsyncTime);

  if (isDisplayInterlaced && m_currentNode.isValid)
    {
      nextfvpsetup = (struct XVPSetup *) m_currentNode.dma_area.pData;
      nextfieldsetup = m_xVP->SelectFieldSetup (isTopFieldOnDisplay,
                                                nextfvpsetup,
                                                m_isPaused,
                                                m_currentNode.nodeType,
                                                m_currentNode.useAltFields);
    }

  if (m_isPaused
      || (m_currentNode.isValid && m_currentNode.info.nfields > 1))
    {
      /*
       * We have got a node on display that is going to be there for more than
       * one frame/field, so we do not get another from the queue yet.
       */
      if (m_isPaused)
        DEBUGF2 (9, ("  fvp paused\n"));
      else
        DEBUGF2 (9, ("  fvp current for %d fields\n", m_currentNode.info.nfields));

      goto write_next_field;
    }

  if (PeekNextNodeFromDisplayList (vsyncTime, nextNode))
    {
      DEBUGF2 (9, ("  fvp next @ %llu\n", nextNode.info.presentationTime));
      if (isDisplayInterlaced)
        {
          if (!nextNode.useAltFields)
            {
              if ((isTopFieldOnDisplay
                   && nextNode.nodeType == GNODE_TOP_FIELD)
                  || (!isTopFieldOnDisplay
                      && nextNode.nodeType == GNODE_BOTTOM_FIELD))
                {
                  DEBUGF2 (1, ("  fvp Waiting for correct field: "
                               "isTopFieldOnDisplay: %s "
                               "nextNode.Type: %s\n",
                               isTopFieldOnDisplay ? "true" : "false",
                               (nextNode.nodeType == GNODE_TOP_FIELD) ? "top" : "bottom"));

                  goto write_next_field;
                }
              DEBUGF2 (9, ("  fvp no alt: disp/next: %s/%s \n",
                       isTopFieldOnDisplay ? "top" : "bot",
                       (nextNode.nodeType == GNODE_TOP_FIELD) ? "top" : "bot"));
            }
          else
            DEBUGF2 (9, ("  using alt\n"));

          nextfvpsetup = (struct XVPSetup *) nextNode.dma_area.pData;
          nextfieldsetup = m_xVP->SelectFieldSetup (isTopFieldOnDisplay,
                                                    nextfvpsetup,
                                                    m_isPaused,
                                                    nextNode.nodeType,
                                                    nextNode.useAltFields);
        }
      else
        {
          nextfvpsetup = (struct XVPSetup *) nextNode.dma_area.pData;

          if (nextNode.nodeType == GNODE_PROGRESSIVE
              || nextNode.nodeType == GNODE_TOP_FIELD)
            nextfieldsetup = &nextfvpsetup->topField;
          else
            nextfieldsetup = &nextfvpsetup->botField;
        }

      m_xVP->UpdateHW (nextfvpsetup, nextfieldsetup);
      /* so we write only once */
      nextfieldsetup = 0;

      /* we don't CGammaCompositorVideoPlane::EnableHW () here, because that
         is done by m_mainVideo->UpdateHW () if necessary. It works on the
         same HW plane, so the results will be as expected. We make sure our
         own notion of m_isEnabled is up to date after m_mainVideo->UpdateHW()
         has returned. */

      SetPendingNode (nextNode);
      PopNextNodeFromDisplayList ();
    }
  else
    DEBUGF2 (7, ("  fvp no next\n"));

write_next_field:
  if (nextfieldsetup)
    m_xVP->UpdateHW (nextfvpsetup, nextfieldsetup);

  m_mainVideo->UpdateHW (isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);

  m_isEnabled = m_mainVideo->isEnabled();

  DEBUGF2 (9, ("==========================\n\n"));
  DEBUGF2 (7, ("\n"));
}





stm_plane_caps_t
CSTi7200FlexVP::GetCapabilities (void) const
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

  stm_plane_caps_t caps = m_mainVideo->GetCapabilities ();
  caps.ulControls |= m_xVP->GetCapabilities ();

  return caps;
}

bool
CSTi7200FlexVP::SetControl (stm_plane_ctrl_t control,
                            ULONG            value)
{
  bool retval;

  DEBUGF2 (8, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_XVP_FIRST ... PLANE_CTRL_XVP_LAST:
      retval = m_xVP->SetControl (control, value);
      break;

    default:
      retval = m_mainVideo->SetControl (control, value);
      break;
    }

  return retval;
}


bool
CSTi7200FlexVP::GetControl (stm_plane_ctrl_t  control,
                            ULONG            *value) const
{
  bool retval;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_XVP_FIRST ... PLANE_CTRL_XVP_LAST:
      retval = m_xVP->GetControl (control, value);
      break;

    default:
      retval = m_mainVideo->GetControl (control, value);
      break;
    }

  return retval;
}


bool
CSTi7200FlexVP::Hide (void)
{
  bool success;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  success = m_mainVideo->Hide ();
  m_ulStatus = m_mainVideo->GetStatus ();

  DEBUGF2 (4, (FEXIT ", success: %c\n",
               __PRETTY_FUNCTION__, success ? 'y' : 'n'));

  return success;
}

bool
CSTi7200FlexVP::Show (void)
{
  bool success;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  success = m_mainVideo->Show ();
  m_ulStatus = m_mainVideo->GetStatus ();

  DEBUGF2 (4, (FEXIT ", success: %c\n",
               __PRETTY_FUNCTION__, success ? 'y' : 'n'));

  return success;
}

// Video Display Processor private implementation

bool
CSTi7200FlexVP::ConnectToOutput (COutput *pOutput)
{
  bool success1, success2 = false;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  if ((success1 = CGammaCompositorVideoPlane::ConnectToOutput (pOutput))
      && ((success2 = m_mainVideo->ConnectToOutput (pOutput)) == false))
    {
      DEBUGF2 (2, ("%s: failed (Gamma/mainVideo: %sok/%sok)\n",
                   __PRETTY_FUNCTION__,
                   success1 ? "" : "n",
                   success2 ? "" : "n"));
      CGammaCompositorVideoPlane::DisconnectFromOutput (pOutput);
    }

  DEBUGF2 (4, (FEXIT ", success: %c\n",
               __PRETTY_FUNCTION__, success2 ? 'y' : 'n'));

  return success2;
}

void
CSTi7200FlexVP::DisconnectFromOutput (COutput *pOutput)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  CGammaCompositorVideoPlane::DisconnectFromOutput (pOutput);
  m_mainVideo->DisconnectFromOutput (pOutput);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

void
CSTi7200FlexVP::Pause (bool bFlushQueue)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  CGammaCompositorVideoPlane::Pause (bFlushQueue);
  m_mainVideo->Pause (bFlushQueue);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

void
CSTi7200FlexVP::Resume (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  CGammaCompositorVideoPlane::Resume ();
  m_mainVideo->Resume ();

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

void
CSTi7200FlexVP::Flush (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  CGammaCompositorVideoPlane::Flush ();
  m_mainVideo->Flush ();

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

void
CSTi7200FlexVP::EnableHW (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  m_mainVideo->EnableHW ();
  m_ulStatus = m_mainVideo->GetStatus ();
  m_isEnabled = m_mainVideo->isEnabled();

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

void
CSTi7200FlexVP::DisableHW (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  m_mainVideo->DisableHW ();
  m_ulStatus = m_mainVideo->GetStatus ();
  m_isEnabled = m_mainVideo->isEnabled();

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}

bool
CSTi7200FlexVP::LockUse (void *user)
{
  bool success1, success2 = false;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  if ((success1 = CGammaCompositorVideoPlane::LockUse (user))
      && ((success2 = m_mainVideo->LockUse (user)) == false))
    {
      DEBUGF2 (2, ("%s: failed (Gamma/mainVideo: %sok/%sok)\n",
                   __PRETTY_FUNCTION__,
                   success1 ? "" : "n",
                   success2 ? "" : "n"));
      CGammaCompositorVideoPlane::Unlock (user);
    }

  DEBUGF2 (4, (FEXIT ", success: %c\n",
               __PRETTY_FUNCTION__, success2 ? 'y' : 'n'));

  return success2;
}

void
CSTi7200FlexVP::Unlock  (void *user)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  CGammaCompositorVideoPlane::Unlock (user);
  m_mainVideo->Unlock (user);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}
