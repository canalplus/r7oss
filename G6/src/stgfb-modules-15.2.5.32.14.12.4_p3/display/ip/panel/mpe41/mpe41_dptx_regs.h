/***********************************************************************
 *
 * File: display/ip/panel/mpe41/mpe41_dptx_regs.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
/*Generated on Wed Jan 04 19:21:08 EST 2012 using DocumentProcessor 2.8.2.eng */
#ifndef _MPE41_DPTX_REGS_
#define _MPE41_DPTX_REGS_



// address block DPTX_DIGITAL_TOP [0x0]


# define DPTX_DIGITAL_TOP_BASE_ADDRESS 0x0


// Register DPTX_MISC_CTRL(0x0)
#define DPTX_MISC_CTRL                                                                    0x00000000 //RW POD:0x1
#define DPTX_PD_AUX_REFCLK_MASK                                                                 0x01
#define DPTX_PD_AUX_REFCLK_OFFSET                                                                  0
#define DPTX_PD_AUX_REFCLK_WIDTH                                                                   1
#define DPTX_RAM_POWER_DOWN_MASK                                                                0x02
#define DPTX_RAM_POWER_DOWN_OFFSET                                                                 1
#define DPTX_RAM_POWER_DOWN_WIDTH                                                                  1
#define DPTX_TEST_IRQ_EN_MASK                                                                   0x04
#define DPTX_TEST_IRQ_EN_OFFSET                                                                    2
#define DPTX_TEST_IRQ_EN_WIDTH                                                                     1
#define DPTX_HPD0_SELECT_MASK                                                                   0x18
#define DPTX_HPD0_SELECT_OFFSET                                                                    3
#define DPTX_HPD0_SELECT_WIDTH                                                                     2
#define DPTX_HPD1_SELECT_MASK                                                                   0x60
#define DPTX_HPD1_SELECT_OFFSET                                                                    5
#define DPTX_HPD1_SELECT_WIDTH                                                                     2
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX_IN_SEL(0x4)
#define DPTX_IN_SEL                                                                       0x00000004 //RW POD:0x0
#define DPTX0_IN_EVN_ODD_SWAP_MASK                                                              0x01
#define DPTX0_IN_EVN_ODD_SWAP_OFFSET                                                               0
#define DPTX0_IN_EVN_ODD_SWAP_WIDTH                                                                1
#define DPTX1_IN_EVN_ODD_SWAP_MASK                                                              0x02
#define DPTX1_IN_EVN_ODD_SWAP_OFFSET                                                               1
#define DPTX1_IN_EVN_ODD_SWAP_WIDTH                                                                1
#define DPTX0_IN_RB_SWAP_MASK                                                                   0x04
#define DPTX0_IN_RB_SWAP_OFFSET                                                                    2
#define DPTX0_IN_RB_SWAP_WIDTH                                                                     1
#define DPTX1_IN_RB_SWAP_MASK                                                                   0x08
#define DPTX1_IN_RB_SWAP_OFFSET                                                                    3
#define DPTX1_IN_RB_SWAP_WIDTH                                                                     1
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX_IRQ_STATUS(0x8)
#define DPTX_IRQ_STATUS                                                                   0x00000008 //RO POD:0x0
#define DPTX0_IRQ_STS_MASK                                                                      0x01
#define DPTX0_IRQ_STS_OFFSET                                                                       0
#define DPTX0_IRQ_STS_WIDTH                                                                        1
#define DPTX1_IRQ_STS_MASK                                                                      0x02
#define DPTX1_IRQ_STS_OFFSET                                                                       1
#define DPTX1_IRQ_STS_WIDTH                                                                        1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX_TEST_BUS_CTRL(0xc)
#define DPTX_TEST_BUS_CTRL                                                                0x0000000c //RW POD:0x0
#define DPTX_TEST_BUS_SEL_MASK                                                                  0x07
#define DPTX_TEST_BUS_SEL_OFFSET                                                                   0
#define DPTX_TEST_BUS_SEL_WIDTH                                                                    3
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5






// address block DPTX0_TOP [0x10]


# define DPTX0_TOP_BASE_ADDRESS 0x10


// Register DPTX0_CONTROL(0x10)
#define DPTX0_CONTROL                                                                     0x00000010 //RW POD:0x0
#define DPTX0_EN_MASK                                                                           0x01
#define DPTX0_EN_OFFSET                                                                            0
#define DPTX0_EN_WIDTH                                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX0_RESET_CTRL(0x14)
#define DPTX0_RESET_CTRL                                                                  0x00000014 //CRW POD:0xf
#define DPTX0_RESET_AUX_CLK_MASK                                                                0x01
#define DPTX0_RESET_AUX_CLK_OFFSET                                                                 0
#define DPTX0_RESET_AUX_CLK_WIDTH                                                                  1
#define DPTX0_RESET_LINK_CLK_MASK                                                               0x02
#define DPTX0_RESET_LINK_CLK_OFFSET                                                                1
#define DPTX0_RESET_LINK_CLK_WIDTH                                                                 1
#define DPTX0_RESET_VIDEO_CLK_MASK                                                              0x04
#define DPTX0_RESET_VIDEO_CLK_OFFSET                                                               2
#define DPTX0_RESET_VIDEO_CLK_WIDTH                                                                1
#define DPTX0_RESET_TCLK_CLK_MASK                                                               0x08
#define DPTX0_RESET_TCLK_CLK_OFFSET                                                                3
#define DPTX0_RESET_TCLK_CLK_WIDTH                                                                 1
#define DPTX0_RESET_UART_CLK_MASK                                                               0x10
#define DPTX0_RESET_UART_CLK_OFFSET                                                                4
#define DPTX0_RESET_UART_CLK_WIDTH                                                                 1
#define DPTX0_RESET_RESERV_MASK                                                                 0xe0
#define DPTX0_RESET_RESERV_OFFSET                                                                  5
#define DPTX0_RESET_RESERV_WIDTH                                                                   3



// Register DPTX0_IRQ_STATUS(0x18)
#define DPTX0_IRQ_STATUS                                                                  0x00000018 //RO POD:0x0
#define DPTX0_AUXCH_IRQ_STS_MASK                                                                0x01
#define DPTX0_AUXCH_IRQ_STS_OFFSET                                                                 0
#define DPTX0_AUXCH_IRQ_STS_WIDTH                                                                  1
#define DPTX0_LT_IRQ_STS_MASK                                                                   0x02
#define DPTX0_LT_IRQ_STS_OFFSET                                                                    1
#define DPTX0_LT_IRQ_STS_WIDTH                                                                     1
#define DPTX0_MS_IRQ_STS_MASK                                                                   0x04
#define DPTX0_MS_IRQ_STS_OFFSET                                                                    2
#define DPTX0_MS_IRQ_STS_WIDTH                                                                     1
#define DPTX0_HDCP_IRQ_STS_MASK                                                                 0x08
#define DPTX0_HDCP_IRQ_STS_OFFSET                                                                  3
#define DPTX0_HDCP_IRQ_STS_WIDTH                                                                   1
#define DPTX0_HPD_IN_IRQ_STS_MASK                                                               0x10
#define DPTX0_HPD_IN_IRQ_STS_OFFSET                                                                4
#define DPTX0_HPD_IN_IRQ_STS_WIDTH                                                                 1
#define DPTX0_AUX_UART_IRQ_STS_MASK                                                             0x20
#define DPTX0_AUX_UART_IRQ_STS_OFFSET                                                              5
#define DPTX0_AUX_UART_IRQ_STS_WIDTH                                                               1
#define DPTX0_PHYA_IRQ_STS_MASK                                                                 0x40
#define DPTX0_PHYA_IRQ_STS_OFFSET                                                                  6
#define DPTX0_PHYA_IRQ_STS_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX0_HPD_DETECTOR_CONTROL(0x20)
#define DPTX0_HPD_DETECTOR_CONTROL                                                        0x00000020 //RW POD:0x9b
#define DPTX0_HPD_CLK_DIV_MASK                                                                  0x7f
#define DPTX0_HPD_CLK_DIV_OFFSET                                                                   0
#define DPTX0_HPD_CLK_DIV_WIDTH                                                                    7
#define DPTX0_HPD_DEGLITCH_EN_MASK                                                              0x80
#define DPTX0_HPD_DEGLITCH_EN_OFFSET                                                               7
#define DPTX0_HPD_DEGLITCH_EN_WIDTH                                                                1



// Register DPTX0_HPD_PULSE_LEVEL(0x24)
#define DPTX0_HPD_PULSE_LEVEL                                                             0x00000024 //RW POD:0x4
#define DPTX0_HPD_PULSE_LEVEL_MASK                                                              0xff
#define DPTX0_HPD_PULSE_LEVEL_OFFSET                                                               0
#define DPTX0_HPD_PULSE_LEVEL_WIDTH                                                                8



// Register DPTX0_HPD_UNPLUG_LEVEL(0x28)
#define DPTX0_HPD_UNPLUG_LEVEL                                                            0x00000028 //RW POD:0xa0
#define DPTX0_HPD_UNPLUG_LEVEL_MASK                                                             0xff
#define DPTX0_HPD_UNPLUG_LEVEL_OFFSET                                                              0
#define DPTX0_HPD_UNPLUG_LEVEL_WIDTH                                                               8



// Register DPTX0_HPD_IRQ_CTRL(0x2c)
#define DPTX0_HPD_IRQ_CTRL                                                                0x0000002c //RW POD:0x0
#define DPTX0_HPD_PULSE_IRQ_EN_MASK                                                             0x01
#define DPTX0_HPD_PULSE_IRQ_EN_OFFSET                                                              0
#define DPTX0_HPD_PULSE_IRQ_EN_WIDTH                                                               1
#define DPTX0_HPD_UNPLUG_IRQ_EN_MASK                                                            0x02
#define DPTX0_HPD_UNPLUG_IRQ_EN_OFFSET                                                             1
#define DPTX0_HPD_UNPLUG_IRQ_EN_WIDTH                                                              1
#define DPTX0_HPD_PLUG_IRQ_EN_MASK                                                              0x04
#define DPTX0_HPD_PLUG_IRQ_EN_OFFSET                                                               2
#define DPTX0_HPD_PLUG_IRQ_EN_WIDTH                                                                1
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_HPD_IRQ_STATUS(0x30)
#define DPTX0_HPD_IRQ_STATUS                                                              0x00000030 //CRO POD:0x0
#define DPTX0_HPD_PULSE_IRQ_STS_MASK                                                            0x01
#define DPTX0_HPD_PULSE_IRQ_STS_OFFSET                                                             0
#define DPTX0_HPD_PULSE_IRQ_STS_WIDTH                                                              1
#define DPTX0_HPD_UNPLUG_IRQ_STS_MASK                                                           0x02
#define DPTX0_HPD_UNPLUG_IRQ_STS_OFFSET                                                            1
#define DPTX0_HPD_UNPLUG_IRQ_STS_WIDTH                                                             1
#define DPTX0_HPD_PLUG_IRQ_STS_MASK                                                             0x04
#define DPTX0_HPD_PLUG_IRQ_STS_OFFSET                                                              2
#define DPTX0_HPD_PLUG_IRQ_STS_WIDTH                                                               1
// #define Reserved_MASK                                                                           0x78
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             4
#define DPTX0_HPD_RAW_STATUS_MASK                                                               0x80
#define DPTX0_HPD_RAW_STATUS_OFFSET                                                                7
#define DPTX0_HPD_RAW_STATUS_WIDTH                                                                 1



// Register DPTX0_HPD_IN_DBNC_TIME(0x34)
#define DPTX0_HPD_IN_DBNC_TIME                                                            0x00000034 //RW POD:0x3
#define DPTX0_HPD_IN_DBNC_TIME_MASK                                                             0x07
#define DPTX0_HPD_IN_DBNC_TIME_OFFSET                                                              0
#define DPTX0_HPD_IN_DBNC_TIME_WIDTH                                                               3
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_AUXREFCLK_DIV(0x38)
#define DPTX0_AUXREFCLK_DIV                                                               0x00000038 //RW POD:0x6c
#define DPTX0_AUXREFCLK_DIV_MASK                                                                0xff
#define DPTX0_AUXREFCLK_DIV_OFFSET                                                                 0
#define DPTX0_AUXREFCLK_DIV_WIDTH                                                                  8



// Register DPTX0_TEST_CTRL(0x50)
#define DPTX0_TEST_CTRL                                                                   0x00000050 //RW POD:0x0
// #define Reserved_MASK                                                                     0x00000001
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                             1
#define DPTX0_PHYD_IN_SEL_MASK                                                            0x00000002
#define DPTX0_PHYD_IN_SEL_OFFSET                                                                   1
#define DPTX0_PHYD_IN_SEL_WIDTH                                                                    1
#define DPTX0_PHYA_IN_SEL_MASK                                                            0x00000004
#define DPTX0_PHYA_IN_SEL_OFFSET                                                                   2
#define DPTX0_PHYA_IN_SEL_WIDTH                                                                    1
#define DPTX0_AUX_TEST_IN_EN_MASK                                                         0x00000008
#define DPTX0_AUX_TEST_IN_EN_OFFSET                                                                3
#define DPTX0_AUX_TEST_IN_EN_WIDTH                                                                 1
#define DPTX0_HPD_LOOPBACK_EN_MASK                                                        0x00000010
#define DPTX0_HPD_LOOPBACK_EN_OFFSET                                                               4
#define DPTX0_HPD_LOOPBACK_EN_WIDTH                                                                1
#define DPTX0_TEST_CTRL_RESRV_MASK                                                        0x000000e0
#define DPTX0_TEST_CTRL_RESRV_OFFSET                                                               5
#define DPTX0_TEST_CTRL_RESRV_WIDTH                                                                3
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register DPTX0_TEST_BUS_SEL(0x54)
#define DPTX0_TEST_BUS_SEL                                                                0x00000054 //RW POD:0x0
#define DPTX0_TEST_BLOCK_SEL_MASK                                                         0x00000007
#define DPTX0_TEST_BLOCK_SEL_OFFSET                                                                0
#define DPTX0_TEST_BLOCK_SEL_WIDTH                                                                 3
#define DPTX0_TEST_BUS_SEL_MASK                                                           0x000000f8
#define DPTX0_TEST_BUS_SEL_OFFSET                                                                  3
#define DPTX0_TEST_BUS_SEL_WIDTH                                                                   5
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register DPTX0_TEST_BUS_SEL1(0x58)
#define DPTX0_TEST_BUS_SEL1                                                               0x00000058 //RW POD:0x0
#define DPTX0_TEST_BLOCK_SEL1_MASK                                                        0x00000007
#define DPTX0_TEST_BLOCK_SEL1_OFFSET                                                               0
#define DPTX0_TEST_BLOCK_SEL1_WIDTH                                                                3
#define DPTX0_TEST_BUS_SEL1_MASK                                                          0x000000f8
#define DPTX0_TEST_BUS_SEL1_OFFSET                                                                 3
#define DPTX0_TEST_BUS_SEL1_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24






// address block DPTX0_PHYA [0x60]


# define DPTX0_PHYA_BASE_ADDRESS 0x60


// Register DPTX0_AUX_PHY_CTRL(0x60)
#define DPTX0_AUX_PHY_CTRL                                                                0x00000060 //RW POD:0x3
// #define Reserved_MASK                                                                         0x0001
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                             1
#define DPTX0_TXAUX_RXPD_MASK                                                                 0x0002
#define DPTX0_TXAUX_RXPD_OFFSET                                                                    1
#define DPTX0_TXAUX_RXPD_WIDTH                                                                     1
#define DPTX0_TXAUX_TXPD_MASK                                                                 0x0004
#define DPTX0_TXAUX_TXPD_OFFSET                                                                    2
#define DPTX0_TXAUX_TXPD_WIDTH                                                                     1
#define DPTX0_AUX_TMODE_MASK                                                                  0x0008
#define DPTX0_AUX_TMODE_OFFSET                                                                     3
#define DPTX0_AUX_TMODE_WIDTH                                                                      1
#define DPTX0_AUX_REF_CLK_POL_MASK                                                            0x0010
#define DPTX0_AUX_REF_CLK_POL_OFFSET                                                               4
#define DPTX0_AUX_REF_CLK_POL_WIDTH                                                                1
#define DPTX0_AUX_IOS_MASK                                                                    0x0060
#define DPTX0_AUX_IOS_OFFSET                                                                       5
#define DPTX0_AUX_IOS_WIDTH                                                                        2
#define DPTX0_EQTMODE_MASK                                                                    0x0080
#define DPTX0_EQTMODE_OFFSET                                                                       7
#define DPTX0_EQTMODE_WIDTH                                                                        1
#define DPTX0_AUX_RSRV_MASK                                                                   0x0f00
#define DPTX0_AUX_RSRV_OFFSET                                                                      8
#define DPTX0_AUX_RSRV_WIDTH                                                                       4
// #define Reserved_MASK                                                                         0xf000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_PHY_CTRL(0x80)
#define DPTX0_PHY_CTRL                                                                    0x00000080 //RW POD:0xf
#define DPTX0_TXL0_PD_MASK                                                                      0x01
#define DPTX0_TXL0_PD_OFFSET                                                                       0
#define DPTX0_TXL0_PD_WIDTH                                                                        1
#define DPTX0_TXL1_PD_MASK                                                                      0x02
#define DPTX0_TXL1_PD_OFFSET                                                                       1
#define DPTX0_TXL1_PD_WIDTH                                                                        1
#define DPTX0_TXL2_PD_MASK                                                                      0x04
#define DPTX0_TXL2_PD_OFFSET                                                                       2
#define DPTX0_TXL2_PD_WIDTH                                                                        1
#define DPTX0_TXL3_PD_MASK                                                                      0x08
#define DPTX0_TXL3_PD_OFFSET                                                                       3
#define DPTX0_TXL3_PD_WIDTH                                                                        1
#define DPTX0_TXL0_RESET_MASK                                                                   0x10
#define DPTX0_TXL0_RESET_OFFSET                                                                    4
#define DPTX0_TXL0_RESET_WIDTH                                                                     1
#define DPTX0_TXL1_RESET_MASK                                                                   0x20
#define DPTX0_TXL1_RESET_OFFSET                                                                    5
#define DPTX0_TXL1_RESET_WIDTH                                                                     1
#define DPTX0_TXL2_RESET_MASK                                                                   0x40
#define DPTX0_TXL2_RESET_OFFSET                                                                    6
#define DPTX0_TXL2_RESET_WIDTH                                                                     1
#define DPTX0_TXL3_RESET_MASK                                                                   0x80
#define DPTX0_TXL3_RESET_OFFSET                                                                    7
#define DPTX0_TXL3_RESET_WIDTH                                                                     1



// Register DPTX0_CLK_CTRL(0x84)
#define DPTX0_CLK_CTRL                                                                    0x00000084 //RW POD:0x4
#define DPTX0_CLKCTRL_RSRV1_MASK                                                                0x03
#define DPTX0_CLKCTRL_RSRV1_OFFSET                                                                 0
#define DPTX0_CLKCTRL_RSRV1_WIDTH                                                                  2
#define DPTX0_LSCLK_135_SRC_MASK                                                                0x04
#define DPTX0_LSCLK_135_SRC_OFFSET                                                                 2
#define DPTX0_LSCLK_135_SRC_WIDTH                                                                  1
#define DPTX0_CLKCTRL_RSRV2_MASK                                                                0x08
#define DPTX0_CLKCTRL_RSRV2_OFFSET                                                                 3
#define DPTX0_CLKCTRL_RSRV2_WIDTH                                                                  1
#define DPTX0_L0_LSCLK_135_INV_MASK                                                             0x10
#define DPTX0_L0_LSCLK_135_INV_OFFSET                                                              4
#define DPTX0_L0_LSCLK_135_INV_WIDTH                                                               1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX0_PLL_CTRL(0x88)
#define DPTX0_PLL_CTRL                                                                    0x00000088 //RW POD:0x0
#define DPTX0_TXPLL_PD_MASK                                                                   0x0001
#define DPTX0_TXPLL_PD_OFFSET                                                                      0
#define DPTX0_TXPLL_PD_WIDTH                                                                       1
#define DPTX0_TXPLL_DLPF_MASK                                                                 0x0002
#define DPTX0_TXPLL_DLPF_OFFSET                                                                    1
#define DPTX0_TXPLL_DLPF_WIDTH                                                                     1
#define DPTX0_TXPLL_ENFBM_MASK                                                                0x0004
#define DPTX0_TXPLL_ENFBM_OFFSET                                                                   2
#define DPTX0_TXPLL_ENFBM_WIDTH                                                                    1
#define DPTX0_TXPLL_ENLPF_MASK                                                                0x0008
#define DPTX0_TXPLL_ENLPF_OFFSET                                                                   3
#define DPTX0_TXPLL_ENLPF_WIDTH                                                                    1
#define DPTX0_TXPLL_ENTSTCLK_MASK                                                             0x0010
#define DPTX0_TXPLL_ENTSTCLK_OFFSET                                                                4
#define DPTX0_TXPLL_ENTSTCLK_WIDTH                                                                 1
// #define Reserved_MASK                                                                         0x0020
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                         0x0040
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             1
// #define Reserved_MASK                                                                         0x0080
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1
#define DPTX0_TXPLL_RSRV_MASK                                                                 0xff00
#define DPTX0_TXPLL_RSRV_OFFSET                                                                    8
#define DPTX0_TXPLL_RSRV_WIDTH                                                                     8



// Register DPTX0_TXPLL_NS(0x8c)
#define DPTX0_TXPLL_NS                                                                    0x0000008c //RW POD:0x2
#define DPTX0_TXPLL_NS0_MASK                                                                    0x01
#define DPTX0_TXPLL_NS0_OFFSET                                                                     0
#define DPTX0_TXPLL_NS0_WIDTH                                                                      1
#define DPTX0_TXPLL_NS1_MASK                                                                    0x02
#define DPTX0_TXPLL_NS1_OFFSET                                                                     1
#define DPTX0_TXPLL_NS1_WIDTH                                                                      1
#define DPTX0_TXPLL_NS2_MASK                                                                    0x04
#define DPTX0_TXPLL_NS2_OFFSET                                                                     2
#define DPTX0_TXPLL_NS2_WIDTH                                                                      1
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_RTERM_CTRL(0x90)
#define DPTX0_RTERM_CTRL                                                                  0x00000090 //RW POD:0x0
#define DPTX0_TXEN_CALRTERM_MASK                                                              0x0001
#define DPTX0_TXEN_CALRTERM_OFFSET                                                                 0
#define DPTX0_TXEN_CALRTERM_WIDTH                                                                  1
#define DPTX0_TXSEL_RTERM_MASK                                                                0x0002
#define DPTX0_TXSEL_RTERM_OFFSET                                                                   1
#define DPTX0_TXSEL_RTERM_WIDTH                                                                    1
#define DPTX0_TX_RTERM_ENB_MASK                                                               0x0004
#define DPTX0_TX_RTERM_ENB_OFFSET                                                                  2
#define DPTX0_TX_RTERM_ENB_WIDTH                                                                   1
#define DPTX0_TX_IMBGEN_PD_MASK                                                               0x0008
#define DPTX0_TX_IMBGEN_PD_OFFSET                                                                  3
#define DPTX0_TX_IMBGEN_PD_WIDTH                                                                   1
#define DPTX0_TX_RTERM_MASK                                                                   0x00f0
#define DPTX0_TX_RTERM_OFFSET                                                                      4
#define DPTX0_TX_RTERM_WIDTH                                                                       4
#define DPTX0_TXBIAS_RSRV_MASK                                                                0x0f00
#define DPTX0_TXBIAS_RSRV_OFFSET                                                                   8
#define DPTX0_TXBIAS_RSRV_WIDTH                                                                    4
// #define Reserved_MASK                                                                         0xf000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_MISC_CTRL(0x94)
#define DPTX0_MISC_CTRL                                                                   0x00000094 //RW POD:0x0
#define DPTX0_VREF_SEL_MASK                                                                     0x01
#define DPTX0_VREF_SEL_OFFSET                                                                      0
#define DPTX0_VREF_SEL_WIDTH                                                                       1
#define DPTX0_TX_ENVBUFC_MASK                                                                   0x02
#define DPTX0_TX_ENVBUFC_OFFSET                                                                    1
#define DPTX0_TX_ENVBUFC_WIDTH                                                                     1
#define DPTX0_TX_ENVBG_MASK                                                                     0x04
#define DPTX0_TX_ENVBG_OFFSET                                                                      2
#define DPTX0_TX_ENVBG_WIDTH                                                                       1
// #define Reserved_MASK                                                                           0x08
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             1
#define DPTX0_DESER_RESET_MASK                                                                  0x10
#define DPTX0_DESER_RESET_OFFSET                                                                   4
#define DPTX0_DESER_RESET_WIDTH                                                                    1
#define DPTX0_RESERVED0_MASK                                                                    0xe0
#define DPTX0_RESERVED0_OFFSET                                                                     5
#define DPTX0_RESERVED0_WIDTH                                                                      3



