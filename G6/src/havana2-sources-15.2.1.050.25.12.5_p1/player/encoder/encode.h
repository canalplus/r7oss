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
#ifndef ENCODE_H
#define ENCODE_H

#include "encoder.h"

class Encode_c : public BaseComponentClass_c
{
public:
    Encode_c(void);
    ~Encode_c(void);

    EncoderStatus_t         AddStream(EncodeStream_t    Stream,
                                      Preproc_t   Preproc,
                                      Coder_t     Coder,
                                      Transporter_t   Transporter);

    EncoderStatus_t         RemoveStream(EncodeStream_t Stream);

    //
    // Mechanisms for data insertion
    //

    EncoderStatus_t         Input(Buffer_t  Buffer);

    EncoderStatus_t         InputData(void         *Data,
                                      unsigned int    DataLength);

    //
    // Internal Support
    //

    EncoderStatus_t     RegisterBufferManager(BufferManager_t            BufferManager);

    BufferPool_t        GetControlStructurePool(void)
    {
        return ControlStructurePool;
    }

    unsigned int        GetEncodeID(void)
    {
        return EncodeID;
    }

    EncodeCoordinator_t GetEncodeCoordinator(void)
    {
        return Encoder.EncodeCoordinator;
    }

    // Function that indicates whether the encode is configured in NRT mode (see STM_SE_CTRL_ENCODE_NRT_MODE)
    bool                IsModeNRT(void)
    {
        return IsNRT;
    }

    //
    // Encodes control management
    //

    EncoderStatus_t     GetControl(stm_se_ctrl_t Control,
                                   void *Data);
    EncoderStatus_t     SetControl(stm_se_ctrl_t Control,
                                   const void *Data);
    EncoderStatus_t     GetCompoundControl(stm_se_ctrl_t Control,
                                           void *Data);
    EncoderStatus_t     SetCompoundControl(stm_se_ctrl_t Control,
                                           const void *Data);

    //
    // Encodes linked list management
    //

    Encode_t            GetNext(void);
    void                SetNext(Encode_t Encode);

    //
    // Low power functions
    //

    EncoderStatus_t     LowPowerEnter(void);
    EncoderStatus_t     LowPowerExit(void);


    //
    // Memory Profile management
    //

    VideoEncodeMemoryProfile_t  GetVideoEncodeMemoryProfile(void);
    void                        SetVideoEncodeMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile);
    surface_format_t            GetVideoInputColorFormatForecasted(void);
    void                        SetVideoInputColorFormatForecasted(surface_format_t ForecastedFormat);

protected:
    OS_Mutex_t            Lock;

    // "VideoEncodeMemoryProfile" enables to dimension internal buffer size for the next encode_stream to be created
    // Information is provided by STKPI client through stm_se_encode_set_control()
    // By default, profile considered is the max / legacy that is HD/1080p
    VideoEncodeMemoryProfile_t VideoEncodeMemoryProfile;

    // This information enables possible memory optimization for the next encode_stream to be created
    // adjusting preproc buffer pool size vs incoming color format
    // Information is provided by STKPI client through stm_se_encode_set_control()
    // By default, 422YUYV input color format is not supported
    // This enable to save 1 extra preproc buffer sized with input resolution as 422YUYV requires a specific color format conversion
    surface_format_t VideoEncodeInputColorFormatForecasted;

    // Save selected NRT mode, as set by STM_SE_CTRL_ENCODE_NRT_MODE control
    bool                  IsNRT;

    // Linked list of existing encode stream instances
    EncodeStream_t        ListOfStreams;

    Encode_t              Next;

    BufferManager_t       BufferManager;

    BufferPool_t          ControlStructurePool;

    unsigned int          EncodeID;
    static unsigned int   sEncodeIdGene;

    EncoderStatus_t       GenerateID(void);

    EncoderStatus_t       SetMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile);
    EncoderStatus_t       SetInputColorFormat(surface_format_t InputColorFormat);
    EncoderStatus_t       SetNrtMode(uint32_t NrtMode);

private:
    DISALLOW_COPY_AND_ASSIGN(Encode_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Encode_c
\brief An Encode encapsulates the encode pipeline of a set of streams which share the same time domain.

*/

#endif /* ENCODE_H */
