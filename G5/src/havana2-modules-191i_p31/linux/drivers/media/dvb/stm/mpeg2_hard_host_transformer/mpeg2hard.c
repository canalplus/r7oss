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

Source file name : mpeg2hard.c
Author :           Nick & Chris

Implemetation of the frame decoding interface to the hardware mpeg2 module
on the STx71XX



Date        Modification                                    Name
----        ------------                                    --------
01-Dec-03   Created                                         Nick
04-Jun-07   Version ported into player2 tree, slight mods   Chris

************************************************************************/

#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "osdev_device.h"
#include "registers.h"
#include "mpeg2hard.h"

// ////////////////////////////////////////////////////////////////////////////////
//
//    Local data

static bool                      SharedContextInitialized       = false;
static OSDEV_Semaphore_t         DecoderLock;
static Mpeg2HardCodecContext_t  *LastStreamContextLoaded        = NULL;
static unsigned int              InterruptStatus;

#if USE_SEMAPHORE
static OSDEV_Semaphore_t         DecodeDone;
#else
static wait_queue_head_t         DecodeDone;
static bool                      DecodeComplete = false;
#endif

// ////////////////////////////////////////////////////////////////////////////////
//
//    Prototypes

static OSDEV_InterruptHandlerEntrypoint( Mpeg2InterruptHandler );

// ////////////////////////////////////////////////////////////////////////////////
//
//    The Initialize function

Mpeg2HardStatus_t Mpeg2HardInit(        char                             *Name,
					Mpeg2HardInitParams_t            *InitParams,
					Mpeg2HardHandle_t                *Handle )
{
	Mpeg2HardCodecContext_t  *Context;
	unsigned int              Value;


	MapRegisters();

	// First check to see if we need a new shared context

	if( !SharedContextInitialized )
	{
		OSDEV_InitializeSemaphore( &DecoderLock, 1 );
		LastStreamContextLoaded         = NULL;

		// Initialize the hardware block
		WriteRegister( VID_IC_CFG, 0x0000 );

		// Clear interrupt mask, and status bits

		WriteRegister( VID_ITM(0), 0x0000 );                            // We don't use interrupts here
		Value = ReadRegister( VID_ITS(0) );                             // Clear status bits

		// Initialize bit buffer

		WriteRegister( VID_BBS(0), 0x0000 );                            // Set bit buffer start
		WriteRegister( VID_BBE(0), 0xFFFFFFFF );                            // Set bit buffer end

		WriteRegister( VID_VLD_PTR(0), (unsigned int)NULL );            // VLD data
		WriteRegister( VID_VLDRL(0),  (unsigned int)NULL );             // VLD data limit
		// Note we do not use the read limit.

		// Soft reset
		WriteRegister( VID_SRS(0), VID_SRS__SRC );                      // Soft reset

		// Install interrupt handler
		if( Mpeg2InterruptInstall() != MPEG2HARD_NO_ERROR )
			return MPEG2HARD_ERROR;

		SharedContextInitialized        = true;
	}

	// Obtain a decoder context
	Context     = (Mpeg2HardCodecContext_t  *)OSDEV_Malloc( sizeof(Mpeg2HardCodecContext_t) );

	if( Context == NULL )
	{
		OSDEV_Print( "Mpeg2HardInit - Unable to allocate memory for context structure\n" );
		return MPEG2HARD_NO_MEMORY;
	}

	memset( Context, 0x00, sizeof(Mpeg2HardCodecContext_t) );
	*Handle     = (void *)Context;


	// Initialise decode semaphore
#if USE_SEMAPHORE
	OSDEV_InitializeSemaphore( &DecodeDone, 0 );
#else
	init_waitqueue_head( &DecodeDone );
	DecodeComplete      = false;
#endif

    return MPEG2HARD_NO_ERROR;

}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Set stream parameters function
//
//      NOTE this function merely stores the stream context values. In the hardware
//      decoder, since we may be decoding multiple streams, the context must be re-loaded
//      whenever we switch streams.
//

