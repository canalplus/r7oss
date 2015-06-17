/*      $Id: hw_i2cuser.c,v 5.3 2010/04/11 18:50:38 lirc Exp $      */

/*
 * Remote control driver for I2C-attached devices from userspace
 *
 * Copyright 2006, 2007 Adam Sampson <ats@offog.org>
 *
 * This does the same job as the lirc_i2c kernel driver and hw_default, but
 * entirely in userspace -- which avoids the need to build the kernel module,
 * and should make this easier to port to other OSs in the future.
 *
 * At the moment, this only supports plain Hauppauge cards, since that's what
 * I've got. To add support for more types of devices, look at lirc_i2c.
 *
 * Based on:
 * Remote control driver for the Creative iNFRA CDrom
 *   by Leonid Froenchenko <lfroen@il.marvell.com>
 * i2c IR lirc plugin for Hauppauge and Pixelview cards
 *   Copyright (c) 2000 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 * Userspace (libusb) driver for ATI/NVidia/X10 RF Remote.
 *   Copyright (C) 2004 Michael Gold <mgold@scs.carleton.ca>
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
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <linux/i2c-dev.h>
#ifndef I2C_SLAVE /* hack */
#include <linux/i2c.h>
#endif

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"

/* The number of bits and bytes in a code. */
#define CODE_SIZE_BITS 13
#define CODE_SIZE 2

/* The I2C address of the device to use. */
#define IR_ADDR 0x1a

/* The time that a key must be held down before it repeats (in seconds). */
#define REPEAT_TIME 0.3

static int i2cuser_init(void);
static int i2cuser_deinit(void);
static void i2cuser_read_loop(int fd);
static char *i2cuser_rec(struct ir_remote *remotes);

struct hardware hw_i2cuser = {
	NULL,                   /* determine device by probing */
	-1,                     /* fd */
	LIRC_CAN_REC_LIRCCODE,  /* features */
	0,                      /* send_mode */
	LIRC_MODE_LIRCCODE,     /* rec_mode */
	CODE_SIZE_BITS,         /* code_length */
	i2cuser_init,           /* init_func */
	i2cuser_deinit,         /* deinit_func */
	NULL,                   /* send_func */
	i2cuser_rec,            /* rec_func */
	receive_decode,         /* decode_func */
	NULL,                   /* ioctl_func */
	NULL,
	"i2cuser"
};

/* FD of the i2c device. Since it's not selectable, we give the lircd core a
   pipe and poll it ourself in a separate process. */
static int i2c_fd = -1;
/* The device name. */
char device_name[256];
/* PID of the child process. */
static pid_t child = -1;

/* Hunt for the appropriate i2c device and open it. */
static int open_i2c_device(void) {
	const char *adapter_dir = "/sys/class/i2c-adapter";
	DIR *dir;
	int found;

	dir = opendir(adapter_dir);
	if (dir == NULL) {
		logprintf(LOG_ERR, "Cannot list i2c-adapter dir %s",
		          adapter_dir);
		return -1;
	}
	found = -1;

	while (1) {
		char s[256];
		FILE *f;
		struct dirent *de;

		de = readdir(dir);
		if (de == NULL)
			break;
		if (de->d_name[0] == '.')
			continue;

		/* Kernels 2.6.22 and later had the name here: */
		snprintf(s, sizeof s, "%s/%s/name",
			 adapter_dir, de->d_name);
		
		f = fopen(s, "r");
		if (f == NULL) {
			/* ... and kernels prior to 2.6.22 have it here: */
			snprintf(s, sizeof s, "%s/%s/device/name",
				 adapter_dir, de->d_name);

			f = fopen(s, "r");
		}
		if (f == NULL) {
			logprintf(LOG_ERR, "Cannot open i2c name file %s", s);
			return -1;
		}
		memset(s, 0, sizeof s);
		fread(s, 1, sizeof s, f);
		fclose(f);

		if (strncmp(s, "bt878", 5) == 0) {
			if (strncmp(de->d_name, "i2c-", 4) != 0) {
				logprintf(LOG_ERR, "i2c adapter dir %s "
				          "has unexpected name", de->d_name);
				return -1;
			}
			found = atoi(de->d_name + 4);
			break;
		}
	}

	closedir(dir);
	if (found == -1) {
		logprintf(LOG_ERR, "Cannot find i2c adapter");
		return -1;
	}

	snprintf(device_name, sizeof device_name, "/dev/i2c-%d", found);
	logprintf(LOG_INFO, "Using i2c device %s", device_name);
	hw.device = device_name;
	return open(device_name, O_RDWR);
}

