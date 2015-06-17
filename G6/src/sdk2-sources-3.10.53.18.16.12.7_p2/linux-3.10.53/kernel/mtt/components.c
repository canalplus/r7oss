/*
 *  Multi-Target Trace solution
 *
 *  MTT - SYSTEM TRACE MODULES SUPPORT.
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
 *
 * parts:
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 *
 * Released under the GPL version 2 only.
 *
 */
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/mtt/mtt.h>

#include <linux/hardirq.h>

#ifdef CONFIG_KPTRACE
#include <linux/mtt/kptrace.h>
#endif

LIST_HEAD(mtt_comp_list);

static struct mutex comp_mutex;

struct mtt_component_obj **mtt_comp_cpu;
struct mtt_component_obj **mtt_comp_kmux;

/* a custom attribute that works just for a struct mtt_component_obj. */
struct mtt_component_attribute {
	struct attribute attr;
	 ssize_t(*show) (struct mtt_component_obj *mtt_component,
			 struct mtt_component_attribute *attr, char *buf);
	 ssize_t(*store) (struct mtt_component_obj *mtt_component,
			  struct mtt_component_attribute *attr,
			  const char *buf, size_t count);
};
#define to_mtt_component_attr(x)\
	container_of(x, struct mtt_component_attribute, attr)

static struct mtt_component_obj *create_mtt_component_obj(int comp_id,
							const char *name,
							int early);

static ssize_t mtt_component_attr_show(struct kobject *kobj,
				       struct attribute *attr, char *buf)
{
	struct mtt_component_attribute *attribute;
	struct mtt_component_obj *mtt_component;

	attribute = to_mtt_component_attr(attr);
	mtt_component = to_mtt_component_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(mtt_component, attribute, buf);
}

/*
 * Just like the default show function above, but this one is for when the
 * sysfs "store" is requested (when a value is written to a file.)
 */
static ssize_t mtt_component_attr_store(struct kobject *kobj,
					struct attribute *attr, const char *buf,
					size_t len)
{
	struct mtt_component_attribute *attribute;
	struct mtt_component_obj *mtt_component;

	attribute = to_mtt_component_attr(attr);
	mtt_component = to_mtt_component_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(mtt_component, attribute, buf, len);
}

/* Our custom sysfs_ops that we will associate with our ktype later on */
static const struct sysfs_ops mtt_component_sysfs_ops = {
	.show = mtt_component_attr_show,
	.store = mtt_component_attr_store,
};

/*
 * The release function for our object.  This is REQUIRED by the kernel to
 * have.  We free the memory held in our object here.
 *
 * NEVER try to get away with just a "blank" release function to try to be
 * smarter than the kernel.  Turns out, no one ever is...
 */
static void mtt_component_release(struct kobject *kobj)
{
	struct mtt_component_obj *mtt_component;
	mtt_component = to_mtt_component_obj(kobj);
	kfree(mtt_component);
}

/*
 * allocate the management structure for a component
 * comp_id: the component ID as defined in mtt_api_internal.h
 * comp_name: NULL or a short string to name the component.
 * early: for early setup code.
 */
struct mtt_component_obj *mtt_component_alloc(uint32_t comp_id,
					      const char *comp_name,
					      int early)
{
	struct mtt_component_obj *co;

	/* This will try to allocate memory with GFP_KERNEL
	 * there is no reason it will be called from a context
	 * that cannot sleep
	 */
	BUG_ON(in_interrupt());

	mutex_lock(&comp_mutex);

	if (comp_id == MTT_COMP_ID_ANY) {
		uint32_t last_kernel_compid;
		uint32_t invl_kernel_compid;

		/*
		 * This should be handle per output driver
		 * (where the constraints are.
		 * output driver knows which channels to use.
		 */
		if (mtt_cur_out_drv) {
			last_kernel_compid = mtt_cur_out_drv->last_ch_ker;
			invl_kernel_compid = mtt_cur_out_drv->invl_ch_ker;
		} else {
			return NULL;
		}

		/* use any free ID */
		comp_id = last_kernel_compid;
		if (last_kernel_compid < invl_kernel_compid) {
			last_kernel_compid++;
			mtt_cur_out_drv->last_ch_ker = last_kernel_compid;
		}
	}

	/* client code component: check first if already opened. */
	if ((comp_id & MTT_COMPID_ST) == 0) {
		list_for_each_entry(co, &mtt_comp_list, list) {
			if (co->id == comp_id) {
				printk(KERN_DEBUG
				       "mtt: comp. 0x%x open multiple times.",
				       comp_id);
				mutex_unlock(&comp_mutex);
				return co;
			}
		}
	}

	co = create_mtt_component_obj(comp_id, comp_name, early);

	if (co) {
		/* Let the output driver prepare a channel
		 * or whatever it needs */
		if (mtt_cur_out_drv && (mtt_cur_out_drv->comp_alloc_func))
			co->private =
			    mtt_cur_out_drv->comp_alloc_func(comp_id,
							     mtt_cur_out_drv->
							     private);

		if ((comp_id & MTT_COMPID_ST) == 0)
			/* Send notification frame */
			mtt_get_cname_tx(comp_id);
	}

	mutex_unlock(&comp_mutex);
	return co;
}

