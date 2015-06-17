/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

System Module	:   HPI4000  (PC and DSP)

Description	:   Definitions of Host flags and host commands passed
		    between PC and 56301DSP over PCI bus. This file is shared
		    between HPI4000.C on the PC side and the DSP message
		    routines to ensure consistency

(C) Copyright AudioScience Inc. 2002-2007
******************************************************************************/
#ifndef _HPI4000_H_
#define _HPI400_H_

/* The host flags are 3 bits, so there are only 8 possible values */
#define HF_MASK		    0x0038	/* also applies to HCTR,DCTR,DSR */
#define HF_CLEAR	    (~HF_MASK)

/* Used for PC->DSP */
#define CMD_NONE	0
#define CMD_SEND_MSG	8
#define CMD_GET_RESP	16
#define CMD_SEND_DATA	24
#define CMD_GET_DATA	32

/* Used for DSP->PC */
#define HF_DSP_STATE_RESET	 0
#define HF_DSP_STATE_IDLE	 8
#define HF_DSP_STATE_RESP_READY 16
#define HF_DSP_STATE_BUSY	24
#define HF_DSP_STATE_CMD_OK	32
#define HF_DSP_STATE_ERROR	40
#define HF_DSP_STATE_WRAP_READY 48
#define HF_PCI_HANDSHAKE	56

/* Each of the CVR values must correspond to an interrupt vector address
in the 56301.  The value here needs CVR_INT added to generate an interrupt
when written to HCVR (Use function DpiCommand())
*/
#define CVR_NMI		    0x8000	/* NMI bit */
#define CVR_INT		    0x0001
#define CVR_RESET	    0x0072
#define CVR_NMIRESET	    (CVR_NMI+CVR_RESET)	/* NMI version to reset */

#define CVR_RESTART	    0x004E
#define CVR_CMD		    0x005a
#define CVR_CMDRESET	    0x005e
#define CVR_DMASTOP			0x005c

/* Size of audio data bursts between PC and DSP */
#define DATA_PACKET_WORDS    4096

#endif

/* Note: don't use / / commments in this file!
*/