Mpeg2HardStatus_t Mpeg2HardSetSequenceParams(
					Mpeg2HardHandle_t                 Handle,
					Mpeg2HardSequenceParams_t        *SequenceParams )
{
	Mpeg2HardCodecContext_t  *Context = (Mpeg2HardCodecContext_t  *)Handle;

	// Copy the 2 quantization matrices
	memcpy( Context->IntraQuantizerMatrix, SequenceParams->intraQuantizerMatrix, QUANTISER_MATRIX_SIZE );
	memcpy( Context->NonIntraQuantizerMatrix, SequenceParams->nonIntraQuantizerMatrix, QUANTISER_MATRIX_SIZE );

	Context->MpegStreamType             = SequenceParams->mpegStreamType;


	// Store the width/height in macroblocks NOTE number of macroblocks must be even.

	Context->WidthInMacroBlocks         = (SequenceParams->horizontalSize+15)/16;
	Context->HeightInMacroBlocks        = (SequenceParams->verticalSize+15)/16;

	// Set the last decode buffer to null to indicate first field if field pictures.
	Context->LumaDecodeFrameBuffer      = NULL;

	// Force re-loading if this is a change to currently loaded values
	if( Context == LastStreamContextLoaded )
		LastStreamContextLoaded         = NULL;

#if 0

	OSDEV_Print( "MPEG Stream type: %d\n", SequenceParams->mpegStreamType);
	OSDEV_Print( "Horizontal Size:  %d\n", SequenceParams->horizontalSize);
	OSDEV_Print( "Vertical Size:    %d\n", SequenceParams->verticalSize);
	OSDEV_Print( "Chroma Format:    %d\n", SequenceParams->chromaFormat);

	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[0],SequenceParams->intraQuantizerMatrix[1],
		SequenceParams->intraQuantizerMatrix[2],SequenceParams->intraQuantizerMatrix[3],SequenceParams->intraQuantizerMatrix[4],
		SequenceParams->intraQuantizerMatrix[5],SequenceParams->intraQuantizerMatrix[6],SequenceParams->intraQuantizerMatrix[7]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[8],SequenceParams->intraQuantizerMatrix[9],
		SequenceParams->intraQuantizerMatrix[10],SequenceParams->intraQuantizerMatrix[11],SequenceParams->intraQuantizerMatrix[12],
		SequenceParams->intraQuantizerMatrix[13],SequenceParams->intraQuantizerMatrix[14],SequenceParams->intraQuantizerMatrix[15]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[16],SequenceParams->intraQuantizerMatrix[17],
		SequenceParams->intraQuantizerMatrix[18],SequenceParams->intraQuantizerMatrix[19],SequenceParams->intraQuantizerMatrix[20],
		SequenceParams->intraQuantizerMatrix[21],SequenceParams->intraQuantizerMatrix[22],SequenceParams->intraQuantizerMatrix[23]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[24],SequenceParams->intraQuantizerMatrix[25],
		SequenceParams->intraQuantizerMatrix[26],SequenceParams->intraQuantizerMatrix[27],SequenceParams->intraQuantizerMatrix[28],
		SequenceParams->intraQuantizerMatrix[29],SequenceParams->intraQuantizerMatrix[30],SequenceParams->intraQuantizerMatrix[31]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[32],SequenceParams->intraQuantizerMatrix[33],
		SequenceParams->intraQuantizerMatrix[34],SequenceParams->intraQuantizerMatrix[35],SequenceParams->intraQuantizerMatrix[36],
		SequenceParams->intraQuantizerMatrix[37],SequenceParams->intraQuantizerMatrix[38],SequenceParams->intraQuantizerMatrix[39]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[40],SequenceParams->intraQuantizerMatrix[41],
		SequenceParams->intraQuantizerMatrix[42],SequenceParams->intraQuantizerMatrix[43],SequenceParams->intraQuantizerMatrix[44],
		SequenceParams->intraQuantizerMatrix[45],SequenceParams->intraQuantizerMatrix[46],SequenceParams->intraQuantizerMatrix[47]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[48],SequenceParams->intraQuantizerMatrix[49],
		SequenceParams->intraQuantizerMatrix[50],SequenceParams->intraQuantizerMatrix[51],SequenceParams->intraQuantizerMatrix[52],
		SequenceParams->intraQuantizerMatrix[53],SequenceParams->intraQuantizerMatrix[54],SequenceParams->intraQuantizerMatrix[55]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->intraQuantizerMatrix[56],SequenceParams->intraQuantizerMatrix[57],
		SequenceParams->intraQuantizerMatrix[58],SequenceParams->intraQuantizerMatrix[59],SequenceParams->intraQuantizerMatrix[60],
		SequenceParams->intraQuantizerMatrix[61],SequenceParams->intraQuantizerMatrix[62],SequenceParams->intraQuantizerMatrix[63]);


	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[0],SequenceParams->nonIntraQuantizerMatrix[1],
		SequenceParams->nonIntraQuantizerMatrix[2],SequenceParams->nonIntraQuantizerMatrix[3],SequenceParams->nonIntraQuantizerMatrix[4],
		SequenceParams->nonIntraQuantizerMatrix[5],SequenceParams->nonIntraQuantizerMatrix[6],SequenceParams->nonIntraQuantizerMatrix[7]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[8],SequenceParams->nonIntraQuantizerMatrix[9],
		SequenceParams->nonIntraQuantizerMatrix[10],SequenceParams->nonIntraQuantizerMatrix[11],SequenceParams->nonIntraQuantizerMatrix[12],
		SequenceParams->nonIntraQuantizerMatrix[13],SequenceParams->nonIntraQuantizerMatrix[14],SequenceParams->nonIntraQuantizerMatrix[15]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[16],SequenceParams->nonIntraQuantizerMatrix[17],
		SequenceParams->nonIntraQuantizerMatrix[18],SequenceParams->nonIntraQuantizerMatrix[19],SequenceParams->nonIntraQuantizerMatrix[20],
		SequenceParams->nonIntraQuantizerMatrix[21],SequenceParams->nonIntraQuantizerMatrix[22],SequenceParams->nonIntraQuantizerMatrix[23]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[24],SequenceParams->nonIntraQuantizerMatrix[25],
		SequenceParams->nonIntraQuantizerMatrix[26],SequenceParams->nonIntraQuantizerMatrix[27],SequenceParams->nonIntraQuantizerMatrix[28],
		SequenceParams->nonIntraQuantizerMatrix[29],SequenceParams->nonIntraQuantizerMatrix[30],SequenceParams->nonIntraQuantizerMatrix[31]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[32],SequenceParams->nonIntraQuantizerMatrix[33],
		SequenceParams->nonIntraQuantizerMatrix[34],SequenceParams->nonIntraQuantizerMatrix[35],SequenceParams->nonIntraQuantizerMatrix[36],
		SequenceParams->nonIntraQuantizerMatrix[37],SequenceParams->nonIntraQuantizerMatrix[38],SequenceParams->nonIntraQuantizerMatrix[39]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[40],SequenceParams->nonIntraQuantizerMatrix[41],
		SequenceParams->nonIntraQuantizerMatrix[42],SequenceParams->nonIntraQuantizerMatrix[43],SequenceParams->nonIntraQuantizerMatrix[44],
		SequenceParams->nonIntraQuantizerMatrix[45],SequenceParams->nonIntraQuantizerMatrix[46],SequenceParams->nonIntraQuantizerMatrix[47]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[48],SequenceParams->nonIntraQuantizerMatrix[49],
		SequenceParams->nonIntraQuantizerMatrix[50],SequenceParams->nonIntraQuantizerMatrix[51],SequenceParams->nonIntraQuantizerMatrix[52],
		SequenceParams->nonIntraQuantizerMatrix[53],SequenceParams->nonIntraQuantizerMatrix[54],SequenceParams->nonIntraQuantizerMatrix[55]);
	OSDEV_Print( "%d %d %d %d %d %d %d %d\n",SequenceParams->nonIntraQuantizerMatrix[56],SequenceParams->nonIntraQuantizerMatrix[57],
		SequenceParams->nonIntraQuantizerMatrix[58],SequenceParams->nonIntraQuantizerMatrix[59],SequenceParams->nonIntraQuantizerMatrix[60],
		SequenceParams->nonIntraQuantizerMatrix[61],SequenceParams->nonIntraQuantizerMatrix[62],SequenceParams->nonIntraQuantizerMatrix[63]);   
