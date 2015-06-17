/******************************************************************************
Copyright (C) 2011, 2012, 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_te.c

Implements the stm_te exported API
******************************************************************************/

#include <stm_te_dbg.h>
#include <stm_te.h>
#include <te_object.h>
#include <te_global.h>
#include <te_demux.h>
#include <te_pid_filter.h>
#include <te_rep_filter.h>
#include <te_ins_filter.h>
#include <te_pes_filter.h>
#include <te_ts_filter.h>
#include <te_pcr_filter.h>
#include <te_ts_index_filter.h>
#include <te_section_filter.h>
#include <te_internal_cfg.h>
#include <te_sysfs.h>
#include <te_tsg_filter.h>
#include <te_tsg_sec_filter.h>
#include <te_tsg_index_filter.h>

/*!
 * \brief Deletes an stm_te object
 *
 * At the stm_te API level all object deletions are the same
 *
 * \param hdl Handle of the stm_te object to delete
 *
 * \return 0 on success or object-specific error
 */
static int stm_te_delete(stm_te_object_h hdl)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(hdl, &obj);
	if (err)
		pr_err("Invalid handle %p\n", hdl);
	else
		err = obj->ops->delete(obj);

	mutex_unlock(&te_global.lock);

	return err;
}

/*!
 * \brief Sets a control value on an stm_te object
 *
 * \param hdl   Handle of the stm_te object to configure
 * \param ctrl  Control to set
 * \param value Pointer to value to set
 * \param size  Size of value to set
 *
 * \return 0 on success or object-specific error
 */
static int stm_te_set_control(stm_te_object_h hdl, uint32_t ctrl,
		const void *value, uint32_t size)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(hdl, &obj);
	if (err)
		pr_err("Invalid handle %p\n", hdl);
	else
		err = obj->ops->set_control(obj, ctrl, value, size);

	mutex_unlock(&te_global.lock);

	return err;
}

/*!
 * \brief Reads a control value on an stm_te object
 *
 * \param hdl   Handle of the stm_te object to check
 * \param ctrl  Control to read
 * \param value Pointer to buffer into which to read the control value
 * \param size  Size of buffer pointed to by buffer
 *
 * \return 0 on success or object-specific error
 */
static int stm_te_get_control(stm_te_object_h hdl, uint32_t ctrl,
		void *value, uint32_t size)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(hdl, &obj);
	if (err)
		pr_err("Invalid handle %p\n", hdl);
	else
		err = obj->ops->get_control(obj, ctrl, value, size);

	mutex_unlock(&te_global.lock);

	return err;
}

/*!
 * \brief Returns the global capabilities of the stm_te library
 *
 * \param capabilities Pointer to the capabilities structure to populate
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EINTR  Call interrupted
 */
int stm_te_get_capabilities(stm_te_caps_t *capabilities)
{
	int err = 0;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	if (!capabilities) {
		err = -EINVAL;
		goto out;
	}

	capabilities->max_demuxes = te_global.demux_class.max;
	capabilities->max_pid_filters = te_global.in_filter_class.max;
	capabilities->max_output_filters = te_global.out_filter_class.max;
	capabilities->max_tsmuxes = te_global.tsmux_class.max;
	capabilities->max_tsg_filters = te_global.tsg_filter_class.max;
	capabilities->max_tsg_filters_per_mux = TE_MAX_TSG_PER_TSMUX;

out:
	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_get_capabilities);

/*!
 * \brief Creates a new stm_te demux object
 *
 * \param name    The name of the new demux object. This must be globally unique
 * \param demux_h Returned handle for the new demux
 *
 * \retval 0       Success
 * \retval -EEXIST Object already exists
 * \retval -ENOMEM Insufficient resources to allocate demux
 * \retval -EIO    Hardware error
 * \retval -EINVAL Bad parameter
 * \retval -EINTR  Call interrupted
 */
