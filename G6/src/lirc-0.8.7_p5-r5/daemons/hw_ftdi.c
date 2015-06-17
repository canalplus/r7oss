/****************************************************************************
 ** hw_ftdi.c ***************************************************************
 ****************************************************************************
 *
 * Mode2 receiver + transmitter using the bitbang mode of an FTDI
 * USB-to-serial chip such as the FT232R, with a demodulating IR receiver
 * connected to one of the FTDI chip's data pins -- by default, D1 (RXD).
 *
 * Copyright (C) 2008 Albert Huitsing <albert@huitsing.nl>
 * Copyright (C) 2008 Adam Sampson <ats@offog.org>
 *
 * Inspired by the UDP driver, which is:
 * Copyright (C) 2002 Jim Paris <jim@jtan.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"
#include "transmit.h"
#include "hw_default.h"

#include <ftdi.h>

/* PID of the child process */
static pid_t child_pid = -1;

#define RXBUFSZ		2048
#define TXBUFSZ		65536

static char *device_config = NULL;
static int tx_baud_rate = 65536;
static int rx_baud_rate = 9600;
static int input_pin = 1; 	/* RXD as input */
static int output_pin = 2;	/* RTS as output */
static int usb_vendor = 0x0403; /* default for FT232 */
static int usb_product = 0x6001;
static const char *usb_desc = NULL;
static const char *usb_serial = NULL;

static int laststate = -1;
static unsigned long rxctr = 0;
extern struct ir_remote *repeat_remote;

static int pipe_main2tx[2] = { -1, -1 };
static int pipe_tx2main[2] = { -1, -1 };

static void parsesamples(unsigned char *buf,int n,int pipe_rxir_w)
{
	int i;
	int res;
	lirc_t usecs;
	
	for (i=0; i<n; i++)
	{
		int curstate = (buf[i] & (1 << input_pin)) != 0;
		rxctr++;

		if (curstate == laststate) continue;
		
		/* Convert number of samples to us.
		 *
		 * The datasheet indicates that the sample rate in
		 * bitbang mode is 16 times the baud rate but 32 seems
		 * to be correct. */
		usecs = (rxctr * 1000000LL) / (rx_baud_rate * 32);
		
		/* Clamp */
		if (usecs > PULSE_MASK)
		{
			usecs = PULSE_MASK;
		}
		
		/* Indicate pulse or bit */
		if (curstate)
		{
			usecs |= PULSE_BIT;
		}
		
		/* Send the sample */
		res = write(pipe_rxir_w, &usecs, sizeof usecs);
		
		/* Remember last state */
		laststate = curstate;
		rxctr = 0;
	}
}

static void child_process(int fd_rx2main, int fd_main2tx, int fd_tx2main)
{
	int ret;
	struct ftdi_context ftdic;

	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	ftdi_init(&ftdic);

	/* indicate we're started: */
	ret = write(fd_tx2main,&ret,1);
	
	while (1)
	{

		/* Open the USB device */
		if (ftdi_usb_open_desc(&ftdic, usb_vendor, usb_product,
				       usb_desc, usb_serial)<0)
		{
			logprintf(LOG_ERR, "unable to open FTDI device (%s)",
				  ftdi_get_error_string(&ftdic));
			goto retry;
		}

		/* Enable bit-bang mode, setting output & input pins
		   direction */
		if (ftdi_enable_bitbang(&ftdic,1<<output_pin)<0)
		{
			logprintf(LOG_ERR,
				  "unable to enable bitbang mode (%s)",
				  ftdi_get_error_string(&ftdic));
			goto retry;
		}

		/* Set baud rate */
		if (ftdi_set_baudrate(&ftdic, rx_baud_rate)<0)
		{
			logprintf(LOG_ERR,
				  "unable to set required baud rate (%s)",
				  ftdi_get_error_string(&ftdic));
			goto retry;
		}

		logprintf(LOG_DEBUG, "opened FTDI device '%s' OK", hw.device);

		do
		{
			unsigned char buf[RXBUFSZ>TXBUFSZ ? RXBUFSZ:TXBUFSZ];

			/* transmit IR */
			ret = read(fd_main2tx, buf, sizeof buf);
			if(ret>0) 
			{ 
				/* select correct transmit baudrate */
				if (ftdi_set_baudrate(&ftdic,tx_baud_rate)<0)
				{
					logprintf(LOG_ERR,"unable to set required baud rate for transmission (%s)",ftdi_get_error_string(&ftdic));
					goto retry;
				}
				if (ftdi_write_data(&ftdic,buf,ret)<0)
				{
					logprintf(LOG_ERR,"enable to write ftdi buffer (%s)",ftdi_get_error_string(&ftdic));
				}
				if (ftdi_usb_purge_tx_buffer(&ftdic)<0)
				{
					logprintf(LOG_ERR,"unable to purge ftdi buffer (%s)",ftdi_get_error_string(&ftdic));
				}

				/* back to rx baudrate: */
				if (ftdi_set_baudrate(&ftdic,rx_baud_rate)<0)
				{
					logprintf(LOG_ERR,"unable to set restore baudrate for reception (%s)",ftdi_get_error_string(&ftdic));
					goto retry;
				}
				
				/* signal transmission ready: */
				ret = write(fd_tx2main,&ret,1);
				
				continue;
			} 

			/* receive IR */
			ret = ftdi_read_data(&ftdic, buf, RXBUFSZ);
			if (ret>0)
			{
				parsesamples(buf, ret, fd_rx2main);
			}
		} while (ret>0);

	retry:
		/* Wait a while and try again */
		usleep(500000);
	}
}

