/*
   ST Microelectronics BDispII driver - node register layout

   (c) Copyright 2007/2008/2011  STMicroelectronics Ltd.

   All rights reserved.
*/

#ifndef __BDISP2_NODEGROUPS_H__
#define __BDISP2_NODEGROUPS_H__


#include <linux/types.h>
#include <linux/stm/bdisp2_registers.h>



/* general config */
struct _BltNodeGroup00 {
	__u32 BLT_NIP;
	__u32 BLT_CIC;
	__u32 BLT_INS;
	__u32 BLT_ACK;
};
/* target config */
struct _BltNodeGroup01 {
	__u32 BLT_TBA;
	__u32 BLT_TTY;
	__u32 BLT_TXY;
	__u32 BLT_TSZ_S1SZ;
};
/* color fill */
struct _BltNodeGroup02 {
	__u32 BLT_S1CF;
	__u32 BLT_S2CF;
};
/* source 1 config */
struct _BltNodeGroup03 {
	__u32 BLT_S1BA;
	__u32 BLT_S1TY;
	__u32 BLT_S1XY;
	__u32 dummy;
};
/* source 2 config */
struct _BltNodeGroup04 {
	__u32 BLT_S2BA;
	__u32 BLT_S2TY;
	__u32 BLT_S2XY;
	__u32 BLT_S2SZ;
};
/* source 3 config */
struct _BltNodeGroup05 {
	__u32 BLT_S3BA;
	__u32 BLT_S3TY;
	__u32 BLT_S3XY;
	__u32 BLT_S3SZ;
};
/* clip config */
struct _BltNodeGroup06 {
	__u32 BLT_CWO;
	__u32 BLT_CWS;
};
/* clut & color space conversion */
struct _BltNodeGroup07 {
	__u32 BLT_CCO;
	__u32 BLT_CML;
};
/* filters control and plane mask */
struct _BltNodeGroup08 {
	__u32 BLT_FCTL_RZC;
	__u32 BLT_PMK;
};
/* 2D (chroma) filter control */
struct _BltNodeGroup09 {
	__u32 BLT_RSF;
	__u32 BLT_RZI;
	__u32 BLT_HFP;
	__u32 BLT_VFP;
};
/* 2D (lumna) filter control */
struct _BltNodeGroup10 {
	__u32 BLT_Y_RSF;
	__u32 BLT_Y_RZI;
	__u32 BLT_Y_HFP;
	__u32 BLT_Y_VFP;
};
/* flicker filter */
struct _BltNodeGroup11 {
	__u32 BLT_FF0;
	__u32 BLT_FF1;
	__u32 BLT_FF2;
	__u32 BLT_FF3;
};
/* color key 1 and 2 */
struct _BltNodeGroup12 {
	__u32 BLT_KEY1;
	__u32 BLT_KEY2;
};
/* XYL */
struct _BltNodeGroup13 {
	__u32 BLT_XYL;
	__u32 BLT_XYP;
};
/* static addressing and user */
struct _BltNodeGroup14 {
	__u32 BLT_SAR;
	__u32 BLT_USR;
};
/* input versatile matrix 0, 1, 2, 3 */
struct _BltNodeGroup15 {
	__u32 BLT_IVMX0;
	__u32 BLT_IVMX1;
	__u32 BLT_IVMX2;
	__u32 BLT_IVMX3;
};
/* output versatile matrix 0, 1, 2, 3 */
struct _BltNodeGroup16 {
	__u32 BLT_OVMX0;
	__u32 BLT_OVMX1;
	__u32 BLT_OVMX2;
	__u32 BLT_OVMX3;
};
/* Pace Dot */
struct _BltNodeGroup17 {
	__u32 BLT_PACE;
	__u32 BLT_DOT;
};
/* VC1 range engine */
struct _BltNodeGroup18 {
	__u32 BLT_VC1R;
	__u32 dummy;
};
/* Gradient fill */
struct _BltNodeGroup19 {
	__u32 BLT_HGF;
	__u32 BLT_VGF;
};


struct _BltNode_Common {
	struct _BltNodeGroup00 ConfigGeneral; /* 4 -> General */
	struct _BltNodeGroup01 ConfigTarget;  /* 4 -> Target */
	struct _BltNodeGroup02 ConfigColor;   /* 2 -> Color */
	struct _BltNodeGroup03 ConfigSource1; /* 4 -> Source1 */
	/* total: 14 * 4 = 56 */
#define BLIT_CIC_BLT_NODE_COMMON (0 \
				  | BLIT_CIC_NODE_COLOR   \
				  | BLIT_CIC_NODE_SOURCE1 \
				 )
};

struct _BltNode_Init {
	struct _BltNode_Common common;       /* 14 */
	struct _BltNodeGroup14 ConfigStatic; /* 2 -> SAR / USR */
	/* total: 16 * 4 = 64 */
#define BLIT_CIC_BLT_NODE_INIT (0 \
				| BLIT_CIC_BLT_NODE_COMMON \
				| BLIT_CIC_NODE_STATIC_real \
			       )
};

struct _BltNode_Complete {
	struct _BltNode_Common common;            /* 14 */

	struct _BltNodeGroup04 ConfigSource2;     /*  4 */
	struct _BltNodeGroup05 ConfigSource3;     /*  4 */
	struct _BltNodeGroup06 ConfigClip;        /*  2 */
	struct _BltNodeGroup07 ConfigClut;        /*  2 */
	struct _BltNodeGroup08 ConfigFilters;     /*  2 */
	struct _BltNodeGroup09 ConfigFiltersChr;  /*  4 */
	struct _BltNodeGroup10 ConfigFiltersLuma; /*  4 */
	struct _BltNodeGroup11 ConfigFlicker;     /*  4 */
	struct _BltNodeGroup12 ConfigColorkey;    /*  2 */
	struct _BltNodeGroup14 ConfigStatic;      /*  2 */
	struct _BltNodeGroup15 ConfigIVMX;        /*  4 */
	struct _BltNodeGroup16 ConfigOVMX;        /*  4 */
	struct _BltNodeGroup18 ConfigVC1;         /*  2 */
	struct _BltNodeGroup19 ConfigGradient;    /*  2 */
	/* total: 56 * 4 = 224 */
};




#endif /* __BDISP2_NODEGROUPS_H__ */
