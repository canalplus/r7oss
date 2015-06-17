/*      $Id: hw_srm7500libusb.c,v 5.2 2010/04/11 18:50:38 lirc Exp $      */

/****************************************************************************
 ** hw_srm7500libusb.c ******************************************************
 ****************************************************************************
 *  Userspace (libusb) driver for Philips SRM7500 RF Remote.
 *  
 *  Copyright (C) 2009 Henning Glawe <glaweh@debian.org>
 *
 *  based on Userspace (libusb) driver for ATI/NVidia/X10 RF Remote.
 *
 *  Copyright (C) 2004 Michael Gold <mgold@scs.carleton.ca>
 *
 *  and hw_devinput.c
 *
 *  Copyright (C) 2002 Oliver Endriss <o.endriss@gmx.de>
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

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <usb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"

#include "hw_srm7500libusb.h"

#define CODE_BYTES 3
#define USB_TIMEOUT (1000*10)
#define CONTROL_BUFFERSIZE 128

static int srm7500_init();
static int srm7500_deinit();
static char *srm7500_rec(struct ir_remote *remotes);
static int srm7500_decode(struct ir_remote *remote,
			   ir_code *prep, ir_code *codep, ir_code *postp,
			   int *repeat_flagp,
			   lirc_t *min_remaining_gapp,
			   lirc_t *max_remaining_gapp);
static int usb_read_loop(int fd);
static struct usb_device *find_usb_device(void);
static int find_device_endpoints(struct usb_device *dev);
static int srm7500_initialize_usbdongle();
static int srm7500_initialize_802154_stack();
static int srm7500_deinitialize_usbdongle();
static int philipsrf_input(philipsrf_incoming_t *buffer_in);
static int philipsrf_output(philipsrf_outgoing_t buffer_out);
static void srm7500_sigterm(int sig);

struct hardware hw_srm7500libusb =
{
	NULL,                       /* default device */
	-1,                         /* fd */
	LIRC_CAN_REC_LIRCCODE,      /* features */
	0,                          /* send_mode */
	LIRC_MODE_LIRCCODE,         /* rec_mode */
	CODE_BYTES * CHAR_BIT,      /* code_length */
	srm7500_init,               /* init_func */
	srm7500_deinit,             /* deinit_func */
	NULL,                       /* send_func */
	srm7500_rec,                /* rec_func */
	srm7500_decode,             /* decode_func */
	NULL,                       /* ioctl_func */
	NULL,                       /* readdata */
	"srm7500libusb"
};

typedef struct {
	u_int16_t vendor;
	u_int16_t product;
} usb_device_id;

/* table of compatible remotes */
static usb_device_id usb_remote_id_table[] = {
	{ 0x0471, 0x0617 }, /* Philips IEEE802.15.4 RF Dongle */
	{ 0, 0 } /* Terminating entry */
};