/* Called when a new driver is configured */
void mtt_component_fixup_co_private(void)
{
	struct list_head *p;
	struct mtt_component_obj *co;

	MTT_BUG_ON(mtt_cur_out_drv == NULL);

	/* The output driver may not require a post-alloc
	 * fixup. */
	if (!mtt_cur_out_drv->comp_alloc_func)
		return;

	/* Make sure we don't miss a component being added
	 * with the previous driver set.
	 */
	mutex_lock(&comp_mutex);

	list_for_each(p, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);
		co->private = mtt_cur_out_drv->comp_alloc_func(co->id,
					mtt_cur_out_drv->private);
	}

	mutex_unlock(&comp_mutex);
}

/* The "mtt_component" file where the ID variable
 * is read from and written to.
 */
static ssize_t id_show(struct mtt_component_obj *mtt_component_obj,
		       struct mtt_component_attribute *attr, char *buf)
{
	return sprintf(buf, "id = %x\n", mtt_component_obj->id);
}

static struct mtt_component_attribute id_attribute =
__ATTR(id, S_IRUGO, id_show, NULL);

static ssize_t level_show(struct mtt_component_obj *co,
			  struct mtt_component_attribute *attr, char *buf)
{
	return sprintf(buf, "level filter = %x\n", co->filter);
}

/* Store the level, with radix 16 as it is a bit mask. */
static ssize_t level_store(struct mtt_component_obj *co,
			   struct mtt_component_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned long new_filter;
	int ret = kstrtoul(buf, 16, &new_filter);

	if (!ret) {
		co->filter = new_filter;
		co->active_filter = new_filter;
	}

	return count;
}

static struct mtt_component_attribute level_attribute =
__ATTR(filter, S_IRUGO | S_IWUSR, level_show, level_store);

static struct attribute *mtt_component_default_attrs[] = {
	&id_attribute.attr,
	&level_attribute.attr,
	NULL,
};

static struct kobj_type mtt_component_ktype = {
	.sysfs_ops = &mtt_component_sysfs_ops,
	.release = mtt_component_release,
	.default_attrs = mtt_component_default_attrs,
};

static struct kset *component_kset;

static struct mtt_component_obj *create_mtt_component_obj(int comp_id,
							  const char *name,
							  int early)
{
	struct mtt_component_obj *mtt_component;
	int retval;

	/* Allocate the memory for the whole object */
	mtt_component = kzalloc(sizeof(*mtt_component), GFP_KERNEL);
	if (!mtt_component)
		return NULL;

	mtt_component->id = comp_id;

	/* Init component filter from mtt_sys_config filter */
	mtt_component->filter = MTT_LEVEL_ALL;
	mtt_component->active_filter = mtt_sys_config.filter;

	/* Some early or internal components will not have a kobj */
	if (!early) {
		mtt_component->kobj.kset = component_kset;
		/* Initialize and add the kobject to the kernel. */
		if (name)
			retval = kobject_init_and_add(&mtt_component->kobj,
					      &mtt_component_ktype,
					      NULL, "%s", name);
		else
			retval = kobject_init_and_add(&mtt_component->kobj,
					      &mtt_component_ktype,
					      NULL, "comp_%05d",
					      comp_id & 0xffff);
		if (retval) {
			kobject_put(&mtt_component->kobj);
			kfree(mtt_component);
			return NULL;
		}

		kobject_uevent(&mtt_component->kobj, KOBJ_ADD);
	}

	/* A mutex is held by the caller */
	list_add_tail(&mtt_component->list, &mtt_comp_list);

	return mtt_component;
}

