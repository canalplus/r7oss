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

#ifndef H_FRAME_PARSER_BASE
#define H_FRAME_PARSER_BASE

#include "bitstream_class.h"
#include "player.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Base_c"

// /////////////////////////////////////////////////////////////////////////

#define MarkerBit( v )      if( Bits.Get(1) != (v) )                                    \
                {                                               \
                    SE_ERROR("Invalid marker bit value\n"); \
                    return FrameParserHeaderSyntaxError;                            \
                }

#define MarkerBits( n, v )  if( Bits.Get((n)) != (v) )                                  \
                {                                               \
                    SE_ERROR("Invalid marker bits value\n");    \
                    return FrameParserHeaderSyntaxError;                            \
                }

// Cleanup copies of the marker bit tests, that release a temporarily held buffer

#define MarkerBitClean( v ) if( Bits.Get(1) != (v) )                                    \
                {                                               \
                    SE_ERROR("Invalid marker bit value\n"); \
                    return FrameParserHeaderSyntaxError;                            \
                }

#define MarkerBitsClean( n, v ) if( Bits.Get((n)) != (v) )                                  \
                {                                               \
                    SE_ERROR("Invalid marker bits value\n");    \
                    return FrameParserHeaderSyntaxError;                            \
                }


typedef struct FrameParserConfiguration_s
{
    const char               *FrameParserName;
    unsigned int              MaxUserDataBlocks;

    unsigned int              StreamParametersCount;
    BufferDataDescriptor_t   *StreamParametersDescriptor;

    unsigned int              FrameParametersCount;
    BufferDataDescriptor_t   *FrameParametersDescriptor;

    unsigned int              MaxReferenceFrameCount;   // Used in reverse play resource management

    bool                      SupportSmoothReversePlay; // Can we support smooth reverse on this stream

    bool                      InitializeStartCodeList;  // Do we need to extract the start code list from input
} FrameParserConfiguration_t;


class FrameParser_Base_c : public FrameParser_c
{
public:
    FrameParser_Base_c(void);
    ~FrameParser_Base_c(void);

    FrameParserStatus_t   Halt(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    FrameParserStatus_t   Input(Buffer_t          CodedBuffer);

    // Retun 20 : minimum estimated amount of video frame references
    // This value could be better estimated after stream parsing
    virtual unsigned int  GetMaxReferenceCount(void) const { return 20; };

    FrameParserStatus_t   TranslatePlaybackTimeNativeToNormalized(
        unsigned long long    NativeTime,
        unsigned long long   *NormalizedTime,
        PlayerTimeFormat_t    NativeTimeFormat = TimeFormatPts);

    FrameParserStatus_t   TranslatePlaybackTimeNormalizedToNative(
        unsigned long long    NormalizedTime,
        unsigned long long   *NativeTime,
        PlayerTimeFormat_t    NativeTimeFormat = TimeFormatPts);

    FrameParserStatus_t   GetNextDecodeFrameIndex(unsigned int       *Index);
    FrameParserStatus_t   SetNextFrameIndices(unsigned int        Value);

    // Class function for default non-implemented functionality

    FrameParserStatus_t   ResetCollatedHeaderState(void) { return PlayerNotSupported; }
    unsigned int      RequiredPresentationLength(unsigned char        StartCode)   { return 0; }
    FrameParserStatus_t   PresentCollatedHeader(unsigned char         StartCode,
                                                unsigned char        *HeaderBytes,
                                                FrameParserHeaderFlag_t  *Flags)   { return PlayerNotSupported; }

    void                IncrementErrorStatistics(FrameParserStatus_t  Status);

    // User data functions

    FrameParserStatus_t ReadUserData(unsigned int UserDataLength, unsigned char *InputBufferData);
    FrameParserStatus_t FillUserDataBuffer(void);
    FrameParserStatus_t ResetReverseFailureCounter(void) {return FrameParserNoError;};
    FrameParserStatus_t SetSmoothReverseSupport(bool SmoothReverseSupport) ;

    //
    // Extensions to the class to support my inheritors
    //
protected:
    OS_Mutex_t                 Lock;
    FrameParserConfiguration_t Configuration;

    BufferPool_t              CodedFrameBufferPool;
    unsigned int              FrameBufferCount;
    BufferPool_t              DecodeBufferPool;
    unsigned int              DecodeBufferCount;
    Port_c                   *mOutputPort;

    BufferManager_t           BufferManager;

    BufferDataDescriptor_t   *StreamParametersDescriptor;
    BufferType_t              StreamParametersType;
    BufferPool_t              StreamParametersPool;
    Buffer_t                  StreamParametersBuffer;

    BufferDataDescriptor_t   *FrameParametersDescriptor;
    BufferType_t              FrameParametersType;
    BufferPool_t              FrameParametersPool;
    Buffer_t                  FrameParametersBuffer;

    Buffer_t                  Buffer;
    unsigned int              BufferLength;
    unsigned char            *BufferData;
    CodedFrameParameters_t   *CodedFrameParameters;
    ParsedFrameParameters_t  *ParsedFrameParameters;
    StartCodeList_t          *StartCodeList;

    bool                      FirstDecodeAfterInputJump;
    bool                      SurplusDataInjected;
    bool                      ContinuousReverseJump;

    unsigned int              NextDecodeFrameIndex;
    unsigned int              NextDisplayFrameIndex;

    unsigned long long        NativeTimeBaseLine;
    unsigned long long        LastNativeTimeUsedInBaseline;

    BitStreamClass_c          Bits;
    bool                      FrameToDecode;

    Rational_t                PlaybackSpeed;
    PlayDirection_t           PlaybackDirection;

    UserData_t                AccumulatedUserData[MAXIMUM_USER_DATA_BUFFERS];  // used to accumulate user data
    // user data index is reflecting the number of user data to be associated to the parsed picture
    unsigned char             UserDataIndex;

    FrameParserStatus_t   RegisterStreamAndFrameDescriptors(void);

    FrameParserStatus_t   GetNewStreamParameters(void           **Pointer);
    FrameParserStatus_t   GetNewFrameParameters(void            **Pointer);

    //
    // Extensions to the class that may be overridden by my inheritors
    //

    virtual FrameParserStatus_t   ProcessBuffer(void);
    virtual FrameParserStatus_t   QueueFrameForDecode(void);

    //
    // Extensions to the class to be fulfilled by my inheritors, all defaulted to null functions
    //
    // Note: This functions are doxygenated at the end of frame_parser_base.cpp, please keep the
    // doxygen comments up to date.
    //

    virtual FrameParserStatus_t   ReadHeaders(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(void) {return FrameParserNoError;}
    virtual FrameParserStatus_t   GeneratePostDecodeParameterSettings(void) {return FrameParserNoError;}

    virtual bool ReadAdditionalUserDataParameters(void) {return false;}

private:
    DISALLOW_COPY_AND_ASSIGN(FrameParser_Base_c);
};

#endif
