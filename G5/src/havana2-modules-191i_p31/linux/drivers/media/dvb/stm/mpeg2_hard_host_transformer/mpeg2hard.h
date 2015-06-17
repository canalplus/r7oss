/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : mpeg2hard.h
Author :           Nick

Definition of the mpeg2 frame based codec interface for
hard mpeg2 decoder on the stm8000



Date        Modification                                    Name
----        ------------                                    --------
01-Dec-03   Created                                         Nick

************************************************************************/

#ifndef H_MPEG2HARD
#define H_MPEG2HARD

/*************/
/* Constants */
/*************/

// decodingFlags bitfields
#define TOP_FIELD_FIRST             (1 << 0)
#define FRAME_PRED_FRAME_DCT        (1 << 1)
#define CONCEALMENT_MOTION_VECTORS  (1 << 2)
#define QSCALE_TYPE                 (1 << 3)
#define INTRA_VLC_FORMAT            (1 << 4)
#define ALTERNATE_SCAN              (1 << 5)
#define PROGRESSIVE                 (1 << 6)
/* 
Semaphore isn't used as there is a bug in the HW cell which
can mean it doesn't return you an interrupt causing some 
'interesting' glitches.
*/
#define USE_SEMAPHORE 0


#define MPEG2_INTERRUPT_NUMBER 148
#define QUANTISER_MATRIX_SIZE 64

/* Definition of picture_coding_type values */
#define PICTURE_CODING_TYPE_I           1
#define PICTURE_CODING_TYPE_P           2
#define PICTURE_CODING_TYPE_B           3
#define PICTURE_CODING_TYPE_D_IN_MPEG1  4

/* Definition of picture_structure values */
#define PICTURE_STRUCTURE_TOP_FIELD     1
#define PICTURE_STRUCTURE_BOTTOM_FIELD  2
#define PICTURE_STRUCTURE_FRAME_PICTURE 3


typedef enum
{
    MPEG2HARD_NO_ERROR,
    MPEG2HARD_ERROR,
    MPEG2HARD_NO_MEMORY,
    MPEG2HARD_DECODE_FIRST_FIELD
} Mpeg2HardStatus_t;

//

typedef void    *Mpeg2HardHandle_t;

typedef enum MpegStreamType_e
{
	MpegStreamTypeMpeg1 = 0,
	MpegStreamTypeMpeg2 = 1
} MpegStreamType_t;
 
typedef struct Mpeg2HardSequenceParams_s
{
    MpegStreamType_t             mpegStreamType;
    unsigned int                 horizontalSize;
    unsigned int                 verticalSize;
    unsigned int                 chromaFormat;
    unsigned char               *intraQuantizerMatrix;
    unsigned char               *nonIntraQuantizerMatrix;
} Mpeg2HardSequenceParams_t;

typedef struct Mpeg2HardCodecContext_s
{
	    MpegStreamType_t    MpegStreamType;
	    unsigned int        IntraQuantizerMatrix[QUANTISER_MATRIX_SIZE/sizeof(unsigned int)];
	    unsigned int        NonIntraQuantizerMatrix[QUANTISER_MATRIX_SIZE/sizeof(unsigned int)];
	    unsigned int        WidthInMacroBlocks;
	    unsigned int        HeightInMacroBlocks;
	    unsigned char      *LumaDecodeFrameBuffer;
	
} Mpeg2HardCodecContext_t;

typedef struct Mpeg2HardFrameParams_s
{
    unsigned char               *compressedDataFrame;
    unsigned int                 compressedDataSize;
    unsigned char               *lumaDecodeFramebuffer;                 // 32 bit aligned
    unsigned char               *chromaDecodeFramebuffer;
    unsigned char               *decimatedLumaDecodeFramebuffer;        // 32 bit aligned
    unsigned char               *decimatedChromaDecodeFramebuffer;
    unsigned char               *lumaBackwardReferenceFrame;            // 32 bit aligned
    unsigned char               *chromaBackwardReferenceFrame;
    unsigned char               *lumaForwardReferenceFrame;             // 32 bit aligned
    unsigned char               *chromaForwardReferenceFrame;
    unsigned char                pictureCodingType;                     // 3 bits
    unsigned char                forwardHorizontalMotionVector;         // 4 bits
    unsigned char                forwardVerticalMotionVector;           // 4 bits
    unsigned char                backwardHorizontalMotionVector;        // 4 bits
    unsigned char                backwardVerticalMotionVector;          // 4 bits
    unsigned char                intraDCPrecision;                      // 2 bits
    unsigned char                pictureStructure;                      // 2 bits
    unsigned char                decodingFlags;                         // 5 bits
    unsigned char                horizontalDecimationFactor;
    unsigned char                verticalDecimationFactor;	
} Mpeg2HardFrameParams_t;

typedef struct Mpeg2HardInitParams_s
{
    void                        *Reserved;
} Mpeg2HardInitParams_t;

#ifdef __cplusplus
extern "C" {
#endif
	
Mpeg2HardStatus_t Mpeg2HardInit( char *Name,
				 Mpeg2HardInitParams_t            *InitParams,
				 Mpeg2HardHandle_t                *Handle );

Mpeg2HardStatus_t Mpeg2HardSetSequenceParams( Mpeg2HardHandle_t                 Handle,
					      Mpeg2HardSequenceParams_t        *SequenceParams );
                                        
Mpeg2HardStatus_t Mpeg2HardDecodeFrame( Mpeg2HardHandle_t Handle,
					Mpeg2HardFrameParams_t *FrameParams );

Mpeg2HardStatus_t Mpeg2HardTerminate(   Mpeg2HardHandle_t Handle );

Mpeg2HardStatus_t Mpeg2InterruptUninstall( void );
Mpeg2HardStatus_t Mpeg2InterruptInstall( void );
	
	/*
static bool                      SharedContextInitialized       = false;
static OSDEV_Semaphore_t         DecoderLock;
static Mpeg2HardCodecContext_t  *LastStreamContextLoaded        = NULL;

#if USE_SEMAPHORE
static OSDEV_Semaphore_t         DecodeDone;
#else
static wait_queue_head_t         DecodeDone;
static bool                      DecodeComplete = false;	
#endif
*/	
#ifdef __cplusplus
}       
#endif 
	
#endif // H_MPEG2HARD
