/***********************************************************************
 *
 * File: hdmirx/src/hdmirx_drv.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/****************I N C L U D E    F I L E S*******************************************/
/* Standard Includes ----------------------------------------------*/

#include <linux/freezer.h>
#include <linux/module.h>

/* Local Includes -------------------------------------------------*/
#include <hdmirx_system.h>
#include <hdmirx_core_export.h>
#include <hdmirxplatform.h>
#include <hdmirx_drv.h>
#include <thread_vib_settings.h>


/****************L O C A L    D E F I N I T I O N S***************************/
#define     STHDMIRX_MAIN_TASK_SCHEDULE_TIME            40
/******************************************************************************
  G L O B A L   V A R I A B L E S
******************************************************************************/
static int thread_vib_hdmirxmonit[2] = { THREAD_VIB_HDMIRXMONIT_POLICY, THREAD_VIB_HDMIRXMONIT_PRIORITY };
module_param_array_named(thread_VIB_HdmiRxMonit,thread_vib_hdmirxmonit, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_HdmiRxMonit, "VIB-HdmiRxMonit thread:s(Mode),p(Priority)");
/******************************************************************************
  FUNCTIONS PROTOTYPE
******************************************************************************/
void sthdmirx_signal_timing_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                      stm_hdmirx_signal_timing_t *pSigTimingData);
void sthdmirx_video_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_video_property_t *pVidPropData);
void sthdmirx_audio_prop_changed_data_fill(const hdmirx_route_handle_t *pInpHandle,
    stm_hdmirx_audio_property_t *pAudPropData);
void sthdmirx_GBD_info_frame_data_fill(const hdmirx_route_handle_t *pInpHandle,
                                       stm_hdmirx_gbd_info_t *GbdInfo);
void hdmirx_get_platform_data(hdmirx_dev_handle_t * const dHandle,
                              stm_hdmirx_platform_data_t *Platform_data_Hdmi);
/*******************************************************************************
 Name            : HDMIRX_Init
 Description     :
 Parameters     :

 Assumptions   :
 Limitations      :
 Returns          :
 *****************************************************************************/
uint32_t hdmirx_init(struct platform_device * pdev )
{
  uint32_t Error;
  stm_hdmirx_platform_data_t *Platform_data_Hdmi;
  hdmirx_dev_handle_t *dHandle_p;

  if(! pdev)
    return -EINVAL;

  Platform_data_Hdmi = pdev->dev.platform_data;
  dHandle_p = &dev_handle[pdev->id];

  if(! Platform_data_Hdmi)
    return -EINVAL;

  dHandle_p->DeviceProp.DeviceId = pdev->id;
  dHandle_p->DeviceProp.dev = &pdev->dev;

  hdmirx_get_platform_data(dHandle_p, Platform_data_Hdmi);

  stm_hdmirx_sema_init(&dHandle_p->hdmirx_pm_sema,1);
  Error = hdmirx_initialisation(dHandle_p);
  return Error;
}

/*******************************************************************************
Name            : hdmirx_term
Description     : Check the Device Registry
Parameters      : DeviceId - Device identifier 0..n.
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_term(struct platform_device *pdev)
{
  hdmirx_dev_handle_t *dHandle_p;
  uint32_t i, Idx;
  dHandle_p = &dev_handle[pdev->id];

  for (Idx = 0; Idx < dHandle_p->DeviceProp.Max_routes; Idx++)
    {
      uninstall_hdmirx_interrupt(dHandle_p,
                                 dHandle_p->RouteHandle[Idx].HdmiRx_Irq);
    }
  /*
   * (Only one alternate function 1 is available in ORLY 2 for those PIO)
   */
  for (Idx = 0; Idx < dHandle_p->DeviceProp.Max_Ports; Idx++) {
    hdmirx_release_pio(&dHandle_p->PortHandle[Idx]);
  }
  iounmap((void *)dHandle_p->CsmCellBaseAddress);
  for (i = 0; i < dHandle_p->DeviceProp.Max_routes; i++)
    {
      iounmap((void *)dHandle_p->RouteHandle[i].BaseAddress);
      iounmap((void *)dHandle_p->RouteHandle[i].MappedClkGenAddress);
      iounmap((void *)dHandle_p->RouteHandle[i].pHYControl.BaseAddress_p);
    }
  stm_hdmirx_sema_delete(dHandle_p->hdmirx_pm_sema);
  /* terminate clkgen */
  sthdmirx_clkgen_term();
}

