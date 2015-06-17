/***********************************************************************
 *
 * File: hdmirx/src/system/hdmirx_services.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/
#include "hdmirx_drv.h"
#include "hdmirx_csm.h"
#include "InternalTypes.h"
#include "hdmirx_system.h"
#include "hdmirx_RegOffsets.h"
#include "hdmirx_hpd.h"

/* Private Typedef ------------------------------------------------*/

/* Private Defines -------------------------------------------------*/

/* Private macro's -------------------------------------------------*/
#define     STHDMIRX_CBL_DET_TIMEOUT_MSEC       (M_NUM_TICKS_PER_MSEC(100))
#define HDCP_INTERNAL_KEY_USED

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes --------------------------------------*/

/* Interface procedures/functions ------------------------------------*/
/******************************************************************************
 FUNCTION     : sthdmirx_source_plug_detect_task()
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS 	  :
******************************************************************************/
stm_error_code_t sthdmirx_CSM_init(const hdmirx_handle_t Handle, sthdmirx_CSM_config_t csm_config)
{
  hdmirx_dev_handle_t *pDevHandle;
  uint32_t value = 0;
  pDevHandle = (hdmirx_dev_handle_t *) Handle;

  value|=(csm_config.hpd_sel<<HDRX_CSM_HPD_SEL_SHIFT)&HDRX_CSM_HPD_SEL_MASK;
  (csm_config.hpd_sel_ctr)?(value|=HDRX_CSM_HPD_REG_SEL):(value&=~HDRX_CSM_HPD_REG_SEL);
  (csm_config.hpd_direct_ctr)?(value|=HDRX_CSM_HPD_AU_DIS):(value&=~HDRX_CSM_HPD_AU_DIS);
  value|=(csm_config.i2c_mccs_sel<<HDRX_CSM_I2C_MCCS_SEL_SHIFT)&HDRX_CSM_I2C_MCCS_SEL_MASK;
  (csm_config.i2c_mccs_sel_ctr)?(value|=HDRX_CSM_I2C_MCCS_REG_SEL):(value&=~HDRX_CSM_I2C_MCCS_REG_SEL);
  value|=(csm_config.i2c_master_sel<<HDRX_CSM_I2C_MASTER_SEL_SHIFT)&HDRX_CSM_I2C_MASTER_SEL_MASK;
  (csm_config.i2c_master_sel_ctr)?(value|=HDRX_CSM_I2C_MASTER_REG_SEL):(value&=~HDRX_CSM_I2C_MASTER_REG_SEL);

  HDMI_WRITE_REG_WORD(pDevHandle->CsmCellBaseAddress + HDRX_CSM_CTRL, value);

  return 0;
}

