/***********************************************************************
 *
 * File: include/stm_display_device.h
 * Copyright (c) 2006-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Gives access to system's output(s) and plane(s) */

#ifndef STM_DISPLAY_DEVICE_H
#define STM_DISPLAY_DEVICE_H

#if defined(__cplusplus)
extern "C" {
#endif

/**************************************************************************//**
 * \file stm_display_device.h
 * \brief C Interface to display device objects
 */

/*!
 * \brief Device handle type
 */
typedef struct stm_display_device_s *stm_display_device_h;

/*!
 * \brief Output handle type
 */
typedef struct stm_display_output_s *stm_display_output_h;

/*!
 * \brief Plane handle type
 */
typedef struct stm_display_plane_s *stm_display_plane_h;

/*!
 * \brief Source handle type
 */
typedef struct stm_display_source_s *stm_display_source_h;

/*!
 * \brief Structure that contains the tuning capability parameters.
 *
 * \apis ::stm_display_device_get_tuning_caps()
 */
typedef struct stm_device_tuning_caps_s
{
  uint16_t tuning_service;
  uint32_t tuning_params_list_max_size;
} stm_device_tuning_caps_t;


/******************************************************************************
 * C Interface to display device objects
 */

/*!
 *  This function returns a device handle that can be used in subsequent function
 *  calls that operate on a device object. The first request for a handle to a
 *  particular device identifier will cause that device object to be
 *  initialized. The call will return -ENODEV if the requested device
 *  identifier does not exist in a particular implementation or if the device
 *  object cannot be initialized. Device identifiers are a contiguous integer
 *  sequence starting from 0; most SoC implementations will contain only one
 *  device.
 *
 * \param  id      A device identifier: 0..n.
 * \param  device  The returned device handle.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EFAULT The source output parameter pointer is an invalid address.
 * \returns -ENODEV The source identifier does not exist on this device.
 *
 */
extern int stm_display_open_device(uint32_t id, stm_display_device_h *device);


/*!
 * This function queries which outputs have some specific capabilitie(s) set
 * and/or unset. This is done by giving an output capabilities value
 * ("caps_match": where each bit indicate the state of an output capability)
 * and an output capabilities mask ("caps_mask": where each bit at 1 indicates
 * which output capability should be checked).
 * This function helps to retrieve the right output for some given
 * capabilities. By this way the application can identify the right role that
 * an output will play in the system based on the capabilities.
 * This function can be also used to query the actual number of outputs in the
 * system. This is done by setting max_ids equal to 0. In this case, "id"
 * output parameter can be a null pointer too.
 *
 * \param device                A handle to device to query a output’s
 *                              capabilities.
 * \param caps_match            State of each output capabilities. For each
 *                              bit set in "caps_mask", the output
 *                              capabilities should match exactly the
 *                              "caps_match".
 * \param caps_mask             Mask indicating which output capabilities
 *                              to check.
 * \param id                    List of output IDs corresponding to the
 *                              list of output capabilities provided.
 * \param max_ids               Max number of elements in the array.
 *
 * /pre    The device handler is obtained.
 *
 * \returns 0 or any positive number    The maximum number of elements that
 *                                      would be for a list of features of the
 *                                      given output.
 * \returns -EINVAL                     Reference passed as parameter was not
 *                                      previously provided by the CoreDisplay.
 * \returns -EFAULT                     The output parameter pointer (id) is an
 *                                      invalid address
 * \returns -EINTR                      The call was interrupted while obtaining
 *                                      the device lock
 */
extern int stm_display_device_find_outputs_with_capabilities(
                                stm_display_device_h               device,
                                stm_display_output_capabilities_t  caps_match,
                                stm_display_output_capabilities_t  caps_mask,
                                uint32_t*                          id,
                                uint32_t                           max_ids);

/*!
 *  This function returns an output handle that can be used in subsequent function
 *  calls that operate on an output object.Output identifiers are a contiguous
 *  integer sequence starting from 0. All of the outputs supported by a device
 *  can be iterated over by incrementing the identifier until the function
 *  returns -NODEV. Once the output handle is no longer required, it should be
 *  closed using stm_display_output_close().
 *
 * \param  device  A handle to the device object to query.
 * \param  id  An output identifier: 0..n.
 * \param  output  The returned output handle.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 * \returns -EFAULT The source output parameter pointer is an invalid address.
 * \returns -ENODEV The source identifier does not exist on this device.
 * \returns -ENOMEM The device failed to allocate internal data.
 *
 */
extern int stm_display_device_open_output(stm_display_device_h device, uint32_t id,
                                          stm_display_output_h *output);


/*!
 * This function queries which planes have some specific capabilitie(s) set
 * and/or unset. This is done by giving a plane capabilities value
 * ("caps_match": where each bit indicate the state of a plane capability)
 * and a plane capabilities mask ("caps_mask": where each bit at 1 indicates
 * which output capability should be checked).
 * This function helps to retrieve the right plane for some given
 * capabilities. By this way the application can identify the right role that
 * a plane will play in the system based on the capabilities.
 * This function can be also used to query the actual number of planes in the
 * system. This is done by setting max_ids equal to 0. In this case, "id"
 * plane parameter can be a null pointer too.
 *
 * \param device                A handle to device to query a plane’s
 *                              capabilities.
 * \param caps_match            State of each plane capabilities. For each
 *                              bit set in "caps_mask", the output
 *                              capabilities should match exactly the
 *                              "caps_match".
 * \param caps_mask             Mask indicating which plane capabilities
 *                              to check.
 * \param id                    List of plane IDs corresponding to the
 *                              list of plane capabilities provided.
 * \param max_ids               Max number of elements in the array.
 *
 * /pre    The device handler is obtained.
 *
 * \returns 0 or any positive number    The maximum number of elements that
 *                                      would be for a list of features of the
 *                                      given plane.
 * \returns -EINVAL                     Reference passed as parameter was not
 *                                      previously provided by the CoreDisplay.
 * \returns -EFAULT                     The plane parameter pointer (id) is an
 *                                      invalid address
 * \returns -EINTR                      The call was interrupted while obtaining
 *                                      the device lock
 */
extern int stm_display_device_find_planes_with_capabilities(
                                stm_display_device_h      device,
                                stm_plane_capabilities_t  caps_match,
                                stm_plane_capabilities_t  caps_mask,
                                uint32_t*                 id,
                                uint32_t                  max_ids);

/*!
 *  This function returns a plane handle that can be used in subsequent function
 *  calls that operate on a plane object.Plane identifiers are a contiguous
 *  integer sequence starting from 0. All of the planes supported by a device
 *  can be iterated over by incrementing the identifier until the function
 *  returns -NODEV.Once the plane handle is no longer required, it should be
 *  closed using stm_display_plane_close().
 *
 * \param device  A handle to the device object to query.
 * \param id      A plane identifier: 0..n.
 * \param plane   The returned plane handle.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 * \returns -EFAULT The source output parameter pointer is an invalid address.
 * \returns -ENODEV The source identifier does not exist on this device.
 * \returns -ENOMEM The device failed to allocate internal data.
 *
 */
extern int stm_display_device_open_plane(stm_display_device_h device, uint32_t id,
                                         stm_display_plane_h  *plane);

/*!
 *  This function returns a source handle that can be used in subsequent function
 *  calls that operate on a source object. Source identifiers are a contiguous
 *  integer sequence starting from 0. All of the sources supported by a device
 *  can be iterated over by incrementing the identifier until the function
 *  returns -NODEV. Once the source handle is no longer required, it should be
 *  closed using stm_display_source_close().
 *
 * \param device  A handle to the device object to query
 * \param id      A source identifier: 0..n
 * \param source  The returned source handle
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 * \returns -EFAULT The source output parameter pointer is an invalid address.
 * \returns -ENODEV The source identifier does not exist on this device.
 * \returns -ENOMEM The device failed to allocate internal data.
 *
 */
extern int stm_display_device_open_source(stm_display_device_h device, uint32_t id,
                                          stm_display_source_h *source);

/*!
 * Query the number of available tuning services supported by the device. If
 * the device does not support any tuning services, this call will succeed and
 * it will return zero in the nservices return parameter.
 *
 * This function should be used to determine how much memory needs to be
 * allocated for the tuning capabilities data returned by
 * ::stm_display_device_get_tuning_caps().
 *
 * \param device    A handle to the device object to query
 * \param nservices The returned number of tuning services provided by the
 *                  device.
 *
 * \returns 0 on success or -1 if the device lock cannot be obtained.
 *
 */
extern int stm_display_device_get_number_of_tuning_services(stm_display_device_h device,
                                                            uint16_t            *nservices);

/*!
 * Query for the device tuning capabilities.
 *
 * \param device      A handle to the device object to query
 * \param tuning_caps The returned tuning capabilities.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 * \returns -EFAULT The source output parameter pointer is an invalid address.
 *
 */
extern int stm_display_device_get_tuning_caps(stm_display_device_h      device,
                                              stm_device_tuning_caps_t* tuning_caps);

/*!
 * Configure a tuning service.
 *
 * \param device           A handle to the device object to configure
 * \param tuning_service   The tuning service selector.
 * \param input_list       Pointer to the input parameter list
 * \param input_list_size  Input parameter list size (in bytes)
 * \param output_list      Pointer to the output parameter list
 * \param output_list_size Output parameter list size (in bytes)
 *
 * \returns 0       Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -EINVAL  The device handle was invalid.
 * \returns -EFAULT  The source output parameter pointer is an invalid address.
 * \returns -ENOTSUP The service is not supported.
 * \returns -ERANGE  The tuning parameters are not valid.
 *
 */
extern int stm_display_device_set_tuning(stm_display_device_h device,
                                  uint16_t             tuning_service,
                                  void                *input_list,
                                  uint32_t             input_list_size,
                                  void                *output_list,
                                  uint32_t             output_list_size);
/*!
 * Update all programming, for hardware managed by a device that is clocked by
 * the provided video timing generator identifier, for the next video frame or
 * field. Suitable video timing generator identifiers can be obtained, for
 * instance for a display output timing generator using
 * ::stm_display_output_get_timing_identifier(). Timing identifiers may also be
 * obtained for display sources that manage an independently clocked buffer
 * queue.
 *
 * This is intended to be called as a result of a VSync interrupt for the given
 * timing generator by the OS/platform component that provides system support
 * for the CoreDisplay kernel API, which includes the OS specific first level
 * interrupt handlers for the display hardware.
 *
 *
 * \note This call can result in memory allocated from the OS heap being
 * released. If that is illegal in interrupt context (e.g. under OS21) then
 * this must be called from a high priority task context instead. On Linux
 * it must happen as part of the VSync interrupt handling as scheduling tasklets
 * or kernel threads can be delayed too long.
 *
 * \param device   device to update
 * \param timing_id timing generator we are updating for.
 */
extern void stm_display_device_update_vsync_irq(stm_display_device_h device, uint32_t timing_id);

/*!
 * Same as stm_display_device_update_vsync_irq() but for operations for Vsync event in threaded IRQ context.
 */
extern void stm_display_device_update_vsync_threaded_irq(stm_display_device_h dev, uint32_t timing_id);

/*!
 * Update all programming, for source hardware managed by a device that is clocked by
 * the provided video timing generator identifier, for the next video frame or
 * field. Suitable video timing generator identifiers can be obtained, for
 * instance for a display source timing generator using
 * ::stm_display_source_get_timing_identifier(). Timing identifiers may also be
 * obtained for display sources that manage an independently clocked buffer
 * queue.
 *
 * This is intended to be called as a result of a VSync interrupt for the given
 * timing generator by the OS/platform component that provides system support
 * for the CoreDisplay kernel API, which includes the OS specific first level
 * interrupt handlers for the display hardware.
 *
 *
 * \note This call can result in memory allocated from the OS heap being
 * released. If that is illegal in interrupt context (e.g. under OS21) then
 * this must be called from a high priority task context instead. On Linux
 * it must happen as part of the VSync interrupt handling as scheduling tasklets
 * or kernel threads can be delayed too long.
 *
 * \param device   device to update
 * \param timing_id timing generator we are updating for.
 */
extern void stm_display_device_source_update(stm_display_device_h device, uint32_t timing_id);

/*!
 * This function closes the given device handle. This may cause the API
 * implementation to shut down and release all of its resources when the last
 * device handle for a particular device object is closed. The device handle
 * is invalid after this call completes. Calling this function with a NULL
 * handle is allowed and will not cause an error or system failure.
 *
 * \param device  The handle to close
 *
 */
extern void stm_display_device_close(stm_display_device_h device);

/*!
 * Freeze the given display device. This may cause the API
 * implementation to shut down all of its resources. Ressources are not
 * released and they are putted in reset state after this call completes.
 *
 * \param device           A handle to the device object to freeze
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_display_device_freeze(stm_display_device_h device);

/*!
 * Suspend the given display device. This may cause the API
 * implementation to shut down all of its resources. Ressources are not
 * released and they are putted in reset state after this call completes.
 *
 * \param device           A handle to the device object to suspend
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_display_device_suspend(stm_display_device_h device);

/*!
 * Resume the given display device. This may cause the API
 * implementation to restart all of its resources. Some of the ressources
 * are not restarted with the original previous configuration as they
 * were putted in reset state.
 *
 * \param device           A handle to the device object to resume
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_display_device_resume(stm_display_device_h device);

/*!
 * Infor the display device that shutting down process is now started
 * so it is no more needed to check the power state.
 *
 * \param device           A handle to the device object
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_display_device_shutting_down(stm_display_device_h device);


#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_DEVICE_H */
