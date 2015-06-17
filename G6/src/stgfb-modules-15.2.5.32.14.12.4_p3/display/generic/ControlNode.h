/***********************************************************************
 *
 * File: display/generic/ControlNode.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef CONTROL_NODE_H
#define CONTROL_NODE_H


enum ControlType
{
    CONTROL_SIMPLE,
    CONTROL_COMPOUND
};


union ControlParams
{
    uint32_t value;
    stm_display_source_h display_source;
};


class CControlNode: public CNode
{
public:
    CControlNode(stm_display_plane_control_t control, ControlParams* params);
    virtual ~CControlNode(void);

protected:


private:
    stm_display_plane_control_t m_control;
    ControlParams m_params;


//Only CDisplayQueue is allowed to call SetNextNode()
friend class CDisplayPlane;
};


#endif /* CONTROL_NODE_H */


