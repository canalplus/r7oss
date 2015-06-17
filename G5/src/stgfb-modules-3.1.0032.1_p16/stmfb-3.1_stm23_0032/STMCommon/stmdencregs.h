/***********************************************************************
 *
 * File: STMCommon/stmdencregs.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMDENCREGS_H
#define _STMDENCREGS_H

/***************** Start of register offset definitions *******************/
#define DENC_CFG0   0x00  /* Configuration register 0                [7:0]   */
#define DENC_CFG1   0x01  /* Configuration register 1                [7:0]   */
#define DENC_CFG2   0x02  /* Configuration register 2                [7:0]   */
#define DENC_CFG3   0x03  /* Configuration register 3                [7:0]   */
#define DENC_CFG4   0x04  /* Configuration register 4                [7:0]   */
#define DENC_CFG5   0x05  /* Configuration register 5                [7:0]   */
#define DENC_CFG6   0x06  /* Configuration register 6                [7:0]   */
#define DENC_CFG7   0x07  /* Configuration register 7, SECAM         [7:0]   */
#define DENC_CFG8   0x08  /* Configuration register 8                [7:0]   */
#define DENC_CFG9   0x51  /* Configuration register 9                [7:0]   */
#define DENC_CFG10  0x5c
#define DENC_CFG11  0x5d
#define DENC_CFG12  0x5e
#define DENC_CFG13  0x5f

#define DENC_STA    0x09  /* Status register                         [7:0]   */

#define DENC_IDFS0  0x0a  /* Increment digital frequency synthesiser [23:16] */
#define DENC_IDFS1  0x0b  /* Increment digital frequency synthesiser [15:8]  */
#define DENC_IDFS2  0x0c  /* Increment digital frequency synthesiser [7:0]   */

#define DENC_PDFS0  0x0d  /* Phase digital frequency synthesiser     [7:0]   */
#define DENC_PDFS1  0x0e  /* Phase digital frequency synthesiser     [15:8]  */

#define DENC_WSS1   0x0f  /* Wide Screen Signalling                  [15:8]  */
#define DENC_WSS0   0x10  /* Wide Screen Signalling                  [7:0]   */

#define DENC_CID    0x18  /* Denc macro-cell identification number   [7:0]   */

#define DENC_VPS0   0x1e  /* Video Programming Sys pt[7:0]          ->[7:0]  */
#define DENC_VPS1   0x1d  /* Video Programming Sys c[1:0] np[5:0]   ->[7:0]  */
#define DENC_VPS2   0x1c  /* Video Programming Sys min[4:0] c[3:2]  ->[7:0]  */
#define DENC_VPS3   0x1b  /* Video Programming Sys m2[2:0] h[4:0]   ->[7:0]  */
#define DENC_VPS4   0x1a  /* Video Programming Sys np[7:6] d[4:0] m3->[7:0]  */
#define DENC_VPS5   0x19  /* Video Programming Sys s[1-0] cni[3:0] ->[7:0]   */

#define DENC_CGMS0  0x1f  /* CGMS data byte 0 [1:4]  -> [3:0]                */
#define DENC_CGMS1  0x20  /* CGMS data byte 1 [5:12] -> [7:0]                */
#define DENC_CGMS2  0x21  /* CGMS data byte 2 [13:20]-> [7:0]                */

#define DENC_TTXL1  0x22  /* Teletext Configuration Line 1                   */
#define DENC_TTXL2  0x23  /* Teletext Configuration Line 2                   */
#define DENC_TTXL3  0x24  /* Teletext Configuration Line 3                   */
#define DENC_TTXL4  0x25  /* Teletext Configuration Line 4                   */
#define DENC_TTXL5  0x26  /* Teletext Configuration Line 5                   */


#define DENC_CC10   0x27  /* Closed Caption second character for field 1     */
#define DENC_CC11   0x28  /* Closed Caption first character for field 1      */
#define DENC_CC20   0x29  /* Closed Caption second character for field 2     */
#define DENC_CC21   0x2a  /* Closed Caption first character for field 1      */

