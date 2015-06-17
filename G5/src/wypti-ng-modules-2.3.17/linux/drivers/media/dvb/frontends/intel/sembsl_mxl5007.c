/* 
 
 Driver APIs for MxL5007 Tuner 
  
 Copyright, Maxlinear, Inc. 
 All Rights Reserved 
  
 File Name:      MxL5007_API.c 
  
 Version:    4.1.3 
*/ 

#define THIS_FILE_ID        0x507 
#define THISFILE_TRACE      SEMDBG_TRACE_BSL 

#include "mxl5007-ce6353.h"

#include "semstbcp.h"                           /* SEMCO STB CP API */ 
#include "semosal.h"                           /* SEMCO STB CP API */ 
 
#include "sembsl_mxl5007.h" 
 
struct i2c_adapter *tuner_adapter; 
/****************************************************************************** 
** 
**  Name: MxL5007_I2C_Write 
** 
**  Description:    I2C write operations 
** 
**  Parameters:    	 
**					DeviceAddr	- MxL201RF Device address 
**					pArray		- Write data array pointer 
**					count		- total number of array 
** 
**  Returns:        0 if success 
** 
**  Revision History: 
** 
**   SCR      Date      Author  Description 
**  ------------------------------------------------------------------------- 
**   N/A   12-16-2007   khuang initial release. 
** 
******************************************************************************/ 
static int  
MxL5007_I2C_Write(MxL5007_TunerConfigS *myTuner, u8* array, int count) 
{
	struct intel_state * state = (struct intel_state *)myTuner->i2c.p_data;
	struct i2c_msg msg = {.addr = state->config->tuner_address, .flags = 0, .buf = array, .len = count};
	int ret=0;

//	if (state->config->tuner_i2c == NULL)
	if (tuner_adapter == NULL)
		return -1;

//	ret = i2c_transfer(state->config->tuner_i2c, &msg, 1);
	ret = i2c_transfer(tuner_adapter, &msg, 1);

	if (ret != 1)
		printk("%s :error in mxl_i2c_write, (addr = 0x%02x)\n",
			__FUNCTION__, state->config->tuner_address);

	return (ret != 1) ? -1 : 0;
} 
 
