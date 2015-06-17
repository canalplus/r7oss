/***********************************************************************
 *
 * File: hdmirx/soc/stiH407/stiH407_hdmirx_drv.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/****************I N C L U D E    F I L E S*******************************************/
/* Standard Includes ----------------------------------------------*/

#ifndef CONFIG_ARCH_STI
#include <linux/stm/gpio.h>
#include <linux/stm/pad.h>
#else
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#endif
#include <linux/module.h>

/* Local Includes -------------------------------------------------*/
#include <hdmirxplatform.h>
#include <hdmirx_drv.h>
#include "stiH407_hdmirx_reg.h"

/*****************************************************************************
 Name             : hdmirx_initialisation
 Description     : Initiliases the HdmiRx Device
 Parameters     :  Handle

 Assumptions   :
 Limitations      :
 Returns          : 0- No Error
 ****************************************************************************/
uint32_t hdmirx_initialisation(hdmirx_handle_t Handle)
{
  hdmirx_dev_handle_t *dHandle_p;
  U8 Idx;
  stm_error_code_t ErrorCode = 0;

  dHandle_p = (hdmirx_dev_handle_t *) Handle;

  //initialise device Handle as invalid
  for (Idx = 0; Idx < STHDMIRX_MAX_DEVICE; Idx++)
    {
      dHandle_p->Handle = HDMIRX_INVALID_DEVICE_HANDLE;
    }
  //initialise the Port Handle as invalid
  for (Idx = 0; Idx < STHDMIRX_MAX_PORT; Idx++)
    {
      dHandle_p->PortHandle[Idx].Handle = HDMIRX_INVALID_PORT_HANDLE;
    }
  //initialise the RouteHandle as invalid
  for (Idx = 0; Idx < STHDMIRX_MAX_ROUTE; Idx++)
    {
      dHandle_p->RouteHandle[Idx].Handle = HDMIRX_INVALID_ROUTE_HANDLE;
      dHandle_p->RouteHandle[Idx].bIsHWInitialized = FALSE;
    }

  dHandle_p->device_count=0;
  dHandle_p->route_count=0;
  dHandle_p->port_count=0;

  hdmirx_init_CSM(dHandle_p);
  /*Initialise HotPlugDetect */
  hdmirx_init_HPD(dHandle_p);
  /*Initialise the I2C Master & Slave Parameters */
  ErrorCode = hdmirx_init_I2C(dHandle_p);
  if (ErrorCode != 0)
    {
      HDMI_PRINT(" hdmirx_init_I2C: failed, error%d\n", ErrorCode);
      return (ErrorCode);
    }
  /*   Load HDCP Keys    */
#ifdef STHDMIRX_HDCP_KEY_LOADING_THRU_ST40
  if (sthdmirx_load_HDCP_key_data(dHandle_p) == TRUE)
    {
      HDMI_PRINT("HDCP Keys are loaded !!\n");
    }
#endif
  /*OneTime Hardware Initilization for processing routes */
  for (Idx = 0; Idx < dHandle_p->DeviceProp.Max_routes; Idx++)
    {
      dHandle_p->RouteHandle[Idx].PacketBaseAddress =
        (U32) ((U32) dHandle_p->RouteHandle[Idx].BaseAddress +
               HDRX_PACKET_MEMORY_ADDRS_BASE_OFFSET);
      dHandle_p->RouteHandle[Idx].bIsHWInitialized = TRUE;
      sthdmirx_clkgen_init(
        (U32) (dHandle_p->RouteHandle[Idx].MappedClkGenAddress),
        &dHandle_p->RouteHandle[Idx]);
      ErrorCode = HdmiRx_InitInput(
                    &dHandle_p->RouteHandle[Idx], dHandle_p->ulMeasClkFreqHz);
      //install interrupts
      if (install_hdmirx_interrupt(
            dHandle_p, dHandle_p->RouteHandle[Idx].HdmiRx_Irq, "HdmiRX_IRQ") != 0)
        {
          HDMI_PRINT(" ST_ERROR_INTERRUPT_INSTALL\n");
          return -EINVAL;
        }
    }
  /*
   * (Only one alternate function 1 is available in ORLY 2 for those PIO)
   */
  for (Idx = 0; Idx < dHandle_p->DeviceProp.Max_Ports; Idx++)
    {
      hdmirx_configure_pio(&dHandle_p->PortHandle[Idx]);
    }

  return 0;
}

/*******************************************************************************
 Name            : hdmirx_get_platform_data
 Description     : Extracts the platform data
 Parameters     :

 Assumptions   :
 Limitations      :
 Returns          :
 *******************************************************************************/