static struct usb_dev_handle *dev_handle = NULL;
static struct usb_endpoint_descriptor *dev_ep_in = NULL, *dev_ep_out = NULL;
static pid_t child = -1;
static ir_code code;
static int repeat_flag=0;
static u_int8_t macShortAddress[2]       = {0, 0};
static u_int8_t macPANId[2]              = {0, 0};
static u_int8_t LogicalChannel           = 0x19;
static u_int8_t remoteShortAddress[2]    = {0, 0};
static u_int8_t remoteExtendedAddress[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int remoteExtendedAddressGiven = 0;
static u_int8_t macBeaconPayload[64] = "PHILIPS";
static int macBeaconPayloadGiven = 0;
static int requested_usb_bus_number = -1;
static int requested_usb_device_number = -1;
static int srm7500_terminate = 0;

/* initialize driver -- returns 1 on success, 0 on error */
static int srm7500_init()
{
	int pipe_fd[2] = { -1, -1 };
	int got_macShortAddress = 0;
	int got_macPANId = 0;
	int got_remoteShortAddress = 0;
	char  *op_start,*op_end,*string_end;
	
	logprintf(LOG_INFO, "initializing driver");

	if (hw.device == NULL) {
		logprintf(LOG_ERR, "no device options supplied, "
			  "please read the documentation in "
			  "philips/lircd.conf.srm7500libusb!");
		return 0;
	}
		
	op_start=hw.device;
	string_end=strchr(hw.device,0);
	while (op_start<string_end) {
		int result;
		op_end=strchrnul(op_start,',');
		if (!strncmp(op_start,"macShortAddress=",16)) {
			result=sscanf(op_start+16,"%hhx:%hhx",
				      macShortAddress, macShortAddress+1);
			if (result == 2) {
				got_macShortAddress = 1;
			} else {
				logprintf(LOG_ERR, "error parsing option "
					  "macShortAddress");
			}
		} else if (!strncmp(op_start,"macPANId=",9)) {
			result=sscanf(op_start+9,"%hhx:%hhx",
				      macPANId,macPANId+1);
			if (result == 2) {
				got_macPANId = 1;
			} else {
				logprintf(LOG_ERR, "error parsing option "
					  "macPANId");
			}
		} else if (!strncmp(op_start,"LogicalChannel=",15)) {
			result=sscanf(op_start+15,"%hhx",
				      &LogicalChannel);
			if (result == 1) {
				if (LogicalChannel!=0x19)
					logprintf(LOG_WARNING,
						  "SRM7500 may not work on "
						  "channels other than 0x19");
			} else {
				logprintf(LOG_ERR, "error parsing option "
					  "LogicalChannel");
			}
		} else if (!strncmp(op_start,"remoteShortAddress=",19)) {
			result=sscanf(op_start+19,"%hhx:%hhx",
				      remoteShortAddress,remoteShortAddress+1);
			if (result == 2) {
				got_remoteShortAddress = 1;
			} else {
				logprintf(LOG_ERR, "error parsing option "
					  "remoteShortAddress");
			}
		} else if (!strncmp(op_start,"remoteExtendedAddress=",22)) {
			result=sscanf(op_start+22,"%hhx:%hhx:%hhx:%hhx:"
				      "%hhx:%hhx:%hhx:%hhx",
				      remoteExtendedAddress,
				      remoteExtendedAddress+1,
				      remoteExtendedAddress+2,
				      remoteExtendedAddress+3,
				      remoteExtendedAddress+4,
				      remoteExtendedAddress+5,
				      remoteExtendedAddress+6,
				      remoteExtendedAddress+7);
			if (result == 8) {
				remoteExtendedAddressGiven = 1;
			} else {
				logprintf(LOG_ERR, "error parsing option "
					  "remoteExtendedAddress");
			}
		} else if (!strncmp(op_start,"macBeaconPayload=",17)) {
			strncpy((char*)macBeaconPayload, op_start+17,
				op_end-(op_start+17));
			macBeaconPayload[op_end-(op_start+17)]=0;
			macBeaconPayloadGiven = 1;
		} else if (!strncmp(op_start,"usb=",4)) {
			result=sscanf(op_start+4,"%i:%i",
				      &requested_usb_bus_number,
				      &requested_usb_device_number);
			if (result == 2) {
				LOGPRINTF(1, "got usb %i:%i",
					  requested_usb_bus_number,
					  requested_usb_device_number);
			} else {
				logprintf(LOG_ERR, "error parsing option usb");
			}
		} else {
			char erroroptionstring[op_end-op_start+1];
			
			strncpy(erroroptionstring,op_start,op_end-op_start+1);
			erroroptionstring[op_end-op_start]=0;
			logprintf(LOG_WARNING, "enparsable option: %s",
				  erroroptionstring);
		}
		op_start=op_end+1;
	}
	if (!(got_remoteShortAddress && got_macPANId && got_macShortAddress)) {
		logprintf(LOG_ERR, "driver needs at least remoteShortAddress, "
			  "macPANId and macShortAddress");
		return 0;
	}
	if (!macBeaconPayloadGiven) {
		if (gethostname((char*)(macBeaconPayload+7),64-7) != 0) {
			logprintf(LOG_ERR, "could not get hostname!");
			return 0;
		}
	}

	logprintf(LOG_INFO, "802.15.4 network parameters");
	logprintf(LOG_INFO, "    macShortAddress %02hhx:%02hhx",
		  macShortAddress[0],macShortAddress[1]);
	logprintf(LOG_INFO, "    macPANId %02hhx:%02hhx",
		  macPANId[0],macPANId[1]);
	logprintf(LOG_INFO, "    LogicalChannel %02hhx",
		  LogicalChannel);
	logprintf(LOG_INFO, "    remoteShortAddress %02hhx:%02hhx",
		  remoteShortAddress[0], remoteShortAddress[1]);
	logprintf(LOG_INFO, "    macBeaconPayload %s",macBeaconPayload);
	if (remoteExtendedAddressGiven) {
		logprintf(LOG_INFO, "    Connectivity restricted to "
			  "remoteExtendedAddress %02hhx:%02hhx:%02hhx:%02hhx:"
			  "%02hhx:%02hhx:%02hhx:%02hhx",
			  remoteExtendedAddress[0],remoteExtendedAddress[1],
			  remoteExtendedAddress[2],remoteExtendedAddress[3],
			  remoteExtendedAddress[4],remoteExtendedAddress[5],
			  remoteExtendedAddress[6],remoteExtendedAddress[7]);
	}
	
	init_rec_buffer();
	
	/* A separate process will be forked to read data from the USB
	 * receiver and write it to a pipe. hw.fd is set to the readable
	 * end of this pipe. */
	if (pipe(pipe_fd) != 0)
	{
		logperror(LOG_ERR, "could not open pipe");
		return 0;
	}
	

	child = fork();
	if (child == -1)
	{
		logperror(LOG_ERR, "could not fork child process");
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		return 0;
	}
	else if (child == 0)
	{
		struct sigaction act;
		int status=1;
	
		alarm(0);

		srm7500_terminate=0;

		act.sa_handler=srm7500_sigterm;
		sigfillset(&act.sa_mask);
		act.sa_flags=SA_RESTART; /* don't fiddle with EINTR */
		sigaction(SIGTERM,&act,NULL);
		sigaction(SIGINT,&act,NULL);

		signal(SIGPIPE, SIG_DFL);
		signal(SIGHUP, SIG_IGN);
		signal(SIGALRM, SIG_IGN);
	
		close(pipe_fd[0]);
		while (! srm7500_terminate) {
			if (! srm7500_initialize_usbdongle())
				goto fail;
			if (! srm7500_initialize_802154_stack())
				goto fail;
			logprintf(LOG_INFO, "USB receiver initialized");
			status=usb_read_loop(pipe_fd[1]);
		fail:
			sleep(2);
		}
		srm7500_deinitialize_usbdongle();
		_exit(status);
	}
	
	close(pipe_fd[1]);
	hw.fd = pipe_fd[0];
	
	return 1;

}

static void srm7500_sigterm(int sig)
{
    /* all signals are blocked now */
    if(srm7500_terminate) return;
    srm7500_terminate=1;
}

static int srm7500_initialize_usbdongle()
{
	struct usb_device *usb_dev;
	int res;
	u_int8_t control_buffer[CONTROL_BUFFERSIZE];
	
	logprintf(LOG_INFO, "initializing Philips USB receiver");

	usb_dev = find_usb_device();
	if (usb_dev == NULL)
	{
		logprintf(LOG_ERR, "could not find a compatible USB device");
		return 0;
	}

	if (!find_device_endpoints(usb_dev))
	{
		logprintf(LOG_ERR, "could not find device endpoints");
		return 0;
	}
	
	dev_handle = usb_open(usb_dev);
	if (dev_handle == NULL)
	{
		logperror(LOG_ERR, "could not open USB receiver");
		return 0;
	}

#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
	res=usb_detach_kernel_driver_np(dev_handle, 0);
	if ( (res < 0) && (res != -ENODATA) ) {
		logperror(LOG_ERR, "could not detach kernel driver");
		return 0;
	}
#endif
	
	if (usb_claim_interface(dev_handle, 0) != 0)
	{
		logperror(LOG_ERR, "could not claim USB interface");
		return 0;
	}
	
	/* device initialization */
	memset(control_buffer, 0, sizeof(control_buffer));
	usb_control_msg(dev_handle,
			USB_TYPE_STANDARD|USB_RECIP_INTERFACE|USB_ENDPOINT_OUT,
			USB_REQ_SET_INTERFACE,
			0x0000,
			0x0000,
			(char*) control_buffer,
			0x0000,
			USB_TIMEOUT);
	control_buffer[0]=0xe4;
	usb_control_msg(dev_handle,
			USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_ENDPOINT_OUT,
			USB_REQ_SET_CONFIGURATION,
			0x0300,
			0x0000,
			(char*) control_buffer,
			0x0010,
			USB_TIMEOUT);
	control_buffer[0]=0;
	usb_control_msg(dev_handle,
			USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_ENDPOINT_IN,
			USB_REQ_CLEAR_FEATURE,
			0x0300,
			0x0000,
			(char*) control_buffer,
			0x0010,
			USB_TIMEOUT);
	/*
	  HG: with this the red control light on usb receiver is
	  switched on seems like the command to enable transceiver
	*/
	memset(control_buffer, 0, sizeof(control_buffer));
	control_buffer[0]=0xe2;
	usb_control_msg(dev_handle,
		USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_ENDPOINT_OUT,
		USB_REQ_SET_CONFIGURATION,
		0x0300,
		0x0000,
		(char*) control_buffer,
		0x0010,
		USB_TIMEOUT);
	memset(control_buffer, 0, sizeof(control_buffer));
	usb_control_msg(dev_handle,
		USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_ENDPOINT_IN,
		USB_REQ_CLEAR_FEATURE,
		0x0300,
		0x0000,
		(char*) control_buffer,
		0x0010,
		USB_TIMEOUT);
	return 1;
}

static int srm7500_deinitialize_usbdongle()
{
	int result = 1;
	u_int8_t control_buffer[CONTROL_BUFFERSIZE];

	logprintf(LOG_INFO,"disabling USB receiver");

	if (!dev_handle)
	{
		return result;
	}
	/*
	  HG: sending e1 as control payload disables the control
	  light. so most probably the transceiver, too
	*/
	memset(control_buffer, 0, sizeof(control_buffer));
	control_buffer[0]=0xe1;
	usb_control_msg(dev_handle,
			USB_TYPE_CLASS|USB_RECIP_INTERFACE|USB_ENDPOINT_OUT,
			USB_REQ_SET_CONFIGURATION,
			0x0300,
			0x0000,
			(char*) control_buffer,
			0x0010,
			USB_TIMEOUT);
	usb_reset(dev_handle);
	if (usb_close(dev_handle) < 0) result = 0;
	dev_handle = NULL;

	return result;
}

static int srm7500_initialize_802154_stack()
{
	philipsrf_outgoing_t packet_buffer_out;
	philipsrf_incoming_t packet_buffer_in;

	int i;
	int outret,inret;
	int tries=3;
	int answer_received=0;
	
	while ((! answer_received) && (tries>0) ) {
		inret = philipsrf_input(&packet_buffer_in);
		if ((packet_buffer_in.type    == MLME_SET_confirm) && 
		    (packet_buffer_in.data[0] == 0) && 
		    (packet_buffer_in.data[1] == PIB_ATTR_macExtendedAddress))
			answer_received=1;
		tries--;
	}
	if ((tries==0) && (packet_buffer_in.type != MLME_SET_confirm))
		return 0;

	packet_buffer_out.length  = 2;
	packet_buffer_out.type    = MLME_RESET_request;
	packet_buffer_out.data[0] = MLME_TRUE;   /* SetDefaultPIB */

	outret=philipsrf_output(packet_buffer_out);
	inret=philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_RESET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not reset USB dongle!");
		return 0;
	}
	
	packet_buffer_out.length  = 4;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macCoordShort_Address;
	packet_buffer_out.data[1] = macShortAddress[0];
	packet_buffer_out.data[2] = macShortAddress[1];
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macCoordShort_Address!");
		return 0;
	}

	packet_buffer_out.length  = 4;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macPANId;
	packet_buffer_out.data[1] = macPANId[0];
	packet_buffer_out.data[2] = macPANId[1];
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macPANId!");
		return 0;
	}

	packet_buffer_out.length  = 4;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macShortAddress;
	packet_buffer_out.data[1] = macShortAddress[0];
	packet_buffer_out.data[2] = macShortAddress[1];
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macShortAddress!");
		return 0;
	}

	packet_buffer_out.length  = 3;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macAssociation_Permit;
	packet_buffer_out.data[1] = MLME_TRUE;
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macAssociation_Permit!");
		return 0;
	}

	packet_buffer_out.length  = 3;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macRxOnWhenIdle;
	packet_buffer_out.data[1] = MLME_TRUE;
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macRxOnWhenIdle!");
		return 0;
	}


	/* beacon data:
	   contents: ASCII PHILIPS + hostname */
	u_int8_t beacon_length=0;
	beacon_length=(u_int8_t)strnlen((char*)macBeaconPayload,64);

	/* beacon data length */
	packet_buffer_out.length  = 3;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macBeaconPayload_Length;
	packet_buffer_out.data[1] = beacon_length;
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macBeaconPayload_Length!");
		return 0;
	}

	/* beacon data */
	packet_buffer_out.length  = 2 + beacon_length;
	packet_buffer_out.type    = MLME_SET_request;
	packet_buffer_out.data[0] = PIB_ATTR_macBeaconPayload;
	for (i=0;i<beacon_length;i++)
		packet_buffer_out.data[i+1]=macBeaconPayload[i];
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_SET_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not set macBeaconPayload!");
		return 0;
	}

	packet_buffer_out.length  = 6;
	packet_buffer_out.type    = MLME_START_request;
	packet_buffer_out.data[0] = macPANId[0];
	packet_buffer_out.data[1] = macPANId[1];
	packet_buffer_out.data[2] = LogicalChannel;
	/* note: the RFC lists three bytes StartTime here. strange. */
	/* BeaconOrder/SuperframeOrder? both are listed in the rfc
	   with 0-15 range so this could be the compression of
	   both... */
	packet_buffer_out.data[3] = 0xff;
	packet_buffer_out.data[4] = MLME_TRUE; /* PANCoordinator */
	/* In RFC, there is a bunch of other data for this primitive
	   (Table 72), which will be interpreted as set to 0 */
	philipsrf_output(packet_buffer_out);
	philipsrf_input(&packet_buffer_in);
	if (!((packet_buffer_in.type == MLME_START_confirm) && 
	      (packet_buffer_in.data[0] == 0))) {
		logprintf(LOG_ERR, "could not start PAN!");
		return 0;
	}
	return 1;
}

