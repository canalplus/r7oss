/*
 * Copyright (C) 2006 ST-Microelectronics R&D <alain.clement@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef RLE_BUILD_PACKET_H_
#define RLE_BUILD_PACKET_H_

#include <directfb.h>

DFBResult
rle_build_packet (
    u8        *buffer,       /*    I/O  :    Packet buffer            */
    u32        buffer_size,  /*    I    :    Packet size              */
    u32        width,        /*    I    :    Image width              */
    u32        height,       /*    I    :    Image height             */
    u32        depth,        /*    I    :    Pixels depth             */
    u32        rawmode,      /*    I    :    Raw mode flag      (0/1) */
    u32        bdmode,       /*    I    :    BD-RLE mode flag   (0/1) */
    u32        topfirstmode, /*    I    :    Topfirst mode flag (0/1) */
    DFBColor  *colormap,     /*    I    :    Palette / NULL           */
    u32        colormap_size,/*    I    :    Palette size             */
    void      *payload,      /*    I    :    Payload / NULL           */
    u32        payload_size  /*    I    :    Payload size             */
);

#define RLE_PREAMBLE_SIZE         (14)
#define RLE_BIHEADER_SIZE         (40)
#define RLE_HEADER_SIZE           (RLE_PREAMBLE_SIZE+RLE_BIHEADER_SIZE)

#endif /*RLE_BUILD_PACKET_H_*/
