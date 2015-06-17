/***********************************************************************
 *
 * File: os21/stglib/stgos21.c
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplaytypes.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <os21.h>
#if defined(__SH4__)
#include <os21/st40.h>
#include <os21/st40/cache.h>
#elif defined(__ST200__)
#include <os21/st200.h>
#include <os21/st200/cache.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#if (OS21_VERSION_MAJOR <= 2) && (OS21_VERSION_MINOR < 4)
extern void _kernel_printf(const char *fmt,...);
#define kernel_printf _kernel_printf
#endif

static const ULONG botguard0  = 0x12345678;
static const ULONG botguard1  = 0x87654321;
static const ULONG topguard0  = 0x89abcdef;
static const ULONG topguard1  = 0xfedcba98;
static const ULONG guardfreed = 0x000f3eed;


static semaphore_t *g_BlitterWaitQueue = 0;
static semaphore_t *g_BlitEngineQueue = 0;

static inline void system_stop(void)
{
  *((char*)0) = 0;
}


static int Init(void)
{
  g_BlitterWaitQueue = semaphore_create_fifo(0);
  g_BlitEngineQueue = semaphore_create_fifo(1);

  return 0;
}


static void Release(void) {}

static semaphore_t* GetWaitQueue(WAIT_QUEUE queue)
{
  switch(queue)
  {
    case BLIT_ENGINE_QUEUE:
      return g_BlitEngineQueue;

    case BLIT_NODE_QUEUE:
      return g_BlitterWaitQueue;

    default:
      return NULL;
  }
}


static int SleepOnQueue(WAIT_QUEUE queue,
			volatile unsigned long* var,
			unsigned long value,
			int cond)
{
  semaphore_t* pQueue = GetWaitQueue(queue);

  if(!pQueue)
  {
    printf("IOS::WakeQueue invalid queue ID\n");
    *((char*)0) = 0; /* Cause exception */
  }

  if(!var)
  {
    semaphore_wait(pQueue);
  }
  else
  {
    task_lock();

    /*
     * Two conditions, wait either until *var == value or *var != value
     */
    while(!(cond?(*var == value):(*var != value)))
    {
      semaphore_wait(pQueue);
    }

    task_unlock();
  }

  return 0;
}


static void WakeQueue(WAIT_QUEUE queue)
{
  semaphore_t* pQueue = GetWaitQueue(queue);

  semaphore_signal(pQueue);
}


static ULONG ReadRegister(volatile ULONG *reg)
{
  return *reg;
}

static void WriteRegister(volatile ULONG *reg,ULONG val)
{
  *reg = val;
}


static UCHAR ReadByteRegister(volatile UCHAR *reg)
{
  return *reg;
}


static void WriteByteRegister(volatile UCHAR *reg,UCHAR val)
{
  *reg = val;
}


static void *AllocateMemory(ULONG size)
{
  char  *mem   = (char*)malloc(size+(sizeof(ULONG)*4));
  ULONG *guard = (ULONG *)mem;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: AllocateMemory called from interrupt context\n");
    system_stop();
  }

  if(mem == 0)
  {
    kernel_printf("AllocateMemory: unable to malloc %lu bytes\n",size);
    return 0;
  }

  guard[0] = size;
  guard[1] = 0x87654321;

  guard = (ULONG *)(mem+size+(sizeof(ULONG)*2));

  guard[0] = 0x89abcdef;
  guard[1] = 0xfedcba98;

  return mem+(sizeof(ULONG)*2);
}


static void FreeMemory(void *mem)
{
  ULONG *guard;
  ULONG *topguard;
  ULONG size;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: FreeMemory called from interrupt context\n");
    system_stop();
  }

  if(mem == 0)
    return;

  guard = (ULONG *)((char*)mem - (sizeof(ULONG)*2));

  size  = guard[0];

  if(guard[1] != botguard1)
  {
    kernel_printf("FreeMemory: bad guard before data mem = %p guard[0] = %lu guard[1] = %lu\n",mem,size,guard[1]);
    kernel_printf("FreeMemory: **************** freeing anyway ******************\n");
  }
  else
  {
    topguard = (ULONG *)((char*)mem+size);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      kernel_printf("FreeMemory: bad guard after data mem = %p topguard[0] = %lu topguard[1] = %lu\n",mem,topguard[0],topguard[1]);
      kernel_printf("FreeMemory: **************** freeing anyway ******************\n");
    }
  }

  guard[0] = guardfreed;
  guard[1] = guardfreed;

  free(guard);

  return;
}


static TIME64 GetSystemTime(void)
{
  return time_now();
}


