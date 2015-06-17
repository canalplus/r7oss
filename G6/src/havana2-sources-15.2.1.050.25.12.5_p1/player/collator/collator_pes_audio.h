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

#ifndef H_COLLATOR_PES_AUDIO
#define H_COLLATOR_PES_AUDIO

#include "collator_pes.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_PesAudio_c"

/// The maximum length (in bytes) of any audio codec's frame header
#define MAX_FRAME_HEADER_LENGTH 65
/// the maximum length (in bytes) of any codec's pes private data area
#define MAX_PES_PRIVATE_DATA_LENGTH 16

typedef struct Collator_PesAdInfo_s
{
    //Audio Descriptor related information
    unsigned int  NextADFadeValue;
    unsigned int  NextADPanValue;
    unsigned int  NextADGainCenter;
    unsigned int  NextADGainFront;
    unsigned int  NextADGainSurround;
    bool          NextADValidFlag;
    bool          NextADInfoAvailable;
} Collator_PesAdInfo_t;

class Collator_PesAudio_c : public Collator_Pes_c
{
public:
    explicit Collator_PesAudio_c(unsigned int frameheaderlength);
    virtual ~Collator_PesAudio_c(void) {}

    CollatorStatus_t   InternalFrameFlush(void);

    CollatorStatus_t   DiscardAccumulatedData(void);

    CollatorStatus_t   ValidateCollatedFrame(void);

protected:
    // The collator states
    typedef enum
    {
        SeekingSyncWord, ///< means the parser is searching for a synchronization word.
        GotSynchronized, ///< means the parser got synchronized, but hasn't analysed the frame header yet
        SeekingFrameEnd, ///< means the parser needs to analyse at least next sub stream to complete the frame
        ReadSubFrame,    ///< means the parser will read and accumulate the last analysed subframe
        SkipSubFrame,    ///< means the parser will skip the last analysed subframe
        GotCompleteFrame, ///< means the parser has a complete frame, and is ready to pass it to the frame parser
        ValidateFrame     ///< means the parser has a frame with a valid SYncword, but there was no sync word at the end. So search for a sync word in the collated data
    } CollatorState_t;

    /// Trailing bytes of the last unit of data scanned for a synchronization sequence.
    unsigned char PotentialFrameHeader[MAX_FRAME_HEADER_LENGTH];

    CollatorState_t CollatorState; ///< State of the elementary parser

    /// True if the PES private data should be passed to the elementary stream handler.
    ///
    /// Typically this will be true either if the stream has no private data area (e.g. broadcast AC3)
    /// or if the PES private data must be considered part of the elementary stream (e.g. DVD-LPCM).
    /// For streams with PES private data that is not considered part of the elementary stream
    /// (e.g. DVD AC3) this value should be false.
    ///
    /// This value should also be true when the elementary stream collator is not locked to the stream
    /// because, if the stream has no private data area, the sync word may appear in the bytes that
    /// are not a private data area but might have been.
    bool PassPesPrivateDataToElementaryStreamHandler;

    /// True if the incoming data should be discarded rather than processed.
    /// This is used to implement PES filters.
    bool DiscardPesPacket;

    /// True if the incoming data can be safely reprocessed during error recovery.
    /// Reprocessing data interferes with some of the recovery heuristics for DVD
    /// streams and should be disabled in that instance.
    bool ReprocessAccumulatedDataDuringErrorRecovery;

    /// Used to detect re-entry of the error recovery code.
    bool AlreadyHandlingMissingNextFrameHeader;

    unsigned int PotentialFrameHeaderLength; ///< Number of bytes stored in PotentialFrameHeader.

    unsigned char *StoredFrameHeader; ///< Pointer to the first byte of the frame header.

    unsigned int RemainingElementaryLength; ///< Number to bytes to be consumed by the HandleElementaryStream loop.
    unsigned char *RemainingElementaryData; ///< Data to be consumed by the HandleElementaryStream loop.
    unsigned char *RemainingElementaryOrigin; ///< Start of data to be consumed by the HandleElementaryStream loop, when we eneter the loop.

