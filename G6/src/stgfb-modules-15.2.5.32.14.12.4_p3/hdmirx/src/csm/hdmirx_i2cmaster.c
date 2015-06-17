/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_i2cmaster.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifdef STHDMIRX_I2C_MASTER_ENABLE
/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/
#include <hdmirx_csm.h>
#include <hdmirx_i2cmaster.h>
/* Private Typedef -----------------------------------------------*/

/* Private Defines ------------------------------------------------*/
#define     I2C_MASTER_READ                 BIT0
#define     I2C_MASTER_TIMEOUT              0x7fff
#define     CLOCK_CORRECTION_FACTOR         2

/* Private macro's ------------------------------------------------*/

#define     GET_I2C_MASTER_STATUS(Address,statusbit)        \
    ((HDMI_READ_REG_DWORD(Address) & (statusbit))==(statusbit)?TRUE:FALSE)

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/
void sthdmirx_I2C_master_transaction(
  sthdmirx_I2C_master_handle_t I2CMasterHandle);
void sthdmirx_I2C_master_stop_transaction(
  sthdmirx_I2C_master_handle_t I2CMasterHandle);

/* Interface procedures/functions ----------------------------------*/
/******************************************************************************
 FUNCTION     :  sthdmirx_I2C_master_init
 USAGE        :  Initialize the I2C master drivers.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_I2C_master_init(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                              sthdmirx_I2C_master_init_params_t *pInits)
{
  U32 ulClkScaler = 0;
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;
  if (pInits->BaudRateHz < 1000)
    {
      /*Program to 100 KHz */
      pInits->BaudRateHz = 100000UL;
    }
  ulClkScaler =
    (U16) (((U32) pInits->InpClkFreqHz / pInits->BaudRateHz) / 6) - 1 - CLOCK_CORRECTION_FACTOR;

  Handle_p->ulCellBaseAddr = (U32) pInits->RegBaseAddrs;

  /* Disable the I2C master Control */
  HDMI_WRITE_REG_DWORD((U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), 0);

  /*Disable the interrupt */
  HDMI_CLEAR_REG_BITS_DWORD(
    (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  /*Write the clock scale value */
  HDMI_WRITE_REG_DWORD(
    (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_CLK_SCALE), ulClkScaler);

  /*Enable the I2C master */
  HDMI_SET_REG_BITS_DWORD(
    (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL),I2C_MASTER_EN);

  sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
}

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_I2C_master_stop_transaction(
  sthdmirx_I2C_master_handle_t I2CMasterHandle)
{
  U16 ulTimeout = I2C_MASTER_TIMEOUT;
  U8 uStatus;
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  uStatus = (U8)HDMI_READ_REG_DWORD(
              (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS));
  HDMI_CLEAR_REG_BITS_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if (uStatus & I2C_MASTER_BUSY)
    {
      HDMI_WRITE_REG_DWORD(
        (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        (I2C_MASTER_IACK | I2C_MASTER_STO));
      while (GET_I2C_MASTER_STATUS(
               (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
               I2C_MASTER_BUSY))
        {
          /* check timeout here */
          if (ulTimeout-- == 0)
            break;

        }
    }
  HDMI_WRITE_REG_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL), I2C_MASTER_IACK);
  Handle_p->eI2CMasterState = I2C_MASTER_STATE_FREE;

}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_waiting_write
 USAGE        :      Write to the I2C Master with Blocking Mode of Operation
 INPUT        :      uDevAddrs  =   I2C Address,
                     puBuffer   =   Pointer to read buffer
                     ulLenght   =   NoOfBytes to be read
                     uStop      =   Stop condition at the end of transaction.
 RETURN       :      Status of write Operation
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_I2C_master_waiting_write(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                       U8 uDevAddrs, U8 *puBuffer, U16 ulLength, U8 uStop)
{
  U16 ulTimeout = I2C_MASTER_TIMEOUT;
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;
  mdelay(1);

  HDMI_CLEAR_REG_BITS_DWORD((U32)
                            (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL),
                            I2C_MASTER_IRQ_EN);

  if ((Handle_p->uTransactionDir == I2C_MASTER_DIR_READ)
      || (Handle_p->uDevAddress != uDevAddrs))
    {
      sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
    }

  Handle_p->uTransactionDir = I2C_MASTER_DIR_WRITE;
  Handle_p->uDevAddress = uDevAddrs;
  Handle_p->uStop = uStop;

  if (Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH)
    {
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_DATA),
        (uDevAddrs & ~I2C_MASTER_READ));
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        (I2C_MASTER_WR | I2C_MASTER_STA | I2C_MASTER_IACK));

      /* wait for transmission to complete */
      while (GET_I2C_MASTER_STATUS(
               (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
               I2C_MASTER_TIP))
        {
          if (ulTimeout-- == 0)
            {
              sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
              HDMI_PRINT
              (" Error !!I2C Master Blocking Mode writing.. TimeOut\n");
              return FALSE;
            }
        }

      /* Check for an ACK from the slave */
      if (GET_I2C_MASTER_STATUS(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
            I2C_MASTER_RXACK))
        {
          sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
          HDMI_PRINT(" Error !!I2C Master Blocking Mode writing.. NACK\n");
          return FALSE;
        }
    }

  while (ulLength)
    {
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_DATA), *puBuffer);
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        (I2C_MASTER_WR | I2C_MASTER_IACK));

      ulTimeout = I2C_MASTER_TIMEOUT;
      /* wait for transmission to complete */
      while (GET_I2C_MASTER_STATUS(
               (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
               I2C_MASTER_TIP))
        {
          if (ulTimeout-- == 0)
            {
              sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
              HDMI_PRINT
              (" Error !!I2C Master Blocking Mode writing.. TimeOut\n");
              return FALSE;
            }
        }

      /*Check the ACK Status */
      if (GET_I2C_MASTER_STATUS(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
            I2C_MASTER_RXACK))
        {
          sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
          sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
          HDMI_PRINT(" Error !!I2C Master Blocking Mode writing.. NACK\n");
        }

      puBuffer++;
      ulLength--;

    }
  /* Send stop signal if only requested */
  if (uStop)
    sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
  else
    Handle_p->eI2CMasterState = I2C_MASTER_STATE_STRETCH;

  return TRUE;
}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_waiting_read
 USAGE        :      Read from the I2C Master with Blocking Mode of Operation
 INPUT        :      uDevAddrs  =   I2C Address,
                     puBuffer   =   Pointer to read buffer
                     ulLength   =   NoOfBytes to be read
                     uStop      =   Stop condition at the end of transaction.
 RETURN       :      Status of Read Operation
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_I2C_master_waiting_read(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                      U8 uDevAddrs, U8 *puBuffer, U16 ulLength, U8 uStop)
{
  U16 ulTimeout = I2C_MASTER_TIMEOUT;
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  HDMI_CLEAR_REG_BITS_DWORD(
    (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if (Handle_p->uDevAddress != uDevAddrs)
    sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
  else
    {
      if (Handle_p->uTransactionDir == I2C_MASTER_DIR_WRITE)
        Handle_p->eI2CMasterState = I2C_MASTER_STATE_FREE;
    }
  Handle_p->uTransactionDir = I2C_MASTER_DIR_READ;
  Handle_p->uDevAddress = uDevAddrs;
  Handle_p->uStop = uStop;
  if (Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH)
    {
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_DATA),
        (uDevAddrs | I2C_MASTER_READ));
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        (I2C_MASTER_WR | I2C_MASTER_STA | I2C_MASTER_IACK));

      /* Wait for transmission to complete */
      while (GET_I2C_MASTER_STATUS(
               (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
               I2C_MASTER_TIP))
        {
          if (ulTimeout-- == 0)
            {
              sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
              HDMI_PRINT
              (" Error !!I2C Master Blocking Mode Reading.. TimeOut\n");
              return FALSE;
            }
        }

      /* Check for an ACK from the slave */
      if (GET_I2C_MASTER_STATUS(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
            I2C_MASTER_RXACK))
        {
          sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
          HDMI_PRINT(" Error !!I2C Master Blocking Mode Reading.. NACK\n");
          return FALSE;
        }

    }
  while (ulLength)
    {
      if ((ulLength > 1) || (!uStop))
        {
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
            (I2C_MASTER_RD | I2C_MASTER_IACK));
        }
      else
        {
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
            (I2C_MASTER_RD | I2C_MASTER_ACK | I2C_MASTER_IACK));
        }
      ulTimeout = I2C_MASTER_TIMEOUT;

      /* Wait for transmission to complete */
      while (GET_I2C_MASTER_STATUS (
               (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
               I2C_MASTER_TIP))
        {
          if (ulTimeout-- == 0)
            {
              sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
              HDMI_PRINT
              (" Error !!I2C Master Blocking Mode Reading.. TimeOut\n");
              return FALSE;
            }
        }

      *puBuffer = (U8) HDMI_READ_REG_DWORD(
                    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_RX_DATA));
      puBuffer++;
      ulLength--;
    }

  /* Send stop signal if only requested */
  if (uStop)
    sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
  else
    Handle_p->eI2CMasterState = I2C_MASTER_STATE_STRETCH;

  return TRUE;
}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_write
 USAGE        :      Write to the I2C Master with Blocking Mode of Operation
 INPUT        :      uDevAddrs  =   I2C Address,
                     puBuffer   =   Pointer to read buffer
                     ulLenght   =   NoOfBytes to be read
                     uStop      =   Stop condition at the end of transaction.
 RETURN       :      Status of write Operation
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_I2C_master_write(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                               U8 uDevAddrs, U8 *puBuffer, U16 ulLength, U8 uStop)
{
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  HDMI_CLEAR_REG_BITS_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if (((Handle_p->uTransactionDir == I2C_MASTER_DIR_READ) ||
       (Handle_p->uDevAddress != uDevAddrs)) ||
      ((Handle_p->eI2CMasterState != I2C_MASTER_STATE_FREE) &&
       (Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH)))
    {
      sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
    }

  if (puBuffer == NULL)
    return FALSE;
  Handle_p->uDevAddress = uDevAddrs;
  Handle_p->puBuffer = puBuffer;
  Handle_p->uLength = ulLength;
  Handle_p->uStop = uStop;
  Handle_p->uTransactionDir = I2C_MASTER_DIR_WRITE;

  HDMI_SET_REG_BITS_DWORD(
    (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if (Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH)
    {
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_DATA),
        (uDevAddrs & ~I2C_MASTER_READ));
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        I2C_MASTER_WR | I2C_MASTER_STA | I2C_MASTER_IACK);
      Handle_p->eI2CMasterState = I2C_MASTER_STATE_START;
    }
  else
    {
      Handle_p->eI2CMasterState = I2C_MASTER_STATE_REP_START;
      sthdmirx_I2C_master_ISR_client(I2CMasterHandle);
    }
  return TRUE;
}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_read
 USAGE        :      Read from the I2C Master with Blocking Mode of Operation
 INPUT        :      uDevAddrs  =   I2C Address,
                     puBuffer   =   Pointer to read buffer
                     ulLength   =   NoOfBytes to be read
                     uStop      =   Stop condition at the end of transaction.
 RETURN       :      Status of Read Operation
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_I2C_master_read(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                              U8 uDevAddrs, U8 *puBuffer, U16 ulLength, U8 uStop)
{
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  HDMI_CLEAR_REG_BITS_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if (((Handle_p->eI2CMasterState != I2C_MASTER_STATE_FREE) &&
       (Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH)))
    {
      sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
    }
  if (Handle_p->uDevAddress != uDevAddrs)
    {
      sthdmirx_I2C_master_stop_transaction(I2CMasterHandle);
    }
  else
    {
      if (Handle_p->uTransactionDir == I2C_MASTER_DIR_WRITE)
        {
          Handle_p->eI2CMasterState = I2C_MASTER_STATE_FREE;
        }
    }

  if (puBuffer == NULL)
    {
      return FALSE;
    }

  Handle_p->uDevAddress = uDevAddrs;
  Handle_p->puBuffer = puBuffer;
  Handle_p->uLength = ulLength;
  Handle_p->uStop = uStop;
  Handle_p->uTransactionDir = I2C_MASTER_DIR_READ;

  HDMI_SET_REG_BITS_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL), I2C_MASTER_IRQ_EN);

  if ((Handle_p->eI2CMasterState != I2C_MASTER_STATE_STRETCH))
    {
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_DATA),
        (uDevAddrs | I2C_MASTER_READ));
      HDMI_WRITE_REG_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
        (I2C_MASTER_WR | I2C_MASTER_STA | I2C_MASTER_IACK));
      Handle_p->eI2CMasterState = I2C_MASTER_STATE_START;
    }
  else
    {
      Handle_p->eI2CMasterState = I2C_MASTER_STATE_REP_START;
      sthdmirx_I2C_master_ISR_client(I2CMasterHandle);
    }
  return TRUE;
}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_ISR_client
 USAGE        :      ISR Routine for I2C Master
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_I2C_master_ISR_client(sthdmirx_I2C_master_handle_t I2CMasterHandle)
{
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  HDMI_WRITE_REG_DWORD(
    (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL), I2C_MASTER_IACK);

  switch (Handle_p->eI2CMasterState)
    {
    case I2C_MASTER_STATE_START:
      if (GET_I2C_MASTER_STATUS(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
            I2C_MASTER_RXACK))
        {
          Handle_p->eI2CMasterState = I2C_MASTER_STATE_NACK;
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
            I2C_MASTER_STO | I2C_MASTER_IACK);
          HDMI_CLEAR_REG_BITS_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL),
            I2C_MASTER_IRQ_EN);
          break;
        }
      Handle_p->eI2CMasterState = I2C_MASTER_STATE_REP_START;
      /* No Break statement, logically it is a else statement */
    case I2C_MASTER_STATE_REP_START:
      if (Handle_p->uLength == 0)
        {
          if (Handle_p->uStop)
            {
              HDMI_WRITE_REG_DWORD(
                (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
                I2C_MASTER_STO);
              Handle_p->eI2CMasterState = I2C_MASTER_STATE_FREE;
            }
          else
            {
              Handle_p->eI2CMasterState = I2C_MASTER_STATE_STRETCH;
            }
        }
      else
        {
          sthdmirx_I2C_master_transaction(I2CMasterHandle);
          if (Handle_p->eI2CMasterState == I2C_MASTER_STATE_REP_START)
            Handle_p->eI2CMasterState = I2C_MASTER_STATE_MIDDLE;
        }
      break;
    case I2C_MASTER_STATE_MIDDLE:
      sthdmirx_I2C_master_transaction(I2CMasterHandle);
      break;
    case I2C_MASTER_STATE_STOP:

      HDMI_CLEAR_REG_BITS_DWORD(
        (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL),
        I2C_MASTER_IRQ_EN);

      if (Handle_p->uTransactionDir == I2C_MASTER_DIR_READ)
        {
          *Handle_p->puBuffer = (U8) HDMI_READ_REG_DWORD(
                                  (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_RX_DATA));
        }
      if (Handle_p->uStop)
        Handle_p->eI2CMasterState = I2C_MASTER_STATE_FREE;
      else
        Handle_p->eI2CMasterState = I2C_MASTER_STATE_STRETCH;
      break;
    default:
      break;
    }
}

