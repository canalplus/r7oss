/*
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * System Trace Module IP v1 register offsets definitions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STM_SYSTRACE_H_
#define __STM_SYSTRACE_H_

/* Register Offset definitions for IPv1 and IPv3 */
#define STM_IP_PID0_OFF 0xFC0
#define STM_IP_PID1_OFF 0xFC8
#define STM_IP_PID2_OFF 0xFD0
#define STM_IP_PID3_OFF 0xFD8
#define STM_IP_CID0_OFF 0xFE0
#define STM_IP_CID1_OFF 0xFE8
#define STM_IP_CID2_OFF 0xFF0
#define STM_IP_CID3_OFF 0xFF8

/* IP V1: Register Offset definitions for IPv1 */
#define STM_IPv1_CR_OFF  0x000
#define STM_IPv1_MMC_OFF 0x008
#define STM_IPv1_TER_OFF 0x010
#define STM_IPv1_ID0_OFF STM_IP_PID0_OFF
#define STM_IPv1_ID1_OFF STM_IP_PID1_OFF
#define STM_IPv1_ID2_OFF STM_IP_PID2_OFF

/* IP V3: Register Offset definitions for IPv3 */
#define STM_IPv3_CR_OFF  0x000
#define STM_IPv3_MCR_OFF 0x008
#define STM_IPv3_TER_OFF 0x010
#define STM_IPv3_DBG_OFF 0x030
#define STM_IPv3_FTR_OFF 0x080
#define STM_IPv3_CTR_OFF 0x088
#define STM_IPv3_PID0_OFF STM_IP_PID0_OFF
#define STM_IPv3_PID1_OFF STM_IP_PID1_OFF
#define STM_IPv3_PID2_OFF STM_IP_PID2_OFF
#define STM_IPv3_PID3_OFF STM_IP_PID3_OFF
#define STM_IPv3_CID0_OFF STM_IP_CID0_OFF
#define STM_IPv3_CID1_OFF STM_IP_CID1_OFF
#define STM_IPv3_CID2_OFF STM_IP_CID2_OFF
#define STM_IPv3_CID3_OFF STM_IP_CID3_OFF

/* IP V1: A new channel ID each 16 bytes */
#define STM_IPv1_CH_SHIFT  4
#define STM_IPv1_CH_OFFSET (1 << STM_IPv1_CH_SHIFT)

/* IP V1: A new core (master) ID each 4k */
#define STM_IPv1_MA_SHIFT  12
#define STM_IPv1_MA_OFFSET (1 << STM_IPv1_MA_SHIFT)

/* IP V1: The offset (from current channel) for "TS-STP" event generation */
#define STM_IPv1_TS_OFFSET 8

/* IP V1: Total number of channels available for a core
 *        With STM-1 we can allocate all channels of a master to a core
 *        Initiator0 -> Arm core 0
 *        Initiator1 -> Arm core 1
 */
#define STM_IPv1_CH_NUMBER 256


/* IP V3: A new channel ID each 256 bytes */
#define STM_IPv3_CH_SHIFT  8
#define STM_IPv3_CH_OFFSET (1 << STM_IPv3_CH_SHIFT)

/* IP V3: A new core (master) ID at channel offset 128
 * (see STM IP V3 register mapping for offset)
 */
#define STM_IPv3_MA_SHIFT  (7 + STM_IPv3_CH_SHIFT)
#define STM_IPv3_MA_OFFSET (1 << STM_IPv3_MA_SHIFT)

/* IP V3: The offset (from current channel) for "TS-STP" event generation
 * (see STM IP V3 register mapping for offset)
 */
#define STM_IPv3_TS_OFFSET 0x18

/* IP V3: Total number of channels available for a core
 * With STM-2 The two Arm cores of the SMP are seeing a window
 * of 256 channels, sharing the same initiator
 *        Initiator0/ChanOFS 0   -> Arm core 0
 *        Initiator0/ChanOFS 128 -> Arm core 1
 */
#define STM_IPv3_CH_NUMBER 128

#define STM_IPv1_VERSION     1
#define STM_IPv3_VERSION     3
#define STM_IPv1_PERI_ID     0x00380dec
#define STM_IPv3_PERI_ID     0x00680dec
#define STM_CELL_ID          0xb105f00d

/* Channel allocation */
/* COMMON CHANNEL DEFINITIONS */
#define MTT_CH_META	 0x00	/* Reserved. */
#define MTT_CH_RAW	 0x01	/* Raw packets channel */
#define MTT_CH_IRQ	 0x02	/* Interrupts */
#define MTT_CH_SLOG	 0x03	/* System log channel */

/* STLINUX CHANNEL DEFINITIONS */
#define MTT_CH_LIN_DATA  0x05	/* Kernel mode data, standard kptrace stuff */
#define MTT_CH_LIN_CTXT  0x06	/* Kernel mode, context switches */
#define MTT_CH_LIN_KMUX  0x07	/* Kernel mode downmux */
#define MTT_CH_LIN_KMETA 0x08	/* Kernel mode meta channel */

#define MTT_CH_MUX	 0x0F	/* Application downmux channel */

/* do not define common channels for this value and beyond,
 * as they may overlap on other cores sharing an STP channel range.
 **/
#define MTT_CH_INVALID	 0x10	/* Reserved for users starting from here */

/* user-defined channels margins */
#define MTT_CH_LIN_APP_FIRST		(MTT_CH_INVALID)
#define MTT_CH_LIN_APP_INVALID(ch_num)	(MTT_CH_INVALID + \
					(ch_num-MTT_CH_INVALID)/2)
#define MTT_CH_LIN_KER_FIRST(ch_num)	(MTT_CH_LIN_APP_INVALID(ch_num))
#define MTT_CH_LIN_KER_INVALID(ch_num)	(ch_num)


#endif /*__STM_SYSTRACE_H_*/
