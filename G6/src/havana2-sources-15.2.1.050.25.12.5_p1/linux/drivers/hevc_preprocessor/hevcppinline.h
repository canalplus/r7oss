/************************************************************************
  Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

  This file is part of the Streaming Engine.

  Streaming Engine is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License version 2 as
  published by the Free Software Foundation.

  Streaming Engine is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with Streaming Engine; see the file COPYING.  If not, write to the Free
  Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
  USA.

  The Streaming Engine Library may alternatively be licensed under a
  proprietary license from ST.
 ************************************************************************/

#ifndef HEVCPPINLINE_H
#define HEVCPPINLINE_H

// this file is meant to be included from player cpp code
// e.g. not to be included from linux native modules
#ifndef __cplusplus
#error "not included from player cpp code"
#endif

#include "osinline.h"
#include "osdev_user.h"

#include "hevcppio.h"

struct HevcppDevice_t {
	OSDEV_DeviceIdentifier_t iUnderlyingDevice;
};

static inline hevcpp_status_t HevcppOpen(struct HevcppDevice_t **aDevice)
{
	OSDEV_Status_t status;

	*aDevice = (HevcppDevice_t *) OS_Malloc(sizeof(struct HevcppDevice_t));
	if (*aDevice == NULL) {
		return hevcpp_error;
	}

	memset(*aDevice, 0, sizeof(struct HevcppDevice_t));

	status = OSDEV_Open(HEVCPP_DEVICE, &((*aDevice)->iUnderlyingDevice));
	if (status != OSDEV_NoError) {
		SE_ERROR("Failed to open device\n");
		OS_Free(*aDevice);
		*aDevice = NULL;
		return hevcpp_error;
	}

	return hevcpp_ok;
}

static inline void HevcppClose(struct HevcppDevice_t *aDevice)
{
	if (aDevice != NULL) {
		OSDEV_Close(aDevice->iUnderlyingDevice);
		OS_Free(aDevice);
	}
}

static inline hevcpp_status_t HevcppQueueBuffer(
        struct HevcppDevice_t *aDevice,
        struct hevcpp_ioctl_queue_t *aQueParams)
{
	int r;

	if (aDevice == NULL) {
		SE_ERROR("Device not open\n");
		return hevcpp_error;
	}

	r = OSDEV_Ioctl(aDevice->iUnderlyingDevice, HEVC_PP_IOCTL_QUEUE_BUFFER,
	                aQueParams, sizeof(struct hevcpp_ioctl_queue_t));
	if (r != OSDEV_NoError) {
		SE_ERROR("Failed to queue buffer\n");
		return hevcpp_error;
	}

	return hevcpp_ok;
}

static inline hevcpp_status_t HevcppGetPreProcessedBuffer(
        struct HevcppDevice_t *aDevice,
        struct hevcpp_ioctl_dequeue_t *aDequeParams)
{
	int r;

	if (aDevice == NULL) {
		SE_ERROR("Device not open\n");
		return hevcpp_error;
	}

	r = OSDEV_Ioctl(aDevice->iUnderlyingDevice,
	                HEVC_PP_IOCTL_GET_PREPROCESSED_BUFFER,
	                aDequeParams, sizeof(struct hevcpp_ioctl_dequeue_t));
	if (r != OSDEV_NoError) {
		SE_ERROR("Failed to get buffer\n");
		return hevcpp_error;
	}

	return hevcpp_ok;
}

static inline hevcpp_status_t HevcppGetPreProcAvailability(struct HevcppDevice_t *aDevice)
{
	int ret = 0;
	int val = 0;

	ret = OSDEV_Ioctl(aDevice->iUnderlyingDevice,
	                  HEVC_PP_IOCTL_GET_PREPROC_AVAILABILITY, (void *)&val, sizeof(val));

	if (ret != OSDEV_NoError) {
		SE_ERROR("Failed to get buffer\n");
		return hevcpp_error;
	}

	return hevcpp_ok;
}

#endif // HEVCPPINLINE_H