static TIME64 GetOneSecond(void)
{
  return time_ticks_per_sec();
}


static void StallExecution(ULONG ulDelay)
{
  task_delay(ulDelay * time_ticks_per_sec()/1000000);
}


static void* MapMemory(ULONG ulBaseAddr, ULONG ulSize)
{
  void *map = vmem_create((void*)ulBaseAddr,ulSize, NULL, (VMEM_CREATE_UNCACHED|VMEM_CREATE_READ|VMEM_CREATE_WRITE));
  if(map != NULL)
    return map;

  /*
   * Mapping the 7200 register base fails, but that is ok because the address
   * is directly usable, so just return it back.
   */
  return (void*)ulBaseAddr;
}


static void UnMapMemory(void* pVMem)
{
  vmem_delete(pVMem);
}


static void ZeroMemory(void* pMem, unsigned long count)
{
  memset(pMem, 0, count);
}


static int CheckDMAAreaGuards(DMA_Area *mem)
{
  ULONG *guard,*topguard;

  if(!mem->pMemory)
    return 0;

  guard = (ULONG*)mem->pMemory;
  if((guard[0] != botguard0) || (guard[1] != botguard1))
  {
    kernel_printf("CheckDMAAreaGuards: bad guard before data mem = %p pMemory = %p guard[0] = %lu guard[1] = %lu\n",mem,mem->pMemory,guard[0],guard[1]);
    return -1;
  }
  else
  {
    topguard = (ULONG *)(mem->pData+mem->ulDataSize);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      kernel_printf("CheckDMAAreaGuards: bad guard after data mem = %p pMemory = %p guard[0] = %lu guard[1] = %lu\n",mem,mem->pMemory,topguard[0],topguard[1]);
      return -1;
    }
  }

  return 0;
}


static void MemsetDMAArea(DMA_Area *mem, ULONG offset, int val, unsigned long count)
{
  if((offset+count) > mem->ulDataSize)
  {
    kernel_printf("MemsetDMAArea: access out of range, size = %lu offset = %lu count = %lu\n",mem->ulDataSize,offset,count);
  }

  memset(mem->pData+offset,val,count);
  cache_purge_data(mem->pData+offset,count);

  CheckDMAAreaGuards(mem);
}


static void MemcpyToDMAArea(DMA_Area *mem, ULONG offset, const void* pSrc, unsigned long count)
{
  if((offset+count) > mem->ulDataSize)
  {
    kernel_printf("MemcpyToDMAArea: access out of range, size = %lu offset = %lu count = %lu\n",mem->ulDataSize,offset,count);
  }

  memcpy(mem->pData+offset,pSrc,count);
  cache_purge_data(mem->pData+offset,count);

  CheckDMAAreaGuards(mem);
}


static void AllocateDMAArea(DMA_Area *mem, ULONG size, ULONG align, STM_DMA_AREA_ALLOC_FLAGS flags)
{
  ULONG *guard;
  void  *physptr;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: AllocateDMAArea called from interrupt context\n");
    system_stop();
  }

  memset(mem, 0, sizeof(DMA_Area));

  if(align<8)
    align = 8; /* Allow room for top and bottom memory guards */

  if(!(flags & (SDAAF_VIDEO_MEMORY|SDAAF_UNCACHED)))
  {
    mem->ulAllocatedSize = size+(align*2);
    mem->ulDataSize      = size;
    mem->pMemory         = malloc(mem->ulAllocatedSize);
  }

  if(mem->pMemory == 0)
  {
    memset(mem, 0, sizeof(DMA_Area));
    return;
  }

  /* Align the data area in the standard fashion */
  mem->pData = (char*)(((ULONG)mem->pMemory + (align-1)) & ~(align-1));
  /* Now move it up to allow room for the bottom memory guard */
  mem->pData += align;

  vmem_virt_to_phys(mem->pData,&physptr);
  mem->ulPhysical = (ULONG)physptr;

  guard = (ULONG*)mem->pMemory; /* Note: NOT pData!!! */
  guard[0] = botguard0;
  guard[1] = botguard1;

  guard = (ULONG*)(mem->pData+mem->ulDataSize);
  guard[0] = topguard0;
  guard[1] = topguard1;

  cache_purge_data(mem->pMemory, mem->ulAllocatedSize);
}


static void FreeDMAArea(DMA_Area *mem)
{
  ULONG *guard;

  if(!mem->pMemory)
    return;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: FreeDMAArea called from interrupt context\n");
    system_stop();
  }

  if(CheckDMAAreaGuards(mem)<0)
    kernel_printf("FreeDMAArea: ******************* freeing corrupted memory ***************\n");

  guard = (ULONG*)mem->pMemory;
  guard[0] = guardfreed;
  guard[1] = guardfreed;

  free(mem->pMemory);

  memset(mem, 0, sizeof(DMA_Area));
}