/****************************************************************************** 
** 
**  Name: MxL5007_I2C_Read 
** 
**  Description:    I2C read operations 
** 
**  Parameters:    	 
**					DeviceAddr	- MxL201RF Device address 
**					Addr		- register address for read 
**					*Data		- data return 
** 
**  Returns:        0 if success 
** 
**  Revision History: 
** 
**   SCR      Date      Author  Description 
**  ------------------------------------------------------------------------- 
**   N/A   12-16-2007   khuang initial release. 
** 
******************************************************************************/ 
static int 
MxL5007_I2C_Read(MxL5007_TunerConfigS* myTuner, u8 reg, u8* val) 
{ 
	int ret;
	u8 b0 [] = {0xFB, reg };
	u8 b1 [] = { 0 };
	struct intel_state * state = (struct intel_state *)myTuner->i2c.p_data;
	struct i2c_msg msg [] = { { .addr = state->config->tuner_address, .flags = 0, .buf = b0, .len = sizeof(b0)/sizeof(b0[0])},
							{ .addr = state->config->tuner_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

//	if (state->config->tuner_i2c == NULL)
	if (tuner_adapter == NULL) {
		printk(KERN_ERR "%s: null tuner adapter (addr = 0x%02x, reg == 0x%02x)\n",
				__FUNCTION__, state->config->tuner_address, reg);
		return -EIO;
	}

//	ret = i2c_transfer (state->config->tuner_i2c, msg, 1);
//	ret |= i2c_transfer (state->config->tuner_i2c, msg+1, 1);
	ret = i2c_transfer (tuner_adapter, msg, 1);
	ret |= i2c_transfer (tuner_adapter, msg+1, 1);

	if (ret != 1) {
		printk("%s: error (addr = 0x%02x, reg == 0x%02x,  ret == %i)\n",
			__FUNCTION__, state->config->tuner_address, reg, ret);
		return -EIO;
	}

	*val = b1[0];
	 return 0;
} 
 
 
 
typedef struct 
{ 
	U8 Num;	/*Register number */
	U8 Val;	/*Register value  */
} IRVType, *PIRVType; 
 
/* local functions called by Init and RFTune */
static U32 
SetIRVBit(PIRVType pIRV, U8 Num, U8 Mask, U8 Val) 
{ 
	while (pIRV->Num || pIRV->Val) 
	{ 
		if (pIRV->Num==Num) 
		{ 
			pIRV->Val&=~Mask; 
			pIRV->Val|=Val; 
		} 
		pIRV++; 
 
	}	 
	return 0; 
} 
 
 
U32 MxL5007_Init
    (
	U8* pArray,				/* a array pointer that store the addr and data pairs for I2C write */
	U32* Array_Size,			/* a integer pointer that store the number of element in above array*/ 
	U8 Mode,				 
	S32 IF_Diff_Out_Level,		  
	U32 Xtal_Freq_Hz,			 
	U32 IF_Freq_Hz,				 
	U8 Invert_IF,											 
	U8 Clk_Out_Enable,     
	U8 Clk_Out_Amp													 
    ) 
{ 
	 
	U32 Reg_Index=0; 
	U32 Array_Index=0; 
 
	IRVType IRV_Init[]= 
	{ 
		{ 0x02, 0x06},  
		{ 0x03, 0x48}, 
		{ 0x05, 0x04},   
		{ 0x06, 0x10},  
		{ 0x2E, 0x15},  /*Override*/ 
		{ 0x30, 0x10},  /*Override*/ 
		{ 0x45, 0x58},  /*Override*/ 
		{ 0x48, 0x19},  /*Override*/ 
		{ 0x52, 0x03},  /*Override*/ 
		{ 0x53, 0x44},  /*Override*/ 
		{ 0x6A, 0x4B},  /*Override*/ 
		{ 0x76, 0x00},  /*Override*/ 
		{ 0x78, 0x18},  /*Override*/ 
		{ 0x7A, 0x17},  /*Override*/ 
		{ 0x85, 0x06},  /*Override*/		 
		{ 0x01, 0x01}, /*TOP_MASTER_ENABLE=1 */
		{ 0, 0} 
	}; 
 
 
	IRVType IRV_Init_Cable[]= 
	{ 
		{ 0x02, 0x06},  
		{ 0x03, 0x48}, 
		{ 0x05, 0x04},   
		{ 0x06, 0x10},   
		{ 0x09, 0x3F},   
		{ 0x0A, 0x3F},   
		{ 0x0B, 0x3F},   
		{ 0x2E, 0x15},  /*Override*/ 
		{ 0x30, 0x10},  /*Override*/ 
		{ 0x45, 0x58},  /*Override*/ 
		{ 0x48, 0x19},  /*Override*/ 
		{ 0x52, 0x03},  /*Override*/ 
		{ 0x53, 0x44},  /*Override*/ 
		{ 0x6A, 0x4B},  /*Override*/ 
		{ 0x76, 0x00},  /*Override*/ 
		{ 0x78, 0x18},  /*Override*/ 
		{ 0x7A, 0x17},  /*Override*/ 
		{ 0x85, 0x06},  /*Override*/	 
		{ 0x01, 0x01},  /*TOP_MASTER_ENABLE=1*/ 
		{ 0, 0} 
	}; 
	/*edit Init setting here*/ 
 
	PIRVType myIRV=0; 
 
	switch(Mode) 
	{ 
	case MxL_MODE_ISDBT: /*ISDB-T Mode*/	 
		myIRV = IRV_Init; 
		SetIRVBit(myIRV, 0x06, 0x1F, 0x10);   
		break; 
	case MxL_MODE_DVBT: /*DVBT Mode*/			 
		myIRV = IRV_Init; 
		SetIRVBit(myIRV, 0x06, 0x1F, 0x11);   
		break; 
	case MxL_MODE_ATSC: /*ATSC Mode*/			 
		myIRV = IRV_Init; 
		SetIRVBit(myIRV, 0x06, 0x1F, 0x12);   
		break;	 
	case MxL_MODE_CABLE:						 
		myIRV = IRV_Init_Cable; 
		SetIRVBit(myIRV, 0x09, 0xFF, 0xC1);	 
		SetIRVBit(myIRV, 0x0A, 0xFF, (U8)(8-IF_Diff_Out_Level));	 
		SetIRVBit(myIRV, 0x0B, 0xFF, 0x17);							 
		break; 
	 
 
	} 
 
	switch(IF_Freq_Hz) 
	{ 
	case MxL_IF_4_MHZ: 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x00); 		 
		break; 
	case MxL_IF_4_5_MHZ: /*4.5MHz*/ 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x02); 		 
		break; 
	case MxL_IF_4_57_MHZ: /*4.57MHz*/ 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x03);  
		break; 
	case MxL_IF_5_MHZ: 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x04);  
		break; 
	case MxL_IF_5_38_MHZ: /*5.38MHz*/ 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x05);  
		break; 
	case MxL_IF_6_MHZ:  
		SetIRVBit(myIRV, 0x02, 0x0F, 0x06);  
		break; 
	case MxL_IF_6_28_MHZ: /*6.28MHz*/ 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x07);  
		break; 
	case MxL_IF_9_1915_MHZ: /*9.1915MHz*/ 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x08);  
		break; 
	case MxL_IF_35_25_MHZ: 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x09);  
		break; 
	case MxL_IF_36_15_MHZ: 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x0a);  
		break; 
	case MxL_IF_44_MHZ: 
		SetIRVBit(myIRV, 0x02, 0x0F, 0x0B);  
		break; 
	} 
 
	if(Invert_IF)  
		SetIRVBit(myIRV, 0x02, 0x10, 0x10);   /*Invert IF*/ 
	else 
		SetIRVBit(myIRV, 0x02, 0x10, 0x00);   /*Normal IF*/ 
 
 
	switch(Xtal_Freq_Hz) 
	{ 
	case MxL_XTAL_16_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x00);  /*select xtal freq & Ref Freq*/ 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x00); 
		break; 
	case MxL_XTAL_20_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x10); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x01); 
		break; 
	case MxL_XTAL_20_25_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x20); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x02); 
		break; 
	case MxL_XTAL_20_48_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x30); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x03); 
		break; 
	case MxL_XTAL_24_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x40); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x04); 
		break; 
	case MxL_XTAL_25_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x50); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x05); 
		break; 
	case MxL_XTAL_25_14_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x60); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x06); 
		break; 
	case MxL_XTAL_27_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x70); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x07); 
		break; 
	case MxL_XTAL_28_8_MHZ:  
		SetIRVBit(myIRV, 0x03, 0xF0, 0x80); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x08); 
		break; 
	case MxL_XTAL_32_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0x90); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x09); 
		break; 
	case MxL_XTAL_40_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0xA0); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x0A); 
		break; 
	case MxL_XTAL_44_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0xB0); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x0B); 
		break; 
	case MxL_XTAL_48_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0xC0); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x0C); 
		break; 
	case MxL_XTAL_49_3811_MHZ: 
		SetIRVBit(myIRV, 0x03, 0xF0, 0xD0); 
		SetIRVBit(myIRV, 0x05, 0x0F, 0x0D); 
		break;	 
	} 
 
	if(!Clk_Out_Enable) /*default is enable*/  
		SetIRVBit(myIRV, 0x03, 0x08, 0x00);    
 
	/*Clk_Out_Amp*/ 
	SetIRVBit(myIRV, 0x03, 0x07, Clk_Out_Amp);    
 
	/*Generate one Array that Contain Data, Address*/ 
	while (myIRV[Reg_Index].Num || myIRV[Reg_Index].Val) 
	{ 
		pArray[Array_Index++] = myIRV[Reg_Index].Num; 
		pArray[Array_Index++] = myIRV[Reg_Index].Val; 
		Reg_Index++; 
	} 
	     
	*Array_Size=Array_Index; 
	return 0; 
} 
 
 
U32 MxL5007_RFTune(U8* pArray, U32* Array_Size, U32 RF_Freq, U8 BWMHz) 
{ 
	IRVType IRV_RFTune[]= 
	{ 
		{ 0x0F, 0x00},  
		{ 0x0C, 0x15}, 
		{ 0x0D, 0x40}, 
		{ 0x0E, 0x0E},	 
		{ 0x1F, 0x87},  /*Override*/ 
		{ 0x20, 0x1F},  /*Override*/ 
		{ 0x21, 0x87},  /*Override*/ 
		{ 0x22, 0x1F},  /*Override*/		 
		{ 0x80, 0x01},  /*Freq Dependent Setting */
		{ 0x0F, 0x01},	 
		{ 0, 0} 
	}; 
 
	U32 dig_rf_freq=0; 
	U32 temp; 
	U32 Reg_Index=0; 
	U32 Array_Index=0; 
	U32 i; 
	U32 frac_divider = 1000000; 
 
	switch(BWMHz) 
	{ 
	case MxL_BW_6MHz: /*6MHz */
			SetIRVBit(IRV_RFTune, 0x0C, 0x3F, 0x15);  /*set DIG_MODEINDEX, DIG_MODEINDEX_A, and DIG_MODEINDEX_CSF */
		break; 
	case MxL_BW_7MHz: /*7MHz */
			SetIRVBit(IRV_RFTune, 0x0C, 0x3F, 0x2A); 
		break; 
	case MxL_BW_8MHz: /*8MHz */
			SetIRVBit(IRV_RFTune, 0x0C, 0x3F, 0x3F); 
		break; 
	} 
 
 
	/*Convert RF frequency into 16 bits => 10 bit integer (MHz) + 6 bit fraction */
	dig_rf_freq = RF_Freq / MHz; 
	temp = RF_Freq % MHz; 
	for(i=0; i<6; i++) 
	{ 
		dig_rf_freq <<= 1; 
		frac_divider /=2; 
		if(temp > frac_divider) 
		{ 
			temp -= frac_divider; 
			dig_rf_freq++; 
		} 
	} 
 
	/*add to have shift center point by 7.8124 kHz */
	if(temp > 7812) 
		dig_rf_freq ++; 
    	 
	SetIRVBit(IRV_RFTune, 0x0D, 0xFF, (U8)dig_rf_freq); 
	SetIRVBit(IRV_RFTune, 0x0E, 0xFF, (U8)(dig_rf_freq>>8)); 
 
	if (RF_Freq >=333*MHz) 
		SetIRVBit(IRV_RFTune, 0x80, 0x40, 0x40); 
 
	/*Generate one Array that Contain Data, Address  */
	while (IRV_RFTune[Reg_Index].Num || IRV_RFTune[Reg_Index].Val) 
	{ 
		pArray[Array_Index++] = IRV_RFTune[Reg_Index].Num; 
		pArray[Array_Index++] = IRV_RFTune[Reg_Index].Val; 
		Reg_Index++; 
	} 
     
	*Array_Size=Array_Index; 
	 
	return 0; 
} 
 
