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
/************************************************************************
Portions (C) COPYRIGHT 2006 HANTRO PRODUCTS OY
************************************************************************/

#ifndef H_VP8HARD
#define H_VP8HARD

#include <linux/interrupt.h>

#include "decapicommon.h"
#include "defcfg.h"
#include "dwl.h"
#include "decppif.h"

typedef const void *VP8DecInst;

typedef enum {
	/* include script-generated part */
#include "8170enum.h"
	HWIF_DEC_IRQ_STAT,
	HWIF_PP_IRQ_STAT,
	HWIF_LAST_REG,

	/* aliases */
	HWIF_MPEG4_DC_BASE = HWIF_I4X4_OR_DC_BASE,
	HWIF_INTRA_4X4_BASE = HWIF_I4X4_OR_DC_BASE,
	/* VP6 */
	HWIF_VP6HWGOLDEN_BASE = HWIF_REFER4_BASE,
	HWIF_VP6HWPART1_BASE = HWIF_REFER13_BASE,
	HWIF_VP6HWPART2_BASE = HWIF_RLC_VLC_BASE,
	HWIF_VP6HWPROBTBL_BASE = HWIF_QTABLE_BASE,
	/* progressive JPEG */
	HWIF_PJPEG_COEFF_BUF = HWIF_DIR_MV_BASE,

	/* MVC */
	HWIF_INTER_VIEW_BASE = HWIF_REFER15_BASE,

} hwIfName_e;

static const unsigned int regMask[33] = { 0x00000000,
                                          0x00000001, 0x00000003, 0x00000007, 0x0000000F,
                                          0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
                                          0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
                                          0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
                                          0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
                                          0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
                                          0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
                                          0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
                                        };

/* { SWREG, BITS, POSITION } */
static const unsigned int hwDecRegSpec[HWIF_LAST_REG + 1][3] = {
	/* include script-generated part */
#include "8170table.h"
	/* HWIF_DEC_IRQ_STAT */ {1, 7, 12},
	/* HWIF_PP_IRQ_STAT */ {60, 2, 12},
	/* dummy entry */ {0, 0, 0}
};

#define VP8DEC_UNINITIALIZED   0U
#define VP8DEC_INITIALIZED     1U
#define VP8DEC_NEW_HEADERS     3U
#define VP8DEC_DECODING        4U
#define VP8DEC_MIDDLE_OF_PIC   5U

#define VP8HWD_VP7             1U
#define VP8HWD_VP8             2U
#define MAX_NBR_OF_DCT_PARTITIONS       (8)
#define VP7_MV_PROBS_PER_COMPONENT      (17)
#define VP8_MV_PROBS_PER_COMPONENT      (19)
#define MAX_NBR_OF_SEGMENTS             (4)
#define MAX_NBR_OF_MB_REF_LF_DELTAS     (4)
#define MAX_NBR_OF_MB_MODE_LF_DELTAS    (4)


#define DEC_8190_ALIGN_MASK         0x07U
#define DEC_8190_MODE_VP8           0x09U

#define VP8HWDEC_HW_RESERVED         0x0100
#define VP8HWDEC_SYSTEM_ERROR        0x0200
#define VP8HWDEC_SYSTEM_TIMEOUT      0x0300

#define DEC_MODE_VP7  9
#define DEC_MODE_VP8 10

#define PROB_TABLE_SIZE  (1<<16) /* TODO */
#define PROB_TABLE_BUFFER_ALIGNMENT 128
#define BPA_PARTITION_NAME "vid-macroblock-0"

#define SEGMENTATION_MAP_BUFFER_ALIGNMENT 64

#define HX170_IP_VERSION_V1 0x67312048U
#define HX170_IP_VERSION_V2 0x67312398U
#define HX170_IP_VERSION_V6 0x67315058U

#define HX170DEC_SYNTH_CFG       50
#define DWL_MPEG2_E         31  /* 1 bit */
#define DWL_VC1_E           29  /* 2 bits */
#define DWL_JPEG_E          28  /* 1 bit */
#define DWL_MPEG4_E         26  /* 2 bits */
#define DWL_H264_E          24  /* 2 bits */
#define DWL_VP6_E           23  /* 1 bit */
#define DWL_PJPEG_E         22  /* 1 bit */
#define DWL_REF_BUFF_E      20  /* 1 bit */

