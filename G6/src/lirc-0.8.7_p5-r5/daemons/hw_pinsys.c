
/****************************************************************************
 ** hw_pinsys.c *************************************************************
 ****************************************************************************
 *
 * adapted routines for Pinnacle Systems PCTV (pro) receiver
 * 
 * Original routines from hw_pixelview.c :
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 * Adapted by Bart Alewijnse (scarfboy@yahoo.com)
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyS0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "hardware.h"
#include "serial.h"
#include "ir_remote.h"
#include "lircd.h"

#include "hw_pinsys.h"
#include <termios.h>

extern struct ir_remote *repeat_remote,*last_remote;

/* Technically, the code is three bytes long, however, only five bits
   in the last byte are needed to identify a button. If you don't
   define the following, the ir_cide code will only be the last
   byte. I don't know why I left it in.. well, who knows.

#define PINSYS_THREEBYTE

*/

#define PINSYS_THREEBYTE

#ifdef PINSYS_THREEBYTE
#define BITS_COUNT 24
#else
#define BITS_COUNT 8
#endif

#define REPEAT_FLAG ((ir_code) 0x000040)
#define REPEAT_MASK ((ir_code) 0x00e840)

static unsigned char b[3];
static struct timeval start,end,last;
static lirc_t signal_length;
static ir_code code;

struct hardware hw_pinsys=
{
	LIRC_IRTTY,               /* default device */
	-1,                       /* fd */
	LIRC_CAN_REC_LIRCCODE,    /* features */
	0,                        /* send_mode */
	LIRC_MODE_LIRCCODE,       /* rec_mode */
	/* remember to change signal_length if you correct this one */
	BITS_COUNT,                       /* code_length */
	pinsys_init,              /* init_func */
	pinsys_deinit,            /* deinit_func */
	NULL,                     /* send_func */
	pinsys_rec,               /* rec_func */
	pinsys_decode,            /* decode_func */
	NULL,                     /* ioctl_func */
	NULL,                     /* readdata */
	"pinsys"
};

/**** start of autodetect code ***************************/
int is_it_is_it_huh(int port)
{
	int j;
	
	tty_clear(port,1,0);
	
	ioctl(port,TIOCMGET, &j);
	if((j&TIOCM_CTS) || (j&TIOCM_DSR))
	{
		return 0;
	}
  
	tty_set(port,1,0);
	ioctl(port,TIOCMGET, &j);
	if((!(j&TIOCM_CTS)) || (j&TIOCM_DSR))
	{
		return 0;
	}
	return 1;
}

/* returns 0-3, the port, or -1 if it can't find the device */
int autodetect(void)
{
	int port,i;
	long backup;
	char device[20];

	/* hardcoded the device names.. it's easy enough to change
	   that, but it's unlikely to be on something else. */

	for(i=0;i<4;i++)
	{
		sprintf(device,"/dev/ttyS%d",i);
		
		if(!tty_create_lock(device))
		{
			continue;
		}
		port=open("/dev/ttyS0", O_RDONLY | O_NOCTTY);
		if(port < 0 )
		{
			logprintf(LOG_WARNING,"couldn't open %s",device);
			tty_delete_lock();
			continue;
		}
		else
		{
			ioctl(port,TIOCMGET, &backup);
			
			if(is_it_is_it_huh(port))
			{
				ioctl(port,TIOCMSET, &backup);
				close(port);
				tty_delete_lock();
				return i;
			}
			ioctl(port,TIOCMSET, &backup);
			close(port);
		}
		tty_delete_lock();
	}
	return -1;
}
/************** end of autodetect code *************/


int pinsys_decode(struct ir_remote *remote,
		  ir_code *prep,ir_code *codep,ir_code *postp,
		  int *repeat_flagp,
		  lirc_t *min_remaining_gapp,
		  lirc_t *max_remaining_gapp)
{
	if(!map_code(remote,prep,codep,postp,
		     0,0,
		     BITS_COUNT, code&REPEAT_FLAG ? code^REPEAT_MASK : code,
		     0,0))
	{
		return(0);
	}
	
	map_gap(remote, &start, &last, signal_length, repeat_flagp,
		min_remaining_gapp, max_remaining_gapp);
	
	if(start.tv_sec-last.tv_sec < 2)
	{
		/* let's believe the remote */
		if(code&REPEAT_FLAG)
		{
			*repeat_flagp=1;
			
			LOGPRINTF(1,"repeat_flag: %d\n",*repeat_flagp);
		}
	}
	
	return(1);
}

