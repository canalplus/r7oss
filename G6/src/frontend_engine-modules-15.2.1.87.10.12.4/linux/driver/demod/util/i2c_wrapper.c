/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : i2c_wrapper.c
Author :           Shobhit

Io wrapper for LLA to access the hw registers

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include "stfe_utilities.h"
#include <stm_fe_os.h>
#include "i2c_wrapper.h"
#include "gen_macros.h"

static void RetrieveDevicemapData(STCHIP_Handle_t hChip, uint8_t *Dest,
				uint32_t FirstRegIndex, int Size);
static void UpdateDevicemapData(STCHIP_Handle_t hChip, uint8_t *Src,
			 uint32_t FirstRegIndex, int Size);

static STCHIP_Error_t GetRegIndex(STCHIP_Handle_t hChip, uint16_t RegId,
				  uint32_t NbRegs, int32_t *RegIndex);

#ifdef CONFIG_ARCH_STI
#define STMFE_I2C_USE_STOP 0xff00 /* Used for quick sti port*/
#endif
/*
 * Name: chip_open()
 *
 * Description: get adapter for demod i2c
 *
 */
fe_i2c_adapter chip_open(uint32_t bus)
{
	int ret = 0;
	fe_i2c_adapter dev_i2c;

	dev_i2c = i2c_get_adapter(bus);
	if (!dev_i2c) {
		pr_err("%s: i2c_get_adapter failed for demod/lnb i2c bus %d\n",
			__func__, bus);
		ret = -ENODEV;
	}
	return dev_i2c;
}
EXPORT_SYMBOL(chip_open);

/*
 * Name: stm_fe_check_i2c_device_id()
 *
 * Description: check the i2c device id with i2c base address(in DT mode)
 *
 */
int stm_fe_check_i2c_device_id(uint32_t *base_addr, int num)
{
	int ret = -1;
	struct i2c_adapter *adap;
	struct platform_device *pdev;

	adap = i2c_get_adapter(num);
	if (!adap) {
		pr_err("%s: incorrect i2c dev id %d\n", __func__, num);
		return ret;
	}
	pdev = (struct platform_device *)to_platform_device(adap->dev.parent);
	if (!pdev) {
		pr_err("%s: pdev NULL (i2c id:%d)\n", __func__, num);
		i2c_put_adapter(adap);
		return ret;
	}
	if (pdev->resource->start != (U32)base_addr) {
		pr_err("%s: base addr not matched %u", __func__,
							   (uint32_t)base_addr);
		i2c_put_adapter(adap);
		return ret;
	}
	i2c_put_adapter(adap);
	return 0;
}
EXPORT_SYMBOL(stm_fe_check_i2c_device_id);

/*
 * Name: ChipSetOneRegister()
 *
 * Description: Writes Value to the register specified by RegAddr
 *
 */
STCHIP_Error_t ChipSetOneRegister(STCHIP_Handle_t hChip, uint16_t RegAddr,
				  uint8_t Value)
{
	int32_t regindex;
	STCHIP_Error_t err;

	err = GetRegIndex(hChip, RegAddr, 1, &regindex);
	if (err != CHIPERR_NO_ERROR)
		return err;
	if (hChip->Abort)
		return CHIPERR_NO_ERROR;
	hChip->pRegMapImage[regindex].Value = Value;
	err = ChipSetRegisters(hChip, RegAddr, 1);
	return err;
}
EXPORT_SYMBOL(ChipSetOneRegister);
/*
 * Name: ChipGetOneRegister()
 *
 * Description: read Value of the register specified by RegAddr
 *
 */
uint8_t ChipGetOneRegister(STCHIP_Handle_t hChip, uint32_t RegAddress)
{
	uint8_t data = 0xFF;
	int32_t regindex = -1;

	STCHIP_Error_t err = CHIPERR_NO_ERROR;

	err = GetRegIndex(hChip, RegAddress, 1, &regindex);
	if (err != CHIPERR_NO_ERROR)
		return err;
	if (hChip->Abort)
		return CHIPERR_NO_ERROR;
	err = ChipGetRegisters(hChip, RegAddress, 1);

	if (err == CHIPERR_NO_ERROR)
		data = hChip->pRegMapImage[regindex].Value;

	return data;
}
EXPORT_SYMBOL(ChipGetOneRegister);

