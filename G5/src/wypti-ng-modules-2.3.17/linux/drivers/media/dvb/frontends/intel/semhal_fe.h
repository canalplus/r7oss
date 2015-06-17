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
* @file			semhal_fe_cfg_id.h 
* @brief		Defines config item ID's for all types (terestrial,cable,satellite,handheld) tuner frontends. For the SEMCO common STB platform API  
* 
* @version	        1.00.004 
* @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
* @document             Common STB platform API v1.5 
* 
* Created 2009.02.12 
* 
*/ 
 
#ifndef _SEMHAL_FE_CFG_ID_H_ 
#define _SEMHAL_FE_CFG_ID_H_ 
 
 
/**
 * 
 * BSL Frontend interface 
 */ 
#define SEMHAL_FE_CFG_MASK 0x03FF                /* items ID mask */ 
#define SEMHAL_FE_STATE    0x0400                /* state flag */ 
#define SEMHAL_FE_TUNER    0x0800                /* tuner IC level flag */ 
#define SEMHAL_FE_DEMOD    0x1000                /* demod IC level flag */ 
 
/** 
 * frontend/demod configuration parameters  
 * (they usualy need SEMHAL_FE_CFG_INIT afterwards) 
 */ 
#define SEMHAL_FE_CFG_BOARD     0 
#define SEMHAL_FE_STATE_BOARD   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_BOARD) 
 
#define SEMHAL_FE_CFG_STANDARD  (1|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_STANDARD   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_STANDARD) 
 
#define SEMHAL_FE_CFG_XTAL      3 
 
#define SEMHAL_FE_CFG_AGCIFMAX  8 
#define SEMHAL_FE_CFG_AGCIFMIN  9 
#define SEMHAL_FE_CFG_AGCRFMAX  10 
#define SEMHAL_FE_CFG_AGCRFMIN  11 
#define SEMHAL_FE_CFG_AGCTRESHOLD (12|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_AGCTRESHOLD (SEMHAL_FE_STATE|SEMHAL_FE_CFG_AGCTRESHOLD) 
 
#define SEMHAL_FE_CFG_EQUALTYPE  13             /* 0 -disable equalizer */ 
#define SEMHAL_FE_STATE_EQUALTYPE (SEMHAL_FE_STATE|SEMHAL_FE_CFG_EQUALTYPE) 
 
#define SEMHAL_FE_CFG_AGCIFPWM  14              /* 1-polarity 2-opendrain */ 
#define SEMHAL_FE_CFG_AGCRFPWM  15              /* 1-polarity 2-opendrain */ 
#define SEMHAL_FE_CFG_BERDEPTH  16 
#define SEMHAL_FE_CFG_BERWINDOW 17 
#define SEMHAL_FE_CFG_CLKOFFSETRANGE 18 
 
#define SEMHAL_FE_CFG_SI        (19|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD)  /* spectral inversion */ 
#define SEMHAL_FE_STATE_SI      (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SI) 
 
#define SEMHAL_FE_SPECTRUMAUTO      2 
#define SEMHAL_FE_SPECTRUMNORMAL    0 
#define SEMHAL_FE_SPECTRUMINVERTED  1 
 
#define SEMHAL_FE_CFG_IF        (20|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
 
#define SEMHAL_FE_CFG_OCLK1     21              /* 1st MPEG TS output clock 1 polarity */ 
#define SEMHAL_FE_CFG_OCLK2     26              /* 2nd MPEG output clock 2 polarity */ 
 
#define SEMHAL_FE_CFG_MODEABC1  24              /* MPEG TS mode A/B/C */ 
 
#define SEMHAL_FE_CFG_MSBFIRST1 23              /* 1st MPEG TS serial MSB first */ 
#define SEMHAL_FE_CFG_MSBFIRST2 27              /* 2nd MPEG TS serial MSB first */ 
 
#define SEMHAL_FE_CFG_SWDYN     28              /* demod. sweep dynamic */ 
#define SEMHAL_FE_STATE_SWDYN   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SWDYN) 
 
