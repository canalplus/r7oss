/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirx/hdmirx_utils.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

#include <linux/kernel.h>
#include <linux/list.h>

/* Local Includes -------------------------------------------------*/

#include "hdmirx_utils.h"

void RetrieveDevicemapData(hdmirx_I2C_info_handle_t hChip, uint8_t *Dest,
                           uint32_t FirstRegIndex, int Size);
void UpdateDevicemapData(hdmirx_I2C_info_handle_t hChip, uint8_t *Src,
                         uint32_t FirstRegIndex, int Size);

static hdmirx_I2C_error_t GetRegIndex(hdmirx_I2C_info_handle_t hChip,
                                      uint16_t RegId, uint32_t NbRegs,
                                      int32_t *RegIndex);

/*
* Name: hdmirx_select_port()
*
* Description: Selects one of the HDMI port
*
*/
int hdmirx_select_port(hdmirx_port_handle_t *Handle)
{
  int Error = 0;
  hdmirx_port_handle_t *Port_Handle;
  Port_Handle = (hdmirx_port_handle_t *) Handle;
  Error = Port_Handle->ext_mux(Port_Handle->portId);
  return Error;
}

/*
* Name: chip_open()
*
* Description: get adapter for demod i2c
*
*/
hdmirx_i2c_adapter chip_open(uint32_t bus)
{
  int ret = 0;
  hdmirx_i2c_adapter dev_i2c;

  dev_i2c = i2c_get_adapter(bus);
  if (dev_i2c == NULL)
    {
      printk(KERN_ERR
             "%s:i2c_get_adapter failed for hdmirx i2c bus %d \n",
             __func__, bus);
      ret = -ENODEV;
    }
  return dev_i2c;
}

/*
* Name: chip_set_one_register()
*
* Description: Writes Value to the register specified by RegAddr
*
*/
hdmirx_I2C_error_t chip_set_one_register(hdmirx_I2C_info_handle_t hChip,
    uint16_t RegAddr, uint8_t Value)
{
  int32_t regindex;
  hdmirx_I2C_error_t error;

  error = GetRegIndex(hChip, RegAddr, 1, &regindex);
  if (error != CHIPERR_NO_ERROR)
    return error;
  if (hChip->Abort)
    return CHIPERR_NO_ERROR;
  hChip->pRegMapImage[regindex].Value = Value;
  error = chip_set_registers(hChip, RegAddr, 1);
  return error;
}

/*
* Name: chip_set_registers()
*
* Description: write  Value to the registers specified by RegAddr
*
*/
hdmirx_I2C_error_t chip_set_registers(hdmirx_I2C_info_handle_t hChip,
                                      uint32_t FirstReg, int NbRegs)
{
  return (chip_set_registersI2C(hChip, FirstReg, NbRegs));
}

/*
* Name: chip_set_registersI2C()
*
* Description: write  Value to the registers specified by RegAddr
*
*/
hdmirx_I2C_error_t chip_set_registersI2C(hdmirx_I2C_info_handle_t hChip,
    uint32_t FirstReg, int NbRegs)
{
  int32_t first_reg_index;
  unsigned char buffer[NbRegs + 3];
  hdmirx_i2c_msg msg[6];
  uint32_t msgindex = 0;
  uint32_t sub_address_bytes = 0;
  uint32_t retry_count = 0;
  hdmirx_I2C_error_t error = CHIPERR_NO_ERROR;
  int ret = -1;
  uint32_t msg_cnt;

  error = GetRegIndex(hChip, FirstReg, NbRegs, &first_reg_index);
  if (error != CHIPERR_NO_ERROR)
    error = CHIPERR_INVALID_REG_ID;
  if (hChip->Abort)
    return CHIPERR_NO_ERROR;
  memset(msg, 0, (sizeof(hdmirx_i2c_msg) * 6));
  prepare_sub_addr_header(hChip->ChipMode, WRITING, FirstReg, buffer,
                          &sub_address_bytes);

  RetrieveDevicemapData(hChip, &buffer[sub_address_bytes],
                        first_reg_index, NbRegs);

  /* Message to write the actual data onto the device */
#ifdef CONFIG_ARCH_STI
  fill_msg(msg, hChip->I2cAddr, &msgindex, sub_address_bytes + NbRegs,
           buffer, WRITING);
  for (msg_cnt=0; msg_cnt < msgindex; msg_cnt++)
  {
    do
    {
      ret = i2c_transfer(hChip->dev_i2c, &msg[msg_cnt], 1);
      if (ret != msgindex)
        {
          if (retry_count == 2)
            {
                printk(KERN_ERR
                       "%s:WR msg[%d] addr=0x%x len=0x%d Err= %d\n",
                       __func__, msg_cnt,
                       msg[msg_cnt].addr,
                       msg[msg_cnt].len, ret);
            }
          hChip->Error = CHIPERR_I2C_NO_ACK;
        }
      retry_count++;
    }
    while ((ret != msgindex) && (retry_count <= 2));
  }
#else
  fill_msg(msg, hChip->I2cAddr, &msgindex, sub_address_bytes + NbRegs,
           buffer, WRITING, true);
  do
    {
      ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
      if (ret != msgindex)
        {
          if (retry_count == 2)
            {
              for (msg_cnt = 0; msg_cnt < msgindex; msg_cnt++)
                printk(KERN_ERR
                       "%s:WR msg[%d] addr=0x%x len=0x%d Err= %d\n",
                       __func__, msg_cnt,
                       msg[msg_cnt].addr,
                       msg[msg_cnt].len, ret);
            }
          hChip->Error = CHIPERR_I2C_NO_ACK;
        }
      retry_count++;
    }
  while ((ret != msgindex) && (retry_count <= 2));

#endif
  return error;
}