/*
 * Name: ChipSetRegisters()
 *
 * Description: write  Value to the registers specified by RegAddr
 *
 */
STCHIP_Error_t ChipSetRegisters(STCHIP_Handle_t hChip, uint32_t FirstReg,
				int NbRegs)
{
	return ChipSetRegistersI2C(hChip, FirstReg, NbRegs);
}
EXPORT_SYMBOL(ChipSetRegisters);

/*
 * Name: ChipGetRegisters()
 *
 * Description: read  Value from the registers
 *
 */
STCHIP_Error_t ChipGetRegisters(STCHIP_Handle_t hChip, uint32_t FirstRegAddress,
				int8_t NbRegs)
{
	return ChipGetRegistersI2C(hChip, FirstRegAddress, NbRegs);
}
EXPORT_SYMBOL(ChipGetRegisters);

#ifdef CONFIG_ARCH_STI
void fe_i2c_create_xfer_grp(fe_i2c_msg *msg, uint32_t start, uint32_t total,
							uint32_t *xfer_size)
{
	uint32_t i;
	uint32_t msg_cnt = 1;
	for (i = start; i < total; i++) {
		if (!(msg[i].flags & STMFE_I2C_USE_STOP)) {
			msg[i].flags &=  ~STMFE_I2C_USE_STOP;
			if (i >= total) {
				break;
			} else {
				msg_cnt++;
				continue;
			}
		} else {
			msg[i].flags &= ~STMFE_I2C_USE_STOP;
			break;
		}
	}
	*xfer_size = msg_cnt;

	return;
}
void fe_i2c_xfer_msg(STCHIP_Handle_t hChip, fe_i2c_msg *msg, uint32_t num_msg)
{
	uint32_t i = 0, j;
	uint32_t retry_count = 0;
	uint32_t msg_cnt = 0;
	int ret = -1;

	do {
		fe_i2c_create_xfer_grp(&msg[0], i, num_msg, &msg_cnt);
		retry_count = 0;

		do {
			ret = i2c_transfer(hChip->dev_i2c, &msg[i], msg_cnt);
			if (ret != msg_cnt && retry_count == 2) {
				for (j = i ; j < (i + msg_cnt) ; j++)
					pr_err("%s: msg[%d]addr = 0x%x len ="
							"%d Err=%d\n" ,
							__func__, j,
							msg[j].addr,
							msg[j].len, ret);
				hChip->Error = CHIPERR_I2C_NO_ACK;
			}
			retry_count++;
		} while ((ret != msg_cnt) && (retry_count <= 2));

		i += msg_cnt;
	} while (i < num_msg);

}
#endif
/*
 * Name: ChipSetRegistersI2C()
 *
 * Description: write  Value to the registers specified by RegAddr
 *
 */
