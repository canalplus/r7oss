/************************************************************************
Copyright (C)  STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : report.h
Author :           Nick

    Contains the header file for the report functions,
    The actual reporting function, and a function to restrict
    the range of severities reported (NOTE Fatal and greater
    messages are always reported). It is upto the caller to
    insert file/line number information into the report. Process
    information will be prepended to any report of severity error
    or greater.

Date        Modification                                    Name
----        ------------                                    ----
18-Dec-96   Created                                         nick
************************************************************************/

#ifndef __REPORT_H
#define __REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

//

#define REPORT_STRING_SIZE      256

typedef enum
    {
	severity_info           = 0,
	severity_note           = 50,
	severity_error          = 100,
	severity_fatal          = 200,
	severity_interrupt      = 1000
    } report_severity_t;

void report_init( void );

void report_restricted_severity_levels( int lower_restriction,
					int upper_restriction );

#if (defined(__KERNEL__) && !defined(CONFIG_PRINTK)) || !defined(REPORT)
static void report(report_severity_t report_severity,const char *format,...) { }
#else
void report( report_severity_t   report_severity,
	     const char         *format, ... );
#endif

#ifdef REPORT
void report_dump_hex( report_severity_t level,
		      unsigned char    *data,
		      int               length,
		      int               width,
		      void*             start );
#else
#define report_dump_hex(a,b,c,d,e) do { } while(0)
#endif

#if 0
#define DEBUG_EVENT_BASE 0x0b000000

static unsigned int *debug_base= (unsigned int *)DEBUG_EVENT_BASE;
static unsigned int *debug_data= (unsigned int *)(DEBUG_EVENT_BASE + 0x40);

#define initialise_debug_event()        {volatile int i; for( i=0; i<0x2000; i++ ) debug_base[i] = 0; i = debug_base[16]; }

#define print_debug_events()            {int i; for( i=0; i<(512+16); i+=8 )    \
						report( severity_info, "        %08x %08x %08x %08x %08x %08x %08x %08x\n",     \
						debug_base[i+0],debug_base[i+1],debug_base[i+2],debug_base[i+3],        \
						debug_base[i+4],debug_base[i+5],debug_base[i+6],debug_base[i+7] ); }

//#define debug_event(code)            if( debug_base[15] != 0xfeedface ) {volatile unsigned int dummy; dummy=debug_base[0]; debug_data[dummy++] = (unsigned int)code; dummy &= 0x1ff; debug_data[dummy] = 0xffffffff; debug_base[0] = dummy; dummy = debug_base[32];} else { while(true) task_delay(100); }
#define debug_event(code)              {volatile unsigned int dummy; dummy=debug_base[0]; debug_data[dummy++] = (unsigned int)code; dummy &= 0x1ff; debug_data[dummy] = 0xffffffff; debug_base[0] = dummy; }


#else
#define initialise_debug_event()
#define print_debug_events()
#define debug_event(code)
#endif

//

#ifdef __cplusplus
}
#endif
#endif
