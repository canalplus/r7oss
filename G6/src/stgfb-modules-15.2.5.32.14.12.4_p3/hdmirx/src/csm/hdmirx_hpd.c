/***********************************************************************
 *
 * File: hdmirx/src/csm/hdmirx_hpd.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/
#include <stddefs.h>

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/
#include <hdmirx_csm.h>
#include <hdmirx_hpd.h>
#include <InternalTypes.h>
#include <hdmirx_RegOffsets.h>
/* Private Typedef -----------------------------------------------*/

//#define ENABLE_DBG_HDRX_HPD_INFO

#ifdef ENABLE_DBG_HDRX_HPD_INFO
#define DBG_HDRX_HPD_INFO	HDMI_PRINT
#else
#define DBG_HDRX_HPD_INFO(a,b...)
#endif
/* Private Defines ------------------------------------------------*/

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION       : sthdmirx_HPD_init()
 USAGE        	: Initializes the HPD block for the all the ports
 INPUT        	: HDMIRx Handle
 RETURN       	:
 USED_REGS 	    :
******************************************************************************/
BOOL sthdmirx_HPD_init(sthdmirx_CSM_context_t *pServiceModule)
{

  sthdmirx_CSM_reset_module_handle(pServiceModule);

  DBG_HDRX_HPD_INFO("sthdmirx_HPD_init done!! \n");

  return TRUE;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_CSM_reset_module_handle()
 USAGE        	:   Initialize the HPD Handle structure
 INPUT        	:   HPD Handle
 RETURN       	:
 USED_REGS 	    :
******************************************************************************/
void sthdmirx_CSM_reset_module_handle(sthdmirx_CSM_context_t *pSMHandle)
{
  /* Initialize all Handle parameters for HPD */
  pSMHandle->CablePDStatus = STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT;
  pSMHandle->HpdPinStatus = STM_HDMIRX_PORT_HPD_STATE_LOW;
  pSMHandle->ulDetectTime = stm_hdmirx_time_now();
  pSMHandle->State = STHDMIRX_CABLE_DETECT_STOP;
  pSMHandle->CurrentMonitorPort = 0;

  DBG_HDRX_HPD_INFO("STHDMIRX_HPD_InitHandle - HPD_Handle initialized \n");
}

/******************************************************************************
 FUNCTION   :   STHDMIRX_CSM_HPDSet()
 USAGE      :   To Control the HPD Pin connected to a port
 INPUT      :   HPD Handle, Port no to be set/cleared, Status to be set
 RETURN     :
 USED_REGS 	:
******************************************************************************/
void sthdmirx_CSM_HPD_pin_drive_ctrl(sthdmirx_CSM_context_t *pSMHandle,
                                     uint32_t PortNo, stm_hdmirx_port_hpd_state_t bPinHigh)
{
  /* Hpd Driving logic is same for External & Internal EDID configuration */
  /* Drive the HPD pin */
  if (bPinHigh == STM_HDMIRX_PORT_HPD_STATE_HIGH)
    HDMI_SET_REG_BITS_DWORD((U32) pSMHandle->ulCsmCellBaseAddrs +HPD_DIRECT_CTRL, (1<<PortNo));
  else
    HDMI_CLEAR_REG_BITS_DWORD((U32) pSMHandle->ulCsmCellBaseAddrs +HPD_DIRECT_CTRL, (1<<PortNo));

  /* Update the software variables as per HPD Pins */
  pSMHandle->HpdPinStatus = bPinHigh;
  DBG_HDRX_HPD_INFO("STHDMIRX_CSM_EDIDControl - HPD Control \n");

  return;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_CSM_getHPD_status()
 USAGE        	:   Gets the HPD Pin Status, External EDID- gives Cable power status, Internal EDID, just HPD Pin Status
 INPUT        	:   Port No.
 RETURN       	:   Status on the given input port
 USED_REGS 	    :
******************************************************************************/
U32 sthdmirx_CSM_getHPD_status(sthdmirx_CSM_context_t *pSMHandle,
                               uint32_t PortNo)
{
  BOOL uHpdStatus;

  uHpdStatus = ((HDMI_READ_REG_DWORD((U32) pSMHandle->ulCsmCellBaseAddrs+HPD_DIRECT_STATUS)&(1<< PortNo))>> PortNo)&0x1;

  DBG_HDRX_HPD_INFO("HPD on Port %d is %d \n", PortNo, uHpdStatus);
  return uHpdStatus;
}

/******************************************************************************
 FUNCTION     : STHDMIRX_CSM_EDIDDisable()
 USAGE        : EDPD Pin is dedicated for PD ( Internal EDID) & EDID Disable ( External EDID)
 INPUT        : Enable/Disable external EDID
 RETURN       :
 USED_REGS 	  :
******************************************************************************/
BOOL sthdmirx_CSM_external_EDID_disable(sthdmirx_CSM_context_t *pSMHandle,
                                        uint32_t PortNo)
{

  HDMI_CLEAR_REG_BITS_DWORD((U32) pSMHandle->ulCsmCellBaseAddrs + EDPD_DIRECT_CTRL, (1<<PortNo));

  DBG_HDRX_HPD_INFO("STHDMIRX_CSM_EDIDControl - EDID Disable :%d \n",PortNo);
  return TRUE;
}

/******************************************************************************
 FUNCTION     : sthdmirx_CSM_external_EDID_enable()
 USAGE        : EDPD Pin is dedicated for PD ( Internal EDID) & EDID Disable ( External EDID)
 INPUT        : Enable/Disable external EDID
 RETURN       :
 USED_REGS 	  :
******************************************************************************/
BOOL sthdmirx_CSM_external_EDID_enable(sthdmirx_CSM_context_t *pSMHandle,
                                       uint32_t PortNo)
{

  HDMI_SET_REG_BITS_DWORD((U32) pSMHandle->ulCsmCellBaseAddrs + EDPD_DIRECT_CTRL, (1<<PortNo));

  DBG_HDRX_HPD_INFO("STHDMIRX_CSM_EDIDControl - EDID Enable :%d \n",PortNo);
  return TRUE;
}

/******************************************************************************
 FUNCTION       : sthdmirx_CSM_I2C_master_port_select()
 USAGE        	: Initializes the HPD block for the all the ports
 INPUT        	: HDMIRx Handle
 RETURN       	:
 USED_REGS 	    :
******************************************************************************/
void sthdmirx_CSM_I2C_master_port_select(sthdmirx_CSM_context_t *pSMHandle, U8 PortId)	//bIsCSM  flag remove
{
  PortId = PortId & 0x07;

  HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
    (U32)((U32) pSMHandle->ulCsmCellBaseAddrs +HDRX_CSM_CTRL),
    HDRX_CSM_I2C_MASTER_SEL_MASK,(PortId << HDRX_CSM_I2C_MASTER_SEL_SHIFT));
}

/* End of file */