/*******************************************************************************
Name            : hdmirx_init_HPD
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_init_HPD(const hdmirx_handle_t Handle)
{
  hdmirx_dev_handle_t *pDevHandle;
  uint8_t PortId;
  pDevHandle = (hdmirx_dev_handle_t *) Handle;

  for (PortId = 0; PortId < pDevHandle->DeviceProp.Max_Ports; PortId++)
    {
      sthdmirx_HPD_init(&(pDevHandle->PortHandle[PortId].stServiceModule));
      pDevHandle->PortHandle[PortId].stServiceModule.uNoOfPortToBeMonitored =
        pDevHandle->DeviceProp.Max_Ports;
    }

}

/*******************************************************************************
Name            : hdmirx_init_CSM
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_init_CSM(const hdmirx_handle_t Handle)
{
  hdmirx_dev_handle_t *pDevHandle;
  sthdmirx_CSM_config_t csm_config;

  pDevHandle = (hdmirx_dev_handle_t *) Handle;

  if(pDevHandle->DeviceProp.Max_Ports < 1) return;

  /* Set initial config for port0 */
  csm_config.hpd_sel=(hdmirx_hpd_sel_t) pDevHandle->PortHandle[0].HPD_Port_Id;
  csm_config.hpd_sel_ctr=FALSE;
  csm_config.hpd_direct_ctr=FALSE;
  csm_config.i2c_mccs_sel=(hdmirx_i2c_sel_t) pDevHandle->PortHandle[0].I2C_MCCS_Id;
  csm_config.i2c_mccs_sel_ctr=TRUE;
  csm_config.i2c_master_sel=(hdmirx_i2c_sel_t) pDevHandle->PortHandle[0].I2C_Master_Id;
  csm_config.i2c_master_sel_ctr=TRUE;

  sthdmirx_CSM_init(pDevHandle, csm_config);
}

/*******************************************************************************
Name            : hdmirx_init_IFM
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_init_IFM(const hdmirx_handle_t Handle, U32 ulMeasFreqHz)
{
  stm_error_code_t ErrorCode = 0;
  hdmirx_route_handle_t *pHandle;

  pHandle = (hdmirx_route_handle_t *) Handle;

  pHandle->IfmControl.ulMeasTCLK_Hz = ulMeasFreqHz;
  pHandle->IfmControl.bIsAWDCaptureEnable = FALSE;

  sthdmirx_IFM_init(&pHandle->IfmControl);

  return (ErrorCode);
}

/*******************************************************************************
Name            : HdmiRx_InitInput
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t HdmiRx_InitInput(hdmirx_route_handle_t *const RouteHandle,
                                  U32 ulMeasRefClkFreqHz)
{
  /* Don't change the programming sequence */
  sthdmirx_CORE_initialize_clks(RouteHandle);
  stm_hdmirx_delay_us(200);	/*Just for test, we can remove later */
  /* Reset the complete core */
  sthdmirx_CORE_enable_reset(RouteHandle, TRUE);
  stm_hdmirx_delay_us(500);
  RouteHandle->pHYControl.RTermValue = RouteHandle->rterm_val;
  RouteHandle->pHYControl.RTermCalibrationMode = RouteHandle->rterm_mode;
  RouteHandle->pHYControl.PhyCellType = HDMIRX_COMBO_ANALOG_PHY;
  RouteHandle->pHYControl.DefaultEqSetting.DviEq = 0x07;
  RouteHandle->pHYControl.DefaultEqSetting.EqGain = 0x0;
  RouteHandle->bIsSignalDetectionStarted = FALSE;
  /* Initilize bIsNoSignalNotified*/
  RouteHandle->bIsNoSignalNotified = FALSE;
  RouteHandle->pHYControl.AdaptiveEqHandle.EqState = EQ_STATE_IDLE;
  sthdmirx_PHY_init(&RouteHandle->pHYControl);
  /*Instrumentation block initialization */
  sthdmirx_MEAS_HW_init(RouteHandle, ulMeasRefClkFreqHz);
  /*Initialse the Input Format Measurement */
  hdmirx_init_IFM(RouteHandle, ulMeasRefClkFreqHz);
  /*Video Block initialization */
  sthdmirx_video_pipe_init(RouteHandle);
  /*Audio Block Initialization */
  sthdmirx_audio_initialize(RouteHandle);
#ifdef STHDMIRX_I2S_TX_IP
  /*I2S Initialization */
  {
    sthdmirx_I2S_init_params_t InitParams;
    InitParams.MasterClk = I2S_CLOCK_128_FS;
    InitParams.DataAlignment = I2S_DATA_I2S_MODE;
    InitParams.WordSelectPolarity = I2S_FIRST_LEFT_CHANNEL;
    InitParams.InputFormat = I2S_INPUT_FORMAT_LPCM;
    InitParams.OutputDataWidth = I2S_DATA_WIDTH_32_BITS;
    InitParams.InputDataWidthForRightAlgn = I2S_DATA_WIDTH_32_BITS;
    sthdmirx_I2S_init(RouteHandle, &InitParams);
  }
