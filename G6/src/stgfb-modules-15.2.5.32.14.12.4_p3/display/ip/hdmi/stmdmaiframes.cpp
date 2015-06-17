/***********************************************************************
 *
 * File: display/ip/hdmi/stmdmaiframes.cpp
 * Copyright (c) 2008-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>
#include <display/generic/MetaDataQueue.h>

#include "stmhdmi.h"
#include "stmhdmiregs.h"
#include "stmdmaiframes.h"

CSTmDMAIFrames::CSTmDMAIFrames(CDisplayDevice *pDev,uint32_t uHDMIOffset,uint32_t uPhysRegisterBase): CSTmCPUIFrames(pDev,uHDMIOffset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_BaseAddress = uPhysRegisterBase+uHDMIOffset;

  m_DMAChannel = 0;
  m_DMAHandle  = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmDMAIFrames::~CSTmDMAIFrames(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_stop_dma_channel(m_DMAChannel);
  vibe_os_delete_dma_transfer(m_DMAHandle);
  vibe_os_release_dma_channel(m_DMAChannel);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmDMAIFrames::Create(CSTmHDMI *parent, COutput *master)
{
  const uint32_t iframe_size = 9*sizeof(uint32_t);
  DMA_Area registers = {};

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CSTmCPUIFrames::Create(parent,master))
    return false;

  DMA_transfer *transfer_list = new DMA_transfer[m_nSlots+1];

  if(!transfer_list)
  {
    TRC( TRC_ID_ERROR, "unable to allocate temporary transfer list memory" );
    return false;
  }

  /*
   * Create a dummy DMA area for the destination, covering the registers.
   */
  registers.ulPhysical = m_BaseAddress+STM_HDMI_IFRAME_HEAD_WD;
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
  vibe_os_zero_memory(&transfer_list[m_nSlots],sizeof(DMA_transfer));

  m_DMAChannel = vibe_os_get_dma_channel(SDTP_HDMI1_IFRAME, iframe_size, SDTF_NONE);
  if(!m_DMAChannel)
  {
    TRC( TRC_ID_ERROR, "DMA transfers not supported on this platform" );
    return false;
  }

  m_DMAHandle = vibe_os_create_dma_transfer(m_DMAChannel, transfer_list, SDTP_HDMI1_IFRAME, SDTF_NONE);

  delete[] transfer_list;

  if(!m_DMAHandle)
  {
    TRC( TRC_ID_ERROR, "DMA transfers not supported on this platform" );
    return false;
  }


  m_p3DEXTQueue = new CMetaDataQueue(STM_METADATA_TYPE_HDMI_VSIF_3D_EXT,2,0);
  if(!m_p3DEXTQueue || !m_p3DEXTQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create 3D extended data queue" );
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
    TRC( TRC_ID_ERROR, "Unable to create ISRC queue" );
    return false;
  }

  /*
   * It isn't quite clear how much information can be sent from the CEA861
   * spec. Size it for 1s worth at 30 frames/s for the moment.
   */
  m_pNTSCQueue = new CMetaDataQueue(STM_METADATA_TYPE_NTSC_IFRAME,30,0);
  if(!m_pNTSCQueue || !m_pNTSCQueue->Create())
  {
    TRC( TRC_ID_ERROR, "Unable to create NTSC queue" );
    return false;
  }

  /*
   * No colour gamut queue because we cannot ensure the transmission
   * timing requirements with this hardware. However we are not
   * supporting xvYcc colour until the full HDMI 1.3a hardware appears, and
   * that has a completely different IFrame transmission scheme and hardware.
   */

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTmDMAIFrames::GetIFrameCompleteHDMIInterruptMask(void)
{
  /*
   * We do not want to see the completion interrupt with the DMA mechanism,
   * that is the whole point we are off-loading this from the CPU.
   */
  return 0;
}


void CSTmDMAIFrames::SendFirstInfoFrame(void)
{
  /*
   * Only kick off the FDMA transfer if we are in HDMI mode; otherwise the
   * hardware doesn't pull any data from the FDMA and we get a warning on
   * each frame where the StartDMATransfer function spots that the previous
   * transfer didn't complete.
   */
  uint32_t val;
  m_pParent->GetControl(OUTPUT_CTRL_VIDEO_OUT_SELECT, &val);
  if(val & STM_VIDEO_OUT_HDMI)
  {
    vibe_os_flush_dma_area(&m_IFrameSlots, 0, m_IFrameSlots.ulDataSize);

    uint32_t mb = m_slots[m_nSlots-1].enable;
    TRC( TRC_ID_UNCLASSIFIED, "mb = 0x%08lx", mb );

    vibe_os_start_dma_transfer(m_DMAHandle);
  }
}
