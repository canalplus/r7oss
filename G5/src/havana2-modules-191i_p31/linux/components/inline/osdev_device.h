/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : osdev_kernel_device.h kernel only version of osdev_device.h
Author :           Nick

Contains the useful operating system functions/types
that allow us to implement OS independent device functionality.


Date        Modification                                    Name
----        ------------                                    --------
31-Mar-03   Created                                         Nick

************************************************************************/

#ifndef H_OSDEV_DEVICE
#define H_OSDEV_DEVICE


/* --- Include the os specific headers we will need --- */

#ifndef __cplusplus

/* warning suppression - use the definitions from <linux/kernel.h> */
#undef min
#undef max

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/syscalls.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include <linux/vmalloc.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>

#include <linux/platform_device.h>

#if !defined (CONFIG_USE_SYSTEM_CLOCK)
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#endif

#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/mman.h>

#define OS_CACHE_LINE_SIZE      32

MODULE_LICENSE("GPL");

#define __st220pft(X,Y) ((void) ((X) + (Y)))

#ifndef bool
#define bool    unsigned char
#endif
#ifndef false
#define false   0
#define true    1
#endif
#endif // __cplusplus

#ifndef boolean
#define boolean bool
#endif

#if defined EXPORT_SYMBOLS_FOR_CXX || defined __cplusplus
#define STATIC_INLINE
#define STATIC_PROTOTYPE extern
#else
#define STATIC_INLINE static inline
#define STATIC_PROTOTYPE static
#endif

// ====================================================================================================================
//      The preamble,
//
// Useful constants
//

#define OSDEV_MAXIMUM_DEVICE_LINKS      16
#define OSDEV_MAXIMUM_MAJOR_NUMBER      256
#define OSDEV_MAX_OPENS                 32

#define OSDEV_HIGHEST_PRIORITY          99
#define OSDEV_MID_PRIORITY              50
#define OSDEV_LOWEST_PRIORITY           0

#define OSDEV_DEFAULT_PRIORITY          (OSDEV_HIGHEST_PRIORITY-16)                /* Devices run higher */
#define OSDEV_DEFAULT_TASK_NAME         "OS-Device"

#define OSDEV_MEMORY_PAGE_SIZE          PAGE_SIZE
#define OSDEV_MEMORY_PAGE_SHIFT         PAGE_SHIFT

#define OSDEV_INFINITE                  0xffffffff
//
// Early declaration of the descriptor type (structure follows later)
//

typedef struct OSDEV_Descriptor_s OSDEV_Descriptor_t;

//
// The device list
//

typedef struct OSDEV_DeviceLink_s
{
    boolean              Valid;
    unsigned int         MajorNumber;
    unsigned int         MinorNumber;
    char                *Name;
} OSDEV_DeviceLink_t;

//
// The context for an open
//

typedef struct OSDEV_OpenContext_s
{
    boolean                      Open;
    unsigned int                 MinorNumber;
    OSDEV_Descriptor_t          *Descriptor;
    void                        *PrivateData;
} OSDEV_OpenContext_t;


extern OSDEV_DeviceLink_t        OSDEV_DeviceList[OSDEV_MAXIMUM_DEVICE_LINKS];
extern OSDEV_Descriptor_t       *OSDEV_DeviceDescriptors[OSDEV_MAXIMUM_MAJOR_NUMBER];

//
// The enumeration types
//

typedef enum
{
    OSDEV_NoError       = 0,
    OSDEV_Error
} OSDEV_Status_t;

//
// Simple types
//

#ifndef __cplusplus
typedef struct semaphore            *OSDEV_Semaphore_t;
typedef wait_queue_head_t           *OSDEV_WaitQueue_t;
#else
typedef void                        *OSDEV_Semaphore_t;
typedef void                        *OSDEV_WaitQueue_t;
#endif

