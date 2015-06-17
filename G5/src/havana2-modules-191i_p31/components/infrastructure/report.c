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

Source file name : report.c
Author :           Nick

Contains the report functions

Date        Modification                                    Name
----        ------------                                    ----
18-Dec-96   Created                                         nick
************************************************************************/

#if defined (__KERNEL__)
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#else
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#endif

#include "osinline.h"
#include "report.h"

/* --- */

/*#define REPORT_KPTRACE*/
#define REPORT_PRINTK

static int              severity_restriction_lower      = 0;
static int              severity_restriction_upper      = 0x7fffffff;
static char             report_buffer[REPORT_STRING_SIZE];

static OS_Mutex_t       report_lock;

#ifdef REPORT_UART
static void UartInit( void );
static void UartOutChar( char    c );
static void UartOutput( char    *String );
#endif

#ifdef REPORT_LOG
#define REPORT_LOG_SZ 2048
static char report_log[REPORT_LOG_SZ+1], *report_log_ptr = report_log;
#endif

/* --- */

void report_init( void )
{
    severity_restriction_lower  = 0;
    severity_restriction_upper  = 0x7fffffff;
    OS_InitializeMutex( &report_lock );

#if defined(REPORT_UART)
    UartInit();
#endif

    OS_RegisterTuneable( "min_trace_level", (unsigned int *) &severity_restriction_lower );

    return;
}

/* --- */

void report_restricted_severity_levels( int lower_restriction,
					int upper_restriction )
{
    severity_restriction_lower  = lower_restriction;
    severity_restriction_upper  = upper_restriction;
}

/* --- */

static void report_output( int level )
{
#if defined (REPORT_UART)
    UartOutput( report_buffer );
#elif defined (REPORT_LOG)
    char *p = report_buffer;

    while (0 != (*report_log_ptr++ = *p++)) {

#ifdef __ST200__
	__asm__ __volatile__ ("prgadd 0[%0] ;;" : : "r" (report_log_ptr));
#endif

	if (report_log_ptr >= (report_log + REPORT_LOG_SZ)) {
		report_log_ptr = report_log;
	}
    }

#ifdef __ST200__
    __asm__ __volatile__ ("prgadd 0[%0] ;;" : : "r" (report_log_ptr));
#endif
#else
#if defined (__KERNEL__)
#ifdef REPORT_KPTRACE
    extern void kptrace_write_record(const char *rec);
    kptrace_write_record( report_buffer );
#endif
#ifdef REPORT_PRINTK
    if (level < severity_error)
        printk( KERN_INFO"%s", report_buffer );
    else
        printk( "%s", report_buffer );
#endif
#else
    printf( "%s", report_buffer );
    fflush( stdout );
#endif
#endif
}

/* -----------------------------------------------------------
	The actual report function
----------------------------------------------------------- */

#if (!defined(__KERNEL__) || defined(CONFIG_PRINTK)) && defined(REPORT)
void report( report_severity_t   report_severity,
	     const char         *format, ... )
{
va_list      list;

    /* --- Should we report this message --- */

    if( !inrange(report_severity, severity_restriction_lower, severity_restriction_upper) &&
	(report_severity < severity_fatal ) ) return;

    /* --- Perform the report --- */

    OS_LockMutex( &report_lock );

    if( report_severity >= severity_error )
    {
	sprintf( report_buffer, "***** %s-%s: ",
		((report_severity >= severity_fatal) ? "Fatal" : "Error"),
		OS_ThreadName() );

	report_output(report_severity);
    }

    va_start(list, format);
    vsnprintf(report_buffer, REPORT_STRING_SIZE, format, list);
    va_end(list);

    report_output(report_severity);

    if( report_severity == severity_fatal ) 
    {
#ifdef __KERNEL__
        dump_stack();
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
#endif
        while( true );
    }
    OS_UnLockMutex( &report_lock );
}
#endif


#ifdef REPORT
/* -----------------------------------------------------------
	A useful reporting hex function
----------------------------------------------------------- */
void report_dump_hex( report_severity_t level,
		      unsigned char    *data,
		      int               length,
		      int               width,
		      void*             start )
{
  int n,i;
  char str[256];

  for (n=0;n<length;n+=width)
  {
    char *str2 = str;
    str2 += sprintf(str2,"0x%08x:",(unsigned int)start + n);
    for (i=0;i<((length - n) < width ? (length - n) : width);i++)
      str2 += sprintf(str2," %02x",data[n+i] & 0x0ff);
    str2 += sprintf(str2,"\n");
    report(level,str);
  }
}
#endif

// =========================================================================================
//
// Uart output stuff
//

#ifdef REPORT_UART

//

#define CPU_FREQUENCY                   133000000               // Actually stbus clock frequency

//

#define PIO_BASE(n)                     (0x18320000 + n*0x1000)

#define PIO_C0(n)                       (PIO_BASE(n) + 0x0020)
#define PIO_C1(n)                       (PIO_BASE(n) + 0x0030)
#define PIO_C2(n)                       (PIO_BASE(n) + 0x0040)

//

#define ASC_BASE(n)                     (0x18330000 + n*0x1000)