static int i2cuser_init(void) {
	int pipe_fd[2] = { -1, -1 };

	if (pipe(pipe_fd) != 0) {
		logprintf(LOG_ERR, "Couldn't open pipe: %s", strerror(errno));
		return 0;
	}
	hw.fd = pipe_fd[0];

	i2c_fd = open_i2c_device();
	if (i2c_fd == -1) {
		logprintf(LOG_ERR, "i2c device cannot be opened");
		goto fail;
	}

	if (ioctl(i2c_fd, I2C_SLAVE, IR_ADDR) < 0) {
		logprintf(LOG_ERR, "Cannot set i2c address %02x", IR_ADDR);
		goto fail;
	}

	child = fork();
	if (child == -1) {
		logprintf(LOG_ERR, "Cannot fork child process: %s",
		          strerror(errno));
		goto fail;
	} else if (child == 0) {
		close(pipe_fd[0]);
		i2cuser_read_loop(pipe_fd[1]);
	}
	close(pipe_fd[1]);

	logprintf(LOG_INFO, "i2cuser driver: i2c device found and ready to go");
	return 1;

fail:
	if (i2c_fd != -1)
		close(i2c_fd);
	if (pipe_fd[0] != -1)
		close(pipe_fd[0]);
	if (pipe_fd[1] != -1)
		close(pipe_fd[1]);
	return 0;
}

static int i2cuser_deinit(void) {
	if (child != -1) {
		if (kill(child, SIGTERM) == -1)
			return 0;
		if (waitpid(child, NULL, 0) == 0)
			return 0;
	}
	if (i2c_fd != -1)
		close(i2c_fd);
	if (hw.fd != -1)
		close(hw.fd);
	return 1;
}

static void i2cuser_read_loop(int out_fd) {
	ir_code last_code = 0;
	double last_time = 0.0;

	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	for (;;) {
		unsigned char buf[3], code_buf[CODE_SIZE];
		int rc, i;
		ir_code new_code;
		struct timeval tv;
		double new_time;

		do {
			/* Poll 20 times per second. */
			struct timespec ts = {0, 50000000};
			nanosleep(&ts, NULL);

			rc = read(i2c_fd, &buf, sizeof buf);
			if (rc < 0 && errno != EREMOTEIO) {
				logprintf(LOG_ERR, "Error reading from i2c "
				          "device: %s", strerror(errno));
				goto fail;
			} else if (rc != sizeof buf) {
				continue;
			}
		} while ((buf[0] & 0x80) == 0);

		gettimeofday(&tv, NULL);
		new_time = tv.tv_sec + (0.000001L * tv.tv_usec);

		new_code = ((buf[0] & 0x7f) << 6) | (buf[1] >> 2);
		if (new_code == last_code) {
			/* Same code as last time -- so we need to see whether
			   it should be repeated or not. (The Hauppauge remote
			   control will send a different code if you release
			   and press the same key.) */
			if (new_time - last_time < REPEAT_TIME)
				continue;
		}

		logprintf(LOG_INFO, "Read input code: %08x", new_code);
		last_time = new_time;
		last_code = new_code;

		for (i = 0; i < CODE_SIZE; i++) {
			code_buf[(CODE_SIZE - 1) - i] = new_code & 0xFF;
			new_code >>= 8;
		}
		if (write(out_fd, code_buf, CODE_SIZE) != CODE_SIZE) {
			logprintf(LOG_ERR, "Write to i2cuser pipe failed: %s",
			          strerror(errno));
			goto fail;
		}
	}

fail:
	_exit(1);
}

static char *i2cuser_rec(struct ir_remote *remotes) {
	if (!clear_rec_buffer()) return NULL;
	return decode_all(remotes);
}