STCHIP_Error_t ChipSetRegistersI2C(STCHIP_Handle_t hChip, uint32_t FirstReg,
					int NbRegs)
{
#ifdef CONFIG_ARCH_STI
	int32_t first_reg_index;
	unsigned char buffer[NbRegs + 3];
	unsigned char repeater_on_buffer[5];
	unsigned char repeater_off_buffer[5];
	fe_i2c_msg msg[10];
	uint32_t num_msg = 0;
	uint32_t sub_address_bytes = 0;

	if (hChip->Abort)
		return CHIPERR_NO_ERROR;

	hChip->Error = GetRegIndex(hChip, FirstReg, NbRegs, &first_reg_index);
	if (hChip->Error)
		return hChip->Error;

	memset(msg, 0, (sizeof(fe_i2c_msg) * 10));

	if (hChip->IORoute == DEMOD_IO_REPEATER) {
		FillRepMsg(hChip, msg, &num_msg, repeater_on_buffer,
				REPEATER_ON);
	}

	PrepareSubAddressHeader(hChip->ChipMode, WRITING, FirstReg, buffer,
				&sub_address_bytes);

	RetrieveDevicemapData(hChip, &buffer[sub_address_bytes],
				  first_reg_index, NbRegs);

	/* Message to write the actual data onto the device */
	FillMsg(msg, hChip->I2cAddr, &num_msg, sub_address_bytes + NbRegs,
		buffer, WRITING, true);

	if ((hChip->IORoute == DEMOD_IO_REPEATER)
		&& (hChip->RepeaterHost->IsAutoRepeaterOffEnable == false)) {
		FillRepMsg(hChip, msg, &num_msg, repeater_off_buffer,
		REPEATER_OFF);
	}

	fe_i2c_xfer_msg(hChip, &msg[0], num_msg);

	return hChip->Error;
#else
	int32_t first_reg_index;
	unsigned char buffer[NbRegs + 3];
	unsigned char repeater_on_buffer[5];
	unsigned char repeater_off_buffer[5];
	fe_i2c_msg msg[6];
	uint32_t msgindex = 0;
	uint32_t sub_address_bytes = 0;
	uint32_t retry_count = 0;
	int ret = -1;
	uint32_t msg_cnt;

	if (hChip->Abort)
		return CHIPERR_NO_ERROR;

	hChip->Error = GetRegIndex(hChip, FirstReg, NbRegs, &first_reg_index);
	if (hChip->Error)
		return hChip->Error;

	memset(msg, 0, (sizeof(fe_i2c_msg) * 6));

	if (hChip->IORoute == DEMOD_IO_REPEATER)
		FillRepMsg(hChip, msg, &msgindex, repeater_on_buffer,
				REPEATER_ON);

	PrepareSubAddressHeader(hChip->ChipMode, WRITING, FirstReg, buffer,
				&sub_address_bytes);

	RetrieveDevicemapData(hChip, &buffer[sub_address_bytes],
				  first_reg_index, NbRegs);

	/* Message to write the actual data onto the device */
	FillMsg(msg, hChip->I2cAddr, &msgindex, sub_address_bytes + NbRegs,
		buffer, WRITING, true);

	if ((hChip->IORoute == DEMOD_IO_REPEATER)
		&& (hChip->RepeaterHost->IsAutoRepeaterOffEnable == false))
		FillRepMsg(hChip, msg, &msgindex, repeater_off_buffer,
		REPEATER_OFF);

	do {
		ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
		if (ret != msgindex && retry_count == 2) {
			for (msg_cnt = 0 ; msg_cnt < msgindex ; msg_cnt++)
				pr_err(
				  "%s:WR msg[%d] addr = 0x%x len = %d Err= %d\n"
				  , __func__, msg_cnt, msg[msg_cnt].addr,
				  msg[msg_cnt].len, ret);
			hChip->Error = CHIPERR_I2C_NO_ACK;
		}
		retry_count++;
	} while ((ret != msgindex) && (retry_count <= 2));

	return hChip->Error;
#endif
}

/*
 * Name: ChipGetRegistersI2C()
 *
 * Description: read  Value from the specific field of register
 *
 */
