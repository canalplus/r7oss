/***********************************************************************
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "vxi.h"
#include "vxi/generic/VxiDevice.h"
#include "stm_vxi.h"
#include "../soc/stiH416/stiH416reg.h"

#define VXI_656_EN       0x1
#define INPUT_COLOR_MODE 0x1<<11
#define VXI_656_INT_EN   0x1<<9

CVxi::CVxi(const char *name, uint32_t id, const CVxiDevice *pVxiDevice, stm_vxi_platform_data_t *pPlatformData)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_name                             = name;
  m_ID                               = id;
  m_lock                             = 0;
  m_pVxiDevice                       = pVxiDevice;
  m_pDevRegs                         = (uint32_t*)pVxiDevice->GetCtrlRegisterBase();
  m_platformData                     = pPlatformData;
  m_uCurrentInterface                = 0;  /* default interface */
  m_uCurrentInput                    = 0;  /* default input*/
  m_ulCapabilities.number_of_inputs  = 3;
  m_inputCapabilities[0].interface_type = STM_VXI_INTERFACE_TYPE_CCIR656;
  m_inputCapabilities[0].bits_per_pixel = 10;
  m_inputCapabilities[0].vbi_support = false;
  m_inputCapabilities[1].interface_type = STM_VXI_INTERFACE_TYPE_SMPTE_274M_296M;
  m_inputCapabilities[1].bits_per_pixel = 10;
  m_inputCapabilities[1].vbi_support = false;
  m_inputCapabilities[2].interface_type = STM_VXI_INTERFACE_TYPE_RGB_YUV_444;
  m_inputCapabilities[2].bits_per_pixel = 10;
  m_inputCapabilities[2].vbi_support = false;
  m_bIsSuspended = false;
  m_bIsFreezed = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CVxi::~CVxi(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_delete_resource_lock(m_lock);

  TRC( TRC_ID_MAIN_INFO, "Vxi %p Destroyed", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

int CVxi::GetCapability(stm_vxi_capability_t *capability)
{
  capability->number_of_inputs = m_ulCapabilities.number_of_inputs;
  return 0;
}

int CVxi::GetInputCapability(stm_vxi_input_capability_t *inputCapability)
{
  if(m_uCurrentInterface<3)
  {
    *inputCapability = m_inputCapabilities[m_uCurrentInterface];
  }
  else
    TRC( TRC_ID_MAIN_INFO, "Vxi %p Interface not set", this );
  return 0;
}

int CVxi::SetControl(stm_vxi_ctrl_t ctrl, uint32_t value)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if(m_bIsSuspended)
    return -EAGAIN;

  if((ctrl == STM_VXI_CTRL_SELECT_INPUT)&&(value < m_platformData->board->num_inputs))
  {
    switch(m_platformData->board->inputs[value].interface_type)
    {
        case STM_VXI_INTERFACE_TYPE_CCIR656:
          WriteVXIReg(VXI_CTRL, INPUT_COLOR_MODE|VXI_656_EN|VXI_656_INT_EN|m_platformData->board->inputs[value].bits_per_component);
          WriteVXIReg(VXI_DATA_TWIST_SWAP_656, m_platformData->board->inputs[value].swap656);
          TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_CTRL       : 0x%x, 0x%x", VXI_CTRL, INPUT_COLOR_MODE|VXI_656_EN|m_platformData->board->inputs[value].bits_per_component );
          break;
        case STM_VXI_INTERFACE_TYPE_SMPTE_274M_296M:
          WriteVXIReg(VXI_CTRL, INPUT_COLOR_MODE|m_platformData->board->inputs[value].bits_per_component);
          TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_CTRL       : 0x%x, 0x%x", VXI_CTRL, INPUT_COLOR_MODE|m_platformData->board->inputs[value].bits_per_component );
          break;
        case STM_VXI_INTERFACE_TYPE_RGB_YUV_444:
          WriteVXIReg(VXI_CTRL, m_platformData->board->inputs[value].bits_per_component);
          TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_CTRL       : 0x%x, 0x%x", VXI_CTRL, m_platformData->board->inputs[value].bits_per_component );
          break;
        default:
          TRC( TRC_ID_MAIN_INFO, "Vxi %p Invalid interface %d", this, value );
         break;
    }
    m_uCurrentInput = value;
    m_uCurrentInterface = m_platformData->board->inputs[value].interface_type;
    if((m_platformData->board->inputs[value].interface_type <= STM_VXI_INTERFACE_TYPE_RGB_YUV_444)
     &&(m_platformData->board->inputs[value].interface_type != STM_VXI_INTERFACE_TYPE_CCIR656))
    {  
      WriteVXIReg(VXI_DATA_SWAP_1, m_platformData->board->inputs[value].swap.data_swap1);
      TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_DATA_SWAP_1: 0x%x, 0x%x", VXI_DATA_SWAP_1, m_platformData->board->inputs[value].swap.data_swap1 );
      WriteVXIReg(VXI_DATA_SWAP_2, m_platformData->board->inputs[value].swap.data_swap2);
      TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_DATA_SWAP_2: 0x%x, 0x%x", VXI_DATA_SWAP_2, m_platformData->board->inputs[value].swap.data_swap2 );
      WriteVXIReg(VXI_DATA_SWAP_3, m_platformData->board->inputs[value].swap.data_swap3);
      TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_DATA_SWAP_3: 0x%x, 0x%x", VXI_DATA_SWAP_3, m_platformData->board->inputs[value].swap.data_swap3 );
      WriteVXIReg(VXI_DATA_SWAP_4, m_platformData->board->inputs[value].swap.data_swap4);
      TRC( TRC_ID_MAIN_INFO, "WriteVXIReg: VXI_DATA_SWAP_4: 0x%x, 0x%x", VXI_DATA_SWAP_4, m_platformData->board->inputs[value].swap.data_swap4 );
    }
  }
  else
  {
  TRC( TRC_ID_MAIN_INFO, "Vxi %p Invalid input %d", this, value );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return 0;
}

int CVxi::GetControl(stm_vxi_ctrl_t ctrl, uint32_t *value)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if((ctrl == STM_VXI_CTRL_SELECT_INPUT))
  {
  *value = m_uCurrentInterface;
  }
  return 0;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CVxi::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_lock = vibe_os_create_resource_lock();
  if(!m_lock)
  {
    TRC( TRC_ID_ERROR, "failed to allocate resource lock" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Vxi %p Created", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

int CVxi::Freeze()
{
  int  res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  res = Suspend();

  m_bIsFreezed = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}

int CVxi::Suspend()
{
  int  res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return res;

  WriteVXIReg(VXI_RESET, VXI_SOFT_RESET);
  WriteVXIReg(VXIINST_SOFT_RESETS, VXI_IFM_RESET);
  WriteVXIReg(VXI_CLK_CONTROL, VXI_CLK_DISABLE);
  WriteVXIReg(VXI_IFM_CLK_CONTROL, VXI_IFM_CLK_DISABLE);
  m_bIsSuspended = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}

int CVxi::Resume()
{
  int  res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return res;

  WriteVXIReg(VXI_CLK_CONTROL, 0);
  WriteVXIReg(VXI_IFM_CLK_CONTROL, 0);
  WriteVXIReg(VXI_RESET, 0);
  WriteVXIReg(VXIINST_SOFT_RESETS, 0);

  m_bIsSuspended = false;
  if(m_bIsFreezed)
  {
    res = SetControl(STM_VXI_CTRL_SELECT_INPUT, m_uCurrentInput);
    m_bIsFreezed = false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}

//////////////////////////////////////////////////////////////////////////////
// C Device interface

#if defined(__cplusplus)
extern "C" {
#endif

int stm_vxi_open(const uint32_t id, stm_vxi_h *vxiHandle)
{
  int rc = stm_vxi_device_open(id, vxiHandle);

  if ( rc == 0 )
    {
      CVxi *pVxi = (CVxi *)((*vxiHandle)->handle);
      TRC( TRC_ID_API_VXI, "id : %u, vxi : %s", id, pVxi->GetName() );
    }

  return rc;
}

int stm_vxi_close(const stm_vxi_h vxiHandle)
{
  if(!stm_vxi_is_handle_valid(vxiHandle))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxiHandle->handle);
  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  return stm_vxi_device_close(vxiHandle);
}

int stm_vxi_get_capability(stm_vxi_h vxi, stm_vxi_capability_t *capability)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  res = pVxi->GetCapability(capability);

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_get_input_capability(stm_vxi_h vxi, stm_vxi_input_capability_t *capability)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  res = pVxi->GetInputCapability(capability);

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_set_ctrl(stm_vxi_h vxi, stm_vxi_ctrl_t ctrl, uint32_t value)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s, value : %u", pVxi->GetName(), value );
  res = pVxi->SetControl(ctrl, value);

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_get_ctrl(stm_vxi_h vxi, stm_vxi_ctrl_t ctrl, uint32_t *value)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  res = pVxi->GetControl(ctrl, value);
  TRC( TRC_ID_API_VXI, "vxi : %s, value : %u", pVxi->GetName(), *value );

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_freeze(stm_vxi_h vxi)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  res = pVxi->Freeze();

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_suspend(stm_vxi_h vxi)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  res = pVxi->Suspend();

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

int stm_vxi_resume(stm_vxi_h vxi)
{
  int res = 0;

  if(!stm_vxi_is_handle_valid(vxi))
    return -ENODEV;

  CVxi *pVxi = (CVxi *)(vxi->handle);

  if(vibe_os_down_semaphore(vxi->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_VXI, "vxi : %s", pVxi->GetName() );
  res = pVxi->Resume();

  vibe_os_up_semaphore(vxi->lock);
  return res;
}

#if defined(__cplusplus)
} // extern "C"
#endif