#endif
  sthdmirx_CORE_HDCP_init(RouteHandle);
  /* Reset the hadware */
  sthdmirx_CORE_enable_reset(RouteHandle, FALSE);
  sthdmirx_CORE_enable_auto_reset_on_no_clk(RouteHandle, TRUE);
  sthdmirx_CORE_enable_auto_reset_on_port_change(RouteHandle, TRUE);
  /* Disable the HDCP authentication */
  sthdmirx_CORE_reset(RouteHandle, RESET_CORE_CLOCK_DOMAIN_ONLY);
  sthdmirx_CORE_set_HDCP_TWSaddrs(RouteHandle, 0x0);
  sthdmirx_CORE_HDCP_enable(RouteHandle, FALSE);
  HDMI_PRINT("Input Processing pipe is initialised\n");
  return 0;
}

/*******************************************************************************
Name            : hdmirx_start_task
Description     : Start The HdmiRx Task
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int hdmirx_start_task(const hdmirx_handle_t Handle)
{

  int ret = 0;
  hdmirx_dev_handle_t *dHandle_p = (hdmirx_dev_handle_t *) Handle;

  ret = stm_hdmirx_sema_init(&dHandle_p->HdmiRx_Task.timeout_sem, 0);
  if (ret != 0)
    {
      HDMI_PRINT(KERN_ERR "%s:semaphore init fail :timeout_sem\n",__func__);
      return ret;
    }
  dHandle_p->HdmiRx_Task.exit = 0;
  dHandle_p->HdmiRx_Task.thread = NULL;
  /* hdmirxtask awakes to perform background processing */
  dHandle_p->HdmiRx_Task.delay_in_ms = STHDMIRX_MAIN_TASK_SCHEDULE_TIME;
  init_waitqueue_head(&(dHandle_p->wait_queue));
  ret = stm_hdmirx_thread_create(&dHandle_p->HdmiRx_Task.thread,
                                 (void (*)(void *))hdmirx_thread,
                                 (void *)dHandle_p,
                                 "VIB-HdmiRxMonit", thread_vib_hdmirxmonit);
  if (ret)
    {
      HDMI_PRINT(KERN_ERR "%s:hdmirx task creation fail \n",
                 __func__);
      return -ENOMEM;
    }
  return ret;
}

/*******************************************************************************
Name            : hdmirx_stop_task
Description     : Stop the Hdmi Receiver Task
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_stop_task(const hdmirx_handle_t Handle)
{
  int ret = 0;
  hdmirx_dev_handle_t *dHandle_p = (hdmirx_dev_handle_t *) Handle;

  /* Wakeup the task (fallthrough early) */
  stm_hdmirx_sema_signal(dHandle_p->HdmiRx_Task.timeout_sem);
  dHandle_p->HdmiRx_Task.exit = 1;	/*Gets task out of while(1) loop */
  /*stop a thread created by kthread_create() */
  ret = stm_hdmirx_thread_wait(&dHandle_p->HdmiRx_Task.thread);
  if (ret)
    HDMI_PRINT(KERN_ERR "%s Failed in stop the HdmiRx task = %d\n",
               __func__, ret);
  ret = stm_hdmirx_free(dHandle_p->HdmiRx_Task.timeout_sem);

  return ret;
}

/*******************************************************************************
Name            : Close
Description     : Close the device handle
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t Close(const hdmirx_handle_t Handle)
{
  stm_error_code_t ErrorCode = 0;
  U8 hIdx;
  hdmirx_dev_handle_t *Handle_p;

  Handle_p = (hdmirx_dev_handle_t *) Handle;

  if (Handle_p->Handle == HDMIRX_INVALID_DEVICE_HANDLE)
    {
      ErrorCode = -EINVAL;
    }
  else
    {
      /*close the input handles */
      for (hIdx = 0; hIdx < Handle_p->DeviceProp.Max_routes; hIdx++)
        {
          if (Handle_p->RouteHandle[hIdx].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
            close_input_route(&Handle_p->RouteHandle[hIdx]);
          Handle_p->RouteHandle[hIdx].bIsPortConnected = FALSE;
        }
      for (hIdx = 0; hIdx < Handle_p->DeviceProp.Max_Ports; hIdx++)
        {
          if (Handle_p->PortHandle[hIdx].Handle != HDMIRX_INVALID_PORT_HANDLE)
            {
              hdmirx_close_in_port(&Handle_p->PortHandle[hIdx]);
            }
          Handle_p->PortHandle[hIdx].bIsRouteConnected = FALSE;
        }
      Handle_p->Handle = HDMIRX_INVALID_DEVICE_HANDLE;
      ErrorCode = 0;
    }
  return (ErrorCode);
}