int pinsys_init(void)
{
	signal_length=(hw.code_length+(hw.code_length/ BITS_COUNT )*2)*1000000/1200;

	if(!tty_create_lock(hw.device))
	{
		logprintf(LOG_ERR,"could not create lock files");
		return(0);
	}
	if((hw.fd=open(hw.device,O_RDWR|O_NONBLOCK|O_NOCTTY))<0)
	{
		int detected;
		/* last character gets overwritten */
		char auto_lirc_device[]="/dev/ttyS_";
		
		tty_delete_lock();
		logprintf(LOG_WARNING,"could not open %s, "
			  "autodetecting on /dev/ttyS[0-3]",hw.device);
		logperror(LOG_WARNING,"pinsys_init()");
		/* it can also mean you compiled serial support as a
		   module and it isn't inserted, but that's unlikely
		   unless you're me. */
		
		detected=autodetect();
		
		if (detected==-1)
		{
			logprintf(LOG_ERR,"no device found on /dev/ttyS[0-3]");
			tty_delete_lock();
			return(0);
		}
		else /* detected */
		{
			auto_lirc_device[9]='0'+detected;
			hw.device=auto_lirc_device;
			if((hw.fd=open(hw.device,
				       O_RDWR|O_NONBLOCK|O_NOCTTY))<0)
			{
				/* unlikely, but hey. */
				logprintf(LOG_ERR,"couldn't open "
					  "autodetected device \"%s\"",
					  hw.device);
				logperror(LOG_ERR,"pinsys_init()");
				tty_delete_lock();
				return(0);
			}
		}
	}
	if(!tty_reset(hw.fd))
	{
		logprintf(LOG_ERR,"could not reset tty");
		pinsys_deinit();
		return(0);
	}
	if(!tty_setbaud(hw.fd,1200))
	{
		logprintf(LOG_ERR,"could not set baud rate");
		pinsys_deinit();
		return(0);
	}
	/* set RTS, clear DTR */
	if(!tty_set(hw.fd,1,0) || !tty_clear(hw.fd,0,1))
	{ 
		logprintf(LOG_ERR,"could not set modem lines (DTR/RTS)");
		pinsys_deinit();
		return(0);
	}

	/* I dunno, but when lircd starts may log `reading of byte 1
	   failed' I know that happened when testing, it's a zero
	   byte. Problem is, flushing doesn't fix it. It's not fatal,
	   it's an indication that it gets fixed.  still... */

	if (tcflush(hw.fd, TCIFLUSH)<0)
	{
		logprintf(LOG_ERR,"could not flush input buffer");
		pinsys_deinit();
		return(0);
	}
	return(1);
}

int pinsys_deinit(void)
{
	close(hw.fd);
	tty_delete_lock();
	return(1);
}

/* The first byte is always 0xFE, the second one, is a kind of checksum
   and the third one is the code itself (6 bits). The 7th bit (0x40) is the 
   repeat flag.
*/

#if 0
static char pinsys_codes[8] = {0xD1,0x73,0xE6,0x1D,0x3A,0x74,0xE8,0x00};

static int pinsys_check_code(char key, char crc)
{
    int b;

    for (b=0; b<8; b++)
    {
        if (key & (1<<b))
            crc ^= pinsys_codes[b];
    }
    return crc==0;
}
#endif

char *pinsys_rec(struct ir_remote *remotes)
{
	char *m;
	int i;
	
	last=end;
	gettimeofday(&start,NULL);
	
	for(i=0;i<3;i++)
	{
		if (i>0)
		{
			if(!waitfordata(20000))
			{
				LOGPRINTF(0,"timeout reading byte %d",i);
				/* likely to be !=3 bytes, so flush. */
				tcflush(hw.fd, TCIFLUSH);
				return(NULL);
			}
		}
		
		if(read(hw.fd,&b[i],1)!=1)
		{
			logprintf(LOG_ERR,"reading of byte %d failed",i);
			logperror(LOG_ERR,NULL);
			return(NULL);
		}
		LOGPRINTF(1,"byte %d: %02x",i,b[i]);
	}
	gettimeofday(&end,NULL);

#ifdef PINSYS_THREEBYTE
	code = (b[2]) | (b[1]<<8) | (b[0]<<16);
#else
	code = b[2];
#endif

	LOGPRINTF(1," -> %016lx",(unsigned long) code);
	m=decode_all(remotes);
	return(m);
}
