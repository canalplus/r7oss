/***********************************************************************
 *
 * File: include/stm_display_source_queue.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_SOURCE_QUEUE_H
#define _STM_DISPLAY_SOURCE_QUEUE_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_source_queue.h
 *  \brief C interface to in-memory display source buffer queues
 */

/*!
 * \brief Source Queue Interface handle type
 */

typedef struct stm_display_source_queue_s *stm_display_source_queue_h;

/*****************************************************************************
 * C interface to in-memory source buffer queues
 *
 */

/*!
 * This function fills an array with a list of supported pixel formats that can
 * be read by this memory source. This will be the list of pixel formats
 *  supported by the planes connected to this source.
 *
 * \note The list itself is owned by the source and must not be modified.
 *
 * \param q           Interface to query.
 * \param formats     Pointer to an array to fill in with the supported
 *                    pixel formats.
 * \param max_formats Max number of elements in the array.
 *
 * \returns           The number of formats filled in the array on success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 *
 */
extern int stm_display_source_queue_get_pixel_formats(stm_display_source_queue_h q,
                                                      stm_pixel_format_t *formats,
                                                      uint32_t max_formats);

/*!
 *  This function obtains exclusive use of the buffer queue for use by the given
 *  interface pointer.
 *
 * \param   q   Queue interface to lock.
 * \param   force Force taking ownership of this source even if it is already locked by someone else.
 *
 * \returns 0         On success.
 * \returns -ENOLCK   The queue has already been locked for use by another
 *                    interface pointer.
 * \returns -EINVAL   The device handle is invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 *
 */
extern int stm_display_source_queue_lock(stm_display_source_queue_h q, bool force);

/*!
 * Release this interface pointer's exclusive use of buffer queue. If the
 * interface pointer does not currently own the buffer queue lock this call has
 * no effect, this is not considered an error.
 *
 * \param q The interface to unlock
 *
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 *
 */
extern int stm_display_source_queue_unlock(stm_display_source_queue_h q);

/*!
 * This function queues a buffer to be read by the source. Only the interface
 * pointer that owns the lock can call this function (the function would fail
 * otherwise).
 *
 * \note: The buffer descriptor contains information about the source, as well
 * as some callback functions to inform the caller when the buffer is displayed
 * and not needed anymore.

 * \param q The interface to queue the buffer on.
 * \param b The buffer descriptor to queue.
 *
 * \returns 0         On success.
 * \returns -ENOLCK   This interface pointer doesn't have exclusive access to
 *                    the buffer queue ( thanks to a preliminary call to
 *                    stm_display_source_queue_lock() ).
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The address of the buffer pointer is not valid.
 * \returns -EINVAL   The queue handle is invalid.
 *
 */
extern int stm_display_source_queue_buffer(stm_display_source_queue_h q,
                                           const stm_display_buffer_t *b);

/*!
 * This function removes buffers from the source's queue.
 * If flush_buffers_on_display is true, all the buffers are flushed.
 * If it is False, all the buffers are flushed except the ones currently
 * displayed by a video planes.
 * Flush_buffers_on_display will be set to false when the client needs to
 * perform a Pause or a change the playing speed. The display will then be
 * frozen on the current picture.
 * The display will restart when the client queues some new buffers to display.
 * Only the interface pointer that owns the lock can call this function
 * (the function would fail otherwise)
 *
 *
 * \param q                        Queue interface to flush.
 * \param flush_buffers_on_display Indicates if all of the buffers should be
 *                                 flushed, including the ones on display.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -1        The call failed.
 *
 */
extern int stm_display_source_queue_flush(stm_display_source_queue_h q,
                                          bool flush_buffers_on_display);

/*!
 * Release the given queue interface.
 *
 * If this interface has locked the source's queue, then this lock is released
 * and the queue flushed.
 *
 * \note The pointer is invalid after this call completes successfully.
 *
 * \param q Queue interface to release
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 *
 */
extern int stm_display_source_queue_release(stm_display_source_queue_h q);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_SOURCE_QUEUE_H */
