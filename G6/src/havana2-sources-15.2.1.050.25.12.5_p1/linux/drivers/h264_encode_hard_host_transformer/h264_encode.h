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

#ifndef H_H264_ENCODE
#define H_H264_ENCODE

#include "h264_encode_hard.h"
#include "h264_encode_nvcl.h"

// A forward declaration of the Context structure is required as we use it inside itself
struct H264EncodeHardCodecContext_s;
typedef int (*H264Callback)(struct H264EncodeHardCodecContext_s *Context);

typedef struct ESBufferContext_s {
	uint8_t *ESStart_p;  // Virtual ES Address Pointer
	uint32_t offset;/* stream pointer offset */
	uint32_t overEstSEISize;
} ESBufferContext_t;

// Buffer fullness computation tool
#define BRC_SET_DEPLETION(Depletion,FrameRate,TargetBitRate) {                      \
          (Depletion).base  = (FrameRate).num;                                      \
          (Depletion).div   = (((TargetBitRate)*(FrameRate).den)/(Depletion).base); \
          (Depletion).rem   = ((TargetBitRate)*(FrameRate).den)%(Depletion).base;   \
}

#define BRC_GET_INTEGER_PART(rational) ((uint32_t)(((int32_t)((rational).rem)>((rational).div/2))?(rational).div+1:(rational).div))

#define BRC_ADD_B_TO_A(a,b) {                                             \
    (a).div += (b).div;                                                   \
    (a).rem += (b).rem;                                                   \
    if ((a).rem >= (a).base) {                                            \
      (a).div += (a).rem / (a).base;                                      \
      (a).rem  = (a).rem % (a).base;                                      \
    }                                                                     \
  }


#define BRC_SUB_B_TO_A(a,b) {                                             \
    (a).div -= (b).div;                                                   \
    if ((a).rem >= (b).rem )                                              \
      (a).rem -= (b).rem;                                                 \
    else {                                                                \
      (a).div--;                                                          \
      (a).rem += (a).base - (b).rem;                                      \
    }                                                                     \
  }

#define BRC_ASSIGN_VALUE_TO_A_WITH_BASE(value,a,b) {        \
          (a).base  = (b);                                  \
          (a).div   = (value);                              \
          (a).rem   = 0;                                    \
        }

#define BRC_SUB_VALUE_FROM_A(a,value)  (a).div-=(value)
#define BRC_ADD_VALUE_FROM_A(a,value)  (a).div+=(value)

#define BRC_C_EQUALTO_A_PLUS_B(a,b,c) {                                      \
    (c).base  = (a).base;                                                    \
    (c).div   = (a).div + (b).div;                                           \
    (c).rem   = (a).rem + (b).rem;                                           \
    if ((c).rem > (c).base) {                                                \
      (c).rem  = (c).div % (c).base;                                         \
      (c).div += (c).div / (c).base;                                         \
    }                                                                        \
  }

#define BRC_A_TIMES_N(a,n)                                    \
  (a).div *= (n);                                             \
  (a).rem *= (n);                                             \
  if ((a).rem >= (a).base) {                                  \
    (a).div += (a).rem / (a).base;                            \
    (a).rem  = (a).rem % (a).base;                            \
  }

typedef struct H264EncodeHardRational_s {
	int32_t  div;
	uint32_t rem;
	uint16_t base;
	uint16_t padding;
} H264EncodeHardRational_t;

typedef struct H264EncodeHardFps_s {
	uint32_t num;
	uint32_t den;
} H264EncodeHardFps_t;

typedef struct H264EncodeHardCodecContext_s {
	/*Max frame dimensions are deduced from memory profile information provided at MME init*/
	uint32_t maxFrameWidth;
	uint32_t maxFrameHeight;
	H264EncodeHardSequenceParams_t globalParameters;
	/*multi instance SDRAM buffer addresses used for HVA internal processing: 1 buffer set per MME instance*/
	uint32_t referenceFrameBufHandler;
	uint32_t referenceFrameBufSize;
	uint32_t referenceFrameBufPhysAddress;
	uint32_t reconstructedFrameBufHandler;
	uint32_t reconstructedFrameBufSize;
	uint32_t reconstructedFrameBufPhysAddress;
	uint32_t spatialContextBufHandler;
	uint32_t spatialContextBufSize;
	uint32_t spatialContextBufPhysAddress;
	uint32_t temporalContextBufHandler;
	uint32_t temporalContextBufSize;
	uint32_t temporalContextBufPhysAddress;
	uint32_t brcInOutBufHandler;
	uint32_t brcInOutBufSize;
	uint32_t brcInOutBufPhysAddress;
	uint32_t paramOutHandler;
	uint32_t paramOutSize;
	uint32_t paramOutPhysAddress;
	/*HVA task descriptor*/
	uint32_t taskDescriptorHandler;
	uint32_t taskDescriptorSize;
	uint32_t taskDescriptorPhysAddress;
	uint32_t sliceHeaderBufHandler;
	uint32_t sliceHeaderBufSize;
	uint32_t sliceHeaderBufPhysAddress;
	uint32_t statusBitstreamSize;
	uint32_t statusRemovalTime;
	uint32_t statusStuffingBits;
	uint32_t statusNonVCLNALUSize;
	H264EncodeHard_InterruptError_t hwError;
	H264EncodeHard_TransformStatus_t transformStatus;
	H264EncodeHardStatus_t encodeStatus;
	uint32_t frameEncodeDuration;
	uint32_t hvcEncodeDuration;
	uint8_t  clientId;         //map clienId of HVA command
	uint8_t  instanceTaskId;   //it is NOT HVA taskID that is a global task counter
	uint16_t discardCommandCounter;
	uint16_t mvToggle;        //HVA task descriptor param transparently managed by MME transformer
	H264EncodeHardRational_t bufferFullness;     // relates to buffer fullness
	uint32_t lastRemovalTime;      // relates to buffer fullness
	int16_t  lastIdrPicId;    // relates to slice header (idr_pic_id)
	uint16_t idrToggle;       // relates to slice header (idr_pic_id)
	uint32_t PastBitstreamSize[4]; // part of VBR computation to assess overflow condition
	uint16_t framerateNumRef;      // to save reference frame rate into context
	uint16_t framerateDenRef;      // to save reference frame rate into context
	uint32_t bitrateRef;           // to save reference bit rate into context
	uint32_t cpbBufferSizeRef;     // to save reference cpb buffer size into context

	H264EncodeNVCLContext_t nVCLContext;
	ESBufferContext_t  ESContext;

	H264EncodeHardFrameParams_t FrameParams;

	uint64_t frameEncodeStart;
	uint64_t frameEncodeEnd;
	uint64_t LastInterruptTime;        // Measuring Scheduler Latency

	struct work_struct CompletionWork; // To request this context be completed outside of Interrupt Context

	H264Callback Callback;// To notify upon encode completion interrupt
	void *CallbackHandle; // For storing handles required in the Callback during Async Operation
} H264EncodeHardCodecContext_t;

#endif // H_H264_ENCODE