#define DENC_CFL1   0x2b  /* Closed Caption line insertion for field 1 [4:0] */
#define DENC_CFL2   0x2c  /* Closed Caption line insertion for field 2       */

#define DENC_TTXCFG 0x40  /* Teletext Configuration                          */
#define DENC_CMTTX  0x41  /* DAC2/C Gain,Mask of TTX,En/Disable brightness.. */

#define DENC_BRIGHT 0x45  /* Brightness                                      */
#define DENC_CONTRA 0x46  /* Contrast                                        */
#define DENC_SATURA 0x47  /* Saturation                                      */

#define DENC_CHRM0  0x48  /* Chroma filter coefficient 0                     */
#define DENC_CHRM1  0x49  /* Chroma filter coefficient 1                     */
#define DENC_CHRM2  0x4a  /* Chroma filter coefficient 2                     */
#define DENC_CHRM3  0x4b  /* Chroma filter coefficient 3                     */
#define DENC_CHRM4  0x4c  /* Chroma filter coefficient 4                     */
#define DENC_CHRM5  0x4d  /* Chroma filter coefficient 5                     */
#define DENC_CHRM6  0x4e  /* Chroma filter coefficient 6                     */
#define DENC_CHRM7  0x4f  /* Chroma filter coefficient 7                     */
#define DENC_CHRM8  0x50  /* Chroma filter coefficient 8                     */

#define DENC_LUMA0  0x52  /* Luma filter coefficient 0                       */
#define DENC_LUMA1  0x53  /* Luma filter coefficient 1                       */
#define DENC_LUMA2  0x54  /* Luma filter coefficient 2                       */
#define DENC_LUMA3  0x55  /* Luma filter coefficient 3                       */
#define DENC_LUMA4  0x56  /* Luma filter coefficient 4                       */
#define DENC_LUMA5  0x57  /* Luma filter coefficient 5                       */
#define DENC_LUMA6  0x58  /* Luma filter coefficient 6                       */
#define DENC_LUMA7  0x59  /* Luma filter coefficient 7                       */
#define DENC_LUMA8  0x5a  /* Luma filter coefficient 8                       */
#define DENC_LUMA9  0x5b  /* Luma filter coefficient 9                       */

#define DENC_HUE    0x69  /* For 7100, also see DENC_DFS_PHASE0 for enable   */

/***************** End of register offset definitions *******************/

/* DENC/Teletext configuration ----------------------------------------------*/
#define DENC_ON   0x1
#define DENC_CKR  0x2

/* DENC_CFG0 - Configuration register 0  (8-bit)---------------------------- */
#define DENC_CFG0_MASK_STD   0x3F /* Mask for standard selected              */
#define DENC_CFG0_PAL_BDGHI  0x00 /* PAL B, D, G, H or I standard selected   */
#define DENC_CFG0_PAL_N      0x40 /* PAL N standard selected                 */
#define DENC_CFG0_NTSC_M     0x80 /* PAL NTSC standard selected              */
#define DENC_CFG0_PAL_M      0xC0 /* PAL M standard selected                 */
#define DENC_CFG0_MASK_SYNC  0xC7 /* Mask for synchro configuration          */
#define DENC_CFG0_ODDE_SLV   0x00 /* ODDEVEN based slave mode (frame lock)   */
#define DENC_CFG0_FRM_SLV    0x08 /* Frame only based slave mode(frame lock) */
#define DENC_CFG0_ODHS_SLV   0x10 /* ODDEVEN + HSYNC based slave mode(line l)*/
#define DENC_CFG0_FRHS_SLV   0x18 /* Frame + HSYNC based slave mode(line l)  */
#define DENC_CFG0_VSYNC_SLV  0x20 /* VSYNC only based slave mode(frame l   ) */
#define DENC_CFG0_VSHS_SLV   0x28 /* VSYNC + HSYNC based slave mode(line l  )*/
#define DENC_CFG0_MASTER     0x30 /* Master mode selected                    */
#define DENC_CFG0_COL_BAR    0x38 /* Test color bar pattern enabled          */
#define DENC_CFG0_HSYNC_POL_HIGH 0x04 /* HSYNC positive pulse                */
#define DENC_CFG0_VSYNC_POL_HIGH 0x02 /* Synchronisation polarity selection  */
#define DENC_CFG0_FREE_RUN       0x01 /* Freerun On                          */


