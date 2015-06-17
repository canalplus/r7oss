/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2007-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef VIBE_OS_H
#define VIBE_OS_H

#include <vibe_utils.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

__extension__ typedef long long vibe_time64_t;


#define STMIOS_WAIT_COND_NOT_EQUAL  0
#define STMIOS_WAIT_COND_EQUAL      1

#define STMIOS_WAIT_TIME_FOREVER    0xffffffff


#define STM_RT_THREAD_PRIO_MIN  1    /* Real time thread priority, lowest priority  */
#define STM_RT_THREAD_PRIO_MAX  99   /* Real time thread priority, highest priority */


typedef struct
{
  uint32_t  ulAllocatedSize; /* Total Allocation including alignment adjustment */
  uint32_t  ulDataSize;      /* Size of the usable data area                    */
  uint32_t  ulFlags;         /* Allocation flags                                */
  uint32_t  allocHandle;     /* Internal handle for implementation use only     */
  void     *pMemory;         /* Pointer returned by memory allocator            */
  char     *pData;           /* Aligned pointer                                 */
  uint32_t  ulPhysical;      /* Physical HW address for DMA/HW engine           */
} DMA_Area;


typedef struct DMA_Channel DMA_Channel;

typedef struct
{
  DMA_Area *src;
  uint32_t  src_offset;
  DMA_Area *dst;
  uint32_t  dst_offset;
  uint32_t  size;
  void      (*completed_cb)(void *param);
  void     *cb_param;
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
  uint32_t             ulDataSize;
  const unsigned char *pData;
} STMFirmware;


typedef enum {
  SDAAF_NONE          = 0x00000000,
  SDAAF_VIDEO_MEMORY  = 0x00000001,
  SDAAF_SECURE        = 0x00000002,

  SDAAF_UNCACHED      = 0x00000080,
  /*
   * Implementation private
   */
  SDAAF_IOREMAPPED    = 0x00000800
} STM_DMA_AREA_ALLOC_FLAGS;


typedef int (*VIBE_OS_ThreadFct_t)( void   *parameter);


struct vibe_clk;

struct vibe_clk {
  uint32_t         id;
  const char      *name;
  void            *clk;
  bool             enabled;
  struct vibe_clk *parent;
  uint32_t         rate;
};

/*
 * Secure data path
 */
enum vibe_access_transformer_id {
    VIBE_ACCESS_TID_NONE,
    VIBE_ACCESS_TID_DISPLAY,
    VIBE_ACCESS_TID_HDMI_CAPTURE
};

enum vibe_access_mode {
    VIBE_ACCESS_O_READ =  (1 << 1),
    VIBE_ACCESS_O_WRITE = (1 << 2),
};

typedef struct
{
  int policy;
  int priority;
} VIBE_OS_thread_settings;

#define VIBE_OS_TRCMASK_SIZE 2
extern unsigned int trcmask[VIBE_OS_TRCMASK_SIZE];

int             vibe_os_init(void);
void            vibe_os_release(void);

bool            vibe_os_create_thread(VIBE_OS_ThreadFct_t fct, void *parameter, const char *name,
                                      const VIBE_OS_thread_settings *thread_settings, void **thread_desc);
int             vibe_os_stop_thread(void *thread_desc);
bool            vibe_os_wake_up_thread(void *thread_desc);
bool            vibe_os_thread_should_stop(void);

uint32_t        vibe_os_read_register(volatile uint32_t *baseAddress, uint32_t offset);
void            vibe_os_write_register(volatile uint32_t *baseAddress, uint32_t offset, uint32_t val);
uint8_t         vibe_os_read_byte_register(volatile uint8_t *reg);
void            vibe_os_write_byte_register(volatile uint8_t *reg,uint8_t val);

vibe_time64_t   vibe_os_get_system_time(void);
vibe_time64_t   vibe_os_get_one_second(void);