/*******************************************************************************
Name            : hdmirx_close_in_port
Description     : Disconnect the input port
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_close_in_port(hdmirx_port_handle_t *Handle)
{

  hdmirx_port_handle_t *Handle_p;
  Handle_p = (hdmirx_port_handle_t *) Handle;
  Handle_p->Handle = HDMIRX_INVALID_PORT_HANDLE;
  Handle_p->IsSourceDetectionStarted = FALSE;
  return 0;
}

/*******************************************************************************
Name            : hdmirx_hotplugpin_ctrl
Description     : Set/Clear the Hot plug Detect PIN
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_hotplugpin_ctrl(const hdmirx_port_handle_t *Handle,
                                        stm_hdmirx_port_hpd_state_t PinSet)
{
  sthdmirx_CSM_HPD_pin_drive_ctrl(
    (sthdmirx_CSM_context_t *) & Handle->stServiceModule,
    Handle->HPD_Port_Id,PinSet);
  return 0;
}

/*******************************************************************************
Name            : hdmirx_open_in_port
Description     : Open the Input Port.
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_open_in_port(hdmirx_port_handle_t *Port_Handle)
{
  stm_error_code_t ErrorCode = 0;

  /* Enable the EDID */
  if (Port_Handle->internal_edid == FALSE)
    {
      sthdmirx_CSM_external_EDID_enable(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule,
        Port_Handle->I2C_Master_Id);
    }
  else
    {
      sthdmirx_CSM_external_EDID_disable(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule,
        Port_Handle->I2C_Master_Id);
    }
  /*Start the sorce detection..means HDMI cable connected or not */
  Port_Handle->IsSourceDetectionStarted = TRUE;

  return ErrorCode;
}

/*******************************************************************************
Name            : hdmirx_read_EDID
Description     : Reads the EDID of the specified port
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_read_EDID(hdmirx_port_handle_t *Port_Handle,
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
      sthdmirx_CSM_external_EDID_enable(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule, PortId);
      /* Connect I2C Master to requested port I2C SDA/SCL Lines */
      sthdmirx_CSM_I2C_master_port_select(
        (sthdmirx_CSM_context_t *) &Port_Handle->stServiceModule, PortId);
      /*Calculate the Page Start Address */
      PageStartAddress = block_number * EDID_BLOCK_SIZE;
      DevAddrs = (U8) (EXTERNAL_EEPROM_DEVICE_ID |
                       (U8) ((U8) (PageStartAddress >> 8) << 1));
      pBuffer[0] = (U8) (PageStartAddress & 0xff);
#ifdef STHDMIRX_I2C_MASTER_ENABLE
      /* Write the Page Start Address, without stop bit */
      if (sthdmirx_I2C_master_waiting_write
          ((sthdmirx_I2C_master_handle_t) & Port_Handle->
           stI2CMasterCtrl, DevAddrs, pBuffer, EEPROM_SIZE_OFFSET,
           0) != TRUE)
        {
          HDMI_PRINT(" I2C Master ReadOp:: Address write Error!!\n");
          return (-EIO);
        }
      /*Read the complete Block */
      if (sthdmirx_I2C_master_waiting_read(
            (sthdmirx_I2C_master_handle_t) & Port_Handle->stI2CMasterCtrl,
            DevAddrs, pBuffer, EDID_BLOCK_SIZE, 1) != TRUE)
        {
          HDMI_PRINT(" I2C Master ReadOp:: Read data Error!!\n");
          return (-EIO);
        }
#endif
      /* copy the buffer data to Requested block buffer */
      stm_hdmirx_memcpy(edid_block,
                        pBuffer,sizeof(stm_hdmirx_port_edid_block_t));
    }
  else
    {
#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
      BOOL ReadStatus;
      /* Make the call to I2C Slave Read */
      ReadStatus = sthdmirx_I2C_slave_operation(edid_block, 128, PortId);
      if (ReadStatus == FALSE)
        {
          HDMI_PRINT("FAILURE in Read from Slave \n");
          return (-EIO);
        }
      else
        {
          HDMI_PRINT("SUCCESS in Read from Slave \n");
        }
#endif
    }
  return 0;
}

/*******************************************************************************
Name            : hdmirx_open_in_route
Description     : Open the Input Port.
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_open_in_route(hdmirx_route_handle_t *const pInpHandle)
{
  hdmirx_port_handle_t *Port_Handle;

  Port_Handle = (hdmirx_port_handle_t *) pInpHandle->PortHandle;
  // pInpHandle->PortHandle = (stm_hdmirx_port_h) Port_Handle;
  pInpHandle->pHYControl.EqualizationType = Port_Handle->Equalization_type;
  pInpHandle->HdmiRxMode = Port_Handle->HdmiRxMode;
  pInpHandle->pHYControl.CustomEqSetting.HighFreqGain =
    Port_Handle->Eq_Config.high_freq_gain;
  pInpHandle->pHYControl.CustomEqSetting.LowFreqGain =
    Port_Handle->Eq_Config.low_freq_gain;
  /*Power ON the Physical Layer */
  sthdmirx_PHY_power_mode_setup(&pInpHandle->pHYControl, PHY_POWER_ON);
  sthdmirx_CORE_select_PHY_source(pInpHandle, 0x10, TRUE);	//Select Combophy
