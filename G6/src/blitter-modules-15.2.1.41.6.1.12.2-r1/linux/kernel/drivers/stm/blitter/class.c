#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#include <linux/pm_runtime.h>

#include <linux/cdev.h>

#include "class.h"

struct stm_blitter_class_device {
	struct device                     *sbcd_device;
	dev_t                              sbcd_devt;
	struct cdev                        sbcd_cdev;
	const struct stm_blitter_file_ops *fops;

	struct dentry                     *debugfs_dir;
};


static struct class *stm_blitter_class;
static dev_t stm_blitter_devt;
static struct dentry *stm_blitter_debugfs;


static int
stm_blitter_open(struct inode *inode,
		 struct file  *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct stm_blitter_class_device *sbcd
		= container_of(cdev, struct stm_blitter_class_device,
			       sbcd_cdev);
	int res = 0;

	filp->private_data = sbcd;

	if (sbcd->fops && sbcd->fops->open)
		res = sbcd->fops->open(sbcd->sbcd_device);

	return res;
}

static int
stm_blitter_release(struct inode *inode,
		    struct file  *filp)
{
	struct stm_blitter_class_device *sbcd = filp->private_data;

	if (sbcd->fops && sbcd->fops->close)
		sbcd->fops->close(sbcd->sbcd_device);

	return 0;
}

static long
stm_blitter_ioctl(struct file   *filp,
		  unsigned int   cmd,
		  unsigned long  arg)
{
	struct stm_blitter_class_device *sbcd = filp->private_data;
	long res = -ENOTTY;

	if (sbcd->fops && sbcd->fops->ioctl)
		res = sbcd->fops->ioctl(sbcd->sbcd_device, cmd, arg);

	return res;
}

static int
stm_blitter_mmap(struct file           *filp,
		 struct vm_area_struct *vma)
{
	struct stm_blitter_class_device *sbcd = filp->private_data;
	int res = -ENODEV;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	if (sbcd->fops && sbcd->fops->mmap)
		res = sbcd->fops->mmap(sbcd->sbcd_device, vma);

	return res;
}

static const struct file_operations stm_blitter_fops = {
	.owner = THIS_MODULE,

	.open = stm_blitter_open,
	.release = stm_blitter_release,
	.unlocked_ioctl = stm_blitter_ioctl,
	.mmap = stm_blitter_mmap,
};

struct stm_blitter_class_device *
stm_blitter_classdev_init(int                                i,
			  struct device                     *parent,
			  char                              *device_name,
			  void                              *dev_data,
			  const struct stm_blitter_file_ops *fops)
{
	struct stm_blitter_class_device *sbcd;
	int                              res;
	char name[20];

	sbcd = kzalloc(sizeof(*sbcd), GFP_KERNEL);
	if (sbcd == NULL)
		return ERR_PTR(-ENOMEM);

	sbcd->sbcd_devt = MKDEV(MAJOR(stm_blitter_devt), i);
	sbcd->fops = fops;

	cdev_init(&sbcd->sbcd_cdev, &stm_blitter_fops);
	sbcd->sbcd_cdev.owner = THIS_MODULE;
	res = cdev_add(&sbcd->sbcd_cdev, sbcd->sbcd_devt, 1);
	if (res)
		goto out;

	snprintf(name, sizeof(name), "%s.%u", device_name, i);
	sbcd->sbcd_device = device_create(stm_blitter_class, parent,
					  sbcd->sbcd_devt,
					  dev_data, name);
	if (IS_ERR(sbcd->sbcd_device)) {
		printk(KERN_ERR "Failed to create class device %d\n", i);
		res = PTR_ERR(sbcd->sbcd_device);
		goto out_cdev;
	}

	if (fops->debugfs) {
		sbcd->debugfs_dir = debugfs_create_dir(name,
						       stm_blitter_debugfs);
		if (IS_ERR(sbcd->debugfs_dir))
			sbcd->debugfs_dir = NULL;
		fops->debugfs(sbcd->sbcd_device, sbcd->debugfs_dir);
	}

	return sbcd;

out_cdev:
	cdev_del(&sbcd->sbcd_cdev);

out:
	kfree(sbcd);
	return ERR_PTR(res);
}

void
stm_blitter_classdev_deinit(struct stm_blitter_class_device *sbcd)
{
	debugfs_remove_recursive(sbcd->debugfs_dir);

	device_destroy(stm_blitter_class, sbcd->sbcd_devt);

	cdev_del(&sbcd->sbcd_cdev);

	kfree(sbcd);
}


int __init
stm_blitter_class_init(int n_devices)
{
	int res;

	stm_blitter_class = class_create(THIS_MODULE, "stm-blitter");
	if (IS_ERR(stm_blitter_class)) {
		printk(KERN_ERR "%s: couldn't create class device\n",
		       __func__);
		return PTR_ERR(stm_blitter_class);
	}
	stm_blitter_class->dev_attrs = NULL;

	res = alloc_chrdev_region(&stm_blitter_devt, 0, n_devices,
				  "stm-blitter");
	if (res < 0) {
		printk(KERN_ERR "%s: couldn't allocate device numbers\n",
		       __func__);
		return res;
	}

	stm_blitter_debugfs = debugfs_create_dir("stm-blitter", NULL);
	if (IS_ERR(stm_blitter_debugfs))
		stm_blitter_debugfs = NULL;

	return res;
}


void
stm_blitter_class_cleanup(int n_devices)
{
	debugfs_remove_recursive(stm_blitter_debugfs);

	class_destroy(stm_blitter_class);

	unregister_chrdev_region(stm_blitter_devt, n_devices);
}

#ifdef CONFIG_PM_RUNTIME
struct device *
stm_blitter_classdev_get_device(struct stm_blitter_class_device *sbcd)
{
    struct device * dev = NULL;

    if (sbcd)
	dev = sbcd->sbcd_device;

    return dev;
}
#endif
