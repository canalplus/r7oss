/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#ifndef _HEVCPP_H_
#define _HEVCPP_H_

#define HEVCPP_RESET_TIME_LIMIT    10

#define HEVCPP_MAX_SUPPORTED_PRE_PROCESSORS 2
#define HEVCPP_REGISTER_OFFSET 0x1000

#define HEVCPP_MAX_CHUNK_SIZE  (0x02U)
#define HEVCPP_MAX_OPCODE_SIZE (0x03U << 2)

#define HEVCPP_CHKSYN_DIS_FORBIDDEN_BYTE_SEQUENCE (1U<<0)
#define HEVCPP_CHKSYN_DIS_HEADER_RANGE            (1U<<1)
#define HEVCPP_CHKSYN_DIS_HEADER_COHERENCY        (1U<<2)
#define HEVCPP_CHKSYN_DIS_HEADER_ALIGNMENT        (1U<<3)
#define HEVCPP_CHKSYN_DIS_WEIGHTED_PREDICTION     (1U<<4)
#define HEVCPP_CHKSYN_DIS_ENTRY_POINT_OFFSETS     (1U<<5)
#define HEVCPP_CHKSYN_DIS_DATA_RANGE              (1U<<6)
#define HEVCPP_CHKSYN_DIS_DATA_RENORM             (1U<<7)
#define HEVCPP_CHKSYN_DIS_DATA_ALIGNMENT          (1U<<8)
#define HEVCPP_CHKSYN_DIS_RBSP_TRAILING_BITS      (1U<<9)
#define HEVCPP_CHKSYN_DIS_RBSP_TRAILING_DATA      (1U<<10)

/* Registers */
#define HEVCPP_CHKSYN_DIS                 (0xC0)
#define HEVCPP_ERROR_STATUS               (0xC4)
#define HEVCPP_CTB_COMMANDS_STOP_OFFSET   (0xC8)
#define HEVCPP_CTB_RESIDUALS_STOP_OFFSET  (0xCC)
#define HEVCPP_SLICE_TABLE_ENTRIES        (0xD0)
#define HEVCPP_SLICE_TABLE_STOP_OFFSET    (0xE0)
#define HEVCPP_CTB_TABLE_STOP_OFFSET      (0xE4)
#define HEVCPP_SLICE_HEADERS_STOP_OFFSET  (0xE8)
#define HEVCPP_SLICE_TABLE_CRC            (0xEC)
#define HEVCPP_CTB_TABLE_CRC              (0xF0)
#define HEVCPP_SLICE_HEADERS_CRC          (0xF4)
#define HEVCPP_CTB_COMMANDS_CRC           (0xF8)
#define HEVCPP_CTB_RESIDUALS_CRC          (0xFC)

#define HEVCPP_PICTURE_SIZE               (0x0)
#define HEVCPP_CTB_CONFIG                 (0x4)
#define HEVCPP_PARALLEL_CONFIG            (0x8)

#define HEVCPP_TILE_COL_END_0             (0xC)
#define HEVCPP_TILE_COL_END_1             (0x10)
#define HEVCPP_TILE_COL_END_2             (0x14)
#define HEVCPP_TILE_COL_END_3             (0x18)
#define HEVCPP_TILE_COL_END_4             (0x1C)

#define HEVCPP_TILE_ROW_END_0             (0x20)
#define HEVCPP_TILE_ROW_END_1             (0x24)
#define HEVCPP_TILE_ROW_END_2             (0x28)
#define HEVCPP_TILE_ROW_END_3             (0x2C)
#define HEVCPP_TILE_ROW_END_4             (0x30)
#define HEVCPP_TILE_ROW_END_5             (0x34)

#define HEVCPP_SLICE_CONFIG               (0x38)
#define HEVCPP_REF_LIST_CONFIG            (0x3C)

#define HEVCPP_NUM_DELTA_POC_0            (0x40)
#define HEVCPP_NUM_DELTA_POC_1            (0x44)
#define HEVCPP_NUM_DELTA_POC_2            (0x48)
#define HEVCPP_NUM_DELTA_POC_3            (0x4C)
#define HEVCPP_NUM_DELTA_POC_4            (0x50)
#define HEVCPP_NUM_DELTA_POC_5            (0x54)
#define HEVCPP_NUM_DELTA_POC_6            (0x58)
#define HEVCPP_NUM_DELTA_POC_7            (0x5C)
#define HEVCPP_NUM_DELTA_POC_8            (0x60)
#define HEVCPP_NUM_DELTA_POC_9            (0x64)
#define HEVCPP_NUM_DELTA_POC_10           (0x68)
#define HEVCPP_NUM_DELTA_POC_11           (0x6C)
#define HEVCPP_NUM_DELTA_POC_12           (0x70)
#define HEVCPP_NUM_DELTA_POC_13           (0x74)
#define HEVCPP_NUM_DELTA_POC_14           (0x78)
#define HEVCPP_NUM_DELTA_POC_15           (0x7C)

#define HEVCPP_RESIDUAL_CONFIG            (0x80)
#define HEVCPP_IPCM_CONFIG                (0x84)
#define HEVCPP_LOOPFILTER_CONFIG          (0x88)
#define HEVCPP_BIT_BUFFER_BASE_ADDR       (0x8C)

#define HEVCPP_BIT_BUFFER_LENGTH          (0x90)
#define HEVCPP_BIT_BUFFER_START_OFFSET    (0x94)
#define HEVCPP_BIT_BUFFER_STOP_OFFSET     (0x98)
#define HEVCPP_SLICE_TABLE_BASE_ADDR      (0x9C)
#define HEVCPP_CTB_TABLE_BASE_ADDR        (0xA0)
#define HEVCPP_SLICE_HEADERS_BASE_ADDR    (0xA4)
#define HEVCPP_CTB_COMMANDS_BASE_ADDR     (0xA8)
#define HEVCPP_CTB_COMMANDS_LENGTH        (0xAC)
#define HEVCPP_CTB_COMMANDS_START_OFFSET  (0xB0)
#define HEVCPP_CTB_COMMANDS_END_OFFSET    (0x130)
#define HEVCPP_CTB_RESIDUALS_BASE_ADDR    (0xB4)
#define HEVCPP_CTB_RESIDUALS_LENGTH       (0xB8)
#define HEVCPP_CTB_RESIDUALS_START_OFFSET (0xBC)
#define HEVCPP_CTB_RESIDUALS_END_OFFSET   (0x134)
#define HEVCPP_SLICE_TABLE_LENGTH         (0xD4)
#define HEVCPP_CTB_TABLE_LENGTH           (0xD8)
#define HEVCPP_SLICE_HEADERS_LENGTH       (0xDC)

#define HEVCPP_START                      (0x100)
#define HEVCPP_CTRL_OBS                   (0x11C)
#define HEVCPP_RESET                      (0x120)

/* Plug registers */
#define HEVCPP_PLUG_REG_G1                (0x0)
#define HEVCPP_PLUG_REG_G2                (0x4)
#define HEVCPP_PLUG_REG_G3                (0x8)

#define HEVCPP_RD_PLUG_REG_G2             (0x804)
#define HEVCPP_RD_PLUG_REG_G3             (0x808)

#define HEVCPP_RD_PLUG_REG_C1             (0x820)

#endif