//
// Prototype the functions that will be exported to C++ in order to help the compiler
// automatically detect bit rot. We only export a very limited subset at present.
//
#ifdef __cplusplus
extern "C" {
#endif
STATIC_PROTOTYPE void            *OSDEV_Malloc(                    unsigned int             Size );
STATIC_PROTOTYPE OSDEV_Status_t   OSDEV_Free(                      void                    *Address );
STATIC_PROTOTYPE void            *OSDEV_TranslateAddressToUncached( void                   *Address );
STATIC_PROTOTYPE void             OSDEV_PurgeCacheAll(void);
STATIC_PROTOTYPE void             OSDEV_FlushCacheRange(void *start, int size);
STATIC_PROTOTYPE void             OSDEV_InvalidateCacheRange(void *start, int size);
STATIC_PROTOTYPE void             OSDEV_SnoopCacheRange(void *start, int size);
STATIC_PROTOTYPE void             OSDEV_PurgeCacheRange(void *start, int size);
#ifdef __cplusplus
}
#endif


//
// C specific section (mostly because after this point we require Linux types which C++ doesn't know about)
//
#ifndef __cplusplus

//
// The device parameter macros
//

#define OSDEV_PARAMETER_INTEGER( var )                          module_param( var, int, S_IRUSR | S_IRGRP | S_IROTH )
#define OSDEV_PARAMETER_INTEGER_ARRAY( var, count )             module_param_array( var, int, count, S_IRUSR | S_IRGRP | S_IROTH )
#define OSDEV_PARAMETER_STRING( var )                           module_param( var, charp, S_IRUSR | S_IRGRP | S_IROTH )
#define OSDEV_PARAMETER_STRING_ARRAY( var, count )              module_param_array( var, charp, count, S_IRUSR | S_IRGRP | S_IROTH )
#define OSDEV_PARAMETER_DESCRIPTION( var, descr )               MODULE_PARM_DESC( var, descr )

//
// The function types and macro's used for device loading and unloading
//
typedef int (*OSDEV_LoadFn_t)( void );

#define OSDEV_LoadEntrypoint(fn)        static int __init fn( void )

//

typedef void (*OSDEV_UnloadFn_t)( void );

#define OSDEV_UnloadEntrypoint(fn)      static void __exit fn( void )

//
// The function types and macros' used for platform device loading and unloading
//
typedef struct PlatformData_s
{
	int   NumberBaseAddresses;
	int   NumberInterrupts;
	int   BaseAddress[10];
	int   Interrupt[10];
	void *PrivateData;
} PlatformData_t;

#define OSDEV_PlatformLoadEntrypoint(fn) static int fn( PlatformData_t *PlatformData )

#define OSDEV_PlatformUnloadEntrypoint(fn) static void fn( void )

#define OSDEV_RegisterPlatformDriverFn( name2, Loadfn, Unloadfn )       \
	OSDEV_PlatformLoadEntrypoint(Loadfn);                           \
	OSDEV_PlatformUnloadEntrypoint(Unloadfn);                       \
	static int plat_probe(struct platform_device *pdev) {           \
		PlatformData_t PData;                                   \
		struct resource *res;                                   \
		if (!pdev || !pdev->name) OSDEV_Print("Error No Platform Data\n"); \
		PData.PrivateData = pdev->dev.platform_data;            \
		PData.NumberBaseAddresses = 0;                          \
		do {                                                    \
			res = platform_get_resource(pdev,IORESOURCE_MEM,PData.NumberBaseAddresses); \
			if (res) { \
				PData.BaseAddress[PData.NumberBaseAddresses] = res->start; \
				PData.NumberBaseAddresses++;            \
			}                                               \
		} while (res);                                          \
		PData.NumberInterrupts = 0;                             \
		do {                                                    \
			res = platform_get_resource(pdev,IORESOURCE_IRQ,PData.NumberInterrupts); \
			if (res) { \
				PData.Interrupt[PData.NumberInterrupts] = res->start; \
				PData.NumberInterrupts++;               \
			}                                               \
		} while (res);                                          \
		return Loadfn(&PData);                                  \
	}                                                               \
	static int plat_remove(struct platform_device *pdev) {          \
		Unloadfn();                                             \
		return 0;                                               \
	}                                                               \
	static struct platform_driver plat_driver = {                   \
        	.driver = {                                             \
	                .name = name2,                                  \
                	.bus = &platform_bus_type,                      \
        	        .owner = THIS_MODULE,                           \
	        },                                                      \
        	.probe = plat_probe,                                    \
	        .remove = plat_remove,                                  \
	};                                                              \
	static int plat_init(void) {                                    \
		return platform_driver_register(&plat_driver);          \
	}                                                               \
	static void plat_cleanup(void) {                                \
		platform_driver_unregister(&plat_driver);               \
	}                                                               \
	module_init(plat_init);                                         \
	module_exit(plat_cleanup);

