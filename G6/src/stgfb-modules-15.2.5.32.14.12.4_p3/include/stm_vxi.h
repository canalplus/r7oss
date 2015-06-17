/***********************************************************************
 *
 * File: include/stm_vxi.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_VXI_H
#define _STM_VXI_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_vxi.h
 *  \brief C interface to VXI objects
 */

/***********************************************************************
 *  Constants
 **********************************************************************/

/*! \enum    stm_vxi_ctrl_e
 *  \brief   VXI control names
 *  \apis    ::stm_vxi_set_control(), ::stm_vxi_get_control()
 */
typedef enum stm_vxi_ctrl_e
{
    STM_VXI_CTRL_SELECT_INPUT
} stm_vxi_ctrl_t;

typedef enum stm_vxi_interface_type_e
{
    STM_VXI_INTERFACE_TYPE_CCIR656,
    STM_VXI_INTERFACE_TYPE_SMPTE_274M_296M,
    STM_VXI_INTERFACE_TYPE_RGB_YUV_444
} stm_vxi_interface_type_t;

/***********************************************************************
 *  Types
 **********************************************************************/

/*! \struct  stm_vxi_h
 *  \brief   A handle to the VXI device object. A device object handle can be obtained
 *           using the device open call stm_vxi_open(). A valid device handle must be
 *           passed to all device API functions to indicate the device that is being used.
 */
typedef struct stm_vxi_s *stm_vxi_h;

/*! \struct  stm_vxi_capability_t
 *  \brief   Defines the VXI device capability.
 *  \apis    ::stm_vxi_get_capability()
 */
typedef struct stm_vxi_capability_s
{
    uint32_t number_of_inputs;
} stm_vxi_capability_t;

/*! \struct  stm_vxi_input_capability_t
 *  \brief   Defines the VXI device input capability.
 *  \apis    ::stm_vxi_get_input_capability()
 */
typedef struct stm_vxi_input_capability_s
{
    stm_vxi_interface_type_t interface_type;
    uint32_t                 bits_per_pixel;
    bool                     vbi_support;

} stm_vxi_input_capability_t;


/***********************************************************************
 *  Functions
 **********************************************************************/
/*!
 * Query a vxi handle.
 *
 * \param vxi Video eXpansion Input handle to query.
 * \param id Video eXpansion Input device identifier.
 * \returns  0      On success.
 * \returns -EINVAL Invalid device identifier.
 * \returns -EFAULT Invalid pointer address.
 */
extern int stm_vxi_open(uint32_t id, stm_vxi_h *vxi);

/*!
 * Release the given vxi handle.
 *
 * \note The pointer is invalid after this call completes successfully.
 * \param vxi Video eXpansion Input handle to release.
 * \returns 0         On success.
 * \returns -EFAULT Invalid handle
 */
extern int stm_vxi_close(stm_vxi_h vxi);

/*!
 *  Returns the capability of an underlying vxi device.
 *
 * \param vxi Video eXpansion Input handle.
 * \param *capability Pointer to capability structure.
 * \returns 0         On success.
 * \returns -EINVAL Invalid device handle..
 * \returns -EFAULT Invalid pointer address.
 */
extern int stm_vxi_get_capability(stm_vxi_h vxi, stm_vxi_capability_t *capability);

/*!
 *  Returns returns the capability of the selected input.
 *
 * \param vxi Video eXpansion Input handle.
 * \param *capability Pointer to input capability structure.
 * \returns 0         On success.
 * \returns -EINVAL Invalid device handle..
 * \returns -EFAULT Invalid pointer address.
 */
extern int stm_vxi_get_input_capability(stm_vxi_h vxi, stm_vxi_input_capability_t *capability);

/*!
 * Set the value of a specified control.
 *
 * \param vxi Video eXpansion Input handle.
 * \param ctrl Control to be set.
 * \param new_value Control value.
 * \returns  0      On success.
 * \returns -EINVAL Invalid vxi handle/parameter value
 * \returns -EPERM  control not permitted
 */
extern int stm_vxi_set_ctrl(stm_vxi_h vxi, stm_vxi_ctrl_t ctrl, uint32_t value);

/*!
 * Get the value of a specified control.
 *
 * \param vxi Video eXpansion Input handle.
 * \param ctrl Control to be retrieved.
 * \param *value Pointer to return value.
 * \returns  0      On success.
 * \returns -EINVAL Invalid route handle.
 * \returns -EFAULT Invalid pointer address.
 */
extern int stm_vxi_get_ctrl( stm_vxi_h vxi, stm_vxi_ctrl_t ctrl, uint32_t *value);

/*!
 * Freeze the given vxi device. This may cause the API
 * implementation to shut down all of its resources.
 *
 * \param vxi Video eXpansion Input handle to freeze
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_vxi_freeze(stm_vxi_h vxi);

/*!
 * Suspend the given vxi device. This may cause the API
 * implementation to shut down all of its resources.
 *
 * \param vxi Video eXpansion Input handle object to suspend
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_vxi_suspend(stm_vxi_h vxi);

/*!
 * Resume the given vxi device. This may cause the API
 * implementation to restart all of its resources. Ressources
 * are restarted with the original previous configuration.
 *
 * \param vxi Video eXpansion Input handle object to resume
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 */
extern int stm_vxi_resume(stm_vxi_h vxi);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_VXI_H */