/* DENC_CFG1 - Configuration register 1  (8-bit)---------------------------- */
#define DENC_CFG1_VBI_SEL      0x80 /* Full VBI selected                     */
#define DENC_CFG1_MASK_FLT     0x9F /* Mask for U/V Chroma filter bandwith   */
                                    /* selection                             */
#define DENC_CFG1_MASK_SYNC_OK 0xEF /* mask for sync in case of frame loss   */
#define DENC_CFG1_FLT_11       0x00 /* FLT Low definition NTSC filter        */
#define DENC_CFG1_FLT_13       0x20 /* FLT Low definition PAL filter         */
#define DENC_CFG1_FLT_16       0x40 /* FLT High definition NTSC filter       */
#define DENC_CFG1_FLT_19       0x60 /* FLT High definition PAL filter        */
#define DENC_CFG1_SYNC_OK      0x10 /* Synchronisation avaibility            */
#define DENC_CFG1_COL_KILL     0x08 /* Color suppressed on CVBS              */
#define DENC_CFG1_SETUP        0x04 /* Pedestal setup (7.5 IRE)              */
#define DENC_CFG1_MASK_CC      0xFC /* Mask for Closed caption encoding mode */
#define DENC_CFG1_CC_DIS       0x00 /* Closed caption data encoding disabled */
#define DENC_CFG1_CC_ENA_F1    0x01 /* Closed caption enabled in field 1     */
#define DENC_CFG1_CC_ENA_F2    0x02 /* Closed caption enabled in field 2     */
#define DENC_CFG1_CC_ENA_BOTH  0x03 /* Closed caption enabled in both fields */
#define DENC_CFG1_DAC_INV      0x80 /* Enable DAC input data inversion       */


/* DENC_CFG2 - Configuration register 2  (8-bit)---------------------------- */
#define DENC_CFG2_NO_INTER     0x80 /* Non-interlaced mode selected          */
#define DENC_CFG2_ENA_RST      0x40 /* Cyclic phase reset enabled            */
#define DENC_CFG2_ENA_BURST    0x20 /* Chrominance burst enabled             */
#define DENC_CFG2_RESERVED     0x10 /* Select 444 input for RGB tri-dacs     */
#define DENC_CFG2_SEL_RST      0x08 /* Reset DDFS with value on DNC_IFx reg. */
#define DENC_CFG2_RST_OSC      0x04 /* Software phase reset of DDFS          */
#define DENC_CFG2_RST_4F       0x02 /* Reset DDFS every 4 fields             */
#define DENC_CFG2_RST_2F       0x01 /* Reset DDFS every 2 fields             */
#define DENC_CFG2_RST_EVE      0x00 /* Reset DDFS every line                 */
#define DENC_CFG2_RST_8F       0x03 /* Reset DDFS every 8 fields             */
#define DENC_CFG2_MASK_RST     0xFC /* Mask for reset DDFS mode              */


/* DENC_CFG3 - Configuration register 3  (8-bit) --------------------------- */
#define DENC_CFG3_ENA_TRFLT    0x80 /* Enable Trap filter                    */
#define DENC_CFG3_TRAP_443     0x40 /* Select 4.43MHz Trap filter            */
#define DENC_CFG3_ENA_CGMS     0x20 /* Enable CGMS endoding                  */
#define DENC_CFG3_ENA_WSS      0x01 /* wide screen signalling enable         */
	/* register macrocell V3/V5 */
#define DENC_CFG3_NOSD         0x10 /*choice of active edge  of 'denc_ref_ck'*/
	/* end register macrocell V3/V5 */
	/* register macrocell V7/V8/V9/10/V11 */
#define DENC_CFG3_CK_IN_PHASE  0x10 /*choice of active edge  of 'denc_ref_ck'*/
	/* end register macrocell V7/V8/V9/10/V11 */
	/* register macrocell V10/V11 */
