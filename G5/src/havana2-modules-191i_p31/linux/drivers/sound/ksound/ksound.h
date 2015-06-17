/*
 * ALSA library for kernel clients (ksound)
 * 
 * Copyright (c) 2007 STMicroelectronics R&D Limited <daniel.thompson@st.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SOUND_KSOUND_H_
#define _SOUND_KSOUND_H_

#ifdef __cplusplus
extern "C" {
	
typedef unsigned long snd_pcm_uframes_t;
typedef signed long snd_pcm_sframes_t;

#define SNDRV_PCM_STREAM_PLAYBACK 0
#define SNDRV_PCM_STREAM_CAPTURE  1
#define SNDRV_PCM_STREAM_LAST     SNDRV_PCM_STREAM_CAPTURE

typedef int snd_pcm_state_t;
#define	SNDRV_PCM_STATE_OPEN		((snd_pcm_state_t) 0) /* stream is open */
#define	SNDRV_PCM_STATE_SETUP		((snd_pcm_state_t) 1) /* stream has a setup */
#define	SNDRV_PCM_STATE_PREPARED	((snd_pcm_state_t) 2) /* stream is ready to start */
#define	SNDRV_PCM_STATE_RUNNING		((snd_pcm_state_t) 3) /* stream is running */
#define	SNDRV_PCM_STATE_XRUN		((snd_pcm_state_t) 4) /* stream reached an xrun */
#define	SNDRV_PCM_STATE_DRAINING	((snd_pcm_state_t) 5) /* stream is draining */
#define	SNDRV_PCM_STATE_PAUSED		((snd_pcm_state_t) 6) /* stream is paused */
#define	SNDRV_PCM_STATE_SUSPENDED	((snd_pcm_state_t) 7) /* hardware is suspended */
#define	SNDRV_PCM_STATE_DISCONNECTED	((snd_pcm_state_t) 8) /* hardware is disconnected */
#define	SNDRV_PCM_STATE_LAST		SNDRV_PCM_STATE_DISCONNECTED
//
//#define __bitwise__ 
//typedef int __bitwise snd_ctl_elem_type_t;
typedef int snd_ctl_elem_type_t;
#define	SNDRV_CTL_ELEM_TYPE_NONE	((snd_ctl_elem_type_t) 0) /* invalid */
#define	SNDRV_CTL_ELEM_TYPE_BOOLEAN	((snd_ctl_elem_type_t) 1) /* boolean type */
#define	SNDRV_CTL_ELEM_TYPE_INTEGER	((snd_ctl_elem_type_t) 2) /* integer type */
#define	SNDRV_CTL_ELEM_TYPE_ENUMERATED	((snd_ctl_elem_type_t) 3) /* enumerated type */
#define	SNDRV_CTL_ELEM_TYPE_BYTES	((snd_ctl_elem_type_t) 4) /* byte array */
#define	SNDRV_CTL_ELEM_TYPE_IEC958	((snd_ctl_elem_type_t) 5) /* IEC958 (S/PDIF) setup */
#define	SNDRV_CTL_ELEM_TYPE_INTEGER64	((snd_ctl_elem_type_t) 6) /* 64-bit integer type */
#define	SNDRV_CTL_ELEM_TYPE_LAST	SNDRV_CTL_ELEM_TYPE_INTEGER64

//typedef int __bitwise snd_ctl_elem_iface_t;
typedef int snd_ctl_elem_iface_t;
#define	SNDRV_CTL_ELEM_IFACE_CARD	((snd_ctl_elem_iface_t) 0) /* global control */
#define	SNDRV_CTL_ELEM_IFACE_HWDEP	((snd_ctl_elem_iface_t) 1) /* hardware dependent device */
#define	SNDRV_CTL_ELEM_IFACE_MIXER	((snd_ctl_elem_iface_t) 2) /* virtual mixer device */
#define	SNDRV_CTL_ELEM_IFACE_PCM	((snd_ctl_elem_iface_t) 3) /* PCM device */
#define	SNDRV_CTL_ELEM_IFACE_RAWMIDI	((snd_ctl_elem_iface_t) 4) /* RawMidi device */
#define	SNDRV_CTL_ELEM_IFACE_TIMER	((snd_ctl_elem_iface_t) 5) /* timer device */
#define	SNDRV_CTL_ELEM_IFACE_SEQUENCER	((snd_ctl_elem_iface_t) 6) /* sequencer client */
#define	SNDRV_CTL_ELEM_IFACE_LAST	SNDRV_CTL_ELEM_IFACE_SEQUENCER

