/***********************************************************************
 *
 * File: STMCommon/stmdmaiframes.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>
#include <Generic/Output.h>
#include <Generic/MetaDataQueue.h>

#include "stmhdmi.h"
#include "stmhdmiregs.h"
#include "stmdmaiframes.h"

CSTmDMAIFrames::CSTmDMAIFrames(CDisplayDevice *pDev,ULONG ulHDMIOffset,ULONG ulPhysRegisterBase): CSTmCPUIFrames(pDev,ulHDMIOffset)
{
  DENTRY();

  m_ulPhysicalRegisterBaseAddress = ulPhysRegisterBase;

  m_DMAChannel = 0;
  m_DMAHandle  = 0;

  DEXIT();
}


CSTmDMAIFrames::~CSTmDMAIFrames(void)
{
  DENTRY();

  g_pIOS->StopDMAChannel(m_DMAChannel);
  g_pIOS->DeleteDMATransfer(m_DMAHandle);
  g_pIOS->ReleaseDMAChannel(m_DMAChannel);

  DEXIT();
}


bool CSTmDMAIFrames::Create(CSTmHDMI *parent, COutput *master)
{
  const ULONG iframe_size = 9*sizeof(ULONG);
  DMA_Area registers = {};

  DENTRY();

  if(!CSTmCPUIFrames::Create(parent,master))
    return false;

  DMA_transfer *transfer_list = new DMA_transfer[m_nSlots+1];

  if(!transfer_list)
  {
    DEBUGF2(1, ("%s: unable to allocate temporary transfer list memory\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * Create a dummy DMA area for the destination, covering the registers.
   */
  registers.ulPhysical = m_ulPhysicalRegisterBaseAddress+m_ulHDMIOffset+STM_HDMI_IFRAME_HEAD_WD;
  registers.ulDataSize = iframe_size;
  registers.ulFlags    = SDAAF_UNCACHED;

  for(int i=0;i<m_nSlots;i++)
  {
    transfer_list[i].src         = &m_IFrameSlots;
    transfer_list[i].src_offset  = i * iframe_size;
    transfer_list[i].dst         = &registers;
    transfer_list[i].dst_offset  = 0;
    transfer_list[i].size        = iframe_size;
    transfer_list[i].completed_cb= 0;
  };

  /*
   * Terminate list with a zeroed entry.
   */
  g_pIOS->ZeroMemory(&transfer_list[m_nSlots],sizeof(DMA_transfer));

  m_DMAChannel = g_pIOS->GetDMAChannel(SDTP_HDMI1_IFRAME, iframe_size, SDTF_NONE);
  if(!m_DMAChannel)
  {
    DEBUGF2(1, ("%s: DMA transfers not supported on this platform\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_DMAHandle = g_pIOS->CreateDMATransfer(m_DMAChannel, transfer_list, SDTP_HDMI1_IFRAME, SDTF_NONE);

  delete[] transfer_list;

  if(!m_DMAHandle)
  {
    DEBUGF2(1, ("%s: DMA transfers not supported on this platform\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * ISRC sized for 3 packets x 3 audio tracks + 1 packet to disable, which
   * is arbitrary but should be sufficient for dealing with very short
   * introduction tracks.
   */
  m_pISRCQueue = new CMetaDataQueue(STM_METADATA_TYPE_ISRC_DATA,10,0);
  if(!m_pISRCQueue || !m_pISRCQueue->Create())
  {
    DERROR("Unable to create ISRC queue\n");
    return false;
  }

  /*
   * It isn't quite clear how much information can be sent from the CEA861
   * spec. Size it for 1s worth at 30 frames/s for the moment.
   */
  m_pNTSCQueue = new CMetaDataQueue(STM_METADATA_TYPE_NTSC_IFRAME,30,0);
  if(!m_pNTSCQueue || !m_pNTSCQueue->Create())
  {
    DERROR("Unable to create NTSC queue\n");
    return false;
  }

  /*
   * No colour gamut queue because we cannot ensure the transmission
   * timing requirements with this hardware. However we are not
   * supporting xvYcc colour until the full HDMI 1.3a hardware appears, and
   * that has a completely different IFrame transmission scheme and hardware.
   */

  DEXIT();
  return true;
}


ULONG CSTmDMAIFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  /*
   * We do not want to see the completion interrupt with the DMA mechanism,
   * that is the whole point we are off-loading this from the CPU.
   */
  return 0;
}


void CSTmDMAIFrames::SendFirstInfoFrame(void)
{
  g_pIOS->FlushCache(m_slots, m_IFrameSlots.ulDataSize);

  ULONG mb = m_slots[m_nSlots-1].enable;
  DEBUGF2(3,("%s: mb = 0x%08lx\n",__PRETTY_FUNCTION__, mb));

  g_pIOS->StartDMATransfer(m_DMAHandle);
}