/*
* Name: chip_get_register_index()
*
* Description: Get the index of a register from the pRegMapImage table
*
*/
int32_t chip_get_register_index(hdmirx_I2C_info_handle_t hChip, uint16_t RegId)
{
  int32_t regIndex = -1, reg = 0;

  /* int32_t top, bottom,mid; to be used for binary search */
  while (reg < hChip->NbRegs)
    {

      if (hChip->pRegMapImage[reg].Addr == RegId)
        {
          regIndex = reg;
          break;
        }
      reg++;
    }

  return regIndex;
}

void chip_abort(hdmirx_I2C_info_handle_t hChip, bool Abort)
{
  hChip->Abort = Abort;
}

/*
* Name: ChipWait_Or_Abort()
*
* Description: wait for n ms or abort if requested
*
*/
void chip_wait_or_abort(hdmirx_I2C_info_handle_t hChip, uint32_t delay_ms)
{
  uint32_t i = 0;

  while ((hChip->Abort == false) && (i++ < (delay_ms / 8)))
    WAIT_N_MS(8);
  i = 0;
  while ((hChip->Abort == false) && (i++ < (delay_ms % 8)))
    WAIT_1_MS;
}

void RetrieveDevicemapData(hdmirx_I2C_info_handle_t hChip, uint8_t *Dest,
                           uint32_t FirstRegIndex, int Size)
{
  uint8_t index;

  for (index = 0; index < Size; index++)
    Dest[index] = hChip->pRegMapImage[FirstRegIndex + index].Value;
}

void UpdateDevicemapData(hdmirx_I2C_info_handle_t hChip, uint8_t *Src,
                         uint32_t FirstRegIndex, int Size)
{
  uint8_t index;

  for (index = 0; index < Size; index++)
    hChip->pRegMapImage[FirstRegIndex + index].Value = Src[index];

}

static hdmirx_I2C_error_t GetRegIndex(hdmirx_I2C_info_handle_t hChip,
                                      uint16_t RegId, uint32_t NbRegs,
                                      int32_t *RegIndex)
{
  hdmirx_I2C_error_t error = CHIPERR_NO_ERROR;

  *RegIndex = chip_get_register_index(hChip, RegId);
  if (((*RegIndex >= 0)
       && ((*RegIndex + NbRegs - 1) < hChip->NbRegs)) == false)
    {
      hChip->Error = CHIPERR_INVALID_REG_ID;
    }
  return error;
}

void prepare_sub_addr_header(STCHIP_Mode_t Mode, context_t Context,
                             uint16_t SubAddress, uint8_t *Buffer,
                             uint32_t *SubAddressByteCount)
{

  switch (Mode)
    {
    case STCHIP_MODE_SUBADR_16:
      Buffer[0] = MSB(SubAddress);
      Buffer[1] = LSB(SubAddress);
      *SubAddressByteCount = 2;
      break;
    case STCHIP_MODE_MXL:
      if (Context == WRITING)
        {
          Buffer[0] = LSB(SubAddress);
          *SubAddressByteCount = 1;
        }
      else
        {
          Buffer[0] = 0xFB;
          Buffer[1] = LSB(SubAddress);
          *SubAddressByteCount = 2;
        }
      break;

    case STCHIP_MODE_SUBADR_8:
    case STCHIP_MODE_SUBADR_8_NS:
    case STCHIP_MODE_SUBADR_8_NS_MICROTUNE:
      Buffer[0] = LSB(SubAddress);
      *SubAddressByteCount = 1;
      break;

    case STCHIP_MODE_NOSUBADR:
      *SubAddressByteCount = 0;
      break;

    case STCHIP_MODE_NOSUBADR_RD:
      if (Context == WRITING)
        {
          Buffer[0] = LSB(SubAddress);
          *SubAddressByteCount = 1;
        }
      else
        *SubAddressByteCount = 0;
      break;

    case STCHIP_MODE_NOSUBADR_SR:
      Buffer[0] = 0x09;
      Buffer[1] = 0xC0;
      *SubAddressByteCount = 2;
      break;
    case STCHIP_MODE_SUBADR_8_SR:
      Buffer[0] = 0x09;
      Buffer[1] = 0xc0;
      Buffer[2] = LSB(SubAddress);
      *SubAddressByteCount = 3;
      break;

    default:
      Buffer[0] = MSB(SubAddress);
      Buffer[1] = LSB(SubAddress);
      *SubAddressByteCount = 2;
    }

}
#ifdef CONFIG_ARCH_STI
void fill_msg(hdmirx_i2c_msg *msg, uint32_t addr, uint32_t *MsgIndex,
              uint32_t BufferLen, uint8_t *Buffer_p, context_t Context)
{

  msg[*MsgIndex].addr = addr;
  msg[*MsgIndex].flags = Context;
  msg[*MsgIndex].buf = Buffer_p;
  msg[*MsgIndex].len = BufferLen;
  (*MsgIndex)++;
}
#else
void fill_msg(hdmirx_i2c_msg *msg, uint32_t addr, uint32_t *MsgIndex,
              uint32_t BufferLen, uint8_t *Buffer_p, context_t Context,
              bool UseStop)
{

  msg[*MsgIndex].addr = addr;
  msg[*MsgIndex].flags = Context;
  if (!UseStop)
    {
      msg[*MsgIndex].flags = Context;
      /*msg[*MsgIndex].flags |= 0;-I2C_M_NOSTOP undeclard error? */
    }
  else
    {
      msg[*MsgIndex].flags |= I2C_M_NOREPSTART;
      /* STI2C sets this false
       *for last message;not done here */
    }
  msg[*MsgIndex].buf = Buffer_p;
  msg[*MsgIndex].len = BufferLen;
  (*MsgIndex)++;
}
#endif