#define DWL_JPEG_EXT_E          31  /* 1 bit */
#define DWL_REF_BUFF_ILACE_E    30  /* 1 bit */
#define DWL_MPEG4_CUSTOM_E      29  /* 1 bit */
#define DWL_REF_BUFF_DOUBLE_E   28  /* 1 bit */
#define DWL_RV_E            26  /* 2 bits */
#define DWL_VP7_E           24  /* 1 bit */
#define DWL_VP8_E           23  /* 1 bit */
#define DWL_AVS_E           22  /* 1 bit */
#define DWL_MVC_E           20  /* 2 bits */
#define DWL_WEBP_E          19  /* 1 bit */
#define DWL_DEC_TILED_L     17  /* 2 bits */

#define DWL_CFG_E           24  /* 4 bits */
#define DWL_PP_E            16  /* 1 bit */
#define DWL_PP_IN_TILED_L   14  /* 2 bits */

#define DWL_SORENSONSPARK_E 11  /* 1 bit */

#define DWL_H264_FUSE_E          31 /* 1 bit */
#define DWL_MPEG4_FUSE_E         30 /* 1 bit */
#define DWL_MPEG2_FUSE_E         29 /* 1 bit */
#define DWL_SORENSONSPARK_FUSE_E 28 /* 1 bit */
#define DWL_JPEG_FUSE_E          27 /* 1 bit */
#define DWL_VP6_FUSE_E           26 /* 1 bit */
#define DWL_VC1_FUSE_E           25 /* 1 bit */
#define DWL_PJPEG_FUSE_E         24 /* 1 bit */
#define DWL_CUSTOM_MPEG4_FUSE_E  23 /* 1 bit */
#define DWL_RV_FUSE_E            22 /* 1 bit */
#define DWL_VP7_FUSE_E           21 /* 1 bit */
#define DWL_VP8_FUSE_E           20 /* 1 bit */
#define DWL_AVS_FUSE_E           19 /* 1 bit */
#define DWL_MVC_FUSE_E           18 /* 1 bit */

#define DWL_DEC_MAX_1920_FUSE_E  15 /* 1 bit */
#define DWL_DEC_MAX_1280_FUSE_E  14 /* 1 bit */
#define DWL_DEC_MAX_720_FUSE_E   13 /* 1 bit */
#define DWL_DEC_MAX_352_FUSE_E   12 /* 1 bit */
#define DWL_REF_BUFF_FUSE_E       7 /* 1 bit */


#define DWL_PP_FUSE_E                31  /* 1 bit */
#define DWL_PP_DEINTERLACE_FUSE_E   30  /* 1 bit */
#define DWL_PP_ALPHA_BLEND_FUSE_E   29  /* 1 bit */
#define DWL_PP_MAX_1920_FUSE_E        15  /* 1 bit */
#define DWL_PP_MAX_1280_FUSE_E        14  /* 1 bit */
#define DWL_PP_MAX_720_FUSE_E        13  /* 1 bit */
#define DWL_PP_MAX_352_FUSE_E        12  /* 1 bit */

#define VP8_INTERRUPT_LINE 89

typedef void           *Vp8HardHandle_t;

typedef struct DataBuffers_ {
	/*Frame buffer where to decode*/
	unsigned int *pPicBufferY;    /* luma output address  */
	unsigned int picBufferBusAddressY;
	unsigned int *pPicBufferC;    /* chroma output address */
	unsigned int picBufferBusAddressC;
	/*the previous reference frame */
	unsigned int *pLastPicBufferY;    /* luma output address  */
	unsigned int LastpicBufferBusAddressY;
	unsigned int *pLastPicBufferC;    /* chroma output address */
	unsigned int LastpicBufferBusAddressC;
	/*the golden reference frame */
	unsigned int *pGoldPicBufferY;    /* luma output address  */
	unsigned int GoldpicBufferBusAddressY;
	unsigned int *pGoldPicBufferC;    /* chroma output address */
	unsigned int GoldpicBufferBusAddressC;
	/*the alternate reference frame */
	unsigned int *pAltPicBufferY;    /* luma output address  */
	unsigned int AltpicBufferBusAddressY;
	unsigned int *pAltPicBufferC;    /* chroma output address */
	unsigned int AltpicBufferBusAddressC;

} DataBuffers_t;