#define DENC_CFG3_DELAY_ENABLE 0x08 /* enable of chroma to luma delay        */
	/* end register macrocell V10/V11 */


/* DENC_CFG4 - Configuration register 4  (8-bit) --------------------------- */
#define DENC_CFG4_SYOUT_0      0x00
#define DENC_CFG4_SYOUT_P_1    0x01
#define DENC_CFG4_SYOUT_P_2    0x02
#define DENC_CFG4_SYOUT_M_1    0x07
#define DENC_CFG4_SYOUT_M_2    0x06

#define DENC_CFG4_MASK_SYIN    0x3F /* Mask for adjustment of incoming       */
                                    /* synchro signals                       */
#define DENC_CFG4_SYIN_0      0x00 /* nominal delay                         */
#define DENC_CFG4_SYIN_P_1    0x40 /* delay = +1 ckref                      */
#define DENC_CFG4_SYIN_P_2    0x80 /* delay = +2 ckref                      */
#define DENC_CFG4_SYIN_P_3    0xC0 /* delay = +3 ckref                      */

#define DENC_CFG4_ALINE        0x08 /* Video active line duration control    */


/* DENC_CFG5 - Configuration register 5  (8-bit) --------------------------- */
#define DENC_CFG5_SEL_INC      0x80 /* Choice of Dig Freq Synthe increment   */
#define DENC_CFG5_DIS_DAC1     0x40 /* DAC 1 input forced to 0               */
#define DENC_CFG5_DIS_DAC2     0x20 /* DAC 2 input forced to 0               */
#define DENC_CFG5_DIS_DAC3     0x10 /* DAC 3 input forced to 0               */
#define DENC_CFG5_DIS_DAC4     0x08 /* DAC 4 input forced to 0               */
#define DENC_CFG5_DIS_DAC5     0x04 /* DAC 5 input forced to 0               */
#define DENC_CFG5_DIS_DAC6     0x02 /* DAC 6 input forced to 0               */
#define DENC_CFG5_DAC_INV      0x01 /* Enable DAC input data inversion       */
#define DENC_CFG5_DAC_NONINV   0x00 /* Enable DAC input data non inversion   */


/* DENC_CFG6 - Configuration register 6 ( 5 bits, 3 unused bits ) ---------- */
#define DENC_CFG6_RST_SOFT     0x80 /* Denc soft reset                       */
#define DENC_CFG6_MASK_LSKP    0x8F /* mask for line skip configuration      */
#define DENC_CFG6_NORM_MODE    0x00 /* normal mode, no insert/skip capable   */
#define DENC_CFG6_MAN_MODE     0x10 /* same as normal, unless skip specified */
#define DENC_CFG6_AUTO_INS     0x40 /* automatic line insert mode            */
#define DENC_CFG6_AUTO_SKP     0x60 /* automatic line skip mode              */
#define DENC_CFG6_FORBIDDEN    0x70 /* Reserved, don't write this value      */
#define DENC_CFG6_MAX_DYN      0x01 /* Maximum dynamic range 1-254 ( 16-240) */
	/* register macrocell V3 */
#define DENC_CFG6_CHGI2C_0     0x02 /* Chip add select; write=0x40,read=0x41 */
#define DENC_CFG6_CHGI2C_1     0x00 /* Chip add select; write=0x42,read=0x43 */
	/* end register macrocell V3 */
	/* register macrocell V7/V8/V9/10/V11 */
#define DENC_CFG6_TTX_ENA      0x02 /* Teletexte enable bit                  */
	/* end register macrocell V7/V8/V9/10/V11 */
	/* register macrocell V5/V7/V8/V9/10/V11 */
#define DENC_CFG6_MASK_CC      0x0C /* Closed Caption mask                   */
#define DENC_CFG6_CC_00        0x00 /* Closed Caption disable                */
#define DENC_CFG6_CC_01        0x04 /* Closed Caption 01                     */
#define DENC_CFG6_CC_02        0x08 /* Closed Caption 02                     */
#define DENC_CFG6_CC_03        0x0C /* Closed Caption 03                     */
	/* end register macrocell V5/V7/V8/V9/10/V11 */


