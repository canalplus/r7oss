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
* @file			sembsl_mxl5007ce6353.c 
* @brief		Implementation of frontend MXL5007t RF + CE6353 DEMOD  
* 
* @version	        1.00.009
* @author	        Alexander Loktionov (alex.loktionov@samsung.com) aka OXY 
* 
* Created 2009.05.06
* 
*/

#include "mxl5007-ce6353.h"
#include "platform_dependent.h"
#include "semstbcp.h"                           /* SEMCO STB CP API */ 
 
#include "semosal.h" 
#include "semhal_fe.h" 
 
 
#include "sembsl_mxl5007.h" 
#include "sembsl_ce6353.h" 
 
#include "sembsl_mxl5007ce6353_local.h" 
 
#define THIS_FILE_ID        0x753 
#define THISFILE_TRACE      SEMDBG_TRACE_BSL 
 
extern struct semhal_fe_ops g_sembsl_mxl5007ce6353; 
 
/** 
* 
* _get_instance() 
* get the instance of the FE object  
* - local to this .c module 
* 
* @param		unit	        I: compound unit number 
* @param		pp_opj		O: frontend object 
* @return		int		O: 0: ok  !=0 error 
* @version	        1.02.002 
* @author		Alex Loktionov (alex.loktionov@samsung.com) 
* 
* Created 2009.02.03 
* 
*/ 
static int 
_get_instance 
    ( 
        U32         	    unit,	        /* I: frontend unit number                      */ 
        struct 
        sembsl_mxl5007ce6353_obj **pp_obj       /* O: front-end object */ 
     ) 
{ 	 
	if (unit >= SEMBSL_MXL5007CE6353_MAX_UNITS) 
		return SEM_EBAD_UNIT_NUMBER; 
 
	*pp_obj = &m_mxl5007ce6353_instance[unit]; 
 
        if( (*pp_obj)->api_flags&SEMBSL_FE_INITED) 
	    return SEM_SOK; 
         
        return SEM_ENOT_INITIALIZED; 
} 
 
static int 
_mxl2semerr(U32 err) 
{ 
    if(!err) return SEM_SOK; 
 
#if SEM_DBG>=SEMDBG_LVL_DBG 
    SEMDBG_TRACE(MK_4CC('_','m','x','l')); 
#endif 
 
    if(MxL_ERR_INIT==err) return SEM_ENOT_INITIALIZED; 
    if(MxL_GET_ID_FAIL==err) return SEM_EIIC_NOACK; 
 
    return SEM_EFAIL; 
} 
 
 
static int 
mxl5007ce6353_deinit                            /* releases all allocated memory and semaphores */ 
    ( 
        U32         	    unit	        /* I: frontend unit number                      */ 
    ); 
 
int 
mxl5007ce6353_init 
    ( 
        U32         	    unit,	        /* I: frontend unit number                      */ 
        struct  
        semosal_i2c_obj     *p_i2c,             /* I: I2C adapter interface */ 
        U32                 *p_cfg_values,	/* I: array of cfg. item_id,value pairs */ 
        U32                 cfg_num             /* I: number of elements in p_cfg_values array */ 
    ) 
{  
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj=0; 
 
    U32                     tuner_id=0; 
    U32                     demod_id=0; 
 
    res=_get_instance(unit, &p_obj); 
    if(SEM_SUCCEEDED(res)) { res=SEM_SFALSE; goto ok_exit;} 
    if(SEM_ENOT_INITIALIZED!=res) goto unit_err_exit; 
    res=SEM_SOK; 
 
    p_obj->standard             = MXL5007CE6353_STANDARD; 
 
    res|= sembsl_ce6353_init 
        ( 
            &p_obj->demod, 
            p_i2c, 
            (unit&1?DEMOD_I2C_ADDRESSS1:DEMOD_I2C_ADDRESSS0),  
            &p_obj->tuner.i2c 
        ); 
    if(SEM_FAILED(res)) goto unit_err_exit; 

    p_obj->search.bw=MXL5007CE6353_BW; 
    p_obj->search.pr=MXL5007CE6353_PR; 
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_INIT,MXL5007CE6353_AGC_TRGT);  //!!!
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_BW,p_obj->search.bw);  
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_HR,p_obj->search.pr); 
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_CFG_IF,MXL5007CE6353_IF);  
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_CFG_AGCTRESHOLD,MXL5007CE6353_AGC_TRGT);  
    res|=sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_INIT,MXL5007CE6353_AGC_TRGT);  

    res|=sembsl_ce6353_get(&p_obj->demod,SEMHAL_FE_STATE_IDENTITY, &demod_id); 
    if(!demod_id) res|=SEM_EIIC_NOACK; 
    if(SEM_FAILED(res)) goto demod_err_exit; 
 
 
    p_obj->tuner.Mode=MxL_MODE_DVBT; 
    p_obj->tuner.Xtal_Freq=MXL5007CE6353_RFXTAL; 
    p_obj->tuner.IF_Freq=MXL5007CE6353_IF; 
    p_obj->tuner.IF_Spectrum=MxL_NORMAL_IF; 