#define SEMHAL_FE_CFG_SWSTEP    29              /* demod. sweep step */ 
#define SEMHAL_FE_STATE_SWSTEP   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SWSTEP) 
 
#define SEMHAL_FE_CFG_SWLEN     30              /* demod. sweep len */ 
#define SEMHAL_FE_STATE_SWLEN   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SWLEN) 
 
#define SEMHAL_FE_CFG_SWRAMP    31              /* demod. sweep ramp */ 
#define SEMHAL_FE_STATE_SWRAMP  (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SWRAMP) 
 
#define SEMHAL_FE_CFG_SWPOS     32   
#define SEMHAL_FE_STATE_SWPOS  (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SWPOS) 
 
#define SEMHAL_FE_CFG_SR        (50|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD)/* set symbolrate */ 
#define SEMHAL_FE_STATE_SR      (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SR) 
 
/* 
 * 0  Device powered on      (D0 state) 
 * 1  Device power standby   (D1 state) 
 * 2  Device power suspended (D2 state) 
 * 3  Device powered off     (D3 state) 
 */ 
#define SEMHAL_FE_CFG_POWER     (51|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD)/* set powerstate D0 0,D1 1 D2 2, D3 3*/ 
#define SEMHAL_FE_STATE_POWER   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_POWER) 
 
 
#define SEMHAL_FE_CFG_CLKOUT    52              /* set XTAL clock out state */ 
#define SEMHAL_FE_STATE_CLKOUT  (SEMHAL_FE_STATE|SEMHAL_FE_CFG_CLKOUT) 
 
#define SEMHAL_FE_CFG_I2CADDR   53 
 
#define SEMHAL_FE_CFG_MODULATION (54|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_MODULATION (SEMHAL_FE_STATE|SEMHAL_FE_CFG_MODULATION) 
 
#define SEMHAL_FE_CFG_PLLMFACTOR (55|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_CFG_PLLNFACTOR (56|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_CFG_PLLPFACTOR (57|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_CFG_PLLPFACTOR (57|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_CFG_PLLFSAMPLING (58|SEMHAL_FE_DEMOD)  
/* ... */
 
#define SEMHAL_FE_CFG_INIT      (100|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_INIT   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_INIT) 
 
#define SEMHAL_FE_CFG_I2CSWITCH (101|SEMHAL_FE_DEMOD)             /* i2c loop through */ 
#define SEMHAL_FE_STATE_I2CSWITCH (SEMHAL_FE_STATE|SEMHAL_FE_CFG_I2CSWITCH) 
 
#define SEMHAL_FE_STATE_RESET   (SEMHAL_FE_STATE|SEMHAL_FE_DEMOD|102) 
 
#define SEMHAL_FE_STATE_TRACKING   (SEMHAL_FE_STATE|SEMHAL_FE_DEMOD|103)  
 
/** 
 * demod read state values  
 */ 
#define SEMHAL_FE_CFG_IDENTITY  (102) /* read 1st part of identity */ 
#define SEMHAL_FE_STATE_IDENTITY (SEMHAL_FE_STATE|SEMHAL_FE_CFG_IDENTITY) 
     
#define SEMHAL_FE_CFG_LOCK      (103|SEMHAL_FE_TUNER|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_LOCK    (SEMHAL_FE_STATE|SEMHAL_FE_CFG_LOCK) 
 
#define SEMHAL_FE_CFG_IQ        (104SEMHAL_FE_DEMOD)/* read IQ */ 
#define SEMHAL_FE_STATE_IQ   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_IQ) 
 
#define SEMHAL_FE_CFG_AGC       (105|SEMHAL_FE_DEMOD)/* read AGC */ 
#define SEMHAL_FE_STATE_AGC   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_AGC) 
 
#define SEMHAL_FE_CFG_POSTBER       (109|SEMHAL_FE_DEMOD)/* read BER, for DVB-S2 PER */ 
#define SEMHAL_FE_STATE_POSTBER   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_POSTBER) 
 