// Register DPTX0_RCAL_RESULTS(0x98)
#define DPTX0_RCAL_RESULTS                                                                0x00000098 //RO POD:0x0
#define DPTX0_TX_RCAL_RESULT_MASK                                                               0x0f
#define DPTX0_TX_RCAL_RESULT_OFFSET                                                                0
#define DPTX0_TX_RCAL_RESULT_WIDTH                                                                 4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_PHYFIFO_CTRL(0x9c)
#define DPTX0_PHYFIFO_CTRL                                                                0x0000009c //CRW POD:0x0
#define DPTX0_PHYFIFO_INIT_MASK                                                                 0x01
#define DPTX0_PHYFIFO_INIT_OFFSET                                                                  0
#define DPTX0_PHYFIFO_INIT_WIDTH                                                                   1
#define DPTX0_PHYFIFO_BYPASS_MASK                                                               0x02
#define DPTX0_PHYFIFO_BYPASS_OFFSET                                                                1
#define DPTX0_PHYFIFO_BYPASS_WIDTH                                                                 1
#define DPTX0_PHYFIFO_RDCLK_INV_L0_MASK                                                         0x04
#define DPTX0_PHYFIFO_RDCLK_INV_L0_OFFSET                                                          2
#define DPTX0_PHYFIFO_RDCLK_INV_L0_WIDTH                                                           1
#define DPTX0_PHYFIFO_RDCLK_INV_L1_MASK                                                         0x08
#define DPTX0_PHYFIFO_RDCLK_INV_L1_OFFSET                                                          3
#define DPTX0_PHYFIFO_RDCLK_INV_L1_WIDTH                                                           1
#define DPTX0_PHYFIFO_RDCLK_INV_L2_MASK                                                         0x10
#define DPTX0_PHYFIFO_RDCLK_INV_L2_OFFSET                                                          4
#define DPTX0_PHYFIFO_RDCLK_INV_L2_WIDTH                                                           1
#define DPTX0_PHYFIFO_RDCLK_INV_L3_MASK                                                         0x20
#define DPTX0_PHYFIFO_RDCLK_INV_L3_OFFSET                                                          5
#define DPTX0_PHYFIFO_RDCLK_INV_L3_WIDTH                                                           1
#define DPTX0_PHYFIFO_RSRV_MASK                                                                 0xc0
#define DPTX0_PHYFIFO_RSRV_OFFSET                                                                  6
#define DPTX0_PHYFIFO_RSRV_WIDTH                                                                   2



// Register DPTX0_L0_GAIN(0xa0)
#define DPTX0_L0_GAIN                                                                     0x000000a0 //RW POD:0x0
#define DPTX0_L0_G_MASK                                                                         0x3f
#define DPTX0_L0_G_OFFSET                                                                          0
#define DPTX0_L0_G_WIDTH                                                                           6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_GAIN1(0xa4)
#define DPTX0_L0_GAIN1                                                                    0x000000a4 //RW POD:0x0
#define DPTX0_L0_G1_MASK                                                                        0x3f
#define DPTX0_L0_G1_OFFSET                                                                         0
#define DPTX0_L0_G1_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_GAIN2(0xa8)
#define DPTX0_L0_GAIN2                                                                    0x000000a8 //RW POD:0x0
#define DPTX0_L0_G2_MASK                                                                        0x3f
#define DPTX0_L0_G2_OFFSET                                                                         0
#define DPTX0_L0_G2_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_GAIN3(0xac)
#define DPTX0_L0_GAIN3                                                                    0x000000ac //RW POD:0x0
#define DPTX0_L0_G3_MASK                                                                        0x3f
#define DPTX0_L0_G3_OFFSET                                                                         0
#define DPTX0_L0_G3_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_PRE_EMPH(0xb0)
#define DPTX0_L0_PRE_EMPH                                                                 0x000000b0 //RW POD:0x0
#define DPTX0_L0_P_MASK                                                                         0x3f
#define DPTX0_L0_P_OFFSET                                                                          0
#define DPTX0_L0_P_WIDTH                                                                           6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_PRE_EMPH1(0xb4)
#define DPTX0_L0_PRE_EMPH1                                                                0x000000b4 //RW POD:0x0
#define DPTX0_L0_P1_MASK                                                                        0x3f
#define DPTX0_L0_P1_OFFSET                                                                         0
#define DPTX0_L0_P1_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_PRE_EMPH2(0xb8)
#define DPTX0_L0_PRE_EMPH2                                                                0x000000b8 //RW POD:0x0
#define DPTX0_L0_P2_MASK                                                                        0x3f
#define DPTX0_L0_P2_OFFSET                                                                         0
#define DPTX0_L0_P2_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_L0_PRE_EMPH3(0xbc)
#define DPTX0_L0_PRE_EMPH3                                                                0x000000bc //RW POD:0x0
#define DPTX0_L0_P3_MASK                                                                        0x3f
#define DPTX0_L0_P3_OFFSET                                                                         0
#define DPTX0_L0_P3_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_TXRSRVBITS(0xc0)
#define DPTX0_TXRSRVBITS                                                                  0x000000c0 //RW POD:0x0
#define DPTX0_TXRSRVBITS_MASK                                                                 0xffff
#define DPTX0_TXRSRVBITS_OFFSET                                                                    0
#define DPTX0_TXRSRVBITS_WIDTH                                                                    16



// Register DPTX0_BIST_CONTROL(0xcc)
#define DPTX0_BIST_CONTROL                                                                0x000000cc //RW POD:0x0
#define DPTX0_BIST_MODE_MASK                                                                    0x01
#define DPTX0_BIST_MODE_OFFSET                                                                     0
#define DPTX0_BIST_MODE_WIDTH                                                                      1
#define DPTX0_BIST_PRBS_EN_MASK                                                                 0x02
#define DPTX0_BIST_PRBS_EN_OFFSET                                                                  1
#define DPTX0_BIST_PRBS_EN_WIDTH                                                                   1
#define DPTX0_BIST_CODE_ORDER_MASK                                                              0x04
#define DPTX0_BIST_CODE_ORDER_OFFSET                                                               2
#define DPTX0_BIST_CODE_ORDER_WIDTH                                                                1
#define DPTX0_BIST_CLK_INV_MASK                                                                 0x08
#define DPTX0_BIST_CLK_INV_OFFSET                                                                  3
#define DPTX0_BIST_CLK_INV_WIDTH                                                                   1
#define DPTX0_BIST_RSRV_MASK                                                                    0x10
#define DPTX0_BIST_RSRV_OFFSET                                                                     4
#define DPTX0_BIST_RSRV_WIDTH                                                                      1
#define DPTX0_BIST_CRC_ENABLE_MASK                                                              0x20
#define DPTX0_BIST_CRC_ENABLE_OFFSET                                                               5
#define DPTX0_BIST_CRC_ENABLE_WIDTH                                                                1
#define DPTX0_BIST_CRC_FORCE_RESET_MASK                                                         0x40
#define DPTX0_BIST_CRC_FORCE_RESET_OFFSET                                                          6
#define DPTX0_BIST_CRC_FORCE_RESET_WIDTH                                                           1
#define DPTX0_BIST_BS_SELECT_MASK                                                               0x80
#define DPTX0_BIST_BS_SELECT_OFFSET                                                                7
#define DPTX0_BIST_BS_SELECT_WIDTH                                                                 1



// Register DPTX0_BIST_CONTROL1(0xd0)
#define DPTX0_BIST_CONTROL1                                                               0x000000d0 //RW POD:0x0
#define DPTX0_BIST_SEQ_LENGTH_MASK                                                              0x07
#define DPTX0_BIST_SEQ_LENGTH_OFFSET                                                               0
#define DPTX0_BIST_SEQ_LENGTH_WIDTH                                                                3
#define DPTX0_BIST_KCODE_MODE_MASK                                                              0x08
#define DPTX0_BIST_KCODE_MODE_OFFSET                                                               3
#define DPTX0_BIST_KCODE_MODE_WIDTH                                                                1
#define DPTX0_BIST_CLK_MODE_MASK                                                                0x10
#define DPTX0_BIST_CLK_MODE_OFFSET                                                                 4
#define DPTX0_BIST_CLK_MODE_WIDTH                                                                  1
#define DPTX0_BIST_BITS_SWAP_MASK                                                               0x20
#define DPTX0_BIST_BITS_SWAP_OFFSET                                                                5
#define DPTX0_BIST_BITS_SWAP_WIDTH                                                                 1
#define DPTX0_BIST_SYMBOL_SWAP_MASK                                                             0x40
#define DPTX0_BIST_SYMBOL_SWAP_OFFSET                                                              6
#define DPTX0_BIST_SYMBOL_SWAP_WIDTH                                                               1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX0_BIST_CONTROL2(0xd4)
#define DPTX0_BIST_CONTROL2                                                               0x000000d4 //RW POD:0x0
#define DPTX0_BISTCLK_DLY_MASK                                                                  0x0f
#define DPTX0_BISTCLK_DLY_OFFSET                                                                   0
#define DPTX0_BISTCLK_DLY_WIDTH                                                                    4
#define DPTX0_BIST_LN_SEL_MASK                                                                  0xf0
#define DPTX0_BIST_LN_SEL_OFFSET                                                                   4
#define DPTX0_BIST_LN_SEL_WIDTH                                                                    4



// Register DPTX0_BIST_PATTERN0(0xd8)
#define DPTX0_BIST_PATTERN0                                                               0x000000d8 //RW POD:0x0
#define DPTX0_BIST_PATTERN0_MASK                                                          0x3fffffff
#define DPTX0_BIST_PATTERN0_OFFSET                                                                 0
#define DPTX0_BIST_PATTERN0_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_BIST_PATTERN1(0xdc)
#define DPTX0_BIST_PATTERN1                                                               0x000000dc //RW POD:0x0
#define DPTX0_BIST_PATTERN1_MASK                                                          0x3fffffff
#define DPTX0_BIST_PATTERN1_OFFSET                                                                 0
#define DPTX0_BIST_PATTERN1_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_BIST_PATTERN2(0xe0)
#define DPTX0_BIST_PATTERN2                                                               0x000000e0 //RW POD:0x0
#define DPTX0_BIST_PATTERN2_MASK                                                          0x3fffffff
#define DPTX0_BIST_PATTERN2_OFFSET                                                                 0
#define DPTX0_BIST_PATTERN2_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_BIST_PATTERN3(0xe4)
#define DPTX0_BIST_PATTERN3                                                               0x000000e4 //RW POD:0x0
#define DPTX0_BIST_PATTERN3_MASK                                                          0x3fffffff
#define DPTX0_BIST_PATTERN3_OFFSET                                                                 0
#define DPTX0_BIST_PATTERN3_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_BIST_PATTERN4(0xe8)
#define DPTX0_BIST_PATTERN4                                                               0x000000e8 //RW POD:0x0
#define DPTX0_BIST_PATTERN4_MASK                                                          0x3fffffff
#define DPTX0_BIST_PATTERN4_OFFSET                                                                 0
#define DPTX0_BIST_PATTERN4_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_BIST_CRC_STATUS(0xec)
#define DPTX0_BIST_CRC_STATUS                                                             0x000000ec //RO POD:0x0
#define DPTX0_ERROR_COUNT_MASK                                                                0x003f
#define DPTX0_ERROR_COUNT_OFFSET                                                                   0
#define DPTX0_ERROR_COUNT_WIDTH                                                                    6
#define DPTX0_ERROR_OVERFLOW_MASK                                                             0x0040
#define DPTX0_ERROR_OVERFLOW_OFFSET                                                                6
#define DPTX0_ERROR_OVERFLOW_WIDTH                                                                 1
#define DPTX0_SIGNATURE_LOCK_MASK                                                             0x0080
#define DPTX0_SIGNATURE_LOCK_OFFSET                                                                7
#define DPTX0_SIGNATURE_LOCK_WIDTH                                                                 1
#define DPTX0_PRBS_ACTIV_MASK                                                                 0x0100
#define DPTX0_PRBS_ACTIV_OFFSET                                                                    8
#define DPTX0_PRBS_ACTIV_WIDTH                                                                     1
// #define Reserved_MASK                                                                         0xfe00
// #define Reserved_OFFSET                                                                            9
// #define Reserved_WIDTH                                                                             7



// Register DPTX0_AUX_MISC_CTRL(0xf0)
#define DPTX0_AUX_MISC_CTRL                                                               0x000000f0 //RW POD:0x0
#define DPTX0_TX_CKTMODE_MASK                                                                   0x01
#define DPTX0_TX_CKTMODE_OFFSET                                                                    0
#define DPTX0_TX_CKTMODE_WIDTH                                                                     1
#define DPTX0_CK_SEL_MASK                                                                       0x06
#define DPTX0_CK_SEL_OFFSET                                                                        1
#define DPTX0_CK_SEL_WIDTH                                                                         2
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_BIST_CRC_SIGNATURE(0xf4)
#define DPTX0_BIST_CRC_SIGNATURE                                                          0x000000f4 //RO POD:0x0
#define DPTX0_BIST_CRC_SIGNATURE_MASK                                                         0xffff
#define DPTX0_BIST_CRC_SIGNATURE_OFFSET                                                            0
#define DPTX0_BIST_CRC_SIGNATURE_WIDTH                                                            16



// Register DPTX0_PLL_CLK_MEAS_CTRL(0x100)
#define DPTX0_PLL_CLK_MEAS_CTRL                                                           0x00000100 //RW POD:0x0
#define DPTX0_PLL_SYSCLK_MEAS_EN_MASK                                                           0x01
#define DPTX0_PLL_SYSCLK_MEAS_EN_OFFSET                                                            0
#define DPTX0_PLL_SYSCLK_MEAS_EN_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX0_PLL_CLK_MEAS_RESULT(0x104)
#define DPTX0_PLL_CLK_MEAS_RESULT                                                         0x00000104 //RO POD:0x0
#define DPTX0_PLL_SYSCLK_RESULT_MASK                                                          0x0fff
#define DPTX0_PLL_SYSCLK_RESULT_OFFSET                                                             0
#define DPTX0_PLL_SYSCLK_RESULT_WIDTH                                                             12
// #define Reserved_MASK                                                                         0xf000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_PHYA_IRQ_CTRL(0x108)
#define DPTX0_PHYA_IRQ_CTRL                                                               0x00000108 //RW POD:0x0
#define DPTX0_NOCLK_IRQ_EN_MASK                                                                 0x01
#define DPTX0_NOCLK_IRQ_EN_OFFSET                                                                  0
#define DPTX0_NOCLK_IRQ_EN_WIDTH                                                                   1
#define DPTX0_CLKERR_IRQ_EN_MASK                                                                0x02
#define DPTX0_CLKERR_IRQ_EN_OFFSET                                                                 1
#define DPTX0_CLKERR_IRQ_EN_WIDTH                                                                  1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_PHYA_IRQ_STATUS(0x10c)
#define DPTX0_PHYA_IRQ_STATUS                                                             0x0000010c //CRO POD:0x0
#define DPTX0_NOCLK_STS_MASK                                                                    0x01
#define DPTX0_NOCLK_STS_OFFSET                                                                     0
#define DPTX0_NOCLK_STS_WIDTH                                                                      1
#define DPTX0_CLKERR_STS_MASK                                                                   0x02
#define DPTX0_CLKERR_STS_OFFSET                                                                    1
#define DPTX0_CLKERR_STS_WIDTH                                                                     1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6






// address block DPTX0_AUX [0x1a0]


# define DPTX0_AUX_BASE_ADDRESS 0x1a0


// Register DPTX0_AUX_CONTROL(0x1a0)
#define DPTX0_AUX_CONTROL                                                                 0x000001a0 //RW POD:0x0
#define DPTX0_LT2AUX_DIS_MASK                                                                   0x01
#define DPTX0_LT2AUX_DIS_OFFSET                                                                    0
#define DPTX0_LT2AUX_DIS_WIDTH                                                                     1
#define DPTX0_I2C2AUX_DIS_MASK                                                                  0x02
#define DPTX0_I2C2AUX_DIS_OFFSET                                                                   1
#define DPTX0_I2C2AUX_DIS_WIDTH                                                                    1
#define DPTX0_OCM2AUX_DIS_MASK                                                                  0x04
#define DPTX0_OCM2AUX_DIS_OFFSET                                                                   2
#define DPTX0_OCM2AUX_DIS_WIDTH                                                                    1
#define DPTX0_HDCP2AUX_DIS_MASK                                                                 0x08
#define DPTX0_HDCP2AUX_DIS_OFFSET                                                                  3
#define DPTX0_HDCP2AUX_DIS_WIDTH                                                                   1
#define DPTX0_AUX_LBACK_EN_MASK                                                                 0x10
#define DPTX0_AUX_LBACK_EN_OFFSET                                                                  4
#define DPTX0_AUX_LBACK_EN_WIDTH                                                                   1
#define DPTX0_AUX_LONG_PREAMBL_MASK                                                             0x20
#define DPTX0_AUX_LONG_PREAMBL_OFFSET                                                              5
#define DPTX0_AUX_LONG_PREAMBL_WIDTH                                                               1
#define DPTX0_AUX_POLARITY_MASK                                                                 0x40
#define DPTX0_AUX_POLARITY_OFFSET                                                                  6
#define DPTX0_AUX_POLARITY_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX0_AUX_CLK_DIV(0x1a4)
#define DPTX0_AUX_CLK_DIV                                                                 0x000001a4 //RW POD:0x65
#define DPTX0_AUX_CLK_HIGH_MASK                                                                 0x0f
#define DPTX0_AUX_CLK_HIGH_OFFSET                                                                  0
#define DPTX0_AUX_CLK_HIGH_WIDTH                                                                   4
#define DPTX0_AUX_CLK_LOW_MASK                                                                  0xf0
#define DPTX0_AUX_CLK_LOW_OFFSET                                                                   4
#define DPTX0_AUX_CLK_LOW_WIDTH                                                                    4



// Register DPTX0_AUX_IRQ_CTRL(0x1b0)
#define DPTX0_AUX_IRQ_CTRL                                                                0x000001b0 //RW POD:0x0
#define DPTX0_OCM2AUX_REPLY_IRQ_EN_MASK                                                         0x01
#define DPTX0_OCM2AUX_REPLY_IRQ_EN_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_IRQ_EN_WIDTH                                                           1
#define DPTX0_OCM2AUX_REPLY_TMOUT_IRQ_EN_MASK                                                   0x02
#define DPTX0_OCM2AUX_REPLY_TMOUT_IRQ_EN_OFFSET                                                    1
#define DPTX0_OCM2AUX_REPLY_TMOUT_IRQ_EN_WIDTH                                                     1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_AUX_IRQ_STATUS(0x1b4)
#define DPTX0_AUX_IRQ_STATUS                                                              0x000001b4 //CRO POD:0x0
#define DPTX0_OCM2AUX_REPLY_IRQ_STS_MASK                                                        0x01
#define DPTX0_OCM2AUX_REPLY_IRQ_STS_OFFSET                                                         0
#define DPTX0_OCM2AUX_REPLY_IRQ_STS_WIDTH                                                          1
#define DPTX0_OCM2AUX_REPLY_TO_IRQ_STS_MASK                                                     0x02
#define DPTX0_OCM2AUX_REPLY_TO_IRQ_STS_OFFSET                                                      1
#define DPTX0_OCM2AUX_REPLY_TO_IRQ_STS_WIDTH                                                       1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_OCM2AUX_CONTROL(0x1c0)
#define DPTX0_OCM2AUX_CONTROL                                                             0x000001c0 //CRW POD:0x0
#define DPTX0_OCM2AUX_REQ_CMD_MASK                                                              0x0f
#define DPTX0_OCM2AUX_REQ_CMD_OFFSET                                                               0
#define DPTX0_OCM2AUX_REQ_CMD_WIDTH                                                                4
#define DPTX0_OCM2AUX_SHORT_REQ_MASK                                                            0x10
#define DPTX0_OCM2AUX_SHORT_REQ_OFFSET                                                             4
#define DPTX0_OCM2AUX_SHORT_REQ_WIDTH                                                              1
#define DPTX0_OCM2AUX_MODE_MASK                                                                 0x20
#define DPTX0_OCM2AUX_MODE_OFFSET                                                                  5
#define DPTX0_OCM2AUX_MODE_WIDTH                                                                   1
#define DPTX0_OCM2AUX_CONTROL_6_MASK                                                            0x40
#define DPTX0_OCM2AUX_CONTROL_6_OFFSET                                                             6
#define DPTX0_OCM2AUX_CONTROL_6_WIDTH                                                              1
#define DPTX0_OCM2AUX_REQ_MASK                                                                  0x80
#define DPTX0_OCM2AUX_REQ_OFFSET                                                                   7
#define DPTX0_OCM2AUX_REQ_WIDTH                                                                    1



// Register DPTX0_OCM2AUX_REQ_ADDR(0x1c4)
#define DPTX0_OCM2AUX_REQ_ADDR                                                            0x000001c4 //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_ADDR_MASK                                                       0x00ffffff
#define DPTX0_OCM2AUX_REQ_ADDR_OFFSET                                                              0
#define DPTX0_OCM2AUX_REQ_ADDR_WIDTH                                                              24
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register DPTX0_OCM2AUX_REQ_LENGTH(0x1c8)
#define DPTX0_OCM2AUX_REQ_LENGTH                                                          0x000001c8 //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_LENGTH_MASK                                                           0x0f
#define DPTX0_OCM2AUX_REQ_LENGTH_OFFSET                                                            0
#define DPTX0_OCM2AUX_REQ_LENGTH_WIDTH                                                             4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_OCM2AUX_RETRY_COUNT(0x1cc)
#define DPTX0_OCM2AUX_RETRY_COUNT                                                         0x000001cc //RW POD:0x0
#define DPTX0_OCM2AUX_RETRY_COUNT_MASK                                                          0xff
#define DPTX0_OCM2AUX_RETRY_COUNT_OFFSET                                                           0
#define DPTX0_OCM2AUX_RETRY_COUNT_WIDTH                                                            8



// Register DPTX0_OCM2AUX_REQ_DATA_0(0x1d0)
#define DPTX0_OCM2AUX_REQ_DATA_0                                                          0x000001d0 //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_DATA_0_MASK                                                     0xffffffff
#define DPTX0_OCM2AUX_REQ_DATA_0_OFFSET                                                            0
#define DPTX0_OCM2AUX_REQ_DATA_0_WIDTH                                                            32



// Register DPTX0_OCM2AUX_REQ_DATA_1(0x1d4)
#define DPTX0_OCM2AUX_REQ_DATA_1                                                          0x000001d4 //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_DATA_1_MASK                                                     0xffffffff
#define DPTX0_OCM2AUX_REQ_DATA_1_OFFSET                                                            0
#define DPTX0_OCM2AUX_REQ_DATA_1_WIDTH                                                            32



// Register DPTX0_OCM2AUX_REQ_DATA_2(0x1d8)
#define DPTX0_OCM2AUX_REQ_DATA_2                                                          0x000001d8 //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_DATA_2_MASK                                                     0xffffffff
#define DPTX0_OCM2AUX_REQ_DATA_2_OFFSET                                                            0
#define DPTX0_OCM2AUX_REQ_DATA_2_WIDTH                                                            32



// Register DPTX0_OCM2AUX_REQ_DATA_3(0x1dc)
#define DPTX0_OCM2AUX_REQ_DATA_3                                                          0x000001dc //RW POD:0x0
#define DPTX0_OCM2AUX_REQ_DATA_3_MASK                                                     0xffffffff
#define DPTX0_OCM2AUX_REQ_DATA_3_OFFSET                                                            0
#define DPTX0_OCM2AUX_REQ_DATA_3_WIDTH                                                            32



// Register DPTX0_OCM2AUX_REPLY_COMMAND(0x1f0)
#define DPTX0_OCM2AUX_REPLY_COMMAND                                                       0x000001f0 //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_COMMAND_MASK                                                        0x0f
#define DPTX0_OCM2AUX_REPLY_COMMAND_OFFSET                                                         0
#define DPTX0_OCM2AUX_REPLY_COMMAND_WIDTH                                                          4
#define DPTX0_OCM2AUX_REPLY_M_MASK                                                              0xf0
#define DPTX0_OCM2AUX_REPLY_M_OFFSET                                                               4
#define DPTX0_OCM2AUX_REPLY_M_WIDTH                                                                4



// Register DPTX0_OCM2AUX_REPLY_LENGTH(0x1f4)
#define DPTX0_OCM2AUX_REPLY_LENGTH                                                        0x000001f4 //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_LENGTH_MASK                                                         0x1f
#define DPTX0_OCM2AUX_REPLY_LENGTH_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_LENGTH_WIDTH                                                           5
#define DPTX0_OCM2AUX_REPLY_ERROR_MASK                                                          0xe0
#define DPTX0_OCM2AUX_REPLY_ERROR_OFFSET                                                           5
#define DPTX0_OCM2AUX_REPLY_ERROR_WIDTH                                                            3



// Register DPTX0_OCM2AUX_REPLY_DATA_0(0x1f8)
#define DPTX0_OCM2AUX_REPLY_DATA_0                                                        0x000001f8 //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_DATA_0_MASK                                                   0xffffffff
#define DPTX0_OCM2AUX_REPLY_DATA_0_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_DATA_0_WIDTH                                                          32



// Register DPTX0_OCM2AUX_REPLY_DATA_1(0x1fc)
#define DPTX0_OCM2AUX_REPLY_DATA_1                                                        0x000001fc //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_DATA_1_MASK                                                   0xffffffff
#define DPTX0_OCM2AUX_REPLY_DATA_1_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_DATA_1_WIDTH                                                          32



// Register DPTX0_OCM2AUX_REPLY_DATA_2(0x200)
#define DPTX0_OCM2AUX_REPLY_DATA_2                                                        0x00000200 //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_DATA_2_MASK                                                   0xffffffff
#define DPTX0_OCM2AUX_REPLY_DATA_2_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_DATA_2_WIDTH                                                          32



// Register DPTX0_OCM2AUX_REPLY_DATA_3(0x204)
#define DPTX0_OCM2AUX_REPLY_DATA_3                                                        0x00000204 //RO POD:0x0
#define DPTX0_OCM2AUX_REPLY_DATA_3_MASK                                                   0xffffffff
#define DPTX0_OCM2AUX_REPLY_DATA_3_OFFSET                                                          0
#define DPTX0_OCM2AUX_REPLY_DATA_3_WIDTH                                                          32