#ifdef STHDMIRX_CLOCK_GEN_ENABLE

  sthdmirx_clkgen_powerupDDS(pInpHandle->stDdsConfigInfo.estVidDds,
                             pInpHandle->MappedClkGenAddress);
  sthdmirx_clkgen_powerupDDS(pInpHandle->stDdsConfigInfo.estAudDds,
                             pInpHandle->MappedClkGenAddress);
  sthdmirx_clkgen_DDS_openloop_force(pInpHandle->stDdsConfigInfo.
                                     estVidDds, VDDS_DEFAULT_INIT_FREQ,
                                     pInpHandle->MappedClkGenAddress);
#if 0
  sthdmirx_clkgen_DDS_openloop_force(pInpHandle->stDdsConfigInfo.
                                     estAudDds, ADDS_DEFAULT_INIT_FREQ,
                                     pInpHandle->MappedClkGenAddress);
#endif
#endif //#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  /*Setup the pixel clock out domain in VCLK by default */
  sthdmirx_CORE_config_clk(pInpHandle, HDMIRX_PIX_CLK_OUT_SEL,
                           HDMIRX_VCLK_SEL, FALSE);
  /*Reinitialise all software state machines */
  sthdmirx_PHY_setup_equalization_scheme(&pInpHandle->pHYControl);
  /*Clear all stored packet information */
  sthdmirx_CORE_clear_packet_memory(pInpHandle);
  /* Clear the signal present/Absent Notification flags */
  pInpHandle->bIsSignalPresentNotify = FALSE;
  pInpHandle->bIsNoSignalNotify = FALSE;
  pInpHandle->HdrxStatus = 0x0;
  pInpHandle->signal_status=STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT;
  /*Re start the clock measurement,clock stability,availability & abnormality */
  sthdmirx_reset_signal_process_handler(pInpHandle);
  pInpHandle->HdrxStatus |= HDRX_STATUS_LINK_INIT;
  sthdmirx_MEAS_reset_measurement(&pInpHandle->sMeasCtrl);
  /*Enable the HDCP Blk */
  sthdmirx_CORE_reset(pInpHandle, RESET_CORE_CLOCK_DOMAIN_ONLY);
  sthdmirx_CORE_set_HDCP_TWSaddrs(pInpHandle, 0x3A);
  sthdmirx_CORE_HDCP_enable(pInpHandle, TRUE);
  return 0;
}

/*******************************************************************************
Name            : hdmirx_resume_inputprocess
Description     : resume the Input signal Processing & .
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_resume_inputprocess(
  hdmirx_route_handle_t *const InpHandle_p)
{
  /*Power Down the Phy Cell */
  sthdmirx_PHY_power_mode_setup(&InpHandle_p->pHYControl, PHY_POWER_ON);
  sthdmirx_CORE_select_PHY_source(InpHandle_p, 0x10, TRUE);	//Select Combophy
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  /*Power Up the AUDDS & VDDS */
  sthdmirx_clkgen_powerupDDS(InpHandle_p->stDdsConfigInfo.estVidDds,
                             InpHandle_p->MappedClkGenAddress);
  sthdmirx_clkgen_powerupDDS(InpHandle_p->stDdsConfigInfo.estAudDds,
                             InpHandle_p->MappedClkGenAddress);
#endif //#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  /*Setup the pixel clock out domain in VCLK by default */
  sthdmirx_CORE_config_clk(InpHandle_p, HDMIRX_PIX_CLK_OUT_SEL,
                           HDMIRX_VCLK_SEL, FALSE);
  /*Reinitialise all software state machines */
  sthdmirx_PHY_setup_equalization_scheme(&InpHandle_p->pHYControl);
  /*Clear all stored packet information */
  sthdmirx_CORE_clear_packet_memory(InpHandle_p);

  /* Clear the signal present/Absent Notification flags */
  InpHandle_p->bIsSignalPresentNotify = FALSE;
  InpHandle_p->bIsNoSignalNotify = FALSE;
  /*Start signal detection */
  if (InpHandle_p->bIsPortConnected == TRUE)
    InpHandle_p->bIsSignalDetectionStarted = TRUE;
  /*Re start the clock measurement,clock stability,availability & abnormality */
  sthdmirx_reset_signal_process_handler(InpHandle_p);
  InpHandle_p->HdrxStatus |= HDRX_STATUS_LINK_INIT;
  sthdmirx_MEAS_reset_measurement(&InpHandle_p->sMeasCtrl);
  /* reset core */
  sthdmirx_CORE_reset(InpHandle_p, RESET_CORE_CLOCK_DOMAIN_ONLY);
  /*Unmute the audio */
  InpHandle_p->bIsAudioOutPutStarted = TRUE;
  sthdmirx_audio_soft_mute(InpHandle_p, FALSE);
  /*Enable the HDCP authentication */
  sthdmirx_CORE_set_HDCP_TWSaddrs(InpHandle_p, 0x3A);
  sthdmirx_CORE_HDCP_enable(InpHandle_p, TRUE);
  return (0);
}

