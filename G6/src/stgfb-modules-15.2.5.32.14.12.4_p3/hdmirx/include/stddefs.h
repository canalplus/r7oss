#ifndef __HDMIRX_STDDEFS__
#define __HDMIRX_STDDEFS__

#include <linux/interrupt.h>
#include <linux/version.h>
#include "stm_hdmirx_os.h"

//#include <linux/kernel.h>

/* Function return error code */

/* Common unsigned types */
#ifndef DEFINED_U8
#define DEFINED_U8
typedef unsigned char  U8;
#endif

#ifndef DEFINED_U16
#define DEFINED_U16
typedef unsigned short U16;
#endif

#ifndef DEFINED_U32
#define DEFINED_U32
typedef unsigned int   U32;
#endif


/* Common signed types */
#ifndef DEFINED_S8
#define DEFINED_S8
typedef signed char  S8;
#endif

#ifndef DEFINED_S16
#define DEFINED_S16
typedef signed short S16;
#endif

#ifndef DEFINED_S32
#define DEFINED_S32
typedef signed int   S32;
#endif

#ifndef DEFINED_BOOL
#define DEFINED_BOOL
// Common Boolean logic
typedef enum bool_enum
{
  FALSE,
  TRUE
} BOOL;
#endif

//#define NULL 0
#define HDMI_PRINT(a,b...)  printk("stmhdmirx: %s: " a, __FUNCTION__ ,##b)
#define UNUSED_PARAMETER(x)    (void)(x)
#define SET_TRACE(a,b,c,d,e)   DEBUG_PRINT(d,e)
#define DEBUG_PRINT            HDMI_PRINT

/* Revision structure */
typedef const char * ST_revision_t;

typedef uint32_t stm_error_code_t;




#if 1  //TBD
//SWENG_0530 (1) Re-define for gm_ReadDWord issue
//  U32 far gm_ReadRegDWord(U16 W_RegAddr);

//******************************************************************************
//  M A C R O   D E F I N I T I O N
//******************************************************************************
//SWENG_0530 (2) For far Register Address Convertion
#define SEGBIT2SHIFT 8

#define LINEAR_TO_FPTR(Linear_Address) ((BYTE far *)\
    ((void _seg *)(((WORD)(Linear_Address >> 16)) << (16 - SEGBIT2SHIFT)) +\
    (void __near *)((WORD)Linear_Address)))

#define gW_HdmiRegBaseAddress 0

/*******U8********/
#define HDMI_READ_REG_BYTE(W_RegAddr)                                   \
    ReadRegisterByte(W_RegAddr)
#define HDMI_WRITE_REG_BYTE(W_RegAddr, RegValue)                        \
    WriteRegisterByte(W_RegAddr,RegValue)
#define HDMI_SET_REG_BITS_BYTE(W_RegAddr, W_RegValue)                   \
    WriteRegisterByte((W_RegAddr), (ReadRegisterByte((W_RegAddr))|((volatile U8)(W_RegValue))))
#define HDMI_CLEAR_REG_BITS_BYTE(W_RegAddr, W_RegValue)                 \
    WriteRegisterByte((W_RegAddr), (ReadRegisterByte((W_RegAddr)) & (~((volatile U8)(W_RegValue)))))
#define HDMI_CLEAR_AND_SET_REG_BITS_BYTE(W_RegAddr, B_AndData, B_OrData)\
    WriteRegisterByte((W_RegAddr), ((ReadRegisterByte((W_RegAddr)) & (~((volatile U8)(B_AndData))))|((volatile U8)(B_OrData))))


/*******U16********/
#define HDMI_READ_REG_WORD(W_RegAddr)                                   \
    ReadRegisterWord(W_RegAddr)
#define HDMI_WRITE_REG_WORD(W_RegAddr, W_RegValue)                      \
    WriteRegisterWord(W_RegAddr,W_RegValue )
#define HDMI_SET_REG_BITS_WORD(W_RegAddr, W_RegValue)                   \
        WriteRegisterWord((W_RegAddr), (ReadRegisterWord((W_RegAddr))|((volatile U16)(W_RegValue))))
#define HDMI_CLEAR_REG_BITS_WORD(W_RegAddr, W_RegValue)                 \
        WriteRegisterWord((W_RegAddr), (ReadRegisterWord((W_RegAddr)) & (~((volatile U16)(W_RegValue)))))
#define HDMI_CLEAR_AND_SET_REG_BITS_WORD(W_RegAddr, B_AndData, B_OrData)\
    WriteRegisterWord((W_RegAddr), ((ReadRegisterWord((W_RegAddr)) & (~((volatile U16)(B_AndData))))|((volatile U16)(B_OrData))))


/*TRI U8*/
#define HDMI_READ_REG_TRI_BYTES(W_RegAddr)                              \
    ReadRegisterTriByte(W_RegAddr)
#define HDMI_WRITE_REG_TRI_BYTES(W_RegAddr, BW_RegValue)                \
    WriteRegisterTriByte(W_RegAddr,BW_RegValue )

/*U32*/
#define HDMI_READ_REG_DWORD(W_RegAddr)                                  \
    ReadRegisterLong(W_RegAddr)
#define HDMI_WRITE_REG_DWORD(W_RegAddr, DW_RegValue)                    \
    WriteRegisterLong(W_RegAddr,DW_RegValue)
#define HDMI_SET_REG_BITS_DWORD(W_RegAddr, DW_RegValue)                 \
    WriteRegisterLong((W_RegAddr), ReadRegisterLong((W_RegAddr)) | (volatile U32)(DW_RegValue))
#define HDMI_CLEAR_REG_BITS_DWORD(W_RegAddr, DW_RegValue)               \
    WriteRegisterLong((W_RegAddr), ReadRegisterLong((W_RegAddr)) & ~((volatile U32)(DW_RegValue)))
#define HDMI_CLEAR_AND_SET_REG_BITS_DWORD(W_RegAddr, DW_AndData, DW_OrData) \
    WriteRegisterLong((W_RegAddr), ((ReadRegisterLong((W_RegAddr)) & (~((volatile U32)(DW_AndData))))|((volatile U32)(DW_OrData))))

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#if 0
#define BYTE U8
#define WORD U16
#define DWORD U32


#define EINVAL 1
#define EFAULT 2
#define ENODEV 3
#define ENOMEM 4
#define EINV 5
#define ENOSUP 6
#endif
#endif

#endif
