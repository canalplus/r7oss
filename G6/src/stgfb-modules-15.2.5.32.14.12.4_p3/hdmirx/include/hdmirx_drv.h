/***********************************************************************
 *
 * File: hdmirx/inlude/hdmirx_drv.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_DRV_H__
#define __HDMIRX_DRV_H__
#include <linux/platform_device.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/gpio.h>
#include <linux/stm/pad.h>
#else
#include <linux/pinctrl/consumer.h>
#endif
#include <stm_hdmirx.h>
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
#include <hdmirx_clkgen.h>
#endif
#include <hdmirx_core.h>
#include <hdmirx_csm.h>
#include <hdmirx_Combophy.h>
#include <hdmirx_utility.h>
#include <hdmirx.h>

#define     STHDMIRX_MAX_DEVICE                     1
#define     STHDMIRX_MAX_PORT                       5 /* In general device will have N ports taking 5 at max*/
#define     STHDMIRX_MAX_ROUTE                      2 /* In general device will have maximum 2 routes */

#define     VDDS_DEFAULT_INIT_FREQ                  0x278F78UL
#define     ADDS_DEFAULT_INIT_FREQ                  0x278F78UL

#define     HDMIRX_INVALID_DEVICE_HANDLE            ((stm_hdmirx_device_h)0xFFFFFFFFUL)
#define     HDMIRX_INVALID_PORT_HANDLE              ((stm_hdmirx_port_h)0xFFFFFFFFUL)
#define     HDMIRX_INVALID_ROUTE_HANDLE             ((stm_hdmirx_route_h)0xFFFFFFFFUL)
#define     INVALID_ID                              0xFF
#define     EDID_BLOCK_SIZE                         128
#define     EEPROM_MAX_PAGE_SIZE                    32
#define     EEPROM_PAGE_SIZE                        16
#define     EEPROM_MAX_SIZE                         (EEPROM_MAX_PAGE_SIZE*EEPROM_PAGE_SIZE)
#define     EEPROM_SIZE_OFFSET                      1
#define     EEPROM_WRITE_DELAY                      5   /* 5 msec */
#define     EEPROM_READ_DELAY                       10  /* Unit is Ticks */
#define     EXTERNAL_EEPROM_DEVICE_ID               0xA0

#define     is_valid_device_handle(hDevice_p,device_h)  (((device_h!=NULL)&&(hDevice_p->Handle==device_h)))
#define     is_valid_port_handle( hInput_p,port_h)   (((hInput_p!=NULL)&&(hInput_p->Handle==port_h)))
#define     is_valid_route_handle( hInput_p,route_h)   (((hInput_p!=NULL)&&(hInput_p->Handle==route_h)))

