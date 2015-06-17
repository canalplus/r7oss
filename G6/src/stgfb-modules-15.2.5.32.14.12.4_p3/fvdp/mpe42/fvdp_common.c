/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_common.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_private.h"
#include "fvdp_update.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

#define MAX_QUEUE_ELEMENTS    32

/* Private Macros ----------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */

static queue_element_t qelements[MAX_QUEUE_ELEMENTS];

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */
uint32_t gRegisterBaseAddress = REG_BASE_ADDR;

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : queue_Init, queue_GetInputSide, queue_GetOutputSide,
              queue_PushInputSide, queue_PushOutputSide,
              queue_PopInputSide, queue_PopOutputSide
Description : various generic queue functions
Parameters  : queue_t* q, void* pool, uint dist, uint size, void* data
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No errors
                    FVDP_ERROR: Error encountered
*******************************************************************************/
void queue_Init (queue_t* q, void* pool, uint8_t max_len,
                 uint32_t data_size)
{
    q->first = NULL;
    q->last = NULL;
    q->pool = pool;
    q->len = 0;
    q->max_len = max_len;
    q->data_size = data_size;
}

static queue_element_t* queue_Reserve (queue_t* q)
{
    uint8_t i;

    for (i=0; i < MAX_QUEUE_ELEMENTS; i++)
    {
        if (qelements[i].reserved == FALSE)
        {
            qelements[i].reserved = TRUE;
            qelements[i].next = NULL;
            qelements[i].prev = NULL;
            qelements[i].index = i;
            return &qelements[i];
        }
    }
    return NULL;
}

