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

Source file name : i2c_wrapper.h
Author :           Shobhit

Io header for LLA to access the hw registers

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _I2C_WRAPPER_H
#define _I2C_WRAPPER_H

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/demod.h>
#include <stm_fe_os.h>
#define WAIT_N_MS(X) schedule_timeout_interruptible(msecs_to_jiffies(X))
#define WAIT_1_MS    usleep_range(900, 1000)

#define REPEATER_ON true
#define REPEATER_OFF false

/* maximum number of chips that can be opened */
#define MAXNBCHIP 10

/* maximum number of chars per chip/tuner name */
#define MAXNAMESIZE 30

#define STFRONTEND_USE_NEW_I2C


typedef enum {
	STCHIP_ACCESS_WR,	/* can be read and written */
	STCHIP_ACCESS_R,	/* only be read from */
	STCHIP_ACCESS_W,	/* only be written to */
	STCHIP_ACCESS_NON	/* cannot be read or written
				*(guarded register, e.g. register skipped by
				*ChipApplyDefaultValues() etc.) */
} STCHIP_Access_t;

/* register field type */
typedef enum {
	CHIP_UNSIGNED = 0,
	CHIP_SIGNED = 1
} STCHIP_FieldType_t;

/* error codes */
typedef enum {
	CHIPERR_NO_ERROR = 0,	/* No error encountered */
	CHIPERR_INVALID_HANDLE,	/* Using of an invalid chip handle */
	CHIPERR_INVALID_REG_ID,	/* Using of an invalid register */
	CHIPERR_INVALID_FIELD_ID,/* Using of an Invalid field */
	CHIPERR_INVALID_FIELD_SIZE,/* Using of a field with an invalid size */
	CHIPERR_I2C_NO_ACK,	/* No acknowledge from the chip */
	CHIPERR_I2C_BURST,	/* Two many registers accessed in burst mode */
	CHIPERR_I2C_OTHER_ERROR	/* Other Errors reported  from I2C driver,
				 * added in stfrontend driver */
} STCHIP_Error_t;

/* how to access I2C bus */
typedef enum {
	STCHIP_MODE_SUBADR_8,	/* <addr><reg8><data><data>(e.g. demod chip) */
	STCHIP_MODE_SUBADR_16,	/* <addr><reg8><data><data>(e.g. demod chip) */
	STCHIP_MODE_NOSUBADR,	/* <addr><data>|<data><data><data>(tuner chip)*/
	STCHIP_MODE_NOSUBADR_RD,
	STCHIP_MODE_SUBADR_8_NS_MICROTUNE,
	STCHIP_MODE_SUBADR_8_NS,
	STCHIP_MODE_MXL,
	STCHIP_MODE_NOSUBADR_SR,	/*start repeat */
	STCHIP_MODE_SUBADR_8_SR
} STCHIP_Mode_t;

/* structures -------------------------------------------------------------- */

/* register information */
typedef struct {
	u_int32_t Addr;		/* Address */
	u_int32_t Value;	/* Current value */
} STCHIP_Register_t;		/*changed to be in sync with LLA :april 09 */

/* register field information */
typedef struct {
	u_int32_t Reg;		/* Register index */
	unsigned char Pos;	/* Bit position */
	unsigned char Bits;	/* Bit width */
	unsigned char Mask;	/* Mask compute with width and position */
	STCHIP_FieldType_t Type;	/* Signed or unsigned */
	char Name[30];		/* Name */
} STCHIP_Field_t;

typedef enum context_e {
	WRITING,
	READING
} context_t;

typedef struct Message_s {

	u_int8_t *Buffer_p;
	u_int32_t BufferLen;
	context_t Context;
} Message_t;

struct stchip_Info_s;
typedef struct stchip_Info_s *STCHIP_Handle_t;
typedef struct stchip_Info_s *TUNER_Handle_t;

typedef struct stchip_Info_s {
	enum demod_i2c_route_e IORoute;
	enum demod_io_type_e IODriver;
	/*following part copied directly from LLA */
	u_int32_t I2cAddr;	/* Chip I2C address */
	char Name[MAXNAMESIZE];	/* Name of the chip */
	u_int32_t NbRegs;	/* Number of registers in the chip */
	u_int32_t NbFields;	/* Number of fields in the chip */
	STCHIP_Register_t *pRegMapImage;	/* Pointer to register map */
	STCHIP_Error_t Error;	/* Error state */
	STCHIP_Mode_t ChipMode;	/* Access bus in demod (SubAdr) or
				 * tuner (NoSubAdr) mode */
	u_int8_t ChipID;	/* Chip cut ID */
	bool IsAutoRepeaterOffEnable;

	bool Repeater;		/* Is repeater enabled or not ? */
	struct stchip_Info_s *RepeaterHost;	/* Owner of the repeater */

#ifdef CHIP_STAPI
	 STCHIP_Error_t(*RepeaterFn) (struct stchip_Info_s *hChip,
		bool State, u_int8_t *Buffer);	/*Pointer to repeater routine*/
#else
	 STCHIP_Error_t(*RepeaterFn) (struct stchip_Info_s *hChip, bool State);
	 /* Pointer to repeater routine */
#endif

	u_int32_t XtalFreq;	/* added for dll purpose */
	/* Parameters needed for non sub address devices */
	u_int32_t WrStart;	/* Id of the first writable register */
	u_int32_t WrSize;	/* Number of writable registers */
	u_int32_t RdStart;	/* Id of the first readable register */
	u_int32_t RdSize;	/* Number of readable registers */
	bool Abort;		/* Abort flag when set to on no register
				 * access and no wait  are done  */
	bool IFmode;		/* IF mode 1; IQ mode 0   */

	fe_i2c_adapter dev_i2c;
	u_int32_t value;
	struct tuner_ops_s *tuner_ops;
	void *pData;		/* pointer to chip data */
	STCHIP_Register_t *qamregmap;	/*additional regmap for dual std demod*/
	STCHIP_Register_t *ofdmregmap;	/*additional regmap for dual std demod*/
} STCHIP_Info_t;

