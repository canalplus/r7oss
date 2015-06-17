/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_odp_regs.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
/*Generated on Tue Jan 03 19:56:18 EST 2012 using DocumentProcessor 2.8.2.eng */
#ifndef _MPE41_ODP_REGS_
#define _MPE41_ODP_REGS_



// address block ODP [0x200]


# define ODP_BASE_ADDRESS 0x200


// Register ODP_PBUS_ENDIAN_SWAP(0x200)
#define ODP_PBUS_ENDIAN_SWAP                                                              0x00000200 //RW POD:0x0
#define ENDIAN_SWAP_EN_MASK                                                               0x00000001
#define ENDIAN_SWAP_EN_OFFSET                                                                      0
#define ENDIAN_SWAP_EN_WIDTH                                                                       1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_ID(0x204)
#define ODP_ID                                                                            0x00000204 //RO POD:0x200
#define ODP_ID_MASK                                                                       0x00000fff
#define ODP_ID_OFFSET                                                                              0
#define ODP_ID_WIDTH                                                                              12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_RESET_CTRL(0x208)
#define ODP_RESET_CTRL                                                                    0x00000208 //RW POD:0x0
#define ODP_HARD_RESET_MASK                                                               0x00000001
#define ODP_HARD_RESET_OFFSET                                                                      0
#define ODP_HARD_RESET_WIDTH                                                                       1
#define ODP_SOFT_RESET_MASK                                                               0x00000002
#define ODP_SOFT_RESET_OFFSET                                                                      1
#define ODP_SOFT_RESET_WIDTH                                                                       1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register ODP_RESET_STATUS(0x20c)
#define ODP_RESET_STATUS                                                                  0x0000020c //RO POD:0x0
#define ODP_RESET_STATUS_MASK                                                             0x00000001
#define ODP_RESET_STATUS_OFFSET                                                                    0
#define ODP_RESET_STATUS_WIDTH                                                                     1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_PA_REG_UPDATE_CTRL(0x210)
#define ODP_PA_REG_UPDATE_CTRL                                                            0x00000210 //CRW POD:0x0
#define ODP_PA_SYNC_UPDATE_MASK                                                           0x00000001
#define ODP_PA_SYNC_UPDATE_OFFSET                                                                  0
#define ODP_PA_SYNC_UPDATE_WIDTH                                                                   1
#define ODP_PA_FORCE_UPDATE_MASK                                                          0x00000002
#define ODP_PA_FORCE_UPDATE_OFFSET                                                                 1
#define ODP_PA_FORCE_UPDATE_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register ODP_PA_REG_READ_CTRL(0x214)
#define ODP_PA_REG_READ_CTRL                                                              0x00000214 //RW POD:0x0
#define ODP_PA_READ_ACTIVE_MASK                                                           0x00000001
#define ODP_PA_READ_ACTIVE_OFFSET                                                                  0
#define ODP_PA_READ_ACTIVE_WIDTH                                                                   1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_DISPLAY_IRQ1_STATUS(0x218)
#define ODP_DISPLAY_IRQ1_STATUS                                                           0x00000218 //CRO POD:0x0
#define D_ACTIVE_MASK                                                                     0x00000001
#define D_ACTIVE_OFFSET                                                                            0
#define D_ACTIVE_WIDTH                                                                             1
#define D_BLANK_MASK                                                                      0x00000002
#define D_BLANK_OFFSET                                                                             1
#define D_BLANK_WIDTH                                                                              1
#define D_LINEFLAG1_MASK                                                                  0x00000004
#define D_LINEFLAG1_OFFSET                                                                         2
#define D_LINEFLAG1_WIDTH                                                                          1
#define D_LINEFLAG2_MASK                                                                  0x00000008
#define D_LINEFLAG2_OFFSET                                                                         3
#define D_LINEFLAG2_WIDTH                                                                          1
#define D_LINEFLAG3_MASK                                                                  0x00000010
#define D_LINEFLAG3_OFFSET                                                                         4
#define D_LINEFLAG3_WIDTH                                                                          1
#define D_LINEFLAG4_MASK                                                                  0x00000020
#define D_LINEFLAG4_OFFSET                                                                         5
#define D_LINEFLAG4_WIDTH                                                                          1
#define D_VS_EDGE_MASK                                                                    0x00000040
#define D_VS_EDGE_OFFSET                                                                           6
#define D_VS_EDGE_WIDTH                                                                            1
#define D_UF_MASK                                                                         0x00000080
#define D_UF_OFFSET                                                                                7
#define D_UF_WIDTH                                                                                 1
#define ODP_FORCE_INTR_MASK                                                               0x00000100
#define ODP_FORCE_INTR_OFFSET                                                                      8
#define ODP_FORCE_INTR_WIDTH                                                                       1
// #define RESERVED_MASK                                                                     0xfffffe00
// #define RESERVED_OFFSET                                                                            9
// #define RESERVED_WIDTH                                                                            23



// Register ODP_DISPLAY_IRQ1_MASK(0x21c)
#define ODP_DISPLAY_IRQ1_MASK                                                             0x0000021c //RW POD:0x0
#define D_ACTIVE_MASK_MASK                                                                0x00000001
#define D_ACTIVE_MASK_OFFSET                                                                       0
#define D_ACTIVE_MASK_WIDTH                                                                        1
#define D_BLANK_MASK_MASK                                                                 0x00000002
#define D_BLANK_MASK_OFFSET                                                                        1
#define D_BLANK_MASK_WIDTH                                                                         1
#define D_LINEFLAG1_MASK_MASK                                                             0x00000004
#define D_LINEFLAG1_MASK_OFFSET                                                                    2
#define D_LINEFLAG1_MASK_WIDTH                                                                     1
#define D_LINEFLAG2_MASK_MASK                                                             0x00000008
#define D_LINEFLAG2_MASK_OFFSET                                                                    3
#define D_LINEFLAG2_MASK_WIDTH                                                                     1
#define D_LINEFLAG3_MASK_MASK                                                             0x00000010
#define D_LINEFLAG3_MASK_OFFSET                                                                    4
#define D_LINEFLAG3_MASK_WIDTH                                                                     1
#define D_LINEFLAG4_MASK_MASK                                                             0x00000020
#define D_LINEFLAG4_MASK_OFFSET                                                                    5
#define D_LINEFLAG4_MASK_WIDTH                                                                     1
#define D_VS_EDGE_MASK_MASK                                                               0x00000040
#define D_VS_EDGE_MASK_OFFSET                                                                      6
#define D_VS_EDGE_MASK_WIDTH                                                                       1
#define D_UF_MASK_MASK                                                                    0x00000080
#define D_UF_MASK_OFFSET                                                                           7
#define D_UF_MASK_WIDTH                                                                            1
#define ODP_FORCE_INTR_MASK_MASK                                                          0x00000100
#define ODP_FORCE_INTR_MASK_OFFSET                                                                 8
#define ODP_FORCE_INTR_MASK_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xfffffe00
// #define RESERVED_OFFSET                                                                            9
// #define RESERVED_WIDTH                                                                            23



// Register ODP_DISPLAY_IRQ1_MODE(0x220)
#define ODP_DISPLAY_IRQ1_MODE                                                             0x00000220 //RW POD:0x3f
#define D_ACTIVE_MODE_MASK                                                                0x00000001
#define D_ACTIVE_MODE_OFFSET                                                                       0
#define D_ACTIVE_MODE_WIDTH                                                                        1
#define D_BLANK_MODE_MASK                                                                 0x00000002
#define D_BLANK_MODE_OFFSET                                                                        1
#define D_BLANK_MODE_WIDTH                                                                         1
#define D_LINEFLAG1_MODE_MASK                                                             0x00000004
#define D_LINEFLAG1_MODE_OFFSET                                                                    2
#define D_LINEFLAG1_MODE_WIDTH                                                                     1
#define D_LINEFLAG2_MODE_MASK                                                             0x00000008
#define D_LINEFLAG2_MODE_OFFSET                                                                    3
#define D_LINEFLAG2_MODE_WIDTH                                                                     1
#define D_LINEFLAG3_MODE_MASK                                                             0x00000010
#define D_LINEFLAG3_MODE_OFFSET                                                                    4
#define D_LINEFLAG3_MODE_WIDTH                                                                     1
#define D_LINEFLAG4_MODE_MASK                                                             0x00000020
#define D_LINEFLAG4_MODE_OFFSET                                                                    5
#define D_LINEFLAG4_MODE_WIDTH                                                                     1
#define D_VS_EDGE_MODE_MASK                                                               0x00000040
#define D_VS_EDGE_MODE_OFFSET                                                                      6
#define D_VS_EDGE_MODE_WIDTH                                                                       1
#define D_UF_MODE_MASK                                                                    0x00000080
#define D_UF_MODE_OFFSET                                                                           7
#define D_UF_MODE_WIDTH                                                                            1
#define ODP_FORCE_INTR_MODE_MASK                                                          0x00000100
#define ODP_FORCE_INTR_MODE_OFFSET                                                                 8
#define ODP_FORCE_INTR_MODE_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xfffffe00
// #define RESERVED_OFFSET                                                                            9
// #define RESERVED_WIDTH                                                                            23



// Register ODP_FORCE_IRQ1(0x224)
#define ODP_FORCE_IRQ1                                                                    0x00000224 //RW POD:0x0
#define ODP_FORCE_INTERRUPT_MASK                                                          0x00000001
#define ODP_FORCE_INTERRUPT_OFFSET                                                                 0
#define ODP_FORCE_INTERRUPT_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_CLK_GATING_CTRL(0x228)
#define ODP_CLK_GATING_CTRL                                                               0x00000228 //RW POD:0x0
#define ODP_CLK_DISABLE_MASK                                                              0x00000001
#define ODP_CLK_DISABLE_OFFSET                                                                     0
#define ODP_CLK_DISABLE_WIDTH                                                                      1
#define PWR_TCLK_DISABLE_MASK                                                             0x00000002
#define PWR_TCLK_DISABLE_OFFSET                                                                    1
#define PWR_TCLK_DISABLE_WIDTH                                                                     1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register ODP_FRAME_RESET_CTRL(0x22c)
#define ODP_FRAME_RESET_CTRL                                                              0x0000022c //RW POD:0x0
#define ODP_FW_FRAME_RESET_EN_MASK                                                        0x00000001
#define ODP_FW_FRAME_RESET_EN_OFFSET                                                               0
#define ODP_FW_FRAME_RESET_EN_WIDTH                                                                1
#define ODP_FRAME_RESET_SEL_MASK                                                          0x00000002
#define ODP_FRAME_RESET_SEL_OFFSET                                                                 1
#define ODP_FRAME_RESET_SEL_WIDTH                                                                  1
#define ODP_HW_FRAME_RESET_MASK_MASK                                                      0x00000004
#define ODP_HW_FRAME_RESET_MASK_OFFSET                                                             2
#define ODP_HW_FRAME_RESET_MASK_WIDTH                                                              1
// #define RESERVED_MASK                                                                     0xfffffff8
// #define RESERVED_OFFSET                                                                            3
// #define RESERVED_WIDTH                                                                            29



// Register FW_FRAME_RESET_CTRL(0x230)
#define FW_FRAME_RESET_CTRL                                                               0x00000230 //CRW POD:0x0
#define ODP_FW_FRAME_RESET_MASK                                                           0x00000001
#define ODP_FW_FRAME_RESET_OFFSET                                                                  0
#define ODP_FW_FRAME_RESET_WIDTH                                                                   1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register CPF_TX_FRAME_RESET_SEL(0x234)
#define CPF_TX_FRAME_RESET_SEL                                                            0x00000234 //RW POD:0x0
#define ODP_LVDS_CPF_FRAME_RESET_SEL_MASK                                                 0x00000001
#define ODP_LVDS_CPF_FRAME_RESET_SEL_OFFSET                                                        0
#define ODP_LVDS_CPF_FRAME_RESET_SEL_WIDTH                                                         1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register CPF_TX_FW_FRAME_RESET_CTRL(0x238)
#define CPF_TX_FW_FRAME_RESET_CTRL                                                        0x00000238 //CRW POD:0x0
#define ODP_LVDS_CPF_FW_FRAME_RESET_MASK                                                  0x00000001
#define ODP_LVDS_CPF_FW_FRAME_RESET_OFFSET                                                         0
#define ODP_LVDS_CPF_FW_FRAME_RESET_WIDTH                                                          1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_COMPO_VSYNC_VCOUNT_CTRL(0x23c)
#define ODP_COMPO_VSYNC_VCOUNT_CTRL                                                       0x0000023c //RW POD:0x0
#define ODP_COMPO_VSYNC_SEL_MASK                                                          0x00000001
#define ODP_COMPO_VSYNC_SEL_OFFSET                                                                 0
#define ODP_COMPO_VSYNC_SEL_WIDTH                                                                  1
#define ODP_COMPO_VCOUNT_SEL_MASK                                                         0x00000002
#define ODP_COMPO_VCOUNT_SEL_OFFSET                                                                1
#define ODP_COMPO_VCOUNT_SEL_WIDTH                                                                 1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register GAMMA_LUT_CTRL(0x240)
#define GAMMA_LUT_CTRL                                                                    0x00000240 //PA POD:0x0 Event:ODP
// #define RESERVED_MASK                                                                     0x0000000f
// #define RESERVED_OFFSET                                                                            0
// #define RESERVED_WIDTH                                                                             4
#define GAMMA_LUT1_CTRL_MASK                                                              0x00000030
#define GAMMA_LUT1_CTRL_OFFSET                                                                     4
#define GAMMA_LUT1_CTRL_WIDTH                                                                      2
// #define RESERVED_MASK                                                                     0x000000c0
// #define RESERVED_OFFSET                                                                            6
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_LUT1_STEEP_CTRL_MASK                                                        0x00000300
#define GAMMA_LUT1_STEEP_CTRL_OFFSET                                                               8
#define GAMMA_LUT1_STEEP_CTRL_WIDTH                                                                2
// #define RESERVED_MASK                                                                     0x00000c00
// #define RESERVED_OFFSET                                                                           10
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_LUT1_STEEP_STEP_MASK                                                        0x00003000
#define GAMMA_LUT1_STEEP_STEP_OFFSET                                                              12
#define GAMMA_LUT1_STEEP_STEP_WIDTH                                                                2
// #define RESERVED_MASK                                                                     0x0000c000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_LUT2_CTRL_MASK                                                              0x00030000
#define GAMMA_LUT2_CTRL_OFFSET                                                                    16
#define GAMMA_LUT2_CTRL_WIDTH                                                                      2
// #define RESERVED_MASK                                                                     0x000c0000
// #define RESERVED_OFFSET                                                                           18
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_LUT2_STEEP_CTRL_MASK                                                        0x00300000
#define GAMMA_LUT2_STEEP_CTRL_OFFSET                                                              20
#define GAMMA_LUT2_STEEP_CTRL_WIDTH                                                                2
// #define RESERVED_MASK                                                                     0x00c00000
// #define RESERVED_OFFSET                                                                           22
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_LUT2_STEEP_STEP_MASK                                                        0x03000000
#define GAMMA_LUT2_STEEP_STEP_OFFSET                                                              24
#define GAMMA_LUT2_STEEP_STEP_WIDTH                                                                2
// #define RESERVED_MASK                                                                     0x0c000000
// #define RESERVED_OFFSET                                                                           26
// #define RESERVED_WIDTH                                                                             2
#define GAMMA_MATRIX_CTRL_MASK                                                            0x30000000
#define GAMMA_MATRIX_CTRL_OFFSET                                                                  28
#define GAMMA_MATRIX_CTRL_WIDTH                                                                    2
// #define RESERVED_MASK                                                                     0xc0000000
// #define RESERVED_OFFSET                                                                           30
// #define RESERVED_WIDTH                                                                             2



// Register GAMMA_LUT1_STEEP_STRT_R(0x244)
#define GAMMA_LUT1_STEEP_STRT_R                                                           0x00000244 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_STRT_R_MASK                                                      0x00003fff
#define GAMMA_LUT1_STEEP_STRT_R_OFFSET                                                             0
#define GAMMA_LUT1_STEEP_STRT_R_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_END_R(0x248)
#define GAMMA_LUT1_STEEP_END_R                                                            0x00000248 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_END_R_MASK                                                       0x00003fff
#define GAMMA_LUT1_STEEP_END_R_OFFSET                                                              0
#define GAMMA_LUT1_STEEP_END_R_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_FINAL_R(0x24c)
#define GAMMA_LUT1_STEEP_FINAL_R                                                          0x0000024c //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_FINAL_R_MASK                                                     0x00007fff
#define GAMMA_LUT1_STEEP_FINAL_R_OFFSET                                                            0
#define GAMMA_LUT1_STEEP_FINAL_R_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT1_STEEP_STRT_G(0x250)
#define GAMMA_LUT1_STEEP_STRT_G                                                           0x00000250 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_STRT_G_MASK                                                      0x00003fff
#define GAMMA_LUT1_STEEP_STRT_G_OFFSET                                                             0
#define GAMMA_LUT1_STEEP_STRT_G_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_END_G(0x254)
#define GAMMA_LUT1_STEEP_END_G                                                            0x00000254 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_END_G_MASK                                                       0x00003fff
#define GAMMA_LUT1_STEEP_END_G_OFFSET                                                              0
#define GAMMA_LUT1_STEEP_END_G_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_FINAL_G(0x258)
#define GAMMA_LUT1_STEEP_FINAL_G                                                          0x00000258 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_FINAL_G_MASK                                                     0x00007fff
#define GAMMA_LUT1_STEEP_FINAL_G_OFFSET                                                            0
#define GAMMA_LUT1_STEEP_FINAL_G_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT1_STEEP_STRT_B(0x25c)
#define GAMMA_LUT1_STEEP_STRT_B                                                           0x0000025c //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_STRT_B_MASK                                                      0x00003fff
#define GAMMA_LUT1_STEEP_STRT_B_OFFSET                                                             0
#define GAMMA_LUT1_STEEP_STRT_B_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_END_B(0x260)
#define GAMMA_LUT1_STEEP_END_B                                                            0x00000260 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_END_B_MASK                                                       0x00003fff
#define GAMMA_LUT1_STEEP_END_B_OFFSET                                                              0
#define GAMMA_LUT1_STEEP_END_B_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT1_STEEP_FINAL_B(0x264)
#define GAMMA_LUT1_STEEP_FINAL_B                                                          0x00000264 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_STEEP_FINAL_B_MASK                                                     0x00007fff
#define GAMMA_LUT1_STEEP_FINAL_B_OFFSET                                                            0
#define GAMMA_LUT1_STEEP_FINAL_B_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT1_FINAL_ENTRY_R(0x268)
#define GAMMA_LUT1_FINAL_ENTRY_R                                                          0x00000268 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_FINAL_ENTRY_R_MASK                                                     0x00007fff
#define GAMMA_LUT1_FINAL_ENTRY_R_OFFSET                                                            0
#define GAMMA_LUT1_FINAL_ENTRY_R_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT1_FINAL_ENTRY_G(0x26c)
#define GAMMA_LUT1_FINAL_ENTRY_G                                                          0x0000026c //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_FINAL_ENTRY_G_MASK                                                     0x00007fff
#define GAMMA_LUT1_FINAL_ENTRY_G_OFFSET                                                            0
#define GAMMA_LUT1_FINAL_ENTRY_G_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT1_FINAL_ENTRY_B(0x270)
#define GAMMA_LUT1_FINAL_ENTRY_B                                                          0x00000270 //PA POD:0x0 Event:ODP
#define GAMMA_LUT1_FINAL_ENTRY_B_MASK                                                     0x00007fff
#define GAMMA_LUT1_FINAL_ENTRY_B_OFFSET                                                            0
#define GAMMA_LUT1_FINAL_ENTRY_B_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_STEEP_STRT_R(0x274)
#define GAMMA_LUT2_STEEP_STRT_R                                                           0x00000274 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_STRT_R_MASK                                                      0x00003fff
#define GAMMA_LUT2_STEEP_STRT_R_OFFSET                                                             0
#define GAMMA_LUT2_STEEP_STRT_R_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_END_R(0x278)
#define GAMMA_LUT2_STEEP_END_R                                                            0x00000278 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_END_R_MASK                                                       0x00003fff
#define GAMMA_LUT2_STEEP_END_R_OFFSET                                                              0
#define GAMMA_LUT2_STEEP_END_R_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_FINAL_R(0x27c)
#define GAMMA_LUT2_STEEP_FINAL_R                                                          0x0000027c //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_FINAL_R_MASK                                                     0x00007fff
#define GAMMA_LUT2_STEEP_FINAL_R_OFFSET                                                            0
#define GAMMA_LUT2_STEEP_FINAL_R_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_STEEP_STRT_G(0x280)
#define GAMMA_LUT2_STEEP_STRT_G                                                           0x00000280 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_STRT_G_MASK                                                      0x00003fff
#define GAMMA_LUT2_STEEP_STRT_G_OFFSET                                                             0
#define GAMMA_LUT2_STEEP_STRT_G_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_END_G(0x284)
#define GAMMA_LUT2_STEEP_END_G                                                            0x00000284 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_END_G_MASK                                                       0x00003fff
#define GAMMA_LUT2_STEEP_END_G_OFFSET                                                              0
#define GAMMA_LUT2_STEEP_END_G_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_FINAL_G(0x288)
#define GAMMA_LUT2_STEEP_FINAL_G                                                          0x00000288 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_FINAL_G_MASK                                                     0x00007fff
#define GAMMA_LUT2_STEEP_FINAL_G_OFFSET                                                            0
#define GAMMA_LUT2_STEEP_FINAL_G_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_STEEP_STRT_B(0x28c)
#define GAMMA_LUT2_STEEP_STRT_B                                                           0x0000028c //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_STRT_B_MASK                                                      0x00003fff
#define GAMMA_LUT2_STEEP_STRT_B_OFFSET                                                             0
#define GAMMA_LUT2_STEEP_STRT_B_WIDTH                                                             14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_END_B(0x290)
#define GAMMA_LUT2_STEEP_END_B                                                            0x00000290 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_END_B_MASK                                                       0x00003fff
#define GAMMA_LUT2_STEEP_END_B_OFFSET                                                              0
#define GAMMA_LUT2_STEEP_END_B_WIDTH                                                              14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT2_STEEP_FINAL_B(0x294)
#define GAMMA_LUT2_STEEP_FINAL_B                                                          0x00000294 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_STEEP_FINAL_B_MASK                                                     0x00007fff
#define GAMMA_LUT2_STEEP_FINAL_B_OFFSET                                                            0
#define GAMMA_LUT2_STEEP_FINAL_B_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_FINAL_ENTRY_R(0x298)
#define GAMMA_LUT2_FINAL_ENTRY_R                                                          0x00000298 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_FINAL_ENTRY_R_MASK                                                     0x00007fff
#define GAMMA_LUT2_FINAL_ENTRY_R_OFFSET                                                            0
#define GAMMA_LUT2_FINAL_ENTRY_R_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_FINAL_ENTRY_G(0x29c)
#define GAMMA_LUT2_FINAL_ENTRY_G                                                          0x0000029c //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_FINAL_ENTRY_G_MASK                                                     0x00007fff
#define GAMMA_LUT2_FINAL_ENTRY_G_OFFSET                                                            0
#define GAMMA_LUT2_FINAL_ENTRY_G_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT2_FINAL_ENTRY_B(0x2a0)
#define GAMMA_LUT2_FINAL_ENTRY_B                                                          0x000002a0 //PA POD:0x0 Event:ODP
#define GAMMA_LUT2_FINAL_ENTRY_B_MASK                                                     0x00007fff
#define GAMMA_LUT2_FINAL_ENTRY_B_OFFSET                                                            0
#define GAMMA_LUT2_FINAL_ENTRY_B_WIDTH                                                            15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT_MATRIX_MULT_COEF11(0x2a4)
#define GAMMA_LUT_MATRIX_MULT_COEF11                                                      0x000002a4 //PA POD:0x400 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF11_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF11_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF11_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF12(0x2a8)
#define GAMMA_LUT_MATRIX_MULT_COEF12                                                      0x000002a8 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF12_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF12_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF12_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF13(0x2ac)
#define GAMMA_LUT_MATRIX_MULT_COEF13                                                      0x000002ac //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF13_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF13_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF13_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF21(0x2b0)
#define GAMMA_LUT_MATRIX_MULT_COEF21                                                      0x000002b0 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF21_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF21_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF21_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF22(0x2b4)
#define GAMMA_LUT_MATRIX_MULT_COEF22                                                      0x000002b4 //PA POD:0x400 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF22_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF22_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF22_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF23(0x2b8)
#define GAMMA_LUT_MATRIX_MULT_COEF23                                                      0x000002b8 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF23_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF23_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF23_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF31(0x2bc)
#define GAMMA_LUT_MATRIX_MULT_COEF31                                                      0x000002bc //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF31_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF31_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF31_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF32(0x2c0)
#define GAMMA_LUT_MATRIX_MULT_COEF32                                                      0x000002c0 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF32_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF32_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF32_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_MULT_COEF33(0x2c4)
#define GAMMA_LUT_MATRIX_MULT_COEF33                                                      0x000002c4 //PA POD:0x400 Event:ODP
#define GAMMA_LUT_MATRIX_MULT_COEF33_MASK                                                 0x00003fff
#define GAMMA_LUT_MATRIX_MULT_COEF33_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_MULT_COEF33_WIDTH                                                        14
// #define RESERVED_MASK                                                                     0xffffc000
// #define RESERVED_OFFSET                                                                           14
// #define RESERVED_WIDTH                                                                            18



