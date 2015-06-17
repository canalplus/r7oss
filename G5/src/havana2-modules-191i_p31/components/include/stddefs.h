/******************************************************************************

File Name   : stddefs.h

Description : Contains a number of generic type declarations and defines.

Copyright (C) 1999 STMicroelectronics

******************************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __VC9_STDDEFS_H
#define __VC9_STDDEFS_H


/* Includes ---------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* Exported Types ---------------------------------------------------------- */

/* Common unsigned types */
#ifndef DEFINED_U8
#define DEFINED_U8
typedef unsigned char  U8;
#endif

#ifndef DEFINED_U16
#define DEFINED_U16
typedef unsigned short U16;
#endif

#ifndef DEFINED_U32
#define DEFINED_U32
typedef unsigned int   U32;
#endif

/* Common signed types */
#ifndef DEFINED_S8
#define DEFINED_S8
typedef signed char  S8;
#endif

#ifndef DEFINED_S16
#define DEFINED_S16
typedef signed short S16;
#endif

#ifndef DEFINED_S32
#define DEFINED_S32
typedef signed int   S32;
#endif

#ifndef DEFINED_ERRORCODE
#define DEFINED_ERRORCODE
typedef int ST_ErrorCode_t;
#endif	
	
/* Boolean type (values should be among TRUE and FALSE constants only) */
#ifndef DEFINED_BOOL
#define DEFINED_BOOL
typedef int BOOL;
/* BOOL type constant values */
#ifndef TRUE
    #define TRUE (1 == 1)
#endif
#ifndef FALSE
    #define FALSE (!TRUE)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef __STDDEFS_H */

/* End of stddefs.h  ------------------------------------------------------- */


