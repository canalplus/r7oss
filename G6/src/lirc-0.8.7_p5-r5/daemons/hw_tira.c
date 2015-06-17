/*   $Id: hw_tira.c,v 5.9 2010/04/11 18:50:38 lirc Exp $  */
/*****************************************************************************
 ** hw_tira.c ****************************************************************
 *****************************************************************************
 * Routines for the HomeElectronics TIRA-2 USB dongle.
 *
 * Serial protocol described at: 
 *    http://www.home-electro.com/Download/Protocol2.pdf
 *
 * Copyright (C) 2003 Gregory McLean <gregm@gxsnmp.org>
 *  modified for 
 *  IRA support, 
 *  transmit feature,
 *  receive in pulse/space mode feature 
 *  by Arnold Pakolitz <spud28@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyUSB0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>

#include "hardware.h"
#include "receive.h"
#include "transmit.h"
#include "serial.h"
#include "ir_remote.h"
#include "lircd.h"
#include "hw_tira.h"

const char failwrite[] = "failed writing to device";
const char strreconly[] = "receive";
const char strsendrec[] = "send / receive";
const char mode_sixbytes[] = "6 bytes mode";
const char mode_timing[] = "timing mode";
static unsigned char deviceflags=0;

int pipe_fd[2] = { -1, -1 };

static unsigned char device_type=0;	//'t' for tira,'i' for ira
static pid_t child_pid = -1;
static struct timeval start,end,last;
static unsigned char b[6];
static unsigned char pulse_space;	//1=pulse

static ir_code code;

#define CODE_LENGTH 64
struct hardware hw_tira = {
	LIRC_IRTTY,                      /* Default device */
	-1,                              /* fd */
	LIRC_CAN_REC_LIRCCODE |          /* Features */
	LIRC_CAN_SEND_PULSE,
	LIRC_MODE_PULSE,                 /* send_mode */
	LIRC_MODE_LIRCCODE,              /* rec_mode */
	CODE_LENGTH,                     /* code_length */
	tira_init,                       /* init_func */
	tira_deinit,                     /* deinit_func */
	tira_send,                       /* send_func */
	tira_rec,                        /* rec_func */
	tira_decode,                     /* decode_func */
	NULL,                            /* ioctl_func */
	NULL,                            /* readdata */
	"tira"
};


struct hardware hw_tira_raw = {
	LIRC_IRTTY,                      /* Default device */
	-1,                              /* fd */
	LIRC_CAN_REC_MODE2,           	 /* Features */
	0,                 		 /* send_mode */
	LIRC_MODE_MODE2,              	 /* rec_mode */
	CODE_LENGTH,                     /* code_length */
	tira_init,                       /* init_func */
	tira_deinit,                     /* deinit_func */
	NULL,                       	 /* send_func, tira cannot
					    transmit in timing mode */
	tira_rec_mode2,                  /* rec_func */
	tira_decode,                     /* decode_func */
	NULL,                            /* ioctl_func */
	tira_readdata,                   /* readdata */
	"tira_raw"
};

int tira_decode (struct ir_remote *remote, ir_code *prep, ir_code *codep,
		 ir_code *postp, int *repeat_flagp,
		 lirc_t *min_remaining_gapp,
		 lirc_t *max_remaining_gapp)
{
	if(!map_code(remote, prep, codep, postp,
		     0, 0, CODE_LENGTH, code, 0, 0))
	{
                return 0;
	}
	
	map_gap(remote, &start, &last, 0, repeat_flagp,
		min_remaining_gapp, max_remaining_gapp);
	
	return 1;
}

char response[64+1];


void displayonline(void)
{
	const char *dflags;
	const char *dmode;
	
	dflags=strreconly;
	if (deviceflags & 1) dflags=strsendrec;
	if (hw.rec_mode == LIRC_MODE_LIRCCODE)
		dmode = mode_sixbytes;
	else
		dmode = mode_timing;
	logprintf(LOG_INFO, "device online, ready to %s remote codes(%s)",
		  dflags, dmode);
}

int tira_setup_sixbytes(void)
{
	int i;
	
	logprintf(LOG_INFO, "Switching to 6bytes mode");
	if (write(hw.fd, "IR", 2 ) != 2)
	{
		logprintf(LOG_ERR, "failed switching device "
				   "into six byte mode");
		return 0;
	}
	/* wait for the chars to be written */
	usleep (2 * (100 * 1000));
		
	i = read (hw.fd, response, 2);
	if (i != 2)
	{
		 logprintf(LOG_ERR, "failed reading response "
		   		    "to six byte mode command");
		 return 0;
	}
	if (strncmp(response, "OK", 2) != 0) return 0;
	displayonline();
	return 1;
}

