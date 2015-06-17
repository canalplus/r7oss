/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of v4l2 audio encoder device
************************************************************************/
#ifndef STM_V4L2_ENCODE_AUDIO_H
#define STM_V4L2_ENCODE_AUDIO_H

typedef struct stm_v4l2_audenc_device {

	stm_se_capture_buffer_t src_metadata;

	struct v4l2_buffer src_v4l2_buf;
	struct v4l2_buffer dst_v4l2_buf;

	int file_flag;

} stm_v4l2_audenc_device_t;

#define STM_SE_AUDENC_MAX_CHANNELS 32

typedef struct audio_encode_params_s {
	int src_codec;
	int codec;

	int is_vbr;
	int bitrate;
	int vbr_quality_factor;
	int bitrate_cap;

	int sample_rate;
	int channel_count;
	int channel[STM_SE_AUDENC_MAX_CHANNELS];
} audio_encode_params_t;

int stm_v4l2_encoder_aud_init_subdev(void *dev);
int stm_v4l2_encoder_aud_exit_subdev(void *dev);

int  stm_v4l2_encoder_aud_queue_init(int type, struct vb2_queue *vq, void *priv);
void stm_v4l2_encoder_aud_queue_release(struct vb2_queue *vq);

/* audio ioctls */
int stm_v4l2_encoder_enum_aud_output(struct v4l2_audioout *output,
				void *dev, int index);

int stm_v4l2_encoder_enum_aud_input(struct v4l2_audio *input,
				void *dev, int index);

int stm_v4l2_encoder_aud_enum_fmt(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f);

int stm_v4l2_encoder_aud_try_fmt(struct file *file, void *fh, struct v4l2_format *f);

int stm_v4l2_encoder_aud_s_fmt(struct file *file, void *fh, struct v4l2_format *f);

int stm_v4l2_encoder_aud_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *param_p);

int stm_v4l2_encoder_aud_reqbufs(struct file *file, void *fh,
			  struct v4l2_requestbuffers *reqbufs);

int stm_v4l2_encoder_aud_querybuf(struct file *file, void *fh,
			   struct v4l2_buffer *buf);

int stm_v4l2_encoder_aud_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf);

int stm_v4l2_encoder_aud_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf);

int stm_v4l2_encoder_aud_streamon(struct file *file, void *fh,
			   enum v4l2_buf_type type);

int stm_v4l2_encoder_aud_streamoff(struct file *file, void *fh,
			    enum v4l2_buf_type type);

int stm_v4l2_encoder_aud_mmap(struct file *file, struct vm_area_struct *vma);

unsigned int stm_v4l2_aud_encoder_poll(struct file *file,
								struct poll_table_struct *wait);

int stm_v4l2_encoder_aud_close(struct file *file);

int stm_v4l2_encoder_aud_create_connection(void);

#endif
