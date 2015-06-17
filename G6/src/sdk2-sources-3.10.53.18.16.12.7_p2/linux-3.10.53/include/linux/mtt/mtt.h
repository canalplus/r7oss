/*
 *  Multi-Target Trace solution
 *
 *  DRIVER/INTERNAL IMPLEMENTATION HEADER

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
#ifndef _MTT_DRIVER_H_
#define _MTT_DRIVER_H_

#ifdef __KERNEL__
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/relay.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#endif

/*#define _MTT_DEBUG_*/
/*#define _MTT_TESTCRC_*/ /*for validation, add a CRC to packets.*/

#include <linux/mtt/mtt_api_internal.h>
#include <linux/mtt/mtt_ptcl.h>
#include <linux/mtt/mtt_ptcl_kptrace.h>

/*the interface version shall remain consistent with kptrace:
 MTT drivers expose basically a kptrace v4+ sysfs.*/
#define MTT_INTERFACE_VERSION ((MTT_KPT_VER_MAJOR<<16) \
			+ (MTT_KPT_VER_MINOR<<8) \
			+ MTT_KPT_VER_BUILD)

#define	MTT_VERSION_STRING "MTT driver v1.0"

/* ===vvv====== ARCH INDEPENDENT STRUCTURES ====vvv===== */

/* Implementation specific parts */
extern mtt_sys_kconfig_t *the_cfg;

struct mtt_outdrv_caps {
uint32_t devid;
};


/* =====^^^=== ARCH INDEPENDENT STRUCTURES ====^^^===== */

/* Use an ioctl to setup the mtt driver (passing a structure). */
#define MTT_IOC_MAGIC ('t') /* do NOT use capital 'T'! */

/* Update system config (deamon will writte this to init the driver */
#define MTT_IOC_SYSCONFIG _IOW(MTT_IOC_MAGIC, 0, mtt_sys_kconfig_t)
#define MTT_IOC_START	_IO(MTT_IOC_MAGIC, 1)
#define MTT_IOC_STOP	_IO(MTT_IOC_MAGIC, 2)
#define MTT_IOC_SET_COMP_FILT _IOW(MTT_IOC_MAGIC, 3, mtt_cmd_get_cfilt_t)
#define MTT_IOC_SET_CORE_FILT _IOW(MTT_IOC_MAGIC, 4, struct mtt_cmd_core_filt)
#define MTT_IOC_GET_COMP_INFO _IOR(MTT_IOC_MAGIC, 5, struct mtt_cmd_comp_info)
#define MTT_IOC_GET_CORE_INFO _IOR(MTT_IOC_MAGIC, 6, struct mtt_cmd_core_info)
#define MTT_IOC_GET_CAPS _IOR(MTT_IOC_MAGIC, 7, mtt_api_caps_t)
#define MTT_IOC_GET_OUTDRV_INFO _IOR(MTT_IOC_MAGIC, 8, struct mtt_outdrv_caps)
#define MTT_IOC_LAST 9

#ifdef __KERNEL__

/* Macros for debugging*/
#ifdef _MTT_DEBUG_
#define __unopt __attribute__((optimize("-O1")))
#define mtt_printk(format, args...) printk(format, ##args)
#define MTT_BUG_ON(cond) BUG_ON(cond)
#else
#define __unopt
#define mtt_printk(format, args...) do { } while (0)
#define MTT_BUG_ON(cond) do { } while (0)
#endif

#define MTTDEV_NAME "mtt"

extern struct list_head components;
void remove_components_tree(void);
int create_components_tree(struct device *parent);

/* =vv= COMPONENTS =vv= */

/*
 * This is our "object" that we will create a few of and register them with
 * sysfs.
 */
struct mtt_component_obj {
	struct kobject kobj; /* Also contains the name */
	struct list_head list;
	uint32_t id; /* Component ID consistent with MTT_COMPONENTID */
	uint32_t filter; /* 0x0 to disable component */
	uint32_t active_filter; /* The resulting testable value */
	void *private; /* Media dependent private configuration */
};

#define to_mtt_component_obj(x) container_of(x, struct mtt_component_obj, kobj)

