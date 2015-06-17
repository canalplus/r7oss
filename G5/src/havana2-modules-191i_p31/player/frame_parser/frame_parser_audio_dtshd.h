/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : frame_parser_audio_dtshd.h
Author :           Sylvain

Definition of the frame parser audio eac3 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08Jun-07   Ported from player1                              Sylvain

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO_DTSHD
#define H_FRAME_PARSER_AUDIO_DTSHD

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "dtshd.h"
#include "frame_parser_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_AudioDtshd_c : public FrameParser_Audio_c
{
private:

    // Data
    
    DtshdAudioParsedFrameHeader_t ParsedFrameHeader;
    
    DtshdAudioStreamParameters_t	* StreamParameters;
    DtshdAudioStreamParameters_t      CurrentStreamParameters;
    DtshdAudioFrameParameters_t     * FrameParameters;
    
    bool FirstTime;
    bool DecodeLowestExtensionId;      ///< in case of multiple extensions, decode the lower extension id (by default)
    unsigned char DecodeExtensionId;   ///< if DecodeLowestExtensionId is false, decode the extension id indicated by this field
    unsigned char SelectedAudioPresentation;   ///< if DecodeLowestExtensionId is false, decode the extension id indicated by this field

    

    // Functions
public:

    //
    // Constructor function
    //

    FrameParser_AudioDtshd_c( void );
    ~FrameParser_AudioDtshd_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(		void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t		Ring );

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders( 					void );
    FrameParserStatus_t   ResetReferenceFrameList(			void );
    FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(	void );
    FrameParserStatus_t   PrepareReferenceFrameList(			void );
    FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(	void );
    FrameParserStatus_t   GeneratePostDecodeParameterSettings(		void );
    FrameParserStatus_t   UpdateReferenceFrameList(			void );

    FrameParserStatus_t   ProcessReverseDecodeUnsatisfiedReferenceStack(void );
    FrameParserStatus_t   ProcessReverseDecodeStack(			void );
    FrameParserStatus_t   PurgeReverseDecodeUnsatisfiedReferenceStack(	void );
    FrameParserStatus_t   PurgeReverseDecodeStack(			void );
    FrameParserStatus_t   TestForTrickModeFrameDrop(			void );

    FrameParserStatus_t ParseFrameHeader( unsigned char *FrameHeader, 
                                          DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                          int RemainingElementaryLength );
    
    static FrameParserStatus_t ParseSingleFrameHeader( unsigned char *FrameHeaderBytes, 
                                                       DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,  
                                                       BitStreamClass_c *Bits,
                                                       unsigned int FrameHeaderLength,
                                                       unsigned char * OtherFrameHeaderBytes,
                                                       unsigned int RemainingFrameHeaderBytes,
                                                       DtshdParseModel_t Model,
                                                       unsigned char SelectedAudioPresentation );

    static FrameParserStatus_t ParseCoreHeader( BitStreamClass_c * Bits, 
                                                DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                unsigned int SyncWord );

    static void ParseExtensionSubstreamAssetHeader( BitStreamClass_c *Bits,
                                                    unsigned int * SamplingFrequency,
                                                    DtshdAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                    unsigned int nuBits4ExSSFsize,
                                                    unsigned char SelectedAudioPresentation );

    static unsigned short CrcUpdate4BitsFast( unsigned char val, unsigned short crc );

    static unsigned int NumSpkrTableLookUp(unsigned int ChannelMask);

    static unsigned int CountBitsSet_to_1(unsigned int ChannelMask);

    static void GetSubstreamOnlyNumberOfSamples(BitStreamClass_c * Bits, 
                                                DtshdAudioParsedFrameHeader_t * ParsedFrameHeader, 
                                                unsigned char * FrameHeaderBytes);
    
};

#endif /* H_FRAME_PARSER_AUDIO_DTSHD */

