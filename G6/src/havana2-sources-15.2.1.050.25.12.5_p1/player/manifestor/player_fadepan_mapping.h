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
#ifndef H_PLAYER_FADEPAN_MAPPING
#define H_PLAYER_FADEPAN_MAPPING

static const unsigned short Pan15deg[16][2] =
{
    {   0, 8191}, /* Angle =   0 */
    { 276, 8186}, /* Angle =   1 */
    { 572, 8171}, /* Angle =   2 */
    { 888, 8143}, /* Angle =   3 */
    {1225, 8099}, /* Angle =   4 */
    {1583, 8037}, /* Angle =   5 */
    {1961, 7953}, /* Angle =   6 */
    {2358, 7844}, /* Angle =   7 */
    {2773, 7707}, /* Angle =   8 */
    {3202, 7539}, /* Angle =   9 */
    {3641, 7337}, /* Angle =  10 */
    {4085, 7100}, /* Angle =  11 */
    {4529, 6825}, /* Angle =  12 */
    {4966, 6514}, /* Angle =  13 */
    {5389, 6168}, /* Angle =  14 */
    {5792, 5792}, /* Angle =  15 */
};

static const unsigned short Pan30deg[31][2] =
{
    {5792, 5792}, /* Angle =   0 */
    {5586, 5990}, /* Angle =   1 */
    {5375, 6181}, /* Angle =   2 */
    {5157, 6363}, /* Angle =   3 */
    {4936, 6537}, /* Angle =   4 */
    {4711, 6700}, /* Angle =   5 */
    {4484, 6855}, /* Angle =   6 */
    {4256, 6999}, /* Angle =   7 */
    {4027, 7133}, /* Angle =   8 */
    {3798, 7257}, /* Angle =   9 */
    {3571, 7372}, /* Angle =  10 */
    {3346, 7476}, /* Angle =  11 */
    {3124, 7572}, /* Angle =  12 */
    {2906, 7658}, /* Angle =  13 */
    {2691, 7736}, /* Angle =  14 */
    {2481, 7806}, /* Angle =  15 */
    {2276, 7868}, /* Angle =  16 */
    {2076, 7923}, /* Angle =  17 */
    {1882, 7972}, /* Angle =  18 */
    {1693, 8014}, /* Angle =  19 */
    {1510, 8051}, /* Angle =  20 */
    {1333, 8082}, /* Angle =  21 */
    {1162, 8108}, /* Angle =  22 */
    { 997, 8130}, /* Angle =  23 */
    { 838, 8148}, /* Angle =  24 */
    { 685, 8162}, /* Angle =  25 */
    { 537, 8173}, /* Angle =  26 */
    { 395, 8181}, /* Angle =  27 */
    { 258, 8187}, /* Angle =  28 */
    { 126, 8190}, /* Angle =  29 */
    {   0, 8191}, /* Angle =  30 */
};

static const unsigned short Pan40deg[41][2] =
{
    {8191,    0}, /* Angle =  30 */
    {8191,   87}, /* Angle =  31 */
    {8189,  177}, /* Angle =  32 */
    {8187,  270}, /* Angle =  33 */
    {8183,  366}, /* Angle =  34 */
    {8178,  465}, /* Angle =  35 */
    {8171,  568}, /* Angle =  36 */
    {8163,  675}, /* Angle =  37 */
    {8153,  785}, /* Angle =  38 */
    {8142,  898}, /* Angle =  39 */
    {8128, 1016}, /* Angle =  40 */
    {8112, 1136}, /* Angle =  41 */
    {8093, 1261}, /* Angle =  42 */
    {8072, 1390}, /* Angle =  43 */
    {8048, 1522}, /* Angle =  44 */
    {8022, 1658}, /* Angle =  45 */
    {7991, 1797}, /* Angle =  46 */
    {7958, 1941}, /* Angle =  47 */
    {7920, 2088}, /* Angle =  48 */
    {7879, 2238}, /* Angle =  49 */
    {7834, 2392}, /* Angle =  50 */
    {7784, 2550}, /* Angle =  51 */
    {7730, 2711}, /* Angle =  52 */
    {7670, 2874}, /* Angle =  53 */
    {7606, 3040}, /* Angle =  54 */
    {7536, 3209}, /* Angle =  55 */
    {7461, 3381}, /* Angle =  56 */
    {7380, 3554}, /* Angle =  57 */
    {7293, 3728}, /* Angle =  58 */
    {7201, 3904}, /* Angle =  59 */
    {7102, 4081}, /* Angle =  60 */
    {6997, 4258}, /* Angle =  61 */
    {6886, 4435}, /* Angle =  62 */
    {6769, 4612}, /* Angle =  63 */
    {6646, 4787}, /* Angle =  64 */
    {6518, 4961}, /* Angle =  65 */
    {6383, 5133}, /* Angle =  66 */
    {6243, 5303}, /* Angle =  67 */
    {6097, 5469}, /* Angle =  68 */
    {5947, 5633}, /* Angle =  69 */
    {5792, 5792}, /* Angle =  70 */
};

