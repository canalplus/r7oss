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

#ifndef H_PES
#define H_PES

#define PES_START_CODE_MASK                     0xf0
#define PES_START_CODE_MASK_VC1                 0xff
#define PES_START_CODE_AUDIO                    0xc0
#define PES_START_CODE_VIDEO                    0xe0
#define PES_START_CODE_VIDEO_VC1                0xfd
//This is a fake start code used to transmit information through data interface rather than control interfaces.
#define PES_START_CODE_CONTROL                  0xfb

#define IS_PES_START_CODE_VIDEO(x)              ((PES_START_CODE_VIDEO == ((x) & PES_START_CODE_MASK)) || (PES_START_CODE_VIDEO_VC1 == (x)))
#define IS_PES_START_CODE_AUDIO(x)              (PES_START_CODE_AUDIO == ((x) & PES_START_CODE_MASK))

#define PES_PADDING_START_CODE                  0xbe
#define PES_PADDING_INITIAL_HEADER_SIZE         6
#define PES_PADDING_SKIP(P)                     ((unsigned int)(((P)[4] << 8) | (P)[5]))

#define PES_START_CODE_PRIVATE_STREAM_1         0xbd
#define IS_PES_START_CODE_PRIVATE_STREAM_1(x)   (PES_START_CODE_PRIVATE_STREAM_1 == (x))

#define PES_START_CODE_EXTENDED_STREAM_ID       0xfd
#define IS_PES_START_CODE_EXTENDED_STREAM_ID(x) (PES_START_CODE_EXTENDED_STREAM_ID == (x))

#define PRIVATE_STREAM_1_SUB_PICTURE            0x20
#define PRIVATE_STREAM_1_SUB_PICTURE_MASK       (~0x1f)
#define IS_PRIVATE_STREAM_1_SUB_PICTURE(x)      (PRIVATE_STREAM_1_SUB_PICTURE == ((x) & PRIVATE_STREAM_1_SUB_PICTURE_MASK)

#define PRIVATE_STREAM_1_VBI                    0x48
#define IS_PRIVATE_STREAM_1_VBI(x)              (PRIVATE_STREAM_1_VBI == (x))

#define PRIVATE_STREAM_1_EXTENDED_SUB_PICTURE   0x60
#define PRIVATE_STREAM_1_EXTENDED_SUB_PICTURE_MASK (~0x1f)
#define IS_PRIVATE_STREAM_1_EXTENDED_SUB_PICTURE(x) (PRIVATE_STREAM_1_EXTENDED_SUB_PICTURE == \
                             ((x) & PRIVATE_STREAM_1_EXTENDED_SUB_PICTURE_MASK))
#define PRIVATE_STREAM_1_AC3                    0x80
#define PRIVATE_STREAM_1_AC3_MASK               (~0x7)
#define IS_PRIVATE_STREAM_1_AC3(x)              (PRIVATE_STREAM_1_AC3 == ((x) & PRIVATE_STREAM_1_AC3_MASK))


#define PRIVATE_STREAM_1_DTS                    0x88
#define PRIVATE_STREAM_1_DTS_MASK               (~0x7)
#define IS_PRIVATE_STREAM_1_DTS(x)              (PRIVATE_STREAM_1_DTS == ((x) & PRIVATE_STREAM_1_DTS_MASK))

#define PRIVATE_STREAM_1_SDDS                   0x90
#define PRIVATE_STREAM_1_SDDS_MASK              (~0x7)
#define IS_PRIVATE_STREAM_1_SDDS(x)             (PRIVATE_STREAM_1_SDDS == ((x) & PRIVATE_STREAM_1_SDDS_MASK))

#define PRIVATE_STREAM_1_LPCM                   0xa0
#define PRIVATE_STREAM_1_LPCM_MASK              (~0x7)
#define IS_PRIVATE_STREAM_1_LPCM(x)             (PRIVATE_STREAM_1_LPCM == ((x) & PRIVATE_STREAM_1_LPCM_MASK))

#define PES_INITIAL_HEADER_SIZE                 9
#define PES_MAX_HEADER_SIZE                     (9+256)
#define PES_HEADER_SIZE(P)                      ((unsigned int)(9+(P)[8]))
#define PES_CONTROL_SIZE                        26

//These are identifiers inserted in dedicated PES packet, coded on 1 byte, on the 15th byte.
#define PES_CONTROL_REQ_TIME                              0
#define PES_CONTROL_BRK_FWD                               1
#define PES_CONTROL_BRK_BWD                               2
#define PES_CONTROL_BRK_BWD_SMOOTH                        3
#define PES_CONTROL_BRK_END_OF_STREAM                     4
#define PES_CONTROL_BRK_SPLICING                          5
#endif
