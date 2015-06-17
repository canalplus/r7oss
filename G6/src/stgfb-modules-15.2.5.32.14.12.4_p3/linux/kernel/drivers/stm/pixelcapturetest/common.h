/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/common.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef CAPTURE_TEST_COMMON_H
#define CAPTURE_TEST_COMMON_H

#include <stm_display_types.h>
#include <stm_pixel_capture.h>

#define USE_PICTURE_FROM_HEADER_FILE

#ifdef USE_PICTURE_FROM_HEADER_FILE
#define BUFFER_720P_HEADER_SIZE       4
#define BUFFER_720P_RGB_WIDTH         800
#define BUFFER_720P_RGB_HEIGHT        300
#define BUFFER_720P_RGB_BPP           3*2 /* 64 bits per pixel */
#define BUFFER_720P_RGB_SIZE          (BUFFER_720P_RGB_WIDTH * BUFFER_720P_RGB_HEIGHT * BUFFER_720P_RGB_BPP) + BUFFER_720P_HEADER_SIZE
#else /* !USE_PICTURE_FROM_HEADER_FILE */
#define BUFFER_720P_HEADER_SIZE       4
#define BUFFER_720P_RGB_WIDTH         1280
#define BUFFER_720P_RGB_HEIGHT        720
#define BUFFER_720P_RGB_BPP           3*2 /* 64 bits per pixel */
#define BUFFER_720P_RGB_SIZE          (BUFFER_720P_RGB_WIDTH * BUFFER_720P_RGB_HEIGHT * BUFFER_720P_RGB_BPP) + BUFFER_720P_HEADER_SIZE
#endif /* USE_PICTURE_FROM_HEADER_FILE */

//info needed to allocate buffers memory from bpa2
struct stm_capture_dma_area {
  struct bpa2_part      *part;
  unsigned long          base;
  size_t                 size;
  volatile void __iomem *memory;
  bool                   cached;
};


void capture_list_inputs (stm_pixel_capture_h pixel_capture);

int capture_set_input_by_index(stm_pixel_capture_h pixel_capture, uint32_t input_idx);

int capture_set_input_by_name (stm_pixel_capture_h pixel_capture, const char * const name);

int capture_set_format(stm_pixel_capture_h pixel_capture, stm_pixel_capture_buffer_format_t format);

int capture_set_crop(stm_pixel_capture_h pixel_capture, stm_pixel_capture_rect_t input_rectangle);

int allocate_capture_buffers_memory(stm_pixel_capture_h pixel_capture,
                          uint32_t number_buffers,
                          stm_pixel_capture_buffer_format_t format,
                          stm_pixel_capture_buffer_descr_t *buffer_descr,
                          struct stm_capture_dma_area *memory_pool);

int free_capture_buffers_memory(struct stm_capture_dma_area *memory_pool);

void capture_update_display(struct fb_info *info, stm_pixel_capture_buffer_descr_t *buffer_descr);
void capture_pan_display(struct fb_info *info, stm_pixel_capture_buffer_descr_t *buffer_descr);

int capture_get_default_window_rect(stm_pixel_capture_h pixel_capture,
                                          stm_pixel_capture_input_params_t input_params,
                                          stm_pixel_capture_rect_t *rect);

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
int stream_init_pattern_injection(stm_pixel_capture_input_params_t InputParams);
void stream_set_pattern_injection_status(bool start, int loop_count);
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */

#endif /* CAPTURE_TEST_COMMON_H */