STCHIP_Error_t ChipGetRegistersI2C(STCHIP_Handle_t hChip,
					uint32_t FirstRegAddress, int NbRegs)
{
#ifdef CONFIG_ARCH_STI
	uint8_t buffer[NbRegs + 3];
	uint8_t repeater_on_buffer[5];
	uint8_t repeater_off_buffer[5];
	int32_t first_reg_index;
	fe_i2c_msg msg[10];
	uint32_t num_msg = 0;
	uint32_t sub_address_bytes = 0;
	bool stop_after_subaddress = true;

	memset(msg, 0, (sizeof(fe_i2c_msg) * 10));

	if (hChip->Abort)
		return CHIPERR_NO_ERROR;

	hChip->Error = GetRegIndex(hChip, FirstRegAddress, NbRegs,
							&first_reg_index);
	if (hChip->Error)
		return hChip->Error;

	if (hChip->IORoute == DEMOD_IO_REPEATER) {
		FillRepMsg(hChip, msg, &num_msg, repeater_on_buffer,
				REPEATER_ON);
	}

	PrepareSubAddressHeader(hChip->ChipMode, READING, FirstRegAddress,
				buffer, &sub_address_bytes);

	if ((hChip->ChipMode == STCHIP_MODE_SUBADR_8_NS)
		|| (hChip->ChipMode == STCHIP_MODE_NOSUBADR_SR))
		stop_after_subaddress = false;

	if (sub_address_bytes) {

		FillMsg(msg, hChip->I2cAddr, &num_msg, sub_address_bytes,
			buffer, WRITING, stop_after_subaddress);

		if ((hChip->IORoute == DEMOD_IO_REPEATER)
		      && (hChip->RepeaterHost->IsAutoRepeaterOffEnable == true)
				&& (stop_after_subaddress == true)) {
			FillRepMsg(hChip, msg, &num_msg,
					repeater_on_buffer, REPEATER_ON);
		}
	}

	FillMsg(msg, hChip->I2cAddr, &num_msg, NbRegs, buffer, READING,
		stop_after_subaddress);

	if ((hChip->IORoute == DEMOD_IO_REPEATER)
		&& (hChip->RepeaterHost->IsAutoRepeaterOffEnable == false)) {
		FillRepMsg(hChip, msg, &num_msg, repeater_off_buffer,
		REPEATER_OFF);
	}


	fe_i2c_xfer_msg(hChip, &msg[0], num_msg);

	UpdateDevicemapData(hChip, buffer, first_reg_index, NbRegs);

	return hChip->Error;
#else
	uint8_t buffer[NbRegs + 3];
	uint8_t repeater_on_buffer[5];
	uint8_t repeater_off_buffer[5];
	int32_t first_reg_index;
	fe_i2c_msg msg[6];
	int ret;
	uint32_t msgindex = 0;
	uint32_t sub_address_bytes = 0;
	uint32_t retry_count = 0;
	bool stop_after_subaddress = true;
	uint32_t msg_cnt;

	memset(msg, 0, (sizeof(fe_i2c_msg) * 6));

	if (hChip->Abort)
		return CHIPERR_NO_ERROR;

	hChip->Error = GetRegIndex(hChip, FirstRegAddress, NbRegs,
							&first_reg_index);
	if (hChip->Error)
		return hChip->Error;

	if (hChip->IORoute == DEMOD_IO_REPEATER)
		FillRepMsg(hChip, msg, &msgindex, repeater_on_buffer,
				REPEATER_ON);

	PrepareSubAddressHeader(hChip->ChipMode, READING, FirstRegAddress,
				buffer, &sub_address_bytes);

	if ((hChip->ChipMode == STCHIP_MODE_SUBADR_8_NS)
		|| (hChip->ChipMode == STCHIP_MODE_NOSUBADR_SR))
		stop_after_subaddress = false;

	if (sub_address_bytes) {

		FillMsg(msg, hChip->I2cAddr, &msgindex, sub_address_bytes,
			buffer, WRITING, stop_after_subaddress);

		if ((hChip->IORoute == DEMOD_IO_REPEATER)
		      && (hChip->RepeaterHost->IsAutoRepeaterOffEnable == true)
				&& (stop_after_subaddress == true))
			FillRepMsg(hChip, msg, &msgindex,
					repeater_on_buffer, REPEATER_ON);
	}

	FillMsg(msg, hChip->I2cAddr, &msgindex, NbRegs, buffer, READING,
		stop_after_subaddress);

	if ((hChip->IORoute == DEMOD_IO_REPEATER)
		&& (hChip->RepeaterHost->IsAutoRepeaterOffEnable == false))
		FillRepMsg(hChip, msg, &msgindex, repeater_off_buffer,
		REPEATER_OFF);

	do {
		ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
		if (ret != msgindex && retry_count == 2) {
			for (msg_cnt = 0 ; msg_cnt < msgindex ; msg_cnt++)
				pr_err(
				      "%s:Rmsg[%d]addr = 0x%x len = %d Err=%d\n"
				      , __func__, msg_cnt, msg[msg_cnt].addr,
				      msg[msg_cnt].len, ret);
			hChip->Error = CHIPERR_I2C_NO_ACK;
		}
		retry_count++;
	} while ((ret != msgindex) && (retry_count <= 2));

	UpdateDevicemapData(hChip, buffer, first_reg_index, NbRegs);

	return hChip->Error;
#endif
}

/*
 * Name: ChipSetField()
 *
 * Description: write  Value to the specific field of register
 *
 */
