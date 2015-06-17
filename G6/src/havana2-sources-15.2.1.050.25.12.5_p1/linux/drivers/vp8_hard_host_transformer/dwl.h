/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : Sytem Wrapper Layer
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl.h,v $
--  $Revision: 1.21 $
--  $Date: 2010/12/01 12:31:03 $
--
------------------------------------------------------------------------------*/

/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.
************************************************************************/

#ifndef H_DWL
#define H_DWL

#include <linux/semaphore.h>

#include "decapicommon.h"

#define assert(expr, args...) do {\
    if (!(expr)) {\
        pr_crit("%s Assert from VP8 decoder\n", __func__);\
    }\
} while (0)

#define DWL_OK                      0
#define DWL_ERROR                  -1

#define MAP_FAILED                  0
#define DWL_HW_WAIT_OK              DWL_OK
#define DWL_HW_WAIT_ERROR           DWL_ERROR
#define DWL_HW_WAIT_TIMEOUT         1

#define DWL_CLIENT_TYPE_H264_DEC         1U
#define DWL_CLIENT_TYPE_MPEG4_DEC        2U
#define DWL_CLIENT_TYPE_JPEG_DEC         3U
#define DWL_CLIENT_TYPE_PP               4U
#define DWL_CLIENT_TYPE_VC1_DEC          5U
#define DWL_CLIENT_TYPE_MPEG2_DEC        6U
#define DWL_CLIENT_TYPE_VP6_DEC          7U
#define DWL_CLIENT_TYPE_AVS_DEC          9U /* TODO: fix */
#define DWL_CLIENT_TYPE_RV_DEC           8U
#define DWL_CLIENT_TYPE_VP8_DEC          10U

#define HX170_CHIP_ID_REG        0x0
#define HX170PP_REG_START       0xF0
#define HX170DEC_REG_START      0x4

#define HX170PP_SYNTH_CFG       100
#define HX170DEC_SYNTH_CFG       50
#define HX170DEC_SYNTH_CFG_2     54

#define HX170PP_FUSE_CFG         99
#define HX170DEC_FUSE_CFG        57

#define X170_SEM_KEY 0x8070

typedef struct hX170dwl_config {
	unsigned int maxDecPicWidth;  /* Maximum video decoding width supported  */
	unsigned int maxPpOutPicWidth;   /* Maximum output width of Post-Processor */
	unsigned int h264Support;     /* HW supports h.264 */
	unsigned int jpegSupport;     /* HW supports JPEG */
	unsigned int mpeg4Support;    /* HW supports MPEG-4 */
	unsigned int customMpeg4Support; /* HW supports custom MPEG-4 features */
	unsigned int vc1Support;      /* HW supports VC-1 Simple */
	unsigned int mpeg2Support;    /* HW supports MPEG-2 */
	unsigned int ppSupport;       /* HW supports post-processor */
	unsigned int ppConfig;        /* HW post-processor functions bitmask */
	unsigned int sorensonSparkSupport;   /* HW supports Sorenson Spark */
	unsigned int refBufSupport;   /* HW supports reference picture buffering */
	unsigned int tiledModeSupport; /* HW supports tiled reference pictuers */
	unsigned int vp6Support;      /* HW supports VP6 */
	unsigned int vp7Support;      /* HW supports VP7 */
	unsigned int vp8Support;      /* HW supports VP8 */
	unsigned int avsSupport;      /* HW supports AVS */
	unsigned int jpegESupport;    /* HW supports JPEG extensions */
	unsigned int rvSupport;       /* HW supports REAL */
	unsigned int mvcSupport;      /* HW supports H264 MVC extension */
	unsigned int webpSupport;     /* HW supports WebP (VP8 snapshot) */
} hX170dwl_config_t;

/* wrapper information */
typedef struct hX170dwl {
	unsigned int clientType;
	volatile unsigned int *pRegBase;  /* IO mem base */
	unsigned int regSize;             /* IO mem size */
	struct semaphore semid;
	struct semaphore Dec_End_sem;
} hX170dwl_t;


typedef struct DWLHwFuseStatus {
	unsigned int h264SupportFuse;     /* HW supports h.264 */
	unsigned int mpeg4SupportFuse;    /* HW supports MPEG-4 */
	unsigned int mpeg2SupportFuse;    /* HW supports MPEG-2 */
	unsigned int sorensonSparkSupportFuse;   /* HW supports Sorenson Spark */
	unsigned int jpegSupportFuse;     /* HW supports JPEG */
	unsigned int vp6SupportFuse;      /* HW supports VP6 */
	unsigned int vp7SupportFuse;      /* HW supports VP6 */
	unsigned int vp8SupportFuse;      /* HW supports VP6 */
	unsigned int vc1SupportFuse;      /* HW supports VC-1 Simple */
	unsigned int jpegProgSupportFuse; /* HW supports Progressive JPEG */
	unsigned int ppSupportFuse;       /* HW supports post-processor */
	unsigned int ppConfigFuse;        /* HW post-processor functions bitmask */
	unsigned int maxDecPicWidthFuse;  /* Maximum video decoding width supported  */
	unsigned int maxPpOutPicWidthFuse; /* Maximum output width of Post-Processor */
	unsigned int refBufSupportFuse;   /* HW supports reference picture buffering */
	unsigned int avsSupportFuse;      /* one of the AVS values defined above */
	unsigned int rvSupportFuse;       /* one of the REAL values defined above */
	unsigned int mvcSupportFuse;
	unsigned int customMpeg4SupportFuse; /* Fuse for custom MPEG-4 */

} DWLHwFuseStatus_t;


/*Function's Prototypes*/
unsigned int DWLReadAsicID(void);
void DWLReadAsicFuseStatus(DWLHwFuseStatus_t *pHwFuseSts);
void DWLReadAsicConfig(hX170dwl_config_t *pHwCfg);
void *DWLInit(unsigned int clientType);
int DWLRelease(const void *instance);
void DWLReadAsicConfig(hX170dwl_config_t *pHwCfg);
void DWLClearSwRegister(void *data);

signed int DWLReserveHw(const void *instance);

unsigned int GetDecRegister(const unsigned int *regBase, unsigned int id);

void SetDecRegister(unsigned int *regBase, unsigned int id, unsigned int value);
void DWLWriteReg(const void *instance, unsigned int offset, unsigned int value);
signed int DWLWaitHwReady(const void *instance, unsigned int timeout);

signed int DWLWaitDecHwReady(const void *instance, unsigned int timeout);

signed int DWLWaitPpHwReady(const void *instance, unsigned int timeout);
signed int DWLWaitPpHwIt(const void *instance, unsigned int timeout);

unsigned int DWLReadReg(const void *instance, unsigned int offset);
void DWLReleaseHw(const void *instance);
int  hx170_check_chip_id(void);


#endif /*H_DWL*/

