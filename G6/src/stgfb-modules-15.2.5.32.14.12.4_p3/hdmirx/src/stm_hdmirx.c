/***********************************************************************
 *
 * File: hdmirx/src/stm_hdmirx.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes -------------*/

#include <linux/errno.h>
#include <linux/semaphore.h>
#include <linux/export.h>

/* Local Includes ---------------*/
#include <stm_hdmirx.h>
#include <hdmirx_drv.h>
#include <stm_hdmirx_os.h>
#include <hdmirx_utils.h>
#include <hdmirx_core_export.h>

hdmirx_dev_handle_t dev_handle[STHDMIRX_MAX_DEVICE];

static sthdmirx_dev_prop_t *get_dev_id(uint32_t *DeviceId);

/* Init/Open/Close/Term protection semaphore */
static stm_hdmirx_semaphore *inst_access_sem;

/*******************************************************************************
Name            : stm_hdmirx_device_open
Description     : This function returns the device handle. All subsequent calls
                  to access the device functions should use this handle
Parameters      : id - Device identifier 0..n.,
                : device - Device handle
Assumptions     :
Limitations     :
Returns         : 0 - No Error
********************************************************************************/
int stm_hdmirx_device_open(stm_hdmirx_device_h *device, uint32_t id)
{
  hdmirx_dev_handle_t *dhandle;
  stm_hdmirx_device_s *device_s;
  stm_error_code_t err = 0;

  if (NULL == device)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EINVAL;
    }

  if (id >= STHDMIRX_MAX_DEVICE)
    {
      HDMI_PRINT("ERROR:Invalid device identifier\n");
      return -EFAULT;
    }

  if (get_dev_id(&id) == NULL)
    {
      HDMI_PRINT("ERROR:Unknown Device\n");
      return -EINVAL;
    }


  dhandle = &dev_handle[id];
  dhandle->device_count++;
  if (dhandle->Handle != HDMIRX_INVALID_DEVICE_HANDLE)
    {
      (*device) = dhandle->Handle;
      return 0;
    }
  /* create the lock task */
  stm_hdmirx_sema_init(&inst_access_sem,1);
  /*lock the task */
  stm_hdmirx_sema_wait(inst_access_sem);
  /*Start Task */
  err = hdmirx_start_task(dhandle);
  if (err != 0)
    {
      HDMI_PRINT("ERROR:hdmirx_start_task failed, error %x\n",
                 err);
      stm_hdmirx_sema_signal(inst_access_sem);
      return (err);
    }

  device_s = stm_hdmirx_malloc(sizeof(stm_hdmirx_device_s));
  if (device_s == NULL)
    {
      HDMI_PRINT("ERROR:KMALLOC FAILED\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return EINVAL;
    }

  device_s->handle = (void *)dhandle;
  (*device) = (stm_hdmirx_device_h) device_s;
  dhandle->Handle = (stm_hdmirx_device_h) device_s;
  /* Unlock the OS task  */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_device_open);

