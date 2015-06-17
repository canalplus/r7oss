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
   @file   pti_debug.c
   @brief  Various debug routines and facilities for PTI

   This file contains functions such as printf capabilities for debugging the PTI.

   The Transport Processor can pass debug information back to the host, however it
   is very important the host takes the string and stores it quickly, so that the
   Transport Processor doesn't get stalled.  This means we need a PrintBuffer.

   The PrintBuffer accumlates prints from the interrupt service routine (TP
   prints) as well as driver prints outside of the interrupt context.

 */

/* Includes ---------------------------------------------------------------- */

/* OS includes */
#include "linuxcommon.h"
#include "stddefs.h"

/* Includes from API level */
#include "pti_debug.h"

/* Includes from the HAL / ObjMan level */

/* MACROS ------------------------------------------------------------------ */
/* Uncomment to add timestamps to beginning of each line */
/* #define STPTI_DEBUG_ADD_TIMESTAMPS */

/* Private Constants ------------------------------------------------------- */
#ifndef STPTI_DEBUG_PRINT_BUFFER_SIZE
#define STPTI_DEBUG_PRINT_BUFFER_SIZE    8192
#endif

#define STPTI_DEBUG_PRINT_TRAILING_ITEMS 8
#define STPTI_DEBUG_MAXIMUM_PRINT_STRING 512

/* Private Variables ------------------------------------------------------- */
static char stpti_PrintBuffer[STPTI_DEBUG_PRINT_BUFFER_SIZE + 1];	/* plus 1 for a null terminator */
static char *stpti_PrintBufferWr_p = NULL;
static char *stpti_PrintBufferRd_p = NULL;
static char *stpti_PrintBufferVS_p = NULL;	/* pointer to the first bit of valid data */
static struct semaphore *stpti_PrintBufferSemaphore_p = NULL;
static void stptiSUPPORT_AddToPrintfBuffer(const char *String, int Length);

static DEFINE_SPINLOCK(PrintBufferLock);

/* Private Function Prototypes --------------------------------------------- */

/* Functions --------------------------------------------------------------- */

/**
   @brief  Prints limited characters to a string, when a specified offset has been reached.

   This function prints to a string after an offset has been reached (specified in ctx_p).  It stops
   printing when the string is full.

   @param  ctx_p   Context for printing (offset, pointer to the sting, string limits)
   @param  fmt     Normal printf format with any necessary parameters following.

 */
void stptiSUPPORT_sprintf(stptiSUPPORT_sprintf_t * ctx_p, const char *fmt, ...)
{
	static char string[512];	/* avoid placing on the stack */
	int n;

	va_list argList;
	va_start(argList, fmt);

	if (!ctx_p->StoppedPrintingEarly) {
		n = vscnprintf(string, sizeof(string), fmt, argList);

		if (ctx_p->CurOffset >= ctx_p->StartOffset) {
			/* Above lower threshold for printing */
			if (n < ctx_p->CharsLeft) {
				/* Below upper threshold for printing */
				strcpy(ctx_p->p, string);
				ctx_p->p += n;
				ctx_p->CurOffset += n;
				ctx_p->CharsLeft -= n;
				ctx_p->CharsOutput += n;
			} else {
				/* Above upper threshold for printing */
				ctx_p->CurOffset += n;
				ctx_p->StoppedPrintingEarly = TRUE;
			}
		} else {
			ctx_p->CurOffset += n;
		}
	}

	va_end(argList);
}

/**
   @brief  This function is for recording debug information outside of an interrupt context.

   This function when used in tandem with the stpti_printf macro will prefix the current function
   and line number and send it to a print buffer, as well as optionally printing it out using
   printf.

   This function should not be called directly (without using the macro).  The macro allows the
   printfs to be compiled out.

   @param  function      A constant string from __FUNCTION__
   @param  line          A constand int from __LINE__
   @param  fmt           A standard printf format string followed by any parameters it needs.

   @return               none.

 */
void stptiSUPPORT_printf_fn(const char *function, const int line, const char *fmt, ...)
{
	static char string[STPTI_DEBUG_MAXIMUM_PRINT_STRING];	/* avoid placing it on the stack */
	const int len = sizeof(string);
	int count;

	va_list argList;
	va_start(argList, fmt);

#if defined( STPTI_DEBUG_ADD_TIMESTAMPS )
	count = scnprintf(string, len, "%10d %s:%d: ", (unsigned)jiffies, function, line);
#else
	count = scnprintf(string, len, "%s:%d: ", function, line);
#endif

#if 0
	/* If this compiler supports vsnprintf we use that to protect against overflows */
	count += vsnprintf(string + count, STPTI_DEBUG_MAXIMUM_PRINT_STRING, fmt, argList);
	string[STPTI_DEBUG_MAXIMUM_PRINT_STRING - 1] = 0;	/* Make sure it is null terminated, even if truncated */
#else
	/* no protection against string overflows */
	count += vscnprintf(string + count, len - count, fmt, argList);
#endif

	count += scnprintf(string + count, len - count, "\n");	/* add new line */
	stptiSUPPORT_AddToPrintfBuffer(string, count);

	va_end(argList);
}

