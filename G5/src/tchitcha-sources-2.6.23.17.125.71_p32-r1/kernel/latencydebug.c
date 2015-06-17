/*
 * latencydebug.c : Warn on late timer interrupts.
 *
 * Allows the kernel to warn when timer interrupts are delayed beyond a given
 * threshold. That can be a clue that interrupts may be being locked for
 * uncomfortable periods.
 *
 * Copyright (C) STMicroelectronics, 2009
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
 */

#include <linux/module.h>
#include <linux/profile.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

static struct timeval last;

/* Default to HZ + 1ms */
static int threshold = 1000000/HZ + 1000;

/* Functionality is off by default, and toggled via debugfs interface */
static int enabled;

static struct dentry *threshold_control;
static struct dentry *enabled_control;
static struct dentry *dir;

static int latency_debug_notify(struct pt_regs *regs)
{
	struct timeval curr;
	do_gettimeofday(&curr);
	if (last.tv_usec != -1 && (curr.tv_usec - last.tv_usec > threshold))  {
		printk(KERN_INFO
			"Warning: timer interrupt fired after %li " \
			"microseconds (HZ implies %d)\n",
				curr.tv_usec - last.tv_usec,
				1000000/HZ);
		dump_stack();
		do_gettimeofday(&curr);
	}
	last = curr;
	return 0;
}

static ssize_t enabled_read(struct file *filp, char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "System latency debugging is %s\n",
					enabled ? "enabled":"disabled");
	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t enabled_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[16];
	char *tmp;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	enabled = simple_strtol(buf, &tmp, 10);
	if (tmp == buf)
		return -EINVAL;

	if (enabled == 1) {
		last.tv_usec  = -1;
		register_timer_hook(latency_debug_notify);
	} else
		unregister_timer_hook(latency_debug_notify);

	return count;
}

static ssize_t threshold_read(struct file *filp, char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[128];

	if (enabled)
		snprintf(buf, sizeof(buf), "Warning when timer interrupts are "\
						"more than %d millseconds " \
						"apart\n", threshold);
	else
		snprintf(buf, sizeof(buf),
				"System latency debugging is disabled\n");

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t threshold_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[16];
	char *tmp;
	int new_threshold = 0;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	new_threshold = simple_strtol(buf, &tmp, 10);
	if (tmp == buf)
		return -EINVAL;

	threshold = new_threshold;

	return count;
}


static struct file_operations threshold_fops = {
	.read = threshold_read,
	.write = threshold_write
};

static struct file_operations enabled_fops = {
	.read = enabled_read,
	.write = enabled_write
};

static int latency_debug_init(void)
{
	dir = debugfs_create_dir("latency_debug", NULL);
	threshold_control = debugfs_create_file("threshold", 0644, dir, NULL,
							&threshold_fops);
	enabled_control = debugfs_create_file("enabled", 0644, dir, NULL,
							&enabled_fops);
	if (threshold_control < 0)
		printk("Failed to add debugfs file\n");

	return 0;
}

static void latency_debug_exit(void)
{
	if (enabled)
		unregister_timer_hook(latency_debug_notify);

	debugfs_remove(threshold_control);
	debugfs_remove(enabled_control);
	debugfs_remove(dir);
}

module_init(latency_debug_init);
module_exit(latency_debug_exit);
MODULE_LICENSE("GPL");

