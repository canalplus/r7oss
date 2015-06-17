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
 * @file		semapal_simplex.c 
 * @brief		Application interface wrapper for all types (terestrial,cable,satellite,handheld) SEMCO tuner frontends.   
 * 
 * @version	        1.04.011
 * @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
 * @document            Common STB platform API v1.5 
 * 
 * @note		A customer coud modify this file only once. And then use it as a common interface for all SEMCO frontends. 
 * 
 * Created 2009.02.24 
 * 
 */ 
 
#include "semstbcp.h"                    /* SEMCO STB CP API */ 
 
#include "semosal.h" 
#include "semhal_fe.h"                   /* abstract FE, enums, constants */ 
 
#include "semapal_fe_simplex.h" 
 
  
#define SEMAPAL_LOOKUPS_RANGE_0_100 
/*#define SEMAPAL_LOOKUPS_RANGE_0_255*/ 
 

#define THIS_FILE_ID        0xAA1 
#define THISFILE_TRACE      SEMDBG_TRACE_APAL 
 
 
/******************************!!! IMPORTANT !!!***********************************/ 
/*      ! CHANGE THIS LINES TO MAKE YOUR APP WORK WITH SEMCO FRONTEND YOU NEED !  */ 
 
/* DNQS44CPS101A DVB-C */ 
/* DNQN44CPS101A MCNS */ 
/* DNQS22CPS101A DVB-C */ 
/*
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mxl201tda10024.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl201tda10024 
}; 
*/

/* DTMD4PCDH085A DVB-C */ 
/* 
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mxl201svd1001.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl201svd1001 
}; 
*/
 
/* DTMD4PCDV085A DVB-C */ 
/* 
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_en4020svd1001.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_en4020svd1001 
}; 
*/ 
 
/* DTMT4PKUV302A DVB-C dual */ 
/* DTMT2NKUV302A MCNS  dual */ 
/* 
#define SEMAPAL_FE_MAX_UNITS 2 
#include "../src/sembsl_mt2063tda10024.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mt2063tda10024, 
    &g_sembsl_mt2063tda10024 
}; 
*/ 
 
/* DTH-200 DOCSIS */ 
/*
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mt2050tda10024.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mt2050tda10024, 
}; 
*/
 
 
/* DNOD886ZH262A DVB-T dual*/ 
/* DNOT44QZH262A DVB-T dual*/ 
/* 
#define SEMAPAL_FE_MAX_UNITS 2 
#include "../src/sembsl_mxl5007ce6250.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl5007ce6250, 
    &g_sembsl_mxl5007ce6250 
}; 
*/
 
/* DNOD22QZV102A DVB-T*/ 
/* DNOD44CDV085A DVB-T*/  

#define SEMAPAL_FE_MAX_UNITS 1 
#include "sembsl_mxl5007ce6353.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl5007ce6353 
}; 

 
/* DNOD44QSV102A ISDBT */ 
/* 
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mxl5007tc90517.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl5007tc90517 
}; 
*/ 

/* DNOD44QSV102A DVB-T*/ 
/*
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mxl5007s5h1432.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl5007s5h1432
}; 
*/

/* DNOT44CDV101A VSB/QAM dual half*/ 
/*
#define SEMAPAL_FE_MAX_UNITS 2 
#include "../src/sembsl_mxl5007s5h1411.h" 
static struct semhal_fe_ops  
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]= 
{ 
    &g_sembsl_mxl5007s5h1411, 
    &g_sembsl_mxl5007s5h1411
}; 
*/

/*  DNVD22AEV261A dual VSB/QAM */  
/*  DNQD22CEH101A MCNS VSB/QAM */ 
/*  DNQD22CEV101A MCNS VSB/QAM */  
/*
#define SEMAPAL_FE_MAX_UNITS 2  
#include "../src/sembsl_mxl201s5h1411.h"  
static struct semhal_fe_ops   
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]=  
{  
    &g_sembsl_mxl201s5h1411,  
    &g_sembsl_mxl201s5h1411 
};  
*/
 
/* SAMSUNG S5H1411 EVB MCNS VSB/QAM */  
/*
#define SEMAPAL_FE_MAX_UNITS 1 
#include "../src/sembsl_mt2063s5h1411.h"  
static struct semhal_fe_ops   
*m_fe_instance[SEMAPAL_FE_MAX_UNITS]=  
{  
    &g_sembsl_mt2063s5h1411 
};  
*/
 
