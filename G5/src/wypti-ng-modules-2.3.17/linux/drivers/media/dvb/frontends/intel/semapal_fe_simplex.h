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
 * @file		semapal_fe_simplex.h 
 * @brief		Application interface for all types (terestrial,cable,satellite,handheld) SEMCO tuner frontends.   
 * 
 * @version	        1.04.003 
 * @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
 * @document            Common STB platform API v1.5 
 * 
 * @note		A customer coud modify this file only once. And then use it as a common interface for all SEMCO frontends. 
 * 
 * Created 2009.03.06 
 * 
 */ 
 
  
#ifndef _SEMAPAL_FE_SIMPLEX_H_ 
#define _SEMAPAL_FE_SIMPLEX_H_ 
 

struct semapal_tp_info_dvbt_s 
{ 
    U32                     freq;               /* KHz */ 
    S32                     freq_offset;        /* Hz */ 
    S32                     rssi;               /* signal strength in dBm*100 */ 
    U32                     snr;                /* signal/noise ratio dB*100 */ 
    BOOL                    locked; 
    U32                     gi;                 /* guard interval */ 
    U32                     fft;                /* FFT */ 
    U32                     hr;                 /* hierarchy */ 
    U32                     mod_hp;             /* modulation */ 
    U32                     mod_lp;             /* modulation */ 
    U32                     sr_hp;              /* symbol rate high priority layer */ 
    U32                     sr_lp;              /* symbol rate low priority layer */ 
    U32                     cr_hp;              /* coderate high priority layer */ 
    U32                     cr_lp;              /* coderate low priority layer */ 
    U32                     pre_ber_hp;         /* multiplyed by 1E8==100000000 */ 
    U32                     post_ber_hp;        /* multiplyed by 1E8==100000000 */ 
    U32                     pre_ber_lp;         /* multiplyed by 1E8==100000000 */ 
    U32                     post_ber_lp;        /* multiplyed by 1E8==100000000 */ 
    U32                     ucb_hp;             /* uncorrected blocks high priority layer */ 
    U32                     ucb_lp;             /* uncorrected blocks low priority layer */ 
}; 
 
struct semapal_tp_info_dvbc_s 
{ 
    U32                     freq;               /* KHz */ 
    S32                     freq_offset;        /* Hz */ 
    S32                     rssi;               /* signal strength in dBm*100 */ 
    U32                     snr;                /* signal/noise ratio dB*100 */ 
    BOOL                    locked; 
    U32                     mod;                /* modulation */ 
    U32                     sr;                 /* symbol rate in bauds*/ 
    U32                     pre_ber;            /* multiplyed by 1E8==100000000 */ 
    U32                     post_ber;           /* multiplyed by 1E8==100000000 */ 
    U32                     ucb;                /* uncorrected blocks*/ 
}; 
 
struct semapal_tp_info_rf_s 
{ 
    U32                     freq;               /* KHz */ 
    S32                     rssi;               /* signal strength in dBm */ 
    BOOL                    lock1;              /* LO1 lock */ 
    BOOL                    lock2;              /* LO2 lock */ 
}; 
 
	 
/** 
 * SEMFE_Init() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_Init 
    (  
        U32                 unit,             
        struct  
        semosal_i2c_obj     *p_i2c              /* I: I2C adapter interface */ 
   ); 
 
 
/** 
 * SEMFE_DeInit() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_DeInit 
    (  
        U32                 unit
    ); 
 
 
/** 
 * SEMFE_GetSWVersion() 
 * 
 * @brief               Gets S/W version for specifyed front-end (unit) 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_GetSWVersion 
    (  
        U32                 unit, 
        U32                 *ver 
    ); 
 
 
int 
SEMFE_GetFEInfo 
    ( 
	U32                 unit,        	/* I: frontend unit number             */  
	struct semhal_frontend_info_s  
			    *p_fe_info,         /* O: frontend info and capabilities   */
	U32                 *p_info_size	/* IO: sizeof() frontend info structure*/	 
    ); 
 
 
