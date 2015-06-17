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

#ifndef UNIT_TEST_RLE_HELPERS_H_
#define UNIT_TEST_RLE_HELPERS_H_

extern int rle_load_image (char           *filename,
                           unsigned long  *width_ptr,
                           unsigned long  *height_ptr,
                           unsigned long  *depth_ptr,
                           unsigned long  *rawmode_ptr,
                           unsigned long  *bdmode_ptr,
                           unsigned long  *topfirst_ptr,
                           unsigned long  *buffer_size_ptr,
                           void          **payload_ptr,
                           unsigned long  *payload_size_ptr,
                           void          **colormap_ptr,
                           unsigned long  *colormap_size_ptr);

#endif /* __UNIT_TEST_RLE_HELPERS_H__ */
