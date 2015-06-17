/*
 *  Multi-Target Trace solution
 *
 *  MTT - CORE DRIVER FILE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#include <linux/module.h>
#include <linux/prctl.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/futex.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/console.h>

#include <linux/mtt/mtt.h>

#ifdef CONFIG_KPTRACE
#include <linux/mtt/kptrace.h>
#endif

MODULE_VERSION(MTT_VERSION_STRING);
MODULE_AUTHOR("Marc Titinger <marc.uitinger@st.com>");
MODULE_AUTHOR("Chris Smith <chris.smith@st.com>");
MODULE_DESCRIPTION("STMicro " MTTDEV_NAME " " MTT_VERSION_STRING);
MODULE_LICENSE("GPL");

static LIST_HEAD(output_drivers);

mtt_sys_kconfig_t mtt_sys_config;
struct mtt_output_driver *mtt_cur_out_drv;

/* ====   char dev support  ================
 */

static int mtt_drv_major;
module_param(mtt_drv_major, int, 0);
#define THIS_MINOR 0

static struct mtt_module_dev_t mtt_drv_data;
static DEFINE_MUTEX(api_mutex);

/* We will use the debugfs mainly for the dynamic tracing
 * stuff (kptrace). Configuration will use ioctl as we may have
 * complex structures to pass down to the driver, from the daemon.
 *
 */

static struct dentry *debugfs_dir;

/* Safer conversion routine from media ID to media name. */
static const char *guid_to_name(uint32_t *guid)
{
	if (guid && ((char *)guid)[3] == '\0')
		return (char *)guid;
	else
		return "Unknown";
}

static ssize_t
mtt_version_show_attrs(struct device *device,
		       struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%08x\n", MTT_INTERFACE_VERSION);
}

void mtt_register_output_driver(struct mtt_output_driver *addme)
{
	printk(KERN_INFO "MTT: registering output driver %s\n",
		   guid_to_name(&addme->guid));

	mutex_lock(&api_mutex);
	list_add_tail(&addme->list, &output_drivers);

	/* Initialize the current output */
	if (!mtt_cur_out_drv)
		mtt_cur_out_drv = addme;

	mutex_unlock(&api_mutex);
}
EXPORT_SYMBOL(mtt_register_output_driver);

void mtt_unregister_output_driver(struct mtt_output_driver *delme)
{
	struct mtt_output_driver *pos;

	mutex_lock(&api_mutex);
	list_for_each_entry(pos, &output_drivers, list)
	    if (pos == delme) {
		mtt_printk(KERN_INFO
			   "kptrace: unregistering output driver %s\n",
			   guid_to_name(&pos->guid));
		if (pos == mtt_cur_out_drv)
			mtt_cur_out_drv = NULL;

		list_del(&pos->list);
	}
	mutex_unlock(&api_mutex);
}
EXPORT_SYMBOL(mtt_unregister_output_driver);

/* Main control is a subsys */
static struct bus_type mtt_subsys = {
	.name = "mtt",
	.dev_name = "mtt",
};

static struct device mtt_device = {
	.id = 0,
	.bus = &mtt_subsys,
};

DEVICE_ATTR(mtt_version, S_IRUGO, mtt_version_show_attrs, NULL);

static void __init mtt_core_reset_cfg(void)
{
	/* Should never be empty. */
	BUG_ON(!mtt_cur_out_drv);

	mtt_cur_out_drv->last_error = 0;
	mtt_sys_config.session_id = 0;
	mtt_sys_config.media_guid = mtt_cur_out_drv->guid;
	mtt_sys_config.state = MTT_STATE_IDLE;
	mtt_printk(KERN_DEBUG "mtt: state set IDLE (%x)", mtt_sys_config.state);

	mtt_set_core_filter(MTT_LEVEL_NONE, NULL);
}

