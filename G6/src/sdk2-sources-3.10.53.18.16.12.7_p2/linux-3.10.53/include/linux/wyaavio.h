/*
 * Linux Kernel device driver
 * for Wyplay Analog Audio/Video Input/Output driver.
 *
 * For analog A/V outputs, analog A/V inputs, analog switches,
 * and additional video circuits.
 *
 * (C) Copyright 2007-2014 WyPlay SAS.
 */

#ifndef _WYAAVIO_H_
#define _WYAAVIO_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define AMIO_PORT_NAME_MAXLEN		31

#define AMIO_MEDIA_NONE			0x0
#define AMIO_MEDIA_AUDIO		0x1
#define AMIO_MEDIA_VIDEO		0x2
#define AMIO_MEDIA_ANY			0x3

#define TRUE  1
#define FALSE 0

struct amio_port_info
{
	char		name[AMIO_PORT_NAME_MAXLEN + 1];
	__u8		index;
	__u8		input;
	__u8		output;
};

struct amio_connect_ports
{
	__u8		input;
	__u8		output;
};

struct amio_connect_status
{
	__u8		input;
	__u8		output;
	__u8		connected;
};

struct amio_port_section
{
	__u8		index;
	__u8		input;
	__u8		output;
};

struct amio_port_status
{
	__u8		index;
	__u8		input;
	__u8		output;
	__u8		disabled;
	__u8		muted;
};

struct amio_gain_mode
{
	__u8		index;
	__u8		input;
	__u8		output;
	__u8		adjustable;
};

struct amio_gain
{
	__u8		index;
	__u8		input;
	__u8		output;
	__s8		gain;
};

struct amio_gain_range
{
	__u8		index;
	__u8		input;
	__u8		output;
	__u8		adjustable;
	__s8		gain;
	__s8		min;
	__s8		max;
	__u8		positions;
	__u8		master;
};

enum e_amio_vmode {
	AMIO_VIDEO_AUTO= 0x1,
	AMIO_VIDEO_CVBS= 0x2,		/* Composite video */
	AMIO_VIDEO_YC=   0x4,			/* Separate video (S-Video) */
	AMIO_VIDEO_YPBPR=0x8,		/* Y-Pb-Pr component video */
	AMIO_VIDEO_RGB=  0x10,			/* RGB component video */
};

struct amio_medium_mode
{
	__u8		index;
	__u8		output;
	__u8		mode;
};

enum e_amio_slowbg {
	AMIO_SLOWBG_INTERNAL,
	AMIO_SLOWBG_EXTERNAL_4_3,
	AMIO_SLOWBG_EXTERNAL_16_9,
};

struct amio_set_slowbg
{
	__u8		index;
	__u8		output;
	__u8		status;
};

struct amio_get_slowbg
{
	__u8		index;
	__u8		status;
};

enum e_amio_port {
	AMIO_PORT_NONE		= 0,
	AMIO_PORT_STB		= 1,
	AMIO_PORT_TV		= 2,
	AMIO_PORT_COUNT		= 2,
};

/* IOCTL: command codes */
#define AMIO_IOC_MAGIC          0xB3  /* TODO register the ioctl magic number
                                       * in Documentation/ioctl-number.txt */

#define AMIO_CGET_API_VERSION _IOR(AMIO_IOC_MAGIC, 1, __u32)
#define AMIO_CGET_PORT_INFO \
		_IOWR(AMIO_IOC_MAGIC, 2, struct amio_port_info)
#define AMIO_CSET_CONNECT_PORTS \
		_IOW(AMIO_IOC_MAGIC, 3, struct amio_connect_ports)
#define AMIO_CSET_DISCONNECT_PORTS \
		_IOW(AMIO_IOC_MAGIC, 4, struct amio_connect_ports)
#define AMIO_CGET_CONNECT_STATUS \
		_IOWR(AMIO_IOC_MAGIC, 5, struct amio_connect_status)
#define AMIO_CSET_ENABLE_PORT \
		_IOWR(AMIO_IOC_MAGIC, 6, struct amio_port_section)
#define AMIO_CSET_DISABLE_PORT \
		_IOWR(AMIO_IOC_MAGIC, 7, struct amio_port_section)
#define AMIO_CSET_UNMUTE_PORT \
		_IOW(AMIO_IOC_MAGIC, 8, struct amio_port_section)
#define AMIO_CSET_MUTE_PORT \
		_IOW(AMIO_IOC_MAGIC, 9, struct amio_port_section)
#define AMIO_CGET_PORT_STATUS \
		_IOWR(AMIO_IOC_MAGIC, 10, struct amio_port_status)
#define AMIO_CSET_GAIN_MODE \
		_IOW(AMIO_IOC_MAGIC, 11, struct amio_gain_mode)
#define AMIO_CSET_GAIN \
		_IOWR(AMIO_IOC_MAGIC, 12, struct amio_gain)
#define AMIO_CGET_GAIN \
		_IOWR(AMIO_IOC_MAGIC, 13, struct amio_gain_range)
#define AMIO_CSET_VIDEO_MODE \
		_IOW(AMIO_IOC_MAGIC, 16, struct amio_medium_mode)
#define AMIO_CGET_VIDEO_MODE \
		_IOWR(AMIO_IOC_MAGIC, 17, struct amio_medium_mode)
#define AMIO_CSET_SLOW_BLANKING \
		_IOW(AMIO_IOC_MAGIC, 19, struct amio_set_slowbg)
#define AMIO_CGET_SLOW_BLANKING \
		_IOW(AMIO_IOC_MAGIC, 20, struct amio_get_slowbg)

//extern int stv6430_command(struct stv6430_handle *handle,unsigned int cmd,void*arg);
extern int stv6430_command( void*handle,unsigned int cmd,void*arg);

#endif /* _WYAAVIO_H_ */