/* Input structure */
typedef struct VP8DecInput_ {
	unsigned char *pStream;   /* Pointer to the input data */
	unsigned int streamBusAddress;   /* DMA bus address of the input stream */
	unsigned int dataLen;         /* Number of bytes to be decoded         */
	/* used only for WebP */
	unsigned int sliceHeight;
	DataBuffers_t DataBuffers;
} VP8DecInput;

typedef enum VP8DecFormat_ {
	VP8DEC_VP7 = 0x01,
	VP8DEC_VP8 = 0x02,
	VP8DEC_WEBP = 0x03
} VP8DecFormat;

typedef enum {
	VP8HARD_NO_ERROR,
	VP8HARD_ERROR,
	VP8HARD_NO_MEMORY
} Vp8HardStatus_t;

#ifndef HANTRO_FALSE
#define HANTRO_FALSE    (0)
#endif

#ifndef HANTRO_TRUE
#define HANTRO_TRUE     (1)
#endif

typedef struct Vp8HardCodecContext_s {
	unsigned int        WidthInMacroBlocks;
	unsigned int        HeightInMacroBlocks;
	unsigned char      *LumaDecodeFrameBuffer;
} Vp8HardCodecContext_t;

typedef struct Vp8HardSequenceParams_s {
	unsigned int                 horizontalSize;
	unsigned int                 verticalSize;
	unsigned int                 chromaFormat;
	unsigned char               *intraQuantizerMatrix;// to be checked
	unsigned char               *nonIntraQuantizerMatrix;
} Vp8HardSequenceParams_t;

typedef enum VP8DecRet_ {
	VP8DEC_OK = 0,
	VP8DEC_STRM_PROCESSED = 1,
	VP8DEC_PIC_RDY = 2,
	VP8DEC_PIC_DECODED = 3,
	VP8DEC_HDRS_RDY = 4,
	VP8DEC_ADVANCED_TOOLS = 5,
	VP8DEC_SLICE_RDY = 6,

	VP8DEC_PARAM_ERROR = -1,
	VP8DEC_STRM_ERROR = -2,
	VP8DEC_NOT_INITIALIZED = -3,
	VP8DEC_MEMFAIL = -4,
	VP8DEC_INITFAIL = -5,
	VP8DEC_HDRS_NOT_RDY = -6,
	VP8DEC_STREAM_NOT_SUPPORTED = -8,

	VP8DEC_HW_RESERVED = -254,
	VP8DEC_HW_TIMEOUT = -255,
	VP8DEC_HW_BUS_ERROR = -256,
	VP8DEC_SYSTEM_ERROR = -257,
	VP8DEC_DWL_ERROR = -258,

	VP8DEC_EVALUATION_LIMIT_EXCEEDED = -999,
	VP8DEC_FORMAT_NOT_SUPPORTED = -1000
} VP8DecRet;

/* Linear memory area descriptor */
typedef struct DWLLinearMem {
	unsigned int *virtualAddress;
	unsigned int busAddress;
	unsigned int size;
//    STAVMEM_BlockHandle_t AllocBufferHandle;
} DWLLinearMem_t;

typedef struct {
	unsigned int *pPicBufferY;
	unsigned int picBufferBusAddrY;
	unsigned int *pPicBufferC;
	unsigned int picBufferBusAddrC;
} userMem_t;