#define SEMHAL_FE_CFG_PREBER       (110|SEMHAL_FE_DEMOD)/* read BER, for DVB-S2 PER */ 
#define SEMHAL_FE_STATE_PREBER   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_PREBER) 
 
#define SEMHAL_FE_CFG_DSPVER    114 /* read DSP version */ 
#define SEMHAL_FE_STATE_DSPVER (SEMHAL_FE_STATE|SEMHAL_FE_CFG_DSPVER) 
 
#define SEMHAL_FE_STATE_MSE     (127|SEMHAL_FE_DEMOD)/* read mean square error from demod */ 
 
#define SEMHAL_FE_CFG_READCOEF  (128|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_READCOEF (SEMHAL_FE_STATE|SEMHAL_FE_CFG_READCOEF) 
 
#define SEMHAL_FE_CFG_STARTEQUAL (129|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_STARTEQUAL   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_STARTEQUAL) 
 
#define SEMHAL_FE_CFG_STOPEQUAL  (130|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_STOPEQUAL   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_STOPEQUAL) 
 
#define SEMHAL_FE_CFG_SNR_DBX100        (150|SEMHAL_FE_DEMOD)/* read snr */ 
#define SEMHAL_FE_STATE_SNR_DBX100   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_SNR_DBX100) 
 
#define SEMHAL_FE_CFG_UCB        (151|SEMHAL_FE_DEMOD)/* read uncorrected block count */ 
#define SEMHAL_FE_STATE_UCB   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_UCB) 
 
#define SEMHAL_FE_STATE_AFC     (SEMHAL_FE_STATE|152|SEMHAL_FE_DEMOD) /* !SIGNED! frequency offset in kHz*/ 
 
#define SEMHAL_FE_CFG_IDENTITY2  (153)/* read second part of identity */ 
#define SEMHAL_FE_STATE_IDENTITY2   (SEMHAL_FE_STATE|SEMHAL_FE_CFG_IDENTITY2) 
 
#define SEMHAL_FE_STATE_PILOTS   (SEMHAL_FE_STATE|154|SEMHAL_FE_DEMOD) /*  DVB-S2 pilots on/off */ 
#define SEMHAL_FE_STATE_FRAMELEN (SEMHAL_FE_STATE|155|SEMHAL_FE_DEMOD) /*  DVB-S2 frame length */ 
 
#define SEMHAL_FE_CFG_RFAGC_INT (156|SEMHAL_FE_TUNER)/* internal RF AGC (RF chip)*/ 
#define SEMHAL_FE_CFG_IFAGC_INT (157|SEMHAL_FE_TUNER)/* internal RF AGC (RF chip)*/ 
#define SEMHAL_FE_CFG_LT        (158|SEMHAL_FE_TUNER)/* RF loop  through*/ 
#define SEMHAL_FE_STATE_LT      (SEMHAL_FE_STATE|SEMHAL_FE_CFG_LT); 
 
#define SEMHAL_FE_STATE_REG (SEMHAL_FE_STATE|159) 
#define SEMHAL_FE_STATE_DEMOD_REG (SEMHAL_FE_STATE|159|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_TUNER_REG (SEMHAL_FE_STATE|159|SEMHAL_FE_TUNER) 
 
#define SEMHAL_FE_STATE_FFT (SEMHAL_FE_STATE|160|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_GI  (SEMHAL_FE_STATE|161|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_CR  (SEMHAL_FE_STATE|162|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_CFG_PR    (163|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_HR  (SEMHAL_FE_STATE|163|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_SRLP (SEMHAL_FE_STATE|164|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_CRLP (SEMHAL_FE_STATE|165|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_PREBERLP (SEMHAL_FE_STATE|166|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_POSTBERLP (SEMHAL_FE_STATE|167|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_UCBLP (SEMHAL_FE_STATE|168|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_MODULATIONLP (SEMHAL_FE_STATE|169|SEMHAL_FE_DEMOD) 
#define SEMHAL_FE_STATE_SYNC (SEMHAL_FE_STATE|170) 
/* ... */
 
