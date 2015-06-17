/*
 * Host driver emulation layer API - HCE specific part
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *
 */

#ifndef _HOST_HCE_DRIVER_EMU_h_
#define _HOST_HCE_DRIVER_EMU_h_

#include "driverEmu.h"

// STHORM register access
void sthorm_write32(uint32_t physicalAddr, uint32_t value);
uint32_t sthorm_read32(uint32_t physicalAddr);
void sthorm_write8(uint32_t physicalAddr, uint8_t value);
void sthorm_write_block(uint32_t physicalAddr, uint8_t *value, unsigned int size);
void sthorm_set_block(uint32_t physicalAddr, uint8_t value, unsigned int size);

#define tlm_write32 sthorm_write32
#define tlm_read32  sthorm_read32
#define tlm_write8  sthorm_write8
#define p2012_write sthorm_write32
#define p2012_read  sthorm_read32

#if 0
// JLX: original functions

static inline void tlm_write32(uint32_t addr, uint32_t value)
{
    esw_write32((esw_uint64_t)addr, (esw_uint32_t)value);
}

static inline void tlm_write16(uint32_t addr, uint16_t value)
{
    esw_write16((esw_uint64_t)addr, (esw_uint16_t)value);
}

static inline void tlm_write8(uint32_t addr, uint8_t value)
{
    //LIB_MSG("tlm_write8 0x%x = 0x%x\n",addr,value);
    esw_write8((esw_uint64_t)addr, (esw_uint8_t)value);
}

static inline int tlm_write_block(uint32_t addr, uint8_t *value, unsigned int size)
{
    /*
    esw_block_write8((esw_uint64_t)addr, (esw_uint8_t*)value, size);
    return 0;
    */

    uint8_t *data = value;

    // Write the first unaligned bytes, if any
    while (((addr & 0x3) != 0) && (size != 0)) {
        tlm_write8(addr, *data);
        addr++; data++; size--;
    }
    // Write all aligned 32bit words, if any
    for ( ; size>=4; addr+=4, data+=4, size-=4) {
        tlm_write32(addr, *((uint32_t*)data));
    }
    // Write the remaining unaligned bytes, if any
    while (size != 0) {
        tlm_write8(addr, *data);
        addr++; data++; size--;
    }
    return 0;
}

static inline uint32_t tlm_read32(uint32_t addr)
{
    uint32_t value = esw_read32((esw_uint64_t)addr);
    //LIB_MSG("tlm_read32 0x%x = %d\n",addr,value);
    return value;
}

static inline uint16_t tlm_read16(uint32_t addr)
{
    return esw_read16((esw_uint64_t)addr);
}

static inline uint8_t tlm_read8(uint32_t addr)
{
    return esw_read8((esw_uint64_t)addr);
}

static inline int tlm_read_block(uint32_t addr, uint8_t *buffer, unsigned int size)
{
    esw_block_read8((esw_uint64_t) addr, (esw_uint8_t *) buffer, size);
    /*
    unsigned int i;
    uint8_t *mem = (uint8_t*) addr;
    for (i=0; i<size; i++) {
        buffer[i] = mem[i];
    }
    */
    return 0;
}
#endif

#endif /* _HOST_HCE_DRIVER_EMU_h_ */
