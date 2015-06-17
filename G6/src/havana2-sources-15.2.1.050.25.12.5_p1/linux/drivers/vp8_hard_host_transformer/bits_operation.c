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

#include "bits_operation.h"
#include "vp8hard.h"

/****************************************************************************
 *
 *  ROUTINE       :     vp8hwdDecodeBool128
 *
 *  INPUTS        :     vpBoolCoder_t *br : pointer to instance of a boolean decoder.
 *
 *  OUTPUTS       :     None.
 *
 *  RETURNS       :        int: Next decoded symbol (0 or 1)
 *
 *  FUNCTION      :     This function determines the next value stored in the
 *                        boolean coder based upon a fixed probability of 0.5
 *                      (128 in normalized units).
 *
 *  SPECIAL NOTES :     vp8hwdDecodeBool128() is a special case of vp8hwdDecodeBool()
 *                      where the input probability is fixed at 128.
 *
 ****************************************************************************/
unsigned int vp8hwdDecodeBool128(vpBoolCoder_t *br)
{
	unsigned int bit = 0;
	unsigned int split;
	unsigned int bigsplit;
	unsigned int count = br->count;
	unsigned int range = br->range;
	unsigned int value = br->value;

	split = (range + 1) >> 1;
	bigsplit = (split << 24);

	range = split;

	if (value >= bigsplit) {
		range = (br->range - split);
		value = (value - bigsplit);
		bit = 1;
	}

	if (range >= 0x80) {
		br->value = value;
		br->range = range;
		return bit;
	} else {
		range <<= 1;
		value <<= 1;

		if (!--count) {
			/* no more stream to read? */
			if (br->pos >= br->streamEndPos) {
				br->strmError = 1;
				return 0; /* any value, not valid */
			}
			count = 8;
			value |= br->buffer[br->pos];
			br->pos++;
		}
	}

	br->count = count;
	br->value = value;
	br->range = range;

	return bit;
}

unsigned int vp8hwdReadBits(vpBoolCoder_t *br, signed int bits)
{
	unsigned int z = 0;
	signed int bit;

	for (bit = bits - 1; bit >= 0; bit--) {
		z |= (vp8hwdDecodeBool128(br) << bit);
	}

	return z;
}

