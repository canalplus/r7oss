/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_services.h
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __OBJECT_POOL_H
#define __OBJECT_POOL_H

#include "fvdp_private.h"

//=============================================================================
//  D E F I N I T I O N S
//=============================================================================

//=============================================================================
//  E N U M E R A T I O N S
//=============================================================================
enum
{
    OBJECT_FREE,
    OBJECT_USED
};

enum
{
    ERROR_UNABLE_TO_ALLOC            = BIT0,
    ERROR_UNABLE_TO_FREE             = BIT1,
    ERROR_UNABLE_TO_FIND             = BIT2,
    ERROR_UNABLE_TO_ALLOC_FROM_INDEX = BIT3
};


//=============================================================================
//  S T R U C T   D E F I N I T I O N S
//=============================================================================
typedef struct
{
    void       * object_ptrs;        // Pointer to array of objects
    uint8_t    * object_states;      // Pointer to array of Used/Free flags
    uint16_t     object_size;        // size of object
    uint8_t      pool_size;          // Size of pool
    uint8_t      num_in_use;         // Current number of objects in use
    uint8_t      max_num_used;       // Maximum number of objects used so far
    uint8_t      status_flags;       // Error conditions
    uint8_t      Datapack1;
    uint8_t      Datapack2;
} ObjectPool_t;


//=============================================================================
//  F U N C T I O N   P R O T O T Y P E S
//=============================================================================
void   ObjectPool_Create                  ( ObjectPool_t * pool, void   * object_ptrs, uint8_t* object_states, uint16_t object_size, uint8_t pool_size);
void   ObjectPool_Reset                   ( ObjectPool_t * pool);
void * ObjectPool_Alloc_Object_From_Index ( ObjectPool_t * pool, uint32_t num   , uint32_t   idx);
void * ObjectPool_Alloc_Object            ( ObjectPool_t * pool, uint32_t num   , uint32_t * idx);
void   ObjectPool_Free_Object             ( ObjectPool_t * pool, void   * object, uint32_t   num);
void * ObjectPool_Search                  ( ObjectPool_t * pool, bool   (*criterion)(void*,uint32_t), uint32_t param);


#endif // __OBJECT_POOL_H

