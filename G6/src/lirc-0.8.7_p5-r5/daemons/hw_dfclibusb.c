/*      $Id: hw_dfclibusb.c,v 5.2 2010/04/11 18:50:38 lirc Exp $      */

/****************************************************************************
 ** hw_dfclibusb.c **********************************************************
 ****************************************************************************
 *  Userspace (libusb) driver for DFC USB InfraRed Remote Control, based on 
 *  hw_atilibusb.c
 *
 *  Copyright (C) 2010 Davio Franke <davio@daviofranke.com>
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
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"

#define CODE_BYTES 6
#define USB_TIMEOUT (5000)

static int dfc_init();
static int dfc_deinit();
static char *dfc_rec(struct ir_remote *remotes);
static void usb_read_loop(int fd);
static struct usb_device *find_usb_device(void);

struct hardware hw_dfclibusb =
{
	NULL,                       /* default device */
	-1,                         /* fd */
	LIRC_CAN_REC_LIRCCODE,      /* features */
	0,                          /* send_mode */
	LIRC_MODE_LIRCCODE,         /* rec_mode */
	CODE_BYTES * CHAR_BIT,      /* code_length */
	dfc_init,                   /* init_func */
	dfc_deinit,                 /* deinit_func */
	NULL,                       /* send_func */
	dfc_rec,                    /* rec_func */
	receive_decode,             /* decode_func */
	NULL,                       /* ioctl_func */
	NULL,                       /* readdata */
	"dfclibusb"
};

typedef struct {
	u_int16_t vendor;
	u_int16_t product;
} usb_device_id;

/* table of compatible remotes -- from lirc_dfcusb */
static usb_device_id usb_remote_id_table[] = {
	{ 0x20a0, 0x410b }, /* DFC USB InfraRed Remote Control */
	{ 0x0dfc, 0x0001 }, /* DFC USB InfraRed Remote Control (for compatibility with first release only) */
	{ 0, 0 } /* Terminating entry */
};

static struct usb_dev_handle *dev_handle = NULL;
static pid_t child = -1;

/****/

/* initialize driver -- returns 1 on success, 0 on error */
static int dfc_init()
{
	struct usb_device *usb_dev;
	int pipe_fd[2] = { -1, -1 };
	
	LOGPRINTF(1, "initializing USB receiver");
	
	init_rec_buffer();
	
	usb_dev = find_usb_device();
	if (usb_dev == NULL)
	{
		logprintf(LOG_ERR, "couldn't find a compatible USB device");
		return 0;
	}

	/* A separate process will be forked to read data from the USB
	 * receiver and write it to a pipe. hw.fd is set to the readable
	 * end of this pipe. */
	if (pipe(pipe_fd) != 0)
	{
		logperror(LOG_ERR, "couldn't open pipe");
		return 0;
	}
	hw.fd = pipe_fd[0];
	
	dev_handle = usb_open(usb_dev);
	if (dev_handle == NULL)
	{
		logperror(LOG_ERR, "couldn't open USB receiver");
		goto fail;
	}
	
	child = fork();
	if (child == -1)
	{
		logperror(LOG_ERR, "couldn't fork child process");
		goto fail;
	}
	else if (child == 0)
	{
		usb_read_loop(pipe_fd[1]);
	}
	
	LOGPRINTF(1, "USB receiver initialized");
	return 1;

fail:
	if (dev_handle)
	{
		usb_close(dev_handle);
		dev_handle = NULL;
	}
	if (pipe_fd[0] >= 0) close(pipe_fd[0]);
	if (pipe_fd[1] >= 0) close(pipe_fd[1]);
	return 0;
}

/* deinitialize driver -- returns 1 on success, 0 on error */
static int dfc_deinit()
{
	int err = 0;
	
	if (dev_handle)
	{
		if (usb_close(dev_handle) < 0) err = 1;
		dev_handle = NULL;
	}
	
	if (hw.fd >= 0)
	{
		if (close(hw.fd) < 0) err = 1;
		hw.fd = -1;
	}
	
	if (child > 1)
	{
		if ( (kill(child, SIGTERM) == -1) ||
		     (waitpid(child, NULL, 0) == 0) ) err = 1;
	}
	
	return !err;
}

static char *dfc_rec(struct ir_remote *remotes)
{
	if (!clear_rec_buffer())
	{
		dfc_deinit();
		return NULL;
	}
	return decode_all(remotes);
}

/* returns 1 if the given device should be used, 0 otherwise */
static int is_device_ok(struct usb_device *dev)
{
	/* TODO: allow exact device to be specified */
	
	/* check if the device ID is in usb_remote_id_table */
	usb_device_id *dev_id;
	for (dev_id = usb_remote_id_table; dev_id->vendor; dev_id++)
	{
		if ( (dev->descriptor.idVendor == dev_id->vendor) &&
		     (dev->descriptor.idProduct == dev_id->product) )
		{
			return 1;
		}
	}
	
	return 0;
}

/* find a compatible USB receiver and return a usb_device,
 * or NULL on failure. */
static struct usb_device *find_usb_device(void)
{
	struct usb_bus *usb_bus;
	struct usb_device *dev;
	
	usb_init();
	usb_find_busses();
	usb_find_devices();
	
	for (usb_bus = usb_busses; usb_bus; usb_bus = usb_bus->next)
	{
		for (dev = usb_bus->devices; dev; dev = dev->next)
		{
			if (is_device_ok(dev)) return dev;
		}
	}
	return NULL;  /* no suitable device found */
}

/* this function is run in a forked process to read data from the USB
 * receiver and write it to the given fd. it calls exit() with result
 * code 0 on success, or 1 on failure. */
static void usb_read_loop(int fd)
{
	int err = 0;
	char rcv_code[6];
	int ptr = 0, count;
	
	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	
	for(;;)
	{
		char buf[16]; // CODE_BYTES
		int bytes_r, bytes_w, pos;
		
		/* read from the USB device */
		bytes_r = usb_control_msg(dev_handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
					  3, 0, 0, &buf[0], sizeof(buf), USB_TIMEOUT);

		if (bytes_r < 0)
		{
			if (errno == EAGAIN || errno == ETIMEDOUT) continue;
			logprintf(LOG_ERR, "can't read from USB device: %s",
				strerror(errno));
			err = 1; goto done;
		}
		
		if (bytes_r > 1)
		{
			for (count = 1; count < bytes_r; count++)
			{
				rcv_code[ptr++] = buf[count];
				if (ptr == 6)
				{
					/* write to the pipe */
					for (pos = 0; pos < ptr; pos += bytes_w)
					{
						bytes_w = write(fd, rcv_code + pos, ptr - pos);
						if (bytes_w < 0)
						{
							logprintf(LOG_ERR, "can't write to pipe: %s",
								strerror(errno));
							err = 1; goto done;
						}
					}
					
					ptr = 0;
				}
			}
		}
	}
	
done:
	close(fd);
	if (!usb_close(dev_handle)) err = 1;
	_exit(err);
}
