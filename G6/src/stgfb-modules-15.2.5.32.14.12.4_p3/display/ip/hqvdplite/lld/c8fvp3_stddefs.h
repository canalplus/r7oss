/******************************************************************************

File Name   : c8fvp3_stddefs.h

Description : Contains a number of generic type declarations and defines.

Copyright (C) 1999 STMicroelectronics

******************************************************************************/

/* Define to prevent recursive inclusion */
#ifndef _HQVDPLITE_STDDEFS_H
#define _HQVDPLITE_STDDEFS_H


/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------------------- */
#ifndef HQVDPLITE_API_FOR_STAPI
  #define MIN(a, b)   ((a) <= (b) ? (a) : (b))
  #define MAX(a, b)   ((a) <= (b) ? (b) : (a))
  #if defined (FW_STXP70) && !defined (TLM_NATIVE)
    #include "global_verif_hal_stddefs.h"
    #define yield()
  #else                /* #if defined (TLM_NATIVE) || defined (NATIVE_CORE) || defined (NCSIM_RTL)  Add NATIVE_CORE and NCSIM_RTL flags for SOC verif*/
    #include "global_verif_services.h"
    #ifdef FW_STXP70
      #include "eswnative_base_esw.h"
      #define yield() esw_sync((unsigned int)1)
    #else
      #define yield() global_verif_hal.delay((unsigned int)1)
    #endif /* #ifdef FW_STXP70 */
  #endif /* #if defined (FW_STXP70) && !defined (TLM_NATIVE) */
#else
  #ifndef __cplusplus
    /*
     * We cannot include <linux/types.h> here because its definition
     * of bool clashes with C++.
    */
    #if !defined(_STM_OS21_PLATFORM_H)
      typedef unsigned char  bool;
      typedef int            int32_t;
      typedef short          int16_t;
      #if (!defined (HQVDPLITE_API_FOR_STAPI_SANITY_CHECK))
        typedef char           int8_t;
      #endif
      typedef unsigned int   uint32_t;
      typedef unsigned short uint16_t;
      typedef unsigned char  uint8_t;
      __extension__ typedef unsigned long long uint64_t;

      /*
       * Some good old style C definitions for where we have imported code
       * from elsewhere.
      */
      #define FALSE (1==2)
      #define TRUE  (!FALSE)
      #undef NULL
      #if defined(__cplusplus)
        #define NULL 0
      #else
        #define NULL ((void *)0)
      #endif
    #endif  /* !defined(_STM_OS21_PLATFORM_H) */
  #endif /* #ifndef __cplusplus */

  #include <vibe_os.h>

  #define U32  uint32_t
  #define U16  uint16_t
  #define U8   uint8_t
  #define BOOL bool
  #define S32  int32_t
  #define S16  int16_t
  #define S8   int8_t

  #define gvh_u8_t   U8
  #define gvh_u16_t  U16
  #define gvh_u32_t  U32
  #define gvh_s8_t   S8
  #define gvh_s16_t  S16
  #define gvh_s32_t  S32
  #define gvh_bool_t BOOL

  #define WriteRegister(addr, val) vibe_os_write_register((volatile uint32_t *)(addr), 0, val)
  #define ReadRegister(addr)  (uint32_t)vibe_os_read_register((volatile uint32_t *)(addr), 0)

  #ifdef  HQVDPLITE_API_FOR_STAPI_SANITY_CHECK
    /** \todo While in sanity check no eswnative_base_esw.h is available. don't check yield */
    #define yield()
  #else
    #ifdef VIRTUAL_PLATFORM_TLM
      #include "eswnative_base_esw.h"
      #define yield() esw_sync((unsigned int)1)
    #else
      #define yield()
    #endif /* #ifdef VIRTUAL_PLATFORM_TLM */
  #endif /* #ifdef  HQVDPLITE_API_FOR_STAPI_SANITY_CHECK */
  /* Match typedef use in lld to typedef needed by the driver */
#endif /*#ifndef HQVDPLITE_API_FOR_STAPI*/

/*------------------------------------------------------------------------------
 * Debug mode temporary done here. Therefore, must be move in stxp70_main.h file
 *----------------------------------------------------------------------------*/

#ifdef DEBUG_MODE
  #include <stdio.h>
  #define printf_on_debug printf
#else
  #define printf_on_debug(...) do {} while (0)