/* DENC_CFG7 - Configuration register 7 ( SECAM mainly ) ------------------- */
#define DENC_CFG7_SECAM        0x80 /* Select SECAM chroma encoding on top   */
                                    /* of config selected in DENC_CFG0       */
#define DENC_CFG7_PHI12_SEC    0x40 /* sub carrier phase sequence start      */
#define DENC_CFG7_INV_PHI_SEC  0x20 /* invert phases on second field         */
#define DENC_CFG7_SETUP_YUV    0x08 /* YUV (8K) or Y/CVBS AUX (5528)
                                      * pedestal enabled
                                      */
#define DENC_CFG7_UV_LEV       0x04 /* UV output level control               */
#define DENC_CFG7_ENA_VPS      0x02 /* enable video programming system       */
#define DENC_CFG7_SQ_PIX       0x01 /* enable square pixel mode (PAL/NTSC)   */


/* DENC_CFG8 - Configuration register 8 ---------------- */
#define DENC_CFG8_PH_RST_1     0x80 /* sub-carrier phase reset mode          */
#define DENC_CFG8_PH_RST_0     0x40 /* sub-carrier phase reset mode          */
#define DENC_CFG8_RESRV_BIT    0x20 /* must be set on some parts (eg 5528)   */
#define DENC_CFG8_BLK_ALL      0x08 /* blanking of all video lines           */
#define DENC_CFG8_TTX_NOTMV    0x04 /* VBI priority order for teletext       */


#define DENC_DAC13_MASK_DAC1   0x0F /* mask for DAC1 configuration           */
#define DENC_DAC13_MASK_DAC3   0xF0 /* mask for DAC3 configuration           */
#define DENC_DAC45_MASK_DAC4   0x0F /* mask for DAC4 configuration           */
#define DENC_DAC45_MASK_DAC5   0xF0 /* mask for DAC5 configuration           */


/* DENC_CHRM0 - Chroma Coef 0 and config -------_--------------------------- */
#define DENC_CHRM0_FLT_S	   0x80	/* Chroma Filter Selection               */
#define DENC_CHRM0_DIV_MASK	   0x9F	/* Mask for chroma Filter Division       */
#define DENC_CHRM0_DIV_512	   0x00	/* Sum of coefficients = 512             */
#define DENC_CHRM0_DIV_1024        0x20 /* Sum of coefficients = 1024            */
#define DENC_CHRM0_DIV_2048        0x40 /* Sum of coefficients = 2048            */
#define DENC_CHRM0_DIV_4096        0x60 /* Sum of coefficients = 4096            */
#define DENC_CHRM0_DIV0   	   0x20	/* Chroma Filter Division bit 0          */
#define DENC_CHRM0_MASK   	   0x1F	/* Mask for Chroma Filter Coef 0         */

/* DENC_CHRM1-8 - Chroma Coef 1-8 and config ------------------------------- */
#define DENC_CHRM1_MASK   	   0x3F	/* Mask for Chroma Filter Coef 1         */
#define DENC_CHRM2_MASK   	   0x7F	/* Mask for Chroma Filter Coef 2         */
#define DENC_CHRM3_MASK   	   0x7F	/* Mask for Chroma Filter Coef 3         */
#define DENC_CHRM4_MASK   	   0xFF	/* Mask for Chroma Filter Coef 4         */
#define DENC_CHRM5_MASK   	   0xFF	/* Mask for Chroma Filter Coef 5         */
#define DENC_CHRM6_MASK   	   0xFF	/* Mask for Chroma Filter Coef 6         */
#define DENC_CHRM7_MASK   	   0xFF	/* Mask for Chroma Filter Coef 7         */
#define DENC_CHRM8_MASK   	   0xFF	/* Mask for Chroma Filter Coef 8         */