void mtt_component_delete(struct mtt_component_obj *co)
{
	mutex_lock(&comp_mutex);

	if (co->kobj.state_initialized) {
		kobject_uevent(&co->kobj, KOBJ_REMOVE);
		kobject_put(&co->kobj);
	}

	list_del(&co->list);

	mutex_unlock(&comp_mutex);
}

/* Reset the filter level for all components to the given value,
 * but if the components has a custom filter value already.
 * this implements snapping to the core.filter. */
void mtt_component_snap_filter(uint32_t filter)
{
	struct list_head *p;
	struct mtt_component_obj *co;

	list_for_each(p, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);

		/* Turning off the core does turn off each component. */
		if ((co->filter == MTT_LEVEL_ALL) || (filter == MTT_LEVEL_NONE))
			co->active_filter = filter;
	}
}

/* Reply to IOC_GET_COMP_INFO */
int mtt_component_info(struct mtt_cmd_comp_info *info /*.id is inout */)
{
	struct list_head *p;
	struct mtt_component_obj *co;

	MTT_BUG_ON(!info);

	list_for_each(p, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);
		if (co && (info->id == co->id)) {
			/* Fill the component info. */
			info->filter = co->filter;
			strncpy(info->name, co->kobj.name, sizeof(info->name));
			return 0;
		}
	}
	return -EINVAL;
}

int __init create_components_tree(struct device *parent)
{
	int core;
	int err = 0;

	/* Pay attention we modify char #8 in the strings below. */
	char name_kmux[] = "mtt_kmux0";
	char name_cpus[] = "mtt_cpus0";

	mutex_init(&comp_mutex);

	/* Create a kset with the name of "components",
	 * located under /sys/kernel/
	 */
	component_kset =
	    kset_create_and_add("components", NULL, &(parent->kobj));
	if (!component_kset)
		return -ENOMEM;

	mtt_comp_kmux = kmalloc(num_possible_cpus()
				* sizeof(struct mtt_component_obj *),
				GFP_KERNEL);

	mtt_comp_cpu = kmalloc(num_possible_cpus()
			       * sizeof(struct mtt_component_obj *),
			       GFP_KERNEL);

	if (!mtt_comp_kmux || !mtt_comp_cpu)
		return -ENOMEM;

	for (core = 0; (core < num_possible_cpus()) && !err; core++) {
		name_kmux[sizeof(name_kmux)-2] = 0x30 + core;
		name_cpus[sizeof(name_cpus)-2] = 0x30 + core;
		mtt_comp_kmux[core] = mtt_component_alloc(MTT_COMPID_LIN_KMUX
						+ MTT_COMPID_MAOFFSET * core,
						name_kmux, 0);
		mtt_comp_cpu[core] = mtt_component_alloc(MTT_COMPID_LIN_DATA
						+ MTT_COMPID_MAOFFSET * core,
						name_cpus, 0);
		if (!mtt_comp_kmux[core] || !mtt_comp_cpu[core])
			err = 1;
	}

	if (err) {
		/* Try release whatever we got */
		printk(KERN_ERR "mtt: releasing component tree (ENOMEM)");
		remove_components_tree();
		return -ENOMEM;
	}

#ifdef CONFIG_KPTRACE
	mtt_kptrace_comp_alloc();
#endif

	return 0;
}

const char *mtt_components_get_name(uint32_t id)
{
	struct list_head *p;
	struct mtt_component_obj *co;

	list_for_each(p, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);
		if (co->id == id)
			return co->kobj.name;
	}

	return "pUnk";
}

struct mtt_component_obj *mtt_component_find(uint32_t id)
{
	struct list_head *p;
	struct mtt_component_obj *co;

	list_for_each(p, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);
		if (co->id == id)
			return co;
	}
	return NULL;
}

void remove_components_tree(void)
{
	struct list_head *p, *tmp;
	struct mtt_component_obj *co;

	list_for_each_prev_safe(p, tmp, &mtt_comp_list) {
		co = list_entry(p, struct mtt_component_obj, list);
		mtt_printk(KERN_DEBUG "mtt: removing %s\n", co->kobj.name);
		mtt_component_delete(co);
	}

	kfree(mtt_comp_kmux);
	kfree(mtt_comp_cpu);

	kset_unregister(component_kset);
}