void hdmirx_get_platform_data(hdmirx_dev_handle_t * const dHandle,
                              stm_hdmirx_platform_data_t *Platform_data_Hdmi)
{
  uint32_t port_id, route_id;
  void *Csm_address;

  dHandle->DeviceProp.Max_Ports = Platform_data_Hdmi->board->num_ports;
  Csm_address =
    ioremap_nocache(Platform_data_Hdmi->soc->csm.start_addr,
                    ((Platform_data_Hdmi->soc->csm.end_addr) -
                     (Platform_data_Hdmi->soc->csm.start_addr)));
  dHandle->CsmCellBaseAddress = (uint32_t) Csm_address;
  dHandle->ulMeasClkFreqHz = Platform_data_Hdmi->soc->meas_clk_freq_hz;
  dHandle->DeviceProp.Max_routes = Platform_data_Hdmi->soc->num_routes;
  for (port_id = 0; port_id < Platform_data_Hdmi->board->num_ports; port_id++)
    {
      dHandle->PortHandle[port_id].portId =
        Platform_data_Hdmi->board->port[port_id].id;
      dHandle->PortHandle[port_id].I2C_Master_Id =
        Platform_data_Hdmi->board->port[port_id].csm_port_id[0];
      dHandle->PortHandle[port_id].I2C_MCCS_Id =
        Platform_data_Hdmi->board->port[port_id].csm_port_id[1];
      dHandle->PortHandle[port_id].HPD_Port_Id =
        Platform_data_Hdmi->board->port[port_id].csm_port_id[2];
      dHandle->PortHandle[port_id].Pd_Pio =
        Platform_data_Hdmi->board->port[port_id].pd_config.pin.pd_pio;
      dHandle->PortHandle[port_id].Hpd_Pio =
        Platform_data_Hdmi->board->port[port_id].hpd_pio;
      dHandle->PortHandle[port_id].EDID_WP =
        Platform_data_Hdmi->board->port[port_id].edid_wp;
      dHandle->PortHandle[port_id].SCL_Pio =
        Platform_data_Hdmi->board->port[port_id].scl_pio;
      dHandle->PortHandle[port_id].SDA_Pio =
        Platform_data_Hdmi->board->port[port_id].sda_pio;
      dHandle->PortHandle[port_id].enable_hpd =
        Platform_data_Hdmi->board->port[port_id].enable_hpd;
      dHandle->PortHandle[port_id].listen_ddc2bi =
        Platform_data_Hdmi->board->port[port_id].enable_ddc2bi;
      dHandle->PortHandle[port_id].internal_edid =
        Platform_data_Hdmi->board->port[port_id].internal_edid;
      dHandle->PortHandle[port_id].Equalization_type =
        Platform_data_Hdmi->board->port[port_id].eq_mode;
      dHandle->PortHandle[port_id].HdmiRxMode =
        Platform_data_Hdmi->board->port[port_id].op_mode;
      dHandle->PortHandle[port_id].Route_Connectivity_Mask =
        Platform_data_Hdmi->board->port[port_id].route_connectivity_mask;
      dHandle->PortHandle[port_id].Eq_Config =
        Platform_data_Hdmi->board->port[port_id].eq_config;
      dHandle->PortHandle[port_id].Ext_Mux =
        Platform_data_Hdmi->board->port[port_id].ext_mux;
      dHandle->PortHandle[port_id].ext_mux =
        Platform_data_Hdmi->board->set_ext_mux;
      dHandle->PortHandle[port_id].stServiceModule.
      ulCsmCellBaseAddrs = (uint32_t) Csm_address;
#ifndef CONFIG_ARCH_STI
   if(Platform_data_Hdmi->board->port[port_id].Pad_Config_p)
      memcpy(&(dHandle->PortHandle[port_id].Pad_Config),Platform_data_Hdmi->board->port[port_id].Pad_Config_p, sizeof(struct stm_pad_config));
#else
      dHandle->PortHandle[port_id].pinctrl_p = Platform_data_Hdmi->board->port[port_id].pinctrl_p;
#endif
    }
  for (route_id = 0; route_id < Platform_data_Hdmi->soc->num_routes; route_id++)
    {
      dHandle->RouteHandle[route_id].RouteID =
        Platform_data_Hdmi->soc->route[route_id].id;
      dHandle->RouteHandle[route_id].BaseAddress = (uint32_t) ioremap_nocache(
            Platform_data_Hdmi->soc->route[route_id].core.start_addr,
            ((Platform_data_Hdmi->soc->route[route_id].core.end_addr) -
             (Platform_data_Hdmi->soc->route[route_id].core.start_addr)));
      dHandle->RouteHandle[route_id].pHYControl.BaseAddress_p =
        (uint32_t) ioremap_nocache(
          Platform_data_Hdmi->soc->route[route_id].phy.start_addr,
          ((Platform_data_Hdmi->soc->route[route_id].phy.end_addr) -
           (Platform_data_Hdmi->soc->route[route_id].phy.start_addr)));
      dHandle->RouteHandle[route_id].MappedClkGenAddress =
        (uint32_t) ioremap_nocache(
          Platform_data_Hdmi->soc->route[route_id].clock_gen.start_addr,
          ((Platform_data_Hdmi->soc->route[route_id].clock_gen.end_addr) -
           (Platform_data_Hdmi->soc->route[route_id].clock_gen.start_addr)));
      dHandle->RouteHandle[route_id].I2SClkFactor =
        Platform_data_Hdmi->soc->route[route_id].i2s_out_clk_scale_factor;
      dHandle->RouteHandle[route_id].OutputPixelWidth =
        Platform_data_Hdmi->soc->route[route_id].output_pixel_width;
      dHandle->RouteHandle[route_id].HdmiRx_Irq =
        Platform_data_Hdmi->soc->route[route_id].irq_num;
      dHandle->RouteHandle[route_id].stDdsConfigInfo.estAudDds =
        Platform_data_Hdmi->soc->route[route_id].clock_gen.audio_clk_gen_id;
      dHandle->RouteHandle[route_id].stDdsConfigInfo.estVidDds =
        Platform_data_Hdmi->soc->route[route_id].clock_gen.video_clk_gen_id;
      dHandle->RouteHandle[route_id].rterm_val =
        Platform_data_Hdmi->soc->route[route_id].phy.rterm_val;
      dHandle->RouteHandle[route_id].rterm_mode =
        Platform_data_Hdmi->soc->route[route_id].phy.rterm_mode;
      dHandle->RouteHandle[route_id].IfmControl.ulBaseAddress =
        dHandle->RouteHandle[route_id].BaseAddress + HDRX_IFM_INSTRUMENT_PU_ADDRS_OFFSET;
      dHandle->RouteHandle[route_id].pHYControl.DeviceBaseAddress =
        dHandle->RouteHandle[route_id].BaseAddress;
    }
}

