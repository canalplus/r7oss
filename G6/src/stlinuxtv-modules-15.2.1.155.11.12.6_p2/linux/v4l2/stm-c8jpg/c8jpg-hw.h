/*
 * c8jpg-hw.h, C8JPG1 h/w configuration
 *
 * Copyright (C) 2012, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __STM_C8JPG_OUTPUTSTAGE_H
#define __STM_C8JPG_OUTPUTSTAGE_H

#define SOF0 0xC0
#define SOF1 0xC1
#define SOF5 0xC5
#define JPG 0xC8
#define SOF13 0xCD

#define DHT 0xC4
#define DAC 0xCC

#define SOI 0xD8
#define EOI 0xD9
#define RST 0xD0 /* 8 RSTn markers */
#define TEM 0x01

#define SOS 0xDA
#define DQT 0xDB
#define DNL 0xDC
#define DRI 0xDD
#define DHP 0xDE
#define EXP 0xDF
#define APP0 0xE0 /* 16 APPn markers */
#define COM 0xFE

#define MIN_JPEG_WIDTH 32
#define MAX_JPEG_WIDTH 65536
#define MAX_JPEG_HEIGHT 65536
#define MAX_JPEG_SIZE 16777216
#define MAX_PICTURE_WIDTH 8192
#define MAX_PICTURE_HEIGHT 65536

struct stm_c8jpg_device;

struct c8jpg_chroma_sampling {
	union {
		struct {
			unsigned unused0:4;
			unsigned hcr:4;
			unsigned hcb:4;
			unsigned hy:4;
			unsigned unused1:4;
			unsigned vcr:4;
			unsigned vcb:4;
			unsigned vy:4;
		};
		unsigned int sampling;
	};
};

#define C8JPG_CHROMA_SAMPLING_420 0x21102110
#define C8JPG_CHROMA_SAMPLING_422 0x11102110
#define C8JPG_CHROMA_SAMPLING_444 0x11101110

struct c8jpg_jpeg_buf {
	unsigned char *data;
	unsigned int size;
	unsigned int pos;
};

struct c8jpg_resizer_state {
	struct c8jpg_chroma_sampling scan_in;
	struct c8jpg_chroma_sampling scan_out;
	unsigned int mcu_width;
	unsigned int init_y, init_uv;
	unsigned int incr_y, incr_uv;
	unsigned int vdec_y, vdec_uv;
	int *y_table, *uv_table;
	unsigned int ratio;
};

struct c8jpg_output_info {
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	struct v4l2_rect region;
};

void c8jpg_hw_init(struct stm_c8jpg_device *dev);
void c8jpg_hw_reset(struct stm_c8jpg_device *dev);

void c8jpg_setup_outputstage(struct stm_c8jpg_device *dev,
			     struct c8jpg_resizer_state *state);

void c8jpg_setup_filtering_tables(struct c8jpg_resizer_state *state);
void c8jpg_setup_h_resize(struct stm_c8jpg_device *dev,
			  struct c8jpg_resizer_state *state);
void c8jpg_setup_v_resize(struct stm_c8jpg_device *dev,
			  struct c8jpg_resizer_state *state);

void c8jpg_set_source(struct stm_c8jpg_device *dev,
		      unsigned long addr, unsigned long size);
void c8jpg_set_destination(struct stm_c8jpg_device *dev,
			   unsigned long yaddr, unsigned long uvaddr);

void c8jpg_set_cropping(struct stm_c8jpg_device *dev,
			struct c8jpg_output_info *output);
void c8jpg_set_output_format(struct stm_c8jpg_device *dev,
			     struct c8jpg_resizer_state *state);

void c8jpg_decode_image(struct stm_c8jpg_device *dev);

#endif /* __STM_C8JPG_OUTPUTSTAGE_H */