#endif  

	return MPEG2HARD_NO_ERROR;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    Local function to load a particular stream context

static void Mpeg2HardLoadStreamContext( Mpeg2HardCodecContext_t  *Context )
{
	unsigned int i;

	// Load the 2 quantization matrices
	for( i=0; i<QUANTISER_MATRIX_SIZE/sizeof(unsigned int); i++ )
		WriteRegister( VID0_QMWIP + (i * sizeof(unsigned int)), Context->IntraQuantizerMatrix[i] );

	for( i=0; i<QUANTISER_MATRIX_SIZE/sizeof(unsigned int); i++ )
		WriteRegister( VID0_QMWNIP + (i * sizeof(unsigned int)), Context->NonIntraQuantizerMatrix[i] );

	// Load the size registers NOTE number of macroblocks must be even.
	WriteRegister( VID_DFW(0), Context->WidthInMacroBlocks );
	WriteRegister( VID_DFH(0), Context->HeightInMacroBlocks );
	WriteRegister( VID_DFS(0), Context->WidthInMacroBlocks * Context->HeightInMacroBlocks );

	LastStreamContextLoaded     = Context;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Decode one frame function

Mpeg2HardStatus_t Mpeg2HardDecodeFrame( Mpeg2HardHandle_t                 Handle,
					Mpeg2HardFrameParams_t           *FrameParams )
{
	Mpeg2HardCodecContext_t  *Context = (Mpeg2HardCodecContext_t  *)Handle;
	unsigned int              PictureParameters;
	bool                      FirstField = true;
#if 0 // used to fill decode buffers with pretty colours
	static unsigned int       Colour = 0;
	static unsigned int       Total = 0;
#endif

	// In a multi use environment, we need to grab the decoder, and if
	// it is not currently loaded, load our stream context.
	OSDEV_ClaimSemaphore( &DecoderLock );

	if( LastStreamContextLoaded != Context )
		Mpeg2HardLoadStreamContext( Context );

#if 0 // used to fill decode buffers with pretty colours
{
	memset ((void*) (((unsigned int)FrameParams->lumaDecodeFramebuffer) | 0xa0000000),   0xff, 1920*1088 );
	memset ((void*) (((unsigned int)FrameParams->chromaDecodeFramebuffer) | 0xa0000000), Colour++ & 0xff, 1920*544 );
	OSDEV_Print( "{0x%08x, 0x%06x, 0x%06x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x},\n",
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->compressedDataFrame),
		     Total,
		     FrameParams->compressedDataSize,
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->lumaDecodeFramebuffer),
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->chromaDecodeFramebuffer),
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->lumaBackwardReferenceFrame),
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->chromaBackwardReferenceFrame),
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->lumaForwardReferenceFrame),
		     OSDEV_PeripheralAddress ((unsigned int)FrameParams->chromaForwardReferenceFrame),
		     FrameParams->pictureCodingType,
		     FrameParams->forwardHorizontalMotionVector,
		     FrameParams->forwardVerticalMotionVector,
		     FrameParams->backwardHorizontalMotionVector,
		     FrameParams->backwardVerticalMotionVector,
		     FrameParams->intraDCPrecision,
		     FrameParams->pictureStructure,
		     FrameParams->decodingFlags);
	Total += FrameParams->compressedDataSize;
	OSDEV_ReleaseSemaphore( &DecoderLock );
	return MPEG2HARD_NO_ERROR;
}
#endif

    // Update reference frame pointers if appropriate
    //    NOTE selective load allows old values to be used for error recovery on I frames
	if( FrameParams->pictureCodingType == PICTURE_CODING_TYPE_B )
	{
		WriteRegister( VID_BFP(0),  (unsigned int)FrameParams->lumaBackwardReferenceFrame );
		WriteRegister( VID_BCHP(0), (unsigned int)FrameParams->chromaBackwardReferenceFrame );
		WriteRegister( VID_FFP(0),   (unsigned int)FrameParams->lumaForwardReferenceFrame );
		WriteRegister( VID_FCHP(0),  (unsigned int)FrameParams->chromaForwardReferenceFrame );
	}
	else if( FrameParams->pictureCodingType == PICTURE_CODING_TYPE_P )
	{
		WriteRegister( VID_FFP(0),  (unsigned int)FrameParams->lumaForwardReferenceFrame );
		WriteRegister( VID_FCHP(0), (unsigned int)FrameParams->chromaForwardReferenceFrame );
	}

	// Update decode frame pointers
	WriteRegister( VID_RFP(0),  (unsigned int)FrameParams->lumaDecodeFramebuffer );
	WriteRegister( VID_RCHP(0), (unsigned int)FrameParams->chromaDecodeFramebuffer );
	

	
	// Setup decimation values
	if ((FrameParams->horizontalDecimationFactor != 1) || (FrameParams->verticalDecimationFactor != 1)) 
	{
		// bit 0 - progressive ?
		// bit 1 - enable secondry output
		// bit 2 - enable primary output
		// bit 3/4 - horizontal factor
		// bit 5/6 - vertical factor
		unsigned int rcm = 0x06;
		WriteRegister( VID_SRFP(0),  (unsigned int)FrameParams->decimatedLumaDecodeFramebuffer );
		WriteRegister( VID_SRCHP(0), (unsigned int)FrameParams->decimatedChromaDecodeFramebuffer );
		
		printk("**** RCM A 0x%08x ****\n",rcm);
		if (FrameParams->decodingFlags & PROGRESSIVE)
			rcm++;
		
		printk("**** RCM B 0x%08x ****\n",rcm);
		if (FrameParams->horizontalDecimationFactor == 2)
			rcm += (1 << 3);
		else if (FrameParams->horizontalDecimationFactor == 4)
			rcm += (1 << 4);
		
		printk("**** RCM C 0x%08x ****\n",rcm);
		if (FrameParams->verticalDecimationFactor == 2)
			rcm += (1 << 5);
		else if (FrameParams->verticalDecimationFactor == 4)
			rcm += (1 << 6);				

		printk("**** RCM D 0x%08x ****\n",rcm);
//		printk("**** Decimated Luma   0x%08x\n",FrameParams->decimatedLumaDecodeFramebuffer);
//		printk("**** Decimated Chroma 0x%08x\n",FrameParams->decimatedChromaDecodeFramebuffer);
		
		WriteRegister(VID_RCM(0),rcm);
//		WriteRegister(VID_RCM(0),0x2e);
	}
	else
		WriteRegister(VID_RCM(0), (unsigned int)( ( (FrameParams->decodingFlags & PROGRESSIVE) != 0) +0x04 ));	


	// Set Compressed data pointers
	WriteRegister( VID_VLD_PTR(0), (unsigned int)FrameParams->compressedDataFrame);  // VLD data
	WriteRegister( VID_VLDRL(0),   (unsigned int)(FrameParams->compressedDataFrame + FrameParams->compressedDataSize + 255 + 1024 ) ); // VLD data limit