int stm_te_demux_new(const char *name, stm_te_object_h *demux_h)
{
	int err = 0;
	struct te_obj *demux = NULL;

	stm_te_trace_in();

	/* Basic parameter checks */
	if (!name || !demux_h) {
		pr_err("Bad parameter\n");
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_new((char *)name, &demux);
	if (!err)
		*demux_h = te_obj_to_hdl(demux);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_new);

/*!
 * \brief Deletes a demux object
 *
 * Deletes a demux object that was previously created with stm_te_demux_new.
 * After deletion the object handle should not be used
 *
 * \param demux_h Demux handle to delete
 *
 * \retval 0       Success
 * \retval -EBUSY  Demux has connections or child objects and cannot be
 *                 deleted
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 */
int stm_te_demux_delete(stm_te_object_h demux_h)
{
	int err;

	stm_te_trace_in();
	err = stm_te_delete(demux_h);
	stm_te_trace_out_result(err);

	return err;
}
EXPORT_SYMBOL(stm_te_demux_delete);

/*!
 * \brief Starts data flowing through a demux object
 *
 * \param demux_h Handle of demux object to start
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_demux_start(stm_te_object_h demux_h)
{
	int err = 0;
	struct te_obj *demux;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (!err)
		err = te_demux_start(demux);

	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_start);

/*!
 * \brief Stops data flowing through a demux object
 *
 * \param demux_h Handle of demux object to stop
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_demux_stop(stm_te_object_h demux_h)
{
	int err = 0;
	struct te_obj *demux;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (!err)
		err = te_demux_stop(demux);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_stop);

/*!
 * \brief Sets the value of a given control for a demux object
 *
 * \param demux_h  Handle of the demux object to set the control value on
 * \param selector Control to set
 * \param value    Value to set
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Control value not available for writing on this demux
 */
int stm_te_demux_set_control(stm_te_object_h demux_h,
			     stm_te_demux_ctrl_t selector, unsigned int value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_DEMUX_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_set_control(demux_h, selector, &value, sizeof(value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_set_control);

/*!
 * \brief Reads the value of a given control for a demux object
 *
 * \param demux_h  Handle of the demux object to read the control value from
 * \param selector Control to read
 * \param value    Returned control value
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Control value not available for reading on this demux
 */
int stm_te_demux_get_control(stm_te_object_h demux_h,
			     stm_te_demux_ctrl_t selector, unsigned int *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_DEMUX_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_get_control(demux_h, selector, value, sizeof(*value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_get_control);

/*!
 * \brief Get a multi-word control value on a filter object
 *
 * \param filter_h Handle of the filter object to get the control on
 * \param selector Control to get
 * \param value    Pointer to the value to get
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Getting this control is value is not supported on this
 *                 filter object
 */
int stm_te_demux_get_compound_control(stm_te_object_h demux_h,
		stm_te_demux_compound_ctrl_t selector, void *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < STM_TE_DEMUX_CNTRL_STATUS ||
		selector >= STM_TE_DEMUX_COMPOUND_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	/* TODO: Compound control should have a size parameter. For now, we use
	 * the size of the largest compound control */
	err = stm_te_get_control(demux_h, selector, value,
				sizeof(stm_te_demux_stats_t));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_demux_get_compound_control);

/*!
 * \brief Creates a new stm_te tsmux object
 *
 * \param name    The name of the new tsmux object. This must be globally unique
 * \param tsmux_h Returned handle for the new tsmux
 *
 * \retval 0       Success
 * \retval -EEXIST Object already exists
 * \retval -ENOMEM Insufficient resources to allocate tsmux
 * \retval -EINVAL Bad parameter
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsmux_new(const char *name, stm_te_object_h *tsmux_h)
{
	int err = 0;
	struct te_obj *tsmux = NULL;

	stm_te_trace_in();

	/* Basic parameter checks */
	if (!name || !tsmux_h) {
		pr_err("Bad parameter\n");
		err = -EINVAL;
		goto error;
	}

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_new((char *)name, &tsmux);
	if (!err)
		*tsmux_h = te_obj_to_hdl(tsmux);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_new);

/*!
 * \brief Deletes a tsmux object
 *
 * Deletes a tsmux object that was previously created with stm_te_tsmux_new.
 * After deletion the object handle should not be used
 *
 * \param tsmux_h TSMux handle to delete
 *
 * \retval 0       Success
 * \retval -EBUSY  TSMux has connections or child objects and cannot be
 *                 deleted
 * \retval -EINVAL Bad parameter
 * \retval -ENXIO  TSMux does not exist
 */
int stm_te_tsmux_delete(stm_te_object_h tsmux_h)
{
	int err;

	stm_te_trace_in();
	err = stm_te_delete(tsmux_h);
	stm_te_trace_out_result(err);

	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_delete);

/*!
 * \brief Starts data flowing through a tsmux object
 *
 * \param tsmux_h Handle of tsmux object to start
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsmux_start(stm_te_object_h tsmux_h)
{
	int err = 0;
	struct te_obj *tsmux;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_from_hdl(tsmux_h, &tsmux);
	if (!err)
		err = te_tsmux_start(tsmux);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_start);

/*!
 * \brief Stops data flowing through a tsmux object
 *
 * \param tsmux_h Handle of tsmux object to stop
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsmux_stop(stm_te_object_h tsmux_h)
{
	int err = 0;
	struct te_obj *tsmux;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_from_hdl(tsmux_h, &tsmux);
	if (!err)
		err = te_tsmux_stop(tsmux);
	if (!err)
		err = te_tsg_filter_stop_all(tsmux);
	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_stop);

/*!
 * \brief Sets the value of a given control for a tsmux object
 *
 * \param tsmux_h  Handle of the tsmux object to set the control value on
 * \param selector Control to set
 * \param value    Value to set
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Control value not available for writing on this tsmux
 */
int stm_te_tsmux_set_control(stm_te_object_h tsmux_h,
			     stm_te_tsmux_ctrl_t selector, unsigned int value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_TSMUX_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_set_control(tsmux_h, selector, &value, sizeof(value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_set_control);

/*!
 * \brief Reads the value of a given control for a tsmux object
 *
 * \param tsmux_h  Handle of the tsmux object to read the control value from
 * \param selector Control to read
 * \param value    Returned control value
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Control value not available for reading on this tsmux
 */
int stm_te_tsmux_get_control(stm_te_object_h tsmux_h,
			     stm_te_tsmux_ctrl_t selector, unsigned int *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_TSMUX_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_get_control(tsmux_h, selector, value, sizeof(*value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_get_control);

/*!
 * \brief Sets a multi-word control value on a tsmux object
 *
 * \param tsmux_h Handle of the tsmux object to set the control on
 * \param selector Control to set
 * \param value    Pointer to the value to set
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Setting this control is value is not supported on this
 *                 tsmux object
 */
int stm_te_tsmux_set_compound_control(stm_te_object_h tsmux_h,
		stm_te_tsmux_compound_ctrl_t selector, const void *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_TSMUX_COMPOUND_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	/* TSMUX Compound controls should all be max of
	 * STM_TE_TSMUX_MAX_DESCRIPTOR in size. */
	err = stm_te_set_control(tsmux_h, selector, value,
			STM_TE_TSMUX_MAX_DESCRIPTOR);

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_set_compound_control);

/*!
 * \brief Gets a multi-word control value on a tsmux object
 *
 * \param tsmux_h Handle of the tsmux object to get the control on
 * \param selector Control to get
 * \param value    Pointer to the value to get
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Getting this control is value is not supported on this
 *                 tsmux object
 */
int stm_te_tsmux_get_compound_control(stm_te_object_h tsmux_h,
		stm_te_tsmux_compound_ctrl_t selector, void *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_TSMUX_COMPOUND_CNTRL_LAST) {
		err = -EINVAL;
		goto done;
	}

	/* TSMUX Compound controls should all be max of
	 * STM_TE_TSMUX_MAX_DESCRIPTOR in size. */
	err = stm_te_get_control(tsmux_h, selector, value,
			STM_TE_TSMUX_MAX_DESCRIPTOR);

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_get_compound_control);

/*!
 * \brief Attaches a tsmux to another STKPI object
 *
 * This function may be used for attaching tsmuxes to other STKPI obkects.
 *
 * E.g. Attaching a tsmux to an stm_memsink
 *
 * \param tsmux_h Handle for the tsmux object to attach
 * \param target   Handle of the STKPI object to attach to
 *
 * \retval 0         Success
 * \retval -EINVAL   Invalid handle
 * \retval -EPERM    Connection is not supported
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsmux_attach(stm_te_object_h tsmux_h, stm_object_h target)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(tsmux_h, &obj);
	if (err)
		pr_err("Invalid handle %p\n", tsmux_h);
	else
		err = obj->ops->attach(obj, target);

	mutex_unlock(&te_global.lock);

	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_attach);

/*!
 * \brief Detaches a tsmux object from another STKPI
 *
 * The objects must previously have been attached using stm_te_tsmux_attach
 *
 * \param tsmux_h Handle of the tsmux object to detach
 * \param target   STKPI object to detach from
 *
 * \retval 0       Success
 * \retval -EINVAL Invalid handle
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsmux_detach(stm_te_object_h tsmux_h, stm_object_h target)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(tsmux_h, &obj);
	if (err)
		pr_err("Invalid handle %p\n", tsmux_h);
	else
		err = obj->ops->detach(obj, target);

	mutex_unlock(&te_global.lock);

	return err;
}
EXPORT_SYMBOL(stm_te_tsmux_detach);

/*!
 * \brief Deletes a filter object
 *
 * \param filter_h Handle of the filter object to delete
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 * \retval -EBUSY  The filter object is attached to another object and cannot
 *                 be deleted
 */
int stm_te_filter_delete(stm_te_object_h filter_h)
{
	int err;

	stm_te_trace_in();
	err = stm_te_delete(filter_h);
	stm_te_trace_out_result(err);

	return err;
}
EXPORT_SYMBOL(stm_te_filter_delete);

/*!
 * \brief Sets a control value on a filter object
 *
 * \param filter_h Handle of the filter object to set the control on
 * \param selector Control to set
 * \param value    The value to set the control to
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Setting this control is value is not supported on this
 *                 filter object
 */
int stm_te_filter_set_control(stm_te_object_h filter_h,
			      stm_te_filter_ctrl_t selector, unsigned int value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_FILTER_CONTROL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_set_control(filter_h, selector, &value, sizeof(value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_filter_set_control);

/*!
 * \brief Reads the value of a given control for a filter object
 *
 * \param filter  Handle of the filter object to read the control value from
 * \param selector Control to read
 * \param value    Returned control value
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Control value not available for reading on this tsmux
 */
int stm_te_filter_get_control(stm_te_object_h filter_h,
				stm_te_filter_ctrl_t selector,
				unsigned int *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < 0 || selector >= STM_TE_FILTER_CONTROL_LAST) {
		err = -EINVAL;
		goto done;
	}

	err = stm_te_get_control(filter_h, selector, value, sizeof(*value));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_filter_get_control);

/*!
 * \brief Sets a multi-word control value on a filter object
 *
 * \param filter_h Handle of the filter object to set the control on
 * \param selector Control to set
 * \param value    Pointer to the value to set
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Setting this control is value is not supported on this
 *                 filter object
 */
int stm_te_filter_set_compound_control(stm_te_object_h filter_h,
		stm_te_filter_compound_ctrl_t selector, const void *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < __deprecated_STM_TE_FILTER_CONTROL_STATUS ||
			selector >= STM_TE_FILTER_COMPOUND_CONTROL_LAST) {
		err = -EINVAL;
		goto done;
	}

	/* TODO: Compound control should have a size parameter. For now, we use
	 * the size of the largest compound control */
	err = stm_te_set_control(filter_h, selector, value,
			sizeof(stm_te_ts_index_set_params_t));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_filter_set_compound_control);

/*!
 * \brief Get a multi-word control value on a filter object
 *
 * \param filter_h Handle of the filter object to get the control on
 * \param selector Control to get
 * \param value    Pointer to the value to get
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Getting this control is value is not supported on this
 *                 filter object
 */
int stm_te_filter_get_compound_control(stm_te_object_h filter_h,
		stm_te_filter_compound_ctrl_t selector, void *value)
{
	int err = 0;

	stm_te_trace_in();

	/* Basic parameter check */
	if (selector < __deprecated_STM_TE_FILTER_CONTROL_STATUS ||
			selector >= STM_TE_FILTER_COMPOUND_CONTROL_LAST) {
		err = -EINVAL;
		goto done;
	}

	/* TODO: Compound control should have a size parameter. For now, we use
	 * the size of the largest compound control */
	err = stm_te_get_control(filter_h, selector, value,
			sizeof(stm_te_ts_index_set_params_t));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_filter_get_compound_control);

/*!
 * \brief Attaches a filter to another STKPI object
 *
 * This function may be used for attaching input filters to output filters or
 * for attaching TE filters to other STKPI objects.
 *
 * E.g. Attaching a PID filter to a stm_ce_transform
 *      Attaching an output filter to an stm_memsink
 *
 * \param filter_h Handle for the filter object to attach
 * \param target   Handle of the STKPI object to attach to
 *
 * \retval 0         Success
 * \retval -EINVAL   Invalid handle
 * \retval -EPERM    Connection is not supported
 * \retval -EIO      Hardware error
 * \retval -EINTR    Call interrupted
 */
int stm_te_filter_attach(stm_te_object_h filter_h, stm_object_h target)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(filter_h, &obj);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);
	else
		err = obj->ops->attach(obj, target);

	mutex_unlock(&te_global.lock);

	return err;
}
EXPORT_SYMBOL(stm_te_filter_attach);

/*!
 * \brief Detaches a filter object from another STKPI
 *
 * The objects must previously have been attached using stm_te_filter_attach
 *
 * \param filter_h Handle of the filter object to deteach
 * \param target   STKPI object to detach from
 *
 * \retval 0       Success
 * \retval -EINVAL Invalid handle
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_filter_detach(stm_te_object_h filter_h, stm_object_h target)
{
	int err;
	struct te_obj *obj;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(filter_h, &obj);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);
	else
		err = obj->ops->detach(obj, target);

	mutex_unlock(&te_global.lock);

	return err;
}
EXPORT_SYMBOL(stm_te_filter_detach);

/*!
 * \brief Retrieves the current status of a filter
 *
 * \param filter_h Handle of the filter object to check
 * \param status   Pointer populated with the returned status
 *
 * \deprecated	This function is deprecated and replaced by
 *              stm_te_filter_get_compound_control with
 *              STM_TE_OUTPUT_FILTER_CONTROL_STATUS control
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 */
int stm_te_filter_get_status(stm_te_object_h filter_h,
			     stm_te_filter_status_t *status)
{
	int err;

	stm_te_trace_in();

	pr_warning("stm_te_filter_get_status is deprecated. "
			"Please use stm_te_filter_get_compound_control with "
			"STM_TE_OUTPUT_FILTER_CONTROL_STATUS control\n");

	/* Basic parameter check*/
	if (status == NULL) {
		pr_err("Bad parameter\n");
		err = -EINVAL;
		goto done;
	}

	err = stm_te_get_control(filter_h,
			STM_TE_OUTPUT_FILTER_CONTROL_STATUS, status,
			sizeof(stm_te_output_filter_stats_t));

done:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_filter_get_status);

/*
 * \brief Checks if a DVB PID is within the valid range
 *
 * \retval true  PID is valid
 * \retval false PID is invalid
 */
static bool te_pid_is_valid(stm_te_pid_t pid)
{
	/* Range check PID */
	if (pid > DVB_PID_MAX_VALUE && STM_TE_PID_NONE != pid &&
			STM_TE_PID_ALL != pid) {
		pr_err("Invalid pid: %d\n", pid);
		return false;
	}
	return true;
}

/*!
 * \brief Creates a new PID filter object
 *
 * \param demux_h  Handle of the parent demux object
 * \param pid      Initial PID for the new PID filter to filter on
 * \param filter_h Returned handle to the new PID filter
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Failed to allocated resources
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_filter_new(stm_te_object_h demux_h,
			  stm_te_pid_t pid, stm_te_object_h *filter_h)
{
	int err;
	struct te_obj *demux = NULL;
	struct te_obj *filter = NULL;

	if (filter_h == NULL)
		return -EINVAL;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (err)
		pr_err("Bad demux handle\n");

	if (!err)
		err = te_pid_filter_new(demux, pid, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);

	return err;
}
EXPORT_SYMBOL(stm_te_pid_filter_new);

/*!
 * \brief Sets the PID for a PID filter to filter on
 *
 * \param filter_h Handle of the PID filter to modify
 * \param pid      New PID to filter on
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_filter_set_pid(stm_te_object_h filter_h, stm_te_pid_t pid)
{
	int err;
	struct te_obj *filter = NULL;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_obj_from_hdl(filter_h, &filter);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_INPUT_FILTER_CONTROL_PID,
				&pid, sizeof(pid));
	else
		pr_err("Invalid handle %p (%d)\n", filter_h, err);

	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_filter_set_pid);

/*!
 * \brief Retrieves the PID that is currently being filtered by a PID filter
 *
 * \param filter_h Handle of the PID filter to check
 * \param pid      Returned currently filtered PID
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_filter_get_pid(stm_te_object_h filter_h, stm_te_pid_t *pid)
{
	int err;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_obj_from_hdl(filter_h, &filter);
	if (!err)
		err = filter->ops->get_control(filter,
				STM_TE_INPUT_FILTER_CONTROL_PID,
				pid, sizeof(*pid));
	else
		pr_err("Invalid handle %p (%d)\n", filter_h, err);

	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_filter_get_pid);

/*!
 * \brief Sets a PID on a PID filter and a new PID to remap any packets to.
 *
 * Any TS filter that is attached to this PID filter will have the input PID
 * remapped to the output PID in its output TS
 *
 * \param filter_h   PID filter to configure
 * \param input_pid  Input PID to filter on
 * \param output_pid The PID to remap filtered packets
 *
 * \retval 0       Success
 * \retval -EINVAL Invalid parameter
 * \retval -EIO    Hardware error
 * \retval -ENOSYS Feature unavailable
 */
int stm_te_pid_filter_set_pid_remap(stm_te_object_h filter_h,
				    stm_te_pid_t input_pid,
				    stm_te_pid_t output_pid)
{
	pr_err("Function unimplemented\n");
	return -ENOSYS;
}
EXPORT_SYMBOL(stm_te_pid_filter_set_pid_remap);

/*!
 * \brief Configures secondary PID filtering on a demux
 *
 * \param demux_h            Demux to register link on
 * \param primary_pid        Primary PID to filter on
 * \param secondary_pid      Secondary PID to filter on
 * \param secondary_pid_mode Operational mode of the secondary PID filtering
 *
 * \retval 0       Success
 * \retval errno   Otherwise
 */
int stm_te_link_secondary_pid(stm_te_object_h demux_h,
                            stm_te_pid_t ppid,
                            stm_te_pid_t spid,
                            stm_te_secondary_pid_mode_t mode)
{
	int err = 0;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_link_secondary_pid(demux_h, ppid, spid, mode);

	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_link_secondary_pid);

/*!
 * \brief Unlinks secondary PID filtering on a demux
 *
 * \param demux_h            Demux to register link on
 * \param primary_pid        Primary PID being filtered
 *
 * \retval 0       Success
 * \retval errno   otherwise
 */
int stm_te_unlink_secondary_pid(stm_te_object_h demux_h,
                            stm_te_pid_t pid)
{
	int err = 0;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_unlink_secondary_pid(demux_h, pid);

	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_unlink_secondary_pid);

/*!
 * \brief Creates a new output filter
 *
 * \param demux_h     Handle of the parent demux object for the new filter
 * \param filter_type Type of output filter to create
 * \param filter_h    Returned handle of the new output filter
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOSYS Feature not available
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_output_filter_new(stm_te_object_h demux_h,
			     stm_te_filter_t filter_type,
			     stm_te_object_h *filter_h)
{
	int err = 0;
	struct te_obj *demux = NULL;
	struct te_obj *filter = NULL;
	unsigned int enable = 1;

	stm_te_trace_in();

	/* Basic parameter check */
	if (!filter_h) {
		pr_err("Bad parameter\n");
		err = -EINVAL;
		goto error;
	}

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		goto out;
	}

	/* Call the corresponding object constructor */
	switch (filter_type) {
	case STM_TE_TS_FILTER:
		err = te_ts_filter_new(demux, &filter);
		break;
	case STM_TE_PES_FILTER:
		err = te_pes_filter_new(demux, &filter);
		break;
	case STM_TE_SECTION_FILTER:
		err = te_section_filter_new(demux, &filter);
		break;
	case STM_TE_TS_INDEX_FILTER:
		err = te_ts_index_filter_new(demux, &filter);
		break;
	case STM_TE_PCR_FILTER:
		err = te_pcr_filter_new(demux, &filter);
		break;
	case STM_TE_ECM_FILTER:
		/* ECM filter is a section filter with an additional control
		 * set */
		pr_warning("STM_TE_ECM_FILTER filters are deprecated."
			   "  Please use a section filter and set the"
			   " STM_TE_SECTION_FILTER_CONTROL_ECM control\n");
		err = te_section_filter_new(demux, &filter);
		if (!err) {
			err = filter->ops->set_control(filter,
					STM_TE_SECTION_FILTER_CONTROL_ECM,
					&enable,
					sizeof(enable));
		}
		break;
	default:
		pr_err("Bad output filter type %d\n", filter_type);
		err = -EINVAL;
	}

out:
	if (!err)
		*filter_h = te_obj_to_hdl(filter);
	mutex_unlock(&te_global.lock);
	pr_debug("New output filter %s (%p)\n", filter->name, filter);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_output_filter_new);

/*!
 * \brief Sets the filter pattern for a section filter
 *
 * Initially section filters are configured to parse all sections. This
 * function allows a section filter to only extract sections that match a
 * particular pattern
 *
 * \param filter_h     Handle of the section filter object to configure
 * \param length       The number of bytes to filter on. Valid range 0-18 bytes
 * \param filter_bytes The byte pattern to filter
 * \param filter_masks The array of bitwise masks to apply to pattern. Masked
 *                     bits always match the filter. 1=apply filter, 0=do not
 *                     apply filter
 *
 * \retval  0      Success
 * \retval -EINVAL Bad paramter
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_section_filter_set(stm_te_object_h filter_h,
			      unsigned int length,
			      unsigned char *filter_bytes,
			      unsigned char *filter_masks)
{
	int err;
	struct te_obj *filter;
	unsigned int enable = 0;

	/* Basic parameter check */
	if (!filter_bytes) {
		pr_err("Invalid filter pattern\n");
		return -EINVAL;
	}

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_obj_from_hdl(filter_h, &filter);
	if (err) {
		pr_err("Bad handle\n");
		goto out;
	}

	err = filter->ops->set_control(filter,
			STM_TE_SECTION_FILTER_CONTROL_PATTERN,
			filter_bytes, length);
	if (!err)
		err = filter->ops->set_control(filter,
			STM_TE_SECTION_FILTER_CONTROL_MASK,
			filter_masks, length);
	if (!err)
		err = filter->ops->set_control(filter,
			STM_TE_SECTION_FILTER_CONTROL_POS_NEG_ENABLED,
			&enable, sizeof(enable));

out:
	mutex_unlock(&te_global.lock);

error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_section_filter_set);

/*!
 * \brief Enables a section filter to filter sections using a positive-negative
 * match mode
 *
 * \param filter_h        Handle of the section filter to configure
 * \param length          The number of bytes to filter on. Valid range 0-18
 *                        bytes
 * \param filter_bytes    The array of bytes to filter on
 * \param filter_masks    The array of bitwise masks to apply to pattern. Masked
 *                        bits always match the filter. 1=apply filter, 0=do not
 *                        apply filter
 * \param pos_neg_pattern A bitwise pattern indicateing which bits in
 *                        filter_bytes must match and which bits must not match
 *                        Postive match sense = 1
 *                        Negative match sense = 0
 *                        In either case the filter mask is applied to disable
 *                        filtering particular bits
 *
 * \retval  0      Success
 * \retval -EINVAL Bad paramter
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_section_filter_positive_negative_set(stm_te_object_h filter_h,
						unsigned int length,
						unsigned char *filter_bytes,
						unsigned char *filter_masks,
						unsigned char *pos_neg_pattern)
{
	int err;
	struct te_obj *filter;
	unsigned int enable = 1;

	/* Basic parameter check */
	if (!filter_bytes) {
		pr_err("Invalid filter pattern\n");
		return -EINVAL;
	}

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_obj_from_hdl(filter_h, &filter);
	if (err) {
		pr_err("Bad handle\n");
		goto out;
	}

	err = filter->ops->set_control(filter,
			STM_TE_SECTION_FILTER_CONTROL_PATTERN,
			filter_bytes, length);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_SECTION_FILTER_CONTROL_MASK,
				filter_masks, length);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_SECTION_FILTER_CONTROL_POS_NEG_PATTERN,
				pos_neg_pattern, length);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_SECTION_FILTER_CONTROL_POS_NEG_ENABLED,
				&enable, sizeof(enable));