/* DENC_CFG9 - Configuration register 9 (status, ReadOnly) ----------------- */
#define DENC_CFG9_FLT_YS  	   0x01 /* Enable software luma coeffs		 */
#define DENC_CFG9_PLG_DIV_Y_256   0x00 /* Sum of coefficients	= 256		 */
#define DENC_CFG9_PLG_DIV_Y_512   0x02 /* Sum of coefficients	= 512		 */
#define DENC_CFG9_PLG_DIV_Y_1024  0x04 /* Sum of coefficients	= 1024		 */
#define DENC_CFG9_PLG_DIV_Y_2048  0x06 /* Sum of coefficients	= 2048		 */
#define DENC_CFG9_MASK_PLG_DIV    0xF9 /* Mask for sum of coefficients		 */
#define DENC_CFG9_RESERVED        0x08
#define DENC_CFG9_DEL_P_0_5       (0x0<<4)
#define DENC_CFG9_DEL_ZERO        (0x1<<4)
#define DENC_CFG9_DEL_M_0_5       (0x2<<4)
#define DENC_CFG9_DEL_M_1_0       (0x3<<4)
#define DENC_CFG9_DEL_M_1_5       (0x4<<4)
#define DENC_CFG9_DEL_M_2_0       (0x5<<4)
#define DENC_CFG9_DEL_P_2_5       (0xC<<4)
#define DENC_CFG9_DEL_P_2_0       (0xD<<4)
#define DENC_CFG9_DEL_P_1_5       (0xE<<4)
#define DENC_CFG9_DEL_P_1_0       (0xF<<4)

/* DENC_LUMA0-9 - Luma Coef 0-9 and config --------------------------------- */
#define DENC_LUMA0_MASK   	   0x1F	/* Mask for Luma Filter Coef 0           */
#define DENC_LUMA1_MASK   	   0x3F	/* Mask for Luma Filter Coef 1           */
#define DENC_LUMA2_MASK   	   0x7F	/* Mask for Luma Filter Coef 2           */
#define DENC_LUMA3_MASK   	   0x7F	/* Mask for Luma Filter Coef 3           */
#define DENC_LUMA4_MASK   	   0xFF	/* Mask for Luma Filter Coef 4           */
#define DENC_LUMA5_MASK   	   0xFF	/* Mask for Luma Filter Coef 5           */
#define DENC_LUMA6_MASK   	   0xFF	/* Mask for Luma Filter Coef 6           */
#define DENC_LUMA7_MASK   	   0xFF	/* Mask for Luma Filter Coef 7           */
#define DENC_LUMA8_MASK   	   0xFF	/* Mask for Luma Filter Coef 8           */
#define DENC_LUMA9_MASK   	   0xFF	/* Mask for Luma Filter Coef 9           */

#define DENC_CFG10_SECAM_IN_AUX           (0x1<<2)
#define DENC_CFG10_RGBSAT_EN              (0x1<<3)
#define DENC_CFG10_AUX_COL_KILL           (0x1<<4)
#define DENC_CFG10_AUX_19_FLT             (0x60)
#define DENC_CFG10_AUX_16_FLT             (0x40)
#define DENC_CFG10_AUX_13_FLT             (0x20)
#define DENC_CFG10_AUX_11_FLT             (0x00)

#define DENC_CFG11_MAIN_IF_DELAY_DISABLED (0x1<<2)
#define DENC_CFG11_RESERVED               (0x1<<3)
#define DENC_CFG11_DEL_P_0_5              (0x0<<4)
#define DENC_CFG11_DEL_ZERO               (0x1<<4)
#define DENC_CFG11_DEL_M_0_5              (0x2<<4)
#define DENC_CFG11_DEL_M_1_0              (0x3<<4)
#define DENC_CFG11_DEL_M_1_5              (0x4<<4)
#define DENC_CFG11_DEL_M_2_0              (0x5<<4)
#define DENC_CFG11_DEL_P_2_5              (0xC<<4)
#define DENC_CFG11_DEL_P_2_0              (0xD<<4)
#define DENC_CFG11_DEL_P_1_5              (0xE<<4)
#define DENC_CFG11_DEL_P_1_0              (0xF<<4)

#define DENC_CFG12_AUX_MAX_DYN            (0x1<<1)
#define DENC_CFG12_ENNOTCH                (0x1<<2)
#define DENC_CFG12_AUX_DEL_EN             (0x1<<3)
#define DENC_CFG12_AUX_ENTRAP             (0x1<<7)