typedef void *hdmirx_handle_t;
struct hdmirx_DeviceHandle_s;
typedef struct
{
  stm_hdmirx_thread thread;
  /*To provide desceduling for the task after one cycle of execution */
  stm_hdmirx_semaphore *timeout_sem;
  uint32_t delay_in_ms;
  uint32_t exit;
  BOOL IsRunning;		/* TRUE if task is running, FALSE otherwise */
  BOOL ToBeDeleted;	/* Set TRUE to ask task to end in order to delete it */
} hdmirx_task_t;
typedef struct
{
  struct device *dev;
  U32 DeviceId;
  U32 MappedRegisterAddrs;
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  U32 MappedClkGenAddress;
#endif
  uint32_t Max_Ports;
  uint32_t Max_routes;
} sthdmirx_dev_prop_t;
typedef struct
{
  sthdmirx_AVDDStypes_t estVidDds;
  sthdmirx_AVDDStypes_t estAudDds;
} STHDMIRX_DDSConfigParams_t;
typedef struct
{
  U32 BaseAddress;
  U32 PacketBaseAddress;
  stm_hdmirx_route_h Handle;
  uint32_t RouteID;
  uint32_t PortNo;
  BOOL bIsHWInitialized;
  stm_hdmirx_operational_mode_t HdmiRxMode;
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  STHDMIRX_DDSConfigParams_t stDdsConfigInfo;
#endif
  sthdmirx_signal_meas_ctrl_t sMeasCtrl;
  sthdmirx_PHY_context_t pHYControl;
  sthdmirx_IFM_context_t IfmControl;
  sthdmirx_sub_sampler_mode_t HdmiSubsamplerMode;
  sthdmirx_FSM_states_t HdrxState;
  sthdmirx_status_flags_t HdrxStatus;
  BOOL bIsSignalDetectionStarted;
  BOOL bIsAudioOutPutStarted;
  BOOL bIsAudioPropertyChanged;
  BOOL bIsSignalPresentNotify;
  BOOL bIsNoSignalNotify;
  BOOL bIsNoSignalNotified;
  BOOL bIsSigPresentNotified;
  BOOL bIsHDCPAuthenticationDetected;
  U32 ulAVIInfoFrameAvblTMO;
  sthdmirx_Sdata_pkt_acquition_t stInfoPacketData;
  sthdmirx_pkt_new_data_flags_t stNewDataFlags;
  sthdmirx_pkt_data_available_flags_t stDataAvblFlags;
  hdrx_input_signal_type_t HwInputSigType;
  stm_hdmirx_output_pixel_width_t OutputPixelWidth;
  sthdmirx_audio_Mngr_ctrl_t stAudioMngr;
  BOOL bIsPacketNoisedetected;
  BOOL bIsPacketNoiseMonitorStarted;
  stm_hdmirx_route_hdcp_status_t HDCPStatus;
  U16 I2SClkFactor;
  uint32_t PortId;
  BOOL bIsPortConnected;
  BOOL repeater_fn_support;
  stm_hdmirx_route_signal_status_t signal_status;
  stm_hdmirx_port_h PortHandle;
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
  U32 MappedClkGenAddress;
#endif
  uint32_t HdmiRx_Irq;
  struct hdmirx_DeviceHandle_s * hDevice;
  U8 DownstreamKsvList[127*5];
  U32 DownstreamKsvSize;
  BOOL RepeaterReady;
  U8 rterm_mode;
  U8 rterm_val;
} hdmirx_route_handle_t;
typedef struct hdmirx_PortHandle_s
{

  stm_hdmirx_port_h Handle;
  uint32_t portId;
  uint32_t I2C_Master_Id;
  uint32_t I2C_MCCS_Id;
  uint32_t HPD_Port_Id;
  BOOL bIsRouteConnected;
  sthdmirx_CSM_context_t stServiceModule;
  U8 IsSourceDetectionStarted;
#ifdef STHDMIRX_I2C_MASTER_ENABLE
  sthdmirx_I2C_master_control_t stI2CMasterCtrl;
#endif
#ifdef STHDMIRX_INTERNAL_EDID_SUPPORT
  sthdmirx_I2C_slave_handle_t I2CSlaveHandle;
#endif
  stm_hdmirx_route_h RouteHandle;
  bool enable_hpd;
  bool internal_edid;
  bool listen_ddc2bi;
  stm_hdmirx_equalization_mode_t Equalization_type;
  stm_hdmirx_operational_mode_t HdmiRxMode;
#ifndef CONFIG_ARCH_STI
  struct stm_pad_config Pad_Config;
  struct stm_pad_state *Pad_State;
#else
  struct pinctrl_state * pinctrl_state;
  struct pinctrl * pinctrl_p;
#endif
  uint32_t Route_Connectivity_Mask;
  stm_hdmirx_equalization_config_t Eq_Config;
  uint32_t Hpd_Pio;
  uint32_t Pd_Pio;
  uint32_t SCL_Pio;
  uint32_t SDA_Pio;
  uint32_t EDID_WP;
  bool Ext_Mux;
  int (*ext_mux) (uint32_t port_id);
  struct hdmirx_DeviceHandle_s * hDevice;
} hdmirx_port_handle_t;

typedef struct hdmirx_DeviceHandle_s
{
  sthdmirx_dev_prop_t DeviceProp;
  U32 AnaPhyBaseAddress;
  stm_hdmirx_device_h Handle;
  stm_hdmirx_semaphore *hdmirx_api_sema;
  stm_hdmirx_semaphore *hdmirx_pm_sema;
  hdmirx_task_t HdmiRx_Task;
  hdmirx_port_handle_t PortHandle[STHDMIRX_MAX_PORT];
  hdmirx_route_handle_t RouteHandle[STHDMIRX_MAX_ROUTE];
  U32 CsmCellBaseAddress;
  U32 ulMeasClkFreqHz;
  wait_queue_head_t wait_queue;
  bool is_standby;
  uint32_t device_count;
  uint32_t route_count;
  uint32_t port_count;
} hdmirx_dev_handle_t;
extern hdmirx_dev_handle_t dev_handle[];