#if MXL5007CE6353_RFCLKOUT
    p_obj->tuner.ClkOut_Setting=MxL_CLKOUT_ENABLE; 
    p_obj->tuner.ClkOut_Amp=MxL_CLKOUT_AMP_0;  
#else 
    p_obj->tuner.ClkOut_Setting=MxL_CLKOUT_DISABLE;  
    p_obj->tuner.ClkOut_Amp=MxL_CLKOUT_AMP_7;  
#endif
    p_obj->tuner.BW_MHz=MXL5007CE6353_BW/1000000; 
    p_obj->tuner.RF_Freq_Hz=666000000; 
    p_obj->tuner.LoopThru_Setting = MxL_LT_DISABLE; 
    res|=SEMOSAL_I2C_OPEN(&p_obj->tuner.i2c,(unit&1?TUNER_I2C_ADDRESSS1:TUNER_I2C_ADDRESSS0)); 
    if(SEM_FAILED(res)) goto demod_err_exit; 

    sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,1); 
        res|=_mxl2semerr(MxL5007_Tuner_Init(&p_obj->tuner)); 
        res|=_mxl2semerr(MxL5007_Wake_Up(&p_obj->tuner));  
        res|=_mxl2semerr(MxL5007_Loop_Through_On((&p_obj->tuner),p_obj->tuner.LoopThru_Setting)); 
        res|=_mxl2semerr(MxL5007_Check_ChipVersion(&p_obj->tuner,&tuner_id)); 
    sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,0); 
    if(!tuner_id) res|=SEM_EIIC_NOACK; 
    if(SEM_FAILED(res)) goto rf_err_exit; 
 
    p_obj->api_flags=SEMBSL_FE_INITED; 
    if(cfg_num) res|=g_sembsl_mxl5007ce6353.set_cfg_bulk(unit,p_cfg_values,cfg_num); 
    if(SEM_FAILED(res)) goto rf_err_exit; 
 
ok_exit:; 
    printk ("%s ok!\n", __FUNCTION__);
    return res; 
 
rf_err_exit:; 
    printk ("%s rf_err_exit\n", __FUNCTION__);
    SEMOSAL_I2C_CLOSE(&p_obj->tuner.i2c); 
demod_err_exit:; 
    printk ("%s demod_err_exit:\n", __FUNCTION__);
    sembsl_ce6353_deinit(&p_obj->demod); 
unit_err_exit:; 
    printk ("%s unit_err_exit:\n", __FUNCTION__);
    return res; 
} 
 
static int 
mxl5007ce6353_deinit                            /* releases all allocated memory and semaphores */ 
    ( 
        U32         	    unit	        /* I: frontend unit number                      */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    p_obj->api_flags&=0; 
 
    SEMOSAL_I2C_CLOSE(&p_obj->tuner.i2c); 
    sembsl_ce6353_deinit(&p_obj->demod); 
 
    memset(p_obj,0,sizeof(struct sembsl_mxl5007ce6353_obj)); 
 
err_exit:; 
    return res; 
} 
 
 
 