/*******************************************************************************
Name            : hdmirx_init_I2C
Description     : Start The HdmiRx Task
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_init_I2C(const hdmirx_handle_t Handle)
{
  stm_error_code_t ErrorCode = 0;
  hdmirx_dev_handle_t *pDevHandle;
  uint8_t PortId;
#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
  uint8_t RouteId = 0;
#endif

#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
  U8 i;
  sthdmirx_I2C_slave_init_params_t pSlvInitparams;
#endif

#ifdef STHDMIRX_I2C_MASTER_ENABLE
  sthdmirx_I2C_master_init_params_t pInitparams;
#endif

  pDevHandle = (hdmirx_dev_handle_t *) Handle;

  /* Initialize the I2C Master */
#ifdef STHDMIRX_I2C_MASTER_ENABLE
  pInitparams.BaudRateHz = 100000UL;	/*100 KHz */
  pInitparams.InpClkFreqHz = pDevHandle->ulMeasClkFreqHz;
  pInitparams.RegBaseAddrs =
    pDevHandle->CsmCellBaseAddress + HDRX_I2C_MASTER_MODULE_ADDRS_OFFSET;
  for (PortId = 0; PortId < pDevHandle->DeviceProp.Max_Ports; PortId++)
    {
      sthdmirx_I2C_master_init(
        (sthdmirx_I2C_master_handle_t) &pDevHandle->PortHandle[PortId].stI2CMasterCtrl,
        &pInitparams);
    }
#endif
  /* Initialize the I2C Slave *///TBD
#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
  pSlvInitparams.bIsClkStretchNeeded = TRUE;
  pSlvInitparams.bIsFIFOModeNeeded = TRUE;
  pSlvInitparams.DevAddress = 0xA0;
  for (RouteId = 0; RouteId < pDevHandle->DeviceProp.Max_routes; RouteId++)
    {
      pSlvInitparams.RegBaseAddrs =
        (U32) pDevHandle->RouteHandle.BaseAddress;
      for (PortId = 0; PortId < pDevHandle->DeviceProp.Max_Ports; PortId++)
        {
          pSlvInitparams.PortNo = PortId;	/* May need to be changed */
          sthdmirx_I2C_slave_init(
            &pDevHandle->PortHandle[PortId].I2CSlaveHandle,&pSlvInitparams);
        }
    }
#endif
  return (ErrorCode);
}