/******************************************************************************
 FUNCTION     : sthdmirx_source_plug_detect_task()
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS 	  :
******************************************************************************/
BOOL sthdmirx_source_plug_detect_task(const hdmirx_handle_t Handle)
{
  hdmirx_dev_handle_t *dHandle_p;
  sthdmirx_CSM_context_t *pSMHandle;
  uint32_t Curnt_port = 0;

  dHandle_p = (hdmirx_dev_handle_t *) Handle;
  pSMHandle =&(dHandle_p->PortHandle[Curnt_port].stServiceModule);

  for (Curnt_port=0; Curnt_port < pSMHandle->uNoOfPortToBeMonitored; Curnt_port++)
    {
      pSMHandle =&(dHandle_p->PortHandle[Curnt_port].stServiceModule);
      if (dHandle_p->PortHandle[Curnt_port].Handle != HDMIRX_INVALID_PORT_HANDLE)
        {
          if (!dHandle_p->PortHandle[Curnt_port].IsSourceDetectionStarted)
            {
              if (pSMHandle->State != STHDMIRX_CABLE_DETECT_STOP)
                {
                  sthdmirx_CSM_reset_module_handle(pSMHandle);
                  pSMHandle->State = STHDMIRX_CABLE_DETECT_STOP;
                  break;
                }
            }
          else
            {
              if (STHDMIRX_CABLE_DETECT_STOP == pSMHandle->State)
                {
                  pSMHandle->State = STHDMIRX_CABLE_DETECT_START;
                }
            }

          switch (pSMHandle->State)
            {
            case STHDMIRX_CABLE_DETECT_START:
              pSMHandle->State = STHDMIRX_CABLE_DISCONNECTED;
              /*fall through */
            case STHDMIRX_CABLE_DISCONNECTED:
              if (!sthdmirx_CSM_getHPD_status(pSMHandle, dHandle_p->PortHandle[Curnt_port].HPD_Port_Id))
                {
                  if (pSMHandle->CablePDStatus ==
                      STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
                    {
                      HDMI_PRINT
                      ("HDMI cable is disconnected !! Port Num=%d\n",
                       Curnt_port + 1);
                      pSMHandle->CablePDStatus =
                        STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT;
                      hdmirx_notify_port_evts(
                        (hdmirx_handle_t *) dHandle_p->PortHandle[Curnt_port].Handle,
                        STM_HDMIRX_PORT_SOURCE_PLUG_EVT);

                    }
                  break;
                }

              pSMHandle->ulDetectTime = stm_hdmirx_time_now();
              /*fall through */
            case STHDMIRX_CABLE_DETECT_WAIT:
              if ((stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                         pSMHandle->ulDetectTime)) >= STHDMIRX_CBL_DET_TIMEOUT_MSEC)
                {
                  pSMHandle->State = STHDMIRX_CABLE_CONNECTED;
                  /*fall through */
                }
              else
                {
                  if (sthdmirx_CSM_getHPD_status(pSMHandle, dHandle_p->PortHandle[Curnt_port].HPD_Port_Id))
                    {
                      pSMHandle->State = STHDMIRX_CABLE_DETECT_WAIT;
                    }
                  else
                    {
                      pSMHandle->State = STHDMIRX_CABLE_DISCONNECTED;
                    }
                  break;
                }
            case STHDMIRX_CABLE_CONNECTED:
              if (sthdmirx_CSM_getHPD_status(pSMHandle, dHandle_p->PortHandle[Curnt_port].HPD_Port_Id))
                {
                  if (pSMHandle->CablePDStatus == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT)
                    {
                      HDMI_PRINT
                      ("HDMI cable is connected!! Port Num=%d\n", Curnt_port + 1);
                      pSMHandle->CablePDStatus = STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN;
                      hdmirx_notify_port_evts(
                        (hdmirx_handle_t *) dHandle_p->PortHandle[Curnt_port].Handle,
                        STM_HDMIRX_PORT_SOURCE_PLUG_EVT);
                    }
                  pSMHandle->State = STHDMIRX_CABLE_CONNECTED;
                }
              else
                {
                  pSMHandle->State = STHDMIRX_CABLE_DISCONNECTED;
                  continue; /* Immediate Cable Disconnect Msg */
                }
              break;

            case STHDMIRX_CABLE_DETECT_STOP:
              break;
            }
        }
    }
  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_load_HDCP_key_data
 USAGE        :     Load the hdcp key data to secure key ram area.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
#ifdef STHDMIRX_HDCP_KEY_LOADING_THRU_ST40
#ifdef HDCP_EXTERNAL_KEY_USED
BOOL sthdmirx_load_HDCP_key_data(const hdmirx_handle_t Handle)
{
  ST_ErrorCode_t ErrorCode = ST_NO_ERROR;
  hdmirx_dev_handle_t *Handle_p;
  STPIO_OpenParams_t OpenParams_p;
  STPIO_Handle_t SCLPioHandle;
  STPIO_Handle_t SDAPioHandle;
  STPIO_Handle_t HpdPioHandle;
  STPIO_Handle_t PDPioHandle;
  U8 pStored[128 * 3];
  U16 PageStartAddress = 0x00;
  U8 DevAddrs;
  U8 BlockNum;
  U8 Addrs;
  U16 i;
  U32 pKeyRegaddrs;
  U32 ulTime;
  U32 ulWriteData;
  BOOL bIsKeyLoaded = FALSE;

  HDMI_WRITE_REG_BYTE(0xfd940000UL, 0x01);
  ulTime = stm_hdmirx_time_now();
  while (!(HDMI_READ_REG_BYTE((0xfd940000UL + 4)) & 0x01))
    {
      if (stm_hdmirx_time_minus(stm_hdmirx_time_now(), ulTime) >
          (U32) ((U32) (ST_GetClocksPerSecond() * 100) / 1000))
        {
          HDMI_PRINT("Key loading is not supported by SOC\n");
          return FALSE;
        }
    }

  Handle_p = (hdmirx_dev_handle_t *) Handle;
  OpenParams_p.BitConfigure[0] = STPIO_BIT_ALTERNATE_BIDIRECTIONAL;
  OpenParams_p.IntHandler = NULL;
  OpenParams_p.ReservedBits = 1;
  ErrorCode |= STPIO_Open("PIO14", &OpenParams_p, &HpdPioHandle);

  OpenParams_p.BitConfigure[2] = STPIO_BIT_ALTERNATE_BIDIRECTIONAL;
  OpenParams_p.IntHandler = NULL;
  OpenParams_p.ReservedBits = 4;
  ErrorCode |= STPIO_Open("PIO14", &OpenParams_p, &PDPioHandle);

  OpenParams_p.BitConfigure[1] = STPIO_BIT_ALTERNATE_BIDIRECTIONAL;
  OpenParams_p.IntHandler = NULL;
  OpenParams_p.ReservedBits = 2;
  ErrorCode |= STPIO_Open("PIO14", &OpenParams_p, &SCLPioHandle);

  OpenParams_p.BitConfigure[3] = STPIO_BIT_ALTERNATE_BIDIRECTIONAL;
  OpenParams_p.IntHandler = NULL;
  OpenParams_p.ReservedBits = 8;
  ErrorCode |= STPIO_Open("PIO14", &OpenParams_p, &SDAPioHandle);

  if (ErrorCode != ST_NO_ERROR)
    HDMI_PRINT(" PIO is not open hdcp\n");

  /*Enable the external eeprom */
  sthdmirx_CSM_external_EDID_enable(
    (sthdmirx_CSM_context_t *) &Handle_p->stServiceModule,
    STHDMIRX_INPUT_PORT_3);
  /* Connect I2C Master to requested port I2C SDA/SCL Lines */
  sthdmirx_CSM_I2C_master_port_select(
    (sthdmirx_CSM_context_t *) &Handle_p->stServiceModule,
    STHDMIRX_INPUT_PORT_3);

  /* Read the encrypted HDCP EPROM header */

  Addrs = 0x0;
  DevAddrs = 0xA0;

  /* Write the Page Start Address, without stop bit */
  if (sthdmirx_I2C_master_waiting_write(
        (sthdmirx_I2C_master_handle_t) & Handle_p->stI2CMasterCtrl,
        DevAddrs,&Addrs, 1, 0) != TRUE)
    {
      HDMI_PRINT(" I2C Master ReadOp:: Address write Error!!\n");
      return (FALSE);
    }

  /*Read the complete Block */
  if (sthdmirx_I2C_master_waiting_read
      ((sthdmirx_I2C_master_handle_t) & Handle_p->stI2CMasterCtrl, DevAddrs,
       pStored, 16, 1) != TRUE)
    {
      HDMI_PRINT(" I2C Master ReadOp:: Read data Error!!\n");
      return (FALSE);
    }

  /* Check the EPROM Type : EDID or HDCP */
  if (pStored[1] == 0xff && pStored[2] == 0xff && pStored[3] == 0xff
      && pStored[4] == 0xff)
    {
      HDMI_PRINT(" EDID EPROM is mounted\n");
    }
  else
    {
      for (BlockNum = 1; BlockNum < 4; BlockNum++)
        {
          /*Calculate the Page Start Address */
          if (BlockNum)
            {
              PageStartAddress = (BlockNum - 1) * 128;
              DevAddrs =
                (U8) (0xA0 | (U8) ((U8) (PageStartAddress >> 8) << 1));
              Addrs = (U8) (PageStartAddress & 0xff);
            }
          /* Write the Page Start Address, without stop bit */
          if (sthdmirx_I2C_master_waiting_write(
                (sthdmirx_I2C_master_handle_t) & Handle_p->stI2CMasterCtrl,
                DevAddrs, &Addrs, 1, 0) != TRUE)
            {
              HDMI_PRINT
              (" I2C Master ReadOp:: Address write Error!!\n");
              return (FALSE);
            }

          /*Read the complete Block */
          if (sthdmirx_I2C_master_waiting_read
              ((sthdmirx_I2C_master_handle_t) & Handle_p->stI2CMasterCtrl, DevAddrs,
               &pStored[128 * (BlockNum - 1)], 128, 1) != TRUE)
            {
              HDMI_PRINT
              (" I2C Master ReadOp:: Read data Error!!\n");
              return (FALSE);
            }
        }

#if 0
      for (i = 0; i < 384; i = i + 8)
        {
          STTBX_Print((" 0x%03x ::  0x%02x   0x%02x   0x%02x   0x%02x   0x%02x  0x%02x   0x%02x   0x%02x\n",
                       i, pStored[i + 0], pStored[1 + i], pStored[2 + i], pStored[3 + i], pStored[4 + i], pStored[5 + i], pStored[6 + i], pStored[7 + i]));
        }
#endif

      HDMI_WRITE_REG_BYTE(0xfd940000UL, 0x01);
      ulTime = stm_hdmirx_time_now();
      while (!(HDMI_READ_REG_BYTE((0xfd940000UL + 4)) & 0x01))
        {
          if (stm_hdmirx_time_minus(stm_hdmirx_time_now(), ulTime) >
              (U32) ((U32) (ST_GetClocksPerSecond() * 100) /1000))
            {
              HDMI_PRINT("Key loading is not supported by SOC\n");
              return FALSE;
            }
        }

      ulWriteData =
        (U32) (((U32) (pStored[3 + 7]) << 24) | ((U32) (pStored[2 + 7]) << 16) |
               ((U32) (pStored[1 + 7]) << 8) | ((U32) (pStored[0 + 7]) << 0));
      pKeyRegaddrs = 0xfd940100UL;

      HDMI_WRITE_REG_DWORD(pKeyRegaddrs, ulWriteData);
      pKeyRegaddrs += 4;
      HDMI_WRITE_REG_BYTE(pKeyRegaddrs, pStored[4 + 7]);
      pKeyRegaddrs++;
      for (i = 0; i < 287; i++)
        {
          HDMI_WRITE_REG_BYTE((U32) (pKeyRegaddrs + i), pStored[i + 12]);
        }

      HDMI_WRITE_REG_BYTE(0xfd940000UL, 0x02);
      bIsKeyLoaded = TRUE;
    }

  STPIO_Close(SCLPioHandle);
  STPIO_Close(SDAPioHandle);
  STPIO_Close(HpdPioHandle);
  STPIO_Close(PDPioHandle);

  return bIsKeyLoaded;
}
#endif

#ifdef HDCP_INTERNAL_KEY_USED

#define SAFMEM_HDCP_KSV_START_ADDRS         0xfeb001c0
#define SAFEMEM_HDCP_KEY_START_ADDRS        0xfeb001d0
#define HDCP_KEY_RAM_AREA_START_ADDRS       0xfeb04100

#define HDCP_BKSV_LENGTH        5
#define NUM_HDCP_KEYS           41
#define HDCP_KEY_LENGTH         7
BOOL sthdmirx_load_HDCP_key_data(const hdmirx_handle_t Handle)
{

  hdmirx_dev_handle_t *Handle_p;
  U16 i;
  U32 ulTime;
  U32 ulWriteData;
  U8 uWrData;
  U32 ulKeyRamAreaAddrs;
  U32 ulSafMemAreaAddrs;
  U32 ulKeyHostAddrs;
  BOOL bIsKeyLoaded = FALSE;
  U32 SafememAddrs;

  Handle_p = (hdmirx_dev_handle_t *) Handle;

  ulKeyRamAreaAddrs =
    (uint32_t) ioremap_nocache(HDCP_KEY_RAM_AREA_START_ADDRS, 0x2ff);
  ulKeyHostAddrs =
    Handle_p->RouteHandle[0].BaseAddress + HDRX_KEYHOST_CONTROL;
  SafememAddrs =
    (uint32_t) ioremap_nocache(SAFMEM_HDCP_KSV_START_ADDRS, 0x3ff);

  /* check the soc versions */
  HDMI_WRITE_REG_BYTE(ulKeyHostAddrs, 0x01);
  ulTime = stm_hdmirx_time_now();
  while (!(HDMI_READ_REG_BYTE((ulKeyHostAddrs + 4)) & 0x01))
    {
      if (stm_hdmirx_time_minus(stm_hdmirx_time_now(), ulTime) >
          (U32) ((U32) (STMGetClocksPerSecond() * 100) / 1000))
        {
          HDMI_PRINT("Key loading is not supported by SOC!!\n");
          return FALSE;
        }
    }

  /*load ksv */
  ulSafMemAreaAddrs = SafememAddrs;
  ulWriteData = HDMI_READ_REG_DWORD(ulSafMemAreaAddrs);
  HDMI_WRITE_REG_DWORD(ulKeyRamAreaAddrs, ulWriteData);
  ulSafMemAreaAddrs += 4;
  ulKeyRamAreaAddrs += 4;
  ulWriteData = HDMI_READ_REG_DWORD(ulSafMemAreaAddrs);
  HDMI_WRITE_REG_BYTE(ulKeyRamAreaAddrs, (U8) ulWriteData);

  /*load keys */

  ulKeyRamAreaAddrs += 1;
  HDMI_READ_REG_BYTE(SafememAddrs + 0x10);	//Sonia HDCP Safemem BYte Alligned
  ulSafMemAreaAddrs = SafememAddrs + 0x10 + 1;
  for (i = 0; i < (NUM_HDCP_KEYS * HDCP_KEY_LENGTH); i++)
    {
      uWrData = HDMI_READ_REG_BYTE(ulSafMemAreaAddrs);
      HDMI_WRITE_REG_BYTE(ulKeyRamAreaAddrs, uWrData);
      ulSafMemAreaAddrs++;
      ulKeyRamAreaAddrs++;
    }

  HDMI_WRITE_REG_BYTE(ulKeyHostAddrs, 0x02);
  bIsKeyLoaded = TRUE;

  return bIsKeyLoaded;
}
#endif
#endif
/* End of file */
