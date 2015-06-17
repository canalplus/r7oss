/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/circularbuffer.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : circularbuffer.h
 * Author :           Cheng

 * Access to all platform specific display information etc
 ************************************************************************/
#ifndef _CIRCULARBUFFER_H
#define _CIRCULARBUFFER_H

typedef struct CircularBuffer_s {
    uint32_t   writeIndex;
    uint32_t   readIndex;
    uint32_t   numOfElement;
    uint32_t   sizeOfElement;
    void      *pElementsBuffer;
    spinlock_t lock;
}CircularBuffer_t;

typedef enum CircularBuffer_Status_e
{
    CIRCULAR_BUFFER_OK,
    CIRCULAR_BUFFER_BAD_PARAM,
    CIRCULAR_BUFFER_FULL,
    CIRCULAR_BUFFER_EMPTY
} CircularBuffer_Status_t;


CircularBuffer_t* CircularBufferInit(CircularBuffer_t** pCircularBuf, uint32_t numOfElement, uint32_t sizeOfElement);
void CircularBufferRelease(CircularBuffer_t* CircularBuf);
int32_t CircularBufferWrite(CircularBuffer_t* CircularBuf, void* pElementBuf, uint32_t sizeOfElement);
int32_t CircularBufferRead(CircularBuffer_t* CircularBuf, void* pElementBuf, uint32_t sizeOfElement);
#endif /*_STMCOREDISPLAYTESTBUFFER_H */