/* asic interface */
typedef struct DecAsicBuffers {
	unsigned int width, height;
	DWLLinearMem_t probTbl;
	DWLLinearMem_t segmentMap;
	DWLLinearMem_t outBuffer;
	DWLLinearMem_t prevOutBuffer;
	DWLLinearMem_t refBuffer;
	DWLLinearMem_t goldenBuffer;
	DWLLinearMem_t alternateBuffer;
	DWLLinearMem_t pictures[16];

	/* Indexes for picture buffers in pictures[] array */
	unsigned int outBufferI;
	unsigned int refBufferI;
	unsigned int goldenBufferI;
	unsigned int alternateBufferI;

	unsigned int wholePicConcealed;
	unsigned int disableOutWriting;
	unsigned int segmentMapSize;
	unsigned int partition1Base;
	unsigned int partition1BitOffset;
	unsigned int partition2Base;
	int dcPred[2];
	int dcMatch[2];

	userMem_t userMem;
} DecAsicBuffers_t;

typedef struct memAccess {
	unsigned int latency;
	unsigned int nonseq;
	unsigned int seq;
} memAccess_t;

typedef struct refBuffer {
#if 0
	int ox[3];
#endif
	int decModeMbWeights[2];
	int mbWeight;
	int oy[3];
	int picWidthInMbs;
	int picHeightInMbs;
	int frmSizeInMbs;
	int fldSizeInMbs;
	int numIntraBlk[3];
	int coverage[3];
	int fldHitsP[3][2];
	int fldHitsB[3][2];
	int fldCnt;
	int mvsPerMb;
	int filterSize;
	/* Thresholds */
	int predIntraBlk;
	int predCoverage;
	int checkpoint;
	unsigned int decMode;
	unsigned int dataExcessMaxPct;

	int busWidthInBits;
	int prevLatency;
	int numCyclesForBuffering;
	int totalDataForBuffering;
	int bufferPenalty;
	int avgCyclesPerMb;
	unsigned int prevWasField;
	unsigned int prevUsedDouble;
	int thrAdj;
	unsigned int prevFrameHitSum;
	memAccess_t currMemModel;   /* Clocks per operation, modifiable from
                                 * testbench. */
	memAccess_t memAccessStats; /* Approximate counts for operations, set
                                 * based on format */
	unsigned int memAccessStatsFlag;

	/* Support flags */
	unsigned int interlacedSupport;
	unsigned int doubleSupport;
	unsigned int offsetSupport;

	/* Internal test mode */
	void (*testFunction)(struct refBuffer *, unsigned int *regBase, unsigned int isIntra, unsigned int mode);

} refBuffer_t;

typedef enum {
	VP8_YCbCr_BT601,
	VP8_CUSTOM
} vpColorSpace_e;

typedef struct vp8EntropyProbs_s {
	unsigned char              probLuma16x16PredMode[4];
	unsigned char              probChromaPredMode[3];
	unsigned char              probMvContext[2][VP8_MV_PROBS_PER_COMPONENT];
	unsigned char              probCoeffs[4][8][3][11];
} vp8EntropyProbs_t;