/*******************************************************************************
Name            : hdmirx_stop_inputprocess
Description     : stop the Input signal Processing & .
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_stop_inputprocess(
  hdmirx_route_handle_t *const InpHandle_p)
{

  /*Mute the audio */
  InpHandle_p->bIsAudioOutPutStarted = FALSE;
  sthdmirx_audio_soft_mute(InpHandle_p, TRUE);
  /*Stop the signal detection */
  InpHandle_p->bIsNoSignalNotify = TRUE;
  InpHandle_p->bIsNoSignalNotified = FALSE;
  /*Power Down the Phy Cell */
  sthdmirx_PHY_power_mode_setup(&InpHandle_p->pHYControl, PHY_POWER_OFF);
  /*Disable the HDCP authentication */
  sthdmirx_CORE_set_HDCP_TWSaddrs(InpHandle_p, 0);
  sthdmirx_CORE_HDCP_enable(InpHandle_p, FALSE);
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  /*Power Down the AUDDS & VDDS */
  sthdmirx_clkgen_powerdownDDS(InpHandle_p->stDdsConfigInfo.estVidDds,
                               InpHandle_p->MappedClkGenAddress);
  sthdmirx_clkgen_powerdownDDS(InpHandle_p->stDdsConfigInfo.estAudDds,
                               InpHandle_p->MappedClkGenAddress);
#endif //#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  return (0);
}

/*******************************************************************************
Name            : close_input_route
Description     : Close the Input Route.
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t close_input_route(hdmirx_route_handle_t *const pInpHandle)
{
  stm_error_code_t ErrorCode = 0;
  hdmirx_route_handle_t *Handle_p;

  Handle_p = (hdmirx_route_handle_t *) pInpHandle;
  ErrorCode = hdmirx_stop_inputprocess(Handle_p);
  Handle_p->Handle = HDMIRX_INVALID_ROUTE_HANDLE;
  return ErrorCode;
}

/*******************************************************************************
Name            : hdmirx_get_audio_chan_status
Description     : Get the Audio Channel Statsu
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_get_audio_chan_status(const hdmirx_handle_t Handle,
    stm_hdmirx_audio_channel_status_t ChannelStatus[])
{
  U8 bAcsData[24];

  /* Get the Raw Audio Channel Status from the HDMIRx Hardware Register */
  sthdmirx_audio_channel_status_get(Handle, bAcsData);
  stm_hdmirx_memcpy(ChannelStatus, bAcsData, 24);
  return (0);
}