/***********************************************/ 
static U32 
m_fe_unit[SEMAPAL_FE_MAX_UNITS]= 
#if 1==SEMAPAL_FE_MAX_UNITS 
{0}; 
#endif 
#if 2==SEMAPAL_FE_MAX_UNITS 
{0,1}; 
#endif 
#if 3==SEMAPAL_FE_MAX_UNITS 
{0,1,2}; 
#endif 
 
/* 
 * SEMCO semaphores for threads sync.  
 */ 
/*VK_SEM_T ChanAccessProtect[SEMAPAL_FE_MAX_UNITS];*/ 
 
/******************************!!! IMPORTANT !!!***********************************/ 
 
  
 
 
/** 
* 
* _get_instance() 
* get the instance of the FE object  
* - local to this .c module 
* 
* @param		unit	        I: compound unit number 
* @param		pp_opj		O: frontend object 
* @param		*p_fe_unit      O: front-end specific unit number  
* @return		int		O: 0: ok  !=0 error 
* @version	        1.02.002 
* @author		Alex Loktionov (alex.loktionov@samsung.com) 
* 
* Created 2009.03.09 
* 
*/ 
static int 
_get_instance 
    ( 
        U32                 unit,             
        struct 
        semhal_fe_ops       **pp_obj,           /* O: front-end interfcace */ 
        U32                 *p_fe_unit          /* O: front-end specific unit number */ 
    ) 
{ 	 
    if (unit >= SEMAPAL_FE_MAX_UNITS) 
	    return SEM_EBAD_UNIT_NUMBER; 
 
    *pp_obj = m_fe_instance[unit]; 
    *p_fe_unit = m_fe_unit[unit]; 
 
    /* 	VK_SM_Take( ChanAccessProtect[unit] );*/        /* ADD your semaphore_wait() here! */ 
 
    return SEM_SOK; 
} 
 
static int 
_release_instance 
    ( 
        U32                 unit 
    ) 
{  
    if (unit >= SEMAPAL_FE_MAX_UNITS) 
	    return SEM_EBAD_UNIT_NUMBER; 
 
    /* VK_SM_Give( ChanAccessProtect[unit] );*/        /*  ADD your semaphore_signal() here! */ 
    return SEM_SOK; 
} 
 
static int 
_get_init_params 
    ( 
        U32                 unit,             
        U32                 **pp_cfg,             
        U32                 *p_cfg_size             
    ) 
{ 
    if(0==unit)                                         /* dual nim sample */ 
    { 
        *pp_cfg=NULL; 
        *p_cfg_size=0; 
    } 
    else 
    { 
        *pp_cfg=NULL; 
        *p_cfg_size=0; 
    } 
 
    return SEM_SOK; 
} 
         
	 
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
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL; 
    U32                     fe_unit=0; 
    U32                     *p_cfg=NULL; 
    U32                     cfg_size=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res);  
    SEMDBG_ENTER(MK_4CC('I','n','i','t'));  
    SEMDBG_DEBUG(unit);
    res=_get_init_params(unit,&p_cfg,&cfg_size); 
    SEMDBG_CHECK(res); 
    res=p_obj->init(fe_unit,p_i2c,p_cfg,cfg_size); 
    SEMDBG_CHECK(res); 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('I','n','i','t')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
/** 
 * SEMFE_Init() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int  
SEMFE_DeInit 
    (  
        U32                 unit 
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('D','e','I','n'));  
    SEMDBG_DEBUG(unit); 
    res=p_obj->deinit(fe_unit); 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('D','e','I','n'));  
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
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
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('S','W','v','e')); 
    res=p_obj->get_sw_version(fe_unit,ver); 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('S','W','v','e')); 
    _release_instance(unit); 
    return res; 
} 
 
int 
SEMFE_GetFEInfo 
    ( 
	U32                 unit,        	/* I: frontend unit number  */
	struct semhal_frontend_info_s  
			    *p_fe_info,         /* O: frontend info and capabilities */
	U32                 *p_info_size	/* IO: sizeof() frontend info structure */			 
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    if(res<0) return 0; 
  
    SEMDBG_ENTER(MK_4CC('I','n','f','o')); 
    SEMDBG_DEBUG(unit); 
    res=p_obj->get_info(fe_unit,p_fe_info,p_info_size); 
 
    SEMDBG_EXIT(MK_4CC('I','n','f','o')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
 
    return res; 
} 
 
 
 