static const unsigned short Pan70deg[70][2] =
{
    {  27, 8191}, /* Angle = 111 */
    {  55, 8191}, /* Angle = 112 */
    {  84, 8191}, /* Angle = 113 */
    { 116, 8190}, /* Angle = 114 */
    { 148, 8190}, /* Angle = 115 */
    { 182, 8189}, /* Angle = 116 */
    { 218, 8188}, /* Angle = 117 */
    { 255, 8187}, /* Angle = 118 */
    { 294, 8186}, /* Angle = 119 */
    { 334, 8184}, /* Angle = 120 */
    { 376, 8182}, /* Angle = 121 */
    { 419, 8180}, /* Angle = 122 */
    { 465, 8178}, /* Angle = 123 */
    { 511, 8175}, /* Angle = 124 */
    { 560, 8172}, /* Angle = 125 */
    { 610, 8168}, /* Angle = 126 */
    { 662, 8164}, /* Angle = 127 */
    { 716, 8160}, /* Angle = 128 */
    { 772, 8155}, /* Angle = 129 */
    { 830, 8149}, /* Angle = 130 */
    { 889, 8143}, /* Angle = 131 */
    { 950, 8136}, /* Angle = 132 */
    {1013, 8128}, /* Angle = 133 */
    {1078, 8120}, /* Angle = 134 */
    {1145, 8111}, /* Angle = 135 */
    {1214, 8100}, /* Angle = 136 */
    {1285, 8090}, /* Angle = 137 */
    {1358, 8078}, /* Angle = 138 */
    {1433, 8065}, /* Angle = 139 */
    {1510, 8051}, /* Angle = 140 */
    {1589, 8035}, /* Angle = 141 */
    {1671, 8019}, /* Angle = 142 */
    {1754, 8001}, /* Angle = 143 */
    {1839, 7982}, /* Angle = 144 */
    {1926, 7961}, /* Angle = 145 */
    {2015, 7939}, /* Angle = 146 */
    {2107, 7915}, /* Angle = 147 */
    {2200, 7890}, /* Angle = 148 */
    {2295, 7863}, /* Angle = 149 */
    {2392, 7834}, /* Angle = 150 */
    {2492, 7803}, /* Angle = 151 */
    {2593, 7770}, /* Angle = 152 */
    {2696, 7735}, /* Angle = 153 */
    {2800, 7697}, /* Angle = 154 */
    {2907, 7658}, /* Angle = 155 */
    {3015, 7616}, /* Angle = 156 */
    {3124, 7572}, /* Angle = 157 */
    {3235, 7525}, /* Angle = 158 */
    {3348, 7476}, /* Angle = 159 */
    {3462, 7424}, /* Angle = 160 */
    {3577, 7369}, /* Angle = 161 */
    {3693, 7311}, /* Angle = 162 */
    {3810, 7251}, /* Angle = 163 */
    {3928, 7188}, /* Angle = 164 */
    {4046, 7122}, /* Angle = 165 */
    {4165, 7053}, /* Angle = 166 */
    {4284, 6981}, /* Angle = 167 */
    {4404, 6906}, /* Angle = 168 */
    {4524, 6829}, /* Angle = 169 */
    {4643, 6748}, /* Angle = 170 */
    {4762, 6664}, /* Angle = 171 */
    {4881, 6578}, /* Angle = 172 */
    {4999, 6489}, /* Angle = 173 */
    {5116, 6397}, /* Angle = 174 */
    {5232, 6302}, /* Angle = 175 */
    {5347, 6205}, /* Angle = 176 */
    {5461, 6105}, /* Angle = 177 */
    {5573, 6003}, /* Angle = 178 */
    {5683, 5898}, /* Angle = 179 */
    {5792, 5792}, /* Angle = 180 */
};

