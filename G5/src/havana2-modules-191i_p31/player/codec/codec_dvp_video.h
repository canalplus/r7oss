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

Source file name : codec_dvp.h
Author :           Chris

Definition of the DVP null codec class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
07-Aug-07   Created                                         Chris

************************************************************************/

#ifndef H_CODEC_DVP_VIDEO
#define H_CODEC_DVP_VIDEO

#include "codec.h"

class Codec_DvpVideo_c : public Codec_c
{
protected:
    
    // Data

    BufferManager_t                       BufferManager;
    bool                                  DataTypesInitialized;

    CodecTrickModeParameters_t		  DvpTrickModeParameters;
   
    BufferPool_t                          DecodeBufferPool;

    Ring_t                                OutputRing;
    
    // Functions

public:

    //
    // Constructor/Destructor methods
    //

    Codec_DvpVideo_c(		void );
    ~Codec_DvpVideo_c(		void );

    //
    // Standard class functions. 
    //
	
    CodecStatus_t   GetTrickModeParameters(     CodecTrickModeParameters_t      *TrickModeParameters );
	
    CodecStatus_t   RegisterOutputBufferRing(   Ring_t                    Ring );	
	
    CodecStatus_t   ReleaseDecodeBuffer(        Buffer_t                  Buffer );	
	
    CodecStatus_t   Input(                      Buffer_t                  CodedBuffer );

    //
    // Stubbed out functions that do nothin a dvp situation
    //

    CodecStatus_t   OutputPartialDecodeBuffers( void )							{return CodecNoError;}
    CodecStatus_t   DiscardQueuedDecodes(       void )							{return CodecNoError;}
    CodecStatus_t   ReleaseReferenceFrame(	unsigned int		ReferenceFrameDecodeIndex )	{return CodecNoError;}
    CodecStatus_t   CheckReferenceFrameList(	unsigned int		NumberOfReferenceFrameLists,
						ReferenceFrameList_t    ReferenceFrameList[] )		{return CodecNoError;}

    //
    // Stubbed out base class function that wrapper expects me to have
    //

    CodecStatus_t   SetModuleParameters(	unsigned int		 ParameterBlockSize,
						void			*ParameterBlock )		{return CodecNoError;}

};

#endif // H_CODEC_DVP            
