/*
 * STMicroelectronics BDispII driver - nodes
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BDISPII_NODES_H__
#define __BDISPII_NODES_H__

char *
bdisp2_sprint_node(const struct _BltNodeGroup00 *config_general,
		   bool                          register_dump);


#endif /* __BDISPII_NODES_H__ */