// Register DPTX0_I2C2AUX_CTRL(0x210)
#define DPTX0_I2C2AUX_CTRL                                                                0x00000210 //RW POD:0x1
#define DPTX0_I2C2AUX_EN_MASK                                                                   0x01
#define DPTX0_I2C2AUX_EN_OFFSET                                                                    0
#define DPTX0_I2C2AUX_EN_WIDTH                                                                     1
#define DPTX0_I2C2AUX_BROADCAST_MASK                                                            0x02
#define DPTX0_I2C2AUX_BROADCAST_OFFSET                                                             1
#define DPTX0_I2C2AUX_BROADCAST_WIDTH                                                              1
#define DPTX0_I2C2AUX_ADDR_CHK_EN_MASK                                                          0x04
#define DPTX0_I2C2AUX_ADDR_CHK_EN_OFFSET                                                           2
#define DPTX0_I2C2AUX_ADDR_CHK_EN_WIDTH                                                            1
// #define Reserved_MASK                                                                           0x08
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             1
#define DPTX0_I2C2AUX_RETRIES_MASK                                                              0xf0
#define DPTX0_I2C2AUX_RETRIES_OFFSET                                                               4
#define DPTX0_I2C2AUX_RETRIES_WIDTH                                                                4



// Register DPTX0_I2C2AUX_ADDR1(0x214)
#define DPTX0_I2C2AUX_ADDR1                                                               0x00000214 //RW POD:0x0
#define DPTX0_I2C2AUX_ADDR1_EN_MASK                                                             0x01
#define DPTX0_I2C2AUX_ADDR1_EN_OFFSET                                                              0
#define DPTX0_I2C2AUX_ADDR1_EN_WIDTH                                                               1
#define DPTX0_I2C2AUX_ADDR1_MASK                                                                0xfe
#define DPTX0_I2C2AUX_ADDR1_OFFSET                                                                 1
#define DPTX0_I2C2AUX_ADDR1_WIDTH                                                                  7



// Register DPTX0_I2C2AUX_ADDR2(0x218)
#define DPTX0_I2C2AUX_ADDR2                                                               0x00000218 //RW POD:0x0
#define DPTX0_I2C2AUX_ADDR2_EN_MASK                                                             0x01
#define DPTX0_I2C2AUX_ADDR2_EN_OFFSET                                                              0
#define DPTX0_I2C2AUX_ADDR2_EN_WIDTH                                                               1
#define DPTX0_I2C2AUX_ADDR2_MASK                                                                0xfe
#define DPTX0_I2C2AUX_ADDR2_OFFSET                                                                 1
#define DPTX0_I2C2AUX_ADDR2_WIDTH                                                                  7



// Register DPTX0_I2C2AUX_ADDR3(0x21c)
#define DPTX0_I2C2AUX_ADDR3                                                               0x0000021c //RW POD:0x0
#define DPTX0_I2C2AUX_ADDR3_EN_MASK                                                             0x01
#define DPTX0_I2C2AUX_ADDR3_EN_OFFSET                                                              0
#define DPTX0_I2C2AUX_ADDR3_EN_WIDTH                                                               1
#define DPTX0_I2C2AUX_ADDR3_MASK                                                                0xfe
#define DPTX0_I2C2AUX_ADDR3_OFFSET                                                                 1
#define DPTX0_I2C2AUX_ADDR3_WIDTH                                                                  7



// Register DPTX0_I2C2AUX_ADDR4(0x220)
#define DPTX0_I2C2AUX_ADDR4                                                               0x00000220 //RW POD:0x0
#define DPTX0_I2C2AUX_ADDR4_EN_MASK                                                             0x01
#define DPTX0_I2C2AUX_ADDR4_EN_OFFSET                                                              0
#define DPTX0_I2C2AUX_ADDR4_EN_WIDTH                                                               1
#define DPTX0_I2C2AUX_ADDR4_MASK                                                                0xfe
#define DPTX0_I2C2AUX_ADDR4_OFFSET                                                                 1
#define DPTX0_I2C2AUX_ADDR4_WIDTH                                                                  7



// Register DPTX0_I2C2AUX_BUF_SIZE(0x224)
#define DPTX0_I2C2AUX_BUF_SIZE                                                            0x00000224 //RW POD:0x0
#define DPTX0_I2C2AUX_WBUF_SIZE_MASK                                                            0x0f
#define DPTX0_I2C2AUX_WBUF_SIZE_OFFSET                                                             0
#define DPTX0_I2C2AUX_WBUF_SIZE_WIDTH                                                              4
#define DPTX0_I2C2AUX_RBUF_SIZE_MASK                                                            0xf0
#define DPTX0_I2C2AUX_RBUF_SIZE_OFFSET                                                             4
#define DPTX0_I2C2AUX_RBUF_SIZE_WIDTH                                                              4



// Register DPTX0_I2C2AUX_STRETCH_TIMEOUT(0x228)
#define DPTX0_I2C2AUX_STRETCH_TIMEOUT                                                     0x00000228 //RW POD:0xff
#define DPTX0_I2C2AUX_STRETCH_TIMEOUT_MASK                                                      0xff
#define DPTX0_I2C2AUX_STRETCH_TIMEOUT_OFFSET                                                       0
#define DPTX0_I2C2AUX_STRETCH_TIMEOUT_WIDTH                                                        8



// Register DPTX0_I2C2AUX_CTRL2(0x22c)
#define DPTX0_I2C2AUX_CTRL2                                                               0x0000022c //RW POD:0x0
#define DPTX0_I2C2AUX_STOP_REQ_MASK                                                             0x01
#define DPTX0_I2C2AUX_STOP_REQ_OFFSET                                                              0
#define DPTX0_I2C2AUX_STOP_REQ_WIDTH                                                               1
#define DPTX0_I2C2AUX_STOP_IRQ_EN_MASK                                                          0x02
#define DPTX0_I2C2AUX_STOP_IRQ_EN_OFFSET                                                           1
#define DPTX0_I2C2AUX_STOP_IRQ_EN_WIDTH                                                            1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_I2C2AUX_STATUS(0x230)
#define DPTX0_I2C2AUX_STATUS                                                              0x00000230 //CRO POD:0x0
#define DPTX0_I2C2AUX_I2C_NACK_MASK                                                             0x01
#define DPTX0_I2C2AUX_I2C_NACK_OFFSET                                                              0
#define DPTX0_I2C2AUX_I2C_NACK_WIDTH                                                               1
#define DPTX0_I2C2AUX_AUX_NACK_MASK                                                             0x02
#define DPTX0_I2C2AUX_AUX_NACK_OFFSET                                                              1
#define DPTX0_I2C2AUX_AUX_NACK_WIDTH                                                               1
#define DPTX0_I2C2AUX_AUX_DEFER_MASK                                                            0x04
#define DPTX0_I2C2AUX_AUX_DEFER_OFFSET                                                             2
#define DPTX0_I2C2AUX_AUX_DEFER_WIDTH                                                              1
#define DPTX0_I2C2AUX_ARB_TIMEOUT_MASK                                                          0x08
#define DPTX0_I2C2AUX_ARB_TIMEOUT_OFFSET                                                           3
#define DPTX0_I2C2AUX_ARB_TIMEOUT_WIDTH                                                            1
#define DPTX0_I2C2AUX_I2C_TIMEOUT_MASK                                                          0x10
#define DPTX0_I2C2AUX_I2C_TIMEOUT_OFFSET                                                           4
#define DPTX0_I2C2AUX_I2C_TIMEOUT_WIDTH                                                            1
#define DPTX0_I2C2AUX_CLK_STRETCH_MASK                                                          0x20
#define DPTX0_I2C2AUX_CLK_STRETCH_OFFSET                                                           5
#define DPTX0_I2C2AUX_CLK_STRETCH_WIDTH                                                            1
#define DPTX0_I2C2AUX_STOP_MASK                                                                 0x40
#define DPTX0_I2C2AUX_STOP_OFFSET                                                                  6
#define DPTX0_I2C2AUX_STOP_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1






// address block DPTX0_PHYD [0x240]


# define DPTX0_PHYD_BASE_ADDRESS 0x240


// Register DPTX0_LINK_CONTROL(0x240)
#define DPTX0_LINK_CONTROL                                                                0x00000240 //RW POD:0xf
#define DPTX0_LINK_EN_MASK                                                                      0x01
#define DPTX0_LINK_EN_OFFSET                                                                       0
#define DPTX0_LINK_EN_WIDTH                                                                        1
#define DPTX0_SCRM_ADVANCE_MASK                                                                 0x02
#define DPTX0_SCRM_ADVANCE_OFFSET                                                                  1
#define DPTX0_SCRM_ADVANCE_WIDTH                                                                   1
#define DPTX0_SCRM_POLYNOMIAL_MASK                                                              0x04
#define DPTX0_SCRM_POLYNOMIAL_OFFSET                                                               2
#define DPTX0_SCRM_POLYNOMIAL_WIDTH                                                                1
#define DPTX0_SKEW_EN_MASK                                                                      0x08
#define DPTX0_SKEW_EN_OFFSET                                                                       3
#define DPTX0_SKEW_EN_WIDTH                                                                        1
#define DPTX0_LANE_POLARITY_MASK                                                                0x10
#define DPTX0_LANE_POLARITY_OFFSET                                                                 4
#define DPTX0_LANE_POLARITY_WIDTH                                                                  1
#define DPTX0_HPD_POLARITY_MASK                                                                 0x20
#define DPTX0_HPD_POLARITY_OFFSET                                                                  5
#define DPTX0_HPD_POLARITY_WIDTH                                                                   1
#define DPTX0_SWAP_LANE_MASK                                                                    0xc0
#define DPTX0_SWAP_LANE_OFFSET                                                                     6
#define DPTX0_SWAP_LANE_WIDTH                                                                      2



// Register DPTX0_LT_CONTROL(0x244)
#define DPTX0_LT_CONTROL                                                                  0x00000244 //CRW POD:0x80
#define DPTX0_LT_START_MASK                                                                     0x01
#define DPTX0_LT_START_OFFSET                                                                      0
#define DPTX0_LT_START_WIDTH                                                                       1
#define DPTX0_LT_END_IRQ_EN_MASK                                                                0x02
#define DPTX0_LT_END_IRQ_EN_OFFSET                                                                 1
#define DPTX0_LT_END_IRQ_EN_WIDTH                                                                  1
#define DPTX0_LT_CONFIG_MASK                                                                    0x04
#define DPTX0_LT_CONFIG_OFFSET                                                                     2
#define DPTX0_LT_CONFIG_WIDTH                                                                      1
#define DPTX0_LONG_LT_EN_MASK                                                                   0x08
#define DPTX0_LONG_LT_EN_OFFSET                                                                    3
#define DPTX0_LONG_LT_EN_WIDTH                                                                     1
#define DPTX0_LT_EQ_DONE_IGNORE_MASK                                                            0x10
#define DPTX0_LT_EQ_DONE_IGNORE_OFFSET                                                             4
#define DPTX0_LT_EQ_DONE_IGNORE_WIDTH                                                              1
#define DPTX0_LT_SLOCKED_IGNORE_MASK                                                            0x20
#define DPTX0_LT_SLOCKED_IGNORE_OFFSET                                                             5
#define DPTX0_LT_SLOCKED_IGNORE_WIDTH                                                              1
#define DPTX0_LT_ADV_CHECK_EN_MASK                                                              0x40
#define DPTX0_LT_ADV_CHECK_EN_OFFSET                                                               6
#define DPTX0_LT_ADV_CHECK_EN_WIDTH                                                                1
#define DPTX0_LT_BW_REDUCE_AUTO_MASK                                                            0x80
#define DPTX0_LT_BW_REDUCE_AUTO_OFFSET                                                             7
#define DPTX0_LT_BW_REDUCE_AUTO_WIDTH                                                              1



// Register DPTX0_LT_STATUS(0x248)
#define DPTX0_LT_STATUS                                                                   0x00000248 //CRO POD:0x0
#define DPTX0_LT_ACTIVE_MASK                                                                    0x01
#define DPTX0_LT_ACTIVE_OFFSET                                                                     0
#define DPTX0_LT_ACTIVE_WIDTH                                                                      1
#define DPTX0_LT_END_IRQ_STS_MASK                                                               0x02
#define DPTX0_LT_END_IRQ_STS_OFFSET                                                                1
#define DPTX0_LT_END_IRQ_STS_WIDTH                                                                 1
#define DPTX0_LT_RESULT_MASK                                                                    0x04
#define DPTX0_LT_RESULT_OFFSET                                                                     2
#define DPTX0_LT_RESULT_WIDTH                                                                      1
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_LL_LINK_BW_SET(0x250)
#define DPTX0_LL_LINK_BW_SET                                                              0x00000250 //RW POD:0x0
#define DPTX0_LL_LINK_BW_SET_MASK                                                               0x1f
#define DPTX0_LL_LINK_BW_SET_OFFSET                                                                0
#define DPTX0_LL_LINK_BW_SET_WIDTH                                                                 5
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX0_LL_LANE_COUNT_SET(0x254)
#define DPTX0_LL_LANE_COUNT_SET                                                           0x00000254 //RW POD:0x4
#define DPTX0_LL_LANE_COUNT_SET_MASK                                                            0x07
#define DPTX0_LL_LANE_COUNT_SET_OFFSET                                                             0
#define DPTX0_LL_LANE_COUNT_SET_WIDTH                                                              3
// #define Reserved_MASK                                                                           0x78
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             4
#define DPTX0_LL_ENHANCED_FRAME_EN_SET_MASK                                                     0x80
#define DPTX0_LL_ENHANCED_FRAME_EN_SET_OFFSET                                                      7
#define DPTX0_LL_ENHANCED_FRAME_EN_SET_WIDTH                                                       1



// Register DPTX0_LL_TRAINING_PATTERN_SET(0x258)
#define DPTX0_LL_TRAINING_PATTERN_SET                                                     0x00000258 //RW POD:0x0
#define DPTX0_LL_TRAINING_PATTERN_SET_MASK                                                      0x03
#define DPTX0_LL_TRAINING_PATTERN_SET_OFFSET                                                       0
#define DPTX0_LL_TRAINING_PATTERN_SET_WIDTH                                                        2
#define DPTX0_LL_LINK_QUAL_PATTERN_SET_MASK                                                     0x0c
#define DPTX0_LL_LINK_QUAL_PATTERN_SET_OFFSET                                                      2
#define DPTX0_LL_LINK_QUAL_PATTERN_SET_WIDTH                                                       2
#define DPTX0_LL_RECOVERRED_CLOCK_OUT_EN_MASK                                                   0x10
#define DPTX0_LL_RECOVERRED_CLOCK_OUT_EN_OFFSET                                                    4
#define DPTX0_LL_RECOVERRED_CLOCK_OUT_EN_WIDTH                                                     1
#define DPTX0_LL_SCRAMBLING_DISABLE_SET_MASK                                                    0x20
#define DPTX0_LL_SCRAMBLING_DISABLE_SET_OFFSET                                                     5
#define DPTX0_LL_SCRAMBLING_DISABLE_SET_WIDTH                                                      1
#define DPTX0_LL_SYMBOL_ERROR_COUNT_SET_MASK                                                    0xc0
#define DPTX0_LL_SYMBOL_ERROR_COUNT_SET_OFFSET                                                     6
#define DPTX0_LL_SYMBOL_ERROR_COUNT_SET_WIDTH                                                      2



// Register DPTX0_LL_TRAINING_L0_SET(0x25c)
#define DPTX0_LL_TRAINING_L0_SET                                                          0x0000025c //RW POD:0x0
#define DPTX0_LL_L0_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX0_LL_L0_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX0_LL_L0_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX0_LL_L0_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX0_LL_L0_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX0_LL_L0_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L1_SET(0x260)
#define DPTX0_LL_TRAINING_L1_SET                                                          0x00000260 //RW POD:0x0
#define DPTX0_LL_L1_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX0_LL_L1_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX0_LL_L1_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX0_LL_L1_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX0_LL_L1_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX0_LL_L1_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L2_SET(0x264)
#define DPTX0_LL_TRAINING_L2_SET                                                          0x00000264 //RW POD:0x0
#define DPTX0_LL_L2_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX0_LL_L2_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX0_LL_L2_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX0_LL_L2_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX0_LL_L2_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX0_LL_L2_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L3_SET(0x268)
#define DPTX0_LL_TRAINING_L3_SET                                                          0x00000268 //RW POD:0x0
#define DPTX0_LL_L3_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX0_LL_L3_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX0_LL_L3_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX0_LL_L3_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX0_LL_L3_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX0_LL_L3_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_LINK_BW_STS(0x270)
#define DPTX0_LL_LINK_BW_STS                                                              0x00000270 //RO POD:0x0
#define DPTX0_LL_LINK_BW_STS_MASK                                                               0x1f
#define DPTX0_LL_LINK_BW_STS_OFFSET                                                                0
#define DPTX0_LL_LINK_BW_STS_WIDTH                                                                 5
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX0_LL_LANE_COUNT_STS(0x274)
#define DPTX0_LL_LANE_COUNT_STS                                                           0x00000274 //RO POD:0x0
#define DPTX0_LL_LANE_COUNT_STS_MASK                                                            0x0f
#define DPTX0_LL_LANE_COUNT_STS_OFFSET                                                             0
#define DPTX0_LL_LANE_COUNT_STS_WIDTH                                                              4
// #define Reserved_MASK                                                                           0x70
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             3
#define DPTX0_LL_ENHANCED_FRAME_EN_STS_MASK                                                     0x80
#define DPTX0_LL_ENHANCED_FRAME_EN_STS_OFFSET                                                      7
#define DPTX0_LL_ENHANCED_FRAME_EN_STS_WIDTH                                                       1



// Register DPTX0_LL_TRAINING_PATTERN_STS(0x278)
#define DPTX0_LL_TRAINING_PATTERN_STS                                                     0x00000278 //RO POD:0x0
#define DPTX0_LL_TRAINING_PATTERN_STS_MASK                                                      0x03
#define DPTX0_LL_TRAINING_PATTERN_STS_OFFSET                                                       0
#define DPTX0_LL_TRAINING_PATTERN_STS_WIDTH                                                        2
#define DPTX0_LL_LINK_QUAL_PATTERN_STS_MASK                                                     0x0c
#define DPTX0_LL_LINK_QUAL_PATTERN_STS_OFFSET                                                      2
#define DPTX0_LL_LINK_QUAL_PATTERN_STS_WIDTH                                                       2
#define DPTX0_LL_SCRAMBLING_DISABLE_STS_MASK                                                    0x10
#define DPTX0_LL_SCRAMBLING_DISABLE_STS_OFFSET                                                     4
#define DPTX0_LL_SCRAMBLING_DISABLE_STS_WIDTH                                                      1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX0_LL_TRAINING_L0_STS(0x27c)
#define DPTX0_LL_TRAINING_L0_STS                                                          0x0000027c //RO POD:0x0
#define DPTX0_LL_L0_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX0_LL_L0_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX0_LL_L0_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX0_LL_L0_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX0_LL_L0_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX0_LL_L0_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX0_LL_L0_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX0_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L1_STS(0x280)
#define DPTX0_LL_TRAINING_L1_STS                                                          0x00000280 //RO POD:0x0
#define DPTX0_LL_L1_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX0_LL_L1_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX0_LL_L1_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX0_LL_L1_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX0_LL_L1_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX0_LL_L1_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX0_LL_L1_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX0_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L2_STS(0x284)
#define DPTX0_LL_TRAINING_L2_STS                                                          0x00000284 //RO POD:0x0
#define DPTX0_LL_L2_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX0_LL_L2_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX0_LL_L2_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX0_LL_L2_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX0_LL_L2_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX0_LL_L2_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX0_LL_L2_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX0_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_LL_TRAINING_L3_STS(0x288)
#define DPTX0_LL_TRAINING_L3_STS                                                          0x00000288 //RO POD:0x0
#define DPTX0_LL_L3_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX0_LL_L3_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX0_LL_L3_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX0_LL_L3_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX0_LL_L3_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX0_LL_L3_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX0_LL_L3_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX0_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_SCRAMBLER_SEED(0x28c)
#define DPTX0_SCRAMBLER_SEED                                                              0x0000028c //RW POD:0xffff
#define DPTX0_SCRAMBLER_SEED_MASK                                                             0xffff
#define DPTX0_SCRAMBLER_SEED_OFFSET                                                                0
#define DPTX0_SCRAMBLER_SEED_WIDTH                                                                16



// Register DPTX0_SYM_GEN_CTRL(0x290)
#define DPTX0_SYM_GEN_CTRL                                                                0x00000290 //RW POD:0x0
#define DPTX0_SYM_GEN_MODE_MASK                                                                 0x01
#define DPTX0_SYM_GEN_MODE_OFFSET                                                                  0
#define DPTX0_SYM_GEN_MODE_WIDTH                                                                   1
#define DPTX0_SYM_K28_SWAP_MASK                                                                 0x02
#define DPTX0_SYM_K28_SWAP_OFFSET                                                                  1
#define DPTX0_SYM_K28_SWAP_WIDTH                                                                   1
#define DPTX0_8B10B_DI_BYTES_SWP_MASK                                                           0x04
#define DPTX0_8B10B_DI_BYTES_SWP_OFFSET                                                            2
#define DPTX0_8B10B_DI_BYTES_SWP_WIDTH                                                             1
#define DPTX0_8B10B_DO_BITS_SWP_MASK                                                            0x08
#define DPTX0_8B10B_DO_BITS_SWP_OFFSET                                                             3
#define DPTX0_8B10B_DO_BITS_SWP_WIDTH                                                              1
#define DPTX0_PRBS7_BIT_REVERSE_MASK                                                            0x10
#define DPTX0_PRBS7_BIT_REVERSE_OFFSET                                                             4
#define DPTX0_PRBS7_BIT_REVERSE_WIDTH                                                              1
#define DPTX0_SYM_RESERVED_MASK                                                                 0xe0
#define DPTX0_SYM_RESERVED_OFFSET                                                                  5
#define DPTX0_SYM_RESERVED_WIDTH                                                                   3



// Register DPTX0_SYM_GEN0(0x294)
#define DPTX0_SYM_GEN0                                                                    0x00000294 //RW POD:0x0
#define DPTX0_SYM_GEN0_MASK                                                                   0x03ff
#define DPTX0_SYM_GEN0_OFFSET                                                                      0
#define DPTX0_SYM_GEN0_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_SYM_GEN1(0x298)
#define DPTX0_SYM_GEN1                                                                    0x00000298 //RW POD:0x0
#define DPTX0_SYM_GEN1_MASK                                                                   0x03ff
#define DPTX0_SYM_GEN1_OFFSET                                                                      0
#define DPTX0_SYM_GEN1_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_SYM_GEN2(0x29c)
#define DPTX0_SYM_GEN2                                                                    0x0000029c //RW POD:0x0
#define DPTX0_SYM_GEN2_MASK                                                                   0x03ff
#define DPTX0_SYM_GEN2_OFFSET                                                                      0
#define DPTX0_SYM_GEN2_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_SYM_GEN3(0x2a0)
#define DPTX0_SYM_GEN3                                                                    0x000002a0 //RW POD:0x0
#define DPTX0_SYM_GEN3_MASK                                                                   0x03ff
#define DPTX0_SYM_GEN3_OFFSET                                                                      0
#define DPTX0_SYM_GEN3_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_L0_PATTERN_CTRL(0x2b0)
#define DPTX0_L0_PATTERN_CTRL                                                             0x000002b0 //CRW POD:0x0
#define DPTX0_L0_PAT_SEL_MASK                                                                   0x07
#define DPTX0_L0_PAT_SEL_OFFSET                                                                    0
#define DPTX0_L0_PAT_SEL_WIDTH                                                                     3
#define DPTX0_L0_IDP_LT_EN_MASK                                                                 0x08
#define DPTX0_L0_IDP_LT_EN_OFFSET                                                                  3
#define DPTX0_L0_IDP_LT_EN_WIDTH                                                                   1
#define DPTX0_L0_PATTERN_SET_MASK                                                               0xf0
#define DPTX0_L0_PATTERN_SET_OFFSET                                                                4
#define DPTX0_L0_PATTERN_SET_WIDTH                                                                 4



// Register DPTX0_L1_PATTERN_CTRL(0x2b4)
#define DPTX0_L1_PATTERN_CTRL                                                             0x000002b4 //CRW POD:0x0
#define DPTX0_L1_PAT_SEL_MASK                                                                   0x07
#define DPTX0_L1_PAT_SEL_OFFSET                                                                    0
#define DPTX0_L1_PAT_SEL_WIDTH                                                                     3
#define DPTX0_L1_IDP_LT_EN_MASK                                                                 0x08
#define DPTX0_L1_IDP_LT_EN_OFFSET                                                                  3
#define DPTX0_L1_IDP_LT_EN_WIDTH                                                                   1
#define DPTX0_L1_PATTERN_SET_MASK                                                               0xf0
#define DPTX0_L1_PATTERN_SET_OFFSET                                                                4
#define DPTX0_L1_PATTERN_SET_WIDTH                                                                 4



// Register DPTX0_L2_PATTERN_CTRL(0x2b8)
#define DPTX0_L2_PATTERN_CTRL                                                             0x000002b8 //CRW POD:0x0
#define DPTX0_L2_PAT_SEL_MASK                                                                   0x07
#define DPTX0_L2_PAT_SEL_OFFSET                                                                    0
#define DPTX0_L2_PAT_SEL_WIDTH                                                                     3
#define DPTX0_L2_IDP_LT_EN_MASK                                                                 0x08
#define DPTX0_L2_IDP_LT_EN_OFFSET                                                                  3
#define DPTX0_L2_IDP_LT_EN_WIDTH                                                                   1
#define DPTX0_L2_PATTERN_SET_MASK                                                               0xf0
#define DPTX0_L2_PATTERN_SET_OFFSET                                                                4
#define DPTX0_L2_PATTERN_SET_WIDTH                                                                 4