/*
 * Currently no FDMA support in OS21
 */
static DMA_Channel *GetDMAChannel(STM_DMA_TRANSFER_PACING pacing, ULONG bytes_per_req, STM_DMA_TRANSFER_FLAGS flags)
{
  return NULL;
}


static void ReleaseDMAChannel(DMA_Channel *channel) {}
static void StopDMAChannel(DMA_Channel *channel) {}


static void* CreateDMATransfer(DMA_Channel *channel, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags)
{
  return NULL;
}


static void  DeleteDMATransfer(void *handle) {}

static int   StartDMATransfer(void *handle)
{
  return -1;
}


static ULONG CreateResourceLock(void)
{
  return 1;
}


static void DeleteResourceLock(ULONG ulHandle)
{
}


static void LockResource(ULONG ulHandle)
{
  interrupt_lock();
}


static void UnlockResource(ULONG ulHandle)
{
  interrupt_unlock();
}


static ULONG CreateSemaphore(int initVal)
{
  semaphore_t *pSema = semaphore_create_fifo(initVal);
  return (unsigned long)pSema;
}


static void DeleteSemaphore(ULONG ulHandle)
{
  semaphore_delete((semaphore_t*)ulHandle);
}


static int DownSemaphore(ULONG ulHandle)
{
  semaphore_t *pSema = (semaphore_t *)ulHandle;
  semaphore_wait(pSema);
  return 0;
}


static void UpSemaphore(ULONG ulHandle)
{
  semaphore_t *pSema = (semaphore_t *)ulHandle;
  semaphore_signal(pSema);
}


static void FlushCache(void *vaddr, int size)
{
  cache_purge_data(vaddr,size);
}


static void ReleaseFirmware(const STMFirmware *fw)
{
  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: ReleaseFirmware called from interrupt context\n");
    system_stop();
  }

  free(fw->pData);
  free((void*)fw);
  return;
}

/*
 * Simple firmware loading over microconnect.
 *
 * This is not safe from interrupt context, but that is OK because changes to
 * AWG done in UpdateHW processing is actually in task context in OS21, not
 * interrupt context (unlike Linux).
 */
static int RequestFirmware(const STMFirmware **firmware_p,
                           const char         *const name)
{
  STMFirmware *fw = NULL;
  int fd,ret;
  struct stat buf;

  if (!firmware_p
      || *firmware_p
      || !name
      || !*name)
  {
    kernel_printf("BUG: RequestFirmware invalid arguments\n");
    return -1;
  }

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: RequestFirmware called from interrupt context\n");
    system_stop();
  }

  fw = malloc(sizeof(STMFirmware));
  if(!fw)
    return -1;

  memset(fw,0,sizeof(STMFirmware));

  fd = open(name,O_RDONLY | O_BINARY);
  if(fd<0)
  {
    kernel_printf("BUG: Cannot find AWG firmware file\n");
    free(fw);
    return fd;
  }

  if(fstat(fd,&buf)<0)
  {
    kernel_printf("BUG: Cannot stat AWG firmware file\n");
    close(fd);
    free(fw);
    return -1;
  }

  fw->ulDataSize = buf.st_size;

  fw->pData = malloc(fw->ulDataSize);
  if(!fw->pData)
  {
    kernel_printf("BUG: Cannot allocate AWG firmware cache memory\n");
    free(fw);
    close(fd);
    return -1;
  }

  memset(fw->pData,0,fw->ulDataSize);

  if((ret = read(fd,fw->pData,fw->ulDataSize))<0)
  {
    kernel_printf("BUG: Cannot read AWG firmware cache memory\n");
    free(fw->pData);
    free(fw);
    close(fd);
    return -1;
  }

  if(ret != fw->ulDataSize)
  {
    printf("BUG: Read AWG firmware returned wrong number of bytes %d instead of %lu\n",ret,fw->ulDataSize);
    free(fw->pData);
    free(fw);
    close(fd);
    return -1;
  }

  *firmware_p = fw;

  close(fd);
  return 0;
}


static int
__attribute__ ((format (printf, 3, 4)))
Snprintf(char *buf, ULONG size, const char *fmt, ...)
{
  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,size,fmt,args);
  va_end(args);
  return i;
}