static int 
mxl5007ce6353_get_info 
    ( 
        U32	            unit,	        /* I: frontend unit number */ 
	struct semhal_frontend_info_s  
			    *p_fe_info,         /* O: frontend info and capabilities    */ 
	U32                 *p_info_size	/* IO: sizeof() frontend info structure	*/		 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
 
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    if(*p_info_size<sizeof(struct semhal_frontend_info_s)) 
        res=SEM_ESMALL; 
    SEMDBG_CHECK(res); 
 
    *p_info_size=sizeof(struct semhal_frontend_info_s); 
     
    p_fe_info->name[0]=0; 
//    semosal_strlcat(p_fe_info->name,"SEMCO MxL5007+CE6353 ",sizeof(p_fe_info->name)); 
    strcat(p_fe_info->name,"SEMCO MxL5007+CE6353 "); 
    if(p_obj->standard&SEMHAL_FE_DVB_S) 
//        semosal_strlcat(p_fe_info->name,"DVB-S ",sizeof(p_fe_info->name)); 
       strcat(p_fe_info->name,"DVB-S "); 
    if(p_obj->standard&SEMHAL_FE_MCNS) 
//        semosal_strlcat(p_fe_info->name,"MCNS/OP ",sizeof(p_fe_info->name)); 
        strcat(p_fe_info->name,"MCNS/OP "); 
    if(p_obj->standard&SEMHAL_FE_DVB_C) 
//        semosal_strlcat(p_fe_info->name,"DVB-C ",sizeof(p_fe_info->name)); 
        strcat(p_fe_info->name,"DVB-C "); 
    if(p_obj->standard&SEMHAL_FE_DVB_T) 
//        semosal_strlcat(p_fe_info->name,"DVB-T ",sizeof(p_fe_info->name)); 
        strcat(p_fe_info->name,"DVB-T "); 
    if(p_obj->standard&SEMHAL_FE_ISDBT) 
//        semosal_strlcat(p_fe_info->name,"ISDB-T ",sizeof(p_fe_info->name)); 
        strcat(p_fe_info->name,"ISDB-T "); 
     
    p_fe_info->caps_modulation=MXL5007CE6353_MOD; 
    p_fe_info->caps_common=SEMHAL_FE_CAN_INVERSION_AUTO; 
    p_fe_info->caps_fec=SEMHAL_FE_FEC_AUTO; 
    p_fe_info->caps_other=SEMHAL_FE_CAN_TRANSMISSION_MODE_AUTO|SEMHAL_FE_CAN_GUARD_INTERVAL_AUTO|SEMHAL_FE_CAN_HIERARCHY_AUTO; 
 
    p_fe_info->frequency_max=860000; 
    p_fe_info->frequency_min= 51000; 
    p_fe_info->frequency_stepsize=1; 
    p_fe_info->frequency_tolerance=50; 
 
    p_fe_info->notifier_delay=0; 
 
    p_fe_info->symbol_rate_min=MXL5007CE6353_SR_MIN; 
    p_fe_info->symbol_rate_max=MXL5007CE6353_SR_MAX; 
    p_fe_info->symbol_rate_tolerance=200; 
 
    p_fe_info->type=MXL5007CE6353_STANDARD; 
     
err_exit: 
    return res; 
} 
 
 
static int 
mxl5007ce6353_get_sw_version 
    ( 
        U32	            unit,	        /* I: frontend unit number        */ 
        U32                 *pu_ver		/* O: SW version 31..24 major,    */ 
                                                /*    23..16 -minor, 15..0 -build */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
 
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    *pu_ver=MK_U32((SEMBSL_MXL5007CE6353_MAJOR_VER<<8)+SEMBSL_MXL5007CE6353_MINOR_VER,SEMBSL_MXL5007CE6353_BUILD_NUM); 
     
err_exit:; 
    return 0; 
} 
 
 
/** 
 * tuning functions 
 */ 
