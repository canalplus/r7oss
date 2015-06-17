/***********************************************************************
 *
 * File: Gamma/GammaCompositorPlane.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GAMMA_COMPOSITOR_PLANE_H
#define GAMMA_COMPOSITOR_PLANE_H

#include <Generic/DisplayPlane.h>

///////////////////////////////////////////////////////////////////////////////
// Base class for compositor hardware planes, GDP, VidPLUG, Cursor and ALPHA

/*
 * The following structure is a "memo" carried around during
 * a QueueBuffer operation. A lot of information and basic
 * setup logic can be shared regardless of the underlying
 * hardware implementation.
 */
struct GAMMA_QUEUE_BUFFER_INFO
{
  const stm_mode_line_t* pCurrentMode;

  stm_plane_crop_t src;
  stm_plane_crop_t dst;

  stm_buffer_presentation_t presentation_info;

  /*
   * Basic information from system and buffer presentation flags
   */
  bool isDisplayInterlaced;
  bool isSourceInterlaced;

  stm_plane_node_type firstFieldType;

  bool repeatFirstField;
  bool firstFieldOnly;
  bool interpolateFields;
  bool isHWDeinterlacing;
  bool isUsingProgressive2InterlacedHW;

  /*
   * Useful memory format
   */
  bool isSrcMacroBlock;
  bool isSrc420;
  bool isSrc422;

  /*
   * Scaling information conversion increments
   */
  unsigned long verticalFilterInputSamples;
  unsigned long verticalFilterOutputSamples;
  unsigned long horizontalFilterOutputSamples;
  unsigned long srcFrameHeight;
  unsigned long dstFrameHeight;
  int           maxYCoordinate;

  unsigned long hsrcinc;
  unsigned long chroma_hsrcinc;
  unsigned long vsrcinc;
  unsigned long chroma_vsrcinc;
  unsigned long line_step;

  /*
   * Non-linear scaling
   */
  unsigned long nlLumaStartInc;
  unsigned long nlChromaStartInc;
  unsigned long pdeltaLuma;
  unsigned long nlZone1;
  unsigned long nlZone2;

  /*
   * Destination Viewport
   */
  unsigned long viewportStartPixel;
  unsigned long viewportStopPixel;
  unsigned long viewportStartLine;
  unsigned long viewportStopLine;
};


class CGammaCompositorPlane: public CDisplayPlane
{
public:
    CGammaCompositorPlane(stm_plane_id_t GDPid);
    virtual ~CGammaCompositorPlane();

    virtual void UpdateHW (bool isdisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const TIME64 &vsyncTime) = 0;

    int   GetFormats(const SURF_FMT** pData) const;

    virtual bool SetControl(stm_plane_ctrl_t control, ULONG value);
    virtual bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

    virtual stm_plane_caps_t GetCapabilities(void) const;

protected:
    // Display plane surface format capabilities
    const SURF_FMT*  m_pSurfaceFormats;
    ULONG            m_nFormats;
    stm_plane_caps_t m_capabilities;
    bool             m_bHasNonLinearZoom;
    bool             m_bHasProgressive2InterlacedHW;

    /*
     * Common helper functions to setup queue buffer hardware
     * programming for different combinations of interlaced/progressive
     * source and display.
     */
    bool GetQueueBufferInfo(const stm_display_buffer_t * const pFrame,
                            const void                 * const user,
                            GAMMA_QUEUE_BUFFER_INFO           &qbinfo) const;

    void AdjustBufferInfoForScaling(const stm_display_buffer_t * const pFrame,
                                    GAMMA_QUEUE_BUFFER_INFO           &qbinfo) const;

    ULONG PackRegister(USHORT hi,USHORT lo) const { return (hi<<16) | lo; }

    int ScaleVerticalSamplePosition(int pos, const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const
    {
      /*
       * This function scales a source vertical sample position by the true
       * vertical scale factor between source and destination, ignoring any
       * scaling to convert between interlaced and progressive sources and
       * destinations. It is used to work out the starting sample position for
       * the first line on the destination, particularly for the start of a
       * bottom display field.
       */
      return (pos * qbinfo.srcFrameHeight) / qbinfo.dstFrameHeight;
    }

    /*
     * This helper function does the common work required to calculate the
     * setup for a 5-tap vertical filter.
     */
    void CalculateVerticalFilterSetup (const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                                       int                            firstSampleOffset,
                                       bool                           doLumaFilter,
                                       int                           &y,
                                       int                           &phase,
                                       int                           &repeat,
                                       int                           &height) const;

    bool QueueSimpleInterlacedContent(void                    *hwSetup,
                                      ULONG                    hwSetupSize,
                                const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    bool QueueInterpolatedInterlacedContent(void                    *hwSetup,
                                            ULONG                    hwSetupSize,
                                      const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    bool QueueProgressiveContent(void                    *hwSetup,
                                 ULONG                    hwSetupSize,
                           const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

private:
    bool m_bUsePiagetAppNoteEquations;

    void CalculateViewport(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;
    void CalculateHorizontalScaling(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;
    void CalculateVerticalScaling(GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void CalculateNonLinearZoom(const stm_display_buffer_t * const pFrame,
                                GAMMA_QUEUE_BUFFER_INFO           &qbinfo) const;
};

#endif