//This process reads data from the device and forwards to the hw pipe
//as PULSE/SPACE data
int child_process(int pipe_w,int oldprotocol)
{
 	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	unsigned char tirabuffer[64];
	int tirabuflen = 0;
	int readsize,tmp;
	lirc_t data,tdata;
	fd_set read_set;
	struct timeval tv,trailtime,currtime;
	unsigned long eusec;
	tv.tv_sec = 0; tv.tv_usec=1000;
	FD_ZERO(&read_set);

	trailtime.tv_sec = 0;
	trailtime.tv_usec = 0;

	while (1)
	{
		FD_SET(hw.fd, &read_set);
		tmp = select(hw.fd+1, &read_set, NULL, NULL, &tv); 

		if (tmp == 0) continue;
		if (tmp < 0)
		{      
			logprintf(LOG_ERR,"Error select()");
			return 0;
		}
		
		if (!FD_ISSET(hw.fd, &read_set)) continue;
		readsize = read(hw.fd, &tirabuffer[tirabuflen],
				sizeof(tirabuffer)-tirabuflen);
		if (readsize <= 0)
		{      
			logprintf(LOG_ERR, "Error reading from Tira");
			logperror(LOG_ERR, NULL);
			return 0;
		}
		if (readsize == 0) continue;
		tirabuflen += readsize;

		tmp = 0;  
		while (tmp < tirabuflen-1)  
		{
			data = tirabuffer[tmp];data<<=8;data += tirabuffer[tmp+1];
			if (!oldprotocol) data <<= 3;else data<<=5;
			if (data == 0) 
			{
				if (tmp > tirabuflen-4) break;	//we have to receive more data
				if (tirabuffer[tmp+3] != 0xB2) 
				{      
					logprintf(LOG_ERR,"Tira error 00 00 xx B2 trailing : missing 0xB2");
					return 0;
				}
				if ((trailtime.tv_sec == 0) && (trailtime.tv_usec == 0)) gettimeofday (&trailtime, NULL);
				if (tmp > tirabuflen-6) break;	//we have to receive more data
				tmp += 4;
				continue;
			}
			tmp += 2;

			if ((trailtime.tv_sec != 0) || (trailtime.tv_usec != 0))
			{
				gettimeofday (&currtime, NULL);
				if (trailtime.tv_usec > currtime.tv_usec) {
					currtime.tv_usec += 1000000;
					currtime.tv_sec--;
				}

				eusec = time_elapsed(&trailtime, &currtime);
				if(eusec > LIRC_VALUE_MASK)
					eusec = LIRC_VALUE_MASK;

				trailtime.tv_sec = 0;
				trailtime.tv_usec = 0;
				if (eusec > data)
				{
					pulse_space = 1;
					tdata = LIRC_SPACE(eusec);
					if (write(pipe_w, &tdata, sizeof(tdata)) != sizeof(tdata))
					{
						logprintf(LOG_ERR,"Error writing pipe");
						return 0;
					}
				    
				}  
			}  

			data = pulse_space ? LIRC_PULSE(data):LIRC_SPACE(data);
			
			pulse_space = 1-pulse_space;
			
			if (write(pipe_w, &data, sizeof(data)) != sizeof(data))
			{
				logprintf(LOG_ERR,"Error writing pipe");
				return 0;
			}
		}

		//Scroll buffer to next position
		if (tmp > 0)
		{
			memmove(tirabuffer, tirabuffer+tmp, tirabuflen-tmp);
			tirabuflen -= tmp;
		}	
 	}
}