/*******************************************************************************
Name            : hdmirx_get_HPD_status
Description     : Get the Hpd Pin Status
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int hdmirx_get_HPD_status(hdmirx_port_handle_t *PortHandle)
{
  BOOL HpdStatus = FALSE;

  HpdStatus = sthdmirx_CSM_getHPD_status(
                &PortHandle->stServiceModule, PortHandle->HPD_Port_Id);
  return HpdStatus;

}

/*******************************************************************************
Name            : hdmirx_get_src_plug_status
Description     : Get the source plug  Status
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_hdmirx_port_source_plug_status_t hdmirx_get_src_plug_status(
  hdmirx_port_handle_t *Port_Handle)
{
  return (Port_Handle->stServiceModule.CablePDStatus);
}

/*******************************************************************************
Name            : hdmirx_get_signal_prop
Description     : Get the Signal property
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_get_signal_prop(hdmirx_route_handle_t *RouteHandle,
                            stm_hdmirx_signal_property_t *property)
{
  stm_hdmirx_audio_property_t AudProp;
  stm_hdmirx_video_property_t VidProp;
  stm_hdmirx_signal_timing_t SigTiming;

  if (RouteHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
    {
      property->signal_type = STM_HDMIRX_SIGNAL_TYPE_DVI;
      memset(&property->video_property, 0, sizeof(stm_hdmirx_video_property_t));
      memset(&property->audio_property, 0, sizeof(stm_hdmirx_audio_property_t));
    }
  else
    {
      property->signal_type = STM_HDMIRX_SIGNAL_TYPE_HDMI;
      sthdmirx_video_prop_changed_data_fill(RouteHandle, &VidProp);
      stm_hdmirx_memcpy(&property->video_property, &VidProp,
                        sizeof(stm_hdmirx_video_property_t));
      sthdmirx_audio_prop_changed_data_fill(RouteHandle, &AudProp);
      stm_hdmirx_memcpy(&property->audio_property, &AudProp,
                        sizeof(stm_hdmirx_audio_property_t));
    }
  sthdmirx_signal_timing_data_fill(RouteHandle, &SigTiming);
  stm_hdmirx_memcpy(&property->timing, &SigTiming,
                    sizeof(stm_hdmirx_signal_timing_t));
}

/*******************************************************************************
Name            : HdmiRx_Get_Video_Property
Description     : Get the Video property
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_get_video_prop(hdmirx_route_handle_t *RouteHandle,
                           stm_hdmirx_video_property_t *video_property)
{
  stm_hdmirx_video_property_t VidProp;

  if (RouteHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
    {
      memset(video_property, 0, sizeof(stm_hdmirx_video_property_t));
    }
   else
    {
      sthdmirx_video_prop_changed_data_fill(RouteHandle, &VidProp);
      stm_hdmirx_memcpy(video_property, &VidProp,
                        sizeof(stm_hdmirx_video_property_t));
    }
}

/*******************************************************************************
Name            : HdmiRx_Get_Audio_Property
Description     : Get the Audio Property
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_get_audio_prop(hdmirx_route_handle_t *RouteHandle,
                           stm_hdmirx_audio_property_t *audio_property)
{
  stm_hdmirx_audio_property_t AudProp;

  if (RouteHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
    {
      memset(audio_property, 0, sizeof(stm_hdmirx_audio_property_t));
    }
   else
    {
      sthdmirx_audio_prop_changed_data_fill(RouteHandle, &AudProp);
      stm_hdmirx_memcpy(audio_property, &AudProp,
                        sizeof(stm_hdmirx_audio_property_t));
    }
}

/*******************************************************************************
Name            : hdmirx_get_HDCP_status
Description     : Get the HDCP Status
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_hdmirx_route_hdcp_status_t hdmirx_get_HDCP_status(
  hdmirx_route_handle_t *RouteHandle)
{
  stm_hdmirx_route_hdcp_status_t HdcpStatus;
  HdcpStatus = sthdmirx_CORE_HDCP_status_get(RouteHandle);
  return HdcpStatus;
}

/*******************************************************************************
Name            : hdmirx_audio_soft_mute
Description     : Mutes the Audio
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_audio_soft_mute(hdmirx_route_handle_t *RouteHandle, BOOL Enable)
{
  sthdmirx_audio_soft_mute(RouteHandle, Enable);
}

/*******************************************************************************
Name            : sthdmirx_fill_infoevent_data
Description     : Get the Info event types
Parameters      : Handle -- route handle
                         info_type -- event type
                         info -- event data to be returned
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t sthdmirx_fill_infoevent_data(
  hdmirx_route_handle_t *const pInpHandle,
  stm_hdmirx_information_t info_type, void *info)
{
  switch (info_type)
    {
    case STM_HDMIRX_INFO_VSINFO:
    {
      sthdmirx_VS_info_data_fill(
        pInpHandle, (stm_hdmirx_vs_info_t *) info);
    }
    break;
    case STM_HDMIRX_INFO_SPDINFO:
    {
      sthdmirx_SPD_info_frame_data_fill(
        pInpHandle, (stm_hdmirx_spd_info_t *) info);
    }
    break;

    case STM_HDMIRX_INFO_MPEGSOURCEINFO:
    {
      sthdmirx_MPEG_info_frame_data_fill(
        pInpHandle, (stm_hdmirx_mpeg_source_info_t *) info);
    }
    break;
    case STM_HDMIRX_INFO_ISRCINFO:
    {
      sthdmirx_ISRC_info_frame_data_fill(
        pInpHandle, (stm_hdmirx_isrc_info_t *) info);
    }
    break;
    case STM_HDMIRX_INFO_ACPINFO:
    {
      sthdmirx_ACP_info_frame_data_fill(
        pInpHandle, (stm_hdmirx_acp_info_t *) info);
    }
    break;
    case STM_HDMIRX_INFO_GBDINFO:
    {
      sthdmirx_GBD_info_frame_data_fill(
        pInpHandle, (stm_hdmirx_gbd_info_t *) info);
    }
    break;

    }

  return (0);
}

/*******************************************************************************
Name            : hdmirx_notify_port_evts
Description     : Notify the requested Event
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_notify_port_evts(hdmirx_handle_t *Handle, U32 EventId)
{
  int ret = -1;
  stm_event_t event;
  event.event_id = EventId;
  event.object = (stm_hdmirx_port_h) Handle;
  ret = stm_event_signal(&event);
  return (ret);
}

/*******************************************************************************
Name            : hdmirx_notify_route_evts
Description     : Notify the requested Event
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_notify_route_evts(hdmirx_handle_t *Handle, U32 EventId)
{
  int ret = -1;
  stm_event_t event;
  event.event_id = EventId;
  event.object = (stm_hdmirx_route_h) Handle;

  ret = stm_event_signal(&event);
  return (ret);
}

/*******************************************************************************
Name            : hdmirx_release_pio
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
stm_error_code_t hdmirx_release_pio(hdmirx_port_handle_t *Port_handle)
{
  int ret=0;
#ifndef CONFIG_ARCH_STI
  if( Port_handle->Pad_State != NULL )
    {
      stm_pad_release(Port_handle->Pad_State);
      Port_handle->Pad_State = NULL;
    }
#else /* ifdef CONFIG_ARCH_STI */
  devm_pinctrl_put(Port_handle->pinctrl_p);