out:
	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_section_filter_positive_negative_set);

/*!
 * \brief Creates a new PID insertion object
 *
 * \param demux_h  Handle of the parent demux for the new PID insertion object
 * \param pid      PID of inserted packets
 * \param filter_h Returned handle of the new PID insertion object
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Insufficient resources for the new object
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_ins_filter_new(stm_te_object_h demux_h, stm_te_pid_t pid,
		stm_te_object_h *filter_h)
{
	int err;
	struct te_obj *demux = NULL;
	struct te_obj *filter = NULL;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (err)
		pr_err("Bad demux handle\n");

	if (!err)
		err = te_ins_filter_new(demux, pid, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_ins_filter_new);

/*!
 * \brief Sets the insertion parameters for a PID insertion object
 *
 * \param filter_h  Handle of the PID insertion object to configure
 * \param data      Pointer to data buffer to insert
 * \param data_size Number of bytes of data to insert
 * \param freq_ms   Insertion repetition interval (0 = single insertion)
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOMEM Insufficient memory for insertion buffer
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_ins_filter_set(stm_te_object_h filter_h,
					unsigned char *data,
					unsigned int data_size,
					unsigned int freq_ms)
{
	int err = 0;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_in_filter_from_hdl(filter_h, &filter);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);

	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_PID_INS_FILTER_CONTROL_DATA,
				data, data_size);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_PID_INS_FILTER_CONTROL_FREQ,
				&freq_ms, sizeof(freq_ms));

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_ins_filter_set);

/*!
 * \brief Trigger insertion filter (one-shot)
 *
 * \param filter_h  Handle of the PID insertion object to configure
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOMEM Insufficient memory for insertion buffer
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_ins_filter_trigger(stm_te_object_h filter_h)
{
	int err = 0;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_in_filter_from_hdl(filter_h, &filter);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);

	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_PID_INS_FILTER_CONTROL_TRIG,
				NULL, 0);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_ins_filter_trigger);

/*!
 * \brief Creates a new PID replacement object
 *
 * \param demux_h  Handle of parent demux for the new PID replacement object
 * \param pid      PID to replace
 * \param filter_h Returned handle for the new PID replacement object
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Insufficient resource for the new object
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_rep_filter_new(stm_te_object_h demux_h, stm_te_pid_t pid,
		stm_te_object_h *filter_h)
{
	int err;
	struct te_obj *demux = NULL;
	struct te_obj *filter = NULL;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_demux_from_hdl(demux_h, &demux);
	if (err)
		pr_err("Bad demux handle\n");

	if (!err)
		err = te_rep_filter_new(demux, pid, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_rep_filter_new);

/*!
 * \brief Sets the replacement parameters for a packet replacement object
 *
 * \param filter_h  Handle for the packet replacement object to configure
 * \param data      Pointer to the data buffer to use for replacement
 * \param data_size Size (in bytes) of the replacement data
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Insufficient resources for the replacement buffer
 * \retval -EIO    Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_rep_filter_set(stm_te_object_h filter_h,
					unsigned char *data,
					unsigned int data_size)
{
	int err = 0;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_in_filter_from_hdl(filter_h, &filter);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);

	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_PID_INS_FILTER_CONTROL_DATA,
				data, data_size);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_pid_rep_filter_set);

/*!
 * \brief Registers a target that will automatically attached to any pid filter
 *        with a matching PID
 *
 * \param demux     Demux to register target with
 * \param pid       PID to automatically attach to
 * \param target    target that will automatically attached to PID filter
 *
 * \retval 0        Success
 * \retval -EINVAL  Bad parameter
 * \retval -ENOMEM  Insufficient resources
 * \retval -EIO     Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_attach(stm_te_object_h demux, stm_te_pid_t pid, void *target)
{
	int res;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	/* Get global te write lock */
	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		res = -EINTR;
		goto error;
	}

	res = te_demux_register_pid_autotarget(demux, pid, target);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(res);
	return res;
}
EXPORT_SYMBOL(stm_te_pid_attach);