#ifdef DEBUG
#define HEXDUMP(prefix, buf, len) \
	hexdump(prefix, buf, len)
#else
#define HEXDUMP(prefix, buf, len)
#endif

#ifdef DEBUG
static void hexdump(char *prefix, u_int8_t *buf, int len)
{
	int i;
	char str[1024];
	int pos = 0;
	if (prefix != NULL) {
		strncpy(str,prefix,sizeof(str));
		pos = strnlen(str,sizeof(str));
	}
	if (len>0) {
		for (i = 0; i < len; i++) {
			if (pos + 3 >= sizeof(str)) {
				break;
			}

			if (!(i % 8)) {
				str[pos++] = ' ';
			}

			sprintf(str + pos, "%02x ", buf[i]);

			pos += 3;
		}
	} else {
		strncpy(str+pos,"NO DATA",sizeof(str));
	}
	LOGPRINTF(1, "%s", str);
}
#endif

static int philipsrf_input(philipsrf_incoming_t *buffer_in) {
	int ret=0;
	
	ret=usb_interrupt_read(dev_handle, dev_ep_in->bEndpointAddress,
			       (char*) buffer_in, 64, USB_TIMEOUT);
	if (ret>0) {
		LOGPRINTF(1, "in: time 0x%08x, length 0x%02x, "
			  "type 0x%02x",
			  ((buffer_in->time[3]<<24)|(buffer_in->time[2]<<16) |
			   (buffer_in->time[1]<< 8)|(buffer_in->time[0])),
			  buffer_in->length,buffer_in->type);
		HEXDUMP("in  data:", buffer_in->data, buffer_in->length-1);
	}
	return ret;
}

