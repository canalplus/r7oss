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

#ifndef CAPTURE_TEST_DISPLAY_H
#define CAPTURE_TEST_DISPLAY_H

#include <stm_display_types.h>
#include <stm_pixel_capture.h>

struct stm_display_io_windows {
  stm_rect_t    input_window;
  stm_rect_t    output_window;
};

struct stm_capture_display_context {
  stm_display_device_h        hDev;
  stm_display_output_h        hOutput;
  stm_display_plane_h         hPlane;
  stm_display_source_h        hSource;
  stm_display_source_queue_h  hQueue;

  struct stm_display_io_windows      io_windows;

  wait_queue_head_t           frameupdate;
  uint32_t                    timingID;
  int                         tunneling;
};

int setup_main_display(uint32_t timingID, stm_pixel_capture_input_params_t *InputParams,
                                  struct stm_capture_display_context      **pDisplayContext,
                                  struct stm_display_io_windows            io_windows,
                                  char *plane_name,
                                  int main_to_aux,
                                  int stream_inj_ena,
                                  int tunneling);

int display_fill_buffer_descriptor(stm_display_buffer_t * pDisplayBuffer,
                            stm_pixel_capture_buffer_descr_t CaptureBuffer,
                            stm_pixel_capture_input_params_t InputParams);

void free_main_display_ressources(struct stm_capture_display_context       *pDisplayContext);

#endif /* CAPTURE_TEST_DISPLAY_H */
