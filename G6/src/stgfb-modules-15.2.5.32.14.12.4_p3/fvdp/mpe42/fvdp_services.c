/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_services.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

//=============================================================================
//  I N C L U D E    F I L E S
//=============================================================================
#include "fvdp_services.h"


uint8_t ObjectAllocFailed;

//=============================================================================
//  C O D E
//=============================================================================

//=============================================================================
//Method:        ObjectPool_Create
//Description:   Initializes the variables of an Object Pool.  Should only be called
//               once for each ObjectPool during initialization.
//In Param:      ObjectPool_t* pool          - pointer to the Object Pool
//               void*         object_ptrs   - pointer to the pool objects
//               uint8_t*           object_states - array that will hold object states
//               uint16_t           object_size   - size of each object
//               uint8_t            pool_size     - size of the Object Pool
//Out Param:
//Return:        void
//=============================================================================
void ObjectPool_Create(ObjectPool_t* pool, void* object_ptrs, uint8_t* object_states,
                       uint16_t object_size, uint8_t pool_size)
{
    uint32_t i;

    pool->object_ptrs = object_ptrs;
    pool->object_states = object_states;
    pool->object_size = object_size;
    pool->pool_size = pool_size;
    pool->num_in_use = 0;
    pool->max_num_used = 0;
    pool->status_flags = 0;

    for (i = 0;  i < pool_size; i++)
    {
        pool->object_states[i] = OBJECT_FREE;
    }
}

//=============================================================================
//Method:        ObjectPool_Reset
//Description:   Resets the object pool.  Will set all objects to FREE state.
//
//In Param:      ObjectPool_t*  pool  - pointer to the Object Pool
//Out Param:
//Return:        void
//=============================================================================
void ObjectPool_Reset(ObjectPool_t* pool)
{
    uint32_t i;

    pool->num_in_use = 0;
    pool->max_num_used = 0;
    pool->status_flags = 0;

    for (i = 0;  i < pool->pool_size; i++)
    {
        pool->object_states[i] = OBJECT_FREE;
    }
}

//=============================================================================
//Method:        ObjectPool_Alloc_Object
//Description:   Allocates an object for use.
//
//In Param:      ObjectPool_t*  pool - pointer to the Object Pool
//               uint32_t       num  - number of consecutive objects to allocate
//               uint32_t       idx  - index of object that should be allocated
//Out Param:
//Return:        void*  - address of allocated object (first object if there
//                        are more than one allocated)
//=============================================================================
void* ObjectPool_Alloc_Object_From_Index(ObjectPool_t* pool, uint32_t num, uint32_t idx)
{
    char*    object_ptr;
    uint32_t i;

    object_ptr = (char*)pool->object_ptrs;

    // Check that the indexes will not overflow the array size
    if ((idx+num) > pool->pool_size)
    {
        pool->status_flags |= ERROR_UNABLE_TO_ALLOC_FROM_INDEX;
        return NULL;
    }

// the FBM will return references to five input frames (current, prev, prev-1, ... prev-3)
// but we store only three CCS buffers.  we are able to accomplish this by using
// a buffer wrap-around method.  on the VCPU-side, we effectively disconnect a buffer
// from a previously associated frame once it is "overwritten", and re-associated with
// the new frame.  on the HOST-side, we simply don't check if the buffer is already used
// or not and trust the VCPU-side.
#if 0
    // Check that all indexes are not used
    for (i = 0; i < num; i++)
    {
        if (pool->object_states[idx+i] == OBJECT_USED)
        {
            pool->status_flags |= ERROR_UNABLE_TO_ALLOC_FROM_INDEX;
            return NULL;
        }
    }
#endif

    for (i = 0; i < num; i++)
    {
        pool->object_states[idx+i] = OBJECT_USED;
        pool->num_in_use++;
    }

    // Return a pointer to the Object
    object_ptr += (pool->object_size) * idx;
    return object_ptr;
}

//=============================================================================
//Method:        ObjectPool_Alloc_Object
//Description:   Allocates an object for use.
//
//In Param:      ObjectPool_t*  pool - pointer to the Object Pool
//               uint32_t       num  - number of consecutive objects to allocate
//               uint32_t*      idx  - pointer to index to return
//Out Param:
//Return:        void*  - address of allocated object (first object if there
//                        are more than one allocated)
//=============================================================================
void* ObjectPool_Alloc_Object(ObjectPool_t* pool, uint32_t num, uint32_t* idx)
{
    uint32_t i, j;
    char* object_ptr;
    uint8_t AreObjectsFree;

    object_ptr = (char*)pool->object_ptrs;

    for (i = 0;  i < pool->pool_size; i++)
    {
        AreObjectsFree = 1;

        // check for consecutive free objects
        for (j = i; j < i + num; j++)
        {
            AreObjectsFree &= ( pool->object_states[j] == OBJECT_FREE) ? 1: 0;
        }

        if ( AreObjectsFree)
        {
            for (j = i; j < i + num; j++)
            {
                pool->object_states[j] = OBJECT_USED;
                pool->num_in_use++;
                if (idx)
                    *idx = j;
            }

            if (pool->num_in_use > pool->max_num_used)
                pool->max_num_used = pool->num_in_use;

            return (object_ptr);
        }

        object_ptr += pool->object_size;
    }

    pool->status_flags |= ERROR_UNABLE_TO_ALLOC;
    ObjectAllocFailed = 1;

    return NULL;
}

//=============================================================================
//Method:        ObjectPool_Free_Object
//Description:   Frees an object.
//
//In Param:      ObjectPool_t*  pool   - pointer to the Object Pool
//               void*          object - pointer to object to be freed
//               uint32_t      num  - number of consecutive objects to free
//Out Param:
//Return:        void
//=============================================================================
void ObjectPool_Free_Object(ObjectPool_t* pool, void* object, uint32_t num)
{
    uint32_t i;
    char* object_ptr;
    uint32_t num_to_free = ( num > 0) ? num: 1;
    char* object_to_free = (char*)object;

    object_ptr = (char*)pool->object_ptrs;

    for (i = 0;  i < pool->pool_size; i++)
    {
        if (pool->object_states[i] == OBJECT_USED)
        {
            if (object_ptr == object_to_free)
            {
                pool->object_states[i] = OBJECT_FREE;
                pool->num_in_use--;

                num_to_free--;

                // free subsequent if more than 1
                if ( num_to_free == 0)
                    return;
                else
                    object_to_free += pool->object_size;
            }
        }

        object_ptr += pool->object_size;
    }

    pool->status_flags |= ERROR_UNABLE_TO_FREE;
}

//=============================================================================
//Method:        ObjectPool_Search
//Description:   Search with a particular criterion within the pool
//
//In Param:      ObjectPool_t*  pool      - pointer to the Object Pool
//               void (*criterion)(void*) - search criterion to use
//               uint32_t param           - generic search parameter
//Out Param:
//Return:        void* pointer to object in pool
//=============================================================================
void* ObjectPool_Search(ObjectPool_t* pool, bool (*criterion)(void*,uint32_t), uint32_t param)
{
    uint32_t i;
    uint8_t* object_ptr;

    if (!criterion)
        return NULL;

    for (i = 0;  i < pool->pool_size; i++)
    {
        if (pool->object_states[i] == OBJECT_USED)
        {
            object_ptr = (char*)pool->object_ptrs + ( pool->object_size * i);
            if ((*criterion)(object_ptr,param))
            {
                return object_ptr;
            }
        }

    }

    return NULL;
}

/* end of file */