/*!
 * \brief Removes a pid from automatic attach, detaches all auto connected
 *        targets
 *
 * \param demux     Demux to register target with
 * \param pid       pid to detach
 *
 * \retval 0        Success
 * \retval -EINVAL  Bad parameter
 * \retval -EIO     Hardware error
 * \retval -EINTR  Call interrupted
 */
int stm_te_pid_detach(stm_te_object_h demux, stm_te_pid_t pid)
{
	int res;

	if (!te_pid_is_valid(pid))
		return -EINVAL;

	/* Get global te write lock */
	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		res = -EINTR;
		goto error;
	}

	res = te_demux_unregister_pid_autotarget(demux, pid);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(res);
	return res;
}
EXPORT_SYMBOL(stm_te_pid_detach);

/*!
 * \brief Creates a new TSG filter object
 *
 * \param tsmux_h  Handle of the parent tsmux for the new TSG filter object
 * \param filter_h Returned handle of the new TSG filter object
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EINTR  Call interrupted
 * \retval -ENOMEM Insufficient resources for the new object
 */
int stm_te_tsg_filter_new(stm_te_object_h tsmux_h, stm_te_object_h *filter_h)
{
	int err;
	struct te_obj *tsmux = NULL;
	struct te_obj *filter = NULL;

	if (filter_h == NULL)
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_from_hdl(tsmux_h, &tsmux);
	if (err)
		pr_err("Bad tsmux handle\n");

	if (!err)
		err = te_tsg_filter_new(tsmux, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsg_filter_new);

/*!
 * \brief Creates a new TSG section filter object
 *
 * \param tsmux_h  Handle of the parent tsmux for the new TSG filter object
 * \param filter_h Returned handle of the new TSG section filter object
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Insufficient resources for the new object
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsg_sec_filter_new (stm_te_object_h tsmux_h, stm_te_object_h *filter_h)
{
	int err = 0;
	struct te_obj *tsmux = NULL;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (filter_h == NULL) {
		err = -EINVAL;
		goto error;
	}

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_from_hdl(tsmux_h, &tsmux);
	if (err)
		pr_err("Bad tsmux handle\n");

	if (!err)
		err = te_tsg_sec_filter_new(tsmux, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsg_sec_filter_new);

/*!
 * \brief Sets the insertion parameters for a PID insertion object
 *
 * \param filter_h  Handle of the PID insertion object to configure
 * \param data      Pointer to data buffer to insert
 * \param data_size Number of bytes of data to insert
 * \param freq_ms   Insertion repetition interval (0 = single insertion)
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -EIO    Hardware error
 * \retval -ENOMEM Insufficient memory for insertion buffer
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsg_sec_filter_set(stm_te_object_h filter_h,
					unsigned char *data,
					unsigned int data_size,
					unsigned int freq_ms)
{
	int err = 0;
	struct te_obj *filter = NULL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsg_filter_from_hdl(filter_h, &filter);
	if (err)
		pr_err("Invalid handle %p\n", filter_h);

	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_TSG_SEC_FILTER_CONTROL_DATA,
				data, data_size);
	if (!err)
		err = filter->ops->set_control(filter,
				STM_TE_TSG_SEC_FILTER_CONTROL_FREQ,
				&freq_ms, sizeof(freq_ms));

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsg_sec_filter_set);

/*!
 * \brief Creates a new TSG index filter object
 *
 * \param tsmux_h  Handle of the parent tsmux of the new TSG Index filter object
 * \param filter_h Returned handle of the new TSG index filter object
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 * \retval -ENOMEM Insufficient resources for the new object
 * \retval -EINTR  Call interrupted
 */
int stm_te_tsg_index_filter_new(stm_te_object_h tsmux_h,
		stm_te_object_h *filter_h)
{
	int err;
	struct te_obj *tsmux = NULL;
	struct te_obj *filter = NULL;

	if (filter_h == NULL)
		return -EINVAL;

	stm_te_trace_in();

	if (mutex_lock_interruptible(&te_global.lock) != 0) {
		err = -EINTR;
		goto error;
	}

	err = te_tsmux_from_hdl(tsmux_h, &tsmux);
	if (err)
		pr_err("Bad tsmux handle\n");

	if (!err)
		err = te_tsg_index_filter_new(tsmux, &filter);

	if (!err)
		*filter_h = te_obj_to_hdl(filter);

	mutex_unlock(&te_global.lock);
error:
	stm_te_trace_out_result(err);
	return err;
}
EXPORT_SYMBOL(stm_te_tsg_index_filter_new);
