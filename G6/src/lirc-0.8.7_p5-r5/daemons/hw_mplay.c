/****************************************************************************
 ** hw_mplay.c **************************************************************
 ****************************************************************************
 *
 * LIRC driver for Vlsys mplay usb ftdi serial port remote control.
 *
 * Driver inspire from hw_accent et hw_alsa_usb.
 *
 * The vlsys mplay is a remote control with an Ir receiver connected to the
 * usb bus via a ftdi driver. The device communicate with the host at 38400
 * 8N1.
 * 
 * For each keypress on the remote controle, one code byte is transmitted
 * follow by regular fixe code byte for repetition if the key is held-down.
 * For example, if you press key 1, the remote first send 0x4d (key code)
 * and next regulary send 0x7e (repetetion code) as you held-down the key.
 * For key 2 you get 0x4e 0x7e 0x7e ...
 *
 * Copyright (c) 2007 Benoit Laurent <ben905@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Functions available for logging (see tools/lircrcd.c).
 *
 * NOTE: if compiled without the DEBUG option and with SYSLOG,
 * you cannot control the amount of debug info sent to syslog,
 * even the LOG_DEBUG messages will be logged.
 *
 * void logprintf(int priority, const char *format, ...)
 *     Calls the syslog(3) function.
 *
 * void logperror(int priority, const char *s)
 *    Uses the syslog(3) to print a message followed by the error message
 *    strerror (%m) associated to the present errno.
 *
 * void LOGPRINTF(int priority, const char *format, ...)
 *    Calls logprintf(), but only if compiled with DEBUG option.
 *
 * void LOGPERROR(int priority, const char *s)
 *    Calls logperror(), but only if compiled with DEBUG option.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyUSB0"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>

#include "lircd.h"
#include "hardware.h"
#include "serial.h"

#include "hw_mplay.h"

/* The mplay code length in bit */
#define MPLAY_CODE_LENGTH 8

/* Code value send by the mplay to indicate a repeatition of the last code */
#define MPLAY_REPEAT_CODE 0x7e

/* Mplay serial baud rate */
#define MPLAY_BAUD_RATE 38400

/* Max time in micro seconde between the reception of repetition code. After
   this time, we ignore the key repeat */
#define MAX_TIME_BETWEEN_TWO_REPETITION_CODE 500000

/**************************************************************************
 * Definition of local struct that permit to save data from call to call
 * of the driver.
 **************************************************************************/
static struct {
        /* the last receive code */
        ir_code rc_code;
        /* Int wich indicate that the last reception was a repetition */
        int repeat_flag;
        /* Date of the last reception */
        struct timeval last_reception_time;
        /* Flag wich indicate a timeout between the reception of repetition
           Some time the receiver lost a key code and only recieved
           the associated repetition code. Then the driver interpret
           this repetition as a repetition of the last receive key code
           and not the lost one (ex: you press Volume+ after Volume-
           and the sound continu to go down). To avoid this problem
           we set a max time between two repetition. */
        int timeout_repetition_flag;
} mplay_local_data = {
        0,
        0,
        {0, 0},
        0
};

/**************************************************************************
 * Definition of the standard internal hardware interface
 * use by lirc for the mplay device
 **************************************************************************/
struct hardware hw_mplay = {
        LIRC_IRTTY,             /* default device */
        -1,                     /* fd */
        LIRC_CAN_REC_LIRCCODE,  /* features */
        0,                      /* send_mode */
        LIRC_MODE_LIRCCODE,     /* rec_mode */
        MPLAY_CODE_LENGTH,      /* code_length */
        mplay_init,             /* init_func */
        mplay_deinit,           /* deinit_func */
        NULL,                   /* send_func */
        mplay_rec,              /* rec_func */
        mplay_decode,           /* decode_func */
        NULL,                   /* ioctl_func */
        NULL,                   /* readdata */
        "mplay"
};

/**************************************************************************
 * Lock and initialize the serial port.
 * This function is called by the LIRC daemon when the first client
 * registers itself.
 * Return 1 on success, 0 on error.
 **************************************************************************/
