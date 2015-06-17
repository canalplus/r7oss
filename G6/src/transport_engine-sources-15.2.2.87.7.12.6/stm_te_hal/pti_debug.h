/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided "AS IS", WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
   @file   pti_debug.h
   @brief  Various debug routines and facilities for PTI

   This file contains functions such as printf capabilities for debugging the PTI.

   These macros will call STTRACE with the class indicated (FATAL, ERROR, WARNING,
   INFO).  STTRACE is enabled in sttrace.h (search for STPTI5).

   You will get all debug messages logged into
   the stpti debug print buffer which can be read later by STPTI_Debug().  This
   is less intrusive on performance than STTRACE, and has the advantage of being
   able to log interrupts prinfs (incl. xp70 messages).  You do not need STTRACE
   enabled to use debug print buffer.

     STPTI_ASSERT(condition, ...)        STTRACE-FATAL (if condition is false)
     STPTI_PRINTF_ERROR( ... )           STTRACE-ERROR
     STPTI_PRINTF_WARNING( ... )         STTRACE-WARNING

     STPTI_PRINTF(...)                   STTRACE-INFO
     STPTI_PRINTX(...)                   STTRACE-INFO
     STPTI_PRINTU(...)                   STTRACE-INFO
     STPTI_PRINTD(...)                   STTRACE-INFO

     STPTI_INTERRUPT_PRINT(...)          (only to stpti debug buffer - need for TP debug)

   If you define STPTI_PRINT at the top of the source file of interest, these become
   active (meant for detailed debugging on a per file basis)...

     stpti_printf(...)                   STTRACE-INFO (if STPTI_PRINT defined)
     stpti_printx(...)                   STTRACE-INFO (if STPTI_PRINT defined)
     stpti_printu(...)                   STTRACE-INFO (if STPTI_PRINT defined)
     stpti_printd(...)                   STTRACE-INFO (if STPTI_PRINT defined)
     stpti_interrupt_print(...)          only to stpti debug buffer (and only if STPTI_PRINT defined)

 */

#ifndef _PTI_DEBUG_H_
#define _PTI_DEBUG_H_

/* Includes ---------------------------------------------------------------- */

#include "linuxcommon.h"

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */

/* Includes from the HAL / ObjMan level */
#include "objman/pti_object.h"

/* Exported Types ---------------------------------------------------------- */
typedef struct {
	char *p;		/**< Pointer to the position in the string to print to */
	int CharsOutput;	/**< Number of characters printed */
	int CharsLeft;		/**< Number of characters space left in the string */
	int CurOffset;		/**< Current Offset into the "virtual file" */
	int StartOffset;	/**< Offset into the "virtual file" when we can start printing */
	BOOL StoppedPrintingEarly;
				/**< Finished printing early (CharsLeft too small to print to string) */
} stptiSUPPORT_sprintf_t;

/* Exported Function Prototypes -------------------------------------------- */

/* Used for adding/removing objects to the debug system */
void stptiHAL_debug_add_object(FullHandle_t handle);
void stptiHAL_debug_add_object_lite(FullHandle_t handle);
void stptiHAL_debug_rem_object(FullHandle_t handle);
void stptiHAL_debug_register_STFE(int numIBs, int numSWTSs, int numMIBs);
void stptiHAL_debug_unregister_STFE(void);

/* used for writing to a string in a controlled manner */
void stptiSUPPORT_sprintf(stptiSUPPORT_sprintf_t * ctx_p, const char *fmt, ...) __attribute__ ((format(printf, 2, 3)));

/* returns a pointer to the print buffer */
int stptiSUPPORT_ReturnPrintfBuffer(char *Buffer_p, int Length);

/* Reset the read pointer to the start of valid data */
int stptiSUPPORT_ResetPrintfRdPointer(void);

/* This is the primary debug interface */
/* MUST BE USED SPARINGLY !!! */
/* Macros all on one line to keep __LINE__ working as expected */
#define STPTI_ASSERT( condition, fmt, ... ) if( !(condition) ) { stptiSUPPORT_printf_fn( __FUNCTION__, __LINE__ , fmt, ## __VA_ARGS__ ); pr_err(fmt "\n", ##__VA_ARGS__); }

#define STPTI_PRINTF_ERROR( fmt, ... ) { stptiSUPPORT_printf_fn( __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__ ); pr_err(fmt "\n", ##__VA_ARGS__); }

#define STPTI_PRINTF_WARNING( fmt, ... ) { stptiSUPPORT_printf_fn( __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__ ); pr_warning(fmt "\n", ##__VA_ARGS__); }

#define STPTI_PRINTF( fmt, ... ) { stptiSUPPORT_printf_fn( __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__ ); pr_info(fmt "\n", ##__VA_ARGS__); }

#define STPTI_INTERRUPT_PRINT( prefix_string, string ) { stptiSUPPORT_interrupt_print_fn( prefix_string, string ); pr_info("%s%s\n", prefix_string, string); }

/* Do not call the following directly.  Use the macros! */
void stptiSUPPORT_printf_fn(const char *function, const int line, const char *fmt, ...)
    __attribute__ ((format(printf, 3, 4)));
void stptiSUPPORT_interrupt_print_fn(const char *prefix_string_p, const char *print_string_p);
ST_ErrorCode_t stptiSUPPORT_AssociateSemaphoreToPrintf(struct semaphore *semaphore_p);

/* Useful for printing variables */
#define STPTI_PRINTX(x)                         STPTI_PRINTF( #x"=0x%08x", (unsigned)x )
#define STPTI_PRINTU(x)                         STPTI_PRINTF( #x"=%u",     (unsigned)x )
#define STPTI_PRINTD(x)                         STPTI_PRINTF( #x"=%d",     (int)x )

/* This is a second level printf interface used for detailed debug).
   Turned on file by file (by setting STPTI_PRINT at the top of the file of interest). */

#define stpti_printf(fmt, ...) pr_debug(fmt, ## __VA_ARGS__)
#define stpti_printx(x)        pr_debug(#x"=0x%08x", (unsigned)x)
#define stpti_printu(x)        pr_debug(#x"=%u", (unsigned)x)
#define stpti_printd(x)        pr_debug(#x"=%d", (int)x)

#endif /* _PTI_DEBUG_H_ */
