/***********************************************************************
 *
 * File: display/ip/panel/stmdptx.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DPTX_H
#define _STM_DPTX_H

class CSTmDPTx
{
public:
    CSTmDPTx(CDisplayDevice *pDev);

    virtual ~CSTmDPTx();

    virtual uint32_t Start(const stm_display_mode_t* pModeLine) = 0;

protected:
    uint32_t *m_pDevRegs;

private:
    CSTmDPTx(const CSTmDPTx&);
    CSTmDPTx& operator=(const CSTmDPTx&);
};

#endif //_STM_DPTX_H