/*******************************************************************************
Name            : hdmirx_update_EDID
Description     : Updates the EDID of the specified port
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_update_EDID(hdmirx_port_handle_t *Port_Handle,
                                    uint32_t block_number,
                                    stm_hdmirx_port_edid_block_t *edid_block)
{
  U8 PortId;
  PortId = Port_Handle->I2C_Master_Id;
  if (Port_Handle->internal_edid == FALSE)
    {
      U8 pBuffer[128];
      U16 PageStartAddress = 0x00;
      U8 DevAddrs;
      U8 tAddrs[1];
      U8 i;
      /* Enable the External EEPROM */
      sthdmirx_CSM_external_EDID_enable(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule, PortId);

      /* Disable the Write Protect if any gpio control */
      if (gpio_is_valid(Port_Handle->EDID_WP))
        gpio_set_value(Port_Handle->EDID_WP,1);
      else
        HDMI_PRINT(" EDID GPIO:: Non valid!!\n");

      /* Connect I2C Master to requested port I2C SDA/SCL Lines */
      sthdmirx_CSM_I2C_master_port_select(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule, PortId);

      /*Calculate the Page Start Address */
      PageStartAddress = block_number * EDID_BLOCK_SIZE;
      DevAddrs = (U8) (EXTERNAL_EEPROM_DEVICE_ID |
                       (U8) ((U8) (PageStartAddress >> 8) << 1));
      tAddrs[0] = (U8) (PageStartAddress & 0xff);

      /*Copy the Block data to local buffer */
      stm_hdmirx_memcpy(pBuffer, edid_block,
                        sizeof(stm_hdmirx_port_edid_block_t));
      /*Write the data per page */
      for (i = 0; i < (EDID_BLOCK_SIZE / EEPROM_PAGE_SIZE); i++)
        {
#ifdef STHDMIRX_I2C_MASTER_ENABLE
          if (sthdmirx_I2C_master_waiting_write(
                (sthdmirx_I2C_master_handle_t) & Port_Handle->stI2CMasterCtrl,
                DevAddrs, tAddrs, EEPROM_SIZE_OFFSET, 0) != TRUE)
            {
              HDMI_PRINT(" I2C Master:: Address write Error!!\n");
              return (-EIO);
            }

          if (sthdmirx_I2C_master_waiting_write(
                (sthdmirx_I2C_master_handle_t) & Port_Handle->stI2CMasterCtrl,
                DevAddrs, &pBuffer[i * EEPROM_PAGE_SIZE], EEPROM_PAGE_SIZE, 1) != TRUE)
            {
              HDMI_PRINT(" I2C Master:: Data write Error!!\n");
              return (-EIO);
            }
#endif
          PageStartAddress += EEPROM_PAGE_SIZE;
          DevAddrs = (U8) (EXTERNAL_EEPROM_DEVICE_ID |
                           (U8) ((U8) (PageStartAddress >> 8) << 1));
          tAddrs[0] = (U8) (PageStartAddress & 0xff);

          stm_hdmirx_delay_us(EEPROM_WRITE_DELAY * 1000);
        }
      /* Restore EDID protection */
      if (gpio_is_valid(Port_Handle->EDID_WP))
        gpio_set_value(Port_Handle->EDID_WP,0);
      else
        HDMI_PRINT(" EDID GPIO:: Non valid!!\n");
    }
  else
    {
#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
      BOOL ReadStatus;
      /* Make the call to I2C Slave Read */
      ReadStatus = sthdmirx_I2C_slave_operation(edid_block, 128, PortId);
      if (ReadStatus == FALSE)
        {
          HDMI_PRINT("FAILURE in Write from Slave \n");
          return (-EIO);
        }
      else
        HDMI_PRINT("SUCCESS in Write from Slave \n");
#endif
    }
  return 0;
}

/*******************************************************************************
 Name            : hdmirx_SetPowerState
 Description     :
 Parameters      :
 Assumptions     :
 Limitations     :
 Returns         :
 *****************************************************************************/