//      OSDEV_Print("Compressed data pointer %x size %x\n",FrameParams->compressedDataFrame, FrameParams->compressedDataSize);

	// Setup the picture parameters
	FirstField          = (FrameParams->lumaDecodeFramebuffer != Context->LumaDecodeFrameBuffer);
	//OSDEV_Print( "MPEG Stream Tye %d (0 MPEG1 : 1 MPEG2)\n",Context->MpegStreamType);


	PictureParameters   = 
		(Context->MpegStreamType                                              << VID_PPR__MP2_SHIFT)  |
		(((FirstField) ? VID_FFN__FIRST_FIELD : VID_FFN__SECOND_FIELD)        << VID_PPR__FFN_SHIFT)  |
		(((FrameParams->decodingFlags & TOP_FIELD_FIRST) != 0)                << VID_PPR__TFF_SHIFT)  |
		(((FrameParams->decodingFlags & FRAME_PRED_FRAME_DCT) != 0)           << VID_PPR__FRM_SHIFT)  |
		(((FrameParams->decodingFlags & CONCEALMENT_MOTION_VECTORS) != 0)     << VID_PPR__CMV_SHIFT)  |
		(((FrameParams->decodingFlags & QSCALE_TYPE) != 0)                    << VID_PPR__QST_SHIFT)  |
		(((FrameParams->decodingFlags & INTRA_VLC_FORMAT) != 0)               << VID_PPR__IVF_SHIFT)  |
		(((FrameParams->decodingFlags & ALTERNATE_SCAN) != 0)                 << VID_PPR__AZZ_SHIFT)  |
		((FrameParams->pictureCodingType & VID_PPR__PCT_MASK)                 << VID_PPR__PCT_SHIFT)  |
		((FrameParams->intraDCPrecision & VID_PPR__DCP_MASK)                  << VID_PPR__DCP_SHIFT)  |
		((FrameParams->pictureStructure & VID_PPR__PST_MASK)                  << VID_PPR__PST_SHIFT);

	if (Context->MpegStreamType == 0) // MPEG 1
	{
		PictureParameters   |= 
			((FrameParams->forwardVerticalMotionVector    & 0x07) << 0) |
			((FrameParams->backwardVerticalMotionVector   & 0x07) << 4) |
			((FrameParams->forwardHorizontalMotionVector  & 0x01) << 3) |
			((FrameParams->backwardHorizontalMotionVector & 0x01) << 7);            
	}
	else // MPEG2 
	{
		PictureParameters   |= 
		((FrameParams->backwardVerticalMotionVector & VID_PPR__BFV_MASK)      << VID_PPR__BFV_SHIFT)  |
		((FrameParams->forwardVerticalMotionVector & VID_PPR__FFV_MASK)       << VID_PPR__FFV_SHIFT)  |
		((FrameParams->backwardHorizontalMotionVector & VID_PPR__BFH_MASK)    << VID_PPR__BFH_SHIFT)  |
		((FrameParams->forwardHorizontalMotionVector & VID_PPR__FFH_MASK)     << VID_PPR__FFH_SHIFT);

	}