// Register DPTX0_L3_PATTERN_CTRL(0x2bc)
#define DPTX0_L3_PATTERN_CTRL                                                             0x000002bc //CRW POD:0x0
#define DPTX0_L3_PAT_SEL_MASK                                                                   0x07
#define DPTX0_L3_PAT_SEL_OFFSET                                                                    0
#define DPTX0_L3_PAT_SEL_WIDTH                                                                     3
#define DPTX0_L3_IDP_LT_EN_MASK                                                                 0x08
#define DPTX0_L3_IDP_LT_EN_OFFSET                                                                  3
#define DPTX0_L3_IDP_LT_EN_WIDTH                                                                   1
#define DPTX0_L3_PATTERN_SET_MASK                                                               0xf0
#define DPTX0_L3_PATTERN_SET_OFFSET                                                                4
#define DPTX0_L3_PATTERN_SET_WIDTH                                                                 4



// Register DPTX0_IDP_LT_PAT1_PERIOD(0x2c0)
#define DPTX0_IDP_LT_PAT1_PERIOD                                                          0x000002c0 //RW POD:0x6993
#define DPTX0_IDP_LT_PAT1_PERIOD_MASK                                                         0xffff
#define DPTX0_IDP_LT_PAT1_PERIOD_OFFSET                                                            0
#define DPTX0_IDP_LT_PAT1_PERIOD_WIDTH                                                            16



// Register DPTX0_IDP_LT_PAT2_PERIOD(0x2c8)
#define DPTX0_IDP_LT_PAT2_PERIOD                                                          0x000002c8 //RW POD:0x6993
#define DPTX0_IDP_LT_PAT2_PERIOD_MASK                                                         0xffff
#define DPTX0_IDP_LT_PAT2_PERIOD_OFFSET                                                            0
#define DPTX0_IDP_LT_PAT2_PERIOD_WIDTH                                                            16



// Register DPTX0_IDP_LT_HPD_CTRL(0x2cc)
#define DPTX0_IDP_LT_HPD_CTRL                                                             0x000002cc //RW POD:0xa8
#define DPTX0_MIN_PULSE_WIDTH_MASK                                                              0xff
#define DPTX0_MIN_PULSE_WIDTH_OFFSET                                                               0
#define DPTX0_MIN_PULSE_WIDTH_WIDTH                                                                8






// address block DPTX0 [0x300]


# define DPTX0_BASE_ADDRESS 0x300


// Register DPTX0_MS_CTRL(0x300)
#define DPTX0_MS_CTRL                                                                     0x00000300 //RW POD:0x0
#define DPTX0_MS_VIDEO_EN_MASK                                                                  0x01
#define DPTX0_MS_VIDEO_EN_OFFSET                                                                   0
#define DPTX0_MS_VIDEO_EN_WIDTH                                                                    1
#define DPTX0_MS_VIDEO_EN_FORCE_MASK                                                            0x02
#define DPTX0_MS_VIDEO_EN_FORCE_OFFSET                                                             1
#define DPTX0_MS_VIDEO_EN_FORCE_WIDTH                                                              1
#define DPTX0_MS_MVID_COEF_SEL_MASK                                                             0x04
#define DPTX0_MS_MVID_COEF_SEL_OFFSET                                                              2
#define DPTX0_MS_MVID_COEF_SEL_WIDTH                                                               1
#define DPTX0_MS_FEFIFO_RESET_MASK                                                              0x08
#define DPTX0_MS_FEFIFO_RESET_OFFSET                                                               3
#define DPTX0_MS_FEFIFO_RESET_WIDTH                                                                1
#define DPTX0_MS_DUAL_BUS_EN_MASK                                                               0x10
#define DPTX0_MS_DUAL_BUS_EN_OFFSET                                                                4
#define DPTX0_MS_DUAL_BUS_EN_WIDTH                                                                 1
#define DPTX0_MS_MVID_TYPE_MASK                                                                 0x20
#define DPTX0_MS_MVID_TYPE_OFFSET                                                                  5
#define DPTX0_MS_MVID_TYPE_WIDTH                                                                   1
#define DPTX0_MS_FIELD_ID_INV_MASK                                                              0x40
#define DPTX0_MS_FIELD_ID_INV_OFFSET                                                               6
#define DPTX0_MS_FIELD_ID_INV_WIDTH                                                                1
#define DPTX0_MS_TWO_FIELDS_VTOTAL_MASK                                                         0x80
#define DPTX0_MS_TWO_FIELDS_VTOTAL_OFFSET                                                          7
#define DPTX0_MS_TWO_FIELDS_VTOTAL_WIDTH                                                           1



// Register DPTX0_MS_UPDATE(0x304)
#define DPTX0_MS_UPDATE                                                                   0x00000304 //CRW POD:0x0
#define DPTX0_MS_UPDATE_CTRL_MASK                                                               0x01
#define DPTX0_MS_UPDATE_CTRL_OFFSET                                                                0
#define DPTX0_MS_UPDATE_CTRL_WIDTH                                                                 1
#define DPTX0_MS_UPDATE_MODE_MASK                                                               0x06
#define DPTX0_MS_UPDATE_MODE_OFFSET                                                                1
#define DPTX0_MS_UPDATE_MODE_WIDTH                                                                 2
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_MS_IRQ_CTRL(0x308)
#define DPTX0_MS_IRQ_CTRL                                                                 0x00000308 //RW POD:0x0
#define DPTX0_MS_VID_ABSENT_IRQ_EN_MASK                                                         0x01
#define DPTX0_MS_VID_ABSENT_IRQ_EN_OFFSET                                                          0
#define DPTX0_MS_VID_ABSENT_IRQ_EN_WIDTH                                                           1
#define DPTX0_MS_FEFIFO_ERR_IRQ_EN_MASK                                                         0x02
#define DPTX0_MS_FEFIFO_ERR_IRQ_EN_OFFSET                                                          1
#define DPTX0_MS_FEFIFO_ERR_IRQ_EN_WIDTH                                                           1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX0_MS_LTA_CTRL(0x30c)
#define DPTX0_MS_LTA_CTRL                                                                 0x0000030c //PA POD:0x20 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_TPG_EN_MASK                                                                    0x01
#define DPTX0_MS_TPG_EN_OFFSET                                                                     0
#define DPTX0_MS_TPG_EN_WIDTH                                                                      1
#define DPTX0_MS_TPG_PATTERN_MASK                                                               0x06
#define DPTX0_MS_TPG_PATTERN_OFFSET                                                                1
#define DPTX0_MS_TPG_PATTERN_WIDTH                                                                 2
#define DPTX0_MS_FEFIFO_EOL_OPT_MASK                                                            0x08
#define DPTX0_MS_FEFIFO_EOL_OPT_OFFSET                                                             3
#define DPTX0_MS_FEFIFO_EOL_OPT_WIDTH                                                              1
#define DPTX0_MS_EQUAL_DISTR_DIS_MASK                                                           0x10
#define DPTX0_MS_EQUAL_DISTR_DIS_OFFSET                                                            4
#define DPTX0_MS_EQUAL_DISTR_DIS_WIDTH                                                             1
#define DPTX0_MS_VBLANK_REGEN_EN_MASK                                                           0x20
#define DPTX0_MS_VBLANK_REGEN_EN_OFFSET                                                            5
#define DPTX0_MS_VBLANK_REGEN_EN_WIDTH                                                             1
#define DPTX0_MSA_ADVANCED_DIS_MASK                                                             0x40
#define DPTX0_MSA_ADVANCED_DIS_OFFSET                                                              6
#define DPTX0_MSA_ADVANCED_DIS_WIDTH                                                               1
#define DPTX0_MS_NOISE_PATGEN_EN_MASK                                                           0x80
#define DPTX0_MS_NOISE_PATGEN_EN_OFFSET                                                            7
#define DPTX0_MS_NOISE_PATGEN_EN_WIDTH                                                             1



// Register DPTX0_MS_STATUS(0x310)
#define DPTX0_MS_STATUS                                                                   0x00000310 //CRO POD:0x0
#define DPTX0_MS_VID_ABSENT_IRQ_STS_MASK                                                        0x01
#define DPTX0_MS_VID_ABSENT_IRQ_STS_OFFSET                                                         0
#define DPTX0_MS_VID_ABSENT_IRQ_STS_WIDTH                                                          1
#define DPTX0_MS_FEFIFO_ERROR_STS_MASK                                                          0x02
#define DPTX0_MS_FEFIFO_ERROR_STS_OFFSET                                                           1
#define DPTX0_MS_FEFIFO_ERROR_STS_WIDTH                                                            1
#define DPTX0_MS_STATUS_RSRV_MASK                                                               0xfc
#define DPTX0_MS_STATUS_RSRV_OFFSET                                                                2
#define DPTX0_MS_STATUS_RSRV_WIDTH                                                                 6



// Register DPTX0_TPG_CTRL(0x314)
#define DPTX0_TPG_CTRL                                                                    0x00000314 //RW POD:0x0
#define DPTX0_TPG_OVERLAY_MODE_MASK                                                             0x01
#define DPTX0_TPG_OVERLAY_MODE_OFFSET                                                              0
#define DPTX0_TPG_OVERLAY_MODE_WIDTH                                                               1
#define DPTX0_TPG_FRAME_RESET_LINE_MASK                                                         0x7e
#define DPTX0_TPG_FRAME_RESET_LINE_OFFSET                                                          1
#define DPTX0_TPG_FRAME_RESET_LINE_WIDTH                                                           6
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX0_MS_TU_SIZE(0x318)
#define DPTX0_MS_TU_SIZE                                                                  0x00000318 //RW POD:0x1f
#define DPTX0_MS_TU_SIZE_MASK                                                                   0x3f
#define DPTX0_MS_TU_SIZE_OFFSET                                                                    0
#define DPTX0_MS_TU_SIZE_WIDTH                                                                     6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_MS_VIDEO_STRM_DLY(0x31c)
#define DPTX0_MS_VIDEO_STRM_DLY                                                           0x0000031c //RW POD:0x21
#define DPTX0_MS_VIDEO_STRM_DLY_MASK                                                            0x3f
#define DPTX0_MS_VIDEO_STRM_DLY_OFFSET                                                             0
#define DPTX0_MS_VIDEO_STRM_DLY_WIDTH                                                              6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_MS_PCKT_GUARD_ZN(0x320)
#define DPTX0_MS_PCKT_GUARD_ZN                                                            0x00000320 //RW POD:0x21
#define DPTX0_MS_PCKT_GUARD_ZN_MASK                                                             0x3f
#define DPTX0_MS_PCKT_GUARD_ZN_OFFSET                                                              0
#define DPTX0_MS_PCKT_GUARD_ZN_WIDTH                                                               6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX0_MS_BS_REGEN_CTRL(0x324)
#define DPTX0_MS_BS_REGEN_CTRL                                                            0x00000324 //RW POD:0x0
#define DPTX0_MS_BS_VB_GEN_SRC_MASK                                                             0x03
#define DPTX0_MS_BS_VB_GEN_SRC_OFFSET                                                              0
#define DPTX0_MS_BS_VB_GEN_SRC_WIDTH                                                               2
#define DPTX0_MS_BS_PER_MEAS_SEL_MASK                                                           0x04
#define DPTX0_MS_BS_PER_MEAS_SEL_OFFSET                                                            2
#define DPTX0_MS_BS_PER_MEAS_SEL_WIDTH                                                             1
#define DPTX0_MS_BS_VB_REGEN_DIS_MASK                                                           0x08
#define DPTX0_MS_BS_VB_REGEN_DIS_OFFSET                                                            3
#define DPTX0_MS_BS_VB_REGEN_DIS_WIDTH                                                             1
#define DPTX0_MS_BS_NV_REGEN_DIS_MASK                                                           0x10
#define DPTX0_MS_BS_NV_REGEN_DIS_OFFSET                                                            4
#define DPTX0_MS_BS_NV_REGEN_DIS_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX0_MS_BS_PER_MEAS(0x328)
#define DPTX0_MS_BS_PER_MEAS                                                              0x00000328 //RO POD:0x0
#define DPTX0_MS_BS_PER_MEAS_MASK                                                             0xffff
#define DPTX0_MS_BS_PER_MEAS_OFFSET                                                                0
#define DPTX0_MS_BS_PER_MEAS_WIDTH                                                                16



// Register DPTX0_MS_BS_VB_PER(0x32c)
#define DPTX0_MS_BS_VB_PER                                                                0x0000032c //RW POD:0x0
#define DPTX0_MS_BS_VB_PER_MASK                                                               0xffff
#define DPTX0_MS_BS_VB_PER_OFFSET                                                                  0
#define DPTX0_MS_BS_VB_PER_WIDTH                                                                  16



// Register DPTX0_MS_BS_NV_PER(0x330)
#define DPTX0_MS_BS_NV_PER                                                                0x00000330 //RW POD:0xffff
#define DPTX0_MS_BS_NV_PER_MASK                                                               0xffff
#define DPTX0_MS_BS_NV_PER_OFFSET                                                                  0
#define DPTX0_MS_BS_NV_PER_WIDTH                                                                  16



// Register DPTX0_MS_SR_PER(0x334)
#define DPTX0_MS_SR_PER                                                                   0x00000334 //RW POD:0x0
#define DPTX0_MS_SR_PER_MASK                                                                    0xff
#define DPTX0_MS_SR_PER_OFFSET                                                                     0
#define DPTX0_MS_SR_PER_WIDTH                                                                      8



// Register DPTX0_MS_FORMAT_0(0x340)
#define DPTX0_MS_FORMAT_0                                                                 0x00000340 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_CLOCK_TYPE_MASK                                                                0x01
#define DPTX0_MS_CLOCK_TYPE_OFFSET                                                                 0
#define DPTX0_MS_CLOCK_TYPE_WIDTH                                                                  1
#define DPTX0_MS_COLOR_SPACE_MASK                                                               0x06
#define DPTX0_MS_COLOR_SPACE_OFFSET                                                                1
#define DPTX0_MS_COLOR_SPACE_WIDTH                                                                 2
#define DPTX0_MS_DYNAMIC_RNG_MASK                                                               0x08
#define DPTX0_MS_DYNAMIC_RNG_OFFSET                                                                3
#define DPTX0_MS_DYNAMIC_RNG_WIDTH                                                                 1
#define DPTX0_MS_COLOR_COEFF_MASK                                                               0x10
#define DPTX0_MS_COLOR_COEFF_OFFSET                                                                4
#define DPTX0_MS_COLOR_COEFF_WIDTH                                                                 1
#define DPTX0_MS_COLOR_DEPTH_MASK                                                               0xe0
#define DPTX0_MS_COLOR_DEPTH_OFFSET                                                                5
#define DPTX0_MS_COLOR_DEPTH_WIDTH                                                                 3



// Register DPTX0_MS_FORMAT_1(0x344)
#define DPTX0_MS_FORMAT_1                                                                 0x00000344 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_VTOTAL_EVEN_MASK                                                               0x01
#define DPTX0_MS_VTOTAL_EVEN_OFFSET                                                                0
#define DPTX0_MS_VTOTAL_EVEN_WIDTH                                                                 1
#define DPTX0_MS_STEREO_VIDEO_MASK                                                              0x02
#define DPTX0_MS_STEREO_VIDEO_OFFSET                                                               1
#define DPTX0_MS_STEREO_VIDEO_WIDTH                                                                1
#define DPTX0_MS_STEREO_MODE_MASK                                                               0x04
#define DPTX0_MS_STEREO_MODE_OFFSET                                                                2
#define DPTX0_MS_STEREO_MODE_WIDTH                                                                 1
#define DPTX0_MS_INTERLACED_MODE_MASK                                                           0x08
#define DPTX0_MS_INTERLACED_MODE_OFFSET                                                            3
#define DPTX0_MS_INTERLACED_MODE_WIDTH                                                             1
#define DPTX0_MS_FORMAT_RSRV_MASK                                                               0xf0
#define DPTX0_MS_FORMAT_RSRV_OFFSET                                                                4
#define DPTX0_MS_FORMAT_RSRV_WIDTH                                                                 4



// Register DPTX0_MS_HTOTAL(0x348)
#define DPTX0_MS_HTOTAL                                                                   0x00000348 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_HTOTAL_MASK                                                                  0xffff
#define DPTX0_MS_HTOTAL_OFFSET                                                                     0
#define DPTX0_MS_HTOTAL_WIDTH                                                                     16



// Register DPTX0_MS_HACT_START(0x34c)
#define DPTX0_MS_HACT_START                                                               0x0000034c //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_HACT_START_MASK                                                              0xffff
#define DPTX0_MS_HACT_START_OFFSET                                                                 0
#define DPTX0_MS_HACT_START_WIDTH                                                                 16



// Register DPTX0_MS_HACT_WIDTH(0x350)
#define DPTX0_MS_HACT_WIDTH                                                               0x00000350 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_HACT_WIDTH_MASK                                                              0xffff
#define DPTX0_MS_HACT_WIDTH_OFFSET                                                                 0
#define DPTX0_MS_HACT_WIDTH_WIDTH                                                                 16



// Register DPTX0_MS_HSYNC_WIDTH(0x354)
#define DPTX0_MS_HSYNC_WIDTH                                                              0x00000354 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_HSYNC_WIDTH_MASK                                                             0x7fff
#define DPTX0_MS_HSYNC_WIDTH_OFFSET                                                                0
#define DPTX0_MS_HSYNC_WIDTH_WIDTH                                                                15
#define DPTX0_MS_HPOLARITY_MASK                                                               0x8000
#define DPTX0_MS_HPOLARITY_OFFSET                                                                 15
#define DPTX0_MS_HPOLARITY_WIDTH                                                                   1



// Register DPTX0_MS_VTOTAL(0x358)
#define DPTX0_MS_VTOTAL                                                                   0x00000358 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_VTOTAL_MASK                                                                  0xffff
#define DPTX0_MS_VTOTAL_OFFSET                                                                     0
#define DPTX0_MS_VTOTAL_WIDTH                                                                     16



// Register DPTX0_MS_VACT_START(0x35c)
#define DPTX0_MS_VACT_START                                                               0x0000035c //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_VACT_START_MASK                                                              0xffff
#define DPTX0_MS_VACT_START_OFFSET                                                                 0
#define DPTX0_MS_VACT_START_WIDTH                                                                 16



// Register DPTX0_MS_VACT_WIDTH(0x360)
#define DPTX0_MS_VACT_WIDTH                                                               0x00000360 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_VACT_WIDTH_MASK                                                              0xffff
#define DPTX0_MS_VACT_WIDTH_OFFSET                                                                 0
#define DPTX0_MS_VACT_WIDTH_WIDTH                                                                 16



// Register DPTX0_MS_VSYNC_WIDTH(0x364)
#define DPTX0_MS_VSYNC_WIDTH                                                              0x00000364 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_VSYNC_WIDTH_MASK                                                             0x7fff
#define DPTX0_MS_VSYNC_WIDTH_OFFSET                                                                0
#define DPTX0_MS_VSYNC_WIDTH_WIDTH                                                                15
#define DPTX0_MS_VPOLARITY_MASK                                                               0x8000
#define DPTX0_MS_VPOLARITY_OFFSET                                                                 15
#define DPTX0_MS_VPOLARITY_WIDTH                                                                   1



// Register DPTX0_MS_M(0x368)
#define DPTX0_MS_M                                                                        0x00000368 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_M_MASK                                                                     0xffffff
#define DPTX0_MS_M_OFFSET                                                                          0
#define DPTX0_MS_M_WIDTH                                                                          24



// Register DPTX0_MS_M_MEAS(0x36c)
#define DPTX0_MS_M_MEAS                                                                   0x0000036c //SRW POD:0x0
#define DPTX0_MS_M_MEAS_MASK                                                                0xffffff
#define DPTX0_MS_M_MEAS_OFFSET                                                                     0
#define DPTX0_MS_M_MEAS_WIDTH                                                                     24



// Register DPTX0_MS_N(0x370)
#define DPTX0_MS_N                                                                        0x00000370 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_N_MASK                                                                     0xffffff
#define DPTX0_MS_N_OFFSET                                                                          0
#define DPTX0_MS_N_WIDTH                                                                          24



// Register DPTX0_MS_TH_CTRL(0x374)
#define DPTX0_MS_TH_CTRL                                                                  0x00000374 //RW POD:0x0
#define DPTX0_MS_M_LOW_TH_MASK                                                                  0x07
#define DPTX0_MS_M_LOW_TH_OFFSET                                                                   0
#define DPTX0_MS_M_LOW_TH_WIDTH                                                                    3
#define DPTX0_MS_TH_RSRV1_MASK                                                                  0x08
#define DPTX0_MS_TH_RSRV1_OFFSET                                                                   3
#define DPTX0_MS_TH_RSRV1_WIDTH                                                                    1
#define DPTX0_MS_DE_TH_MASK                                                                     0x70
#define DPTX0_MS_DE_TH_OFFSET                                                                      4
#define DPTX0_MS_DE_TH_WIDTH                                                                       3
#define DPTX0_MS_TH_RSRV2_MASK                                                                  0x80
#define DPTX0_MS_TH_RSRV2_OFFSET                                                                   7
#define DPTX0_MS_TH_RSRV2_WIDTH                                                                    1



// Register DPTX0_MS_VID_STATUS(0x378)
#define DPTX0_MS_VID_STATUS                                                               0x00000378 //RO POD:0x0
#define DPTX0_EXT_VIDEO_STS_MASK                                                                0x01
#define DPTX0_EXT_VIDEO_STS_OFFSET                                                                 0
#define DPTX0_EXT_VIDEO_STS_WIDTH                                                                  1
#define DPTX0_VIDEO_CLK_STS_MASK                                                                0x02
#define DPTX0_VIDEO_CLK_STS_OFFSET                                                                 1
#define DPTX0_VIDEO_CLK_STS_WIDTH                                                                  1
#define DPTX0_DE_STS_MASK                                                                       0x04
#define DPTX0_DE_STS_OFFSET                                                                        2
#define DPTX0_DE_STS_WIDTH                                                                         1
#define DPTX0_VBLANK_STS_MASK                                                                   0x08
#define DPTX0_VBLANK_STS_OFFSET                                                                    3
#define DPTX0_VBLANK_STS_WIDTH                                                                     1
#define DPTX0_MS_VID_STS_MASK                                                                   0xf0
#define DPTX0_MS_VID_STS_OFFSET                                                                    4
#define DPTX0_MS_VID_STS_WIDTH                                                                     4



// Register DPTX0_MSA_CTRL(0x37c)
#define DPTX0_MSA_CTRL                                                                    0x0000037c //RW POD:0x0
#define DPTX0_MSA_VB_TX_COUNT_MASK                                                            0x0007
#define DPTX0_MSA_VB_TX_COUNT_OFFSET                                                               0
#define DPTX0_MSA_VB_TX_COUNT_WIDTH                                                                3
#define DPTX0_VBLANK_DEL_EN_MASK                                                              0x0008
#define DPTX0_VBLANK_DEL_EN_OFFSET                                                                 3
#define DPTX0_VBLANK_DEL_EN_WIDTH                                                                  1
#define DPTX0_MSA_NV_TX_COUNT_MASK                                                            0x0070
#define DPTX0_MSA_NV_TX_COUNT_OFFSET                                                               4
#define DPTX0_MSA_NV_TX_COUNT_WIDTH                                                                3
#define DPTX0_MSA_NV_TX_EN_MASK                                                               0x0080
#define DPTX0_MSA_NV_TX_EN_OFFSET                                                                  7
#define DPTX0_MSA_NV_TX_EN_WIDTH                                                                   1
#define DPTX0_MSA_VB_TX_POSITION_MASK                                                         0xff00
#define DPTX0_MSA_VB_TX_POSITION_OFFSET                                                            8
#define DPTX0_MSA_VB_TX_POSITION_WIDTH                                                             8



// Register DPTX0_MS_RSRV(0x380)
#define DPTX0_MS_RSRV                                                                     0x00000380 //RW POD:0x0
#define DPTX0_MS_DATPAK_FIFO_STALL_MASK                                                         0x01
#define DPTX0_MS_DATPAK_FIFO_STALL_OFFSET                                                          0
#define DPTX0_MS_DATPAK_FIFO_STALL_WIDTH                                                           1
#define DPTX0_MS_VIDEO_RESET_MASK                                                               0x02
#define DPTX0_MS_VIDEO_RESET_OFFSET                                                                1
#define DPTX0_MS_VIDEO_RESET_WIDTH                                                                 1
#define DPTX0_MS_M_MEAS_FORCE_MASK                                                              0x04
#define DPTX0_MS_M_MEAS_FORCE_OFFSET                                                               2
#define DPTX0_MS_M_MEAS_FORCE_WIDTH                                                                1
#define DPTX0_MS_SCNDR_RESET_MASK                                                               0x08
#define DPTX0_MS_SCNDR_RESET_OFFSET                                                                3
#define DPTX0_MS_SCNDR_RESET_WIDTH                                                                 1
#define DPTX0_AUTO_SECOND_PACK_RST_MASK                                                         0x10
#define DPTX0_AUTO_SECOND_PACK_RST_OFFSET                                                          4
#define DPTX0_AUTO_SECOND_PACK_RST_WIDTH                                                           1
#define DPTX0_MS_IDP_MODE_MASK                                                                  0x20
#define DPTX0_MS_IDP_MODE_OFFSET                                                                   5
#define DPTX0_MS_IDP_MODE_WIDTH                                                                    1
#define DPTX0_MSA_DISABLE_MASK                                                                  0x40
#define DPTX0_MSA_DISABLE_OFFSET                                                                   6
#define DPTX0_MSA_DISABLE_WIDTH                                                                    1
#define DPTX0_MS_RSRV_MASK                                                                      0x80
#define DPTX0_MS_RSRV_OFFSET                                                                       7
#define DPTX0_MS_RSRV_WIDTH                                                                        1