/* Remove all tracepoints */
static void mtt_stop_tracing(void)
{
	if (mtt_sys_config.state & MTT_STATE_RUNNING) {
		/* Notify downstream actors. */
		mtt_stop_tx();
		mtt_sys_config.state &= ~MTT_STATE_RUNNING;
		mtt_printk(KERN_DEBUG "mtt: state clear running (STOP: %x)",
			   mtt_sys_config.state);
	}

#ifdef CONFIG_KPTRACE
	/* Dynamic tracing activated at session start
	 * changing the kptrace component level requires
	 * restart. This is an optim/exception to other
	 * components that can be changed on the fly.*/
	mtt_kptrace_stop();

#endif /*CONFIG_KPTRACE*/
	if (mtt_cur_out_drv)
		mtt_cur_out_drv->last_error = 0;

	mtt_set_core_filter(MTT_LEVEL_NONE, NULL);
}

/* Insert all tracepoints */
static int mtt_start_tracing(void)
{
	int ret = 0;

	if (mtt_cur_out_drv->last_error) {
		printk(KERN_ERR
		       "mtt start: cannot start device in fault state\n");
		return -EBUSY;
	}

	if (mtt_sys_config.state == MTT_STATE_IDLE) {
		printk(KERN_ERR "mtt start: please config first\n");
		return -EINVAL;
	}

	mtt_sys_config.state |= MTT_STATE_RUNNING;
	mtt_printk(KERN_DEBUG "mtt: state set RUNNING (%x)",
		   mtt_sys_config.state);

#ifdef CONFIG_KPTRACE
	/* Tell kptrace the session is started. This will insert kprobe
	 * if any wanted. Actual tracing can be toggled with the kptrace
	 * component level.
	 */
	ret = mtt_kptrace_start();
#endif
	return ret;
}

/* Add the main sysdev */
static int __init create_sysfs_tree(void)
{
	int err;

	err = subsys_system_register(&mtt_subsys, NULL);
	if (err) {
		printk(KERN_ERR "could not register mtt bus\n");
		return err;
	}

	err = device_register(&mtt_device);
	if (err) {
		printk(KERN_ERR "could not register mtt subsys\n");
		bus_unregister(&mtt_subsys);
		return err;
	}

	/* In /sys/devices/system/mtt/mtt0 */
	device_create_file(&mtt_device, &dev_attr_mtt_version);

	/* Initialize the component tree */
	create_components_tree(&mtt_device);

#ifdef CONFIG_KPTRACE
	/*add kptrace part of sysfs */
	mtt_kptrace_init(&mtt_subsys);

#endif
	return 0;
}

static void remove_sysfs_tree(void)
{
#ifdef CONFIG_KPTRACE
	/*remove kptrace part of sysfs */
	mtt_kptrace_cleanup();
#endif
	remove_components_tree();

	if ((mtt_cur_out_drv) && (mtt_cur_out_drv->debugfs))
		debugfs_remove(mtt_cur_out_drv->debugfs);

	/* In /sys/devices/system/mtt/mtt0 */
	device_remove_file(&mtt_device, &dev_attr_mtt_version);

	device_unregister(&mtt_device);

	bus_unregister(&mtt_subsys);
}

/* === CDEV handlers ===
*/

static int mtt_cdev_open(struct inode *inode, struct file *filp)
{
	mtt_printk(KERN_DEBUG "mtt_cdev_open()\n");
	return 0;
}

static int mtt_cdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	mtt_printk(KERN_DEBUG "mtt_cdev_mmap()\n");

	/* if the current output driver support io range mapping
	   call it's mmap */
	if ((mtt_cur_out_drv) && (mtt_cur_out_drv->mmap_func))
		return mtt_cur_out_drv->mmap_func(filp, vma,
						  mtt_cur_out_drv->private);
	return -ENXIO;
}