// Register GAMMA_LUT_MATRIX_IN_OFFSET1(0x2c8)
#define GAMMA_LUT_MATRIX_IN_OFFSET1                                                       0x000002c8 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_IN_OFFSET1_MASK                                                  0x00007fff
#define GAMMA_LUT_MATRIX_IN_OFFSET1_OFFSET                                                         0
#define GAMMA_LUT_MATRIX_IN_OFFSET1_WIDTH                                                         15
// #define RESERVED_MASK                                                                     0x00008000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                             1
#define GAMMA_LUT_MATRIX_IN_OFFSET1_CLAMP_MASK                                            0x00010000
#define GAMMA_LUT_MATRIX_IN_OFFSET1_CLAMP_OFFSET                                                  16
#define GAMMA_LUT_MATRIX_IN_OFFSET1_CLAMP_WIDTH                                                    1
// #define RESERVED_MASK                                                                     0xfffe0000
// #define RESERVED_OFFSET                                                                           17
// #define RESERVED_WIDTH                                                                            15



// Register GAMMA_LUT_MATRIX_IN_OFFSET2(0x2cc)
#define GAMMA_LUT_MATRIX_IN_OFFSET2                                                       0x000002cc //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_IN_OFFSET2_MASK                                                  0x00007fff
#define GAMMA_LUT_MATRIX_IN_OFFSET2_OFFSET                                                         0
#define GAMMA_LUT_MATRIX_IN_OFFSET2_WIDTH                                                         15
// #define RESERVED_MASK                                                                     0x00008000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                             1
#define GAMMA_LUT_MATRIX_IN_OFFSET2_CLAMP_MASK                                            0x00010000
#define GAMMA_LUT_MATRIX_IN_OFFSET2_CLAMP_OFFSET                                                  16
#define GAMMA_LUT_MATRIX_IN_OFFSET2_CLAMP_WIDTH                                                    1
// #define RESERVED_MASK                                                                     0xfffe0000
// #define RESERVED_OFFSET                                                                           17
// #define RESERVED_WIDTH                                                                            15



// Register GAMMA_LUT_MATRIX_IN_OFFSET3(0x2d0)
#define GAMMA_LUT_MATRIX_IN_OFFSET3                                                       0x000002d0 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_IN_OFFSET3_MASK                                                  0x00007fff
#define GAMMA_LUT_MATRIX_IN_OFFSET3_OFFSET                                                         0
#define GAMMA_LUT_MATRIX_IN_OFFSET3_WIDTH                                                         15
// #define RESERVED_MASK                                                                     0x00008000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                             1
#define GAMMA_LUT_MATRIX_IN_OFFSET3_CLAMP_MASK                                            0x00010000
#define GAMMA_LUT_MATRIX_IN_OFFSET3_CLAMP_OFFSET                                                  16
#define GAMMA_LUT_MATRIX_IN_OFFSET3_CLAMP_WIDTH                                                    1
// #define RESERVED_MASK                                                                     0xfffe0000
// #define RESERVED_OFFSET                                                                           17
// #define RESERVED_WIDTH                                                                            15



// Register GAMMA_LUT_MATRIX_OUT_OFFSET1(0x2d4)
#define GAMMA_LUT_MATRIX_OUT_OFFSET1                                                      0x000002d4 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_OUT_OFFSET1_MASK                                                 0x00007fff
#define GAMMA_LUT_MATRIX_OUT_OFFSET1_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_OUT_OFFSET1_WIDTH                                                        15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT_MATRIX_OUT_OFFSET2(0x2d8)
#define GAMMA_LUT_MATRIX_OUT_OFFSET2                                                      0x000002d8 //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_OUT_OFFSET2_MASK                                                 0x00007fff
#define GAMMA_LUT_MATRIX_OUT_OFFSET2_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_OUT_OFFSET2_WIDTH                                                        15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT_MATRIX_OUT_OFFSET3(0x2dc)
#define GAMMA_LUT_MATRIX_OUT_OFFSET3                                                      0x000002dc //PA POD:0x0 Event:ODP
#define GAMMA_LUT_MATRIX_OUT_OFFSET3_MASK                                                 0x00007fff
#define GAMMA_LUT_MATRIX_OUT_OFFSET3_OFFSET                                                        0
#define GAMMA_LUT_MATRIX_OUT_OFFSET3_WIDTH                                                        15
// #define RESERVED_MASK                                                                     0xffff8000
// #define RESERVED_OFFSET                                                                           15
// #define RESERVED_WIDTH                                                                            17



// Register GAMMA_LUT_WINDOW_CTRL(0x2e0)
#define GAMMA_LUT_WINDOW_CTRL                                                             0x000002e0 //RW POD:0x0
#define GAMMA_LUT_WIN_CTRL_MASK                                                           0x00000003
#define GAMMA_LUT_WIN_CTRL_OFFSET                                                                  0
#define GAMMA_LUT_WIN_CTRL_WIDTH                                                                   2
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register GAMMA_LUT_LOADER_ADDR_CFG(0x2e4)
#define GAMMA_LUT_LOADER_ADDR_CFG                                                         0x000002e4 //RW POD:0x3080000
#define GAMMA_LUT_START_ADDR_MASK                                                         0x00001fff
#define GAMMA_LUT_START_ADDR_OFFSET                                                                0
#define GAMMA_LUT_START_ADDR_WIDTH                                                                13
#define GAMMA_FDMA_BURST_SIZE_MASK                                                        0x001fe000
#define GAMMA_FDMA_BURST_SIZE_OFFSET                                                              13
#define GAMMA_FDMA_BURST_SIZE_WIDTH                                                                8
#define GAMMA_FDMA_BURST_CNT_MASK                                                         0xffe00000
#define GAMMA_FDMA_BURST_CNT_OFFSET                                                               21
#define GAMMA_FDMA_BURST_CNT_WIDTH                                                                11



// Register GAMMA_LUT_LOADER_CTRL(0x2e8)
#define GAMMA_LUT_LOADER_CTRL                                                             0x000002e8 //CRW POD:0x0
#define GAMMA_LUT_LOADER_EN_MASK                                                          0x00000001
#define GAMMA_LUT_LOADER_EN_OFFSET                                                                 0
#define GAMMA_LUT_LOADER_EN_WIDTH                                                                  1
#define GAMMA_LUT_LOADER_SOFT_RESET_MASK                                                  0x00000002
#define GAMMA_LUT_LOADER_SOFT_RESET_OFFSET                                                         1
#define GAMMA_LUT_LOADER_SOFT_RESET_WIDTH                                                          1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register GAMMA_LUT_LOADER_CTRL2(0x2ec)
#define GAMMA_LUT_LOADER_CTRL2                                                            0x000002ec //RW POD:0x0
#define GAMMA_LUT_LOAD_ON_VBI_EDGE_MASK                                                   0x00000001
#define GAMMA_LUT_LOAD_ON_VBI_EDGE_OFFSET                                                          0
#define GAMMA_LUT_LOAD_ON_VBI_EDGE_WIDTH                                                           1
#define GAMMA_LUT_FORCE_LOAD_MASK                                                         0x00000002
#define GAMMA_LUT_FORCE_LOAD_OFFSET                                                                1
#define GAMMA_LUT_FORCE_LOAD_WIDTH                                                                 1
#define GAMMA_LUT_DISABLE_ENDIAN_MAPPING_MASK                                             0x00000004
#define GAMMA_LUT_DISABLE_ENDIAN_MAPPING_OFFSET                                                    2
#define GAMMA_LUT_DISABLE_ENDIAN_MAPPING_WIDTH                                                     1
#define ODP_PBUS_RAM_MODE_MASK                                                            0x00000008
#define ODP_PBUS_RAM_MODE_OFFSET                                                                   3
#define ODP_PBUS_RAM_MODE_WIDTH                                                                    1
#define GAMMA_LUT_LOAD_RGB_PARALLEL_MASK                                                  0x00000010
#define GAMMA_LUT_LOAD_RGB_PARALLEL_OFFSET                                                         4
#define GAMMA_LUT_LOAD_RGB_PARALLEL_WIDTH                                                          1
#define GAMMA_LUT_LOAD_LUTS_PARALLEL_MASK                                                 0x00000020
#define GAMMA_LUT_LOAD_LUTS_PARALLEL_OFFSET                                                        5
#define GAMMA_LUT_LOAD_LUTS_PARALLEL_WIDTH                                                         1
// #define RESERVED_MASK                                                                     0xffffffc0
// #define RESERVED_OFFSET                                                                            6
// #define RESERVED_WIDTH                                                                            26



// Register GAMMA_LUT_LOADER_FIFO_STATUS(0x2f0)
#define GAMMA_LUT_LOADER_FIFO_STATUS                                                      0x000002f0 //RO POD:0x0
#define GAMMA_LUT_LOADER_FIFO_OVF_MASK                                                    0x00000001
#define GAMMA_LUT_LOADER_FIFO_OVF_OFFSET                                                           0
#define GAMMA_LUT_LOADER_FIFO_OVF_WIDTH                                                            1
#define GAMMA_LUT_LOADER_FIFO_UF_MASK                                                     0x00000002
#define GAMMA_LUT_LOADER_FIFO_UF_OFFSET                                                            1
#define GAMMA_LUT_LOADER_FIFO_UF_WIDTH                                                             1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register GAMMA_LUT_LOOKUP_ERROR(0x2f4)
#define GAMMA_LUT_LOOKUP_ERROR                                                            0x000002f4 //CRO POD:0x0
#define GRN_LUT1_LOOKUP_ERR_MASK                                                          0x00000001
#define GRN_LUT1_LOOKUP_ERR_OFFSET                                                                 0
#define GRN_LUT1_LOOKUP_ERR_WIDTH                                                                  1
#define GRN_LUT2_LOOKUP_ERR_MASK                                                          0x00000002
#define GRN_LUT2_LOOKUP_ERR_OFFSET                                                                 1
#define GRN_LUT2_LOOKUP_ERR_WIDTH                                                                  1
#define BLU_LUT1_LOOKUP_ERR_MASK                                                          0x00000004
#define BLU_LUT1_LOOKUP_ERR_OFFSET                                                                 2
#define BLU_LUT1_LOOKUP_ERR_WIDTH                                                                  1
#define BLU_LUT2_LOOKUP_ERR_MASK                                                          0x00000008
#define BLU_LUT2_LOOKUP_ERR_OFFSET                                                                 3
#define BLU_LUT2_LOOKUP_ERR_WIDTH                                                                  1
#define RED_LUT1_LOOKUP_ERR_MASK                                                          0x00000010
#define RED_LUT1_LOOKUP_ERR_OFFSET                                                                 4
#define RED_LUT1_LOOKUP_ERR_WIDTH                                                                  1
#define RED_LUT2_LOOKUP_ERR_MASK                                                          0x00000020
#define RED_LUT2_LOOKUP_ERR_OFFSET                                                                 5
#define RED_LUT2_LOOKUP_ERR_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xffffffc0
// #define RESERVED_OFFSET                                                                            6
// #define RESERVED_WIDTH                                                                            26



// Register ODP_DITHER_CTRL(0x2f8)
#define ODP_DITHER_CTRL                                                                   0x000002f8 //RW POD:0x10
#define DISP_LUT_DITH_SEL_MASK                                                            0x00000003
#define DISP_LUT_DITH_SEL_OFFSET                                                                   0
#define DISP_LUT_DITH_SEL_WIDTH                                                                    2
#define DISP_LUT_DITH_MOD_MASK                                                            0x0000000c
#define DISP_LUT_DITH_MOD_OFFSET                                                                   2
#define DISP_LUT_DITH_MOD_WIDTH                                                                    2
#define DISP_LUT_DITH_OUT_SEL_MASK                                                        0x00000070
#define DISP_LUT_DITH_OUT_SEL_OFFSET                                                               4
#define DISP_LUT_DITH_OUT_SEL_WIDTH                                                                3
#define DISP_LUT_DITH_898_EN_MASK                                                         0x00000080
#define DISP_LUT_DITH_898_EN_OFFSET                                                                7
#define DISP_LUT_DITH_898_EN_WIDTH                                                                 1
#define DISP_LUT_DITH_NEW_ROM_EN_MASK                                                     0x00000100
#define DISP_LUT_DITH_NEW_ROM_EN_OFFSET                                                            8
#define DISP_LUT_DITH_NEW_ROM_EN_WIDTH                                                             1
#define DISP_LUT_DITH_HALF_COUNT_EN_MASK                                                  0x00000200
#define DISP_LUT_DITH_HALF_COUNT_EN_OFFSET                                                         9
#define DISP_LUT_DITH_HALF_COUNT_EN_WIDTH                                                          1
#define DISP_LUT_DITH_LESS_TYPE_MASK                                                      0x00000400
#define DISP_LUT_DITH_LESS_TYPE_OFFSET                                                            10
#define DISP_LUT_DITH_LESS_TYPE_WIDTH                                                              1
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register ODP_CTRL(0x2fc)
#define ODP_CTRL                                                                          0x000002fc //PA POD:0x80 Event:ODP
// #define RESERVED_MASK                                                                     0x00000001
// #define RESERVED_OFFSET                                                                            0
// #define RESERVED_WIDTH                                                                             1
#define ODP_RGB2YUV_OUT_EN_MASK                                                           0x00000002
#define ODP_RGB2YUV_OUT_EN_OFFSET                                                                  1
#define ODP_RGB2YUV_OUT_EN_WIDTH                                                                   1
#define ODP_444TO422_OUT_EN_MASK                                                          0x00000004
#define ODP_444TO422_OUT_EN_OFFSET                                                                 2
#define ODP_444TO422_OUT_EN_WIDTH                                                                  1
#define ODP_422_12BIT_OUT_EN_MASK                                                         0x00000008
#define ODP_422_12BIT_OUT_EN_OFFSET                                                                3
#define ODP_422_12BIT_OUT_EN_WIDTH                                                                 1
#define ODP_HOR_FLIP_BYPASS_MASK                                                          0x00000010
#define ODP_HOR_FLIP_BYPASS_OFFSET                                                                 4
#define ODP_HOR_FLIP_BYPASS_WIDTH                                                                  1
#define ODP_HOR_FLIP_EN_MASK                                                              0x00000020
#define ODP_HOR_FLIP_EN_OFFSET                                                                     5
#define ODP_HOR_FLIP_EN_WIDTH                                                                      1
// #define RESERVED_MASK                                                                     0x00000040
// #define RESERVED_OFFSET                                                                            6
// #define RESERVED_WIDTH                                                                             1
#define ODP_LEDBL_BYPASS_MASK                                                             0x00000080
#define ODP_LEDBL_BYPASS_OFFSET                                                                    7
#define ODP_LEDBL_BYPASS_WIDTH                                                                     1
// #define RESERVED_MASK                                                                     0xffffff00
// #define RESERVED_OFFSET                                                                            8
// #define RESERVED_WIDTH                                                                            24



// Register DISPLAY_CTRL(0x300)
#define DISPLAY_CTRL                                                                      0x00000300 //RW POD:0x800
#define DTG_RUN_EN_MASK                                                                   0x00000001
#define DTG_RUN_EN_OFFSET                                                                          0
#define DTG_RUN_EN_WIDTH                                                                           1
#define DCNTL_EN_MASK                                                                     0x00000002
#define DCNTL_EN_OFFSET                                                                            1
#define DCNTL_EN_WIDTH                                                                             1
#define DDATA_EN_MASK                                                                     0x00000004
#define DDATA_EN_OFFSET                                                                            2
#define DDATA_EN_WIDTH                                                                             1
#define DEN_INV_MASK                                                                      0x00000008
#define DEN_INV_OFFSET                                                                             3
#define DEN_INV_WIDTH                                                                              1
#define DHS_INV_MASK                                                                      0x00000010
#define DHS_INV_OFFSET                                                                             4
#define DHS_INV_WIDTH                                                                              1
#define DVS_INV_MASK                                                                      0x00000020
#define DVS_INV_OFFSET                                                                             5
#define DVS_INV_WIDTH                                                                              1
#define CSYNC_EN_MASK                                                                     0x00000040
#define CSYNC_EN_OFFSET                                                                            6
#define CSYNC_EN_WIDTH                                                                             1
#define DISP_HI_Z_EN_MASK                                                                 0x00000080
#define DISP_HI_Z_EN_OFFSET                                                                        7
#define DISP_HI_Z_EN_WIDTH                                                                         1
#define FORCE_COLOR_MASK                                                                  0x00000100
#define FORCE_COLOR_OFFSET                                                                         8
#define FORCE_COLOR_WIDTH                                                                          1
#define COLOR_WINDOW_EN_MASK                                                              0x00000200
#define COLOR_WINDOW_EN_OFFSET                                                                     9
#define COLOR_WINDOW_EN_WIDTH                                                                      1
#define INTERLACED_TIMING_MASK                                                            0x00000400
#define INTERLACED_TIMING_OFFSET                                                                  10
#define INTERLACED_TIMING_WIDTH                                                                    1
#define TTL_LVDS_SEL_MASK                                                                 0x00000800
#define TTL_LVDS_SEL_OFFSET                                                                       11
#define TTL_LVDS_SEL_WIDTH                                                                         1
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_MISC_CTRL(0x304)
#define ODP_MISC_CTRL                                                                     0x00000304 //RW POD:0x0
#define ODP_CSYNC_TYPE_MASK                                                               0x00000003
#define ODP_CSYNC_TYPE_OFFSET                                                                      0
#define ODP_CSYNC_TYPE_WIDTH                                                                       2
#define ODP_CSYNC_HALFLINE_EN_MASK                                                        0x00000004
#define ODP_CSYNC_HALFLINE_EN_OFFSET                                                               2
#define ODP_CSYNC_HALFLINE_EN_WIDTH                                                                1
#define ODP_CSYNC_POLARITY_MASK                                                           0x00000008
#define ODP_CSYNC_POLARITY_OFFSET                                                                  3
#define ODP_CSYNC_POLARITY_WIDTH                                                                   1
// #define RESERVED_MASK                                                                     0xfffffff0
// #define RESERVED_OFFSET                                                                            4
// #define RESERVED_WIDTH                                                                            28