/**																		    
 * Tuner Functions								 
 */
U32 MxL5007_Set_Register(MxL5007_TunerConfigS* myTuner, U8 RegAddr, U8 RegData) 
{ 
	U32 Status=0; 
	U8 pArray[2]; 
	pArray[0] = RegAddr; 
	pArray[1] = RegData; 
	Status = MxL5007_I2C_Write(myTuner, pArray, 2); 
	if(Status) return MxL_ERR_SET_REG; 
 
	return MxL_OK; 
 
} 
 
U32 MxL5007_Get_Register(MxL5007_TunerConfigS* myTuner, U8 RegAddr, U8 *RegData) 
{ 
	if(MxL5007_I2C_Read(myTuner, RegAddr, RegData)) 
		return MxL_ERR_GET_REG; 
	return MxL_OK; 
 
} 
 
U32 MxL5007_Soft_Reset(MxL5007_TunerConfigS* myTuner) 
{ 
    U8 pArray[2];   /* a array pointer that store the addr and data pairs for I2C write */    
      
    pArray[0] = 0xFF;  
    pArray[1] = 0xFF;  
  
    if(MxL5007_I2C_Write(myTuner, pArray, 2))  
        return MxL_ERR_OTHERS;  
  
    return MxL_OK;  
} 

U32 MxL5007_Loop_Through_On(MxL5007_TunerConfigS* myTuner, MxL5007_LoopThru isOn) 
{	 
	U8 pArray[2];	/* a array pointer that store the addr and data pairs for I2C write */
	 
	pArray[0]=0x04; 
	if(isOn) 
	 pArray[1]= 0x01; 
	else 
	 pArray[1]= 0x0; 
 
	if(MxL5007_I2C_Write(myTuner, pArray, 2)) 
		return MxL_ERR_OTHERS; 
 
	return MxL_OK; 
} 
 
