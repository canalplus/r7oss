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
--  Abstract : interface between pp and decoder
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: decppif.h,v $
--  $Date: 2010/12/02 10:53:50 $
--  $Revision: 1.16 $
--
------------------------------------------------------------------------------*/

#ifndef _PPDEC_H_
#define _PPDEC_H_


/* PP input types (picStruct) */
/* Frame or top field */
#define DECPP_PIC_FRAME_OR_TOP_FIELD                       0U
/* Bottom field only */
#define DECPP_PIC_BOT_FIELD                                1U
/* top and bottom is separate locations */
#define DECPP_PIC_TOP_AND_BOT_FIELD                        2U
/* top and bottom interleaved */
#define DECPP_PIC_TOP_AND_BOT_FIELD_FRAME                  3U
/* interleaved top only */
#define DECPP_PIC_TOP_FIELD_FRAME                          4U
/* interleaved bottom only */
#define DECPP_PIC_BOT_FIELD_FRAME                          5U

/* Control interface between decoder and pp */
/* decoder writes, pp read-only */

typedef struct DecPpInterface_ {
	enum {
		DECPP_IDLE = 0,
		DECPP_RUNNING,  /* PP was started */
		DECPP_PIC_READY, /* PP has finished a picture */
		DECPP_PIC_NOT_FINISHED /* PP still processing a picture */
	} ppStatus; /* Decoder keeps track of what it asked the pp to do */

	enum {
		MULTIBUFFER_UNINIT = 0, /* buffering mode not yet decided */
		MULTIBUFFER_DISABLED,   /* Single buffer legacy mode */
		MULTIBUFFER_SEMIMODE,   /* enabled but full pipel cannot be used */
		MULTIBUFFER_FULLMODE    /* enabled and full pipeline successful */
	} multiBufStat;

	unsigned int inputBusLuma;
	unsigned int inputBusChroma;
	unsigned int bottomBusLuma;
	unsigned int bottomBusChroma;
	unsigned int picStruct;           /* structure of input picture */
	unsigned int topField;
	unsigned int inwidth;
	unsigned int inheight;
	unsigned int usePipeline;
	unsigned int littleEndian;
	unsigned int wordSwap;
	unsigned int croppedW;
	unsigned int croppedH;

	unsigned int bufferIndex;         /* multibuffer, where to put PP output */
	unsigned int displayIndex;        /* multibuffer, next picture in display order */
	unsigned int prevAnchorDisplayIndex;

	/* VC-1 */
	unsigned int rangeRed;
	unsigned int rangeMapYEnable;
	unsigned int rangeMapYCoeff;
	unsigned int rangeMapCEnable;
	unsigned int rangeMapCCoeff;

	unsigned int tiledMode;
} DecPpInterface;

/* Decoder asks with this struct information about pp setup */
/* pp writes, decoder read-only */

typedef struct DecPpQuery_ {
	/* Dec-to-PP direction */
	unsigned int tiledMode;

	/* PP-to-Dec direction */
	unsigned int pipelineAccepted;    /* PP accepts pipeline */
	unsigned int deinterlace;         /* Deinterlace feature used */
	unsigned int multiBuffer;         /* multibuffer PP output enabled */
	unsigned int ppConfigChanged;     /* PP config changed after previous output */
} DecPpQuery;

#endif