// Register DH_TOTAL(0x308)
#define DH_TOTAL                                                                          0x00000308 //PA POD:0x0 Event:ODP
#define DH_TOTAL_MASK                                                                     0x00000fff
#define DH_TOTAL_OFFSET                                                                            0
#define DH_TOTAL_WIDTH                                                                            12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_DE_START(0x30c)
#define DH_DE_START                                                                       0x0000030c //PA POD:0x24 Event:ODP
#define DH_DE_START_MASK                                                                  0x00000fff
#define DH_DE_START_OFFSET                                                                         0
#define DH_DE_START_WIDTH                                                                         12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_DE_END(0x310)
#define DH_DE_END                                                                         0x00000310 //PA POD:0x0 Event:ODP
#define DH_DE_END_MASK                                                                    0x00000fff
#define DH_DE_END_OFFSET                                                                           0
#define DH_DE_END_WIDTH                                                                           12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_ACTIVE_START(0x314)
#define DH_ACTIVE_START                                                                   0x00000314 //PA POD:0x0 Event:ODP
#define DH_ACTIVE_START_MASK                                                              0x00000fff
#define DH_ACTIVE_START_OFFSET                                                                     0
#define DH_ACTIVE_START_WIDTH                                                                     12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_ACTIVE_WIDTH(0x318)
#define DH_ACTIVE_WIDTH                                                                   0x00000318 //PA POD:0x0 Event:ODP
#define DH_ACTIVE_WIDTH_MASK                                                              0x00000fff
#define DH_ACTIVE_WIDTH_OFFSET                                                                     0
#define DH_ACTIVE_WIDTH_WIDTH                                                                     12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_COLOR_START(0x31c)
#define DH_COLOR_START                                                                    0x0000031c //PA POD:0x0 Event:ODP
#define DH_COLOR_START_MASK                                                               0x00000fff
#define DH_COLOR_START_OFFSET                                                                      0
#define DH_COLOR_START_WIDTH                                                                      12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_COLOR_WIDTH(0x320)
#define DH_COLOR_WIDTH                                                                    0x00000320 //PA POD:0x0 Event:ODP
#define DH_COLOR_WIDTH_MASK                                                               0x00000fff
#define DH_COLOR_WIDTH_OFFSET                                                                      0
#define DH_COLOR_WIDTH_WIDTH                                                                      12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DH_ACTIVE_WIDTH_HIREZ(0x324)
#define DH_ACTIVE_WIDTH_HIREZ                                                             0x00000324 //RO POD:0x0
#define DH_ACTIVE_WIDTH_HIREZ_MASK                                                        0x00000fff
#define DH_ACTIVE_WIDTH_HIREZ_OFFSET                                                               0
#define DH_ACTIVE_WIDTH_HIREZ_WIDTH                                                               12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DV_TOTAL(0x328)
#define DV_TOTAL                                                                          0x00000328 //PA POD:0x0 Event:ODP
#define DV_TOTAL_MASK                                                                     0x000007ff
#define DV_TOTAL_OFFSET                                                                            0
#define DV_TOTAL_WIDTH                                                                            11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_DE_START(0x32c)
#define DV_DE_START                                                                       0x0000032c //PA POD:0x0 Event:ODP
#define DV_DE_START_MASK                                                                  0x000007ff
#define DV_DE_START_OFFSET                                                                         0
#define DV_DE_START_WIDTH                                                                         11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_DE_END(0x330)
#define DV_DE_END                                                                         0x00000330 //PA POD:0x0 Event:ODP
#define DV_DE_END_MASK                                                                    0x000007ff
#define DV_DE_END_OFFSET                                                                           0
#define DV_DE_END_WIDTH                                                                           11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_ACTIVE_START(0x334)
#define DV_ACTIVE_START                                                                   0x00000334 //PA POD:0x0 Event:ODP
#define DV_ACTIVE_START_MASK                                                              0x000007ff
#define DV_ACTIVE_START_OFFSET                                                                     0
#define DV_ACTIVE_START_WIDTH                                                                     11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_ACTIVE_LENGTH(0x338)
#define DV_ACTIVE_LENGTH                                                                  0x00000338 //PA POD:0x0 Event:ODP
#define DV_ACTIVE_LENGTH_MASK                                                             0x000007ff
#define DV_ACTIVE_LENGTH_OFFSET                                                                    0
#define DV_ACTIVE_LENGTH_WIDTH                                                                    11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_COLOR_START(0x33c)
#define DV_COLOR_START                                                                    0x0000033c //PA POD:0x0 Event:ODP
#define DV_COLOR_START_MASK                                                               0x000007ff
#define DV_COLOR_START_OFFSET                                                                      0
#define DV_COLOR_START_WIDTH                                                                      11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_COLOR_LENGTH(0x340)
#define DV_COLOR_LENGTH                                                                   0x00000340 //PA POD:0x0 Event:ODP
#define DV_COLOR_LENGTH_MASK                                                              0x000007ff
#define DV_COLOR_LENGTH_OFFSET                                                                     0
#define DV_COLOR_LENGTH_WIDTH                                                                     11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register D_WINDOW_COLOR(0x344)
#define D_WINDOW_COLOR                                                                    0x00000344 //PA POD:0x0 Event:ODP
#define COLOR_GRN_MASK                                                                    0x0000003f
#define COLOR_GRN_OFFSET                                                                           0
#define COLOR_GRN_WIDTH                                                                            6
#define COLOR_BLU_MASK                                                                    0x000007c0
#define COLOR_BLU_OFFSET                                                                           6
#define COLOR_BLU_WIDTH                                                                            5
#define COLOR_RED_MASK                                                                    0x0000f800
#define COLOR_RED_OFFSET                                                                          11
#define COLOR_RED_WIDTH                                                                            5
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register ODP_BLANKING_COLOR(0x348)
#define ODP_BLANKING_COLOR                                                                0x00000348 //RW POD:0x0
#define BLANK_COLOR_Y_GRN_MASK                                                            0x0000003f
#define BLANK_COLOR_Y_GRN_OFFSET                                                                   0
#define BLANK_COLOR_Y_GRN_WIDTH                                                                    6
#define BLANK_COLOR_U_BLU_MASK                                                            0x000007c0
#define BLANK_COLOR_U_BLU_OFFSET                                                                   6
#define BLANK_COLOR_U_BLU_WIDTH                                                                    5
#define BLANK_COLOR_V_RED_MASK                                                            0x0000f800
#define BLANK_COLOR_V_RED_OFFSET                                                                  11
#define BLANK_COLOR_V_RED_WIDTH                                                                    5
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register DH_HS_WIDTH(0x34c)
#define DH_HS_WIDTH                                                                       0x0000034c //PA POD:0x0 Event:ODP
#define DH_HS_WIDTH_MASK                                                                  0x0000007f
#define DH_HS_WIDTH_OFFSET                                                                         0
#define DH_HS_WIDTH_WIDTH                                                                          7
#define DH_HS_WIDTH_LSB_MASK                                                              0x00000180
#define DH_HS_WIDTH_LSB_OFFSET                                                                     7
#define DH_HS_WIDTH_LSB_WIDTH                                                                      2
// #define RESERVED_MASK                                                                     0xfffffe00
// #define RESERVED_OFFSET                                                                            9
// #define RESERVED_WIDTH                                                                            23



// Register DISPLAY_VSYNC(0x350)
#define DISPLAY_VSYNC                                                                     0x00000350 //PA POD:0x0 Event:ODP
#define DH_VCTEN_MASK                                                                     0x000000ff
#define DH_VCTEN_OFFSET                                                                            0
#define DH_VCTEN_WIDTH                                                                             8
#define DH_VS_END_MASK                                                                    0x0000ff00
#define DH_VS_END_OFFSET                                                                           8
#define DH_VS_END_WIDTH                                                                            8
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register DV_FLAGLINE12(0x354)
#define DV_FLAGLINE12                                                                     0x00000354 //RW POD:0x0
#define DV_FLAGLINE1_MASK                                                                 0x000007ff
#define DV_FLAGLINE1_OFFSET                                                                        0
#define DV_FLAGLINE1_WIDTH                                                                        11
// #define RESERVED_MASK                                                                     0x0000f800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                             5
#define DV_FLAGLINE2_MASK                                                                 0x07ff0000
#define DV_FLAGLINE2_OFFSET                                                                       16
#define DV_FLAGLINE2_WIDTH                                                                        11
// #define RESERVED_MASK                                                                     0xf8000000
// #define RESERVED_OFFSET                                                                           27
// #define RESERVED_WIDTH                                                                             5



// Register DV_FLAGLINE34(0x358)
#define DV_FLAGLINE34                                                                     0x00000358 //RW POD:0x0
#define DV_FLAGLINE3_MASK                                                                 0x000007ff
#define DV_FLAGLINE3_OFFSET                                                                        0
#define DV_FLAGLINE3_WIDTH                                                                        11
// #define RESERVED_MASK                                                                     0x0000f800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                             5
#define DV_FLAGLINE4_MASK                                                                 0x07ff0000
#define DV_FLAGLINE4_OFFSET                                                                       16
#define DV_FLAGLINE4_WIDTH                                                                        11
// #define RESERVED_MASK                                                                     0xf8000000
// #define RESERVED_OFFSET                                                                           27
// #define RESERVED_WIDTH                                                                             5



// Register DV_POSITION(0x35c)
#define DV_POSITION                                                                       0x0000035c //RO POD:0x0
#define DV_POSITION_MASK                                                                  0x000007ff
#define DV_POSITION_OFFSET                                                                         0
#define DV_POSITION_WIDTH                                                                         11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register INTERLACE_CTRL(0x360)
#define INTERLACE_CTRL                                                                    0x00000360 //RW POD:0x0
#define INTERLACE_DATA_EN_MASK                                                            0x00000001
#define INTERLACE_DATA_EN_OFFSET                                                                   0
#define INTERLACE_DATA_EN_WIDTH                                                                    1
#define INTERLACE_ODD_SEL_MASK                                                            0x00000002
#define INTERLACE_ODD_SEL_OFFSET                                                                   1
#define INTERLACE_ODD_SEL_WIDTH                                                                    1
#define INTERLACE_ODD_INV_IN_MASK                                                         0x00000004
#define INTERLACE_ODD_INV_IN_OFFSET                                                                2
#define INTERLACE_ODD_INV_IN_WIDTH                                                                 1
// #define RESERVED_MASK                                                                     0xfffffff8
// #define RESERVED_OFFSET                                                                            3
// #define RESERVED_WIDTH                                                                            29



// Register SYNC_CONTROL(0x364)
#define SYNC_CONTROL                                                                      0x00000364 //PA POD:0x0 Event:ODP
#define FRAME_LOCK_MODE_MASK                                                              0x00000007
#define FRAME_LOCK_MODE_OFFSET                                                                     0
#define FRAME_LOCK_MODE_WIDTH                                                                      3
#define LOCK_SOURCE_MASK                                                                  0x00000008
#define LOCK_SOURCE_OFFSET                                                                         3
#define LOCK_SOURCE_WIDTH                                                                          1
#define FRAME_MODULUS_MASK                                                                0x000001f0
#define FRAME_MODULUS_OFFSET                                                                       4
#define FRAME_MODULUS_WIDTH                                                                        5
#define INTERLACE_EN_MASK                                                                 0x00000200
#define INTERLACE_EN_OFFSET                                                                        9
#define INTERLACE_EN_WIDTH                                                                         1
#define ODD_EN_MASK                                                                       0x00000400
#define ODD_EN_OFFSET                                                                             10
#define ODD_EN_WIDTH                                                                               1
#define INVERSE_MODULUS_MASK                                                              0x0000f800
#define INVERSE_MODULUS_OFFSET                                                                    11
#define INVERSE_MODULUS_WIDTH                                                                      5
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register DFL_CTRL(0x368)
#define DFL_CTRL                                                                          0x00000368 //PA POD:0x0 Event:ODP
#define DFL_FILT_SELECT_MASK                                                              0x00000007
#define DFL_FILT_SELECT_OFFSET                                                                     0
#define DFL_FILT_SELECT_WIDTH                                                                      3
#define DFL_HCORR_SELECT_MASK                                                             0x00000008
#define DFL_HCORR_SELECT_OFFSET                                                                    3
#define DFL_HCORR_SELECT_WIDTH                                                                     1
#define DFL_HALF_CORR_MASK                                                                0x00000010
#define DFL_HALF_CORR_OFFSET                                                                       4
#define DFL_HALF_CORR_WIDTH                                                                        1
#define DFL_CORR_RST_MODE_MASK                                                            0x00000060
#define DFL_CORR_RST_MODE_OFFSET                                                                   5
#define DFL_CORR_RST_MODE_WIDTH                                                                    2
#define DFL_ERR_RD_BK_TYPE_MASK                                                           0x00000080
#define DFL_ERR_RD_BK_TYPE_OFFSET                                                                  7
#define DFL_ERR_RD_BK_TYPE_WIDTH                                                                   1
#define DFL_HODP_USE_LAST_CORR_MASK                                                       0x00000100
#define DFL_HODP_USE_LAST_CORR_OFFSET                                                              8
#define DFL_HODP_USE_LAST_CORR_WIDTH                                                               1
#define DFL_MODE_MASK                                                                     0x00003e00
#define DFL_MODE_OFFSET                                                                            9
#define DFL_MODE_WIDTH                                                                             5
#define DFL_USE_CORR_REM_MASK                                                             0x00004000
#define DFL_USE_CORR_REM_OFFSET                                                                   14
#define DFL_USE_CORR_REM_WIDTH                                                                     1
#define DFL_ERR_CORR_SHIFT_MASK                                                           0x00018000
#define DFL_ERR_CORR_SHIFT_OFFSET                                                                 15
#define DFL_ERR_CORR_SHIFT_WIDTH                                                                   2
// #define RESERVED_MASK                                                                     0xfffe0000
// #define RESERVED_OFFSET                                                                           17
// #define RESERVED_WIDTH                                                                            15



// Register DFL_CORR_TOL(0x36c)
#define DFL_CORR_TOL                                                                      0x0000036c //PA POD:0x0 Event:ODP
#define DFL_H_CORR_TOL_MASK                                                               0x000007ff
#define DFL_H_CORR_TOL_OFFSET                                                                      0
#define DFL_H_CORR_TOL_WIDTH                                                                      11
#define DFL_V_CORR_TOL_MASK                                                               0x0000f800
#define DFL_V_CORR_TOL_OFFSET                                                                     11
#define DFL_V_CORR_TOL_WIDTH                                                                       5
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register DFL_MAX_ERR_CORR(0x370)
#define DFL_MAX_ERR_CORR                                                                  0x00000370 //PA POD:0x0 Event:ODP
#define DFL_MAX_ERR_CORR_MASK                                                             0x0007ffff
#define DFL_MAX_ERR_CORR_OFFSET                                                                    0
#define DFL_MAX_ERR_CORR_WIDTH                                                                    19
// #define RESERVED_MASK                                                                     0xfff80000
// #define RESERVED_OFFSET                                                                           19
// #define RESERVED_WIDTH                                                                            13



// Register DFL_MAX_DELTA_ERR_CORR(0x374)
#define DFL_MAX_DELTA_ERR_CORR                                                            0x00000374 //PA POD:0x0 Event:ODP
#define DFL_MAX_DELTA_ERR_CORR_MASK                                                       0x0000001f
#define DFL_MAX_DELTA_ERR_CORR_OFFSET                                                              0
#define DFL_MAX_DELTA_ERR_CORR_WIDTH                                                               5
// #define RESERVED_MASK                                                                     0xffffffe0
// #define RESERVED_OFFSET                                                                            5
// #define RESERVED_WIDTH                                                                            27



// Register DFL_PERIOD_COUNT(0x378)
#define DFL_PERIOD_COUNT                                                                  0x00000378 //PA POD:0x0 Event:ODP
#define DFL_PERIOD_COUNT_MASK                                                             0x3fffffff
#define DFL_PERIOD_COUNT_OFFSET                                                                    0
#define DFL_PERIOD_COUNT_WIDTH                                                                    30
// #define RESERVED_MASK                                                                     0xc0000000
// #define RESERVED_OFFSET                                                                           30
// #define RESERVED_WIDTH                                                                             2



// Register DFL_PERIOD_ERROR(0x37c)
#define DFL_PERIOD_ERROR                                                                  0x0000037c //RO POD:0x0
#define DFL_PERIOD_ERROR_MASK                                                             0x3fffffff
#define DFL_PERIOD_ERROR_OFFSET                                                                    0
#define DFL_PERIOD_ERROR_WIDTH                                                                    30
// #define RESERVED_MASK                                                                     0xc0000000
// #define RESERVED_OFFSET                                                                           30
// #define RESERVED_WIDTH                                                                             2



// Register LOCK_STATUS(0x380)
#define LOCK_STATUS                                                                       0x00000380 //RO POD:0x0
#define DFL_PERIOD_LOCK_MASK                                                              0x00000001
#define DFL_PERIOD_LOCK_OFFSET                                                                     0
#define DFL_PERIOD_LOCK_WIDTH                                                                      1
#define DFL_PHASE_LOCK_MASK                                                               0x00000002
#define DFL_PHASE_LOCK_OFFSET                                                                      1
#define DFL_PHASE_LOCK_WIDTH                                                                       1
#define FFL_DISP_ADJ_SOFT_ERR_MASK                                                        0x000007fc
#define FFL_DISP_ADJ_SOFT_ERR_OFFSET                                                               2
#define FFL_DISP_ADJ_SOFT_ERR_WIDTH                                                                9
#define FFL_HARD_SYNC_MASK                                                                0x00000800
#define FFL_HARD_SYNC_OFFSET                                                                      11
#define FFL_HARD_SYNC_WIDTH                                                                        1
#define ODP_DISP_UF_MASK                                                                  0x00001000
#define ODP_DISP_UF_OFFSET                                                                        12
#define ODP_DISP_UF_WIDTH                                                                          1
// #define RESERVED_MASK                                                                     0xffffe000
// #define RESERVED_OFFSET                                                                           13
// #define RESERVED_WIDTH                                                                            19



// Register DISPLAY_ERROR(0x384)
#define DISPLAY_ERROR                                                                     0x00000384 //RW POD:0x8
#define DISP_ERR_MASK                                                                     0x00000003
#define DISP_ERR_OFFSET                                                                            0
#define DISP_ERR_WIDTH                                                                             2
// #define RESERVED_MASK                                                                     0x00000004
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                             1
#define ERROR_STATUS_EN_MASK                                                              0x00000008
#define ERROR_STATUS_EN_OFFSET                                                                     3
#define ERROR_STATUS_EN_WIDTH                                                                      1
// #define RESERVED_MASK                                                                     0xfffffff0
// #define RESERVED_OFFSET                                                                            4
// #define RESERVED_WIDTH                                                                            28



// Register DH_LOCK_LOAD(0x388)
#define DH_LOCK_LOAD                                                                      0x00000388 //PA POD:0x0 Event:ODP
#define DH_LOCK_LOAD_MASK                                                                 0x00000fff
#define DH_LOCK_LOAD_OFFSET                                                                        0
#define DH_LOCK_LOAD_WIDTH                                                                        12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DV_LOCK_LOAD(0x38c)
#define DV_LOCK_LOAD                                                                      0x0000038c //PA POD:0x0 Event:ODP
#define DV_LOCK_LOAD_MASK                                                                 0x000007ff
#define DV_LOCK_LOAD_OFFSET                                                                        0
#define DV_LOCK_LOAD_WIDTH                                                                        11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register DV_INVERSE_MODULUS(0x390)
#define DV_INVERSE_MODULUS                                                                0x00000390 //PA POD:0x0 Event:ODP
#define DV_DELTA_VTOTAL_MASK                                                              0x000003ff
#define DV_DELTA_VTOTAL_OFFSET                                                                     0
#define DV_DELTA_VTOTAL_WIDTH                                                                     10
#define DV_DELTA_LOCK_LOAD_MASK                                                           0x000ffc00
#define DV_DELTA_LOCK_LOAD_OFFSET                                                                 10
#define DV_DELTA_LOCK_LOAD_WIDTH                                                                  10
// #define RESERVED_MASK                                                                     0xfff00000
// #define RESERVED_OFFSET                                                                           20
// #define RESERVED_WIDTH                                                                            12



// Register ACT_EARLY_ADJ_CTRL(0x394)
#define ACT_EARLY_ADJ_CTRL                                                                0x00000394 //RW POD:0x668
#define ACT_EARLY_ADJ_MASK                                                                0x0000000f
#define ACT_EARLY_ADJ_OFFSET                                                                       0
#define ACT_EARLY_ADJ_WIDTH                                                                        4
#define CH2_ACT_EARLY_ADJ_MASK                                                            0x000000f0
#define CH2_ACT_EARLY_ADJ_OFFSET                                                                   4
#define CH2_ACT_EARLY_ADJ_WIDTH                                                                    4
#define CH1_ACT_EARLY_ADJ_MASK                                                            0x00000f00
#define CH1_ACT_EARLY_ADJ_OFFSET                                                                   8
#define CH1_ACT_EARLY_ADJ_WIDTH                                                                    4
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_FRAME_RESET_OFFSET(0x398)
#define ODP_FRAME_RESET_OFFSET                                                            0x00000398 //RW POD:0xa Event:ODP
#define FRAME_RESET_OFFSET_MASK                                                           0x000003ff
#define FRAME_RESET_OFFSET_OFFSET                                                                  0
#define FRAME_RESET_OFFSET_WIDTH                                                                  10
// #define RESERVED_MASK                                                                     0xfffffc00
// #define RESERVED_OFFSET                                                                           10
// #define RESERVED_WIDTH                                                                            22



// Register AFR_DDDS_ERROR_ENABLE(0x39c)
#define AFR_DDDS_ERROR_ENABLE                                                             0x0000039c //PA POD:0x0 Event:ODP
#define AFR_DDDS_ERR_EN_MAIN_MASK                                                         0x00000001
#define AFR_DDDS_ERR_EN_MAIN_OFFSET                                                                0
#define AFR_DDDS_ERR_EN_MAIN_WIDTH                                                                 1
#define AFR_DDDS_ERR_EN_PIP_MASK                                                          0x00000002
#define AFR_DDDS_ERR_EN_PIP_OFFSET                                                                 1
#define AFR_DDDS_ERR_EN_PIP_WIDTH                                                                  1
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register ODP_PIXGRAB_CTRL(0x3a0)
#define ODP_PIXGRAB_CTRL                                                                  0x000003a0 //RW POD:0x0
#define ODP_PIXELGRAB_EN_MASK                                                             0x00000001
#define ODP_PIXELGRAB_EN_OFFSET                                                                    0
#define ODP_PIXELGRAB_EN_WIDTH                                                                     1
#define ODP_PIXGRAB_HILIGHT_EN_MASK                                                       0x00000002
#define ODP_PIXGRAB_HILIGHT_EN_OFFSET                                                              1
#define ODP_PIXGRAB_HILIGHT_EN_WIDTH                                                               1
#define ODP_PIXGRAB_HILIGHT_XOR_MASK                                                      0x00000004
#define ODP_PIXGRAB_HILIGHT_XOR_OFFSET                                                             2
#define ODP_PIXGRAB_HILIGHT_XOR_WIDTH                                                              1
// #define RESERVED_MASK                                                                     0xfffffff8
// #define RESERVED_OFFSET                                                                            3
// #define RESERVED_WIDTH                                                                            29



// Register ODP_PIXGRAB_H(0x3a4)
#define ODP_PIXGRAB_H                                                                     0x000003a4 //RW POD:0x0
#define ODP_H_PGRAB_MASK                                                                  0x00000fff
#define ODP_H_PGRAB_OFFSET                                                                         0
#define ODP_H_PGRAB_WIDTH                                                                         12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_PIXGRAB_V(0x3a8)
#define ODP_PIXGRAB_V                                                                     0x000003a8 //RW POD:0x0
#define ODP_V_PGRAB_MASK                                                                  0x000007ff
#define ODP_V_PGRAB_OFFSET                                                                         0
#define ODP_V_PGRAB_WIDTH                                                                         11
// #define RESERVED_MASK                                                                     0xfffff800
// #define RESERVED_OFFSET                                                                           11
// #define RESERVED_WIDTH                                                                            21



// Register ODP_PIXGRAB_RED(0x3ac)
#define ODP_PIXGRAB_RED                                                                   0x000003ac //RO POD:0x0
#define ODP_PIXGRAB_RED_MASK                                                              0x00000fff
#define ODP_PIXGRAB_RED_OFFSET                                                                     0
#define ODP_PIXGRAB_RED_WIDTH                                                                     12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_PIXGRAB_GRN(0x3b0)
#define ODP_PIXGRAB_GRN                                                                   0x000003b0 //RO POD:0x0
#define ODP_PIXGRAB_GRN_MASK                                                              0x00000fff
#define ODP_PIXGRAB_GRN_OFFSET                                                                     0
#define ODP_PIXGRAB_GRN_WIDTH                                                                     12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register ODP_PIXGRAB_BLU(0x3b4)
#define ODP_PIXGRAB_BLU                                                                   0x000003b4 //RO POD:0x0
#define ODP_PIXGRAB_BLU_MASK                                                              0x00000fff
#define ODP_PIXGRAB_BLU_OFFSET                                                                     0
#define ODP_PIXGRAB_BLU_WIDTH                                                                     12
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register PANEL_POWER_STATUS(0x3b8)
#define PANEL_POWER_STATUS                                                                0x000003b8 //RO POD:0x0
#define PANEL_STATUS_MASK                                                                 0x00000003
#define PANEL_STATUS_OFFSET                                                                        0
#define PANEL_STATUS_WIDTH                                                                         2
// #define RESERVED_MASK                                                                     0xfffffffc
// #define RESERVED_OFFSET                                                                            2
// #define RESERVED_WIDTH                                                                            30



// Register PANEL_PWR_UP(0x3bc)
#define PANEL_PWR_UP                                                                      0x000003bc //RW POD:0x0
#define PANEL_PWR_UP_T1_MASK                                                              0x000000ff
#define PANEL_PWR_UP_T1_OFFSET                                                                     0
#define PANEL_PWR_UP_T1_WIDTH                                                                      8
#define PANEL_PWR_UP_T2_MASK                                                              0x0000ff00
#define PANEL_PWR_UP_T2_OFFSET                                                                     8
#define PANEL_PWR_UP_T2_WIDTH                                                                      8
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register PANEL_PWR_DN(0x3c0)
#define PANEL_PWR_DN                                                                      0x000003c0 //RW POD:0x0
#define PANEL_PWR_DN_T1_MASK                                                              0x000000ff
#define PANEL_PWR_DN_T1_OFFSET                                                                     0
#define PANEL_PWR_DN_T1_WIDTH                                                                      8
#define PANEL_PWR_DN_T2_MASK                                                              0x0000ff00
#define PANEL_PWR_DN_T2_OFFSET                                                                     8
#define PANEL_PWR_DN_T2_WIDTH                                                                      8
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register PANEL_PWR_DIV(0x3c4)
#define PANEL_PWR_DIV                                                                     0x000003c4 //RW POD:0x0
#define PANEL_PWR_SEQ_TCLK_TICK_MASK                                                      0x0000000f
#define PANEL_PWR_SEQ_TCLK_TICK_OFFSET                                                             0
#define PANEL_PWR_SEQ_TCLK_TICK_WIDTH                                                              4
// #define RESERVED_MASK                                                                     0xfffffff0
// #define RESERVED_OFFSET                                                                            4
// #define RESERVED_WIDTH                                                                            28



