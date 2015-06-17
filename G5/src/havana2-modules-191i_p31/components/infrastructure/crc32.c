/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : crc32.c
Author :           Daniel

Simple 32-bit CRC implementation for debugging purpose.


Date        Modification                                    Name
----        ------------                                    --------
30-Aug-07   Created                                         Daniel

************************************************************************/

/**
 * Perform a 32-bit CRC of the input data.
 *
 * Algorithm comes from the extended material supporting the book
 * Hacker's Delight (Addison-Wesley, 2003) by Henry S. Jr Warren.
 * See http://www.hackersdelight.org/crc.pdf .
 * 
 * \todo Move the CRC lookup table into static data.
 */
unsigned int crc32(unsigned char *data, unsigned int length)
{
    int i;
    unsigned int byte, crc, mask;
    unsigned char *end;
    static unsigned int table[256];

    /*
     * Set up the table, if necessary.
     * 
     * If two threads 'collide' at this point it doesn't really matter,
     * all that will happen if both threads will initialize the lookup
     * table with the same values, then proceed as normal.
     */

    if (table[1] == 0) {
        for (byte = 0; byte <= 255; byte++) {
            crc = byte;
            for (i = 7; i >= 0; i--) { // Do eight times.
                mask = -(crc & 1);
                crc = (crc >> 1) ^ (0xedb88320 & mask);
            }
            table[byte] = crc;
        }
    }

    /* Through with table setup, now calculate the CRC. */

    crc = 0xffffffff;
    end = data + length;
    while (data < end) {
        byte = *data++; // Get next byte.
        crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF];
    }

    return ~crc;
}