/*      
	OSDEV_Print( "*   intra_dc_precision           : %6d\n",(FrameParams->intraDCPrecision & VID_PPR__DCP_MASK)); 
	OSDEV_Print( "*   picture_structure            : %6d\n",(FrameParams->pictureStructure & VID_PPR__PST_MASK)); 
	OSDEV_Print( "*   top_field_first              : %6d\n",((FrameParams->decodingFlags & TOP_FIELD_FIRST) != 0)); 
	OSDEV_Print( "*   frame_pred_frame_dct         : %6d\n",((FrameParams->decodingFlags & FRAME_PRED_FRAME_DCT) != 0)); 
	OSDEV_Print( "*   concealment_motion_vectors   : %6d\n",((FrameParams->decodingFlags & CONCEALMENT_MOTION_VECTORS) != 0)); 
	OSDEV_Print( "*   q_scale_type                 : %6d\n",((FrameParams->decodingFlags & QSCALE_TYPE) != 0)); 
	OSDEV_Print( "*   intra_vlc_format             : %6d\n",((FrameParams->decodingFlags & INTRA_VLC_FORMAT) != 0)); 
	OSDEV_Print( "*   alternate_scan               : %6d\n",((FrameParams->decodingFlags & ALTERNATE_SCAN) != 0));
*/      

	// Remember the decode buffer to check whether we are the second half of a field picture
	Context->LumaDecodeFrameBuffer  = FrameParams->lumaDecodeFramebuffer;

