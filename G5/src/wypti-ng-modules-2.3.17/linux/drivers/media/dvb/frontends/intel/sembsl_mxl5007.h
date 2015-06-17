/* 
  
 Driver APIs for MxL5007 Tuner 
  
 Copyright, Maxlinear, Inc. 
 All Rights Reserved 
  
 File Name:      MxL5007_API.h 
  
 */ 
#ifndef __MxL5007_API_H 
#define __MxL5007_API_H 

/******************************/ 
/*  MxL5007 Err message       */ 
/******************************/ 
#define MxL_OK			   0 
#define MxL_ERR_INIT		   1 
#define MxL_ERR_RFTUNE		   2 
#define MxL_ERR_SET_REG		   3 
#define MxL_ERR_GET_REG		   4 
#define MxL_ERR_OTHERS		   10 
 
/******************************/ 
/*  MxL5007 Chip verstion     */ 
/******************************/ 
typedef enum{ 
	MxL_UNKNOWN_ID		= 0x00, 
	MxL_5007T_V4		= 0x14, 
	MxL_GET_ID_FAIL		= 0xFF 
}MxL5007_ChipVersion; 
 
#ifndef MHz 
	#define MHz 1000000 
#endif 
 
#define MAX_ARRAY_SIZE 100 
 
 
/* Enumeration of Mode */
typedef enum  
{ 
	MxL_MODE_ISDBT = 0, 
	MxL_MODE_DVBT = 1, 
	MxL_MODE_ATSC = 2,	 
	MxL_MODE_CABLE = 0x10 
} MxL5007_Mode ; 
#define MxL_MODE_CAB_STD MxL_MODE_CABLE

typedef enum 
{ 
	MxL_IF_4_MHZ	  = 4000000, 
	MxL_IF_4_5_MHZ	  = 4500000, 
	MxL_IF_4_57_MHZ	  =	4570000, 
	MxL_IF_5_MHZ	  =	5000000, 
	MxL_IF_5_38_MHZ	  =	5380000, 
	MxL_IF_6_MHZ	  =	6000000, 
	MxL_IF_6_28_MHZ	  =	6280000, 
	MxL_IF_9_1915_MHZ =	9191500, 
	MxL_IF_35_25_MHZ  = 35250000, 
	MxL_IF_36_15_MHZ  = 36150000, 
	MxL_IF_44_MHZ	  = 44000000 
} MxL5007_IF_Freq ; 
 
typedef enum 
{ 
	MxL_XTAL_16_MHZ		= 16000000, 
	MxL_XTAL_20_MHZ		= 20000000, 
	MxL_XTAL_20_25_MHZ	= 20250000, 
	MxL_XTAL_20_48_MHZ	= 20480000, 
	MxL_XTAL_24_MHZ		= 24000000, 
	MxL_XTAL_25_MHZ		= 25000000, 
	MxL_XTAL_25_14_MHZ	= 25140000, 
	MxL_XTAL_27_MHZ		= 27000000, 
	MxL_XTAL_28_8_MHZ	= 28800000, 
	MxL_XTAL_32_MHZ		= 32000000, 
	MxL_XTAL_40_MHZ		= 40000000, 
	MxL_XTAL_44_MHZ		= 44000000, 
	MxL_XTAL_48_MHZ		= 48000000, 
	MxL_XTAL_49_3811_MHZ = 49381100	 
} MxL5007_Xtal_Freq ; 
 
typedef enum 
{ 
	MxL_BW_6MHz = 6, 
	MxL_BW_7MHz = 7, 
	MxL_BW_8MHz = 8 
} MxL5007_BW_MHz; 
 
typedef enum 
{ 
	MxL_NORMAL_IF = 0 , 
	MxL_INVERT_IF 
 
} MxL5007_IF_Spectrum ; 
 
typedef enum 
{ 
	MxL_LT_DISABLE = 0 , 
	MxL_LT_ENABLE 
 
} MxL5007_LoopThru ; 
 
typedef enum 
{ 
	MxL_CLKOUT_DISABLE = 0 , 
	MxL_CLKOUT_ENABLE 
 
} MxL5007_ClkOut; 
 
typedef enum 
{ 
	MxL_CLKOUT_AMP_0 = 0 , 
	MxL_CLKOUT_AMP_1, 
	MxL_CLKOUT_AMP_2, 
	MxL_CLKOUT_AMP_3, 
	MxL_CLKOUT_AMP_4, 
	MxL_CLKOUT_AMP_5, 
	MxL_CLKOUT_AMP_6, 
	MxL_CLKOUT_AMP_7 
} MxL5007_ClkOut_Amp; 
 