struct mtt_component_obj *mtt_component_alloc(const uint32_t comp_id,
					 const char *comp_name,
					 int early);

void mtt_component_delete(struct mtt_component_obj *co);
const char *mtt_components_get_name(uint32_t id);

int mtt_component_info(struct mtt_cmd_comp_info *info);
struct mtt_component_obj *mtt_component_find(uint32_t id);
void mtt_component_snap_filter(uint32_t level);
void mtt_component_fixup_co_private(void);
void mtt_core_get_caps(mtt_api_caps_t *caps);

/* =^^= COMPONENTS =^^= */

struct mtt_module_dev_t {
	struct semaphore sem;
	struct cdev cdev;
	dev_t dev;
	int major; /* Major number assigned to our device driver */
};

extern mtt_sys_kconfig_t mtt_sys_config;
extern struct mtt_output_driver *mtt_cur_out_drv;

/* Fake tty output option for testing */
void mtt_drv_tty_write(mtt_packet_t *p, int lock);

/* RelayFS output driver write routine */
void mtt_drv_relay_write(mtt_packet_t *p, int lock);

mtt_return_t debug_sanity_check(void);
void debug_dump_sys_config(void);

/* Return a preallocated meta-data packet, or allocate*/
mtt_packet_t *mtt_pkt_alloc(uint32_t target);

mtt_packet_t *mtt_pkt_alloc_raw(mtt_target_type_t target, size_t len);

/* Initialize payload for a simple string */
uint32_t *mtt_pkt_get(struct mtt_component_obj *co, mtt_packet_t *p,
		  const mtt_trace_item_t type_info, uint32_t loc);

uint32_t *mtt_pkt_get_with_hint(struct mtt_component_obj *co, mtt_packet_t *p,
			    const mtt_trace_item_t type_info, uint32_t loc);

uint32_t *mtt_pkt_get_raw(struct mtt_component_obj *co, mtt_packet_t *p,
		      size_t len, uint32_t loc);

extern spinlock_t mttlib_lock;

extern uint32_t mtt_current_filter;

/* Pre-allocated packets*/
extern mtt_packet_t *mtt_pkts_cmd;

#define mtt_pkt_update_align32(p_len, val) (p_len += ((val+3) & 0x0000FFFC))

#define DRVLOCK	1 /* Tell the medium to manage locking */
#define APILOCK	0 /* Tell the medium the packet is locked */

struct mtt_output_driver {
	void (*write_func) (mtt_packet_t *p, int lock);
	int (*mmap_func) (struct file *filp, struct vm_area_struct *vma,
			 void *data);
	void *(*comp_alloc_func) (uint32_t comp_id, void *data);
	mtt_return_t (*config_func) (struct mtt_sys_kconfig *cfg);
	struct dentry *debugfs;
	struct list_head list;
	uint32_t guid;/* Has the nice property of being a
			 null-terminated string too look at KPT_GUID_DFS */
	void *private;
	mtt_return_t last_error;
	uint32_t devid;
	unsigned int last_ch_ker;
	unsigned int invl_ch_ker;
};

/* Register/unregister alternative drivers. */
void mtt_register_output_driver(struct mtt_output_driver *addme);
void mtt_unregister_output_driver(struct mtt_output_driver *delme);

/* Builtin output drivers **/
int __init mtt_pkt_init(void);
int __init mtt_relay_init(void);
void mtt_pkt_config(void);
void mtt_pkt_cleanup(void);
void mtt_pkt_put(mtt_packet_t *p);

void mtt_relay_cleanup(void);

int __init mtt_stm_init(void *);
void __exit mtt_stm_cleanup(void);

/* MTT Common components */
extern struct mtt_component_obj **mtt_comp_cpu;
extern struct mtt_component_obj **mtt_comp_kmux;

/* Notification transmit handlers. */
void mtt_get_cname_tx(uint32_t comp_id);
void mtt_version_tx(void);
void mtt_stop_tx(void);

#endif /*_KERNEL_*/
#endif /*_MTT_DRIVER_H_*/