// Register DATA_CHECK_CTRL(0x3c8)
#define DATA_CHECK_CTRL                                                                   0x000003c8 //RW POD:0x0
#define DATA_CHECK_EN_MASK                                                                0x00000001
#define DATA_CHECK_EN_OFFSET                                                                       0
#define DATA_CHECK_EN_WIDTH                                                                        1
#define DATA_CHECK_COLOR_SELECT_MASK                                                      0x00000006
#define DATA_CHECK_COLOR_SELECT_OFFSET                                                             1
#define DATA_CHECK_COLOR_SELECT_WIDTH                                                              2
#define DATA_CHECK_SEL_MASK                                                               0x00000008
#define DATA_CHECK_SEL_OFFSET                                                                      3
#define DATA_CHECK_SEL_WIDTH                                                                       1
// #define RESERVED_MASK                                                                     0xfffffff0
// #define RESERVED_OFFSET                                                                            4
// #define RESERVED_WIDTH                                                                            28



// Register DATA_CHECK_SCORE(0x3cc)
#define DATA_CHECK_SCORE                                                                  0x000003cc //RO POD:0x0
#define DATA_CHECK_SCORE_MASK                                                             0x000000ff
#define DATA_CHECK_SCORE_OFFSET                                                                    0
#define DATA_CHECK_SCORE_WIDTH                                                                     8
// #define RESERVED_MASK                                                                     0xffffff00
// #define RESERVED_OFFSET                                                                            8
// #define RESERVED_WIDTH                                                                            24



// Register DATA_CHECK_RESULT(0x3d0)
#define DATA_CHECK_RESULT                                                                 0x000003d0 //RO POD:0x0
#define DATA_CHECK_RESULT_MASK                                                            0xffffffff
#define DATA_CHECK_RESULT_OFFSET                                                                   0
#define DATA_CHECK_RESULT_WIDTH                                                                   32



// Register READ_ONLY_LOCKOUT(0x3d4)
#define READ_ONLY_LOCKOUT                                                                 0x000003d4 //RW POD:0x0
#define READ_ONLY_LOCKOUT_MASK                                                            0x00000001
#define READ_ONLY_LOCKOUT_OFFSET                                                                   0
#define READ_ONLY_LOCKOUT_WIDTH                                                                    1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_TESTBUS_SELCTRL(0x3d8)
#define ODP_TESTBUS_SELCTRL                                                               0x000003d8 //RW POD:0x0
#define ODP_TESTBUS_SEL_MASK                                                              0x0000000f
#define ODP_TESTBUS_SEL_OFFSET                                                                     0
#define ODP_TESTBUS_SEL_WIDTH                                                                      4
// #define RESERVED_MASK                                                                     0x000000f0
// #define RESERVED_OFFSET                                                                            4
// #define RESERVED_WIDTH                                                                             4
#define ODP_DISPENG_TESTBUSA_EN_MASK                                                      0x0001ff00
#define ODP_DISPENG_TESTBUSA_EN_OFFSET                                                             8
#define ODP_DISPENG_TESTBUSA_EN_WIDTH                                                              9
#define ODP_DISPENG_TESTBUSB_EN_MASK                                                      0x03fe0000
#define ODP_DISPENG_TESTBUSB_EN_OFFSET                                                            17
#define ODP_DISPENG_TESTBUSB_EN_WIDTH                                                              9
// #define RESERVED_MASK                                                                     0xfc000000
// #define RESERVED_OFFSET                                                                           26
// #define RESERVED_WIDTH                                                                             6



// Register ODP_TESTBUS_OUTPUT(0x3dc)
#define ODP_TESTBUS_OUTPUT                                                                0x000003dc //RO POD:0x0
#define ODP_TESTBUS_OUT_MASK                                                              0x0000ffff
#define ODP_TESTBUS_OUT_OFFSET                                                                     0
#define ODP_TESTBUS_OUT_WIDTH                                                                     16
// #define RESERVED_MASK                                                                     0xffff0000
// #define RESERVED_OFFSET                                                                           16
// #define RESERVED_WIDTH                                                                            16



// Register ODP_DISPLAY_IRQ2_STATUS(0x3e0)
#define ODP_DISPLAY_IRQ2_STATUS                                                           0x000003e0 //CRO POD:0x0
#define Q2D_LINEFLAG2_MASK                                                                0x00000001
#define Q2D_LINEFLAG2_OFFSET                                                                       0
#define Q2D_LINEFLAG2_WIDTH                                                                        1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_DISPLAY_IRQ2_MASK(0x3e4)
#define ODP_DISPLAY_IRQ2_MASK                                                             0x000003e4 //RW POD:0x0
#define Q2D_LINEFLAG2_MASK_MASK                                                           0x00000001
#define Q2D_LINEFLAG2_MASK_OFFSET                                                                  0
#define Q2D_LINEFLAG2_MASK_WIDTH                                                                   1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_DISPLAY_IRQ2_MODE(0x3e8)
#define ODP_DISPLAY_IRQ2_MODE                                                             0x000003e8 //RW POD:0x1
#define Q2D_LINEFLAG2_MODE_MASK                                                           0x00000001
#define Q2D_LINEFLAG2_MODE_OFFSET                                                                  0
#define Q2D_LINEFLAG2_MODE_WIDTH                                                                   1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31



// Register ODP_TX_STALL_SET(0x3ec)
#define ODP_TX_STALL_SET                                                                  0x000003ec //RW POD:0x1
#define ODP_CPF_ENABLE_MASK                                                               0x00000001
#define ODP_CPF_ENABLE_OFFSET                                                                      0
#define ODP_CPF_ENABLE_WIDTH                                                                       1
#define ODP_TX_STALL_SET_MASK                                                             0x00000006
#define ODP_TX_STALL_SET_OFFSET                                                                    1
#define ODP_TX_STALL_SET_WIDTH                                                                     2
// #define RESERVED_MASK                                                                     0xfffffff8
// #define RESERVED_OFFSET                                                                            3
// #define RESERVED_WIDTH                                                                            29



// Register PRIM_VID_RX_DATA_SET(0x3f0)
#define PRIM_VID_RX_DATA_SET                                                              0x000003f0 //RW POD:0x222
#define MAIN_VID1_RX_DATA_SET_MASK                                                        0x0000000f
#define MAIN_VID1_RX_DATA_SET_OFFSET                                                               0
#define MAIN_VID1_RX_DATA_SET_WIDTH                                                                4
#define PIP_VID2_RX_DATA_SET_MASK                                                         0x000000f0
#define PIP_VID2_RX_DATA_SET_OFFSET                                                                4
#define PIP_VID2_RX_DATA_SET_WIDTH                                                                 4
#define ALPHA_RX_DATA_SET_MASK                                                            0x00000f00
#define ALPHA_RX_DATA_SET_OFFSET                                                                   8
#define ALPHA_RX_DATA_SET_WIDTH                                                                    4
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register DISPLAY_SPARE1(0x3f4)
#define DISPLAY_SPARE1                                                                    0x000003f4 //RW POD:0x0
#define DISPLAY_SPARE1_MASK                                                               0xffffffff
#define DISPLAY_SPARE1_OFFSET                                                                      0
#define DISPLAY_SPARE1_WIDTH                                                                      32



// Register DISPLAY_SPARE2(0x3f8)
#define DISPLAY_SPARE2                                                                    0x000003f8 //RW POD:0x0
#define DISPLAY_SPARE2_MASK                                                               0xffffffff
#define DISPLAY_SPARE2_OFFSET                                                                      0
#define DISPLAY_SPARE2_WIDTH                                                                      32



// Register DISPLAY_SPARE3(0x3fc)
#define DISPLAY_SPARE3                                                                    0x000003fc //RW POD:0x0
#define DISPLAY_SPARE3_MASK                                                               0xffffffff
#define DISPLAY_SPARE3_OFFSET                                                                      0
#define DISPLAY_SPARE3_WIDTH                                                                      32






// address block LEDBL [0x600]


# define LEDBL_BASE_ADDRESS 0x600


// Register LEDBL_ID(0x600)
#define LEDBL_ID                                                                          0x00000600 //RO POD:0x7200
#define LEDBL_ID_MASK                                                                     0x00000fff
#define LEDBL_ID_OFFSET                                                                            0
#define LEDBL_ID_WIDTH                                                                            12
#define BOND_LED_EN_MASK                                                                  0x00001000
#define BOND_LED_EN_OFFSET                                                                        12
#define BOND_LED_EN_WIDTH                                                                          1
#define BOND_RGB_LED_EN_MASK                                                              0x00002000
#define BOND_RGB_LED_EN_OFFSET                                                                    13
#define BOND_RGB_LED_EN_WIDTH                                                                      1
#define BOND_REGIONAL_LED_EN_MASK                                                         0x00004000
#define BOND_REGIONAL_LED_EN_OFFSET                                                               14
#define BOND_REGIONAL_LED_EN_WIDTH                                                                 1
// #define Reserved_MASK                                                                     0xffff8000
// #define Reserved_OFFSET                                                                           15
// #define Reserved_WIDTH                                                                            17



// Register BL_CONTROL_0(0x624)
#define BL_CONTROL_0                                                                      0x00000624 //RW POD:0x0
#define BL_CTL_ENABLE_MASK                                                                0x00000001
#define BL_CTL_ENABLE_OFFSET                                                                       0
#define BL_CTL_ENABLE_WIDTH                                                                        1
#define BL_CTL_AUTO_MEAS_MASK                                                             0x00000002
#define BL_CTL_AUTO_MEAS_OFFSET                                                                    1
#define BL_CTL_AUTO_MEAS_WIDTH                                                                     1
// #define Reserved_MASK                                                                     0x00000004
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             1
#define BL_CTL_2CHAN_MASK                                                                 0x00000008
#define BL_CTL_2CHAN_OFFSET                                                                        3
#define BL_CTL_2CHAN_WIDTH                                                                         1
#define BL_CTL_WHITE_BL_MASK                                                              0x00000010
#define BL_CTL_WHITE_BL_OFFSET                                                                     4
#define BL_CTL_WHITE_BL_WIDTH                                                                      1
#define BL_CTL_LRCHAN_MASK                                                                0x00000020
#define BL_CTL_LRCHAN_OFFSET                                                                       5
#define BL_CTL_LRCHAN_WIDTH                                                                        1
// #define Reserved_MASK                                                                     0xffffffc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                            26



// Register BL_DBG_GRAB_CTRL(0x628)
#define BL_DBG_GRAB_CTRL                                                                  0x00000628 //RW POD:0x0
#define BL_DBG_GRAB_SEL_MASK                                                              0x0000003f
#define BL_DBG_GRAB_SEL_OFFSET                                                                     0
#define BL_DBG_GRAB_SEL_WIDTH                                                                      6
#define BL_DBG_GRAB_MARK_MASK                                                             0x00000040
#define BL_DBG_GRAB_MARK_OFFSET                                                                    6
#define BL_DBG_GRAB_MARK_WIDTH                                                                     1
#define BL_DEBUGRID_MASK                                                                  0x00000080
#define BL_DEBUGRID_OFFSET                                                                         7
#define BL_DEBUGRID_WIDTH                                                                          1
#define BL_DBG_SWITCH_TEST_MASK                                                           0x00000100
#define BL_DBG_SWITCH_TEST_OFFSET                                                                  8
#define BL_DBG_SWITCH_TEST_WIDTH                                                                   1
#define BL_DGB_GRIDBLOB_MASK                                                              0x00000200
#define BL_DGB_GRIDBLOB_OFFSET                                                                     9
#define BL_DGB_GRIDBLOB_WIDTH                                                                      1
#define BL_DBG_BLKGRID_MASK                                                               0x00000400
#define BL_DBG_BLKGRID_OFFSET                                                                     10
#define BL_DBG_BLKGRID_WIDTH                                                                       1
#define BL_DBG_FORCE_GRAY_MASK                                                            0x00000800
#define BL_DBG_FORCE_GRAY_OFFSET                                                                  11
#define BL_DBG_FORCE_GRAY_WIDTH                                                                    1
#define BL_DBG_GRAB_CH_MASK                                                               0x00003000
#define BL_DBG_GRAB_CH_OFFSET                                                                     12
#define BL_DBG_GRAB_CH_WIDTH                                                                       2
#define BL_DBG_1GRID_MASK                                                                 0x00004000
#define BL_DBG_1GRID_OFFSET                                                                       14
#define BL_DBG_1GRID_WIDTH                                                                         1
// #define Reserved_MASK                                                                     0xffff8000
// #define Reserved_OFFSET                                                                           15
// #define Reserved_WIDTH                                                                            17



// Register BL_DBG_GRABXPOS(0x62c)
#define BL_DBG_GRABXPOS                                                                   0x0000062c //RW POD:0x0
#define BL_DBG_GRAB_XPOS_MASK                                                             0x00000fff
#define BL_DBG_GRAB_XPOS_OFFSET                                                                    0
#define BL_DBG_GRAB_XPOS_WIDTH                                                                    12
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_DBG_GRABYPOS(0x630)
#define BL_DBG_GRABYPOS                                                                   0x00000630 //RW POD:0x0
#define BL_DBG_GRAB_YPOS_MASK                                                             0x000007ff
#define BL_DBG_GRAB_YPOS_OFFSET                                                                    0
#define BL_DBG_GRAB_YPOS_WIDTH                                                                    11
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_DBG_GRABDATA(0x634)
#define BL_DBG_GRABDATA                                                                   0x00000634 //RO POD:0x0
#define BL_DBG_GRAB_DATA_MASK                                                             0x0000ffff
#define BL_DBG_GRAB_DATA_OFFSET                                                                    0
#define BL_DBG_GRAB_DATA_WIDTH                                                                    16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_ZETM_CTRL_0(0x658)
#define BL_ZETM_CTRL_0                                                                    0x00000658 //RW POD:0x0
#define BL_ZETM_FULLSTART_MASK                                                            0x000000ff
#define BL_ZETM_FULLSTART_OFFSET                                                                   0
#define BL_ZETM_FULLSTART_WIDTH                                                                    8
#define BL_ZETM_FULLEND_MASK                                                              0x0000ff00
#define BL_ZETM_FULLEND_OFFSET                                                                     8
#define BL_ZETM_FULLEND_WIDTH                                                                      8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_ZETM_CTRL_1(0x65c)
#define BL_ZETM_CTRL_1                                                                    0x0000065c //RW POD:0x0
#define BL_ZETM_HALFSTART_MASK                                                            0x000000ff
#define BL_ZETM_HALFSTART_OFFSET                                                                   0
#define BL_ZETM_HALFSTART_WIDTH                                                                    8
#define BL_ZETM_HALFEND_MASK                                                              0x0000ff00
#define BL_ZETM_HALFEND_OFFSET                                                                     8
#define BL_ZETM_HALFEND_WIDTH                                                                      8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_ZETM_CTRL_2(0x660)
#define BL_ZETM_CTRL_2                                                                    0x00000660 //RW POD:0x0
#define BL_ZETM_reserved_MASK                                                             0x000001ff
#define BL_ZETM_reserved_OFFSET                                                                    0
#define BL_ZETM_reserved_WIDTH                                                                     9
#define BL_ZETM_FORCE_ENABLES_MASK                                                        0x00000200
#define BL_ZETM_FORCE_ENABLES_OFFSET                                                               9
#define BL_ZETM_FORCE_ENABLES_WIDTH                                                                1
// #define Reserved_MASK                                                                     0xfffffc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                            22



// Register BL_MWIN_HSTRT(0x68c)
#define BL_MWIN_HSTRT                                                                     0x0000068c //RW POD:0x0
#define BL_MWIN_HSTART_MASK                                                               0x00000fff
#define BL_MWIN_HSTART_OFFSET                                                                      0
#define BL_MWIN_HSTART_WIDTH                                                                      12
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_MWIN_WDTH(0x690)
#define BL_MWIN_WDTH                                                                      0x00000690 //RW POD:0x0
#define BL_MWIN_WIDTH_MASK                                                                0x00000fff
#define BL_MWIN_WIDTH_OFFSET                                                                       0
#define BL_MWIN_WIDTH_WIDTH                                                                       12
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_MWIN_XZONE_SZ(0x694)
#define BL_MWIN_XZONE_SZ                                                                  0x00000694 //RW POD:0x0
#define BL_MWIN_XZONESIZE_MASK                                                            0x00000fff
#define BL_MWIN_XZONESIZE_OFFSET                                                                   0
#define BL_MWIN_XZONESIZE_WIDTH                                                                   12
#define BL_MWIN_NUMXZONES_MASK                                                            0x0003f000
#define BL_MWIN_NUMXZONES_OFFSET                                                                  12
#define BL_MWIN_NUMXZONES_WIDTH                                                                    6
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_MWIN_VSTRT(0x698)
#define BL_MWIN_VSTRT                                                                     0x00000698 //RW POD:0x0
#define BL_MWIN_VSTART_MASK                                                               0x000007ff
#define BL_MWIN_VSTART_OFFSET                                                                      0
#define BL_MWIN_VSTART_WIDTH                                                                      11
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_MWIN_LNGTH(0x69c)
#define BL_MWIN_LNGTH                                                                     0x0000069c //RW POD:0x0
#define BL_MWIN_LENGTH_MASK                                                               0x000007ff
#define BL_MWIN_LENGTH_OFFSET                                                                      0
#define BL_MWIN_LENGTH_WIDTH                                                                      11
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_MWIN_YZONE_SZ(0x6a0)
#define BL_MWIN_YZONE_SZ                                                                  0x000006a0 //RW POD:0x0
#define BL_MWIN_YZONESIZE_MASK                                                            0x000007ff
#define BL_MWIN_YZONESIZE_OFFSET                                                                   0
#define BL_MWIN_YZONESIZE_WIDTH                                                                   11
#define BL_MWIN_NUMYZONES_MASK                                                            0x0000f800
#define BL_MWIN_NUMYZONES_OFFSET                                                                  11
#define BL_MWIN_NUMYZONES_WIDTH                                                                    5
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_BRT_MANR(0x6a4)
#define BL_BRT_MANR                                                                       0x000006a4 //RW POD:0x0
#define BL_BRT_MANBRITER_MASK                                                             0x0000ffff
#define BL_BRT_MANBRITER_OFFSET                                                                    0
#define BL_BRT_MANBRITER_WIDTH                                                                    16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_BRT_MANG(0x6a8)
#define BL_BRT_MANG                                                                       0x000006a8 //RW POD:0x0
#define BL_BRT_MANBRITEG_MASK                                                             0x0000ffff
#define BL_BRT_MANBRITEG_OFFSET                                                                    0
#define BL_BRT_MANBRITEG_WIDTH                                                                    16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_BRT_MANB(0x6ac)
#define BL_BRT_MANB                                                                       0x000006ac //RW POD:0x0
#define BL_BRT_MANBRITEB_MASK                                                             0x0000ffff
#define BL_BRT_MANBRITEB_OFFSET                                                                    0
#define BL_BRT_MANBRITEB_WIDTH                                                                    16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_BRT_MIN(0x6b0)
#define BL_BRT_MIN                                                                        0x000006b0 //RW POD:0x0
#define BL_BRT_MINBRITE_MASK                                                              0x000000ff
#define BL_BRT_MINBRITE_OFFSET                                                                     0
#define BL_BRT_MINBRITE_WIDTH                                                                      8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_BRT_CTRL(0x6b4)
#define BL_BRT_CTRL                                                                       0x000006b4 //RW POD:0x0
#define BL_BRT_MANSHIFT_MASK                                                              0x0000001f
#define BL_BRT_MANSHIFT_OFFSET                                                                     0
#define BL_BRT_MANSHIFT_WIDTH                                                                      5
#define BL_BRT_AUTO_BRITE_MASK                                                            0x00000020
#define BL_BRT_AUTO_BRITE_OFFSET                                                                   5
#define BL_BRT_AUTO_BRITE_WIDTH                                                                    1
// #define Reserved_MASK                                                                     0xffffffc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                            26



// Register BL_BRT_FILT_CTRL(0x6b8)
#define BL_BRT_FILT_CTRL                                                                  0x000006b8 //RW POD:0x0
#define BL_BRT_FILTBLEND_MASK                                                             0x00000007
#define BL_BRT_FILTBLEND_OFFSET                                                                    0
#define BL_BRT_FILTBLEND_WIDTH                                                                     3
#define BL_BRT_EBL_FILT_MASK                                                              0x00000008
#define BL_BRT_EBL_FILT_OFFSET                                                                     3
#define BL_BRT_EBL_FILT_WIDTH                                                                      1
#define BL_BRT_HOZFILTSHFT_MASK                                                           0x000000f0
#define BL_BRT_HOZFILTSHFT_OFFSET                                                                  4
#define BL_BRT_HOZFILTSHFT_WIDTH                                                                   4
#define BL_BRT_VERTFILTSHFT_MASK                                                          0x00000f00
#define BL_BRT_VERTFILTSHFT_OFFSET                                                                 8
#define BL_BRT_VERTFILTSHFT_WIDTH                                                                  4
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_BRT_SCALE(0x6bc)
#define BL_BRT_SCALE                                                                      0x000006bc //RW POD:0x0
#define BL_BRT_RED_SCALE_MASK                                                             0x000000ff
#define BL_BRT_RED_SCALE_OFFSET                                                                    0
#define BL_BRT_RED_SCALE_WIDTH                                                                     8
#define BL_BRT_GRN_SCALE_MASK                                                             0x0000ff00
#define BL_BRT_GRN_SCALE_OFFSET                                                                    8
#define BL_BRT_GRN_SCALE_WIDTH                                                                     8
#define BL_BRT_BLU_SCALE_MASK                                                             0x00ff0000
#define BL_BRT_BLU_SCALE_OFFSET                                                                   16
#define BL_BRT_BLU_SCALE_WIDTH                                                                     8
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_BRT_MASKBR(0x6c0)
#define BL_BRT_MASKBR                                                                     0x000006c0 //RW POD:0x0
#define BL_BRT_MASKBRITE_MASK                                                             0x000000ff
#define BL_BRT_MASKBRITE_OFFSET                                                                    0
#define BL_BRT_MASKBRITE_WIDTH                                                                     8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_ZCAL_CTRL_0(0x6cc)
#define BL_ZCAL_CTRL_0                                                                    0x000006cc //RW POD:0x0
#define BL_ZC_CRS_CAL_EBL_MASK                                                            0x00000001
#define BL_ZC_CRS_CAL_EBL_OFFSET                                                                   0
#define BL_ZC_CRS_CAL_EBL_WIDTH                                                                    1
#define ZCAL_CTRL_reserved_MASK                                                           0x00000006
#define ZCAL_CTRL_reserved_OFFSET                                                                  1
#define ZCAL_CTRL_reserved_WIDTH                                                                   2
#define BL_ZC_FIN_CAL_EBL_MASK                                                            0x00000008
#define BL_ZC_FIN_CAL_EBL_OFFSET                                                                   3
#define BL_ZC_FIN_CAL_EBL_WIDTH                                                                    1
// #define Reserved_MASK                                                                     0xfffffff0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                            28