void ConvertAngle2PanCoef(unsigned short PanAngle, unsigned short PanCoef[8], int Front, int Surround)
{
    memset(PanCoef, 0, 8 * sizeof(unsigned short));

    if (PanAngle <= 30)
    {
        if (Front == 2)
        {
            PanCoef[ACC_MAIN_LEFT] = Pan30deg[PanAngle][0];
            PanCoef[ACC_MAIN_RGHT] = Pan30deg[PanAngle][1];
        }
        else
        {
            if (PanAngle <= 15)
            {
                PanCoef[ACC_MAIN_RGHT] = Pan15deg[PanAngle][0];
                PanCoef[ACC_MAIN_CNTR] = Pan15deg[PanAngle][1];
            }
            else
            {
                PanCoef[ACC_MAIN_RGHT] = Pan15deg[30 - PanAngle][1];
                PanCoef[ACC_MAIN_CNTR] = Pan15deg[30 - PanAngle][0];
            }
        }
    }
    else if (PanAngle <= 330)
    {
        if (Surround)
        {
            if (PanAngle <= 70)
            {
                PanCoef[ACC_MAIN_RGHT] = Pan40deg[PanAngle - 30][0];
                PanCoef[ACC_MAIN_RSUR] = Pan40deg[PanAngle - 30][1];
            }
            else if (PanAngle <= 110)
            {
                PanCoef[ACC_MAIN_RGHT] = Pan40deg[110 - PanAngle][1];
                PanCoef[ACC_MAIN_RSUR] = Pan40deg[110 - PanAngle][0];
            }
            else if (PanAngle <= 180)
            {
                PanCoef[ACC_MAIN_LSUR] = Pan70deg[PanAngle - 111][0];
                PanCoef[ACC_MAIN_RSUR] = Pan70deg[PanAngle - 111][1];
            }
            else if (PanAngle < 250)
            {
                PanCoef[ACC_MAIN_LSUR] = Pan70deg[249 - PanAngle][1];
                PanCoef[ACC_MAIN_RSUR] = Pan70deg[249 - PanAngle][0];
            }
            else if (PanAngle < 290)
            {
                PanCoef[ACC_MAIN_LEFT] = Pan40deg[PanAngle - 250][1];
                PanCoef[ACC_MAIN_LSUR] = Pan40deg[PanAngle - 250][0];
            }
            else
            {
                PanCoef[ACC_MAIN_LEFT] = Pan40deg[330 - PanAngle][0];
                PanCoef[ACC_MAIN_LSUR] = Pan40deg[330 - PanAngle][1];
            }
        }
        else
        {
            if (PanAngle <= 150)
            {
                PanCoef[ACC_MAIN_LEFT] = Pan30deg[30][0];
                PanCoef[ACC_MAIN_RGHT] = Pan30deg[30][1];
            }
            else if (PanAngle < 180)
            {
                PanCoef[ACC_MAIN_LEFT] = Pan30deg[180 - PanAngle][0];
                PanCoef[ACC_MAIN_RGHT] = Pan30deg[180 - PanAngle][1];
            }
            else if (PanAngle < 210)
            {
                PanCoef[ACC_MAIN_LEFT] = Pan30deg[PanAngle - 180][1];
                PanCoef[ACC_MAIN_RGHT] = Pan30deg[PanAngle - 180][0];
            }
            else
            {
                PanCoef[ACC_MAIN_LEFT] = Pan30deg[30][1];
                PanCoef[ACC_MAIN_RGHT] = Pan30deg[30][0];
            }
        }
    }
    else /* if ( PanAngle < 360 ) */
    {
        if (Front == 2)
        {
            PanCoef[ACC_MAIN_LEFT] = Pan30deg[360 - PanAngle][1];
            PanCoef[ACC_MAIN_RGHT] = Pan30deg[360 - PanAngle][0];
        }
        else
        {
            if (PanAngle < 345)
            {
                PanCoef[ACC_MAIN_LEFT] = Pan15deg[PanAngle - 330][1];
                PanCoef[ACC_MAIN_CNTR] = Pan15deg[PanAngle - 330][0];
            }
            else
            {
                PanCoef[ACC_MAIN_LEFT] = Pan15deg[360 - PanAngle][0];
                PanCoef[ACC_MAIN_CNTR] = Pan15deg[360 - PanAngle][1];
            }
        }
    }
}