#endif

/* Exported Types ---------------------------------------------------------- */


/* Exported Constants ------------------------------------------------------ */

/* To define addresses aligned on 64-bits */
#define ALIGNED_64BITS(addr)    ((((addr) + 7) >> 3) << 3)

/* To define addresses aligned on 512 bytes (for MB alignment) */
#define ALIGNED_512BYTES(addr)    ((((addr) + 511) >> 9) << 9)
/* To define addresses aligned on 16 bytes (for MB alignment) */
#define ALIGNED_16BYTES(addr)    ((((addr) + 15) >> 4) << 4)
/* To define addresses aligned on 32 bytes (for MB alignment) */
#define ALIGNED_32BYTES(addr)    ((((addr) + 31) >> 5) << 5)
/* To define addresses aligned on 64 bytes (for MB alignment) */
#define ALIGNED_64BYTES(addr)    ((((addr) + 63) >> 6) << 6)

/* Exported Types ---------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------ */

/* Exported Macros --------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------ */

#if (!defined (HQVDPLITE_API_FOR_STAPI))
#pragma inline
static void WriteRegister(
    gvh_u32_t Addr,
    gvh_u32_t Value);
#pragma inline
static gvh_u32_t ReadRegister(
    gvh_u32_t Addr);

/*******************************************************************************
 * Name        : WriteRegister
 * Description : Assign a value (Value) to a register (Addr)
 * Parameters  : ???
 * Assumptions : ???
 * Limitations : ???
 * Returns     : ???
 * *******************************************************************************/
static void WriteRegister(
    gvh_u32_t Addr,
    gvh_u32_t Value)
{

#if ((defined (FW_STXP70) && !defined (TLM_NATIVE)) || defined (HQVDPLITE_API_FOR_STAPI_SANITY_CHECK))
  /*#if defined (FW_STXP70) && !defined (TLM_NATIVE)  */
  volatile gvh_u32_t* Pt = (gvh_u32_t*) Addr;
  *Pt = Value;
#else
  /* volatile gvh_u32_t* Pt = (gvh_u32_t*) esw_tlm_to_host(Addr);
   * *Pt = Value;
   * yield();
   */
  /* If we want to stop using pointer dereferencing...
     U64 baseReg = (U64)Addr;
     gvh_u32_t data = Value; */
  global_verif_hal.write(Addr,0 , Value);
  /*esw_sync(1);*/
#endif


  /*printf("WriteRegister: write 0x%x at address 0x%x \n", Value, Addr);
    On systemC platform, force the current thread to be descheduled. This is
    necessary to do that if we want the streamer transfers to be done.
    esw_sync(0); */


}

/*******************************************************************************
 * Name        : ReadRegister
 * Description : Read a register (Addr)
 * Parameters  : ???
 * Assumptions : ???
 * Limitations : ???
 * Returns     : ???
 * *******************************************************************************/
static gvh_u32_t ReadRegister(
    gvh_u32_t Addr)
{
    gvh_u32_t Value;

#if ((defined (FW_STXP70) && !defined (TLM_NATIVE)) || defined (HQVDPLITE_API_FOR_STAPI_SANITY_CHECK))
    /*#if defined (FW_STXP70) && !defined (TLM_NATIVE) */
    volatile gvh_u32_t* Pt = (gvh_u32_t*) Addr;
    Value = *Pt;
#else
    /*  volatile gvh_u32_t* Pt = (gvh_u32_t*) esw_tlm_to_host(Addr);
     *  Value = *Pt;
     *  yield();
     */
    /* If we want to stop using pointer dereferencing...

    Value = esw_read32((gvh_u64_t)Addr);
    U64 baseReg = (U64)Addr; */
    global_verif_hal.read(Addr,0 , &Value);
    /*esw_sync(1);*/
#endif


    /*printf("ReadRegister: read 0x%x at address 0x%x \n", *Pt, Addr);
      If we want to stop using pointer dereferencing...
      On systemC platform, force the current thread to be descheduled. This is
      necessary to do that if we want the streamer transfers to be done.
      esw_sync(0); */

    return Value;
}

#endif  /* #ifndef (HQVDPLITE_API_FOR_STAPI || HQVDPLITE_API_FOR_STAPI_SANITY_CHECK)*/

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif  /* #ifndef _HQVDPLITE_STDDEFS_H */