STCHIP_Error_t ChipSetField(STCHIP_Handle_t hChip, uint32_t FieldId, int Value)
{
	uint8_t regValue;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	int32_t mask, regIndex, sign, bits, pos;

	err = GetRegIndex(hChip, (FieldId >> 16) & 0xFFFF, 1, &regIndex);
	if (err != CHIPERR_NO_ERROR)
		return err;
	if (hChip->Abort)
		return CHIPERR_NO_ERROR;
	regValue = hChip->pRegMapImage[regIndex].Value; /* Read the register*/
	/* regValue = ChipGetOneRegister(hChip, (FieldId >> 16) & 0xFFFF);*/
	sign = ChipGetFieldSign(FieldId);
	mask = ChipGetFieldMask(FieldId);
	pos = ChipGetFieldPosition(mask);
	bits = ChipGetFieldBits(mask, pos);
	/* compute signed fieldval */
	if (sign == CHIP_SIGNED)
		Value = (Value > 0) ? Value : Value + (bits);

	Value = mask & (Value << pos); /* Shift and mask value*/
	/* Concat register value and fieldval */
	regValue = (regValue & (~mask)) + Value;
	ChipSetOneRegister(hChip, (FieldId >> 16) & 0xFFFF, regValue);

	return hChip->Error;
}
EXPORT_SYMBOL(ChipSetField);

/*
 * Name: ChipGetField()
 *
 * Description: read  Value from the specific field of register
 *
 */
uint8_t ChipGetField(STCHIP_Handle_t hChip, uint32_t FieldId)
{
	int32_t value = 0xFF;
	int32_t sign, mask, pos, bits;

	if (hChip && !hChip->Error) {
		/* I2C Read : register address set - up*/
		sign = ChipGetFieldSign(FieldId);
		mask = ChipGetFieldMask(FieldId);
		pos = ChipGetFieldPosition(mask);
		bits = ChipGetFieldBits(mask, pos);
		/* Read the register */
		value = ChipGetOneRegister(hChip, FieldId >> 16);
		value = (value & mask) >> pos; /* Extract field*/

		if ((sign == CHIP_SIGNED)
			&& (value & (1 << (bits - 1))))
			value = value - (1 << bits);
	}

	return value;
}
EXPORT_SYMBOL(ChipGetField);

/*
 * Name: ChipSetFieldImage()
 *
 * Description: Set value of a field in RegMap
 *
 */
STCHIP_Error_t ChipSetFieldImage(STCHIP_Handle_t hChip, uint32_t FieldId,
				 int32_t Value)
{
	int32_t regIndex, mask, sign, bits, regAddress, pos;
	STCHIP_Error_t err;

	regAddress = ChipGetRegAddress(FieldId);

	err = GetRegIndex(hChip, regAddress, 1, &regIndex);
	if (err != CHIPERR_NO_ERROR)
		return err;

	sign = ChipGetFieldSign(FieldId);
	mask = ChipGetFieldMask(FieldId);
	pos = ChipGetFieldPosition(mask);
	bits = ChipGetFieldBits(mask, pos);

	if (sign == CHIP_SIGNED)
		Value = (Value > 0) ? Value : Value + (1 << bits);
	Value = mask & (Value << pos); /* Shift and mask value */

	hChip->pRegMapImage[regIndex].Value =
		(hChip->pRegMapImage[regIndex].Value & (~mask)) + Value;

	return hChip->Error;
}
EXPORT_SYMBOL(ChipSetFieldImage);

/*
 * Name: ChipGetFieldImage()
 *
 * Description: get the value of a field from RegMap
 *
 */
int32_t ChipGetFieldImage(STCHIP_Handle_t hChip, uint32_t FieldId)
{
	int32_t value = 0xFF;
	int32_t regIndex, mask, sign, bits, regAddress, pos;

	regAddress = ChipGetRegAddress(FieldId);
	regIndex = ChipGetRegisterIndex(hChip, regAddress);

	sign = ChipGetFieldSign(FieldId);
	mask = ChipGetFieldMask(FieldId);
	pos = ChipGetFieldPosition(mask);
	bits = ChipGetFieldBits(mask, pos);

	value = hChip->pRegMapImage[regIndex].Value;

	value = (value & mask) >> pos; /* Extract field */

	if ((sign == CHIP_SIGNED) && (value & (1 << (bits - 1))))
		value = value - (1 << bits); /* Compute signed value*/

	return value;
}
EXPORT_SYMBOL(ChipGetFieldImage);

/*
 * Name: ChipGetRegisterIndex()
 *
 * Description: Get the index of a register from the pRegMapImage table
 *
 */