/** 
 * SEMFE_Tune_C() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_Tune_C 
    ( 
        U32                 unit,             
        U32		    freq_khz,  
        U32                 symbol_rate,  
        U32                 modulation 
    ); 
 
 
/** 
 * SEMFE_Tune_T() 
 * 
 * @note                Terestrial NIM API 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_Tune_T 
    ( 
        U32                 unit,             
        U32		    freq_khz,  
        U32                 bw,  
        U32                 pr                  /* 0 - Low priority 1-High priority */ 
    ); 
 
/** 
 * SEMFE_GetLockStatus() 
 * 
 * @return		TRUE/FALSE 
 */ 
BOOL 
SEMFE_GetLockStatus 
    ( 
        U32                 unit 
    ); 
 
/** 
 * SEMFE_GetStrength() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetSignalStrength 
    (  
        U32                 unit,             
	U32 		    *pvalue  
    ); 
 
 
/** 
 * SEMFE_GetSNRQuality() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_GetSNRQuality 
    ( 
        U32                 unit,             
        U32		    *pvalue 	        /* O: */ 
    ); 
 
/** 
 * SEMFE_GetBERQuality() 
 * 
 * @brief               Gets Post Viterbi Before Reed Solomon BER. In % (lookup table) to display bar 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 *  
 * @note                if not locked provides 0 (see SEMFE_GetLockStatus()) 
 *     
 */ 
int 
SEMFE_GetBERQuality 
    ( 
        U32                 unit,             
        U32 		    *pvalue 	        /* O: */ 
    ); 
 
/** 
 * SEMFE_GetRFSignalStrength() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetRFSignalStrength 
    ( 
        U32                 unit,             
        U32		    *pvalue 		/* O: */ 
    ); 
 
/** 
 * SEMFE_SetPowerState() 
 * 
 * @param		mode		[IN]	0:powered on(D0 state) 1:power standby(D1 state) 2:power suspended(D2 state) 3:powered off(D3 state) 
 *
 * @brief		Is used to sleep/wakeup the front-end (i.e. control low/normal power consumption state).
 * @brief		You need it if your STB supports "green mode sleep" (low power use, less than 1Watt) 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_SetPowerState 
    ( 
        U32                 unit,             
        U32                 mode 
    ); 
 
 
/** 
 * SEMFE_GetTpInfo_DVBT() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetTpInfo_DVBT	 
    ( 
        U32                 unit,             
        struct  
        semapal_tp_info_dvbt_s *tp_info           /* O: (STB OS dependant) pointer to struct */ 
    ); 
 
/** 
 * SEMFE_GetTpInfo_DVBC 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetTpInfo_DVBC 
    ( 
        U32                 unit,             
        struct  
        semapal_tp_info_dvbc_s *tp_info           /* O: (STB OS dependant) pointer to struct */ 
    ); 
 
/** 
 * SEMFE_GetTpInfo_RF 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetTpInfo_RF 
    ( 
        U32                 unit,             
        struct  
        semapal_tp_info_rf_s *tp_info            /* O: (STB OS dependant) pointer to struct */ 
    ); 
 
 
/** 
 * SEMFE_Get 
 * 
 * @brief   Backdoor funtion to SEM HAL FE. For geting front-end parameters 
 */ 
int  
SEMFE_Get 
    ( 
        U32                 unit,               /* I: front-end unit number */             
        U32                 item,               /* I: config item ID (look semhal_fe.h) */ 
        U32                 *p_val              /* IO: value, usually output only */ 
    ); 
 
/** 
 * SEMFE_Set 
 * 
 * @brief   Backdoor funtion to SEM HAL FE. For seting front-end parameters 
 */ 
int  
SEMFE_Set 
    ( 
        U32                 unit,               /* I: front-end unit number */             
        U32                 item,               /* I: config item ID (look semhal_fe.h) */ 
        U32                 val                 /* I: value */ 
    ); 
 
 
 
#endif /* #ifndef _SEMAPAL_FE_SIMPLEX_H_ */