int tira_setup_timing(int oldprotocol)
{
	long fd_flags;
	int i;

	if (oldprotocol)
	      if (!tty_setbaud(hw.fd, 57600)) return 0;
	  
	logprintf(LOG_INFO, "Switching to timing mode");
	if (!oldprotocol)
	{  
		if (write(hw.fd, "IC\0\0", 4 ) != 4)
		{
			logprintf(LOG_ERR, "failed switching device into timing mode");
			return 0;
		}
		/* wait for the chars to be written */
		usleep (2 * (100 * 1000));
		
		i = read (hw.fd, response, 3);
		if (i != 3)
		{
			  logprintf(LOG_ERR, "failed reading response to timing mode command");
			  return 0;
		}
		if (strncmp(response, "OIC", 3) != 0) return 0;
	}
	pulse_space=1;	//pulse
	/* Allocate a pipe for lircd to read from */
	if (pipe(pipe_fd) == -1)
	{
		logperror(LOG_ERR, "unable to create pipe");
		goto fail;
	}


	fd_flags = fcntl(pipe_fd[0], F_GETFL);
  	if(fcntl(pipe_fd[0], F_SETFL, fd_flags | O_NONBLOCK) == -1)
	{
		logperror(LOG_ERR, "can't set pipe to non-blocking");
		goto fail;
	}	
	
	/* Spawn the child process */
	
	child_pid = fork();
	if (child_pid == -1)
	{
		logperror(LOG_ERR, "unable to fork child process");
		goto fail;
	}
	else if (child_pid == 0)
	{
		close(pipe_fd[0]);
		child_process(pipe_fd[1], oldprotocol);
		close(pipe_fd[1]);
		_exit(0);
	}
	else
	{
		/* parent reads from pipe */
		close(hw.fd);
		hw.fd = pipe_fd[0];
	}
	close(pipe_fd[1]);
	displayonline();
	return(1);

fail:
	if (pipe_fd[0] != -1)
	{
		close(pipe_fd[0]);
		close(pipe_fd[1]);
	}
	return(0);

}

int tira_setup(void)
{
	int  i;
	int ptr;
	/* Clear the port of any random data */
	while (read(hw.fd, &ptr, 1) >= 0) ;
	
	/* Start off with the IP command. This was initially used to
	   switch to timing mode on the Tira-1. The Tira-2 also
	   supports this mode, however it does not switch the Tira-2
	   into timing mode.
	*/
	if (write (hw.fd, "IP", 2) != 2)
	{
		logprintf(LOG_ERR, failwrite);
		return 0;
	}
	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	*/
	usleep (2 * (100 * 1000));
	i = read (hw.fd, response, 3);
	
	if (strncmp(response, "OIP", 3) == 0)
	{
		read (hw.fd, &ptr, 1);   /* read the calibration value */
		read (hw.fd, &ptr, 1);   /* read the version word */
		/* Bits 4:7 in the version word set to one indicates a
		   Tira-2 */
		deviceflags = ptr & 0x0f;
		if (ptr & 0xF0)
		{
			logprintf(LOG_INFO, "Tira-2 detected");
			/* Lets get the firmware version */
			write (hw.fd, "IV", 2);
			usleep (2 * (100 * 1000));
			memset (response, 0, sizeof(response));
			i = read (hw.fd, response, sizeof(response)-1);
			logprintf(LOG_INFO, "firmware version %s", response);
		}
		else
		{
			logprintf(LOG_INFO, "Ira/Tira-1 detected");
		}
	
		/* According to the docs we can do some bit work here
		   and figure out what the device supports from the
		   version word retrived.
	 
		   At this point we have a Device of some sort. Lets
		   kick it into "Six bytes or Timing" mode.
		*/
		if (hw.rec_mode == LIRC_MODE_LIRCCODE) return(tira_setup_sixbytes());
		if (hw.rec_mode == LIRC_MODE_MODE2) return(tira_setup_timing(0));

		return 0;	//unknown recmode
	}
	logprintf(LOG_ERR, "unexpected response from device");
	return 0;
}

int ira_setup_sixbytes(unsigned char info)
{
	int i;
	if (info != 0) logprintf(LOG_INFO, "Switching to 6bytes mode");
	if (write (hw.fd, "I", 1) != 1)
	{
		logprintf(LOG_ERR, failwrite);
		return 0;
	}
	usleep(200000);
	if (write (hw.fd, "R", 1) != 1)
	{
		logprintf(LOG_ERR, failwrite);
		return 0;
	}
	
	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	*/
	usleep(100000);
	i = read (hw.fd, response, 2);if (i != 2) return 0;
	if (strncmp(response, "OK", 2) != 0) return 0;
	if (info != 0) displayonline();
	return 1;
}