// Register DPTX0_MS_IDP_DATA_LN_CNT(0x384)
#define DPTX0_MS_IDP_DATA_LN_CNT                                                          0x00000384 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_IDP_DATA_LN_CNT_MASK                                                           0x0f
#define DPTX0_MS_IDP_DATA_LN_CNT_OFFSET                                                            0
#define DPTX0_MS_IDP_DATA_LN_CNT_WIDTH                                                             4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_MS_IDP_PHY_LN_CNT(0x388)
#define DPTX0_MS_IDP_PHY_LN_CNT                                                           0x00000388 //RW POD:0x0
#define DPTX0_MS_IDP_PHY_LN_CNT_MASK                                                            0x0f
#define DPTX0_MS_IDP_PHY_LN_CNT_OFFSET                                                             0
#define DPTX0_MS_IDP_PHY_LN_CNT_WIDTH                                                              4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX0_MS_IDP_PACK_CTRL(0x38c)
#define DPTX0_MS_IDP_PACK_CTRL                                                            0x0000038c //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_MS_IDP_PACK_MODE_MASK                                                             0x07
#define DPTX0_MS_IDP_PACK_MODE_OFFSET                                                              0
#define DPTX0_MS_IDP_PACK_MODE_WIDTH                                                               3
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX0_FE_FIFO_CTRL(0x390)
#define DPTX0_FE_FIFO_CTRL                                                                0x00000390 //RW POD:0x0
#define DPTX0_ENABLE_FE_FIFO_ACC_MASK                                                           0x01
#define DPTX0_ENABLE_FE_FIFO_ACC_OFFSET                                                            0
#define DPTX0_ENABLE_FE_FIFO_ACC_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX0_FE_FIFO_ACC(0x394)
#define DPTX0_FE_FIFO_ACC                                                                 0x00000394 //RW POD:0x0
#define DPTX0_FE_FIFO_READ_RATE_MASK                                                          0xffff
#define DPTX0_FE_FIFO_READ_RATE_OFFSET                                                             0
#define DPTX0_FE_FIFO_READ_RATE_WIDTH                                                             16



// Register DPTX0_SDP_RSRV(0x398)
#define DPTX0_SDP_RSRV                                                                    0x00000398 //RW POD:0x0
#define DPTX0_SDP_RSRV_MASK                                                                     0xff
#define DPTX0_SDP_RSRV_OFFSET                                                                      0
#define DPTX0_SDP_RSRV_WIDTH                                                                       8



// Register DPTX0_TEST_CRC_CTRL(0x3a0)
#define DPTX0_TEST_CRC_CTRL                                                               0x000003a0 //PA POD:0x0 Event:DPTX0_MS_UPDATE
#define DPTX0_CRC_EN_MASK                                                                       0x01
#define DPTX0_CRC_EN_OFFSET                                                                        0
#define DPTX0_CRC_EN_WIDTH                                                                         1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX0_TEST_CRC_R(0x3a4)
#define DPTX0_TEST_CRC_R                                                                  0x000003a4 //RO POD:0x0
#define DPTX0_TEST_CRC_R_MASK                                                                 0xffff
#define DPTX0_TEST_CRC_R_OFFSET                                                                    0
#define DPTX0_TEST_CRC_R_WIDTH                                                                    16



// Register DPTX0_TEST_CRC_G(0x3a8)
#define DPTX0_TEST_CRC_G                                                                  0x000003a8 //RO POD:0x0
#define DPTX0_TEST_CRC_G_MASK                                                                 0xffff
#define DPTX0_TEST_CRC_G_OFFSET                                                                    0
#define DPTX0_TEST_CRC_G_WIDTH                                                                    16



// Register DPTX0_TEST_CRC_B(0x3ac)
#define DPTX0_TEST_CRC_B                                                                  0x000003ac //RO POD:0x0
#define DPTX0_TEST_CRC_B_MASK                                                                 0xffff
#define DPTX0_TEST_CRC_B_OFFSET                                                                    0
#define DPTX0_TEST_CRC_B_WIDTH                                                                    16






// address block DPTX1_TOP [0x810]


# define DPTX1_TOP_BASE_ADDRESS 0x810


// Register DPTX1_CONTROL(0x810)
#define DPTX1_CONTROL                                                                     0x00000810 //RW POD:0x0
#define DPTX1_EN_MASK                                                                           0x01
#define DPTX1_EN_OFFSET                                                                            0
#define DPTX1_EN_WIDTH                                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX1_RESET_CTRL(0x814)
#define DPTX1_RESET_CTRL                                                                  0x00000814 //CRW POD:0xf
#define DPTX1_RESET_AUX_CLK_MASK                                                                0x01
#define DPTX1_RESET_AUX_CLK_OFFSET                                                                 0
#define DPTX1_RESET_AUX_CLK_WIDTH                                                                  1
#define DPTX1_RESET_LINK_CLK_MASK                                                               0x02
#define DPTX1_RESET_LINK_CLK_OFFSET                                                                1
#define DPTX1_RESET_LINK_CLK_WIDTH                                                                 1
#define DPTX1_RESET_VIDEO_CLK_MASK                                                              0x04
#define DPTX1_RESET_VIDEO_CLK_OFFSET                                                               2
#define DPTX1_RESET_VIDEO_CLK_WIDTH                                                                1
#define DPTX1_RESET_TCLK_CLK_MASK                                                               0x08
#define DPTX1_RESET_TCLK_CLK_OFFSET                                                                3
#define DPTX1_RESET_TCLK_CLK_WIDTH                                                                 1
#define DPTX1_RESET_UART_CLK_MASK                                                               0x10
#define DPTX1_RESET_UART_CLK_OFFSET                                                                4
#define DPTX1_RESET_UART_CLK_WIDTH                                                                 1
#define DPTX1_RESET_RESERV_MASK                                                                 0xe0
#define DPTX1_RESET_RESERV_OFFSET                                                                  5
#define DPTX1_RESET_RESERV_WIDTH                                                                   3



// Register DPTX1_IRQ_STATUS(0x818)
#define DPTX1_IRQ_STATUS                                                                  0x00000818 //RO POD:0x0
#define DPTX1_AUXCH_IRQ_STS_MASK                                                                0x01
#define DPTX1_AUXCH_IRQ_STS_OFFSET                                                                 0
#define DPTX1_AUXCH_IRQ_STS_WIDTH                                                                  1
#define DPTX1_LT_IRQ_STS_MASK                                                                   0x02
#define DPTX1_LT_IRQ_STS_OFFSET                                                                    1
#define DPTX1_LT_IRQ_STS_WIDTH                                                                     1
#define DPTX1_MS_IRQ_STS_MASK                                                                   0x04
#define DPTX1_MS_IRQ_STS_OFFSET                                                                    2
#define DPTX1_MS_IRQ_STS_WIDTH                                                                     1
#define DPTX1_HDCP_IRQ_STS_MASK                                                                 0x08
#define DPTX1_HDCP_IRQ_STS_OFFSET                                                                  3
#define DPTX1_HDCP_IRQ_STS_WIDTH                                                                   1
#define DPTX1_HPD_IN_IRQ_STS_MASK                                                               0x10
#define DPTX1_HPD_IN_IRQ_STS_OFFSET                                                                4
#define DPTX1_HPD_IN_IRQ_STS_WIDTH                                                                 1
#define DPTX1_AUX_UART_IRQ_STS_MASK                                                             0x20
#define DPTX1_AUX_UART_IRQ_STS_OFFSET                                                              5
#define DPTX1_AUX_UART_IRQ_STS_WIDTH                                                               1
#define DPTX1_PHYA_IRQ_STS_MASK                                                                 0x40
#define DPTX1_PHYA_IRQ_STS_OFFSET                                                                  6
#define DPTX1_PHYA_IRQ_STS_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX1_HPD_DETECTOR_CONTROL(0x820)
#define DPTX1_HPD_DETECTOR_CONTROL                                                        0x00000820 //RW POD:0x9b
#define DPTX1_HPD_CLK_DIV_MASK                                                                  0x7f
#define DPTX1_HPD_CLK_DIV_OFFSET                                                                   0
#define DPTX1_HPD_CLK_DIV_WIDTH                                                                    7
#define DPTX1_HPD_DEGLITCH_EN_MASK                                                              0x80
#define DPTX1_HPD_DEGLITCH_EN_OFFSET                                                               7
#define DPTX1_HPD_DEGLITCH_EN_WIDTH                                                                1



// Register DPTX1_HPD_PULSE_LEVEL(0x824)
#define DPTX1_HPD_PULSE_LEVEL                                                             0x00000824 //RW POD:0x4
#define DPTX1_HPD_PULSE_LEVEL_MASK                                                              0xff
#define DPTX1_HPD_PULSE_LEVEL_OFFSET                                                               0
#define DPTX1_HPD_PULSE_LEVEL_WIDTH                                                                8



// Register DPTX1_HPD_UNPLUG_LEVEL(0x828)
#define DPTX1_HPD_UNPLUG_LEVEL                                                            0x00000828 //RW POD:0xa0
#define DPTX1_HPD_UNPLUG_LEVEL_MASK                                                             0xff
#define DPTX1_HPD_UNPLUG_LEVEL_OFFSET                                                              0
#define DPTX1_HPD_UNPLUG_LEVEL_WIDTH                                                               8



// Register DPTX1_HPD_IRQ_CTRL(0x82c)
#define DPTX1_HPD_IRQ_CTRL                                                                0x0000082c //RW POD:0x0
#define DPTX1_HPD_PULSE_IRQ_EN_MASK                                                             0x01
#define DPTX1_HPD_PULSE_IRQ_EN_OFFSET                                                              0
#define DPTX1_HPD_PULSE_IRQ_EN_WIDTH                                                               1
#define DPTX1_HPD_UNPLUG_IRQ_EN_MASK                                                            0x02
#define DPTX1_HPD_UNPLUG_IRQ_EN_OFFSET                                                             1
#define DPTX1_HPD_UNPLUG_IRQ_EN_WIDTH                                                              1
#define DPTX1_HPD_PLUG_IRQ_EN_MASK                                                              0x04
#define DPTX1_HPD_PLUG_IRQ_EN_OFFSET                                                               2
#define DPTX1_HPD_PLUG_IRQ_EN_WIDTH                                                                1
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_HPD_IRQ_STATUS(0x830)
#define DPTX1_HPD_IRQ_STATUS                                                              0x00000830 //CRO POD:0x0
#define DPTX1_HPD_PULSE_IRQ_STS_MASK                                                            0x01
#define DPTX1_HPD_PULSE_IRQ_STS_OFFSET                                                             0
#define DPTX1_HPD_PULSE_IRQ_STS_WIDTH                                                              1
#define DPTX1_HPD_UNPLUG_IRQ_STS_MASK                                                           0x02
#define DPTX1_HPD_UNPLUG_IRQ_STS_OFFSET                                                            1
#define DPTX1_HPD_UNPLUG_IRQ_STS_WIDTH                                                             1
#define DPTX1_HPD_PLUG_IRQ_STS_MASK                                                             0x04
#define DPTX1_HPD_PLUG_IRQ_STS_OFFSET                                                              2
#define DPTX1_HPD_PLUG_IRQ_STS_WIDTH                                                               1
// #define Reserved_MASK                                                                           0x78
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             4
#define DPTX1_HPD_RAW_STATUS_MASK                                                               0x80
#define DPTX1_HPD_RAW_STATUS_OFFSET                                                                7
#define DPTX1_HPD_RAW_STATUS_WIDTH                                                                 1



// Register DPTX1_HPD_IN_DBNC_TIME(0x834)
#define DPTX1_HPD_IN_DBNC_TIME                                                            0x00000834 //RW POD:0x3
#define DPTX1_HPD_IN_DBNC_TIME_MASK                                                             0x07
#define DPTX1_HPD_IN_DBNC_TIME_OFFSET                                                              0
#define DPTX1_HPD_IN_DBNC_TIME_WIDTH                                                               3
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_AUXREFCLK_DIV(0x838)
#define DPTX1_AUXREFCLK_DIV                                                               0x00000838 //RW POD:0x6c
#define DPTX1_AUXREFCLK_DIV_MASK                                                                0xff
#define DPTX1_AUXREFCLK_DIV_OFFSET                                                                 0
#define DPTX1_AUXREFCLK_DIV_WIDTH                                                                  8



// Register DPTX1_TEST_CTRL(0x850)
#define DPTX1_TEST_CTRL                                                                   0x00000850 //RW POD:0x0
// #define Reserved_MASK                                                                     0x00000001
// #define Reserved_OFFSET                                                                            0
// #define Reserved_WIDTH                                                                             1
#define DPTX1_PHYD_IN_SEL_MASK                                                            0x00000002
#define DPTX1_PHYD_IN_SEL_OFFSET                                                                   1
#define DPTX1_PHYD_IN_SEL_WIDTH                                                                    1
#define DPTX1_PHYA_IN_SEL_MASK                                                            0x00000004
#define DPTX1_PHYA_IN_SEL_OFFSET                                                                   2
#define DPTX1_PHYA_IN_SEL_WIDTH                                                                    1
#define DPTX1_AUX_TEST_IN_EN_MASK                                                         0x00000008
#define DPTX1_AUX_TEST_IN_EN_OFFSET                                                                3
#define DPTX1_AUX_TEST_IN_EN_WIDTH                                                                 1
#define DPTX1_HPD_LOOPBACK_EN_MASK                                                        0x00000010
#define DPTX1_HPD_LOOPBACK_EN_OFFSET                                                               4
#define DPTX1_HPD_LOOPBACK_EN_WIDTH                                                                1
#define DPTX1_TEST_CTRL_RESRV_MASK                                                        0x000000e0
#define DPTX1_TEST_CTRL_RESRV_OFFSET                                                               5
#define DPTX1_TEST_CTRL_RESRV_WIDTH                                                                3
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register DPTX1_TEST_BUS_SEL(0x854)
#define DPTX1_TEST_BUS_SEL                                                                0x00000854 //RW POD:0x0
#define DPTX1_TEST_BLOCK_SEL_MASK                                                         0x00000007
#define DPTX1_TEST_BLOCK_SEL_OFFSET                                                                0
#define DPTX1_TEST_BLOCK_SEL_WIDTH                                                                 3
#define DPTX1_TEST_BUS_SEL_MASK                                                           0x000000f8
#define DPTX1_TEST_BUS_SEL_OFFSET                                                                  3
#define DPTX1_TEST_BUS_SEL_WIDTH                                                                   5
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24



// Register DPTX1_TEST_BUS_SEL1(0x858)
#define DPTX1_TEST_BUS_SEL1                                                               0x00000858 //RW POD:0x0
#define DPTX1_TEST_BLOCK_SEL1_MASK                                                        0x00000007
#define DPTX1_TEST_BLOCK_SEL1_OFFSET                                                               0
#define DPTX1_TEST_BLOCK_SEL1_WIDTH                                                                3
#define DPTX1_TEST_BUS_SEL1_MASK                                                          0x000000f8
#define DPTX1_TEST_BUS_SEL1_OFFSET                                                                 3
#define DPTX1_TEST_BUS_SEL1_WIDTH                                                                  5
// #define Reserved_MASK                                                                     0xffffff00
// #define Reserved_OFFSET                                                                            8
// #define Reserved_WIDTH                                                                            24






// address block DPTX1_PHYA [0x880]


# define DPTX1_PHYA_BASE_ADDRESS 0x880


// Register DPTX1_PHY_CTRL(0x880)
#define DPTX1_PHY_CTRL                                                                    0x00000880 //RW POD:0xf
#define DPTX1_TXL0_PD_MASK                                                                      0x01
#define DPTX1_TXL0_PD_OFFSET                                                                       0
#define DPTX1_TXL0_PD_WIDTH                                                                        1
#define DPTX1_TXL1_PD_MASK                                                                      0x02
#define DPTX1_TXL1_PD_OFFSET                                                                       1
#define DPTX1_TXL1_PD_WIDTH                                                                        1
#define DPTX1_TXL2_PD_MASK                                                                      0x04
#define DPTX1_TXL2_PD_OFFSET                                                                       2
#define DPTX1_TXL2_PD_WIDTH                                                                        1
#define DPTX1_TXL3_PD_MASK                                                                      0x08
#define DPTX1_TXL3_PD_OFFSET                                                                       3
#define DPTX1_TXL3_PD_WIDTH                                                                        1
#define DPTX1_TXL0_RESET_MASK                                                                   0x10
#define DPTX1_TXL0_RESET_OFFSET                                                                    4
#define DPTX1_TXL0_RESET_WIDTH                                                                     1
#define DPTX1_TXL1_RESET_MASK                                                                   0x20
#define DPTX1_TXL1_RESET_OFFSET                                                                    5
#define DPTX1_TXL1_RESET_WIDTH                                                                     1
#define DPTX1_TXL2_RESET_MASK                                                                   0x40
#define DPTX1_TXL2_RESET_OFFSET                                                                    6
#define DPTX1_TXL2_RESET_WIDTH                                                                     1
#define DPTX1_TXL3_RESET_MASK                                                                   0x80
#define DPTX1_TXL3_RESET_OFFSET                                                                    7
#define DPTX1_TXL3_RESET_WIDTH                                                                     1



// Register DPTX1_CLK_CTRL(0x884)
#define DPTX1_CLK_CTRL                                                                    0x00000884 //RW POD:0x4
#define DPTX1_CLKCTRL_RSRV1_MASK                                                                0x03
#define DPTX1_CLKCTRL_RSRV1_OFFSET                                                                 0
#define DPTX1_CLKCTRL_RSRV1_WIDTH                                                                  2
#define DPTX1_LSCLK_135_SRC_MASK                                                                0x04
#define DPTX1_LSCLK_135_SRC_OFFSET                                                                 2
#define DPTX1_LSCLK_135_SRC_WIDTH                                                                  1
#define DPTX1_CLKCTRL_RSRV2_MASK                                                                0x08
#define DPTX1_CLKCTRL_RSRV2_OFFSET                                                                 3
#define DPTX1_CLKCTRL_RSRV2_WIDTH                                                                  1
#define DPTX1_L0_LSCLK_135_INV_MASK                                                             0x10
#define DPTX1_L0_LSCLK_135_INV_OFFSET                                                              4
#define DPTX1_L0_LSCLK_135_INV_WIDTH                                                               1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX1_PHYFIFO_CTRL(0x89c)
#define DPTX1_PHYFIFO_CTRL                                                                0x0000089c //CRW POD:0x0
#define DPTX1_PHYFIFO_INIT_MASK                                                                 0x01
#define DPTX1_PHYFIFO_INIT_OFFSET                                                                  0
#define DPTX1_PHYFIFO_INIT_WIDTH                                                                   1
#define DPTX1_PHYFIFO_BYPASS_MASK                                                               0x02
#define DPTX1_PHYFIFO_BYPASS_OFFSET                                                                1
#define DPTX1_PHYFIFO_BYPASS_WIDTH                                                                 1
#define DPTX1_PHYFIFO_RDCLK_INV_L0_MASK                                                         0x04
#define DPTX1_PHYFIFO_RDCLK_INV_L0_OFFSET                                                          2
#define DPTX1_PHYFIFO_RDCLK_INV_L0_WIDTH                                                           1
#define DPTX1_PHYFIFO_RDCLK_INV_L1_MASK                                                         0x08
#define DPTX1_PHYFIFO_RDCLK_INV_L1_OFFSET                                                          3
#define DPTX1_PHYFIFO_RDCLK_INV_L1_WIDTH                                                           1
#define DPTX1_PHYFIFO_RDCLK_INV_L2_MASK                                                         0x10
#define DPTX1_PHYFIFO_RDCLK_INV_L2_OFFSET                                                          4
#define DPTX1_PHYFIFO_RDCLK_INV_L2_WIDTH                                                           1
#define DPTX1_PHYFIFO_RDCLK_INV_L3_MASK                                                         0x20
#define DPTX1_PHYFIFO_RDCLK_INV_L3_OFFSET                                                          5
#define DPTX1_PHYFIFO_RDCLK_INV_L3_WIDTH                                                           1
#define DPTX1_PHYFIFO_RSRV_MASK                                                                 0xc0
#define DPTX1_PHYFIFO_RSRV_OFFSET                                                                  6
#define DPTX1_PHYFIFO_RSRV_WIDTH                                                                   2



// Register DPTX1_L0_GAIN(0x8a0)
#define DPTX1_L0_GAIN                                                                     0x000008a0 //RW POD:0x0
#define DPTX1_L0_G_MASK                                                                         0x3f
#define DPTX1_L0_G_OFFSET                                                                          0
#define DPTX1_L0_G_WIDTH                                                                           6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_GAIN1(0x8a4)
#define DPTX1_L0_GAIN1                                                                    0x000008a4 //RW POD:0x0
#define DPTX1_L0_G1_MASK                                                                        0x3f
#define DPTX1_L0_G1_OFFSET                                                                         0
#define DPTX1_L0_G1_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_GAIN2(0x8a8)
#define DPTX1_L0_GAIN2                                                                    0x000008a8 //RW POD:0x0
#define DPTX1_L0_G2_MASK                                                                        0x3f
#define DPTX1_L0_G2_OFFSET                                                                         0
#define DPTX1_L0_G2_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_GAIN3(0x8ac)
#define DPTX1_L0_GAIN3                                                                    0x000008ac //RW POD:0x0
#define DPTX1_L0_G3_MASK                                                                        0x3f
#define DPTX1_L0_G3_OFFSET                                                                         0
#define DPTX1_L0_G3_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_PRE_EMPH(0x8b0)
#define DPTX1_L0_PRE_EMPH                                                                 0x000008b0 //RW POD:0x0
#define DPTX1_L0_P_MASK                                                                         0x3f
#define DPTX1_L0_P_OFFSET                                                                          0
#define DPTX1_L0_P_WIDTH                                                                           6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_PRE_EMPH1(0x8b4)
#define DPTX1_L0_PRE_EMPH1                                                                0x000008b4 //RW POD:0x0
#define DPTX1_L0_P1_MASK                                                                        0x3f
#define DPTX1_L0_P1_OFFSET                                                                         0
#define DPTX1_L0_P1_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_PRE_EMPH2(0x8b8)
#define DPTX1_L0_PRE_EMPH2                                                                0x000008b8 //RW POD:0x0
#define DPTX1_L0_P2_MASK                                                                        0x3f
#define DPTX1_L0_P2_OFFSET                                                                         0
#define DPTX1_L0_P2_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_L0_PRE_EMPH3(0x8bc)
#define DPTX1_L0_PRE_EMPH3                                                                0x000008bc //RW POD:0x0
#define DPTX1_L0_P3_MASK                                                                        0x3f
#define DPTX1_L0_P3_OFFSET                                                                         0
#define DPTX1_L0_P3_WIDTH                                                                          6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_TXRSRVBITS(0x8c0)
#define DPTX1_TXRSRVBITS                                                                  0x000008c0 //RW POD:0x0
#define DPTX1_TXRSRVBITS_MASK                                                                 0xffff
#define DPTX1_TXRSRVBITS_OFFSET                                                                    0
#define DPTX1_TXRSRVBITS_WIDTH                                                                    16



// Register DPTX1_BIST_CONTROL(0x8cc)
#define DPTX1_BIST_CONTROL                                                                0x000008cc //RW POD:0x0
#define DPTX1_BIST_MODE_MASK                                                                    0x01
#define DPTX1_BIST_MODE_OFFSET                                                                     0
#define DPTX1_BIST_MODE_WIDTH                                                                      1
#define DPTX1_BIST_PRBS_EN_MASK                                                                 0x02
#define DPTX1_BIST_PRBS_EN_OFFSET                                                                  1
#define DPTX1_BIST_PRBS_EN_WIDTH                                                                   1
#define DPTX1_BIST_CODE_ORDER_MASK                                                              0x04
#define DPTX1_BIST_CODE_ORDER_OFFSET                                                               2
#define DPTX1_BIST_CODE_ORDER_WIDTH                                                                1
#define DPTX1_BIST_CLK_INV_MASK                                                                 0x08
#define DPTX1_BIST_CLK_INV_OFFSET                                                                  3
#define DPTX1_BIST_CLK_INV_WIDTH                                                                   1
#define DPTX1_BIST_RSRV_MASK                                                                    0x10
#define DPTX1_BIST_RSRV_OFFSET                                                                     4
#define DPTX1_BIST_RSRV_WIDTH                                                                      1
#define DPTX1_BIST_CRC_ENABLE_MASK                                                              0x20
#define DPTX1_BIST_CRC_ENABLE_OFFSET                                                               5
#define DPTX1_BIST_CRC_ENABLE_WIDTH                                                                1
#define DPTX1_BIST_CRC_FORCE_RESET_MASK                                                         0x40
#define DPTX1_BIST_CRC_FORCE_RESET_OFFSET                                                          6
#define DPTX1_BIST_CRC_FORCE_RESET_WIDTH                                                           1
#define DPTX1_BIST_BS_SELECT_MASK                                                               0x80
#define DPTX1_BIST_BS_SELECT_OFFSET                                                                7
#define DPTX1_BIST_BS_SELECT_WIDTH                                                                 1



