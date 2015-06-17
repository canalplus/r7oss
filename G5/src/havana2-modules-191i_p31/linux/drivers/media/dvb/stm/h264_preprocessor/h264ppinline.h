/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : h264ppinline.h
Author :           Nick

Contains the useful operating system dependent inline functions/types
that allow use of the h264 cabac preprocessor device.

Date        Modification                                    Name
----        ------------                                    --------
30-Nov-05   Created                                         Nick

************************************************************************/

#ifndef H_H264PPINLINE
#define H_H264PPINLINE

/* --- Include the os specific headers we will need --- */

#include "report.h"
#include "osinline.h"
#include "osdev_user.h"
#include "h264.h"
#include "h264pp.h"
#include "h264ppio.h"

/* --- */

typedef enum
{
    h264pp_ok                = 0,
    h264pp_error
} h264pp_status_t;

//

struct h264pp_device_s
{
    OSDEV_DeviceIdentifier_t     UnderlyingDevice;
};

typedef struct h264pp_device_s  *h264pp_device_t;
#define H264_PP_INVALID_DEVICE   NULL

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline h264pp_status_t   H264ppOpen( h264pp_device_t     *Device )
{
OSDEV_Status_t           Status;

//
//  Malloc the device structure
//

    *Device = (h264pp_device_t)OS_Malloc( sizeof(struct h264pp_device_s) );
    if( *Device == NULL )
	return h264pp_error;

    memset( *Device, 0, sizeof(struct h264pp_device_s) );

//
//  Open the device
//

    Status = OSDEV_Open( H264_PP_DEVICE, &((*Device)->UnderlyingDevice) );
    if( Status != OSDEV_NoError )
    {
	report( severity_error, "H264ppOpen : Failed to open\n" );
	OS_Free( *Device );
	*Device = H264_PP_INVALID_DEVICE;
	return h264pp_error;
    }

//

    return h264pp_ok;
}

// ------------------------------------------------------------------------------------------------------------
//   The close function

static inline void              H264ppClose( h264pp_device_t Device )
{
    if( Device != H264_PP_INVALID_DEVICE )
    {
	OSDEV_Close( Device->UnderlyingDevice );
	OS_Free( Device );
    }
}

// ------------------------------------------------------------------------------------------------------------
//   Function to prepare a buffer, and the associated parameters