/** 
 * SEMFE_Tune_C() 
 * 
 * @note                Cable NIM API 
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
    ) 
{ 
    int                     res; 
    U32                     wait; 
    U32                     status; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
     
    SEMDBG_ENTER(MK_4CC('T','u','n','C')); 
    SEMDBG_DEBUG(unit); 
    SEMDBG_DEBUG(freq_khz); 
    SEMDBG_DEBUG(symbol_rate); 
    SEMDBG_DEBUG(modulation); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_RF,freq_khz); 
    SEMDBG_CHECK(res); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_MODULATION,modulation); 
    SEMDBG_CHECK(res); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_SR,symbol_rate); 
    SEMDBG_CHECK(res); 
 
    res=p_obj->search_start(fe_unit); 
    SEMDBG_CHECK(res); 
 
    wait=0; 
    while(SEM_SPENDING==(res=p_obj->search_algo(fe_unit,&status,&wait))) 
    { 
        //SEMCO you can add additional check for timeout here 
        //SEMCO or do some background job 
        //SEMCO do not forget to call pnim->fe.search_abort(fe_unit); 
        //SEMCO if you cancel search process 
        semosal_wait_msec(wait); 
    } 
     
 
err_exit:; 
    if(res<0) 
        p_obj->search_abort(fe_unit); 
 
    SEMDBG_EXIT(MK_4CC('T','u','n','C')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
/** 
 * SEMFE_Tune_C() 
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
        U32                 pr 
    ) 
{ 
    int                     res; 
    U32                     wait; 
    U32                     status; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
     
    SEMDBG_ENTER(MK_4CC('T','u','n','T'));  
    SEMDBG_DEBUG(unit); 
    SEMDBG_DEBUG(freq_khz); 
    SEMDBG_DEBUG(bw); 
    SEMDBG_DEBUG(pr); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_RF,freq_khz); 
    SEMDBG_CHECK(res); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_BW,bw); 
    SEMDBG_CHECK(res); 
    res=p_obj->set_cfg(fe_unit,SEMHAL_FE_CFG_PR,pr); 
    SEMDBG_CHECK(res); 
 
    res=p_obj->search_start(fe_unit); 
    SEMDBG_CHECK(res); 
 
    wait=0; 
    while(SEM_SPENDING==(res=p_obj->search_algo(fe_unit,&status,&wait))) 
    { 
        //SEMCO you can add additional check for timeout here 
        //SEMCO or do some background job 
        //SEMCO do not forget to call pnim->fe.search_abort(fe_unit); 
        //SEMCO if you cancel search process 
        semosal_wait_msec(wait); 
    } 
     
 
err_exit:; 
    if(res<0) 
        p_obj->search_abort(fe_unit); 
 
    SEMDBG_EXIT(MK_4CC('T','u','n','T')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
/** 
 * SEMFE_GetLockStatus() 
 * 
 * @return		TRUE/FALSE 
 */ 
BOOL 
SEMFE_GetLockStatus 
    ( 
        U32                 unit             
    ) 
{ 
    int                     res; 
    U32                     status=0; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    if(res<0) return 0; 
  
    SEMDBG_ENTER(MK_4CC('G','t','L','k')); 
    SEMDBG_DEBUG(unit); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_LOCK,&status); 
    SEMDBG_DEBUG(status);
 
    SEMDBG_EXIT(MK_4CC('G','t','L','k')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
 
    return (res>=0)&&(status&SEMHAL_FE_HAS_LOCK); 
} 
 
 
 
#define INRANGE( x,  y, z) (((x)<=(y) && (y)<=(z))||((z)<=(y) && (y)<=(x))) 
static int  
_lookup 
    ( 
        S32                 val, 
        S32                 table[][2], 
        U32                 size 
    ) 
{ 
    U32                     ret; 
    U32                     i; 
 
    ret = table[0][1];                          /* to relax the C compiler */ 
    size=(size/(sizeof(U32)<<1))-1; 
    if(INRANGE(table[size][0], val, table[0][0])) 
    { 
        for(i=0; i<size; ++i) 
        { 
            if( INRANGE(table[i+1][0], val, table[i][0]) ) 
            { 
                ret = table[i][1]; 
                break; 
            } 
        }	 
    } 
    else  
    { 
        ret = (val>table[0][0])?table[0][1]:table[size][1]; 
    } 
     
    return ret; 
     
} 
 
