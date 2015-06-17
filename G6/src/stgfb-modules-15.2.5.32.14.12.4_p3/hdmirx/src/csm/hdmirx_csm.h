/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_csm.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_CSM_H__
#define __HDMIRX_CSM_H__

/*Includes------------------------------------------------------------------------------*/
#include <stm_hdmirx.h>
#include <stddefs.h>

#ifdef STHDMIRX_I2C_MASTER_ENABLE
#include <hdmirx_i2cmaster.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

  /* Private Types ---------------------------------------------------------- --------------*/
#define STHDMIRX_CSM_MAX_INPUT_PORT     4
  /* Private Constants --------------------------------------------------------------------- */

  /* Private variables (static) ---------------------------------------------------------------*/

  /* Global Variables ----------------------------------------------------------------------- */

  typedef U32 sthdmirx_I2C_slave_handle_t;
  /*Initialization Structure of I2C Slave module*/
  typedef struct
  {
    U32 RegBaseAddrs;
    U8 PortNo;
    BOOL bIsClkStretchNeeded;
    BOOL bIsFIFOModeNeeded;
    U8 DevAddress;
  } sthdmirx_I2C_slave_init_params_t;

#ifdef STHDMIRX_I2C_MASTER_ENABLE
  typedef U32 sthdmirx_I2C_master_handle_t;

  /*Initialization Structure of I2C Master module*/
  typedef struct
  {
    U32 RegBaseAddrs;
    U32 InpClkFreqHz;
    U32 BaudRateHz;
  } sthdmirx_I2C_master_init_params_t;
#endif

  typedef enum
  {
    STHDMIRX_CABLE_DETECT_START,
    STHDMIRX_CABLE_DETECT_STOP,
    STHDMIRX_CABLE_DETECT_WAIT,
    STHDMIRX_CABLE_CONNECTED,
    STHDMIRX_CABLE_DISCONNECTED
  } sthdmirx_cable_pwr_detect_states_t;

typedef enum
{
  STM_HDMIRX_CSM_HPD_SEL_A,
  STM_HDMIRX_CSM_HPD_SEL_B,
  STM_HDMIRX_CSM_HPD_SEL_C
} hdmirx_hpd_sel_t;

typedef enum
{
  STM_HDMIRX_CSM_I2C_SEL_A,
  STM_HDMIRX_CSM_I2C_SEL_B,
  STM_HDMIRX_CSM_I2C_SEL_C
} hdmirx_i2c_sel_t;

  typedef struct
  {
    U32 ulCsmCellBaseAddrs;
    U8 uNoOfPortToBeMonitored;
    U8 CurrentMonitorPort;
    U32 ulDetectTime;
    stm_hdmirx_port_source_plug_status_t CablePDStatus;
    stm_hdmirx_port_hpd_state_t HpdPinStatus;
    sthdmirx_cable_pwr_detect_states_t State;
  } sthdmirx_CSM_context_t;

  typedef struct
  {
    hdmirx_hpd_sel_t hpd_sel;
    BOOL hpd_sel_ctr;
    BOOL hpd_direct_ctr;
    hdmirx_i2c_sel_t i2c_mccs_sel;
    BOOL i2c_mccs_sel_ctr;
    hdmirx_i2c_sel_t i2c_master_sel;
    BOOL i2c_master_sel_ctr;
  } sthdmirx_CSM_config_t;

  /* Private Macros ------------------------------------------------------------------------ */

  /* Exported Macros--------------------------------------------------------- --------------*/

  /* Exported Functions ----------------------------------------------------- ---------------*/

  /*I2C Master Function prototypes*/
#ifdef STHDMIRX_I2C_MASTER_ENABLE
  void sthdmirx_I2C_master_init(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                sthdmirx_I2C_master_init_params_t *pInits);
  void sthdmirx_I2C_master_ISR_client(
    sthdmirx_I2C_master_handle_t I2CMasterHandle);
  BOOL sthdmirx_I2C_master_read(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                U8 uDevAddrs,U8 *puBuffer, U16 ulLength, U8 uStop);
  BOOL sthdmirx_I2C_master_write(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                 U8 uDevAddrs,U8 *puBuffer, U16 ulLength, U8 uStop);
  BOOL sthdmirx_I2C_master_waiting_read(
    sthdmirx_I2C_master_handle_t I2CMasterHandle, U8 uDevAddrs,
    U8 *puBuffer, U16 ulLength, U8 uStop);
  BOOL sthdmirx_I2C_master_waiting_write(sthdmirx_I2C_master_handle_t I2CMasterHandle,
                                         U8 uDevAddrs, U8 *puBuffer, U16 ulLength, U8 uStop);
#endif

  /* I2C Slave Function prototypes */
  void sthdmirx_I2C_slave_init(sthdmirx_I2C_slave_handle_t *I2CSlaveHandle,
                               sthdmirx_I2C_slave_init_params_t *slInit);
  BOOL sthdmirx_I2C_slave_operation(U8 *puBuffer, U16 ulLength, U32 PortNum);
  U8 sthdmirx_I2C_read_from_slaveFIFO(U8 *puBuffer, U16 ulLength,
                                      U8 DeviceAddrs, U32 PortNum);
  BOOL sthdmirx_I2C_write_to_slaveFIFO(U8 *puBuffer, U16 ulLength,
                                       U8 DeviceAddrs, U32 PortNum);
  void sthdmirx_I2C_slave_reset(U8 PortNum);
  BOOL sthdmirx_I2C_register_dev_id(U8 DeviceAddrs, U8 PortNum);
  BOOL sthdmirx_I2C_unregister_dev_id(U8 DeviceAddrs, U8 PortNum);

  /*Service Module*/
  void sthdmirx_CSM_HPD_pin_drive_ctrl(sthdmirx_CSM_context_t *pSMHandle,
                                       uint32_t PortNo, stm_hdmirx_port_hpd_state_t bPinHigh);
  BOOL sthdmirx_CSM_external_EDID_disable(sthdmirx_CSM_context_t *pSMHandle,
                                          uint32_t PortNo);
  BOOL sthdmirx_CSM_external_EDID_enable(sthdmirx_CSM_context_t *pSMHandle,
                                         uint32_t PortNo);

  BOOL sthdmirx_HPD_init(sthdmirx_CSM_context_t *pServiceModule);
  BOOL sthdmirx_CSM_cable_power_detect(sthdmirx_CSM_context_t *pSMHandle,
                                       uint32_t PortNo);
  void sthdmirx_CSM_reset_module_handle(sthdmirx_CSM_context_t *pSMHandle);
  void sthdmirx_CSM_I2C_master_port_select(sthdmirx_CSM_context_t *pSMHandle,
      U8 PortId);
  U32 sthdmirx_CSM_getHPD_status(sthdmirx_CSM_context_t *pSMHandle,
                                 uint32_t Port);

  /* ------------------------------- End of file --------------------------------------------- */
#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CSM_H__ */