static int philipsrf_output(philipsrf_outgoing_t buffer_out) {
	int ret=0;
	
	LOGPRINTF(1, "out: length 0x%02x, type 0x%02x",
		  buffer_out.length,buffer_out.type);
	HEXDUMP("out data:", buffer_out.data, buffer_out.length-1);
	
	ret = usb_interrupt_write(dev_handle, dev_ep_out->bEndpointAddress,
				  (char*)&buffer_out, buffer_out.length+1,
				  USB_TIMEOUT);
	return ret;
}

/* deinitialize driver -- returns 1 on success, 0 on error */
static int srm7500_deinit()
{
	int result = 1;

	logprintf(LOG_INFO, "disabling driver");

	if (hw.fd >= 0)
	{
		if (close(hw.fd) < 0) result = 0;
		hw.fd = -1;
	}
	
	if (child > 1)
	{
		if ( (kill(child, SIGTERM) == -1)
		     || (waitpid(child, NULL, 0) == 0) ) result = 0;
	}
	
	return result;
}

static char *srm7500_rec(struct ir_remote *remotes)
{
	u_int8_t rccode[3];
	int rd;

	rd = read(hw.fd, &rccode, 3);
	if (rd != 3) {
		logprintf(LOG_ERR, "error reading from usb worker process");
		if(rd <= 0 && errno != EINTR) srm7500_deinit();
		return 0;
	}

	LOGPRINTF(1, "key %02x%02x, type %02x",
		  rccode[0], rccode[1], rccode[2]);
	
	if (rccode[2] == SRM7500_KEY_REPEAT) {
		rccode[2] = 1;
		repeat_flag = 1;
	} else {
		repeat_flag = 0;
	}

	code = ((rccode[0] << 16) | (rccode[1] << 8) | rccode[2]);

	LOGPRINTF(1, "code %.8llx", code);

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

	if ((requested_usb_bus_number > 0) &&
	    (requested_usb_device_number > 0)) {
		for (usb_bus=usb_busses;usb_bus; usb_bus=usb_bus->next)
			if (atoi(usb_bus->dirname) == requested_usb_bus_number)
				break;
		if (! usb_bus) {
			logprintf(LOG_ERR, "requested USB bus %d does not "
				  "exist", requested_usb_bus_number);
			return NULL;
		}
		for (dev=usb_bus->devices;dev;dev=dev->next)
			if (dev->devnum == requested_usb_device_number)
				break;
		if (! dev) {
			logprintf(LOG_ERR, "requested USB device %d:%d "
				  "does not exist",
				  requested_usb_bus_number,
				  requested_usb_device_number);
			return NULL;
		}
		if (is_device_ok(dev)) {
			return dev;
		} else {
			logprintf(LOG_ERR, "requested USB device %d:%d, but "
				  "id %04x:%04x not handled by this driver",
				  requested_usb_bus_number,
				  requested_usb_device_number,
				  dev->descriptor.idVendor,
				  dev->descriptor.idProduct);
		}
	} else {
		for (usb_bus = usb_busses; usb_bus; usb_bus = usb_bus->next)
		{
			for (dev = usb_bus->devices; dev; dev = dev->next)
			{
				if (is_device_ok(dev)) return dev;
			}
		}
	}
	return NULL;  /* no suitable device found */
}

