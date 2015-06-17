/*
 * Player2 Platform registration
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#if defined(CONFIG_SH_ST_CB102) || defined(CONFIG_SH_ST_CB103) || defined(CONFIG_SH_ST_CB104) || defined(CONFIG_SH_ST_CB104_MC)

#include <asm-sh/irq-ilc.h>
#include <linux/stm/sysconf.h>

struct platform_device dvp_device_7200 = {
    .name          = "dvp",
    .id            = -1,
    .num_resources =  2,
    .resource      = (struct resource[]) {
        [0] = { .start = 0xFDA40000,
                .end   = 0xFDA40FFF,
                .flags = IORESOURCE_MEM,},
        [1] = { .start = ILC_IRQ(46), 
                .end   = ILC_IRQ(46),
                .flags = IORESOURCE_IRQ },
    },
};

static struct platform_device *platform_cb10x[] __initdata = {
        &dvp_device_7200,
};

#define BOARD_SPECIFIC_CONFIG
static int __init register_board_drivers(void)
{	
	static int *syscfg40;
	static int *syscfg7;
        
	// Configure DVP
	syscfg40 = ioremap(0xfd7041a0,4);
	*syscfg40 = 1 << 16;
	
	syscfg7 = ioremap(0xfd70411c,4);
	*syscfg7 = *syscfg7 | (1 << 29);

	return platform_add_devices(platform_cb10x,sizeof(platform_cb10x)/sizeof(struct platform_device*));

}
#endif
