/*
 * -------------------------------------------------------------------------
 * pms.c
 * -------------------------------------------------------------------------
 * (C) STMicroelectronics 2009
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 * -------------------------------------------------------------------------
 * May be copied or modified under the terms of the GNU General Public
 * License v.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/stm/pms.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/parser.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>
#include <asm/clock.h>
#include "../base/power/power.h"

#undef dgb_print

#ifdef CONFIG_PMS_DEBUG
#define dgb_print(fmt, args...)  printk(KERN_INFO \
			"%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#define PMS_STATE_NAME_SIZE		24

enum pms_type {
#ifdef CONFIG_CPU_FREQ
	PMS_TYPE_CPU,
#endif
	PMS_TYPE_CLK,
	PMS_TYPE_DEV,
	PMS_TYPE_MAX
};

struct pms_object {
	int			type;
	union {
		void		*data;
		int		cpu_id; /* all the cpu managed by pms */
		struct clk	*clk;	/* all the clock  managed by pms */
		struct device	*dev;	/* all the device managed by pms */
		};
	struct list_head	node;
	struct list_head constraints; /* all the constraint on this object */
};

struct pms_state {
	struct kobject		kobj;
	struct kset		subsys;
	char			name[PMS_STATE_NAME_SIZE];
	struct list_head	node;  		/* the states list */
	struct list_head	constraints;	/* the constraint list */
};

struct pms_constraint {
	struct pms_object	*obj; 		/* the constraint owner */
	struct pms_state	*state;
	unsigned long		value;		/* the constraint value */
	struct list_head 	obj_node;
	struct list_head	state_node;
};

static LIST_HEAD(pms_state_list);
static DECLARE_MUTEX(pms_sem);

/*
 * The PMS uses 'PMS_TYPE_MAX lists' of objects
 * if CONFIG_CPU_FREQ is defined we have:
 *   - 0. cpus
 *   - 1. clocks
 *   - 2. devices
 * else we have:
 *   - 0. clocks
 *   - 1. devices
 */
static struct list_head pms_obj_lists[PMS_TYPE_MAX] = {
	LIST_HEAD_INIT(pms_obj_lists[0]),
	LIST_HEAD_INIT(pms_obj_lists[1]),
#ifdef CONFIG_CPU_FREQ
	LIST_HEAD_INIT(pms_obj_lists[2]),
#endif

};

static spinlock_t pms_lock = __SPIN_LOCK_UNLOCKED();

static struct pms_state **pms_active_states;
static int		  pms_active_nr = -1;	/* how many profile are active*/
static char 		 *pms_active_buf;	/* the last command line */

/*
 * Utility functions
 */

static inline char
*_strsep(char **s, const char *d)
{
	int i, len = strlen(d);
retry:
	if (!(*s) || !(**s))
		return NULL;
	for (i = 0; i < len; ++i) {
		if (**s != *(d+i))
			continue;
		++(*s);
		goto retry;
	}
	return strsep(s, d);
}

static inline int
clk_is_readonly(struct clk *clk)
{
	return (!clk->ops || !clk->ops->set_rate);
}
/*
 * End Utility functions
 */


static int
pms_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t ret = -EIO;
	struct subsys_attribute *sub_attr
		= container_of(attr, struct subsys_attribute, attr);
	struct kset *subsys
		= container_of(kobj, struct kset, kobj);
	if (sub_attr->show)
		ret = sub_attr->show(subsys, buf);
	return ret;
}

static ssize_t
pms_attr_store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	ssize_t ret = -EIO;
	struct subsys_attribute *sub_attr
		= container_of(attr, struct subsys_attribute, attr);
	struct kset *subsys
		= container_of(kobj, struct kset, kobj);
	if (sub_attr->store)
		ret = sub_attr->store(subsys, buf, count);
	return ret;
}

static struct sysfs_ops pms_sysfs_ops = {
	.show   = pms_attr_show,
	.store  = pms_attr_store,
};

static struct kobj_type ktype_pms = {
	.sysfs_ops = &pms_sysfs_ops,
};

decl_subsys(pms, &ktype_pms, NULL);