uint32_t hdmirx_SetPowerState(struct platform_device * pdev, hdmirx_pwr_modes_t power_modes)
{
  uint32_t ErrorCode = 0;
  int hIdx;
  hdmirx_dev_handle_t *dHandle_p;

  if(! pdev)
    return -EINVAL;

  dHandle_p = &dev_handle[pdev->id];

  if (dHandle_p->Handle == HDMIRX_INVALID_DEVICE_HANDLE)
  {
    return -EINVAL;
  }
  switch (power_modes) {
  case HDMIRX_PM_RESTORE:
    DEBUG_PRINT(">> HDMIRX_PM_RESTORE!\n");
    /*Initialise CSM modules */
    hdmirx_init_CSM(dHandle_p);
    hdmirx_init_HPD(dHandle_p);
    hdmirx_init_I2C(dHandle_p);
    for (hIdx = 0; hIdx < dHandle_p->DeviceProp.Max_Ports; hIdx++)
      {
        if (dHandle_p->PortHandle[hIdx].Handle != HDMIRX_INVALID_PORT_HANDLE)
          {
            dHandle_p->PortHandle[hIdx].IsSourceDetectionStarted = TRUE;
          }
      }
    DEBUG_PRINT("<< HDMIRX_PM_RESTORE!\n");
    /* no break */
  case HDMIRX_PM_RESUME:
    DEBUG_PRINT(">> HDMIRX_PM_RESUME!\n");
    /*start the input */
    stm_hdmirx_sema_wait(dHandle_p->hdmirx_pm_sema);
    for (hIdx = 0; hIdx < dHandle_p->DeviceProp.Max_routes; hIdx++)
      {
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
        sthdmirx_clkgen_init((U32) (dHandle_p->RouteHandle[hIdx].MappedClkGenAddress), &dHandle_p->RouteHandle[hIdx]);
#endif
        HdmiRx_InitInput(&dHandle_p->RouteHandle[hIdx], dHandle_p->ulMeasClkFreqHz);
        if (dHandle_p->RouteHandle[hIdx].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
        {
          hdmirx_resume_inputprocess(&dHandle_p->RouteHandle[hIdx]);
        }
      }
    dHandle_p->is_standby=FALSE;
    stm_hdmirx_sema_signal(dHandle_p->hdmirx_pm_sema);
    DEBUG_PRINT("<< HDMIRX_PM_RESUME!\n");
    break;
  case HDMIRX_PM_FREEZE:
    DEBUG_PRINT(">> HDMIRX_PM_FREEZE!\n");
    for (hIdx = 0; hIdx < dHandle_p->DeviceProp.Max_Ports; hIdx++)
      {
        if (dHandle_p->PortHandle[hIdx].Handle != HDMIRX_INVALID_PORT_HANDLE)
          {
            dHandle_p->PortHandle[hIdx].IsSourceDetectionStarted = FALSE;
          }
      }
    DEBUG_PRINT("<< HDMIRX_PM_FREEZE!\n");
      /* no break */
  case HDMIRX_PM_SUSPEND:
    DEBUG_PRINT(">> HDMIRX_PM_SUSPEND!\n");
    /*stop the input */
    stm_hdmirx_sema_wait(dHandle_p->hdmirx_pm_sema);
    for (hIdx = 0; hIdx < dHandle_p->DeviceProp.Max_routes; hIdx++)
      {
        if (dHandle_p->RouteHandle[hIdx].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
        {
          hdmirx_stop_inputprocess(&dHandle_p->RouteHandle[hIdx]);
        }
      }
    dHandle_p->is_standby=TRUE;
    stm_hdmirx_sema_signal(dHandle_p->hdmirx_pm_sema);
    DEBUG_PRINT("<< HDMIRX_PM_SUSPEND!\n");
    break;
  default:
    /* power mode not supported*/
    ErrorCode=-EINVAL;
    break;
  }
  return ErrorCode;
}

/*******************************************************************************
Name            : hdmirx_configure_pio
Description     : opens pio
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_configure_pio(hdmirx_port_handle_t *Port_handle)
{

  if (gpio_is_valid(Port_handle->EDID_WP)) {
    if (gpio_request(Port_handle->EDID_WP, "HDMIRx")) {
      return -EINVAL;
    }
    gpio_direction_output(Port_handle->EDID_WP, 0);
  }
#ifndef CONFIG_ARCH_STI
  Port_handle->Pad_State = stm_pad_claim(&Port_handle->Pad_Config, "HDMIRx");
  if (Port_handle->Pad_State == NULL)
     HDMI_PRINT("Failed to Claim HdmiRx Pad\n");
#else /* ifdef CONFIG_ARCH_STI */
  Port_handle->pinctrl_state = pinctrl_lookup_state(Port_handle->pinctrl_p, PINCTRL_STATE_DEFAULT);
  if (IS_ERR(Port_handle->pinctrl_state))
    HDMI_PRINT("could not get default pinstate\n");
  else
    pinctrl_select_state(Port_handle->pinctrl_p, Port_handle->pinctrl_state);
#endif /* CONFIG_ARCH_STI */
  return 0;
}

BOOL hdmirx_configure_vtac(void)
{
  return FALSE;
}

BOOL hdmirx_reset_vtac(void)
{
  return FALSE;
}
