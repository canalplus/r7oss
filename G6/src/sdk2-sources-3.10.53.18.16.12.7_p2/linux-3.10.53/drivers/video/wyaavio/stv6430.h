/*
 * Linux Kernel device driver for STV6430
 *
 * (C) Copyright 2007-2012 WyPlay SAS.
 * Frederic Mazuel <fmazuel@wyplay.com>
 */
#ifndef _STV6430_H_
#define _STV6430_H_
#ifdef __KERNEL__

#include <linux/types.h>

/* ----------------------------------------------------------------------- */

#define STV6430_DRV_NAME "stv6430"
#define DRV_VERSION "0.2"
#define STV6430_PORT_NAME_MAXLEN 31

enum e_stv6430_port {
	STV6430_PORT_NONE = 0,
	STV6430_PORT_TV = 1, /* SCART and composite */
//	STV6430_PORT_YPBPR = 2, /* YPbPr */
	STV6430_PORT_COUNT = 2,
};

enum e_stv6430_command {
	STV6430_CGET_API_VERSION,
	STV6430_CGET_PORT_INFO,
	STV6430_CSET_CONNECT_PORTS,
	STV6430_CSET_DISCONNECT_PORTS,
	STV6430_CGET_CONNECT_STATUS,
	STV6430_CSET_UNMUTE_PORT,
	STV6430_CSET_MUTE_PORT,
	STV6430_CSET_DISABLE_PORT,
	STV6430_CSET_ENABLE_PORT,
	STV6430_CGET_PORT_STATUS,
	STV6430_CSET_GAIN_MODE,
	STV6430_CSET_GAIN,
	STV6430_CGET_GAIN,
	STV6430_CSET_VIDEO_MODE,
	STV6430_CGET_VIDEO_MODE,
	STV6430_CSET_SLOW_BLANKING,
	STV6430_CSET_FAST_BLANKING,
	STV6430_CMD_MAX,
};

enum e_stv6430_media {
	STV6430_MEDIA_NONE = 0,
	STV6430_MEDIA_ANY = 1,
	STV6430_MEDIA_AUDIO = 2,
	STV6430_MEDIA_VIDEO = 3,
};

enum e_stv6430_vmode {
	STV6430_VIDEO_AUTO = 0x1,
	STV6430_VIDEO_CVBS = 0x2, /* Composite video */
	STV6430_VIDEO_YC   = 0x4, /* Separate video (S-Video) */
	STV6430_VIDEO_YPBPR = 0x8, /* Y-Pb-Pr component video */
	STV6430_VIDEO_RGB  = 0x10, /* RGB component video */
};

struct stv6430_medium_mode
{
	uint8_t index;
	uint8_t output;
	uint8_t mode;
};

enum e_stv6430_fastbg {
	STV6430_FASTBG_CVBS,
	STV6430_FASTBG_RGB,
};

enum e_stv6430_slowbg {
	STV6430_SLOWBG_INTERNAL,
	STV6430_SLOWBG_EXTERNAL_4_3,
	STV6430_SLOWBG_EXTERNAL_16_9,
};

struct stv6430_port_info {
	char name[STV6430_PORT_NAME_MAXLEN + 1];
	uint8_t index;
	uint8_t input;
	uint8_t output;
};

struct stv6430_state
{
	uint8_t *regs;
	struct i2c_client *i2cClient;
};

struct stv6430_handle
{
	struct stv6430_state *state;
	unsigned int use_count;
};

struct stv6430_set_fastbg
{
	uint8_t index;
	uint8_t output;
	uint8_t status;
};

struct stv6430_set_slowbg
{
	uint8_t index;
	uint8_t output;
	uint8_t status;
};

#endif /* __KERNEL__ */
#endif /* _STV6430_H_ */