/******************************************************************************
 FUNCTION     :      sthdmirx_I2C_master_transaction
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_I2C_master_transaction(sthdmirx_I2C_master_handle_t I2CMasterHandle)
{
  sthdmirx_I2C_master_control_t *Handle_p;

  Handle_p = (sthdmirx_I2C_master_control_t *) I2CMasterHandle;

  switch (Handle_p->uTransactionDir)
    {
    case I2C_MASTER_DIR_WRITE:
    {
      U8 uCtrlWord = I2C_MASTER_WR | I2C_MASTER_IACK;
      if (GET_I2C_MASTER_STATUS(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_STATUS),
            I2C_MASTER_RXACK))
        {
          Handle_p->eI2CMasterState = I2C_MASTER_STATE_NACK;
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
            (I2C_MASTER_STO | I2C_MASTER_IACK));
          HDMI_CLEAR_REG_BITS_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_CTRL),
            I2C_MASTER_IRQ_EN);
        }
      else
        {
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_RX_DATA),
            (U8) (*Handle_p->puBuffer));
          if (Handle_p->uLength > 1)
            {
              Handle_p->puBuffer++;
              Handle_p->uLength--;
            }
          else
            {
              if (Handle_p->uStop)
                {
                  uCtrlWord |= I2C_MASTER_STO;
                }

              Handle_p->eI2CMasterState = I2C_MASTER_STATE_STOP;
            }
          HDMI_WRITE_REG_DWORD(
            (U32) (Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL),
            uCtrlWord);
        }
    }
    break;
    case I2C_MASTER_DIR_READ:
    {
      U8 uCtrlWord = I2C_MASTER_RD | I2C_MASTER_IACK;
      if (Handle_p->eI2CMasterState != I2C_MASTER_STATE_REP_START)
        {
          *Handle_p->puBuffer = (U8)HDMI_READ_REG_DWORD(
                                  (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_RX_DATA));
          Handle_p->puBuffer++;
        }
      if (Handle_p->uLength > 1)
        Handle_p->uLength--;
      else
        {
          if (Handle_p->uStop)
            uCtrlWord |= (I2C_MASTER_STO | I2C_MASTER_ACK);
          Handle_p->eI2CMasterState = I2C_MASTER_STATE_STOP;
        }
      HDMI_WRITE_REG_DWORD(
        (U32)(Handle_p->ulCellBaseAddr + I2C_MASTER_TX_CTRL), uCtrlWord);
    }
    break;
    default:
      break;
    }
}
#endif
/* End of file */
