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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>

#include <stm_display_types.h>

#include <vibe_os.h>
#include <vibe_debug.h>


#if (OS21_VERSION_MAJOR <= 2) && (OS21_VERSION_MINOR < 4)
extern void _kernel_printf(const char *fmt,...);
#define kernel_printf _kernel_printf
#endif

static const uint32_t botguard0  = 0x12345678;
static const uint32_t botguard1  = 0x87654321;
static const uint32_t topguard0  = 0x89abcdef;
static const uint32_t topguard1  = 0xfedcba98;
static const uint32_t guardfreed = 0x000f3eed;


static semaphore_t *g_BlitterWaitQueue = 0;
static semaphore_t *g_BlitEngineQueue = 0;

static inline void system_stop(void)
{
  *((char*)0) = 0;
}


int vibe_os_init(void)
{
  g_BlitterWaitQueue = semaphore_create_fifo(0);
  g_BlitEngineQueue = semaphore_create_fifo(1);

  return 0;
}


void vibe_os_release(void) {}

bool vibe_os_create_thread(VIBE_OS_ThreadFct_t            fct,
                         void                          *parameter,
                         const char                    *name,
                         void                         **thread_desc)
{
  /* Not implemented */
  return false;
}

int vibe_os_stop_thread(void *thread_desc)
{
  /* Not implemented */
  return 0;
}

bool vibe_os_thread_should_stop(void)
{
  /* Not implemented */
  return false;
}

void vibe_os_sleep_ms(uint32_t ms)
{
  task_delay((ms * time_ticks_per_sec())/1000);
}

uint32_t vibe_os_read_register(volatile uint32_t *baseAddress, uint32_t offset)
{
  return *(baseAddress + (offset>>2));
}

void vibe_os_write_register(volatile uint32_t *baseAddress, uint32_t offset, uint32_t val)
{
  *(baseAddress + (offset>>2)) = val;
}

uint8_t vibe_os_read_byte_register(volatile uint8_t *reg)
{
  return *reg;
}


void vibe_os_write_byte_register(volatile uint8_t *reg,uint8_t val)
{
  *reg = val;
}


void *vibe_os_allocate_memory(uint32_t size)
{
  char  *mem   = (char*)malloc(size+(sizeof(uint32_t)*4));
  uint32_t *guard = (uint32_t *)mem;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_allocate_memory called from interrupt context\n");
    system_stop();
  }

  if(mem == 0)
  {
    kernel_printf("vibe_os_allocate_memory: unable to malloc %u bytes\n",size);
    return 0;
  }

  guard[0] = size;
  guard[1] = 0x87654321;

  guard = (uint32_t *)(mem+size+(sizeof(uint32_t)*2));

  guard[0] = 0x89abcdef;
  guard[1] = 0xfedcba98;

  return mem+(sizeof(uint32_t)*2);
}


void vibe_os_free_memory(void *mem)
{
  uint32_t *guard;
  uint32_t *topguard;
  uint32_t size;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_free_memory called from interrupt context\n");
    system_stop();
  }

  if(mem == 0)
    return;

  guard = (uint32_t *)((char*)mem - (sizeof(uint32_t)*2));

  size  = guard[0];

  if(guard[1] != botguard1)
  {
    kernel_printf("vibe_os_free_memory: bad guard before data mem = %p guard[0] = %u guard[1] = %u\n",mem,size,guard[1]);
    kernel_printf("vibe_os_free_memory: **************** freeing anyway ******************\n");
  }
  else
  {
    topguard = (uint32_t *)((char*)mem+size);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      kernel_printf("vibe_os_free_memory: bad guard after data mem = %p topguard[0] = %u topguard[1] = %u\n",mem,topguard[0],topguard[1]);
      kernel_printf("vibe_os_free_memory: **************** freeing anyway ******************\n");
    }
  }

  guard[0] = guardfreed;
  guard[1] = guardfreed;

  free(guard);

  return;
}


stm_time64_t vibe_os_get_system_time(void)
{
  return time_now();
}


stm_time64_t vibe_os_get_one_second(void)
{
  return time_ticks_per_sec();
}


void vibe_os_stall_execution(uint32_t ulDelay)
{
  task_delay(ulDelay * time_ticks_per_sec()/1000000);
}


