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
#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>

#include "osdev_mem.h"

#include "vp8hard.h"
#include "defcfg.h"
#include "dwl.h"
#include "vp8hard_decode.h"
#include "bits_operation.h"

// ////////////////////////////////////////////////////////////////////////////////
//
//    Local data

#define STREAM_TRACE(msg,arg)


#define SCAN(i)         HWIF_SCAN_MAP_ ## i
#define MAX_DIRECT_MVS      (3000)

static const unsigned int ScanTblRegId[16] = { 0 /* SCAN(0) */ ,
                                               SCAN(1), SCAN(2), SCAN(3), SCAN(4), SCAN(5), SCAN(6), SCAN(7), SCAN(8),
                                               SCAN(9), SCAN(10), SCAN(11), SCAN(12), SCAN(13), SCAN(14), SCAN(15)
                                             };

#define BASE(i)         HWIF_DCT_STRM ## i ## _BASE
static const unsigned int DctBaseId[] = { HWIF_VP6HWPART2_BASE,
                                          BASE(1), BASE(2), BASE(3), BASE(4), BASE(5), BASE(6), BASE(7)
                                        };

#define OFFSET(i)         HWIF_DCT ## i ## _START_BIT
static const unsigned int DctStartBit[] = { HWIF_STRM_START_BIT,
                                            OFFSET(1), OFFSET(2), OFFSET(3), OFFSET(4),
                                            OFFSET(5), OFFSET(6), OFFSET(7)
                                          };

#define TAP(i, j)       HWIF_PRED_BC_TAP_ ## i ## _ ## j

static const unsigned int TapRegId[8][4] = {
	{TAP(0, 0), TAP(0, 1), TAP(0, 2), TAP(0, 3)},
	{TAP(1, 0), TAP(1, 1), TAP(1, 2), TAP(1, 3)},
	{TAP(2, 0), TAP(2, 1), TAP(2, 2), TAP(2, 3)},
	{TAP(3, 0), TAP(3, 1), TAP(3, 2), TAP(3, 3)},
	{TAP(4, 0), TAP(4, 1), TAP(4, 2), TAP(4, 3)},
	{TAP(5, 0), TAP(5, 1), TAP(5, 2), TAP(5, 3)},
	{TAP(6, 0), TAP(6, 1), TAP(6, 2), TAP(6, 3)},
	{TAP(7, 0), TAP(7, 1), TAP(7, 2), TAP(7, 3)}
};

/* VP7 QP LUTs */
static const unsigned short YDcQLookup[128] = {
	4,    4,    5,    6,    6,    7,    8,    8,
	9,   10,   11,   12,   13,   14,   15,   16,
	17,   18,   19,   20,   21,   22,   23,   23,
	24,   25,   26,   27,   28,   29,   30,   31,
	32,   33,   33,   34,   35,   36,   36,   37,
	38,   39,   39,   40,   41,   41,   42,   43,
	43,   44,   45,   45,   46,   47,   48,   48,
	49,   50,   51,   52,   53,   53,   54,   56,
	57,   58,   59,   60,   62,   63,   65,   66,
	68,   70,   72,   74,   76,   79,   81,   84,
	87,   90,   93,   96,  100,  104,  108,  112,
	116,  121,  126,  131,  136,  142,  148,  154,
	160,  167,  174,  182,  189,  198,  206,  215,
	224,  234,  244,  254,  265,  277,  288,  301,
	313,  327,  340,  355,  370,  385,  401,  417,
	434,  452,  470,  489,  509,  529,  550,  572
};

static const unsigned short YAcQLookup[128] = {
	4,    4,    5,    5,    6,    6,    7,    8,
	9,   10,   11,   12,   13,   15,   16,   17,
	19,   20,   22,   23,   25,   26,   28,   29,
	31,   32,   34,   35,   37,   38,   40,   41,
	42,   44,   45,   46,   48,   49,   50,   51,
	53,   54,   55,   56,   57,   58,   59,   61,
	62,   63,   64,   65,   67,   68,   69,   70,
	72,   73,   75,   76,   78,   80,   82,   84,
	86,   88,   91,   93,   96,   99,  102,  105,
	109,  112,  116,  121,  125,  130,  135,  140,
	146,  152,  158,  165,  172,  180,  188,  196,
	205,  214,  224,  234,  245,  256,  268,  281,
	294,  308,  322,  337,  353,  369,  386,  404,
	423,  443,  463,  484,  506,  529,  553,  578,
	604,  631,  659,  688,  718,  749,  781,  814,
	849,  885,  922,  960, 1000, 1041, 1083, 1127

};

static const unsigned short Y2DcQLookup[128] = {
	7,    9,   11,   13,   15,   17,   19,   21,
	23,   26,   28,   30,   33,   35,   37,   39,
	42,   44,   46,   48,   51,   53,   55,   57,
	59,   61,   63,   65,   67,   69,   70,   72,
	74,   75,   77,   78,   80,   81,   83,   84,
	85,   87,   88,   89,   90,   92,   93,   94,
	95,   96,   97,   99,  100,  101,  102,  104,
	105,  106,  108,  109,  111,  113,  114,  116,
	118,  120,  123,  125,  128,  131,  134,  137,
	140,  144,  148,  152,  156,  161,  166,  171,
	176,  182,  188,  195,  202,  209,  217,  225,
	234,  243,  253,  263,  274,  285,  297,  309,
	322,  336,  350,  365,  381,  397,  414,  432,
	450,  470,  490,  511,  533,  556,  579,  604,
	630,  656,  684,  713,  742,  773,  805,  838,
	873,  908,  945,  983, 1022, 1063, 1105, 1148
};

static const unsigned short Y2AcQLookup[128] = {
	7,    9,   11,   13,   16,   18,   21,   24,
	26,   29,   32,   35,   38,   41,   43,   46,
	49,   52,   55,   58,   61,   64,   66,   69,
	72,   74,   77,   79,   82,   84,   86,   88,
	91,   93,   95,   97,   98,  100,  102,  104,
	105,  107,  109,  110,  112,  113,  115,  116,
	117,  119,  120,  122,  123,  125,  127,  128,
	130,  132,  134,  136,  138,  141,  143,  146,
	149,  152,  155,  158,  162,  166,  171,  175,
	180,  185,  191,  197,  204,  210,  218,  226,
	234,  243,  252,  262,  273,  284,  295,  308,
	321,  335,  350,  365,  381,  398,  416,  435,
	455,  476,  497,  520,  544,  569,  595,  622,
	650,  680,  711,  743,  776,  811,  848,  885,
	925,  965, 1008, 1052, 1097, 1144, 1193, 1244,
	1297, 1351, 1407, 1466, 1526, 1588, 1652, 1719
};

static const unsigned short UvDcQLookup[128] = {
	4,    4,    5,    6,    6,    7,    8,    8,
	9,   10,   11,   12,   13,   14,   15,   16,
	17,   18,   19,   20,   21,   22,   23,   23,
	24,   25,   26,   27,   28,   29,   30,   31,
	32,   33,   33,   34,   35,   36,   36,   37,
	38,   39,   39,   40,   41,   41,   42,   43,
	43,   44,   45,   45,   46,   47,   48,   48,
	49,   50,   51,   52,   53,   53,   54,   56,
	57,   58,   59,   60,   62,   63,   65,   66,
	68,   70,   72,   74,   76,   79,   81,   84,
	87,   90,   93,   96,  100,  104,  108,  112,
	116,  121,  126,  131,  132,  132,  132,  132,
	132,  132,  132,  132,  132,  132,  132,  132,
	132,  132,  132,  132,  132,  132,  132,  132,
	132,  132,  132,  132,  132,  132,  132,  132,
	132,  132,  132,  132,  132,  132,  132,  132
};


static const unsigned short UvAcQLookup[128] = {
	4,    4,    5,    5,    6,    6,    7,    8,
	9,   10,   11,   12,   13,   15,   16,   17,
	19,   20,   22,   23,   25,   26,   28,   29,
	31,   32,   34,   35,   37,   38,   40,   41,
	42,   44,   45,   46,   48,   49,   50,   51,
	53,   54,   55,   56,   57,   58,   59,   61,
	62,   63,   64,   65,   67,   68,   69,   70,
	72,   73,   75,   76,   78,   80,   82,   84,
	86,   88,   91,   93,   96,   99,  102,  105,
	109,  112,  116,  121,  125,  130,  135,  140,
	146,  152,  158,  165,  172,  180,  188,  196,
	205,  214,  224,  234,  245,  256,  268,  281,
	294,  308,  322,  337,  353,  369,  386,  404,
	423,  443,  463,  484,  506,  529,  553,  578,
	604,  631,  659,  688,  718,  749,  781,  814,
	849,  885,  922,  960, 1000, 1041, 1083, 1127
};

/**/
static const unsigned int mcFilter[8][6] = {
	{ 0,  0,  128,    0,   0,  0 },
	{ 0, -6,  123,   12,  -1,  0 },
	{ 2, -11, 108,   36,  -8,  1 },
	{ 0, -9,   93,   50,  -6,  0 },
	{ 3, -16,  77,   77, -16,  3 },
	{ 0, -6,   50,   93,  -9,  0 },
	{ 1, -8,   36,  108, -11,  2 },
	{ 0, -1,   12,  123,  -6,  0 }
};

static const unsigned char MvUpdateProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
	{
		237, 246, 253, 253, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 250, 250, 252, 254, 254
	},
	{
		231, 243, 245, 253, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 251, 251, 254, 254, 254
	}
};

static const unsigned char Vp8DefaultMvProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
	{
		162, 128, 225, 146, 172, 147, 214, 39, 156,
		128, 129, 132, 75, 145, 178, 206, 239, 254, 254
	},
	{
		164, 128, 204, 170, 119, 235, 140, 230, 228,
		128, 130, 130, 74, 148, 180, 203, 236, 254, 254
	}
};

static const unsigned char Vp7DefaultMvProbs[2][VP7_MV_PROBS_PER_COMPONENT] = {
	{
		162, 128, 225, 146, 172, 147, 214, 39, 156,
		247, 210, 135,  68, 138, 220, 239, 246
	},
	{
		164, 128, 204, 170, 119, 235, 140, 230, 228,
		244, 184, 201,  44, 173, 221, 239, 253
	}
};

static const unsigned char CoeffUpdateProbs[4][8][3][11] = {
	{
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255, },
			{250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255, },
			{234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255, },
		},
		{
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255, },
			{234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255, },
			{251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255, },
			{248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },

		},
	},
};


