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

#ifndef H_VP8HARD_DECODE
#define H_VP8HARD_DECODE

#include "defcfg.h"
#include "dwl.h"
#include "vp8hard.h"

#include "VP8_hard_VideoTransformerTypes.h"

#define MAX_SNAPSHOT_WIDTH  1024
#define MAX_SNAPSHOT_HEIGHT 1024

#define DEBUG_PRINT(args)
#define TRACE_PP_CTRL(args)

/* Read direct MV from output memory; indexes i0..i2 used to
 * handle both big- and little-endian modes. */
#define DIR_MV_VER(p,i0,i1,i2) \
    ((((unsigned int)(p[(i0)])) << 3) | (((unsigned int)(p[(i1)])) >> 5) | (((unsigned int)(p[(i2)] & 0x3) ) << 11 ))
#define DIR_MV_BE_VER(p) \
    ((((unsigned int)(p[1])) << 3) | (((unsigned int)(p[0])) >> 5) | (((unsigned int)(p[2] & 0x3) ) << 11 ))
#define DIR_MV_LE_VER(p) \
    ((((unsigned int)(p[2])) << 3) | (((unsigned int)(p[3])) >> 5) | (((unsigned int)(p[1] & 0x3) ) << 11 ))
#define SIGN_EXTEND(value, bits) (value) = (((value)<<(32-bits))>>(32-bits))

/* Distribution ranges and zero point */
#define VER_DISTR_MIN           (-256)
#define VER_DISTR_MAX           (255)
#define VER_DISTR_RANGE         (512)
#define VER_DISTR_ZERO_POINT    (256)
#define VER_MV_RANGE            (16)
#define HOR_CALC_WIDTH          (32)

/* macro to get absolute value */
#define ABS(a) (((a) < 0) ? -(a) : (a))

#define END_OF_STREAM   (0xFFFFFFFF)
#define CHECK_END_OF_STREAM(s) if((s)==END_OF_STREAM) return (s)

#define CLIP3(l, h, v) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define MB_MULTIPLE(x)  (((x)+15)&~15)

#define DEC_8170_IRQ_RDY            0x01
#define DEC_8170_IRQ_BUS            0x02
#define DEC_8170_IRQ_BUFFER         0x04
#define DEC_8170_IRQ_ASO            0x08
#define DEC_8170_IRQ_ERROR          0x10
#define DEC_8170_IRQ_SLICE          0x20
#define DEC_8170_IRQ_TIMEOUT        0x40

#define DEC_8190_IRQ_RDY            DEC_8170_IRQ_RDY
#define DEC_8190_IRQ_BUS            DEC_8170_IRQ_BUS
#define DEC_8190_IRQ_BUFFER         DEC_8170_IRQ_BUFFER
#define DEC_8190_IRQ_ASO            DEC_8170_IRQ_ASO
#define DEC_8190_IRQ_ERROR          DEC_8170_IRQ_ERROR
#define DEC_8190_IRQ_SLICE          DEC_8170_IRQ_SLICE
#define DEC_8190_IRQ_TIMEOUT        DEC_8170_IRQ_TIMEOUT

/* Feature support flags */
#define REFBU_SUPPORT_GENERIC       (1)
#define REFBU_SUPPORT_INTERLACED    (2)
#define REFBU_SUPPORT_DOUBLE        (4)
#define REFBU_SUPPORT_OFFSET        (8)

/* Buffering info */
#define REFBU_BUFFER_SINGLE_FIELD   (1)
#define REFBU_MULTIPLE_REF_FRAMES   (2)
#define REFBU_DISABLE_CHECKPOINT    (4)
#define REFBU_FORCE_ADAPTIVE_SINGLE (8)

typedef enum {
	REFBU_FRAME,
	REFBU_FIELD,
	REFBU_MBAFF
} refbuMode_e;


enum {
	HANTRO_NOK =  1,
	HANTRO_OK   = 0
};

#define TILED_REF_NONE  (0)
#define TILED_REF_8x4   (1)

#define THR_ADJ_MAX (8)
#define THR_ADJ_MIN (1)

#define DEC_X170_MODE_H264  (0)
#define DEC_X170_MODE_MPEG4 (1)
#define DEC_X170_MODE_H263  (2)
#define DEC_X170_MODE_VC1   (4)
#define DEC_X170_MODE_MPEG2 (5)
#define DEC_X170_MODE_MPEG1 (6)
#define DEC_X170_MODE_VP6   (7)
#define DEC_X170_MODE_RV    (8)
#define DEC_X170_MODE_VP7   (9)
#define DEC_X170_MODE_VP8   (10)
#define DEC_X170_MODE_AVS   (11)

/* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/* macro to get greater of two values */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

VP8DecRet VP8DecDecode(VP8DecInst decInst, VP8_ParamPicture_t *Picture_Data_Struct_p, VP8DecInput *pInput);

signed int VP8HwdAsicAllocatePictures(VP8DecContainer_t *pDecCont);

void vp8hwdDecodeFrameTag(VP8_ParamPicture_t *Picture_Data_Struct_p, unsigned char *pStrm, vp8Decoder_t *dec);

void vp8hwdResetDecoder(vp8Decoder_t *dec, DecAsicBuffers_t *asicBuff);

void vp8hwdResetProbs(vp8Decoder_t *dec);

void vp8hwdPrepareVp7Scan(vp8Decoder_t *dec, unsigned int *newOrder);

unsigned int vp8hwdDecodeFrameHeader(VP8_ParamPicture_t *Picture_Data_Struct_p, unsigned char *pStrm,
                                     unsigned int strmLen, vpBoolCoder_t *bc, vp8Decoder_t *dec);

unsigned int ScaleDimension(unsigned int orig, unsigned int scale);

void vp8hwdBoolStart(vpBoolCoder_t *br, unsigned char *source, unsigned int len);

unsigned int DecodeSegmentationData(vpBoolCoder_t *bc, vp8Decoder_t *dec);

unsigned int DecodeMbLfAdjustments(vpBoolCoder_t *bc, vp8Decoder_t *dec);

signed int DecodeQuantizerDelta(vpBoolCoder_t *bc);

unsigned int  vp8hwdDecodeCoeffUpdate(vpBoolCoder_t *bc, vp8Decoder_t *dec);

unsigned int  vp8hwdDecodeMvUpdate(vpBoolCoder_t *bc, vp8Decoder_t *dec);

void vp8hwdFreeze(VP8DecContainer_t *pDecCont);

void VP8HwdAsicProbUpdate(VP8DecContainer_t *pDecCont);

void VP8HwdAsicInitPicture(VP8DecContainer_t *pDecCont);

void VP8HwdAsicStrmPosUpdate(VP8DecContainer_t *pDecCont, unsigned int strmBusAddress);

void vp8hwdPreparePpRun(VP8DecContainer_t *pDecCont);

void VP8HwdAsicContPicture(VP8DecContainer_t *pDecCont);

void vp8hwdUpdateOutBase(VP8DecContainer_t *pDecCont);

unsigned int vp8hwdSetPartitionOffsets(unsigned char *stream, unsigned int len, vp8Decoder_t *dec);

unsigned int VP8HwdAsicRun(VP8DecContainer_t *pDecCont);

unsigned int vp8hwdDecodeBool(vpBoolCoder_t *br, signed int probability);

signed int RefbuGetHitThreshold(refBuffer_t *pRefbu);

unsigned int RefbuVpxGetPrevFrameStats(refBuffer_t *pRefbu);

unsigned int DecSetupTiledReference(unsigned int *regBase, unsigned int tiledModeSupport,
                                    unsigned int interlacedStream);

void VP8HwdAsicRefreshRegs(VP8DecContainer_t *pDecCont);

void RefbuMvStatistics(refBuffer_t *pRefbu, unsigned int *regBase, unsigned int *pMv, unsigned int directMvsAvailable,
                       unsigned int isIntraPicture);

unsigned int ReadPartitionSize(unsigned char *cxSize);

void RefbuSetup(refBuffer_t *pRefbu, unsigned int *regBase, refbuMode_e mode, unsigned int isIntraFrame,
                unsigned int isBframe, unsigned int refPicId0, unsigned int refPicId1, unsigned int flags);

unsigned int GetSettings(refBuffer_t *pRefbu, signed int *pX, signed int *pY, unsigned int isBpic,
                         unsigned int isFieldPic);

unsigned int DecideParityMode(refBuffer_t *pRefbu, unsigned int isBframe);

void VP8HwdAsicFlushRegs(VP8DecContainer_t *pDecCont);

void DirectMvStatistics(refBuffer_t *pRefbu, unsigned int *pMv, signed int numIntraBlk, unsigned int bigEndian);

void BuildDistribution(unsigned int *pDistrVer, unsigned int *pMv, signed int frmSizeInMbs, unsigned int mvsPerMb,
                       unsigned int bigEndian, signed int *minY, signed int *maxY);

void UpdateMemModel(refBuffer_t *pRefbu);

void vp8HwdAsicBufUpdate(VP8DecInst decInst, VP8DecInput *pInput);

#endif /*H_VP8HARD_DECODE*/