static ssize_t pms_constraint_show(struct kset *subsys, char *buf)
{
	int ret = 0;
	int i;
	struct pms_state *state =
		container_of(subsys, struct pms_state, subsys);
	struct pms_object *obj;
	struct pms_constraint *constr;

	dgb_print("\n");
	ret += sprintf(buf + ret, " -- state: %s --\n", state->name);
	for (i = 0; i < PMS_TYPE_MAX; ++i)
		list_for_each_entry(obj, &pms_obj_lists[i], node)
		list_for_each_entry(constr, &obj->constraints, obj_node) {
			if (constr->state == state) {
				switch (obj->type) {
#ifdef CONFIG_CPU_FREQ
				case PMS_TYPE_CPU:
					ret += sprintf(buf+ret,
						" + cpu: %10u @ %10u\n",
						(unsigned int)obj->cpu_id,
						(unsigned int)constr->value);
					break;
#endif
				case PMS_TYPE_CLK:
					ret += sprintf(buf+ret,
						" + clk: %10s @ %10u\n",
						obj->clk->name,
						(unsigned int)constr->value);
					break;
				case PMS_TYPE_DEV:
					ret += sprintf(buf+ret,
						" + dev: %10s is ",
						obj->dev->bus_id);
					if (constr->value == PM_EVENT_ON)
						ret += sprintf(buf + ret,
							"on\n");
					else
					      ret += sprintf(buf+ret, "off\n");
				}
			}
		}
	return ret;
}

static struct subsys_attribute pms_constraints =
	__ATTR(constraints, S_IRWXU , pms_constraint_show, NULL);

static ssize_t pms_valids_show(struct kset *subsys, char *buf)
{
	int ret = 0;
	struct pms_state *main_state =
			container_of(subsys, struct pms_state, subsys);
	struct pms_state *statep;
	dgb_print("\n");
	list_for_each_entry(statep, &pms_state_list, node) {
		if (statep == main_state)
			continue;
		if (!pms_check_valid(main_state, statep))
			ret += sprintf(buf+ret, " + %s\n", statep->name);
	}
	return ret;
}

static struct subsys_attribute pms_valids =
	__ATTR(valids, S_IRUGO, pms_valids_show, NULL);

struct pms_state *pms_state_get(const char *name)
{
	struct pms_state *statep;
	if (!name)
		return NULL;
	list_for_each_entry(statep, &pms_state_list, node)
		if (!strcmp(name, statep->name))
			return statep;
	return NULL;
}

static struct pms_object *pms_find_object(int type, void *data)
{
	struct pms_object *obj;
	struct list_head *head;

	head = &pms_obj_lists[type];

	list_for_each_entry(obj, head, node)
		if (obj->data == data)
			return obj;
	return NULL;
}

static int dev_match_address(struct device *dev, void *child)
{       return (dev == (struct device *)child); 	}

static inline int device_is_parent(struct device *parent, struct device *child)
{
	if (device_find_child(parent, child, dev_match_address))
		return 1;
	return 0;
}

static int pms_register_object(struct pms_object *obj)
{
	unsigned long flags;
	void *data_parent;
	int no_childs;
	struct pms_object *parent = NULL;
	struct pms_object *entry;
	struct list_head *head;

	dgb_print("\n");
	head = &pms_obj_lists[obj->type];
	switch (obj->type) {
	case PMS_TYPE_CLK:
		data_parent = (void *)obj->clk->parent;
		no_childs = list_empty(&obj->clk->childs);
		break;
	case PMS_TYPE_DEV:
		data_parent = (void *)obj->dev->parent;
		no_childs = list_empty(&obj->dev->klist_children.k_list);
		break;
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:
		data_parent = NULL;
		no_childs = (1 == 1);
		break;
#endif
	default:
		printk("Error Object type not supported\n");
		return -1;
	}

	spin_lock_irqsave(&pms_lock, flags);
	/* objects are in a sorted list*/
	/* 1. No parent... go in head */
	if (!data_parent) {
		list_add(&obj->node, head);
		goto reg_complete;
	}
	/* 2. with parent and no child... go in tail */
	if (no_childs) {
		list_add_tail(&obj->node, head);
		goto reg_complete;
	}
	/* 3. with parent and child... go after your parent */
	/* 3.1 check if the parent is registerd */
	list_for_each_entry(entry, head, node)
	if (entry->data == data_parent) {
		parent = entry;
		break;
	}
	if (!parent)
		/* the parent isn't registered...
		   go to the head (safe for child)...*/
		list_add(&obj->node, head);
	else {
		/* 3.1.1 added after the parent */
		obj->node.next = parent->node.next;
		obj->node.prev = &parent->node;
		parent->node.next = &obj->node;
		parent->node.prev = &obj->node;
		}
reg_complete:
	spin_unlock_irqrestore(&pms_lock, flags);
	return 0;
}

static struct pms_object *pms_create_object(int type, void *data)
{
	struct pms_object *obj = NULL;

