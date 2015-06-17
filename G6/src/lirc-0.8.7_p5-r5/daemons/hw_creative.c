/*      $Id: hw_creative.c,v 5.12 2010/04/11 18:50:38 lirc Exp $      */

/****************************************************************************
 ** hw_creative.c ***********************************************************
 ****************************************************************************
 *
 * routines for Creative receiver
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
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
#include "hw_creative.h"

#define NUMBYTES 6 
#define TIMEOUT 20000

extern struct ir_remote *repeat_remote,*last_remote;

unsigned char b[NUMBYTES];
struct timeval start,end,last;
lirc_t gap,signal_length;
ir_code pre,code;

struct hardware hw_creative=
{
	LIRC_IRTTY,               /* default device */
	-1,                       /* fd */
	LIRC_CAN_REC_LIRCCODE,    /* features */
	0,                        /* send_mode */
	LIRC_MODE_LIRCCODE,       /* rec_mode */
	32,                       /* code_length */
	creative_init,            /* init_func */
	creative_deinit,          /* deinit_func */
	NULL,                     /* send_func */
	creative_rec,             /* rec_func */
	creative_decode,          /* decode_func */
	NULL,                     /* ioctl_func */
	NULL,                     /* readdata */
	"creative"
};

int creative_decode(struct ir_remote *remote,
		    ir_code *prep,ir_code *codep,ir_code *postp,
		    int *repeat_flagp,
		    lirc_t *min_remaining_gapp,
		    lirc_t *max_remaining_gapp)
{
	if(!map_code(remote,prep,codep,postp,
		     16,pre,16,code,0,0))
	{
		return(0);
	}
	
	map_gap(remote, &start, &last, signal_length, repeat_flagp,
		min_remaining_gapp, max_remaining_gapp);

	return(1);
}

int creative_init(void)
{
	signal_length=108000;
	
	if(!tty_create_lock(hw.device))
	{
		logprintf(LOG_ERR,"could not create lock files");
		return(0);
	}
	if((hw.fd=open(hw.device,O_RDWR|O_NONBLOCK|O_NOCTTY))<0)
	{
		logprintf(LOG_ERR,"could not open %s",hw.device);
		logperror(LOG_ERR,"creative_init()");
		tty_delete_lock();
		return(0);
	}
	if(!tty_reset(hw.fd))
	{
		logprintf(LOG_ERR,"could not reset tty");
		creative_deinit();
		return(0);
	}
	if(!tty_setbaud(hw.fd,2400))
	{
		logprintf(LOG_ERR,"could not set baud rate");
		creative_deinit();
		return(0);
	}
	return(1);
}

int creative_deinit(void)
{
	close(hw.fd);
	tty_delete_lock();
	return(1);
}

char *creative_rec(struct ir_remote *remotes)
{
	char *m;
	int i;
	
	b[0]=0x4d;
	b[1]=0x05;
	b[4]=0xac;
	b[5]=0x21;

	last=end;
	gettimeofday(&start,NULL);
	for(i=0;i<NUMBYTES;i++)
	{
		if(i>0)
		{
			if(!waitfordata(TIMEOUT))
			{
				logprintf(LOG_ERR,"timeout reading byte %d",i);
				return(NULL);
			}
		}
		if(read(hw.fd,&b[i],1)!=1)
		{
			logprintf(LOG_ERR,"reading of byte %d failed",i);
			logperror(LOG_ERR,NULL);
			return(NULL);
		}
		if(b[0]!=0x4d ||
		   b[1]!=0x05 /* || b[4]!=0xac || b[5]!=0x21 */)
		{
			logprintf(LOG_ERR,"bad envelope");
			return(NULL);
		}
		if(i==5)
		{
			if(b[2]!=((~b[3])&0xff))
			{
				logprintf(LOG_ERR,"bad checksum");
				return(NULL);
			}
		}
		LOGPRINTF(1,"byte %d: %02x",i,b[i]);
	}
	gettimeofday(&end,NULL);

	/* pre=0x8435; */
	pre=reverse((((ir_code) b[4])<<8) | ((ir_code) b[5]),16);
	code = reverse((((ir_code) b[2]) << 8) | ((ir_code) b[3]), 16);
	
	m=decode_all(remotes);
	return(m);
}

