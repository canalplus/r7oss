/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_cec is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_cec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : cec_reg.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#ifndef _CEC_REG_H_
#define _CEC_REG_H_

#define CEC_BIT0			0x00000001
#define CEC_BIT1			0x00000002
#define CEC_BIT2			0x00000004
#define CEC_BIT3			0x00000008
#define CEC_BIT4			0x00000010
#define CEC_BIT5			0x00000020
#define CEC_BIT6			0x00000040
#define CEC_BIT7			0x00000080
#define CEC_BIT8			0x00000100
#define CEC_BIT9			0x00000200
#define CEC_BIT10			0x00000400
#define CEC_BIT11			0x00000800
#define CEC_BIT12			0x00001000
#define CEC_BIT13			0x00002000
#define CEC_BIT14			0x00004000
#define CEC_BIT15			0x00008000



/* CEC registers  */
#define CEC_CLK_DIV			0x0
#define CEC_CTRL			0x4
#define CEC_IRQ_CTRL			0x8
#define CEC_STATUS			0xC
#define CEC_EXT_STATUS			0x10
#define CEC_TX_CTRL			0x14
#define CEC_FREE_TIME_THRESH		0x18
#define CEC_BIT_TOUT_THRESH		0x1C
#define CEC_BIT_PULSE_THRESH		0x20
#define CEC_DATA			0x24
#define CEC_TX_ARRAY_CTRL		0x28
#define CEC_CTRL2			0x2C
#define CEC_TX_ERROR_STS		0x30
#define CEC_ADDR_TABLE			0x34
#define CEC_DATA_ARRAY_CTRL		0x38
#define CEC_DATA_ARRAY_STATUS		0x3C



#define CEC_TX_DATA_BASE		0x40
#define CEC_TX_DATA_TOP			0x50
#define CEC_TX_DATA_SIZE		0x1

#define CEC_RX_DATA_BASE		0x54
#define CEC_RX_DATA_TOP			0x64
#define CEC_RX_DATA_SIZE		0x1

/****************************************************************************
    B I T   D E F I N I T I O N S
****************************************************************************/

/* CEC_CLK_DIV*/
#define CEC_RSRV                              CEC_BIT15

/* CEC_CTRL*/
#define CEC_IN_FILTER_EN                      CEC_BIT0
#define CEC_PWR_SAVE_EN                       CEC_BIT1
#define CEC_EN                                CEC_BIT4
#define CEC_ACK_CTRL                          CEC_BIT5
#define CEC_RX_RESET_EN                       CEC_BIT6
#define CEC_IGNORE_RX_ERROR                   CEC_BIT7

/* CEC_IRQ_CTRL*/
#define CEC_TX_DONE_IRQ_EN                    CEC_BIT0
#define CEC_ERROR_IRQ_EN                      CEC_BIT2
#define CEC_RX_DONE_IRQ_EN                    CEC_BIT3
#define CEC_RX_SOM_IRQ_EN                     CEC_BIT4
#define CEC_RX_EOM_IRQ_EN                     CEC_BIT5
#define CEC_FREE_TIME_IRQ_EN                  CEC_BIT6
#define CEC_PIN_STS_IRQ_EN                    CEC_BIT7

/* CEC_STATUS*/
#define CEC_TX_DONE_STS                       CEC_BIT0
#define CEC_TX_ACK_GET_STS                    CEC_BIT1
#define CEC_ERROR_STS                         CEC_BIT2
#define CEC_RX_DONE_STS                       CEC_BIT3
#define CEC_RX_SOM_STS                        CEC_BIT4
#define CEC_RX_EOM_STS                        CEC_BIT5
#define CEC_FREE_TIME_IRQ_STS                 CEC_BIT6
#define CEC_PIN_STS                           CEC_BIT7
#define CEC_SBIT_TOUT_STS                     CEC_BIT8
#define CEC_DBIT_TOUT_STS                     CEC_BIT9
#define CEC_LPULSE_ERROR_STS                  CEC_BIT10
#define CEC_HPULSE_ERROR_STS                  CEC_BIT11
#define CEC_TX_ERROR                          CEC_BIT12
#define CEC_TX_ARB_ERROR                      CEC_BIT13
#define CEC_RX_ERROR_MIN                      CEC_BIT14
#define CEC_RX_ERROR_MAX                      CEC_BIT15

/* CEC_EXT_STATUS*/
#define CEC_FREE_TIME                         0x0F
#define CEC_RX_BUSY_STS                       CEC_BIT4
#define CEC_PIN_RD                            CEC_BIT5


/* CEC_TX_CTRL*/
#define CEC_TX_SOM                            CEC_BIT0
#define CEC_TX_EOM                            CEC_BIT1
#define CEC_PIN_WR                            0x0C
#define CEC_PIN_WR_SHIFT                      2
#define CEC_BIT_PERIOD_THRESH                 0xF0
#define CEC_BIT_PERIOD_THRESH_SHIFT           4

/*CEC_BIT_TOUT_THRESH*/
#define CEC_SCEC_BITPER_TOUT_THRESH               0x07
#define CEC_DCEC_BITPER_TOUT_THRESH               0x70
#define CEC_DCEC_BITPER_TOUT_THRESH_SHIFT         4

/* CEC_BIT_PULSE_THRESH*/
#define CEC_BIT_LPULSE_THRESH                 0x03
#define CEC_BIT_HPULSE_THRESH                 0x0C
#define CEC_BIT_HPULSE_THRESH_SHIFT           2

/* CEC_TX_ARRAY_CTRL*/
#define CEC_TX_N_OF_BYTES                     0x1F
#define CEC_TX_START                          CEC_BIT5
#define CEC_TX_AUTO_SOM_EN                    CEC_BIT6
#define CEC_TX_AUTO_EOM_EN                    CEC_BIT7

/* CEC_CTRL2*/
#define CEC_LINE_INACTIVE_EN                  CEC_BIT0
#define CEC_AUTO_BUS_ERR_EN                   CEC_BIT1
#define CEC_STOP_ON_ARB_ERR_EN                CEC_BIT2
#define CEC_TX_REQ_WAIT_EN                    CEC_BIT3

/* CEC_DATA_ARRAY_CTRL*/
#define CEC_TX_ARRAY_EN                       CEC_BIT0
#define CEC_RX_ARRAY_EN                       CEC_BIT1
#define CEC_TX_ARRAY_RESET                    CEC_BIT2
#define CEC_RX_ARRAY_RESET                    CEC_BIT3
#define CEC_TX_N_OF_BYTES_IRQ_EN              CEC_BIT4
#define CEC_TX_STOP_ON_NACK                   CEC_BIT7

/* CEC_DATA_ARRAY_STATUS*/
#define CEC_RX_N_OF_BYTES                     0x1F
#define CEC_TX_N_OF_BYTES_SENT                CEC_BIT5
#define CEC_RX_OVERRUN                        CEC_BIT6



#endif