// Register DPTX1_BIST_CONTROL1(0x8d0)
#define DPTX1_BIST_CONTROL1                                                               0x000008d0 //RW POD:0x0
#define DPTX1_BIST_SEQ_LENGTH_MASK                                                              0x07
#define DPTX1_BIST_SEQ_LENGTH_OFFSET                                                               0
#define DPTX1_BIST_SEQ_LENGTH_WIDTH                                                                3
#define DPTX1_BIST_KCODE_MODE_MASK                                                              0x08
#define DPTX1_BIST_KCODE_MODE_OFFSET                                                               3
#define DPTX1_BIST_KCODE_MODE_WIDTH                                                                1
#define DPTX1_BIST_CLK_MODE_MASK                                                                0x10
#define DPTX1_BIST_CLK_MODE_OFFSET                                                                 4
#define DPTX1_BIST_CLK_MODE_WIDTH                                                                  1
#define DPTX1_BIST_BITS_SWAP_MASK                                                               0x20
#define DPTX1_BIST_BITS_SWAP_OFFSET                                                                5
#define DPTX1_BIST_BITS_SWAP_WIDTH                                                                 1
#define DPTX1_BIST_SYMBOL_SWAP_MASK                                                             0x40
#define DPTX1_BIST_SYMBOL_SWAP_OFFSET                                                              6
#define DPTX1_BIST_SYMBOL_SWAP_WIDTH                                                               1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX1_BIST_CONTROL2(0x8d4)
#define DPTX1_BIST_CONTROL2                                                               0x000008d4 //RW POD:0x0
#define DPTX1_BISTCLK_DLY_MASK                                                                  0x0f
#define DPTX1_BISTCLK_DLY_OFFSET                                                                   0
#define DPTX1_BISTCLK_DLY_WIDTH                                                                    4
#define DPTX1_BIST_LN_SEL_MASK                                                                  0xf0
#define DPTX1_BIST_LN_SEL_OFFSET                                                                   4
#define DPTX1_BIST_LN_SEL_WIDTH                                                                    4



// Register DPTX1_BIST_PATTERN0(0x8d8)
#define DPTX1_BIST_PATTERN0                                                               0x000008d8 //RW POD:0x0
#define DPTX1_BIST_PATTERN0_MASK                                                          0x3fffffff
#define DPTX1_BIST_PATTERN0_OFFSET                                                                 0
#define DPTX1_BIST_PATTERN0_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_BIST_PATTERN1(0x8dc)
#define DPTX1_BIST_PATTERN1                                                               0x000008dc //RW POD:0x0
#define DPTX1_BIST_PATTERN1_MASK                                                          0x3fffffff
#define DPTX1_BIST_PATTERN1_OFFSET                                                                 0
#define DPTX1_BIST_PATTERN1_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_BIST_PATTERN2(0x8e0)
#define DPTX1_BIST_PATTERN2                                                               0x000008e0 //RW POD:0x0
#define DPTX1_BIST_PATTERN2_MASK                                                          0x3fffffff
#define DPTX1_BIST_PATTERN2_OFFSET                                                                 0
#define DPTX1_BIST_PATTERN2_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_BIST_PATTERN3(0x8e4)
#define DPTX1_BIST_PATTERN3                                                               0x000008e4 //RW POD:0x0
#define DPTX1_BIST_PATTERN3_MASK                                                          0x3fffffff
#define DPTX1_BIST_PATTERN3_OFFSET                                                                 0
#define DPTX1_BIST_PATTERN3_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_BIST_PATTERN4(0x8e8)
#define DPTX1_BIST_PATTERN4                                                               0x000008e8 //RW POD:0x0
#define DPTX1_BIST_PATTERN4_MASK                                                          0x3fffffff
#define DPTX1_BIST_PATTERN4_OFFSET                                                                 0
#define DPTX1_BIST_PATTERN4_WIDTH                                                                 30
// #define Reserved_MASK                                                                     0xc0000000
// #define Reserved_OFFSET                                                                           30
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_BIST_CRC_STATUS(0x8ec)
#define DPTX1_BIST_CRC_STATUS                                                             0x000008ec //RO POD:0x0
#define DPTX1_ERROR_COUNT_MASK                                                                0x003f
#define DPTX1_ERROR_COUNT_OFFSET                                                                   0
#define DPTX1_ERROR_COUNT_WIDTH                                                                    6
#define DPTX1_ERROR_OVERFLOW_MASK                                                             0x0040
#define DPTX1_ERROR_OVERFLOW_OFFSET                                                                6
#define DPTX1_ERROR_OVERFLOW_WIDTH                                                                 1
#define DPTX1_SIGNATURE_LOCK_MASK                                                             0x0080
#define DPTX1_SIGNATURE_LOCK_OFFSET                                                                7
#define DPTX1_SIGNATURE_LOCK_WIDTH                                                                 1
#define DPTX1_PRBS_ACTIV_MASK                                                                 0x0100
#define DPTX1_PRBS_ACTIV_OFFSET                                                                    8
#define DPTX1_PRBS_ACTIV_WIDTH                                                                     1
// #define Reserved_MASK                                                                         0xfe00
// #define Reserved_OFFSET                                                                            9
// #define Reserved_WIDTH                                                                             7



// Register DPTX1_AUX_MISC_CTRL(0x8f0)
#define DPTX1_AUX_MISC_CTRL                                                               0x000008f0 //RW POD:0x0
#define DPTX1_TX_CKTMODE_MASK                                                                   0x01
#define DPTX1_TX_CKTMODE_OFFSET                                                                    0
#define DPTX1_TX_CKTMODE_WIDTH                                                                     1
#define DPTX1_CK_SEL_MASK                                                                       0x06
#define DPTX1_CK_SEL_OFFSET                                                                        1
#define DPTX1_CK_SEL_WIDTH                                                                         2
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_BIST_CRC_SIGNATURE(0x8f4)
#define DPTX1_BIST_CRC_SIGNATURE                                                          0x000008f4 //RO POD:0x0
#define DPTX1_BIST_CRC_SIGNATURE_MASK                                                         0xffff
#define DPTX1_BIST_CRC_SIGNATURE_OFFSET                                                            0
#define DPTX1_BIST_CRC_SIGNATURE_WIDTH                                                            16



// Register DPTX1_PLL_CLK_MEAS_CTRL(0x900)
#define DPTX1_PLL_CLK_MEAS_CTRL                                                           0x00000900 //RW POD:0x0
#define DPTX1_PLL_SYSCLK_MEAS_EN_MASK                                                           0x01
#define DPTX1_PLL_SYSCLK_MEAS_EN_OFFSET                                                            0
#define DPTX1_PLL_SYSCLK_MEAS_EN_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX1_PLL_CLK_MEAS_RESULT(0x904)
#define DPTX1_PLL_CLK_MEAS_RESULT                                                         0x00000904 //RO POD:0x0
#define DPTX1_PLL_SYSCLK_RESULT_MASK                                                          0x0fff
#define DPTX1_PLL_SYSCLK_RESULT_OFFSET                                                             0
#define DPTX1_PLL_SYSCLK_RESULT_WIDTH                                                             12
// #define Reserved_MASK                                                                         0xf000
// #define Reserved_OFFSET                                                                           12
// #define Reserved_WIDTH                                                                             4



// Register DPTX1_PHYA_IRQ_CTRL(0x908)
#define DPTX1_PHYA_IRQ_CTRL                                                               0x00000908 //RW POD:0x0
#define DPTX1_NOCLK_IRQ_EN_MASK                                                                 0x01
#define DPTX1_NOCLK_IRQ_EN_OFFSET                                                                  0
#define DPTX1_NOCLK_IRQ_EN_WIDTH                                                                   1
#define DPTX1_CLKERR_IRQ_EN_MASK                                                                0x02
#define DPTX1_CLKERR_IRQ_EN_OFFSET                                                                 1
#define DPTX1_CLKERR_IRQ_EN_WIDTH                                                                  1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_PHYA_IRQ_STATUS(0x90c)
#define DPTX1_PHYA_IRQ_STATUS                                                             0x0000090c //CRO POD:0x0
#define DPTX1_NOCLK_STS_MASK                                                                    0x01
#define DPTX1_NOCLK_STS_OFFSET                                                                     0
#define DPTX1_NOCLK_STS_WIDTH                                                                      1
#define DPTX1_CLKERR_STS_MASK                                                                   0x02
#define DPTX1_CLKERR_STS_OFFSET                                                                    1
#define DPTX1_CLKERR_STS_WIDTH                                                                     1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6






// address block DPTX1_AUX [0x9a0]


# define DPTX1_AUX_BASE_ADDRESS 0x9a0


// Register DPTX1_AUX_CONTROL(0x9a0)
#define DPTX1_AUX_CONTROL                                                                 0x000009a0 //RW POD:0x0
#define DPTX1_LT2AUX_DIS_MASK                                                                   0x01
#define DPTX1_LT2AUX_DIS_OFFSET                                                                    0
#define DPTX1_LT2AUX_DIS_WIDTH                                                                     1
#define DPTX1_I2C2AUX_DIS_MASK                                                                  0x02
#define DPTX1_I2C2AUX_DIS_OFFSET                                                                   1
#define DPTX1_I2C2AUX_DIS_WIDTH                                                                    1
#define DPTX1_OCM2AUX_DIS_MASK                                                                  0x04
#define DPTX1_OCM2AUX_DIS_OFFSET                                                                   2
#define DPTX1_OCM2AUX_DIS_WIDTH                                                                    1
#define DPTX1_HDCP2AUX_DIS_MASK                                                                 0x08
#define DPTX1_HDCP2AUX_DIS_OFFSET                                                                  3
#define DPTX1_HDCP2AUX_DIS_WIDTH                                                                   1
#define DPTX1_AUX_LBACK_EN_MASK                                                                 0x10
#define DPTX1_AUX_LBACK_EN_OFFSET                                                                  4
#define DPTX1_AUX_LBACK_EN_WIDTH                                                                   1
#define DPTX1_AUX_LONG_PREAMBL_MASK                                                             0x20
#define DPTX1_AUX_LONG_PREAMBL_OFFSET                                                              5
#define DPTX1_AUX_LONG_PREAMBL_WIDTH                                                               1
#define DPTX1_AUX_POLARITY_MASK                                                                 0x40
#define DPTX1_AUX_POLARITY_OFFSET                                                                  6
#define DPTX1_AUX_POLARITY_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX1_AUX_CLK_DIV(0x9a4)
#define DPTX1_AUX_CLK_DIV                                                                 0x000009a4 //RW POD:0x65
#define DPTX1_AUX_CLK_HIGH_MASK                                                                 0x0f
#define DPTX1_AUX_CLK_HIGH_OFFSET                                                                  0
#define DPTX1_AUX_CLK_HIGH_WIDTH                                                                   4
#define DPTX1_AUX_CLK_LOW_MASK                                                                  0xf0
#define DPTX1_AUX_CLK_LOW_OFFSET                                                                   4
#define DPTX1_AUX_CLK_LOW_WIDTH                                                                    4



// Register DPTX1_AUX_IRQ_CTRL(0x9b0)
#define DPTX1_AUX_IRQ_CTRL                                                                0x000009b0 //RW POD:0x0
#define DPTX1_OCM2AUX_REPLY_IRQ_EN_MASK                                                         0x01
#define DPTX1_OCM2AUX_REPLY_IRQ_EN_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_IRQ_EN_WIDTH                                                           1
#define DPTX1_OCM2AUX_REPLY_TMOUT_IRQ_EN_MASK                                                   0x02
#define DPTX1_OCM2AUX_REPLY_TMOUT_IRQ_EN_OFFSET                                                    1
#define DPTX1_OCM2AUX_REPLY_TMOUT_IRQ_EN_WIDTH                                                     1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_AUX_IRQ_STATUS(0x9b4)
#define DPTX1_AUX_IRQ_STATUS                                                              0x000009b4 //CRO POD:0x0
#define DPTX1_OCM2AUX_REPLY_IRQ_STS_MASK                                                        0x01
#define DPTX1_OCM2AUX_REPLY_IRQ_STS_OFFSET                                                         0
#define DPTX1_OCM2AUX_REPLY_IRQ_STS_WIDTH                                                          1
#define DPTX1_OCM2AUX_REPLY_TO_IRQ_STS_MASK                                                     0x02
#define DPTX1_OCM2AUX_REPLY_TO_IRQ_STS_OFFSET                                                      1
#define DPTX1_OCM2AUX_REPLY_TO_IRQ_STS_WIDTH                                                       1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_OCM2AUX_CONTROL(0x9c0)
#define DPTX1_OCM2AUX_CONTROL                                                             0x000009c0 //CRW POD:0x0
#define DPTX1_OCM2AUX_REQ_CMD_MASK                                                              0x0f
#define DPTX1_OCM2AUX_REQ_CMD_OFFSET                                                               0
#define DPTX1_OCM2AUX_REQ_CMD_WIDTH                                                                4
#define DPTX1_OCM2AUX_SHORT_REQ_MASK                                                            0x10
#define DPTX1_OCM2AUX_SHORT_REQ_OFFSET                                                             4
#define DPTX1_OCM2AUX_SHORT_REQ_WIDTH                                                              1
#define DPTX1_OCM2AUX_MODE_MASK                                                                 0x20
#define DPTX1_OCM2AUX_MODE_OFFSET                                                                  5
#define DPTX1_OCM2AUX_MODE_WIDTH                                                                   1
#define DPTX1_OCM2AUX_CONTROL_6_MASK                                                            0x40
#define DPTX1_OCM2AUX_CONTROL_6_OFFSET                                                             6
#define DPTX1_OCM2AUX_CONTROL_6_WIDTH                                                              1
#define DPTX1_OCM2AUX_REQ_MASK                                                                  0x80
#define DPTX1_OCM2AUX_REQ_OFFSET                                                                   7
#define DPTX1_OCM2AUX_REQ_WIDTH                                                                    1



// Register DPTX1_OCM2AUX_REQ_ADDR(0x9c4)
#define DPTX1_OCM2AUX_REQ_ADDR                                                            0x000009c4 //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_ADDR_MASK                                                       0x00ffffff
#define DPTX1_OCM2AUX_REQ_ADDR_OFFSET                                                              0
#define DPTX1_OCM2AUX_REQ_ADDR_WIDTH                                                              24
// #define Reserved_MASK                                                                     0xff000000
// #define Reserved_OFFSET                                                                           24
// #define Reserved_WIDTH                                                                             8



// Register DPTX1_OCM2AUX_REQ_LENGTH(0x9c8)
#define DPTX1_OCM2AUX_REQ_LENGTH                                                          0x000009c8 //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_LENGTH_MASK                                                           0x0f
#define DPTX1_OCM2AUX_REQ_LENGTH_OFFSET                                                            0
#define DPTX1_OCM2AUX_REQ_LENGTH_WIDTH                                                             4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX1_OCM2AUX_RETRY_COUNT(0x9cc)
#define DPTX1_OCM2AUX_RETRY_COUNT                                                         0x000009cc //RW POD:0x0
#define DPTX1_OCM2AUX_RETRY_COUNT_MASK                                                          0xff
#define DPTX1_OCM2AUX_RETRY_COUNT_OFFSET                                                           0
#define DPTX1_OCM2AUX_RETRY_COUNT_WIDTH                                                            8



// Register DPTX1_OCM2AUX_REQ_DATA_0(0x9d0)
#define DPTX1_OCM2AUX_REQ_DATA_0                                                          0x000009d0 //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_DATA_0_MASK                                                     0xffffffff
#define DPTX1_OCM2AUX_REQ_DATA_0_OFFSET                                                            0
#define DPTX1_OCM2AUX_REQ_DATA_0_WIDTH                                                            32



// Register DPTX1_OCM2AUX_REQ_DATA_1(0x9d4)
#define DPTX1_OCM2AUX_REQ_DATA_1                                                          0x000009d4 //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_DATA_1_MASK                                                     0xffffffff
#define DPTX1_OCM2AUX_REQ_DATA_1_OFFSET                                                            0
#define DPTX1_OCM2AUX_REQ_DATA_1_WIDTH                                                            32



// Register DPTX1_OCM2AUX_REQ_DATA_2(0x9d8)
#define DPTX1_OCM2AUX_REQ_DATA_2                                                          0x000009d8 //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_DATA_2_MASK                                                     0xffffffff
#define DPTX1_OCM2AUX_REQ_DATA_2_OFFSET                                                            0
#define DPTX1_OCM2AUX_REQ_DATA_2_WIDTH                                                            32



// Register DPTX1_OCM2AUX_REQ_DATA_3(0x9dc)
#define DPTX1_OCM2AUX_REQ_DATA_3                                                          0x000009dc //RW POD:0x0
#define DPTX1_OCM2AUX_REQ_DATA_3_MASK                                                     0xffffffff
#define DPTX1_OCM2AUX_REQ_DATA_3_OFFSET                                                            0
#define DPTX1_OCM2AUX_REQ_DATA_3_WIDTH                                                            32



// Register DPTX1_OCM2AUX_REPLY_COMMAND(0x9f0)
#define DPTX1_OCM2AUX_REPLY_COMMAND                                                       0x000009f0 //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_COMMAND_MASK                                                        0x0f
#define DPTX1_OCM2AUX_REPLY_COMMAND_OFFSET                                                         0
#define DPTX1_OCM2AUX_REPLY_COMMAND_WIDTH                                                          4
#define DPTX1_OCM2AUX_REPLY_M_MASK                                                              0xf0
#define DPTX1_OCM2AUX_REPLY_M_OFFSET                                                               4
#define DPTX1_OCM2AUX_REPLY_M_WIDTH                                                                4



// Register DPTX1_OCM2AUX_REPLY_LENGTH(0x9f4)
#define DPTX1_OCM2AUX_REPLY_LENGTH                                                        0x000009f4 //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_LENGTH_MASK                                                         0x1f
#define DPTX1_OCM2AUX_REPLY_LENGTH_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_LENGTH_WIDTH                                                           5
#define DPTX1_OCM2AUX_REPLY_ERROR_MASK                                                          0xe0
#define DPTX1_OCM2AUX_REPLY_ERROR_OFFSET                                                           5
#define DPTX1_OCM2AUX_REPLY_ERROR_WIDTH                                                            3



// Register DPTX1_OCM2AUX_REPLY_DATA_0(0x9f8)
#define DPTX1_OCM2AUX_REPLY_DATA_0                                                        0x000009f8 //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_DATA_0_MASK                                                   0xffffffff
#define DPTX1_OCM2AUX_REPLY_DATA_0_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_DATA_0_WIDTH                                                          32



// Register DPTX1_OCM2AUX_REPLY_DATA_1(0x9fc)
#define DPTX1_OCM2AUX_REPLY_DATA_1                                                        0x000009fc //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_DATA_1_MASK                                                   0xffffffff
#define DPTX1_OCM2AUX_REPLY_DATA_1_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_DATA_1_WIDTH                                                          32



// Register DPTX1_OCM2AUX_REPLY_DATA_2(0xa00)
#define DPTX1_OCM2AUX_REPLY_DATA_2                                                        0x00000a00 //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_DATA_2_MASK                                                   0xffffffff
#define DPTX1_OCM2AUX_REPLY_DATA_2_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_DATA_2_WIDTH                                                          32



// Register DPTX1_OCM2AUX_REPLY_DATA_3(0xa04)
#define DPTX1_OCM2AUX_REPLY_DATA_3                                                        0x00000a04 //RO POD:0x0
#define DPTX1_OCM2AUX_REPLY_DATA_3_MASK                                                   0xffffffff
#define DPTX1_OCM2AUX_REPLY_DATA_3_OFFSET                                                          0
#define DPTX1_OCM2AUX_REPLY_DATA_3_WIDTH                                                          32



// Register DPTX1_I2C2AUX_CTRL(0xa10)
#define DPTX1_I2C2AUX_CTRL                                                                0x00000a10 //RW POD:0x1
#define DPTX1_I2C2AUX_EN_MASK                                                                   0x01
#define DPTX1_I2C2AUX_EN_OFFSET                                                                    0
#define DPTX1_I2C2AUX_EN_WIDTH                                                                     1
#define DPTX1_I2C2AUX_BROADCAST_MASK                                                            0x02
#define DPTX1_I2C2AUX_BROADCAST_OFFSET                                                             1
#define DPTX1_I2C2AUX_BROADCAST_WIDTH                                                              1
#define DPTX1_I2C2AUX_ADDR_CHK_EN_MASK                                                          0x04
#define DPTX1_I2C2AUX_ADDR_CHK_EN_OFFSET                                                           2
#define DPTX1_I2C2AUX_ADDR_CHK_EN_WIDTH                                                            1
// #define Reserved_MASK                                                                           0x08
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             1
#define DPTX1_I2C2AUX_RETRIES_MASK                                                              0xf0
#define DPTX1_I2C2AUX_RETRIES_OFFSET                                                               4
#define DPTX1_I2C2AUX_RETRIES_WIDTH                                                                4



// Register DPTX1_I2C2AUX_ADDR1(0xa14)
#define DPTX1_I2C2AUX_ADDR1                                                               0x00000a14 //RW POD:0x0
#define DPTX1_I2C2AUX_ADDR1_EN_MASK                                                             0x01
#define DPTX1_I2C2AUX_ADDR1_EN_OFFSET                                                              0
#define DPTX1_I2C2AUX_ADDR1_EN_WIDTH                                                               1
#define DPTX1_I2C2AUX_ADDR1_MASK                                                                0xfe
#define DPTX1_I2C2AUX_ADDR1_OFFSET                                                                 1
#define DPTX1_I2C2AUX_ADDR1_WIDTH                                                                  7



// Register DPTX1_I2C2AUX_ADDR2(0xa18)
#define DPTX1_I2C2AUX_ADDR2                                                               0x00000a18 //RW POD:0x0
#define DPTX1_I2C2AUX_ADDR2_EN_MASK                                                             0x01
#define DPTX1_I2C2AUX_ADDR2_EN_OFFSET                                                              0
#define DPTX1_I2C2AUX_ADDR2_EN_WIDTH                                                               1
#define DPTX1_I2C2AUX_ADDR2_MASK                                                                0xfe
#define DPTX1_I2C2AUX_ADDR2_OFFSET                                                                 1
#define DPTX1_I2C2AUX_ADDR2_WIDTH                                                                  7



// Register DPTX1_I2C2AUX_ADDR3(0xa1c)
#define DPTX1_I2C2AUX_ADDR3                                                               0x00000a1c //RW POD:0x0
#define DPTX1_I2C2AUX_ADDR3_EN_MASK                                                             0x01
#define DPTX1_I2C2AUX_ADDR3_EN_OFFSET                                                              0
#define DPTX1_I2C2AUX_ADDR3_EN_WIDTH                                                               1
#define DPTX1_I2C2AUX_ADDR3_MASK                                                                0xfe
#define DPTX1_I2C2AUX_ADDR3_OFFSET                                                                 1
#define DPTX1_I2C2AUX_ADDR3_WIDTH                                                                  7



// Register DPTX1_I2C2AUX_ADDR4(0xa20)
#define DPTX1_I2C2AUX_ADDR4                                                               0x00000a20 //RW POD:0x0
#define DPTX1_I2C2AUX_ADDR4_EN_MASK                                                             0x01
#define DPTX1_I2C2AUX_ADDR4_EN_OFFSET                                                              0
#define DPTX1_I2C2AUX_ADDR4_EN_WIDTH                                                               1
#define DPTX1_I2C2AUX_ADDR4_MASK                                                                0xfe
#define DPTX1_I2C2AUX_ADDR4_OFFSET                                                                 1
#define DPTX1_I2C2AUX_ADDR4_WIDTH                                                                  7



// Register DPTX1_I2C2AUX_BUF_SIZE(0xa24)
#define DPTX1_I2C2AUX_BUF_SIZE                                                            0x00000a24 //RW POD:0x0
#define DPTX1_I2C2AUX_WBUF_SIZE_MASK                                                            0x0f
#define DPTX1_I2C2AUX_WBUF_SIZE_OFFSET                                                             0
#define DPTX1_I2C2AUX_WBUF_SIZE_WIDTH                                                              4
#define DPTX1_I2C2AUX_RBUF_SIZE_MASK                                                            0xf0
#define DPTX1_I2C2AUX_RBUF_SIZE_OFFSET                                                             4
#define DPTX1_I2C2AUX_RBUF_SIZE_WIDTH                                                              4



// Register DPTX1_I2C2AUX_STRETCH_TIMEOUT(0xa28)
#define DPTX1_I2C2AUX_STRETCH_TIMEOUT                                                     0x00000a28 //RW POD:0xff
#define DPTX1_I2C2AUX_STRETCH_TIMEOUT_MASK                                                      0xff
#define DPTX1_I2C2AUX_STRETCH_TIMEOUT_OFFSET                                                       0
#define DPTX1_I2C2AUX_STRETCH_TIMEOUT_WIDTH                                                        8



// Register DPTX1_I2C2AUX_CTRL2(0xa2c)
#define DPTX1_I2C2AUX_CTRL2                                                               0x00000a2c //RW POD:0x0
#define DPTX1_I2C2AUX_STOP_REQ_MASK                                                             0x01
#define DPTX1_I2C2AUX_STOP_REQ_OFFSET                                                              0
#define DPTX1_I2C2AUX_STOP_REQ_WIDTH                                                               1
#define DPTX1_I2C2AUX_STOP_IRQ_EN_MASK                                                          0x02
#define DPTX1_I2C2AUX_STOP_IRQ_EN_OFFSET                                                           1
#define DPTX1_I2C2AUX_STOP_IRQ_EN_WIDTH                                                            1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_I2C2AUX_STATUS(0xa30)
#define DPTX1_I2C2AUX_STATUS                                                              0x00000a30 //CRO POD:0x0
#define DPTX1_I2C2AUX_I2C_NACK_MASK                                                             0x01
#define DPTX1_I2C2AUX_I2C_NACK_OFFSET                                                              0
#define DPTX1_I2C2AUX_I2C_NACK_WIDTH                                                               1
#define DPTX1_I2C2AUX_AUX_NACK_MASK                                                             0x02
#define DPTX1_I2C2AUX_AUX_NACK_OFFSET                                                              1
#define DPTX1_I2C2AUX_AUX_NACK_WIDTH                                                               1
#define DPTX1_I2C2AUX_AUX_DEFER_MASK                                                            0x04
#define DPTX1_I2C2AUX_AUX_DEFER_OFFSET                                                             2
#define DPTX1_I2C2AUX_AUX_DEFER_WIDTH                                                              1
#define DPTX1_I2C2AUX_ARB_TIMEOUT_MASK                                                          0x08
#define DPTX1_I2C2AUX_ARB_TIMEOUT_OFFSET                                                           3
#define DPTX1_I2C2AUX_ARB_TIMEOUT_WIDTH                                                            1
#define DPTX1_I2C2AUX_I2C_TIMEOUT_MASK                                                          0x10
#define DPTX1_I2C2AUX_I2C_TIMEOUT_OFFSET                                                           4
#define DPTX1_I2C2AUX_I2C_TIMEOUT_WIDTH                                                            1
#define DPTX1_I2C2AUX_CLK_STRETCH_MASK                                                          0x20
#define DPTX1_I2C2AUX_CLK_STRETCH_OFFSET                                                           5
#define DPTX1_I2C2AUX_CLK_STRETCH_WIDTH                                                            1
#define DPTX1_I2C2AUX_STOP_MASK                                                                 0x40
#define DPTX1_I2C2AUX_STOP_OFFSET                                                                  6
#define DPTX1_I2C2AUX_STOP_WIDTH                                                                   1
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1