// Register BL_ZCAL_STATUS_0(0x6d0)
#define BL_ZCAL_STATUS_0                                                                  0x000006d0 //CRO POD:0x0
#define BL_ZC_CalcGridOverun_MASK                                                         0x00000001
#define BL_ZC_CalcGridOverun_OFFSET                                                                0
#define BL_ZC_CalcGridOverun_WIDTH                                                                 1
// #define Reserved_MASK                                                                     0xfffffffe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                            31



// Register BL_PWIN_SIZE(0x6d4)
#define BL_PWIN_SIZE                                                                      0x000006d4 //RW POD:0x0
#define BL_PWIN_HSTART_MASK                                                               0x00000fff
#define BL_PWIN_HSTART_OFFSET                                                                      0
#define BL_PWIN_HSTART_WIDTH                                                                      12
#define BL_PWIN_WIDTH_MASK                                                                0x00fff000
#define BL_PWIN_WIDTH_OFFSET                                                                      12
#define BL_PWIN_WIDTH_WIDTH                                                                       12
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_PWIN_RSTART(0x6d8)
#define BL_PWIN_RSTART                                                                    0x000006d8 //RW POD:0x0
#define BL_PWIN_RSTART_XGRID_MASK                                                         0x000001ff
#define BL_PWIN_RSTART_XGRID_OFFSET                                                                0
#define BL_PWIN_RSTART_XGRID_WIDTH                                                                 9
#define BL_PWIN_Reserved_MASK                                                             0x0000fe00
#define BL_PWIN_Reserved_OFFSET                                                                    9
#define BL_PWIN_Reserved_WIDTH                                                                     7
#define BL_PWIN_RSTART_XPOSINGRID_MASK                                                    0xffff0000
#define BL_PWIN_RSTART_XPOSINGRID_OFFSET                                                          16
#define BL_PWIN_RSTART_XPOSINGRID_WIDTH                                                           16



// Register BL_PWIN_XCONF(0x6dc)
#define BL_PWIN_XCONF                                                                     0x000006dc //RW POD:0x0
#define BL_PWIN_NUMXZONES_MASK                                                            0x0000003f
#define BL_PWIN_NUMXZONES_OFFSET                                                                   0
#define BL_PWIN_NUMXZONES_WIDTH                                                                    6
// #define Reserved_MASK                                                                     0x000000c0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2
#define BL_PWIN_NUMXGRIDS_MASK                                                            0x0001ff00
#define BL_PWIN_NUMXGRIDS_OFFSET                                                                   8
#define BL_PWIN_NUMXGRIDS_WIDTH                                                                    9
// #define Reserved_MASK                                                                     0xfffe0000
// #define Reserved_OFFSET                                                                           17
// #define Reserved_WIDTH                                                                            15



// Register BL_PWIN_XGRID_SZ(0x6e0)
#define BL_PWIN_XGRID_SZ                                                                  0x000006e0 //RW POD:0x0
#define BL_PWIN_XGRID_SIZE_MASK                                                           0x00007fff
#define BL_PWIN_XGRID_SIZE_OFFSET                                                                  0
#define BL_PWIN_XGRID_SIZE_WIDTH                                                                  15
// #define Reserved_MASK                                                                     0xffff8000
// #define Reserved_OFFSET                                                                           15
// #define Reserved_WIDTH                                                                            17



// Register BL_PWIN_XGRID_IIF(0x6e4)
#define BL_PWIN_XGRID_IIF                                                                 0x000006e4 //RW POD:0x0
#define BL_PWIN_XGRID_IINCF_MASK                                                          0x0003ffff
#define BL_PWIN_XGRID_IINCF_OFFSET                                                                 0
#define BL_PWIN_XGRID_IINCF_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_XGRID_IIS(0x6e8)
#define BL_PWIN_XGRID_IIS                                                                 0x000006e8 //RW POD:0x0
#define BL_PWIN_XGRID_IINCS_MASK                                                          0x0003ffff
#define BL_PWIN_XGRID_IINCS_OFFSET                                                                 0
#define BL_PWIN_XGRID_IINCS_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_XGRID_IIL(0x6ec)
#define BL_PWIN_XGRID_IIL                                                                 0x000006ec //RW POD:0x0
#define BL_PWIN_XGRID_IINCL_MASK                                                          0x0003ffff
#define BL_PWIN_XGRID_IINCL_OFFSET                                                                 0
#define BL_PWIN_XGRID_IINCL_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_XGRID_PZ(0x6f0)
#define BL_PWIN_XGRID_PZ                                                                  0x000006f0 //RW POD:0x0
#define BL_PWIN_XGRID_PZONE_MASK                                                          0x000001ff
#define BL_PWIN_XGRID_PZONE_OFFSET                                                                 0
#define BL_PWIN_XGRID_PZONE_WIDTH                                                                  9
// #define Reserved_MASK                                                                     0xfffffe00
// #define Reserved_OFFSET                                                                            9
// #define Reserved_WIDTH                                                                            23



// Register BL_PWIN_VSTRT(0x6f4)
#define BL_PWIN_VSTRT                                                                     0x000006f4 //RW POD:0x0
#define BL_PWIN_VSTART_MASK                                                               0x000007ff
#define BL_PWIN_VSTART_OFFSET                                                                      0
#define BL_PWIN_VSTART_WIDTH                                                                      11
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_PWIN_LNTH(0x6f8)
#define BL_PWIN_LNTH                                                                      0x000006f8 //RW POD:0x0
#define BL_PWIN_LENGTH_MASK                                                               0x000007ff
#define BL_PWIN_LENGTH_OFFSET                                                                      0
#define BL_PWIN_LENGTH_WIDTH                                                                      11
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_PWIN_YCONF(0x6fc)
#define BL_PWIN_YCONF                                                                     0x000006fc //RW POD:0x0
#define BL_PWIN_NUMYZONES_MASK                                                            0x0000001f
#define BL_PWIN_NUMYZONES_OFFSET                                                                   0
#define BL_PWIN_NUMYZONES_WIDTH                                                                    5
// #define Reserved_MASK                                                                     0x000000e0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3
#define BL_PWIN_NUMYGRIDS_MASK                                                            0x0000ff00
#define BL_PWIN_NUMYGRIDS_OFFSET                                                                   8
#define BL_PWIN_NUMYGRIDS_WIDTH                                                                    8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_PWIN_YGRID_SZ(0x700)
#define BL_PWIN_YGRID_SZ                                                                  0x00000700 //RW POD:0x0
#define BL_PWIN_YGRID_SIZE_MASK                                                           0x00007fff
#define BL_PWIN_YGRID_SIZE_OFFSET                                                                  0
#define BL_PWIN_YGRID_SIZE_WIDTH                                                                  15
// #define Reserved_MASK                                                                     0xffff8000
// #define Reserved_OFFSET                                                                           15
// #define Reserved_WIDTH                                                                            17



// Register BL_PWIN_YGRID_IIF(0x704)
#define BL_PWIN_YGRID_IIF                                                                 0x00000704 //RW POD:0x0
#define BL_PWIN_YGRID_IINCF_MASK                                                          0x0003ffff
#define BL_PWIN_YGRID_IINCF_OFFSET                                                                 0
#define BL_PWIN_YGRID_IINCF_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_YGRID_IIS(0x708)
#define BL_PWIN_YGRID_IIS                                                                 0x00000708 //RW POD:0x0
#define BL_PWIN_YGRID_IINCS_MASK                                                          0x0003ffff
#define BL_PWIN_YGRID_IINCS_OFFSET                                                                 0
#define BL_PWIN_YGRID_IINCS_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_YGRID_IIL(0x70c)
#define BL_PWIN_YGRID_IIL                                                                 0x0000070c //RW POD:0x0
#define BL_PWIN_YGRID_IINCL_MASK                                                          0x0003ffff
#define BL_PWIN_YGRID_IINCL_OFFSET                                                                 0
#define BL_PWIN_YGRID_IINCL_WIDTH                                                                 18
// #define Reserved_MASK                                                                     0xfffc0000
// #define Reserved_OFFSET                                                                           18
// #define Reserved_WIDTH                                                                            14



// Register BL_PWIN_YGRID_PZ(0x710)
#define BL_PWIN_YGRID_PZ                                                                  0x00000710 //RW POD:0x0
#define BL_PWIN_YGRID_PZONE_MASK                                                          0x000000ff
#define BL_PWIN_YGRID_PZONE_OFFSET                                                                 0
#define BL_PWIN_YGRID_PZONE_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_PWIN_PWM_PZ(0x714)
#define BL_PWIN_PWM_PZ                                                                    0x00000714 //RW POD:0x0
#define BL_PWIN_PWM_PZONE_MASK                                                            0x000000ff
#define BL_PWIN_PWM_PZONE_OFFSET                                                                   0
#define BL_PWIN_PWM_PZONE_WIDTH                                                                    8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_PWIN_PWM_PGD(0x71c)
#define BL_PWIN_PWM_PGD                                                                   0x0000071c //RW POD:0x0
#define BL_PWIN_PWM_PGRID_MASK                                                            0x000000ff
#define BL_PWIN_PWM_PGRID_OFFSET                                                                   0
#define BL_PWIN_PWM_PGRID_WIDTH                                                                    8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_PWIN_PWM_PPD(0x720)
#define BL_PWIN_PWM_PPD                                                                   0x00000720 //RW POD:0x0
#define BL_PWIN_PWM_PFRAME_MASK                                                           0x000000ff
#define BL_PWIN_PWM_PFRAME_OFFSET                                                                  0
#define BL_PWIN_PWM_PFRAME_WIDTH                                                                   8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_PWIN_XGRID_OFF(0x724)
#define BL_PWIN_XGRID_OFF                                                                 0x00000724 //RW POD:0x0
#define BL_PWIN_XGRID_OFFSET_MASK                                                         0x0000ffff
#define BL_PWIN_XGRID_OFFSET_OFFSET                                                                0
#define BL_PWIN_XGRID_OFFSET_WIDTH                                                                16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_PWIN_YGRID_OFF(0x728)
#define BL_PWIN_YGRID_OFF                                                                 0x00000728 //RW POD:0x0
#define BL_PWIN_YGRID_OFFSET_MASK                                                         0x0000ffff
#define BL_PWIN_YGRID_OFFSET_OFFSET                                                                0
#define BL_PWIN_YGRID_OFFSET_WIDTH                                                                16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_ZC_MINVAL(0x734)
#define BL_ZC_MINVAL                                                                      0x00000734 //RW POD:0x0
#define BL_ZC_MIN_MASK                                                                    0x00001fff
#define BL_ZC_MIN_OFFSET                                                                           0
#define BL_ZC_MIN_WIDTH                                                                           13
// #define Reserved_MASK                                                                     0xffffe000
// #define Reserved_OFFSET                                                                           13
// #define Reserved_WIDTH                                                                            19



// Register BL_ZC_OFFS(0x73c)
#define BL_ZC_OFFS                                                                        0x0000073c //RW POD:0x0
#define BL_ZC_XOFF_MASK                                                                   0x000000ff
#define BL_ZC_XOFF_OFFSET                                                                          0
#define BL_ZC_XOFF_WIDTH                                                                           8
#define BL_ZC_YOFF_MASK                                                                   0x0000ff00
#define BL_ZC_YOFF_OFFSET                                                                          8
#define BL_ZC_YOFF_WIDTH                                                                           8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_ZC_XZONETYP_0_7(0x740)
#define BL_ZC_XZONETYP_0_7                                                                0x00000740 //RW POD:0x0
#define BL_ZC_XZONETYP_0_MASK                                                             0x00000007
#define BL_ZC_XZONETYP_0_OFFSET                                                                    0
#define BL_ZC_XZONETYP_0_WIDTH                                                                     3
#define BL_ZC_XZONETYP_1_MASK                                                             0x00000038
#define BL_ZC_XZONETYP_1_OFFSET                                                                    3
#define BL_ZC_XZONETYP_1_WIDTH                                                                     3
#define BL_ZC_XZONETYP_2_MASK                                                             0x000001c0
#define BL_ZC_XZONETYP_2_OFFSET                                                                    6
#define BL_ZC_XZONETYP_2_WIDTH                                                                     3
#define BL_ZC_XZONETYP_3_MASK                                                             0x00000e00
#define BL_ZC_XZONETYP_3_OFFSET                                                                    9
#define BL_ZC_XZONETYP_3_WIDTH                                                                     3
#define BL_ZC_XZONETYP_4_MASK                                                             0x00007000
#define BL_ZC_XZONETYP_4_OFFSET                                                                   12
#define BL_ZC_XZONETYP_4_WIDTH                                                                     3
#define BL_ZC_XZONETYP_5_MASK                                                             0x00038000
#define BL_ZC_XZONETYP_5_OFFSET                                                                   15
#define BL_ZC_XZONETYP_5_WIDTH                                                                     3
#define BL_ZC_XZONETYP_6_MASK                                                             0x001c0000
#define BL_ZC_XZONETYP_6_OFFSET                                                                   18
#define BL_ZC_XZONETYP_6_WIDTH                                                                     3
#define BL_ZC_XZONETYP_7_MASK                                                             0x00e00000
#define BL_ZC_XZONETYP_7_OFFSET                                                                   21
#define BL_ZC_XZONETYP_7_WIDTH                                                                     3
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_ZC_XZONETYP_8_15(0x744)
#define BL_ZC_XZONETYP_8_15                                                               0x00000744 //RW POD:0x0
#define BL_ZC_XZONETYP_8_MASK                                                             0x00000007
#define BL_ZC_XZONETYP_8_OFFSET                                                                    0
#define BL_ZC_XZONETYP_8_WIDTH                                                                     3
#define BL_ZC_XZONETYP_9_MASK                                                             0x00000038
#define BL_ZC_XZONETYP_9_OFFSET                                                                    3
#define BL_ZC_XZONETYP_9_WIDTH                                                                     3
#define BL_ZC_XZONETYP_10_MASK                                                            0x000001c0
#define BL_ZC_XZONETYP_10_OFFSET                                                                   6
#define BL_ZC_XZONETYP_10_WIDTH                                                                    3
#define BL_ZC_XZONETYP_11_MASK                                                            0x00000e00
#define BL_ZC_XZONETYP_11_OFFSET                                                                   9
#define BL_ZC_XZONETYP_11_WIDTH                                                                    3
#define BL_ZC_XZONETYP_12_MASK                                                            0x00007000
#define BL_ZC_XZONETYP_12_OFFSET                                                                  12
#define BL_ZC_XZONETYP_12_WIDTH                                                                    3
#define BL_ZC_XZONETYP_13_MASK                                                            0x00038000
#define BL_ZC_XZONETYP_13_OFFSET                                                                  15
#define BL_ZC_XZONETYP_13_WIDTH                                                                    3
#define BL_ZC_XZONETYP_14_MASK                                                            0x001c0000
#define BL_ZC_XZONETYP_14_OFFSET                                                                  18
#define BL_ZC_XZONETYP_14_WIDTH                                                                    3
#define BL_ZC_XZONETYP_15_MASK                                                            0x00e00000
#define BL_ZC_XZONETYP_15_OFFSET                                                                  21
#define BL_ZC_XZONETYP_15_WIDTH                                                                    3
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_ZC_XZONETYP_16_23(0x748)
#define BL_ZC_XZONETYP_16_23                                                              0x00000748 //RW POD:0x0
#define BL_ZC_XZONETYP_16_MASK                                                            0x00000007
#define BL_ZC_XZONETYP_16_OFFSET                                                                   0
#define BL_ZC_XZONETYP_16_WIDTH                                                                    3
#define BL_ZC_XZONETYP_17_MASK                                                            0x00000038
#define BL_ZC_XZONETYP_17_OFFSET                                                                   3
#define BL_ZC_XZONETYP_17_WIDTH                                                                    3
#define BL_ZC_XZONETYP_18_MASK                                                            0x000001c0
#define BL_ZC_XZONETYP_18_OFFSET                                                                   6
#define BL_ZC_XZONETYP_18_WIDTH                                                                    3
#define BL_ZC_XZONETYP_19_MASK                                                            0x00000e00
#define BL_ZC_XZONETYP_19_OFFSET                                                                   9
#define BL_ZC_XZONETYP_19_WIDTH                                                                    3
#define BL_ZC_XZONETYP_20_MASK                                                            0x00007000
#define BL_ZC_XZONETYP_20_OFFSET                                                                  12
#define BL_ZC_XZONETYP_20_WIDTH                                                                    3
#define BL_ZC_XZONETYP_21_MASK                                                            0x00038000
#define BL_ZC_XZONETYP_21_OFFSET                                                                  15
#define BL_ZC_XZONETYP_21_WIDTH                                                                    3
#define BL_ZC_XZONETYP_22_MASK                                                            0x001c0000
#define BL_ZC_XZONETYP_22_OFFSET                                                                  18
#define BL_ZC_XZONETYP_22_WIDTH                                                                    3
#define BL_ZC_XZONETYP_23_MASK                                                            0x00e00000
#define BL_ZC_XZONETYP_23_OFFSET                                                                  21
#define BL_ZC_XZONETYP_23_WIDTH                                                                    3
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_ZC_YZONETYP_0_7(0x74c)
#define BL_ZC_YZONETYP_0_7                                                                0x0000074c //RW POD:0x0
#define BL_ZC_YZONETYP_0_MASK                                                             0x00000007
#define BL_ZC_YZONETYP_0_OFFSET                                                                    0
#define BL_ZC_YZONETYP_0_WIDTH                                                                     3
#define BL_ZC_YZONETYP_1_MASK                                                             0x00000038
#define BL_ZC_YZONETYP_1_OFFSET                                                                    3
#define BL_ZC_YZONETYP_1_WIDTH                                                                     3
#define BL_ZC_YZONETYP_2_MASK                                                             0x000001c0
#define BL_ZC_YZONETYP_2_OFFSET                                                                    6
#define BL_ZC_YZONETYP_2_WIDTH                                                                     3
#define BL_ZC_YZONETYP_3_MASK                                                             0x00000e00
#define BL_ZC_YZONETYP_3_OFFSET                                                                    9
#define BL_ZC_YZONETYP_3_WIDTH                                                                     3
#define BL_ZC_YZONETYP_4_MASK                                                             0x00007000
#define BL_ZC_YZONETYP_4_OFFSET                                                                   12
#define BL_ZC_YZONETYP_4_WIDTH                                                                     3
#define BL_ZC_YZONETYP_5_MASK                                                             0x00038000
#define BL_ZC_YZONETYP_5_OFFSET                                                                   15
#define BL_ZC_YZONETYP_5_WIDTH                                                                     3
#define BL_ZC_YZONETYP_6_MASK                                                             0x001c0000
#define BL_ZC_YZONETYP_6_OFFSET                                                                   18
#define BL_ZC_YZONETYP_6_WIDTH                                                                     3
#define BL_ZC_YZONETYP_7_MASK                                                             0x00e00000
#define BL_ZC_YZONETYP_7_OFFSET                                                                   21
#define BL_ZC_YZONETYP_7_WIDTH                                                                     3
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_ZC_YZONETYP_8_15(0x750)
#define BL_ZC_YZONETYP_8_15                                                               0x00000750 //RW POD:0x0
#define BL_ZC_YZONETYP_8_MASK                                                             0x00000007
#define BL_ZC_YZONETYP_8_OFFSET                                                                    0
#define BL_ZC_YZONETYP_8_WIDTH                                                                     3
#define BL_ZC_YZONETYP_9_MASK                                                             0x00000038
#define BL_ZC_YZONETYP_9_OFFSET                                                                    3
#define BL_ZC_YZONETYP_9_WIDTH                                                                     3
#define BL_ZC_YZONETYP_10_MASK                                                            0x000001c0
#define BL_ZC_YZONETYP_10_OFFSET                                                                   6
#define BL_ZC_YZONETYP_10_WIDTH                                                                    3
#define BL_ZC_YZONETYP_11_MASK                                                            0x00000e00
#define BL_ZC_YZONETYP_11_OFFSET                                                                   9
#define BL_ZC_YZONETYP_11_WIDTH                                                                    3
#define BL_ZC_YZONETYP_12_MASK                                                            0x00007000
#define BL_ZC_YZONETYP_12_OFFSET                                                                  12
#define BL_ZC_YZONETYP_12_WIDTH                                                                    3
#define BL_ZC_YZONETYP_13_MASK                                                            0x00038000
#define BL_ZC_YZONETYP_13_OFFSET                                                                  15
#define BL_ZC_YZONETYP_13_WIDTH                                                                    3
#define BL_ZC_YZONETYP_14_MASK                                                            0x001c0000
#define BL_ZC_YZONETYP_14_OFFSET                                                                  18
#define BL_ZC_YZONETYP_14_WIDTH                                                                    3
#define BL_ZC_YZONETYP_15_MASK                                                            0x00e00000
#define BL_ZC_YZONETYP_15_OFFSET                                                                  21
#define BL_ZC_YZONETYP_15_WIDTH                                                                    3
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_ZC_CONTOURTYP_0_3(0x754)
#define BL_ZC_CONTOURTYP_0_3                                                              0x00000754 //RW POD:0x0
#define BL_ZC_CONTOURTYP_0_MASK                                                           0x0000001f
#define BL_ZC_CONTOURTYP_0_OFFSET                                                                  0
#define BL_ZC_CONTOURTYP_0_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_1_MASK                                                           0x000003e0
#define BL_ZC_CONTOURTYP_1_OFFSET                                                                  5
#define BL_ZC_CONTOURTYP_1_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_2_MASK                                                           0x00007c00
#define BL_ZC_CONTOURTYP_2_OFFSET                                                                 10
#define BL_ZC_CONTOURTYP_2_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_3_MASK                                                           0x000f8000
#define BL_ZC_CONTOURTYP_3_OFFSET                                                                 15
#define BL_ZC_CONTOURTYP_3_WIDTH                                                                   5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_4_7(0x758)
#define BL_ZC_CONTOURTYP_4_7                                                              0x00000758 //RW POD:0x0
#define BL_ZC_CONTOURTYP_4_MASK                                                           0x0000001f
#define BL_ZC_CONTOURTYP_4_OFFSET                                                                  0
#define BL_ZC_CONTOURTYP_4_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_5_MASK                                                           0x000003e0
#define BL_ZC_CONTOURTYP_5_OFFSET                                                                  5
#define BL_ZC_CONTOURTYP_5_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_6_MASK                                                           0x00007c00
#define BL_ZC_CONTOURTYP_6_OFFSET                                                                 10
#define BL_ZC_CONTOURTYP_6_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_7_MASK                                                           0x000f8000
#define BL_ZC_CONTOURTYP_7_OFFSET                                                                 15
#define BL_ZC_CONTOURTYP_7_WIDTH                                                                   5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_8_11(0x75c)
#define BL_ZC_CONTOURTYP_8_11                                                             0x0000075c //RW POD:0x0
#define BL_ZC_CONTOURTYP_8_MASK                                                           0x0000001f
#define BL_ZC_CONTOURTYP_8_OFFSET                                                                  0
#define BL_ZC_CONTOURTYP_8_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_9_MASK                                                           0x000003e0
#define BL_ZC_CONTOURTYP_9_OFFSET                                                                  5
#define BL_ZC_CONTOURTYP_9_WIDTH                                                                   5
#define BL_ZC_CONTOURTYP_10_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_10_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_10_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_11_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_11_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_11_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_12_15(0x760)
#define BL_ZC_CONTOURTYP_12_15                                                            0x00000760 //RW POD:0x0
#define BL_ZC_CONTOURTYP_12_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_12_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_12_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_13_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_13_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_13_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_14_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_14_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_14_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_15_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_15_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_15_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_16_19(0x764)
#define BL_ZC_CONTOURTYP_16_19                                                            0x00000764 //RW POD:0x0
#define BL_ZC_CONTOURTYP_16_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_16_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_16_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_17_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_17_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_17_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_18_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_18_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_18_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_19_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_19_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_19_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_20_23(0x768)
#define BL_ZC_CONTOURTYP_20_23                                                            0x00000768 //RW POD:0x0
#define BL_ZC_CONTOURTYP_20_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_20_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_20_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_21_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_21_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_21_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_22_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_22_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_22_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_23_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_23_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_23_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_24_27(0x76c)
#define BL_ZC_CONTOURTYP_24_27                                                            0x0000076c //RW POD:0x0
#define BL_ZC_CONTOURTYP_24_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_24_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_24_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_25_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_25_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_25_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_26_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_26_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_26_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_27_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_27_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_27_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_28_31(0x770)
#define BL_ZC_CONTOURTYP_28_31                                                            0x00000770 //RW POD:0x0
#define BL_ZC_CONTOURTYP_28_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_28_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_28_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_29_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_29_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_29_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_30_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_30_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_30_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_31_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_31_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_31_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_32_35(0x774)
#define BL_ZC_CONTOURTYP_32_35                                                            0x00000774 //RW POD:0x0
#define BL_ZC_CONTOURTYP_32_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_32_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_32_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_33_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_33_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_33_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_34_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_34_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_34_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_35_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_35_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_35_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_36_39(0x778)
#define BL_ZC_CONTOURTYP_36_39                                                            0x00000778 //RW POD:0x0
#define BL_ZC_CONTOURTYP_36_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_36_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_36_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_37_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_37_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_37_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_38_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_38_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_38_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_39_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_39_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_39_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_40_43(0x77c)
#define BL_ZC_CONTOURTYP_40_43                                                            0x0000077c //RW POD:0x0
#define BL_ZC_CONTOURTYP_40_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_40_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_40_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_41_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_41_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_41_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_42_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_42_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_42_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_43_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_43_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_43_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_44_47(0x780)
#define BL_ZC_CONTOURTYP_44_47                                                            0x00000780 //RW POD:0x0
#define BL_ZC_CONTOURTYP_44_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_44_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_44_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_45_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_45_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_45_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_46_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_46_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_46_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_47_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_47_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_47_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_48_51(0x784)
#define BL_ZC_CONTOURTYP_48_51                                                            0x00000784 //RW POD:0x0
#define BL_ZC_CONTOURTYP_48_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_48_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_48_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_49_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_49_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_49_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_50_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_50_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_50_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_51_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_51_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_51_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_52_55(0x788)
#define BL_ZC_CONTOURTYP_52_55                                                            0x00000788 //RW POD:0x0
#define BL_ZC_CONTOURTYP_52_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_52_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_52_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_53_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_53_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_53_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_54_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_54_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_54_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_55_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_55_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_55_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_56_59(0x78c)
#define BL_ZC_CONTOURTYP_56_59                                                            0x0000078c //RW POD:0x0
#define BL_ZC_CONTOURTYP_56_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_56_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_56_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_57_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_57_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_57_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_58_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_58_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_58_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_59_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_59_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_59_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CONTOURTYP_60_63(0x790)
#define BL_ZC_CONTOURTYP_60_63                                                            0x00000790 //RW POD:0x0
#define BL_ZC_CONTOURTYP_60_MASK                                                          0x0000001f
#define BL_ZC_CONTOURTYP_60_OFFSET                                                                 0
#define BL_ZC_CONTOURTYP_60_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_61_MASK                                                          0x000003e0
#define BL_ZC_CONTOURTYP_61_OFFSET                                                                 5
#define BL_ZC_CONTOURTYP_61_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_62_MASK                                                          0x00007c00
#define BL_ZC_CONTOURTYP_62_OFFSET                                                                10
#define BL_ZC_CONTOURTYP_62_WIDTH                                                                  5
#define BL_ZC_CONTOURTYP_63_MASK                                                          0x000f8000
#define BL_ZC_CONTOURTYP_63_OFFSET                                                                15
#define BL_ZC_CONTOURTYP_63_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xfff00000
// #define Reserved_OFFSET                                                                           20
// #define Reserved_WIDTH                                                                            12