/*Used in LLA chip, placed here as LLA includes chip.h */
typedef struct {
	STCHIP_Info_t *Chip;/*pointer to parameters to pass to the CHIP API*/
	u_int32_t NbDefVal;/* number of default values  */
} Demod_InitParams_t;

#ifdef CHIP_STAPI
typedef STCHIP_Error_t(*STCHIP_RepeaterFn_t) (STCHIP_Handle_t hChip,
	bool State, u_int8_t *Buffer);	/* Pointer to repeater routine */
#else
typedef STCHIP_Error_t(*STCHIP_RepeaterFn_t) (STCHIP_Handle_t hChip,
			bool State);	/* Pointer to repeater routine */
#endif

STCHIP_Error_t ChipSetOneRegister(STCHIP_Handle_t hChip, u_int16_t RegAddr,
				  u_int8_t Value);
u_int8_t ChipGetOneRegister(STCHIP_Handle_t hChip, u_int32_t RegAddress);

STCHIP_Error_t ChipSetRegisters(STCHIP_Handle_t hChip,
				u_int32_t FirstRegAddress, int Number);
STCHIP_Error_t ChipGetRegisters(STCHIP_Handle_t hChip,
				u_int32_t FirstRegAddress, int8_t Number);

STCHIP_Error_t ChipSetRegistersI2C(STCHIP_Handle_t hChip, u_int32_t FirstReg,
				   int NbRegs);
STCHIP_Error_t ChipGetRegistersI2C(STCHIP_Handle_t hChip,
				   u_int32_t FirstRegAddress, int NbRegs);

STCHIP_Error_t ChipSetField(STCHIP_Handle_t hChip, u_int32_t FieldIndex,
			    int Value);
u_int8_t ChipGetField(STCHIP_Handle_t hChip, u_int32_t FieldIndex);

STCHIP_Error_t ChipSetFieldImage(STCHIP_Handle_t hChip, u_int32_t FieldId,
				 int32_t Value);
int32_t ChipGetFieldImage(STCHIP_Handle_t hChip, u_int32_t FieldId);

void ChipWaitOrAbort(STCHIP_Handle_t hChip, u_int32_t delay_ms);
void ChipAbort(STCHIP_Handle_t hChip, bool Abort);

u_int32_t ChipFieldExtractVal(u_int8_t RegisterVal, int FieldIndex);
u_int16_t ChipGetRegAddress(u_int32_t FieldId);
int ChipGetFieldMask(u_int32_t FieldId);
int ChipGetFieldSign(u_int32_t FieldId);
int ChipGetFieldPosition(u_int8_t Mask);
int ChipGetFieldBits(int mask, int Position);
int32_t ChipGetRegisterIndex(STCHIP_Handle_t hChip, u_int16_t RegId);
STCHIP_Error_t ChipUpdateDefaultValues(STCHIP_Handle_t hChip,
				       STCHIP_Register_t *pRegMap);
STCHIP_Error_t ChipFillRepeaterMessage(STCHIP_Handle_t hChip, u_int32_t FieldId,
				       int Value, u_int8_t *Buffer);
u_int32_t ChipGetRepeaterMessageLength(STCHIP_Handle_t hChip);
void PrepareSubAddressHeader(STCHIP_Mode_t Mode, context_t Context,
			     u_int16_t SubAddress, u_int8_t *Buffer,
			     u_int32_t *SubAddressByteCount);

STCHIP_Error_t ChipCheckAck(STCHIP_Handle_t hChip);
STCHIP_Error_t Chip_I2cReadWrite(STCHIP_Handle_t hChip, u_int8_t ChipAddress,
					      unsigned char *Data, int NbData);

STCHIP_Error_t FillRepMsg(STCHIP_Handle_t hChip, struct i2c_msg *msg,
			  u_int32_t *MsgIndex, unsigned char *Buffer,
			  bool Status);

void FillMsg(struct i2c_msg *msg, u_int32_t addr, u_int32_t *MsgIndex,
	     u_int32_t BufferLen, u_int8_t *Buffer_p, context_t Context,
	     bool UseStop);
fe_i2c_adapter chip_open(uint32_t bus);
int stm_fe_check_i2c_device_id(unsigned int *base_address, int num);

#endif /* _I2C_WRAPPER_H */