static int 
mxl5007ce6353_search_start                      /* set parameters for search for fe_search_algo() */  
    ( 
        U32         	unit	                /* I: frontend unit number */ 
    ) 
{ 
    int                     res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
 
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    if(p_obj->api_flags&(SEMBSL_FE_X_BW|SEMBSL_FE_X_SR)) 
    { 
        res|=g_sembsl_mxl5007ce6353.set_state(unit,SEMHAL_FE_STATE_BW,p_obj->search.bw); 
    } 
    if(p_obj->api_flags&(SEMBSL_FE_X_RF|SEMBSL_FE_X_BW|SEMBSL_FE_X_STD)) 
    { 
        /* tuner is connected to I2C bus */
        sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,1); 
        res|= _mxl2semerr(MxL5007_Tuner_RFTune( &p_obj->tuner,p_obj->search.freq*1000,p_obj->search.bw/1000000)); 
        sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,0); 
        /* tuner is disconnected from the I2C bus */
    } 
     
    if(p_obj->api_flags&(SEMBSL_FE_X_MOD|SEMBSL_FE_X_STD)) 
    { 
        res|=g_sembsl_mxl5007ce6353.set_state(unit,SEMHAL_FE_STATE_MODULATION,p_obj->search.modulation); 
    } 
    { 
        p_obj->api_flags&=~SEMBSL_FE_LCKD; 
        res|=g_sembsl_mxl5007ce6353.set_state(unit,SEMHAL_FE_STATE_RESET,0); 
    } 
    if(p_obj->api_flags&(SEMBSL_FE_X_PR)) 
    { 
        res|=g_sembsl_mxl5007ce6353.set_state(unit,SEMHAL_FE_STATE_HR,p_obj->search.pr); 
    } 
    if(p_obj->api_flags&(SEMBSL_FE_X_BW|SEMBSL_FE_X_SR))  
    {  
        semosal_wait_msec(200); 
    }  
 
    p_obj->api_flags&=~(SEMBSL_FE_X_RF|SEMBSL_FE_X_BW|SEMBSL_FE_X_MOD|SEMBSL_FE_X_SR|SEMBSL_FE_X_PR|SEMBSL_FE_X_STD); 
    p_obj->api_flags|=SEMBSL_FE_SEARCHING; 
    p_obj->search_start=semosal_tick_now(); 
 
err_exit: 
    return res; 
} 
 
 
static int 
mxl5007ce6353_search_algo                      /* actual search function, must be called in loop */ 
    ( 
        U32	            unit,	        /* I: frontend unit number */ 
        U32                 *p_fe_status,	/* O: status bit mask.  
						      see SEMHAL_FE_STATE_LOCK 
                                                 */ 
        U32                 *p_delay            /* O: delay before next step */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
    U32                     time_passed; 
 
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    *p_delay=0; 
 
    *p_fe_status=0; 
    res=g_sembsl_mxl5007ce6353.get_state(unit,SEMHAL_FE_STATE_LOCK,p_fe_status); 
    SEMDBG_CHECK(res); 
 
    if(p_obj->api_flags&SEMBSL_FE_SEARCHING) 
    { 
        time_passed=semosal_elapsed_msec(p_obj->search_start,semosal_tick_now()); 
 
        if( SEMHAL_FE_HAS_LOCK&(*p_fe_status) ) 
        { 
            p_obj->api_flags&=~SEMBSL_FE_SEARCHING; 
            res|=g_sembsl_mxl5007ce6353.set_state(unit,SEMHAL_FE_STATE_TRACKING,1); 
        } 
        else if(time_passed<MXL5007CE6353_TO_AGC) 
        { 
            res=SEM_SPENDING; 
            *p_delay=20; 
        } 
        else if 
            ( 
                time_passed<MXL5007CE6353_TO_LOCK &&  
                ((SEMHAL_FE_HAS_SIGNAL|SEMHAL_FE_HAS_CARRIER)&(*p_fe_status)) 
            ) 
        { 
            res=SEM_SPENDING; 
            *p_delay=20; 
        } 
        else 
        { 
            p_obj->api_flags&=~SEMBSL_FE_SEARCHING; 
        } 
    } 
 
err_exit: 
    return res; 
} 
 