int ira_setup(void)
{
	int  i;
	int ptr;
	/* Clear the port of any random data */
	while (read(hw.fd, &ptr, 1) >= 0) ;
	
	if (ira_setup_sixbytes(0) == 0) return 0;
	
	if (write (hw.fd, "I", 1) != 1)
	{
		logprintf(LOG_ERR, failwrite);
		return 0;
	}
	usleep(200000);
	if (write (hw.fd, "P", 1) != 1)
	{
		logprintf(LOG_ERR, failwrite);
		return 0;
	}
	
	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	*/
	if (!tty_setbaud(hw.fd, 57600)) return 0;
	usleep(50000);
	i = read (hw.fd, response, 5);

	if (!tty_setbaud(hw.fd, 9600)) return 0;

	if (i < 5) return 0;
	if (strncmp(response, "OIP", 3) == 0)
	{
		deviceflags = response[4] & 0x0f;
		if (response[4] & 0xF0)
		{
			/* Lets get the firmware version */
			if (write (hw.fd, "I", 1) != 1)
			{
				logprintf(LOG_ERR, failwrite);
				return 0;
			}
			usleep(200000);
			if (write (hw.fd, "V", 1) != 1)
			{
				logprintf(LOG_ERR, failwrite);
				return 0;
			}

			usleep(200000);
			memset (response, 0, sizeof(response));
			i = read (hw.fd, response, sizeof(response)-1);
			logprintf(LOG_INFO, "Ira %s detected",response);
		}
		else
		{
			logprintf(LOG_INFO, "Ira-1 detected");
		}
	  
		if (hw.rec_mode == LIRC_MODE_LIRCCODE) return(ira_setup_sixbytes(1));	//switch back to 6bytes mode
		if (hw.rec_mode == LIRC_MODE_MODE2) return(tira_setup_timing(1));
		return 0;	//unknown recmode
	}
	logprintf(LOG_ERR, "unexpected response from device");
	return 0;
}

int check_tira(void)
{
	logprintf(LOG_ERR, "Searching for Tira");
	if(!tty_reset(hw.fd) ||
	   !tty_setbaud(hw.fd, 9600) ||
	   !tty_setrtscts(hw.fd, 1)) return 0;

	usleep (50000);
	
	return tira_setup();
}

int check_ira(void)
{
	logprintf(LOG_ERR, "Searching for Ira");
	if(!tty_reset(hw.fd) ||
	   !tty_setbaud(hw.fd, 9600) ||
	   !tty_setrtscts(hw.fd, 0) ||
	   !tty_setdtr(hw.fd, 1)) return 0;

	usleep (50000);
	
	return ira_setup();
}

