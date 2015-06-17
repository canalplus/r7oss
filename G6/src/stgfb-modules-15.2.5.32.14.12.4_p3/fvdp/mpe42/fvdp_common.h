/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_common.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_COMMON_H
#define FVDP_COMMON_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Includes ----------------------------------------------------------------- */
#include "fvdp_private.h"
#include "fvdp_regs.h"

/* Exported Macros ---------------------------------------------------------- */

#if defined(FVDP_SIMULATION)

    #define REG_BASE_ADDR           0x9B000000
    #define REG32_Write(Addr, Val)  vibe_os_write_register((volatile uint32_t*) \
                                    (gRegisterBaseAddress), Addr, Val)
    #define REG32_Read(Addr)        (uint32_t)(vibe_os_read_register((volatile uint32_t*) \
                                    (gRegisterBaseAddress), Addr))
    #define ZERO_MEMORY(Addr,Len)   vibe_os_zero_memory(Addr,Len);
    #define _do_nothing()           usleep(1)

  #if defined(SYSTEM_BUILD_VCPU)
      extern void INT_Init(void);   // use the one in fvdp_sim.c
  #else
      #define INT_Init()
  #endif

#else
    #define REG_BASE_ADDR           0       // (to be initialized by host)
    #define REG32_Write(Addr, Val) { TRC(TRC_ID_UNCLASSIFIED, "FVDP : %08x <-  %08x", Addr, Val); vibe_os_write_register((volatile uint32_t*)gRegisterBaseAddress, Addr, Val); }
    #define REG32_Read(Addr) ({ uint32_t __ret = (uint32_t)(vibe_os_read_register((volatile uint32_t*)gRegisterBaseAddress, Addr)); TRC(TRC_ID_UNCLASSIFIED, "FVDP : %08x  -> %08x", Addr, __ret); __ret; })
    #define ZERO_MEMORY(Addr,Len)   vibe_os_zero_memory(Addr,Len);
    #define _do_nothing()

    #define INT_Init()
#endif

#define REG32_Set(Addr, BitsToSet)      REG32_Write(Addr,((REG32_Read(Addr))| \
                                                (uint32_t)(BitsToSet)))
#define REG32_Clear(Addr,BitsToClear)   REG32_Write(Addr,(REG32_Read(Addr)& \
                                                (~(uint32_t)(BitsToClear))))
#define REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
    REG32_Write(Addr,((REG32_Read(Addr)&(~(uint32_t)(BitsToClear))) | \
    ((uint32_t)(BitsToClear) & ((uint32_t)(ValueToSet)<<(Offset)))))
#define REG32_SetField(Addr,Field,ValueToSet) \
    REG32_ClearAndSet(Addr,Field ## _MASK,Field ## _OFFSET,ValueToSet)
#define REG32_GetField(Addr,Field) \
    ((REG32_Read(Addr) & (Field ## _MASK)) >> (Field ## _OFFSET))
#define REG32_Bit(Address,Mask,ToSet) \
    {if (ToSet) {REG32_Set(Address,Mask);} else {REG32_Clear(Address,Mask);}}

#define SETBIT(VAR,BIT)         ( (VAR) = (VAR) | (BIT) )
#define CLRBIT(VAR,BIT)         ( (VAR) = (VAR) & (~(BIT)) )
#define CHECKBIT(VAR,BIT)       ( (VAR) & (BIT) )
#define SETCLRBIT(VAR,BIT,ToSet) \
    {if (ToSet) {SETBIT((VAR),(BIT));} else {CLRBIT((VAR),(BIT));}}

#define SETBIT_VECT(bv32,pos)   (bv32)[(pos) >> 5] |= 1 << ((pos) & (0x20-1))
#define CLRBIT_VECT(bv32,pos)   (bv32)[(pos) >> 5] &= ~(1 << ((pos) & (0x20-1)))
#define CHECKBIT_VECT(bv32,pos) (((bv32)[(pos) >> 5] >> ((pos) & (0x20-1))) != 0)

#define ALIGNED64BYTE(VAL)  ((VAL + 0x3F) & 0xFFFFFFC0)
#define ALIGNED32BYTE(VAL)  ((VAL + 0xF) & 0xFFFFFFF0)

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

typedef struct queue_element_s* queue_element_ptr;

typedef struct queue_element_s
{
    queue_element_ptr prev,next;
    uint8_t reserved;
    uint8_t index;
} queue_element_t;

typedef struct queue_s
{
    queue_element_ptr first,last;
    void* pool;
    uint8_t len, max_len;
    uint32_t data_size;
} queue_t;

/* Exported Functions ------------------------------------------------------- */

extern void loop_copy(void* dest, void* source, uint32_t size);
extern uint8_t queue_GetLength (queue_t* q);
extern bool queue_PeekInputSide (queue_t* q, uint8_t dist, void** addr);
extern bool queue_GetInputSide (queue_t* q, uint8_t dist, void* data);
extern bool queue_PeekOutputSide (queue_t* q, uint8_t dist, void** addr);
extern bool queue_GetOutputSide (queue_t* q, uint8_t dist, void* data);
extern bool queue_SetInputSide (queue_t* q, uint8_t dist, void* data);
extern bool queue_SetOutputSide (queue_t* q, uint8_t dist, void* data);
extern void queue_Init (queue_t* q, void* pool, uint8_t max_len, uint32_t data_size);
extern queue_element_t* queue_PushInputSide (queue_t* q, void* data);
extern queue_element_t* queue_PushOutputSide (queue_t* q, void* data);
extern bool queue_PopInputSide (queue_t* q, void* data);
extern bool queue_PopOutputSide (queue_t* q, void* data);
extern uint32_t DivRoundUp (uint32_t dividend, uint32_t divisor);
extern uint32_t ScaleAndRound (uint32_t val, uint16_t numerator, uint16_t denominator);
extern void SetDeviceBaseAddress (uint32_t address);
extern uint32_t GetDeviceBaseAddress (void);

/* Exported Variables ------------------------------------------------------- */

extern uint32_t gRegisterBaseAddress;

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_COMMON_H */

