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
* @file			sembsl_ce6353.c 
* @brief		Implementation of CE6353 DEMOD  
* 
* @version	        1.00.005
* @author	        Alexander Loktionov (alex.loktionov@samsung.com) aka OXY 
* 
* Created 2009.05.06
* 
*/ 
 
#include "mxl5007-ce6353.h" 
#include "semstbcp.h" 
#include "semosal.h" 
#include "semhal_fe.h" 
#include "sembsl_ce6353.h" 
 
 
#define THIS_FILE_ID        0x353 
#define THISFILE_TRACE      SEMDBG_TRACE_BSL 
 
 
#define CE6353_PAR_SER      0xC4                /* Parallel mode : 0xC4, Serial mode : 0xE4 */ 
#define CE6353_BW           8000000             /* Bandwidth n Hz */  
#define CE6353_AGC_TRGT     0x30 
#define CE6353_IF           0x19F8
#define CE6353_BOARD        0 

static int debug = 1;

#define dprintk(fmt, args...) \
	do { \
		if (debug) printk(KERN_DEBUG "%s: "fmt, __FUNCTION__, ## args); \
	} while (0)

 
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
    ) 
{ 
    int                     res; 
  
    SEMDBG_ENTER(MK_4CC('i','n','i','t'));  

    p_obj->i2c=*p_i2c_adapter; 
    *tuner_i2c=*p_i2c_adapter; 
     
    res=SEMOSAL_I2C_OPEN(&p_obj->i2c,i2c_addr); 
    if(SEM_FAILED(res)) goto open_err_exit; 
 
    res=sembsl_ce6353_set(p_obj, SEMHAL_FE_CFG_BOARD, CE6353_BOARD); 
    SEMDBG_CHECK(res); 
 
    SEMDBG_EXIT(MK_4CC('i','n','i','t'));  
    return res; 
 
err_exit:; 
    sembsl_ce6353_deinit(p_obj); 
 
open_err_exit:; 
    SEMDBG_EXIT(MK_4CC('i','n','i','t'));  
    SEMDBG_DEBUG(res); 
    return res; 
} 
 
int 
sembsl_ce6353_deinit 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj 
    ) 
{ 
    int                     res; 
 
    SEMDBG_ENTER(MK_4CC('d','i','n','i'));  
    res=SEMOSAL_I2C_CLOSE(&p_obj->i2c); 
    SEMDBG_CHECK(res); 
 
err_exit: 
    SEMDBG_EXIT(MK_4CC('d','i','n','i'));  
    SEMDBG_DEBUG(res); 
    return res; 
} 
 
 
 
     
 
int 
sembsl_ce6353_get_sw_version 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 *pu_ver		/* O: SW version 31..24 major, 
                                                      23..16 -minor, 15..0 -build */ 
    ) 
{ 
    if(!p_obj) 
        return SEM_EBAD_PARAMETER; 
 
    *pu_ver=MK_U32((SEMBSL_CE6353_MAJOR_VER<<8)+SEMBSL_CE6353_MINOR_VER,SEMBSL_CE6353_BUILD_NUM); 
     
    return SEM_SOK; 
} 
 
 
static int 
_i2c_read 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 reg, 
        U32                 *p_value 
    ) 
{
	u8 buf[] = {(u8)reg};
	u8 b[] = {0};
	int ret = 0;

	struct intel_state *state = (struct intel_state *)p_obj->i2c.p_data;
	struct i2c_msg msg[] = { { .addr = state->demod_address, .flags = 0, .buf = buf, .len = 1},
						{ .addr = state->demod_address, .flags = I2C_M_RD, .buf = b, .len = 1}};

	ret = i2c_transfer(state->demod_i2c, msg, 2);
	if (ret != 2)
	{
		printk("%s error: addr = 0x%02x, reg=0x%02x, ret = 0x%02x\n", __FUNCTION__, state->demod_address, reg, ret);
		return -1;
	}

	*p_value = (int)b[0];
	return 0;
} 
 
static int 
_i2c_write 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 reg, 
        U32                 value 
    ) 
{
	u8 buf[] = {(u8)reg, (u8)value};
	int ret = 0;

	struct intel_state *state = (struct intel_state *)p_obj->i2c.p_data;
	struct i2c_msg msg = { .addr = state->demod_address, .flags = 0, .buf = buf, .len = 2};

	ret = i2c_transfer(state->demod_i2c, &msg, 1);
	if (ret != 1)
	{
		printk ("%s error :(addr) = 0x%02x, (reg) = 0x%02x, (ret) = 0x%02x\n", __FUNCTION__, state->demod_address, reg, ret);
		return -1;
	}
	return 0;
} 
 
