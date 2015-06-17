/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_i2cmaster.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_I2CMASTER_H__
#define __HDMIRX_I2CMASTER_H__

/*Includes------------------------------------------------------------------------------*/
#include "stddefs.h"
#include "InternalTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/
  typedef enum {
    I2C_MASTER_STATE_FREE = 0,
    I2C_MASTER_STATE_STRETCH,
    I2C_MASTER_STATE_NACK,
    I2C_MASTER_STATE_START,
    I2C_MASTER_STATE_REP_START,
    I2C_MASTER_STATE_MIDDLE,
    I2C_MASTER_STATE_STOP,
    I2C_MASTER_STATE_NO_INITED,
    I2C_MASTER_STATE_END
  }
  sthdmirx_I2C_master_state_t;

  typedef enum
  {
    I2C_MASTER_DIR_WRITE,
    I2C_MASTER_DIR_READ
  } sthdmirx_I2C_master_direction_t;

  typedef enum
  {
    I2C_OK,
    I2C_ERR_TMO,
    I2C_ERR_NACK
  } sthdmirx_I2C_error_type_t;

  typedef struct
  {
    U32 ulCellBaseAddr;
    U8 uDevAddress;
    U8 uStop;
    sthdmirx_I2C_master_direction_t uTransactionDir;
    sthdmirx_I2C_master_state_t eI2CMasterState;
    U32 uLength;
    U8 *puBuffer;
  } sthdmirx_I2C_master_control_t;
  /* Private Constants ---------------------------------------------------------------------*/

  /* Private variables (static) ----------------------------------------------------------------*/

  /* Private Macros ------------------------------------------------------------------------*/

  /* Global Variables -----------------------------------------------------------------------*/

  /* Exported Macros--------------------------------------------------------- --------------*/

  /*DOUBLE U16*/

  /****************************************************************************
          REG OFFSET   D E F I N I T I O N S
  ****************************************************************************/
#define     I2C_MASTER_CTRL                                 0x0000
#define     I2C_MASTER_CLK_SCALE                            0x0004
#define     I2C_MASTER_TX_CTRL                              0x0008
#define     I2C_MASTER_TX_DATA                              0x000C
#define     I2C_MASTER_STATUS                               0x0010
#define     I2C_MASTER_RX_DATA                              0x0014

  /****************************************************************************
      B I T   D E F I N I T I O N S
  ****************************************************************************/
  /* I2C_MASTER_CTRL                                      (0x84C0) */
#define I2C_MASTER_EN                                   BIT0
#define I2C_MASTER_IRQ_EN                               BIT1

  /* I2C_MASTER_TX_CTRL                                   (0x84C8) */
#define I2C_MASTER_IACK                                 BIT0
#define I2C_SLVID_RSRV                                  0x06
#define I2C_SLVID_RSRV_SHIFT                            1
#define I2C_MASTER_ACK                                  BIT3
#define I2C_MASTER_WR                                   BIT4
#define I2C_MASTER_RD                                   BIT5
#define I2C_MASTER_STO                                  BIT6
#define I2C_MASTER_STA                                  BIT7

  /* I2C_MASTER_STATUS                                    (0x84D0) */
#define I2C_MASTER_IRQ_STS                              BIT0
#define I2C_MASTER_TIP                                  BIT1
#define I2C_MASTER_BUSY                                 BIT6
#define I2C_MASTER_RXACK                                BIT7

  /* Exported Functions ----------------------------------------------------- ---------------*/

  /* ------------------------------- End of file --------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_I2CMASTER_H__ */