/*******************************************************************************
Name            : stm_hdmirx_device_close
Description     : This function closes all the device operation and relinquishes
                  the control over device
Parameters      : device - Device handle
Assumptions     :
Limitations     :
Returns         : 0 - No Error
********************************************************************************/
int stm_hdmirx_device_close(stm_hdmirx_device_h device)
{
  hdmirx_dev_handle_t *dhandle;
  uint32_t dev_idx = 0;
  stm_error_code_t err = 0;
  stm_hdmirx_device_s *device_s;

  if ((NULL == device))
    {
      HDMI_PRINT("ERROR:Invalid Device Handle\n");
      return -EINVAL;
    }

  /*Obtain the Global Semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  device_s = (stm_hdmirx_device_s *) device;
  dhandle = (hdmirx_dev_handle_t *) device_s->handle;
  /* Check the Valid Device Handle */
  if (!is_valid_device_handle(dhandle, device))
    {
      HDMI_PRINT("ERROR:Device ...ST_ERROR_INVALID_HANDLE\n");
      return -EINVAL;
    }

  dev_idx = dhandle->DeviceProp.DeviceId;
  if (get_dev_id(&dev_idx) == NULL)
    {
      HDMI_PRINT("ERROR:ST_ERROR_UNKNOWN_DEVICE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if(--dhandle->device_count!=0)
  {
    stm_hdmirx_sema_signal(inst_access_sem);
    return 0;
  }

  /*stop the task */
  err = hdmirx_stop_task(dhandle);
  if (0 != err)
    {
      printk(KERN_ERR "ERROR:HDMIRX Stop task failed\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return err;
    }

  /*Close the Device Handle & InputHandle */
  err = Close(dhandle);
  if (0 != err)
    {
      HDMI_PRINT("ERROR:HDMIRX Device Close : failed, error \n");
    }
  if(stm_hdmirx_free(device_s)<0)
    HDMI_PRINT("ERROR:Can't delete hdmi-rx semaphore\n");

  /*Release the Global Semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  /* delete the lock task */
  stm_hdmirx_sema_delete(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_device_close);

/*******************************************************************************
Name            : stm_hdmirx_device_get_capability
Description     : This function returns the capability of underlying device
Parameters      : device -- DeviceHandle
				  capability -- Device Capability structure
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_device_get_capability(stm_hdmirx_device_h device,
                                     stm_hdmirx_device_capability_t
                                     *capability)
{

  hdmirx_dev_handle_t *dhandle;
  stm_hdmirx_device_s *dev;

  if ((NULL == device))
    {
      HDMI_PRINT("ERROR:Invalid Device Handle\n");
      return -EINVAL;
    }

  if (NULL == capability)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*Obtain the Global Semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  dev = (stm_hdmirx_device_s *) device;
  dhandle = (hdmirx_dev_handle_t *) dev->handle;
  /* Check the Valid Device Handle */
  if (!is_valid_device_handle(dhandle, device))
    {
      HDMI_PRINT("ERROR:Device ...ST_ERROR_INVALID_HANDLE\n");
      return -EINVAL;
    }

  capability->number_of_ports = dhandle->DeviceProp.Max_Ports;
  capability->number_of_routes = dhandle->DeviceProp.Max_routes;
  /*Release the Global Semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_device_get_capability);

/*******************************************************************************
Name            : stm_hdmirx_port_open
Description     : This function returns the port handle. All subsequent calls to
                  access the port related functions should use this handle.
                  The port identifier is a contiguous integer sequence from 0.
                  The number of ports supported by the device shall be  known
                  from the device capability structure
Parameters      : device - Device handle
                  id     - Port identifier 0..n.
                  port   - Port handle
Assumptions     :
Limitations     :
Returns         : 0 - No error
*******************************************************************************/
int stm_hdmirx_port_open(stm_hdmirx_device_h device, uint32_t id,
                         stm_hdmirx_port_h *port)
{
  hdmirx_dev_handle_t *dhandle;
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;
  stm_hdmirx_device_s *dev;
  stm_error_code_t err = 0;

  if (NULL == device)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*lock the task */
  stm_hdmirx_sema_wait(inst_access_sem);
  dev = (stm_hdmirx_device_s *) device;
  dhandle = (hdmirx_dev_handle_t *) dev->handle;

  if (id > dhandle->DeviceProp.Max_Ports)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (!is_valid_device_handle(dhandle, device))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle->port_count++;

  /*look for Free Port Handle */
  if (dhandle->PortHandle[id].Handle != HDMIRX_INVALID_PORT_HANDLE)
    {
      *port = dhandle->PortHandle[id].Handle;
      stm_hdmirx_sema_signal(inst_access_sem);
      return 0;
    }

  /* get port handle using device handle */
  dhandle->PortHandle[id].bIsRouteConnected = FALSE;
  port_handle = &dhandle->PortHandle[id];
  port_handle->hDevice = dhandle;
  hport = stm_hdmirx_malloc(sizeof(stm_hdmirx_port_s));
  if (hport == NULL)
    {
      HDMI_PRINT("ERROR:KMALLOC FAILED\n");
      return -EINVAL;
    }

  hport->handle = port_handle;
  *port = (stm_hdmirx_port_h) hport;
  dhandle->PortHandle[id].Handle = (stm_hdmirx_port_h) hport;
  /*Open Port */
  err = hdmirx_open_in_port(port_handle);
  if (0 != err)
    {
      HDMI_PRINT("ERROR:OPEN INPUT PORT FAILED\n");
    }
  /*release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_port_open);
/*******************************************************************************
Name            : stm_hdmirx_port_close
Description     : This function closes all the port related operation and
                  relinquishes the control over port.
Parameters      : port   - Port handle
Assumptions     :
Limitations     :
Returns         : 0 - No error
*******************************************************************************/
int stm_hdmirx_port_close(stm_hdmirx_port_h port)
{
  stm_error_code_t err = 0;
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  /*lock the task */
  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT("ERROR: Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (port_handle->bIsRouteConnected)
    {
      HDMI_PRINT
      ("ERROR:Route is connected to this port, Disconnect Route "
       "and Port\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if(port_handle->hDevice && (--port_handle->hDevice->port_count!=0))
  {
    stm_hdmirx_sema_signal(inst_access_sem);
    return 0;
  }

  err = hdmirx_close_in_port(port_handle);
  if (0 != err)
    {
      HDMI_PRINT("ERROR:Close Poty Failed, error 0x%x\n", err);
    }

  if(stm_hdmirx_free(hport)<0)
    HDMI_PRINT("ERROR:Can't delete hdmi-rx semaphore\n");

  /*Release the Global Semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_port_close);

/*******************************************************************************
Name            : stm_hdmirx_port_get_capability
Description     : This function returns the port capabilities
Parameters      : port -- Port Handle
				  capability -- Pointer to Port Capability Structure
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_port_get_capability(stm_hdmirx_port_h port,
                                   stm_hdmirx_port_capability_t *capability)
{
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == capability)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*lock the task */
  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT("ERROR:Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  /* fill the capability structure of port */
  capability->ddc2bi = port_handle->listen_ddc2bi;
  capability->hpd_support = port_handle->enable_hpd;
  capability->internal_edid = port_handle->internal_edid;
  /*release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_get_capability);

/*******************************************************************************
Name            : stm_hdmirx_port_set_hpd_signal
Description     : This function sets the hpd signal to given state.
Parameters      : port -- Port Handle
				  state -- Signal State
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_set_hpd_signal(stm_hdmirx_port_h port,
                                   stm_hdmirx_port_hpd_state_t state)
{
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;
  stm_error_code_t err = 0;

  if (NULL == port || state < 0 || state > 1)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT("ERROR: Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  /*Action to adjust the Hot Plug Detect Pin Status */
  err = hdmirx_hotplugpin_ctrl(port_handle, state);
  if (0 != err)
    {
      HDMI_PRINT(" ERROR:PORT SET HPDSIGNAL  FAILED\n");
    }
  /*release the control */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_port_set_hpd_signal);

/*******************************************************************************
Name            : stm_hdmirx_port_get_hpd_signal
Description     : This function returns the current state of the hpd signal
Parameters      : port -- Port Handle
				  state -- Pointer to HPD State
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_get_hpd_signal(stm_hdmirx_port_h port,
                                   stm_hdmirx_port_hpd_state_t *state)
{
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }
  if (NULL == state)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }
  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT("ERROR: Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  *state = hdmirx_get_HPD_status(port_handle);
  /*release the control */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_get_hpd_signal);

/*******************************************************************************
Name            : stm_hdmirx_port_get_source_plug_status
Description     : The successful of return of this function means,
				  the automatic source plug detection function is active and
				  all events related to source plug detection are sent to user.
Parameters      : port -- PortHandle
				  status -- pointer to the Source plug status
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_get_source_plug_status(stm_hdmirx_port_h port,
    stm_hdmirx_port_source_plug_status_t
    *status)
{

  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == status)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT(" ERROR:Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  *status = hdmirx_get_src_plug_status(port_handle);
  /*release the control */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_get_source_plug_status);
/*******************************************************************************
Name            : stm_hdmirx_port_init_edid
Description     :  Initialzies EDID data for the port.
				   This function is used in the case of internal EDID emulation.
				   In case of internal EDID configuration HDP signal shall be set high after initialzing  EDID.
Parameters      : port -- PortHandle
				  edid_block -- Pointer to array of  edid blocks
				  number_of_blocks -- Number of blocks
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_init_edid(stm_hdmirx_port_h port,
                              stm_hdmirx_port_edid_block_t *edid_block,
                              uint8_t number_of_blocks)
{
  //TBD
  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_init_edid);

/*******************************************************************************
Name            : stm_hdmirx_port_read_edid_block
Description     : Returns the specified edid block data
Parameters      : port -- Port Handle
				  block_number -- Block number
				  edid_block -- Pointer to the return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_read_edid_block(stm_hdmirx_port_h port,
                                    uint8_t block_number,
                                    stm_hdmirx_port_edid_block_t *edid_block)
{
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;
  stm_error_code_t err;

  if ((NULL == port)
      || ((EDID_BLOCK_SIZE * block_number) > EEPROM_MAX_SIZE))
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == edid_block)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT(" ERROR:Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  err = hdmirx_read_EDID(port_handle, block_number, edid_block);
  if (err != 0)
    {
      HDMI_PRINT("ERROR:Read EDID Failed\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return err;
    }

  /*Exit */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_read_edid_block);
/*******************************************************************************
Name            : stm_hdmirx_port_update_edid_block
Description     : Updates the specified edid block.
Parameters      : port -- Port Handle
				  block_number -- Block number
				  edid_block -- Pointer to the return value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_port_update_edid_block(stm_hdmirx_port_h port,
                                      uint8_t block_number,
                                      stm_hdmirx_port_edid_block_t *edid_block)
{
  stm_hdmirx_port_s *hport;
  hdmirx_port_handle_t *port_handle;
  stm_error_code_t err;

  if ((NULL == port)
      || ((EDID_BLOCK_SIZE * block_number) > EEPROM_MAX_SIZE))
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == edid_block)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Port Handle */
  if (!is_valid_port_handle(port_handle, port))
    {
      HDMI_PRINT("ERROR: Port ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  err = hdmirx_update_EDID(port_handle, block_number, edid_block);
  if (err != 0)
    {
      HDMI_PRINT("ERROR:Update EDID Failed\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return err;
    }

  /*Exit */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_port_update_edid_block);
/*******************************************************************************
Name            : stm_hdmirx_port_set_ddc2bi_msg
Description     : This functions sets the ddc2bi msg to be sent to ddc2bi host device
Parameters      : port -- Port Handle
				  msg --Pointer to msg structures
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int stm_hdmirx_port_set_ddc2bi_msg(stm_hdmirx_port_h port,
                                   stm_hdmirx_port_ddc2bi_msg_t *msg)
{
  return 0;
  //TBD
}
EXPORT_SYMBOL(stm_hdmirx_port_set_ddc2bi_msg);

/*******************************************************************************
Name            : stm_hdmirx_port_get_ddc2bi_msg
Description     : This functions returns the last received ddc2bi message
Parameters      : port -- Port Handle
				  msg -- Pointer to msg structure
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_port_get_ddc2bi_msg(stm_hdmirx_port_h port,
                                   stm_hdmirx_port_ddc2bi_msg_t *msg)
{
  return 0;
  //TBD
}
EXPORT_SYMBOL(stm_hdmirx_port_get_ddc2bi_msg);

/*******************************************************************************
Name            : stm_hdmirx_route_open
Description     : This function returns the route handle. All subsequent calls to
                  access the port related functions should use this handle.
                  The route identifier is a contiguous integer sequence from 0.
                  The number of route supported by the device shall be  known
                  from the device capability structure
Parameters      : device - Device handle
                  id     - Port identifier 0..n.
                  route  - Route handle
Assumptions     :
Limitations     :
Returns         : 0 - No error
*******************************************************************************/
int stm_hdmirx_route_open(stm_hdmirx_device_h device, uint32_t id,
                          stm_hdmirx_route_h *route)
{
  hdmirx_dev_handle_t *dev_handle;
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  stm_hdmirx_device_s *hdmirx;
  stm_error_code_t err = 0;

  if ((NULL == device) || (id >= STHDMIRX_MAX_ROUTE))
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == route)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*lock the task */
  stm_hdmirx_sema_wait(inst_access_sem);
  hdmirx = (stm_hdmirx_device_s *) device;
  dev_handle = (hdmirx_dev_handle_t *) hdmirx->handle;

  if (!is_valid_device_handle(dev_handle, device))
    {
      HDMI_PRINT(" ERROR:Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  dev_handle->route_count++;

  /*look for Free Route Handle */
  if (dev_handle->RouteHandle[id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
    {
      *route = dev_handle->RouteHandle[id].Handle;
      stm_hdmirx_sema_signal(inst_access_sem);
      return 0;
    }

  /* get route handle using device handle */
  dev_handle->RouteHandle[id].bIsPortConnected = FALSE;
  route_handle = &dev_handle->RouteHandle[id];
  route_handle->hDevice = dev_handle;
  hroute = stm_hdmirx_malloc(sizeof(stm_hdmirx_route_s));
  if (hroute == NULL)
    {
      HDMI_PRINT("ERROR:KMALLOC FAILED\n");
      return EINVAL;
    }

  hroute->handle = route_handle;
  *route = (stm_hdmirx_route_h) hroute;
  dev_handle->RouteHandle[id].Handle = (stm_hdmirx_route_h) hroute;
  route_handle->bIsAudioOutPutStarted = FALSE;
  if (0 != err)
    {
      HDMI_PRINT("ERROR:OPEN INPUT ROUTE FAILED\n");
    }

  /*release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_route_open);
/*******************************************************************************
Name            : stm_hdmirx_route_close
Description     : This function closes all the route related operation and
                  relinquishes the control over route.
Parameters      : route   - Route handle
Assumptions     :
Limitations     :
Returns         : 0 - No error
*******************************************************************************/
int stm_hdmirx_route_close(stm_hdmirx_route_h route)
{
  stm_error_code_t err = 0;
  stm_hdmirx_route_s *hroute;
  hdmirx_route_handle_t *route_handle;

  if (NULL == route)
    {
      HDMI_PRINT("ERROR:ST_ERROR_BAD_PARAMETER\n");
      return -EINVAL;
    }

  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  if (!is_valid_route_handle(route_handle, route))
    {
      HDMI_PRINT(" ERROR:Port ...ST_ERROR_INVALID_ROUTE_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (route_handle->bIsPortConnected)
    {
      HDMI_PRINT
      ("ERROR:Port is connected to this Route. First disconnect "
       "the port and route\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if(route_handle->hDevice && (--route_handle->hDevice->route_count!=0))
  {
    stm_hdmirx_sema_signal(inst_access_sem);
    return 0;
  }
  err = close_input_route(route_handle);
  if (0 != err)
    {
      HDMI_PRINT("ERROR: Close Route Failed, error %x\n", err);
    }

  if(stm_hdmirx_free(hroute)<0)
    HDMI_PRINT("ERROR:Can't delete hdmi-rx semaphore\n");

  /*Release the Global Semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_route_close);

/*******************************************************************************
Name            : stm_hdmirx_route_get_capability
Description     : This function returns the route capabilities
Parameters      : route -- Route Handle
				  capability -- Pointer to capability structure
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_capability(stm_hdmirx_route_h route,
                                    stm_hdmirx_route_capability_t *capability)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;

  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == capability)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  capability->video_3d_support = TRUE;
  capability->repeater_fn_support = route_handle->repeater_fn_support;
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_capability);
/*******************************************************************************
Name            : stm_hdmirx_port_connect
Description     : This function Connects the specified port to the specified route.
Parameters      : route  - Route handle
                  port - Port Handle
Assumptions     :
Limitations     :
Returns         : 0 - No error
*******************************************************************************/
int stm_hdmirx_port_connect(stm_hdmirx_port_h port, stm_hdmirx_route_h route)
{
  hdmirx_route_handle_t *route_handle;
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_route_s *hroute;
  stm_hdmirx_port_s *hport;
  hdmirx_dev_handle_t *dhandle;
  stm_error_code_t err = 0;

  if (NULL == route || NULL == port)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  /* look for Valid Route and Port Handle */
  if ((!is_valid_route_handle(route_handle, route))
      || (!is_valid_port_handle(port_handle, port)))
    {
      HDMI_PRINT(" ERROR:Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (port_handle->bIsRouteConnected == TRUE)
    {
      HDMI_PRINT("ERROR:ROUTE ALREADY CONNECTED\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EALREADY;
    }

  if (route_handle->bIsPortConnected == TRUE)
    {
      HDMI_PRINT("ERROR:PORT ALREADY CONNECTED\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EALREADY;
    }

  if (!
      (((route_handle->RouteID == 0)
        && (port_handle->Route_Connectivity_Mask & 0x1))
       || ((route_handle->RouteID == 1)
           && (port_handle->Route_Connectivity_Mask & 0x2))))
    {
      HDMI_PRINT
      ("ERROR:Cannot connect to Specified Route , Check for "
       "Configuration of Port\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -ECONNREFUSED;
    }
  port_handle->bIsRouteConnected = TRUE;
  route_handle->bIsPortConnected = TRUE;
  port_handle->RouteHandle = (stm_hdmirx_route_h) route_handle;
  route_handle->PortHandle = (stm_hdmirx_port_h) port_handle;
  route_handle->PortNo = port_handle->portId;
  /*Open Input Port */
  dhandle=route_handle->hDevice;
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  err = hdmirx_open_in_route(route_handle);
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
  if (port_handle->Ext_Mux != FALSE && port_handle->ext_mux != NULL)
    {
      err = hdmirx_select_port(port_handle);
    }

  if (err != 0)
    {
      HDMI_PRINT("ERROR:Failed to connect Route and Port\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return err;
    }

  /*Start the signal detection */
  route_handle->bIsSignalDetectionStarted = TRUE;
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_port_connect);
/*******************************************************************************
        Name            : stm_hdmirx_port_disconnect
        Description     : This function Connects the specified port to the specified route.
        Parameters      : route  - Route handle
                          port - Port Handle
        Assumptions     :
        Limitations     :
        Returns         : 0 - No error
 *******************************************************************************/
int stm_hdmirx_port_disconnect(stm_hdmirx_port_h port)
{
  hdmirx_route_handle_t *route_handle;
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;
  hdmirx_dev_handle_t *dhandle;

  if (NULL == port)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  stm_hdmirx_sema_wait(inst_access_sem);
  hport = (stm_hdmirx_port_s *) port;
  port_handle = (hdmirx_port_handle_t *) hport->handle;
  if (port_handle->bIsRouteConnected == FALSE)
    {
      HDMI_PRINT("ERROR:ROUTE is not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  route_handle = (hdmirx_route_handle_t *) port_handle->RouteHandle;
  dhandle=route_handle->hDevice;
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  hdmirx_stop_inputprocess(route_handle);
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
  port_handle->bIsRouteConnected = FALSE;
  route_handle->bIsPortConnected = FALSE;
  route_handle->PortNo = INVALID_ID;
  port_handle->RouteHandle = HDMIRX_INVALID_ROUTE_HANDLE;
  route_handle->PortHandle = HDMIRX_INVALID_PORT_HANDLE;
  /*Start the signal detection */
  route_handle->bIsSignalDetectionStarted = FALSE;
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_port_disconnect);

/*******************************************************************************
Name            : stm_hdmirx_route_get_signal_status
Description     : Returns the detected signal status
Parameters      : route -- Route Handle
				  status -- Pointer to return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_signal_status(stm_hdmirx_route_h route,
                                       stm_hdmirx_route_signal_status_t
                                       *status)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == status)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  *status = route_handle->signal_status;
  /*Release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_signal_status);

/*******************************************************************************
Name            : stm_hdmirx_route_get_hdcp_status
Description     : Returns the detected hdcp status
Parameters      : route -- Route Handle
				  status -- Pointer to return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/

int stm_hdmirx_route_get_hdcp_status(stm_hdmirx_route_h route,
                                     stm_hdmirx_route_hdcp_status_t *status)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == status)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  *status = hdmirx_get_HDCP_status(route_handle);
  /*Release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_hdcp_status);

/*******************************************************************************
Name            : stm_hdmirx_route_get_signal_property
Description     : Returns the signal property information
Parameters      : route -- Route Handle
                  property -- Pointer to return value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_signal_property(stm_hdmirx_route_h route,
    stm_hdmirx_signal_property_t *property)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;
  stm_error_code_t err = 0;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == property)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (route_handle->signal_status ==
      STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
    hdmirx_get_signal_prop(route_handle, property);
  else
    HDMI_PRINT("Signal Status :%d \n", route_handle->signal_status);
  /*Release the global semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return (err);
}
EXPORT_SYMBOL(stm_hdmirx_route_get_signal_property);

/*******************************************************************************
Name            : stm_hdmirx_route_get_video_property
Description     : Returns the video property information
Parameters      : route -- Route Handle
				  property -- Pointer to return value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_video_property(stm_hdmirx_route_h route,
                                        stm_hdmirx_video_property_t *property)
{
  stm_error_code_t err = 0;
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == property)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT(" ERROR:Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (route_handle->signal_status ==
      STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
    hdmirx_get_video_prop(route_handle, property);
  else
    HDMI_PRINT("Signal Status :%d \n", route_handle->signal_status);

  /*unlock */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_video_property);

/*******************************************************************************
Name            : stm_hdmirx_route_get_audio_property
Description     : Returns the audio property information
Parameters      : route -- Route Handle
				  property -- Pointer to return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_audio_property(stm_hdmirx_route_h route,
                                        stm_hdmirx_audio_property_t *property)
{

  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;
  stm_error_code_t err = 0;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == property)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT(" ERROR:Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (route_handle->signal_status ==
      STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
    hdmirx_get_audio_prop(route_handle, property);
  else
    HDMI_PRINT("signal status :%d \n", route_handle->signal_status);

  /*unlock */
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_audio_property);
/*******************************************************************************
Name            : stm_hdmirx_route_get_audio_channel_status
Description     : Returns IEC defined channel status information
Parameters      : route -- Route Handle
				  status -- Return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_audio_channel_status(stm_hdmirx_route_h route,
    stm_hdmirx_audio_channel_status_t
    status[])
{

  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;
  stm_error_code_t err = 0;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == status)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  err = hdmirx_get_audio_chan_status(route_handle, status);
  stm_hdmirx_sema_signal(inst_access_sem);

  return err;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_audio_channel_status);
/*******************************************************************************
Name            : stm_hdmirx_route_set_audio_out_enable
Description     : Enables/disables audio output of the route
Parameters      : route -- Route Handle
				  enable -- enable value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_route_h route, bool enable)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  if (enable)
    {
      /*start the audio output */
      route_handle->bIsAudioOutPutStarted = TRUE;
      /*Hardware level action- need to identify */
      hdmirx_audio_soft_mute(route_handle, FALSE);
    }
  else
    {
      /*stop the audio output */
      route_handle->bIsAudioOutPutStarted = FALSE;
      /*Stop the audio output- Hardware Action Level */
      hdmirx_audio_soft_mute(route_handle, TRUE);
    }

  /*unlock */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_set_audio_out_enable);
/*******************************************************************************
Name            : stm_hdmirx_route_get_audio_out_enable
Description     : Returns  the audio output enable status
Parameters      : route -- Route Handle
				  status -- pointer to return value
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/

int stm_hdmirx_route_get_audio_out_enable(stm_hdmirx_route_h route,
    bool *status)
{
  stm_hdmirx_route_s *hroute;
  hdmirx_route_handle_t *route_handle;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == status)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR:Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  *status = route_handle->bIsAudioOutPutStarted;
  /*unlock */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_audio_out_enable);

/*******************************************************************************
Name            : stm_hdmirx_route_get_info
Description     : Returns information specified by the info_type
Parameters      : route -- Route Handle
				  info_type -- Type of information retreived
				  info -- Pointer to return
Assumptions     :
Limitations     :
Returns         :  0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_info(stm_hdmirx_route_h route,
                              stm_hdmirx_information_t info_type, void *info)
{
  hdmirx_route_handle_t *route_handle;
  stm_hdmirx_route_s *hroute;
  hdmirx_dev_handle_t *dhandle;

  /*check the input parameters */
  if (NULL == route || 0 == info_type)
    {
      HDMI_PRINT("ERROR:BAD PARAMETER\n");
      return -EINVAL;
    }

  if (NULL == info)
    {
      HDMI_PRINT("ERROR:Invalid Pointer Address\n");
      return -EFAULT;
    }

  /*obtain the global semaphore */
  stm_hdmirx_sema_wait(inst_access_sem);
  hroute = (stm_hdmirx_route_s *) route;
  route_handle = (hdmirx_route_handle_t *) hroute->handle;
  /* look for Valid Route  */
  if ((!is_valid_route_handle(route_handle, route)))
    {
      HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  dhandle=route_handle->hDevice;
  /* look for Valid device  */
  if(!dhandle || (!is_valid_device_handle(dhandle, dhandle->Handle)))
    {
      HDMI_PRINT("ERROR: Device ...ST_ERROR_INVALID_HANDLE\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }
  /* check is_standby mode */
  stm_hdmirx_sema_wait(dhandle->hdmirx_pm_sema);
  if(dhandle->is_standby==TRUE)
    {
      HDMI_PRINT("ERROR: Device is standby\n");
      stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EAGAIN;
    }
  stm_hdmirx_sema_signal(dhandle->hdmirx_pm_sema);

  if (route_handle->bIsPortConnected == FALSE)
    {
      HDMI_PRINT("ERROR:Port not Connected\n");
      stm_hdmirx_sema_signal(inst_access_sem);
      return -EINVAL;
    }

  /*look for valid handle */
  if (route_handle->Handle != HDMIRX_INVALID_ROUTE_HANDLE)
    {
      sthdmirx_fill_infoevent_data(route_handle, info_type, info);
    }

  /*Release the Global Semaphore */
  stm_hdmirx_sema_signal(inst_access_sem);

  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_info);

/*******************************************************************************
Name            : stm_hdmirx_route_video_connect
Description     :
Parameters      : route - Route Handle
                         video sink  - video sink object
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int stm_hdmirx_route_video_connect(stm_hdmirx_route_h route,
                                   stm_object_h video_sink)
{
  //TBD
  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_video_connect);
/*******************************************************************************
Name            : stm_hdmirx_route_video_disconnect
Description     :
Parameters      : route - Route Handle
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int stm_hdmirx_route_video_disconnect(stm_hdmirx_route_h route)
{
  //TBD
  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_video_disconnect);

/*******************************************************************************
Name            : stm_hdmirx_route_audio_connect
Description     :
Parameters      : route - Route Handle
                        audio_sink - audio sink object
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int stm_hdmirx_route_audio_connect(stm_hdmirx_route_h route,
                                   stm_object_h audio_sink)
{
  //TBD
  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_audio_connect);

/*******************************************************************************
Name            : stm_hdmirx_route_audio_disconnect
Description     :
Parameters      : route - Route Handle
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
int stm_hdmirx_route_audio_disconnect(stm_hdmirx_route_h route)
{
  //TBD
  return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_audio_disconnect);
/*******************************************************************************
Name            : stm_hdmirx_route_set_ctrl
Description     : This function sets the specified control
Parameters      : route - Route Handle
                : ctrl - Control to be set
                : value - Control value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_set_ctrl (stm_hdmirx_route_h route,
                               stm_hdmirx_route_ctrl_t ctrl,
                              uint32_t value)
{
          hdmirx_route_handle_t *route_handle;
          stm_hdmirx_route_s *hroute;

          if (NULL == route) {
              HDMI_PRINT("ERROR:BAD PARAMETER\n");
              return -EINVAL;
          }

          stm_hdmirx_sema_wait(inst_access_sem);
          hroute = (stm_hdmirx_route_s *) route;
          route_handle = (hdmirx_route_handle_t *) hroute->handle;
          /* look for Valid Route  */
          if ((!is_valid_route_handle(route_handle, route))) {
              HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
              stm_hdmirx_sema_signal(inst_access_sem);
              return -EINVAL;
          }

          switch(ctrl) {
          case STM_HDMIRX_ROUTE_CTRL_OPMODE:
            if (value == STM_HDMIRX_ROUTE_OPMODE_DVI)
              route_handle->HdmiRxMode = STM_HDMIRX_ROUTE_OP_MODE_DVI;
            else if (value == STM_HDMIRX_ROUTE_OPMODE_HDMI)
              route_handle->HdmiRxMode = STM_HDMIRX_ROUTE_OP_MODE_HDMI;
            else
              route_handle->HdmiRxMode = STM_HDMIRX_ROUTE_OP_MODE_AUTO;

            // Do we need to call init functions ?
            // We may need to re-initialize the HDMI Rx cell
            sthdmirx_CORE_HDCP_init(route_handle);
            break;
          case STM_HDMIRX_ROUTE_CTRL_AUDIO_OUT_ENABLE:
            //TBD
            break;
          case STM_HDMIRX_ROUTE_CTRL_REPEATER_MODE_ENABLE:
           if (!route_handle->repeater_fn_support) {
             stm_hdmirx_sema_signal(inst_access_sem);
             return -EPERM;
           }
            sthdmirx_CORE_HDCP_set_repeater_mode_enable(route_handle,value);
            break;
          case STM_HDMIRX_ROUTE_CTRL_REPEATER_STATUS_READY:
            sthdmirx_CORE_HDCP_set_repeater_status_ready(route_handle,value);
            break;
          default:
            break;
          }

          stm_hdmirx_sema_signal(inst_access_sem);

          return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_set_ctrl);

/*******************************************************************************
Name            : stm_hdmirx_route_get_ctrl
Description     : This function returns the programmed control value
Parameters      : route - Route Handle
                : ctrl - Control to be retrieved
                : value - Pointer to return value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_get_ctrl (stm_hdmirx_route_h route,
                               stm_hdmirx_route_ctrl_t ctrl,
                               uint32_t *value)
{
        hdmirx_route_handle_t *route_handle;
        stm_hdmirx_route_s *hroute;

        if (NULL == route) {
            HDMI_PRINT("ERROR:BAD PARAMETER\n");
            return -EINVAL;
        }

        if (NULL == value) {
            HDMI_PRINT("ERROR:Invalid Pointer Address\n");
            return -EFAULT;
        }

        stm_hdmirx_sema_wait(inst_access_sem);
        hroute = (stm_hdmirx_route_s *) route;
        route_handle = (hdmirx_route_handle_t *) hroute->handle;
        /* look for Valid Route  */
        if ((!is_valid_route_handle(route_handle, route))) {
            HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
            stm_hdmirx_sema_signal(inst_access_sem);
            return -EINVAL;
        }

        switch(ctrl) {
        case STM_HDMIRX_ROUTE_CTRL_OPMODE:
          if (route_handle->HdmiRxMode == STM_HDMIRX_ROUTE_OP_MODE_DVI)
            *value = STM_HDMIRX_ROUTE_OPMODE_DVI;
          else if (route_handle->HdmiRxMode == STM_HDMIRX_ROUTE_OP_MODE_HDMI)
            *value = STM_HDMIRX_ROUTE_OPMODE_HDMI;
          else
            *value = STM_HDMIRX_ROUTE_OPMODE_AUTO;
          break;
        case STM_HDMIRX_ROUTE_CTRL_AUDIO_OUT_ENABLE:
          //TBD
          break;
        case STM_HDMIRX_ROUTE_CTRL_REPEATER_MODE_ENABLE:
          *value = sthdmirx_CORE_HDCP_is_repeater_enabled(route_handle);
          break;
        case STM_HDMIRX_ROUTE_CTRL_REPEATER_STATUS_READY:
          *value = sthdmirx_CORE_HDCP_is_repeater_ready(route_handle);
          break;
        default:
         stm_hdmirx_sema_signal(inst_access_sem);
         return -EPERM;
        }

        stm_hdmirx_sema_signal(inst_access_sem);

        return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_get_ctrl);

/*******************************************************************************
Name            : stm_hdmirx_route_set_hdcp_downstream_status
Description     : This function is used to set the hdcp downstream status (Bstatus) information
Parameters      : route - Route Handle
                : ctrl - Control to be retrieved
                : value - Pointer to return value
Assumptions     :
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_set_hdcp_downstream_status (stm_hdmirx_route_h route,
                                                 stm_hdmirx_hdcp_downstream_status_t *status)
{
          hdmirx_route_handle_t *route_handle;
          stm_hdmirx_route_s *hroute;

          if (NULL == route) {
              HDMI_PRINT("ERROR:BAD PARAMETER\n");
              return -EINVAL;
          }

          if (NULL == status) {
              HDMI_PRINT("ERROR:Invalid Pointer Address\n");
              return -EFAULT;
          }

          stm_hdmirx_sema_wait(inst_access_sem);
          hroute = (stm_hdmirx_route_s *) route;
          route_handle = (hdmirx_route_handle_t *) hroute->handle;
          /* look for Valid Route  */
          if ((!is_valid_route_handle(route_handle, route))) {
              HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
              stm_hdmirx_sema_signal(inst_access_sem);
              return -EINVAL;
          }

          sthdmirx_CORE_HDCP_set_repeater_downstream_status(route_handle,status);

          stm_hdmirx_sema_signal(inst_access_sem);

          return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_set_hdcp_downstream_status);

/*******************************************************************************
Name            : stm_hdmirx_route_set_hdcp_downstream_ksv_list
Description     : This function is used to set the downstream KSV list which will be read by the source device
Parameters      : route - Route Handle
                : ctrl - Control to be retrieved
                : value - Pointer to return value
Assumptions     : The sire should be a multiple of 5
Limitations     :
Returns         : 0 - No Error
*******************************************************************************/
int stm_hdmirx_route_set_hdcp_downstream_ksv_list (stm_hdmirx_route_h route,
                                                   uint8_t *ksv_list,
                                                   uint32_t size)
{
        hdmirx_route_handle_t *route_handle;
        stm_hdmirx_route_s *hroute;

        if (NULL == route) {
            HDMI_PRINT("ERROR:BAD PARAMETER\n");
            return -EINVAL;
        }

        if (NULL == ksv_list) {
            HDMI_PRINT("ERROR:Invalid Pointer Address\n");
            return -EFAULT;
        }

        stm_hdmirx_sema_wait(inst_access_sem);
        hroute = (stm_hdmirx_route_s *) route;
        route_handle = (hdmirx_route_handle_t *) hroute->handle;
        /* look for Valid Route  */
        if ((!is_valid_route_handle(route_handle, route))) {
            HDMI_PRINT("ERROR: Route ...ST_ERROR_INVALID_HANDLE\n");
            stm_hdmirx_sema_signal(inst_access_sem);
            return -EINVAL;
        }

        sthdmirx_CORE_HDCP_set_downstream_ksv_list(route_handle, ksv_list, size);

        stm_hdmirx_sema_signal(inst_access_sem);

        return 0;
}
EXPORT_SYMBOL(stm_hdmirx_route_set_hdcp_downstream_ksv_list);

/*******************************************************************************
Name            : get_dev_id
Description     : Check the Device Registry
Parameters      : DeviceId - Device identifier 0..n.
Assumptions     :
Limitations     :
Returns         : Nothing
*******************************************************************************/
static sthdmirx_dev_prop_t *get_dev_id(uint32_t *DeviceId)
{
  U8 idx;
  sthdmirx_dev_prop_t *wanted_device = NULL;

  for (idx = 0; idx < STHDMIRX_MAX_DEVICE; idx++)
    {
      if (dev_handle[idx].DeviceProp.DeviceId == *DeviceId)
        wanted_device = &dev_handle[idx].DeviceProp;
    }
  return (wanted_device);
}