//      flush_cache_all();

	WriteRegister( VID_PPR(0), PictureParameters );

	ReadRegister( VID_ITS(0) );                                 // Clear interrupt status bits
	WriteRegister( VID_TIS(0), VID_TIS__RMM | VID_TIS__MVC );
	WriteRegister( VID_ITM(0), VID_STA__DID   |
		       VID_STA__VLDRL |
		       VID_STA__DSE   |
		       VID_STA__MSE   |
		       VID_STA__DUE   |
		       VID_STA__DOE ); // Set interrupt mask for decoder idle
	WriteRegister( VID0_EXE, VID_EXE__EXE );                    // Go

    // Now wait for the decode to complete
#if USE_SEMAPHORE
	OSDEV_ClaimSemaphore( &DecodeDone );                        // Wait for interrupt handler to signal decode complete
#else
	{
		unsigned int            DecodeTimeout;

		DecodeTimeout       = msecs_to_jiffies( 80 );           /* Set timeout for 2 frame periods at 25 frames a second */
		DecodeComplete      = false;
//              wait_event( DecodeDone, DecodeComplete);
		DecodeTimeout       = wait_event_timeout( DecodeDone, DecodeComplete, DecodeTimeout );
		if( !DecodeComplete )
		{
			unsigned int        MacroBlock;
			unsigned int        Row;
			unsigned int        Column;

			MacroBlock  = ReadRegister( VID_MBNM(0) );          /* Read macroblock decoder is working on */
			Column      = (MacroBlock >> 16) & 0x7f;
			Row         =  MacroBlock & 0x7f;
			OSDEV_Print( "***** Error-%s: Decode timeout. Decoded to macroblock row %d, column %d\n", __FUNCTION__, Row, Column );
		}
	}