void* vibe_os_map_memory(uint32_t ulBaseAddr, uint32_t ulSize)
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


void vibe_os_unmap_memory(void* pVMem)
{
  vmem_delete(pVMem);
}


void vibe_os_zero_memory(void* pMem, uint32_t count)
{
  memset(pMem, 0, count);
}

int vibe_os_memcmp(const void *pMem1, const void *pMem2, uint32_t size)
{
    return memcmp(pMem1, pMem2, size);
}

int vibe_os_check_dma_area_guards(DMA_Area *mem)
{
  uint32_t *guard,*topguard;

  if(!mem->pMemory)
    return 0;

  guard = (uint32_t*)mem->pMemory;
  if((guard[0] != botguard0) || (guard[1] != botguard1))
  {
    kernel_printf("vibe_os_check_dma_area_guards: bad guard before data mem = %p pMemory = %p guard[0] = %u guard[1] = %u\n",mem,mem->pMemory,guard[0],guard[1]);
    return -1;
  }
  else
  {
    topguard = (uint32_t *)(mem->pData+mem->ulDataSize);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      kernel_printf("vibe_os_check_dma_area_guards: bad guard after data mem = %p pMemory = %p guard[0] = %u guard[1] = %u\n",mem,mem->pMemory,topguard[0],topguard[1]);
      return -1;
    }
  }

  return 0;
}