static int 
_i2c_write_bulk 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U8                  *bytes 
    ) 
{
	int ret=-1;

	for(; *bytes; bytes+=2)
	{
		ret = _i2c_write(p_obj, *bytes, *(bytes+1));
		if (ret)
		{
			printk("%s failed\n", __FUNCTION__);
			return -1;
		}
	}
	return 0;
} 
 
#define TPS_CONSTELLATION(wTPS) (((wTPS)>>13) & 0x03) 
#define TPS_HIERARCHY(wTPS)     (((wTPS)>>10) & 0x07) 
#define TPS_HPCODERATE(wTPS)    (((wTPS)>>7) & 0x07) 
#define TPS_LPCODERATE(wTPS)    (((wTPS)>>4) & 0x07) 
#define TPS_GUARDINTERVAL(wTPS) (((wTPS)>>2) & 0x03) 
#define TPS_ISFFTMODE8K(wTPS)      ( (wTPS) &  0x01) 
#define TPS_LPACTIVE(wTPS)		(BOOL)((wTPS) & 0x8000) 
 
 
static int 
_sembsl_ce6353_get_tps 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj 
    ) 
{ 
    int                     res; 
    U32                     tmp; 
     
    res =_i2c_read(p_obj,0x1D,&tmp); 
    p_obj->state.tps=tmp<<8; 
    res|=_i2c_read(p_obj,0x1E,&tmp); 
    p_obj->state.tps|=tmp; 
 
    return res; 
} 
 
static int 
_sembsl_ce6353_get_post_ber 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 *p_val 
    ) 
{ 
    int                     res; 
    U32                     cnt,per; 
    U32                     val; 
 
    res=SEM_SOK; 
    res|=_i2c_read(p_obj,0x60,&val); 
    per = val; 
    res|=_i2c_read(p_obj,0x61,&val); 
    per <<=8 ; 
    per += val; 
    res|=_i2c_read(p_obj,0x11,&val); 
    cnt = val; 
    res|=_i2c_read(p_obj,0x12,&val); 
    cnt <<=8 ; 
    cnt += val; 
    res|=_i2c_read(p_obj,0x13,&val); 
    cnt <<=8 ; 
    cnt += val; 
 
    if (per) 
    { 
        cnt *= 240;  /* x60 and x4 for maximum resolution*/ 
        cnt/=per; 
        cnt +=2; 
        cnt/=4; 
        if (0x80000000&cnt) 
        { 
            cnt=0x7FFFFFFFU; 
        } 
    } 
    else 
    { 
        cnt=0xFFFFFFFFU; /* error */ 
    } 
    *p_val=cnt; 
 
    return res; 
} 
 