// address block DPTX1_PHYD [0xa40]


# define DPTX1_PHYD_BASE_ADDRESS 0xa40


// Register DPTX1_LINK_CONTROL(0xa40)
#define DPTX1_LINK_CONTROL                                                                0x00000a40 //RW POD:0xf
#define DPTX1_LINK_EN_MASK                                                                      0x01
#define DPTX1_LINK_EN_OFFSET                                                                       0
#define DPTX1_LINK_EN_WIDTH                                                                        1
#define DPTX1_SCRM_ADVANCE_MASK                                                                 0x02
#define DPTX1_SCRM_ADVANCE_OFFSET                                                                  1
#define DPTX1_SCRM_ADVANCE_WIDTH                                                                   1
#define DPTX1_SCRM_POLYNOMIAL_MASK                                                              0x04
#define DPTX1_SCRM_POLYNOMIAL_OFFSET                                                               2
#define DPTX1_SCRM_POLYNOMIAL_WIDTH                                                                1
#define DPTX1_SKEW_EN_MASK                                                                      0x08
#define DPTX1_SKEW_EN_OFFSET                                                                       3
#define DPTX1_SKEW_EN_WIDTH                                                                        1
#define DPTX1_LANE_POLARITY_MASK                                                                0x10
#define DPTX1_LANE_POLARITY_OFFSET                                                                 4
#define DPTX1_LANE_POLARITY_WIDTH                                                                  1
#define DPTX1_HPD_POLARITY_MASK                                                                 0x20
#define DPTX1_HPD_POLARITY_OFFSET                                                                  5
#define DPTX1_HPD_POLARITY_WIDTH                                                                   1
#define DPTX1_SWAP_LANE_MASK                                                                    0xc0
#define DPTX1_SWAP_LANE_OFFSET                                                                     6
#define DPTX1_SWAP_LANE_WIDTH                                                                      2



// Register DPTX1_LT_CONTROL(0xa44)
#define DPTX1_LT_CONTROL                                                                  0x00000a44 //CRW POD:0x80
#define DPTX1_LT_START_MASK                                                                     0x01
#define DPTX1_LT_START_OFFSET                                                                      0
#define DPTX1_LT_START_WIDTH                                                                       1
#define DPTX1_LT_END_IRQ_EN_MASK                                                                0x02
#define DPTX1_LT_END_IRQ_EN_OFFSET                                                                 1
#define DPTX1_LT_END_IRQ_EN_WIDTH                                                                  1
#define DPTX1_LT_CONFIG_MASK                                                                    0x04
#define DPTX1_LT_CONFIG_OFFSET                                                                     2
#define DPTX1_LT_CONFIG_WIDTH                                                                      1
#define DPTX1_LONG_LT_EN_MASK                                                                   0x08
#define DPTX1_LONG_LT_EN_OFFSET                                                                    3
#define DPTX1_LONG_LT_EN_WIDTH                                                                     1
#define DPTX1_LT_EQ_DONE_IGNORE_MASK                                                            0x10
#define DPTX1_LT_EQ_DONE_IGNORE_OFFSET                                                             4
#define DPTX1_LT_EQ_DONE_IGNORE_WIDTH                                                              1
#define DPTX1_LT_SLOCKED_IGNORE_MASK                                                            0x20
#define DPTX1_LT_SLOCKED_IGNORE_OFFSET                                                             5
#define DPTX1_LT_SLOCKED_IGNORE_WIDTH                                                              1
#define DPTX1_LT_ADV_CHECK_EN_MASK                                                              0x40
#define DPTX1_LT_ADV_CHECK_EN_OFFSET                                                               6
#define DPTX1_LT_ADV_CHECK_EN_WIDTH                                                                1
#define DPTX1_LT_BW_REDUCE_AUTO_MASK                                                            0x80
#define DPTX1_LT_BW_REDUCE_AUTO_OFFSET                                                             7
#define DPTX1_LT_BW_REDUCE_AUTO_WIDTH                                                              1



// Register DPTX1_LT_STATUS(0xa48)
#define DPTX1_LT_STATUS                                                                   0x00000a48 //CRO POD:0x0
#define DPTX1_LT_ACTIVE_MASK                                                                    0x01
#define DPTX1_LT_ACTIVE_OFFSET                                                                     0
#define DPTX1_LT_ACTIVE_WIDTH                                                                      1
#define DPTX1_LT_END_IRQ_STS_MASK                                                               0x02
#define DPTX1_LT_END_IRQ_STS_OFFSET                                                                1
#define DPTX1_LT_END_IRQ_STS_WIDTH                                                                 1
#define DPTX1_LT_RESULT_MASK                                                                    0x04
#define DPTX1_LT_RESULT_OFFSET                                                                     2
#define DPTX1_LT_RESULT_WIDTH                                                                      1
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_LL_LINK_BW_SET(0xa50)
#define DPTX1_LL_LINK_BW_SET                                                              0x00000a50 //RW POD:0x0
#define DPTX1_LL_LINK_BW_SET_MASK                                                               0x1f
#define DPTX1_LL_LINK_BW_SET_OFFSET                                                                0
#define DPTX1_LL_LINK_BW_SET_WIDTH                                                                 5
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX1_LL_LANE_COUNT_SET(0xa54)
#define DPTX1_LL_LANE_COUNT_SET                                                           0x00000a54 //RW POD:0x4
#define DPTX1_LL_LANE_COUNT_SET_MASK                                                            0x07
#define DPTX1_LL_LANE_COUNT_SET_OFFSET                                                             0
#define DPTX1_LL_LANE_COUNT_SET_WIDTH                                                              3
// #define Reserved_MASK                                                                           0x78
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             4
#define DPTX1_LL_ENHANCED_FRAME_EN_SET_MASK                                                     0x80
#define DPTX1_LL_ENHANCED_FRAME_EN_SET_OFFSET                                                      7
#define DPTX1_LL_ENHANCED_FRAME_EN_SET_WIDTH                                                       1



// Register DPTX1_LL_TRAINING_PATTERN_SET(0xa58)
#define DPTX1_LL_TRAINING_PATTERN_SET                                                     0x00000a58 //RW POD:0x0
#define DPTX1_LL_TRAINING_PATTERN_SET_MASK                                                      0x03
#define DPTX1_LL_TRAINING_PATTERN_SET_OFFSET                                                       0
#define DPTX1_LL_TRAINING_PATTERN_SET_WIDTH                                                        2
#define DPTX1_LL_LINK_QUAL_PATTERN_SET_MASK                                                     0x0c
#define DPTX1_LL_LINK_QUAL_PATTERN_SET_OFFSET                                                      2
#define DPTX1_LL_LINK_QUAL_PATTERN_SET_WIDTH                                                       2
#define DPTX1_LL_RECOVERRED_CLOCK_OUT_EN_MASK                                                   0x10
#define DPTX1_LL_RECOVERRED_CLOCK_OUT_EN_OFFSET                                                    4
#define DPTX1_LL_RECOVERRED_CLOCK_OUT_EN_WIDTH                                                     1
#define DPTX1_LL_SCRAMBLING_DISABLE_SET_MASK                                                    0x20
#define DPTX1_LL_SCRAMBLING_DISABLE_SET_OFFSET                                                     5
#define DPTX1_LL_SCRAMBLING_DISABLE_SET_WIDTH                                                      1
#define DPTX1_LL_SYMBOL_ERROR_COUNT_SET_MASK                                                    0xc0
#define DPTX1_LL_SYMBOL_ERROR_COUNT_SET_OFFSET                                                     6
#define DPTX1_LL_SYMBOL_ERROR_COUNT_SET_WIDTH                                                      2



// Register DPTX1_LL_TRAINING_L0_SET(0xa5c)
#define DPTX1_LL_TRAINING_L0_SET                                                          0x00000a5c //RW POD:0x0
#define DPTX1_LL_L0_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX1_LL_L0_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX1_LL_L0_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX1_LL_L0_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX1_LL_L0_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX1_LL_L0_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L1_SET(0xa60)
#define DPTX1_LL_TRAINING_L1_SET                                                          0x00000a60 //RW POD:0x0
#define DPTX1_LL_L1_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX1_LL_L1_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX1_LL_L1_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX1_LL_L1_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX1_LL_L1_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX1_LL_L1_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L2_SET(0xa64)
#define DPTX1_LL_TRAINING_L2_SET                                                          0x00000a64 //RW POD:0x0
#define DPTX1_LL_L2_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX1_LL_L2_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX1_LL_L2_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX1_LL_L2_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX1_LL_L2_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX1_LL_L2_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L3_SET(0xa68)
#define DPTX1_LL_TRAINING_L3_SET                                                          0x00000a68 //RW POD:0x0
#define DPTX1_LL_L3_DRIVE_CURRENT_SET_MASK                                                      0x03
#define DPTX1_LL_L3_DRIVE_CURRENT_SET_OFFSET                                                       0
#define DPTX1_LL_L3_DRIVE_CURRENT_SET_WIDTH                                                        2
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_SET_MASK                                                0x04
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_SET_OFFSET                                                 2
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_SET_WIDTH                                                  1
#define DPTX1_LL_L3_PRE_EMPHASIS_SET_MASK                                                       0x18
#define DPTX1_LL_L3_PRE_EMPHASIS_SET_OFFSET                                                        3
#define DPTX1_LL_L3_PRE_EMPHASIS_SET_WIDTH                                                         2
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_MASK                                           0x20
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_OFFSET                                            5
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_SET_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_LINK_BW_STS(0xa70)
#define DPTX1_LL_LINK_BW_STS                                                              0x00000a70 //RO POD:0x0
#define DPTX1_LL_LINK_BW_STS_MASK                                                               0x1f
#define DPTX1_LL_LINK_BW_STS_OFFSET                                                                0
#define DPTX1_LL_LINK_BW_STS_WIDTH                                                                 5
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX1_LL_LANE_COUNT_STS(0xa74)
#define DPTX1_LL_LANE_COUNT_STS                                                           0x00000a74 //RO POD:0x0
#define DPTX1_LL_LANE_COUNT_STS_MASK                                                            0x0f
#define DPTX1_LL_LANE_COUNT_STS_OFFSET                                                             0
#define DPTX1_LL_LANE_COUNT_STS_WIDTH                                                              4
// #define Reserved_MASK                                                                           0x70
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             3
#define DPTX1_LL_ENHANCED_FRAME_EN_STS_MASK                                                     0x80
#define DPTX1_LL_ENHANCED_FRAME_EN_STS_OFFSET                                                      7
#define DPTX1_LL_ENHANCED_FRAME_EN_STS_WIDTH                                                       1



// Register DPTX1_LL_TRAINING_PATTERN_STS(0xa78)
#define DPTX1_LL_TRAINING_PATTERN_STS                                                     0x00000a78 //RO POD:0x0
#define DPTX1_LL_TRAINING_PATTERN_STS_MASK                                                      0x03
#define DPTX1_LL_TRAINING_PATTERN_STS_OFFSET                                                       0
#define DPTX1_LL_TRAINING_PATTERN_STS_WIDTH                                                        2
#define DPTX1_LL_LINK_QUAL_PATTERN_STS_MASK                                                     0x0c
#define DPTX1_LL_LINK_QUAL_PATTERN_STS_OFFSET                                                      2
#define DPTX1_LL_LINK_QUAL_PATTERN_STS_WIDTH                                                       2
#define DPTX1_LL_SCRAMBLING_DISABLE_STS_MASK                                                    0x10
#define DPTX1_LL_SCRAMBLING_DISABLE_STS_OFFSET                                                     4
#define DPTX1_LL_SCRAMBLING_DISABLE_STS_WIDTH                                                      1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX1_LL_TRAINING_L0_STS(0xa7c)
#define DPTX1_LL_TRAINING_L0_STS                                                          0x00000a7c //RO POD:0x0
#define DPTX1_LL_L0_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX1_LL_L0_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX1_LL_L0_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX1_LL_L0_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX1_LL_L0_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX1_LL_L0_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX1_LL_L0_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX1_LL_L0_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L1_STS(0xa80)
#define DPTX1_LL_TRAINING_L1_STS                                                          0x00000a80 //RO POD:0x0
#define DPTX1_LL_L1_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX1_LL_L1_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX1_LL_L1_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX1_LL_L1_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX1_LL_L1_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX1_LL_L1_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX1_LL_L1_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX1_LL_L1_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L2_STS(0xa84)
#define DPTX1_LL_TRAINING_L2_STS                                                          0x00000a84 //RO POD:0x0
#define DPTX1_LL_L2_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX1_LL_L2_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX1_LL_L2_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX1_LL_L2_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX1_LL_L2_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX1_LL_L2_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX1_LL_L2_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX1_LL_L2_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_LL_TRAINING_L3_STS(0xa88)
#define DPTX1_LL_TRAINING_L3_STS                                                          0x00000a88 //RO POD:0x0
#define DPTX1_LL_L3_DRIVE_CURRENT_STS_MASK                                                      0x03
#define DPTX1_LL_L3_DRIVE_CURRENT_STS_OFFSET                                                       0
#define DPTX1_LL_L3_DRIVE_CURRENT_STS_WIDTH                                                        2
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_STS_MASK                                                0x04
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_STS_OFFSET                                                 2
#define DPTX1_LL_L3_MAX_CURRENT_REACHED_STS_WIDTH                                                  1
#define DPTX1_LL_L3_PRE_EMPHASIS_STS_MASK                                                       0x18
#define DPTX1_LL_L3_PRE_EMPHASIS_STS_OFFSET                                                        3
#define DPTX1_LL_L3_PRE_EMPHASIS_STS_WIDTH                                                         2
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_MASK                                           0x20
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_OFFSET                                            5
#define DPTX1_LL_L3_MAX_PRE_EMPHASIS_REACHED_STS_WIDTH                                             1
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_SCRAMBLER_SEED(0xa8c)
#define DPTX1_SCRAMBLER_SEED                                                              0x00000a8c //RW POD:0xffff
#define DPTX1_SCRAMBLER_SEED_MASK                                                             0xffff
#define DPTX1_SCRAMBLER_SEED_OFFSET                                                                0
#define DPTX1_SCRAMBLER_SEED_WIDTH                                                                16



// Register DPTX1_SYM_GEN_CTRL(0xa90)
#define DPTX1_SYM_GEN_CTRL                                                                0x00000a90 //RW POD:0x0
#define DPTX1_SYM_GEN_MODE_MASK                                                                 0x01
#define DPTX1_SYM_GEN_MODE_OFFSET                                                                  0
#define DPTX1_SYM_GEN_MODE_WIDTH                                                                   1
#define DPTX1_SYM_K28_SWAP_MASK                                                                 0x02
#define DPTX1_SYM_K28_SWAP_OFFSET                                                                  1
#define DPTX1_SYM_K28_SWAP_WIDTH                                                                   1
#define DPTX1_8B10B_DI_BYTES_SWP_MASK                                                           0x04
#define DPTX1_8B10B_DI_BYTES_SWP_OFFSET                                                            2
#define DPTX1_8B10B_DI_BYTES_SWP_WIDTH                                                             1
#define DPTX1_8B10B_DO_BITS_SWP_MASK                                                            0x08
#define DPTX1_8B10B_DO_BITS_SWP_OFFSET                                                             3
#define DPTX1_8B10B_DO_BITS_SWP_WIDTH                                                              1
#define DPTX1_PRBS7_BIT_REVERSE_MASK                                                            0x10
#define DPTX1_PRBS7_BIT_REVERSE_OFFSET                                                             4
#define DPTX1_PRBS7_BIT_REVERSE_WIDTH                                                              1
#define DPTX1_SYM_RESERVED_MASK                                                                 0xe0
#define DPTX1_SYM_RESERVED_OFFSET                                                                  5
#define DPTX1_SYM_RESERVED_WIDTH                                                                   3



// Register DPTX1_SYM_GEN0(0xa94)
#define DPTX1_SYM_GEN0                                                                    0x00000a94 //RW POD:0x0
#define DPTX1_SYM_GEN0_MASK                                                                   0x03ff
#define DPTX1_SYM_GEN0_OFFSET                                                                      0
#define DPTX1_SYM_GEN0_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_SYM_GEN1(0xa98)
#define DPTX1_SYM_GEN1                                                                    0x00000a98 //RW POD:0x0
#define DPTX1_SYM_GEN1_MASK                                                                   0x03ff
#define DPTX1_SYM_GEN1_OFFSET                                                                      0
#define DPTX1_SYM_GEN1_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_SYM_GEN2(0xa9c)
#define DPTX1_SYM_GEN2                                                                    0x00000a9c //RW POD:0x0
#define DPTX1_SYM_GEN2_MASK                                                                   0x03ff
#define DPTX1_SYM_GEN2_OFFSET                                                                      0
#define DPTX1_SYM_GEN2_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_SYM_GEN3(0xaa0)
#define DPTX1_SYM_GEN3                                                                    0x00000aa0 //RW POD:0x0
#define DPTX1_SYM_GEN3_MASK                                                                   0x03ff
#define DPTX1_SYM_GEN3_OFFSET                                                                      0
#define DPTX1_SYM_GEN3_WIDTH                                                                      10
// #define Reserved_MASK                                                                         0xfc00
// #define Reserved_OFFSET                                                                           10
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_L0_PATTERN_CTRL(0xab0)
#define DPTX1_L0_PATTERN_CTRL                                                             0x00000ab0 //CRW POD:0x0
#define DPTX1_L0_PAT_SEL_MASK                                                                   0x07
#define DPTX1_L0_PAT_SEL_OFFSET                                                                    0
#define DPTX1_L0_PAT_SEL_WIDTH                                                                     3
#define DPTX1_L0_IDP_LT_EN_MASK                                                                 0x08
#define DPTX1_L0_IDP_LT_EN_OFFSET                                                                  3
#define DPTX1_L0_IDP_LT_EN_WIDTH                                                                   1
#define DPTX1_L0_PATTERN_SET_MASK                                                               0xf0
#define DPTX1_L0_PATTERN_SET_OFFSET                                                                4
#define DPTX1_L0_PATTERN_SET_WIDTH                                                                 4



// Register DPTX1_L1_PATTERN_CTRL(0xab4)
#define DPTX1_L1_PATTERN_CTRL                                                             0x00000ab4 //CRW POD:0x0
#define DPTX1_L1_PAT_SEL_MASK                                                                   0x07
#define DPTX1_L1_PAT_SEL_OFFSET                                                                    0
#define DPTX1_L1_PAT_SEL_WIDTH                                                                     3
#define DPTX1_L1_IDP_LT_EN_MASK                                                                 0x08
#define DPTX1_L1_IDP_LT_EN_OFFSET                                                                  3
#define DPTX1_L1_IDP_LT_EN_WIDTH                                                                   1
#define DPTX1_L1_PATTERN_SET_MASK                                                               0xf0
#define DPTX1_L1_PATTERN_SET_OFFSET                                                                4
#define DPTX1_L1_PATTERN_SET_WIDTH                                                                 4



// Register DPTX1_L2_PATTERN_CTRL(0xab8)
#define DPTX1_L2_PATTERN_CTRL                                                             0x00000ab8 //CRW POD:0x0
#define DPTX1_L2_PAT_SEL_MASK                                                                   0x07
#define DPTX1_L2_PAT_SEL_OFFSET                                                                    0
#define DPTX1_L2_PAT_SEL_WIDTH                                                                     3
#define DPTX1_L2_IDP_LT_EN_MASK                                                                 0x08
#define DPTX1_L2_IDP_LT_EN_OFFSET                                                                  3
#define DPTX1_L2_IDP_LT_EN_WIDTH                                                                   1
#define DPTX1_L2_PATTERN_SET_MASK                                                               0xf0
#define DPTX1_L2_PATTERN_SET_OFFSET                                                                4
#define DPTX1_L2_PATTERN_SET_WIDTH                                                                 4



// Register DPTX1_L3_PATTERN_CTRL(0xabc)
#define DPTX1_L3_PATTERN_CTRL                                                             0x00000abc //CRW POD:0x0
#define DPTX1_L3_PAT_SEL_MASK                                                                   0x07
#define DPTX1_L3_PAT_SEL_OFFSET                                                                    0
#define DPTX1_L3_PAT_SEL_WIDTH                                                                     3
#define DPTX1_L3_IDP_LT_EN_MASK                                                                 0x08
#define DPTX1_L3_IDP_LT_EN_OFFSET                                                                  3
#define DPTX1_L3_IDP_LT_EN_WIDTH                                                                   1
#define DPTX1_L3_PATTERN_SET_MASK                                                               0xf0
#define DPTX1_L3_PATTERN_SET_OFFSET                                                                4
#define DPTX1_L3_PATTERN_SET_WIDTH                                                                 4



// Register DPTX1_IDP_LT_PAT1_PERIOD(0xac0)
#define DPTX1_IDP_LT_PAT1_PERIOD                                                          0x00000ac0 //RW POD:0x6993
#define DPTX1_IDP_LT_PAT1_PERIOD_MASK                                                         0xffff
#define DPTX1_IDP_LT_PAT1_PERIOD_OFFSET                                                            0
#define DPTX1_IDP_LT_PAT1_PERIOD_WIDTH                                                            16



// Register DPTX1_IDP_LT_PAT2_PERIOD(0xac8)
#define DPTX1_IDP_LT_PAT2_PERIOD                                                          0x00000ac8 //RW POD:0x6993
#define DPTX1_IDP_LT_PAT2_PERIOD_MASK                                                         0xffff
#define DPTX1_IDP_LT_PAT2_PERIOD_OFFSET                                                            0
#define DPTX1_IDP_LT_PAT2_PERIOD_WIDTH                                                            16



// Register DPTX1_IDP_LT_HPD_CTRL(0xacc)
#define DPTX1_IDP_LT_HPD_CTRL                                                             0x00000acc //RW POD:0xa8
#define DPTX1_MIN_PULSE_WIDTH_MASK                                                              0xff
#define DPTX1_MIN_PULSE_WIDTH_OFFSET                                                               0
#define DPTX1_MIN_PULSE_WIDTH_WIDTH                                                                8






// address block DPTX1 [0xb00]


# define DPTX1_BASE_ADDRESS 0xb00


// Register DPTX1_MS_CTRL(0xb00)
#define DPTX1_MS_CTRL                                                                     0x00000b00 //RW POD:0x0
#define DPTX1_MS_VIDEO_EN_MASK                                                                  0x01
#define DPTX1_MS_VIDEO_EN_OFFSET                                                                   0
#define DPTX1_MS_VIDEO_EN_WIDTH                                                                    1
#define DPTX1_MS_VIDEO_EN_FORCE_MASK                                                            0x02
#define DPTX1_MS_VIDEO_EN_FORCE_OFFSET                                                             1
#define DPTX1_MS_VIDEO_EN_FORCE_WIDTH                                                              1
#define DPTX1_MS_MVID_COEF_SEL_MASK                                                             0x04
#define DPTX1_MS_MVID_COEF_SEL_OFFSET                                                              2
#define DPTX1_MS_MVID_COEF_SEL_WIDTH                                                               1
#define DPTX1_MS_FEFIFO_RESET_MASK                                                              0x08
#define DPTX1_MS_FEFIFO_RESET_OFFSET                                                               3
#define DPTX1_MS_FEFIFO_RESET_WIDTH                                                                1
#define DPTX1_MS_DUAL_BUS_EN_MASK                                                               0x10
#define DPTX1_MS_DUAL_BUS_EN_OFFSET                                                                4
#define DPTX1_MS_DUAL_BUS_EN_WIDTH                                                                 1
#define DPTX1_MS_MVID_TYPE_MASK                                                                 0x20
#define DPTX1_MS_MVID_TYPE_OFFSET                                                                  5
#define DPTX1_MS_MVID_TYPE_WIDTH                                                                   1
#define DPTX1_MS_FIELD_ID_INV_MASK                                                              0x40
#define DPTX1_MS_FIELD_ID_INV_OFFSET                                                               6
#define DPTX1_MS_FIELD_ID_INV_WIDTH                                                                1
#define DPTX1_MS_TWO_FIELDS_VTOTAL_MASK                                                         0x80
#define DPTX1_MS_TWO_FIELDS_VTOTAL_OFFSET                                                          7
#define DPTX1_MS_TWO_FIELDS_VTOTAL_WIDTH                                                           1



// Register DPTX1_MS_UPDATE(0xb04)
#define DPTX1_MS_UPDATE                                                                   0x00000b04 //CRW POD:0x0
#define DPTX1_MS_UPDATE_CTRL_MASK                                                               0x01
#define DPTX1_MS_UPDATE_CTRL_OFFSET                                                                0
#define DPTX1_MS_UPDATE_CTRL_WIDTH                                                                 1
#define DPTX1_MS_UPDATE_MODE_MASK                                                               0x06
#define DPTX1_MS_UPDATE_MODE_OFFSET                                                                1
#define DPTX1_MS_UPDATE_MODE_WIDTH                                                                 2
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_MS_IRQ_CTRL(0xb08)
#define DPTX1_MS_IRQ_CTRL                                                                 0x00000b08 //RW POD:0x0
#define DPTX1_MS_VID_ABSENT_IRQ_EN_MASK                                                         0x01
#define DPTX1_MS_VID_ABSENT_IRQ_EN_OFFSET                                                          0
#define DPTX1_MS_VID_ABSENT_IRQ_EN_WIDTH                                                           1
#define DPTX1_MS_FEFIFO_ERR_IRQ_EN_MASK                                                         0x02
#define DPTX1_MS_FEFIFO_ERR_IRQ_EN_OFFSET                                                          1
#define DPTX1_MS_FEFIFO_ERR_IRQ_EN_WIDTH                                                           1
// #define Reserved_MASK                                                                           0xfc
// #define Reserved_OFFSET                                                                            2
// #define Reserved_WIDTH                                                                             6