struct snd_ctl_elem_id {
	unsigned int numid;		/* numeric identifier, zero = invalid */
	snd_ctl_elem_iface_t iface;	/* interface identifier */
	unsigned int device;		/* device/client number */
	unsigned int subdevice;		/* subdevice (substream) number */
        unsigned char name[44];		/* ASCII name of item */
	unsigned int index;		/* index of item */
};
typedef struct snd_ctl_elem_id snd_ctl_elem_id_t;
//
struct snd_aes_iec958 {
	unsigned char status[24];	/* AES/IEC958 channel status bits */
	unsigned char subcode[147];	/* AES/IEC958 subcode bits */
	unsigned char pad;		/* nothing */
	unsigned char dig_subframe[4];	/* AES/IEC958 subframe bits */
};
typedef long __kernel_time_t;
typedef __kernel_time_t		time_t;
#ifndef _TIME_H
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};
#endif
struct snd_ctl_elem_value {
	struct snd_ctl_elem_id id;	/* W: element ID */
	unsigned int indirect: 1;	/* W: use indirect pointer (xxx_ptr member) */
        union {
		union {
			long value[128];
			long *value_ptr;
		} integer;
		union {
			long long value[64];
			long long *value_ptr;
		} integer64;
		union {
			unsigned int item[128];
			unsigned int *item_ptr;
		} enumerated;
		union {
			unsigned char data[512];
			unsigned char *data_ptr;
		} bytes;
		struct snd_aes_iec958 iec958;
        } value;                /* RO */
	struct timespec tstamp;
        unsigned char reserved[128-sizeof(struct timespec)];
};
typedef struct snd_ctl_elem_value snd_ctl_elem_value_t;
//
struct snd_kcontrol;
typedef int (snd_kcontrol_info_t) (struct snd_kcontrol * kcontrol, struct snd_ctl_elem_info * uinfo);
typedef int (snd_kcontrol_get_t) (struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol);
typedef int (snd_kcontrol_put_t) (struct snd_kcontrol * kcontrol, struct snd_ctl_elem_value * ucontrol);


typedef struct snd_ctl_file { int dummy; };
typedef int pid_t;

struct snd_kcontrol_volatile {
	struct snd_ctl_file *owner;	/* locked */
	pid_t owner_pid;
	unsigned int access;	/* access rights */
};
struct list_head {
	struct list_head *next, *prev;
};
struct snd_kcontrol {
	struct list_head list;		/* list of controls */
	struct snd_ctl_elem_id id;
	unsigned int count;		/* count of same elements */
	snd_kcontrol_info_t *info;
	snd_kcontrol_get_t *get;
	snd_kcontrol_put_t *put;
	unsigned long private_value;
	void *private_data;
	void (*private_free)(struct snd_kcontrol *kcontrol);
	struct snd_kcontrol_volatile vd[0];	/* volatile data */
};
typedef struct snd_kcontrol snd_kcontrol_t;
struct snd_pcm_substream { int dummy; };


/************* sound/typedef.h ************/
typedef struct snd_pcm_substream snd_pcm_substream_t;
/******************************************/

/**** asound.h *****/
typedef unsigned long snd_pcm_uframes_t;

//typedef int __bitwise snd_pcm_hw_param_t;
typedef int snd_pcm_hw_param_t;
#define	SNDRV_PCM_HW_PARAM_ACCESS	((snd_pcm_hw_param_t) 0) /* Access type */
#define	SNDRV_PCM_HW_PARAM_FORMAT	((snd_pcm_hw_param_t) 1) /* Format */
#define	SNDRV_PCM_HW_PARAM_SUBFORMAT	((snd_pcm_hw_param_t) 2) /* Subformat */
#define	SNDRV_PCM_HW_PARAM_FIRST_MASK	SNDRV_PCM_HW_PARAM_ACCESS
#define	SNDRV_PCM_HW_PARAM_LAST_MASK	SNDRV_PCM_HW_PARAM_SUBFORMAT