static int
AtomicCmpXchg(ATOMIC *atomic, int oldval, int newval)
{
  int retval;

  interrupt_lock();

  retval = atomic->counter;
  if (retval == oldval)
    atomic->counter = newval;

  interrupt_unlock();

  return retval;
}

static void
AtomicSet(ATOMIC *atomic, int val)
{
  interrupt_lock();

  atomic->counter = val;

  interrupt_unlock();
}

static void
Barrier(void)
{
  /* FIXME: 'synco' is only available starting w/ ST-300 series, i.e.
     not STb7109 and it is not understood by the STLinux-2.2 binutils!
     But it seems to do no harm on CPUs that don't implement it. */
#ifdef __SH4__
  __asm__ __volatile__ ("synco" : : : "memory");
#else
//  __asm__ __volatile__ ("" : : : "memory")
#error Please implement Barrier() for this CPU first
#endif
}


static ULONG
CallBios (long         call,
          unsigned int n_args, ...)
{
  return 0 /* BIOS_FAIL */;
}


static IOS devFBIOS = {
  .Init               = Init,
  .Release            = Release,

  .ReadRegister       = ReadRegister,
  .WriteRegister      = WriteRegister,
  .ReadByteRegister   = ReadByteRegister,
  .WriteByteRegister  = WriteByteRegister,

  .GetSystemTime      = GetSystemTime,
  .GetOneSecond       = GetOneSecond,
  .StallExecution     = StallExecution,

  .MapMemory          = MapMemory,
  .UnMapMemory        = UnMapMemory,

  .ZeroMemory         = ZeroMemory,
  .MemsetDMAArea      = MemsetDMAArea,
  .MemcpyToDMAArea    = MemcpyToDMAArea,
  .CheckDMAAreaGuards = CheckDMAAreaGuards,

  .GetDMAChannel      = GetDMAChannel,
  .ReleaseDMAChannel  = ReleaseDMAChannel,
  .StopDMAChannel     = StopDMAChannel,
  .CreateDMATransfer  = CreateDMATransfer,
  .DeleteDMATransfer  = DeleteDMATransfer,
  .StartDMATransfer   = StartDMATransfer,

  .AllocateMemory     = AllocateMemory,
  .FreeMemory         = FreeMemory,

  .AllocateDMAArea    = AllocateDMAArea,
  .FreeDMAArea        = FreeDMAArea,

  .FlushCache         = FlushCache,

  /* maybe a FIXME? But no code should in effect use the result of this on
     OS21 */
  .OSPageSize         = 4096,

  .CreateResourceLock = CreateResourceLock,
  .DeleteResourceLock = DeleteResourceLock,
  .LockResource       = LockResource,
  .UnlockResource     = UnlockResource,

  .CreateSemaphore    = CreateSemaphore,
  .DeleteSemaphore    = DeleteSemaphore,
  .DownSemaphore      = DownSemaphore,
  .UpSemaphore        = UpSemaphore,

  .WakeQueue          = WakeQueue,
  .SleepOnQueue       = SleepOnQueue,

  .RequestFirmware    = RequestFirmware,
  .ReleaseFirmware    = ReleaseFirmware,

  .snprintf           = Snprintf,

  .AtomicCmpXchg      = AtomicCmpXchg,
  .AtomicSet          = AtomicSet,
  .Barrier            = Barrier,

  .CallBios           = CallBios,
};

IOS *g_pIOS = &devFBIOS;

/****************************************************************************
 * IDebug implementation
 */
static int  IDebugInit(void)
{
  return 0;
}

static void IDebugRelease(void) {}

static void IDebugBreak(const char *cond, const char *file, unsigned int line)
{
  if(task_context(NULL,NULL) == task_context_task)
  {
    fprintf(stderr, "\n%s:%u:FATAL ASSERTION FAILURE %s.\n", file, line, cond);
    fflush(stderr);
  }
  else
  {
    kernel_printf("\n%s:%u:FATAL ASSERTION FAILURE %s.\n", file, line, cond);
  }

  system_stop();
}


static void IDebugPrintf(const char *fmt,...)
{
  if(task_context(NULL,NULL) == task_context_task)
  {
    va_list ap;
    va_start(ap,fmt);
    vfprintf(stderr,fmt,ap);
    va_end(ap);
  }
  else
  {
    kernel_printf("DEBUG In Interrupt Context: \"%s\"\n",fmt);
  }
}

static IDebug devFBDebug = {
   .Init         = IDebugInit,
   .Release      = IDebugRelease,
   .IDebugPrintf = IDebugPrintf,
   .IDebugBreak  = IDebugBreak
};

IDebug *g_pIDebug = &devFBDebug;