/**
   @brief  This function is for recording debug information inside an interrupt context.

   This function when used in tandem with the stpti_interrupt_print macro will send the two
   supplied strings to the print buffer.  No printf template functionality is provided to keep
   the function speedy.

   This function should not be called directly (without using the macro).  The macro allows the
   printfs to be compiled out.

   @param  prefix_string_p  A string to prefix.
   @param  print_string_p   The string to print

   @return               none.

 */
void stptiSUPPORT_interrupt_print_fn(const char *prefix_string_p, const char *print_string_p)
{
	static char string[STPTI_DEBUG_MAXIMUM_PRINT_STRING];	/* avoid placing it on the stack */
	const int len = sizeof(string);
	int count;

#if defined( STPTI_DEBUG_ADD_TIMESTAMPS )
	count = scnprintf(string, len, "%10d %s%s\n", (unsigned)jiffies, prefix_string_p, print_string_p);
#else
	count = scnprintf(string, len, "%s%s\n", prefix_string_p, print_string_p);
#endif
	stptiSUPPORT_AddToPrintfBuffer(string, strlen(string));
}

/**
   @brief  This function adds to the Print Buffer.

   This function adds a string to a print buffer, task locking to make sure it is not
   preempted (either by interrupt or task).

   It is important to make sure that in multiple TANGO environments that all the TANGO interrupts
   are at the same priority else this task could be preempted by another interrupt.

   If stptiSUPPORT_AssociateSemaphoreToPrintf has been called then the specified semaphore will be
   signalled at the end of the function.

   To facilitate use of a debugger to read this buffer we have been careful to preserve the string
   null terminators.  To dump out the print buffer in gdb...

       printf "%s" , stpti_PrintBufferRd_p
       if( stpti_PrintBufferRd_p > stpti_PrintBufferWr_p )
          printf "%s" , stpti_PrintBuffer
       endif

   @param  String        String to add to print buffer (null terminated)
   @param  Length        The length of String (ignoring null terminator).

   @return               none.

 */
static void stptiSUPPORT_AddToPrintfBuffer(const char *String, int Length)
{
	char *NewWr_p;
	int BytesToBufferEnd;
	unsigned long flags;

	/* To guarantee integrity of the print buffer, we prevent preemption either by interrupt or task */
	spin_lock_irqsave(&PrintBufferLock, flags);
	{
		/* Do we need to initialise? */
		if (stpti_PrintBufferWr_p == NULL) {
			stpti_PrintBuffer[STPTI_DEBUG_PRINT_BUFFER_SIZE] = 0;	/* string terminator */
			stpti_PrintBufferWr_p = &stpti_PrintBuffer[0];
			*stpti_PrintBufferWr_p = 0;	/* string terminator */
			stpti_PrintBufferRd_p = stpti_PrintBufferWr_p;
			stpti_PrintBufferVS_p = stpti_PrintBufferWr_p;
		}

		/* copy into buffer */
		BytesToBufferEnd = &stpti_PrintBuffer[STPTI_DEBUG_PRINT_BUFFER_SIZE] - stpti_PrintBufferWr_p;
		if (Length <= BytesToBufferEnd) {
			NewWr_p = stpti_PrintBufferWr_p + Length;
			if (NewWr_p == &stpti_PrintBuffer[STPTI_DEBUG_PRINT_BUFFER_SIZE]) {
				/* We have reached the end of the buffer */
				NewWr_p = &stpti_PrintBuffer[0];
			}

			/* NULL terminate the buffer.  This helps if quizzing the buffer using a debugger. We do
			   this now in case the debugger is stopped during the memcpy */
			*NewWr_p = 0;
			memcpy(stpti_PrintBufferWr_p, String, Length);

			if ((stpti_PrintBufferRd_p > stpti_PrintBufferWr_p) && (stpti_PrintBufferRd_p <= NewWr_p)) {
				/* If the write pointer has pushed past read pointer update the read pointer. */
				stpti_PrintBufferRd_p = NewWr_p + 1;
			}

			if ((stpti_PrintBufferVS_p > stpti_PrintBufferWr_p) && (stpti_PrintBufferVS_p <= NewWr_p)) {
				/* If the write pointer has pushed past valid start pointer - update */
				stpti_PrintBufferVS_p = NewWr_p + 1;
			}
		} else {
			/* wrapping case */
			NewWr_p = stpti_PrintBuffer + (Length - BytesToBufferEnd);

			/* NULL terminate the buffer.  This helps if quizzing the buffer using a debugger. We do
			   this now in case the debugger is stopped during the memcpys */
			*NewWr_p = 0;
			memcpy(stpti_PrintBufferWr_p, String, BytesToBufferEnd);
			memcpy(stpti_PrintBuffer, String + BytesToBufferEnd, Length - BytesToBufferEnd);

			if ((stpti_PrintBufferRd_p <= NewWr_p) || (stpti_PrintBufferRd_p >= stpti_PrintBufferWr_p)) {
				/* If the write pointer has pushed past read pointer update the read pointer. */
				stpti_PrintBufferRd_p = NewWr_p + 1;
			}

			if ((stpti_PrintBufferVS_p <= NewWr_p) || (stpti_PrintBufferVS_p >= stpti_PrintBufferWr_p)) {
				/* If the write pointer has pushed past valid start pointer - update */
				stpti_PrintBufferVS_p = NewWr_p + 1;
			}
		}

		stpti_PrintBufferWr_p = NewWr_p;
	}
	spin_unlock_irqrestore(&PrintBufferLock, flags);

	if (stpti_PrintBufferSemaphore_p != NULL) {
		/* This is inside a task lock to prevent something altering stpti_PrintBufferSemaphore_p
		   whilst it is being used.  Signalling whilst task locked is allowed. */
		up(stpti_PrintBufferSemaphore_p);
	}
}