int32_t ChipGetRegisterIndex(STCHIP_Handle_t hChip, uint16_t RegId)
{
	int32_t regIndex = -1, reg = 0;

/* int32_t top, bottom, mid ; to be used for binary search*/
	while (reg < hChip->NbRegs) {

		if (hChip->pRegMapImage[reg].Addr == RegId) {
			regIndex = reg;
			break;
		}
		reg++;
	}

	return regIndex;
}

/*
 * Name: ChipUpdateDefaultValues()
 *
 * Description: update the default values of the RegMap chip
 *
 */
STCHIP_Error_t ChipUpdateDefaultValues(STCHIP_Handle_t hChip,
						STCHIP_Register_t *pRegMap)
{
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	int32_t reg;

	if (hChip) {
		for (reg = 0 ; reg < hChip->NbRegs ; reg++) {
			hChip->pRegMapImage[reg].Addr = pRegMap[reg].Addr;
			hChip->pRegMapImage[reg].Value = pRegMap[reg].Value;
		}
	} else {
		err = CHIPERR_INVALID_HANDLE;
	}

	return err;
}
EXPORT_SYMBOL(ChipUpdateDefaultValues);


/*
 * Name: ChipFieldExtractVal()
 *
 * Description: Compute the value of the field with the 8 bit register value
 *
 */
uint32_t ChipFieldExtractVal(uint8_t RegisterVal, int FieldIndex)
{
	uint16_t regaddress;
	int mask;
	int sign;
	int bits;
	int pos;
	uint32_t value;

	value = RegisterVal;
	regaddress = ChipGetRegAddress(FieldIndex);
	sign = ChipGetFieldSign(FieldIndex);
	mask = ChipGetFieldMask(FieldIndex);
	pos = ChipGetFieldPosition(mask);
	bits = ChipGetFieldBits(mask, pos);

	/* Extract field */
	value = ((value & mask) >> pos);

	if ((sign == CHIP_SIGNED) && (value & (1 << (bits - 1))))
		value = (value - (1 << bits)); /* Compute signed values */

	return value;
}

/*
 * Name: ChipGetRegAddress()
 *
 * Description:
 *
 */
uint16_t ChipGetRegAddress(uint32_t FieldId)
{
	uint16_t regaddress;

	regaddress = (FieldId >> 16) & 0xFFFF; /* FieldId is [reg address]
						*[reg address][sign][mask]
						*--- 4 bytes */
	return regaddress;
}

/*
 * Name: ChipGetFieldMask()
 *
 * Description:
 *
 */
int ChipGetFieldMask(uint32_t FieldId)
{
	int mask;

	mask = FieldId & 0xFF; /* FieldId is [reg address]
				*[reg address][sign][mask]
				*--- 4 bytes */
	return mask;
}

/*
 * Name: ChipGetFieldSign()
 *
 * Description:
 *
 */
int ChipGetFieldSign(uint32_t FieldId)
{
	int sign;

	sign = (FieldId >> 8) & 0x01; /* FieldId is [reg address][reg address]
					*[sign][mask] --- 4 bytes */
	return sign;
}

/*
 * Name: ChipGetFieldPosition()
 *
 * Description:
 *
 */
int ChipGetFieldPosition(uint8_t Mask)
{
	int position = 0, i = 0;
	int fpos = 0;

	while ((!position) && (i < 8)) {
		position = (Mask >> i) & 0x01;
		i++;
	}
	fpos = i - 1;
	return fpos;
}

/*
 * Name: ChipGetFieldBits()
 *
 * Description:
 *
 */
int ChipGetFieldBits(int mask, int Position)
{
	int bits, bit;
	int i = 0;

	bits = mask >> Position;
	bit = bits;
	while ((bit > 0) && (i < 8)) {
		i++;
		bit = bits >> i;

	}
	return i;
}

void ChipAbort(STCHIP_Handle_t hChip, BOOL Abort)
{
	hChip->Abort = Abort;
}
EXPORT_SYMBOL(ChipAbort);

/*
 * Name: ChipWait_Or_Abort()
 *
 * Description: wait for n ms or abort if requested
 *
 */