U32 MxL5007_Stand_By(MxL5007_TunerConfigS* myTuner) 
{ 
	U8 pArray[4];	/* a array pointer that store the addr and data pairs for I2C write */	 
	 
	pArray[0] = 0x01; 
	pArray[1] = 0x0; 
	pArray[2] = 0x0F; 
	pArray[3] = 0x0; 
 
	if(MxL5007_I2C_Write(myTuner, pArray, 4)) 
		return MxL_ERR_OTHERS; 
 
	return MxL_OK; 
} 
 
U32 MxL5007_Wake_Up(MxL5007_TunerConfigS* myTuner) 
{ 
	U8 pArray[2];	/* a array pointer that store the addr and data pairs for I2C write */	 
	 
	pArray[0] = 0x01; 
	pArray[1] = 0x01; 
 
	if(MxL5007_I2C_Write(myTuner, pArray, 2)) 
		return MxL_ERR_OTHERS; 
 
	if(MxL5007_Tuner_RFTune(myTuner, myTuner->RF_Freq_Hz, myTuner->BW_MHz)) 
		return MxL_ERR_RFTUNE; 
 
	return MxL_OK; 
} 
 
U32 MxL5007_Tuner_Init(MxL5007_TunerConfigS* myTuner) 
{	 
	U8 pArray[MAX_ARRAY_SIZE];	/* a array pointer that store the addr and data pairs for I2C write */
	U32 Array_Size;			/* a integer pointer that store the number of element in above array */
 
	/*Soft reset tuner*/
	if(MxL5007_Soft_Reset(myTuner)) 
		return MxL_ERR_INIT; 
 
	/*perform initialization calculation*/
	MxL5007_Init(pArray, &Array_Size, (U8)myTuner->Mode, myTuner->IF_Diff_Out_Level, (U32)myTuner->Xtal_Freq,  
				(U32)myTuner->IF_Freq, (U8)myTuner->IF_Spectrum, (U8)myTuner->ClkOut_Setting, (U8)myTuner->ClkOut_Amp); 
 
	/*perform I2C write here*/
	if(MxL5007_I2C_Write(myTuner, pArray, Array_Size)) 
		return MxL_ERR_INIT; 
 
	return MxL_OK; 
} 
 
 
U32 MxL5007_Tuner_RFTune(MxL5007_TunerConfigS* myTuner, U32 RF_Freq_Hz, MxL5007_BW_MHz BWMHz) 
{ 
	U8 pArray[MAX_ARRAY_SIZE];	
	U32 Array_Size;
 
	/*Store information into struc */
	myTuner->RF_Freq_Hz = RF_Freq_Hz; 
	myTuner->BW_MHz = BWMHz; 
 
	/*perform Channel Change calculation */
	MxL5007_RFTune(pArray,&Array_Size,RF_Freq_Hz,(U8)BWMHz); 
 
	/*perform I2C write here */
	if(MxL5007_I2C_Write(myTuner, pArray, Array_Size)) 
		return MxL_ERR_RFTUNE; 
 
	/*wait for 3ms */
	semosal_wait_msec(3);  
 
	return MxL_OK; 
} 
 