/** 
 * Hardcoded values 
 */ 
#define SEMHAL_FE_CFG_NBOFUNIT  200             /* get maximum number of units */  
#define SEMHAL_FE_CFG_TYPE      201             /* frontend type (see semhal_frontend_info_s.type)*/ 
#define SEMHAL_FE_CFG_FREQMIN   202             /* minimum frequency in kiloherz */   
#define SEMHAL_FE_CFG_FREQMAX   203             /* maximum frequency in kiloherz */   
#define SEMHAL_FE_CFG_FREQSTEP  204             /* frequency step in kiloherz */   
#define SEMHAL_FE_CFG_BWMIN     205             /* minimum bandwidth in kiloherz */   
#define SEMHAL_FE_CFG_BWMAX     206              
#define SEMHAL_FE_CFG_BWSTEP    207 
/* ... */
 
/* 
 * tuner related things 
 */ 
#define SEMHAL_FE_CFG_BW        300             /* bandwidth */ 
#define SEMHAL_FE_STATE_BW      (SEMHAL_FE_STATE|SEMHAL_FE_CFG_BW) 
 
#define SEMHAL_FE_STATE_RSSI_DBMX100 (SEMHAL_FE_STATE|301)  /* !SIGNED! dBm multiplied by 100 */ 
 
#define SEMHAL_FE_STATE_RFRSSI_DBMX100 (SEMHAL_FE_STATE|302)  /* !SIGNED! dBm multiplied by 100 */ 
 
#define SEMHAL_FE_CFG_RF        (303|SEMHAL_FE_TUNER)/* frequency in kiloherz */   
#define SEMHAL_FE_STATE_RF      (SEMHAL_FE_STATE|SEMHAL_FE_CFG_RF) 
 
#define SEMHAL_FE_CFG_MODE      304             /* RF/DEMOD mode mainly RF chip chip maker  
                                                   can support lightly different settings 
                                                   for tuning different transponders 
                                                   CABLE/OFFAIR,VSB/QAM/OFDM,ATSC/DVBC/DOCSIS  
                                                 */   
#define SEMHAL_FE_STATE_MODE    (SEMHAL_FE_STATE|SEMHAL_FE_CFG_MODE) 
 
 
/** 
 * FE search 
 */ 
#define SEMHAL_FE_SEARCH_AUTO   0xFFFFFFFF 
#define SEMHAL_FE_SEARCH_BLIND  0               /* everything is unknown */ 
#define SEMHAL_FE_SEARCH_COLD   1               /* only the SR is known */ 
#define SEMHAL_FE_SEARCH_WARM   2               /* offset freq and SR are known */ 
 
/** 
 * FE lock status (SEMHAL_FE_STATE_LOCK) 
 */ 
#define SEMHAL_FE_HAS_SIGNAL       0x0001       /* found something above the noise level */ 
#define SEMHAL_FE_HAS_CARRIER      0x0002       /* carrier lock */ 
#define SEMHAL_FE_HAS_VITERBI      0x0004       /* FEC is stable */ 
#define SEMHAL_FE_HAS_SYNC         0x0008       /* found sync bytes */ 
#define SEMHAL_FE_HAS_LOCK         0x0010       /* Tuner+Demod work Ok */ 
#define SEMHAL_FE_TIMEDOUT         0x0020       /* no lock within the last ~2 seconds */ 
#define SEMHAL_FE_REINIT           0x0040       /* frontend was reinitialized, application is  
                                                   recomended to reset DiSEqC, tone again */ 

#endif /* #ifndef _SEMHAL_FE_CFG_ID_H_ */
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
* @file			semhal_fe_caps.h 
* @brief		Front end capabilities constants (Modulation,CodeRate)
* 
* @version	        1.00.005
* @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
* @document             Common STB platform API v1.5 
* 
* Created 2009.02.12 
* 
*/ 
 