static int 
mxl5007ce6353_search_abort                      /* recomended to call to reduce power usage */ 
    ( 
        U32         	unit			/* I: frontend unit number  */
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    p_obj->api_flags&=~SEMBSL_FE_SEARCHING; 
 
err_exit: 
    return res; 
} 
 
 
/** 
 * set and get functions 
 */ 
static int 
mxl5007ce6353_get_cfg 
    ( 
        U32         	    unit,	        /* I: frontend unit number   */ 
	U32                 item_id,	        /* I: identifier of the item */ 
	U32                 *p_val		/* O: returned value         */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    switch(item_id) 
    { 
    case SEMHAL_FE_CFG_RF: 
        *p_val=p_obj->tuner.RF_Freq_Hz/1000; 
        break; 
 
    case SEMHAL_FE_CFG_LOCK: 
        *p_val=p_obj->state; 
        break; 
 
    default: 
        res=SEM_ENOT_SUPPORTED; 
    } 
 
err_exit: 
    return res; 
} 
 
static int 
mxl5007ce6353_set_cfg 
    ( 
        U32         	    unit,	        /* I: frontend unit number   */ 
	U32                 item_id,	        /* I: identifier of the item */ 
        U32	            val		        /* I: input value            */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    if(res<0&&SEM_ENOT_INITIALIZED!=res) goto err_exit; 
    res=SEM_SOK; 
 
    switch(item_id) 
    { 
    case SEMHAL_FE_CFG_RF: 
        p_obj->search.freq=val; 
        if(p_obj->tuner.RF_Freq_Hz!=val*1000) 
        { 
            p_obj->api_flags|=SEMBSL_FE_X_RF; 
        } 
        break; 
 
    case SEMHAL_FE_CFG_MODULATION: 
        if(p_obj->search.modulation!=val) 
        { 
            p_obj->api_flags|=SEMBSL_FE_X_MOD; 
            p_obj->search.modulation=val; 
        } 
        break; 
 
    case SEMHAL_FE_CFG_BW: 
        if( val && (p_obj->search.bw!=val)) 
        { 
            p_obj->api_flags|=SEMBSL_FE_X_BW|SEMBSL_FE_X_SR; 
            p_obj->search.bw=val; 
        } 
        break; 
 
    case SEMHAL_FE_CFG_PR: 
        if( p_obj->search.pr!=val) 
        { 
            p_obj->api_flags|=SEMBSL_FE_X_PR; 
            p_obj->search.pr=val; 
        } 
        break; 
 
    case SEMHAL_FE_CFG_SR: 
        if( val<MXL5007CE6353_SR_MIN || val>MXL5007CE6353_SR_MAX) 
	{  
           res|=SEM_EBAD_PARAMETER; 
	}  
        else 
        { 
            val+=500000; 
            val/=1000000; 
            val*=1000000; 
            res|=g_sembsl_mxl5007ce6353.set_cfg(unit,SEMHAL_FE_CFG_BW,val); 
        } 
        break; 
 
    default: 
        res=SEM_ENOT_SUPPORTED; 
    } 
 
err_exit: 
    return res; 
} 
 
 
#define INRANGE( x,  y, z) (((x)<=(y) && (y)<=(z))||((z)<=(y) && (y)<=(x)))  
static U32   
_lookup  
    (  
        S32                 val,  
        S32                 table[][2],  
        U32                 size  
    )  
{  
    U32                     ret;  
    U32                     i;  
      
    ret = table[0][1];                          /* to relax C compiler */  
    size=(size/(sizeof(U32)<<1))-1;  
    if(INRANGE(table[size][0], val, table[0][0]))  
    {  
        for(i=0; i<size; ++i)  
        {  
            if( INRANGE(table[i+1][0], val, table[i][0]) )  
            {  
                ret = table[i+1][1]+(table[i][1]-table[i+1][1])*(val-table[i+1][0])/(table[i][0]-table[i+1][0]);  
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
   
#ifdef DNOD22QZV102A 
/* Lookup table BIGGEST value is the FIRST! */  
static S32 _rssi_dbx100_lookup[][2]=  
{  
    {	975	,	-9900	}, 
    {	961	,	-9300	}, 
    {	938	,	-8300	}, 
    {	909	,	-7300	}, 
    {	876	,	-6300	}, 
    {	837	,	-5200	}, 
    {	795	,	-4300	}, 
    {	737	,	-3300	}, 
    {	665	,	-2300	}, 
    {	582	,	-1300	}, 
    {	476	,	0	}, 
    {	404	,	500	}, 
    {   0x0000  ,       1000    }  
};  
#endif 
 
#ifdef DNOD44CDV085A 
/* Lookup table BIGGEST value is the FIRST! */   
static S32 _rssi_dbx100_lookup[][2]=   
{   
    {	790     ,	-9900	}, 
    {	760	,	-9000	}, 
    {	715	,	-8000	}, 
    {	665	,	-7000	}, 
    {	615	,	-6000	}, 
    {	561	,	-5000	}, 
    {	506	,	-4000	}, 
    {	434	,	-3000	}, 
    {	361	,	-2000	}, 
    {	272	,	-1000	}, 
    {	190	,	0	}, 
    {	175	,	200	}, 
    {   0x0000  ,       300     }   
};   
#endif  
 
static int 
mxl5007ce6353_get_state 
    ( 
        U32         	    unit,	        /* I: frontend unit number   */ 
	U32                 item_id,	        /* I: identifier of the item */ 
	U32                 *p_val		/* O: returned value         */ 
    ) 
{ 
    int                         res; 
    struct  
    sembsl_mxl5007ce6353_obj    *p_obj; 
    U32                         val;
 
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
 
    switch(item_id) 
    { 
    case SEMHAL_FE_STATE_LOCK: 
        *p_val=0; 
        res = sembsl_ce6353_get(&p_obj->demod,SEMHAL_FE_STATE_LOCK,p_val);  
        if((*p_val)&SEMHAL_FE_HAS_LOCK) p_obj->api_flags|=SEMBSL_FE_LCKD; 
        p_obj->state=*p_val; 
        break; 
 
    case SEMHAL_FE_STATE_SNR_DBX100:            /* ! result is signed! */ 
    case SEMHAL_FE_STATE_PREBER: 
    case SEMHAL_FE_STATE_PREBERLP: 
    case SEMHAL_FE_STATE_POSTBER: 
    case SEMHAL_FE_STATE_POSTBERLP: 
    case SEMHAL_FE_STATE_AFC:                   /* ! result is signed! */ 
    case SEMHAL_FE_STATE_MODULATION: 
    case SEMHAL_FE_STATE_MODULATIONLP: 
    case SEMHAL_FE_STATE_UCB: 
    case SEMHAL_FE_STATE_UCBLP: 
    case SEMHAL_FE_STATE_SR: 
    case SEMHAL_FE_STATE_FFT: 
    case SEMHAL_FE_STATE_GI: 
    case SEMHAL_FE_STATE_CR: 
    case SEMHAL_FE_STATE_CRLP: 
    case SEMHAL_FE_STATE_AGCTRESHOLD: 
    case SEMHAL_FE_STATE_HR: 
    case SEMHAL_FE_STATE_AGC:  
        *p_val=0; 
        res = sembsl_ce6353_get(&p_obj->demod,item_id,p_val);  
        break; 
 
    case SEMHAL_FE_STATE_RSSI_DBMX100: 
        *p_val=0;  
        res|=sembsl_ce6353_get(&p_obj->demod,SEMHAL_FE_STATE_AGC,&val);   
        SEMDBG_CHECK(res);  
        *p_val=_lookup((S32)(val),_rssi_dbx100_lookup,sizeof(_rssi_dbx100_lookup));  
        break; 
 
    case SEMHAL_FE_STATE_DEMOD_REG: 
        res = sembsl_ce6353_get(&p_obj->demod,item_id,p_val);  
        break; 
 
    default: 
        res=SEM_ENOT_SUPPORTED; 
    } 
 
err_exit: 
    return res; 
} 
 
static int 
mxl5007ce6353_set_state 
    ( 
        U32         	    unit,	        /* I: frontend unit number */ 
	U32                 item_id,	        /* I: identifier of the item */ 
        U32	            val		        /* I: input value */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 

    switch(item_id) 
    { 
    case SEMHAL_FE_STATE_MODULATION: 
    case SEMHAL_FE_STATE_SR: 
    case SEMHAL_FE_STATE_DEMOD_REG: 
    case SEMHAL_FE_STATE_RESET: 
    case SEMHAL_FE_STATE_BW: 
    case SEMHAL_FE_STATE_TRACKING: 
    case SEMHAL_FE_STATE_AGCTRESHOLD: 
    case SEMHAL_FE_STATE_HR: 
        res|= sembsl_ce6353_set(&p_obj->demod,item_id,val);  
        break; 
 
    case SEMHAL_FE_STATE_POWER: 
        { 
            sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,1); 
                if(val) 
                    res|=_mxl2semerr(MxL5007_Stand_By(&p_obj->tuner)); 
                else 
                    res|=_mxl2semerr(MxL5007_Wake_Up(&p_obj->tuner)); 
            sembsl_ce6353_set(&p_obj->demod,SEMHAL_FE_STATE_I2CSWITCH,0); 
            res|= sembsl_ce6353_set(&p_obj->demod,item_id,val);   
            p_obj->api_flags&=~SEMBSL_FE_PWR; 
        } 
        break; 
	case SEMHAL_FE_STATE_I2CSWITCH:
		res |= sembsl_ce6353_set(&p_obj->demod, SEMHAL_FE_STATE_I2CSWITCH, val);
		break;
    default: 
        res=SEM_ENOT_SUPPORTED; 
    } 
 
err_exit: 
    return res; 
} 
 
 
static int 
mxl5007ce6353_get_cfg_bulk 
    ( 
        U32         	    unit,	        /* I: frontend unit number   */ 
        U32                 *p_cfg_values,	/* O: array of cfg. item_id,value pairs */ 
        U32                 cfg_num             /* I: number of elements in p_cfg_values array */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    SEMDBG_CHECK(res); 
     
    if(cfg_num&&p_cfg_values) 
    { 
        res=SEM_ENOT_SUPPORTED;/*!!!*/
    } 
 
err_exit: 
    return res; 
} 
 
static int 
mxl5007ce6353_set_cfg_bulk 
    ( 
        U32         	    unit,	        /* I: frontend unit number                      */ 
        U32                 *p_cfg_values,	/* O: array of cfg. item_id,value pairs         */ 
        U32                 cfg_num             /* I: number of elements in p_cfg_values array. */ 
    ) 
{ 
    int res; 
    struct  
    sembsl_mxl5007ce6353_obj *p_obj; 
     
    res=_get_instance(unit, &p_obj); 
    if(res<0&&SEM_ENOT_INITIALIZED!=res) goto err_exit; 
    res=SEM_SOK; 
 
    if(cfg_num&&p_cfg_values) 
    { 
        res=SEM_ENOT_SUPPORTED;/*!!!*/ 
    } 
 
err_exit: 
    return res; 
} 
 
 
 
 
 
/** 
 * Constants 
 */ 
struct semhal_fe_ops g_sembsl_mxl5007ce6353= 
{ 
    mxl5007ce6353_init, 
    mxl5007ce6353_deinit, 
    mxl5007ce6353_get_info, 
    mxl5007ce6353_get_sw_version, 
    mxl5007ce6353_search_start, 
    mxl5007ce6353_search_algo, 
    mxl5007ce6353_search_abort, 
    mxl5007ce6353_get_cfg, 
    mxl5007ce6353_set_cfg, 
    mxl5007ce6353_get_state, 
    mxl5007ce6353_set_state, 
    mxl5007ce6353_get_cfg_bulk, 
    mxl5007ce6353_set_cfg_bulk,
    {NULL,NULL,NULL}
}; 
	 
 
