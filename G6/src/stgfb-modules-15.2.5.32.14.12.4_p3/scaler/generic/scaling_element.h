/***********************************************************************
 *
 * File: scaler/generic/scaling_element.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_ELEMENT_H
#define SCALING_ELEMENT_H

#ifdef __cplusplus

typedef enum {
    TYPE_UNKNOWN,
    TYPE_SCALING_CTX,
    TYPE_SCALING_TASK,
} element_type_t;

class CScalingElement
{
public:
    CScalingElement(element_type_t elementType) { m_elementType = elementType; m_isUsed = true; }

protected:
    element_type_t m_elementType;  // Type of element, checked by scaling pool and scaling queue
    bool           m_isUsed;       // Indicate if element is used, checked by scaling pool and scaling queue

friend class CScalingPool;
}; /* CScalingElement */

#endif /* __cplusplus */

#endif /* SCALING_ELEMENT_H */