void Gen2chPanCoef(unsigned char PanByte, unsigned short PanCoef[8])
{
    unsigned int PanAngle = (int)(((unsigned int) PanByte * 360 + (256 - 1)) / 256);  // Angle in Degrees
    ConvertAngle2PanCoef(PanAngle, PanCoef, 2, 0);
}

void Gen4chPanCoef(unsigned char PanByte, unsigned short PanCoef[8])
{
    unsigned int PanAngle = (int)(((unsigned int) PanByte * 360 + (256 - 1)) / 256);  // Angle in Degrees
    ConvertAngle2PanCoef(PanAngle, PanCoef, 3, 0);
}

void Gen6chPanCoef(unsigned char PanByte, unsigned short PanCoef[8])
{
    unsigned int PanAngle = (int)(((unsigned int) PanByte * 360 + (256 - 1)) / 256);  // Angle in Degrees
    ConvertAngle2PanCoef(PanAngle, PanCoef, 3, 2);
}


// Fade Value varies from 0x00 to 0xFF
// 0x00 --> 0dB attenuation on main
// 0xFF --> mute of main
// 0x01 step implies an step of 0.3 dB
// Mapping done for storing in Q3_13 format with 0x00 --> 0x1FFF
// Index of array maps to Fade Value and content maps to Alpha Level of main

