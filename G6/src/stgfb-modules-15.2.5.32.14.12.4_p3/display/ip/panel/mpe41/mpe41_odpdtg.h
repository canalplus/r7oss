/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_odpdtg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _MPE41_ODPDTG_H
#define _MPE41_ODPDTG_H

#include <display/ip/panel/stmodp.h>
#include <display/ip/panel/stmodpdtg.h>

class CDisplayDevice;

class CSTmMPE41DTGOdp: public CSTmDTGOdp
{
public:
    CSTmMPE41DTGOdp(CDisplayDevice* pDev, uint32_t uRegOffset, CSTmODP *pODP, CSTmFSynth *pFSynth = 0, bool bDoubleClocked = true, stm_vtg_sync_type_t refpol = STVTG_SYNC_TIMING_MODE);
    virtual ~CSTmMPE41DTGOdp(void);


    uint32_t GetInterruptStatus(void);

protected:
    uint32_t          m_uDTGOffset;
    CSTmODP       *m_pODP;
    CSTmFSynth       *m_pFSynth;

    void EnableDTG(void);
    void DisableDTG(void);
    void EnableInterrupts(void);
    void DisableInterrupts(void);
    void DTGSetModeParams(void);
    void ProgramDTGSyncsParams(void);
    void ProgramDTGTimingsParams(void);
    int  ProgramDTGInputLockingParams(void);

#define DTG32_Write(Addr, Val) vibe_os_write_register(m_pDevRegs, m_uDTGOffset + Addr, Val)

#define DTG32_Read(Addr) (uint32_t)(vibe_os_read_register(m_pDevRegs, m_uDTGOffset + Addr))

#define DTG32_Set(Addr, Val) DTG32_Write(Addr,((DTG32_Read(Addr))| \
        (uint32_t)(Val)))

#define DTG32_Clear(Addr, Val) DTG32_Write(Addr,(DTG32_Read(Addr)& \
        (~(uint32_t)(Val))))

#define DTG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
        DTG32_Write(Addr,((DTG32_Read(Addr)&(~(uint32_t)(BitsToClear))) | \
        ((uint32_t)(BitsToClear) & ((uint32_t)(ValueToSet)<<(Offset)))))

#define DTG32_SetField(Addr,Field,ValueToSet) \
    DTG32_ClearAndSet(Addr,Field ## _MASK,Field ## _OFFSET,ValueToSet)

#define DTG32_GetField(Addr,Field) \
    ((DTG32_Read(Addr) & (Field ## _MASK)) >> (Field ## _OFFSET))

#define DTG32_Bit(Address,Mask,ToSet) \
    {if (ToSet) {DTG32_Set(Address,Mask);} else {DTG32_Clear(Address,Mask);}}

private:
    CSTmMPE41DTGOdp(const CSTmMPE41DTGOdp&);
    CSTmMPE41DTGOdp& operator=(const CSTmMPE41DTGOdp&);
};

#endif // _MPE41_ODPDTG_H
