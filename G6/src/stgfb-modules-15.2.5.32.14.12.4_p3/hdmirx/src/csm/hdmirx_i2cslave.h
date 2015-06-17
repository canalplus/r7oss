/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_i2cslave.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_I2CSLAVE_H__
#define __HDMIRX_I2CSLAVE_H__

/*Includes------------------------------------------------------------------------------*/
#include "stddefs.h"
#include "InternalTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/

  typedef enum {
    I2C_SLAVE_STATE_FREE = 0,
    I2C_SLAVE_STATE_DATA_ERROR,
    I2C_SLAVE_STATE_READ,
    I2C_SLAVE_STATE_WRITE,
    I2C_SLAVE_STATE_NACK,
    I2C_SLAVE_STATE_START,
    I2C_SLAVE_STATE_STOP
  }
  sthdmirx_I2C_slave_state_t;

  typedef enum
  {
    I2C_SLAVE_DIR_WRITE,
    I2C_SLAVE_DIR_READ
  } sthdmirx_I2C_slave_direction_t;

  typedef struct
  {
    U8 uDevAddress;
    U8 uStop;
    sthdmirx_I2C_slave_direction_t uTransactionDir;
    sthdmirx_I2C_slave_state_t eI2CSlaveState;
    U32 uLength;
    U8 *puBuffer;
    U8 PortNo;
  } sthdmirx_I2C_slave_control_t;
  /* Private Constants ---------------------------------------------------------------------*/

  /* Private variables (static) ----------------------------------------------------------------*/

  /* Private Macros ------------------------------------------------------------------------*/
#define I2C_SLAVE_ADDR_READ_MASK			0x01
#define I2C_SLAVE_DEVICEID_MASK				0xFE
#define I2C_SLAVE_TIMEOUT					0x7FFF
#define I2C_CLK_STRETCH_DELAY				0x4000
#define I2C_MAX_FIFO_LENGTH					8
#define I2C_SLAVE_BYTE_MASK					0x0000000F
#define I2C_SLAVE_REG_TABLE_BITS			32

  /* Global Variables -----------------------------------------------------------------------*/

  /* Exported Macros--------------------------------------------------------- --------------*/

  /****************************************************************************
          REG OFFSET   D E F I N I T I O N S
  ****************************************************************************/
#define     I2C_SLAVE_MODULE_BASE_ADDRS             0x03F0

#define     I2C_SLV1_CTRL                           0x0000
#define     I2C_SLV1_IRQ_CTRL                       0x0004
#define     I2C_SLV1_STS_CLR	                    0x0008
#define     I2C_SLV1_STATUS	                        0x000C
#define     I2C_SLV1_IRQ_STATUS                     0x0010
#define     I2C_SLV1_IN_FIFO_CNTR					0x0014
#define     I2C_SLV1_OUT_FIFO_CNTR				    0x0018
#define     I2C_SLV1_DATA							0x001c
#define     I2C_SLV1_RX_DEV_ID						0x0020
#define     I2C_SLV1_ADDR_TBL0						0x0024
#define     I2C_SLV1_ADDR_TBL1						0x0028
#define     I2C_SLV1_ADDR_TBL2						0x002c
#define     I2C_SLV1_ADDR_TBL3						0x0030

  /* Offset for each of the slave register blocks */
#define     I2C_SLV_REG_SIZE						0x34

  /****************************************************************************
      B I T   D E F I N I T I O N S
  ****************************************************************************/
  /* I2C_SLV1_CTRL                                    (0x84C0) */
  // I2C_SLV1_CTRL                                    (0x83F0)
#define I2C_SLV1_EN                                 BIT0
#define I2C_SLV1_CLOCK_STRETCH_EN                	BIT1
#define I2C_SLV1_DMA_RFIFO_EN                       BIT2
#define I2C_SLV1_DMA_WRITE_FIFO_EN              	BIT3
#define I2C_SLV1_MAKE_STRETCH                       BIT4

#define I2C_SLV1_DEVID_STR_BEFORE_ACK	    	    BIT5
#define I2C_SLV1_NO_READ_FIFO_MODE			        BIT6
#define I2C_SLV1_NO_WRITE_FIFO_MODE		            BIT7

  /* I2C_SLV1_IRQ_STATUS                                                  (0x8400) */

#define I2C_SLV1_STOP			                    BIT0
#define I2C_SLV1_START                       	    BIT1
#define I2C_SLV1_STRETCH_IN              			BIT2
#define I2C_SLV1_STRETCH_OUT             			BIT3
#define I2C_SLV1_DEV_ID_RECEIVED			        BIT4
#define I2C_SLV1_READ_FIFO_nE                  		BIT5
#define I2C_SLV1_READ_FIFO_AF                   	BIT6
#define I2C_SLV1_WRITE_FIFO_E                   	BIT7
#define I2C_SLV1_WRITE_FIFO_AE                   	BIT8
#define I2C_SLV1_DATA_ERROR              			BIT9
#define I2C_SLV1_IRQ_EN                             BIT10

  /* I2C_SLV1_IRQ_CTRL                                (0x84C8) */

#define I2C_SLV1_IRQ_CTRL_MASK				        0x000003ff

  /* I2C_SLV1_STS_CLR */
#define I2C_SLV1_READ_FIFO_RESET                	0x00000001
#define I2C_SLV1_WRITE_FIFO_RESET    	            0x00000002
#define I2C_SLV1_DEV_STRETCH_CLR                 	0x00000004
#define I2C_SLV1_NACK_SET                           0x00000008

  /* I2C_SLV1_STATUS                                  (0x84D0) */
#define I2C_SLV1_WRITE_FIFO_NF                      BIT0
#define I2C_SLV1_READ_FIFO_F                        BIT1
#define I2C_SLV1_STRETCH_OUT_STS                  	BIT2
#define I2C_SLV1_STRETCH_IN_STS                    	BIT3

  /* I2C_SLV1_DATA */
#define I2C_SLV1_DATA_MASK					        0x000000ff

  /* Exported Functions ----------------------------------------------------- ---------------*/

  /* ------------------------------- End of file --------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_I2CSLAVE_H__ */