static int hwftdi_init()
{
	int flags;
	int pipe_rx2main[2] = { -1, -1 };
	int res;
	unsigned char buf[1];
	
	char *p;

	logprintf(LOG_INFO, "Initializing FTDI: %s", hw.device);

	/* Parse the device string, which has the form key=value,
	 * key=value, ...  This isn't very nice, but it's not a lot
	 * more complicated than what some of the other drivers do. */
	p = device_config = strdup(hw.device);
	while (p)
	{
		char *comma, *value;

		comma = strchr(p,',');
		if (comma != NULL)
		{
			*comma = '\0';
		}

		/* Skip empty options. */
		if (*p == '\0')
		{
			goto next;
		}

		value = strchr(p, '=');
		if (value == NULL)
		{
			logprintf(LOG_ERR, "device configuration option "
				  "must contain an '=': '%s'", p);
			goto fail_start;
		}
		*value++ = '\0';

		if (strcmp(p, "vendor") == 0)
		{
			usb_vendor = strtol(value, NULL, 0);
		}
		else if (strcmp(p, "product") == 0)
		{
			usb_product = strtol(value, NULL, 0);
		}
		else if (strcmp(p, "desc") == 0)
		{
			usb_desc = value;
		}
		else if (strcmp(p, "serial") == 0)
		{
			usb_serial = value;
		}
		else if (strcmp(p, "input") == 0)
		{
			input_pin = strtol(value, NULL, 0);
		}
		else if (strcmp(p, "baud") == 0)
		{
			rx_baud_rate = strtol(value, NULL, 0);
		}
		else if (strcmp(p, "output") == 0)
		{
			output_pin = strtol(value, NULL, 0);
		}
		else if (strcmp(p, "txbaud") == 0) 
		{
			tx_baud_rate = strtol(value, NULL, 0);
		}
		else
		{
			logprintf(LOG_ERR, "unrecognised device configuration "
				  "option: '%s'",p);
			goto fail_start;
		}

	next:
		if (comma == NULL)
		{
			break;
		}
		p = comma + 1;
	}

	init_rec_buffer();

	/* Allocate a pipe for lircd to read from */
	if (pipe(pipe_rx2main) == -1)
	{
		logprintf(LOG_ERR,"unable to create pipe_rx2main");
		goto fail_start;
	}
	if (pipe(pipe_main2tx) == -1)
	{
		logprintf(LOG_ERR,"unable to create pipe_main2tx");
		goto fail_main2tx;
	}
	if (pipe(pipe_tx2main) == -1)
	{
		logprintf(LOG_ERR,"unable to create pipe_tx2main");
		goto fail_tx2main;
	}

	hw.fd = pipe_rx2main[0];

	flags = fcntl(hw.fd,F_GETFL);
	
	/* make the read end of the pipe non-blocking: */
	if (fcntl(hw.fd,F_SETFL,flags | O_NONBLOCK)==-1)
	{
		logprintf(LOG_ERR,"unable to make pipe read end non-blocking");
		goto fail;
	}

	/* Make the read end of the send pipe non-blocking */
	flags = fcntl(pipe_main2tx[0],F_GETFL);
	if (fcntl(pipe_main2tx[0],F_SETFL,flags | O_NONBLOCK)==-1)
	{
		logprintf(LOG_ERR,"unable to make pipe read end non-blocking");
		goto fail;
	}

	/* Spawn the child process */
	child_pid = fork();
	if (child_pid == -1)
	{
		logprintf(LOG_ERR, "unable to fork child process");
		goto fail;
	}
	else if (child_pid == 0)
	{
		/* we're the child: */
		close(pipe_rx2main[0]);
		close(pipe_main2tx[1]);
		close(pipe_tx2main[0]);
		child_process(pipe_rx2main[1], pipe_main2tx[0], pipe_tx2main[1]);
	}
	
	/* we're the parent: */
	close(pipe_rx2main[1]);
	close(pipe_main2tx[0]);
	pipe_main2tx[0] = -1;
	close(pipe_tx2main[1]);
	pipe_tx2main[1] = -1;
	
	/* wait for child to be started */
	res = read(pipe_tx2main[0], &buf, 1);

	return(1);

 fail:
	hw.fd = -1;
	
	close(pipe_tx2main[0]);
	close(pipe_tx2main[1]);
	pipe_tx2main[0] = -1;
	pipe_tx2main[1] = -1;

 fail_tx2main:
	close(pipe_main2tx[0]);
	close(pipe_main2tx[1]);
	pipe_main2tx[0] = -1;
	pipe_main2tx[1] = -1;

 fail_main2tx:
	close(pipe_rx2main[0]);
	close(pipe_rx2main[1]);

 fail_start:
	if (device_config != NULL)
	{
		free(device_config);
		device_config = NULL;
	}

	return(0);
}