void ChipWaitOrAbort(STCHIP_Handle_t hChip, uint32_t delay_ms)
{
	uint32_t i = 0;

	while ((hChip->Abort == false) && (i++ < (delay_ms / 8)))
		WAIT_N_MS(8);
	i = 0;
	while ((hChip->Abort == false) && (i++ < (delay_ms % 8)))
		WAIT_1_MS;
}
EXPORT_SYMBOL(ChipWaitOrAbort);

/*
 * Name: ChipGetRepeaterMessageLength()
 *
 * Description: Get repeater message length
 *
 */
uint32_t ChipGetRepeaterMessageLength(STCHIP_Handle_t hChip)
{
	if (hChip->ChipMode == STCHIP_MODE_SUBADR_16)
		return 3;
	else if (hChip->ChipMode == STCHIP_MODE_SUBADR_8)
		return 2;
	else
		return 0;
}

/*
 * Name: ChipFillRepeaterMessage()
 *
 * Description: Fill repeater message
 *
 */
STCHIP_Error_t ChipFillRepeaterMessage(STCHIP_Handle_t hChip, uint32_t FieldId,
						int Value, uint8_t *Buffer)
{
	uint16_t regaddress;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;

	ChipSetFieldImage(hChip, FieldId, Value);
	regaddress = ChipGetRegAddress(FieldId);

	switch (hChip->ChipMode) {
	case STCHIP_MODE_SUBADR_16:
		Buffer[0] = MSB(regaddress);
		Buffer[1] = LSB(regaddress);
		Buffer[2] = ChipGetFieldImage(hChip, (FieldId | 0xFF));
		break;
	case STCHIP_MODE_SUBADR_8:
		Buffer[0] = LSB(regaddress);
		Buffer[1] = ChipGetFieldImage(hChip, (FieldId | 0xFF));
		break;
	default:
		return err;

	}

	return err;
}
EXPORT_SYMBOL(ChipFillRepeaterMessage);

/*
*Name	ChipCheckAck
*
*Description	Test if the chip acknowledge at its supposed address
*/
STCHIP_Error_t ChipCheckAck(STCHIP_Handle_t hChip)
{
	U8 data = 0;
	unsigned char Buffer[6];

	if (!hChip)
		return CHIPERR_INVALID_HANDLE;
	hChip->Error = CHIPERR_NO_ERROR;
	/* Set repeater ON */
	if (hChip->Repeater && hChip->RepeaterHost && hChip->RepeaterFn)
		hChip->Error = hChip->RepeaterFn(hChip->RepeaterHost,
							   REPEATER_ON, Buffer);
	if (hChip->RepeaterHost)
		hChip->Error |= Chip_I2cReadWrite(hChip->RepeaterHost,
						  hChip->I2cAddr, Buffer, 1);

	hChip->Error |= Chip_I2cReadWrite(hChip, hChip->I2cAddr, &data, 1);
	/* Set repeater OFF */
	if (hChip->Repeater && hChip->RepeaterHost && hChip->RepeaterFn)
		hChip->Error |= hChip->RepeaterFn(hChip->RepeaterHost,
							  REPEATER_OFF, Buffer);

	if (hChip->Error != CHIPERR_NO_ERROR)
		hChip->Error = CHIPERR_I2C_NO_ACK;

	return hChip->Error;
}
EXPORT_SYMBOL(ChipCheckAck);

STCHIP_Error_t Chip_I2cReadWrite(STCHIP_Handle_t hChip, U8 ChipAddress,
						unsigned char *Data, int NbData)
{

	fe_i2c_msg msg[6];
	int ret;
	uint32_t msgindex = 0;
	uint32_t retry_count = 0;
	uint32_t msg_cnt;

	/* Message to write the actual data onto the device */
	FillMsg(msg, hChip->I2cAddr, &msgindex, NbData, Data, WRITING, true);

	do {
		ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
		if (ret != msgindex && retry_count == 2) {
			for (msg_cnt = 0 ; msg_cnt < msgindex ; msg_cnt++)
				pr_err(
				  "%s:WR msg[%d] addr = 0x%x len = %d Err= %d\n"
				  , __func__, msg_cnt, msg[msg_cnt].addr,
				  msg[msg_cnt].len, ret);
			hChip->Error = CHIPERR_I2C_NO_ACK;
		}
	retry_count++;
	} while ((ret != msgindex) && (retry_count <= 2));

		return hChip->Error;

}
void RetrieveDevicemapData(STCHIP_Handle_t hChip, uint8_t *Dest,
				uint32_t FirstRegIndex, int Size)
{
	uint8_t index;

	for (index = 0 ; index < Size ; index++)
		Dest[index] = hChip->pRegMapImage[FirstRegIndex + index].Value;
}

