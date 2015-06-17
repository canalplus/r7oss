/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_i2s.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifdef STHDMIRX_I2S_TX_IP

/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/

#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <hdmirx_RegOffsets.h>
#include <InternalTypes.h>

/* Private Typedef -----------------------------------------------*/

/* Private Defines ------------------------------------------------*/

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION     :
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_I2S_init(const hdmirx_handle_t Handle,
                       sthdmirx_I2S_init_params_t *pI2SInit)
{

  U32 W_HdrxI2sTxControl,W_HdrxI2sTxClockCntrl = 0, W_HdrxI2sTxDataLsbPos = 0;

  hdmirx_route_handle_t *InpHandle_p;
  InpHandle_p = (hdmirx_route_handle_t *) Handle;

  W_HdrxI2sTxControl = HDRX_I2S_TX_OUT_CH3_EN | HDRX_I2S_TX_OUT_CH2_EN |
                       HDRX_I2S_TX_OUT_CH1_EN | HDRX_I2S_TX_OUT_CH0_EN |
                       HDRX_I2S_TX_OUT_EN | HDRX_I2S_TX_EN;

  if (pI2SInit->InputFormat == I2S_INPUT_FORMAT_IEC60958)
    {
      W_HdrxI2sTxControl |= HDRX_I2S_TX_LPCM_MD_DIS;
    }

  HDMI_WRITE_REG_WORD((U32)(InpHandle_p->BaseAddress + HDRX_I2S_TX_CONTROL),
                      W_HdrxI2sTxControl);

  switch (pI2SInit->MasterClk / (pI2SInit->OutputDataWidth * 2))
    {
    case 2:
      W_HdrxI2sTxClockCntrl = 0;
      break;
    case 4:
      W_HdrxI2sTxClockCntrl = 1;
      break;
    case 8:
      W_HdrxI2sTxClockCntrl = 2;
      break;
    case 16:
      W_HdrxI2sTxClockCntrl = 3;
      break;
    }
  if (pI2SInit->WordSelectPolarity == I2S_FIRST_LEFT_CHANNEL)
    {
      W_HdrxI2sTxClockCntrl |= HDRX_I2S_TX_WRD_CLK_POL;
    }

  if (pI2SInit->DataAlignment != I2S_DATA_I2S_MODE)
    {
      W_HdrxI2sTxClockCntrl |= HDRX_I2S_TX_WRD_CLK_MODE;
    }

  HDMI_WRITE_REG_WORD((U32)(InpHandle_p->BaseAddress + HDRX_I2S_TX_CLK_CNTRL),
                      W_HdrxI2sTxClockCntrl);

  HDMI_WRITE_REG_WORD((U32)(InpHandle_p->BaseAddress + HDRX_I2S_TX_BIT_WIDTH),
                      pI2SInit->OutputDataWidth - 1);

  if (pI2SInit->DataAlignment == I2S_DATA_RIGHT_ALIGNED_MODE)
    {
      if (pI2SInit->OutputDataWidth > pI2SInit->InputDataWidthForRightAlgn)
        {
          W_HdrxI2sTxDataLsbPos = pI2SInit->OutputDataWidth -
                                  pI2SInit->InputDataWidthForRightAlgn;
        }
    }
  else
    {
      W_HdrxI2sTxDataLsbPos = 0;
    }
  HDMI_WRITE_REG_WORD((U32)(InpHandle_p->BaseAddress +
                            HDRX_I2S_TX_DATA_LSB_POS), W_HdrxI2sTxDataLsbPos);

  HDMI_WRITE_REG_WORD((U32)(InpHandle_p->BaseAddress + HDRX_I2S_TX_SYNC_SYS),
                      0x0A);

  //HDMI_PRINT("API call: sthdmirx_I2S_init\n");

}

/****************************************** End of file***********************************************************/
#endif /*STHDMIRX_I2S_TX_IP */