static int hwftdi_deinit(void)
{
	if (child_pid != -1)
	{
		/* Kill the child process, and wait for it to exit */
		if (kill(child_pid, SIGTERM) == -1)
		{
			return(0);
		}
		if (waitpid(child_pid, NULL, 0) == 0)
		{
			return(0);
		}
		child_pid = -1;
	}

	close(hw.fd);
	hw.fd = -1;
	
	close(pipe_main2tx[1]);
	pipe_main2tx[1] = -1;
	close(pipe_tx2main[0]);
	pipe_tx2main[0] = -1;

	free(device_config);
	device_config = NULL;

	return(1);
}

static char *hwftdi_rec(struct ir_remote *remotes)
{
	if (!clear_rec_buffer()) return(NULL);
	return(decode_all(remotes));
}

static lirc_t hwftdi_readdata(lirc_t timeout)
{
	int n;
	lirc_t res = 0;

	if (!waitfordata((long)timeout))
	{
		return 0;
	}

	n = read(hw.fd, &res, sizeof res);
	if (n != sizeof res)
	{
		res = 0;
	}

	return(res);
}


static int hwftdi_send(struct ir_remote *remote, struct ir_ncode *code)
{
	unsigned long f_sample	= tx_baud_rate*8;
	unsigned long f_carrier =
		remote->freq == 0 ? DEFAULT_FREQ : remote->freq;
	unsigned long div_carrier;
	int val_carrier;
	lirc_t *pulseptr;
	lirc_t pulse;
	int n_pulses;
	int pulsewidth;
	int bufidx;
	int sendpulse;
	int res;
	unsigned char buf[TXBUFSZ];

	logprintf(LOG_DEBUG, "hwftdi_send() carrier=%dHz f_sample=%dHz ",
		  f_carrier,f_sample);
	
	/* initialize decoded buffer: */
	if (!init_send(remote,code))
	{
		return 0;
	}
	
	/* init vars: */
	n_pulses = send_buffer.wptr;
	pulseptr = send_buffer.data;
	bufidx = 0;
	div_carrier = 0;
	val_carrier = 0;
	sendpulse = 0;
	pulsewidth = 0;

	while (n_pulses--)
	{
		/* take pulse from buffer */
		pulse = *pulseptr++;
		
		/* compute the pulsewidth (in # samples) */
		pulsewidth = f_sample * 
			((unsigned long) (pulse & PULSE_MASK)) / 1000000ul;
		
		/* toggle pulse / space */
		sendpulse = sendpulse ? 0:1;
		
		while (pulsewidth--)
		{
			
			/* carrier generator (generates a carrier
			   continously, will be modulated by the
			   requested signal): */
			div_carrier += f_carrier*2;
			if (div_carrier >= f_sample)
			{
				div_carrier -= f_sample;
				val_carrier = val_carrier ? 0 : 255;
			}

			/* send carrier or send space ? */
			if (sendpulse)
			{
				buf[bufidx++] = val_carrier;
			}
			else
			{
				buf[bufidx++] = 0;
			}

			/* flush txbuffer? */
			/* note: be sure to have room for last '0' */
			if (bufidx>=(TXBUFSZ-1))
			{
				logprintf(LOG_ERR, "buffer overflow "
					  "while generating IR pattern");
				return 0;
			}
		}

	}
	
	/* always end with 0 to turn off transmitter: */
	buf[bufidx++] = 0;
	
	/* let the child process transmit the pattern */
	res = write(pipe_main2tx[1], buf, bufidx);
	
	/* wait for child process to be ready with it */
	res = read(pipe_tx2main[0], buf, 1);
	
	return (1);
}

static int hwftdi_ioctl(unsigned int cmd, void *arg)
{
	int res = -1;
	
	switch (cmd)
	{
		default:
			logprintf(LOG_ERR, "unsupported ioctl - %d", cmd);
			res = -1;
			break;
	}
	
	return res;
}

struct hardware hw_ftdi=
{
	"",                 /* "device" -> used as configuration */
	-1,                 /* fd */
	
	LIRC_CAN_REC_MODE2 |
	LIRC_CAN_SEND_PULSE |
	LIRC_CAN_SET_SEND_CARRIER, /* features */
	
	LIRC_MODE_PULSE,    /* send_mode */
	LIRC_MODE_MODE2,    /* rec_mode */
	0,                  /* code_length */
	hwftdi_init,	    /* init_func */
	hwftdi_deinit,      /* deinit_func */
	hwftdi_send,	    /* send_func */
	hwftdi_rec,         /* rec_func */
	receive_decode,     /* decode_func */
	hwftdi_ioctl,       /* ioctl_func */
	hwftdi_readdata,    /* readdata */
	"ftdi"
};