#ifndef _SEMHAL_FE_CAPS_H_ 
#define _SEMHAL_FE_CAPS_H_ 
 
 
 
/** 
 * front-end capabilities structure and definitions 
 */ 
 
/**
 * type T/C/S/H 
 * type(standard) 
 */
#define SEMHAL_FE_DVB_S        (1 << 0)            /* DVB-S frontend with QPSK modulation */ 
#define SEMHAL_FE_DVB_C        (1 << 1)            /* DVB-C frontend with QAM modulation */ 
#define SEMHAL_FE_DVB_T        (1 << 2)            /* DVB-T frontend with OFDM modulation */ 
#define SEMHAL_FE_DVB_H        (1 << 3)            /* DVB-H frontend with OFDM modulation */ 
#define SEMHAL_FE_DVB_S2       (1 << 4) 
#define SEMHAL_FE_MCNS         (1 << 5) 
#define SEMHAL_FE_ATSC         (1 << 6) 
#define SEMHAL_FE_ISDBT        (1 << 7) 
#define SEMHAL_FE_ABS_S        (1 << 8) 
 
/**
 * available forward error correction code rates 
 * caps_fec 
 */
#define SEMHAL_FE_FEC_NONE     (1 << 0) 
#define SEMHAL_FE_FEC_1_2      (1 << 1) 
#define SEMHAL_FE_FEC_2_3      (1 << 2) 
#define SEMHAL_FE_FEC_3_4      (1 << 3) 
#define SEMHAL_FE_FEC_4_5      (1 << 4) 
#define SEMHAL_FE_FEC_5_6      (1 << 5) 
#define SEMHAL_FE_FEC_6_7      (1 << 6) 
#define SEMHAL_FE_FEC_7_8      (1 << 7) 
#define SEMHAL_FE_FEC_8_9      (1 << 8) 
#define SEMHAL_FE_FEC_AUTO     (1 << 9) 
 
#define SEMHAL_FE_GI_NONE      (1 << 10) 
#define SEMHAL_FE_GI_1_32      (1 << 11) 
#define SEMHAL_FE_GI_1_16      (1 << 12) 
#define SEMHAL_FE_GI_1_8       (1 << 13) 
#define SEMHAL_FE_GI_1_4       (1 << 14) 
#define SEMHAL_FE_GI_1_64      (1 << 15) 
#define SEMHAL_FE_GI_1_128     (1 << 16) 
#define SEMHAL_FE_GI_1_256     (1 << 17) 
#define SEMHAL_FE_GI_AUTO      (1 << 18) 
 
#define SEMHAL_FE_FFT_2K       (1 << 19) 
#define SEMHAL_FE_FFT_4K       (1 << 20) 
#define SEMHAL_FE_FFT_8K       (1 << 21) 
#define SEMHAL_FE_FFT_AUTO     (1 << 22) 
 
#define SEMHAL_FE_HIERARCHY_NONE 0 
#define SEMHAL_FE_HIERARCHY_1  0x1 
#define SEMHAL_FE_HIERARCHY_2  0x2  
#define SEMHAL_FE_HIERARCHY_4  0x4 
 

#define SEMHAL_FE_HP            1               /* High priority */
#define SEMHAL_FE_LP            0               /* Low priority */


/**
 * available modulation types 
 * caps_modulation 
 */
#define SEMHAL_FE_QPSK         (1 << 0) 
#define SEMHAL_FE_QAM_4        SEMHAL_FE_QPSK 
#define SEMHAL_FE_QAM_16       (1 << 1) 
#define SEMHAL_FE_QAM_32       (1 << 2) 
#define SEMHAL_FE_QAM_64       (1 << 3) 
#define SEMHAL_FE_QAM_128      (1 << 4) 
#define SEMHAL_FE_QAM_256      (1 << 5) 
#define SEMHAL_FE_QAM_AUTO     (1 << 6) 
#define SEMHAL_FE_TRANSMISSION_MODE_AUTO   (1 << 7) 
#define SEMHAL_FE_BANDWIDTH_AUTO	        (1 << 8) 
#define SEMHAL_FE_GUARD_INTERVAL_AUTO	(1 << 9) 
#define SEMHAL_FE_HIERARCHY_AUTO		(1 << 10) 
#define SEMHAL_FE_8VSB	    (1 << 11) 
#define SEMHAL_FE_16VSB	    (1 << 12) 
#define SEMHAL_FE_8PSK         (1 << 13)     
#define SEMHAL_FE_16APSK       (1 << 14)     
#define SEMHAL_FE_32APSK       (1 << 15)     
#define SEMHAL_FE_DPSK         (1 << 16) 
 