unsigned short Fade_Mapping[256] =
{
    32767 >> 2,
    31656 >> 2,
    30581 >> 2,
    29543 >> 2,
    28540 >> 2,
    27571 >> 2,
    26635 >> 2,
    25731 >> 2,
    24857 >> 2,
    24013 >> 2,
    23198 >> 2,
    22410 >> 2,
    21650 >> 2,
    20915 >> 2,
    20205 >> 2,
    19519 >> 2,
    18856 >> 2,
    18216 >> 2,
    17597 >> 2,
    17000 >> 2,
    16423 >> 2,
    15865 >> 2,
    15327 >> 2,
    14806 >> 2,
    14304 >> 2,
    13818 >> 2,
    13349 >> 2,
    12896 >> 2,
    12458 >> 2,
    12035 >> 2,
    11627 >> 2,
    11232 >> 2,
    10851 >> 2,
    10482 >> 2,
    10126 >> 2,
    9783 >> 2,
    9450 >> 2,
    9130 >> 2,
    8820 >> 2,
    8520 >> 2,
    8231 >> 2,
    7952 >> 2,
    7682 >> 2,
    7421 >> 2,
    7169 >> 2,
    6925 >> 2,
    6690 >> 2,
    6463 >> 2,
    6244 >> 2,
    6032 >> 2,
    5827 >> 2,
    5629 >> 2,
    5438 >> 2,
    5254 >> 2,
    5075 >> 2,
    4903 >> 2,
    4736 >> 2,
    4576 >> 2,
    4420 >> 2,
    4270 >> 2,
    4125 >> 2,
    3985 >> 2,
    3850 >> 2,
    3719 >> 2,
    3593 >> 2,
    3471 >> 2,
    3353 >> 2,
    3239 >> 2,
    3129 >> 2,
    3023 >> 2,
    2920 >> 2,
    2821 >> 2,
    2726 >> 2,
    2633 >> 2,
    2544 >> 2,
    2457 >> 2,
    2374 >> 2,
    2293 >> 2,
    2215 >> 2,
    2140 >> 2,
    2068 >> 2,
    1997 >> 2,
    1930 >> 2,
    1864 >> 2,
    1801 >> 2,
    1740 >> 2,
    1681 >> 2,
    1623 >> 2,
    1568 >> 2,
    1515 >> 2,
    1464 >> 2,
    1414 >> 2,
    1366 >> 2,
    1320 >> 2,
    1275 >> 2,
    1232 >> 2,
    1190 >> 2,
    1149 >> 2,
    1110 >> 2,
    1073 >> 2,
    1036 >> 2,
    1001 >> 2,
    967 >> 2,
    934 >> 2,
    903 >> 2,
    872 >> 2,
    842 >> 2,
    814 >> 2,
    786 >> 2,
    759 >> 2,
    734 >> 2,
    709 >> 2,
    685 >> 2,
    661 >> 2,
    639 >> 2,
    617 >> 2,
    596 >> 2,
    576 >> 2,
    556 >> 2,
    538 >> 2,
    519 >> 2,
    502 >> 2,
    485 >> 2,
    468 >> 2,
    452 >> 2,
    437 >> 2,
    422 >> 2,
    408 >> 2,
    394 >> 2,
    381 >> 2,
    368 >> 2,
    355 >> 2,
    343 >> 2,
    331 >> 2,
    320 >> 2,
    309 >> 2,
    299 >> 2,
    289 >> 2,
    279 >> 2,
    269 >> 2,
    260 >> 2,
    251 >> 2,
    243 >> 2,
    235 >> 2,
    227 >> 2,
    219 >> 2,
    212 >> 2,
    204 >> 2,
    197 >> 2,
    191 >> 2,
    184 >> 2,
    178 >> 2,
    172 >> 2,
    166 >> 2,
    160 >> 2,
    155 >> 2,
    150 >> 2,
    145 >> 2,
    140 >> 2,
    135 >> 2,
    130 >> 2,
    126 >> 2,
    122 >> 2,
    118 >> 2,
    114 >> 2,
    110 >> 2,
    106 >> 2,
    102 >> 2,
    99  >> 2,
    96  >> 2,
    92  >> 2,
    89  >> 2,
    86 >> 2,
    83 >> 2,
    80 >> 2,
    78 >> 2,
    75 >> 2,
    73 >> 2,
    70 >> 2,
    68 >> 2,
    65 >> 2,
    63 >> 2,
    61 >> 2,
    59 >> 2,
    57 >> 2,
    55 >> 2,
    53 >> 2,
    51 >> 2,
    50 >> 2,
    48 >> 2,
    46 >> 2,
    45 >> 2,
    43 >> 2,
    42 >> 2,
    40 >> 2,
    39 >> 2,
    38 >> 2,
    36 >> 2,
    35 >> 2,
    34 >> 2,
    33 >> 2,
    32 >> 2,
    31 >> 2,
    30 >> 2,
    29 >> 2,
    28 >> 2,
    27 >> 2,
    26 >> 2,
    25 >> 2,
    24 >> 2,
    23 >> 2,
    22 >> 2,
    22 >> 2,
    21 >> 2,
    20 >> 2,
    20 >> 2,
    19 >> 2,
    18 >> 2,
    18 >> 2,
    17 >> 2,
    16 >> 2,
    16 >> 2,
    15 >> 2,
    15 >> 2,
    14 >> 2,
    14 >> 2,
    13 >> 2,
    13 >> 2,
    12 >> 2,
    12 >> 2,
    12 >> 2,
    11 >> 2,
    11 >> 2,
    10 >> 2,
    10 >> 2,
    10 >> 2,
    9 >> 2,
    9 >> 2,
    9 >> 2,
    9 >> 2,
    8 >> 2,
    8 >> 2,
    8 >> 2,
    7 >> 2,
    7 >> 2,
    7 >> 2,
    7 >> 2,
    6 >> 2,
    6 >> 2,
    6 >> 2,
    6 >> 2,
    6 >> 2,
    5 >> 2,
    5 >> 2,
    5 >> 2,
    0
};

#endif

