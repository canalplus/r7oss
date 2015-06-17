/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_dptx.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _MPE41_DPTX_H
#define _MPE41_DPTX_H

#include <display/ip/panel/stmdptx.h>

class CDisplayDevice;

class CSTmMPE41DPTx : public CSTmDPTx
{
public:
    CSTmMPE41DPTx(CDisplayDevice *pDev,
          uint32_t ulDPTxRegs);

    virtual uint32_t Start(const stm_display_mode_t* pModeLine);

    virtual ~CSTmMPE41DPTx();

protected:
    uint32_t *m_pDevRegs;
    uint32_t m_ulDPTxOffset;

#define DPTX32_Write(Addr, Val) vibe_os_write_register(m_pDevRegs, m_ulDPTxOffset + Addr, Val)

#define DPTX32_Read(Addr) (uint32_t)(vibe_os_read_register(m_pDevRegs, m_ulDPTxOffset + Addr))

#define DPTX32_Set(Addr, Val) DPTX32_Write(Addr,((DPTX32_Read(Addr))| \
        (uint32_t)(Val)))

#define DPTX32_Clear(Addr, Val) DPTX32_Write(Addr,(DPTX32_Read(Addr)& \
        (~(uint32_t)(Val))))

#define DPTX32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
        DPTX32_Write(Addr,((DPTX32_Read(Addr)&(~(uint32_t)(BitsToClear))) | \
        ((uint32_t)(BitsToClear) & ((uint32_t)(ValueToSet)<<(Offset)))))

#define DPTX32_SetField(Addr,Field,ValueToSet) \
    DPTX32_ClearAndSet(Addr,Field ## _MASK,Field ## _OFFSET,ValueToSet)

#define DPTX32_GetField(Addr,Field) \
    ((DPTX32_Read(Addr) & (Field ## _MASK)) >> (Field ## _OFFSET))

#define DPTX32_Bit(Address,Mask,ToSet) \
    {if (ToSet) {DPTX32_Set(Address,Mask);} else {DPTX32_Clear(Address,Mask);}}

private:
    CSTmMPE41DPTx(const CSTmMPE41DPTx&);
    CSTmMPE41DPTx& operator=(const CSTmMPE41DPTx&);
};

#endif //_MPE41_DPTX_H
