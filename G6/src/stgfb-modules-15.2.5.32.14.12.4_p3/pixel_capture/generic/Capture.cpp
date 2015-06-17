/***********************************************************************
 *
 * File: pixel_capture/generic/Capture.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "CaptureDevice.h"
#include "Capture.h"


CCapture::CCapture(const char                           *name,
                               uint32_t                               id,
                               const CPixelCaptureDevice             *pCaptureDevice,
                               const stm_pixel_capture_capabilities_flags_t   caps)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Create Capture %s with Id = %d", name, id );

  m_name                = name;
  m_ID                  = id;
  m_lock                = 0;
  m_user                = 0;
  m_Capabilities        = caps;
  m_pCaptureDevice      = pCaptureDevice;
  m_pReg                = 0;
  m_ulTimingID          = 0;

  m_isStarted           = false;
  m_hasBuffersToRelease = false;

  m_Status              = 0;

  m_InputWindow.x       = 0;
  m_InputWindow.y       = 0;
  m_InputWindow.width   = 0;
  m_InputWindow.height  = 0;

  m_InputWindowCaps.max_input_window_area.x       = 4092;
  m_InputWindowCaps.max_input_window_area.y       = 2048;
  m_InputWindowCaps.max_input_window_area.width   = 4092;
  m_InputWindowCaps.max_input_window_area.height  = 2048;

  m_InputWindowCaps.default_input_window_area.x       = 0;
  m_InputWindowCaps.default_input_window_area.y       = 0;
  m_InputWindowCaps.default_input_window_area.width   = 1280;
  m_InputWindowCaps.default_input_window_area.height  = 720;

  /* setup default capture buffer format */
  m_CaptureFormat = (stm_pixel_capture_buffer_format_t){ 0 };

  m_CaptureFormat.format      = STM_PIXEL_FORMAT_NONE;
  m_CaptureFormat.width       = 1;
  m_CaptureFormat.height      = 1;
  m_CaptureFormat.stride      = 0;
  m_CaptureFormat.color_space = STM_PIXEL_CAPTURE_RGB;

  /*
   * The CAPTURE sample rate converters have an n.8 fixed point format,
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

  m_currentNode  = 0;
  m_pendingNode  = 0;
  m_previousNode = 0;

  m_Sink         = 0;

  m_bIsSuspended = false;
  m_bIsFrozen    = false;

  m_InputParams     = (stm_pixel_capture_input_params_t){ 0 };
  m_StreamParams    = (stm_pixel_capture_params_t) { 0 };
  m_CaptureQueue    = 0;
  m_nFormats        = 0;
  m_pSurfaceFormats = 0;
  m_ulMaxLineStep   = 0;

  m_pCaptureInputs  = 0;
  m_pCurrentInput   = (stm_pixel_capture_inputs_t){ 0 };
  m_numInputs       = 0;

  m_AreInputParamsChanged   = false;
  m_ulCaptureCTL            = CAPTURE_HW_INVALID_CTL;
  m_QueueLock               = 0;

  m_FrameDuration           = 0ull;

  m_EventSubscription       = (stm_event_subscription_h)NULL;
  m_WaitQueueEvent          = (VIBE_OS_WaitQueue_t)NULL;
  m_CaptureEvent.event_id   = 0;
  m_CaptureEvent.object     = (stm_object_h)NULL;
  m_lastPresentationTime    = 0ull;
  vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));
  vibe_os_zero_memory(&m_PushGetInterface, sizeof(stm_data_interface_push_get_sink_t));
  m_NumberOfQueuedBuffer    = 0;

  TRC( TRC_ID_MAIN_INFO, "Capture = %p named %s created", this, name );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CCapture::~CCapture(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * First, call the capture CleanUp routine.
   */
  CleanUp();

  vibe_os_delete_resource_lock(m_lock);

  if(m_CaptureQueue)
  {
    delete m_CaptureQueue;
    m_CaptureQueue = 0;
  }
  TRC( TRC_ID_MAIN_INFO, "Capture %p Destroyed", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CCapture::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_lock = vibe_os_create_resource_lock();
  if(!m_lock)
  {
    TRC( TRC_ID_ERROR, "failed to allocate resource lock" );
    return false;
  }

  m_QueueLock = vibe_os_create_semaphore(1);
  if(!m_QueueLock)
  {
    TRC( TRC_ID_ERROR, "Failed to create Resource Lock!" );
    vibe_os_delete_resource_lock(m_lock);
    return false;
  }

  m_CaptureQueue = new CCaptureQueue(CAPTURE_MAX_NODES_VALUE,true);
  if(!m_CaptureQueue || !m_CaptureQueue->Create())
  {
    TRC( TRC_ID_ERROR, "failed to create capture nodes queue" );
    vibe_os_delete_resource_lock(m_lock);
    vibe_os_delete_semaphore(m_QueueLock);
    return false;
  }

  if(InitializeEvent())
  {
    TRC( TRC_ID_ERROR, "failed to initialize capture events" );
    vibe_os_delete_resource_lock(m_lock);
    vibe_os_delete_semaphore(m_QueueLock);
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Capture %p Created", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


void CCapture::CleanUp(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* Destroy capture nodes list */
  if(m_CaptureQueue)
  {
    delete m_CaptureQueue;
    m_CaptureQueue = 0;
  }
  vibe_os_delete_semaphore(m_QueueLock);

  /* Delete event subscription */
  TerminateEvent();

  TRC( TRC_ID_MAIN_INFO, "Capture %p cleaned", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


int CCapture::InitializeEvent(void)
{
  int ret=0;
  stm_event_subscription_entry_t         EventEntry;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  EventEntry.object     = (stm_object_h)this;
  EventEntry.event_mask = STM_PIXEL_CAPTURE_EVENT_NEW_BUFFER;
  EventEntry.cookie     = 0;

  vibe_os_allocate_queue_event(&m_WaitQueueEvent);
  if(!m_WaitQueueEvent)
  {
    TRC( TRC_ID_ERROR, "Failed to create Resource Lock!" );
    ret = -ENOMEM;
    goto exit_error;
  }

  ret = stm_event_subscription_create(&EventEntry, 1, &m_EventSubscription);
  if(ret)
  {
    TRC( TRC_ID_ERROR, "Error can't create event subscription" );
    goto exit_error;
  }

  ret = stm_event_set_wait_queue(m_EventSubscription, vibe_os_get_wait_queue_data(m_WaitQueueEvent), true);
  if (ret)
  {
    TRC( TRC_ID_ERROR, "Error can't set wait queue event" );
    goto exit_error;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return ret;

exit_error:
  if(m_EventSubscription)
  {
    // supress warn_unused_result with stm_event_subscription_delete()
    int retval;
    retval = stm_event_subscription_delete(m_EventSubscription);
  }

  if(m_WaitQueueEvent)
    vibe_os_release_queue_event(m_WaitQueueEvent);

  return ret;
}


int CCapture::TerminateEvent(void)
{
  int ret=0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_EventSubscription)
    ret = stm_event_subscription_delete(m_EventSubscription);

  if(m_WaitQueueEvent)
    vibe_os_release_queue_event(m_WaitQueueEvent);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return ret;
}


void CCapture::SetStatus(stm_pixel_capture_status_t statusFlag, bool set)
{
  if(!set)
  {
    switch(statusFlag)
    {
      case STM_PIXEL_CAPTURE_LOCKED:
        m_Status = 0;
        break;
      case STM_PIXEL_CAPTURE_STARTED:
        m_Status = STM_PIXEL_CAPTURE_LOCKED;
        break;
      case STM_PIXEL_CAPTURE_NO_BUFFER:
      case STM_PIXEL_CAPTURE_UNDERFLOW:
      case STM_PIXEL_CAPTURE_OVERFLOW:
        m_Status = STM_PIXEL_CAPTURE_STARTED;
        break;
      default :
        TRC( TRC_ID_ERROR, "Capture %p: Invalid Status value %x", this, statusFlag );
        break;
    }
  }
  else
  {
    if(statusFlag >= static_cast<stm_pixel_capture_status_t>(m_Status))
    {
      m_Status = statusFlag;
    }
    else
    {
      /*
       * Don't overwrite current capture status as it satisfy the required
       * status.
       */
      TRC( TRC_ID_MAIN_INFO, "Capture %p already in wanted state %x", this, statusFlag );
    }
  }
}


bool CCapture::LockUse(void *user)
{
  bool retval = false;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Allow multiple calls to lock with the same user handle
   */
  if(m_user == user)
      return true;

  if(!m_user)
  {
      TRC( TRC_ID_MAIN_INFO, "CCapture::LockUse user = %p using queue = %u", user, GetID() );
      m_user = user;

      vibe_os_lock_resource(m_lock);
      SetStatus(STM_PIXEL_CAPTURE_LOCKED, true);
      vibe_os_unlock_resource(m_lock);

      retval = true;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  /*
   * Another user has the queue locked, so fail
   */
  return retval;
}


void CCapture::Unlock(void *user)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((user != 0) && (m_user != user))
    return;

  TRC( TRC_ID_MAIN_INFO, "CCapture::Unlock user = %p capture = %u", user, GetID() );

  m_user = 0;

  vibe_os_lock_resource(m_lock);
  SetStatus(STM_PIXEL_CAPTURE_LOCKED, false);
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


int CCapture::GetCapabilities(stm_pixel_capture_capabilities_flags_t *caps)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  *caps = m_Capabilities;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


void CCapture::UpdateCurrentNode(const stm_pixel_capture_time_t &vsyncTime)
{
  TRC( TRC_ID_UNCLASSIFIED, "CCapture::UpdateCurrentNode capture = %u vsyncTime = %lld", GetID(),vsyncTime );

  vibe_os_lock_resource(m_lock);

  m_Statistics.CurCapPicPTS = vsyncTime;

  vibe_os_unlock_resource(m_lock);
}


bool CCapture::SetPendingNode(capture_node_h &pendingNode)
{
  bool ret = true;

  TRC( TRC_ID_UNCLASSIFIED, "CCapture::SetPendingNode capture = %u node = %p", GetID(),&pendingNode );

  vibe_os_lock_resource(m_lock);

  m_pendingNode = pendingNode;

  vibe_os_unlock_resource(m_lock);
  return ret;
}


bool CCapture::CheckInputParams(stm_pixel_capture_input_params_t params)
{
  bool is_params_supported = true;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((params.active_window.width == 0) || (params.active_window.height == 0)
  || (params.src_frame_rate == 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Capture Input Params!");
    is_params_supported = false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return is_params_supported;
}


int CCapture::SetInputParams(stm_pixel_capture_input_params_t params)
{
  int retval = -EINVAL;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(this->CheckInputParams(params))
  {
    m_InputParams = params;

    /*
     * Recalculate FrameDuration in usec.
     */
    m_FrameDuration = (stm_pixel_capture_time_t)vibe_os_div64(((uint64_t)vibe_os_get_one_second()*1000ULL),
                       (uint64_t)m_InputParams.src_frame_rate);

    retval = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return retval;
}


int CCapture::GetInputParams(stm_pixel_capture_input_params_t *params)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  *params = m_InputParams;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CCapture::GetFormats(const stm_pixel_capture_format_t** pFormats) const
{
  *pFormats = m_pSurfaceFormats;
  return m_nFormats;
}


bool CCapture::CheckStreamParams(stm_pixel_capture_params_t params)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


int CCapture::SetStreamParams(stm_pixel_capture_params_t params)
{
  int retval = -EINVAL;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(this->CheckStreamParams(params))
  {
    m_StreamParams = params;
    retval = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return retval;
}


int CCapture::GetStreamParams(stm_pixel_capture_params_t *params)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  *params = m_StreamParams;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CCapture::GetInputWindow(stm_pixel_capture_rect_t *input_window)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  *input_window = m_InputWindow;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


bool CCapture::CheckInputWindow(stm_pixel_capture_rect_t input_window)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


int CCapture::SetInputWindow(stm_pixel_capture_rect_t input_window)
{
  int retval = -EINVAL;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(this->CheckInputWindow(input_window))
  {
    m_InputWindow = input_window;
    retval = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return retval;
}


int CCapture::GetInputWindowCaps(stm_pixel_capture_input_window_capabilities_t *input_window_capability)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  *input_window_capability = m_InputWindowCaps;
  return 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CCapture::IsFormatSupported(const stm_pixel_capture_buffer_format_t format)
{
  bool isFormatSupported = false;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Go trough list of formats to check the requested one is supported
  for(uint32_t f=0; f<m_nFormats; f++)
  {
    if(m_pSurfaceFormats[f] == format.format)
    {
      isFormatSupported=true;
      break;
    }
  }

  // Check if format is valid
  if((format.width == 0) || (format.height == 0))
  {
    /*
     * The stride can be zeroed in case of tunneled capture as this will be
     * filled when getting buffers
     */
    isFormatSupported = false;
  }

  /* FIXME : check for other capture format params? */

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return (isFormatSupported);
}


int CCapture::GetCurrentInput(uint32_t *input)
{
  uint32_t idx=0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_UNCLASSIFIED, "input = %p", input );

  if (input == 0)
    return -EINVAL;

  for(idx=0; idx<m_numInputs; idx++)
    if(m_pCaptureInputs[idx].input_id == m_pCurrentInput.input_id)
      break;

  *input = idx;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return (idx==m_numInputs) ? -1:0;
}


int CCapture::SetCurrentInput(uint32_t input)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(input>=m_numInputs)
    return -ENODEV;

  TRC( TRC_ID_UNCLASSIFIED, "input = %d", input );

  m_pCurrentInput = m_pCaptureInputs[input];
  m_ulTimingID    = m_pCurrentInput.pipeline_id;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CCapture::GetAvailableInputs(uint32_t *input, uint32_t max_inputs)
{
  uint32_t i=0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_UNCLASSIFIED, "input = %p max_inputs = %d", input ,max_inputs );

  if (input == 0)
    return m_numInputs;

  for(i=0; (i<m_numInputs) && (i<max_inputs) ; i++)
  {
    input[i] = m_pCaptureInputs[i].input_id;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return i;
}


int CCapture::GetCaptureName(const uint32_t input, const char **name)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(input>=m_numInputs)
    return -ENODEV;

  *name=m_pCaptureInputs[input].input_name;

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CCapture::SetCaptureFormat(const stm_pixel_capture_buffer_format_t format)
{
  int retval = -ENOTSUP;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Go trough list of formats to check the requested one is supported
  if(IsFormatSupported(format))
  {
    /* set the capture format */
    uint32_t old_stride = m_CaptureFormat.stride;
    m_CaptureFormat = format;

    /*
     * The stride can be invalid at this point in case of tunneled capture.
     * Then we keep orignal value.
     */
    if(format.stride == 0)
    {
      /* backup old stride value */
      m_CaptureFormat.stride = old_stride;
    }
    retval=0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return (retval);
}



int CCapture::Start(void)
{
  int           res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(IsAttached())
  {
    int           i = 0;

    /*
     * Don't allow start with invalid buffer format : need to call s_fmt before!
     */
    if((m_CaptureFormat.width == 0) || (m_CaptureFormat.height == 0)
      || (m_CaptureFormat.format == STM_PIXEL_FORMAT_NONE))
    {
      TRC( TRC_ID_ERROR, "Invalid Buffer Format! Please set a valid format before!");
      return -EINVAL;
    }

    /*
     * Try to get CAPTURE_NB_BUFFERS buffers for tunneling.
     * Queue ALL nodes for capture.
     */
    if(m_PushGetInterface.get_buffer)
    {
      for(i=0; i<CAPTURE_NB_BUFFERS; i++)
      {
        capture_node_h                    capNode   = {0};

        res = GetCaptureNode(capNode, true);
        if (res)
        {
          // get buffer fails : push back previous buffers...
          TRC( TRC_ID_ERROR, "Error in GetCapturedNode(%p) ret = %d\n", m_Sink, res);
          break;
        }
      }
    }

    if (res)
    {
      /*
       * Flush all queued capture nodes.
       */
      Flush(true);
    }
    else
    {
      /*
       * Enable the hardware update.
       */
      m_hasBuffersToRelease = true;
    }
  }

  if(!res)
  {
    /* Lock capture device before going on */
    vibe_os_lock_resource(m_lock);

    m_isStarted = true;
    SetStatus(STM_PIXEL_CAPTURE_STARTED, true);

    /* Unlock capture device before continue */
    vibe_os_unlock_resource(m_lock);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return res;
}


int CCapture::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /* Lock capture device before going on */
  vibe_os_lock_resource(m_lock);

  m_isStarted = false;
  SetStatus(STM_PIXEL_CAPTURE_STARTED, false);

  /* Unlock capture device before exiting */
  vibe_os_unlock_resource(m_lock);

  /*
   * Notify the play_stream interface about the start of flush operation.
   */
  if(IsAttached() && (m_PushGetInterface.notify_flush_start))
  {
    TRC( TRC_ID_UNCLASSIFIED, "Notify Sink (%p) about the starting of flush operation\n", m_Sink);
    m_PushGetInterface.notify_flush_start(m_Sink);
  }

  /*
   * Simply we are flushing ALL queued nodes.
   */
  Flush(true);

  /*
   * Notify the play_stream interface about the end of flush operation.
   */
  if(IsAttached() && (m_PushGetInterface.notify_flush_end))
  {
    TRC( TRC_ID_UNCLASSIFIED, "Notify Sink (%p) about the end of flush operation\n", m_Sink);
    m_PushGetInterface.notify_flush_end(m_Sink);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return 0;
}


int CCapture::HandleInterrupts(uint32_t *timingevent)
{
  return -ENOTSUP;
}


int CCapture::GetCaptureNode(capture_node_h &capNode, bool free_node)
{
  int res = -ENOMEM;
  stm_i_push_get_sink_get_desc_t   *pGetBufferDesc  = (stm_i_push_get_sink_get_desc_t *)NULL;

  // Allocate a 'push' buffers descriptors. This buffer will be freed by CQueue::ReleaseNode
  pGetBufferDesc = (stm_i_push_get_sink_get_desc_t *) vibe_os_allocate_memory (sizeof(stm_i_push_get_sink_get_desc_t));
  if(!pGetBufferDesc)
  {
    TRC( TRC_ID_ERROR, "failed to allocate pPushBufferDesc" );
    return res;
  }
  vibe_os_zero_memory(pGetBufferDesc, sizeof(stm_i_push_get_sink_get_desc_t));

  /*
   * Fill 'get' buffers descriptors from current set capture buffer format.
   */
  pGetBufferDesc->width                 = m_CaptureFormat.width;
  pGetBufferDesc->height                = m_CaptureFormat.height;
  pGetBufferDesc->format                = m_CaptureFormat.format;
  pGetBufferDesc->pitch                 = 0;  /* to be filled by allocator */
  pGetBufferDesc->video_buffer_addr     = 0;  /* to be filled by allocator */
  pGetBufferDesc->chroma_buffer_offset  = 0;  /* to be filled by allocator */
  pGetBufferDesc->allocator_data        = 0;  /* to be filled by allocator */

  res = m_PushGetInterface.get_buffer(m_Sink, (void *)pGetBufferDesc);
  if (res)
  {
    // get buffer fails : push back previous buffers...
    TRC( TRC_ID_ERROR, "Error in m_PushGetInterface.get_buffer(%p) ret = %d\n", m_Sink, res);
    return res;
  }

  /*
   * Update Current Buffer format with valid values.
   */
  m_CaptureFormat.stride                   = pGetBufferDesc->pitch;

  /* Now queue this empty buffer for capture */
  if(free_node)
  {
    capNode = m_CaptureQueue->GetFreeNode();
    if(!capNode)
    {
      TRC( TRC_ID_ERROR, "Error : Unable to get a new capture Node!\n");
      res = -ENOMEM;
      goto failed_get_free_cap_node;
    }
  }

  /* Set the new node data */
  m_CaptureQueue->SetNodeData(capNode,pGetBufferDesc);

  /* Finally queue the new capture node */
  if(!m_CaptureQueue->QueueNode(capNode))
  {
    TRC( TRC_ID_ERROR, "Error : Unable to queue a new capture Node!\n");
    res = -EIO;
    goto failed_queue_cap_node;
  }

  /*
   * SDP : grant access for Capture HW.
   */
  {
    void * base_address = (void *)pGetBufferDesc->video_buffer_addr;
    uint32_t buffer_size = pGetBufferDesc->height * pGetBufferDesc->pitch;

    if(pGetBufferDesc->chroma_buffer_offset != 0)
      buffer_size *= 2;

    vibe_os_grant_access(base_address, buffer_size,
                         VIBE_ACCESS_TID_HDMI_CAPTURE,
                         VIBE_ACCESS_O_WRITE);
  }

  TRC( TRC_ID_UNCLASSIFIED, "Get new Node 0x%p Captured Buffer 0x%p (video_buffer_addr = 0x%p)", (void*)capNode, (void*)pGetBufferDesc, (void*)pGetBufferDesc->video_buffer_addr);

  m_Statistics.PicQueued++;
  pGetBufferDesc = 0;    // This buffer is now under the control of queue_node

  return 0;

failed_queue_cap_node:
  m_CaptureQueue->ReleaseNode(capNode);
failed_get_free_cap_node:
  vibe_os_free_memory(pGetBufferDesc);

  return res;
}


int CCapture::PushCapturedNode(capture_node_h &capNode, stm_pixel_capture_time_t vsyncTime, int content_is_valid, bool release_node)
{
  int res = -EINVAL;
  stm_i_push_get_sink_get_desc_t   *pGetBufferDesc  = (stm_i_push_get_sink_get_desc_t *)m_CaptureQueue->GetNodeData(capNode);
  stm_i_push_get_sink_push_desc_t   PushBufferDesc  = {0};
  stm_i_push_get_sink_push_desc_t  *pPushBufferDesc = &PushBufferDesc;

  if(!pGetBufferDesc)
  {
    TRC( TRC_ID_ERROR, "Invalid node (buffer is corrupted?)!" );
    return res;
  }

  /*
   * Fill 'push' buffers descriptors from current set capture buffer format.
   */
  pPushBufferDesc->buffer_desc          = *pGetBufferDesc;
  pPushBufferDesc->color_space          = m_CaptureFormat.color_space;
  pPushBufferDesc->src_frame_rate.numerator    = m_InputParams.src_frame_rate;
  pPushBufferDesc->src_frame_rate.denominator  = 1000;
  pPushBufferDesc->content_type         = STM_PIXEL_CAPTURE_CONTENT_TYPE_CINEMA;        // ??;
  pPushBufferDesc->pixel_aspect_ratio   = m_InputParams.pixel_aspect_ratio;
  pPushBufferDesc->captured_time        = vsyncTime - m_FrameDuration;
  pPushBufferDesc->content_is_valid     = content_is_valid;    /* set buffer to be flushed not displayed */

  /*
   * We always produce frames :
   * - Update buffer flags for PROGRESSIVE content.
   * - Setup the frame_rate according to input field type.
   */
  bool isInputInterlaced = !!((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);
  pPushBufferDesc->flags = isInputInterlaced?STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM:0;

  /*
   * SDP : grant access for Capture HW.
   */
  {
    void * base_address = (void *)pGetBufferDesc->video_buffer_addr;
    uint32_t buffer_size = pGetBufferDesc->height * pGetBufferDesc->pitch;

    if(pGetBufferDesc->chroma_buffer_offset != 0)
      buffer_size *= 2;

    vibe_os_revoke_access(base_address, buffer_size,
                         VIBE_ACCESS_TID_HDMI_CAPTURE,
                         VIBE_ACCESS_O_WRITE);
  }

  res = m_PushGetInterface.push_buffer(m_Sink, (void *)pPushBufferDesc);
  if(res)
  {
    TRC( TRC_ID_ERROR, "Push Buffer FAILED (Node 0x%p - buffer = 0x%p new buffer addr 0x%p)!", (void*)capNode, (void *)pPushBufferDesc, (void*)pPushBufferDesc->buffer_desc.video_buffer_addr);
    return res;
  }

  if(content_is_valid == 1)
    m_Statistics.PicPushedToSink++;

  TRC( TRC_ID_UNCLASSIFIED, "Push old Node 0x%p Captured Buffer 0x%p (video_buffer_addr = 0x%p)", (void*)capNode, (void*)pPushBufferDesc, (void*)pPushBufferDesc->buffer_desc.video_buffer_addr);

  if(release_node)
  {
    /* Now release this capture node */
    m_CaptureQueue->ReleaseNode(capNode);
    m_Statistics.PicReleased++;
  }
  else
  {
    // Invalidate buffer address for this node
    pGetBufferDesc->video_buffer_addr = 0;
    m_CaptureQueue->SetNodeData(capNode,pGetBufferDesc);
  }

  return res;
}


int CCapture::NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime)
{
  int res=0;

  /*
   * Notify upper level about released buffer.
   * Note that the m_WaitQueueEvent is begin waked up  withing this
   * event notification. No need to call vibe_os_wake_up_queue_event().
   */
  m_CaptureEvent.object   = (stm_object_h)m_user;
  m_CaptureEvent.event_id = STM_PIXEL_CAPTURE_EVENT_NEW_BUFFER;
  res=stm_event_signal (&m_CaptureEvent);
  if(res)
  {
    /*
     * We may be failing to get event notified in case we don't have any
     * subscriber already waiting for it.
     *
     * In that case we should be getting specific error number which
     * we can be using to bypass the error but this is not the case as
     * per current event manager implementation (See Bug #50907).
     *
     * For the moment we are going to ignore error.
     */
    res = 0;
    TRC( TRC_ID_UNCLASSIFIED, "Buffer notification failed!" );
  }

  return res;
}


int CCapture::QueueBuffer(stm_pixel_capture_buffer_descr_t *pBuffer,
                                    const void                * const user)
{
  CaptureQueueBufferInfo   qbinfo;
  int                      res=0;

  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  TRC( TRC_ID_UNCLASSIFIED, "'pBuffer = %p - user = %p (owner = %p)'", pBuffer, user, m_user );

  /* capture should be already started before queueing buffers */
  if(m_user != user)
  {
    return -1;
  }

  res = GetQueueBufferInfo(pBuffer, qbinfo);
  if(!res)
    return -1;

  res = PrepareAndQueueNode(pBuffer, qbinfo);
  if(res)
  {
    TRC( TRC_ID_UNCLASSIFIED, "CDvpCAP::QueueBuffer %p completed", pBuffer );
    m_NumberOfQueuedBuffer++;
  }

  m_Statistics.PicQueued++;

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return res ? 0:-1;
}


/////////////////////////////////////////////////////////////////
// Helper function for capture QueueBuffer implementations that
// extract flags and do some common processing for interlaced
// input and output content.
bool CCapture::GetQueueBufferInfo(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                               CaptureQueueBufferInfo              &qbinfo)
{
  TRCIN( TRC_ID_UNCLASSIFIED, "" );

  vibe_os_zero_memory(&qbinfo,sizeof(qbinfo));

  if((m_InputParams.active_window.width == 0) || (m_InputParams.active_window.height == 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Active Window size! Please setup a correct active window" );
    return false;
  }

  if((m_InputWindow.width == 0) || (m_InputWindow.height == 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Input Window size! Please setup a correct capture input window" );
    return false;
  }

  if((pBuffer->cap_format.width == 0) || (pBuffer->cap_format.height == 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Buffer Window size! Please setup a correct capture buffer size" );
    return false;
  }

  qbinfo.isBufferInterlaced       = ((pBuffer->cap_format.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);
  qbinfo.isInputInterlaced        = ((m_InputParams.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED) == STM_PIXEL_CAPTURE_BUFFER_INTERLACED);

  qbinfo.InputVisibleArea.x       = m_InputParams.active_window.x;
  qbinfo.InputVisibleArea.y       = m_InputParams.active_window.y;
  qbinfo.InputVisibleArea.width   = m_InputParams.active_window.width;
  qbinfo.InputVisibleArea.height  = m_InputParams.active_window.height;
  qbinfo.InputPixelAspectRatio    = m_InputParams.pixel_aspect_ratio;

  qbinfo.InputFrameRect.x         = m_InputWindow.x;
  qbinfo.InputFrameRect.y         = m_InputWindow.y;
  qbinfo.InputFrameRect.width     = m_InputWindow.width;
  qbinfo.InputFrameRect.height    = m_InputWindow.height;
  qbinfo.InputHeight              = m_InputWindow.height;

  qbinfo.BufferFrameRect.x        = 0;
  qbinfo.BufferFrameRect.y        = 0;
  qbinfo.BufferFrameRect.width    = pBuffer->cap_format.width;
  qbinfo.BufferFrameRect.height   = pBuffer->cap_format.height;
  qbinfo.BufferHeight             = qbinfo.BufferFrameRect.height;

  qbinfo.BufferFormat             = pBuffer->cap_format;

  /*
   * Convert the source origin to fixed point format ready for setting up
   * the resize filters. Note that the incoming coordinates are in
   * multiples of a 16th of a pixel/scanline.
   */
  qbinfo.InputFrameRectFixedPointX = ValToFixedPoint(qbinfo.InputFrameRect.x, 16);
  qbinfo.InputFrameRectFixedPointY = ValToFixedPoint(qbinfo.InputFrameRect.y, 16);

  if((m_InputParams.pixel_format == STM_PIXEL_FORMAT_YCbCr422R) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP))
  {
    /* Input is YCbCr422 */
    qbinfo.isInputRGB = false;
    qbinfo.isInput422 = true;
  }
  else if((m_InputParams.pixel_format == STM_PIXEL_FORMAT_RGB565) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_RGB888) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_ARGB1555) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_ARGB4444) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_ARGB8565) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_ARGB8888) ||
     (m_InputParams.pixel_format == STM_PIXEL_FORMAT_RGB_10B10B10B_SP))
  {
    /* Input is RGB */
    qbinfo.isInputRGB = true;
    qbinfo.isInput422 = false;
  }
  else
  {
    /* Input is YCbCr444 */
    qbinfo.isInputRGB = false;
    qbinfo.isInput422 = false;
  }

  TRCOUT( TRC_ID_UNCLASSIFIED, "" );

  return true;
}


void CCapture::AdjustBufferInfoForScaling(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                                       CaptureQueueBufferInfo              &qbinfo) const
{
  CalculateHorizontalScaling(qbinfo);
  CalculateVerticalScaling(qbinfo);

  /*
   * Now adjust the source coordinate system to take into account line skipping
   */
  qbinfo.InputFrameRectFixedPointY /= qbinfo.line_step;

  /*
   * Define the Y coordinate limit in the source image, used to ensure we
   * do not go outside of the required source image crop when the Y position
   * is adjusted for re-scale purposes.
   */
  qbinfo.maxYCoordinate = ((qbinfo.InputFrameRectFixedPointY / m_fixedpointONE)
                           + qbinfo.verticalFilterInputSamples - 1);
}


void CCapture::CalculateHorizontalScaling(CaptureQueueBufferInfo &qbinfo) const
{
  bool bRecalculateDstWidth = false;

  /*
   * Calculate the scaling factors, with one extra bit of precision so we can
   * round the result.
   */
  qbinfo.hsrcinc = (qbinfo.InputFrameRect.width  * m_fixedpointONE * 2) / qbinfo.BufferFrameRect.width;

  if(qbinfo.isInput422)
  {
    /*
     * For formats with half chroma, we have to round up or down to an even
     * number, so that the chroma value which is half this value cannot lose
     * precision.
     */
    qbinfo.hsrcinc += 1L<<1;
    qbinfo.hsrcinc &= ~0x3;
    qbinfo.hsrcinc >>= 1;
  }
  else
  {
    /*
     * As chroma is not an issue here just round the result and convert to
     * the correct fixed point format.
     */
    qbinfo.hsrcinc += 1;
    qbinfo.hsrcinc >>= 1;
  }

  if(qbinfo.hsrcinc < m_ulMinHSrcInc)
  {
      qbinfo.hsrcinc = m_ulMinHSrcInc;
      bRecalculateDstWidth = true;
  }

  if(qbinfo.hsrcinc > m_ulMaxHSrcInc)
  {
      qbinfo.hsrcinc = m_ulMaxHSrcInc;
      bRecalculateDstWidth = true;
  }

  /*
   * Chroma src used only for YUV formats on planes that support separate
   * chroma filtering.
   */
  if(qbinfo.isInput422)
    qbinfo.chroma_hsrcinc = qbinfo.hsrcinc/2;
  else
    qbinfo.chroma_hsrcinc = qbinfo.hsrcinc;

  if(bRecalculateDstWidth)
    qbinfo.InputFrameRect.width  = (qbinfo.InputFrameRect.width  * m_fixedpointONE) / qbinfo.hsrcinc;

  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x hsrcinc = 0x%x chsrcinc = 0x%x", m_fixedpointONE, qbinfo.hsrcinc, qbinfo.chroma_hsrcinc );
}


void CCapture::CalculateVerticalScaling(CaptureQueueBufferInfo &qbinfo) const
{
  bool bRecalculateDstHeight = false;
  unsigned long srcHeight;

  qbinfo.line_step = 1;

  if(qbinfo.BufferHeight == 0)
  {
    TRC( TRC_ID_ERROR, "Invalid Buffer height value" );
    return;
  }

  /*
   * If we are hardware de-interlacing then the source image is vertically
   * upsampled before it before it gets to the resize filter. If we are
   * using the P2I block then the required number of samples out of the filter
   * is twice the destination height (which is now in field lines).
   */
  qbinfo.verticalFilterOutputSamples = qbinfo.BufferHeight;
restart:
  srcHeight = qbinfo.InputHeight / qbinfo.line_step;
  qbinfo.verticalFilterInputSamples  = srcHeight;

  /*
   * Calculate the scaling factors, with one extra bit of precision so we can
   * round the result.
   */
  qbinfo.vsrcinc = (qbinfo.verticalFilterInputSamples * m_fixedpointONE * 2) / qbinfo.verticalFilterOutputSamples;

  /*
   * Round the result and convert to the correct fixed point format.
   */
  qbinfo.vsrcinc += 1;
  qbinfo.vsrcinc >>= 1;

  if(qbinfo.vsrcinc < m_ulMinVSrcInc)
  {
    qbinfo.vsrcinc = m_ulMinVSrcInc;
    bRecalculateDstHeight = true;
  }

  if(qbinfo.vsrcinc > m_ulMaxVSrcInc)
  {
    if(qbinfo.line_step < m_ulMaxLineStep)
    {
      ++qbinfo.line_step;
      goto restart;
    }
    else
    {
      qbinfo.vsrcinc = m_ulMaxVSrcInc;
      bRecalculateDstHeight = true;
    }
  }

  qbinfo.chroma_vsrcinc = qbinfo.vsrcinc;

  TRC( TRC_ID_UNCLASSIFIED, "one = 0x%x vsrcinc = 0x%x cvsrcinc = 0x%x", m_fixedpointONE, qbinfo.vsrcinc, qbinfo.chroma_vsrcinc );

  if(bRecalculateDstHeight)
  {
    qbinfo.BufferHeight = (qbinfo.verticalFilterInputSamples * m_fixedpointONE) / qbinfo.vsrcinc;
  }
}


void CCapture::Freeze(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsFrozen = true;
  m_bIsSuspended = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CCapture::Suspend(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsSuspended = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CCapture::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsFrozen = false;
  m_bIsSuspended = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


int CCapture::Attach(const stm_object_h sink)
{
  int           res = 0;
  stm_object_h  SinkType;
  char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
  int32_t       returnedSize;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(IsAttached())
  {
    TRC( TRC_ID_ERROR, "Pixel Capture device already attached to sink %p", m_Sink );
    res = (sink == m_Sink) ? -EALREADY:-ENOTSUPP;
    goto out_error;
  }

  if(m_isStarted)
  {
    TRC( TRC_ID_MAIN_INFO, "trying to attach capture while it is running");
    return -ENOTSUPP;
  }

  /*
   * Manage play_stream attachement.
   */
  if(sink)
  {
    /*
     * Check sink object support STM_DATA_INTERFACE_PUSH_GET interface
     */
    res = stm_registry_get_object_type(sink, &SinkType);
    if (res)
    {
      TRC( TRC_ID_ERROR, "Error in stm_registry_get_object_type(%p, &%p)\n", sink, SinkType);
      goto out_error;
    }

    res = stm_registry_get_attribute(SinkType,
                                        STM_DATA_INTERFACE_PUSH_GET,
                                        tagTypeName,
                                        sizeof(stm_data_interface_push_get_sink_t),
                                        &m_PushGetInterface,
                                        (int *)&returnedSize);
    if ((res) || (returnedSize != sizeof(stm_data_interface_push_get_sink_t)))
    {
      TRC( TRC_ID_ERROR, "Pixel Capture failed to attach to sink %p", sink);
      goto out_error;
    }

    /*
     * Validate Sink pointer.
     */
    m_Sink = (stm_object_h)sink;
  }

out_error:
  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return res;
}


int CCapture::Detach(const stm_object_h sink)
{
  int res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!IsAttached())
  {
    TRC( TRC_ID_MAIN_INFO, "capture is already detached, nothing to do");
    return res;
  }

  if(m_isStarted)
  {
    TRC( TRC_ID_MAIN_INFO, "trying to detach capture while it is still running");
    return -ENOTSUPP;
  }

  if(sink != m_Sink)
  {
    TRC( TRC_ID_ERROR, "trying to detach an invalid sink %p (current attached sink = %p)", sink, m_Sink );
    res = -EINVAL;
    goto out_error;
  }

  m_Sink = 0;

out_error:
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}

void CCapture::GrantCaptureBufferAccess(const stm_pixel_capture_buffer_descr_t *const pBuffer)
{
  if(!pBuffer)
  {
    TRC( TRC_ID_ERROR, "Invalid pBuffer address (null pointer)!!!" );
    return;
  }

  if(pBuffer->rgb_address)
  {
    vibe_os_grant_access((void*)pBuffer->rgb_address,
                         pBuffer->length,
                         VIBE_ACCESS_TID_HDMI_CAPTURE,
                         VIBE_ACCESS_O_WRITE);
  }
  else
  {
    TRC( TRC_ID_ERROR, "null pointer !!!" );
  }
}


void CCapture::RevokeCaptureBufferAccess(const stm_pixel_capture_buffer_descr_t *const pBuffer)
{
  if(!pBuffer)
  {
    TRC( TRC_ID_ERROR, "Invalid pBuffer address (null pointer)!!!" );
    return;
  }

  if(pBuffer->rgb_address)
  {
    vibe_os_revoke_access((void *)pBuffer->rgb_address,
                          pBuffer->length,
                          VIBE_ACCESS_TID_HDMI_CAPTURE,
                          VIBE_ACCESS_O_WRITE);
  }
  else
  {
    TRC( TRC_ID_ERROR, "null pointer !!!" );
  }
}

bool CCapture::RegisterStatistics(void)
{
  char Tag[STM_REGISTRY_MAX_TAG_SIZE];

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicVSyncNb" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicVSyncNb, sizeof( m_Statistics.PicVSyncNb )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PictQueued" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicQueued, sizeof( m_Statistics.PicQueued )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicReleased" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicReleased, sizeof( m_Statistics.PicReleased )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }


  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicCaptured" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicCaptured, sizeof( m_Statistics.PicCaptured )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicRepeated" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicRepeated, sizeof( m_Statistics.PicRepeated )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicSkipped" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicSkipped, sizeof( m_Statistics.PicSkipped )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicInjected" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicInjected, sizeof( m_Statistics.PicInjected )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_PicPushedToSink" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicPushedToSink, sizeof( m_Statistics.PicPushedToSink )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
  vibe_os_snprintf( Tag, sizeof( Tag ), "CaptureStat_CurCapPicPTS" );

  if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.CurCapPicPTS, sizeof( m_Statistics.CurCapPicPTS )) != 0 )
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, GetID() );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, GetID() );
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
// C Device interface

#if defined(__cplusplus)
extern "C" {
#endif

int stm_pixel_capture_open(stm_pixel_capture_device_type_t type, const uint32_t instance,
                                  stm_pixel_capture_h *pixel_capture)
{
  int rc;

  rc = stm_pixel_capture_device_open(type, instance, pixel_capture);

  if ( rc == 0 )
    {
      TRC( TRC_ID_API_PIXEL_CAPTURE, "type : %u, instance : %u, pixel_capture : %s", (uint32_t)type, instance, ((CCapture *)(*pixel_capture)->handle)->GetName() );
    }

  return rc;
}


void stm_pixel_capture_close(const stm_pixel_capture_h pixel_capture)
{
  int res;

  if(stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    {
      TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );
    }

  res = stm_pixel_capture_device_close(pixel_capture);

  if(res)
    TRC( TRC_ID_ERROR, "failed to close pixel capture" );
}


int stm_pixel_capture_lock(const stm_pixel_capture_h pixel_capture)
{
  CCapture *pCapture = (CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->LockUse(pixel_capture->owner)?0:-ENOLCK;

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_unlock(const stm_pixel_capture_h pixel_capture)
{
  CCapture *pCapture = (CCapture *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  pCapture->Unlock(pixel_capture->owner);

  vibe_os_up_semaphore(pixel_capture->lock);

  return 0;
}


int stm_pixel_capture_enum_inputs(const stm_pixel_capture_h pixel_capture,
                                         const uint32_t input,
                                         const char **name)
{
  CCapture* pCapture = (CCapture*)NULL;
  int res = 0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -EINVAL;

  pCapture = (CCapture*)(pixel_capture->handle);

  //input pointer should be valid
  if(!CHECK_ADDRESS(name))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetCaptureName(input, name);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_enum_image_formats(const stm_pixel_capture_h pixel_capture,
                                         const stm_pixel_capture_format_t **formats)
{
  CCapture* pCapture = (CCapture*)NULL;
  int n = 0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -EINVAL;

  pCapture = (CCapture*)(pixel_capture->handle);

  // formats pointer should be valid
  if(!CHECK_ADDRESS(formats))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  n = pCapture->GetFormats(formats);

  vibe_os_up_semaphore(pixel_capture->lock);

  return n;
}


int stm_pixel_capture_set_input(const stm_pixel_capture_h pixel_capture,
                                       const uint32_t input )
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->SetCurrentInput(input);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_input(const stm_pixel_capture_h pixel_capture,
                                       uint32_t *input)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(input))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetCurrentInput(input);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_input_window_capabilities(const stm_pixel_capture_h pixel_capture,
                                           stm_pixel_capture_input_window_capabilities_t * input_window_caps)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(input_window_caps))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetInputWindowCaps(input_window_caps);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_input_window(const stm_pixel_capture_h pixel_capture,
                                      stm_pixel_capture_rect_t * input_window)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(input_window))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetInputWindow(input_window);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_set_input_window(const stm_pixel_capture_h  pixel_capture,
                                      const stm_pixel_capture_rect_t input_window)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->SetInputWindow(input_window);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_try_format(const stm_pixel_capture_h pixel_capture,
                                        const stm_pixel_capture_buffer_format_t format,
                                        bool *supported)
{
  CCapture *pCapture =(CCapture *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(supported))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  *supported = pCapture->IsFormatSupported(format);

  vibe_os_up_semaphore(pixel_capture->lock);

  return 0;
}


int stm_pixel_capture_set_format(const stm_pixel_capture_h pixel_capture,
                                        const stm_pixel_capture_buffer_format_t format)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->SetCaptureFormat(format);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_format(const stm_pixel_capture_h pixel_capture,
                                        stm_pixel_capture_buffer_format_t * const format)
{
  CCapture *pCapture =(CCapture *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(format))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  *format = pCapture->GetCaptureFormat();

  vibe_os_up_semaphore(pixel_capture->lock);

  return 0;
}


int stm_pixel_capture_start(const stm_pixel_capture_h pixel_capture)
{
  CCapture *pCapture = (CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->Start();

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_stop(const stm_pixel_capture_h pixel_capture)
{
  CCapture *pCapture = (CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->Stop();

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_query_capabilities(const stm_pixel_capture_h pixel_capture,
                                        stm_pixel_capture_capabilities_flags_t *capabilities_flags)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(capabilities_flags))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetCapabilities(capabilities_flags);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_queue_buffer(const stm_pixel_capture_h pixel_capture,
                                   stm_pixel_capture_buffer_descr_t *buffer)
{
  CCapture* pCapture = (CCapture*)NULL;
  int res = 0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -EINVAL;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture*)(pixel_capture->handle);

  //input pointer should be valid
  if(!CHECK_ADDRESS(buffer))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s, buffer : %p", pCapture->GetName(), buffer );
  res = pCapture->QueueBuffer(buffer,pixel_capture->owner);

  if (res == 0)
  {
    pCapture->GrantCaptureBufferAccess(buffer);
  }

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_dequeue_buffer(const stm_pixel_capture_h pixel_capture,
                                           stm_pixel_capture_buffer_descr_t *buffer)
{
  CCapture* pCapture = (CCapture*)NULL;
  int res = 0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -EINVAL;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture*)(pixel_capture->handle);

  //input pointer should be valid
  if(!CHECK_ADDRESS(buffer))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s, buffer : %p", pCapture->GetName(), buffer );
  res = pCapture->ReleaseBuffer(buffer);

  pCapture->RevokeCaptureBufferAccess(buffer);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_attach(const stm_pixel_capture_h pixel_capture,
                             const stm_object_h sink)
{
  CCapture *pCapture = (CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->Attach(sink);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_detach(const stm_pixel_capture_h pixel_capture,
                             const stm_object_h sink)
{
  CCapture *pCapture = (CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture = (CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->Detach(sink);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_status( const stm_pixel_capture_h pixel_capture,
                    stm_pixel_capture_status_t * status)
{
  CCapture *pCapture = (CCapture *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  *status = (stm_pixel_capture_status_t)pCapture->GetStatus();

  vibe_os_up_semaphore(pixel_capture->lock);

  return 0;
}


int stm_pixel_capture_set_stream_params(const stm_pixel_capture_h pixel_capture,
                                               stm_pixel_capture_params_t params)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->SetStreamParams(params);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_stream_params(const stm_pixel_capture_h pixel_capture,
                                               stm_pixel_capture_params_t * params)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(params))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetStreamParams(params);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_set_input_params( const stm_pixel_capture_h pixel_capture,
                            stm_pixel_capture_input_params_t params)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  if(stm_pixel_capture_device_is_suspended(pixel_capture))
    return -EAGAIN;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->SetInputParams(params);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_get_input_params(const stm_pixel_capture_h pixel_capture,
                            stm_pixel_capture_input_params_t * params)
{
  CCapture *pCapture =(CCapture *)NULL;
  int res;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(params))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  res = pCapture->GetInputParams(params);

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}

int stm_pixel_capture_handle_interrupts(stm_pixel_capture_h pixel_capture,
                            uint32_t * timingevent)
{
  CCapture *pCapture =(CCapture *)NULL;

  pCapture =(CCapture *)(pixel_capture->handle);

  if(!CHECK_ADDRESS(timingevent))
    return -EFAULT;

  return (pCapture->HandleInterrupts(timingevent));
}

#if defined(__cplusplus)
} // extern "C"
#endif