static const unsigned char DefaultCoeffProbs [4][8][3][11] = {
	{
		{
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
		},
		{
			{ 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
			{ 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
			{ 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128}
		},
		{
			{ 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
			{ 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
			{ 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128}
		},
		{
			{ 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
			{ 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
			{ 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
			{ 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
			{ 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128}
		},
		{
			{ 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
			{ 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
			{ 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
			{ 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
			{ 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
		}
	},
	{
		{
			{ 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62},
			{ 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1},
			{ 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128}
		},
		{
			{ 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
			{ 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
			{ 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128}
		},
		{
			{ 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
			{ 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
			{ 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128}
		},
		{
			{ 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
			{ 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
			{ 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128}
		},
		{
			{ 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
			{ 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
			{ 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128}
		},
		{
			{ 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
			{ 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
			{ 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128}
		},
		{
			{ 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
			{ 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
			{ 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128}
		},
		{
			{ 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
			{ 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
			{ 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128}
		}
	},
	{
		{
			{ 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
			{ 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
			{ 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128}
		},
		{
			{ 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
			{ 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
			{ 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128}
		},
		{
			{ 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
			{ 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
			{ 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128}
		},
		{
			{ 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
			{ 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
			{ 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
			{ 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
			{ 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128}
		},
		{
			{ 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
			{ 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
			{ 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128}
		},
		{
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
		}
	},
	{
		{
			{ 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
			{ 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
			{ 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128}
		},
		{
			{ 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
			{ 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
			{ 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128}
		},
		{
			{ 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
			{ 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
			{ 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128}
		},
		{
			{ 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
			{ 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
			{ 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
		},
		{
			{ 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
			{ 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
			{ 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128}
		},
		{
			{ 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
			{ 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
			{ 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128}
		},
		{
			{ 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
			{ 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
			{ 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128}
		},
		{
			{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
			{ 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
		}
	}
};


/*------------------------------------------------------------------------------
    Function name   : VP8DecDecode
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst decInst
    Argument        : const VP8DecInput * pInput
    Argument        : VP8DecFrame * pOutput
------------------------------------------------------------------------------*/
VP8DecRet VP8DecDecode(VP8DecInst decInst, VP8_ParamPicture_t *Picture_Data_Struct_p,
                       VP8DecInput *pInput)
{
	VP8DecContainer_t *pDecCont = (VP8DecContainer_t *) decInst;
	DecAsicBuffers_t *pAsicBuff;
	signed int ret;
	unsigned int asic_status;
//    unsigned int errorConcealment = 0;

	assert(pInput);
	assert(pDecCont);

	/* Check for valid decoder instance */
	if (pDecCont->checksum != pDecCont) {
		pr_err("Error: %s Decoder not initialized\n", __func__);
		return (VP8DEC_NOT_INITIALIZED);
	}

	if (pInput->dataLen == 0 ||
	    pInput->dataLen > DEC_X170_MAX_STREAM ||
	    X170_CHECK_VIRTUAL_ADDRESS(pInput->pStream) ||
	    X170_CHECK_BUS_ADDRESS(pInput->streamBusAddress)) {
		pr_err("Error: %s Memory issue\n", __func__);
		return VP8DEC_PARAM_ERROR;
	}

	if ((pInput->DataBuffers.pPicBufferY != NULL && pInput->DataBuffers.picBufferBusAddressY == 0) ||
	    (pInput->DataBuffers.pPicBufferY == NULL && pInput->DataBuffers.picBufferBusAddressY != 0) ||
	    (pInput->DataBuffers.pPicBufferC != NULL && pInput->DataBuffers.picBufferBusAddressC == 0) ||
	    (pInput->DataBuffers.pPicBufferC == NULL && pInput->DataBuffers.picBufferBusAddressC != 0) ||
	    (pInput->DataBuffers.pPicBufferY == NULL && pInput->DataBuffers.pPicBufferC != 0) ||
	    (pInput->DataBuffers.pPicBufferY != NULL && pInput->DataBuffers.pPicBufferC == 0)) {
		pr_err("Error: %s Invalid arg value\n", __func__);
		return VP8DEC_PARAM_ERROR;
	}

	/* aliases */
	pAsicBuff = pDecCont->asicBuff;

	/* application indicates that slice mode decoding should be used ->
	 * disabled unless WebP and PP not used */
	if (pDecCont->decStat != VP8DEC_MIDDLE_OF_PIC &&
	    pDecCont->intraOnly && pInput->sliceHeight &&
	    pDecCont->pp.ppInstance == NULL) {
		pDecCont->sliceHeight = pInput->sliceHeight;
	}

	if (pDecCont->intraOnly && pInput->DataBuffers.pPicBufferY) {
		pDecCont->userMem = 1;
		pDecCont->asicBuff->userMem.pPicBufferY = pInput->DataBuffers.pPicBufferY;
		pDecCont->asicBuff->userMem.picBufferBusAddrY =
		        pInput->DataBuffers.picBufferBusAddressY;
		pDecCont->asicBuff->userMem.pPicBufferC = pInput->DataBuffers.pPicBufferC;
		pDecCont->asicBuff->userMem.picBufferBusAddrC =
		        pInput->DataBuffers.picBufferBusAddressC;
	}

	if ((pDecCont->decStat == VP8DEC_NEW_HEADERS) || (pDecCont->decStat == VP8DEC_INITIALIZED)) {
		pDecCont->decStat = VP8DEC_DECODING;

		if (VP8HwdAsicAllocatePictures(pDecCont) != 0) {
			pr_err("Error: %s Picture memory allocation failed\n", __func__);
			return VP8DEC_MEMFAIL;
		}
	}
	if (pDecCont->decStat != VP8DEC_MIDDLE_OF_PIC) {
		/* decode frame tag, copy data from parsed fields */
		vp8hwdDecodeFrameTag(Picture_Data_Struct_p, pInput->pStream, &pDecCont->decoder);
		// pInput->pStream += pDecCont->decoder.frameTagSize; /* Skip used bytes before initializing BoolDecoder*/
		//pInput->dataLen -= pDecCont->decoder.frameTagSize; /*size of frameTag header in VP8*/

		/* When on key-frame, reset probabilities and such */
		if (pDecCont->decoder.keyFrame) {
			vp8hwdResetDecoder(&pDecCont->decoder, pAsicBuff);
		}
		/* intra only and non key-frame */
		else if (pDecCont->intraOnly) {
			pr_err("Error: %s Intra Only and non key-frame\n", __func__);
			return VP8DEC_STRM_ERROR;
		}

		if (pDecCont->decoder.keyFrame || pDecCont->decoder.vpVersion > 0) {
			pAsicBuff->dcPred[0] = pAsicBuff->dcPred[1] =
			                               pAsicBuff->dcMatch[0] = pAsicBuff->dcMatch[1] = 0;
		}

		/* Decode frame header (now starts bool coder as well) */
		ret = vp8hwdDecodeFrameHeader(Picture_Data_Struct_p,
		                              pInput->pStream + pDecCont->decoder.frameTagSize,
		                              pInput->dataLen - pDecCont->decoder.frameTagSize,
		                              &pDecCont->bc, &pDecCont->decoder);

		if (ret != HANTRO_OK) {
			pr_err("Error: %s Frame header decoding failed\n", __func__);
			if (!pDecCont->picNumber || pDecCont->decStat != VP8DEC_DECODING) {
				pr_err("Error: %s Frame header decoding failed\n", __func__);
				return VP8DEC_STRM_ERROR;
			} else {
				vp8hwdFreeze(pDecCont);
				pr_err("Error: %s VP8DEC_PIC_DECODED\n", __func__);
				return VP8DEC_PIC_DECODED;
			}
		}

		ret = vp8hwdSetPartitionOffsets(pInput->pStream, pInput->dataLen,
		                                &pDecCont->decoder);
		if (ret != HANTRO_OK) {
			if (!pDecCont->picNumber || pDecCont->decStat != VP8DEC_DECODING) {
				pr_err("Error: %s hwdSetPartitionOffsets\n", __func__);
				return VP8DEC_STRM_ERROR;
			} else {
				vp8hwdFreeze(pDecCont);
				pr_err("Error: %s VP8DEC_PIC_DECODED\n", __func__);
				return VP8DEC_PIC_DECODED;
			}
		}

		/* If we are here and dimensions are still 0, it means that we have
		 * yet to decode a valid keyframe, in which case we must give up. */
		if (pDecCont->width == 0 || pDecCont->height == 0) {
			return VP8DEC_STRM_PROCESSED;
		}

		/* If output picture is broken and we are not decoding a base frame,
		 * don't even start HW, just output same picture again. */
		if (!pDecCont->decoder.keyFrame &&
		    pDecCont->pictureBroken &&
		    pDecCont->intraFreeze) {
			vp8hwdFreeze(pDecCont);
			pr_err("Error: %s VP8DEC_PIC_DECODED\n", __func__);
			return VP8DEC_PIC_DECODED;
		}
	}

	/*Update pointer to asicbuffers (codec adaptation to player2)*/
	vp8HwdAsicBufUpdate(pDecCont, pInput);
	if (pDecCont->decStat != VP8DEC_MIDDLE_OF_PIC) {
		pDecCont->refToOut = 0;

		/* prepare asic */
		VP8HwdAsicProbUpdate(pDecCont);

		VP8HwdAsicInitPicture(pDecCont);

		VP8HwdAsicStrmPosUpdate(pDecCont, pInput->streamBusAddress);

		/* PP setup stuff */
		vp8hwdPreparePpRun(pDecCont);
	} else {
		VP8HwdAsicContPicture(pDecCont);
	}

	/* run the hardware */
	asic_status = VP8HwdAsicRun(pDecCont);

	/* Rollback entropy probabilities if refresh is not set */
	if (pDecCont->decoder.refreshEntropyProbs == HANTRO_FALSE) {
		memcpy(&pDecCont->decoder.entropy, &pDecCont->decoder.entropyLast,
		       sizeof(vp8EntropyProbs_t));
		memcpy(pDecCont->decoder.vp7ScanOrder, pDecCont->decoder.vp7PrevScanOrder,
		       sizeof(pDecCont->decoder.vp7ScanOrder));
	}

	/* Handle system error situations */
	if (asic_status == VP8HWDEC_SYSTEM_TIMEOUT) {
		/* This timeout is DWL(software/os) generated */
//        pr_err("Error: %s VP8DecDecode# VP8DEC_HW_TIMEOUT, SW generated\n", __func__);
		return VP8DEC_HW_TIMEOUT;
	} else if (asic_status == VP8HWDEC_SYSTEM_ERROR) {
		pr_err("Error: %s VP8HWDEC_SYSTEM_ERROR\n", __func__);
		return VP8DEC_SYSTEM_ERROR;
	} else if (asic_status == VP8HWDEC_HW_RESERVED) {
		pr_err("Error: %s VP8HWDEC_HW_RESERVED\n", __func__);
		return VP8DEC_HW_RESERVED;
	}

	/* Handle possible common HW error situations */
	if (asic_status & DEC_8190_IRQ_BUS) {
		pr_info("Asic Status : 0x%x\n", asic_status);
		pr_err("Error: %s VP8DEC_HW_BUS_ERROR\n", __func__);
		return VP8DEC_HW_BUS_ERROR;
	}
	/* for all the rest we will output a picture (concealed or not) */
	if ((asic_status & DEC_8190_IRQ_TIMEOUT) ||
	    (asic_status & DEC_8190_IRQ_ERROR)) {
		/* This timeout is HW generated */
		if (asic_status & DEC_8190_IRQ_TIMEOUT) {
			pr_err("Error: %s hw timeout\n", __func__);
		} else {
			pr_err("Error: %s stream\n", __func__);
		}

		/* PP has to run again for the concealed picture */
		if (pDecCont->pp.ppInstance != NULL && pDecCont->pp.decPpIf.usePipeline) {
			/* concealed current, i.e. last  ref to PP */
			pr_err("Error: %s Concealed picture, PP should run again\n", __func__);
			pDecCont->pp.decPpIf.ppStatus = DECPP_RUNNING;
		}

		/*SE Comment this line to power on the error concealment */
//        errorConcealment = 1;
	} else if (asic_status & DEC_8190_IRQ_RDY) {
		if (pDecCont->decoder.keyFrame) {
			pDecCont->pictureBroken = 0;
		}

		if (pDecCont->sliceHeight) {
			pDecCont->outputRows = pAsicBuff->height -  pDecCont->totDecodedRows * 16;
			if (pDecCont->totDecodedRows) {
				pDecCont->outputRows += 8;
			}
			return VP8DEC_PIC_DECODED;
		}
	} else if (asic_status & DEC_8190_IRQ_SLICE) {
		pDecCont->decStat = VP8DEC_MIDDLE_OF_PIC;

		pDecCont->outputRows = pDecCont->sliceHeight * 16;
		if (!pDecCont->totDecodedRows) {
			pDecCont->outputRows -= 8;
		}

		pDecCont->totDecodedRows += pDecCont->sliceHeight;

		return VP8DEC_SLICE_RDY;
	} else {
		assert(0);
	}

	pDecCont->picNumber++;
	if (pDecCont->decoder.showFrame) {
		pDecCont->outCount++;
	}
	return VP8DEC_PIC_DECODED;
}


/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicAllocatePictures
    Description     : inform the hardware about the allocated buffer
    Return type     : signed int
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
int VP8HwdAsicAllocatePictures(VP8DecContainer_t *pDecCont)
{
//    const void *dwl = pDecCont->dwl;
//    signed int dwl_ret;
	unsigned int count;
	DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;

	//unsigned int pict_buff_size;

	unsigned int memorySize;
	unsigned long tempPhysAddr = 0;
	unsigned long tempVirtualAddr = 0;

#if 0
	/*segmentation map buffer transfered to VP8HwdAsicAllocateMem
	  (allocate only one time this buffer and use it for all pictures)*/

	unsigned int numMbs;

	/* allocate segment map */

	numMbs = (pAsicBuff->width >> 4) * (pAsicBuff->height >> 4);
	memorySize = (numMbs + 3) >> 2; /* We fit 4 MBs on data into every full byte */
	memorySize = 64 * ((memorySize + 63) >> 6); /* Round up to next multiple of 64 bytes */

	dwl_ret = DWLMallocLinear(dwl, memorySize, &pAsicBuff->segmentMap);

	if (dwl_ret != DWL_OK) {
		VP8HwdAsicReleasePictures(pDecCont);
		return -1;
	}

	pAsicBuff->segmentMapSize = memorySize;
	SetDecRegister(pDecCont->vp8Regs, HWIF_SEGMENT_BASE,
	               pAsicBuff->segmentMap.busAddress);
	memset(pAsicBuff->segmentMap.virtualAddress, 0, pAsicBuff->segmentMapSize);
#endif
	memset(pAsicBuff->pictures, 0, sizeof(pAsicBuff->pictures));

	count = pDecCont->numBuffers;

	/*
	if (!pDecCont->sliceHeight) {
	        pict_buff_size = pAsicBuff->width * pAsicBuff->height * 3 / 2;
	} else
	        pict_buff_size = pAsicBuff->width *
	            (pDecCont->sliceHeight + 1) * 16 * 3 / 2;
	*/

	memset(&pAsicBuff->outBuffer, 0, sizeof(DWLLinearMem_t));
	memset(&pAsicBuff->prevOutBuffer, 0, sizeof(DWLLinearMem_t));
	memset(&pAsicBuff->refBuffer, 0, sizeof(DWLLinearMem_t));
	memset(&pAsicBuff->goldenBuffer, 0, sizeof(DWLLinearMem_t));
	memset(&pAsicBuff->alternateBuffer, 0, sizeof(DWLLinearMem_t));

	/* webp/snapshot -> allocate memory for HW above row storage */
	if (pDecCont->intraOnly) {
		unsigned int tmpW = 0;
		assert(count == 1);

		/* width of the memory in macroblocks, has to be multiple of amount
		 * of mbs stored/load per access */
		/* TODO: */
		while (tmpW < MAX_SNAPSHOT_WIDTH) {
			tmpW += 120;
		}

		/* TODO: check amount of memory per macroblock for each HW block */
		memorySize = tmpW /* num mbs */ *
		             (2 + 2 /* streamd (cbf + intra pred modes) */ +
		              16 + 2 * 8 /* intra pred (luma + cb + cr) */ +
		              16 * 8 + 2 * 8 * 4 /* filterd */);

		tempVirtualAddr = (unsigned int)OSDEV_AllignedMallocHwBuffer(32, memorySize, BPA_PARTITION_NAME, &tempPhysAddr);
		if (!tempVirtualAddr) {
			pr_err("Error: %s Unable to allocate hw buffer for webp/snapshot", __func__);
			return -1;
		}

		pAsicBuff->pictures[1].busAddress = tempPhysAddr;
		pAsicBuff->pictures[1].virtualAddress = (unsigned int *)tempVirtualAddr;
		pAsicBuff->pictures[1].size = memorySize;

		memset(pAsicBuff->pictures[1].virtualAddress, 0, memorySize);
		SetDecRegister(pDecCont->vp8Regs, HWIF_REFER6_BASE,
		               pAsicBuff->pictures[1].busAddress);
		SetDecRegister(pDecCont->vp8Regs, HWIF_REFER2_BASE,
		               pAsicBuff->pictures[1].busAddress + 4 * tmpW);
		SetDecRegister(pDecCont->vp8Regs, HWIF_REFER3_BASE,
		               pAsicBuff->pictures[1].busAddress + 36 * tmpW);
	}

	SetDecRegister(pDecCont->vp8Regs, HWIF_PIC_MB_WIDTH, (pAsicBuff->width / 16) & 0x1FF);
	SetDecRegister(pDecCont->vp8Regs, HWIF_PIC_MB_HEIGHT_P, (pAsicBuff->height / 16) & 0xFF);
	SetDecRegister(pDecCont->vp8Regs, HWIF_PIC_MB_W_EXT, (pAsicBuff->width / 16) >> 9);
	SetDecRegister(pDecCont->vp8Regs, HWIF_PIC_MB_H_EXT, (pAsicBuff->height / 16) >> 8);

	return 0;
}



/*------------------------------------------------------------------------------
    vp8hwdDecodeFrameTag

         copy fields from VP8_ParamPicture_t to decoder instance
------------------------------------------------------------------------------*/
void vp8hwdDecodeFrameTag(VP8_ParamPicture_t *Picture_Data_Struct_p, unsigned char *pStrm, vp8Decoder_t *dec)
{
#if  0
	dec->showFrame          = Picture_Data_Struct_p->ShowFrame;
	dec->vpVersion          = Picture_Data_Struct_p->vpVersion;
	dec->offsetToDctParts   = Picture_Data_Struct_p->offsetToDctParts;
	dec->keyFrame           = Picture_Data_Struct_p->KeyFrame;
	dec->frameTagSize       = 3;
#else
	u32 keyFrame = 0;
	u32 showFrame = 1;
	u32 version = 0;
	u32 partSize = 0;

	partSize = (pStrm[1] << 3) |
	           (pStrm[2] << 11);
	keyFrame = pStrm[0] & 0x1;
	version = (pStrm[0] >> 1) & 0x7;
	if (dec->decMode == VP8HWD_VP7) {
		partSize <<= 1;
		partSize = partSize | ((pStrm[0] >> 4) & 0xF);
		dec->frameTagSize = version >= 1 ? 3 : 4;
	} else {
		showFrame = (pStrm[0] >> 4) & 0x1;
		partSize = partSize | ((pStrm[0] >> 5) & 0x7);
		dec->frameTagSize = 3;
	}

//pr_info("My ShowFrame : %d, VpVersion : %d, offsetToDct : %d, keyFrame : %d\n",showFrame,version,partSize,!keyFrame);
//pr_info("P2 ShowFrame : %d, VpVersion : %d, offsetToDct : %d, keyFrame : %d\n",Picture_Data_Struct_p->ShowFrame,Picture_Data_Struct_p->vpVersion,Picture_Data_Struct_p->offsetToDctParts,Picture_Data_Struct_p->KeyFrame);
	dec->showFrame          = showFrame;
	dec->vpVersion          = version;
	dec->offsetToDctParts   = partSize;
	dec->keyFrame           = !keyFrame;
#endif
}
/*------------------------------------------------------------------------------
    vp8hwdResetDecoder

        Reset decoder to initial state
------------------------------------------------------------------------------*/
void vp8hwdResetDecoder(vp8Decoder_t *dec, DecAsicBuffers_t *asicBuff)
{
	if (asicBuff->segmentMap.virtualAddress) {
		memset(asicBuff->segmentMap.virtualAddress, 0, asicBuff->segmentMapSize);
	}
	vp8hwdResetProbs(dec);
	vp8hwdPrepareVp7Scan(dec, NULL);
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdResetProbs
    Description     : Reset probabilities to default values
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdResetProbs(vp8Decoder_t *dec)
{
	unsigned int i, j, k, l;

	/* Intra-prediction modes */
	dec->entropy.probLuma16x16PredMode[0] = 112;
	dec->entropy.probLuma16x16PredMode[1] = 86;
	dec->entropy.probLuma16x16PredMode[2] = 140;
	dec->entropy.probLuma16x16PredMode[3] = 37;
	dec->entropy.probChromaPredMode[0] = 162;
	dec->entropy.probChromaPredMode[1] = 101;
	dec->entropy.probChromaPredMode[2] = 204;

	/* MV context */
	k = 0;
	if (dec->decMode == VP8HWD_VP8) {
		for (i = 0 ; i < 2 ; ++i)
			for (j = 0 ; j < VP8_MV_PROBS_PER_COMPONENT ; ++j, ++k) {
				dec->entropy.probMvContext[i][j] = Vp8DefaultMvProbs[i][j];
			}
	} else {
		for (i = 0 ; i < 2 ; ++i)
			for (j = 0 ; j < VP7_MV_PROBS_PER_COMPONENT ; ++j, ++k) {
				dec->entropy.probMvContext[i][j] = Vp7DefaultMvProbs[i][j];
			}
	}

	/* Coefficients */
	for (i = 0 ; i < 4 ; ++i)
		for (j = 0 ; j < 8 ; ++j)
			for (k = 0 ; k < 3 ; ++k)
				for (l = 0 ; l < 11 ; ++l)
					dec->entropy.probCoeffs[i][j][k][l] =
					        DefaultCoeffProbs[i][j][k][l];
}

/*------------------------------------------------------------------------------
    ResetScan

        Reset decoder to initial state
------------------------------------------------------------------------------*/
void vp8hwdPrepareVp7Scan(vp8Decoder_t *dec, unsigned int *newOrder)
{
	unsigned int i;
	static const unsigned int Vp7DefaultScan[] = {
		0,  1,  4,  8,  5,  2,  3,  6,
		9, 12, 13, 10,  7, 11, 14, 15,
	};

	if (!newOrder) {
		for (i = 0 ; i < 16 ; ++i) {
			dec->vp7ScanOrder[i] = Vp7DefaultScan[i];
		}
	} else {
		for (i = 0 ; i < 16 ; ++i) {
			dec->vp7ScanOrder[i] = Vp7DefaultScan[newOrder[i]];
		}
	}
}

unsigned int vp8hwdDecodeFrameHeader(VP8_ParamPicture_t *Picture_Data_Struct_p,
                                     unsigned char *pStrm, unsigned int strmLen, vpBoolCoder_t *bc, vp8Decoder_t *dec)
{
	unsigned int tmp;
	unsigned int i;

	if (dec->keyFrame) {
		/* Frame width */
		dec->width          = Picture_Data_Struct_p->decode_width;
		dec->scaledWidth    = ScaleDimension(dec->width, Picture_Data_Struct_p->horizontal_scale);
		dec->height         = Picture_Data_Struct_p->decode_height;
		dec->scaledHeight   = ScaleDimension(dec->height, Picture_Data_Struct_p->vertical_scale);
		pStrm += 7; /* Skip used bytes before initializing BoolDecoder*/
		strmLen -= 7;
	}

	/* Start bool code */
	vp8hwdBoolStart(bc, pStrm, strmLen);

	if (dec->keyFrame) {
#if 0
		/* Color space type */
		dec->colorSpace = (vpColorSpace_e)Picture_Data_Struct_p->colour_space;
		/* Clamping type */
		dec->clamping = Picture_Data_Struct_p->clamping;
#endif

		/* Color space type */
		dec->colorSpace = (vpColorSpace_e)vp8hwdReadBits(bc, 1);
		STREAM_TRACE("vp8_color_space", dec->colorSpace);

//    pr_info("My ColorSpace : %d\n",dec->colorSpace);
//    pr_info("ColorSpace : %d\n",Picture_Data_Struct_p->colour_space);

		/* Clamping type */
		dec->clamping = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("vp8_clamping", dec->clamping);
//    pr_info("My clamping : %d\n",dec->clamping);
//    pr_info("ColorSpace : %d\n",Picture_Data_Struct_p->clamping);

	}

	/* Segment based adjustments */
	tmp = DecodeSegmentationData(bc, dec);
	if (tmp != HANTRO_OK) {
		return tmp;
	}

	/* Loop filter adjustments */
#if 0
	dec->loopFilterType      =  Picture_Data_Struct_p->filter_type;
	dec->loopFilterLevel     = Picture_Data_Struct_p->loop_filter_level;
	dec->loopFilterSharpness = Picture_Data_Struct_p->sharpness_level;
#else
	dec->loopFilterType      = vp8hwdReadBits(bc, 1);
	dec->loopFilterLevel     = vp8hwdReadBits(bc, 6);
	dec->loopFilterSharpness = vp8hwdReadBits(bc, 3);
#endif
//    pr_info("My loopFilter : %d, %d, %d\n",dec->loopFilterType,dec->loopFilterLevel,dec->loopFilterSharpness);
//    pr_info("loopFilter : %d, %d, %d\n",Picture_Data_Struct_p->filter_type,Picture_Data_Struct_p->loop_filter_level,Picture_Data_Struct_p->sharpness_level);

	tmp = DecodeMbLfAdjustments(bc, dec);
	if (tmp != HANTRO_OK) {
		return tmp;
	}

	/* Number of DCT partitions */
	tmp = vp8hwdReadBits(bc, 2);
	STREAM_TRACE("nbr_of_dct_partitions", tmp);
	dec->nbrDctPartitions = 1 << tmp;

	/* Quantizers */
	dec->qpYAc = vp8hwdReadBits(bc, 7);
	STREAM_TRACE("qp_y_ac", dec->qpYAc);
	dec->qpYDc = DecodeQuantizerDelta(bc);
	dec->qpY2Dc = DecodeQuantizerDelta(bc);
	dec->qpY2Ac = DecodeQuantizerDelta(bc);
	dec->qpChDc = DecodeQuantizerDelta(bc);
	dec->qpChAc = DecodeQuantizerDelta(bc);
	STREAM_TRACE("qp_y_dc", dec->qpYDc);
	STREAM_TRACE("qp_y2_dc", dec->qpY2Dc);
	STREAM_TRACE("qp_y2_ac", dec->qpY2Ac);
	STREAM_TRACE("qp_ch_dc", dec->qpChDc);
	STREAM_TRACE("qp_ch_ac", dec->qpChAc);

	/* Frame buffer operations */
	if (!dec->keyFrame) {
		/* Refresh golden */
		dec->refreshGolden = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("refresh_golden", dec->refreshGolden);

		/* Refresh alternate */
		dec->refreshAlternate = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("refresh_alternate", dec->refreshAlternate);

		if (dec->refreshGolden == 0) {
			/* Copy to golden */
			dec->copyBufferToGolden = vp8hwdReadBits(bc, 2);
			STREAM_TRACE("copy_buffer_to_golden", dec->copyBufferToGolden);
		} else {
			dec->copyBufferToGolden = 0;
		}

		if (dec->refreshAlternate == 0) {
			/* Copy to alternate */
			dec->copyBufferToAlternate = vp8hwdReadBits(bc, 2);
			STREAM_TRACE("copy_buffer_to_alternate", dec->copyBufferToAlternate);
		} else {
			dec->copyBufferToAlternate = 0;
		}

		/* Sign bias for golden frame */
		dec->refFrameSignBias[0] = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("sign_bias_golden", dec->refFrameSignBias[0]);

		/* Sign bias for alternate frame */
		dec->refFrameSignBias[1] = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("sign_bias_alternate", dec->refFrameSignBias[1]);

		/* Refresh entropy probs */
		dec->refreshEntropyProbs = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("refresh_entropy_probs", dec->refreshEntropyProbs);

		/* Refresh last */
		dec->refreshLast = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("refresh_last", dec->refreshLast);
	} else { /* Key frame */
		dec->refreshGolden          = HANTRO_TRUE;
		dec->refreshAlternate       = HANTRO_TRUE;
		dec->copyBufferToGolden     = 0;
		dec->copyBufferToAlternate  = 0;

		/* Refresh entropy probs */
		dec->refreshEntropyProbs = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("refresh_entropy_probs", dec->refreshEntropyProbs);

		dec->refFrameSignBias[0] =
		        dec->refFrameSignBias[1] = 0;
		dec->refreshLast = HANTRO_TRUE;
	}

	/* Make a "backup" of current entropy probabilities if refresh is
	 * not set */
	if (dec->refreshEntropyProbs == HANTRO_FALSE) {
		memcpy(&dec->entropyLast, &dec->entropy,
		       sizeof(vp8EntropyProbs_t));
		memcpy(dec->vp7PrevScanOrder, dec->vp7ScanOrder,
		       sizeof(dec->vp7ScanOrder));
	}

	/* Coefficient probability update */
	tmp = vp8hwdDecodeCoeffUpdate(bc, dec);
	if (tmp != HANTRO_OK) {
		return (tmp);
	}

	/* Coeff skip element used */
	tmp = vp8hwdReadBits(bc, 1);
	STREAM_TRACE("no_coeff_skip", tmp);
	dec->coeffSkipMode = tmp;

	if (!dec->keyFrame) {
		/* Skipped MB probability */
		tmp = vp8hwdReadBits(bc, 8);
		STREAM_TRACE("prob_skip_mb", tmp);
		dec->probMbSkipFalse = tmp;

		/* Intra MB probability */
		tmp = vp8hwdReadBits(bc, 8);
		STREAM_TRACE("prob_intra_mb", tmp);
		dec->probIntra = tmp;

		/* Last ref frame probability */
		tmp = vp8hwdReadBits(bc, 8);
		STREAM_TRACE("prob_ref_frame_0", tmp);
		dec->probRefLast = tmp;

		/* Golden ref frame probability */
		tmp = vp8hwdReadBits(bc, 8);
		STREAM_TRACE("prob_ref_frame_1", tmp);
		dec->probRefGolden = tmp;

		/* Intra 16x16 pred mode probabilities */
		tmp = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("intra_16x16_prob_update_flag", tmp);
		if (tmp) {
			for (i = 0 ; i < 4 ; ++i) {
				tmp = vp8hwdReadBits(bc, 8);
				STREAM_TRACE("intra_16x16_prob", tmp);
				dec->entropy.probLuma16x16PredMode[i] = tmp;
			}
		}

		/* Chroma pred mode probabilities */
		tmp = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("chroma_prob_update_flag", tmp);
		if (tmp) {
			for (i = 0 ; i < 3 ; ++i) {
				tmp = vp8hwdReadBits(bc, 8);
				STREAM_TRACE("chroma_prob", tmp);
				dec->entropy.probChromaPredMode[i] = tmp;
			}
		}

		/* Motion vector tree update */
		tmp = vp8hwdDecodeMvUpdate(bc, dec);
		if (tmp != HANTRO_OK) {
			return (tmp);
		}
	} else {
		/* Skipped MB probability */
		tmp = vp8hwdReadBits(bc, 8);
		STREAM_TRACE("prob_skip_mb", tmp);
		dec->probMbSkipFalse = tmp;
	}

	if (bc->strmError) {
		return (HANTRO_NOK);
	}

	return (HANTRO_OK);

}

/*------------------------------------------------------------------------------
    ScaleDimension

        Scale frame dimension
------------------------------------------------------------------------------*/
unsigned int ScaleDimension(unsigned int orig, unsigned int scale)
{
	switch (scale) {
	case 0:
		return orig;
	case 1: /* 5/4 */
		return (5 * orig) / 4;
	case 2: /* 5/3 */
		return (5 * orig) / 3;
	case 3: /* 2 */
		return 2 * orig;
	default:
		assert(0);
	}
	return 0;//  assert(0);
}

void vp8hwdBoolStart(vpBoolCoder_t *br, unsigned char *source, unsigned int len)
{

	br->lowvalue = 0;
	br->range = 255;
	br->count = 8;
	br->buffer = source;
	br->pos = 0;

	br->value =
	        (br->buffer[0] << 24) + (br->buffer[1] << 16) + (br->buffer[2] << 8) +
	        (br->buffer[3]);

	br->pos += 4;

	br->streamEndPos = len;
	br->strmError = br->pos > br->streamEndPos;
}


/*------------------------------------------------------------------------------
    DecodeSegmentationData

        Decode segment-based adjustments from bitstream.
------------------------------------------------------------------------------*/
unsigned int DecodeSegmentationData(vpBoolCoder_t *bc, vp8Decoder_t *dec)
{
	unsigned int tmp;
	unsigned int sign;
	unsigned int j;

	/* Segmentation enabled? */
	dec->segmentationEnabled = vp8hwdReadBits(bc, 1);
	STREAM_TRACE("segmentation_enabled", dec->segmentationEnabled);
	if (dec->segmentationEnabled) {
		/* Segmentation map update */
		dec->segmentationMapUpdate = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("segmentation_map_update", dec->segmentationMapUpdate);
		/* Segment feature data update */
		tmp = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("segment_feature_data_update", tmp);
		if (tmp) {
			/* Absolute/relative mode */
			dec->segmentFeatureMode = vp8hwdReadBits(bc, 1);
			STREAM_TRACE("segment_feature_mode", dec->segmentFeatureMode);

			/* TODO: what to do with negative numbers if absolute mode? */
			/* Quantiser */
			for (j = 0 ; j < MAX_NBR_OF_SEGMENTS ; ++j) {
				/* Feature data update ? */
				tmp = vp8hwdReadBits(bc, 1);
				STREAM_TRACE("quantizer_update_flag", tmp);
				if (tmp) {
					/* Payload */
					tmp = vp8hwdReadBits(bc, 7);
					/* Sign */
					sign = vp8hwdReadBits(bc, 1);
					STREAM_TRACE("quantizer_payload", tmp);
					STREAM_TRACE("quantizer_sign", sign);
					dec->segmentQp[j] = tmp;
					if (sign) {
						dec->segmentQp[j] = -dec->segmentQp[j];
					}
				}
			}

			/* Loop filter level */
			for (j = 0 ; j < MAX_NBR_OF_SEGMENTS ; ++j) {
				/* Feature data update ? */
				tmp = vp8hwdReadBits(bc, 1);
				STREAM_TRACE("loop_filter_update_flag", tmp);
				if (tmp) {
					/* Payload */
					tmp = vp8hwdReadBits(bc, 6);
					/* Sign */
					sign = vp8hwdReadBits(bc, 1);
					STREAM_TRACE("loop_filter_payload", tmp);
					STREAM_TRACE("loop_filter_sign", sign);
					dec->segmentLoopfilter[j] = tmp;
					if (sign) {
						dec->segmentLoopfilter[j] = -dec->segmentLoopfilter[j];
					}
				}
			}

		}

		/* Segment probabilities */
		if (dec->segmentationMapUpdate) {
			dec->probSegment[0] = 255;
			dec->probSegment[1] = 255;
			for (j = 0 ; j < 3 ; ++j) {
				tmp = vp8hwdReadBits(bc, 1);
				STREAM_TRACE("segment_prob_update_flag", tmp);
				if (tmp) {
					tmp = vp8hwdReadBits(bc, 8);
					STREAM_TRACE("segment_prob", tmp);
					dec->probSegment[j] = tmp;
				}
			}
		}
	} /* SegmentationEnabled */

	if (bc->strmError) {
		return (HANTRO_NOK);
	}

	return (HANTRO_OK);
}



/*------------------------------------------------------------------------------
    DecodeMbLfAdjustments

        Decode MB loop filter adjustments from bitstream.
------------------------------------------------------------------------------*/
unsigned int DecodeMbLfAdjustments(vpBoolCoder_t *bc, vp8Decoder_t *dec)
{
	unsigned int sign;
	unsigned int tmp;
	unsigned int j;

	/* Adjustments enabled? */
	dec->modeRefLfEnabled = vp8hwdReadBits(bc, 1);
	STREAM_TRACE("loop_filter_adj_enable", dec->modeRefLfEnabled);

	if (dec->modeRefLfEnabled) {
		/* Mode update? */
		tmp = vp8hwdReadBits(bc, 1);
		STREAM_TRACE("loop_filter_adj_update_flag", tmp);
		if (tmp) {
			/* Reference frame deltas */
			for (j = 0; j < MAX_NBR_OF_MB_REF_LF_DELTAS; j++) {
				tmp = vp8hwdReadBits(bc, 1);
				STREAM_TRACE("ref_frame_delta_update_flag", tmp);
				if (tmp) {
					/* Payload */
					tmp = vp8hwdReadBits(bc, 6);
					/* Sign */
					sign = vp8hwdReadBits(bc, 1);
					STREAM_TRACE("loop_filter_payload", tmp);
					STREAM_TRACE("loop_filter_sign", sign);

					dec->mbRefLfDelta[j] = tmp;
					if (sign) {
						dec->mbRefLfDelta[j] = -dec->mbRefLfDelta[j];
					}
				}
			}

			/* Mode deltas */
			for (j = 0; j < MAX_NBR_OF_MB_MODE_LF_DELTAS; j++) {
				tmp = vp8hwdReadBits(bc, 1);
				STREAM_TRACE("mb_type_delta_update_flag", tmp);
				if (tmp) {
					/* Payload */
					tmp = vp8hwdReadBits(bc, 6);
					/* Sign */
					sign = vp8hwdReadBits(bc, 1);
					STREAM_TRACE("loop_filter_payload", tmp);
					STREAM_TRACE("loop_filter_sign", sign);

					dec->mbModeLfDelta[j] = tmp;
					if (sign) {
						dec->mbModeLfDelta[j] = -dec->mbModeLfDelta[j];
					}
				}
			}
		}
	} /* Mb mode/ref lf adjustment */

	if (bc->strmError) {
		return (HANTRO_NOK);
	}

	return (HANTRO_OK);
}


/*------------------------------------------------------------------------------
    DecodeQuantizerDelta

        Decode VP8 delta-coded quantizer value
------------------------------------------------------------------------------*/
signed int DecodeQuantizerDelta(vpBoolCoder_t *bc)
{
	unsigned int sign;
	signed int delta;

	if (vp8hwdReadBits(bc, 1)) {
		delta = vp8hwdReadBits(bc, 4);
		sign = vp8hwdReadBits(bc, 1);
		if (sign) {
			delta = -delta;
		}
		return delta;
	} else {
		return 0;
	}
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdDecodeCoeffUpdate
    Description     : Decode coeff probability update from stream
    Return type     : OK/NOK
    Argument        :
------------------------------------------------------------------------------*/
unsigned int  vp8hwdDecodeCoeffUpdate(vpBoolCoder_t *bc, vp8Decoder_t *dec)
{
	unsigned int i, j, k, l;
	unsigned int tmp;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 3; k++) {
				for (l = 0; l < 11; l++) {
					tmp = vp8hwdDecodeBool(bc, CoeffUpdateProbs[i][j][k][l]);
					CHECK_END_OF_STREAM(tmp);
					if (tmp) {
						tmp = vp8hwdReadBits(bc, 8);
						CHECK_END_OF_STREAM(tmp);
						dec->entropy.probCoeffs[i][j][k][l] = tmp;
					}
				}
			}
		}
	}
	return (HANTRO_OK);
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdDecodeMvUpdate
    Description     : Decode mv probability update from stream
    Return type     : OK/NOK
    Argument        :
------------------------------------------------------------------------------*/
unsigned int  vp8hwdDecodeMvUpdate(vpBoolCoder_t *bc, vp8Decoder_t *dec)
{
	unsigned int i, j;
	unsigned int tmp;
	unsigned int mvProbs;

	if (dec->decMode == VP8HWD_VP8) { mvProbs = VP8_MV_PROBS_PER_COMPONENT; }
	else { mvProbs = VP7_MV_PROBS_PER_COMPONENT; }

	for (i = 0 ; i < 2 ; ++i) {
		for (j = 0 ; j < mvProbs ; ++j) {
			tmp = vp8hwdDecodeBool(bc, MvUpdateProbs[i][j]);
			CHECK_END_OF_STREAM(tmp);
			if (tmp == 1) {
				tmp = vp8hwdReadBits(bc, 7);
				CHECK_END_OF_STREAM(tmp);
				if (tmp) { tmp = tmp << 1; }
				else { tmp = 1; }
				dec->entropy.probMvContext[i][j] = tmp;
			}
		}
	}
	return (HANTRO_OK);
}

void vp8hwdFreeze(VP8DecContainer_t *pDecCont)
{

	/* Skip */
	pDecCont->picNumber++;
	pDecCont->refToOut = 1;
	if (pDecCont->decoder.showFrame) {
		pDecCont->outCount++;
	}

	if (pDecCont->pp.ppInstance != NULL) {
		/* last ref to PP */
		pDecCont->pp.decPpIf.usePipeline = 0;
		pDecCont->pp.decPpIf.ppStatus = DECPP_RUNNING;
	}

}


/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicProbUpdate
    Description     :
    Return type     : void
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicProbUpdate(VP8DecContainer_t *pDecCont)
{
	unsigned char *dst;
//    const unsigned char *src;
	unsigned int i, j, k;
//    signed int r;
	unsigned int *asicProbBase = pDecCont->asicBuff->probTbl.virtualAddress;
	unsigned int size_ProbTable, mot;
	unsigned int *tmp_p;


	/* first probs */
	dst = ((unsigned char *) asicProbBase);

	dst[3] = pDecCont->decoder.probMbSkipFalse;
	dst[2] = pDecCont->decoder.probIntra;
	dst[1] = pDecCont->decoder.probRefLast;
	dst[0] = pDecCont->decoder.probRefGolden;
	dst[7] = pDecCont->decoder.probSegment[0];
	dst[6] = pDecCont->decoder.probSegment[1];
	dst[5] = pDecCont->decoder.probSegment[2];
	dst[4] = 0; /*unused*/

	dst += 8;
	dst[3] = pDecCont->decoder.entropy.probLuma16x16PredMode[0];
	dst[2] = pDecCont->decoder.entropy.probLuma16x16PredMode[1];
	dst[1] = pDecCont->decoder.entropy.probLuma16x16PredMode[2];
	dst[0] = pDecCont->decoder.entropy.probLuma16x16PredMode[3];
	dst[7] = pDecCont->decoder.entropy.probChromaPredMode[0];
	dst[6] = pDecCont->decoder.entropy.probChromaPredMode[1];
	dst[5] = pDecCont->decoder.entropy.probChromaPredMode[2];
	dst[4] = 0; /*unused*/

	/* mv probs */
	dst += 8;
	dst[3] = pDecCont->decoder.entropy.probMvContext[0][0]; /* is short */
	dst[2] = pDecCont->decoder.entropy.probMvContext[1][0];
	dst[1] = pDecCont->decoder.entropy.probMvContext[0][1]; /* sign */
	dst[0] = pDecCont->decoder.entropy.probMvContext[1][1];
	dst[7] = pDecCont->decoder.entropy.probMvContext[0][8 + 9];
	dst[6] = pDecCont->decoder.entropy.probMvContext[0][9 + 9];
	dst[5] = pDecCont->decoder.entropy.probMvContext[1][8 + 9];
	dst[4] = pDecCont->decoder.entropy.probMvContext[1][9 + 9];
	dst += 8;
	for (i = 0 ; i < 2 ; ++i) {
		for (j = 0 ; j < 8 ; j += 4) {
			dst[3] = pDecCont->decoder.entropy.probMvContext[i][j + 9 + 0];
			dst[2] = pDecCont->decoder.entropy.probMvContext[i][j + 9 + 1];
			dst[1] = pDecCont->decoder.entropy.probMvContext[i][j + 9 + 2];
			dst[0] = pDecCont->decoder.entropy.probMvContext[i][j + 9 + 3];
			dst += 4;
		}
	}
	for (i = 0 ; i < 2 ; ++i) {
		dst[3] = pDecCont->decoder.entropy.probMvContext[i][0 + 2];
		dst[2] = pDecCont->decoder.entropy.probMvContext[i][1 + 2];
		dst[1] = pDecCont->decoder.entropy.probMvContext[i][2 + 2];
		dst[0] = pDecCont->decoder.entropy.probMvContext[i][3 + 2];
		dst[7] = pDecCont->decoder.entropy.probMvContext[i][4 + 2];
		dst[6] = pDecCont->decoder.entropy.probMvContext[i][5 + 2];
		dst[5] = pDecCont->decoder.entropy.probMvContext[i][6 + 2];
		dst[4] = 0; /*unused*/
		dst += 8;
	}

	/* coeff probs (header part) */
	dst = ((char *) asicProbBase) + 8 * 7;
	for (i = 0 ; i < 4 ; ++i) {
		for (j = 0 ; j < 8 ; ++j) {
			for (k = 0 ; k < 3 ; ++k) {
				dst[3] = pDecCont->decoder.entropy.probCoeffs[i][j][k][0];
				dst[2] = pDecCont->decoder.entropy.probCoeffs[i][j][k][1];
				dst[1] = pDecCont->decoder.entropy.probCoeffs[i][j][k][2];
				dst[0] = pDecCont->decoder.entropy.probCoeffs[i][j][k][3];
				dst += 4;
			}
		}
	}

	/* coeff probs (footer part) */
	dst = ((unsigned char *) asicProbBase) + 8 * 55;
	for (i = 0 ; i < 4 ; ++i) {
		for (j = 0 ; j < 8 ; ++j) {
			for (k = 0 ; k < 3 ; ++k) {
				dst[3] = pDecCont->decoder.entropy.probCoeffs[i][j][k][4];
				dst[2] = pDecCont->decoder.entropy.probCoeffs[i][j][k][5];
				dst[1] = pDecCont->decoder.entropy.probCoeffs[i][j][k][6];
				dst[0] = pDecCont->decoder.entropy.probCoeffs[i][j][k][7];
				dst[7] = pDecCont->decoder.entropy.probCoeffs[i][j][k][8];
				dst[6] = pDecCont->decoder.entropy.probCoeffs[i][j][k][9];
				dst[5] = pDecCont->decoder.entropy.probCoeffs[i][j][k][10];
				dst[4] = 0; /*unused*/
				dst += 8;
			}
		}
	}
	/*DWLSaveMemory*/
	size_ProbTable = ((unsigned int) dst - (unsigned int)asicProbBase); /*0x4b8*/
	tmp_p = (unsigned int *) asicProbBase;/*back to the first byte of the ProbTable*/

#if 1
	/*In order to fix Endianess bug*/
	tmp_p --;
	for (i = 0; i < size_ProbTable / 4; i++) {
		tmp_p ++;
		mot = *tmp_p;
		mot = (((mot & 0x000000FF) << 24) |
		       ((mot & 0x0000FF00) << 8) |
		       ((mot & 0x00FF0000) >>  8) |
		       (mot & 0xFF000000) >> 24);
		*tmp_p = mot;
	}
#endif
#if defined(DEBUG_VIDEO)
	{
		char *My_prob = (unsigned char *) asicProbBase;
		pr_info("AsicProbBase :: Qtable virt @ : 0x%x", asicProbBase);
		struct file *filp;
		filp = filp_open("Qtable.bin", O_CREAT | O_RDWR, 0644);
		if (IS_ERR(filp)) {
			pr_alert("Oops ! filp_open failed\n");
		} else {
			mm_segment_t old_fs = get_fs();
			set_fs(get_ds());
			filp->f_op->write(filp, (char *) My_prob, size_ProbTable, &filp->f_pos);

			set_fs(old_fs);
			filp_close(filp, NULL);
			pr_alert("File Operations END\n");
		}
	}
#endif /*defined(DEBUG_VIDEO)*/
}


/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicInitPicture
    Description     :
    Return type     : void
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicInitPicture(VP8DecContainer_t *pDecCont)
{

	vp8Decoder_t *dec = &pDecCont->decoder;
	DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;

#ifdef SET_EMPTY_PICTURE_DATA   /* USE THIS ONLY FOR DEBUGGING PURPOSES */
	{
		signed int bgd = SET_EMPTY_PICTURE_DATA;

		memset(pAsicBuff->outBuffer.virtualAddress, bgd, (dec->width * dec->height * 3) / 2);
	}
#endif

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_DIS, 0);

	if (pDecCont->intraOnly && pDecCont->pp.ppInfo.pipelineAccepted) {
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_DIS, 1);
	} else if (!pDecCont->userMem && !pDecCont->sliceHeight) {
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_BASE,
		               pAsicBuff->outBuffer.busAddress);

		if (!dec->keyFrame)
			/* previous picture */
			SetDecRegister(pDecCont->vp8Regs, HWIF_REFER0_BASE,
			               pAsicBuff->refBuffer.busAddress);
		else { /* chroma output base address */
			SetDecRegister(pDecCont->vp8Regs, HWIF_REFER0_BASE,
			               pAsicBuff->outBuffer.busAddress +
			               pAsicBuff->width * pAsicBuff->height);
		}
	} else {
		unsigned int sliceHeight;

		if (pDecCont->sliceHeight * 16 > pAsicBuff->height) {
			sliceHeight = pAsicBuff->height / 16;
		} else {
			sliceHeight = pDecCont->sliceHeight;
		}

		SetDecRegister(pDecCont->vp8Regs, HWIF_JPEG_SLICE_H, sliceHeight);

		vp8hwdUpdateOutBase(pDecCont);
	}

	/* golden reference */
	SetDecRegister(pDecCont->vp8Regs, HWIF_REFER4_BASE,
	               pAsicBuff->goldenBuffer.busAddress);
	SetDecRegister(pDecCont->vp8Regs, HWIF_GREF_SIGN_BIAS,
	               dec->refFrameSignBias[0]);

	/* alternate reference */
	SetDecRegister(pDecCont->vp8Regs, HWIF_REFER5_BASE,
	               pAsicBuff->alternateBuffer.busAddress);
	SetDecRegister(pDecCont->vp8Regs, HWIF_AREF_SIGN_BIAS,
	               dec->refFrameSignBias[1]);

	SetDecRegister(pDecCont->vp8Regs, HWIF_PIC_INTER_E, !dec->keyFrame);


	/* mb skip mode [Codec X] */
	SetDecRegister(pDecCont->vp8Regs, HWIF_SKIP_MODE, !dec->coeffSkipMode);

	/* loop filter */
	SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_TYPE,
	               dec->loopFilterType);
	SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_SHARPNESS,
	               dec->loopFilterSharpness);

	if (!dec->segmentationEnabled)
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_0,
		               dec->loopFilterLevel);
	else if (dec->segmentFeatureMode) { /* absolute mode */
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_0,
		               dec->segmentLoopfilter[0]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_1,
		               dec->segmentLoopfilter[1]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_2,
		               dec->segmentLoopfilter[2]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_3,
		               dec->segmentLoopfilter[3]);
	} else { /* delta mode */
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_0,
		               CLIP3(0, 63, (int)(dec->loopFilterLevel + dec->segmentLoopfilter[0])));
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_1,
		               CLIP3(0, 63, (int)(dec->loopFilterLevel + dec->segmentLoopfilter[1])));
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_2,
		               CLIP3(0, 63, (int)(dec->loopFilterLevel + dec->segmentLoopfilter[2])));
		SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_LEVEL_3,
		               CLIP3(0, 63, (int)(dec->loopFilterLevel + dec->segmentLoopfilter[3])));
	}

	SetDecRegister(pDecCont->vp8Regs, HWIF_SEGMENT_E, dec->segmentationEnabled);
	SetDecRegister(pDecCont->vp8Regs, HWIF_SEGMENT_UPD_E, dec->segmentationMapUpdate);

	/* TODO: seems that ref dec does not disable filtering based on version,
	 * check */
	/*SetDecRegister(pDecCont->vp8Regs, HWIF_FILTERING_DIS, dec->vpVersion >= 2);*/
	SetDecRegister(pDecCont->vp8Regs, HWIF_FILTERING_DIS,
	               dec->loopFilterLevel == 0);

	/* full pell chroma mvs for VP8 version 3 */
	SetDecRegister(pDecCont->vp8Regs, HWIF_CH_MV_RES,
	               dec->decMode == VP8HWD_VP7 || dec->vpVersion != 3);

	SetDecRegister(pDecCont->vp8Regs, HWIF_BILIN_MC_E,
	               dec->decMode == VP8HWD_VP8 && (dec->vpVersion & 0x3));

	/* first bool decoder status */
	SetDecRegister(pDecCont->vp8Regs, HWIF_BOOLEAN_VALUE,
	               (pDecCont->bc.value >> 24) & (0xFFU));

	SetDecRegister(pDecCont->vp8Regs, HWIF_BOOLEAN_RANGE,
	               pDecCont->bc.range & (0xFFU));

	/* QP */
	if (pDecCont->decMode == VP8HWD_VP7) {
		/* LUT */
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_0, YDcQLookup[dec->qpYDc]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_1, YAcQLookup[dec->qpYAc]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_2, Y2DcQLookup[dec->qpY2Dc]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_3, Y2AcQLookup[dec->qpY2Ac]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_4, UvDcQLookup[dec->qpChDc]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_5, UvAcQLookup[dec->qpChAc]);
	} else {
		if (!dec->segmentationEnabled) {
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_0, dec->qpYAc);
		} else if (dec->segmentFeatureMode) { /* absolute mode */
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_0, dec->segmentQp[0]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_1, dec->segmentQp[1]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_2, dec->segmentQp[2]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_3, dec->segmentQp[3]);
		} else { /* delta mode */
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_0,
			               CLIP3(0, 127, dec->qpYAc + dec->segmentQp[0]));
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_1,
			               CLIP3(0, 127, dec->qpYAc + dec->segmentQp[1]));
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_2,
			               CLIP3(0, 127, dec->qpYAc + dec->segmentQp[2]));
			SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_3,
			               CLIP3(0, 127, dec->qpYAc + dec->segmentQp[3]));
		}
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_DELTA_0, dec->qpYDc);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_DELTA_1, dec->qpY2Dc);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_DELTA_2, dec->qpY2Ac);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_DELTA_3, dec->qpChDc);
		SetDecRegister(pDecCont->vp8Regs, HWIF_QUANT_DELTA_4, dec->qpChAc);

		if (dec->modeRefLfEnabled) {
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_0,  dec->mbRefLfDelta[0]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_1,  dec->mbRefLfDelta[1]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_2,  dec->mbRefLfDelta[2]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_3,  dec->mbRefLfDelta[3]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_0,  dec->mbModeLfDelta[0]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_1,  dec->mbModeLfDelta[1]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_2,  dec->mbModeLfDelta[2]);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_3,  dec->mbModeLfDelta[3]);
		} else {
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_0,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_1,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_2,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_REF_ADJ_3,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_0,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_1,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_2,  0);
			SetDecRegister(pDecCont->vp8Regs, HWIF_FILT_MB_ADJ_3,  0);
		}
	}

	/* scan order */
	if (pDecCont->decMode == VP8HWD_VP7) {
		signed int i;

		for (i = 1; i < 16; i++) {
			SetDecRegister(pDecCont->vp8Regs, ScanTblRegId[i],
			               pDecCont->decoder.vp7ScanOrder[i]);
		}
	}

	/* prediction filter taps */
	/* normal 6-tap filters */
	if ((dec->vpVersion & 0x3) == 0 || dec->decMode == VP8HWD_VP7) {
		signed int i, j;

		for (i = 0; i < 8; i++) {
			for (j = 0; j < 4; j++) {
				SetDecRegister(pDecCont->vp8Regs, TapRegId[i][j],
				               mcFilter[i][j + 1]);
			}
			if (i == 2) {
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_2_M1,
				               mcFilter[i][0]);
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_2_4,
				               mcFilter[i][5]);
			} else if (i == 4) {
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_4_M1,
				               mcFilter[i][0]);
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_4_4,
				               mcFilter[i][5]);
			} else if (i == 6) {
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_6_M1,
				               mcFilter[i][0]);
				SetDecRegister(pDecCont->vp8Regs, HWIF_PRED_TAP_6_4,
				               mcFilter[i][5]);
			}
		}
	}
	/* TODO: tarviiko tapit bilinear caselle? */

	if (dec->decMode == VP8HWD_VP7) {
		SetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_COMP0,
		               pAsicBuff->dcPred[0]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_COMP1,
		               pAsicBuff->dcPred[1]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_MATCH0,
		               pAsicBuff->dcMatch[0]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_MATCH1,
		               pAsicBuff->dcMatch[1]);
		SetDecRegister(pDecCont->vp8Regs, HWIF_VP7_VERSION,
		               dec->vpVersion != 0);
	}

	/* Setup reference picture buffer */
	if (pDecCont->refBufSupport && !pDecCont->intraOnly) {
		unsigned int cntLast = 0,
		             cntGolden = 0,
		             cntAlt = 0;
		unsigned int cntBest;
		unsigned int mul = 0;
		unsigned int allMbs = 0;
		unsigned int bufPicId = 0;
		unsigned int forceDisable = 0;
		unsigned int tmp;
		unsigned int cov;
		unsigned int intraMbs;
		unsigned int flags = 0;
		unsigned int threshold;

		if (!dec->keyFrame) {
			/* Calculate most probable reference frame */

#define CALCULATE_SHARE(range, prob) \
    (((range)*(prob-1))/254)

			allMbs = mul = dec->width * dec->height >> 8;        /* All mbs in frame */

			/* Deduct intra MBs */
			intraMbs = CALCULATE_SHARE(mul, dec->probIntra);
			mul = allMbs - intraMbs;

			cntLast = CALCULATE_SHARE(mul, dec->probRefLast);
			if (pDecCont->decMode == VP8HWD_VP8) {
				mul -= cntLast;     /* What's left is mbs either Golden or Alt */
				cntGolden = CALCULATE_SHARE(mul, dec->probRefGolden);
				cntAlt = mul - cntGolden;
			} else {
				cntGolden = mul - cntLast; /* VP7 has no Alt ref frame */
				cntAlt = 0;
			}

#undef CALCULATE_SHARE

			/* Select best reference frame */

			if (cntLast > cntGolden) {
				tmp = (cntLast > cntAlt);
				bufPicId = tmp ? 0 : 5;
				cntBest  = tmp ? cntLast : cntAlt;
			} else {
				tmp = (cntGolden > cntAlt);
				bufPicId = tmp ? 4 : 5;
				cntBest  = tmp ? cntGolden : cntAlt;
			}

			/* Check against refbuf-calculated threshold value; if  it
			 * seems we'll miss our target, then don't bother enabling
			 * the feature at all... */
			threshold = RefbuGetHitThreshold(&pDecCont->refBufferCtrl);
			threshold *= (dec->height / 16);
			threshold /= 4;
			if (cntBest < threshold) {
				forceDisable = 1;
			}

			/* Next frame has enough reference picture hits, now also take
			 * actual hits and intra blocks into calculations... */
			if (!forceDisable) {
				cov = RefbuVpxGetPrevFrameStats(&pDecCont->refBufferCtrl);

				/* If we get prediction for coverage, we can disable checkpoint
				 * and do calculations here */
				if (cov > 0) {
					/* Calculate percentage of hits from last frame, multiply
					 * predicted reference frame referrals by it and compare.
					 * Note: if refbuffer was off for previous frame, we might
					 *    not get an accurate estimation... */

					tmp = dec->refbuPredHits;
					if (tmp) { cov = (256 * cov) / tmp; }
					else { cov = 0; }
					cov = (cov * cntBest) >> 8;

					if (cov < threshold) {
						forceDisable = 1;
					} else {
						flags = REFBU_DISABLE_CHECKPOINT;
					}
				}
			}
			dec->refbuPredHits = cntBest;
		} else {
			dec->refbuPredHits = 0;
		}

		RefbuSetup(&pDecCont->refBufferCtrl, pDecCont->vp8Regs,
		           REFBU_FRAME,
		           dec->keyFrame || forceDisable,
		           HANTRO_FALSE,
		           bufPicId, 0,
		           flags);
	}

	if (pDecCont->tiledModeSupport && !pDecCont->intraOnly) {
		pDecCont->tiledReferenceEnable =
		        DecSetupTiledReference(pDecCont->vp8Regs,
		                               pDecCont->tiledModeSupport,
		                               0);
	} else {
		pDecCont->tiledReferenceEnable = 0;
	}
}


/*------------------------------------------------------------------------------
    Function name : VP8HwdAsicStrmPosUpdate
    Description   : Set stream base and length related registers

    Return type   :
    Argument      : container
------------------------------------------------------------------------------*/
void VP8HwdAsicStrmPosUpdate(VP8DecContainer_t *pDecCont, unsigned int strmBusAddress)
{
	unsigned int i, tmp;
	unsigned int hwBitPos;
	unsigned int tmpAddr;
	unsigned int tmp2;
	unsigned int byteOffset;
	unsigned int extraBytesPacked = 0;
	vp8Decoder_t *dec = &pDecCont->decoder;

	DEBUG_PRINT(("VP8HwdAsicStrmPosUpdate:\n"));

	/* TODO: miksi bitin tarkkuudella (count) kun kuitenki luetaan tavu
	 * kerrallaan? Vai lukeeko HW eri lailla? */
	/* start of control partition */
	tmp = (pDecCont->bc.pos) * 8 + (8 - pDecCont->bc.count);

	if (dec->frameTagSize == 4) { tmp += 8; }

	if (pDecCont->decMode == VP8HWD_VP8 &&
	    pDecCont->decoder.keyFrame) {
		extraBytesPacked += 7;
	}

	tmp += extraBytesPacked * 8;

	tmpAddr = strmBusAddress + tmp / 8;
	hwBitPos = (tmpAddr & DEC_8190_ALIGN_MASK) * 8;
	tmpAddr &= (~DEC_8190_ALIGN_MASK);  /* align the base */

	hwBitPos += tmp & 0x7;

	/* control partition */
	SetDecRegister(pDecCont->vp8Regs, HWIF_VP6HWPART1_BASE, tmpAddr);
	SetDecRegister(pDecCont->vp8Regs, HWIF_STRM1_START_BIT, hwBitPos);

	/* total stream length */
	/*tmp = pDecCont->bc.streamEndPos - (tmpAddr - strmBusAddress);*/

	/* calculate dct partition length here instead */
	tmp = pDecCont->bc.streamEndPos + dec->frameTagSize - dec->dctPartitionOffsets[0];
	tmp += (dec->nbrDctPartitions - 1) * 3;
	tmp2 = strmBusAddress + extraBytesPacked + dec->dctPartitionOffsets[0];
	tmp += (tmp2 & 0x7);

	SetDecRegister(pDecCont->vp8Regs, HWIF_STREAM_LEN, tmp);

	/* control partition length */
	tmp = dec->offsetToDctParts + dec->frameTagSize - (tmpAddr - strmBusAddress - extraBytesPacked);
	if (pDecCont->decMode == VP8HWD_VP7) { /* give extra byte for VP7 to pass test cases */
		tmp ++;
	}

	SetDecRegister(pDecCont->vp8Regs, HWIF_STREAM1_LEN, tmp);

	/* number of dct partitions */
	SetDecRegister(pDecCont->vp8Regs, HWIF_COEFFS_PART_AM,
	               dec->nbrDctPartitions - 1);

	/* base addresses and bit offsets of dct partitions */
	for (i = 0; i < dec->nbrDctPartitions; i++) {
		tmpAddr = strmBusAddress + extraBytesPacked + dec->dctPartitionOffsets[i];
		byteOffset = tmpAddr & 0x7;
		SetDecRegister(pDecCont->vp8Regs, DctBaseId[i], tmpAddr & 0xFFFFFFF8);
		SetDecRegister(pDecCont->vp8Regs, DctStartBit[i], byteOffset * 8);
	}
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdPreparePpRun
    Description     :
    Return type     : void
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdPreparePpRun(VP8DecContainer_t *pDecCont)
{
	DecPpInterface *decPpIf = &pDecCont->pp.decPpIf;

	if (pDecCont->pp.ppInstance != NULL) { /* we have PP connected */
		pDecCont->pp.ppInfo.tiledMode =
		        pDecCont->tiledReferenceEnable;
		pDecCont->pp.PPConfigQuery(pDecCont->pp.ppInstance,
		                           &pDecCont->pp.ppInfo);

		TRACE_PP_CTRL
		("vp8hwdPreparePpRun: output picture => PP could run!\n");

		decPpIf->usePipeline = pDecCont->pp.ppInfo.pipelineAccepted & 1;

		/* pipeline accepted, but previous pic needs to be processed
		 * (pp config probably changed from one that cannot be run in
		 * pipeline to one that can) */
		pDecCont->pendingPicToPp = 0;
		if (decPpIf->usePipeline && pDecCont->outCount) {
			decPpIf->usePipeline = 0;
			pDecCont->pendingPicToPp = 1;
		}

		if (decPpIf->usePipeline) {
			TRACE_PP_CTRL
			("vp8hwdPreparePpRun: pipeline=ON => PP will be running\n");
			decPpIf->ppStatus = DECPP_RUNNING;
		}
		/* parallel processing if previous output pic exists */
		else if (pDecCont->outCount) {
			TRACE_PP_CTRL
			("vp8hwdPreparePpRun: pipeline=OFF => PP has to run after DEC\n");
			decPpIf->ppStatus = DECPP_RUNNING;
		}
	}
}

/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicContPicture
    Description     :
    Return type     : void
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicContPicture(VP8DecContainer_t *pDecCont)
{
	unsigned int sliceHeight;

	/* update output picture buffer if not pipeline */
	if (pDecCont->pp.ppInstance == NULL ||
	    !pDecCont->pp.decPpIf.usePipeline) {
		vp8hwdUpdateOutBase(pDecCont);
	}

	/* slice height */
	if (pDecCont->totDecodedRows + pDecCont->sliceHeight >
	    pDecCont->asicBuff->height / 16) {
		sliceHeight = pDecCont->asicBuff->height / 16 - pDecCont->totDecodedRows;
	} else {
		sliceHeight = pDecCont->sliceHeight;
	}

	SetDecRegister(pDecCont->vp8Regs, HWIF_JPEG_SLICE_H, sliceHeight);
}


void vp8hwdUpdateOutBase(VP8DecContainer_t *pDecCont)
{

	DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;

	assert(pDecCont->intraOnly);

	if (pDecCont->userMem) {
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_BASE,
		               pAsicBuff->userMem.picBufferBusAddrY);
		SetDecRegister(pDecCont->vp8Regs, HWIF_REFER0_BASE,
		               pAsicBuff->userMem.picBufferBusAddrC);
	} else { /* if (startOfPic)*/
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_BASE,
		               pAsicBuff->outBuffer.busAddress);

		SetDecRegister(pDecCont->vp8Regs, HWIF_REFER0_BASE,
		               pAsicBuff->outBuffer.busAddress +
		               pAsicBuff->width *
		               (pDecCont->sliceHeight ? (pDecCont->sliceHeight + 1) * 16 :
		                pAsicBuff->height));
	}
}

/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicRun
    Description     :
    Return type     : unsigned int
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
unsigned int VP8HwdAsicRun(VP8DecContainer_t *pDecCont)
{
	unsigned int asic_status = 0;
	signed int ret;

	if (!pDecCont->asicRunning) {
		ret = DWLReserveHw(pDecCont->dwl);
		if (ret != DWL_OK) {
			return VP8HWDEC_HW_RESERVED;
		}

		pDecCont->asicRunning = 1;

		if (pDecCont->pp.ppInstance != NULL &&
		    pDecCont->pp.decPpIf.ppStatus == DECPP_RUNNING) {
			DecPpInterface *decPpIf = &pDecCont->pp.decPpIf;
			DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;

			TRACE_PP_CTRL("VP8HwdAsicRun: PP Run\n");

			decPpIf->inwidth  = MB_MULTIPLE(pDecCont->width);
			decPpIf->inheight = MB_MULTIPLE(pDecCont->height);
			decPpIf->croppedW = decPpIf->inwidth;
			decPpIf->croppedH = decPpIf->inheight;

			/* forward tiled mode */
			decPpIf->tiledMode = pDecCont->tiledReferenceEnable;

			decPpIf->picStruct = DECPP_PIC_FRAME_OR_TOP_FIELD;
			decPpIf->littleEndian =
			        GetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_ENDIAN);
			decPpIf->wordSwap =
			        GetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUTSWAP32_E);

			if (decPpIf->usePipeline) {
				decPpIf->inputBusLuma = decPpIf->inputBusChroma = 0;
			} else { /* parallel processing */
				decPpIf->inputBusLuma = pAsicBuff->refBuffer.busAddress;
				decPpIf->inputBusChroma =
				        decPpIf->inputBusLuma + decPpIf->inwidth * decPpIf->inheight;
			}

			pDecCont->pp.PPDecStart(pDecCont->pp.ppInstance, decPpIf);
		}
#if 1 /*Comment this to avoid freeze with orly1 (writing in the @ of Delta's registers instead of G1 decoder's registers.*/
		VP8HwdAsicFlushRegs(pDecCont);
		//DWLSaveMemory(0x1fb38180, 0x4b8, "qtable.mem_swap",1);

		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_E, 1);
		/* Enable HW */
		DWLWriteReg(pDecCont->dwl, 4 * 1, pDecCont->vp8Regs[1]);
	} else {
		DWLWriteReg(pDecCont->dwl, 4 * 13, pDecCont->vp8Regs[13]);
		DWLWriteReg(pDecCont->dwl, 4 * 14, pDecCont->vp8Regs[14]);
		DWLWriteReg(pDecCont->dwl, 4 * 15, pDecCont->vp8Regs[15]);
		DWLWriteReg(pDecCont->dwl, 4 * 1, pDecCont->vp8Regs[1]);
#endif
	}

	ret = DWLWaitHwReady(pDecCont->dwl, (unsigned int) DEC_X170_TIMEOUT_LENGTH);

	if (ret != DWL_HW_WAIT_OK) {
		pr_err("Error: %s DWLWaitHwReadyreturned: %d\n", __func__, ret);

		/* reset HW */
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ_STAT, 0);
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ, 0);

		/* HW done, release it! */
		DWLWriteReg(pDecCont->dwl, 4 * 1, pDecCont->vp8Regs[1]);

		/* Wait for PP to end also */
		if (pDecCont->pp.ppInstance != NULL &&
		    pDecCont->pp.decPpIf.ppStatus == DECPP_RUNNING) {
			pDecCont->pp.decPpIf.ppStatus = DECPP_PIC_READY;

			TRACE_PP_CTRL("VP8HwdAsicRun: PP Wait for end\n");

			pDecCont->pp.PPDecWaitEnd(pDecCont->pp.ppInstance);

			TRACE_PP_CTRL("VP8HwdAsicRun: PP Finished\n");
		}

		pDecCont->asicRunning = 0;
		DWLReleaseHw(pDecCont->dwl);

		return (ret == DWL_HW_WAIT_ERROR) ?
		       VP8HWDEC_SYSTEM_ERROR : VP8HWDEC_SYSTEM_TIMEOUT;
	}

	VP8HwdAsicRefreshRegs(pDecCont);

	/* React to the HW return value */

	asic_status = GetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ_STAT);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ_STAT, 0);
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ, 0); /* just in case */

	if (pDecCont->decoder.decMode == VP8HWD_VP7) {
		pDecCont->asicBuff->dcPred[0] =
		        GetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_COMP0);
		pDecCont->asicBuff->dcPred[1] =
		        GetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_COMP1);
		pDecCont->asicBuff->dcMatch[0] =
		        GetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_MATCH0);
		pDecCont->asicBuff->dcMatch[1] =
		        GetDecRegister(pDecCont->vp8Regs, HWIF_INIT_DC_MATCH1);
	}
	if (!(asic_status & DEC_8190_IRQ_SLICE)) { /* not slice interrupt */
		/* HW done, release it! */
		DWLWriteReg(pDecCont->dwl, 4 * 1, pDecCont->vp8Regs[1]);
		pDecCont->asicRunning = 0;

		/* Wait for PP to end also, this is pipeline case */
		if (pDecCont->pp.ppInstance != NULL &&
		    pDecCont->pp.decPpIf.ppStatus == DECPP_RUNNING) {
			pDecCont->pp.decPpIf.ppStatus = DECPP_PIC_READY;

			TRACE_PP_CTRL("VP8HwdAsicRun: PP Wait for end\n");

			pDecCont->pp.PPDecWaitEnd(pDecCont->pp.ppInstance);

			TRACE_PP_CTRL("VP8HwdAsicRun: PP Finished\n");
		}

		DWLReleaseHw(pDecCont->dwl);

		if (pDecCont->refBufSupport &&
		    (asic_status & DEC_8190_IRQ_RDY) &&
		    pDecCont->asicRunning == 0) {
			RefbuMvStatistics(&pDecCont->refBufferCtrl,
			                  pDecCont->vp8Regs,
			                  NULL, HANTRO_FALSE,
			                  pDecCont->decoder.keyFrame);
		}
	}

	return asic_status;
}


/****************************************************************************
 *
 *  ROUTINE       :     vp8hwdDecodeBool
 *
 *  INPUTS        :     vpBoolCoder_t *br  : pointer to instance of a boolean decoder.
 *                        int probability : probability that next symbol is a 0 (0-255)
 *
 *  OUTPUTS       :     None.
 *
 *  RETURNS       :        Next decoded symbol (0 or 1)
 *
 *  FUNCTION      :     Decodes the next symbol (0 or 1) using the specified
 *                      boolean decoder.
 *
 *  SPECIAL NOTES :     None.
 *
 ****************************************************************************/
unsigned int vp8hwdDecodeBool(vpBoolCoder_t *br, signed int probability)
{
	unsigned int bit = 0;
	unsigned int split;
	unsigned int bigsplit;
	unsigned int count = br->count;
	unsigned int range = br->range;
	unsigned int value = br->value;

	split = 1 + (((range - 1) * probability) >> 8);
	bigsplit = (split << 24);

#ifdef BOOLDEC_TRACE
	printf("    p=%3d %9d/%4d s=%3d",
	       probability, value >> 24, range, split);
#endif /* BOOLDEC_TRACE */

	range = split;

	if (value >= bigsplit) {
		range = br->range - split;
		value = value - bigsplit;
		bit = 1;
	}

#ifdef BOOLDEC_TRACE
	printf(" --> %d\n", bit);
#endif /* BOOLDEC_TRACE */

	if (range >= 0x80) {
		br->value = value;
		br->range = range;
		return bit;
	} else {
		do {
			range += range;
			value += value;

			if (!--count) {
				/* no more stream to read? */
				if (br->pos >= br->streamEndPos) {
					br->strmError = 1;
					break;
				}
				count = 8;
				value |= br->buffer[br->pos];
				br->pos++;
			}
		} while (range < 0x80);
	}


	br->count = count;
	br->value = value;
	br->range = range;

	return bit;
}

/*------------------------------------------------------------------------------
    vp8hwdSetPartitionOffsets

        Read partition offsets from stream and initialize into internal
        structures.
------------------------------------------------------------------------------*/
unsigned int vp8hwdSetPartitionOffsets(unsigned char *stream, unsigned int len, vp8Decoder_t *dec)
{
	unsigned int i = 0;
	unsigned int offset = 0;
	unsigned int baseOffset;
	unsigned int extraBytesPacked = 0;

	if (dec->decMode == VP8HWD_VP8 &&
	    dec->keyFrame) {
		extraBytesPacked += 7;
	}

	stream += dec->frameTagSize;

	baseOffset = dec->frameTagSize + dec->offsetToDctParts +
	             3 * (dec->nbrDctPartitions - 1);

	stream += dec->offsetToDctParts + extraBytesPacked;
	for (i = 0 ; i < dec->nbrDctPartitions - 1 ; ++i) {
		dec->dctPartitionOffsets[i] = baseOffset + offset;
		offset += ReadPartitionSize(stream);
		stream += 3;
	}
	dec->dctPartitionOffsets[i] = baseOffset + offset;

	return (dec->dctPartitionOffsets[i] < len ? HANTRO_OK : HANTRO_NOK);

}

/*------------------------------------------------------------------------------
    Function name : RefbuGetHitThreshold
    Description   :

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
signed int RefbuGetHitThreshold(refBuffer_t *pRefbu)
{

	signed int requiredHitsClk = 0;
	signed int requiredHitsData = 0;
	signed int divisor;
	signed int tmp;

#ifdef REFBUFFER_TRACE
	signed int predMiss;
	printf("***** ref buffer threshold trace *****\n");
#endif /* REFBUFFER_TRACE */

	divisor = pRefbu->avgCyclesPerMb - pRefbu->bufferPenalty;

	if (divisor > 0) {
		requiredHitsClk = (4 * pRefbu->numCyclesForBuffering) / divisor;
	}

	divisor = pRefbu->mbWeight;
#ifdef REFBUFFER_TRACE
	printf("mb weight           = %7d\n", divisor);
#endif /* REFBUFFER_TRACE */

	if (divisor > 0) {

		divisor = (divisor * pRefbu->dataExcessMaxPct) / 100;

#ifdef REFBUFFER_TRACE
		predMiss = 4 * pRefbu->frmSizeInMbs - pRefbu->predIntraBlk -
		           pRefbu->predCoverage;
		printf("predicted misses    = %7d\n", predMiss);
		printf("data excess %%       = %7d\n", pRefbu->dataExcessMaxPct);
		printf("divisor             = %7d\n", divisor);
#endif /* REFBUFFER_TRACE */

		/*tmp = (( DATA_EXCESS_MAX_PCT - 100 ) * pRefbu->mbWeight * predMiss ) / 400;*/
		tmp = 0;
		requiredHitsData = (4 * pRefbu->totalDataForBuffering - tmp);
		requiredHitsData /= divisor;
	}

	if (pRefbu->picHeightInMbs) {
		requiredHitsClk /= pRefbu->picHeightInMbs;
		requiredHitsData /= pRefbu->picHeightInMbs;
	}

#ifdef REFBUFFER_TRACE
	printf("target (clk)        = %7d\n", requiredHitsClk);
	printf("target (data)       = %7d\n", requiredHitsData);
	printf("***** ref buffer threshold trace *****\n");

#endif /* REFBUFFER_TRACE */

	return requiredHitsClk > requiredHitsData ?
	       requiredHitsClk : requiredHitsData;
}

/*------------------------------------------------------------------------------
    Function name : RefbuGetVpxCoveragePrediction
    Description   : Return coverage and intra block prediction for VPx
                    "intelligent" ref buffer control. Prediction algorithm
                    to be finalized

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
unsigned int RefbuVpxGetPrevFrameStats(refBuffer_t *pRefbu)
{
	signed int cov, tmp;

	tmp = pRefbu->prevFrameHitSum;
	if (tmp >= pRefbu->checkpoint && pRefbu->checkpoint) {
		cov = tmp / 4;
	} else {
		cov = 0;
	}
	return cov;
}
/*------------------------------------------------------------------------------
    DecSetupTiledReference
        Enable/disable tiled reference mode on HW. See inside function for
        disable criterias.

        Returns tile mode.
------------------------------------------------------------------------------*/
unsigned int DecSetupTiledReference(unsigned int *regBase, unsigned int tiledModeSupport, unsigned int interlacedStream)
{
	unsigned int tiledAllowed = 1;
	unsigned int mode = 0;

	if (!tiledModeSupport) {
		SetDecRegister(regBase, HWIF_TILED_MODE_MSB, 0);
		SetDecRegister(regBase, HWIF_TILED_MODE_LSB, 0);
		return TILED_REF_NONE;
	}

	/* disable for interlaced streams */
	if (interlacedStream) {
		tiledAllowed = 0;
	}

	/* if tiled mode allowed, pick a tile mode that suits us best (pretty easy
	 * as we currently only support 8x4 */
	if (tiledAllowed) {
		if (tiledModeSupport == 1) {
			mode = TILED_REF_8x4;
		}
	} else {
		mode = TILED_REF_NONE;
	}

	/* Setup HW registers */
	SetDecRegister(regBase, HWIF_TILED_MODE_MSB, (mode >> 1) & 0x1);
	SetDecRegister(regBase, HWIF_TILED_MODE_LSB, mode & 0x1);

	return mode;
}

/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicRefreshRegs
    Description     :
    Return type     : void
    Argument        : decContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicRefreshRegs(VP8DecContainer_t *pDecCont)
{
	signed int i;
	unsigned int offset = 0x0;

	unsigned int *decRegs = pDecCont->vp8Regs;

	for (i = DEC_X170_REGISTERS; i > 0; --i) {
		*decRegs++ = DWLReadReg(pDecCont->dwl, offset);
		offset += 4;
	}
}


/*------------------------------------------------------------------------------
    Function name : RefbuMvStatistics
    Description   :

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
void RefbuMvStatistics(refBuffer_t *pRefbu, unsigned int *regBase,
                       unsigned int *pMv, unsigned int directMvsAvailable, unsigned int isIntraPicture)
{
	signed int *pTmp;
	signed int tmp;
	signed int numIntraBlk;
	signed int topFldCnt;
	signed int botFldCnt;
	unsigned int bigEndian;

	if (isIntraPicture) {
		/*IntraFrame( pRefbu );*/
		return; /* Clear buffers etc. ? */
	}

	if (pRefbu->prevWasField && !pRefbu->interlacedSupport) {
		return;
	}

	/* Determine HW endianness; this affects how we read motion vector
	 * map from HW. Map endianness is same as decoder output endianness */
	if (GetDecRegister(regBase, HWIF_DEC_OUT_ENDIAN) == 0) {
		bigEndian = 1;
	} else {
		bigEndian = 0;
	}

	if (!pRefbu->offsetSupport || 1) /* Note: offset always disabled for now,
                                     * so always disallow direct mv data */
	{
		directMvsAvailable = 0;
	}

	numIntraBlk = GetDecRegister(regBase, HWIF_REFBU_INTRA_SUM);
	topFldCnt = GetDecRegister(regBase, HWIF_REFBU_TOP_SUM);
	botFldCnt = GetDecRegister(regBase, HWIF_REFBU_BOT_SUM);

#ifdef REFBUFFER_TRACE
	printf("***** ref buffer mv statistics trace *****\n");
	printf("intra blocks        = %7d\n", numIntraBlk);
	printf("top fields          = %7d\n", topFldCnt);
	printf("bottom fields       = %7d\n", botFldCnt);
#endif /* REFBUFFER_TRACE */

	if (pRefbu->fldCnt > 0 &&
	    GetDecRegister(regBase, HWIF_FIELDPIC_FLAG_E) &&
	    (topFldCnt || botFldCnt)) {
		pRefbu->fldHitsP[2][0] = pRefbu->fldHitsP[1][0];    pRefbu->fldHitsP[2][1] = pRefbu->fldHitsP[1][1];
		pRefbu->fldHitsP[1][0] = pRefbu->fldHitsP[0][0];    pRefbu->fldHitsP[1][1] = pRefbu->fldHitsP[0][1];
		if (GetDecRegister(regBase, HWIF_PIC_TOPFIELD_E)) {
			pRefbu->fldHitsP[0][0] = topFldCnt;         pRefbu->fldHitsP[0][1] = botFldCnt;
		} else {
			pRefbu->fldHitsP[0][0] = botFldCnt;         pRefbu->fldHitsP[0][1] = topFldCnt;
		}
	}
	if (GetDecRegister(regBase, HWIF_FIELDPIC_FLAG_E)) {
		pRefbu->fldCnt++;
	}

	pRefbu->coverage[2] = pRefbu->coverage[1];
	pRefbu->coverage[1] = pRefbu->coverage[0];
	if (directMvsAvailable) {
		DirectMvStatistics(pRefbu, pMv, numIntraBlk, bigEndian);
	} else if (pRefbu->offsetSupport) {
		signed int interMvs;
		signed int sum;
		sum = GetDecRegister(regBase, HWIF_REFBU_Y_MV_SUM);
		SIGN_EXTEND(sum, 22);
		interMvs = (4 * pRefbu->frmSizeInMbs - numIntraBlk) / 4;
		if (pRefbu->prevWasField) {
			interMvs *= 2;
		}
		if (interMvs == 0) {
			interMvs = 1;
		}
		/* Require at least 50% mvs present to calculate reliable avg offset */
		if (interMvs * 50 >= pRefbu->frmSizeInMbs) {
			/* Store updated offsets */
			pTmp = pRefbu->oy;
			pTmp[2] = pTmp[1];
			pTmp[1] = pTmp[0];
			pTmp[0] = sum / interMvs;
		}
	}

	/* Read buffer hits from previous frame. If number of hits < threshold
	 * for the frame, then we know that HW switched buffering off. */
	tmp = GetDecRegister(regBase, HWIF_REFBU_HIT_SUM);
	pRefbu->prevFrameHitSum = tmp;
	if (tmp >= pRefbu->checkpoint && pRefbu->checkpoint) {
		if (pRefbu->prevWasField) {
			tmp *= 2;
		}
		pRefbu->coverage[0] = tmp;

#ifdef REFBUFFER_TRACE
		printf("actual coverage     = %7d\n", tmp);
#endif /* REFBUFFER_TRACE */

	} else if (!directMvsAvailable) {
		/* Buffering was disabled for previous frame, no direct mv
		 * data available either, so assume we got a bit more hits than
		 * the frame before that */
		if (pRefbu->coverage[1] != -1) {
			pRefbu->coverage[0] = (4 *
			                       pRefbu->picWidthInMbs *
			                       pRefbu->picHeightInMbs + 5 * pRefbu->coverage[1]) / 6;
		} else {
			pRefbu->coverage[0] = pRefbu->frmSizeInMbs * 4;
		}

#ifdef REFBUFFER_TRACE
		printf("calculated coverage = %7d\n", pRefbu->coverage[0]);
#endif /* REFBUFFER_TRACE */

	} else {

#ifdef REFBUFFER_TRACE
		printf("estimated coverage  = %7d\n", pRefbu->coverage[0]);
#endif /* REFBUFFER_TRACE */

	}

	/* Store intra counts for rate prediction */
	pTmp = pRefbu->numIntraBlk;
	pTmp[2] = pTmp[1];
	pTmp[1] = pTmp[0];
	pTmp[0] = numIntraBlk;

	/* Predict number of intra mbs for next frame */
	if (pTmp[2] != -1) { tmp = (pTmp[0] + pTmp[1] + pTmp[2]) / 3; }
	else if (pTmp[1] != -1) { tmp = (pTmp[0] + pTmp[1]) / 2; }
	else { tmp = pTmp[0]; }
	pRefbu->predIntraBlk = MIN(pTmp[0], tmp);

#ifdef REFBUFFER_TRACE
	printf("predicted intra blk = %7d\n", pRefbu->predIntraBlk);
	printf("***** ref buffer mv statistics trace *****\n");
#endif /* REFBUFFER_TRACE */


}


/*------------------------------------------------------------------------------
    ReadPartitionSize

        Read partition size from stream.
------------------------------------------------------------------------------*/
unsigned int ReadPartitionSize(unsigned char *cxSize)
{
	unsigned int size;
	size = (unsigned int)(*cxSize)
	       + ((unsigned int)(* (cxSize + 1)) << 8)
	       + ((unsigned int)(* (cxSize + 2)) << 16);
	return size;
}


/*------------------------------------------------------------------------------
    Function name : RefbuSetup
    Description   : Setup reference buffer for next frame.

    Return type   :
    Argument      : pRefbu              Reference buffer descriptor
                    regBase             Pointer to SW/HW control registers
                    isInterlacedField
                    isIntraFrame        Is frame intra-coded (or IDR for H.264)
                    isBframe            Is frame B-coded
                    refPicId            pic_id for reference picture, if
                                        applicable. For H.264 pic_id for
                                        nearest reference picture of L0.
------------------------------------------------------------------------------*/
void RefbuSetup(refBuffer_t *pRefbu, unsigned int *regBase,
                refbuMode_e mode,
                unsigned int isIntraFrame,
                unsigned int isBframe,
                unsigned int refPicId0, unsigned int refPicId1, unsigned int flags)
{
	signed int ox, oy;
	signed int enable;
	signed int tmp;
	signed int thr2 = 0;
	unsigned int featureDisable = 0;
	unsigned int useAdaptiveMode = 0;
	unsigned int useDoubleMode = 0;
	unsigned int disableCheckpoint = 0;
	unsigned int multipleRefFrames = 1;
	unsigned int multipleRefFields = 1;
	unsigned int forceAdaptiveSingle = 1;
	unsigned int singleRefField = 0;
	unsigned int pic0 = 0, pic1 = 0;

	SetDecRegister(regBase, HWIF_REFBU_THR, 0);
	SetDecRegister(regBase, HWIF_REFBU2_THR, 0);
	SetDecRegister(regBase, HWIF_REFBU_PICID, 0);
	SetDecRegister(regBase, HWIF_REFBU_Y_OFFSET, 0);

	multipleRefFrames = (flags & REFBU_MULTIPLE_REF_FRAMES) ? 1 : 0;
	disableCheckpoint = (flags & REFBU_DISABLE_CHECKPOINT) ? 1 : 0;
	forceAdaptiveSingle = (flags & REFBU_FORCE_ADAPTIVE_SINGLE) ? 1 : 0;

	pRefbu->prevWasField = (mode == REFBU_FIELD && !isBframe);

	/* Check supported features */
	if (mode != REFBU_FRAME && !pRefbu->interlacedSupport) {
		featureDisable = 1;
	}

	if (!isIntraFrame && !featureDisable) {
		if (pRefbu->prevLatency != pRefbu->currMemModel.latency) {
			UpdateMemModel(pRefbu);
			pRefbu->prevLatency = pRefbu->currMemModel.latency;
		}

		enable = GetSettings(pRefbu, &ox, &oy,
		                     isBframe, mode == REFBU_FIELD);

		tmp = RefbuGetHitThreshold(pRefbu);
		pRefbu->checkpoint = tmp;

		if (mode == REFBU_FIELD) {
			tmp = DecideParityMode(pRefbu, isBframe);
			SetDecRegister(regBase, HWIF_REFBU_FPARMOD_E, tmp);
			if (!tmp) {
				pRefbu->thrAdj = 1;
			}
		} else {
			pRefbu->thrAdj = 1;
		}

		SetDecRegister(regBase, HWIF_REFBU_E, enable);
		if (enable) {
			/* Figure out which features to enable */
			if (pRefbu->doubleSupport) {
				if (!isBframe) { /* P field/frame */
					if (mode == REFBU_FIELD) {
						if (singleRefField) {
							/* Buffer only reference field given in refPicId0 */
						} else {
							useDoubleMode = 1;
							if (multipleRefFields) {
								/* Let's not try to guess */
								useAdaptiveMode = 1;
							}
							/* Buffer both reference fields explicitly */
							pRefbu->checkpoint /= pRefbu->thrAdj;
							thr2 = pRefbu->checkpoint;
						}
					} else if (forceAdaptiveSingle) {
						useAdaptiveMode = 1;
						useDoubleMode = 0;
					} else if (multipleRefFrames) {
						useAdaptiveMode = 1;
						useDoubleMode = 1;
						pRefbu->checkpoint /= pRefbu->thrAdj;
						thr2 = pRefbu->checkpoint;
					} else {
						/* Content to buffer just one ref pic */
					}
				} else { /* B field/frame */
					if (mode == REFBU_FIELD) {
						/* Let's not try to guess */
						useAdaptiveMode = 1;
						useDoubleMode = 1;
						pRefbu->checkpoint /= pRefbu->thrAdj;
						/*pRefbu->checkpoint /= 2;*/
						thr2 = pRefbu->checkpoint;
					} else if (!multipleRefFrames) {
						/* Buffer forward and backward pictures as given in
						 * refPicId0 and refPicId1 */
						useDoubleMode = 1;
						pRefbu->checkpoint /= pRefbu->thrAdj;
						thr2 = pRefbu->checkpoint;
					} else {
						/* Let's not try to guess */
						useDoubleMode = 1;
						useAdaptiveMode = 1;
						pRefbu->checkpoint /= pRefbu->thrAdj;
						thr2 = pRefbu->checkpoint;
					}
				}
			} else { /* Just single buffering supported */
				if (!isBframe) { /* P field/frame */
					if (mode == REFBU_FIELD) {
						useAdaptiveMode = 1;
					} else if (forceAdaptiveSingle) {
						useAdaptiveMode = 1;
					} else if (multipleRefFrames) {
						/*useAdaptiveMode = 1;*/
					} else {
					}
				} else { /* B field/frame */
					useAdaptiveMode = 1;
				}
			}

			if (!useAdaptiveMode) {
				pic0 = refPicId0;
				if (useDoubleMode) {
					pic1 = refPicId1;
				}
			}

			SetDecRegister(regBase, HWIF_REFBU_EVAL_E, useAdaptiveMode);

			/* Calculate amount of hits required for first mb row */

			if (mode == REFBU_MBAFF) {
				pRefbu->checkpoint *= 2;
				thr2 *= 2;
			}

			if (useDoubleMode) {
				oy = 0; /* Disable offset */
			}

			/* Limit offset */
			{
				signed int limit, height;
				height = pRefbu->picHeightInMbs;
				if (mode == REFBU_FIELD) {
					height /= 2;        /* adjust to field height */
				}

				if (mode == REFBU_MBAFF) {
					limit = 64;        /* 4 macroblock rows */
				} else {
					limit = 48;        /* 3 macroblock rows */
				}

				if ((signed int)(oy + limit) > (signed int)(height * 16)) {
					oy = height * 16 - limit;
				}
				if ((signed int)((-oy) + limit) > (signed int)(height * 16)) {
					oy = -(height * 16 - limit);
				}
			}

			/* Disable offset just to make sure */
			if (!pRefbu->offsetSupport || 1) { /* NOTE: always disabled for now */
				oy = 0;
			}

			if (!disableCheckpoint) {
				SetDecRegister(regBase, HWIF_REFBU_THR, pRefbu->checkpoint);
			} else {
				SetDecRegister(regBase, HWIF_REFBU_THR, 0);
			}
			SetDecRegister(regBase, HWIF_REFBU_PICID, pic0);
			SetDecRegister(regBase, HWIF_REFBU_Y_OFFSET, oy);

			if (pRefbu->doubleSupport) {
				/* Very much TODO */
				SetDecRegister(regBase, HWIF_REFBU2_BUF_E, useDoubleMode);
				SetDecRegister(regBase, HWIF_REFBU2_THR, thr2);
				SetDecRegister(regBase, HWIF_REFBU2_PICID, pic1);
				pRefbu->prevUsedDouble = useDoubleMode;
			}
		}
		pRefbu->prevWasField = (mode == REFBU_FIELD && !isBframe);
	} else {
		pRefbu->checkpoint = 0;
		SetDecRegister(regBase, HWIF_REFBU_E, HANTRO_FALSE);
	}

	if (pRefbu->testFunction) {
		pRefbu->testFunction(pRefbu, regBase, isIntraFrame, mode);
	}

}

/*------------------------------------------------------------------------------
    Function name : UpdateMemModel
    Description   : Update memory model for reference buffer

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
void UpdateMemModel(refBuffer_t *pRefbu)
{
	signed int widthInMbs;
	signed int heightInMbs;
	signed int latency;
	signed int nonseq;
	signed int seq;

	signed int busWidth;
	signed int tmp, tmp2;

#define DIV_CEIL(a,b) (((a)+(b)-1)/(b))

	widthInMbs  = pRefbu->picWidthInMbs;
	heightInMbs = pRefbu->picHeightInMbs;
	busWidth    = pRefbu->busWidthInBits;

	tmp = busWidth >> 2;    /* initially buffered mbs per row */
	tmp2 = busWidth >> 3;   /* n:o of mbs buffered at refresh */
	tmp = (1 + DIV_CEIL(widthInMbs - tmp, tmp2));    /* latencies per mb row */
	tmp2 = 24 * heightInMbs; /* Number of rows to buffer in total */
	/* Latencies per frame */
	latency = 2 * tmp * heightInMbs;
	nonseq  = tmp2 * tmp;
	seq = (DIV_CEIL(16 * widthInMbs, busWidth >> 3)) * tmp2 -
	      nonseq;

	pRefbu->numCyclesForBuffering =
	        latency * pRefbu->currMemModel.latency +
	        nonseq * (1 + pRefbu->currMemModel.nonseq) +
	        seq * (1 + pRefbu->currMemModel.seq);

	pRefbu->bufferPenalty =
	        pRefbu->memAccessStats.nonseq +
	        pRefbu->memAccessStats.seq;
	if (busWidth == 32) {
		pRefbu->bufferPenalty >>= 1;
	}

	pRefbu->avgCyclesPerMb =
	        ((pRefbu->memAccessStats.latency * pRefbu->currMemModel.latency) / 100) +
	        pRefbu->memAccessStats.nonseq * (1 + pRefbu->currMemModel.nonseq) +
	        pRefbu->memAccessStats.seq * (1 + pRefbu->currMemModel.seq);

#ifdef REFBUFFER_TRACE
	printf("***** ref buffer mem model trace *****\n");
	printf("latency             = %7d clk\n", pRefbu->currMemModel.latency);
	printf("sequential          = %7d clk\n", pRefbu->currMemModel.nonseq);
	printf("non-sequential      = %7d clk\n", pRefbu->currMemModel.seq);

	printf("latency (mb)        = %7d\n", pRefbu->memAccessStats.latency);
	printf("sequential (mb)     = %7d\n", pRefbu->memAccessStats.nonseq);
	printf("non-sequential (mb) = %7d\n", pRefbu->memAccessStats.seq);

	printf("bus-width           = %7d\n", busWidth);

	printf("buffering cycles    = %7d\n", pRefbu->numCyclesForBuffering);
	printf("buffer penalty      = %7d\n", pRefbu->bufferPenalty);
	printf("avg cycles per mb   = %7d\n", pRefbu->avgCyclesPerMb);

	printf("***** ref buffer mem model trace *****\n");
#endif /* REFBUFFER_TRACE */

#undef DIV_CEIL

}


/*------------------------------------------------------------------------------
    Function name : GetSettings
    Description   : Determine whether or not to enable buffer, and calculate
                    buffer offset.

    Return type   : unsigned int
    Argument      :
------------------------------------------------------------------------------*/
unsigned int GetSettings(refBuffer_t *pRefbu, signed int *pX, signed int *pY, unsigned int isBpic,
                         unsigned int isFieldPic)
{
	signed int tmp;
	unsigned int enable = HANTRO_TRUE;
	unsigned int frmSizeInMbs;
	signed int cov;
	signed int sign;
	signed int numCyclesForBuffering;

	frmSizeInMbs = pRefbu->frmSizeInMbs;
	*pX = 0;
	*pY = 0;

	/* Disable automatically for pictures less than 16MB wide */
	if (pRefbu->picWidthInMbs <= 16) {
		return HANTRO_FALSE;
	}

	numCyclesForBuffering = pRefbu->numCyclesForBuffering;
	if (isFieldPic) {
		numCyclesForBuffering /= 2;
	}

	if (pRefbu->prevUsedDouble) {
		cov = pRefbu->coverage[0];
		tmp = pRefbu->avgCyclesPerMb * cov / 4;

		/* Check whether it is viable to enable buffering */
		tmp = (2 * numCyclesForBuffering < tmp);
		if (!tmp) {
			pRefbu->thrAdj -= 2;
			if (pRefbu->thrAdj < THR_ADJ_MIN) {
				pRefbu->thrAdj = THR_ADJ_MIN;
			}
		} else {
			pRefbu->thrAdj++;
			if (pRefbu->thrAdj > THR_ADJ_MAX) {
				pRefbu->thrAdj = THR_ADJ_MAX;
			}
		}
	}

	if (!isBpic) {
		if (pRefbu->coverage[1] != -1) {
			cov = (5 * pRefbu->coverage[0] - 1 * pRefbu->coverage[1]) / 4;
			if (pRefbu->coverage[2] != -1) {
				cov = cov + (pRefbu->coverage[0] + pRefbu->coverage[1] + pRefbu->coverage[2]) / 3;
				cov /= 2;
			}

		} else if (pRefbu->coverage[0] != -1) {
			cov = pRefbu->coverage[0];
		} else {
			cov = 4 * frmSizeInMbs;
		}
	} else {
		cov = pRefbu->coverage[0];
		if (cov == -1) {
			cov = 4 * frmSizeInMbs;
		}
		/* MPEG-4 B-frames have no intra coding possibility, so extrapolate
		 * hit rate to match it */
		else if (pRefbu->predIntraBlk < 4 * frmSizeInMbs &&
		         pRefbu->decMode == DEC_X170_MODE_MPEG4) {
			cov *= (128 * 4 * frmSizeInMbs) / (4 * frmSizeInMbs - pRefbu->predIntraBlk) ;
			cov /= 128;
		}
		/* Assume that other formats have less intra MBs in B pictures */
		else {
			cov *= (128 * 4 * frmSizeInMbs) / (4 * frmSizeInMbs - pRefbu->predIntraBlk / 2) ;
			cov /= 128;
		}
	}
	if (cov < 0) { cov = 0; }
	pRefbu->predCoverage = cov;

	/* Check whether it is viable to enable buffering */
	/* 1.criteria = cycles */
	tmp = pRefbu->avgCyclesPerMb * cov / 4;
	numCyclesForBuffering += pRefbu->bufferPenalty * cov / 4;
	enable = (numCyclesForBuffering < tmp);
	/* 2.criteria = data */
	/*
	tmp = ( DATA_EXCESS_MAX_PCT * cov ) / 400;
	tmp = tmp * pRefbu->mbWeight;
	enable = enable && (pRefbu->totalDataForBuffering < tmp);
	*/

#ifdef REFBUFFER_TRACE
	printf("***** ref buffer algorithm trace *****\n");
	printf("predicted coverage  = %7d\n", cov);
	printf("bus width in calc   = %7d\n", pRefbu->busWidthInBits);
	printf("coverage history    = %7d%7d%7d\n",
	       pRefbu->coverage[0],
	       pRefbu->coverage[1],
	       pRefbu->coverage[2]);
	printf("enable              = %d (%8d<%8d)\n", enable, numCyclesForBuffering, tmp);
#endif /* REFBUFFER_TRACE */

	/* If still enabled, calculate offsets */
	if (enable) {
		/* Round to nearest 16 multiple */
		tmp = (pRefbu->oy[0] + pRefbu->oy[1] + 1) / 2;
		sign = (tmp < 0);
		if (pRefbu->prevWasField) {
			tmp /= 2;
		}
		tmp = ABS(tmp);
		tmp = tmp & ~15;
		if (sign) { tmp = -tmp; }
		*pY = tmp;
	}

#ifdef REFBUFFER_TRACE
	printf("offset_x            = %7d\n", *pX);
	printf("offset_y            = %7d (%d %d)\n", *pY, pRefbu->oy[0], pRefbu->oy[1]);
	printf("***** ref buffer algorithm trace *****\n");
#endif /* REFBUFFER_TRACE */


	return enable;
}

/*------------------------------------------------------------------------------
    Function name : DecideParityMode
    Description   : Setup reference buffer for next frame.

    Return type   :
    Argument      : pRefbu              Reference buffer descriptor
                    isBframe            Is frame B-coded
------------------------------------------------------------------------------*/
unsigned int DecideParityMode(refBuffer_t *pRefbu, unsigned int isBframe)
{
	signed int same, opp;

	if (pRefbu->decMode != DEC_X170_MODE_H264) {
		/* Don't use parity mode for other formats than H264
		 * for now */
		return 0;
	}

	/* Read history */
	if (isBframe) {
		same = pRefbu->fldHitsB[0][0];
		/*
		if( pRefbu->fldHitsB[1][0] >= 0 )
		    same += pRefbu->fldHitsB[1][0];
		if( pRefbu->fldHitsB[2][0] >= 0 )
		    same += pRefbu->fldHitsB[2][0];
		    */
		opp  = pRefbu->fldHitsB[0][1];
		/*
		if( pRefbu->fldHitsB[1][1] >= 0 )
		    opp += pRefbu->fldHitsB[1][1];
		if( pRefbu->fldHitsB[2][0] >= 0 )
		    opp += pRefbu->fldHitsB[2][1];*/
	} else {
		same = pRefbu->fldHitsP[0][0];
		/*
		if( pRefbu->fldHitsP[1][0] >= 0 )
		    same += pRefbu->fldHitsP[1][0];
		if( pRefbu->fldHitsP[2][0] >= 0 )
		    same += pRefbu->fldHitsP[2][0];
		    */
		opp  = pRefbu->fldHitsP[0][1];
		/*
		if( pRefbu->fldHitsP[1][1] >= 0 )
		    opp += pRefbu->fldHitsP[1][1];
		if( pRefbu->fldHitsP[2][0] >= 0 )
		    opp += pRefbu->fldHitsP[2][1];
		    */
	}

	/* If not enough data yet, bail out */
	if (same == -1 || opp == -1) {
		return 0;
	}

	if (opp * 2 <= same) {
		return 1;
	}

	return 0;

}

/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicFlushRegs
    Description     :
    Return type     : void
    Argument        : decContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicFlushRegs(VP8DecContainer_t *pDecCont)
{
	signed int i;
	unsigned int offset = 0x04;
	const unsigned int *decRegs = pDecCont->vp8Regs + 1;

#ifdef TRACE_START_MARKER
	/* write ID register to trigger logic analyzer */
	DWLWriteReg(pDecCont->dwl, 0x00, ~0);
#endif

	for (i = DEC_X170_REGISTERS; i > 1; --i) {
		DWLWriteReg(pDecCont->dwl, offset, *decRegs);
		decRegs++;
		offset += 4;
	}

#if defined(DEBUG_VIDEO)
	char *My_regs = (char *) pDecCont->vp8Regs;
	struct file *filp;
	filp = filp_open("regs_dump.bin", O_CREAT | O_RDWR, 0644);
	if (IS_ERR(filp)) {
		pr_alert("Oops ! filp_open failed\n");
	} else {
		mm_segment_t old_fs = get_fs();
		set_fs(get_ds());

		filp->f_op->write(filp, (char *) My_regs, 240, &filp->f_pos);

		set_fs(old_fs);
		filp_close(filp, NULL);
		pr_alert("File Operations END\n");
	}
#endif
}


/*------------------------------------------------------------------------------
    Function name : DirectMvStatistics
    Description   :

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
void DirectMvStatistics(refBuffer_t *pRefbu, unsigned int *pMv, signed int numIntraBlk,
                        unsigned int bigEndian)
{
	signed int *pTmp;
	signed int frmSizeInMbs;
	signed int i;
	signed int oy = 0;
	signed int best = 0;
	signed int sum;
	signed int mvsPerMb;
	signed int minY = VER_DISTR_MAX, maxY = VER_DISTR_MIN;

	/* Mv distributions per component*/
//    unsigned int  distrVer[VER_DISTR_RANGE] = { 0 };
	unsigned int *distrVer = OSDEV_Malloc(VER_DISTR_RANGE * 4);
	unsigned int *pDistrVer = distrVer + VER_DISTR_ZERO_POINT;

	mvsPerMb = pRefbu->mvsPerMb;

	if (pRefbu->prevWasField) {
		frmSizeInMbs = pRefbu->fldSizeInMbs;
	} else {
		frmSizeInMbs = pRefbu->frmSizeInMbs;
	}

	if (numIntraBlk < 4 * frmSizeInMbs) {
		BuildDistribution(pDistrVer,
		                  pMv,
		                  frmSizeInMbs,
		                  mvsPerMb,
		                  bigEndian,
		                  &minY, &maxY);

		/* Fix Intra occurences */
		pDistrVer[0] -= numIntraBlk;

#if 0 /* Use median for MV calculation */
		/* Find median */
		{
			tmp = (frmSizeInMbs - numIntraMbs) / 2;
			sum = 0;
			i = VER_DISTR_MIN;
			for (i = VER_DISTR_MIN ; sum < tmp ; i++) {
				sum += pDistrVer[i];
			}
			oy = i - 1;

			/* Calculate coverage percent */
			best = 0;
			i = MAX(VER_DISTR_MIN, oy - VER_MV_RANGE);
			limit = MIN(VER_DISTR_MAX, oy + VER_MV_RANGE);
			for (; i < limit ; ++i) {
				best += pDistrVer[i];
			}
		}
		else
#endif
		{
			signed int y;
			signed int penalty;

			/* Initial sum */
			sum = 0;
			minY += VER_DISTR_ZERO_POINT;
			maxY += VER_DISTR_ZERO_POINT;

			for (i = 0 ; i < 2 * VER_MV_RANGE ; ++i) {
				sum += distrVer[i];
			}
			best = 0;
			/* Other sums */
			maxY -= 2 * VER_MV_RANGE;
			for (i = 0 ; i < VER_DISTR_RANGE - 2 * VER_MV_RANGE - 1 ; ++i) {
				sum -= distrVer[i];
				sum += distrVer[2 * VER_MV_RANGE + i];
				y = VER_DISTR_MIN + VER_MV_RANGE + i + 1;
				if (ABS(y) > 8) {
					penalty = ABS(y) - 8;
					penalty = (frmSizeInMbs * penalty) / 16;
				} else {
					penalty = 0;
				}
				/*if( (ABS(y) & 15) == 0 )*/
				{
					if (sum - penalty > best) {
						best = sum - penalty;
						oy = y;
					} else if (sum - penalty == best) {
						if (ABS(y) < ABS(oy)) { oy = y; }
					}
				}
			}
		}

		if (pRefbu->prevWasField) {
			best *= 2;
		}
		pRefbu->coverage[0] = best;

		/* Store updated offsets */
		pTmp = pRefbu->oy;
		pTmp[2] = pTmp[1];
		pTmp[1] = pTmp[0];
		pTmp[0] = oy;

	} else {
		pTmp = pRefbu->oy;
		pTmp[2] = pTmp[1];
		pTmp[1] = pTmp[0];
		pTmp[0] = 0;
	}

	/*Now free the allocated buffer*/
	OSDEV_Free(distrVer);
}

/*------------------------------------------------------------------------------
    Function name : BuildDistribution
    Description   :

    Return type   :
    Argument      :
------------------------------------------------------------------------------*/
void BuildDistribution(unsigned int *pDistrVer,
                       unsigned int *pMv, signed int frmSizeInMbs,
                       unsigned int mvsPerMb,
                       unsigned int bigEndian,
                       signed int *minY, signed int *maxY)
{
	signed int mb;
	signed int ver;
	unsigned char *pMvTmp;
	unsigned int mvs;
	unsigned int skipMv = mvsPerMb * 4;
	unsigned int multiplier = 4;
	unsigned int div = 2;

	mvs = frmSizeInMbs;
	/* Try to keep total n:o of mvs checked under MAX_DIRECT_MVS */
	if (mvs > MAX_DIRECT_MVS) {
		while (mvs / div > MAX_DIRECT_MVS) {
			div++;
		}

		mvs /= div;
		skipMv *= div;
		multiplier *= div;
	}

	pMvTmp = (unsigned char *)pMv;
	if (bigEndian) {
		for (mb = 0 ; mb < mvs ; ++mb) {
			{
				ver         = DIR_MV_BE_VER(pMvTmp);
				SIGN_EXTEND(ver, 13);
				/* Cut fraction and saturate */
				/*lint -save -e702 */
				ver >>= 2;
				/*lint -restore */
				if (ver >= VER_DISTR_MIN && ver <= VER_DISTR_MAX) {
					pDistrVer[ver] += multiplier;
					if (ver < *minY) { *minY = ver; }
					if (ver > *maxY) { *maxY = ver; }
				}
			}
			pMvTmp += skipMv; /* Skip all other blocks for macroblock */
		}
	} else {
		for (mb = 0 ; mb < mvs ; ++mb) {
			{
				ver         = DIR_MV_LE_VER(pMvTmp);
				SIGN_EXTEND(ver, 13);
				/* Cut fraction and saturate */
				/*lint -save -e702 */
				ver >>= 2;
				/*lint -restore */
				if (ver >= VER_DISTR_MIN && ver <= VER_DISTR_MAX) {
					pDistrVer[ver] += multiplier;
					if (ver < *minY) { *minY = ver; }
					if (ver > *maxY) { *maxY = ver; }
				}
			}
			pMvTmp += skipMv; /* Skip all other blocks for macroblock */
		}
	}
}
void vp8HwdAsicBufUpdate(VP8DecInst decInst, VP8DecInput *pInput)
{
	VP8DecContainer_t *pDecCont = (VP8DecContainer_t *) decInst;
	DecAsicBuffers_t *pAsicBuff;

	pAsicBuff = pDecCont->asicBuff;

	pAsicBuff->alternateBuffer.busAddress = pInput->DataBuffers.AltpicBufferBusAddressY;
//    pAsicBuff->alternateBuffer->size =
	pAsicBuff->alternateBuffer.virtualAddress = pInput->DataBuffers.pAltPicBufferY;

	pAsicBuff->goldenBuffer.busAddress = pInput->DataBuffers.GoldpicBufferBusAddressY;
//    pAsicBuff->goldenBuffer->size =
	pAsicBuff->goldenBuffer.virtualAddress = pInput->DataBuffers.pGoldPicBufferY;

	pAsicBuff->prevOutBuffer.busAddress = pInput->DataBuffers.LastpicBufferBusAddressY;
//    pAsicBuff->prevOutBuffer->size =
	pAsicBuff->prevOutBuffer.virtualAddress = pInput->DataBuffers.pLastPicBufferY;

	pAsicBuff->refBuffer.busAddress = pInput->DataBuffers.LastpicBufferBusAddressY;
//    pAsicBuff->prevOutBuffer->size =
	pAsicBuff->refBuffer.virtualAddress = pInput->DataBuffers.pLastPicBufferY;

	pAsicBuff->outBuffer.busAddress = pInput->DataBuffers.picBufferBusAddressY;
//    pAsicBuff->outBuffer->size =
	pAsicBuff->outBuffer.virtualAddress = pInput->DataBuffers.pPicBufferY;

}

