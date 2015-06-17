/***********************************************************************
 *
 * File: soc/sti7200/sti7200flexvp.h
 * Copyright (c) 2007/2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STB7200_FLEXVP_H
#define _STB7200_FLEXVP_H


#include <Generic/Output.h>
#include <Gamma/GammaCompositorVideoPlane.h>

class CSTi7200Cut2MainVideoPipe;
class CSTi7200xVP;



/*
 * The 7200 FlexVP has a 7200 main video pipe and an xVP.
 */
class CSTi7200FlexVP: public CGammaCompositorVideoPlane
{
friend class CSTi7200xVP;
public:
  CSTi7200FlexVP  (stm_plane_id_t   GDPid,
                   CGammaVideoPlug *plug,
                   ULONG            baseAddr);
  virtual ~CSTi7200FlexVP (void);

  virtual bool Create (void);

  virtual void UpdateHW (bool          isDisplayInterlaced,
                         bool          isTopFieldOnDisplay,
                         const TIME64 &vsyncTime);
  virtual bool QueueBuffer (const stm_display_buffer_t * const pFrame,
                            const void                 * const user);

  virtual stm_plane_caps_t GetCapabilities(void) const;

  virtual bool SetControl (stm_plane_ctrl_t control, ULONG  value);
  virtual bool GetControl (stm_plane_ctrl_t control, ULONG *value) const;

  virtual bool Hide (void);
  virtual bool Show (void);

  virtual bool ConnectToOutput      (COutput *pOutput);
  virtual void DisconnectFromOutput (COutput *pOutput);

  virtual void Pause  (bool bFlushQueue);
  virtual void Resume (void);
  virtual void Flush  (void);

  virtual bool LockUse (void *user);
  virtual void Unlock  (void *user);

protected:
  virtual void EnableHW  (void);
  virtual void DisableHW (void);

private:
  CSTi7200Cut2MainVideoPipe *m_mainVideo;
  CSTi7200xVP               *m_xVP;

  ULONG m_FlexVPAddress;

  /* CSTi7200xVP needs these */
  const stm_plane_node *GetPreviousNode  (void) const { return &m_previousNode; }
  const stm_plane_node *GetCurrentNode   (void) const { return &m_currentNode; }
  const stm_plane_node *GetReaderNode    (void) const { return &m_pNodeList[m_readerPos]; }


  void  WriteFlexVPReg (ULONG reg,
                        ULONG value) { DEBUGF2 (9, ("w %p <- %#.8lx\n",
                                                    (volatile ULONG *) m_FlexVPAddress + (reg>>2),
                                                    value));
                                       g_pIOS->WriteRegister ((volatile ULONG *) m_FlexVPAddress + (reg>>2),
                                                              value); }
  ULONG ReadFlexVPReg (ULONG reg)    { const ULONG value = g_pIOS->ReadRegister ((volatile ULONG *) m_FlexVPAddress + (reg>>2));
                                       DEBUGF2 (9, ("r %p -> %#.8lx\n",
                                                    (volatile ULONG *) m_FlexVPAddress + (reg>>2),
                                                    value));
                                       return value; }
};


#endif // _STB7200_FLEXVP_H
