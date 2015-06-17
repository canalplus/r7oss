/************************************************************************
Copyright (C) 2010, 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the hdmirx Library.

hdmirx is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

hdmirx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with hdmirx; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The hdmirx Library may alternatively be licensed under a proprietary
license from ST.

Source file name : hdmirx_packet.c
Author :            Pravin Kumar


Date        Modification                                    Name
----        ------------                                    --------
6th July,2009   Created                                         Pravin Kumar

************************************************************************/

#ifndef __INTRENAL_TYPES_H__
#define __INTRENAL_TYPES_H__


#include <stm_hdmirx_os.h>



#include <linux/string.h>
/* General bit definition*/

#define BIT0        0x00000001
#define BIT1        0x00000002
#define BIT2        0x00000004
#define BIT3        0x00000008
#define BIT4        0x00000010
#define BIT5        0x00000020
#define BIT6        0x00000040
#define BIT7        0x00000080
#define BIT8        0x00000100
#define BIT9        0x00000200
#define BIT10       0x00000400
#define BIT11       0x00000800
#define BIT12       0x00001000
#define BIT13       0x00002000
#define BIT14       0x00004000
#define BIT15       0x00008000

#define BIT16       0x00010000UL
#define BIT17       0x00020000UL
#define BIT18       0x00040000UL
#define BIT19       0x00080000UL
#define BIT20       0x00100000UL
#define BIT21       0x00200000UL
#define BIT22       0x00400000UL
#define BIT23       0x00800000UL
#define BIT24       0x01000000UL
#define BIT25       0x02000000UL
#define BIT26       0x04000000UL
#define BIT27       0x08000000UL
#define BIT28       0x10000000UL
#define BIT29       0x20000000UL
#define BIT30       0x40000000UL
#define BIT31       0x80000000UL


/*Platform specific functions to operate with memory blocks*/
#define HDMI_STR_CPY(dest, src)     strcpy(dest, src)
#define HDMI_MEM_CPY(dest, src, n)      stm_hdmirx_memcpy(dest, src, n)
#define HDMI_MEM_SET(src, c, n)         memset(src, c, n)
#define HDMI_MEM_CMP(s1, s2, n)         memcmp(s1, s2, n)
#define HDMI_STR_LEN( src)              strlen(src)
#define HDMI_STR_CMP(s1, s2)            strcmp(s1, s2)

/*Short delay*/

#define     CPU_DELAY(count)                        stm_hdmirx_delay_us(count)
#define     M_NUM_TICKS_PER_MSEC(msec_num)          ((U32)((U32)(msec_num*STMGetClocksPerSecond()))/1000UL)
#define     TICKS_PER_MSEC                           1 //TBD
#define     STHDMIRX_GET_SYSTEM_TIME()              (stm_hdmirx_time_now()/TICKS_PER_MSEC)
#define     STHDMIRX_DELAY_1ms(x)                    stm_hdmirx_delay_ms(x)
#define     STMGetClocksPerSecond()                   HZ

#define HDMI_RAM
#define HDMI_MEM

#endif

/*********************************  END  **************************************/