/* Apply configuration as received for UI */
static mtt_return_t mtt_core_set_config(struct mtt_sys_kconfig *new_config)
{
	struct mtt_output_driver *driver = NULL;
	int err = MTT_ERR_PARAM;

	/* Copy new to local config. */
	memcpy(&mtt_sys_config, new_config, sizeof(struct mtt_sys_kconfig));

	/* Called from ioctl with mutex hold. */
	list_for_each_entry(driver, &output_drivers, list)
	    if (driver->guid == mtt_sys_config.media_guid) {
		err = 0;
		break;
		}

	if (err) {
		printk(KERN_ERR
		       "mtt: output media '%s' not found.\n",
		       guid_to_name(&(mtt_sys_config.media_guid)));
		return err;
	}

	mtt_printk(KERN_DEBUG "mtt: configuring '%s'.\n",
		   guid_to_name(&(driver->guid)));

	/* Create debugfs tree if necessary */
	if (!driver->debugfs)
		driver->debugfs =
		    debugfs_create_dir(guid_to_name(&(driver->guid)),
				       debugfs_dir);

	/* Call this driver's config, it'll create its debugfs in its
	 * subdir if it needs one.*/
	if (driver->config_func)
		err = driver->config_func(&mtt_sys_config);

	/* If the core.filter changed, propagate to all components that
	 * did not become independent by setting a specific level from UI.
	 */
	if (!err) {
		/* We can now switch over to the new output driver */
		mtt_cur_out_drv = driver;

		/* in case the output driver needs for pre-cooking
		 * for the component to channel resolution.
		 */
		mtt_component_fixup_co_private();

		mtt_component_snap_filter(mtt_sys_config.filter);

		mtt_sys_config.state |= MTT_STATE_READY;
		mtt_printk(KERN_DEBUG "mtt: state set READY (%x)",
			   mtt_sys_config.state);
	}

	return err;
}

/* Reply to IOC_GET_CORE_INFO */
int mtt_core_info(struct mtt_cmd_core_info *info /*.id is inout */)
{
	/* Right now, we only support LIN cores, as the inter-core
	 * comm spec is still pending.
	 */
	MTT_BUG_ON(!info);

	/* The target IDs beyond 0x0F may be assigned to
	 * a different Core type, like M4. Arbitrarily set
	 * the limit to 16 Linux cores.*/
	if (unlikely(info->id >= (MTT_TARGET_LIN0 + num_possible_cpus()))) {
		printk(KERN_ERR
		       "mtt: remote config supported only"
		       "for Linux cores for now.\n");
		return MTT_ERR_PARAM;
	}

	strcpy(info->name, "Linux");

	info->filter = mtt_sys_config.filter;

	/* Retrieve STM channel mapping,
	 * and more generally core-specific settings.
	 */
	return MTT_ERR_NONE;
}