typedef struct vp8Decoder_s {
	unsigned int             decMode;

	/* Current frame dimensions */
	unsigned int             width;
	unsigned int             height;
	unsigned int             scaledWidth;
	unsigned int             scaledHeight;

	unsigned int             vpVersion;
	unsigned int             vpProfile;

	unsigned int             keyFrame;

	unsigned int             coeffSkipMode;

	/* DCT coefficient partitions */
	unsigned int             offsetToDctParts;
	unsigned int             nbrDctPartitions;
	unsigned int             dctPartitionOffsets[MAX_NBR_OF_DCT_PARTITIONS];

	vpColorSpace_e  colorSpace;
	unsigned int             clamping;
	unsigned int             showFrame;


	unsigned int             refreshGolden;
	unsigned int             refreshAlternate;
	unsigned int             refreshLast;
	unsigned int             refreshEntropyProbs;
	unsigned int             copyBufferToGolden;
	unsigned int             copyBufferToAlternate;

	unsigned int             refFrameSignBias[2];
	unsigned int             useAsReference;
	unsigned int             loopFilterType;
	unsigned int             loopFilterLevel;
	unsigned int             loopFilterSharpness;

	/* Quantization parameters */
	int             qpYAc, qpYDc, qpY2Ac, qpY2Dc, qpChAc, qpChDc;

	/* From here down, frame-to-frame persisting stuff */

	unsigned int             vp7ScanOrder[16];
	unsigned int             vp7PrevScanOrder[16];

	/* Probabilities */
	unsigned int             probIntra;
	unsigned int             probRefLast;
	unsigned int             probRefGolden;
	unsigned int             probMbSkipFalse;
	unsigned int             probSegment[3];
	vp8EntropyProbs_t entropy,
	                  entropyLast;

	/* Segment and macroblock specific values */
	unsigned int             segmentationEnabled;
	unsigned int             segmentationMapUpdate;
	unsigned int             segmentFeatureMode; /* delta/abs */
	int             segmentQp[MAX_NBR_OF_SEGMENTS];
	int             segmentLoopfilter[MAX_NBR_OF_SEGMENTS];
	unsigned int             modeRefLfEnabled;
	int             mbRefLfDelta[MAX_NBR_OF_MB_REF_LF_DELTAS];
	int             mbModeLfDelta[MAX_NBR_OF_MB_MODE_LF_DELTAS];

	unsigned int             frameTagSize;

	/* Value to remember last frames prediction for hits into most
	 * probable reference frame */
	unsigned int             refbuPredHits;

}
vp8Decoder_t;

typedef struct {
	unsigned int lowvalue;
	unsigned int range;
	unsigned int value;
	int count;
	unsigned int pos;
	unsigned char *buffer;
	unsigned int BitCounter;
	unsigned int streamEndPos;
	unsigned int strmError;
} vpBoolCoder_t;

typedef struct VP8DecContainer {
	const void *checksum;
	unsigned int decMode;
	unsigned int decStat;
	unsigned int picNumber;
	unsigned int asicRunning;
	unsigned int width;
	unsigned int height;
	unsigned int vp8Regs[DEC_X170_REGISTERS];
	DecAsicBuffers_t asicBuff[1];
	const void *dwl;         /* DWL instance */
	unsigned int refBufSupport;
	refBuffer_t refBufferCtrl;

	vp8Decoder_t decoder;
	vpBoolCoder_t bc;

	struct {
		const void *ppInstance;
		void (*PPDecStart)(const void *, const DecPpInterface *);
		void (*PPDecWaitEnd)(const void *);
		void (*PPConfigQuery)(const void *, DecPpQuery *);
		DecPpInterface decPpIf;
		DecPpQuery ppInfo;
	} pp;

	unsigned int pictureBroken;
	unsigned int intraFreeze;
	unsigned int outCount;
	unsigned int refToOut;
	unsigned int pendingPicToPp;

//    bufferQueue_t bq;
	unsigned int numBuffers;

	unsigned int intraOnly;
	unsigned int userMem;
	unsigned int sliceHeight;
	unsigned int totDecodedRows;
	unsigned int outputRows;

	unsigned int tiledModeSupport;
	unsigned int tiledReferenceEnable;

} VP8DecContainer_t;

typedef struct Vp8HardInitParams_s {
	VP8DecContainer_t decInstance;
	VP8DecInput       decIn;
	VP8DecFormat decFormat;
	unsigned int useVideoFreezeConcealment;
	unsigned int numFrameBuffers;
	DecRefFrmFormat referenceFrameFormat;
	unsigned int   CodedWidth;
	unsigned int   CodedHeight;
	unsigned int   SegmentationMapBuffer;
	unsigned int   ProbTableBuffer;
} Vp8HardInitParams_t;

/*      Functions                    */

VP8DecRet Vp8HardInit(Vp8HardInitParams_t *InitParams);
void VP8HwdAsicInit(VP8DecContainer_t *pDecCont);
int VP8HwdAsicAllocateMem(VP8DecContainer_t *pDecCont);
void VP8HwdAsicFreeMem(VP8DecContainer_t *pDecCont);
irqreturn_t VP8InterruptHandler(int irq, void *data);
VP8DecRet Vp8HardDeInit(const void *data);

#endif /*H_VP8HARD*/