int tira_init(void)
{
	if (child_pid != -1) tira_deinit();

	LOGPRINTF (1, "Tira init");
	
	if(!tty_create_lock(hw.device))
	{
		logprintf(LOG_ERR,"could not create lock files");
		return 0;
	}
	if ( (hw.fd = open (hw.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0)
	{
		tty_delete_lock ();
		logprintf (LOG_ERR, "Could not open the '%s' device",
			   hw.device);
		return 0;
	}
	LOGPRINTF(1, "device '%s' opened", hw.device);

	/* We want 9600 8N1 with CTS/RTS handshaking, lets set that
	 * up. The specs state a baud rate of 100000, looking at the
	 * ftdi_sio driver it forces the issue so we can set to what
	 * we would like. And seeing as this is mapped to 9600 under
	 * windows should be a safe bet.
	 */

	/* Determine device type */
	device_type=0;
	if (check_tira()) device_type='t';else
	if (check_ira()) device_type='i';

#ifdef DEBUG
	const char *device_string;
	
	switch(device_type)
	{
	case 't':
		device_string = "Tira";
		break;
	case 'i':
		device_string = "Ira";
		break;
	default:
		device_string = "unknown";
	}
	LOGPRINTF(1, "device type %s", device_string);
#endif

	if (device_type == 0)
	{
		tira_deinit();
		return 0;
	}  

	return 1;
}

int tira_deinit (void)
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

	if(hw.fd != -1)
	{
		close(hw.fd); /* pipe_fd[0] or actual device */
		hw.fd = -1;
	}
	sleep(1);
	tty_delete_lock();
	return 1;
}

char *tira_rec_mode2 (struct ir_remote *remotes)
{
	if(!clear_rec_buffer()) return(NULL);
	return(decode_all(remotes));
}

char *tira_rec (struct ir_remote *remotes)
{
	char        *m;
	int         i, x;

	last = end;
	x = 0;
	gettimeofday (&start, NULL);
	for (i = 0 ; i < 6; i++)
	{
		if (i > 0)
		{
			if (!waitfordata(20000))
			{
				LOGPRINTF(0,"timeout reading byte %d",i);
				/* likely to be !=6 bytes, so flush. */
				tcflush(hw.fd, TCIFLUSH);
				return NULL;
			}
		}
		if (read(hw.fd, &b[i], 1) != 1)
		{
			logprintf(LOG_ERR, "reading of byte %d failed.", i);
			logperror(LOG_ERR,NULL);
			return NULL;
		}
		LOGPRINTF(1, "byte %d: %02x", i, b[i]);
		x++;
	}
	gettimeofday(&end,NULL);
	code = 0;
	for ( i = 0 ; i < x ; i++ )
	{
		code |= ((ir_code) b[i]);
		code =  code << 8;
	}

	LOGPRINTF(1," -> %0llx",(unsigned long long) code);

	m = decode_all(remotes);
	return m;
}

static int tira_send (struct ir_remote *remote, struct ir_ncode *code)
{
	int retval = 0;
	unsigned int freq;
	unsigned char *sendtable;

	if ((deviceflags & 1) == 0) 
	{  
		logprintf(LOG_ERR, "this device cannot send ir signals!");
		return(0);
	}

	if (hw.rec_mode != LIRC_MODE_LIRCCODE) 
	{  
		logprintf(LOG_ERR, "can't send ir signals in timing mode!");
		return(0);
	}
	/* set the carrier frequency if necessary */
	freq = remote->freq;
	if(freq == 0) freq = DEFAULT_FREQ;
	logprintf(LOG_INFO, "modulation freq %d Hz",freq);
	freq=2000000/freq;	/* this will be the clock word */
	if (freq > 255) freq = 255;
    	
	if (!init_send(remote, code)) return(0);
	
	int length,i,s;
	lirc_t *signals;
	char idx;
	int tmp;

	length = send_buffer.wptr;
	signals = send_buffer.data;

	sendtable = malloc(length);
	if (sendtable == NULL) return retval;
	memset(sendtable,-1,length);

	/* Create burst space array for tira */
	int bsa[12];
	memset(&bsa,0,sizeof(bsa));

	for (i = 0; i < length; i++)
	{
		idx = -1;
		tmp = signals[i] / 8;
		/* Find signal length in table */
		for (s = 0; s < 12; s++) 
			if (bsa[s] == tmp) 
			{
				idx = s;
				break;
			}

		if (idx==-1) 
			for (s = 0; s < 12; s++) 
				if ((tmp < bsa[s]+(freq/16)) &&
				    (tmp > bsa[s]-(freq/16))) 
				{
					idx = s; 
					break;
				} 
		       
		  
		if (idx == -1) 
		{
			/* Add a new entry into bsa table */
			for (s = 0; s < 12; s++)
			{
				if (bsa[s] == 0)
				{
					bsa[s] = tmp;
					idx = s;
					break;
				}
			}
		} 
		      
		if (idx == -1)
		{
			logprintf(LOG_ERR, "can't send ir signal with "
				  "more than 12 different timings");
			return retval;  
		}	

		sendtable[i] = idx;
	}


	tmp=0;
	for (i = 0; i < length; i+=2)
	{
		s = sendtable[i] * 16;
		if (i < length-1) s += sendtable[i+1]; else s+=15; 
		sendtable[tmp] = s;
		tmp++;
	}  

	unsigned char *wrtbuf;
	wrtbuf = malloc(length+28);
	if (wrtbuf == NULL) return retval;

	wrtbuf[0]='I';
	wrtbuf[1]='X';
	wrtbuf[2]=freq;
	wrtbuf[3]=0;	/* reserved */
	for (i=0; i<12; i++)
	{
		wrtbuf[4+i*2] = (bsa[i] & 0xFFFF) >> 8;
		wrtbuf[5+i*2] = bsa[i] & 0xFF;
	}
		     
	for (i = 0; i < tmp; i++) wrtbuf[28+i]=sendtable[i];
	length = 28+tmp;

	if (device_type == 'i') 
	{
		i = length;
		if (write (hw.fd, wrtbuf, 1) != 1) i = 0;
		if (i != 0)
		{
			usleep(200000);
			if (write (hw.fd, &wrtbuf[1], length-1) != length-1)
			{
				i = 0;  
			}
		} 
	} else i = write (hw.fd, wrtbuf, length);
		
	if (i != length) logprintf(LOG_ERR, failwrite); else 
	{
		usleep(200000);
		i = read (hw.fd, wrtbuf, 3);
		if (strncmp((char *) wrtbuf, "OIX", 3) == 0) retval = 1;
		else logprintf(LOG_ERR, "no response from device");
	}
			
	free(wrtbuf);
	free(sendtable);

	return retval;
}

lirc_t tira_readdata(lirc_t timeout)
{
	lirc_t data = 0;
	int ret;

	if (!waitfordata((long) timeout))
		return 0;

	ret=read(hw.fd,&data,sizeof(data));
	if(ret!=sizeof(data))
	{
		logprintf(LOG_ERR, "error reading from %s", hw.device);
		logperror(LOG_ERR, NULL);
		tira_deinit();
		return 0;
	}
	return data;
}
