#ifndef PLATFORM_7111_H
#define PLATFORM_7111_H

static struct resource h264pp_resource_7111[] = {
        [0] = { .start = 0xFE540000,
                .end   = 0xFE5FFFFF,
                .flags = IORESOURCE_MEM,     },
        [1] = { .start = evt2irq(0x14a0),
                .end   = evt2irq(0x14a0),
                .flags = IORESOURCE_IRQ, },
};

struct platform_device h264pp_device_7111 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources = ARRAY_SIZE(h264pp_resource_7111),
	.resource      = &h264pp_resource_7111
};

static struct platform_device *platform_7111[] __initdata = {
	&h264pp_device_7111,
};

static __init int platform_init_7111(void)
{

//    register_board_drivers();     
    return platform_add_devices(platform_7111,sizeof(platform_7111)/sizeof(struct platform_device*));

}

module_init(platform_init_7111);

#endif