// -----------------------------------------------------------------------------------------------
//
// malloc and free used later
//

STATIC_INLINE void            *OSDEV_Malloc(                    unsigned int             Size )
{
    void *p;
    if( Size < (PAGE_SIZE * 4) )
	p = (void *)kmalloc( Size, GFP_KERNEL );
    else
	p = bigphysarea_alloc( Size );

#ifdef ENABLE_MALLOC_POISONING
    if (p) {
	memset(p, 0xca, Size);
    }
#endif

    return p;
}

// -----------------------------------------------------------------------------------------------


STATIC_INLINE OSDEV_Status_t   OSDEV_Free(                      void                    *Address )
{
    unsigned long  Base, Size;
    unsigned long  Addr = (unsigned long)Address;

    if( Address == NULL )
    {
	printk( "OSDEV_Free- Attempt to free null pointer\n" );
	return OSDEV_Error;
    }

    bigphysarea_memory( &Base, &Size );
    if( (Addr >= Base) && (Addr < (Base + Size)) )
	bigphysarea_free_pages( Address );
    else
	kfree( Address );

    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------
//
// Partioned versions of malloc and free 
//

STATIC_INLINE void            *OSDEV_MallocPartitioned( char                    *Partition,
							unsigned int             Size )
{
    struct bpa2_part    *partition;
    unsigned int         numpages;
    unsigned long        p;

    partition   = bpa2_find_part(Partition);
    if( partition == NULL )
	return NULL;

    numpages    = (Size + OSDEV_MEMORY_PAGE_SIZE - 1) / OSDEV_MEMORY_PAGE_SIZE;
    p           = bpa2_alloc_pages( partition, numpages, 0, 0 /*priority*/ );

   
    
    
//    if( p )
//	p	= (unsigned long)phys_to_virt(p);


#ifdef ENABLE_MALLOC_POISONING
    if( p ) 
    {
	memset( (void *)p, 0xca, Size);
    }
#endif

    return (void *)p;
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE OSDEV_Status_t   OSDEV_FreePartitioned(   char                    *Partition,
							void                    *Address )
{
    struct bpa2_part    *partition;

    partition   = bpa2_find_part(Partition);
    if( partition == NULL )
    {
	printk( "OSDEV_FreePartitioned - Partition not found\n" );
	return OSDEV_Error;
    }

    if( Address == NULL )
    {
	printk( "OSDEV_FreePartitioned- Attempt to free null pointer\n" );
	return OSDEV_Error;
    }

    bpa2_free_pages( partition, (unsigned int)Address );

    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------
//
// The function types and macro's used for thread creation
//

typedef struct task_struct             *OSDEV_Thread_t;
typedef void                           *OSDEV_ThreadParam_t;
typedef unsigned int                    OSDEV_ThreadPriority_t;

typedef void (*OSDEV_ThreadFn_t)( void   *Parameter );

#define OSDEV_ThreadEntrypoint(fn)      void fn( void   *Parameter )

//
// The function types and macro's used in defining the device descriptor type
//

typedef OSDEV_Status_t (*OSDEV_OpenFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext );

#define OSDEV_OpenEntrypoint(fn)        OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext )

//

typedef OSDEV_Status_t (*OSDEV_CloseFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext );

#define OSDEV_CloseEntrypoint(fn)       OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext )

//

typedef ssize_t (*OSDEV_WriteFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext,   const char  *buf, size_t  count, loff_t  *ppos );

#define OSDEV_WriteEntrypoint(fn)       ssize_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   const char  *buf,  size_t count, loff_t  *ppos )

//

typedef int (*OSDEV_PollFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext,   poll_table  *wait );

#define OSDEV_PollEntrypoint(fn)        int fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   poll_table  *wait  )

//

typedef OSDEV_Status_t (*OSDEV_IoctlFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int   cmd,   unsigned long   arg );

#define OSDEV_IoctlEntrypoint(fn)       OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int   cmd,   unsigned long   arg )

//

typedef OSDEV_Status_t (*OSDEV_MmapFn_t)( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int  OSDEV_VMOffset,   unsigned int   OSDEV_VMSize,   unsigned char **OSDEV_VMAddress );

#define OSDEV_MmapEntrypoint(fn)        OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int  OSDEV_VMOffset, unsigned int   OSDEV_VMSize,   unsigned char **OSDEV_VMAddress )

//

typedef irqreturn_t (*OSDEV_InterruptHandlerFn_t)( int Irq, void* Parameter);

#define OSDEV_InterruptHandlerEntrypoint(fn)    irqreturn_t fn( int Irq, void* Parameter)

//

struct OSDEV_Descriptor_s
{
    char                *Name;
    unsigned int         MajorNumber;

    OSDEV_OpenFn_t       OpenFn;
    OSDEV_CloseFn_t      CloseFn;
    OSDEV_IoctlFn_t      IoctlFn;
    OSDEV_MmapFn_t       MmapFn;
    OSDEV_PollFn_t       PollFn;

    OSDEV_OpenContext_t  OpenContexts[OSDEV_MAX_OPENS];
};

// -----------------------------------------------------------------------------------------------

#define OSDEV_Print            printk

// ====================================================================================================================
//      The implementation level device functions to support device implementation

static inline OSDEV_Status_t   OSDEV_RegisterDevice(    OSDEV_Descriptor_t      *Descriptor )
{
    if( OSDEV_DeviceDescriptors[Descriptor->MajorNumber] != NULL )
    {
	OSDEV_Print( "OSDEV_RegisterDevice - %s - Attempt to register two devices with same MajorNumber.\n", Descriptor->Name );
	return OSDEV_Error;
    }

    OSDEV_DeviceDescriptors[Descriptor->MajorNumber] = (OSDEV_Descriptor_t *)OSDEV_Malloc( sizeof(OSDEV_Descriptor_t) );
    if( OSDEV_DeviceDescriptors[Descriptor->MajorNumber] == NULL )
    {
	OSDEV_Print( "OSDEV_RegisterDevice - %s - Insufficient memory to hold device descriptor.\n", Descriptor->Name );
	return OSDEV_Error;
    }

    memset( OSDEV_DeviceDescriptors[Descriptor->MajorNumber], 0, sizeof(OSDEV_Descriptor_t) );

    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->MajorNumber       = Descriptor->MajorNumber;
    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->OpenFn            = Descriptor->OpenFn;
    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->CloseFn           = Descriptor->CloseFn;
    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->IoctlFn           = Descriptor->IoctlFn;
    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->MmapFn            = Descriptor->MmapFn;

    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name              = OSDEV_Malloc( strlen( Descriptor->Name )+1 );
    if( OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name == NULL )
    {
	OSDEV_Print( "OSDEV_RegisterDevice - %s - Insufficient memory to record device name.\n", Descriptor->Name );

	OSDEV_Free( OSDEV_DeviceDescriptors[Descriptor->MajorNumber] );
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber] = NULL;

	return OSDEV_Error;
    }
    strcpy( OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name, Descriptor->Name );

    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_DeRegisterDevice(  OSDEV_Descriptor_t      *Descriptor )
{
int     i;

//

    if( OSDEV_DeviceDescriptors[Descriptor->MajorNumber] == NULL )
    {
	OSDEV_Print( "OSDEV_DeRegisterDevice - %s - Attempt to de-register an unregistered device.\n", Descriptor->Name );
	return OSDEV_Error;
    }

    Descriptor                                          = OSDEV_DeviceDescriptors[Descriptor->MajorNumber];
    OSDEV_DeviceDescriptors[Descriptor->MajorNumber]    = NULL;

    for( i=0; i<OSDEV_MAX_OPENS; i++ )
	if( Descriptor->OpenContexts[i].Open )
	{
	    OSDEV_Print( "OSDEV_DeRegisterDevice - %s - Forcing close\n", Descriptor->Name );
	    Descriptor->CloseFn( &Descriptor->OpenContexts[i] );
	}

    if( Descriptor->Name != NULL )
	OSDEV_Free( Descriptor->Name );

    OSDEV_Free( Descriptor );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_LinkDevice(        char                    *Name,
							unsigned int             MajorNumber,
							unsigned int             MinorNumber )
{
int     i;

//

    OSDEV_Print( "OSDEV_LinkDevice - %s, Major %d, Minor %d\n", Name, MajorNumber, MinorNumber );

    for( i=0; i<OSDEV_MAXIMUM_DEVICE_LINKS; i++ )
	if( !OSDEV_DeviceList[i].Valid )
	{
	    OSDEV_DeviceList[i].MajorNumber     = MajorNumber;
	    OSDEV_DeviceList[i].MinorNumber     = MinorNumber;
	    OSDEV_DeviceList[i].Name            = OSDEV_Malloc( strlen( Name )+1 );

	    if( OSDEV_DeviceList[i].Name == NULL )
	    {
		OSDEV_Print( "OSDEV_LinkDevice - Unable to allocate memory for link name.\n" );
		return OSDEV_Error;
	    }
	    strcpy( OSDEV_DeviceList[i].Name, Name );

	    OSDEV_DeviceList[i].Valid           = true;
	    return OSDEV_NoError;
	}

    OSDEV_Print( "OSDEV_LinkDevice - All device links used.\n" );
    return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_UnLinkDevice(      char                    *Name )
{
int     i;

//

    for( i=0; i<OSDEV_MAXIMUM_DEVICE_LINKS; i++ )
	if( OSDEV_DeviceList[i].Valid && (strcmp(Name, OSDEV_DeviceList[i].Name) == 0) )
	{
	    OSDEV_Free( OSDEV_DeviceList[i].Name );
	    OSDEV_DeviceList[i].Valid           = false;
	    return OSDEV_NoError;
	}

    OSDEV_Print( "OSDEV_UnLinkDevice - Device not found.\n" );
    return OSDEV_Error;
}


#ifndef H_OSINLINE

// --------------------------------------------------------------
//      Specialist st200 intruction emulation functions

static inline unsigned int      __swapbw( unsigned int a )      // ByteSwap
{
unsigned int tmp1 = (a << 8) & 0xFF00FF00;
unsigned int tmp2 = (a >> 8) & 0x00FF00FF;
unsigned int tmp3 = tmp1 | tmp2;

    return ((tmp3 >> 16) | (tmp3 << 16));
}

//

static inline unsigned int      __lzcntw( unsigned int a )      // CountLeadingZeros
{
unsigned int    i = 0;
unsigned int    b;

    i = (a ? 0 : 1);

    b  = a & 0xffff0000;
    a  = (b ? b : a);
    i += (b ? 0 : 16);

    b  = a & 0xff00ff00;
    a = (b ? b : a);
    i += (b ? 0 : 8);

    b  = a & 0xf0f0f0f0;
    a  = (b ? b : a);
    i += (b ? 0 : 4);

    b  = a & 0xcccccccc;
    a  = (b ? b : a);
    i += (b ? 0 : 2);

    b  = a & 0xaaaaaaaa;
    i += (b ? 0 : 1);

    return i;
}

//

static inline unsigned int      __gethw( unsigned long long a )
{
    return (unsigned int)(a >> 32);
}

//

static inline unsigned int      __getlw( unsigned long long a )
{
    return (unsigned int)(a & 0xffffffff);
}

#endif

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_InitializeSemaphore( OSDEV_Semaphore_t     *Semaphore,
							  unsigned int           InitialCount )
{
    *Semaphore = (OSDEV_Semaphore_t)OSDEV_Malloc( sizeof(struct semaphore) );
    if( *Semaphore != NULL )
    {
	sema_init( *Semaphore, InitialCount );
	return OSDEV_NoError;
    }
    else
      return OSDEV_Error;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_ReInitializeSemaphore( OSDEV_Semaphore_t     *Semaphore,
							    unsigned int           InitialCount )
{
    sema_init( *Semaphore, InitialCount );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_DeInitializeSemaphore( OSDEV_Semaphore_t   *Semaphore )
{
    OSDEV_Free( *Semaphore );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_ClaimSemaphore(    OSDEV_Semaphore_t       *Semaphore )
{
    down( *Semaphore );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_ClaimSemaphoreInterruptible(    OSDEV_Semaphore_t       *Semaphore )
{
    if( down_interruptible( *Semaphore ) != 0 )
	return OSDEV_Error;
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_ReleaseSemaphore(  OSDEV_Semaphore_t       *Semaphore )
{
    up( *Semaphore );
    return OSDEV_NoError;
}


// -----------------------------------------------------------------------------------------------
// Wait queues - used to implement events

static inline OSDEV_Status_t    OSDEV_InitializeWaitQueue(      OSDEV_WaitQueue_t      *WaitQueue )
{
    *WaitQueue  = (OSDEV_WaitQueue_t)OSDEV_Malloc( sizeof(wait_queue_head_t) );
    if( *WaitQueue != NULL )
    {
	init_waitqueue_head( *WaitQueue );
	return OSDEV_NoError;
    }
    else
      return OSDEV_Error;
}

static inline OSDEV_Status_t    OSDEV_ReInitializeWaitQueue(    OSDEV_WaitQueue_t      WaitQueue )
{
    init_waitqueue_head( WaitQueue );
    return OSDEV_NoError;
}

static inline OSDEV_Status_t    OSDEV_DeInitializeWaitQueue(    OSDEV_WaitQueue_t      WaitQueue )
{
    OSDEV_Free( WaitQueue );
    return OSDEV_NoError;
}

static inline OSDEV_Status_t    OSDEV_WaitForQueue(             OSDEV_WaitQueue_t       WaitQueue,
								bool*                   Condition,
								unsigned int            Timeout )
{
    if (Timeout == OSDEV_INFINITE)
	wait_event( *WaitQueue, *Condition );
    else
	wait_event_timeout( *WaitQueue, *Condition, msecs_to_jiffies (Timeout) );
    return OSDEV_NoError;
}

static inline OSDEV_Status_t    OSDEV_WakeUpQueue (             OSDEV_WaitQueue_t       WaitQueue )
{
    wake_up( WaitQueue );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------


static inline void            *OSDEV_AllignedMalloc(            unsigned int             Allignment,
								unsigned int             Size )
{
void    *Base;
void    *Result;

    Result                      = NULL;
    Size                        = Size + sizeof(void *) + Allignment - 1;
    Base                        = OSDEV_Malloc( Size );

    if( Base != NULL )
    {
	Result                  = (void *)((((unsigned int)Base + sizeof(void *)) + (Allignment - 1)) & (~(Allignment-1)));
	((void **)Result)[-1]   = Base;
    }

    return Result;
}

// -----------------------------------------------------------------------------------------------


static inline void            *OSDEV_Calloc(                    unsigned int             Quantity,
								unsigned int             Size )
{
    void* Memory = OSDEV_Malloc(Quantity * Size);

    if( Memory != NULL )
	memset( Memory, '\0', Quantity * Size );
    return Memory;
}

// -----------------------------------------------------------------------------------------------


static inline OSDEV_Status_t   OSDEV_AllignedFree(              void                    *Address )
{
void    *Base;

    if( Address != NULL )
    {
	Base            = ((void **)Address)[-1];
	OSDEV_Free( Base );
    }
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

static inline unsigned int OSDEV_GetTimeInMilliSeconds(         void )
{
struct timeval  Now;

#if !defined (CONFIG_USE_SYSTEM_CLOCK)
ktime_t         Ktime   = ktime_get();

    Now                 = ktime_to_timeval(Ktime);

#else
    do_gettimeofday( &Now );

#endif

    return ((Now.tv_sec * 1000) + (Now.tv_usec/1000));
}

// -----------------------------------------------------------------------------------------------

static inline unsigned long long OSDEV_GetTimeInMicroSeconds(         void )
{
#if defined (CONFIG_USE_SYSTEM_CLOCK)
struct timeval  Now;

    do_gettimeofday( &Now );
    return (((unsigned long long)Now.tv_sec * 1000000LL) + (unsigned long long)Now.tv_usec);
#else
ktime_t         Ktime   = ktime_get();

    return ktime_to_us(Ktime);
#endif
}

// -----------------------------------------------------------------------------------------------

static inline void OSDEV_SleepMilliSeconds( unsigned int Value )
{
sigset_t allset, oldset;
unsigned int  Jiffies = ((Value*HZ)/1000)+1;

    /* Block all signals during the sleep (i.e. work correctly when called via ^C) */
    sigfillset(&allset);
    sigprocmask(SIG_BLOCK, &allset, &oldset);

    /* Sleep */
    set_current_state( TASK_INTERRUPTIBLE );
    schedule_timeout( Jiffies );

    /* Restore the original signal mask */
    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_CopyToDeviceSpace(         void                    *DeviceAddress,
								unsigned int             UserAddress,
								unsigned int             Size )
{
    memcpy( DeviceAddress, (void *)UserAddress, Size );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_CopyToUserSpace(           unsigned int             UserAddress,
								void                    *DeviceAddress,
								unsigned int             Size )
{
    memcpy( (void *)UserAddress, DeviceAddress, Size );
    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void            *OSDEV_TranslateAddressToUncached( void                   *Address )
{
    return (void *)((((unsigned int)Address) & 0x1fffffff) | 0xa0000000);
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void OSDEV_PurgeCacheAll(void)
{
    flush_cache_all();
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void OSDEV_FlushCacheRange(void *start, int size)
{
    dma_cache_wback(start, size);
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void OSDEV_InvalidateCacheRange(void *start, int size)
{
    dma_cache_inv(start, size);
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void OSDEV_SnoopCacheRange(void *start, int size)
{
	/* this can only be support on SH-4 platforms but it's
	 * capabilities are too good to ignore
	 */
#ifdef __SH4__
	extern void sh4_cache_snoop(void *p, unsigned int len);
	sh4_cache_snoop(start, size);
#endif
}

// -----------------------------------------------------------------------------------------------

STATIC_INLINE void OSDEV_PurgeCacheRange(void *start, int size)
{
    dma_cache_wback_inv(start, size);
}

// -----------------------------------------------------------------------------------------------

struct ThreadInfo_s {
  OSDEV_ThreadFn_t       thread;
  void                  *args;
  unsigned int          priority;
};

static int             OSDEV_CreateThreadHelper(                void                    *p )
{
struct ThreadInfo_s *t=(struct ThreadInfo_s*)p;

    t->thread(t->args);
    kfree(t);

    return 0;
}

static inline OSDEV_Status_t   OSDEV_CreateThread( OSDEV_Thread_t          *Thread,
						   OSDEV_ThreadFn_t         Entrypoint,
						   OSDEV_ThreadParam_t      Parameter,
						   const char              *Name,
						   OSDEV_ThreadPriority_t   Priority )
{
struct ThreadInfo_s *t = (struct ThreadInfo_s *)kmalloc(sizeof(struct ThreadInfo_s),GFP_KERNEL);
struct sched_param    Param;
struct task_struct          *Taskp;

    t->thread    = Entrypoint;
    t->args      = Parameter;
    t->priority  = Priority;


    /* create the task and leave it suspended */
    Taskp = kthread_create(OSDEV_CreateThreadHelper, t, "%s", Name);
    if (IS_ERR(Taskp)) {
	return OSDEV_Error;
    }
    Taskp->flags |= PF_NOFREEZE;

    /* switch to real time scheduling (if requested) */
    if (0 != Priority) {
	Param.sched_priority = Priority;
	if (0 != sched_setscheduler(Taskp, SCHED_RR, &Param)) {
	    printk(KERN_ERR "FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Priority);
	    Priority = 0;
	}
    }

    wake_up_process(Taskp);

    *Thread = Taskp;

    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

#define OSDEV_ExitThread()              return

// -----------------------------------------------------------------------------------------------

static inline OSDEV_Status_t   OSDEV_SetPriority( OSDEV_ThreadPriority_t   Priority )
{
    int Result;
    struct sched_param Param = { .sched_priority = Priority }; 

    /* switch to real time scheduling (if requested) */
    Result = sched_setscheduler(current, (Priority ? SCHED_RR : SCHED_NORMAL), &Param);
    if (0 != Result) {
	printk(KERN_ERR "FAILED to set scheduling parameters to priority %d (%s)\n",
	       Priority, (Priority ? "SCHED_RR" : "SCHED_NORMAL"));
	return OSDEV_Error;
    }

    return OSDEV_NoError;
}

// -----------------------------------------------------------------------------------------------

static inline char *OSDEV_ThreadName(void)
{
    return current->comm;
}

// -----------------------------------------------------------------------------------------------
//
// The entry/exit macros for use on each of the device fns
// and the access macros to store and retrieve context
//
// During the functions, variables such as OSDEV_PrivateData,
// and OSDEV_MinorDeviceNumber are available. The private data
// is stored on exit, all others are not.
//

#define OSDEV_OpenEntry()       void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;

#define OSDEV_OpenExit(Status)  OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
				(void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
				return Status;

//

#define OSDEV_CloseEntry()      void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;


#define OSDEV_CloseExit(Status) OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
				(void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
				return Status;

//

#define OSDEV_WriteEntry()      void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				const char      *OSDEV_Buffer                   = buf;                                  \
				size_t           OSDEV_Count                    = count;                                \
				loff_t          *OSDEV_Offset                   = ppos;


#define OSDEV_WriteExit(Status) filp->private_data                              = OSDEV_PrivateData;    \
				return (Status == OSDEV_NoError) ? 0 : -1;

//

#define OSDEV_PollEntry()       void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				poll_table      *OSDEV_PollTable                = wait;

#define OSDEV_PollExit(Status)  OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
				return Status;

//

#define OSDEV_IoctlEntry()      void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;       \
				unsigned int     OSDEV_IoctlCode                = cmd;                                  \
				unsigned int     OSDEV_ParameterAddress         = arg;


#define OSDEV_IoctlExit(Status) OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
				(void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
				return Status;

//

#define OSDEV_MmapEntry()       void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
				unsigned char   *OSDEV_VirtualMemoryStart       = NULL;                                 \
				unsigned char   *OSDEV_VirtualMemoryEnd         = (unsigned char *)OSDEV_VMSize;        \
				unsigned int     OSDEV_Offset                   = OSDEV_VMOffset;

#define OSDEV_MmapExit(Status)  OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;                    \
				*OSDEV_VMAddress                                = OSDEV_VirtualMemoryStart;             \
				(void) OSDEV_VirtualMemoryEnd; /* warning suppression */                                \
				(void) OSDEV_Offset; /* warning suppression */                                          \
				return Status;

#define OSDEV_MmapPerformMap(PhysicalAddress, MappingStatus)                                                            \
				{                                                                                       \
				    MappingStatus                               = OSDEV_NoError;                        \
				    OSDEV_VirtualMemoryStart                    = PhysicalAddress;                      \
				}

// -----------------------------------------------------------------------------------------------
//
// The initialization/termination macros. These are export the name of the appropriate functions
// in systems (such as linux) where execution of these functions is automatic
//

#define OSDEV_RegisterDeviceLoadFn( Fn )                module_init(Fn)
#define OSDEV_RegisterDeviceUnloadFn( Fn )              module_exit(Fn)

#define OSDEV_ExportSymbol( Fn )                        EXPORT_SYMBOL(Fn)

//

#define OSDEV_LoadEntry()

#define OSDEV_LoadExit(Status)                          return (Status == OSDEV_NoError) ? 0 : -1;


//

#define OSDEV_UnloadEntry()

/*#define OSDEV_UnloadExit(Status)                        return Status;*/
#define OSDEV_UnloadExit(Status)                        return;


// -----------------------------------------------------------------------------------------------
//
// Macros to access to IO space
//

static inline unsigned int OSDEV_IOReMap( unsigned int BaseAddress, unsigned int Size )
{
    return (unsigned int)ioremap_nocache( BaseAddress, Size );
}


static inline void OSDEV_IOUnMap( unsigned int MapAddress )
{
//    OSDEV_Print("Unmapping %x\n",MapAddress);
    iounmap( (void*)MapAddress );
}


static inline unsigned int OSDEV_PeripheralAddress( unsigned int CPUAddress )
{
    return virt_to_phys( (void *) CPUAddress );
}


static inline unsigned int OSDEV_ReadLong( unsigned int Address )
{
    return (unsigned int)readl( Address );
}


static inline unsigned int OSDEV_ReadByte( unsigned int Address )
{
    return (unsigned int)readb( Address );
}


static inline void OSDEV_WriteLong( unsigned int Address, unsigned int Value )
{
    writel( Value, Address );
}


static inline void OSDEV_WriteByte( unsigned int Address, unsigned int Value )
{
    writeb( Value, Address );
}

#endif // __cplusplus

#endif // H_OSDEV_DEVICE