// Register DPTX1_MS_LTA_CTRL(0xb0c)
#define DPTX1_MS_LTA_CTRL                                                                 0x00000b0c //PA POD:0x20 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_TPG_EN_MASK                                                                    0x01
#define DPTX1_MS_TPG_EN_OFFSET                                                                     0
#define DPTX1_MS_TPG_EN_WIDTH                                                                      1
#define DPTX1_MS_TPG_PATTERN_MASK                                                               0x06
#define DPTX1_MS_TPG_PATTERN_OFFSET                                                                1
#define DPTX1_MS_TPG_PATTERN_WIDTH                                                                 2
#define DPTX1_MS_FEFIFO_EOL_OPT_MASK                                                            0x08
#define DPTX1_MS_FEFIFO_EOL_OPT_OFFSET                                                             3
#define DPTX1_MS_FEFIFO_EOL_OPT_WIDTH                                                              1
#define DPTX1_MS_EQUAL_DISTR_DIS_MASK                                                           0x10
#define DPTX1_MS_EQUAL_DISTR_DIS_OFFSET                                                            4
#define DPTX1_MS_EQUAL_DISTR_DIS_WIDTH                                                             1
#define DPTX1_MS_VBLANK_REGEN_EN_MASK                                                           0x20
#define DPTX1_MS_VBLANK_REGEN_EN_OFFSET                                                            5
#define DPTX1_MS_VBLANK_REGEN_EN_WIDTH                                                             1
#define DPTX1_MSA_ADVANCED_DIS_MASK                                                             0x40
#define DPTX1_MSA_ADVANCED_DIS_OFFSET                                                              6
#define DPTX1_MSA_ADVANCED_DIS_WIDTH                                                               1
#define DPTX1_MS_NOISE_PATGEN_EN_MASK                                                           0x80
#define DPTX1_MS_NOISE_PATGEN_EN_OFFSET                                                            7
#define DPTX1_MS_NOISE_PATGEN_EN_WIDTH                                                             1



// Register DPTX1_MS_STATUS(0xb10)
#define DPTX1_MS_STATUS                                                                   0x00000b10 //CRO POD:0x0
#define DPTX1_MS_VID_ABSENT_IRQ_STS_MASK                                                        0x01
#define DPTX1_MS_VID_ABSENT_IRQ_STS_OFFSET                                                         0
#define DPTX1_MS_VID_ABSENT_IRQ_STS_WIDTH                                                          1
#define DPTX1_MS_FEFIFO_ERROR_STS_MASK                                                          0x02
#define DPTX1_MS_FEFIFO_ERROR_STS_OFFSET                                                           1
#define DPTX1_MS_FEFIFO_ERROR_STS_WIDTH                                                            1
#define DPTX1_MS_STATUS_RSRV_MASK                                                               0xfc
#define DPTX1_MS_STATUS_RSRV_OFFSET                                                                2
#define DPTX1_MS_STATUS_RSRV_WIDTH                                                                 6



// Register DPTX1_TPG_CTRL(0xb14)
#define DPTX1_TPG_CTRL                                                                    0x00000b14 //RW POD:0x0
#define DPTX1_TPG_OVERLAY_MODE_MASK                                                             0x01
#define DPTX1_TPG_OVERLAY_MODE_OFFSET                                                              0
#define DPTX1_TPG_OVERLAY_MODE_WIDTH                                                               1
#define DPTX1_TPG_FRAME_RESET_LINE_MASK                                                         0x7e
#define DPTX1_TPG_FRAME_RESET_LINE_OFFSET                                                          1
#define DPTX1_TPG_FRAME_RESET_LINE_WIDTH                                                           6
// #define Reserved_MASK                                                                           0x80
// #define Reserved_OFFSET                                                                            7
// #define Reserved_WIDTH                                                                             1



// Register DPTX1_MS_TU_SIZE(0xb18)
#define DPTX1_MS_TU_SIZE                                                                  0x00000b18 //RW POD:0x1f
#define DPTX1_MS_TU_SIZE_MASK                                                                   0x3f
#define DPTX1_MS_TU_SIZE_OFFSET                                                                    0
#define DPTX1_MS_TU_SIZE_WIDTH                                                                     6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_MS_VIDEO_STRM_DLY(0xb1c)
#define DPTX1_MS_VIDEO_STRM_DLY                                                           0x00000b1c //RW POD:0x21
#define DPTX1_MS_VIDEO_STRM_DLY_MASK                                                            0x3f
#define DPTX1_MS_VIDEO_STRM_DLY_OFFSET                                                             0
#define DPTX1_MS_VIDEO_STRM_DLY_WIDTH                                                              6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_MS_PCKT_GUARD_ZN(0xb20)
#define DPTX1_MS_PCKT_GUARD_ZN                                                            0x00000b20 //RW POD:0x21
#define DPTX1_MS_PCKT_GUARD_ZN_MASK                                                             0x3f
#define DPTX1_MS_PCKT_GUARD_ZN_OFFSET                                                              0
#define DPTX1_MS_PCKT_GUARD_ZN_WIDTH                                                               6
// #define Reserved_MASK                                                                           0xc0
// #define Reserved_OFFSET                                                                            6
// #define Reserved_WIDTH                                                                             2



// Register DPTX1_MS_BS_REGEN_CTRL(0xb24)
#define DPTX1_MS_BS_REGEN_CTRL                                                            0x00000b24 //RW POD:0x0
#define DPTX1_MS_BS_VB_GEN_SRC_MASK                                                             0x03
#define DPTX1_MS_BS_VB_GEN_SRC_OFFSET                                                              0
#define DPTX1_MS_BS_VB_GEN_SRC_WIDTH                                                               2
#define DPTX1_MS_BS_PER_MEAS_SEL_MASK                                                           0x04
#define DPTX1_MS_BS_PER_MEAS_SEL_OFFSET                                                            2
#define DPTX1_MS_BS_PER_MEAS_SEL_WIDTH                                                             1
#define DPTX1_MS_BS_VB_REGEN_DIS_MASK                                                           0x08
#define DPTX1_MS_BS_VB_REGEN_DIS_OFFSET                                                            3
#define DPTX1_MS_BS_VB_REGEN_DIS_WIDTH                                                             1
#define DPTX1_MS_BS_NV_REGEN_DIS_MASK                                                           0x10
#define DPTX1_MS_BS_NV_REGEN_DIS_OFFSET                                                            4
#define DPTX1_MS_BS_NV_REGEN_DIS_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xe0
// #define Reserved_OFFSET                                                                            5
// #define Reserved_WIDTH                                                                             3



// Register DPTX1_MS_BS_PER_MEAS(0xb28)
#define DPTX1_MS_BS_PER_MEAS                                                              0x00000b28 //RO POD:0x0
#define DPTX1_MS_BS_PER_MEAS_MASK                                                             0xffff
#define DPTX1_MS_BS_PER_MEAS_OFFSET                                                                0
#define DPTX1_MS_BS_PER_MEAS_WIDTH                                                                16



// Register DPTX1_MS_BS_VB_PER(0xb2c)
#define DPTX1_MS_BS_VB_PER                                                                0x00000b2c //RW POD:0x0
#define DPTX1_MS_BS_VB_PER_MASK                                                               0xffff
#define DPTX1_MS_BS_VB_PER_OFFSET                                                                  0
#define DPTX1_MS_BS_VB_PER_WIDTH                                                                  16



// Register DPTX1_MS_BS_NV_PER(0xb30)
#define DPTX1_MS_BS_NV_PER                                                                0x00000b30 //RW POD:0xffff
#define DPTX1_MS_BS_NV_PER_MASK                                                               0xffff
#define DPTX1_MS_BS_NV_PER_OFFSET                                                                  0
#define DPTX1_MS_BS_NV_PER_WIDTH                                                                  16



// Register DPTX1_MS_SR_PER(0xb34)
#define DPTX1_MS_SR_PER                                                                   0x00000b34 //RW POD:0x0
#define DPTX1_MS_SR_PER_MASK                                                                    0xff
#define DPTX1_MS_SR_PER_OFFSET                                                                     0
#define DPTX1_MS_SR_PER_WIDTH                                                                      8



// Register DPTX1_MS_FORMAT_0(0xb40)
#define DPTX1_MS_FORMAT_0                                                                 0x00000b40 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_CLOCK_TYPE_MASK                                                                0x01
#define DPTX1_MS_CLOCK_TYPE_OFFSET                                                                 0
#define DPTX1_MS_CLOCK_TYPE_WIDTH                                                                  1
#define DPTX1_MS_COLOR_SPACE_MASK                                                               0x06
#define DPTX1_MS_COLOR_SPACE_OFFSET                                                                1
#define DPTX1_MS_COLOR_SPACE_WIDTH                                                                 2
#define DPTX1_MS_DYNAMIC_RNG_MASK                                                               0x08
#define DPTX1_MS_DYNAMIC_RNG_OFFSET                                                                3
#define DPTX1_MS_DYNAMIC_RNG_WIDTH                                                                 1
#define DPTX1_MS_COLOR_COEFF_MASK                                                               0x10
#define DPTX1_MS_COLOR_COEFF_OFFSET                                                                4
#define DPTX1_MS_COLOR_COEFF_WIDTH                                                                 1
#define DPTX1_MS_COLOR_DEPTH_MASK                                                               0xe0
#define DPTX1_MS_COLOR_DEPTH_OFFSET                                                                5
#define DPTX1_MS_COLOR_DEPTH_WIDTH                                                                 3



// Register DPTX1_MS_FORMAT_1(0xb44)
#define DPTX1_MS_FORMAT_1                                                                 0x00000b44 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_VTOTAL_EVEN_MASK                                                               0x01
#define DPTX1_MS_VTOTAL_EVEN_OFFSET                                                                0
#define DPTX1_MS_VTOTAL_EVEN_WIDTH                                                                 1
#define DPTX1_MS_STEREO_VIDEO_MASK                                                              0x02
#define DPTX1_MS_STEREO_VIDEO_OFFSET                                                               1
#define DPTX1_MS_STEREO_VIDEO_WIDTH                                                                1
#define DPTX1_MS_STEREO_MODE_MASK                                                               0x04
#define DPTX1_MS_STEREO_MODE_OFFSET                                                                2
#define DPTX1_MS_STEREO_MODE_WIDTH                                                                 1
#define DPTX1_MS_INTERLACED_MODE_MASK                                                           0x08
#define DPTX1_MS_INTERLACED_MODE_OFFSET                                                            3
#define DPTX1_MS_INTERLACED_MODE_WIDTH                                                             1
#define DPTX1_MS_FORMAT_RSRV_MASK                                                               0xf0
#define DPTX1_MS_FORMAT_RSRV_OFFSET                                                                4
#define DPTX1_MS_FORMAT_RSRV_WIDTH                                                                 4



// Register DPTX1_MS_HTOTAL(0xb48)
#define DPTX1_MS_HTOTAL                                                                   0x00000b48 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_HTOTAL_MASK                                                                  0xffff
#define DPTX1_MS_HTOTAL_OFFSET                                                                     0
#define DPTX1_MS_HTOTAL_WIDTH                                                                     16



// Register DPTX1_MS_HACT_START(0xb4c)
#define DPTX1_MS_HACT_START                                                               0x00000b4c //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_HACT_START_MASK                                                              0xffff
#define DPTX1_MS_HACT_START_OFFSET                                                                 0
#define DPTX1_MS_HACT_START_WIDTH                                                                 16



// Register DPTX1_MS_HACT_WIDTH(0xb50)
#define DPTX1_MS_HACT_WIDTH                                                               0x00000b50 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_HACT_WIDTH_MASK                                                              0xffff
#define DPTX1_MS_HACT_WIDTH_OFFSET                                                                 0
#define DPTX1_MS_HACT_WIDTH_WIDTH                                                                 16



// Register DPTX1_MS_HSYNC_WIDTH(0xb54)
#define DPTX1_MS_HSYNC_WIDTH                                                              0x00000b54 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_HSYNC_WIDTH_MASK                                                             0x7fff
#define DPTX1_MS_HSYNC_WIDTH_OFFSET                                                                0
#define DPTX1_MS_HSYNC_WIDTH_WIDTH                                                                15
#define DPTX1_MS_HPOLARITY_MASK                                                               0x8000
#define DPTX1_MS_HPOLARITY_OFFSET                                                                 15
#define DPTX1_MS_HPOLARITY_WIDTH                                                                   1



// Register DPTX1_MS_VTOTAL(0xb58)
#define DPTX1_MS_VTOTAL                                                                   0x00000b58 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_VTOTAL_MASK                                                                  0xffff
#define DPTX1_MS_VTOTAL_OFFSET                                                                     0
#define DPTX1_MS_VTOTAL_WIDTH                                                                     16



// Register DPTX1_MS_VACT_START(0xb5c)
#define DPTX1_MS_VACT_START                                                               0x00000b5c //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_VACT_START_MASK                                                              0xffff
#define DPTX1_MS_VACT_START_OFFSET                                                                 0
#define DPTX1_MS_VACT_START_WIDTH                                                                 16



// Register DPTX1_MS_VACT_WIDTH(0xb60)
#define DPTX1_MS_VACT_WIDTH                                                               0x00000b60 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_VACT_WIDTH_MASK                                                              0xffff
#define DPTX1_MS_VACT_WIDTH_OFFSET                                                                 0
#define DPTX1_MS_VACT_WIDTH_WIDTH                                                                 16



// Register DPTX1_MS_VSYNC_WIDTH(0xb64)
#define DPTX1_MS_VSYNC_WIDTH                                                              0x00000b64 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_VSYNC_WIDTH_MASK                                                             0x7fff
#define DPTX1_MS_VSYNC_WIDTH_OFFSET                                                                0
#define DPTX1_MS_VSYNC_WIDTH_WIDTH                                                                15
#define DPTX1_MS_VPOLARITY_MASK                                                               0x8000
#define DPTX1_MS_VPOLARITY_OFFSET                                                                 15
#define DPTX1_MS_VPOLARITY_WIDTH                                                                   1



// Register DPTX1_MS_M(0xb68)
#define DPTX1_MS_M                                                                        0x00000b68 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_M_MASK                                                                     0xffffff
#define DPTX1_MS_M_OFFSET                                                                          0
#define DPTX1_MS_M_WIDTH                                                                          24



// Register DPTX1_MS_M_MEAS(0xb6c)
#define DPTX1_MS_M_MEAS                                                                   0x00000b6c //SRW POD:0x0
#define DPTX1_MS_M_MEAS_MASK                                                                0xffffff
#define DPTX1_MS_M_MEAS_OFFSET                                                                     0
#define DPTX1_MS_M_MEAS_WIDTH                                                                     24



// Register DPTX1_MS_N(0xb70)
#define DPTX1_MS_N                                                                        0x00000b70 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_N_MASK                                                                     0xffffff
#define DPTX1_MS_N_OFFSET                                                                          0
#define DPTX1_MS_N_WIDTH                                                                          24



// Register DPTX1_MS_TH_CTRL(0xb74)
#define DPTX1_MS_TH_CTRL                                                                  0x00000b74 //RW POD:0x0
#define DPTX1_MS_M_LOW_TH_MASK                                                                  0x07
#define DPTX1_MS_M_LOW_TH_OFFSET                                                                   0
#define DPTX1_MS_M_LOW_TH_WIDTH                                                                    3
#define DPTX1_MS_TH_RSRV1_MASK                                                                  0x08
#define DPTX1_MS_TH_RSRV1_OFFSET                                                                   3
#define DPTX1_MS_TH_RSRV1_WIDTH                                                                    1
#define DPTX1_MS_DE_TH_MASK                                                                     0x70
#define DPTX1_MS_DE_TH_OFFSET                                                                      4
#define DPTX1_MS_DE_TH_WIDTH                                                                       3
#define DPTX1_MS_TH_RSRV2_MASK                                                                  0x80
#define DPTX1_MS_TH_RSRV2_OFFSET                                                                   7
#define DPTX1_MS_TH_RSRV2_WIDTH                                                                    1



// Register DPTX1_MS_VID_STATUS(0xb78)
#define DPTX1_MS_VID_STATUS                                                               0x00000b78 //RO POD:0x0
#define DPTX1_EXT_VIDEO_STS_MASK                                                                0x01
#define DPTX1_EXT_VIDEO_STS_OFFSET                                                                 0
#define DPTX1_EXT_VIDEO_STS_WIDTH                                                                  1
#define DPTX1_VIDEO_CLK_STS_MASK                                                                0x02
#define DPTX1_VIDEO_CLK_STS_OFFSET                                                                 1
#define DPTX1_VIDEO_CLK_STS_WIDTH                                                                  1
#define DPTX1_DE_STS_MASK                                                                       0x04
#define DPTX1_DE_STS_OFFSET                                                                        2
#define DPTX1_DE_STS_WIDTH                                                                         1
#define DPTX1_VBLANK_STS_MASK                                                                   0x08
#define DPTX1_VBLANK_STS_OFFSET                                                                    3
#define DPTX1_VBLANK_STS_WIDTH                                                                     1
#define DPTX1_MS_VID_STS_MASK                                                                   0xf0
#define DPTX1_MS_VID_STS_OFFSET                                                                    4
#define DPTX1_MS_VID_STS_WIDTH                                                                     4



// Register DPTX1_MSA_CTRL(0xb7c)
#define DPTX1_MSA_CTRL                                                                    0x00000b7c //RW POD:0x0
#define DPTX1_MSA_VB_TX_COUNT_MASK                                                            0x0007
#define DPTX1_MSA_VB_TX_COUNT_OFFSET                                                               0
#define DPTX1_MSA_VB_TX_COUNT_WIDTH                                                                3
#define DPTX1_VBLANK_DEL_EN_MASK                                                              0x0008
#define DPTX1_VBLANK_DEL_EN_OFFSET                                                                 3
#define DPTX1_VBLANK_DEL_EN_WIDTH                                                                  1
#define DPTX1_MSA_NV_TX_COUNT_MASK                                                            0x0070
#define DPTX1_MSA_NV_TX_COUNT_OFFSET                                                               4
#define DPTX1_MSA_NV_TX_COUNT_WIDTH                                                                3
#define DPTX1_MSA_NV_TX_EN_MASK                                                               0x0080
#define DPTX1_MSA_NV_TX_EN_OFFSET                                                                  7
#define DPTX1_MSA_NV_TX_EN_WIDTH                                                                   1
#define DPTX1_MSA_VB_TX_POSITION_MASK                                                         0xff00
#define DPTX1_MSA_VB_TX_POSITION_OFFSET                                                            8
#define DPTX1_MSA_VB_TX_POSITION_WIDTH                                                             8



// Register DPTX1_MS_RSRV(0xb80)
#define DPTX1_MS_RSRV                                                                     0x00000b80 //RW POD:0x0
#define DPTX1_MS_DATPAK_FIFO_STALL_MASK                                                         0x01
#define DPTX1_MS_DATPAK_FIFO_STALL_OFFSET                                                          0
#define DPTX1_MS_DATPAK_FIFO_STALL_WIDTH                                                           1
#define DPTX1_MS_VIDEO_RESET_MASK                                                               0x02
#define DPTX1_MS_VIDEO_RESET_OFFSET                                                                1
#define DPTX1_MS_VIDEO_RESET_WIDTH                                                                 1
#define DPTX1_MS_M_MEAS_FORCE_MASK                                                              0x04
#define DPTX1_MS_M_MEAS_FORCE_OFFSET                                                               2
#define DPTX1_MS_M_MEAS_FORCE_WIDTH                                                                1
#define DPTX1_MS_SCNDR_RESET_MASK                                                               0x08
#define DPTX1_MS_SCNDR_RESET_OFFSET                                                                3
#define DPTX1_MS_SCNDR_RESET_WIDTH                                                                 1
#define DPTX1_AUTO_SECOND_PACK_RST_MASK                                                         0x10
#define DPTX1_AUTO_SECOND_PACK_RST_OFFSET                                                          4
#define DPTX1_AUTO_SECOND_PACK_RST_WIDTH                                                           1
#define DPTX1_MS_IDP_MODE_MASK                                                                  0x20
#define DPTX1_MS_IDP_MODE_OFFSET                                                                   5
#define DPTX1_MS_IDP_MODE_WIDTH                                                                    1
#define DPTX1_MSA_DISABLE_MASK                                                                  0x40
#define DPTX1_MSA_DISABLE_OFFSET                                                                   6
#define DPTX1_MSA_DISABLE_WIDTH                                                                    1
#define DPTX1_MS_RSRV_MASK                                                                      0x80
#define DPTX1_MS_RSRV_OFFSET                                                                       7
#define DPTX1_MS_RSRV_WIDTH                                                                        1



// Register DPTX1_MS_IDP_DATA_LN_CNT(0xb84)
#define DPTX1_MS_IDP_DATA_LN_CNT                                                          0x00000b84 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_IDP_DATA_LN_CNT_MASK                                                           0x0f
#define DPTX1_MS_IDP_DATA_LN_CNT_OFFSET                                                            0
#define DPTX1_MS_IDP_DATA_LN_CNT_WIDTH                                                             4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX1_MS_IDP_PHY_LN_CNT(0xb88)
#define DPTX1_MS_IDP_PHY_LN_CNT                                                           0x00000b88 //RW POD:0x0
#define DPTX1_MS_IDP_PHY_LN_CNT_MASK                                                            0x0f
#define DPTX1_MS_IDP_PHY_LN_CNT_OFFSET                                                             0
#define DPTX1_MS_IDP_PHY_LN_CNT_WIDTH                                                              4
// #define Reserved_MASK                                                                           0xf0
// #define Reserved_OFFSET                                                                            4
// #define Reserved_WIDTH                                                                             4



// Register DPTX1_MS_IDP_PACK_CTRL(0xb8c)
#define DPTX1_MS_IDP_PACK_CTRL                                                            0x00000b8c //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_MS_IDP_PACK_MODE_MASK                                                             0x07
#define DPTX1_MS_IDP_PACK_MODE_OFFSET                                                              0
#define DPTX1_MS_IDP_PACK_MODE_WIDTH                                                               3
// #define Reserved_MASK                                                                           0xf8
// #define Reserved_OFFSET                                                                            3
// #define Reserved_WIDTH                                                                             5



// Register DPTX1_FE_FIFO_CTRL(0xb90)
#define DPTX1_FE_FIFO_CTRL                                                                0x00000b90 //RW POD:0x0
#define DPTX1_ENABLE_FE_FIFO_ACC_MASK                                                           0x01
#define DPTX1_ENABLE_FE_FIFO_ACC_OFFSET                                                            0
#define DPTX1_ENABLE_FE_FIFO_ACC_WIDTH                                                             1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX1_FE_FIFO_ACC(0xb94)
#define DPTX1_FE_FIFO_ACC                                                                 0x00000b94 //RW POD:0x0
#define DPTX1_FE_FIFO_READ_RATE_MASK                                                          0xffff
#define DPTX1_FE_FIFO_READ_RATE_OFFSET                                                             0
#define DPTX1_FE_FIFO_READ_RATE_WIDTH                                                             16



// Register DPTX1_SDP_RSRV(0xb98)
#define DPTX1_SDP_RSRV                                                                    0x00000b98 //RW POD:0x0
#define DPTX1_SDP_RSRV_MASK                                                                     0xff
#define DPTX1_SDP_RSRV_OFFSET                                                                      0
#define DPTX1_SDP_RSRV_WIDTH                                                                       8



// Register DPTX1_TEST_CRC_CTRL(0xba0)
#define DPTX1_TEST_CRC_CTRL                                                               0x00000ba0 //PA POD:0x0 Event:DPTX1_MS_UPDATE
#define DPTX1_CRC_EN_MASK                                                                       0x01
#define DPTX1_CRC_EN_OFFSET                                                                        0
#define DPTX1_CRC_EN_WIDTH                                                                         1
// #define Reserved_MASK                                                                           0xfe
// #define Reserved_OFFSET                                                                            1
// #define Reserved_WIDTH                                                                             7



// Register DPTX1_TEST_CRC_R(0xba4)
#define DPTX1_TEST_CRC_R                                                                  0x00000ba4 //RO POD:0x0
#define DPTX1_TEST_CRC_R_MASK                                                                 0xffff
#define DPTX1_TEST_CRC_R_OFFSET                                                                    0
#define DPTX1_TEST_CRC_R_WIDTH                                                                    16



// Register DPTX1_TEST_CRC_G(0xba8)
#define DPTX1_TEST_CRC_G                                                                  0x00000ba8 //RO POD:0x0
#define DPTX1_TEST_CRC_G_MASK                                                                 0xffff
#define DPTX1_TEST_CRC_G_OFFSET                                                                    0
#define DPTX1_TEST_CRC_G_WIDTH                                                                    16



// Register DPTX1_TEST_CRC_B(0xbac)
#define DPTX1_TEST_CRC_B                                                                  0x00000bac //RO POD:0x0
#define DPTX1_TEST_CRC_B_MASK                                                                 0xffff
#define DPTX1_TEST_CRC_B_OFFSET                                                                    0
#define DPTX1_TEST_CRC_B_WIDTH                                                                    16





#endif /*_MPE41_DPTX_REGS_ */