/* set dev_ep_in and dev_ep_out to the in/out endpoints of the given
 * device. returns 1 on success, 0 on failure. */
static int find_device_endpoints(struct usb_device *dev)
{
	struct usb_interface_descriptor *idesc;
	
	if (dev->descriptor.bNumConfigurations != 1) return 0;
	if (dev->config[0].bNumInterfaces != 1) return 0;
	if (dev->config[0].interface[0].num_altsetting != 1) return 0;
	
	idesc = &dev->config[0].interface[0].altsetting[0];
	if (idesc->bNumEndpoints != 2) return 0;
	
	dev_ep_in = &idesc->endpoint[1];
	if ((dev_ep_in->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			!= USB_ENDPOINT_IN) return 0;
	if ((dev_ep_in->bmAttributes & USB_ENDPOINT_TYPE_MASK)
			!= USB_ENDPOINT_TYPE_INTERRUPT) return 0;

	dev_ep_out = &idesc->endpoint[0];
	if ((dev_ep_out->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			!= USB_ENDPOINT_OUT) return 0;
	if ((dev_ep_out->bmAttributes & USB_ENDPOINT_TYPE_MASK)
			!= USB_ENDPOINT_TYPE_INTERRUPT) return 0;
	
	return 1;
}

/* this function is run in a forked process to read data from the USB
 * receiver and write it to the given fd. it calls exit() with result
 * code 0 on success, or 1 on failure. */
static int usb_read_loop(int fd)
{
	while (! srm7500_terminate)
	{
		philipsrf_outgoing_t packet_buffer_out;
		philipsrf_incoming_t packet_buffer_in;
		int inret, outret;
		int i;
		int is_ok;
		inret=philipsrf_input(&packet_buffer_in);
		if (srm7500_terminate)
			break;

		if (inret == -ETIMEDOUT)
			continue;
		
		if (inret < 0) {
			logprintf(LOG_ERR, "read error %d from usb dongle, "
				  "aborting\n", inret);
			return 0;
		}

		if (inret == 0)
			continue;

		switch (packet_buffer_in.type) {
		case MLME_ASSOCIATE_indication:
			/* setting up the remote pc connection
			 *    in: 0b 47 869ef4bc453e9000 88                    10
			 *              DeviceAddress    CapabilityInformation SecurityLevel(3bit),KeyIdMode(2bit) KeySource KeyIndex
			 *   out: 0d 48 869ef4bc453e9000 0039               00      00
			 *	        DeviceAddress    AssocShortAddress  status  SecurityLevel(3bit),KeyIdMode(2bit) KeySource KeyIndex
			*/
			/* first packet in setup */
			packet_buffer_out.length   =   13;
			packet_buffer_out.type     = MLME_ASSOCIATE_response;
			for (i=0;i<8;i++) {
				packet_buffer_out.data[i]=packet_buffer_in.data[i];
			}    /* DeviceAddress, copied from indication */
			packet_buffer_out.data[8]  = remoteShortAddress[0];    /* AssocShortAddress[0] */
			packet_buffer_out.data[9]  = remoteShortAddress[1];    /* AssocShortAddress[1] */
			
			is_ok=1;
			if (remoteExtendedAddressGiven)
				for (i=0;i<8;i++) 
					if (packet_buffer_in.data[i] != remoteExtendedAddress[i])
						is_ok=0;
			
			if (is_ok) {
				logprintf(LOG_NOTICE, "MLME_ASSOCIATE.response: device %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x associated",
					  packet_buffer_in.data[0],
					  packet_buffer_in.data[1],
					  packet_buffer_in.data[2],
					  packet_buffer_in.data[3],
					  packet_buffer_in.data[4],
					  packet_buffer_in.data[5],
					  packet_buffer_in.data[6],
					  packet_buffer_in.data[7]);
				/* Status: Successful */
				packet_buffer_out.data[10] =    0;
			} else {
				logprintf(LOG_NOTICE,
					  "MLME_ASSOCIATE.response: "
					  "unknown device %02x:%02x:%02x:%02x:"
					  "%02x:%02x:%02x:%02x rejected",
					  packet_buffer_in.data[0],
					  packet_buffer_in.data[1],
					  packet_buffer_in.data[2],
					  packet_buffer_in.data[3],
					  packet_buffer_in.data[4],
					  packet_buffer_in.data[5],
					  packet_buffer_in.data[6],
					  packet_buffer_in.data[7]);
				/* Status: Access denied */
				packet_buffer_out.data[10] = 0x02;
			}
			/* SecurityLevel,KeyIdMode */
			packet_buffer_out.data[11] = 0;
			outret = philipsrf_output(packet_buffer_out);
			inret = philipsrf_input(&packet_buffer_in);
			if ((packet_buffer_in.type ==
			     MLME_COMM_STATUS_indication) &&
			    (packet_buffer_in.data[packet_buffer_in.length-2] == 0) ) {
			}
			break;
		case MLME_ORPHAN_indication:
			/* after waking up remote:
			 *    in: 0a 52 869ef4bc453e9000 10
			 *        OrphanAddress
			 *        SecurityLevel(3bit),KeyIdMode(2bit)
			 *        KeySource
			 *        KeyIndex
			 *   out: 0c 53 869ef4bc453e9000 0039 01
			 *        OrphanAddress
			 *        ShortAddress
			 *        AssociatedMember
			 *    in: 165ab59e 034a6100 0000eb0d 0003869e f4bc453e 900000
			 * authorizing the remote? */
			packet_buffer_out.length  =   12;
			
			packet_buffer_out.type    = MLME_ORPHAN_response;
			
			/* OrphanAddress, copied from indication */
			memcpy(packet_buffer_out.data, packet_buffer_in.data,
			       8);
			/* ShortAddress */
			packet_buffer_out.data[8] = remoteShortAddress[0];    
			packet_buffer_out.data[9] = remoteShortAddress[1];
				
			is_ok=1;
			if (remoteExtendedAddressGiven)
				for (i=0; i<8; i++) 
					if (packet_buffer_in.data[i] !=
					    remoteExtendedAddress[i])
						is_ok=0;
			
			if (is_ok) {
				/* AssociatedMember */
				packet_buffer_out.data[10] = MLME_TRUE;
			} else {
				logprintf(LOG_NOTICE, "MLME_ORPHAN.response: "
					  "unknown device %02x:%02x:%02x:%02x:"
					  "%02x:%02x:%02x:%02x rejected",
					  packet_buffer_in.data[0],
					  packet_buffer_in.data[1],
					  packet_buffer_in.data[2],
					  packet_buffer_in.data[3],
					  packet_buffer_in.data[4],
					  packet_buffer_in.data[5],
					  packet_buffer_in.data[6],
					  packet_buffer_in.data[7]);
				/* AssociatedMember */
				packet_buffer_out.data[10] = MLME_FALSE;
			}
			
			philipsrf_output(packet_buffer_out);
			philipsrf_input(&packet_buffer_in);
			break;
		case MCPS_DATA_indication:
			/* nomal key on remote pressed
			 *   press: 1242  02 b59e 0027  02 b59e 4a61 
			 *            04  00 00 46 01  ff 10
			 *  repeat: 1242  02 b59e 0027  02 b59e 4a61
			 *            04  00 00 46 02  ff 10
			 * release: 1242  02 b59e 0027  02 b59e 4a61
			 *            04  00 00 46 03  ff 10 */
			LOGPRINTF(1, "MCPS_DATA_indication: "
				  "Dest(0x%04x:%04x) Source(0x%04x:0x%04x)\n",
				  ((packet_buffer_in.zig.DstPANId[0]<<8) |
				   (packet_buffer_in.zig.DstPANId[1])),
				  ((packet_buffer_in.zig.DstAddr[0]<<8)  |
				   (packet_buffer_in.zig.DstAddr[1])),
				  ((packet_buffer_in.zig.SrcPANId[0]<<8) |
				   (packet_buffer_in.zig.SrcPANId[1])),
				  ((packet_buffer_in.zig.SrcAddr[0]<<8)  |
				   (packet_buffer_in.zig.SrcAddr[1]))
				);
			HEXDUMP("MCPS_DATA_indication payload:",
				packet_buffer_in.zig.data,
				packet_buffer_in.zig.msduLength);
			if ((packet_buffer_in.zig.data[0]==0x00) &&
			    (packet_buffer_in.zig.msduLength==4)) {
				int bytes_w, pos;
				for (pos = 1;
				     pos < packet_buffer_in.zig.msduLength;
				     pos += bytes_w)
				{
					bytes_w = write(fd, packet_buffer_in.zig.data + pos, packet_buffer_in.zig.msduLength - pos);
					if (bytes_w < 0)
					{
						logperror(LOG_ERR, "could not write to pipe");
						return 0;
					}
				}
			}
			break;
		default:
			logprintf(LOG_INFO, "unhandled incoming usb packet "
				  "0x%02x\n", packet_buffer_in.type);
		}
	}
	
	return 1;

}

int srm7500_decode(struct ir_remote *remote,
		    ir_code *prep, ir_code *codep, ir_code *postp,
		    int *repeat_flagp,
		    lirc_t *min_remaining_gapp,
		    lirc_t *max_remaining_gapp)
{
	LOGPRINTF(1, "srm7500_decode");

	if(!map_code(remote,prep,codep,postp,
		0,0,hw_srm7500libusb.code_length,code,0,0))
	{
		return 0;
	}
	
	*repeat_flagp       = repeat_flag;
	*min_remaining_gapp = 0;
	*max_remaining_gapp = 0;
	
	return 1;
}
