#ifndef _PLATFORM_7141_H_
#define _PLATFORM_7141_H_

static struct resource h264pp_resource_7141[] = {
        [0] = { .start = 0xFE540000,
                .end   = 0xFE5FFFFF,
                .flags = IORESOURCE_MEM,     },
        [1] = { .start = ILC_IRQ(122),
                .end   = ILC_IRQ(122),
                .flags = IORESOURCE_IRQ, },
};

struct platform_device h264pp_device_7141 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources = ARRAY_SIZE(h264pp_resource_7141),
	.resource      = &h264pp_resource_7141
};

static struct platform_device *platform_7141[] __initdata = {
	&h264pp_device_7141,
};

static __init int platform_init_7141(void)
{

//    register_board_drivers();     
    return platform_add_devices(platform_7141,sizeof(platform_7141)/sizeof(struct platform_device*));

}

module_init(platform_init_7141);
#endif