void            vibe_os_sleep_ms(uint32_t ms);
void            vibe_os_stall_execution(uint32_t ulDelay);

void*           vibe_os_map_memory(uint32_t ulBaseAddr, uint32_t ulSize);
void            vibe_os_unmap_memory(void* pVMem);

/* vibe_os_zero_memory() must not be called for initializing a C++ object
 * because the virtual destructor table will be corrupted.
 * The C++ object constructor must be used for initializing it.
 * This functions is dedicated for structure or table of pointers initialization.
 */
void            vibe_os_zero_memory(void* pMem, uint32_t count);
int             vibe_os_memcmp(const void *pMem1, const void *pMem2, uint32_t size);

int             vibe_os_check_dma_area_guards(DMA_Area *mem);
void            vibe_os_flush_dma_area(DMA_Area *mem, uint32_t offset, uint32_t count);
void            vibe_os_memset_dma_area(DMA_Area *mem, uint32_t offset, int val, uint32_t count);
void            vibe_os_memcpy_to_dma_area(DMA_Area *mem, uint32_t offset, const void* pSrc, uint32_t count);
void            vibe_os_memcpy_from_dma_area(DMA_Area *mem, uint32_t offset, void* pDst, uint32_t count);

/* Do do not use these 2 following functions with a C++ object as it will not call the
 * constructor or destructor (new/delete must be used for C++ object).
 * Also this function can not be mixed with new/delete.
 */
void *          vibe_os_allocate_memory(uint32_t size);
void            vibe_os_free_memory(void *mem);

/* for Secure Data Path */
int             vibe_os_grant_access(void *buf, unsigned int size, enum vibe_access_transformer_id tid, enum vibe_access_mode mode);
int             vibe_os_revoke_access(void *buf, unsigned int size, enum vibe_access_transformer_id tid, enum vibe_access_mode mode);
int             vibe_os_create_region(void *buf, unsigned int l);
int             vibe_os_destroy_region(void *buf, unsigned int l);
void            vibe_os_allocate_bpa2_dma_area(DMA_Area *mem, uint32_t align);
void            vibe_os_allocate_kernel_dma_area(DMA_Area *mem, uint32_t align);
void            vibe_os_allocate_dma_area(DMA_Area *mem, uint32_t size, uint32_t align, STM_DMA_AREA_ALLOC_FLAGS flags);
void            vibe_os_free_dma_area(DMA_Area *mem);

DMA_Channel *   vibe_os_get_dma_channel(STM_DMA_TRANSFER_PACING pacing, uint32_t bytes_per_req, STM_DMA_TRANSFER_FLAGS flags);
void            vibe_os_release_dma_channel(DMA_Channel *chan);
void            vibe_os_stop_dma_channel(DMA_Channel *chan);
void*           vibe_os_create_dma_transfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags);
void            vibe_os_delete_dma_transfer(void *handle);
int             vibe_os_start_dma_transfer(void *handle);

void *          vibe_os_create_resource_lock ( void            );
void            vibe_os_delete_resource_lock ( void * ulHandle );
void            vibe_os_lock_resource        ( void * ulHandle );
void            vibe_os_unlock_resource      ( void * ulHandle );

void *          vibe_os_create_semaphore       ( int    initVal  );
void            vibe_os_delete_semaphore       ( void * ulHandle );
int             vibe_os_down_semaphore         ( void * ulHandle );
int             vibe_os_down_trylock_semaphore ( void * ulHandle );
void            vibe_os_up_semaphore           ( void * ulHandle );

void *          vibe_os_create_mutex           ( void );
int             vibe_os_is_locked_mutex        ( void * ulHandle );
void            vibe_os_delete_mutex           ( void * ulHandle );
int             vibe_os_lock_mutex             ( void * ulHandle );
int             vibe_os_trylock_mutex          ( void * ulHandle );
void            vibe_os_unlock_mutex           ( void * ulHandle );

int             vibe_os_get_master_output_id (int output_id);

