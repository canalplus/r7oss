/*
 * STMicroelectronics BDispII driver - linux debugfs support
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STM_BDISPII_DEBUGFS_H__
#define __STM_BDISPII_DEBUGFS_H__

void stm_bdisp2_add_debugfs(const struct device *sbcd_device,
			    struct dentry       *root);


#endif /* __STM_BDISPII_DEBUGFS_H__ */