    unsigned int FrameHeaderLength; ///< Number of bytes that must be accumulated for the frame length can be calculated.
    unsigned int Framecount;

    virtual CollatorStatus_t FindNextSyncWord(int *CodeOffset) = 0;
    virtual CollatorStatus_t DecideCollatorNextStateAndGetLength(unsigned int *FrameLength) = 0;
    virtual void             SetPesPrivateDataLength(unsigned char SpecificCode) = 0;
    virtual CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);

    virtual void             ResetCollatorStateAfterForcedFrameFlush();

    /// Determine the offset into the current packet of the RemainingElementaryData pointer.
    inline int GetOffsetIntoPacket()
    {
        return (PesPayloadLength - PesPayloadRemaining) + (RemainingElementaryData - RemainingElementaryOrigin);
    }

    CollatorStatus_t   AccumulateData(unsigned int              Length,
                                      unsigned char            *Data);

private:
    // Control Data Management
    enum ControlDataDetectionState
    {
        SeekingControlData,
        GotPartialControlData,
        GotControlData
    };

    ControlDataDetectionState       mControlDataDetectionState;
    int                             mControlDataTrailingStartCodeBytes;
    int                             mGotPartialControlDataBytes;
    unsigned char                  *mControlDataRemainingData;
    int                             mControlDataRemainingLength;
    unsigned char                   mStoredControlData[PES_CONTROL_SIZE];

    unsigned int PesPayloadRemaining; ///< Number of bytes until the next PES header.

    /// Non-zero if the end of the last scanned section contains a partial PES start code.
    ///
    /// Valid values:
    /// - 0 means there is nothing if interest in the previous block.
    /// - 1 means that the final byte of the last block was zero.
    /// - 2 means that the final two bytes of the last block were both zero.
    /// - 3 means that the final three bytes of the last block were 0, 0, 1 respectively.
    ///
    /// All other values are illegal.
    unsigned int TrailingStartCodeBytes;

    unsigned int GotPartialFrameHeaderBytes; ///< The number of accumulated frame header (in bytes).

    int FramePayloadRemaining; ///< Number of bytes until the next frame header.

    bool AccumulatedFrameReady; ///< True if we have got a frame but must check the next frame header before emitting it.

    bool                NextPlaybackTimeValid;
    unsigned long long  NextPlaybackTime;
    bool                NextDecodeTimeValid;
    unsigned long long  NextDecodeTime;

    Collator_PesAdInfo_t  NextPesADInfo;

    CollatorStatus_t HandleWronglyAccumulatedControlData(PlayerInputDescriptor_t *Input, bool NonBlocking, unsigned int *DataLengthRemaining);

    bool FindNextControlDataStartCode(unsigned char *data, int length, int *CodeOffset, int *trailingStartCodeBytes);
    void SearchForControlData(void);
    void ReadPartialControlData(void);

    CollatorStatus_t ProcessControlData(void);
    CollatorStatus_t SearchForSyncWord(void);
    CollatorStatus_t ReadPartialFrameHeader(void);
    CollatorStatus_t HandleMissingNextFrameHeader(void);
    CollatorStatus_t ReadFrame(void);

    CollatorStatus_t HandleElementaryStream(unsigned int Length, unsigned char *Data);

    CollatorStatus_t SearchForPesHeader(void);
    CollatorStatus_t ReadPartialPesHeader(void);
    CollatorStatus_t ReadPesPacket(void);

    CollatorStatus_t InputSecondStage(PlayerInputDescriptor_t  *Input,
                                      unsigned int              DataLength,
                                      void                     *Data,
                                      bool                      NonBlocking = false,
                                      unsigned int             *DataLengthRemaining = NULL);

    // derived classes can override this method if control data insertion
    // is not supported by some formats
    virtual bool ESSupportsControlDataInsertion() { return true; }

    virtual CollatorStatus_t   DoInput(PlayerInputDescriptor_t *Input,
                                       unsigned int             DataLength,
                                       void                    *Data,
                                       bool                     NonBlocking,
                                       unsigned int            *DataLengthRemaining);

    DISALLOW_COPY_AND_ASSIGN(Collator_PesAudio_c);
};

#endif