void            vibe_os_release_firmware     ( const STMFirmware  * fw_pub                              );
int             vibe_os_request_firmware     ( const STMFirmware ** firmware_p, const char * const name );

int __attribute__ ((format (printf, 3, 4))) vibe_os_snprintf(char * buf, uint32_t size, const char * fmt, ...);
uint32_t        vibe_os_call_bios (long call, unsigned int n_args, ...);
uint64_t        vibe_os_div64(uint64_t n, uint64_t d);
void            vibe_os_rational_reduction(uint64_t *pNum, uint64_t *pDen);

/*** Workqueue APIs ***/
typedef struct workqueue_struct   *VIBE_OS_WorkQueue_t;
typedef struct VIBE_OS_Worker_s   *VIBE_OS_Worker_t;
typedef void (*VIBE_OS_WorkerFct_t)(void *);

VIBE_OS_WorkQueue_t vibe_os_create_work_queue(const char *name);
void vibe_os_destroy_work_queue(VIBE_OS_WorkQueue_t workqueue);

void* vibe_os_allocate_workerdata(VIBE_OS_WorkerFct_t function, int data_size);
void vibe_os_delete_workerdata(void* data);
bool vibe_os_queue_workerdata(VIBE_OS_WorkQueue_t workqueue, void* data);

/*** MessageThread APIs ***/
typedef struct VIBE_OS_MessageThread_s *VIBE_OS_MessageThread_t;
typedef struct VIBE_OS_Message_s       *VIBE_OS_Message_t;
typedef void (*VIBE_OS_MessageCallback_t)(void *data);

VIBE_OS_MessageThread_t vibe_os_create_message_thread(const char *name, const int *thread_settings);
void vibe_os_destroy_message_thread(VIBE_OS_MessageThread_t message_thread);

void* vibe_os_allocate_message(VIBE_OS_MessageCallback_t callback, int data_size);
void  vibe_os_delete_message(void* data);
bool  vibe_os_queue_message(VIBE_OS_MessageThread_t message_thread, void* data);

/*** WaitQueue APIs ***/
typedef struct VIBE_OS_WaitQueue_s *VIBE_OS_WaitQueue_t;

int  vibe_os_allocate_queue_event(VIBE_OS_WaitQueue_t *queue);
void vibe_os_release_queue_event(VIBE_OS_WaitQueue_t queue);
void * vibe_os_get_wait_queue_data(VIBE_OS_WaitQueue_t queue);
void vibe_os_wake_up_queue_event(VIBE_OS_WaitQueue_t queue);
int vibe_os_wait_queue_event(VIBE_OS_WaitQueue_t queue,
                        volatile uint32_t* var, uint32_t value,
                        int cond, unsigned int timeout_ms);

/*** Enable/Disable IRQ APIs ***/
void vibe_os_enable_irq(unsigned int irq);
void vibe_os_disable_irq_nosync(unsigned int irq);
void vibe_os_disable_irq(unsigned int irq);
int  vibe_os_in_interrupt(void);

/*** GPIO APIs ***/
int32_t vibe_os_gpio_get_value(uint32_t gpio);

/*** clock LLA implementation ***/
int vibe_os_clk_enable(struct vibe_clk *clock);
int vibe_os_clk_disable(struct vibe_clk  *clock);
int vibe_os_clk_resume(struct vibe_clk *clock);
int vibe_os_clk_suspend(struct vibe_clk *clock);
int vibe_os_clk_set_parent(struct vibe_clk *clock, struct vibe_clk *parent);
struct vibe_clk *vibe_os_clk_get_parent(struct vibe_clk *clock);
int vibe_os_clk_set_rate(struct vibe_clk *clock, unsigned long rate);
unsigned long vibe_os_clk_get_rate(struct vibe_clk *clock);

/*** CHIP VERSION APIs ***/
bool vibe_os_get_chip_version(uint32_t *major, uint32_t *minor);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* VIBE_OS_H */
