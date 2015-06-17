/** 
*         (C) Copyright 2008-2009 Samsung Electro-Mechanics CO., LTD. 
* 
* PROPRIETARY RIGHTS of Samsung Electro-Mechanics are involved in the subject  
* matter of this material.  All manufacturing, reproduction, use, and sales  
* rights pertaining to this subject matter are governed by the license agreement. 
* The recipient of this software implicitly accepts the terms of the license. 
* 
*/ 
 
/** 
* 
* @file			sembsl_mxl5007ce6353_local.h 
* @brief		Defines MIN MAX constants and local structures for MXL201 RF + CE6353 DEMOD frontend 
* 
* @version	        1.00.004
* @author	        Alexander Loktionov (alex.loktionov@samsung.com) 
* @note                 Should be included only once! in sembsl_mxl5007ce6353.c 
* 
* Created 2009.05.06 
* 
*/ 
 
 
#ifndef _SEMBSL_MXL5007CE6353_LOCAL_H_ 
#define _SEMBSL_MXL5007CE6353_LOCAL_H_ 
 
  
#define DNOD22QZV102A            1  
/*#define  DNOD44CDV085A          1*/
 

/*
 * Constants 
 */ 
#define SEMBSL_MXL5007CE6353_MAJOR_VER	1 
#define SEMBSL_MXL5007CE6353_MINOR_VER	0 
#define SEMBSL_MXL5007CE6353_BUILD_NUM	7
 
 
 
#define MXL5007CE6353_TO_AGC           530                
#define MXL5007CE6353_TO_LOCK          1100                
 
#define MXL5007CE6353_BW               8000000 
#define MXL5007CE6353_MOD              SEMHAL_FE_QAM_AUTO 
#define MXL5007CE6353_STANDARD         SEMHAL_FE_DVB_T 
#define MXL5007CE6353_SR_MIN           5699000 
#define MXL5007CE6353_SR_MAX           7610000  
 
#ifdef DNOD22QZV102A
#define SEMBSL_MXL5007CE6353_MAX_UNITS	1 
#define MXL5007CE6353_IF               4570000 
#define MXL5007CE6353_RFXTAL           20480000  
#define MXL5007CE6353_RFCLKOUT              1 
#define MXL5007CE6353_AGC_TRGT            0x30 
#define TUNER_I2C_ADDRESSS0              0xC0     
#define DEMOD_I2C_ADDRESSS0              0x26  
#define TUNER_I2C_ADDRESSS1              0xC0     
#define DEMOD_I2C_ADDRESSS1              0x1A  
#endif 
 
#ifdef DNOD44CDV085A 
#define SEMBSL_MXL5007CE6353_MAX_UNITS	1 
#define MXL5007CE6353_IF               36150000  
#define MXL5007CE6353_RFXTAL           24000000 
#define MXL5007CE6353_RFCLKOUT              0 
#define MXL5007CE6353_AGC_TRGT            0x15 
#define TUNER_I2C_ADDRESSS0              0xC0     
#define DEMOD_I2C_ADDRESSS0              0x1E  
#define TUNER_I2C_ADDRESSS1              0xC0     
#define DEMOD_I2C_ADDRESSS1              0x1E  
#endif 

#define MXL5007CE6353_PR               1        /* 0-Low priority 1-high priority 2-super high 3-super duper high*/ 
 
 
/**
 * Structures 
 */ 
struct sembsl_fe_search_params_s 
{ 
    U32                     freq;               /* I: Frequency in Khz          SCAN_START */ 
    U32                     range;		/* I: +/- frequency in  Hz!     SCAN_STEP */ 
    U32                     bw;		        /* I: bandwidth */ 
    U32                     modulation;         /* I: transponder type */ 
    U32                     algo;               /* I: search */ 
    U32                     pr;                 /* I: priority */ 
}; 
 
 
#define SEMBSL_FE_SEARCHING 1	 
#define SEMBSL_FE_INITED    2 
#define SEMBSL_FE_X_RF      4 
#define SEMBSL_FE_X_MOD     8 
#define SEMBSL_FE_X_SR      16 
#define SEMBSL_FE_X_BW      32 
#define SEMBSL_FE_X_STD     64 
#define SEMBSL_FE_LCKD      128	 
#define SEMBSL_FE_X_PR      256 
#define SEMBSL_FE_PWR       (~(SEMBSL_FE_INITED|SEMBSL_FE_X_STD|SEMBSL_FE_X_BW|SEMBSL_FE_X_RF)) 
	 
struct sembsl_mxl5007ce6353_obj 
{ 
	U32		    api_flags; 
 
        TICK_T              search_start; 
 
	struct  
        sembsl_fe_search_params_s   
                            search; 
 
        U32                 standard; 
        U32                 state; 
 
        struct  
        sembsl_ce6353_obj   demod; 
        MxL5007_TunerConfigS tuner; 
 
}; 
	 
/**
 * Module data 
 */ 
static struct  
sembsl_mxl5007ce6353_obj    m_mxl5007ce6353_instance[SEMBSL_MXL5007CE6353_MAX_UNITS]; 
 
 
#endif /* #ifndef _SEMBSL_MXL5007CE6353_LOCAL_H_ */