/* 
 * Lookup table BIGGEST value is the FIRST!  
 * 
 * {dBm*100, 'strength value' }, strength value could be ANY range, 
 * 0-100,0-255, -1000000 to +1000000 
 * 
 * Example: {-8000,  0}  - means -80.00dBm is 0 strength 
 */ 
#ifdef SEMAPAL_LOOKUPS_RANGE_0_100
/**
 * 0..100 range example
 */
static S32 _strength_lookup[][2]=                   /* dBm*100 -> strength */ 
{ 
    {-5000, 100}, 
    {-5500, 100}, 
    {-5700, 90}, 
    {-6000, 85}, 
    {-6100, 70}, 
    {-6200, 65}, 
    {-6150, 57}, 
    {-6200, 49}, 
    {-6350, 40}, 
    {-6400, 35}, 
    {-6500, 30}, 
    {-6600, 25}, 
    {-6650, 22}, 
    {-6700, 20}, 
    {-6750, 17}, 
    {-6800, 15}, 
    {-7000, 10}, 
    {-8000,  0}, 
}; 
#endif 

#ifdef  SEMAPAL_LOOKUPS_RANGE_0_255
/**
 * 0..255 range example
 */
static S32 _strength_lookup[][2]=                   /* dBm*100 -> strength */ 
{
	{	-1000	,	255	},
	{	-1400	,	242	}, 
	{	-1700	,	230	},
	{	-2500	,	205	},
	{	-3000	,	180	},
	{	-4100	,	155	},
	{	-4500	,	130	},
	{	-5000	,	105	},
	{	-5700	,	80	},
	{	-6250	,	55	},
	{	-7000	,	30	},
	{	-7500	,	5	},
	{	-8000	,	0	}
};
#endif  
 
/* 
 * Lookup table BIGGEST value is the FIRST!  
 * 
 * {dB*100, 'qality value' }, quality value could be ANY range, 
 * 0-100,0-255, -1000000 to +1000000 
 * 
 * Example: {3900,100}  - means 39.00dB is 100 quality 
 */ 
#ifdef SEMAPAL_LOOKUPS_RANGE_0_100
/**
 * 0..100 range example
 */
static S32 _quality_lookup[][2]=                    /* dB -> quality */ 
{ 
	{	3500	,	100	}, 
	{	3375	,	95	}, 
	{	3250	,	90	}, 
	{	3125	,	85	}, 
	{	3000	,	80	}, 
	{	2875	,	75	}, 
	{	2750	,	70	}, 
	{	2625	,	65	}, 
	{	2500	,	60	}, 
	{	2375	,	55	}, 
	{	2250	,	50	}, 
	{	2125	,	45	}, 
	{	2000	,	40	}, 
	{	1875	,	35	}, 
	{	1750	,	30	}, 
	{	1625	,	25	}, 
	{	1500	,	20	}, 
	{	1375	,	15	}, 
	{	1250	,	10	}, 
	{	1125	,	5	}, 
	{	400	,	0	} 
}; 
#endif
  
 
#if SEMAPAL_LOOKUPS_RANGE_0_255 
/** 
 * 0..255 range example 
 */ 
static S32 _quality_lookup[][2]=                    /* dB -> quality */  
{  
	{	3500	,	255	}, 
	{	3375	,	242	}, 
	{	3250	,	229	}, 
	{	3125	,	216	}, 
	{	3000	,	204	}, 
	{	2875	,	191	}, 
	{	2750	,	178	}, 
	{	2625	,	165	}, 
	{	2500	,	153	}, 
	{	2375	,	140	}, 
	{	2250	,	127	}, 
	{	2125	,	114	}, 
	{	2000	,	102	}, 
	{	1875	,	89	}, 
	{	1750	,	76	}, 
	{	1625	,	63	}, 
	{	1500	,	51	}, 
	{	1375	,	38	}, 
	{	1250	,	25	}, 
	{	1125	,	12	}, 
	{	400	,	10	}, 
	{	0	,	0	} 
};  
#endif  