typedef enum
{
  HDMIRX_PM_RESUME   = 0,
  HDMIRX_PM_SUSPEND  = 1,
  HDMIRX_PM_FREEZE   = 2,
  HDMIRX_PM_POWEROFF = 3,
  HDMIRX_PM_RESTORE  = 4
} hdmirx_pwr_modes_t;

void hdmirx_term(struct platform_device *pdev);
uint32_t hdmirx_init(struct platform_device * pdev);
uint32_t hdmirx_initialisation(hdmirx_handle_t Handle);
uint32_t hdmirx_SetPowerState(struct platform_device * pdev, hdmirx_pwr_modes_t power_modes);

void hdmirx_init_CSM(const hdmirx_handle_t Handle);
void hdmirx_init_HPD(const hdmirx_handle_t Handle);
stm_error_code_t hdmirx_init_I2C(const hdmirx_handle_t Handle);
stm_error_code_t hdmirx_init_IFM(const hdmirx_handle_t Handle,
                                 U32 ulMeasFreqHz);
stm_error_code_t HdmiRx_InitInput(hdmirx_route_handle_t *const RouteHandle,
                                  U32 ulMeasRefClkFreqHz);
int hdmirx_start_task(const hdmirx_handle_t Handle);
stm_error_code_t hdmirx_stop_task(const hdmirx_handle_t Handle);
stm_error_code_t Close(const hdmirx_handle_t Handle);
stm_error_code_t hdmirx_close_in_port(hdmirx_port_handle_t *Handle);
stm_error_code_t close_input_route(hdmirx_route_handle_t *const pInpHandle);
stm_error_code_t hdmirx_open_in_route(hdmirx_route_handle_t *const pInpHandle);
stm_error_code_t hdmirx_hotplugpin_ctrl(const hdmirx_port_handle_t *Handle,
                                        stm_hdmirx_port_hpd_state_t PinSet);
void hdmirx_audio_soft_mute(hdmirx_route_handle_t *RouteHandle, BOOL Enable);
stm_error_code_t hdmirx_stop_inputprocess(
  hdmirx_route_handle_t *const InpHandle_p);
stm_error_code_t hdmirx_resume_inputprocess(
  hdmirx_route_handle_t *const InpHandle_p);
stm_error_code_t hdmirx_get_audio_chan_status(const hdmirx_handle_t Handle,
    stm_hdmirx_audio_channel_status_t ChannelStatus[]);
stm_error_code_t sthdmirx_fill_infoevent_data(
  hdmirx_route_handle_t *const pInpHandle,
  stm_hdmirx_information_t info_type, void *info);
int hdmirx_get_HPD_status(hdmirx_port_handle_t *PortHandle);
void hdmirx_get_signal_prop(hdmirx_route_handle_t *RouteHandle,
                            stm_hdmirx_signal_property_t *property);
void hdmirx_get_video_prop(hdmirx_route_handle_t *RouteHandle,
                           stm_hdmirx_video_property_t *property);
void hdmirx_get_audio_prop(hdmirx_route_handle_t *RouteHandle,
                           stm_hdmirx_audio_property_t *property);
stm_hdmirx_route_hdcp_status_t hdmirx_get_HDCP_status(
  hdmirx_route_handle_t *RouteHandle);
int install_hdmirx_interrupt(const hdmirx_dev_handle_t *Handle, uint32_t irq, char *irq_name);
int uninstall_hdmirx_interrupt(const hdmirx_dev_handle_t *Handle, U32 IrqNum);
stm_error_code_t hdmirx_notify_port_evts(hdmirx_handle_t *Handle, U32 EventId);
stm_error_code_t hdmirx_notify_route_evts(hdmirx_handle_t *Handle,U32 EventId);
void hdmirx_thread(void *data);
stm_error_code_t hdmirx_open_in_port(hdmirx_port_handle_t *Port_Handle);
stm_error_code_t hdmirx_configure_pio(hdmirx_port_handle_t *Port_handle);
stm_error_code_t hdmirx_release_pio(hdmirx_port_handle_t *Port_handle);
stm_hdmirx_port_source_plug_status_t hdmirx_get_src_plug_status(
  hdmirx_port_handle_t *Port_Handle);
stm_error_code_t hdmirx_read_EDID(hdmirx_port_handle_t *Port_Handle,
                                  uint32_t block_number,stm_hdmirx_port_edid_block_t *edid_block);
stm_error_code_t hdmirx_update_EDID(hdmirx_port_handle_t *Port_Handle,
                                    uint32_t block_number,stm_hdmirx_port_edid_block_t *edid_block);

#endif /*__HDMIRX_DRV_H__*/
