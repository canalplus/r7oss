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

#ifndef H_DSE
#define H_DSE

/* DSE Registers */
/* HWCFG_IPF_MONOTH */
#define DSE_KS2_MASK                0x07
#define DSE_KS2_EMP                 24
#define DSE_FIELD_ADAPT_ADB_MASK    0x03
#define DSE_FIELD_ADAPT_ADB_EMP     16
#define DSE_QUAN_ADAPT_ADB_MASK     0x01
#define DSE_QUAN_ADAPT_ADB_EMP      14
#define DSE_QUAN_MASK               0x3F
#define DSE_QUAN_EMP                8
#define DSE_KS1_MASK                0xFF
#define DSE_KS1_EMP                 0
/* HWCFG_IPF_ADRTH */
#define DSE_TT3_MASK                0xFF
#define DSE_TT3_EMP                 24
#define DSE_TT2_MASK                0xFF
#define DSE_TT2_EMP                 16
#define DSE_TT1_MASK                0xFF
#define DSE_TT1_EMP                 8
#define DSE_TM_MASK                 0xFF
#define DSE_TM_EMP                  0
/* HWCFG_IPF_ADRC */
#define DSE_FIELD_ADAPT_ADR_MASK    0x07
#define DSE_FIELD_ADAPT_ADR_EMP     16
#define DSE_TMIN_MASK               0xFF
#define DSE_TMIN_EMP                8
#define DSE_CURVE_MASK              0x0F
#define DSE_CURVE_EMP               1
#define DSE_CURVE_ADAPT_MASK        0x01
#define DSE_CURVE_ADAPT_EMP         0
/* HWCFG_IPF_ADRID */
#define DSE_TI3_MASK                0xFF
#define DSE_TI3_EMP                 0
/* HWCFG_IPF_ADRTQINTRA */
#define DSE_TQ2_INTRA_MASK          0x7F
#define DSE_TQ2_INTRA_EMP           16
#define DSE_TQ1_INTRA_MASK          0x7F
#define DSE_TQ1_INTRA_EMP           8
#define DSE_TQ0_INTRA_MASK          0x7F
#define DSE_TQ0_INTRA_EMP           0
/* HWCFG_IPF_ADRTQINTER */
#define DSE_TQ2_INTER_MASK          0x7F
#define DSE_TQ2_INTER_EMP           16
#define DSE_TQ1_INTER_MASK          0x7F
#define DSE_TQ1_INTER_EMP           8
#define DSE_TQ0_INTER_MASK          0x7F
#define DSE_TQ0_INTER_EMP           0



typedef struct
{
    bool IPF_DeringingLumaOnly;
    bool IPF_DeblockingLumaOnly;
    U32 IPF_MONOTH;
    U32 IPF_ADRTH;
    U32 IPF_ADRC;
    U32 IPF_ADRID;
    U32 IPF_ADRTQ_INTRA;
    U32 IPF_ADRTQ_INTER;
} DSE_PostProDebug_t;

struct
{
    int32_t             level;
    DSE_PostProDebug_t  DSE_PostProDebug;
} DSE_PostProDebugTable_t;

/*
 * Post Processing DSE - DEBLOCKING, DERINGING configuration
 * according 3 levels (LOW - MID - HIGH)
 */