/* 
 * Lookup table BIGGEST value is the FIRST!  
 * 
 * {BER*100000000, 'ber value' }, ber value could be ANY range 
 * 0-100,0-255, -1000000 to +1000000 
 * 
 * Example: {200000,100} - means BER 2E-3 is 100 'BER value' 
 */ 
#ifdef SEMAPAL_LOOKUPS_RANGE_0_100
/**
 * 0..100 range example
 */
static S32 _ber_lookup[][2]=                        /* BER*1E8 -> BER% */ 
{ 
    {200000,  0},                               //2E-3  
    {100000,  5},                               //1E-3 
    { 50000, 10},                               //5E-4 
    { 10000, 15},                               //1E-4 
    {  9000, 20},                               //9E-5 
    {  7700, 25},                               //7.71E-5 
    {  7000, 30},                                
    {  5900, 35},                                
    {  5800, 40},                                
    {  5700, 45},                                
    {  5600, 50},                                
    {  5600, 55},                                
    {  5500, 60},                                
    {  5400, 65},                                
    {  5300, 70},                                
    {  5200, 73},                                
    {  5100, 75},                                
    {  5000, 78},                                
    {  4900, 80},                                
    {  4800, 88}, 
    {  4700, 88}, 
    {  4500, 90}, 
    {  4300, 95}, 
    {  4000,100} 
}; 
#endif 

#if SEMAPAL_LOOKUPS_RANGE_0_255
/**
 * 0..255 range example
 */
static S32 _ber_lookup[][2]=                        /* BER*1E8 -> BER% */ 
{ 
    {200000,  0},                               //2E-3  
    {  4000,255} 
}; 
/* Please, make yourself. Customize... */
#endif 

 
/** 
 * SEMFE_GetStrength() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 *  
 * @note                if not locked provides 0 (see SEMFE_GetLockStatus()) 
 *     
 */ 
int 
SEMFE_GetSignalStrength 
    (  
        U32                 unit,             
        U32		    *pvalue 	        /* O: */ 
    ) 
{ 
    int                     res; 
    S32                     rssi1=0; 
    S32                     rssi2=0; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
  
    SEMDBG_ENTER(MK_4CC('R','S','S','I')); 
    SEMDBG_DEBUG(unit); 
    *pvalue=0; 
    // SEMCO usualy STB should display signal strength even transponder is ot locked 
    //if(!SEMFE_GetLockStatus(unit)) return FALSE;   
 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RSSI_DBMX100,(U32*)&rssi1);/* RSSI signed! dBm*100*/ 
    SEMDBG_CHECK(res); 
    SEMDBG_DEBUG(rssi1); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RSSI_DBMX100,(U32*)&rssi2);/* RSSI signed! dBm*100*/ 
    SEMDBG_CHECK(res); 
    SEMDBG_DEBUG(rssi2); 
     
    // !!! SEMCO rssi value is dBm*100  so -35.5dBm=> -3550==rssi !!! 
    *pvalue= 
        (_lookup(rssi1,_strength_lookup,sizeof(_strength_lookup))+ 
        _lookup(rssi2,_strength_lookup,sizeof(_strength_lookup)))>>1; 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('R','S','S','I')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
 
/** 
 * SEMFE_GetSNRQuality() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 *  
 * @note                if not locked provides 0 (see SEMFE_GetLockStatus()) 
 *     
 */ 
int  
SEMFE_GetSNRQuality 
    ( 
        U32                 unit,             
        U32		    *pvalue 	        /* O: */ 
    ) 
{ 
    int                     res; 
    U32                     snr1=0; 
    U32                     snr2=0; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
  
    SEMDBG_ENTER(MK_4CC('S','N','R','Q'));  
    SEMDBG_DEBUG(unit); 
    *pvalue=0; 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SNR_DBX100,&snr1);/* SNR in dB * 100 */ 
    SEMDBG_CHECK(res); 
    SEMDBG_DEBUG(snr1); 
    if(!SEMFE_GetLockStatus(unit)) {res=SEM_SFALSE; goto err_exit; };   
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SNR_DBX100,&snr2);/* SNR in dB * 100 */ 
    SEMDBG_CHECK(res); 
    SEMDBG_DEBUG(snr2); 
 
    // !!! SEMCO provides here your lookup table for all nim modules (dBm->*pvalue) !!! 
    *pvalue=(_lookup(snr1,_quality_lookup,sizeof(_quality_lookup))+ 
            _lookup(snr2,_quality_lookup,sizeof(_quality_lookup)))>>1; 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('S','N','R','Q')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
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
        U32		    *pvalue 	        /* O: */ 
    ) 
{ 
    int                     res; 
    U32                     ber=0; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_DEBUG(unit); 
    SEMDBG_CHECK(res); 
  
    SEMDBG_ENTER(MK_4CC('B','E','R','Q')); 
    *pvalue=0; 
    if(!SEMFE_GetLockStatus(unit)) {res=SEM_SFALSE; goto err_exit; };   
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_POSTBER,&ber); 
 
    // !!! SEMCO provides here your lookup table for all nim modules (ber->*pvalue) !!! 
    *pvalue=_lookup(ber,_ber_lookup,sizeof(_ber_lookup)); 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('B','E','R','Q')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