static long mtt_cdev_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	int ret = 0;

	mtt_printk(KERN_DEBUG "mtt_cdev_ioctl(%x)\n", cmd);

	/* Don't even decode wrong cmds: better return ENOTTY than EFAULT */
	if (_IOC_TYPE(cmd) != MTT_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > MTT_IOC_LAST)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg,
				 _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	/* Locked : watch out for the return path ! */
	mutex_lock(&api_mutex);

	switch (cmd) {
	case MTT_IOC_GET_CAPS:
		{
			mtt_api_caps_t caps;
			mtt_core_get_caps(&caps);
			ret = copy_to_user((void __user *)arg, &caps,
					   sizeof(mtt_api_caps_t));
			break;
		}
	case MTT_IOC_SYSCONFIG:
		{
			struct mtt_sys_kconfig new_config;

			if (mtt_sys_config.state & MTT_STATE_RUNNING) {
				printk(KERN_DEBUG
				   "mtt_core_set_config: already running.\n");
				ret = -EBUSY;
			} else {
				/* Set the current channel to write to from
				 * the chardev the access_ok check was made
				 *  before. */
				ret = copy_from_user((void *)(&new_config),
						     (void *)arg,
						     sizeof(struct
							    mtt_sys_kconfig));
				if (!ret) {
					mtt_stop_tracing();

					/* Apply config */
					ret = mtt_core_set_config(&new_config);

					/* Apply new parameters to packets. */
					mtt_pkt_config();
				}
				if (ret) {
					printk(KERN_ERR
					   "mtt_core_set_config failed (%d)\n",
					    ret);
					ret = -ENOTTY;
				}
			}
		}
		break;
	case MTT_IOC_START:
		ret = mtt_start_tracing();
		break;
	case MTT_IOC_STOP:
		mtt_stop_tracing();
		break;
	case MTT_IOC_SET_CORE_FILT:
		{
			struct mtt_cmd_core_filt cmd;

			/* The access_ok check was made before. */
			ret = copy_from_user((void *)&cmd, (void *)arg,
					     sizeof(struct mtt_cmd_core_filt));

			 /* For now, only Linux can be enabled this way.
			  * this must be changed once we designed the intercore
			  * command scheme. */
			if (!ret)
				ret = mtt_set_core_filter(cmd.core_filter,
							  NULL);
		}
		break;
	case MTT_IOC_SET_COMP_FILT:
		{
			struct mtt_component_obj *co = NULL;
			struct mtt_cmd_get_cfilt cmd;

			/* The access_ok check was made before. */
			ret = copy_from_user((void *)&cmd, (void *)arg,
					     sizeof(struct mtt_cmd_get_cfilt));

			/* Return -EINVAL is not found */
			if (!ret)
				co = mtt_component_find(cmd.comp_id);

			if (!co)
				ret = -EINVAL;
			else
				ret =
				    mtt_set_component_filter((mtt_comp_handle_t)
							     co,
							     cmd.comp_filter,
							     NULL);
		}
		break;
	case MTT_IOC_GET_COMP_INFO:
		{
			struct mtt_cmd_comp_info info;

			ret = copy_from_user((void *)&info, (void *)arg,
					     sizeof(struct mtt_cmd_comp_info));

			if (!ret)
				ret = mtt_component_info(&info);

			if (!ret)
				ret = copy_to_user((void __user *)arg, &info,
						   sizeof(struct
							  mtt_cmd_comp_info));
		}
		break;
	case MTT_IOC_GET_CORE_INFO:
		{
			struct mtt_cmd_core_info info;

			ret = copy_from_user((void *)&info, (void *)arg,
					     sizeof(struct mtt_cmd_core_info));
			if (!ret)
				ret = mtt_core_info(&info);

			if (!ret)
				ret = copy_to_user((void __user *)arg, &info,
						   sizeof(struct
							  mtt_cmd_core_info));
		}
		break;
	case MTT_IOC_GET_OUTDRV_INFO:
		{
			struct mtt_outdrv_caps info;
			ret = copy_from_user((void *)&info, (void *)arg,
					     sizeof(struct mtt_outdrv_caps));
			if (!ret) {
				if (!mtt_cur_out_drv)
					ret = -EFAULT;
				else {
					info.devid = mtt_cur_out_drv->devid;
					ret = MTT_ERR_NONE;
					}
				}
			if (!ret)
				ret = copy_to_user((void __user *)arg, &info,
						   sizeof(struct
							  mtt_outdrv_caps));
		}
		break;
	default:
		ret = -ENOTTY;
	}

	mutex_unlock(&api_mutex);

	return ret;
}

static int mtt_cdev_release(struct inode *inode, struct file *filp)
{
	mtt_printk(KERN_DEBUG "mtt_cdev_release()\n");
	return 0;
}

/* The fops */
static const struct file_operations mtt_cdev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mtt_cdev_ioctl,
	.open = mtt_cdev_open,
	.llseek = no_llseek,
	.release = mtt_cdev_release,
	.mmap = mtt_cdev_mmap,
};

static void mtt_console_write(struct console *co, const char *s, unsigned count)
{
	mtt_packet_t *pkt;
	long core;
	uint32_t target;
	uint32_t type_info = MTT_TRACEITEM_STRING(count);

	if (!co->data)
		return;

	core = raw_smp_processor_id();
	target = MTT_TARGET_LIN0 + core;

	/* allocate or retrieve a preallocated TRACE frame */
	pkt = mtt_pkt_alloc(target);

	memcpy(mtt_pkt_get(co->data, pkt, type_info, 0), s, count);

	mtt_cur_out_drv->write_func(pkt, DRVLOCK);
};

static struct console mtt_console = {
	.name   = "mttS",
	.write  = mtt_console_write,
	.flags  = CON_PRINTBUFFER,
	.data   = NULL,
};

