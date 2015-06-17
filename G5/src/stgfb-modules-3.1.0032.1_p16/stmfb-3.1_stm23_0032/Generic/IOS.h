/***********************************************************************
 *
 * File: Generic/IOS.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef IOS_H
#define IOS_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef enum
{
  BLIT_ENGINE_QUEUE,
  BLIT_NODE_QUEUE
} WAIT_QUEUE;

#define STMIOS_WAIT_COND_NOT_EQUAL 0
#define STMIOS_WAIT_COND_EQUAL 1

typedef struct
{
  ULONG     ulAllocatedSize; /* Total Allocation including alignment adjustment */
  ULONG     ulDataSize;      /* Size of the usable data area                    */
  ULONG     ulFlags;         /* Allocation flags                                */
  void     *pMemory;         /* Pointer returned by memory allocator            */
  char     *pData;           /* Aligned pointer                                 */
  ULONG     ulPhysical;      /* Physical HW address for DMA/HW engine           */
} DMA_Area;

typedef struct
{
  long channel_nr;
  void *paced_request;
} DMA_Channel;

typedef struct
{
  DMA_Area *src;
  ULONG     src_offset;
  DMA_Area *dst;
  ULONG     dst_offset;
  ULONG     size;
  void      (*completed_cb)(unsigned long param);
  ULONG     cb_param;
} DMA_transfer;


typedef enum
{
  SDTP_UNPACED,
  SDTP_HDMI1_IFRAME,
  SDTP_DENC1_TELETEXT,
  SDTP_DENC2_TELETEXT,
  SDTP_LAST_ENTRY
} STM_DMA_TRANSFER_PACING;

typedef enum
{
  SDTF_NONE                   = 0x00000000,
  SDTF_DST_IS_SINGLE_REGISTER = 0x00000001
} STM_DMA_TRANSFER_FLAGS;

typedef struct
{
  ULONG          ulDataSize;
  unsigned char *pData;
} STMFirmware;


typedef enum {
  SDAAF_NONE          = 0x00000000,
  SDAAF_VIDEO_MEMORY  = 0x00000001,

  SDAAF_UNCACHED      = 0x80000000,
} STM_DMA_AREA_ALLOC_FLAGS;

typedef struct { volatile int counter; } ATOMIC;


typedef struct
{
  int    (*Init)(void);
  void   (*Release)(void);

  /* Device register access */
  ULONG  (*ReadRegister)(volatile ULONG  *reg);
  void   (*WriteRegister)(volatile ULONG *reg, ULONG  val);

  UCHAR  (*ReadByteRegister)(volatile UCHAR  *reg);
  void   (*WriteByteRegister)(volatile UCHAR *reg, UCHAR  val);

  /* Time */
  TIME64 (*GetSystemTime)(void);
  TIME64 (*GetOneSecond)(void);

  void   (*StallExecution)(ULONG usec);

  /* Memory mapping */
  void* (*MapMemory)(ULONG ulBaseAddr, ULONG ulSize);
  void  (*UnMapMemory)(void* pVMem);

  void  (*ZeroMemory)(void* pMem, unsigned long count);

  void  (*MemsetDMAArea)  (DMA_Area *mem, ULONG offset, int val,    unsigned long count);
  void  (*MemcpyToDMAArea)(DMA_Area *mem, ULONG offset, const void* pSrc, unsigned long count);

  /* Host memory allocation/deallocation */
  void* (*AllocateMemory)(ULONG size);
  void  (*FreeMemory)(void* mem);
  /* HW DMA memory allocation/deallocation */
  void  (*AllocateDMAArea)(DMA_Area *mem, ULONG size, ULONG alignment, STM_DMA_AREA_ALLOC_FLAGS flags);
  void  (*FreeDMAArea)(DMA_Area *mem);
  int   (*CheckDMAAreaGuards)(DMA_Area *mem);

  DMA_Channel *(*GetDMAChannel)(STM_DMA_TRANSFER_PACING pacing, ULONG bytes_per_req, STM_DMA_TRANSFER_FLAGS flags);
  void  (*ReleaseDMAChannel)(DMA_Channel *channel);
  void  (*StopDMAChannel)(DMA_Channel *channel);
  void* (*CreateDMATransfer)(DMA_Channel *channel, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags);
  void  (*DeleteDMATransfer)(void *handle);
  int   (*StartDMATransfer)(void *handle);

  void  (*FlushCache)    (void* vAddr, int size);

  ULONG (*CreateResourceLock)(void);
  void  (*DeleteResourceLock)(ULONG ulHandle);
  void  (*LockResource)(ULONG ulhandle);
  void  (*UnlockResource)(ULONG ulHandle);

  ULONG (*CreateSemaphore)(int initVal);
  void  (*DeleteSemaphore)(ULONG ulHandle);
  int   (*DownSemaphore)(ULONG ulHandle);
  void  (*UpSemaphore)(ULONG ulHandle);

  void	(*WakeQueue)(WAIT_QUEUE queue);
  int   (*SleepOnQueue)(WAIT_QUEUE queue, volatile unsigned long *var, unsigned long value, int condition);

  int   (*RequestFirmware)(const STMFirmware **firmware_p,
                           const char         * const name);
  void  (*ReleaseFirmware)(const STMFirmware  *fw);

  int   (*snprintf)(char * buf, ULONG size, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

  const ULONG OSPageSize;

  int   (*AtomicCmpXchg) (ATOMIC *atomic, int oldval, int newval);
  void  (*AtomicSet)     (ATOMIC *atomic, int val);
  void  (*Barrier) (void);

  /* BIOS_FAIL == 0, BIOS_OK == 1, arguments are LONGs */
  ULONG (*CallBios) (long call, unsigned int n_args, ...);
} IOS;

extern IOS* g_pIOS;

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* IOS_H */
