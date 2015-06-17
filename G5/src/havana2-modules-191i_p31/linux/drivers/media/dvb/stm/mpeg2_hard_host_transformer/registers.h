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

Source file name : registers.h
Author :           Nick

Definition of the register addresses and bitfields for the mpeg2 decoder
on the STM8000 as seen from the Audio Encoder LX


Date        Modification                                    Name
----        ------------                                    --------
28-Nov-03   Created                                         Nick

************************************************************************/

static unsigned int              RegisterBase;

//
// Register access macros
//

/*
#define WriteRegister( address, value)  *((volatile unsigned int *)(address))   = (value)
#define ReadRegister(  address)         *((volatile unsigned int *)(address))
*/
#define WriteRegister(     Address, Value)      OSDEV_WriteLong( RegisterBase+Address, Value );
#define ReadRegister(      Address)             OSDEV_ReadLong(  RegisterBase+Address )
#define WriteByteRegister( Address, Value)      OSDEV_WriteByte( RegisterBase+Address, Value );
#define ReadByteRegister(  Address)             OSDEV_ReadByte(  RegisterBase+Address )

//
// Register addresses
//

#define VIDEO_REGISTER_BASE     0x19240000
#define VIDEO_REGISTER_SIZE     0x1000

static void inline MapRegisters(void)
{    
    RegisterBase = (unsigned int)OSDEV_IOReMap( VIDEO_REGISTER_BASE, VIDEO_REGISTER_SIZE);
}
static void inline UnMapRegisters(void)
{
    OSDEV_IOUnMap( RegisterBase );
}


#define VID0_EXE                0x0008                                  /* Execute decoding task */
#define VID_IC_CFG              0x0010                                  /* Video decoder interconnect configuration */

#define VID_MBE0                0x0070                                  /* Macroblock error statistic 0 */
#define VID_MBE1                0x0074                                  /* Macroblock error statistic 1 */
#define VID_MBE2                0x0078                                  /* Macroblock error statistic 2 */
#define VID_MBE3                0x007c                                  /* Macroblock error statistic 3 */

#define VID0_QMWIP              0x0100                                  /* Quantization matrix data, intra table */
#define VID0_QMWNIP             0x0180                                  /* Quantization matrix data, non-intra table */

#define VIDEO_PIPELINE_BASE(n)  ((n) * 0x400)                           /* Not necessary as only single pipeline, but maybe this will change */

#define VID_TIS(n)              (VIDEO_PIPELINE_BASE(n) + 0x0300)       /* Task instruction */
#define VID_PPR(n)              (VIDEO_PIPELINE_BASE(n) + 0x0304)       /* Picture parameters */
#define VID_SRS(n)              (VIDEO_PIPELINE_BASE(n) + 0x030c)       /* Decoding soft reset */

#define VID_ITM(n)              (VIDEO_PIPELINE_BASE(n) + 0x03f0)       /* Interrupt mask */
#define VID_ITS(n)              (VIDEO_PIPELINE_BASE(n) + 0x03f4)       /* Interrupt status */
#define VID_STA(n)              (VIDEO_PIPELINE_BASE(n) + 0x03f8)       /* Status */

#define VID_DFH(n)              (VIDEO_PIPELINE_BASE(n) + 0x0400)       /* Decoded frame height */
#define VID_DFS(n)              (VIDEO_PIPELINE_BASE(n) + 0x0404)       /* Decoded frame size */
#define VID_DFW(n)              (VIDEO_PIPELINE_BASE(n) + 0x0408)       /* Decoded frame width */
#define VID_BBS(n)              (VIDEO_PIPELINE_BASE(n) + 0x040c)       /* Video elementary stream bit buffer start */
#define VID_BBE(n)              (VIDEO_PIPELINE_BASE(n) + 0x0414)       /* Video elementary stream bit buffer end */
#define VID_VLDRL(n)            (VIDEO_PIPELINE_BASE(n) + 0x0448)       /* Variable length decoder read limit */
#define VID_VLD_PTR(n)          (VIDEO_PIPELINE_BASE(n) + 0x045c)       /* Variable length decoder read pointer */

