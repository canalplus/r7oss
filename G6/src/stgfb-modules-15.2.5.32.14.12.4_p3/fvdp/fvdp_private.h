/***********************************************************************
 *
 * File: fvdp/fvdp_private.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_PRIVATE_H
#define FVDP_PRIVATE_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#ifdef FVDP_SIMULATION
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif


/* Exported Constants ------------------------------------------------------- */

// PLATFORM
#define PLATFORM_ARM        0
#define PLATFORM_XP70       1
#define PLATFORM_WIN32      2
#define PLATFORM_LINUX      3

#undef NULL
#define NULL    0

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

/* Exported Types ----------------------------------------------------------- */

/* To be replaced by adding coreddisplay type header once it error in coredisplay is resolved. */
#if defined(_STM_OS21_PLATFORM_H)

#else
/*
  #if defined(FVDP_SIMULATION)
    #define __extension__
  #endif
*/
  #if defined(_SYS_TYPES_H)
    typedef unsigned char  bool;
    typedef u_int32_t      uint32_t;
    typedef u_int16_t      uint16_t;
    typedef u_int8_t       uint8_t;
    typedef u_int64_t      uint64_t;

  #elif !defined(_LINUX_TYPES_H)
    typedef unsigned char  bool;
    typedef int            int32_t;
    typedef short          int16_t;
    typedef char           int8_t;
    typedef unsigned int   uint32_t;
    typedef unsigned short uint16_t;
    typedef unsigned char  uint8_t;
    __extension__ typedef signed long long int64_t;
    __extension__ typedef unsigned long long uint64_t;
  #endif

#endif

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */
#define BIT0            0x00000001
#define BIT1            0x00000002
#define BIT2            0x00000004
#define BIT3            0x00000008
#define BIT4            0x00000010
#define BIT5            0x00000020
#define BIT6            0x00000040
#define BIT7            0x00000080
#define BIT8            0x00000100
#define BIT9            0x00000200
#define BIT10           0x00000400
#define BIT11           0x00000800
#define BIT12           0x00001000
#define BIT13           0x00002000
#define BIT14           0x00004000
#define BIT15           0x00008000
#define BIT16           0x00010000
#define BIT17           0x00020000
#define BIT18           0x00040000
#define BIT19           0x00080000
#define BIT20           0x00100000
#define BIT21           0x00200000
#define BIT22           0x00400000
#define BIT23           0x00800000
#define BIT24           0x01000000
#define BIT25           0x02000000
#define BIT26           0x04000000
#define BIT27           0x08000000
#define BIT28           0x10000000
#define BIT29           0x20000000
#define BIT30           0x40000000
#define BIT31           0x80000000



#if defined(FVDP_SIMULATION) || !defined(SYSTEM_BUILD_VCPU)
#include <stm_display.h>
#include <stm_display_types.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#endif
#include "fvdp.h"

#ifdef SYSTEM_BUILD_VCPU
#define CPU_STRING      "XP70: "
#else
#define CPU_STRING      "ARM : "
#endif

#define DBG_ERROR   1
#define DBG_WARN    2
#define DBG_INFO    3

#define CHECK(test)                                                 \
    if (!(test))                                                    \
    {                                                               \
        TRC( TRC_ID_MAIN_INFO, "assertion " #test " failed" );      \
        return FVDP_ERROR;                                          \
    }

#define TraceOn( level, ... )                                   \
  do {                                                          \
    switch ( level ) {                                          \
    case DBG_ERROR :                                            \
      /* fallthrough */                                         \
    case DBG_WARN :                                             \
      TRC( TRC_ID_ERROR, CPU_STRING __VA_ARGS__ );              \
      break;                                                    \
    case DBG_INFO :                                             \
      TRC( TRC_ID_MAIN_INFO, CPU_STRING __VA_ARGS__ );          \
      break;                                                    \
    default :                                                   \
      TRC( TRC_ID_UNCLASSIFIED, CPU_STRING __VA_ARGS__ );       \
      break;                                                    \
    }                                                           \
  } while ( 0 )

#define TraceModuleOn( level, thresh, ... )     \
  do {                                          \
    if ( level <= thresh ) {                    \
      TraceOn( level, __VA_ARGS__ );            \
    }                                           \
  } while ( 0 )

/* Exported Functions ------------------------------------------------------- */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_PRIVATE_H */

/* end of file */