/** 
 * common supported frontend capabilities. 
 * caps_common 
 */
#define SEMHAL_FE_CAN_INVERSION_AUTO       (1 << 0) /* can detect spectrum inversion automatically */ 
#define SEMHAL_FE_CAN_RECOVER              (1 << 1) /* can recover from a cable unplug automatically */ 
#define SEMHAL_FE_CAN_MUTE_TS              (1 << 2) /* can stop spurious TS data output */ 
#define SEMHAL_FE_SUP_HIGH_LNB_VOLTAGE     (1 << 3) /* can deliver higher lnb voltages */ 

/** 
 * other available, frontend type dependent capabilities 
 * other_cap  
 */
#define SEMHAL_FE_CAN_TRANSMISSION_MODE_AUTO   (1 << 0) /*(DVB-T specific) */ 
#define SEMHAL_FE_CAN_BANDWIDTH_AUTO           (1 << 1) /*(DVB-T specific) */ 
#define SEMHAL_FE_CAN_GUARD_INTERVAL_AUTO      (1 << 2) /*(DVB-T specific) */ 
#define SEMHAL_FE_CAN_HIERARCHY_AUTO           (1 << 3) /*(DVB-T specific) */ 

 
 
 
#endif /* #ifndef _SEMHAL_FE_H_ */
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
* @file			semhal_fe_types.h 
* @brief		Defines hardware independant interface for all types (terestrial,cable,satellite,handheld) tuner frontends. For the SEMCO common STB platform API  
* 
* @version	        1.00.003 
* @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
* @document             Common STB platform API v1.5 
* 
* Created 2009.02.12 
* 
*/ 
 
#ifndef _SEMHAL_FE_TYPES_H_ 
#define _SEMHAL_FE_TYPES_H_ 
 
 

/**
 *  
 * BSL Frontend initialization structures 
 * used to bind HAL to specific OS 
 */ 
 
struct semhal_fe_ops; 

 
struct semhal_diseqc_ops 
{ 
    /** 
     * DISECC interface 
     */ 
    int 
    (*tx)                                       /* send bytes to slave */ 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number */ 
            U8                  *p_bytes,	/* I: bytes */ 
            U32                 p_num           /* I: number of bytes */ 
        ); 
 
    int 
    (*rx)                                       /* receive bytes from slave  
                                                   returns  
                                                   SEM_PENDING if receiving is not finished yet 
                                                   SEM_OK if 0 or more bytes received 
                                                 */ 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number */ 
            U8                  *p_bytes,	/* O: bytes */ 
            U32                 *p_num          /* O: number of bytes */ 
        ); 
 
    int 
    (*set_tone)                                 /* set continous 22KHz tone ON/OFF */ 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number */ 
            BOOL                on	        /* I: ON/OFF */ 
        ); 
}; 
 
struct semosal_i2c_obj;                         /* I2C abstract interface, defined in semosal.h */ 
struct semhal_frontend_info_s;                  /* tuner type and capabilities info , defined in this file */ 
 
struct semhal_fe_ops 
{ 
    /** 
     * init functions 
     */ 
    int 
    (*init) 
    	( 
            U32	            unit,    	        /* I: frontend unit number */ 
            struct  
            semosal_i2c_obj *p_i2c,             /* I: I2C adapter interface */ 
            U32             *p_cfg_values,	/* I: array of cfg. item_id,value pairs */ 
            U32             cfg_num             /* I: number of elements in p_cfg_values array */ 
        ); 
     