// Register BL_ZC_CTRL(0x794)
#define BL_ZC_CTRL                                                                        0x00000794 //RW POD:0x0
#define BL_ZC_CONTOUR_TBL_EBL_MASK                                                        0x00000001
#define BL_ZC_CONTOUR_TBL_EBL_OFFSET                                                               0
#define BL_ZC_CONTOUR_TBL_EBL_WIDTH                                                                1
#define BL_ZC_CTRL_reserved_MASK                                                          0x00000002
#define BL_ZC_CTRL_reserved_OFFSET                                                                 1
#define BL_ZC_CTRL_reserved_WIDTH                                                                  1
#define BL_ZC_VERT_SYM_MASK                                                               0x00000004
#define BL_ZC_VERT_SYM_OFFSET                                                                      2
#define BL_ZC_VERT_SYM_WIDTH                                                                       1
#define BL_ZC_HOZ_SYM_MASK                                                                0x00000008
#define BL_ZC_HOZ_SYM_OFFSET                                                                       3
#define BL_ZC_HOZ_SYM_WIDTH                                                                        1
#define BL_ZC_CONTOUR_DBL_HIGH_MASK                                                       0x00000010
#define BL_ZC_CONTOUR_DBL_HIGH_OFFSET                                                              4
#define BL_ZC_CONTOUR_DBL_HIGH_WIDTH                                                               1
#define BL_ZC_CONTOUR_DBL_WIDE_MASK                                                       0x00000020
#define BL_ZC_CONTOUR_DBL_WIDE_OFFSET                                                              5
#define BL_ZC_CONTOUR_DBL_WIDE_WIDTH                                                               1
#define BL_ZC_REP_COL0_MASK                                                               0x00000040
#define BL_ZC_REP_COL0_OFFSET                                                                      6
#define BL_ZC_REP_COL0_WIDTH                                                                       1
#define BL_ZC_REP_ROW0_MASK                                                               0x00000080
#define BL_ZC_REP_ROW0_OFFSET                                                                      7
#define BL_ZC_REP_ROW0_WIDTH                                                                       1
#define BL_ZC_NUMCONTOURS_MASK                                                            0x00000300
#define BL_ZC_NUMCONTOURS_OFFSET                                                                   8
#define BL_ZC_NUMCONTOURS_WIDTH                                                                    2
// #define Reserved_MASK                                                                     0xfffffc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                            22



// Register BL_GBTABLE_ADD(0x7a0)
#define BL_GBTABLE_ADD                                                                    0x000007a0 //RW POD:0x0
#define BL_GRIDBRITE_reserved_MASK                                                        0x000007ff
#define BL_GRIDBRITE_reserved_OFFSET                                                               0
#define BL_GRIDBRITE_reserved_WIDTH                                                               11
#define BL_GRIDBRITE_EBL_MASK                                                             0x00000800
#define BL_GRIDBRITE_EBL_OFFSET                                                                   11
#define BL_GRIDBRITE_EBL_WIDTH                                                                     1
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_BRC_CTRL(0x7b8)
#define BL_BRC_CTRL                                                                       0x000007b8 //RW POD:0x0
#define BL_BRC_reserved_MASK                                                              0x00000001
#define BL_BRC_reserved_OFFSET                                                                     0
#define BL_BRC_reserved_WIDTH                                                                      1
#define BL_BRC_AUTO_CALC_MASK                                                             0x00000002
#define BL_BRC_AUTO_CALC_OFFSET                                                                    1
#define BL_BRC_AUTO_CALC_WIDTH                                                                     1
#define BL_BRC_AUTO_SCL_MASK                                                              0x00000004
#define BL_BRC_AUTO_SCL_OFFSET                                                                     2
#define BL_BRC_AUTO_SCL_WIDTH                                                                      1
#define BL_BRC_SCL_EBL_MASK                                                               0x00000008
#define BL_BRC_SCL_EBL_OFFSET                                                                      3
#define BL_BRC_SCL_EBL_WIDTH                                                                       1
#define BL_BRC_BASE_FRAME_MASK                                                            0x000000f0
#define BL_BRC_BASE_FRAME_OFFSET                                                                   4
#define BL_BRC_BASE_FRAME_WIDTH                                                                    4
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_BRC_SHFT(0x7bc)
#define BL_BRC_SHFT                                                                       0x000007bc //RW POD:0x0
#define BL_BRC_ITC_SHFT_MASK                                                              0x0000001f
#define BL_BRC_ITC_SHFT_OFFSET                                                                     0
#define BL_BRC_ITC_SHFT_WIDTH                                                                      5
#define BL_BRC_IT_SHFT_MASK                                                               0x000000e0
#define BL_BRC_IT_SHFT_OFFSET                                                                      5
#define BL_BRC_IT_SHFT_WIDTH                                                                       3
#define BL_BRC_SHIFTSEED_MASK                                                             0x00001f00
#define BL_BRC_SHIFTSEED_OFFSET                                                                    8
#define BL_BRC_SHIFTSEED_WIDTH                                                                     5
// #define Reserved_MASK                                                                     0xffffe000
// #define Reserved_OFFSET                                                                           13
// #define Reserved_WIDTH                                                                            19



// Register BL_BRC_SCALE(0x7c0)
#define BL_BRC_SCALE                                                                      0x000007c0 //RW POD:0x0
#define BL_BRC_RED_SCALE_MASK                                                             0x00000fff
#define BL_BRC_RED_SCALE_OFFSET                                                                    0
#define BL_BRC_RED_SCALE_WIDTH                                                                    12
#define BL_BRC_GRN_SCALE_MASK                                                             0x00fff000
#define BL_BRC_GRN_SCALE_OFFSET                                                                   12
#define BL_BRC_GRN_SCALE_WIDTH                                                                    12
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_BRC_SCALE2(0x7c4)
#define BL_BRC_SCALE2                                                                     0x000007c4 //RW POD:0x0
#define BL_BRC_BLU_SCALE_MASK                                                             0x00000fff
#define BL_BRC_BLU_SCALE_OFFSET                                                                    0
#define BL_BRC_BLU_SCALE_WIDTH                                                                    12
// #define Reserved_MASK                                                                     0xfffff000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                            20



// Register BL_CONTROL_1(0x7c8)
#define BL_CONTROL_1                                                                      0x000007c8 //RW POD:0x0
#define BL_COMMS_FORM_EBL_MASK                                                            0x00000001
#define BL_COMMS_FORM_EBL_OFFSET                                                                   0
#define BL_COMMS_FORM_EBL_WIDTH                                                                    1
#define BL_CKG_CLK0_INVERT_MASK                                                           0x00000002
#define BL_CKG_CLK0_INVERT_OFFSET                                                                  1
#define BL_CKG_CLK0_INVERT_WIDTH                                                                   1
#define BL_CKG_CLK1_INVERT_MASK                                                           0x00000004
#define BL_CKG_CLK1_INVERT_OFFSET                                                                  2
#define BL_CKG_CLK1_INVERT_WIDTH                                                                   1
#define BL_CKG_CLK2_INVERT_MASK                                                           0x00000008
#define BL_CKG_CLK2_INVERT_OFFSET                                                                  3
#define BL_CKG_CLK2_INVERT_WIDTH                                                                   1
#define BL_CKG_CLK0_STOPIM_MASK                                                           0x00000010
#define BL_CKG_CLK0_STOPIM_OFFSET                                                                  4
#define BL_CKG_CLK0_STOPIM_WIDTH                                                                   1
#define BL_CKG_CLK1_STOPIM_MASK                                                           0x00000020
#define BL_CKG_CLK1_STOPIM_OFFSET                                                                  5
#define BL_CKG_CLK1_STOPIM_WIDTH                                                                   1
#define BL_CKG_CLK2_STOPIM_MASK                                                           0x00000040
#define BL_CKG_CLK2_STOPIM_OFFSET                                                                  6
#define BL_CKG_CLK2_STOPIM_WIDTH                                                                   1
#define FORCE_MAX_BRITES_MASK                                                             0x00000080
#define FORCE_MAX_BRITES_OFFSET                                                                    7
#define FORCE_MAX_BRITES_WIDTH                                                                     1
#define PAD_OE_INVERT_MASK                                                                0x00000100
#define PAD_OE_INVERT_OFFSET                                                                       8
#define PAD_OE_INVERT_WIDTH                                                                        1
#define BL_COMMS_RESET_MASK                                                               0x00000200
#define BL_COMMS_RESET_OFFSET                                                                      9
#define BL_COMMS_RESET_WIDTH                                                                       1
// #define Reserved_MASK                                                                     0xfffffc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                            22



// Register BL_CKG_PIXCK_CK0(0x7cc)
#define BL_CKG_PIXCK_CK0                                                                  0x000007cc //RW POD:0x0
#define BL_CKG_PIXCLK_PER_CLK0_MASK                                                       0x007fffff
#define BL_CKG_PIXCLK_PER_CLK0_OFFSET                                                              0
#define BL_CKG_PIXCLK_PER_CLK0_WIDTH                                                              23
// #define Reserved_MASK                                                                     0xff800000
// #define Reserved_OFFSET                                                                           23
// #define Reserved_WIDTH                                                                             9



// Register BL_CKG_CK0_GTE(0x7d0)
#define BL_CKG_CK0_GTE                                                                    0x000007d0 //RW POD:0x0
#define BL_CKG_CLK0_GATE_MASK                                                             0x0000000f
#define BL_CKG_CLK0_GATE_OFFSET                                                                    0
#define BL_CKG_CLK0_GATE_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0xfffffff0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                            28



// Register BL_CKG_CK0_CYC(0x7d4)
#define BL_CKG_CK0_CYC                                                                    0x000007d4 //RW POD:0x0
#define BL_CKG_CLK0_PER_PERIOD_MASK                                                       0x00003fff
#define BL_CKG_CLK0_PER_PERIOD_OFFSET                                                              0
#define BL_CKG_CLK0_PER_PERIOD_WIDTH                                                              14
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_CKG_PIXCK_CK1(0x7d8)
#define BL_CKG_PIXCK_CK1                                                                  0x000007d8 //RW POD:0x0
#define BL_CKG_PIXCLK_PER_CLK1_MASK                                                       0x007fffff
#define BL_CKG_PIXCLK_PER_CLK1_OFFSET                                                              0
#define BL_CKG_PIXCLK_PER_CLK1_WIDTH                                                              23
// #define Reserved_MASK                                                                     0xff800000
// #define Reserved_OFFSET                                                                           23
// #define Reserved_WIDTH                                                                             9



// Register BL_CKG_CK1_GTE(0x7dc)
#define BL_CKG_CK1_GTE                                                                    0x000007dc //RW POD:0x0
#define BL_CKG_CLK1_GATE_MASK                                                             0x0000000f
#define BL_CKG_CLK1_GATE_OFFSET                                                                    0
#define BL_CKG_CLK1_GATE_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0xfffffff0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                            28



// Register BL_CKG_CK1_CYC(0x7e0)
#define BL_CKG_CK1_CYC                                                                    0x000007e0 //RW POD:0x0
#define BL_CKG_CLK1_PER_PERIOD_MASK                                                       0x00003fff
#define BL_CKG_CLK1_PER_PERIOD_OFFSET                                                              0
#define BL_CKG_CLK1_PER_PERIOD_WIDTH                                                              14
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_CKG_PIXCK_CK2(0x7e4)
#define BL_CKG_PIXCK_CK2                                                                  0x000007e4 //RW POD:0x0
#define BL_CKG_PIXCLK_PER_CLK2_MASK                                                       0x007fffff
#define BL_CKG_PIXCLK_PER_CLK2_OFFSET                                                              0
#define BL_CKG_PIXCLK_PER_CLK2_WIDTH                                                              23
// #define Reserved_MASK                                                                     0xff800000
// #define Reserved_OFFSET                                                                           23
// #define Reserved_WIDTH                                                                             9



// Register BL_CKG_CK2_GTE(0x7e8)
#define BL_CKG_CK2_GTE                                                                    0x000007e8 //RW POD:0x0
#define BL_CKG_CLK2_GATE_MASK                                                             0x0000000f
#define BL_CKG_CLK2_GATE_OFFSET                                                                    0
#define BL_CKG_CLK2_GATE_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0xfffffff0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                            28



// Register BL_CKG_CK2_CYC(0x7ec)
#define BL_CKG_CK2_CYC                                                                    0x000007ec //RW POD:0x0
#define BL_CKG_CLK2_PER_PERIOD_MASK                                                       0x00003fff
#define BL_CKG_CLK2_PER_PERIOD_OFFSET                                                              0
#define BL_CKG_CLK2_PER_PERIOD_WIDTH                                                              14
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH0_CTRL(0x7f8)
#define BL_SSH0_CTRL                                                                      0x000007f8 //RW POD:0x0
#define BL_SSH0_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH0_NUM_BITS_OFFSET                                                                    0
#define BL_SSH0_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH0_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH0_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH0_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH0_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH0_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH0_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH0_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH0_CLK_SEL_OFFSET                                                                    10
#define BL_SSH0_CLK_SEL_WIDTH                                                                      2
#define BL_SSH0_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH0_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH0_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH0_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH0_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH0_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH1_CTRL(0x7fc)
#define BL_SSH1_CTRL                                                                      0x000007fc //RW POD:0x0
#define BL_SSH1_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH1_NUM_BITS_OFFSET                                                                    0
#define BL_SSH1_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH1_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH1_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH1_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH1_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH1_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH1_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH1_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH1_CLK_SEL_OFFSET                                                                    10
#define BL_SSH1_CLK_SEL_WIDTH                                                                      2
#define BL_SSH1_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH1_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH1_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH1_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH1_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH1_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH2_CTRL(0x800)
#define BL_SSH2_CTRL                                                                      0x00000800 //RW POD:0x0
#define BL_SSH2_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH2_NUM_BITS_OFFSET                                                                    0
#define BL_SSH2_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH2_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH2_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH2_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH2_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH2_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH2_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH2_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH2_CLK_SEL_OFFSET                                                                    10
#define BL_SSH2_CLK_SEL_WIDTH                                                                      2
#define BL_SSH2_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH2_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH2_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH2_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH2_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH2_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH3_CTRL(0x804)
#define BL_SSH3_CTRL                                                                      0x00000804 //RW POD:0x0
#define BL_SSH3_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH3_NUM_BITS_OFFSET                                                                    0
#define BL_SSH3_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH3_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH3_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH3_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH3_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH3_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH3_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH3_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH3_CLK_SEL_OFFSET                                                                    10
#define BL_SSH3_CLK_SEL_WIDTH                                                                      2
#define BL_SSH3_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH3_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH3_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH3_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH3_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH3_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH4_CTRL(0x808)
#define BL_SSH4_CTRL                                                                      0x00000808 //RW POD:0x0
#define BL_SSH4_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH4_NUM_BITS_OFFSET                                                                    0
#define BL_SSH4_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH4_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH4_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH4_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH4_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH4_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH4_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH4_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH4_CLK_SEL_OFFSET                                                                    10
#define BL_SSH4_CLK_SEL_WIDTH                                                                      2
#define BL_SSH4_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH4_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH4_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH4_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH4_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH4_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH5_CTRL(0x80c)
#define BL_SSH5_CTRL                                                                      0x0000080c //RW POD:0x0
#define BL_SSH5_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH5_NUM_BITS_OFFSET                                                                    0
#define BL_SSH5_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH5_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH5_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH5_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH5_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH5_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH5_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH5_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH5_CLK_SEL_OFFSET                                                                    10
#define BL_SSH5_CLK_SEL_WIDTH                                                                      2
#define BL_SSH5_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH5_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH5_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH5_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH5_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH5_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH6_CTRL(0x810)
#define BL_SSH6_CTRL                                                                      0x00000810 //RW POD:0x0
#define BL_SSH6_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH6_NUM_BITS_OFFSET                                                                    0
#define BL_SSH6_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH6_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH6_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH6_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH6_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH6_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH6_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH6_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH6_CLK_SEL_OFFSET                                                                    10
#define BL_SSH6_CLK_SEL_WIDTH                                                                      2
#define BL_SSH6_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH6_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH6_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH6_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH6_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH6_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_SSH7_CTRL(0x814)
#define BL_SSH7_CTRL                                                                      0x00000814 //RW POD:0x0
#define BL_SSH7_NUM_BITS_MASK                                                             0x0000000f
#define BL_SSH7_NUM_BITS_OFFSET                                                                    0
#define BL_SSH7_NUM_BITS_WIDTH                                                                     4
// #define Reserved_MASK                                                                     0x000000f0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4
#define BL_SSH7_SHIFT_LEFT_MASK                                                           0x00000100
#define BL_SSH7_SHIFT_LEFT_OFFSET                                                                  8
#define BL_SSH7_SHIFT_LEFT_WIDTH                                                                   1
#define BL_SSH7_FALL_EDGE_MASK                                                            0x00000200
#define BL_SSH7_FALL_EDGE_OFFSET                                                                   9
#define BL_SSH7_FALL_EDGE_WIDTH                                                                    1
#define BL_SSH7_CLK_SEL_MASK                                                              0x00000c00
#define BL_SSH7_CLK_SEL_OFFSET                                                                    10
#define BL_SSH7_CLK_SEL_WIDTH                                                                      2
#define BL_SSH7_SYNCSHIFT_MASK                                                            0x00001000
#define BL_SSH7_SYNCSHIFT_OFFSET                                                                  12
#define BL_SSH7_SYNCSHIFT_WIDTH                                                                    1
#define BL_SSH7_CLKOUT_FALL_ALGN_MASK                                                     0x00002000
#define BL_SSH7_CLKOUT_FALL_ALGN_OFFSET                                                           13
#define BL_SSH7_CLKOUT_FALL_ALGN_WIDTH                                                             1
// #define Reserved_MASK                                                                     0xffffc000
// #define Reserved_OFFSET                                                                           14
// #define Reserved_WIDTH                                                                            18



