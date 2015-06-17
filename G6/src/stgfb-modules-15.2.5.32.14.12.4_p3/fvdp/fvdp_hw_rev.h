/***********************************************************************
 *
 * File: fvdp/fvdp_hw_rev.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_HW_REV_H
#define FVDP_HW_REV_H

/* CHIP FEATURE DEFINES------------------------------------------------------ */

//
// HW revision
//
#define REV_NONE                    0       // HW does not exist
#define REV_1_0                     10
#define REV_2_0                     20
#define REV_2_1                     21
#define REV_3_0                     30
#define REV_3_1                     31
#define REV_4_0                     40
#define REV_5_0                     50
#define REV_6_0                     60

//
// The FVDP HW version is based on the SOC that is compiled.
//
#if defined(CONFIG_MPE42)
#define FVDP_HW_VERSION                 REV_4_0
#endif

//
// FVDP_ENC
//
#if (FVDP_HW_VERSION == REV_4_0)
    #define ENC_HW_REV                  REV_1_0
#else
    #define ENC_HW_REV                  REV_NONE
#endif

//
// ISM
//
#if (FVDP_HW_VERSION == REV_3_0)
    #define ISM_HW_REV                  REV_3_0
#elif (FVDP_HW_VERSION == REV_4_0)
    #define ISM_HW_REV                  REV_4_0
#else
    #define ISM_HW_REV                  REV_NONE
#endif

//
// MCTI
//
#if (FVDP_HW_VERSION == REV_3_0)
    #define MCTI_HW_REV                 REV_6_0
#else
    #define MCTI_HW_REV                 REV_NONE
#endif

//
// DBLK
//
#if (FVDP_HW_VERSION == REV_3_0)
    #define DBLK_HW_REV                 REV_1_0
#else
    #define DBLK_HW_REV                 REV_NONE
#endif

//
// MSQNR
//
#if (FVDP_HW_VERSION == REV_3_0)
    #define MNR_HW_REV                  REV_1_0
#else
    #define MNR_HW_REV                  REV_NONE
#endif

#endif /* FVDP_HW_REV_H */
