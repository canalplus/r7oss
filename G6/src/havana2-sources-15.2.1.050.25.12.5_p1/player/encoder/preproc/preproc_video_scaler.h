/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#ifndef PREPROC_VIDEO_SCALER_H
#define PREPROC_VIDEO_SCALER_H

#include "osinline.h"
#include <stm_scaler.h>

#include "preproc_video.h"

#define SCALER_ID                      0
#define SD_WIDTH                       720
#define SCALER_CONTEXT_MAX_BUFFERS     16

typedef struct Scaler_Context_s
{

    void                      *PreprocVideoScaler;
    stm_scaler_task_descr_t   Desc;
    Buffer_t                  InputBuffer;
    Buffer_t                  OutputBuffer;
    Buffer_t                  ContextBuffer;
    FRCCtrl_t                 FRCCtrl;

} Scaler_Context_t;

#define BUFFER_SCALER_CONTEXT       "ScalerContext"
#define BUFFER_SCALER_CONTEXT_TYPE  {BUFFER_SCALER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Scaler_Context_t)}

class Preproc_Video_Scaler_c: public Preproc_Video_c
{
public:
    Preproc_Video_Scaler_c(void);
    ~Preproc_Video_Scaler_c(void);

    PreprocStatus_t   FinalizeInit(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   Input(Buffer_t Buffer);
    PreprocStatus_t   TaskCallback(Scaler_Context_t *Context);
    PreprocStatus_t   InputBufferCallback(Scaler_Context_t *Context);
    PreprocStatus_t   OutputBufferCallback(Scaler_Context_t *Context);
    PreprocStatus_t   ErrorCallback(Scaler_Context_t *Context);

    PreprocStatus_t   GetControl(stm_se_ctrl_t    Control,
                                 void            *Data);

    PreprocStatus_t   SetControl(stm_se_ctrl_t    Control,
                                 const void      *Data);

    PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t    Control,
                                         void            *Data);

    PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t    Control,
                                         const void      *Data);

    PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t    Discontinuity);

    static PreprocVideoCaps_t   GetCapabilities(void);

protected:
    stm_scaler_h             scaler;

    Buffer_t                 PreprocFrameBuffer2;
    Buffer_t                 PreprocContextBuffer2;

    // Scaler task completion semaphore, ensuring that stm_se_encode_stream_inject_frame() method is blocking as long as the input buffer is being used
    OS_Semaphore_t           ScalerTaskCompletionSemaphore;
    Scaler_Context_t        *ScalerContext;

    PreprocStatus_t   WaitForAllScalerContexts();
    bool              CheckPendingTask();
    PreprocStatus_t   WaitForPendingTasksToComplete();
    PreprocStatus_t   GetNewScalerContext(Buffer_t *Buffer);
    PreprocStatus_t   PrepareScalerTask(Buffer_t InputBuffer, Buffer_t OutputBuffer, ARCCtrl_t *ARCCtrl);
    PreprocStatus_t   DispatchScaler(Buffer_t InputBuffer, Buffer_t OutputBuffer,
                                     Buffer_t ContextBuffer, uint32_t BufferFlag);
    PreprocStatus_t   RepeatOutputFrame(Buffer_t InputBuffer, Buffer_t OutputBuffer, FRCCtrl_t *FRCCtrl);
    PreprocStatus_t   ProcessField(Buffer_t InputBuffer, Buffer_t OutputBuffer,
                                   Buffer_t ContextBuffer, bool SecondField);

private:
    unsigned int    mRelayfsIndex;

    DISALLOW_COPY_AND_ASSIGN(Preproc_Video_Scaler_c);
};

#endif /* PREPROC_VIDEO_SCALER_H */