static int 
_sembsl_ce6353_get_afc 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
        U32                 *p_val 
    ) 
{ 
    int                     res; 
    S32                     tmpOffset;  
    S32                     tmp; 
    S32                     lOffsetkHz; 
    U32                     val1; 
    U32                     val; 
    S16                     ret; 
    S16                     itb_freq_off; 
     
    res=SEM_SOK; 
    res|=_i2c_read(p_obj,0x18,&val); 
    res|=_i2c_read(p_obj,0x18,&val1); 
 
    if(SEM_SUCCEEDED(res)) 
    { 
        tmp = val; 
        res|=_i2c_read(p_obj,0x19,&val); 
        tmp <<=8 ; 
        tmp += val; 
        res|=_i2c_read(p_obj,0x1a,&val); 
        tmp <<=8 ; 
        tmp += val; 
         
        tmpOffset = (S32)tmp;  
        lOffsetkHz = tmpOffset;  
         
        lOffsetkHz = (lOffsetkHz<<8)>>8; 
        lOffsetkHz = lOffsetkHz * (p_obj->cfg.bw/1000000); 
        if (val1 & 0x03) 
        { 
            lOffsetkHz/=4; 
        } 
         
        lOffsetkHz /= 14680; 
        if (lOffsetkHz>32767) lOffsetkHz=32767;  
        if (lOffsetkHz<-32768) lOffsetkHz=-32768;  
    } 
    else 
    { 
        lOffsetkHz =0; 
    } 
 
    res|=_i2c_read(p_obj,0x16,&val); 
    if(SEM_SUCCEEDED(res)) 
    { 
        tmp = val; 
        res|=_i2c_read(p_obj,0x17,&val); 
        tmp <<=8 ; 
        tmp += val; 
         
        itb_freq_off = (S16)tmp; 
        itb_freq_off = (S16)((itb_freq_off  * 11) / 256); 
         
        lOffsetkHz += itb_freq_off; 
    } 
    ret=(S16)(lOffsetkHz * -1); 
    *(S32*)(p_val)=ret*1000; 
 
    return res;  
} 
  
 
static int 
_sembsl_read_status 
    (  
        struct   
        sembsl_ce6353_obj   *p_obj 
    ) 
{ 
	u8 b[] = {0x06};
	u8 b1[4]; 
	int ret = 0;

	struct intel_state *state = (struct intel_state *)p_obj->i2c.p_data;
	struct i2c_msg msg[] = {{ .addr = state->demod_address, .flags = 0, .buf = b, .len = 1},
							{.addr = state->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 4 }};


	ret = i2c_transfer(state->demod_i2c, msg, 2);

	if (ret != 2)
	{
		printk("%s error: addr = 0x%02x, ret = 0x%02x\n", __FUNCTION__, state->demod_address, ret);
		return -1;
	}

	p_obj->state.status=(b1[0]<<24)|(b1[1]<<16)|(b1[2]<<8)|b1[3];
	return 0;
} 

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
    ) 
{ 
    int                     res; 
    U32                     val; 
    U32                     val2; 
    U32                     i; 
 
    SEMDBG_ENTER(MK_4CC('_','g','e','t'));  
    SEMDBG_DEBUG(item_id); 
    
    res=SEM_SOK; 
    if(!p_val) res=SEM_EBAD_PARAMETER; 
    SEMDBG_CHECK(res); 
 
    res=SEM_SOK; 
    item_id&=SEMHAL_FE_CFG_MASK; 
    switch(item_id) 
    { 
    case SEMHAL_FE_STATE_LOCK&SEMHAL_FE_CFG_MASK: 
        *p_val=0; 
        res|=_sembsl_read_status(p_obj); 
        SEMDBG_CHECK(res); 
        if(p_obj->state.status&0x04000000) *p_val|=SEMHAL_FE_HAS_CARRIER; 
        if(p_obj->state.status&0x01000000) *p_val|=SEMHAL_FE_HAS_SIGNAL; 
        if(p_obj->state.status&0x00200000) *p_val|=SEMHAL_FE_HAS_SYNC; 
        if(p_obj->state.status&0x08000000) *p_val|=SEMHAL_FE_HAS_VITERBI; 
        if(p_obj->state.status&0x40000000) *p_val|=SEMHAL_FE_HAS_LOCK;  
        break; 
 
    case SEMHAL_FE_STATE_AGC&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_read(p_obj,0x0A,&val); 
        i=(S32)val; 
        res|=_i2c_read(p_obj,0x0B,&val); 
        i<<=8; 
        i|=(S32)val; 
        i>>= 4;  
        *p_val=i;
        break; 
 
    case SEMHAL_FE_CFG_SNR_DBX100&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_read(p_obj,0x10,p_val); 
        *p_val=(*p_val*25)/2; 
        break; 
     
    case SEMHAL_FE_STATE_POSTBERLP&SEMHAL_FE_CFG_MASK: 
    case SEMHAL_FE_STATE_POSTBER&SEMHAL_FE_CFG_MASK: 
        { 
            res|=_sembsl_ce6353_get_post_ber(p_obj,p_val); 
        } 
        break; 
 
    case SEMHAL_FE_STATE_PREBER&SEMHAL_FE_CFG_MASK: 
    case SEMHAL_FE_STATE_PREBERLP&SEMHAL_FE_CFG_MASK: 
        *p_val=0; 
        res|=SEM_SFALSE; 
        break; 
 
    case SEMHAL_FE_STATE_UCBLP&SEMHAL_FE_CFG_MASK: 
    case SEMHAL_FE_STATE_UCB&SEMHAL_FE_CFG_MASK: 
        { 
            res|=_i2c_read(p_obj,0x14,&val); 
            res|=_i2c_read(p_obj,0x15,&val2); 
            if(SEM_SUCCEEDED(res)) 
            { 
	        val2<<=8; 
                p_obj->state.ucb+=val|val2; 
	        *p_val=p_obj->state.ucb; 
            } 
        } 
        break; 
 
    case SEMHAL_FE_STATE_MODULATIONLP&SEMHAL_FE_CFG_MASK: 
    case SEMHAL_FE_STATE_MODULATION&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
        switch(TPS_CONSTELLATION(p_obj->state.tps)) 
        { 
	case 0: *p_val=SEMHAL_FE_QAM_4;break; 
	case 1: *p_val=SEMHAL_FE_QAM_16;break; 
	case 2: *p_val=SEMHAL_FE_QAM_64;break; 
	default:*p_val=0;; 
        }; 
        break; 
 
    case SEMHAL_FE_STATE_FFT&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
        if(TPS_ISFFTMODE8K(p_obj->state.tps)) 
            *p_val=SEMHAL_FE_FFT_8K; 
        else 
            *p_val=SEMHAL_FE_FFT_2K; 
        break; 
 
    case SEMHAL_FE_STATE_GI&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
	switch (TPS_GUARDINTERVAL(p_obj->state.tps)) 
	{ 
	case 0:*p_val=SEMHAL_FE_GI_1_32;break; 
	case 1:*p_val=SEMHAL_FE_GI_1_16;break; 
	case 2:*p_val=SEMHAL_FE_GI_1_8;break; 
	case 3:*p_val=SEMHAL_FE_GI_1_4;break; 
	default:*p_val=SEMHAL_FE_GI_NONE;break; 
	} 
        break; 
 
    case SEMHAL_FE_STATE_CR&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
	switch (TPS_HPCODERATE(p_obj->state.tps)) 
	{ 
	case 0:*p_val=SEMHAL_FE_FEC_1_2;break; 
	case 1:*p_val=SEMHAL_FE_FEC_2_3;break; 
	case 2:*p_val=SEMHAL_FE_FEC_3_4;break; 
	case 3:*p_val=SEMHAL_FE_FEC_5_6;break; 
	case 4:*p_val=SEMHAL_FE_FEC_7_8;break; 
	case 5:*p_val=SEMHAL_FE_FEC_8_9;break; 
	default:*p_val=SEMHAL_FE_FEC_NONE;break; 
	} 
        break; 
    case SEMHAL_FE_STATE_CRLP&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
	switch (TPS_LPCODERATE(p_obj->state.tps)) 
	{ 
	case 0:*p_val=SEMHAL_FE_FEC_1_2;break; 
	case 1:*p_val=SEMHAL_FE_FEC_2_3;break; 
	case 2:*p_val=SEMHAL_FE_FEC_3_4;break; 
	case 3:*p_val=SEMHAL_FE_FEC_5_6;break; 
	case 4:*p_val=SEMHAL_FE_FEC_7_8;break; 
	case 5:*p_val=SEMHAL_FE_FEC_8_9;break; 
	default:*p_val=SEMHAL_FE_FEC_NONE;break; 
	} 
        break; 
 
    case SEMHAL_FE_STATE_HR&SEMHAL_FE_CFG_MASK: 
        res|=_sembsl_ce6353_get_tps(p_obj); 
        switch(TPS_HIERARCHY(p_obj->state.tps)) 
	{ 
	case 0: *p_val=SEMHAL_FE_HIERARCHY_NONE;break; 
	case 1: *p_val=SEMHAL_FE_HIERARCHY_1;break; 
	case 2: *p_val=SEMHAL_FE_HIERARCHY_2;break; 
	case 3: *p_val=SEMHAL_FE_HIERARCHY_4;break; 
	default:*p_val=0; 
	} 
        break; 
 
    case SEMHAL_FE_STATE_AFC&SEMHAL_FE_CFG_MASK:/* ! result is signed! */ 
        res|=_sembsl_ce6353_get_afc(p_obj,p_val); 
        break; 
 
    case SEMHAL_FE_STATE_SR&SEMHAL_FE_CFG_MASK: 
        *p_val=0; 
        if(8000000==p_obj->cfg.bw) *p_val=7607000;  
        if(7000000==p_obj->cfg.bw) *p_val=6656000; 
        if(6000000==p_obj->cfg.bw) *p_val=5705000; 
        res|=SEM_SFALSE; 
        break; 
 
    case SEMHAL_FE_STATE_AGCTRESHOLD&SEMHAL_FE_CFG_MASK:  
        res|=SEM_SFALSE; 
        break;  
 
    case SEMHAL_FE_STATE_IDENTITY&SEMHAL_FE_CFG_MASK: 
        *p_val=0; 

        res=_i2c_read(p_obj,0x7F,p_val); 
        if(0x14==*p_val) 
            *p_val=MK_U32(SEMID_ENCA('c','e','@'),SEMID_ENCI(6353,0));  
        else 
            *p_val=0; 
        break; 
 
    case SEMHAL_FE_STATE_DEMOD_REG&SEMHAL_FE_CFG_MASK: 
        val=*p_val; 
        res=_i2c_read(p_obj,val,p_val);  
        break; 
 
    default: 
        res=SEM_ENOT_SUPPORTED; 
        SEMDBG_CHECK(res); 
    } 
 
err_exit: 
    SEMDBG_EXIT(MK_4CC('_','g','e','t'));  
    SEMDBG_DEBUG(res); 
    return res; 
} 
 