#endif

	// Check statuses are as expected
	//Status = ReadRegister( VID_ITS(0) );                        // Read interrupt status bits
#if defined (ENABLE_HD)
	if( (InterruptStatus & VID_STA__DID) != VID_STA__DID )
		OSDEV_Print( "Error: Interrupt status = %08x\n", InterruptStatus );
#endif

	// Release the decoder for use, and return
	OSDEV_ReleaseSemaphore( &DecoderLock );

	if( ((FrameParams->pictureStructure == PICTURE_STRUCTURE_TOP_FIELD) || (FrameParams->pictureStructure == PICTURE_STRUCTURE_BOTTOM_FIELD)) &&
	    FirstField )
		return MPEG2HARD_DECODE_FIRST_FIELD;
	else
		return MPEG2HARD_NO_ERROR;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The terminate and tidy up function

Mpeg2HardStatus_t       Mpeg2HardTerminate( Mpeg2HardHandle_t                    Handle )
{
	Mpeg2HardCodecContext_t  *Context = (Mpeg2HardCodecContext_t  *)Handle;

	// First, if our stream context is loaded, mark it as unloaded
	if( Context == LastStreamContextLoaded )
		LastStreamContextLoaded         = NULL;

	Mpeg2InterruptUninstall();
	UnMapRegisters();

	// Free the context and return
	OSDEV_Free( Context );
	return MPEG2HARD_NO_ERROR;
}

// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler installation function

//static 
Mpeg2HardStatus_t Mpeg2InterruptInstall( void )
{

	if( request_irq( MPEG2_INTERRUPT_NUMBER, Mpeg2InterruptHandler, 0, "mpeg2", NULL ) != 0 )
	{
		OSDEV_Print( "MPEG2Hard interrupt install failure\n" );
		return MPEG2HARD_ERROR;
	}

	return MPEG2HARD_NO_ERROR;
}


// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler removal function

//static 
Mpeg2HardStatus_t Mpeg2InterruptUninstall( void )
{
	OSDEV_Print( "MPEG2 Hard interrupt uninstall (CURRENTLY DISABLED)\n" );

#if 0
	// for some reason freeing the irq prevents the MPEG decoder initialising
	// correctly when the device is reopened.
	free_irq( MPEG2_INTERRUPT_NUMBER, NULL );
#endif

	return MPEG2HARD_NO_ERROR;
}

// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler code

OSDEV_InterruptHandlerEntrypoint( Mpeg2InterruptHandler )
{
	unsigned int  Status;
	unsigned int  MacroBlock;
	unsigned int  Row;
	unsigned int  Column;

	Status     = ReadRegister( VID_ITS(0) );                    // Read interrupt status bits
	MacroBlock = ReadRegister( VID_MBNM(0) );                   // Read macroblock decoder is working on
	Column     = (MacroBlock >> 16) & 0x7f;
	Row        =  MacroBlock & 0x7f;

	if( LastStreamContextLoaded == NULL )
		return IRQ_HANDLED;

	if( ((Row    != (LastStreamContextLoaded->HeightInMacroBlocks-1))   ||
	     (Column != (LastStreamContextLoaded->WidthInMacroBlocks-1)))      &&
	    ((Status & VID_STA__DID) != VID_STA__DID) )
	{
		if( Status & VID_STA__VLDRL )
		{
			WriteRegister( VID_SRS(0), VID_SRS__SRC );
			OSDEV_Print( "%s: MPEG Soft Reset Interrupt Status = 0x%08x Row = 0x%08x Column = 0x%08x\n",
				     __FUNCTION__, Status, Row, Column );
		}
		return IRQ_HANDLED;
	}
	InterruptStatus = Status;
	//WriteRegister( VID_ITM(0), 0x0000 );                        // Clear interrupt mask

#if USE_SEMAPHORE
	OSDEV_ReleaseSemaphore( &DecodeDone );
#else
	DecodeComplete      = true;
	wake_up( &DecodeDone );
#endif
	return IRQ_HANDLED;
}