/** 
 * SEMFE_GetRFSignalStrength() 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetRFSignalStrength 
    ( 
        U32                 unit,             
        U32		    *pvalue 	        /* O: */ 
    ) 
{ 
    int                     res; 
    U32                     rssi=0; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
  
    SEMDBG_ENTER(MK_4CC('R','F','R','S')); 
    SEMDBG_DEBUG(unit); 
    *pvalue=0; 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RFRSSI_DBMX100,&rssi); 
    if(SEM_ENOT_SUPPORTED==res)                 /* not all RF IC support RSSI feature */ 
    { 
        res=SEMFE_GetSignalStrength(unit,pvalue); 
        if(!res) res=SEM_SFALSE;                /* value is faked from DEMOD AGC */ 
    } 
    else 
    { 
        *pvalue=_lookup(rssi,_strength_lookup,sizeof(_strength_lookup)); 
    } 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('R','F','R','S')); 
    _release_instance(unit); 
    return res; 
} 
 
 
/** 
 * SEMFE_SetPowerState() 
 * 
 * @param		mode		[IN]	0:powered on(D0 state) 1:power standby(D1 state) 2:power suspended(D2 state) 3:powered off(D3 state) 
 *
 * @brief		It is used to sleep/wakeup the front-end (i.e. control low/normal power consumption state).
 * @brief		You need it if your STB supports "green mode sleep" (low power use, less than 1Watt) 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_SetPowerState 
    ( 
        U32                 unit,             
        U32                 mode 
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
  
    SEMDBG_ENTER(MK_4CC('P','o','w','r')); 
    SEMDBG_DEBUG(unit); 
    SEMDBG_DEBUG(mode); 
    res=p_obj->set_state(fe_unit,SEMHAL_FE_STATE_POWER,mode); 
 
err_exit:; 
    SEMDBG_EXIT(MK_4CC('P','o','w','r')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
 
 
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
        semapal_tp_info_dvbc_s *tp_info      	/* O: (STB OS dependant) pointer to struct */ 
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('T','p','I','C')); 
    SEMDBG_DEBUG(unit); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_LOCK,&tp_info->locked); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_cfg(fe_unit,SEMHAL_FE_CFG_RF,&tp_info->freq); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_AFC,(U32*)&tp_info->freq_offset); 
    SEMDBG_CHECK(res); 
    tp_info->snr=tp_info->pre_ber=tp_info->post_ber=0; 
    if(tp_info->locked) 
    { 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_PREBER,&tp_info->pre_ber); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_POSTBER,&tp_info->post_ber); 
 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SNR_DBX100,(U32*)&tp_info->snr); 
        SEMDBG_CHECK(res); 
    } 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_UCB,&tp_info->ucb); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RFRSSI_DBMX100,(U32*)&tp_info->rssi); 
    if(SEM_ENOT_SUPPORTED==res)                 /* not all RF IC support RSSI feature */ 
    { 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RSSI_DBMX100,(U32*)&tp_info->rssi); 
    } 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SR,&tp_info->sr); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_MODULATION,&tp_info->mod); 
    SEMDBG_CHECK(res); 
     