static U8 _ary_init[]= 
{ 
    0x8E,0x40, 
    0x50,0x03, 
    0x51,0x44, 
    0x52,0x46, 
    0x53,0x15, 
    0x54,0x0F, 
    0x55,0x80, 
    0x9C,0xA1, 
    0xBA,0xFD, 
    0x7C,0x2B, 
    0x5A,0xCD, 
    0x55,0x40, 
    0xEA,0x01, 
    0xEA,0x00, 
    0x67,0xC0, 
    0x84,0xF5, 
    0x85,0x09, 
    0xD6,0x33, 
    0x61,0x0F, 
    0 
}; 
static U8 _ary_bw8[]= 
{ 
    0x64,0x36, 
    0x65,0x67, 
    0x66,0xE5, 
    0xCC,0x73, 
    0x9A,0x00, 
    0x9B,0x00, 
    0 
}; 
 
static U8 _ary_bw7[]= 
{ 
    0x64,0x35, 
    0x65,0x5A, 
    0x66,0xE9, 
    0xCC,0x73, 
    0x9A,0x00, 
    0x9B,0x00, 
    0 
}; 
 
static U8 _ary_bw6[]=  
{  
    0  
};  
 
 
static U8 _ary_reset8[]=  
{  
    0x64,0x36, 
    0xCC,0x73, 
    0x65,0x67, 
    0x66,0xE5, 
    0x5C,0x75, 
    0x5F,0x11, 
    0x5E,0x40, 
    0x71,0x01, 
    0 
};
static U8 _ary_reset7[]=  
{  
    0x64,0x35, 
    0xCC,0xA0, 
    0x65,0x5A, 
    0x66,0xE9, 
    0x5C,0x86, 
    0x5F,0x11, 
    0x5E,0x40, 
    0x71,0x01, 
    0 
}; 
  
 
static U8 _ary_reset6[]=  
{ 
    0x64,0x34, 
    0x64,0x3C, 
    0x9A,0xB4, 
    0x9B,0x04, 
    0x9A,0xCD, 
    0x9B,0x19, 
    0x9A,0x87, 
    0x9B,0x22, 
    0x9A,0xBD, 
    0x9B,0x3D, 
    0x9A,0x19, 
    0x9B,0x4A, 
    0x9A,0x09, 
    0x9B,0x53, 
    0x9A,0xC1, 
    0x9B,0x6B, 
    0x9A,0x5C, 
    0x9B,0x7A, 
    0x9A,0x8B, 
    0x9B,0x83, 
    0x9A,0x37, 
    0x9B,0x9B, 
    0x9A,0x6E, 
    0x9B,0xAA, 
    0x9A,0xDF, 
    0x9B,0xB3, 
    0x9A,0x34, 
    0x9B,0xC0, 
    0x9A,0x51, 
    0x9B,0xD0, 
    0x64,0x30, 
    0xDC,0x06, 
    0xDD,0x07, 
    0xCC,0xDC, 
    0x65,0x4D, 
    0x66,0xEC, 
    0x5C,0x9C, 
    0x5F,0x11, 
    0x5E,0x40, 
    0x71,0x01, 
    0 
}; 
 

