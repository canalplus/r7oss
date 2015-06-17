/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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
************************************************************************/
#ifndef _H264PP_H_
#define _H264PP_H_

//

#define H264_PP_MAX_SLICES	256					// Need 137 in principle
#define H264_PP_SESB_SIZE       (8 * H264_PP_MAX_SLICES)		// 2 words per slice
#define H264_PP_OUTPUT_SIZE     (0x00180000 - H264_PP_SESB_SIZE)	// 1.5 Mb - SESB data
#define H264_PP_BUFFER_SIZE     (H264_PP_SESB_SIZE + H264_PP_OUTPUT_SIZE)

#define H264_PP_RESET_TIME_LIMIT	100

//

#define H264_PP_MAX_SUPPORTED_PRE_PROCESSORS	4
#define H264_PP_REGISTER_SIZE			256

#define H264_PP_MAX_SUPPORTED_BUFFERS_PER_OPEN	8
#define H264_PP_VIRTUAL_PP_PER_OPEN		2

//

#define PP_BASE(N)                    (DeviceContext.RegisterBase[N])

#define PP_READ_START(N)              (PP_BASE(N) + 0x0000)
#define PP_READ_STOP(N)               (PP_BASE(N) + 0x0004)
#define PP_BBG(N)                     (PP_BASE(N) + 0x0008)
#define PP_BBS(N)                     (PP_BASE(N) + 0x000c)
#define PP_ISBG(N)                    (PP_BASE(N) + 0x0010)
#define PP_IPBG(N)                    (PP_BASE(N) + 0x0014)
#define PP_IBS(N)                     (PP_BASE(N) + 0x0018)
#define PP_WDL(N)                     (PP_BASE(N) + 0x001c)
#define PP_CFG(N)                     (PP_BASE(N) + 0x0020)
#define PP_PICWIDTH(N)                (PP_BASE(N) + 0x0024)
#define PP_CODELENGTH(N)              (PP_BASE(N) + 0x0028)
#define PP_START(N)                   (PP_BASE(N) + 0x002c)
#define PP_MAX_OPC_SIZE(N)            (PP_BASE(N) + 0x0030)
#define PP_MAX_CHUNK_SIZE(N)          (PP_BASE(N) + 0x0034)
#define PP_MAX_MESSAGE_SIZE(N)        (PP_BASE(N) + 0x0038)
#define PP_ITS(N)                     (PP_BASE(N) + 0x003c)
#define PP_ITM(N)                     (PP_BASE(N) + 0x0040)
#define PP_SRS(N)                     (PP_BASE(N) + 0x0044)
#define PP_DFV_OUTCTRL(N)             (PP_BASE(N) + 0x0048)


/* PP bit fields/shift values */

#define PP_CFG__CONTROL_MODE__START_STOP        0x00000000
#define PP_CFG__CONTROL_MODE__START_PAUSE       0x10000000
#define PP_CFG__CONTROL_MODE__RESTART_STOP      0x20000000
#define PP_CFG__CONTROL_MODE__RESTART_PAUSE     0x30000000

#define PP_CFG__MONOCHROME                      31              // Not in the documentation
#define PP_CFG__DIRECT8X8FLAG_SHIFT             30              // Not in the documentation
#define PP_CFG__TRANSFORM8X8MODE_SHIFT          27              // Not in the documentation
#define PP_CFG__QPINIT_SHIFT                    21
#define PP_CFG__IDXL1_SHIFT                     16
#define PP_CFG__IDXL0_SHIFT                     11
#define PP_CFG__DEBLOCKING_SHIFT                10
#define PP_CFG__BIPREDFLAG_SHIFT                9
#define PP_CFG__PREDFLAG_SHIFT                  8
#define PP_CFG__DPOFLAG_SHIFT                   6
#define PP_CFG__POPFLAG_SHIFT                   5
#define PP_CFG__POCTYPE_SHIFT                   3
#define PP_CFG__FRAMEFLAG_SHIFT                 2
#define PP_CFG__ENTROPYFLAG_SHIFT               1
#define PP_CFG__MBADAPTIVEFLAG_SHIFT            0

#define PP_CODELENGTH__MPOC_SHIFT               5
#define PP_CODELENGTH__MFN_SHIFT                0

#define PP_PICWIDTH__MBINPIC_SHIFT              16
#define PP_PICWIDTH__PICWIDTH_SHIFT             0

#define PP_ITM__WRITE_ERROR                     0x00000100	// error has been detected when writing the pre-processing result in external memory.
#define PP_ITM__READ_ERROR                      0x00000080	// error has been detected when reading the compressed data from external memory.
#define PP_ITM__BIT_BUFFER_OVERFLOW             0x00000040	//
#define PP_ITM__BIT_BUFFER_UNDERFLOW            0x00000020	// Read stop address reached (PP_BBS), picture decoding not finished
#define PP_ITM__INT_BUFFER_OVERFLOW             0x00000010	// Write address for intermediate buffer reached PP_IBS
#define PP_ITM__ERROR_BIT_INSERTED              0x00000008	// Error bit has been inserted in Slice Error Status Buffer
#define PP_ITM__ERROR_SC_DETECTED               0x00000004	// Error Start Code has been detected
#define PP_ITM__SRS_COMP                        0x00000002	// Soft reset is completed
#define PP_ITM__DMA_CMP                         0x00000001	// Write DMA is completed

//

#endif
