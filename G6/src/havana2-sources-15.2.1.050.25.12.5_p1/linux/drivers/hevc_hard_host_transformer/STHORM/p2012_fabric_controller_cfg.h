/**
 * \brief Configuration for a P2012 Fabric Controller
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
 * Authors: Arthur STOUTCHININ, STMicroelectronics (arthur.stoutchinin@st.com)
 *          Bruno GALILEE, STMicroelectronics (bruno.galilee@st.com)
 *          Bruno LAVIGUEUR (bruno.lavigueur@st.com)
 *
 */

#ifndef _P2012_FABRIC_CONTROLLER_CFG_included_
#define _P2012_FABRIC_CONTROLLER_CFG_included_

#define P2012_FC_CID                0x3FF

// =================================================================
//                        Internal Memories
// =================================================================

#define P2012_CONF_FC_TCDM_OFFSET   0x00000000
#define P2012_IMPL_FC_TCDM_SIZE     0x00008000
#define P2012_CONF_FC_TCDM_SIZE     0x00010000

// =================================================================
//                           Peripherals
// =================================================================
#define P2012_CONF_FC_PR_OFFSET     (P2012_CONF_FC_TCDM_OFFSET + P2012_CONF_FC_TCDM_SIZE)
#define P2012_CONF_FC_PR_SIZE       0x00010000

// Warning: pr_control module both include CTRL *and* ITC

// =================================================================
//                             DMA
// =================================================================

#define P2012_CONF_FC_DMA_OFFSET    (P2012_CONF_FC_PR_OFFSET + P2012_CONF_FC_PR_SIZE)
#define P2012_CONF_FC_DMA_SIZE      0x00010000

// =================================================================
//                              STM
// =================================================================
#define P2012_CONF_FC_STM_OFFSET    (P2012_CONF_FC_DMA_OFFSET + P2012_CONF_FC_DMA_SIZE)
#define P2012_CONF_FC_STM_SIZE      0x00010000


// =================================================================
//                      STxP70 CFG, CFG1
// =================================================================

// - 64 bits external and MFU interfaces
// - 32b extension interface
// - 7 + 1 interrupts
// - 0B TCPM
// - 32KB TCDM
// - no EVC : 4 global events, 4 local events


//
//    B7    B6   B5    B4   B3   B2    B1    B0
//    8     1    D     0    3    9     1     C
//   .....______....._____......_____......______
//
//   1 0000 0 0111 01 00000 011 10 010 001 1 1 00
//   - ---- - ---- -- ----- --- -- --- --- - - --
//   | |    | |    |  |     |   |  |   |   | | |
//   | |    | |    |  |     |   |  |   |   | | 1 contexts
//   | |    | |    |  |     |   |  |   |   | 2 register bank per ctxt = 2 banks total
//   | |    | |    |  |     |   |  |   |   Multiplier implemented in X3
//   | |    | |    |  |     |   |  |   EFU: 32-bit
//   | |    | |    |  |     |   |  MFU: 64 bits
//   | |    | |    |  |     |   EXT: 64-bit
//   | |    | |    |  |     ITC: 31+1 nodes
//   | |    | |    |  no event controller
//   | |    | |    2 hardware loops implemented shared by all contexts
//   | |    | data memory is 32K Bytes
//   | |    DCACHE off
//   | Program Memory none
//   PCACHE on
//

// IDPA: This new core config is a temporary solution to avoid segmentation fault in cosimulation platforms.
//        There are mainly two changes:
//        - the Data Cache is set 'OFF'. Setting 'ON' the data cache is the source of the segmentation fault.
//        - Set 2 register bank per ctxt. There are some libraries(as crt0.o) configured with 2 register bank.
//                                        When these libraries are linked with the compiled program and the program is run,
//                                        an error is raised showing the mismatch between core configs.


#define P2012_FABRIC_CONTROLLER_CORE_CFG_VAL     0x81D0391C