static inline h264pp_status_t   H264ppProcessBuffer( 	h264pp_device_t       Device,
							H264SliceHeader_t    *Slice,
							unsigned int          QueueIdentifier,
							unsigned int	      InputBufferSize,
							void		     *InputBufferCachedAddress,
							void		     *InputBufferPhysicalAddress,
							void		     *OutputBufferCachedAddress,
							void		     *OutputBufferPhysicalAddress )
{
int				 Status;
h264pp_ioctl_queue_t             Parameters;
unsigned int                     log2_max_pic_order_cnt_lsb;
unsigned int                     log2_max_frame_num;
unsigned int                     picture_width;
unsigned int                     nb_MB_in_picture_minus1;
unsigned int			 monochrome;
unsigned int                     transform_8x8_mode_flag;
unsigned int                     direct_8x8_inference_flag;
unsigned int                     pic_init_qp;
unsigned int                     num_ref_idx_l1_active_minus1;
unsigned int                     num_ref_idx_l0_active_minus1;
unsigned int                     deblocking_filter_control_present_flag;
unsigned int                     weighted_bipred_idc_flag;
unsigned int                     weighted_pred_flag;
unsigned int                     delta_pic_order_always_zero_flag;
unsigned int                     pic_order_present_flag;
unsigned int                     pic_order_cnt_type;
unsigned int                     frame_mbs_only_flag;
unsigned int                     entropy_coding_mode_flag;
unsigned int                     mb_adaptive_frame_field_flag;

    //
    // Is this ok
    //

    if( Device == H264_PP_INVALID_DEVICE )
    {
	report( severity_error, "H264ppProcessBuffer : Device not open.\n" );
	return h264pp_error;
    }

    if( Slice->first_mb_in_slice != 0 )
    {
	report( severity_error, "H264ppProcessBuffer : Attempt to process a buffer with a non-first slice.\n" );
	return h264pp_error;
    }

    //
    // Record the buffer parameters
    //

    memset( &Parameters, 0, sizeof(h264pp_ioctl_queue_t) );

    Parameters.QueueIdentifier			= QueueIdentifier;
    Parameters.InputSize			= InputBufferSize;
    Parameters.InputBufferCachedAddress		= InputBufferCachedAddress;
    Parameters.InputBufferPhysicalAddress	= InputBufferPhysicalAddress;
    Parameters.OutputBufferCachedAddress	= OutputBufferCachedAddress;
    Parameters.OutputBufferPhysicalAddress	= OutputBufferPhysicalAddress;
    Parameters.Field				= Slice->field_pic_flag;

    //
    // Calculate the register values for this data
    //
	
    log2_max_pic_order_cnt_lsb		= Slice->SequenceParameterSet->log2_max_pic_order_cnt_lsb_minus4 + 4;
    log2_max_frame_num			= Slice->SequenceParameterSet->log2_max_frame_num_minus4 + 4;

    Parameters.CodeLength		= (log2_max_pic_order_cnt_lsb   << PP_CODELENGTH__MPOC_SHIFT) |
					  (log2_max_frame_num           << PP_CODELENGTH__MFN_SHIFT);

//

    picture_width			= Slice->SequenceParameterSet->pic_width_in_mbs_minus1 + 1;
    nb_MB_in_picture_minus1		= ((Slice->SequenceParameterSet->pic_height_in_map_units_minus1 + 1) * picture_width) - 1;
    if( !Slice->SequenceParameterSet->frame_mbs_only_flag && !Slice->field_pic_flag )
	nb_MB_in_picture_minus1		= (2 * (nb_MB_in_picture_minus1 + 1)) - 1;

    Parameters.PicWidth			= (nb_MB_in_picture_minus1      << PP_PICWIDTH__MBINPIC_SHIFT) |
					  (picture_width                << PP_PICWIDTH__PICWIDTH_SHIFT);

//

    pic_init_qp					= Slice->PictureParameterSet->pic_init_qp_minus26 + 26;
    num_ref_idx_l1_active_minus1		= Slice->PictureParameterSet->num_ref_idx_l1_active_minus1;
    num_ref_idx_l0_active_minus1		= Slice->PictureParameterSet->num_ref_idx_l0_active_minus1;
    deblocking_filter_control_present_flag	= Slice->PictureParameterSet->deblocking_filter_control_present_flag;
    weighted_bipred_idc_flag			= (Slice->PictureParameterSet->weighted_bipred_idc == 1) ? 1 : 0;
    weighted_pred_flag				= Slice->PictureParameterSet->weighted_pred_flag;
    delta_pic_order_always_zero_flag		= Slice->SequenceParameterSet->delta_pic_order_always_zero_flag;
    pic_order_present_flag			= Slice->PictureParameterSet->pic_order_present_flag;
    pic_order_cnt_type				= Slice->SequenceParameterSet->pic_order_cnt_type;
    frame_mbs_only_flag				= Slice->SequenceParameterSet->frame_mbs_only_flag;
    entropy_coding_mode_flag			= Slice->PictureParameterSet->entropy_coding_mode_flag;
    mb_adaptive_frame_field_flag		= Slice->SequenceParameterSet->mb_adaptive_frame_field_flag;
    transform_8x8_mode_flag			= Slice->PictureParameterSet->transform_8x8_mode_flag;
    direct_8x8_inference_flag			= Slice->SequenceParameterSet->direct_8x8_inference_flag;
    monochrome					= (Slice->SequenceParameterSet->chroma_format_idc == 0) ? 1 : 0;

    Parameters.Cfg                         	= (monochrome			                << PP_CFG__MONOCHROME)          |
						  (direct_8x8_inference_flag                    << PP_CFG__DIRECT8X8FLAG_SHIFT) |
						  (transform_8x8_mode_flag                      << PP_CFG__TRANSFORM8X8MODE_SHIFT) |
						  (pic_init_qp                                  << PP_CFG__QPINIT_SHIFT)        |
						  (num_ref_idx_l1_active_minus1                 << PP_CFG__IDXL1_SHIFT)         |
						  (num_ref_idx_l0_active_minus1                 << PP_CFG__IDXL0_SHIFT)         |
						  (deblocking_filter_control_present_flag       << PP_CFG__DEBLOCKING_SHIFT)    |
						  (weighted_bipred_idc_flag                     << PP_CFG__BIPREDFLAG_SHIFT)    |
						  (weighted_pred_flag                           << PP_CFG__PREDFLAG_SHIFT)      |
						  (delta_pic_order_always_zero_flag             << PP_CFG__DPOFLAG_SHIFT)       |
						  (pic_order_present_flag                       << PP_CFG__POPFLAG_SHIFT)       |
						  (pic_order_cnt_type                           << PP_CFG__POCTYPE_SHIFT)       |
						  (frame_mbs_only_flag                          << PP_CFG__FRAMEFLAG_SHIFT)     |
						  (entropy_coding_mode_flag                     << PP_CFG__ENTROPYFLAG_SHIFT)   |
						  (mb_adaptive_frame_field_flag                 << PP_CFG__MBADAPTIVEFLAG_SHIFT);

    //
    // Now queue the request to the underlying driver
    //

    Status = OSDEV_Ioctl(  Device->UnderlyingDevice, H264_PP_IOCTL_QUEUE_BUFFER,
                          &Parameters, sizeof(h264pp_ioctl_queue_t) );
    if( Status != OSDEV_NoError )
    {
	report( severity_error, "H264ppProcessBuffer : Failed to queue a buffer for processing.\n" );
        return h264pp_error;
    }

//

    return h264pp_ok;
}


// ------------------------------------------------------------------------------------------------------------
//   The User address function

static inline h264pp_status_t   H264ppGetPreProcessedBuffer( h264pp_device_t     Device,
							     unsigned int       *QueueIdentifier,
							     unsigned int	*OutputSize,
							     unsigned int       *ErrorMask )
{
h264pp_ioctl_dequeue_t   Parameters;
int                      Status;

    //
    // Is this ok
    //

    if( Device == H264_PP_INVALID_DEVICE )
    {
	report( severity_error, "H264ppGetPreProcessedBuffer : Device not open.\n" );
	return h264pp_error;
    }

//

    Status = OSDEV_Ioctl(  Device->UnderlyingDevice, H264_PP_IOCTL_GET_PREPROCESSED_BUFFER,
			   &Parameters, sizeof(h264pp_ioctl_dequeue_t) );
    if( Status != OSDEV_NoError )
    {
	report( severity_error, "H264ppGetPreProcessedBuffer : Failed to get a pre-processed buffer.\n" );
	return h264pp_error;
    }

//

    *ErrorMask          = Parameters.ErrorMask;
    *OutputSize		= Parameters.OutputSize;
    *QueueIdentifier	= Parameters.QueueIdentifier;

//

    return h264pp_ok;
}

#endif
