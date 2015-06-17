/***********************************************************************
 *
 * File: hdmirx/include/hdmirx_utils.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef HDMIRX_UTILS
#define HDMIRX_UTILS

/******************************************************************************
  I N C L U D E    F I L E S
******************************************************************************/

/* Standard Includes ----------------------------------------------*/
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>

/* Local Includes -------------------------------------------------*/
#include <hdmirx_drv.h>
#include <stm_hdmirx_os.h>

#define WAIT_N_MS(X) schedule_timeout_interruptible(msecs_to_jiffies(X))
#define WAIT_1_MS    usleep_range(900, 1000)

/* error codes */
typedef enum
{
  CHIPERR_NO_ERROR = 0,	/* No error encountered */
  CHIPERR_INVALID_HANDLE,	/* Using of an invalid chip handle */
  CHIPERR_INVALID_REG_ID,	/* Using of an invalid register */
  CHIPERR_INVALID_FIELD_ID,	/* Using of an Invalid field */
  CHIPERR_INVALID_FIELD_SIZE,	/* Using of a field with an invalid size */
  CHIPERR_I2C_NO_ACK,	/* No acknowledge from the chip */
  CHIPERR_I2C_BURST,	/* Two many registers accessed in burst mode */
  CHIPERR_I2C_OTHER_ERROR	/* Other Errors reported  from I2C driver, added in stfrontend driver */
} hdmirx_I2C_error_t;

/* how to access I2C bus */
typedef enum
{
  STCHIP_MODE_SUBADR_8,	/* <addr><reg8><data><data>(e.g. demod chip) */
  STCHIP_MODE_SUBADR_16,	/* <addr><reg8><data><data>(e.g. demod chip) */
  STCHIP_MODE_NOSUBADR,	/* <addr><data>|<data><data><data>(tuner chip) */
  STCHIP_MODE_NOSUBADR_RD,
  STCHIP_MODE_SUBADR_8_NS_MICROTUNE,
  STCHIP_MODE_SUBADR_8_NS,
  STCHIP_MODE_MXL,
  STCHIP_MODE_NOSUBADR_SR,	/*start repeat */
  STCHIP_MODE_SUBADR_8_SR
} STCHIP_Mode_t;

/* structures -------------------------------------------------------------- */

/* register information */
typedef struct
{
  u_int32_t Addr;		/* Address */
  u_int32_t Value;	/* Current value */
} STCHIP_Register_t;		/*changed to be in sync with LLA :april 09 */

typedef enum context_e
{
  WRITING,
  READING
} context_t;

typedef struct Message_s
{

  u_int8_t *Buffer_p;
  u_int32_t BufferLen;
  context_t Context;
} Message_t;
typedef struct hdmirx_i2c_info_s *hdmirx_I2C_info_handle_t;

#define LSB(X) ( ( (X) & 0xFF ) )
#define MSB(Y) ( ( (Y)>>8  ) & 0xFF )
typedef struct hdmirx_i2c_info_s
{

  u_int32_t I2cAddr;	/* Chip I2C address */
  u_int32_t NbRegs;	/* Number of registers in the chip */
  u_int32_t NbFields;	/* Number of fields in the chip */
  STCHIP_Register_t *pRegMapImage;	/* Pointer to register map */
  hdmirx_I2C_error_t Error;	/* Error state */
  STCHIP_Mode_t ChipMode;
  bool Abort;		/* Abort flag when set to on no register  * access and no wait  are done  */
  hdmirx_i2c_adapter dev_i2c;
  u_int32_t value;

} hdmirx_i2c_info_t;

int hdmirx_select_port(hdmirx_port_handle_t *Handle);

hdmirx_I2C_error_t chip_set_one_register(hdmirx_I2C_info_handle_t hChip,
                                         u_int16_t RegAddr, u_int8_t Value);

hdmirx_I2C_error_t chip_set_registers(hdmirx_I2C_info_handle_t hChip,
                                      u_int32_t FirstRegAddress, int Number);

hdmirx_I2C_error_t chip_set_registersI2C(hdmirx_I2C_info_handle_t hChip,
                                         u_int32_t FirstReg, int NbRegs);

void chip_wait_or_abort(hdmirx_I2C_info_handle_t hChip, u_int32_t delay_ms);
void chip_abort(hdmirx_I2C_info_handle_t hChip, bool Abort);

int32_t chip_get_register_index(hdmirx_I2C_info_handle_t hChip, u_int16_t RegId);

void prepare_sub_addr_header(STCHIP_Mode_t Mode, context_t Context,
                             u_int16_t SubAddress, u_int8_t *Buffer,
                             u_int32_t *SubAddressByteCount);

#ifdef CONFIG_ARCH_STI
void fill_msg(struct i2c_msg *msg, u_int32_t addr, u_int32_t *MsgIndex,
              u_int32_t BufferLen, u_int8_t *Buffer_p, context_t Context);
#else
void fill_msg(struct i2c_msg *msg, u_int32_t addr, u_int32_t *MsgIndex,
              u_int32_t BufferLen, u_int8_t *Buffer_p, context_t Context,
              bool UseStop);
#endif
hdmirx_i2c_adapter chip_open(uint32_t bus);

#endif