static bool queue_Unreserve (queue_element_t* qe)
{
    uint8_t i;

    for (i=0; i < MAX_QUEUE_ELEMENTS; i++)
    {
        if (qe == &qelements[i])
        {
            qelements[i].reserved = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}

void loop_copy(void* dest, void* source, uint32_t size)
{
    char* d = (char*)dest;
    char* s = (char*)source;
    while (size --)
        *d++ = *s++;
}

uint8_t queue_GetLength (queue_t* q)
{
    return q->len;
}

bool queue_PeekInputSide (queue_t* q, uint8_t dist, void** addr)
{
    queue_element_t* qe;

    if (!q || !(qe = q->first)) return FALSE;

    while (dist-- && qe)
        qe = qe->next;
    if (!qe || !addr) return FALSE;

    *addr = (char*)q->pool + qe->index * q->data_size;
    return TRUE;
}

bool queue_GetInputSide (queue_t* q, uint8_t dist, void* data)
{
    queue_element_t* qe;

    if (!q || !(qe = q->first)) return FALSE;

    while (dist-- && qe)
        qe = qe->next;
    if (!qe || !data) return FALSE;

    loop_copy (data, (char*)q->pool + qe->index * q->data_size, q->data_size);
    return TRUE;
}

bool queue_PeekOutputSide (queue_t* q, uint8_t dist, void** addr)
{
    queue_element_t* qe;

    if (!q || !(qe = q->last)) return FALSE;

    while (dist-- && qe)
        qe = qe->prev;
    if (!qe || !addr) return FALSE;

    *addr = (char*)q->pool + qe->index * q->data_size;
    return TRUE;
}

bool queue_GetOutputSide (queue_t* q, uint8_t dist, void* data)
{
    queue_element_t* qe;

    if (!q || !(qe = q->last)) return FALSE;

    while (dist-- && qe)
        qe = qe->prev;
    if (!qe || !data) return FALSE;

    loop_copy (data, (char*)q->pool + qe->index * q->data_size, q->data_size);
    return TRUE;
}

bool queue_SetInputSide (queue_t* q, uint8_t dist, void* data)
{
    queue_element_t* qe;

    if (!q || !(qe = q->first)) return FALSE;

    while (dist-- && qe)
        qe = qe->next;
    if (!qe || !data) return FALSE;

    loop_copy ((char*)q->pool + qe->index * q->data_size, data, q->data_size);
    return TRUE;
}

bool queue_SetOutputSide (queue_t* q, uint8_t dist, void* data)
{
    queue_element_t* qe;

    if (!q || !(qe = q->last)) return FALSE;

    while (dist-- && qe)
        qe = qe->prev;
    if (!qe || !data) return FALSE;

    loop_copy ((char*)q->pool + qe->index * q->data_size, data, q->data_size);
    return TRUE;
}

queue_element_t* queue_PushInputSide (queue_t* q, void* data)
{
    queue_element_t* qf;
    queue_element_t* qr;

    if (!q || !data || q->len >= q->max_len) return NULL;
    if (!(qr = queue_Reserve(q))) return NULL;

    qf = q->first;
    loop_copy ((char*)q->pool + qr->index * q->data_size, data, q->data_size);

    if (qf)
    {
        qf->prev = qr;
        qr->next = qf;
        q->first = qr;
    }
    else
    {
        q->first = qr;
        q->last = qr;
    }
    q->len ++;
    return qr;
}

queue_element_t* queue_PushOutputSide (queue_t* q, void* data)
{
    queue_element_t* ql;
    queue_element_t* qr;

    if (!q || !data || q->len >= q->max_len) return NULL;
    if (!(qr = queue_Reserve(q))) return NULL;

    ql = q->last;
    loop_copy ((char*)q->pool + qr->index * q->data_size, data, q->data_size);

    if (ql)
    {
        ql->next = qr;
        qr->prev = ql;
        q->last = qr;
    }
    else
    {
        q->first = qr;
        q->last = qr;
    }
    q->len ++;
    return qr;
}

bool queue_PopInputSide (queue_t* q, void* data)
{
    queue_element_t* qe;

    if (!q) return FALSE;
    if (!(qe = q->first)) return FALSE;

    if (data) loop_copy (data, (char*)q->pool + qe->index * q->data_size, q->data_size);

    qe = qe->next;

    queue_Unreserve(q->first);

    q->first = qe;

    if (!q->first)
         q->last = NULL;
    else qe->prev = NULL;

    q->len --;
    return TRUE;
}

bool queue_PopOutputSide (queue_t* q, void* data)
{
    queue_element_t* qe;

    if (!q) return FALSE;
    if (!(qe = q->last)) return FALSE;

    if (data) loop_copy (data, (char*)q->pool + qe->index * q->data_size, q->data_size);

    qe = qe->prev;

    queue_Unreserve(q->last);

    q->last = qe;

    if (!q->last)
         q->first = NULL;
    else qe->next = NULL;

    q->len --;
    return TRUE;
}

/*******************************************************************************
Name        : DivRoundUp
Description : Support function for strip mode parameter division calculations,
              with results rounding up
Parameters  : uint32_t dividend, uint32_t divisor
Assumptions : divisor is non-zero
Limitations :
Returns     : uint32_t
*******************************************************************************/
uint32_t DivRoundUp (uint32_t dividend, uint32_t divisor)
{
    uint32_t quotient;

    if (divisor == 0)
    {
        TRC( TRC_ID_MAIN_INFO, "division-by-zero!" );
        return 0;
    }
    else if (divisor == 1)
        quotient = dividend;
    else
        quotient = (dividend + (divisor - 1)) / divisor;

    return quotient;
}

/*******************************************************************************
Name        : ScaleAndRound
Description : scale by specified numerator and denominator, then round value
Parameters  : uint32_t val, uint16_t numerator, uint16_t denominator
Assumptions : denominator is non-zero
Limitations :
Returns     : uint32_t
*******************************************************************************/
uint32_t ScaleAndRound (uint32_t val, uint16_t numerator, uint16_t denominator)
{
    if (denominator == 0)
    {
        TRC( TRC_ID_MAIN_INFO, "division-by-zero!" );
        return 0;
    }
    val *= numerator * 2;
    val += denominator;
    val /= denominator * 2;
    return val;
}

/*******************************************************************************
Name        : SetDeviceBaseAddress
Description : Set Devide Base Address for FVDP Register. The register address of FVDP will be added
                reference to DeviceBaseAddress
Parameters  : uint32_t BaseAddress
Assumptions :
Limitations :
Returns     : uint32_t
*******************************************************************************/
void SetDeviceBaseAddress (uint32_t address)
{
    gRegisterBaseAddress = address;
}

/*******************************************************************************
Name        : GetDeviceBaseAddress
Description : Get Devide Base Address for FVDP Register. The register address of FVDP will be added
                reference to DeviceBaseAddress
Parameters  : uint32_t BaseAddress
Assumptions :
Limitations :
Returns     : uint32_t
*******************************************************************************/
uint32_t GetDeviceBaseAddress (void)
{
    return gRegisterBaseAddress;
}