// Register BL_COMMS_BIT_CTRL(0x838)
#define BL_COMMS_BIT_CTRL                                                                 0x00000838 //RW POD:0x0
#define BL_COMMS_CTRL_BIT0_MASK                                                           0x00000001
#define BL_COMMS_CTRL_BIT0_OFFSET                                                                  0
#define BL_COMMS_CTRL_BIT0_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT1_MASK                                                           0x00000002
#define BL_COMMS_CTRL_BIT1_OFFSET                                                                  1
#define BL_COMMS_CTRL_BIT1_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT2_MASK                                                           0x00000004
#define BL_COMMS_CTRL_BIT2_OFFSET                                                                  2
#define BL_COMMS_CTRL_BIT2_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT3_MASK                                                           0x00000008
#define BL_COMMS_CTRL_BIT3_OFFSET                                                                  3
#define BL_COMMS_CTRL_BIT3_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT4_MASK                                                           0x00000010
#define BL_COMMS_CTRL_BIT4_OFFSET                                                                  4
#define BL_COMMS_CTRL_BIT4_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT5_MASK                                                           0x00000020
#define BL_COMMS_CTRL_BIT5_OFFSET                                                                  5
#define BL_COMMS_CTRL_BIT5_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT6_MASK                                                           0x00000040
#define BL_COMMS_CTRL_BIT6_OFFSET                                                                  6
#define BL_COMMS_CTRL_BIT6_WIDTH                                                                   1
#define BL_COMMS_CTRL_BIT7_MASK                                                           0x00000080
#define BL_COMMS_CTRL_BIT7_OFFSET                                                                  7
#define BL_COMMS_CTRL_BIT7_WIDTH                                                                   1
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_COMMS_BYTE_CTRL_01(0x83c)
#define BL_COMMS_BYTE_CTRL_01                                                             0x0000083c //RW POD:0x0
#define BL_COMMS_CTRL_BYTE0_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTE0_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTE0_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTE1_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTE1_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTE1_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_23(0x840)
#define BL_COMMS_BYTE_CTRL_23                                                             0x00000840 //RW POD:0x0
#define BL_COMMS_CTRL_BYTE2_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTE2_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTE2_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTE3_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTE3_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTE3_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_45(0x844)
#define BL_COMMS_BYTE_CTRL_45                                                             0x00000844 //RW POD:0x0
#define BL_COMMS_CTRL_BYTE4_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTE4_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTE4_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTE5_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTE5_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTE5_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_67(0x848)
#define BL_COMMS_BYTE_CTRL_67                                                             0x00000848 //RW POD:0x0
#define BL_COMMS_CTRL_BYTE6_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTE6_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTE6_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTE7_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTE7_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTE7_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_89(0x84c)
#define BL_COMMS_BYTE_CTRL_89                                                             0x0000084c //RW POD:0x0
#define BL_COMMS_CTRL_BYTE8_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTE8_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTE8_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTE9_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTE9_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTE9_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_AB(0x850)
#define BL_COMMS_BYTE_CTRL_AB                                                             0x00000850 //RW POD:0x0
#define BL_COMMS_CTRL_BYTEA_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTEA_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTEA_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTEB_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTEB_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTEB_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_CD(0x854)
#define BL_COMMS_BYTE_CTRL_CD                                                             0x00000854 //RW POD:0x0
#define BL_COMMS_CTRL_BYTEC_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTEC_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTEC_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTED_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTED_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTED_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_CTRL_EF(0x858)
#define BL_COMMS_BYTE_CTRL_EF                                                             0x00000858 //RW POD:0x0
#define BL_COMMS_CTRL_BYTEE_MASK                                                          0x000000ff
#define BL_COMMS_CTRL_BYTEE_OFFSET                                                                 0
#define BL_COMMS_CTRL_BYTEE_WIDTH                                                                  8
#define BL_COMMS_CTRL_BYTEF_MASK                                                          0x0000ff00
#define BL_COMMS_CTRL_BYTEF_OFFSET                                                                 8
#define BL_COMMS_CTRL_BYTEF_WIDTH                                                                  8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_0(0x85c)
#define BL_COMMS_WORD_CTRL_0                                                              0x0000085c //RW POD:0x0
#define BL_COMMS_CTRL_WORD0_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD0_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD0_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_1(0x860)
#define BL_COMMS_WORD_CTRL_1                                                              0x00000860 //RW POD:0x0
#define BL_COMMS_CTRL_WORD1_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD1_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD1_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_2(0x864)
#define BL_COMMS_WORD_CTRL_2                                                              0x00000864 //RW POD:0x0
#define BL_COMMS_CTRL_WORD2_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD2_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD2_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_3(0x868)
#define BL_COMMS_WORD_CTRL_3                                                              0x00000868 //RW POD:0x0
#define BL_COMMS_CTRL_WORD3_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD3_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD3_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_4(0x86c)
#define BL_COMMS_WORD_CTRL_4                                                              0x0000086c //RW POD:0x0
#define BL_COMMS_CTRL_WORD4_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD4_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD4_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_5(0x870)
#define BL_COMMS_WORD_CTRL_5                                                              0x00000870 //RW POD:0x0
#define BL_COMMS_CTRL_WORD5_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD5_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD5_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_6(0x874)
#define BL_COMMS_WORD_CTRL_6                                                              0x00000874 //RW POD:0x0
#define BL_COMMS_CTRL_WORD6_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD6_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD6_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_CTRL_7(0x878)
#define BL_COMMS_WORD_CTRL_7                                                              0x00000878 //RW POD:0x0
#define BL_COMMS_CTRL_WORD7_MASK                                                          0x0000ffff
#define BL_COMMS_CTRL_WORD7_OFFSET                                                                 0
#define BL_COMMS_CTRL_WORD7_WIDTH                                                                 16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BIT_STAT(0x884)
#define BL_COMMS_BIT_STAT                                                                 0x00000884 //RO POD:0x0
#define BL_COMMS_ST_BIT0_MASK                                                             0x00000001
#define BL_COMMS_ST_BIT0_OFFSET                                                                    0
#define BL_COMMS_ST_BIT0_WIDTH                                                                     1
#define BL_COMMS_ST_BIT1_MASK                                                             0x00000002
#define BL_COMMS_ST_BIT1_OFFSET                                                                    1
#define BL_COMMS_ST_BIT1_WIDTH                                                                     1
#define BL_COMMS_ST_BIT2_MASK                                                             0x00000004
#define BL_COMMS_ST_BIT2_OFFSET                                                                    2
#define BL_COMMS_ST_BIT2_WIDTH                                                                     1
#define BL_COMMS_ST_BIT3_MASK                                                             0x00000008
#define BL_COMMS_ST_BIT3_OFFSET                                                                    3
#define BL_COMMS_ST_BIT3_WIDTH                                                                     1
#define BL_COMMS_ST_BIT4_MASK                                                             0x00000010
#define BL_COMMS_ST_BIT4_OFFSET                                                                    4
#define BL_COMMS_ST_BIT4_WIDTH                                                                     1
#define BL_COMMS_ST_BIT5_MASK                                                             0x00000020
#define BL_COMMS_ST_BIT5_OFFSET                                                                    5
#define BL_COMMS_ST_BIT5_WIDTH                                                                     1
#define BL_COMMS_ST_BIT6_MASK                                                             0x00000040
#define BL_COMMS_ST_BIT6_OFFSET                                                                    6
#define BL_COMMS_ST_BIT6_WIDTH                                                                     1
#define BL_COMMS_ST_BIT7_MASK                                                             0x00000080
#define BL_COMMS_ST_BIT7_OFFSET                                                                    7
#define BL_COMMS_ST_BIT7_WIDTH                                                                     1
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_COMMS_BYTE_STAT_01(0x888)
#define BL_COMMS_BYTE_STAT_01                                                             0x00000888 //RO POD:0x0
#define BL_COMMS_ST_BYTE0_MASK                                                            0x000000ff
#define BL_COMMS_ST_BYTE0_OFFSET                                                                   0
#define BL_COMMS_ST_BYTE0_WIDTH                                                                    8
#define BL_COMMS_ST_BYTE1_MASK                                                            0x0000ff00
#define BL_COMMS_ST_BYTE1_OFFSET                                                                   8
#define BL_COMMS_ST_BYTE1_WIDTH                                                                    8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_BYTE_STAT_23(0x88c)
#define BL_COMMS_BYTE_STAT_23                                                             0x0000088c //RO POD:0x0
#define BL_COMMS_ST_BYTE2_MASK                                                            0x000000ff
#define BL_COMMS_ST_BYTE2_OFFSET                                                                   0
#define BL_COMMS_ST_BYTE2_WIDTH                                                                    8
#define BL_COMMS_ST_BYTE3_MASK                                                            0x0000ff00
#define BL_COMMS_ST_BYTE3_OFFSET                                                                   8
#define BL_COMMS_ST_BYTE3_WIDTH                                                                    8
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_STAT_0(0x890)
#define BL_COMMS_WORD_STAT_0                                                              0x00000890 //RO POD:0x0
#define BL_COMMS_ST_WORD0_MASK                                                            0x0000ffff
#define BL_COMMS_ST_WORD0_OFFSET                                                                   0
#define BL_COMMS_ST_WORD0_WIDTH                                                                   16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_STAT_1(0x894)
#define BL_COMMS_WORD_STAT_1                                                              0x00000894 //RO POD:0x0
#define BL_COMMS_ST_WORD1_MASK                                                            0x0000ffff
#define BL_COMMS_ST_WORD1_OFFSET                                                                   0
#define BL_COMMS_ST_WORD1_WIDTH                                                                   16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_STAT_2(0x898)
#define BL_COMMS_WORD_STAT_2                                                              0x00000898 //RO POD:0x0
#define BL_COMMS_ST_WORD2_MASK                                                            0x0000ffff
#define BL_COMMS_ST_WORD2_OFFSET                                                                   0
#define BL_COMMS_ST_WORD2_WIDTH                                                                   16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_WORD_STAT_3(0x89c)
#define BL_COMMS_WORD_STAT_3                                                              0x0000089c //RO POD:0x0
#define BL_COMMS_ST_WORD3_MASK                                                            0x0000ffff
#define BL_COMMS_ST_WORD3_OFFSET                                                                   0
#define BL_COMMS_ST_WORD3_WIDTH                                                                   16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_PIN_CTRL_0(0x8a8)
#define BL_COMMS_PIN_CTRL_0                                                               0x000008a8 //RW POD:0x0
#define BL_COMMS_PIN0_CTRL0_MASK                                                          0x00000001
#define BL_COMMS_PIN0_CTRL0_OFFSET                                                                 0
#define BL_COMMS_PIN0_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN1_CTRL0_MASK                                                          0x00000002
#define BL_COMMS_PIN1_CTRL0_OFFSET                                                                 1
#define BL_COMMS_PIN1_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN2_CTRL0_MASK                                                          0x00000004
#define BL_COMMS_PIN2_CTRL0_OFFSET                                                                 2
#define BL_COMMS_PIN2_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN3_CTRL0_MASK                                                          0x00000008
#define BL_COMMS_PIN3_CTRL0_OFFSET                                                                 3
#define BL_COMMS_PIN3_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN4_CTRL0_MASK                                                          0x00000010
#define BL_COMMS_PIN4_CTRL0_OFFSET                                                                 4
#define BL_COMMS_PIN4_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN5_CTRL0_MASK                                                          0x00000020
#define BL_COMMS_PIN5_CTRL0_OFFSET                                                                 5
#define BL_COMMS_PIN5_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN6_CTRL0_MASK                                                          0x00000040
#define BL_COMMS_PIN6_CTRL0_OFFSET                                                                 6
#define BL_COMMS_PIN6_CTRL0_WIDTH                                                                  1
#define BL_COMMS_PIN7_CTRL0_MASK                                                          0x00000080
#define BL_COMMS_PIN7_CTRL0_OFFSET                                                                 7
#define BL_COMMS_PIN7_CTRL0_WIDTH                                                                  1
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register BL_COMMS_PIN_CTRL_1(0x8ac)
#define BL_COMMS_PIN_CTRL_1                                                               0x000008ac //RW POD:0x0
#define BL_COMMS_PIN8_CTRL1_MASK                                                          0x00000003
#define BL_COMMS_PIN8_CTRL1_OFFSET                                                                 0
#define BL_COMMS_PIN8_CTRL1_WIDTH                                                                  2
#define BL_COMMS_PIN9_CTRL1_MASK                                                          0x0000000c
#define BL_COMMS_PIN9_CTRL1_OFFSET                                                                 2
#define BL_COMMS_PIN9_CTRL1_WIDTH                                                                  2
#define BL_COMMS_PIN10_CTRL1_MASK                                                         0x00000030
#define BL_COMMS_PIN10_CTRL1_OFFSET                                                                4
#define BL_COMMS_PIN10_CTRL1_WIDTH                                                                 2
#define BL_COMMS_PIN11_CTRL0_MASK                                                         0x00000040
#define BL_COMMS_PIN11_CTRL0_OFFSET                                                                6
#define BL_COMMS_PIN11_CTRL0_WIDTH                                                                 1
#define BL_COMMS_PIN12_CTRL0_MASK                                                         0x00000080
#define BL_COMMS_PIN12_CTRL0_OFFSET                                                                7
#define BL_COMMS_PIN12_CTRL0_WIDTH                                                                 1
#define BL_COMMS_PIN13_CTRL0_MASK                                                         0x00000100
#define BL_COMMS_PIN13_CTRL0_OFFSET                                                                8
#define BL_COMMS_PIN13_CTRL0_WIDTH                                                                 1
#define BL_COMMS_PIN14_CTRL0_MASK                                                         0x00000200
#define BL_COMMS_PIN14_CTRL0_OFFSET                                                                9
#define BL_COMMS_PIN14_CTRL0_WIDTH                                                                 1
#define BL_COMMS_PIN15_CTRL0_MASK                                                         0x00000400
#define BL_COMMS_PIN15_CTRL0_OFFSET                                                               10
#define BL_COMMS_PIN15_CTRL0_WIDTH                                                                 1
// #define Reserved_MASK                                                                     0xfffff800
// #define Reserved_OFFSET                                                                           11
// #define Reserved_WIDTH                                                                            21



// Register BL_COMMS_PIN_I2C_CTRL(0x8b0)
#define BL_COMMS_PIN_I2C_CTRL                                                             0x000008b0 //RW POD:0x0
#define BL_COMMS_PIN0_I2C_CTRL_MASK                                                       0x00000001
#define BL_COMMS_PIN0_I2C_CTRL_OFFSET                                                              0
#define BL_COMMS_PIN0_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN1_I2C_CTRL_MASK                                                       0x00000002
#define BL_COMMS_PIN1_I2C_CTRL_OFFSET                                                              1
#define BL_COMMS_PIN1_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN2_I2C_CTRL_MASK                                                       0x00000004
#define BL_COMMS_PIN2_I2C_CTRL_OFFSET                                                              2
#define BL_COMMS_PIN2_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN3_I2C_CTRL_MASK                                                       0x00000008
#define BL_COMMS_PIN3_I2C_CTRL_OFFSET                                                              3
#define BL_COMMS_PIN3_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN4_I2C_CTRL_MASK                                                       0x00000010
#define BL_COMMS_PIN4_I2C_CTRL_OFFSET                                                              4
#define BL_COMMS_PIN4_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN5_I2C_CTRL_MASK                                                       0x00000020
#define BL_COMMS_PIN5_I2C_CTRL_OFFSET                                                              5
#define BL_COMMS_PIN5_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN6_I2C_CTRL_MASK                                                       0x00000040
#define BL_COMMS_PIN6_I2C_CTRL_OFFSET                                                              6
#define BL_COMMS_PIN6_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN7_I2C_CTRL_MASK                                                       0x00000080
#define BL_COMMS_PIN7_I2C_CTRL_OFFSET                                                              7
#define BL_COMMS_PIN7_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN8_I2C_CTRL_MASK                                                       0x00000100
#define BL_COMMS_PIN8_I2C_CTRL_OFFSET                                                              8
#define BL_COMMS_PIN8_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN9_I2C_CTRL_MASK                                                       0x00000200
#define BL_COMMS_PIN9_I2C_CTRL_OFFSET                                                              9
#define BL_COMMS_PIN9_I2C_CTRL_WIDTH                                                               1
#define BL_COMMS_PIN10_I2C_CTRL_MASK                                                      0x00000400
#define BL_COMMS_PIN10_I2C_CTRL_OFFSET                                                            10
#define BL_COMMS_PIN10_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN11_I2C_CTRL_MASK                                                      0x00000800
#define BL_COMMS_PIN11_I2C_CTRL_OFFSET                                                            11
#define BL_COMMS_PIN11_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN12_I2C_CTRL_MASK                                                      0x00001000
#define BL_COMMS_PIN12_I2C_CTRL_OFFSET                                                            12
#define BL_COMMS_PIN12_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN13_I2C_CTRL_MASK                                                      0x00002000
#define BL_COMMS_PIN13_I2C_CTRL_OFFSET                                                            13
#define BL_COMMS_PIN13_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN14_I2C_CTRL_MASK                                                      0x00004000
#define BL_COMMS_PIN14_I2C_CTRL_OFFSET                                                            14
#define BL_COMMS_PIN14_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN15_I2C_CTRL_MASK                                                      0x00008000
#define BL_COMMS_PIN15_I2C_CTRL_OFFSET                                                            15
#define BL_COMMS_PIN15_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN16_I2C_CTRL_MASK                                                      0x00010000
#define BL_COMMS_PIN16_I2C_CTRL_OFFSET                                                            16
#define BL_COMMS_PIN16_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN17_I2C_CTRL_MASK                                                      0x00020000
#define BL_COMMS_PIN17_I2C_CTRL_OFFSET                                                            17
#define BL_COMMS_PIN17_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN18_I2C_CTRL_MASK                                                      0x00040000
#define BL_COMMS_PIN18_I2C_CTRL_OFFSET                                                            18
#define BL_COMMS_PIN18_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN19_I2C_CTRL_MASK                                                      0x00080000
#define BL_COMMS_PIN19_I2C_CTRL_OFFSET                                                            19
#define BL_COMMS_PIN19_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN20_I2C_CTRL_MASK                                                      0x00100000
#define BL_COMMS_PIN20_I2C_CTRL_OFFSET                                                            20
#define BL_COMMS_PIN20_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN21_I2C_CTRL_MASK                                                      0x00200000
#define BL_COMMS_PIN21_I2C_CTRL_OFFSET                                                            21
#define BL_COMMS_PIN21_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN22_I2C_CTRL_MASK                                                      0x00400000
#define BL_COMMS_PIN22_I2C_CTRL_OFFSET                                                            22
#define BL_COMMS_PIN22_I2C_CTRL_WIDTH                                                              1
#define BL_COMMS_PIN23_I2C_CTRL_MASK                                                      0x00800000
#define BL_COMMS_PIN23_I2C_CTRL_OFFSET                                                            23
#define BL_COMMS_PIN23_I2C_CTRL_WIDTH                                                              1
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_COMMS_MON_ADDR(0x8b4)
#define BL_COMMS_MON_ADDR                                                                 0x000008b4 //RW POD:0x0
#define BL_COMMS_MON_ADDRESS_MASK                                                         0x000001ff
#define BL_COMMS_MON_ADDRESS_OFFSET                                                                0
#define BL_COMMS_MON_ADDRESS_WIDTH                                                                 9
// #define Reserved_MASK                                                                     0xfffffe00
// #define Reserved_OFFSET                                                                            9
// #define Reserved_WIDTH                                                                            23



// Register BL_COMMS_MON_JMP(0x8b8)
#define BL_COMMS_MON_JMP                                                                  0x000008b8 //RO POD:0x0
#define BL_COMMS_MON_JUMP_MASK                                                            0xffffffff
#define BL_COMMS_MON_JUMP_OFFSET                                                                   0
#define BL_COMMS_MON_JUMP_WIDTH                                                                   32



// Register BL_COMMS_MON_NJMP(0x8bc)
#define BL_COMMS_MON_NJMP                                                                 0x000008bc //RO POD:0x0
#define BL_COMMS_MON_NO_JUMP_MASK                                                         0xffffffff
#define BL_COMMS_MON_NO_JUMP_OFFSET                                                                0
#define BL_COMMS_MON_NO_JUMP_WIDTH                                                                32



