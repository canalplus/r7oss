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
 * Implementation of v4l2 video encoder device
************************************************************************/
#ifndef STM_V4L2_ENCODE_VIDEO_H
#define STM_V4L2_ENCODE_VIDEO_H

#define STM_V4L2_ENCODE_CIF_HEIGHT    352
#define STM_V4L2_ENCODE_CIF_WIDTH     288
#define STM_V4L2_ENCODE_SD_HEIGHT     720
#define STM_V4L2_ENCODE_SD_WIDTH      576
#define STM_V4L2_ENCODE_720P_HEIGHT  1280
#define STM_V4L2_ENCODE_720P_WIDTH    720
#define STM_V4L2_ENCODE_HD_HEIGHT    1920
#define STM_V4L2_ENCODE_HD_WIDTH     1088

/*Max video coded frame header size in bytes*/
#define STM_V4L2_ENCODE_VIDEO_MAX_HEADER_SIZE  200

typedef struct stm_v4l2_viddenc_device {

	stm_se_capture_buffer_t src_metadata;

	struct v4l2_buffer src_v4l2_buf;
	struct v4l2_buffer dst_v4l2_buf;

	int file_flag;

} stm_v4l2_videnc_device_t;

/*
frame width in pixel shouldn't change within one sequence.
if it is changed, a new sequence should be triggered
*/
typedef struct video_encode_params_s {

	unsigned int width;
	unsigned int height;
	unsigned int input_framerate_num;
	unsigned int input_framerate_den;
	unsigned int surface_format;
	unsigned int pitch;
	unsigned int vertical_alignment;
	unsigned int deinterlace;
	unsigned int scan_type;
	unsigned int top_field_first;
	unsigned int noise_filter;
	unsigned int colorspace;

} video_encode_params_t;


int stm_v4l2_encoder_vid_init_subdev(void *dev);
int stm_v4l2_encoder_vid_exit_subdev(void *dev);

int  stm_v4l2_encoder_vid_queue_init(int type, struct vb2_queue *vq, void *priv);
void stm_v4l2_encoder_vid_queue_release(struct vb2_queue *vq);

int stm_v4l2_encode_convert_resolution_to_profile(int width, int height);

/* video ioctls */

int stm_v4l2_encoder_enum_vid_output(struct v4l2_output *output,
				     void *dev, int index);

int stm_v4l2_encoder_enum_vid_input(struct v4l2_input *input,
				    void *dev, int index);

int stm_v4l2_encoder_vid_enum_fmt(struct file *file, void *fh,
				  struct v4l2_fmtdesc *f);

int stm_v4l2_encoder_vid_try_fmt(struct file *file, void *fh,
				 struct v4l2_format *f);

int stm_v4l2_encoder_vid_s_fmt(struct file *file, void *fh,
			       struct v4l2_format *f);

int stm_v4l2_encoder_vid_s_parm(struct file *file, void *fh,
				struct v4l2_streamparm *param_p);

int stm_v4l2_encoder_vid_reqbufs(struct file *file, void *fh,
				 struct v4l2_requestbuffers *reqbufs);

int stm_v4l2_encoder_vid_querybuf(struct file *file, void *fh,
				  struct v4l2_buffer *buf);

int stm_v4l2_encoder_vid_qbuf(struct file *file, void *fh,
			      struct v4l2_buffer *buf);

int stm_v4l2_encoder_vid_dqbuf(struct file *file, void *fh,
			       struct v4l2_buffer *buf);

int stm_v4l2_encoder_vid_streamon(struct file *file, void *fh,
				  enum v4l2_buf_type type);

int stm_v4l2_encoder_vid_streamoff(struct file *file, void *fh,
				   enum v4l2_buf_type type);

int stm_v4l2_encoder_vid_mmap(struct file *file, struct vm_area_struct *vma);

unsigned int stm_v4l2_vid_encoder_poll(struct file *file,
				       struct poll_table_struct *wait);

int stm_v4l2_encoder_vid_close(struct file *file);

int stm_v4l2_encoder_vid_create_connection(void);

#endif