typedef enum 
{ 
	MxL_I2C_ADDR_96 = 96 , 
	MxL_I2C_ADDR_97 = 97 , 
	MxL_I2C_ADDR_98 = 98 , 
	MxL_I2C_ADDR_99 = 99 	 
} MxL5007_I2CAddr ; 
 
typedef struct _MxL5007_TunerConfigS 
{ 
    MxL5007_Mode	    Mode; 
    S32                     IF_Diff_Out_Level; 
    MxL5007_Xtal_Freq       Xtal_Freq; 
    MxL5007_IF_Freq	    IF_Freq; 
    MxL5007_IF_Spectrum     IF_Spectrum; 
    MxL5007_ClkOut	    ClkOut_Setting; 
    MxL5007_ClkOut_Amp	    ClkOut_Amp; 
    MxL5007_BW_MHz	    BW_MHz; 
    U32		            RF_Freq_Hz; 
    MxL5007_LoopThru        LoopThru_Setting; 
 
    struct  
    semosal_i2c_obj         i2c; 
} MxL5007_TunerConfigS; 

U32 MxL5007_Set_Register(MxL5007_TunerConfigS* myTuner, U8 RegAddr, U8 RegData); 
U32 MxL5007_Get_Register(MxL5007_TunerConfigS* myTuner, U8 RegAddr, U8 *RegData);
 
/****************************************************************************** 
** 
**  Name: MxL_Tuner_Init 
** 
**  Description:    MxL5007 Initialization 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_INIT if fail 
** 
******************************************************************************/ 
U32 MxL5007_Tuner_Init(MxL5007_TunerConfigS* ); 
 
/****************************************************************************** 
** 
**  Name: MxL_Tuner_RFTune 
** 
**  Description:    Frequency tunning for channel 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
**					RF_Freq_Hz			- RF Frequency in Hz 
**					BWMHz				- Bandwidth 6, 7 or 8 MHz 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_RFTUNE if fail 
** 
******************************************************************************/ 
U32 MxL5007_Tuner_RFTune(MxL5007_TunerConfigS*, U32 RF_Freq_Hz, MxL5007_BW_MHz BWMHz);		 
 
/****************************************************************************** 
** 
**  Name: MxL_Soft_Reset 
** 
**  Description:    Software Reset the MxL5007 Tuner 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail 
** 
******************************************************************************/ 
U32 MxL5007_Soft_Reset(MxL5007_TunerConfigS*); 
 
/****************************************************************************** 
** 
**  Name: MxL_Loop_Through_On 
** 
**  Description:    Turn On/Off on-chip Loop-through 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
**					isOn				- True to turn On Loop Through 
**										- False to turn off Loop Through 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail 
** 
******************************************************************************/ 
U32 MxL5007_Loop_Through_On(MxL5007_TunerConfigS*, MxL5007_LoopThru); 
 
/****************************************************************************** 
** 
**  Name: MxL_Standby 
** 
**  Description:    Enter Standby Mode 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail 
** 
******************************************************************************/ 
U32 MxL5007_Stand_By(MxL5007_TunerConfigS*); 
 
/****************************************************************************** 
** 
**  Name: MxL_Wakeup 
** 
**  Description:    Wakeup from Standby Mode (Note: after wake up, please call RF_Tune again) 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail 
** 
******************************************************************************/ 
U32 MxL5007_Wake_Up(MxL5007_TunerConfigS*); 
 
/****************************************************************************** 
** 
**  Name: MxL_Check_ChipVersion 
** 
**  Description:    Return the MxL5007 Chip ID 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
**			 
**  Returns:        MxL_ChipVersion			 
** 
******************************************************************************/ 
U32 MxL5007_Check_ChipVersion(MxL5007_TunerConfigS*, U32*); 
 
/****************************************************************************** 
** 
**  Name: MxL_RFSynth_Lock_Status 
** 
**  Description:    RF synthesizer lock status of MxL5007 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
**					isLock				- Pointer to Lock Status 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail 
** 
******************************************************************************/ 
U32 MxL5007_RFSynth_Lock_Status(MxL5007_TunerConfigS* , BOOL* isLock); 
 
/****************************************************************************** 
** 
**  Name: MxL_REFSynth_Lock_Status 
** 
**  Description:    REF synthesizer lock status of MxL5007 
** 
**  Parameters:    	myTuner				- Pointer to MxL5007_TunerConfigS 
**					isLock				- Pointer to Lock Status 
** 
**  Returns:        U32			- MxL_OK if success	 
**										- MxL_ERR_OTHERS if fail	 
** 
******************************************************************************/ 
U32 MxL5007_REFSynth_Lock_Status(MxL5007_TunerConfigS* , BOOL* isLock); 
 
#endif /* __MxL5007_API_H */

