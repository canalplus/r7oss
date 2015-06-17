#ifndef _PLATFORM_7200_H_
#define _PLATFORM_7200_H_

struct platform_device h264pp_device_7200 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources =  4,
	.resource      = (struct resource[]) {
		[0] = { .start = 0xFD900000,
			.end   = 0xFD900FFF,
			.flags = IORESOURCE_MEM, },
		[1] = { .start = ILC_IRQ(44),
			.end   = ILC_IRQ(44),
			.flags = IORESOURCE_IRQ, },             
		[2] = { .start = 0xFD920000,
			.end   = 0xFD920FFF,
			.flags = IORESOURCE_MEM, },
		[3] = { .start = ILC_IRQ(48),
			.end   = ILC_IRQ(48),
			.flags = IORESOURCE_IRQ, },
	},
};
struct platform_device cap_device_7200 = {
    .name          = "cap",
    .id            = -1,
    .num_resources =  1,
    .resource      = (struct resource[]) {
	[0] = { .start = 0xFDA41E00,
		.end   = 0xFDA41EFF,
		.flags = IORESOURCE_MEM,},
    },
};

struct platform_device monitor_device_7200 = {
    .name          = "stm-monitor",
    .id            = -1,
    .num_resources =  1,
    .resource      = (struct resource[]) {
		[0] = { .start = 0xfd81bf94, // FDMA Clock register, this can be seen by all
			.end   = 0xfd81bf94,
			.flags = IORESOURCE_MEM,},
    },
};

static struct platform_device *platform_7200[] __initdata = {
	&h264pp_device_7200,
	&cap_device_7200,
	&monitor_device_7200
};

static __init int platform_init_7200(void)
{

#ifdef BOARD_SPECIFIC_CONFIG
    register_board_drivers();
#endif

    return platform_add_devices(platform_7200,sizeof(platform_7200)/sizeof(struct platform_device*));
}

module_init(platform_init_7200);
#endif