#define DENC_DFS_PHASE0_HUE_EN            (0x1<<4) /* HUE (phase offset) enable for 7100 */

#define DENC_WSS1_ASPECT_MASK             (0xF)
#define DENC_WSS1_NO_AFD                  (0x0)
#define DENC_WSS1_14_9_CENTER             (0x1)
#define DENC_WSS1_14_9_TOP                (0x2)
#define DENC_WSS1_16_9_TOP                (0x4)
#define DENC_WSS1_16_9_FULL               (0x7)
#define DENC_WSS1_4_3_FULL                (0x8)
#define DENC_WSS1_16_9_CENTER             (0xB)
#define DENC_WSS1_GT_16_9_CENTER          (0xD)
#define DENC_WSS1_4_3_SAP_14_9            (0xE)
#define DENC_WSS1_PAL_PLUS_SHIFT          (4)
#define DENC_WSS1_PAL_PLUS_MASK           (0x7<<DENC_WSS1_PAL_PLUS_SHIFT)

#define DENC_WSS0_SUB_TELETEXT            (0x1<<2)
#define DENC_WSS0_OPEN_SUBTITLE_SHIFT     (3)
#define DENC_WSS0_OPEN_SUBTITLE_MASK      (0x3<<DENC_WSS0_OPEN_SUBTITLE_SHIFT)
#define DENC_WSS0_SURROUND_SOUND          (0x1<<5)
#define DENC_WSS0_CP_SHIFT                (6)
#define DENC_WSS0_CP_MASK                 (0x3<<DENC_WSS0_CP_SHIFT)

#define DENC_CGMS0_4_3                    (0x0)
#define DENC_CGMS0_16_9                   (0x1<<3)
#define DENC_CGMS0_NORMAL                 (0x0)
#define DENC_CGMS0_LETTERBOX              (0x1<<2)

/*
 * Note that the IEC bits are numbered from 0 in public documentation, but
 * the DENC documentation numbers them starting at 1.
 */
#define DENC_CGMS1_IEC61880_B6_SHIFT      (5)
#define DENC_CGMS1_IEC61880_B7_SHIFT      (4)
#define DENC_CGMS1_IEC61880_B8_SHIFT      (3)
#define DENC_CGMS1_IEC61880_B9_SHIFT      (2)

#define DENC_CGMS1_PRERECORDED_ANALOGUE_SOURCE (1L<<1)

#define DENC_TTXL1_DEL_SHIFT              (4)
#define DENC_TTXL1_DEL_MASK               (0x7<<DENC_TTX1_DEL_SHIFT)
#define DENC_TTXL1_FULL_PAGE_EN           (0x1<<7)

#define DENC_TTXCFG_RESERVED_BIT_4        (0x1<<4)
#define DENC_TTXCFG_SYSTEM_SHIFT          (5)
#define DENC_TTXCFG_SYSTEM_A              (0x0<<DENC_TTXCFG_SYSTEM_SHIFT)
#define DENC_TTXCFG_SYSTEM_B              (0x1<<DENC_TTXCFG_SYSTEM_SHIFT)
#define DENC_TTXCFG_SYSTEM_C              (0x2<<DENC_TTXCFG_SYSTEM_SHIFT)
#define DENC_TTXCFG_SYSTEM_D              (0x3<<DENC_TTXCFG_SYSTEM_SHIFT)
#define DENC_TTXCFG_SYSTEM_MASK           (0x3<<DENC_TTXCFG_SYSTEM_SHIFT)
#define DENC_TTXCFG_100IRE                (0x1<<7)

/*
 * Note: this was called the DAC2 register in older chips
 */
#define DENC_CMTTX_BCS_MAIN_EN           (0x1<<0)
#define DENC_CMTTX_BCS_AUX_EN            (0x1<<1) /* 5528 only */
#define DENC_CMTTX_FCODE_MASK_DISABLE    (0x1<<3)
#define DENC_CMTTX_C_MULT_MASK           (0xF<<4)

#endif // _STMDENCREGS_H