/* Initialize the default ouput and the control file system */
static int __init mtt_core_init(void)
{
	int ret = 0;

	mtt_pkt_init();

	/* in /sys/kernel/debug/mtt/... */
	debugfs_dir = debugfs_create_dir("mtt", NULL);
	if (!debugfs_dir) {
		printk(KERN_ERR "Couldn't create relay app directory.\n");
		return -ENOMEM;
	}

	mtt_printk("mtt: compiled with _MTT_DEBUG_ defined.\n");

	/* in /sys/device/system/mtt/... */

	if (create_sysfs_tree()) {
		debugfs_remove(debugfs_dir);
		printk(KERN_ERR "Couldn't create sysfs tree\n");
		return -ENOSYS;
	}
	/* Init device address pointers */
	memset(&mtt_drv_data, 0, sizeof(struct mtt_module_dev_t));

	/* Alloc major */
	if (mtt_drv_major) {
		mtt_drv_data.dev = MKDEV(mtt_drv_major, THIS_MINOR);
		ret = register_chrdev_region(mtt_drv_data.dev, 1, MTTDEV_NAME);
	} else {
		ret =
		    alloc_chrdev_region(&(mtt_drv_data.dev), THIS_MINOR, 1,
					MTTDEV_NAME);
		mtt_drv_major = MAJOR(mtt_drv_data.dev);
	}

	if (ret < 0) {
		printk(KERN_ERR "Registering \"" MTTDEV_NAME "\"  failed\n");
		return ret;
	}

	mtt_drv_data.major = MAJOR(mtt_drv_data.dev);

	/* Register chardev */
	cdev_init(&(mtt_drv_data.cdev), &mtt_cdev_fops);

	mtt_drv_data.cdev.owner = THIS_MODULE;
	mtt_drv_data.cdev.ops = &mtt_cdev_fops;

	ret = cdev_add(&(mtt_drv_data.cdev),
		       (unsigned int)(mtt_drv_data.dev), 1);

	if (ret) {
		printk(KERN_ERR "Error %d adding \"" MTTDEV_NAME "\"\n", ret);
		unregister_chrdev_region(mtt_drv_data.dev, 1);
		return ret;
	}

	if (mtt_cur_out_drv) {
		mtt_console.data = mtt_component_alloc(MTT_COMPID_SLOG,
						"syslog", 0);
		register_console(&mtt_console);
	}

	/* Init the builtin drivers. */
	mtt_relay_init();

	/* Set the output driver to the default one. */
	mtt_core_reset_cfg();

	printk(KERN_INFO "MTT: multicore-trace initialized.\n");

	return 0;
}
late_initcall(mtt_core_init);

/*
 * mtt_cleanup - free all the tracepoints and sets, remove the sysdev and
 * destroy the relay channel.
 */
static void __exit mtt_core_cleanup(void)
{
	dev_t dev;

	mtt_printk(KERN_DEBUG "mtt: mtt_cleanup\n");

	mtt_stop_tracing();

	/* Unregister builtin output drivers */
	mtt_relay_cleanup();

	mtt_pkt_cleanup();

	remove_sysfs_tree();

	dev = mtt_drv_data.dev;

	cdev_del(&(mtt_drv_data.cdev));

	unregister_chrdev_region(dev, 1);

	if (debugfs_dir) {
		debugfs_remove(debugfs_dir);
		debugfs_dir = NULL;
	}
}

/* Fill in the version structure */
void mtt_core_get_caps(mtt_api_caps_t *caps)
{
	struct mtt_output_driver *driver = NULL;

	caps->impl_info.version = MTT_API_VERSION;
	strncpy(caps->impl_info.string, MTT_API_VER_STRING,
		sizeof(caps->impl_info.string));

	/* We don't support packet states per component, nor
	 * systematically sending each trace's level in the data stream
	 */
	caps->caps = 0;
	caps->caps = (MTT_INIT_LOCAL_TIME | MTT_INIT_CONTEXT);

	list_for_each_entry(driver, &output_drivers, list)
		if (driver->guid == MTT_DRV_GUID_STM) {
			if (driver->devid == STM_IPv3_VERSION)
				caps->caps |= MTT_CAPS_STMV3;
			else
				caps->caps |= MTT_CAPS_STMV1;
			break;
		}
}