/**
   @brief  This function associates a semaphore to the print buffer.

   This function associates a semaphore to the print buffer which is signalled whenever something
   is added to the print buffer.

   @param  semaphore_p   A pointer to a semaphore, or NULL if disabling.

   @return               none.

 */
ST_ErrorCode_t stptiSUPPORT_AssociateSemaphoreToPrintf(struct semaphore *semaphore_p)
{
	stpti_PrintBufferSemaphore_p = semaphore_p;
	return (ST_NO_ERROR);
}

/**
   @brief  This function returns all or part of the print buffer.

   This function will return up to a specified number of characters from the print buffer.

   @warning  This function disables interrupts whilst the data is copied, as interrupt routines
   are allowed to printf to this buffer.  It could be possible to avoid this by having a second
   buffer which we divert all the printfs to, whilst we are doing the copy here.  However this
   would complicate the above functions a lot.

   @param  Buffer_p      A pointer to a buffer to fill with characters from the print buffer.
   @param  Length        The maximum size of the Buffer referenced by Buffer_p.

   @return               The number of characters copied.

 */
int stptiSUPPORT_ReturnPrintfBuffer(char *Buffer_p, int Length)
{
	char *char_p = Buffer_p;
	unsigned long flags;

	*Buffer_p = 0;

	/* Quick check to avoid an unnecessary interrupt lock */
	if ((stpti_PrintBufferWr_p == NULL) || (stpti_PrintBufferRd_p == stpti_PrintBufferWr_p)) {
		/* Not initialised or nothing to print */
		return (0);
	}

	Length -= 1;		/* Allow for string terminator */

	/* To guarantee integrity of buffer, prevent preemption either by interrupt or task (interrupts
	   can add to the buffer as well as regular tasks).  This is unfortunate, but this is fairly
	   short quick code with no calls. */
	spin_lock_irqsave(&PrintBufferLock, flags);

	while ((Length > 0) && (*stpti_PrintBufferRd_p != 0)) {
		*(char_p++) = *(stpti_PrintBufferRd_p++);
		if (stpti_PrintBufferRd_p == &stpti_PrintBuffer[STPTI_DEBUG_PRINT_BUFFER_SIZE]) {
			stpti_PrintBufferRd_p = &stpti_PrintBuffer[0];
		}
		Length--;
	}

	spin_unlock_irqrestore(&PrintBufferLock, flags);

	*char_p = 0;		/* Add string terminator */

	return (char_p - Buffer_p);
}

int stptiSUPPORT_ResetPrintfRdPointer(void)
{
	unsigned long flags;
	spin_lock_irqsave(&PrintBufferLock, flags);
	stpti_PrintBufferRd_p = stpti_PrintBufferVS_p;
	spin_unlock_irqrestore(&PrintBufferLock, flags);

	return 0;
}