//
// - no pixel mode
// - 2 ALUs
//
//   B7    B6   B5    B4   B3   B2    B1    B0
//    0     0    1     4    0    C     0     0
//   .....______....._____......_____......______
//
//   0000 0000 0001 0100 0 0 00 11 000 000 0 00 0
//   ---- ---- ---- ---- - - -- -- --- --- - -- -
//   |    |    |    |    | | |  |  |   |   | |  |
//   |    |    |    |    | | |  |  |   |   | |  pixel mode not supported
//   |    |    |    |    | | |  |  |   |   | pixel size (unused)
//   |    |    |    |    | | |  |  |   |   ROM Patch not enabled
//   |    |    |    |    | | |  |  |   Misalignment: none
//   |    |    |    |    | | |  |  Misalignment grain: unused
//   |    |    |    |    | | |  Dual Issue + 2 ALUs
//   |    |    |    |    | | MPU: no MPU
//   |    |    |    |    | Watchpoint not supported
//   |    |    |    |     Unused
//   |    |    |    User CFG: 4 (p2012 specific, for idle mode)
//   |    |    Branch History Buffer: 8 entries
//   |    Unused
//   Unused
//
#define P2012_FABRIC_CONTROLLER_CORE_CFG1_VAL    0x00140C00

// =================================================================
//                           STxP70 PCU_CFG
// =================================================================

//                0x     0    0    0    0    0    2    1    9
//                0b  0000 0000 0000 0000 0000 0010 0001 1001
//PCU_CFG.LINE_SZ     .... .... .... .... .... .... .... ..01 - 256 bits (default configuration)
//Reserved            .... .... .... .... .... .... .... .0.. - Reserved
//PCU_CFG.SIZE        .... .... .... .... .... .... ...1 1... - 32 K-Bytes
//Reserved            .... .... .... .... .... .... .00. .... - Reserved
//PCU_CFG.L2          .... .... .... .... .... .... 0... .... - No L2 memory implemented
//PCU_CFG.EXTM        .... .... .... .... .... ..10 .... .... - 64-bit PCU external interface
//PCU_CFG.SA          .... .... .... .... .... .0.. .... .... - Sequential accesses disabled
//Reserved            0000 0000 0000 0000 0000 0... .... .... - Reserved

#ifdef HEVC_HADES_CUT1
#define P2012_FABRIC_CONTROLLER_PCU_CFG_VAL      0x00000219
#else
#define P2012_FABRIC_CONTROLLER_PCU_CFG_VAL      0x00000209
#endif
// =================================================================
//                           STxP70 DCU_CFG
// =================================================================

//                0x     2    2    0    0    0    0    1    1
//                0b  0010 0010 0000 0000 0000 0000 0001 0001
//DCU_CFG.LINE_SZ     .... .... .... .... .... .... .... ..01 - 256 bits (default configuration)
//Reserved            .... .... .... .... .... .... .... .0.. - Reserved
//DCU_CFG.SIZE        .... .... .... .... .... .... ...1 0... - 16 K-Bytes
//Reserved            .... .... .... .... .... .... 000. .... - Reserved
//DCU_CFG.EXTM        .... .... .... .... .... ...0 .... .... - 64-bit DCU external interface
//Reserved            0000 0000 0000 0000 0000 000. .... .... - Reserved

// no D$ -> no DCU
// #define P2012_FABRIC_CONTROLLER_DCU_CFG_VAL      0x00000011

// =================================================================
//                           STxP70 DSTA
// =================================================================

//                0x     2    2    0    0    0    0    0    0
//                0b  0010 0010 0000 0000 0000 0000 0000 0000
//DCU_CFG.BYP         .... .... .... .... .... .............0 - Bypass bit
//Reserved            .... ...0 0000 0000 0000 0000 0000 000. - Reserved


// =================================================================
//                     TCU Interrupts mapping
// =================================================================

#define P2012_FC_TCU_IRQ_OUTPUT       3   // Timer IT line linked to the FC processor
#define P2012_FC_TCU_IRQ_PERIPH       7   // Timer IT line linked to the FC periph reg

// =================================================================
//                     Peripheral Register
// =================================================================
// bit field position
#define P2012_FC_PR_FP_FETCH_ENABLE_POS 31  // Peripheral Register bitfield mapping
#define P2012_FC_PR_FP_SW_RST_POS       31
#define P2012_FC_PR_FP_IT_NMI_POS       31
#define P2012_FC_PR_FP_FETCH_ENABLE_MSK (1 << P2012_FC_PR_FP_FETCH_ENABLE_POS)
#define P2012_FC_PR_FP_SW_RST_MSK       (1 << P2012_FC_PR_FP_SW_RST_POS)
#define P2012_FC_PR_FP_IT_NMI_MSK       (1 << P2012_FC_PR_FP_IT_NMI_POS)