	dgb_print("\n");
	if (pms_find_object(type, data)) {
		printk(KERN_INFO"pms: object already registered\n");
		goto err_0;
	}

	obj = (struct pms_object *)
		kmalloc(sizeof(struct pms_object), GFP_KERNEL);
	if (!obj)
		goto err_0;

	obj->data = data;
	obj->type = type;
	INIT_LIST_HEAD(&obj->constraints);
	if (pms_register_object(obj))
		goto err_1;
#if 0
	/* Initializes the constraints value for the state already registered */
	list_for_each_entry(statep, &pms_state_list, node) {
		struct pms_constraint *constr;
		constr = (struct pms_constraint *)
			kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
		constr->obj = obj;
		constr->state = statep;
		list_add(&constr->obj_node, &obj->constraints);
		list_add(&constr->state_node, &statep->constraints);
		switch (type) {
		case PMS_TYPE_CLK:
			constr->value = clk_get_rate(obj->clk);
			break;
		case PMS_TYPE_DEV:
			constr->value = PM_EVENT_ON;
			break;
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			constr->value =
				cpufreq_get((unsigned int)obj->cpu_id)*1000;
			break;
#endif
		}
	}
#endif

   return obj;
err_1:
   kfree(obj);
err_0:
   return NULL;
}

struct pms_object *pms_register_clock(struct clk *clk)
{
	struct pms_object *obj;
/*
 * If the object is already registerd then returns the object it-self
 */
	dgb_print("\n");
	if ((obj = pms_find_object(PMS_TYPE_CLK, (void *)clk)))
		return obj;
	return pms_create_object(PMS_TYPE_CLK, (void *)clk);
}
EXPORT_SYMBOL(pms_register_clock);

struct pms_object *pms_register_device(struct device *dev)
{
	struct pms_object *obj;
	dgb_print("\n");
	if ((obj = pms_find_object(PMS_TYPE_DEV, (void *)dev)))
		return obj;
	return pms_create_object(PMS_TYPE_DEV, (void *)dev);
}
EXPORT_SYMBOL(pms_register_device);

struct pms_object *pms_register_cpu(int cpu_id)
{
#ifdef CONFIG_CPU_FREQ
	struct pms_object *obj;
	dgb_print("\n");
	if (cpu_id >= NR_CPUS)
		return NULL;
	if ((obj = pms_find_object(PMS_TYPE_CPU, (void *)cpu_id)))
		return obj;
	return pms_create_object(PMS_TYPE_CPU, (void *)cpu_id);
#else
	return NULL;
#endif
}
EXPORT_SYMBOL(pms_register_cpu);

struct pms_object *pms_register_clock_by_name(char *clk_name)
{
	struct clk *clk;
	if (!(clk = clk_get(NULL, clk_name))) {
		dgb_print("Clock not declared\n");
		return NULL;
	 }
	dgb_print("cmd_add_clk: '%s'\n", clk_name);
	return pms_register_clock(clk);
}
EXPORT_SYMBOL(pms_register_clock_by_name);

static int pms_dev_match_name(struct device *dev, void *data)
{
	return (strcmp((char *)data, dev->bus_id) == 0) ? 1: 0 ;
}
struct bus_type *find_bus(char *name);

struct pms_object *pms_register_device_by_path(char *dev_path)
{
	struct bus_type *bus;
	struct device *dev;
	char *bus_name;
	char *dev_name;
	char *loc_dev_path = kzalloc(strlen(dev_path)+1, GFP_KERNEL);

	strncpy(loc_dev_path, dev_path, strlen(dev_path));
	dgb_print("\n");
	if (!(bus_name = _strsep((char **)&loc_dev_path, "/ \n\t\0"))) {
		dgb_print("Error on bus name\n");
		return NULL;
	}
	if (!(dev_name = _strsep((char **)&loc_dev_path, " \n\t\0"))) {
		dgb_print("Error on dev name\n");
		return NULL;
	}
	if (!(bus = find_bus(bus_name))) {
		dgb_print("Bus not declared\n");
		return NULL;
		}
	if (!(dev = bus_find_device(bus, NULL,
			(void *)dev_name, pms_dev_match_name))) {
		dgb_print("Device not found\n");
		return NULL;
		}
	kfree(loc_dev_path);
	return pms_register_device(dev);
}
EXPORT_SYMBOL(pms_register_device_by_path);

static int pms_unregister_object(int type, void *_obj)
{
	struct pms_object *obj;
	struct pms_constraint *constraint;
	if (!(obj = pms_find_object(type, _obj)))
		return -1;
	list_del(&obj->node); /* removed in the object list */
	list_for_each_entry(constraint, &obj->constraints, obj_node) {
		list_del(&constraint->state_node);
		kfree(constraint);
	}
	kfree(obj);
	return 0;
}