int mplay_init(void)
{
        int result = 1;
        LOGPRINTF(1, "Entering mplay_init()");
        /* Creation of a lock file for the port */
        if (!tty_create_lock(hw.device)) {
                logprintf(LOG_ERR,   "Could not create the lock file");
                LOGPRINTF(1, "Could not create the lock file");
                result = 0;
        }
        /* Try to open serial port */
        else if ((hw.fd = open(hw.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0)
	{
                logprintf(LOG_ERR,   "Could not open the serial port");
                LOGPRINTF(1, "Could not open the serial port");
                mplay_deinit();
                result = 0;
        }
        /* Serial port configuration */
        else if (!tty_reset(hw.fd) || !tty_setbaud(hw.fd, MPLAY_BAUD_RATE))
        {
                logprintf(LOG_ERR,   "could not configure the serial port for "
			  "'%s'", hw.device);
                LOGPRINTF(1, "could not configure the serial port for "
                        "'%s'", hw.device);
                mplay_deinit();
        }
        return result;
}

/**************************************************************************
 * Close and release the serial line.
 **************************************************************************/
int mplay_deinit(void)
{
        LOGPRINTF(1, "Entering mplay_deinit()");
        close(hw.fd);
        tty_delete_lock();
        hw.fd = -1;
        return(1);
}

/**************************************************************************
 * Receive a code (1 byte) from the remote.
 * This function is called by the LIRC daemon when I/O is pending
 * from a registered client, e.g. irw.
 *
 * return NULL if nothing have been received or a lirc code
 **************************************************************************/
char *mplay_rec(struct ir_remote *remotes)
{
        unsigned char rc_code;
        signed int len;
        struct timeval current_time;
        LOGPRINTF(1, "Entering mplay_rec()");
        len = read(hw.fd, &rc_code, 1);
        gettimeofday(&current_time, NULL);
        if (len != 1) 
        {
                /* Something go wrong during the read, we close the device 
                   for prevent endless looping when the device 
                   is disconnected */
                LOGPRINTF(1, "Reading error in mplay_rec()");
                mplay_deinit();
                return NULL;
        }
        else
        {
                /* We have received a code */
                if (rc_code == MPLAY_REPEAT_CODE)
                {
                        if (mplay_local_data.timeout_repetition_flag == 1)
                        {
                               /* We ignore the repetition */ 
                               return NULL;
                        }
                        else
                        {
                               if (time_elapsed(&mplay_local_data.last_reception_time, 
                                                &current_time) <= 
                                                MAX_TIME_BETWEEN_TWO_REPETITION_CODE)
                               {
                                       /* This reception is a repeat */
                                       mplay_local_data.repeat_flag = 1;
                                       /* We save the reception time */
                                       mplay_local_data.last_reception_time = current_time;
                               }
                               else
                               {
                                       /* To much time between repetition, 
                                          the receiver have  probably miss 
                                          a valide key code. We ignore the 
                                          repetition */ 
                                       mplay_local_data.timeout_repetition_flag = 1;
                                       mplay_local_data.repeat_flag = 0;
                                       return NULL;
                               }
                        }
                }
                else
                {
                       /* This is a new code */
                       mplay_local_data.rc_code = rc_code;
                       mplay_local_data.repeat_flag = 0;
                       mplay_local_data.timeout_repetition_flag = 0;
                       mplay_local_data.last_reception_time = current_time;
                }
                LOGPRINTF(1, 
                       "code: %u",
                       (unsigned int) mplay_local_data.rc_code);
                LOGPRINTF(1, "repeat_flag: %d", mplay_local_data.repeat_flag);
                return decode_all(remotes);
        }
}

/**************************************************************************
 * This function is called by the LIRC daemon during the transform of a
 * received code into an lirc event.
 *
 * It gets the global variable code (remote keypress code).
 *
 * It returns:
 *  prep                Code prefix (zero for this LIRC driver)
 *  codep               Code of keypress
 *  postp               Trailing code (zero for this LIRC dirver)
 *  repeat_flagp        True if the keypress is a repeated keypress
 *  min_remaining_gapp  Min extimated time gap remaining before next code
 *  max_remaining_gapp  Max extimated time gap remaining before next code
 **************************************************************************/
int mplay_decode(
    struct ir_remote *remote,
    ir_code *prep,
    ir_code *codep,
    ir_code *postp,
    int *repeat_flagp,
    lirc_t *min_remaining_gapp,
    lirc_t *max_remaining_gapp)
{
        LOGPRINTF(1, "Entering mplay_decode(), code = %u\n",
		  (unsigned int)mplay_local_data.rc_code);

        if(!map_code(
                remote,
                prep,
                codep,
                postp,
                0,
                0,
                MPLAY_CODE_LENGTH,
                mplay_local_data.rc_code,
                0,
                0))
        {
                return(0);
        }
        *repeat_flagp = mplay_local_data.repeat_flag;
        *min_remaining_gapp = 0;
        *max_remaining_gapp = 0;
        return 1;
}
