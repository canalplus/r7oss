/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/circularbuffer.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ************************************************************************/
#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/wait.h>

#include <circularbuffer.h>

#include <vibe_debug.h>

/* Init Circular Buffer
 * pCircularBuf:   pointer of circular buffer
 * numOfElement:   number of elements in circular buffer
 * sizeOfElement:  size of each element
 */
CircularBuffer_t* CircularBufferInit(CircularBuffer_t** pCircularBuf, uint32_t numOfElement, uint32_t sizeOfElement)
{
    uint32_t sz = 0;

    *pCircularBuf  = (CircularBuffer_t*) kzalloc(sizeof(CircularBuffer_t), GFP_KERNEL);
    if(*pCircularBuf)
    {
        sz = numOfElement*sizeOfElement;
        (*pCircularBuf)->pElementsBuffer = kzalloc(sz, GFP_KERNEL);
        if((*pCircularBuf)->pElementsBuffer)
        {
            (*pCircularBuf)->numOfElement  = numOfElement;
            (*pCircularBuf)->sizeOfElement = sizeOfElement;
            (*pCircularBuf)->writeIndex    = 0;
            (*pCircularBuf)->readIndex     = 0;
            spin_lock_init(&(*pCircularBuf)->lock);
        }
        else
        {
            kfree(*pCircularBuf);
            *pCircularBuf = NULL;
            TRC(TRC_ID_ERROR, "memory allocation error for *pCircularBuf->pElementsBuffer (size %d)", sz);
        }
    }
    else
    {
        *pCircularBuf = NULL;
        TRC(TRC_ID_ERROR, "memory allocation error for pCircularBuf (size %d)", sizeof(CircularBuffer_t));
    }
    return *pCircularBuf;
}

/* Release Circular Buffer
 * pCircularBuf: pointer of circular buffer
 */
void CircularBufferRelease(CircularBuffer_t* CircularBuf)
{
    if(CircularBuf != NULL)
    {
        if(CircularBuf->pElementsBuffer != NULL)
        {
            kfree(CircularBuf->pElementsBuffer);
            CircularBuf->pElementsBuffer = NULL;

        }
        kfree(CircularBuf);
        CircularBuf = NULL;
    }
}

/* CircularBufferWrite
 * CircularBuf       :  pointer of circular buffer
 * pElementBuf       :  pointer of an element to be stored
 * sizeOfElement     :  size of an element
 */
int32_t CircularBufferWrite(CircularBuffer_t* CircularBuf, void* pElementBuf, uint32_t sizeOfElement)
{
    unsigned long saveFlags;
    uint32_t      freeSlot = 0;
    void*         wrPtr;
    int32_t       ret = CIRCULAR_BUFFER_OK;

    if ( (pElementBuf == NULL) || (sizeOfElement > CircularBuf->sizeOfElement) )
    {
        TRC(TRC_ID_ERROR, "Invalid element! pElementBuf = %p, sizeOfElement = %d", pElementBuf, sizeOfElement);
        return -CIRCULAR_BUFFER_BAD_PARAM;
    }

    /* First: Find a free slot to use */
    spin_lock_irqsave(&(CircularBuf->lock), saveFlags);

    /* Is the buffer full? */
    if (((CircularBuf->writeIndex + 1) % CircularBuf->numOfElement) != CircularBuf->readIndex)
    {
        /* The circular buffer is NOT full so we can get a Free slot */
        freeSlot = CircularBuf->writeIndex++;
        CircularBuf->writeIndex %= CircularBuf->numOfElement;
        wrPtr  = (void*)(CircularBuf->pElementsBuffer + freeSlot * CircularBuf->sizeOfElement);
        memcpy(wrPtr, pElementBuf, CircularBuf->sizeOfElement);
    }
    else
    {
        ret = -CIRCULAR_BUFFER_FULL;
        TRC(TRC_ID_ERROR, "CircularBuf = %p is full!!!", CircularBuf);
    }

    spin_unlock_irqrestore(&CircularBuf->lock, saveFlags);
    return (ret);
}

/* CircularBufferRead
 * CircularBuf       :  pointer of circular buffer
 * pElementBuf       :  pointer of an element to be read
 * sizeOfElement     :  size of an element
 */
int32_t CircularBufferRead(CircularBuffer_t* CircularBuf, void* pElementBuf, uint32_t sizeOfElement)
{
    unsigned long saveFlags;
    uint32_t      slotRead = 0;
    void*         rdPtr;
    int32_t       ret = CIRCULAR_BUFFER_OK;

    if ( (pElementBuf == NULL) || (sizeOfElement < CircularBuf->sizeOfElement) )
    {
        TRC(TRC_ID_ERROR, "Invalid element! pElementBuf = %p, sizeOfElement = %d", pElementBuf, sizeOfElement);
        return -CIRCULAR_BUFFER_BAD_PARAM;
    }

    spin_lock_irqsave(&(CircularBuf->lock), saveFlags);

    if (CircularBuf->readIndex == CircularBuf->writeIndex)
    {
        ret = -CIRCULAR_BUFFER_EMPTY;
        TRC(TRC_ID_UNCLASSIFIED, "ERROR: CircularBuf = %p is empty!!!", CircularBuf);
    }
    else
    {
        slotRead = CircularBuf->readIndex++;
        CircularBuf->readIndex %= CircularBuf->numOfElement;
        rdPtr = (void*)(CircularBuf->pElementsBuffer + slotRead * CircularBuf->sizeOfElement);
        memcpy(pElementBuf, rdPtr, sizeOfElement);
    }
    spin_unlock_irqrestore(&CircularBuf->lock, saveFlags);

    return (ret);
}