#define ASC_BAUDRATE(n)                 (ASC_BASE(n) + 0x0000)
#define ASC_TX_BUFFER(n)                (ASC_BASE(n) + 0x0004)
#define ASC_RX_BUFFER(n)                (ASC_BASE(n) + 0x0008)
#define ASC_CONTROL(n)                  (ASC_BASE(n) + 0x000c)
#define ASC_INT_ENABLE(n)               (ASC_BASE(n) + 0x0010)
#define ASC_STATUS(n)                   (ASC_BASE(n) + 0x0014)
#define ASC_GUARDTIME(n)                (ASC_BASE(n) + 0x0018)
#define ASC_TIMEOUT(n)                  (ASC_BASE(n) + 0x001c)
#define ASC_TX_RESET(n)                 (ASC_BASE(n) + 0x0020)
#define ASC_RX_RESET(n)                 (ASC_BASE(n) + 0x0024)
#define ASC_RETRIES(n)                  (ASC_BASE(n) + 0x0028)

//

#define ASC_CTL_8_BIT                   0x00000001
#define ASC_CTL_7_BIT_WITH_PARITY       0x00000003
#define ASC_CTL_9_BIT                   0x00000004
#define ASC_CTL_8_BIT_WITH_WAKEUP       0x00000005
#define ASC_CTL_8_BIT_WITH_PARITY       0x00000007

#define ASC_CTL_0_5_STOP_BITS           0x00000000
#define ASC_CTL_1_0_STOP_BITS           0x00000008
#define ASC_CTL_1_5_STOP_BITS           0x00000010
#define ASC_CTL_2_0_STOP_BITS           0x00000018

#define ASC_CTL_PARITY_EVEN             0x00000000
#define ASC_CTL_PARITY_ODD              0x00000020

#define ASC_CTL_LOOPBACK                0x00000040
#define ASC_CTL_RUN                     0x00000080
#define ASC_CTL_RX_ENABLE               0x00000100
#define ASC_CTL_SC_ENABLE               0x00000200
#define ASC_CTL_FIFO_ENABLE             0x00000400
#define ASC_CTL_CTS_ENABLE              0x00000800

#define ASC_CTL_BAUDRATE_MODE_0         0x00000000
#define ASC_CTL_BAUDRATE_MODE_1         0x00001000

//

#define ASC_INT_RBE                     0x00000001              // Receive buffer full
#define ASC_INT_TE                      0x00000002              // Transmit buffer empty
#define ASC_INT_THE                     0x00000004              // Transmit buffer half empty
#define ASC_INT_PE                      0x00000008              // Parity error
#define ASC_INT_FE                      0x00000010              // Framing error
#define ASC_INT_OE                      0x00000020              // Overrun error
#define ASC_INT_TNE                     0x00000040              // Timeout when not empty
#define ASC_INT_TOI                     0x00000080              // Timeout when receive fifo empty
#define ASC_INT_RHF                     0x00000100              // Receive fifo half full

//

#define ASC_STATUS_RBF                  0x00000001              // Receive buffer full
#define ASC_STATUS_TE                   0x00000002              // Transmit buffer empty
#define ASC_STATUS_THE                  0x00000004              // Transmit buffer half empty
#define ASC_STATUS_PE                   0x00000008              // Parity error
#define ASC_STATUS_FE                   0x00000010              // Framing error
#define ASC_STATUS_OE                   0x00000020              // Overrun error
#define ASC_STATUS_TNE                  0x00000040              // Timeout when not empty
#define ASC_STATUS_TOI                  0x00000080              // Timeout when receive fifo empty
#define ASC_STATUS_RHF                  0x00000100              // Receive fifo half full
#define ASC_STATUS_TF                   0x00000200              // Transmit fifo full
#define ASC_STATUS_NKD                  0x00000400              // Transmittion failure acknowledge by card (smartcard gubbins)

//

#define set_pio_pins()          os_register_write( PIO_C0(5), 0x00 ); \
				os_register_write( PIO_C1(5), 0x99 ); \
				os_register_write( PIO_C2(5), 0xff );

#define set_baudrate( rate )    os_register_write( ASC_BAUDRATE(1), (CPU_FREQUENCY / ( 16 * rate )) )
#define set_interrupt_mask()    os_register_write( ASC_INT_ENABLE(1), 0 )
#define reset_transmit()        os_register_write( ASC_TX_RESET(1), 0 )
#define transmit_buffer_full()  ((os_register_read(ASC_STATUS(1)) & ASC_STATUS_TF) != 0)
#define transmit_char(c)        os_register_write( ASC_TX_BUFFER(1), c )
#define set_control()           os_register_write( ASC_CONTROL(1), (ASC_CTL_8_BIT | ASC_CTL_1_0_STOP_BITS | ASC_CTL_RUN | ASC_CTL_FIFO_ENABLE | ASC_CTL_BAUDRATE_MODE_0) )

unsigned int os_register_read (unsigned int address);
void         os_register_write (unsigned int address, unsigned int value);

// ----------------------------------------------------------------------------------------

static void UartInit( void )
{
    set_pio_pins();
    set_interrupt_mask();
    set_baudrate( 38400 );
    set_control();
    reset_transmit();
}

//

static void UartOutChar( char    c )
{
    if( transmit_buffer_full() )
    {
	OS_SleepMilliSeconds( 2 );                      // 8 bytes at 38400

	if( transmit_buffer_full() )
	    reset_transmit();                   // Should have clocked some by now
    }

    transmit_char( c );
}

//

static void UartOutput( char    *String )
{
    while( *String != 0 )
    {
	UartOutChar( *String );
	if( *(String++) == '\n' )
	    UartOutChar( '\r' );
    }
}


#endif