int 
sembsl_ce6353_set 
    ( 
        struct  
        sembsl_ce6353_obj   *p_obj, 
	U32                 item_id,	        /* I: identifier of the item */ 
        U32	            val		        /* I: input value */ 
    ) 
{ 
    int                     res; 
  
    SEMDBG_ENTER(MK_4CC('_','s','e','t'));  
    SEMDBG_DEBUG(item_id); 
    SEMDBG_DEBUG(val); 

    res=SEM_SOK; 
    item_id&=SEMHAL_FE_CFG_MASK; 
    switch(item_id) 
    { 
    case SEMHAL_FE_STATE_I2CSWITCH&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_write(p_obj,0x62, val?0x12:0x0A); 
        break; 
 
    case SEMHAL_FE_STATE_RESET&SEMHAL_FE_CFG_MASK: 
        if(8==p_obj->cfg.bw/1000000)  
        {  
            res|=_i2c_write_bulk(p_obj,_ary_reset8);  
        } 
        else if(7==p_obj->cfg.bw/1000000)  
        {  
            res|=_i2c_write_bulk(p_obj,_ary_reset7);  
        } 
        else if(6==p_obj->cfg.bw/1000000)  
        {  
            res|=_i2c_write_bulk(p_obj,_ary_reset6);  
        } 
        else  
        {  
            res|=SEM_ENOT_SUPPORTED;  
        }  
        break; 
     
    case SEMHAL_FE_STATE_TRACKING&SEMHAL_FE_CFG_MASK: 
        p_obj->state.ucb=0; 
        break; 
 
    case SEMHAL_FE_STATE_HR&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_write(p_obj,0x6E,val?0x00:0x80);  
        p_obj->state.hr=val; 
        p_obj->state.ucb=0; 
        break; 
 
    case SEMHAL_FE_STATE_BW&SEMHAL_FE_CFG_MASK: 
        if(8==val/1000000) 
        { 
            res|=_i2c_write_bulk(p_obj,_ary_bw8); 
            p_obj->cfg.bw=val; 
        } 
        else if(7==val/1000000) 
        { 
            res|=_i2c_write_bulk(p_obj,_ary_bw7); 
            p_obj->cfg.bw=val; 
        } 
        else if(6==val/1000000)  
        {  
            res|=_i2c_write_bulk(p_obj,_ary_bw6);  
            p_obj->cfg.bw=val;  
        }  
        else 
        { 
            res|=SEM_ENOT_SUPPORTED; 
        } 
        break; 
 
    case SEMHAL_FE_CFG_BOARD&SEMHAL_FE_CFG_MASK: 
        p_obj->cfg.if_freq  =CE6353_IF; 
        p_obj->cfg.out_par_ser=CE6353_PAR_SER; 
        p_obj->cfg.bw       =CE6353_BW;  
        p_obj->cfg.agc_trgt=CE6353_AGC_TRGT; 

        break; 
 
    case SEMHAL_FE_CFG_AGCTRESHOLD&SEMHAL_FE_CFG_MASK:  
        p_obj->cfg.agc_trgt=val; 
        break;  
    case SEMHAL_FE_CFG_IF&SEMHAL_FE_CFG_MASK: 
        if(4570000==val) 
            p_obj->cfg.if_freq=0x19F8; 
        else if(36150000==val) 
            p_obj->cfg.if_freq=0xCD7E; 
        else 
            res|=SEM_ENOT_SUPPORTED; 
        break;  
        
    case SEMHAL_FE_STATE_INIT&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_write_bulk(p_obj,_ary_init);  
        res|=_i2c_write(p_obj, 0x6C, p_obj->cfg.if_freq>>8); 
        res|=_i2c_write(p_obj, 0x6D, p_obj->cfg.if_freq);  
        res|=_i2c_write(p_obj, 0x56, p_obj->cfg.agc_trgt );
        break; 
 
    case SEMHAL_FE_STATE_DEMOD_REG&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_write(p_obj,U32_HI(val),U32_LO(val));  
        break; 
 
    case SEMHAL_FE_STATE_MODULATION&SEMHAL_FE_CFG_MASK: 
    case SEMHAL_FE_STATE_SR&SEMHAL_FE_CFG_MASK: 
        res|=SEM_SFALSE; 
        break; 
 
    case SEMHAL_FE_STATE_POWER&SEMHAL_FE_CFG_MASK: 
        res|=_i2c_write(p_obj,0x50,val?0x02:0x03); 
        if(!val)
            res|= sembsl_ce6353_set(p_obj,SEMHAL_FE_STATE_RESET,val);   
        break; 
 
    default: 
        res=SEM_ENOT_SUPPORTED; 
        SEMDBG_CHECK(res);  
    } 
    SEMDBG_CHECK(res); 
 
err_exit: 
    SEMDBG_EXIT(MK_4CC('_','s','e','t'));  
    SEMDBG_DEBUG(res); 
    return res; 
} 
 
 
