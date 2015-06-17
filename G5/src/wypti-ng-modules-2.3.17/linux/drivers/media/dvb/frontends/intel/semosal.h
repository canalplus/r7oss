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
* @file			semosal.h 
* @brief		Contains a STB OS Abstraction Layer interface types,functions,structures for the SEMCO common STB platform API. 
* 
* @version	        2.01.001 
* @author	        Alex Loktionoff (alex.loktionov@samsung.com) 
* @document             Common STB platform API v1.5 
* 
* @note			Contains ONLY ABSTRACT extern definitions, for implementation look for semosal_win32*.*, semosal_linux*.* 
* 
* Created 2009.02.07 
* 
*/ 
 
#ifndef _SEMOSAL_H_ 
#define _SEMOSAL_H_ 
 
 
 
/**
 * I2C functionality 
 * 
 * All semosal_xxx_xxx() functions are defined in STB OS specific .C file 
 * look in semosal_st20.c or semosal_win32.c ... 
 */ 
 
extern int 
semosal_init 
    ( 
    ); 
 
/** 
 * ST similar style interface 
 */ 
#define SEM_I2C_READ        0		        /* Read from slave */ 
#define SEM_I2C_WRITE       1		        /* Write to slave */ 
#define SEM_I2C_WRITENOSTOP 2                   /* Write to slave without stop */    
 
#define SEM_I2C_OK          0                   /* Non 0 value means error */ 
 
#define SEM_I2C_ADDRESS_7_BITS  0 
#define SEM_I2C_ADDRESS_10_BITS 1 
 
 
struct semosal_i2c_obj 
{ 
    int  
    (*readwrite)                                /* ST style I2C driver */ 
        ( 
            struct  
            semosal_i2c_obj *p_obj, 
            U32             mode,               /* I: I2C mode READ/WRITE/WRITENOSTOP */ 
            U8              *pbytes,            /* IO:I2C data */ 
            U32             nbytes              /* I: number of bytes */ 
        ); 
 
    int  
    (*open)                                     /* ST style I2C driver. Get handle of the RF or Demod */ 
        ( 
            struct  
            semosal_i2c_obj *p_obj, 
            U32             addr,               /* I: I2C slave address */ 
            U32             addr_10bits         /* I: I2C slave address type 7/10 bits */ 
        ); 
 
    int  
    (*close)                                    /* ST style I2C driver. Get handle of the RF or Demod */ 
        ( 
            struct  
            semosal_i2c_obj *p_obj 
        ); 
 
    int   
    (*lock)                                    /* locking I2C bus for read/write transactions */  
        (  
            struct   
            semosal_i2c_obj *p_obj,
            BOOL            lfag
        ); 
     
  
    HI2C_T                  h_i2c_handle;       /* I: i2c controller abstract interface */ 
    void                    *p_data; 
}; 
 
#define SEMOSAL_I2C_WRITE(p_obj,pbytes,nbytes) (p_obj)->readwrite(p_obj,SEM_I2C_WRITE,pbytes,nbytes) 
#define SEMOSAL_I2C_WRITENOSTOP(p_obj,pbytes,nbytes) (p_obj)->readwrite(p_obj,SEM_I2C_WRITENOSTOP,pbytes,nbytes) 
#define SEMOSAL_I2C_READ(p_obj,pbytes,nbytes) (p_obj)->readwrite(p_obj,SEM_I2C_READ,pbytes,nbytes) 
 
#define SEMOSAL_I2C_OPEN(p_obj,addr) (p_obj)->open(p_obj,addr,SEM_I2C_ADDRESS_7_BITS) 
#define SEMOSAL_I2C_OPEN10(p_obj,addr) (p_obj)->open(p_obj,addr,SEM_I2C_ADDRESS_10_BITS) 
#define SEMOSAL_I2C_CLOSE(p_obj) (p_obj)->close(p_obj) 
 
#define SEMOSAL_I2C_LOCK(p_obj) (p_obj)->lock(p_obj,TRUE)  
#define SEMOSAL_I2C_UNLOCK(p_obj) (p_obj)->lock(p_obj,FALSE)  
 
/**
 * 
 * Timer functionality 
 */ 
extern BOOL 
semosal_tick_after                              /* TRUE if t1 after t2 */ 
    ( 
        TICK_T      t1, 
        TICK_T      t2 
    ); 
 
extern TICK_T 
semosal_tick_now(void);                          /* current tick count */ 
	 
extern TICK_T 
semosal_tick_minus                              /* t1-t2 */ 
    ( 
        TICK_T      t1, 
        TICK_T      t2 
    ); 
 
extern TICK_T                                    
semosal_tick_plus                              /* t1+t2 */ 
    ( 
        TICK_T      t1, 
        TICK_T      t2 
    ); 
	 
extern U32 
semosal_elapsed_msec                             
    ( 
        TICK_T      begin, 
        TICK_T      end 
    ); 
 
extern TICK_T                                    
semosal_msec2tick                               /* msec->system ticks */ 
    ( 
        U32         msec 
    ); 
 
extern void 
semosal_wait_msec 
    (  
        U32         msec 
    ); 
 
 
/**
 * 
 * Debug trace functionality 
 */ 
extern void 
semosal_set_trace_level 
	( 
	    U32	            id 
        ); 
 
 
extern int 
semosal_trace 
	( 
	    U32	            id, 
            U32             param 
        ); 
 
/**
 * STR/MEM  
 */ 
 
#if 0 
 
#define semosal_memset  memset 
#define semosal_strlcpy strlcpy 
#define semosal_strlcat strlcat 
#define semosal_strlen  strnlen 
 
#else 
void 
semosal_memset(void *d,U8 b,U32 size); 
 
/**  
 * @note    See http://www.gratisoft.us/todd/papers/strlcpy.html  
 */ 
U32 
semosal_strlcpy(char *d, const char *s, U32 siz); 
U32 
semosal_strlcat(char *d, const char *s, U32 siz); 
U32 
semosal_strnlen(const char *s, U32 siz); 
int 
semosal_strncmp(const char *s1,const char *s2,U32 n); 
 
#endif 
 
 
#endif /*#ifndef _SEMOSAL_H_ */
 