#define	SNDRV_PCM_HW_PARAM_SAMPLE_BITS	((snd_pcm_hw_param_t) 8) /* Bits per sample */
#define	SNDRV_PCM_HW_PARAM_FRAME_BITS	((snd_pcm_hw_param_t) 9) /* Bits per frame */
#define	SNDRV_PCM_HW_PARAM_CHANNELS	((snd_pcm_hw_param_t) 10) /* Channels */
#define	SNDRV_PCM_HW_PARAM_RATE		((snd_pcm_hw_param_t) 11) /* Approx rate */
#define	SNDRV_PCM_HW_PARAM_PERIOD_TIME	((snd_pcm_hw_param_t) 12) /* Approx distance between interrupts in us */
#define	SNDRV_PCM_HW_PARAM_PERIOD_SIZE	((snd_pcm_hw_param_t) 13) /* Approx frames between interrupts */
#define	SNDRV_PCM_HW_PARAM_PERIOD_BYTES	((snd_pcm_hw_param_t) 14) /* Approx bytes between interrupts */
#define	SNDRV_PCM_HW_PARAM_PERIODS	((snd_pcm_hw_param_t) 15) /* Approx interrupts per buffer */
#define	SNDRV_PCM_HW_PARAM_BUFFER_TIME	((snd_pcm_hw_param_t) 16) /* Approx duration of buffer in us */
#define	SNDRV_PCM_HW_PARAM_BUFFER_SIZE	((snd_pcm_hw_param_t) 17) /* Size of buffer in frames */
#define	SNDRV_PCM_HW_PARAM_BUFFER_BYTES	((snd_pcm_hw_param_t) 18) /* Size of buffer in bytes */
#define	SNDRV_PCM_HW_PARAM_TICK_TIME	((snd_pcm_hw_param_t) 19) /* Approx tick duration in us */
#define	SNDRV_PCM_HW_PARAM_FIRST_INTERVAL	SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define	SNDRV_PCM_HW_PARAM_LAST_INTERVAL	SNDRV_PCM_HW_PARAM_TICK_TIME

typedef unsigned int u_int32_t;

struct snd_interval {
	unsigned int min, max;
	unsigned int openmin:1,
		     openmax:1,
		     integer:1,
		     empty:1;
};

#define SNDRV_MASK_MAX	256

struct snd_mask {
	u_int32_t bits[(SNDRV_MASK_MAX+31)/32];
};

struct snd_pcm_hw_params {
	unsigned int flags;
	struct snd_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK - 
			       SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
	struct snd_mask mres[5];	/* reserved masks */
	struct snd_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
				        SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
	struct snd_interval ires[9];	/* reserved intervals */
	unsigned int rmask;		/* W: requested masks */
	unsigned int cmask;		/* R: changed masks */
	unsigned int info;		/* R: Info flags for returned setup */
	unsigned int msbits;		/* R: used most significant bits */
	unsigned int rate_num;		/* R: rate numerator */
	unsigned int rate_den;		/* R: rate denominator */
	snd_pcm_uframes_t fifo_size;	/* R: chip FIFO size in frames */
	unsigned char reserved[64];	/* reserved for future */
};
/*******************/

//
//#warning KSOUND.H AS CPP INCLUDE
#else
//#warning KSOUND.H AS C INCLUDE

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/minors.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <linux/soundcard.h>
#include <sound/initval.h>
#include <sound/asoundef.h>
#include <sound/control.h>
//#include <sound/typedefs.h>
#include <sound/asound.h>

#if defined (CONFIG_KERNELVERSION)    
typedef struct snd_kcontrol snd_kcontrol_t;
typedef struct snd_ctl_elem_value snd_ctl_elem_value_t;
typedef struct snd_ctl_elem_id snd_ctl_elem_id_t;
/************* sound/typedef.h ************/
typedef struct snd_pcm_substream snd_pcm_substream_t;
/******************************************/
#endif     
    
#endif

typedef struct _ksnd_pcm ksnd_pcm_t;
typedef struct ksnd_pcm_streaming * ksnd_pcm_streaming_t;
typedef struct snd_pcm_hw_params ksnd_pcm_hw_params_t;


typedef int snd_pcm_stream_t;
#define SND_PCM_STREAM_PLAYBACK SNDRV_PCM_STREAM_PLAYBACK
#define SND_PCM_STREAM_CAPTURE  SNDRV_PCM_STREAM_CAPTURE
#define SND_PCM_STREAM_LAST     SNDRV_PCM_STREAM_LAST

/* TODO: ksnd? */
typedef struct _snd_pcm_channel_area {
	/** base address of channel samples */
	void *addr;
	/** offset to first sample in bits */
	unsigned int first;
	/** samples distance in bits */
	unsigned int step;
	/** playback/capture stream */
	int stream;
} snd_pcm_channel_area_t;

struct _ksnd_pcm {
	snd_pcm_substream_t *substream;
	snd_pcm_channel_area_t hwareas[10];
	ksnd_pcm_hw_params_t actual_hwparams;
	ksnd_pcm_streaming_t streaming;
};

int ksnd_pcm_open(ksnd_pcm_t **handle, int card, int device,
			snd_pcm_stream_t stream);
void ksnd_pcm_close(ksnd_pcm_t *handle);
int ksnd_pcm_delay(ksnd_pcm_t *handle, snd_pcm_sframes_t *delay);
int ksnd_pcm_get_samplerate(ksnd_pcm_t *substream);