    int 
    (*deinit)                                   /* releases all allocated memory and semaphores */ 
    	( 
            U32	                frontend_unit	/* I: frontend unit number */ 
        ); 
 
    int 
    (*get_info) 
	( 
	    U32                 frontend_unit,	/* I: frontend unit number  */
	    struct semhal_frontend_info_s  
				*p_fe_info,     /* O: frontend info and capabilities */
	    U32                 *p_info_size	/* IO: sizeof() frontend info structure	*/
        ); 

    int 
    (*get_sw_version) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number        */ 
            U32                 *pu_ver		/* O: SW version 31..24 major,    */
                                                /*    23..16 -minor, 15..0 -build */
        ); 
 
    /** 
     * tuning functions 
     */ 
    int 
    (*search_start)                          	/* set parameters for search for fe_search_algo() */  
	( 
            U32         	frontend_unit	/* I: frontend unit number */ 
    	); 
    int 
    (*search_algo)                           	/* actual search function, must be called in loop */ 
	( 
            U32         	frontend_unit,	/* I: frontend unit number */ 
            U32                 *p_fe_status,	/* O: status bit mask.  
						      see SEMHAL_FE_STATE_LOCK 
                                                 */ 
            U32                 *p_delay        /* O: delay before next step */ 
	); 
    int 
    (*search_abort)                          /* recomended to call to reduce power usage */ 
	( 
            U32         	frontend_unit	/* I: frontend unit number */ 
	); 
 
 
    /** 
     * set and get functions 
     */ 
    int 
    (*get_cfg) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
	    U32                 item_id,	/* I: identifier of the item */ 
	    U32                 *p_val		/* O: returned value         */
        ); 
    int 
    (*set_cfg) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
	    U32                 item_id,	/* I: identifier of the item */
            U32	                val		/* I: input value            */
        ); 
    int 
    (*get_state) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
	    U32                 item_id,	/* I: identifier of the item */
	    U32                 *p_val		/* O: returned value         */
        ); 
    int 
    (*set_state) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
	    U32                 item_id,	/* I: identifier of the item */
            U32	                val		/* I: input value            */
        ); 
 
    int 
    (*get_cfg_bulk) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
            U32                 *p_cfg_values,	/* O: array of cfg. item_id,value pairs */
            U32                 cfg_num         /* I: number of elements in p_cfg_values array*/
        ); 
    int 
    (*set_cfg_bulk) 
        ( 
            U32         	frontend_unit,	/* I: frontend unit number   */
            U32                 *p_cfg_values,	/* I: array of cfg. item_id,value pairs */
            U32                 cfg_num         /* I: number of elements in p_cfg_values array*/
        ); 
 
    struct  
    semhal_diseqc_ops           diseqc; 
}; 

 
struct semhal_frontend_info_s  
{ 
    char name[128];		                /* model name */ 
    U32	type;			                /* S/C/T/H */ 
    U32	frequency_min; 
    U32	frequency_max; 
    U32	frequency_stepsize; 
    U32	frequency_tolerance; 
    U32	symbol_rate_min; 
    U32	symbol_rate_max; 
    U32	symbol_rate_tolerance;	                /* ppm */ 
    U32	notifier_delay;                         /* depricated */ 
 
    U32	caps_fec;		                /* capabilities mask */ 
    U32	caps_modulation; 
    U32	caps_common; 
    U32	caps_other; 
}; 
 
 
struct semhal_tp_info_dvbt_s 
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
 
struct semhal_tp_info_dvbc_s 
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
 
struct semhal_tp_info_rf_s 
{ 
    U32                     freq;               /* KHz */ 
    S32                     rssi;               /* signal strength in dBm */ 
    BOOL                    lock1;              /* LO1 lock */ 
    BOOL                    lock2;              /* LO2 lock */ 
}; 
 
 
#endif /* #ifndef _SEMHAL_FE_TYPES_H_ */