int pms_unregister_clock(struct clk *clk)
{
	return pms_unregister_object(PMS_TYPE_CLK, (void *)clk);
}
EXPORT_SYMBOL(pms_unregister_clock);

int pms_unregister_device(struct device *dev)
{
	return pms_unregister_object(PMS_TYPE_DEV, (void *)dev);
}
EXPORT_SYMBOL(pms_unregister_device);

int pms_unregister_cpu(int cpu_id)
{
#ifdef CONFIG_CPU_FREQ
	return pms_unregister_object(PMS_TYPE_CPU, (void *)cpu_id);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(pms_unregister_cpu);

int pms_set_wakeup(struct pms_object *obj, int enable)
{
	struct device *dev = (struct device *)obj->data;
	if (obj->type != PMS_TYPE_DEV)
		return -EINVAL;

	if (!device_can_wakeup(dev))
		return -EINVAL;
	dev->power.should_wakeup = enable ? 1 : 0;
	return 0;
}
EXPORT_SYMBOL(pms_set_wakeup);

int pms_get_wakeup(struct pms_object *obj)
{
	struct device *dev = (struct device *)obj->data;
	if (obj->type != PMS_TYPE_DEV)
		return -EINVAL;
	return dev->power.should_wakeup;
}
EXPORT_SYMBOL(pms_get_wakeup);

struct pms_state *pms_create_state(char *name)
{
	struct pms_state *state = NULL;
	unsigned long flags;
	int retval;
	dgb_print("\n");
	if (!name)
		return state;

	if ((state = pms_state_get(name)))
		return state;
	state = kzalloc(sizeof(struct pms_state), GFP_KERNEL);
	if (!state)
		return state;
	/* Initialise some fields... */
	strncpy(state->name, name, PMS_STATE_NAME_SIZE);
	INIT_LIST_HEAD(&state->constraints);
	kobject_init(&state->kobj);
	kobject_set_name(&state->subsys.kobj, "%s", state->name);
	subsys_set_kset(state, pms_subsys);
	/* add to the list */
	spin_lock_irqsave(&pms_lock, flags);
	list_add_tail(&state->node, &pms_state_list);
	spin_unlock_irqrestore(&pms_lock, flags);

	retval = subsystem_register(&state->subsys);
	if (retval)
		goto error;

	if (subsys_create_file(&state->subsys, &pms_constraints));
	if (subsys_create_file(&state->subsys, &pms_valids));
#if 0
/* Initializes the constraints value for all the
 * objects already registered
 */
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
	list_for_each_entry(obj, pms_obj_lists[idx], node) {
		struct pms_constraint *constr;
		constr = (struct pms_constraint *)
			kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
		constr->obj = obj;
		constr->state = state;
		list_add(&constr->obj_node, &obj->constraints);
		list_add(&constr->state_node, &state->constraints);
		switch (obj->type) {
		case PMS_TYPE_CLK:
			constr->value = clk_get_rate(obj->clk);
			break;
		case PMS_TYPE_DEV:
			constr->value = PM_EVENT_ON;
			break;
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			constr->value =
				cpufreq_get((unsigned int)obj->cpu_id) *1000;
			break;
#endif
		} /* switch */
	} /* list... */
#endif
	return state;

error:
	subsystem_unregister(&state->subsys);
	spin_lock_irqsave(&pms_lock, flags);
	list_del(&state->node);
	spin_unlock_irqrestore(&pms_lock, flags);
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(pms_create_state);

int pms_destry_state(struct pms_state *state)
{
	struct pms_constraint *constraint;
	unsigned long flags;
	if (!state)
		return -1;
	spin_lock_irqsave(&pms_lock, flags);
	list_del(&state->node);
	list_for_each_entry(constraint, &state->constraints, state_node) {
		list_del(&constraint->obj_node);
		kfree(constraint);
	}
	subsystem_unregister(&state->subsys);
	spin_unlock_irqrestore(&pms_lock, flags);
	kfree(state);
	return 0;
}
EXPORT_SYMBOL(pms_destry_state);

int pms_set_constraint(struct pms_state *state,
		struct pms_object *obj, unsigned long value)
{
	struct pms_constraint *constraint;
	dgb_print("\n");
	if (!obj || !state || obj->type >= PMS_TYPE_MAX)
		return -1;
	 dgb_print("state: %s - obj: %s - data: %u\n",
		state->name,
		(obj->type == PMS_TYPE_CLK) ? obj->clk->name: obj->dev->bus_id,
		(unsigned int)value);
	list_for_each_entry(constraint, &state->constraints, state_node)
		if (constraint->obj == obj) {
			constraint->value = value;
			return 0;
		}
/* 	there is no contraint already created
 *  	therefore I have to create it
 */
	constraint = (struct pms_constraint *)
		kmalloc(sizeof(struct pms_constraint), GFP_KERNEL);
	constraint->obj = obj;
	constraint->state = state;
	constraint->value = value;
	list_add(&constraint->obj_node, &obj->constraints);
	list_add(&constraint->state_node, &state->constraints);
   return 0;
}
EXPORT_SYMBOL(pms_set_constraint);

int pms_check_valid(struct pms_state *a, struct pms_state *b)
{
	struct pms_object *obj;
	struct pms_constraint *constraint;
	struct pms_constraint *ca, *cb;
	int idx;
	dgb_print("\n");
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
		list_for_each_entry(obj, &pms_obj_lists[idx], node) {
			ca = cb = NULL;
			list_for_each_entry(constraint, &obj->constraints,
				obj_node){
				if (constraint->state == a) ca = constraint;
				if (constraint->state == b) cb = constraint;
				if (ca && cb && ca->value != cb->value)
					/* this means both the states have a
					 * contraint on the object obj
					 */
					return -EPERM;
				}
		}
	return 0;
}
EXPORT_SYMBOL(pms_check_valid);
/*
 * Check if the constraint is already taken
 */
static int pms_check_constraint(struct pms_constraint *constraint)
{
	switch (constraint->obj->type) {
	case PMS_TYPE_CLK:
		return clk_get_rate(constraint->obj->clk)
				== constraint->value;
	case PMS_TYPE_DEV:
		return constraint->obj->dev->power.power_state.event
				== constraint->value;
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:
		return cpufreq_get((unsigned int)constraint->obj->cpu_id)*1000
				== constraint->value;
#endif
	default:
		printk(KERN_ERR"pms_check_constraint: "
			"Constraint type invalid...\n");
		return -1;
	}
	return 0;
}

static void pms_update_constraint(struct pms_constraint *constraint)
{
	struct clk *clk    = constraint->obj->clk;
	struct device *dev = constraint->obj->dev;

	switch (constraint->obj->type) {
#ifdef CONFIG_CPU_FREQ
	case PMS_TYPE_CPU:{
		struct cpufreq_policy policy;
		cpufreq_get_policy(&policy,
			(unsigned int)constraint->obj->cpu_id);
		if (!strncmp(policy.governor->name, "userspace", 8))
			cpufreq_driver_target(&policy,
				constraint->value/1000, 0);
		else
			printk(KERN_ERR "Try to force a cpu rate while using"
					" a not 'userspace' governor");
		}
	return;
#endif
	case PMS_TYPE_CLK:
		if (!constraint->value) {
			clk_disable(clk);
			return;
		}
		if (clk_get_rate(clk) == 0)
			clk_enable(clk);
		clk_set_rate(clk, constraint->value);
		return;
	case PMS_TYPE_DEV:
		if (constraint->value == PM_EVENT_ON) {
			printk(KERN_INFO "pms resumes device %s\n",
				dev->bus_id);
			resume_device(dev);
			/* ...and moves into the active list...*/
			mutex_lock(&dpm_list_mtx);
			list_move_tail(&dev->power.entry, &dpm_active);
			mutex_unlock(&dpm_list_mtx);
		} else if (!suspend_device(dev, PMSG_SUSPEND)) {
			printk(KERN_INFO "pms suspends device %s\n",
				dev->bus_id);
			/* ...and moves into the inactive list... */
			mutex_lock(&dpm_list_mtx);
			list_move(&dev->power.entry, &dpm_off);
			mutex_unlock(&dpm_list_mtx);
		}
		return;
	}/* switch... */
   return;
}

static int pms_active_state(struct pms_state *state)
{
	struct pms_constraint *constraint;
	struct pms_object *obj;
	int idx;

	dgb_print("\n");
	if (!state) {
		dgb_print("State NULL\n");
		return -1;
	}
   /* Check for global agreement */
#if 0
	list_for_each_entry(constraintp, &new_state->clk_constr, node) {
		struct clk *clk = constraintp->obj->clk;
		if (!constraintp->value)
			ret |= clk_disable(clk, 0);
		else {
			if (clk_get_rate(clk) == 0)
				ret |= clk_enable(clk, 0);
			else
				ret |= clk_set_rate(clk, constraintp->value);
			}
	if (ret)
		return -1;
	}
#endif
	/* 1.nd step... */
	for (idx = 0; idx < PMS_TYPE_MAX; ++idx)
		list_for_each_entry(obj, &pms_obj_lists[idx], node)
			list_for_each_entry(constraint, &obj->constraints,
					obj_node)
				if (constraint->state == state &&
				    !pms_check_constraint(constraint))
					pms_update_constraint(constraint);
	return 0;
}

int pms_set_current_states(char *buf)
{
	char *buf0, *buf1;
	int n_state = 0, i;
	struct pms_state **new_active_states;

	dgb_print("\n");
	if (!buf)
		return -1;
	dgb_print("Parsing of: %s\n", buf);

	buf0 = kmalloc(strlen(buf)+1, GFP_KERNEL);
	strcpy(buf0, buf);
	buf1 = buf0;
	for (; _strsep(&buf1, " ;\n") != NULL; ++n_state);
	dgb_print("Found %d state\n", n_state);

	new_active_states = (struct pms_state **)
			kmalloc(sizeof(struct pms_state *)*n_state, GFP_KERNEL);
	if (!new_active_states) {
		dgb_print("No Memory\n");
		return -ENOMEM;
	}
	strcpy(buf0, buf);
	buf1 = buf0;
	for (i = 0; i < n_state; ++i)
		if (!(new_active_states[i] = pms_state_get(
				_strsep(&buf1, " ;\n"))))
			goto error_pms_set_current_states;
#ifdef CONFIG_PMS_CHECK_GROUP
/* before we active the states we check if
 * there is no conclict in the set it-self
 */
	{
	int j;
	for (i = 0; i < n_state-1; ++i)
		for (j = i+1; j < n_state; ++j)
			if (pms_check_valid(new_active_states[i],
					new_active_states[j])) {
				printk(KERN_ERR
					"pms error: states invalid: %s - %s",
					new_active_states[i]->name,
					new_active_states[j]->name);
				 goto error_pms_set_current_states;
			}
	}
#endif
	/* now migrate to the new 'states' */
	pms_active_nr = n_state; 	/* how many state are active */
	for (i = 0; i < n_state; ++i)
		pms_active_state(*(new_active_states+i));/* active the state */

	if (pms_active_states)
		kfree(pms_active_states);	/* frees the previous states */
	pms_active_states = new_active_states;
	if (pms_active_buf)
		 kfree(pms_active_buf);
	strcpy(buf0, buf);
	pms_active_buf = buf0;
	return 0;

error_pms_set_current_states:
	dgb_print("Error in the sets of state required\n");
	kfree(new_active_states);
	kfree(buf0);
	return -EINVAL;
}
EXPORT_SYMBOL(pms_set_current_states);

char *pms_get_current_state(void)
{
	dgb_print("\n");
	if (pms_active_states)
		return pms_active_buf;
	return NULL;
}
EXPORT_SYMBOL(pms_get_current_state);

extern unsigned int wokenup_by;
int pms_global_standby(pms_standby_t state)
{
	int ret = -EINVAL;
	switch (state) {
#ifdef CONFIG_SUSPEND
	case PMS_GLOBAL_STANDBY:
	case PMS_GLOBAL_MEMSTANDBY:
		ret = pm_suspend(state == PMS_GLOBAL_STANDBY ?
			PM_SUSPEND_STANDBY : PM_SUSPEND_MEM);
		if (ret >= 0)
			ret = (int)wokenup_by;
		break;
#endif
#ifdef CONFIG_HIBERNATION
	case PMS_GLOBAL_HIBERNATION:
		ret = hibernate();
		break;
	case PMS_GLOBAL_MEMHIBERNATION:
		printk(KERN_ERR "PMS Error: MemHibernation is currently"
			" not supported.\n");
		break;
#endif
	default:
		printk(KERN_ERR "PMS Error: in %s state 0x%x not supported\n",
			__FUNCTION__, state);
	}

	return ret;
}
EXPORT_SYMBOL(pms_global_standby);

#ifdef CONFIG_STM_LPC
int pms_set_wakeup_timers(unsigned long long second)
{
	return 0;
}
#endif

enum {
	cmd_add_state = 1,
	cmd_add_clk,
	cmd_add_dev,
	cmd_add_clk_constr,
	cmd_add_dev_constr,
	cmd_add_cpu,
	cmd_add_cpu_constr,
};

static match_table_t tokens = {
	{ cmd_add_state,	 "state" 	},
	{ cmd_add_clk,	 "clock"	},
	{ cmd_add_dev,	 "device"	},
	{ cmd_add_clk_constr, "clock_rate"	},
	{ cmd_add_dev_constr, "device_state"	},
	{ cmd_add_cpu,	 "cpu"		},
	{ cmd_add_cpu_constr, "cpu_rate"	},
};

static ssize_t pms_control_store(struct kset *subsys,
				 const char *buf, size_t count)
{
   int token;
   char *p ;
   dgb_print("\n");
   while ((p = _strsep ((char **)&buf, " \t\n"))) {
   token = match_token(p, tokens, NULL);
   switch (token) {
   case cmd_add_state:{
	char *name;
	dgb_print("state command\n");
	if (!(name  = _strsep((char **)&buf, " \t\n")))
		continue;
	pms_create_state(name);
	};
   break;
   case cmd_add_clk: {
	char *clk_name;
	dgb_print("clk command\n");
	if (!(clk_name = _strsep((char **)&buf, " \n\t")))
		continue;
	pms_register_clock_by_name(clk_name);
	}
   break;
   case cmd_add_dev: {
	char *dev_path;
	if (!(dev_path = _strsep((char **)&buf, " \n\t")))
		continue;
	dgb_print("device command\n");
	pms_register_device_by_path(dev_path);
   }
   break;
   case cmd_add_cpu: {
	char *id_name;
	int id;
	if (!(id_name = _strsep((char **)&buf, " \t\n")))
		continue;
	id = simple_strtoul(id_name, NULL, 10);
	pms_register_cpu(id);
	}
   break;
   case cmd_add_clk_constr: {
	char *clk_name;	struct clk *clk;
	char *state_name;	struct pms_state *state;
	char *rate_name;	unsigned long rate;
	struct pms_object	*obj;
	/* State.Clock */
	dgb_print("Adding ...cmd_add_clock_constraint\n");
	if (!(state_name = _strsep((char **)&buf, ": \t\n")))
		continue;
	if (!(clk_name = _strsep((char **)&buf, " \n\t")))
		continue;
	if (!(rate_name = _strsep((char **)&buf, "- \t\n")))
		continue;
	rate = simple_strtoul(rate_name, NULL, 10);
	dgb_print("cmd_add_clock_constraint '%s.%s' @ %10u\n",
		state_name, clk_name, (unsigned int)rate);
	if (!(state = pms_state_get(state_name)) ||
	    !(clk = clk_get(NULL, clk_name))) {
		dgb_print("State and/or Clock not declared\n");
		continue;
		}
	if (clk_is_readonly(clk)) {
		dgb_print("Clock read only\n");
		continue;
		}
	/* check if the clock is in the pms */
	if (!(obj = pms_find_object(PMS_TYPE_CLK, (void *)clk))) {
		dgb_print("Clock not in the pms\n");
		continue;
		}
	pms_set_constraint(state, obj, rate);
	}
   break;
   case cmd_add_dev_constr: {
	  char *state_name;	struct pms_state *state;
	  char *bus_name;	struct bus_type  *bus;
	  char *dev_name;	struct device    *dev;
	  char *pmstate_name;	unsigned long pmstate;
	  struct pms_object	*obj;
	  /* State.Bus.Device on/off*/
	  dgb_print("Adding ...cmd_add_dev_constraint\n");
	  if (!(state_name = _strsep((char **)&buf, ": \t\n")))
		continue;
	  if (!(bus_name = _strsep((char **)&buf, "/ \n\t")))
		continue;
	  if (!(dev_name = _strsep((char **)&buf, " \n\t")))
		continue;
	  if (!(pmstate_name = _strsep((char **)&buf, " \n\t")))
		continue;
	  dgb_print("cmd_add_dev_constraint '%s\n", dev_name);
	  if (!(state = pms_state_get(state_name)) ||
	      !(bus = find_bus(bus_name))) {
		dgb_print("State and/or Bus not declared\n");
		continue;
		}
	  if (!(dev = bus_find_device(bus, NULL, (void *)dev_name,
				pms_dev_match_name))){
		dgb_print("Device not found\n");
		continue;
	  }
	  /* check if the device is in the pms */
	  if (!(obj = pms_find_object(PMS_TYPE_DEV, (void *)dev))) {
		dgb_print("Device not in the pms\n");
		continue;
	  }
	  if (!strcmp(pmstate_name, "on")) {
		pmstate = PM_EVENT_ON;
		dgb_print("Device state: %s.%s.%s state: on\n",
			state_name, bus_name, dev_name);
	  } else {
		pmstate = PM_EVENT_SUSPEND;
		dgb_print("Device state: %s.%s.%s state: off\n",
			state_name, bus_name, dev_name);
	  }
	  pms_set_constraint(state, obj, pmstate);
	}
   break;
   case cmd_add_cpu_constr: {
		char *state_name;	struct pms_state *state;
		char *cpu_id_name;	int cpu_id;
		char *rate_name;	unsigned long rate;
		struct pms_object *obj;
		dgb_print("Adding ...cmd_add_cpu_constraint\n");
		if (!(state_name = _strsep((char **)&buf, ": \t\n")))
			continue;
		if (!(cpu_id_name = _strsep((char **)&buf, " \t\n")))
			continue;
		if (!(rate_name = _strsep((char **)&buf, " \t\n")))
			continue;
		if (!(state = pms_state_get(state_name))) {
			dgb_print("State not declared\n");
			continue;
			}
		rate = simple_strtoul(rate_name, NULL, 10);
		cpu_id = simple_strtoul(cpu_id_name, NULL, 10);
#ifdef CONFIG_CPU_FREQ
		/* check if the cpu is in the pms */
		if (!(obj = pms_find_object(PMS_TYPE_CPU, (void *)cpu_id))) {
			dgb_print("CPU not in the pms\n");
		}
		dgb_print("CPU-%d constreint @ %u MHz\n",
			cpu_id, (unsigned int)rate);
		pms_set_constraint(state, obj, rate);
#endif
		}
   break;
   }/* switch */
   }/* while */
   return count;
}

static struct subsys_attribute pms_control_attr =
	__ATTR(control, S_IWUSR, NULL, pms_control_store);

static ssize_t pms_current_show(struct kset *subsys, char *buf)
{
	int idx, ret = 0;
	dgb_print("\n");
	if (!pms_active_states)
		return -1;
	ret += sprintf(buf+ret, "%s", pms_active_states[0]->name);
	for (idx = 1; idx < pms_active_nr; ++idx)
		ret += sprintf(buf+ret, ";%s", pms_active_states[idx]->name);
	ret += sprintf(buf+ret, "\n");
	return ret;
}

static char *pms_global_state_n[4] = {
	"pms_standby",
	"pms_memstandby",
	"pms_hibernation",
	"pms_memhibernation"
};

static ssize_t pms_current_store(struct kset *subsys,
				 const char *buf, size_t count)
{
	int i;
	int tmp;
	dgb_print("\n");
	if (!buf)
		return -1;
	for (i = 0; i < ARRAY_SIZE(pms_global_state_n); ++i) {
		if (!strcmp(pms_global_state_n[i], buf)) {
			tmp = pms_global_standby((pms_standby_t)i);
			dgb_print("PMS: woken up by %d\n", tmp);
			return count;
		}
	}

	if (pms_set_current_states((char *)buf) < 0)
		return -1;
	return count;
}

static struct subsys_attribute pms_current_attr =
	__ATTR(current_state, S_IRUSR | S_IWUSR,
		pms_current_show, pms_current_store);

static ssize_t pms_objects_show(struct kset *subsys, char *buf)
{
	struct pms_object *obj;
	int ret = 0;
	int i;
	for (i = 0; i < PMS_TYPE_MAX; ++i)
		list_for_each_entry(obj, &pms_obj_lists[i], node)
		switch (obj->type) {
#ifdef CONFIG_CPU_FREQ
		case PMS_TYPE_CPU:
			ret += sprintf(buf+ret, " + cpu: %10u\n",
					(unsigned int)obj->cpu_id);
			break;
#endif
		case PMS_TYPE_CLK:
			ret += sprintf(buf+ret, " + clk: %10s\n",
					obj->clk->name);
			break;
		case PMS_TYPE_DEV:
			ret += sprintf(buf+ret, " + dev: %10s\n",
					obj->dev->bus_id);
			break;
		}
	return ret;
}

static struct subsys_attribute pms_objects_attr =
	__ATTR(objects, S_IRUSR, pms_objects_show, NULL);

static struct subsys_attribute *pms_attr_group[] = {
	&pms_current_attr,
	&pms_control_attr,
	&pms_objects_attr,
};

static int __init pms_init(void)
{
	int i;
	dgb_print("pms initialization\n");
	if (subsystem_register(&pms_subsys));

	for (i = 0; i < ARRAY_SIZE(pms_attr_group); ++i)
		if (subsys_create_file(&pms_subsys, pms_attr_group[i]));
   return 0;
}

subsys_initcall(pms_init);
