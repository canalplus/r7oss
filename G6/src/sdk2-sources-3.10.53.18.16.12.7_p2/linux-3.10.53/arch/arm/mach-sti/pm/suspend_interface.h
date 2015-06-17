/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *	   Sudeep Biswas	  <sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#ifndef __SUSPEND_INTERFACE__
#define __SUSPEND_INTERFACE__

#include <linux/list.h>

/*
 * This file exposes interface for registering notification of HPS wakeup
 * from other kernel modules. Other kernel modules should include this
 * header file.
 */
enum sti_hps_notify_ret {
	STI_HPS_RET_OK,
	STI_HPS_RET_AGAIN
};

/*
 * sti_hps_notify
 * irq: The irq number for which if the wakeup happens the callback must be
 * called sti_hps_notify_ret: the callback fucntion provided by the registering
 * client driver
 */
struct sti_hps_notify {
	struct list_head list;
	int irq;
	enum sti_hps_notify_ret (*notify)(void);
};

/*
 * sti_hps_register_notify can be used by drivers to register a callback
 * that will be called when the system is woken up from hps state,
 * the argument handler contains a field notify that will be called when
 * wakeup reason irq numer is same as that of the mentioned in the handler
 * argument
 */
int sti_hps_register_notify(struct sti_hps_notify *notify);

/*
 * sti_hps_unregister_notify can be used by drivers to unregister a callback
 * that is intially registered using the sti_register_pm_notify API
 */
int sti_hps_unregister_notify(struct sti_hps_notify *notify);

#endif /* __SUSPEND_INTERFACE__ */