// =================================================================
//                      STxP70 Interrupts
// =================================================================
#define P2012_FC_IRQ_MAILBOX_def       0  // STxP70 Interrupt from maibox
#define P2012_FC_IRQ_TIMER_GLB0_def    1  // STxP70 Interrupt from timer
#define P2012_FC_IRQ_TIMER_GLB1_def    2  // STxP70 Interrupt from timer
#define P2012_FC_IRQ_FC_ERROR_def      3  // STxP70 interrupt from FC Periph Control
#define P2012_FC_IRQ_COMB_ITAND_def    4  // STxP70 interrupt from IT combiner
#define P2012_FC_IRQ_COMB_ITOR_def     5
#define P2012_FC_IRQ_ERROR_def         7  // See STxP70_MEMORY_MAP_ERROR
#define P2012_FC_IRQ_IO0_def           8
#define P2012_FC_IRQ_IO1_def           9
#define P2012_FC_IRQ_IO2_def          10
#define P2012_FC_IRQ_IO3_def          11
#define P2012_FC_IRQ_IO4_def          12
#define P2012_FC_IRQ_IO5_def          13
#define P2012_FC_IRQ_IO6_def          14
#define P2012_FC_IRQ_IO7_def          15
#define P2012_FC_IRQ_DCHAN0_def       16  // STxP70 interrupt from DCHAN0
#define P2012_FC_IRQ_DCHAN0_ERR_def   17
#define P2012_FC_IRQ_CVP_GLOBAL_def   29
#define P2012_FC_IRQ_CVP_CORE_def     30

#ifndef P2012_ASM_FILE // 'enum' prevents this header from being included in an ASM .S file
enum p2012_fp_it_map {
  P2012_FC_IRQ_MAILBOX    =  P2012_FC_IRQ_MAILBOX_def,
  P2012_FC_IRQ_TIMER_GLB0 =  P2012_FC_IRQ_TIMER_GLB0_def,
  P2012_FC_IRQ_TIMER_GLB1 =  P2012_FC_IRQ_TIMER_GLB1_def,
  P2012_FC_IRQ_FC_ERROR   =  P2012_FC_IRQ_FC_ERROR_def,
  P2012_FC_IRQ_COMB_ITAND =  P2012_FC_IRQ_COMB_ITAND_def,
  P2012_FC_IRQ_COMB_ITOR  =  P2012_FC_IRQ_COMB_ITOR_def,
  P2012_FC_IRQ_ERROR      =  P2012_FC_IRQ_ERROR_def,
  P2012_FC_IRQ_IO0        =  P2012_FC_IRQ_IO0_def,
  P2012_FC_IRQ_IO1        =  P2012_FC_IRQ_IO1_def,
  P2012_FC_IRQ_IO2        =  P2012_FC_IRQ_IO2_def,
  P2012_FC_IRQ_IO3        =  P2012_FC_IRQ_IO3_def,
  P2012_FC_IRQ_IO4        =  P2012_FC_IRQ_IO4_def,
  P2012_FC_IRQ_IO5        =  P2012_FC_IRQ_IO5_def,
  P2012_FC_IRQ_IO6        =  P2012_FC_IRQ_IO6_def,
  P2012_FC_IRQ_IO7        =  P2012_FC_IRQ_IO7_def,
  P2012_FC_IRQ_DCHAN0     =  P2012_FC_IRQ_DCHAN0_def,
  P2012_FC_IRQ_DCHAN0_ERR =  P2012_FC_IRQ_DCHAN0_ERR_def,
  P2012_FC_IRQ_CVP_GLOBAL =  P2012_FC_IRQ_CVP_GLOBAL_def,
  P2012_FC_IRQ_CVP_CORE   =  P2012_FC_IRQ_CVP_CORE_def,
};
#endif

#define P2012_FC_IRQ_IO_SIZE 8  // (14-7)

#define P2012_FC_EVT_OUT_PR_IDX       0
#define P2012_FC_ITCOMBI_EVTx_IDX     0
#define P2012_FC_EVT_OUT_SIZE         8

// ITComb connection mapping
#define P2012_FC_DMA_ITCOMBI_IDX      16
#define P2012_FC_ITCOMBI_DMA_IDX      16
#define P2012_FC_ITCOMBI_DMA_SIZE     16

// EVTx connection mapping
#define P2012_FC_EVTXIN_ITCOMB_OR      0
#define P2012_FC_EVTXIN_ITCOMB_AND     1
#define P2012_FC_EVTXIN_DMA_IDX        2 // Start 'evtx_in' port index on EVTx side
#define P2012_FC_DMA_EVTXIN_IDX        0 // Start 'dma_event' port index on DMA side
#define P2012_FC_EVTXIN_DMA_SIZE       4

#endif // _P2012_FABRIC_CONTROLLER_CFG_included_

