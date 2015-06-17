#ifndef __STM_BLITTER_CLASS_H__
#define __STM_BLITTER_CLASS_H__

#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/fs.h>


struct stm_blitter_class_device;

struct stm_blitter_file_ops {
	int  (* open)  (const struct device   *sbcd_device);
	void (* close) (const struct device   *sbcd_device);
	long (* ioctl) (const struct device   *sbcd_device,
			unsigned int           cmd,
			unsigned long          arg);
	int  (* mmap)  (const struct device   *sbcd_device,
			struct vm_area_struct *vma);

	void (* debugfs) (const struct device *sbcd_device,
			  struct dentry       *root);
};

int __init stm_blitter_class_init(int n_devices);
void stm_blitter_class_cleanup(int n_devices);

struct stm_blitter_class_device *
stm_blitter_classdev_init(int                                i,
			  struct device                     *parent,
			  char                              *device_name,
			  void                              *dev_data,
			  const struct stm_blitter_file_ops *fops);

void
stm_blitter_classdev_deinit(struct stm_blitter_class_device *dev);

#ifdef CONFIG_PM_RUNTIME
struct device *
stm_blitter_classdev_get_device(struct stm_blitter_class_device *dev);
#endif

#endif /* __STM_BLITTER_CLASS_H__ */