int ksnd_pcm_start(ksnd_pcm_t *substream);
/* TODO: ksnd_pcm_stop not found in alsalib */
int ksnd_pcm_stop(ksnd_pcm_t *handle);
int ksnd_pcm_pause(ksnd_pcm_t *handle, unsigned int push);
int ksnd_pcm_mute(ksnd_pcm_t *handle, unsigned int push);
int ksnd_pcm_prepare(ksnd_pcm_t *substream);
int ksnd_pcm_drain(ksnd_pcm_t *substream);

snd_pcm_state_t ksnd_pcm_state(ksnd_pcm_t *pcm);
int ksnd_pcm_htimestamp(ksnd_pcm_t *pcm, snd_pcm_uframes_t *avail, struct timespec *tstamp);
int ksnd_pcm_mtimestamp(ksnd_pcm_t *pcm, snd_pcm_uframes_t *avail, struct timespec *mstamp);
snd_pcm_uframes_t ksnd_pcm_avail_update(ksnd_pcm_t *substream);
int ksnd_pcm_wait(ksnd_pcm_t *pcm, int timeout);
int ksnd_pcm_writei(ksnd_pcm_t *handle,
		    int *data, unsigned int size, unsigned int srcchannels);
#define ksnd_pcm_mmap_writei(h, d, s, sc) ksnd_pcm_writei(h, d, s, sc)

/* TODO: ksnd_pcm_readi 
#define ksnd_pcm_mmap_readi(h, d, s, sc) ksnd_pcm_readi(h, d, s, sc)
*/

int ksnd_pcm_mmap_begin(ksnd_pcm_t *pcm, const snd_pcm_channel_area_t **areas,
			snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t ksnd_pcm_mmap_commit(ksnd_pcm_t *pcm,
				       snd_pcm_uframes_t offset,
				       snd_pcm_uframes_t frames);

int ksnd_pcm_hw_params(ksnd_pcm_t *kpcm, ksnd_pcm_hw_params_t *params);
int ksnd_pcm_hw_params_any(ksnd_pcm_t *kpcm, ksnd_pcm_hw_params_t *params);

/*
 * application helpers
 */

/* TODO: ksnd_pcm_set_params differs significantly from snd_pcm_set_params */
int ksnd_pcm_set_params(ksnd_pcm_t *handle,
                       int nrchannels, int sampledepth, int samplerate,
                       int periodsize, int buffersize);
int ksnd_pcm_get_params(ksnd_pcm_t *kpcm,
                        snd_pcm_uframes_t *buffer_size,
                        snd_pcm_uframes_t *period_size);

/*
 * accessors for h/ware parameters
 */

int ksnd_pcm_hw_params_malloc(ksnd_pcm_hw_params_t **ptr);
void ksnd_pcm_hw_params_free(ksnd_pcm_hw_params_t *obj);

int ksnd_pcm_hw_params_get_period_size(const ksnd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);

int ksnd_pcm_hw_params_get_buffer_size(const ksnd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);


/*
 * control functions
 */
void ksnd_ctl_elem_id_alloca(snd_ctl_elem_id_t **id);
void ksnd_ctl_elem_id_set_interface(snd_ctl_elem_id_t *obj, snd_ctl_elem_iface_t val);
void ksnd_ctl_elem_id_set_name(snd_ctl_elem_id_t *obj, const char *val);
void ksnd_ctl_elem_id_set_device(snd_ctl_elem_id_t *obj, unsigned int val);
void ksnd_ctl_elem_id_set_index(snd_ctl_elem_id_t *obj, unsigned int val);
snd_kcontrol_t *ksnd_substream_find_elem(snd_pcm_substream_t *substream, snd_ctl_elem_id_t *id);
void ksnd_ctl_elem_value_alloca(snd_ctl_elem_value_t **id);
void ksnd_ctl_elem_value_set_id(snd_ctl_elem_value_t *obj, const snd_ctl_elem_id_t *ptr);
void ksnd_ctl_elem_value_set_integer(snd_ctl_elem_value_t *obj, unsigned int idx, long val);
void ksnd_ctl_elem_value_set_iec958(snd_ctl_elem_value_t *obj, const struct snd_aes_iec958 *ptr);
int ksnd_hctl_elem_write(snd_kcontrol_t *elem, snd_ctl_elem_value_t *control);


/*
 * capture -> playback stream redirection
 * both handles shall be previously open and set up (possibly with the same parameters)
 */
int ksnd_pcm_streaming_start(ksnd_pcm_streaming_t *handle, ksnd_pcm_t *capture, ksnd_pcm_t *playback);
void ksnd_pcm_streaming_stop(ksnd_pcm_streaming_t handle);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _SOUND_KSOUND_H_ */