err_exit:; 
    SEMDBG_EXIT(MK_4CC('T','p','I','C')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
/** 
 * SEMFE_GetTpInfo_DVBC 
 * 
 * @return		0:OK,NEGATIVE-Error,POSITIVE-other see "semstbcp.h" 
 */ 
int 
SEMFE_GetTpInfo_DVBT 
    ( 
        U32                 unit,             
        struct  
        semapal_tp_info_dvbt_s *tp_info      	/* O: (STB OS dependant) pointer to struct */ 
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('T','p','I','T')); 
    SEMDBG_DEBUG(unit); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_LOCK,&tp_info->locked); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_cfg(fe_unit,SEMHAL_FE_CFG_RF,&tp_info->freq); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_AFC,(U32*)&tp_info->freq_offset); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RFRSSI_DBMX100,(U32*)&tp_info->rssi); 
    if(SEM_ENOT_SUPPORTED==res)                 /* not all RF IC support RSSI feature */ 
    { 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RSSI_DBMX100,(U32*)&tp_info->rssi); 
    } 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_FFT,(U32*)&tp_info->fft); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_GI,(U32*)&tp_info->gi); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_MODULATION,&tp_info->mod_hp); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_CR,&tp_info->cr_hp); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_UCB,&tp_info->ucb_hp); 
    SEMDBG_CHECK(res); 
 
    tp_info->pre_ber_hp=tp_info->post_ber_hp=0; 
    tp_info->pre_ber_lp=tp_info->post_ber_lp=0; 
    tp_info->snr=0; 
    if(tp_info->locked) 
    { 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SNR_DBX100,(U32*)&tp_info->snr); 
        SEMDBG_CHECK(res); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_PREBER,&tp_info->pre_ber_hp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_POSTBER,&tp_info->post_ber_hp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_PREBERLP,&tp_info->pre_ber_lp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_POSTBERLP,&tp_info->post_ber_lp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_CRLP,&tp_info->cr_lp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_MODULATIONLP,&tp_info->mod_lp); 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_UCBLP,&tp_info->ucb_lp); 
    } 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_HR,&tp_info->hr); 
    SEMDBG_CHECK(res); 
     
err_exit:; 
    SEMDBG_EXIT(MK_4CC('T','p','I','T')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
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
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
    U32                     state=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('T','p','I','R')); 
    SEMDBG_DEBUG(unit); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_SYNC|SEMHAL_FE_TUNER,&state); 
    SEMDBG_CHECK(res); 
    if(state&1) tp_info->lock1=1; 
    if(state&2) tp_info->lock2=1; 
 
    res=p_obj->get_cfg(fe_unit,SEMHAL_FE_CFG_RF,&tp_info->freq); 
    SEMDBG_CHECK(res); 
    res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RFRSSI_DBMX100,(U32*)&tp_info->rssi); 
    if(SEM_ENOT_SUPPORTED==res)                 /* not all RF IC support RSSI feature */ 
    { 
        res=p_obj->get_state(fe_unit,SEMHAL_FE_STATE_RSSI_DBMX100,(U32*)&tp_info->rssi); 
    } 
    SEMDBG_CHECK(res); 
     
err_exit:; 
    SEMDBG_EXIT(MK_4CC('T','p','I','R')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
 
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
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('_','G','e','t')); 
    SEMDBG_DEBUG(unit); 
    SEMDBG_DEBUG(item); 
    if(item&SEMHAL_FE_STATE) 
        res=p_obj->get_state(fe_unit,item,p_val); 
    else 
        res=p_obj->get_cfg(fe_unit,item,p_val); 
    SEMDBG_CHECK(res); 
     
err_exit:; 
    SEMDBG_EXIT(MK_4CC('_','G','e','t')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
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
    ) 
{ 
    int                     res; 
    struct semhal_fe_ops    *p_obj=NULL;  
    U32                     fe_unit=0; 
 
    res=_get_instance(unit, &p_obj,&fe_unit); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_ENTER(MK_4CC('_','S','e','t')); 
    SEMDBG_DEBUG(unit); 
    SEMDBG_DEBUG(item); 
    SEMDBG_DEBUG(val); 
    if(item&SEMHAL_FE_STATE) 
        res=p_obj->set_state(fe_unit,item,val); 
    else 
        res=p_obj->set_cfg(fe_unit,item,val); 
    SEMDBG_CHECK(res); 
     
err_exit:; 
    SEMDBG_EXIT(MK_4CC('_','S','e','t')); 
    SEMDBG_DEBUG(res); 
    _release_instance(unit); 
    return res; 
} 
 
 
 
