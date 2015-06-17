/*******************************************************************************
 *
 * FILE NAME          : MxL101SF_OEM_Drv.cpp
 * 
 * AUTHOR             : Brenndon Lee
 * DATE CREATED       : 1/24/2008
 *
 * DESCRIPTION        : This file contains I2C emulation routine
 *                      
 *******************************************************************************
 *                Copyright (c) 2006, MaxLinear, Inc.
 ******************************************************************************/
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "dvb_frontend.h"

#include <linux/rtc.h>
#include <linux/kdev_t.h>
#include <linux/idr.h>

#include "../mxl101sf.h"
#include "MxL101SF_OEM_Drv.h"

/*----------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_ReadRegister
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|                 8/22/2008
--|
--| DESCRIPTION   : This function reads register data of the provided-address
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_ReadRegister(UINT8 regAddr, UINT8 *dataPtr)
{
	MXL_STATUS status = MXL_TRUE;
	UINT8 buf[] = {0xFB, regAddr};
	UINT8 buf1[] = {0};
	SINT32 ret;

	struct i2c_msg msg [] = { { .addr = get_demod_addr(), .flags = 0, .buf = buf, .len = 2 },
			   { .addr = get_demod_addr(), .flags = I2C_M_RD, .buf = buf1, .len = 1 } };
	struct i2c_adapter *i2c = get_i2c_adapter();

	if (i2c == NULL)
	{
		printk(KERN_ERR "%s : NULL pointer\n", __FUNCTION__);
		status = MXL_FALSE;
	}
	else
	{
		ret = i2c_transfer (i2c, msg, 1);
		ret |= i2c_transfer (i2c, msg+1, 1);
		if (ret != 1)
		{
			printk("%s: error (reg == 0x%02x,  ret == %i)\n",
				__FUNCTION__, regAddr, ret);
			status = MXL_FALSE;
		}
		else
		{
			*dataPtr = buf1[0];
		}
	}

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_WriteRegister
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function writes the provided value to the specified
--|                 address.
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_WriteRegister(UINT8 regAddr, UINT8 regData)
{
	MXL_STATUS status = MXL_TRUE;
	UINT32 ret  = 0;
	UINT8 buf [] = { regAddr,  regData };
	struct i2c_msg msg = { .addr = get_demod_addr(),  .flags = 0,  .buf = buf,  .len = 2 };
	struct i2c_adapter *i2c = get_i2c_adapter();

	if (i2c == NULL)
	{
		printk(KERN_ERR "%s : NULL pointer\n", __FUNCTION__);
		status = MXL_FALSE;
	}
	else
	{
		ret = i2c_transfer (i2c, &msg, 1);

		if (ret != 1)
		{
			printk("%s: error (addr == 0x%02x,reg == 0x%02x, val == 0x%02x, ret == %i)\n",
			__FUNCTION__, msg.addr,regAddr, regData, ret);
			status = MXL_FALSE;
		}

	}
	return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_Sleep
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 8/31/2009
--|
--| DESCRIPTION   : This function will cause the calling thread to be suspended 
--|                 from execution until the number of milliseconds specified by 
--|                 the argument time has elapsed 
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_Sleep(UINT16 TimeinMilliseconds)
{
	MXL_STATUS status = MXL_TRUE;

	msleep(TimeinMilliseconds);

	return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : Ctrl_GetTime
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 10/05/2009
--|
--| DESCRIPTION   : This function will return current system's timestamp in 
--|                 milliseconds resolution. 
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS Ctrl_GetTime(UINT32 *TimeinMilliseconds)
{
  //*TimeinMilliseconds = current systems timestamp in milliseconds.
  // User should implement his own routine to get current system's timestamp in milliseconds.
	struct timespec tm;
	u32 time;

	tm = current_kernel_time();
	time = tm.tv_sec * 1000 + (tm.tv_nsec/1000000);
  
	*TimeinMilliseconds = time;
	return MXL_TRUE;
}
