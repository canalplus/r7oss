/***********************************************************************
 *
 * File: scaler/generic/scaling_node.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALING_NODE_H
#define SCALING_NODE_H

#ifdef __cplusplus

#include "scaling_element.h"

class CScalingNode : public CScalingElement
{
public:
    CScalingNode(element_type_t elementType) : CScalingElement(elementType)
      {
        m_pNextNode = 0;
      }

protected:
    CScalingNode  *m_pNextNode;  // Next scaling node when elements are queued, used by scaling queue

friend class CScalingQueue;
};


#endif /* __cplusplus */

#endif /* SCALING_NODE_H */