void UpdateDevicemapData(STCHIP_Handle_t hChip, uint8_t *Src,
			 uint32_t FirstRegIndex, int Size)
{
	uint8_t index;

	for (index = 0 ; index < Size ; index++)
		hChip->pRegMapImage[FirstRegIndex + index].Value = Src[index];

}

static STCHIP_Error_t GetRegIndex(STCHIP_Handle_t hChip, uint16_t RegId,
				  uint32_t NbRegs, int32_t *RegIndex)
{
	*RegIndex = ChipGetRegisterIndex(hChip, RegId);

	if (((*RegIndex >= 0)
		 && ((*RegIndex + NbRegs - 1) < hChip->NbRegs)) == false) {

		hChip->Error = CHIPERR_INVALID_REG_ID;
	} else {
		hChip->Error = CHIPERR_NO_ERROR;
	}
	return hChip->Error;
}

void PrepareSubAddressHeader(STCHIP_Mode_t Mode, context_t Context,
				 uint16_t SubAddress, uint8_t *Buffer,
				 uint32_t *SubAddressByteCount)
{
	switch (Mode) {
	case STCHIP_MODE_SUBADR_16:
		Buffer[0] = MSB(SubAddress);
		Buffer[1] = LSB(SubAddress);
		*SubAddressByteCount = 2;
		break;
	case STCHIP_MODE_MXL:
		if (Context == WRITING) {
			Buffer[0] = LSB(SubAddress);
			*SubAddressByteCount = 1;
		} else {
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
		if (Context == WRITING) {
			Buffer[0] = LSB(SubAddress);
			*SubAddressByteCount = 1;
		} else {
			*SubAddressByteCount = 0;
		}
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

void FillMsg(fe_i2c_msg *msg, uint32_t addr, uint32_t *MsgIndex,
		 uint32_t BufferLen, uint8_t *Buffer_p, context_t Context,
		 bool UseStop)
{
#ifdef CONFIG_ARCH_STI
	msg[*MsgIndex].flags = Context;
	if (UseStop)
		msg[*MsgIndex].flags |= STMFE_I2C_USE_STOP;

	msg[*MsgIndex].addr = addr;

	msg[*MsgIndex].buf = Buffer_p;
	msg[*MsgIndex].len = BufferLen;
	(*MsgIndex)++;
#else
	msg[*MsgIndex].addr = addr;
	msg[*MsgIndex].flags = Context;
	if (!UseStop) {
		msg[*MsgIndex].flags = Context;
		/*msg[*MsgIndex].flags |= 0;-I2C_M_NOSTOP undeclard err?*/
	} else {
		msg[*MsgIndex].flags |= I2C_M_NOREPSTART;
		/* STI2C sets this false
		*for last message ; not done here */
	}
	msg[*MsgIndex].buf = Buffer_p;
	msg[*MsgIndex].len = BufferLen;
	(*MsgIndex)++;
#endif
}

STCHIP_Error_t FillRepMsg(STCHIP_Handle_t hChip, fe_i2c_msg *msg,
			  uint32_t *MsgIndex, unsigned char *Buffer,
			  bool Status)
{
	STCHIP_Error_t err;
	STCHIP_Handle_t handle;
	handle = (STCHIP_Handle_t) hChip->RepeaterHost;

	err = hChip->RepeaterFn(hChip->RepeaterHost, Status, Buffer);
	/* Message to turn on the repeater bus */

	FillMsg(msg, hChip->RepeaterHost->I2cAddr, MsgIndex,
		ChipGetRepeaterMessageLength(handle), Buffer, WRITING, true);

	return err;
}

static int32_t __init stm_fe_util_init(void)
{
	pr_info("Loading stmfe utility module ...\n");
	return 0;
}

module_init(stm_fe_util_init);

static void __exit stm_fe_util_term(void)
{
	pr_info("Removing stmfe utility module ...\n");
}

module_exit(stm_fe_util_term);

MODULE_DESCRIPTION("Common i2c wrapper for all frontend devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