// Register BL_COMMS_WAIT_STAT(0x8c0)
#define BL_COMMS_WAIT_STAT                                                                0x000008c0 //RO POD:0x0
#define BL_COMMS_WAIT_STATUS_MASK                                                         0x0000ffff
#define BL_COMMS_WAIT_STATUS_OFFSET                                                                0
#define BL_COMMS_WAIT_STATUS_WIDTH                                                                16
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register BL_COMMS_GPIO_OVERRIDE(0x8c4)
#define BL_COMMS_GPIO_OVERRIDE                                                            0x000008c4 //RW POD:0x0
#define BL_GPIO_OVERRIDE_0_MASK                                                           0x00000001
#define BL_GPIO_OVERRIDE_0_OFFSET                                                                  0
#define BL_GPIO_OVERRIDE_0_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_1_MASK                                                           0x00000002
#define BL_GPIO_OVERRIDE_1_OFFSET                                                                  1
#define BL_GPIO_OVERRIDE_1_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_2_MASK                                                           0x00000004
#define BL_GPIO_OVERRIDE_2_OFFSET                                                                  2
#define BL_GPIO_OVERRIDE_2_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_3_MASK                                                           0x00000008
#define BL_GPIO_OVERRIDE_3_OFFSET                                                                  3
#define BL_GPIO_OVERRIDE_3_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_4_MASK                                                           0x00000010
#define BL_GPIO_OVERRIDE_4_OFFSET                                                                  4
#define BL_GPIO_OVERRIDE_4_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_5_MASK                                                           0x00000020
#define BL_GPIO_OVERRIDE_5_OFFSET                                                                  5
#define BL_GPIO_OVERRIDE_5_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_6_MASK                                                           0x00000040
#define BL_GPIO_OVERRIDE_6_OFFSET                                                                  6
#define BL_GPIO_OVERRIDE_6_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_7_MASK                                                           0x00000080
#define BL_GPIO_OVERRIDE_7_OFFSET                                                                  7
#define BL_GPIO_OVERRIDE_7_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_8_MASK                                                           0x00000100
#define BL_GPIO_OVERRIDE_8_OFFSET                                                                  8
#define BL_GPIO_OVERRIDE_8_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_9_MASK                                                           0x00000200
#define BL_GPIO_OVERRIDE_9_OFFSET                                                                  9
#define BL_GPIO_OVERRIDE_9_WIDTH                                                                   1
#define BL_GPIO_OVERRIDE_10_MASK                                                          0x00000400
#define BL_GPIO_OVERRIDE_10_OFFSET                                                                10
#define BL_GPIO_OVERRIDE_10_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_11_MASK                                                          0x00000800
#define BL_GPIO_OVERRIDE_11_OFFSET                                                                11
#define BL_GPIO_OVERRIDE_11_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_12_MASK                                                          0x00001000
#define BL_GPIO_OVERRIDE_12_OFFSET                                                                12
#define BL_GPIO_OVERRIDE_12_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_13_MASK                                                          0x00002000
#define BL_GPIO_OVERRIDE_13_OFFSET                                                                13
#define BL_GPIO_OVERRIDE_13_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_14_MASK                                                          0x00004000
#define BL_GPIO_OVERRIDE_14_OFFSET                                                                14
#define BL_GPIO_OVERRIDE_14_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_15_MASK                                                          0x00008000
#define BL_GPIO_OVERRIDE_15_OFFSET                                                                15
#define BL_GPIO_OVERRIDE_15_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_16_MASK                                                          0x00010000
#define BL_GPIO_OVERRIDE_16_OFFSET                                                                16
#define BL_GPIO_OVERRIDE_16_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_17_MASK                                                          0x00020000
#define BL_GPIO_OVERRIDE_17_OFFSET                                                                17
#define BL_GPIO_OVERRIDE_17_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_18_MASK                                                          0x00040000
#define BL_GPIO_OVERRIDE_18_OFFSET                                                                18
#define BL_GPIO_OVERRIDE_18_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_19_MASK                                                          0x00080000
#define BL_GPIO_OVERRIDE_19_OFFSET                                                                19
#define BL_GPIO_OVERRIDE_19_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_20_MASK                                                          0x00100000
#define BL_GPIO_OVERRIDE_20_OFFSET                                                                20
#define BL_GPIO_OVERRIDE_20_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_21_MASK                                                          0x00200000
#define BL_GPIO_OVERRIDE_21_OFFSET                                                                21
#define BL_GPIO_OVERRIDE_21_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_22_MASK                                                          0x00400000
#define BL_GPIO_OVERRIDE_22_OFFSET                                                                22
#define BL_GPIO_OVERRIDE_22_WIDTH                                                                  1
#define BL_GPIO_OVERRIDE_23_MASK                                                          0x00800000
#define BL_GPIO_OVERRIDE_23_OFFSET                                                                23
#define BL_GPIO_OVERRIDE_23_WIDTH                                                                  1
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_COMMS_GPIO_OE(0x8c8)
#define BL_COMMS_GPIO_OE                                                                  0x000008c8 //RW POD:0x0
#define BL_GPIO_OE_0_MASK                                                                 0x00000001
#define BL_GPIO_OE_0_OFFSET                                                                        0
#define BL_GPIO_OE_0_WIDTH                                                                         1
#define BL_GPIO_OE_1_MASK                                                                 0x00000002
#define BL_GPIO_OE_1_OFFSET                                                                        1
#define BL_GPIO_OE_1_WIDTH                                                                         1
#define BL_GPIO_OE_2_MASK                                                                 0x00000004
#define BL_GPIO_OE_2_OFFSET                                                                        2
#define BL_GPIO_OE_2_WIDTH                                                                         1
#define BL_GPIO_OE_3_MASK                                                                 0x00000008
#define BL_GPIO_OE_3_OFFSET                                                                        3
#define BL_GPIO_OE_3_WIDTH                                                                         1
#define BL_GPIO_OE_4_MASK                                                                 0x00000010
#define BL_GPIO_OE_4_OFFSET                                                                        4
#define BL_GPIO_OE_4_WIDTH                                                                         1
#define BL_GPIO_OE_5_MASK                                                                 0x00000020
#define BL_GPIO_OE_5_OFFSET                                                                        5
#define BL_GPIO_OE_5_WIDTH                                                                         1
#define BL_GPIO_OE_6_MASK                                                                 0x00000040
#define BL_GPIO_OE_6_OFFSET                                                                        6
#define BL_GPIO_OE_6_WIDTH                                                                         1
#define BL_GPIO_OE_7_MASK                                                                 0x00000080
#define BL_GPIO_OE_7_OFFSET                                                                        7
#define BL_GPIO_OE_7_WIDTH                                                                         1
#define BL_GPIO_OE_8_MASK                                                                 0x00000100
#define BL_GPIO_OE_8_OFFSET                                                                        8
#define BL_GPIO_OE_8_WIDTH                                                                         1
#define BL_GPIO_OE_9_MASK                                                                 0x00000200
#define BL_GPIO_OE_9_OFFSET                                                                        9
#define BL_GPIO_OE_9_WIDTH                                                                         1
#define BL_GPIO_OE_10_MASK                                                                0x00000400
#define BL_GPIO_OE_10_OFFSET                                                                      10
#define BL_GPIO_OE_10_WIDTH                                                                        1
#define BL_GPIO_OE_11_MASK                                                                0x00000800
#define BL_GPIO_OE_11_OFFSET                                                                      11
#define BL_GPIO_OE_11_WIDTH                                                                        1
#define BL_GPIO_OE_12_MASK                                                                0x00001000
#define BL_GPIO_OE_12_OFFSET                                                                      12
#define BL_GPIO_OE_12_WIDTH                                                                        1
#define BL_GPIO_OE_13_MASK                                                                0x00002000
#define BL_GPIO_OE_13_OFFSET                                                                      13
#define BL_GPIO_OE_13_WIDTH                                                                        1
#define BL_GPIO_OE_14_MASK                                                                0x00004000
#define BL_GPIO_OE_14_OFFSET                                                                      14
#define BL_GPIO_OE_14_WIDTH                                                                        1
#define BL_GPIO_OE_15_MASK                                                                0x00008000
#define BL_GPIO_OE_15_OFFSET                                                                      15
#define BL_GPIO_OE_15_WIDTH                                                                        1
#define BL_GPIO_OE_16_MASK                                                                0x00010000
#define BL_GPIO_OE_16_OFFSET                                                                      16
#define BL_GPIO_OE_16_WIDTH                                                                        1
#define BL_GPIO_OE_17_MASK                                                                0x00020000
#define BL_GPIO_OE_17_OFFSET                                                                      17
#define BL_GPIO_OE_17_WIDTH                                                                        1
#define BL_GPIO_OE_18_MASK                                                                0x00040000
#define BL_GPIO_OE_18_OFFSET                                                                      18
#define BL_GPIO_OE_18_WIDTH                                                                        1
#define BL_GPIO_OE_19_MASK                                                                0x00080000
#define BL_GPIO_OE_19_OFFSET                                                                      19
#define BL_GPIO_OE_19_WIDTH                                                                        1
#define BL_GPIO_OE_20_MASK                                                                0x00100000
#define BL_GPIO_OE_20_OFFSET                                                                      20
#define BL_GPIO_OE_20_WIDTH                                                                        1
#define BL_GPIO_OE_21_MASK                                                                0x00200000
#define BL_GPIO_OE_21_OFFSET                                                                      21
#define BL_GPIO_OE_21_WIDTH                                                                        1
#define BL_GPIO_OE_22_MASK                                                                0x00400000
#define BL_GPIO_OE_22_OFFSET                                                                      22
#define BL_GPIO_OE_22_WIDTH                                                                        1
#define BL_GPIO_OE_23_MASK                                                                0x00800000
#define BL_GPIO_OE_23_OFFSET                                                                      23
#define BL_GPIO_OE_23_WIDTH                                                                        1
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_COMMS_GPIO_OUT(0x8cc)
#define BL_COMMS_GPIO_OUT                                                                 0x000008cc //RW POD:0x0
#define BL_GPIO_OUT_0_MASK                                                                0x00000001
#define BL_GPIO_OUT_0_OFFSET                                                                       0
#define BL_GPIO_OUT_0_WIDTH                                                                        1
#define BL_GPIO_OUT_1_MASK                                                                0x00000002
#define BL_GPIO_OUT_1_OFFSET                                                                       1
#define BL_GPIO_OUT_1_WIDTH                                                                        1
#define BL_GPIO_OUT_2_MASK                                                                0x00000004
#define BL_GPIO_OUT_2_OFFSET                                                                       2
#define BL_GPIO_OUT_2_WIDTH                                                                        1
#define BL_GPIO_OUT_3_MASK                                                                0x00000008
#define BL_GPIO_OUT_3_OFFSET                                                                       3
#define BL_GPIO_OUT_3_WIDTH                                                                        1
#define BL_GPIO_OUT_4_MASK                                                                0x00000010
#define BL_GPIO_OUT_4_OFFSET                                                                       4
#define BL_GPIO_OUT_4_WIDTH                                                                        1
#define BL_GPIO_OUT_5_MASK                                                                0x00000020
#define BL_GPIO_OUT_5_OFFSET                                                                       5
#define BL_GPIO_OUT_5_WIDTH                                                                        1
#define BL_GPIO_OUT_6_MASK                                                                0x00000040
#define BL_GPIO_OUT_6_OFFSET                                                                       6
#define BL_GPIO_OUT_6_WIDTH                                                                        1
#define BL_GPIO_OUT_7_MASK                                                                0x00000080
#define BL_GPIO_OUT_7_OFFSET                                                                       7
#define BL_GPIO_OUT_7_WIDTH                                                                        1
#define BL_GPIO_OUT_8_MASK                                                                0x00000100
#define BL_GPIO_OUT_8_OFFSET                                                                       8
#define BL_GPIO_OUT_8_WIDTH                                                                        1
#define BL_GPIO_OUT_9_MASK                                                                0x00000200
#define BL_GPIO_OUT_9_OFFSET                                                                       9
#define BL_GPIO_OUT_9_WIDTH                                                                        1
#define BL_GPIO_OUT_10_MASK                                                               0x00000400
#define BL_GPIO_OUT_10_OFFSET                                                                     10
#define BL_GPIO_OUT_10_WIDTH                                                                       1
#define BL_GPIO_OUT_11_MASK                                                               0x00000800
#define BL_GPIO_OUT_11_OFFSET                                                                     11
#define BL_GPIO_OUT_11_WIDTH                                                                       1
#define BL_GPIO_OUT_12_MASK                                                               0x00001000
#define BL_GPIO_OUT_12_OFFSET                                                                     12
#define BL_GPIO_OUT_12_WIDTH                                                                       1
#define BL_GPIO_OUT_13_MASK                                                               0x00002000
#define BL_GPIO_OUT_13_OFFSET                                                                     13
#define BL_GPIO_OUT_13_WIDTH                                                                       1
#define BL_GPIO_OUT_14_MASK                                                               0x00004000
#define BL_GPIO_OUT_14_OFFSET                                                                     14
#define BL_GPIO_OUT_14_WIDTH                                                                       1
#define BL_GPIO_OUT_15_MASK                                                               0x00008000
#define BL_GPIO_OUT_15_OFFSET                                                                     15
#define BL_GPIO_OUT_15_WIDTH                                                                       1
#define BL_GPIO_OUT_16_MASK                                                               0x00010000
#define BL_GPIO_OUT_16_OFFSET                                                                     16
#define BL_GPIO_OUT_16_WIDTH                                                                       1
#define BL_GPIO_OUT_17_MASK                                                               0x00020000
#define BL_GPIO_OUT_17_OFFSET                                                                     17
#define BL_GPIO_OUT_17_WIDTH                                                                       1
#define BL_GPIO_OUT_18_MASK                                                               0x00040000
#define BL_GPIO_OUT_18_OFFSET                                                                     18
#define BL_GPIO_OUT_18_WIDTH                                                                       1
#define BL_GPIO_OUT_19_MASK                                                               0x00080000
#define BL_GPIO_OUT_19_OFFSET                                                                     19
#define BL_GPIO_OUT_19_WIDTH                                                                       1
#define BL_GPIO_OUT_20_MASK                                                               0x00100000
#define BL_GPIO_OUT_20_OFFSET                                                                     20
#define BL_GPIO_OUT_20_WIDTH                                                                       1
#define BL_GPIO_OUT_21_MASK                                                               0x00200000
#define BL_GPIO_OUT_21_OFFSET                                                                     21
#define BL_GPIO_OUT_21_WIDTH                                                                       1
#define BL_GPIO_OUT_22_MASK                                                               0x00400000
#define BL_GPIO_OUT_22_OFFSET                                                                     22
#define BL_GPIO_OUT_22_WIDTH                                                                       1
#define BL_GPIO_OUT_23_MASK                                                               0x00800000
#define BL_GPIO_OUT_23_OFFSET                                                                     23
#define BL_GPIO_OUT_23_WIDTH                                                                       1
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register BL_COMMS_GPIO_IN(0x8d0)
#define BL_COMMS_GPIO_IN                                                                  0x000008d0 //RO POD:0x0
#define BL_GPIO_IN_0_MASK                                                                 0x00000001
#define BL_GPIO_IN_0_OFFSET                                                                        0
#define BL_GPIO_IN_0_WIDTH                                                                         1
#define BL_GPIO_IN_1_MASK                                                                 0x00000002
#define BL_GPIO_IN_1_OFFSET                                                                        1
#define BL_GPIO_IN_1_WIDTH                                                                         1
#define BL_GPIO_IN_2_MASK                                                                 0x00000004
#define BL_GPIO_IN_2_OFFSET                                                                        2
#define BL_GPIO_IN_2_WIDTH                                                                         1
#define BL_GPIO_IN_3_MASK                                                                 0x00000008
#define BL_GPIO_IN_3_OFFSET                                                                        3
#define BL_GPIO_IN_3_WIDTH                                                                         1
#define BL_GPIO_IN_4_MASK                                                                 0x00000010
#define BL_GPIO_IN_4_OFFSET                                                                        4
#define BL_GPIO_IN_4_WIDTH                                                                         1
#define BL_GPIO_IN_5_MASK                                                                 0x00000020
#define BL_GPIO_IN_5_OFFSET                                                                        5
#define BL_GPIO_IN_5_WIDTH                                                                         1
#define BL_GPIO_IN_6_MASK                                                                 0x00000040
#define BL_GPIO_IN_6_OFFSET                                                                        6
#define BL_GPIO_IN_6_WIDTH                                                                         1
#define BL_GPIO_IN_7_MASK                                                                 0x00000080
#define BL_GPIO_IN_7_OFFSET                                                                        7
#define BL_GPIO_IN_7_WIDTH                                                                         1
#define BL_GPIO_IN_8_MASK                                                                 0x00000100
#define BL_GPIO_IN_8_OFFSET                                                                        8
#define BL_GPIO_IN_8_WIDTH                                                                         1
#define BL_GPIO_IN_9_MASK                                                                 0x00000200
#define BL_GPIO_IN_9_OFFSET                                                                        9
#define BL_GPIO_IN_9_WIDTH                                                                         1
#define BL_GPIO_IN_10_MASK                                                                0x00000400
#define BL_GPIO_IN_10_OFFSET                                                                      10
#define BL_GPIO_IN_10_WIDTH                                                                        1
#define BL_GPIO_IN_11_MASK                                                                0x00000800
#define BL_GPIO_IN_11_OFFSET                                                                      11
#define BL_GPIO_IN_11_WIDTH                                                                        1
#define BL_GPIO_IN_12_MASK                                                                0x00001000
#define BL_GPIO_IN_12_OFFSET                                                                      12
#define BL_GPIO_IN_12_WIDTH                                                                        1
#define BL_GPIO_IN_13_MASK                                                                0x00002000
#define BL_GPIO_IN_13_OFFSET                                                                      13
#define BL_GPIO_IN_13_WIDTH                                                                        1
#define BL_GPIO_IN_14_MASK                                                                0x00004000
#define BL_GPIO_IN_14_OFFSET                                                                      14
#define BL_GPIO_IN_14_WIDTH                                                                        1
#define BL_GPIO_IN_15_MASK                                                                0x00008000
#define BL_GPIO_IN_15_OFFSET                                                                      15
#define BL_GPIO_IN_15_WIDTH                                                                        1
#define BL_GPIO_IN_16_MASK                                                                0x00010000
#define BL_GPIO_IN_16_OFFSET                                                                      16
#define BL_GPIO_IN_16_WIDTH                                                                        1
#define BL_GPIO_IN_17_MASK                                                                0x00020000
#define BL_GPIO_IN_17_OFFSET                                                                      17
#define BL_GPIO_IN_17_WIDTH                                                                        1
#define BL_GPIO_IN_18_MASK                                                                0x00040000
#define BL_GPIO_IN_18_OFFSET                                                                      18
#define BL_GPIO_IN_18_WIDTH                                                                        1
#define BL_GPIO_IN_19_MASK                                                                0x00080000
#define BL_GPIO_IN_19_OFFSET                                                                      19
#define BL_GPIO_IN_19_WIDTH                                                                        1
#define BL_GPIO_IN_20_MASK                                                                0x00100000
#define BL_GPIO_IN_20_OFFSET                                                                      20
#define BL_GPIO_IN_20_WIDTH                                                                        1
#define BL_GPIO_IN_21_MASK                                                                0x00200000
#define BL_GPIO_IN_21_OFFSET                                                                      21
#define BL_GPIO_IN_21_WIDTH                                                                        1
#define BL_GPIO_IN_22_MASK                                                                0x00400000
#define BL_GPIO_IN_22_OFFSET                                                                      22
#define BL_GPIO_IN_22_WIDTH                                                                        1
#define BL_GPIO_IN_23_MASK                                                                0x00800000
#define BL_GPIO_IN_23_OFFSET                                                                      23
#define BL_GPIO_IN_23_WIDTH                                                                        1
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8






// address block VOUT [0x900]


# define VOUT_BASE_ADDRESS 0x900


// Register VOUT_REVISION(0x900)
#define VOUT_REVISION                                                                     0x00000900 //RO POD:0x300
#define VOUT_SUB_REVISION_MASK                                                            0x0000000f
#define VOUT_SUB_REVISION_OFFSET                                                                   0
#define VOUT_SUB_REVISION_WIDTH                                                                    4
#define VOUT_MINOR_REVISION_MASK                                                          0x000000f0
#define VOUT_MINOR_REVISION_OFFSET                                                                 4
#define VOUT_MINOR_REVISION_WIDTH                                                                  4
#define VOUT_MAJOR_REVISION_MASK                                                          0x00000f00
#define VOUT_MAJOR_REVISION_OFFSET                                                                 8
#define VOUT_MAJOR_REVISION_WIDTH                                                                  4
// #define RESERVED_MASK                                                                     0xfffff000
// #define RESERVED_OFFSET                                                                           12
// #define RESERVED_WIDTH                                                                            20



// Register VOUT_TESTBUS_CTRL(0x904)
#define VOUT_TESTBUS_CTRL                                                                 0x00000904 //RW POD:0x0
#define VOUT_TESTBUS_A_SEL_MASK                                                           0x000000ff
#define VOUT_TESTBUS_A_SEL_OFFSET                                                                  0
#define VOUT_TESTBUS_A_SEL_WIDTH                                                                   8
#define VOUT_TESTBUS_A_DEBUG_MASK                                                         0x0000ff00
#define VOUT_TESTBUS_A_DEBUG_OFFSET                                                                8
#define VOUT_TESTBUS_A_DEBUG_WIDTH                                                                 8
#define VOUT_TESTBUS_B_SEL_MASK                                                           0x00ff0000
#define VOUT_TESTBUS_B_SEL_OFFSET                                                                 16
#define VOUT_TESTBUS_B_SEL_WIDTH                                                                   8
#define VOUT_TESTBUS_B_DEBUG_MASK                                                         0xff000000
#define VOUT_TESTBUS_B_DEBUG_OFFSET                                                               24
#define VOUT_TESTBUS_B_DEBUG_WIDTH                                                                 8



// Register VOUT_TESTBUS_VAL(0x908)
#define VOUT_TESTBUS_VAL                                                                  0x00000908 //RO POD:0x0
#define VOUT_TESTBUS_A_VAL_MASK                                                           0x000000ff
#define VOUT_TESTBUS_A_VAL_OFFSET                                                                  0
#define VOUT_TESTBUS_A_VAL_WIDTH                                                                   8
// #define RESERVED_MASK                                                                     0x0000ff00
// #define RESERVED_OFFSET                                                                            8
// #define RESERVED_WIDTH                                                                             8
#define VOUT_TESTBUS_B_VAL_MASK                                                           0x00ff0000
#define VOUT_TESTBUS_B_VAL_OFFSET                                                                 16
#define VOUT_TESTBUS_B_VAL_WIDTH                                                                   8
// #define RESERVED_MASK                                                                     0xff000000
// #define RESERVED_OFFSET                                                                           24
// #define RESERVED_WIDTH                                                                             8



// Register VOUT_FIELD_ID(0x90c)
#define VOUT_FIELD_ID                                                                     0x0000090c //RW POD:0x0
#define VOUT_FIELD_ID_MASK                                                                0x00000001
#define VOUT_FIELD_ID_OFFSET                                                                       0
#define VOUT_FIELD_ID_WIDTH                                                                        1
// #define RESERVED_MASK                                                                     0xfffffffe
// #define RESERVED_OFFSET                                                                            1
// #define RESERVED_WIDTH                                                                            31






// address block VOUT_SPARE [0x30000]


# define VOUT_SPARE_BASE_ADDRESS 0x30000


// Register VOUT_SPARE_T1_REG1(0x30000)
#define VOUT_SPARE_T1_REG1                                                                0x00030000 //RW POD:0x0
#define PIO_5_SEL_ALT_OEN_MASK                                                            0x000000ff
#define PIO_5_SEL_ALT_OEN_OFFSET                                                                   0
#define PIO_5_SEL_ALT_OEN_WIDTH                                                                    8
#define PIO_6_SEL_ALT_OEN_MASK                                                            0x0000ff00
#define PIO_6_SEL_ALT_OEN_OFFSET                                                                   8
#define PIO_6_SEL_ALT_OEN_WIDTH                                                                    8
#define PIO_7_SEL_ALT_OEN_MASK                                                            0x00ff0000
#define PIO_7_SEL_ALT_OEN_OFFSET                                                                  16
#define PIO_7_SEL_ALT_OEN_WIDTH                                                                    8
#define PIO_11_SEL_ALT_OEN_MASK                                                           0xff000000
#define PIO_11_SEL_ALT_OEN_OFFSET                                                                 24
#define PIO_11_SEL_ALT_OEN_WIDTH                                                                   8



// Register VOUT_SPARE_T1_REG2(0x30004)
#define VOUT_SPARE_T1_REG2                                                                0x00030004 //RW POD:0x0
#define PIO_11_ALT_OEN_MASK                                                               0x000000ff
#define PIO_11_ALT_OEN_OFFSET                                                                      0
#define PIO_11_ALT_OEN_WIDTH                                                                       8
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register VOUT_SPARE_T1_REG3(0x30008)
#define VOUT_SPARE_T1_REG3                                                                0x00030008 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE_T1_REG4(0x3000c)
#define VOUT_SPARE_T1_REG4                                                                0x0003000c //RW POD:0x0
#define DPTX_OSC_CONTROL_MASK                                                             0x00000001
#define DPTX_OSC_CONTROL_OFFSET                                                                    0
#define DPTX_OSC_CONTROL_WIDTH                                                                     1
// #define Reserved_MASK                                                                     0xfffffffe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                            31



// Register VOUT_SPARE_T1_REG5(0x30010)
#define VOUT_SPARE_T1_REG5                                                                0x00030010 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE_T1_REG6(0x30014)
#define VOUT_SPARE_T1_REG6                                                                0x00030014 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE_T1_REG7(0x30018)
#define VOUT_SPARE_T1_REG7                                                                0x00030018 //RO POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE_T1_REG8(0x3001c)
#define VOUT_SPARE_T1_REG8                                                                0x0003001c //RW POD:0x0
// #define Reserved_MASK                                                                     0x00000001
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                     0x00000002
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                     0x0000fffc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                            14
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16



// Register VOUT_SPARE2_T1_REG1(0x38000)
#define VOUT_SPARE2_T1_REG1                                                               0x00038000 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG2(0x38004)
#define VOUT_SPARE2_T1_REG2                                                               0x00038004 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG3(0x38008)
#define VOUT_SPARE2_T1_REG3                                                               0x00038008 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG4(0x3800c)
#define VOUT_SPARE2_T1_REG4                                                               0x0003800c //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG5(0x38010)
#define VOUT_SPARE2_T1_REG5                                                               0x00038010 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG6(0x38014)
#define VOUT_SPARE2_T1_REG6                                                               0x00038014 //RW POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG7(0x38018)
#define VOUT_SPARE2_T1_REG7                                                               0x00038018 //RO POD:0x0
// #define Reserved_MASK                                                                     0xffffffff
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                            32



// Register VOUT_SPARE2_T1_REG8(0x3801c)
#define VOUT_SPARE2_T1_REG8                                                               0x0003801c //RW POD:0x0
// #define Reserved_MASK                                                                     0x00000001
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                     0x00000002
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                     0x0000fffc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                            14
// #define Reserved_MASK                                                                     0xffff0000
// #define Reserved_OFFSET                                                                           16
// #define Reserved_WIDTH                                                                            16





#endif /* _MPE41_ODP_REGS_ */
