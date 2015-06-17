/***********************************************************************
 *
 * File: private/include/display_device_priv.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef DISPLAY_DEVICE_PRIV_H
#define DISPLAY_DEVICE_PRIV_H

#if defined(__cplusplus)
extern "C" {
#endif


#define STKPI_LOCK(dev)             stm_display_device_stkpi_lock  (dev, __PRETTY_FUNCTION__)
#define STKPI_UNLOCK(dev)           stm_display_device_stkpi_unlock(dev, __PRETTY_FUNCTION__)

#define IS_DEVICE_SUSPENDED(dev)    stm_display_device_is_suspended(dev)


/*****************************************************************************
 * C private interface for display device
 */

/*!
 * Create the display device.
 *
 * \param id     A device identifier: 0..n..
 * \param device The returned device handle.
 *
 * \returns 0       Success
 * \returns -ENODEV The device could not be created
 * \returns -EFAULT The device parameter pointer is an invalids addresses
 */
extern int stm_display_device_create(uint32_t id, stm_display_device_h *device);

/*!
 * Destroy the display device.
 *
 * \param device The device handle.
 *
 */
extern void stm_display_device_destroy(stm_display_device_h device);

/*!
 * Register the display device power hooks.
 *
 * \param dev Display device to query.
 * \param get A pointer to the power pm_runtime_get() function
 * \param put A pointer to the power pm_runtime_put() function
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The device handle was invalid
 * \returns -EFAULT The power hooks parameter pointers are an invalids addresses
 */
extern int stm_display_device_register_pm_runtime_hooks(stm_display_device_h dev, int (*get)(const uint32_t id), int (*put)(const uint32_t id));

/*!
 * Get the display device power status.
 *
 * \param dev Display device to query.
 * \param pm_state A pointer to the power state's value
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The device handle was invalid
 * \returns -EFAULT The power status parameter pointer is an invalid address
 */
extern int stm_display_device_get_power_state(stm_display_device_h dev, uint32_t *pm_state);

/*!
 * Check if the display device is suspended or not.
 *
 * \param dev       Display device to query.
 *
 * \returns false   The display device is active
 * \returns true    The display device is not active
 */
extern bool stm_display_device_is_suspended(stm_display_device_h dev);

/*!
 * Power down video DACs.
 *
 * \param dev Display device to query.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The device handle was invalid
 */
extern int stm_display_device_power_down_video_dacs(stm_display_device_h dev);

/*!
 * Lock the STKPI mutex
 *
 * \param dev       Display device to query.
 * \param fct_name  Name of the STKPI function calling the lock
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the lock
 * \returns -EINVAL The device handle was invalid
 */
extern int stm_display_device_stkpi_lock(stm_display_device_h dev, const char *fct_name);

/*!
 * Unlock the STKPI mutex
 *
 * \param dev       Display device to query.
 * \param fct_name  Name of the STKPI function calling the unlock
 *
 */
extern void stm_display_device_stkpi_unlock(stm_display_device_h dev, const char *fct_name);


#if defined(__cplusplus)
}
#endif

#endif
