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
* @file			sembsl_ce6353.h 
* @brief		Implementation of CE6353 DEMOD  
* 
* @version	        1.00.001 
* @author	        Alexander Loktionov (alex.loktionov@samsung.com) aka OXY 
* 
* Created 2009.05.06
* 
*/ 
 
#ifndef _SEMBSL_CE6353_H_ 
#define _SEMBSL_CE6353_H_ 
 
 
/**
 * Constants 
 */ 
#define SEMBSL_CE6353_MAJOR_VER	1 
#define SEMBSL_CE6353_MINOR_VER	0 
#define SEMBSL_CE6353_BUILD_NUM	2 
 
/**
 * Abstract demodulator interface 
 */ 
 
struct sembsl_ce6353_cfg 
{ 
        U32                 if_freq; 
        U32                 bw;  
        U32                 agc_trgt;  
        U8                  out_par_ser;        /* parallel/serial mode */ 
}; 
 
struct sembsl_ce6353_state 
{ 
        U32                 ucb; 
        U32                 tps;  
        U32                 status;
        U32                 hr; 
}; 
 
struct sembsl_ce6353_obj 
{ 
        struct  
        semosal_i2c_obj     i2c; 
        struct  
        sembsl_ce6353_cfg   cfg; 
        struct  
        sembsl_ce6353_state state; 
}; 
 
int 
sembsl_ce6353_init 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        struct  
        semosal_i2c_obj     *p_i2c_adapter, 
        U32                 i2c_addr, 
        struct  
        semosal_i2c_obj     *tuner_i2c          /* O: I2C interface to access tuner*/ 
    ); 
 
int 
sembsl_ce6353_deinit 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj 
    ); 
 
     
 
int 
sembsl_ce6353_get_sw_version 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 *pu_ver		/* O: SW version 31..24 major, 
                                                      23..16 -minor, 15..0 -build */ 
    ); 
 
/** 
 * set and get functions 
 */ 
int 
sembsl_ce6353_get 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
	U32                 item_id,	        /* I: identifier of the item */ 
	U32                 *p_val		/* O: returned value */ 
    ); 
 
int 
sembsl_ce6353_set 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
	U32                 item_id,	        /* I: identifier of the item */ 
        U32	            val		        /* I: input value */ 
    ); 
 
 
 
 
 
#endif /* #ifndef _SEMBSL_CE6353_H_ */
