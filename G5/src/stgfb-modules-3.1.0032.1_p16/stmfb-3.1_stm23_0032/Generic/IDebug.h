/***********************************************************************
 *
 * File: Generic/IDebug.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Operating System Debug Services Abstract Interface
 *
\***********************************************************************/

#ifndef IDEBUG_H
#define IDEBUG_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef struct
{
  int  (*Init)(void);
  void (*Release)(void);
  void (*IDebugPrintf)(const char *,...) __attribute__ ((format (printf,1,2)));
  void (*IDebugBreak)(const char *, const char *, unsigned int);
} IDebug;

/* This is the pointer to the IDebug object that implements debug
 * for this operating system
 */
extern IDebug *g_pIDebug;

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 2
#endif

/*
 * DEBUGF(PrintfArg)
 * Debug Formated. Similar to printf. takes variable args inside double
 * brackets:
 * DEBUGF(("A number %d", number));
 *
 * ASSERTF(Cond, PrintfArg)
 * Halts execution if the condition is false and prints the formated string
 * ASSERTF(ptr == NULL, ("A number %d", number));
 *
 * Definitions if we have access to IOS
 * Always compile asserts
 */
#define ASSERTF(x, y) { if (!(x)) { g_pIDebug->IDebugPrintf y; g_pIDebug->IDebugBreak(#x, __FILE__, __LINE__); } }

#ifdef DEBUG
#define DEBUGF(arg) 	g_pIDebug->IDebugPrintf arg
#else
#define DEBUGF(arg)     __idebugprintf arg
static inline int __attribute__ ((format (printf, 1, 2)))
__idebugprintf (const char *format, ...)
{
  return 0;
}
#endif

#define DEBUGF2(level,arg)  \
do {                        \
  if(level <= DEBUG_LEVEL)  \
    DEBUGF(arg);            \
} while(0)

/*
 * These depend on having a GNU compiler...
 */
#define FENTRY "+++%s"
#define FEXIT "---%s"
#define FERROR "!!!%s: "

#define DENTRY() DEBUGF2(2,(FENTRY " @ %p\n",__PRETTY_FUNCTION__, this))
#define DENTRYn(level) DEBUGF2(level,(FENTRY " @ %p\n",__PRETTY_FUNCTION__, this))
#define DEXIT()  DEBUGF2(2,(FEXIT " @ %p\n",__PRETTY_FUNCTION__, this))
#define DEXITn(level) DEBUGF2(level,(FEXIT " @ %p\n",__PRETTY_FUNCTION__, this))
#define DERROR(arg) DEBUGF2(1,(FERROR arg "\n",__PRETTY_FUNCTION__))

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* IDEBUG_H */