#endif /* CONFIG_ARCH_STI */
  return ret;
}

/*******************************************************************************
Name            : install_hdmirx_interrupt
Description     : Install the Hdmi Receiver  Interrupt,
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/

int install_hdmirx_interrupt(const hdmirx_dev_handle_t * Handle, uint32_t irq, char *irq_name)
{
#ifdef HDMIRX_IRQ_ENABLE
  if (irq != -1)
    {
      HDMI_PRINT("Installing Interrupt Handler  for IRQ %d\n", irq);
      if ((request_irq(irq, HdmiRXInterrupt_Handler, IRQF_DISABLED, irq_name,(void *) Handle)) < 0)
        {
          HDMI_PRINT("\nERROR: Failed to install hdmirx_core_irq Handler\n");
          return -ENODEV;
        }
    }
#endif
  return 0;
}

/*******************************************************************************
Name            : HdmiRXInterrupt_Handler
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/

#ifdef HDMIRX_IRQ_ENABLE
static irqreturn_t HdmiRXInterrupt_Handler(int irq, void *data)
{
  hdmirx_dev_handle_t *dHandle_p = (hdmirx_dev_handle_t *) data;

  return IRQ_HANDLED;

}
#endif

/*******************************************************************************
Name            : uninstall_hdmirx_interrupt
Description     :
Parameters      : None
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/

int uninstall_hdmirx_interrupt(const hdmirx_dev_handle_t * Handle, U32 IrqNum)
{
#ifdef HDMIRX_IRQ_ENABLE
  free_irq(IrqNum, Handle);

#endif

  return 0;
}

/*******************************************************************************
Name            : hdmirx_thread
Description     : starting of the HdmiRx State machine
Parameters      : HdmiRx Handle
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
void hdmirx_thread(void *data)
{
  int ret;
  uint16_t timeout;
  U8 hIdx;
  hdmirx_dev_handle_t *dHandle_p;
  hdmirx_route_handle_t *Route_Handle;

  dHandle_p = (hdmirx_dev_handle_t *) data;
  timeout = dHandle_p->HdmiRx_Task.delay_in_ms;
  ret = stm_hdmirx_thread_start();
  if (ret != 0)
    {
      HDMI_PRINT(KERN_ERR "%s:stmhdmirx thread start fail \n",__func__);
    }
  set_freezable();
  stm_hdmirx_sema_init(&dHandle_p->hdmirx_api_sema,1);
  do
    {
      wait_event_freezable_timeout(dHandle_p->wait_queue, kthread_should_stop(), msecs_to_jiffies(timeout));
      /*converting ms to jiffies */
      if(kthread_should_stop()) break;
      for (hIdx = 0; hIdx < dHandle_p->DeviceProp.Max_routes && !kthread_should_stop(); hIdx++)
        {
          /*look for valid handle */
          if (dHandle_p->RouteHandle[hIdx].Handle != HDMIRX_INVALID_ROUTE_HANDLE)  	/*look for valid handle */
            {
              Route_Handle =
                (hdmirx_route_handle_t *) & dHandle_p->RouteHandle[hIdx];
              stm_hdmirx_sema_wait(dHandle_p->hdmirx_api_sema);
              sthdmirx_main_signal_process_handler(Route_Handle);
              sthdmirx_evaluate_packet_events(Route_Handle);
              stm_hdmirx_sema_signal(dHandle_p->hdmirx_api_sema);
            }
        }
      /*Source plug detection */
      stm_hdmirx_sema_wait(dHandle_p->hdmirx_api_sema);
      sthdmirx_source_plug_detect_task(dHandle_p);
      stm_hdmirx_sema_signal(dHandle_p->hdmirx_api_sema);
    }
  while (dHandle_p->HdmiRx_Task.exit == 0);

  stm_hdmirx_sema_delete(dHandle_p->hdmirx_api_sema);
  stm_hdmirx_thread_exit();
}