static const DSE_PostProDebug_t DSE_PostProDebug_Level[3] =
{
    {
        42,
        {    /* LOW */
            false,
            false,
            ((3     & DSE_KS2_MASK) << DSE_KS2_EMP) |
            ((2     & DSE_FIELD_ADAPT_ADB_MASK) << DSE_FIELD_ADAPT_ADB_EMP) |
            ((1     & DSE_QUAN_ADAPT_ADB_MASK) << DSE_QUAN_ADAPT_ADB_EMP) |
            ((16    & DSE_QUAN_MASK) << DSE_QUAN_EMP) |
            ((0     & DSE_KS1_MASK) << DSE_KS1_EMP),

            /* Disable monotone detector and texture analyzer */
            ((8     & DSE_TT3_MASK) << DSE_TT3_EMP) |
            ((4     & DSE_TT2_MASK) << DSE_TT2_EMP) |
            ((2     & DSE_TT1_MASK) << DSE_TT1_EMP) |
            ((32    & DSE_TM_MASK) << DSE_TM_EMP),

            ((3     & DSE_FIELD_ADAPT_ADR_MASK) << DSE_FIELD_ADAPT_ADR_EMP) |
            ((8     & DSE_TMIN_MASK) << DSE_TMIN_EMP) |
            ((0     & DSE_CURVE_MASK) << DSE_CURVE_EMP) |
            ((1     & DSE_CURVE_ADAPT_MASK) << DSE_CURVE_ADAPT_EMP),

            ((110   & DSE_TI3_MASK) << DSE_TI3_EMP),

            ((48    & DSE_TQ2_INTRA_MASK) << DSE_TQ2_INTRA_EMP) |
            ((30    & DSE_TQ1_INTRA_MASK) << DSE_TQ1_INTRA_EMP) |
            ((12    & DSE_TQ0_INTRA_MASK) << DSE_TQ0_INTRA_EMP),

            ((96    & DSE_TQ2_INTER_MASK) << DSE_TQ2_INTER_EMP) |
            ((60    & DSE_TQ1_INTER_MASK) << DSE_TQ1_INTER_EMP) |
            ((24    & DSE_TQ0_INTER_MASK) << DSE_TQ0_INTER_EMP)
        },
    },
    {
        85,
        {    /* MID */
            false,
            false,
            ((3     & DSE_KS2_MASK) << DSE_KS2_EMP) |
            ((2     & DSE_FIELD_ADAPT_ADB_MASK) << DSE_FIELD_ADAPT_ADB_EMP) |
            ((1     & DSE_QUAN_ADAPT_ADB_MASK) << DSE_QUAN_ADAPT_ADB_EMP) |
            ((16    & DSE_QUAN_MASK) << DSE_QUAN_EMP) |
            ((1     & DSE_KS1_MASK) << DSE_KS1_EMP),

            /* Disable monotone detector and texture analyzer */
            ((8     & DSE_TT3_MASK) << DSE_TT3_EMP) |
            ((4     & DSE_TT2_MASK) << DSE_TT2_EMP) |
            ((2     & DSE_TT1_MASK) << DSE_TT1_EMP) |
            ((32    & DSE_TM_MASK) << DSE_TM_EMP),

            ((3     & DSE_FIELD_ADAPT_ADR_MASK) << DSE_FIELD_ADAPT_ADR_EMP) |
            ((16    & DSE_TMIN_MASK) << DSE_TMIN_EMP) |
            ((0     & DSE_CURVE_MASK) << DSE_CURVE_EMP) |
            ((1     & DSE_CURVE_ADAPT_MASK) << DSE_CURVE_ADAPT_EMP),

            ((110   & DSE_TI3_MASK) << DSE_TI3_EMP),

            ((40    & DSE_TQ2_INTRA_MASK) << DSE_TQ2_INTRA_EMP) |
            ((24    & DSE_TQ1_INTRA_MASK) << DSE_TQ1_INTRA_EMP) |
            ((8     & DSE_TQ0_INTRA_MASK) << DSE_TQ0_INTRA_EMP),

            ((80    & DSE_TQ2_INTER_MASK) << DSE_TQ2_INTER_EMP) |
            ((48    & DSE_TQ1_INTER_MASK) << DSE_TQ1_INTER_EMP) |
            ((16    & DSE_TQ0_INTER_MASK) << DSE_TQ0_INTER_EMP)
        }
    },
    {
        128,
        {    /* HIGH */
            false,
            false,
            ((3     & DSE_KS2_MASK) << DSE_KS2_EMP) |
            ((2     & DSE_FIELD_ADAPT_ADB_MASK) << DSE_FIELD_ADAPT_ADB_EMP) |
            ((1     & DSE_QUAN_ADAPT_ADB_MASK) << DSE_QUAN_ADAPT_ADB_EMP) |
            ((16    & DSE_QUAN_MASK) << DSE_QUAN_EMP) |
            ((2     & DSE_KS1_MASK) << DSE_KS1_EMP),

            /* Disable monotone detector and texture analyzer */
            ((8     & DSE_TT3_MASK) << DSE_TT3_EMP) |
            ((4     & DSE_TT2_MASK) << DSE_TT2_EMP) |
            ((2     & DSE_TT1_MASK) << DSE_TT1_EMP) |
            ((32    & DSE_TM_MASK) << DSE_TM_EMP),

            ((3     & DSE_FIELD_ADAPT_ADR_MASK) << DSE_FIELD_ADAPT_ADR_EMP) |
            ((32    & DSE_TMIN_MASK) << DSE_TMIN_EMP) |
            ((0     & DSE_CURVE_MASK) << DSE_CURVE_EMP) |
            ((1     & DSE_CURVE_ADAPT_MASK) << DSE_CURVE_ADAPT_EMP),

            ((110   & DSE_TI3_MASK) << DSE_TI3_EMP),

            ((32    & DSE_TQ2_INTRA_MASK) << DSE_TQ2_INTRA_EMP) |
            ((18    & DSE_TQ1_INTRA_MASK) << DSE_TQ1_INTRA_EMP) |
            ((4     & DSE_TQ0_INTRA_MASK) << DSE_TQ0_INTRA_EMP),

            ((64    & DSE_TQ2_INTER_MASK) << DSE_TQ2_INTER_EMP) |
            ((36    & DSE_TQ1_INTER_MASK) << DSE_TQ1_INTER_EMP) |
            ((8     & DSE_TQ0_INTER_MASK) << DSE_TQ0_INTER_EMP)
        }
    }
};

#endif /* H_DSE */
