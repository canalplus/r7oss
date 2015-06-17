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

#include "spanning_startcode.h"

/**
 * Check if after concatenation buf1 and buf2 there will be Start Code crossing
 * boundary of buffers.
 *
 * \return number of buffers from buf2 that attached to buf1 form Start Code.
 * In case that there is not enough data in buf2 then assume they will form startcode
 * startcode is a sequence of bytes: 0 0 1 x
 *
 * Example:
 *   buf1= "... 0 0 1"                => 1
 *   buf1= "..... 0 0" buf2= "1..."   => 2
 *   buf1= "..... 0 0" buf2= ""       => 2 (there is no enough data in buf2 so assume it will make start code)
 *
 */
int ScanForSpanningStartcode(unsigned char *buf1, int len1, unsigned char *buf2, int len2)
{
    /*  ______  ______
     *  buf1 | | buf2
     *  a b c    k l
     *
     *  0 0 1            => 1
     *    0 0    1       => 2
     *      0    0 1     => 3
     * */
    unsigned a, b, c, k, l;
    a = b = c = k = l = 0xff;

    switch (len1)
    {
    default:

    // fallthrough
    case 3: a = buf1[len1 - 3];

    // fallthrough
    case 2: b = buf1[len1 - 2];

    // fallthrough
    case 1: c = buf1[len1 - 1];
        break;

    case 0:
        return 0;
    }

    switch (len2)
    {
    default:

    // fallthrough
    case 2: l = buf2[1];

    // fallthrough
    case 1: k = buf2[0];
        break;

    case 0:
        break;
    }

    if (a == 0 && b == 0 && c == 1)
    {
        return 1;
    }

    if (b == 0 && c == 0)
    {
        if ((len2 >= 1 && k == 1)  || len2 == 0)
        {
            return 2;
        }
    }

    if (c == 0)
    {
        switch (len2)
        {
        default:

        // fallthrough
        case 2:
            if (k == 0 && l == 1)
            {
                return 3;
            }
            else
            {
                return 0;
            }

            break;

        case 1:
            if (k == 0)
            {
                return 3;    // possible there is "1" in next buffer;
            }
            else
            {
                return 0;    // no hope for startcode
            }

            break;

        case 0:
            return 3;
        }
    }

    return 0;
}


/*
 * simple unit tests
 * to compile and run execute:
 * gcc -D SPANNING_SC_UNITARYTEST -I../include spanning_startcode.cpp  -o sp_test && ./sp_test
 */

#ifdef SPANNING_SC_UNITARYTEST

int cntOk = 0, cntBad = 0;
#define TEST_INT( EXP, V) { int r = EXP; if(r != V){  printf("Test failed. %d != %d. line %d\n",r,V,__LINE__); cntBad++;} else cntOk++;}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
main()
{
    unsigned char a[5], b[5];
    memset(a, 0xff, 5); memset(b, 0xff, 5);
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 0);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 0);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[3] = 0; a[4] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 0);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[2] = 0;  a[3] = 0; a[4] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 1);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[2] = 0;  a[3] = 0; a[4] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 0), 1);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[3] = 0;  a[4] = 0; b[0] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 2);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[3] = 0;  a[4] = 0; b[0] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 1), 2);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[3] = 0;  a[4] = 0; b[0] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 0), 2);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0; b[0] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 0);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 0), 3);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0; b[0] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 1), 3);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0; b[0] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 0);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[4] = 0; b[0] = 0, b[1] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 5, b, 5), 3);
    // short A (len1 =1)
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[0] = 0; b[0] = 0, b[1] = 1;
    TEST_INT(ScanForSpanningStartcode(a, 1, b, 5), 3);
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[0] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 1, b, 5), 0);
    // short A and B
    memset(a, 0xff, 5); memset(b, 0xff, 5); a[0] = 0; b[0] = 0;
    TEST_INT(ScanForSpanningStartcode(a, 1, b, 1), 3);
    printf("ok: %d bad: %d\n", cntOk, cntBad);
}
#endif