U32 MxL5007_Check_ChipVersion(MxL5007_TunerConfigS* myTuner, U32* pid) 
{	 
	U8 Data; 

	if(MxL5007_I2C_Read(myTuner, 0xD9, &Data)) 
		return MxL_ERR_OTHERS; 
		 
	switch(Data) 
	{ 
	case 0x14:  
//            *pid=MK_U32(SEMID_ENCA('m','x','l'),SEMID_ENCI(5007,0));  
			*pid = Data;
            break; 
 
	default:  
            *pid=0; break; 
	}	 
 
        return MxL_OK; 
} 
 
U32 MxL5007_RFSynth_Lock_Status(MxL5007_TunerConfigS* myTuner, BOOL* isLock) 
{	 
	U8 Data; 

	*isLock = FALSE;  
	if(MxL5007_I2C_Read(myTuner, 0xD8, &Data)) 
		return MxL_ERR_OTHERS; 
	Data &= 0x0C; 
	if (Data == 0x0C) 
		*isLock = TRUE;  /*RF Synthesizer is Lock*/	 
	return MxL_OK; 
} 
 
U32 MxL5007_REFSynth_Lock_Status(MxL5007_TunerConfigS* myTuner, BOOL* isLock) 
{ 
	U8 Data; 

	*isLock = FALSE;  
	if(MxL5007_I2C_Read(myTuner, 0xD8, &Data)) 
		return MxL_ERR_OTHERS; 
	Data &= 0x03; 
	if (Data == 0x03) 
		*isLock = TRUE;   /*REF Synthesizer is Lock*/ 
	return MxL_OK; 
} 

EXPORT_SYMBOL(tuner_adapter);
