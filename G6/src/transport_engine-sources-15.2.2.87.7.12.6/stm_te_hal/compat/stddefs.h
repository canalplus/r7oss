/******************************************************************************

File Name   : stddefs.h

Description : Contains a number of generic type declarations and defines.

Copyright (C) 1999-2007 STMicroelectronics

******************************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __STDDEFS_H
#define __STDDEFS_H

/* Includes ---------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* Exported Types ---------------------------------------------------------- */

/* Common unsigned types */
#ifndef DEFINED_U8
#define DEFINED_U8
	typedef unsigned char U8;
#endif

#ifndef DEFINED_U16
#define DEFINED_U16
	typedef unsigned short U16;
#endif

#ifndef DEFINED_U32
#define DEFINED_U32
	typedef unsigned int U32;
#endif

/* Common signed types */
#ifndef DEFINED_S8
#define DEFINED_S8
	typedef signed char S8;
#endif

#ifndef DEFINED_S16
#define DEFINED_S16
	typedef signed short S16;
#endif

#ifndef DEFINED_S32
#define DEFINED_S32
	typedef signed int S32;
#endif

#ifndef DEFINED_U64
#define DEFINED_U64
	typedef unsigned long long U64;
#endif

#ifndef DEFINED_S64
#define DEFINED_S64
	typedef signed long long S64;
#endif

/* Boolean type (values should be among TRUE and FALSE constants only) */
#ifndef DEFINED_BOOL
#define DEFINED_BOOL
	typedef int BOOL;
#endif

/* General purpose string type */
	typedef char *ST_String_t;

/* Function return error code */
	typedef U32 ST_ErrorCode_t;

/* Revision structure */
	typedef const char *ST_Revision_t;

/* Device name type */
#define ST_MAX_DEVICE_NAME 16	/* 15 characters plus '\0' */
	typedef char ST_DeviceName_t[ST_MAX_DEVICE_NAME];

/* Exported Constants ------------------------------------------------------ */

/* BOOL type constant values */
#ifndef TRUE
#define TRUE (1 == 1)
#endif
#ifndef FALSE
#define FALSE (!TRUE)
#endif

/* Common driver error constants */
#define ST_DRIVER_ID   0
#define ST_DRIVER_BASE (ST_DRIVER_ID << 16)
	enum {
		ST_NO_ERROR = ST_DRIVER_BASE,	/* 0 */
		ST_ERROR_BAD_PARAMETER,		/* 1 Bad parameter passed       */
		ST_ERROR_NO_MEMORY,		/* 2 Memory allocation failed   */
		ST_ERROR_UNKNOWN_DEVICE,	/* 3 Unknown device name        */
		ST_ERROR_ALREADY_INITIALIZED,	/* 4 Device already initialized */
		ST_ERROR_NO_FREE_HANDLES,	/* 5 Cannot open device again   */
		ST_ERROR_OPEN_HANDLE,		/* 6 At least one open handle   */
		ST_ERROR_INVALID_HANDLE,	/* 7 Handle is not valid        */
		ST_ERROR_FEATURE_NOT_SUPPORTED,	/* 8 Feature unavailable        */
		ST_ERROR_INTERRUPT_INSTALL,	/* 9 Interrupt install failed   */
		ST_ERROR_INTERRUPT_UNINSTALL,	/* 10 Interrupt uninstall failed */
		ST_ERROR_TIMEOUT,		/* 11 Timeout occured            */
		ST_ERROR_DEVICE_BUSY,		/* 12 Device is currently busy   */
		ST_ERROR_SUSPENDED		/* 13 Device is in D1 or D2 state */
	};

/* Exported Variables ------------------------------------------------------ */

/* Exported Macros --------------------------------------------------------- */
#define UNUSED_PARAMETER(x)    (void)(x)

/* Exported Functions ------------------------------------------------------ */

#ifdef __cplusplus
}
#endif
#endif /* #ifndef __STDDEFS_H */
