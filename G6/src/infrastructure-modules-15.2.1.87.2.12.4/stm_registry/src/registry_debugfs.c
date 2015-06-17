#include <linux/debugfs.h>
#include <linux/module.h>
#include "stm_registry.h"
#include "registry_internal.h"
static struct dentry *stm_reg_dir;
static struct dentry *stm_reg_entry;

static ssize_t registry_debugfs_dumptheregistry(struct file *f, char __user *buf, size_t count, loff_t *ppos);

static const struct file_operations registry_fops = {
	.owner = THIS_MODULE,
	.read = registry_debugfs_dumptheregistry
};

static ssize_t registry_debugfs_dumptheregistry(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	registry_internal_dump_registry();
	return 0;
}

int registry_create_debugfs(void)
{
	stm_reg_dir = debugfs_create_dir("stm_registry", NULL);
	if (!stm_reg_dir) {
		REGISTRY_ERROR_MSG(" <%s> : <%d> Failed to create stm directory folder!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	stm_reg_entry = debugfs_create_file("dump_registry", 0644, stm_reg_dir, 0, &registry_fops);
	if (!stm_reg_entry) {
		REGISTRY_ERROR_MSG("<%s> : <%d> Failed to create dentry for Dump_registry\n", __FUNCTION__, __LINE__);
		return -1;
	}
	return 0;
}

void registry_remove_debugfs(void)
{
	debugfs_remove(stm_reg_entry);
	debugfs_remove(stm_reg_dir);
}
