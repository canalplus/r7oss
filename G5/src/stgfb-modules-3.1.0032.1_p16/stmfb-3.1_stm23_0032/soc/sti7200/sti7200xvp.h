/***********************************************************************
 *
 * File: soc/sti7200/sti7200xvp.h
 * Copyright (c) 2007/2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI7200_XVP_H
#define _STI7200_XVP_H


#include <Gamma/GammaCompositorPlane.h>


enum XVPState {
  XVPSTATE_IDLE,
  XVPSTATE_RUNNING,
  XVPSTATE_FWUPLOAD_PREP,
  XVPSTATE_FWUPLOAD
};


enum TNRFrames {
  TNR_FRAME_DEST,    /* destination for next field (where to put new filtered field) */
  TNR_FRAME_PREV,    /* previous (filtered) field */
  TNR_FRAME_PREPREV, /* pre-previous (filtered) field */

  TNR_FRAME_COUNT
};


struct XVPFieldSetup {
  ULONG tnr_field[TNR_FRAME_COUNT]; /* physical address of filtered fields */
  ULONG lumaZoom;
  ULONG vpo;
  ULONG vps;
  ULONG TNRflags;

  ULONG origVideoBuffer;

  bool  bComplete; /* so we do the updateTNR only once */
};

struct XVPSetup {
  enum  PlaneCtrlxVPConfiguration eMode;
  ULONG stride;       /* length in bytes of one line */
  ULONG pictureSize;  /* height << 16 | width */
  ULONG flags;        /* TNR new sequence */

  struct XVPFieldSetup topField;
  struct XVPFieldSetup botField;
  struct XVPFieldSetup altTopField;
  struct XVPFieldSetup altBotField;
};

struct XVP_QUEUE_BUFFER_INFO {
  ULONG tViewPortOrigin;
  ULONG tViewPortSize;
  ULONG tVHSRC_LUMA_VSRC;
  ULONG bViewPortOrigin;
  ULONG bViewPortSize;
  ULONG bVHSRC_LUMA_VSRC;
  ULONG atViewPortOrigin;
  ULONG atViewPortSize;
  ULONG atVHSRC_LUMA_VSRC;
  ULONG abViewPortOrigin;
  ULONG abViewPortSize;
  ULONG abVHSRC_LUMA_VSRC;
  ULONG cVHSRC_LUMA_HSRC;
};

class CSTi7200FlexVP;


class CSTi7200xVP
{
public:
  CSTi7200xVP  (const CSTi7200FlexVP * const flexvp, ULONG baseAddr);
  ~CSTi7200xVP (void);

  void Create (void);

  bool m_bUseTNR;

  ULONG GetCapabilities (void) const; /* PLANE_CTRL_CAPS_TNR etc */

  bool SetControl (stm_plane_ctrl_t control,
                   ULONG            value);
  bool GetControl (stm_plane_ctrl_t  control,
                   ULONG            *value) const;

  bool CanHandleBuffer (const stm_display_buffer_t * const pFrame,
                        struct XVPSetup            * const setup) const;
  bool QueueBuffer1 (stm_display_buffer_t * const pFrame,
                     struct XVPSetup      * const setup);
  bool QueueBuffer2 (const stm_display_buffer_t         * const pFrame,
                     struct XVPSetup                    * const setup,
                     const struct XVP_QUEUE_BUFFER_INFO * const qbi) const;
  void UpdateHW    (struct XVPSetup      * const setup,
                    struct XVPFieldSetup * const field);


  struct XVPFieldSetup *
    SelectFieldSetup (bool                 isTopFieldOnDisplay,
                      struct XVPSetup     * const setup,
                      bool                 bIsPaused,
                      stm_plane_node_type  nodeType,
                      bool                 bUseAltFields) const;

private:
  ULONG m_xVPAddress;

  const CSTi7200FlexVP *m_FlexVP;

  unsigned char m_NLE_Override;
  unsigned long NLE_DUMP[270];
  unsigned char m_CurrentNleIdx;

  DMA_Area      m_TNRMailbox[2];
  unsigned char m_CurrentTNRMailbox;
  DMA_Area      m_TNRFilteredFrame[TNR_FRAME_COUNT];
  unsigned char m_TNRFieldN;

  const STMFirmware *m_imem;
  const STMFirmware *m_dmem;

  enum XVPState m_eState;

  enum PlaneCtrlxVPConfiguration m_eMode;

  void cluster_init (void);

  void xp70init     (void);
  void xp70deinit   (void);

  void ReleaseFirmwares          (void);
  bool xp70PrepareFirmwareUpload (enum PlaneCtrlxVPConfiguration newmode);

  void PreviousTNRFieldSetup    (struct XVPSetup * const setup,
                                 bool             isTopNode) const;
  void PrePreviousTNRFieldSetup (struct XVPSetup * const setup,
                                 bool             isTopNode) const;
  void UpdateTNRFields (struct XVPSetup      * const setup,
                        struct XVPFieldSetup * const field) const;
  void UpdateHWTNRHandler (struct XVPSetup      * const setup,
                           struct XVPFieldSetup * const field,
                           ULONG                 extra_flags);

  void  WriteXVPReg (ULONG reg,
                     ULONG value) { DEBUGF2 (9, ("w %p <- %#.8lx\n",
                                                 (volatile ULONG *) m_xVPAddress + (reg>>2),
                                                 value));
                                    g_pIOS->WriteRegister ((volatile ULONG *) m_xVPAddress + (reg>>2),
                                                           value); }
  ULONG ReadXVPReg (ULONG reg)    { const ULONG value = g_pIOS->ReadRegister ((volatile ULONG *) m_xVPAddress + (reg>>2));
                                    DEBUGF2 (9, ("r %p -> %#.8lx\n",
                                                 (volatile ULONG *) m_xVPAddress + (reg>>2),
                                                 value));
                                    return value; }
};

#endif /* _STI7200_XVP_H */