#define VID_MBNM(n)             (VIDEO_PIPELINE_BASE(n) + 0x0480)       /* Macroblock number for the main reconstruction (debug) */
#define VID_MBNS(n)             (VIDEO_PIPELINE_BASE(n) + 0x0484)       /* Macroblock number for the secondary reconstruction */
#define VID_BCHP(n)             (VIDEO_PIPELINE_BASE(n) + 0x0488)       /* Backward chroma frame pointer */
#define VID_BFP(n)              (VIDEO_PIPELINE_BASE(n) + 0x048c)       /* Backward luma frame pointer */
#define VID_FCHP(n)             (VIDEO_PIPELINE_BASE(n) + 0x0490)       /* Forward chroma frame pointer */
#define VID_FFP(n)              (VIDEO_PIPELINE_BASE(n) + 0x0494)       /* Forward luma frame pointer */
#define VID_RCHP(n)             (VIDEO_PIPELINE_BASE(n) + 0x0498)       /* Main reconstructed chroma frame pointer */
#define VID_RFP(n)              (VIDEO_PIPELINE_BASE(n) + 0x049c)       /* Main reconstructed luma frame pointer */
#define VID_SRCHP(n)            (VIDEO_PIPELINE_BASE(n) + 0x04a0)       /* Secondary reconstructed chroma frame pointer */
#define VID_SRFP(n)             (VIDEO_PIPELINE_BASE(n) + 0x04a4)       /* Secondary reconstructed luma frame pointer */
#define VID_RCM(n)              (VIDEO_PIPELINE_BASE(n) + 0x04ac)       /* Reconstruction mode */


//
// Register field defines,
//


#define VID_EXE__EXE                            0x00000001              /* Execute */

#define VID_IC_CFG__PRDC_MASK                   0x0003
#define VID_IC_CFG__PRDC_SHIFT                  5
#define VID_IC_CFG__PRDL_MASK                   0x0003
#define VID_IC_CFG__PRDL_SHIFT                  3
#define VID_IC_CFG__LP_MASK                     0x0007
#define VID_IC_CFG__LP_SHIFT                    0

#define VID_MBEN__MBEX3_MASK                    0x00ff
#define VID_MBEN__MBEX3_SHIFT                   24
#define VID_MBEN__MBEX2_MASK                    0x00ff
#define VID_MBEN__MBEX2_SHIFT                   16
#define VID_MBEN__MBEX1_MASK                    0x00ff
#define VID_MBEN__MBEX1_SHIFT                   8
#define VID_MBEN__MBEX0_MASK                    0x00ff
#define VID_MBEN__MBEX0_SHIFT                   0


#define VID_TIS__RMM                            0x00000008              /* Reconstruct missing macroblock */
#define VID_TIS__MVC                            0x00000004              /* Motion vector check */
#define VID_TIS__SBD                            0x00000002              /* Simplified B picture decoding */
#define VID_TIS__OVW                            0x00000001              /* Enable overwrite */


#define VID_PPR__MP2_SHIFT                      30                      /* MPEG-2 mode */
#define VID_PPR__FFN_MASK                       0x0003
#define VID_PPR__FFN_SHIFT                      28                      /* Force field number */
#define VID_PPR__TFF_SHIFT                      27                      /* Top field first */
#define VID_PPR__FRM_SHIFT                      26
#define VID_PPR__CMV_SHIFT                      25
#define VID_PPR__QST_SHIFT                      24
#define VID_PPR__IVF_SHIFT                      23
#define VID_PPR__AZZ_SHIFT                      22
#define VID_PPR__PCT_MASK                       0x0003
#define VID_PPR__PCT_SHIFT                      20
#define VID_PPR__DCP_MASK                       0x0003
#define VID_PPR__DCP_SHIFT                      18
#define VID_PPR__PST_MASK                       0x0003
#define VID_PPR__PST_SHIFT                      16
#define VID_PPR__BFV_MASK                       0x000f
#define VID_PPR__BFV_SHIFT                      12
#define VID_PPR__FFV_MASK                       0x000f
#define VID_PPR__FFV_SHIFT                      8
#define VID_PPR__BFH_MASK                       0x000f
#define VID_PPR__BFH_SHIFT                      4
#define VID_PPR__FFH_MASK                       0x000f
#define VID_PPR__FFH_SHIFT                      0

#define VID_FFN__FIRST_FIELD                    0x01                    /* Force first field */
#define VID_FFN__SECOND_FIELD                   0x03                    /* Force second field */

#define VID_SRS__SRC                            0x00000001              /* Soft reset */

#define VID_STA__R_OPC                          0x00000040              /* */
#define VID_STA__VLDRL                          0x00000020              /* Variable length decoder read limit */
#define VID_STA__DSE                            0x00000010              /* Semantic or syntax error */
#define VID_STA__MSE                            0x00000008              /* Set after task if above error detected */
#define VID_STA__DUE                            0x00000004              /* Set after task if underflow error detected */
#define VID_STA__DOE                            0x00000002              /* Set after task if overflow error detected */
#define VID_STA__DID                            0x00000001              /* Decoder idle */

