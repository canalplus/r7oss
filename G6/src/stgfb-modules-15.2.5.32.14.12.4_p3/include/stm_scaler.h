/***********************************************************************
 *
 * File: include/stm_scaler.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_SCALER_H
#define STM_SCALER_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct stm_scaler_s *stm_scaler_h;

typedef long long stm_scaler_time_t;

typedef struct  stm_scaler_rect_s
{
    int32_t      x;
    int32_t      y;
    uint32_t     width;
    uint32_t     height;
} stm_scaler_rect_t;

typedef enum stm_scaler_color_space_e
{
    STM_SCALER_COLORSPACE_RGB,
    STM_SCALER_COLORSPACE_RGB_VIDEORANGE,
    STM_SCALER_COLORSPACE_BT601,
    STM_SCALER_COLORSPACE_BT601_FULLRANGE,
    STM_SCALER_COLORSPACE_BT709,
    STM_SCALER_COLORSPACE_BT709_FULLRANGE
} stm_scaler_color_space_t;

typedef enum stm_scaler_buffer_flags_e
{
    STM_SCALER_BUFFER_INTERLACED = (1L<<0),
    STM_SCALER_BUFFER_TOP        = (1L<<1),
    STM_SCALER_BUFFER_BOTTOM     = (1L<<2)
} stm_scaler_buffer_flags_t;

typedef enum stm_scaler_flags_e
{
    STM_SCALER_FLAG_TNR = (1L<<0),
} stm_scaler_flags_t;

typedef enum stm_scaler_capabilities_e
{
    STM_SCALER_CAPS_NONE =       0,
    STM_SCALER_CAPS_TNR  = (1L<<0),
} stm_scaler_capabilities_t;

typedef enum stm_scaler_buffer_format_e
{
    STM_SCALER_BUFFER_RGB888,
    STM_SCALER_BUFFER_YUV420_NV12,
    STM_SCALER_BUFFER_YUV422_NV16,
} stm_scaler_buffer_format_t;

typedef struct stm_scaler_buffer_descr_s
{
    void                           *user_data;
    uint32_t                        width;
    uint32_t                        height;
    uint32_t                        stride;
    uint32_t                        size;
    uint32_t                        buffer_flags;                                         /*!< Or'd stm_scaler_buffer_flags_t values. */
    stm_scaler_color_space_t        color_space;
    stm_scaler_buffer_format_t      format;
    union
    {
        uint32_t                    rgb_address;
        struct
        {
            uint32_t                luma_address;
            uint32_t                chroma_offset;
        };
    };
} stm_scaler_buffer_descr_t;

typedef struct stm_scaler_config_s
{
    void    (*scaler_task_done_callback)         (void *output_buffer_user_data,
                                                  stm_scaler_time_t time, bool status);   /* Callback issued when the scaling task is completed.
                                                                                             The scaler provides the task buffer descriptor (user_data pointer),
                                                                                             and the time when the task was completed and a status indicating
                                                                                             if rescaled buffer is valid or not. */
    void    (*scaler_input_buffer_done_callback) (void *input_buffer_user_data);          /* Callback issued when the input buffer is no longer needed for any
                                                                                             future temporal processing. */
    void    (*scaler_output_buffer_done_callback)(void *output_buffer_user_data);         /* Callback issued when the output buffer is no longer needed for any
                                                                                             future temporal processing. */
} stm_scaler_config_t;


typedef struct stm_scaler_task_desc_s
{
    uint32_t                            scaler_flags;      /*!< Or'd stm_scaler_flags_t values. */
    stm_scaler_time_t                   due_time;          /* The time when the task is expected to be completed. */
    stm_scaler_buffer_descr_t           input;             /* The input buffer descriptor. */
    stm_scaler_rect_t                   input_crop_area;   /* Crop area within the input buffer. */
    stm_scaler_buffer_descr_t           output;            /* The output buffer descriptor. */
    stm_scaler_rect_t                   output_image_area; /* Video area of the output image. */
} stm_scaler_task_descr_t;

/******************************************************************************************************/

/* Scaler device handle is opened separately for every new stream on which scaling has to be performed */
extern int  stm_scaler_open             (const uint32_t id, stm_scaler_h *scaler_handle, const stm_scaler_config_t *scaler_config);
extern int  stm_scaler_close            (const stm_scaler_h scaler_handle );
extern int  stm_scaler_get_capabilities (const stm_scaler_h scaler_handle, uint32_t *capabilities); /* Or'd stm_scaler_capabilities_t value */
extern int  stm_scaler_dispatch         (const stm_scaler_h scaler_handle, const stm_scaler_task_descr_t *task_descr);
extern int  stm_scaler_abort            (const stm_scaler_h scaler_handle );

#if defined(__cplusplus)
}
#endif

#endif /* STM_SCALER_H */