void vibe_os_memset_dma_area(DMA_Area *mem, uint32_t offset, int val, unsigned long count)
{
  if((offset+count) > mem->ulDataSize)
  {
    kernel_printf("vibe_os_memset_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
  }

  memset(mem->pData+offset,val,count);
  if (!(mem->ulFlags & SDAAF_UNCACHED))
    cache_purge_data(mem->pData+offset,count);

  vibe_os_check_dma_area_guards(mem);
}


void vibe_os_memcpy_to_dma_area(DMA_Area *mem, uint32_t offset, const void* pSrc, uint32_t count)
{
  if((offset+count) > mem->ulDataSize)
  {
    kernel_printf("vibe_os_memcpy_to_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
  }

  memcpy(mem->pData+offset,pSrc,count);
  if (!(mem->ulFlags & SDAAF_UNCACHED))
    cache_purge_data(mem->pData+offset,count);

  vibe_os_check_dma_area_guards(mem);
}


void vibe_os_memcpy_from_dma_area(DMA_Area *mem, uint32_t offset, void* pDst, uint32_t count)
{
  if((offset > mem->ulDataSize) ||
     (count  > mem->ulDataSize) ||
     ((offset+count) > mem->ulDataSize))
  {
    kernel_printf("vibe_os_memcpy_from_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
  }

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    cache_invalidate_data(mem->pData+offset,count);

  memcpy(pDst, mem->pData+offset,count);

  vibe_os_check_dma_area_guards(mem);
}


void vibe_os_flush_dma_area(DMA_Area *mem, uint32_t offset, uint32_t count)
{
  if((offset+count) > mem->ulDataSize)
  {
    kernel_printf("vibe_os_flush_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
  }

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    cache_purge_data(mem->pData+offset,count);
}


void vibe_os_allocate_dma_area(DMA_Area *mem, uint32_t size, uint32_t align, STM_DMA_AREA_ALLOC_FLAGS flags)
{
  uint32_t *guard;
  void  *physptr;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_allocate_dma_area called from interrupt context\n");
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
  mem->pData = (char*)(((uint32_t)mem->pMemory + (align-1)) & ~(align-1));
  /* Now move it up to allow room for the bottom memory guard */
  mem->pData += align;

  vmem_virt_to_phys(mem->pData,&physptr);
  mem->ulPhysical = (uint32_t)physptr;

  guard = (uint32_t*)mem->pMemory; /* Note: NOT pData!!! */
  guard[0] = botguard0;
  guard[1] = botguard1;

  guard = (uint32_t*)(mem->pData+mem->ulDataSize);
  guard[0] = topguard0;
  guard[1] = topguard1;

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    cache_purge_data(mem->pMemory, mem->ulAllocatedSize);
}


void vibe_os_free_dma_area(DMA_Area *mem)
{
  uint32_t *guard;

  if(!mem->pMemory)
    return;

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_free_dma_area called from interrupt context\n");
    system_stop();
  }

  if(vibe_os_check_dma_area_guards(mem)<0)
    kernel_printf("vibe_os_free_dma_area: ******************* freeing corrupted memory ***************\n");

  guard = (uint32_t*)mem->pMemory;
  guard[0] = guardfreed;
  guard[1] = guardfreed;

  free(mem->pMemory);

  memset(mem, 0, sizeof(DMA_Area));
}


/*
 * Currently no FDMA support in OS21
 */
DMA_Channel *vibe_os_get_dma_channel(STM_DMA_TRANSFER_PACING pacing, uint32_t bytes_per_req, STM_DMA_TRANSFER_FLAGS flags)
{
  return NULL;
}


void vibe_os_release_dma_channel(DMA_Channel *channel) {}
void vibe_os_stop_dma_channel(DMA_Channel *channel) {}


void* vibe_os_create_dma_transfer(DMA_Channel *channel, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags)
{
  return NULL;
}


void  vibe_os_delete_dma_transfer(void *handle) {}

int   vibe_os_start_dma_transfer(void *handle)
{
  return -1;
}


uint32_t vibe_os_create_resource_lock(void)
{
  return 1;
}


void vibe_os_delete_resource_lock(uint32_t ulHandle)
{
}


void vibe_os_lock_resource(uint32_t ulHandle)
{
  interrupt_lock();
}


void vibe_os_unlock_resource(uint32_t ulHandle)
{
  interrupt_unlock();
}


uint32_t vibe_os_create_semaphore(int initVal)
{
  semaphore_t *pSema = semaphore_create_fifo(initVal);
  return (unsigned long)pSema;
}


void vibe_os_delete_semaphore(uint32_t ulHandle)
{
  semaphore_delete((semaphore_t*)ulHandle);
}


int vibe_os_down_semaphore(uint32_t ulHandle)
{
  semaphore_t *pSema = (semaphore_t *)ulHandle;
  semaphore_wait(pSema);
  return 0;
}


void vibe_os_up_semaphore(uint32_t ulHandle)
{
  semaphore_t *pSema = (semaphore_t *)ulHandle;
  semaphore_signal(pSema);
}


void vibe_os_release_firmware(const STMFirmware *fw)
{
  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_release_firmware called from interrupt context\n");
    system_stop();
  }

  free((void*)fw->pData);
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
int vibe_os_request_firmware(const STMFirmware **firmware_p,
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
    kernel_printf("BUG: vibe_os_request_firmware invalid arguments\n");
    return -1;
  }

  if(task_context(NULL,NULL) == task_context_system)
  {
    kernel_printf("BUG: vibe_os_request_firmware called from interrupt context\n");
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

  memset((void*)fw->pData,0,fw->ulDataSize);

  if((ret = read(fd,(void*)fw->pData,fw->ulDataSize))<0)
  {
    kernel_printf("BUG: Cannot read AWG firmware cache memory\n");
    free((void*)fw->pData);
    free(fw);
    close(fd);
    return -1;
  }

  if(ret != fw->ulDataSize)
  {
    printf("BUG: Read AWG firmware returned wrong number of bytes %d instead of %u\n",ret,fw->ulDataSize);
    free((void*)fw->pData);
    free(fw);
    close(fd);
    return -1;
  }

  *firmware_p = fw;

  close(fd);
  return 0;
}


int
__attribute__ ((format (printf, 3, 4)))
vibe_os_snprintf(char *buf, uint32_t size, const char *fmt, ...)
{
  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,size,fmt,args);
  va_end(args);
  return i;
}

uint32_t
vibe_os_call_bios (long         call,
          unsigned int n_args, ...)
{
  return 0 /* BIOS_FAIL */;
}


uint64_t vibe_os_div64(uint64_t n, uint64_t d)
{
  return (n / d);
}

/****************************************************************************
 * IDebug implementation
 */
int  vibe_debug_init(void)
{
  return 0;
}

void vibe_debug_release(void) {}

void vibe_break(const char *cond, const char *file, unsigned int line)
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


void vibe_printf(const char *fmt,...)
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

void vibe_kpprintf(const char *fmt,...)
{
/* Feature not available, so empty */
}


