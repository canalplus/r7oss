/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_odp.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _MPE41_ODP_H
#define _MPE41_ODP_H

#include <display/ip/panel/stmodp.h>

class CDisplayDevice;

class CSTmMPE41ODP: public CSTmODP
{
public:
    CSTmMPE41ODP(CDisplayDevice *pDev,
          uint32_t ulODPRegs);

    virtual uint32_t SetPowerTiming(
                                    stm_display_panel_power_timing_config_t* pTiming);
    virtual uint32_t SetForceUpdate(ForceUpdateRequestFlags_t update);
    virtual uint32_t HardwareUpdate(void);

    virtual ~CSTmMPE41ODP();

protected:
    uint32_t *m_pDevRegs;
    uint32_t  m_ulODPRegOffset;

    void Init(void);


#define ODP32_Write(Addr, Val) vibe_os_write_register(m_pDevRegs, m_ulODPRegOffset + Addr, Val)

#define ODP32_Read(Addr) (uint32_t)(vibe_os_read_register(m_pDevRegs, m_ulODPRegOffset + Addr))

#define ODP32_Set(Addr, Val) ODP32_Write(Addr,((ODP32_Read(Addr))| \
        (uint32_t)(Val)))

#define ODP32_Clear(Addr, Val) ODP32_Write(Addr,(ODP32_Read(Addr)& \
        (~(uint32_t)(Val))))

#define ODP32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
        ODP32_Write(Addr,((ODP32_Read(Addr)&(~(uint32_t)(BitsToClear))) | \
        ((uint32_t)(BitsToClear) & ((uint32_t)(ValueToSet)<<(Offset)))))

#define ODP32_SetField(Addr,Field,ValueToSet) \
    ODP32_ClearAndSet(Addr,Field ## _MASK,Field ## _OFFSET,ValueToSet)

#define ODP32_GetField(Addr,Field) \
    ((ODP32_Read(Addr) & (Field ## _MASK)) >> (Field ## _OFFSET))

#define ODP32_Bit(Address,Mask,ToSet) \
    {if (ToSet) {ODP32_Set(Address,Mask);} else {ODP32_Clear(Address,Mask);}}


private:
    CSTmMPE41ODP(const CSTmMPE41ODP&);
    CSTmMPE41ODP& operator=(const CSTmMPE41ODP&);
};

#endif //_MPE41_ODP_H
