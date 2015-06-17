/****************************************************************************
 ** hw_atwf83.c *************************************************************
 ****************************************************************************
 *
 * Lirc driver for Aureal remote (ATWF@83-W001 ESKY.CC USB_V3B)
 *
 * Copyright (C) 2010 Romain Henriet <romain-devel@laposte.net>
 *
 * Distribute under GPL version 2 or later.
 *
 */

#include <stdio.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <signal.h>
#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"

enum
{
	RPT_NO = 0,
	RPT_YES = 1,
};

static int atwf83_init();
static int atwf83_deinit();
static char *atwf83_rec(struct ir_remote *remotes);
static int atwf83_decode(struct ir_remote *remote,
			 ir_code *prep, ir_code *codep, ir_code *postp,
			 int *repeat_flagp,
			 lirc_t *min_remaining_gapp,
			 lirc_t *max_remaining_gapp);
static void* atwf83_repeat();

const unsigned release_code =0x00000000;
/** Time to wait before first repetition */
const unsigned repeat_time1_us = 500000;
/** Time to wait between two repetitions */
const unsigned repeat_time2_us = 100000;
/** Pipe between main thread and repetition thread */
static int fd_pipe[2] = {-1,-1};
/** Thread that simulates repetitions */
static pthread_t repeat_thread;
/** File descriptor for the real device */
static int fd_hidraw;

const int main_code_length = 32;
static signed int main_code = 0;
static struct timeval start,end,last;
static int repeat_state = RPT_NO;

/* Aureal USB iR Receiver */
struct hardware hw_atwf83=
{
	"/dev/hidraw0",		/* "device" */
	-1,			/* fd (device) */
	LIRC_CAN_REC_LIRCCODE,	/* features */
	0,			/* send_mode */
	LIRC_MODE_LIRCCODE,	/* rec_mode */
	32,			/* code_length */
	atwf83_init,		/* init_func */
	atwf83_deinit,		/* deinit_func */
	NULL,			/* send_func */
	atwf83_rec,		/* rec_func */
	atwf83_decode,		/* decode_func */
	NULL,			/* ioctl_func */
	NULL,			/* readdata */
	"atwf83"		/* name */
};

int atwf83_decode(struct ir_remote *remote,
		  ir_code *prep, ir_code *codep, ir_code *postp,
		  int *repeat_flagp,
		  lirc_t *min_remaining_gapp,
		  lirc_t *max_remaining_gapp)
{
	LOGPRINTF(1, "atwf83_decode");

	if(!map_code(remote,prep,codep,postp,0,0,
		     main_code_length,main_code,0,0))
	{
		return 0;
	}

	map_gap(remote, &start, &last, 0, repeat_flagp,
		min_remaining_gapp, max_remaining_gapp);
	/* override repeat */
	*repeat_flagp = repeat_state;

	return 1;
}

int atwf83_init()
{
	logprintf(LOG_INFO, "initializing '%s'", hw.device);
	if ((fd_hidraw = open(hw.device, O_RDONLY)) < 0)
	{
		logprintf(LOG_ERR, "unable to open '%s'", hw.device);
		return 0;
	}
	/* Create pipe so that events sent by the repeat thread will
	   trigger main thread */
	if (pipe(fd_pipe) != 0)
	{
		logperror(LOG_ERR, "couldn't open pipe");
		close(fd_hidraw);
		return 0;
	}
	hw.fd = fd_pipe[0];
	/* Create thread to simulate repetitions */
	if (pthread_create(&repeat_thread, NULL, atwf83_repeat, NULL))
	{
		logprintf(LOG_ERR, "Could not create \"repeat thread\"");
		return 0;
	}
	return 1;
}

int atwf83_deinit()
{
	pthread_cancel(repeat_thread);
	if(fd_hidraw != -1)
	{
		logprintf(LOG_INFO, "closing '%s'", hw.device);
		close(fd_hidraw);
		fd_hidraw=-1;
	}
	if (fd_pipe[0] >= 0)
	{
		close(fd_pipe[0]);
		fd_pipe[0] = -1;
	}
	if (fd_pipe[1] >= 0)
	{
		close(fd_pipe[1]);
		fd_pipe[1] = -1;
	}
	hw.fd = -1;
	return 1;
}

/**
 *	Runtime that reads device, forwards codes to main thread
 *	and simulates repetitions.
 */
void* atwf83_repeat()
{
	unsigned ev[2];
	unsigned current_code;
	int rd, sel;
	fd_set files;
	struct timeval delay;
	int pressed=0;
	int fd = fd_pipe[1];

	while(1)
	{
		// Initialize set to monitor device's events
		FD_ZERO(&files);
		FD_SET(fd_hidraw, &files);
		if (pressed)
		{
			sel = select(FD_SETSIZE, &files, NULL, NULL, &delay);
		}
		else
		{
			sel = select(FD_SETSIZE, &files, NULL, NULL, NULL);
		}

		switch (sel)
		{
		case 1:
			// Data ready in device's file
			rd = read(fd_hidraw, ev, sizeof(ev));
			
			if (rd==-1)
			{
				// Error
				logprintf(LOG_ERR, "(%s) Could not read %s",
					  __FUNCTION__, hw.device);
				goto exit_loop;
			}
			if ((rd==8 && ev[0]!=0) || (rd==6 && ev[0]>2) )	
			{
				// Key code : forward it to main thread
				pressed = 1;
				delay.tv_sec = 0;
				delay.tv_usec = repeat_time1_us;
				current_code = ev[0];
			}
			else
			{
				// Release code : stop repetitions
				pressed = 0;
				current_code = release_code;
			}
			break;
		case 0:
			// Timeout : send current_code again to main
			//           thread to simulate repetition
			delay.tv_sec = 0;
			delay.tv_usec = repeat_time2_us;
			break;
		default:
			// Error
			logprintf(LOG_ERR,"(%s) select() failed",
				  __FUNCTION__);
			goto exit_loop;
		}
		// Send code to main thread through pipe
		write(fd, &current_code, sizeof(current_code));
	}
 exit_loop:
	
	fd_pipe[1] = -1;
	close(fd);
	return NULL;
}

/*
*  Aureal Technology ATWF@83 cheap remote
*  specific code.
*/

char *atwf83_rec(struct ir_remote *remotes)
{
	unsigned ev;
	int rd;
	last = end;
	gettimeofday(&start,NULL);
	rd = read(hw.fd, &ev, sizeof(ev));

	if (rd==-1)
	{
		// Error
		logprintf(LOG_ERR,"(%s) could not read pipe", __FUNCTION__);
		atwf83_deinit();
		return 0;
	}

	if (ev == 0)
	{
		// Release code
		main_code = 0;
		return 0;
	}

	LOGPRINTF(1, "atwf83 : %x", ev);
	// Record the code and check for repetition
	if (main_code == ev)
	{
		repeat_state = RPT_YES;
	}
	else
	{
		main_code = ev;
		repeat_state = RPT_NO;
	}
	gettimeofday(&end,NULL);
	return decode_all(remotes);
}
