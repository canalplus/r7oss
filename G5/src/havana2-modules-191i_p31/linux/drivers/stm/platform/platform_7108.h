#ifndef PLATFORM_7108_H
#define PLATFORM_7108_H

static struct resource h264pp_resource_7108[] = {
        [0] = { .start = 0xFDE00000,
                .end   = 0xFDE1FFFF,
                .flags = IORESOURCE_MEM,     },
        [1] = { .start = ILC_IRQ(88),
                .end   = ILC_IRQ(88),
                .flags = IORESOURCE_IRQ, },
};

struct platform_device h264pp_device_7108 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources = ARRAY_SIZE(h264pp_resource_7108),
	.resource      = &h264pp_resource_7108
};

struct platform_device cap_device_7108 = {
    .name          = "cap",
    .id            = -1,
    .num_resources =  1,
    .resource      = (struct resource[]) {
        [0] = { .start = 0xFD545E00,
                .end   = 0xFD545EFF,
                .flags = IORESOURCE_MEM,},
    },
};

static struct platform_device *platform_7108[] __initdata = {
	&h264pp_device_7108,
	&cap_device_7108,
};

static __init int platform_init_7108(void)
{
	
	// H264 PP clock now on clockgen A.
	// Recommended speed is 200 Mhz.
	// CLK_DIV_HS[2] - PLL0 HS 1000/5 = 200 MHz
	
/*
	// iomap so memory available
	int *data = (int*)ioremap(0xfd546000,0x1000);

	printk( "Switching Preprocessor clock to full speed.\n" );

	// unlock CKGB_LCK to allow clockgen programming
	data[0x10/4] = 0xC0DE;

	data[0x80/4] = data[0x70/4];
	data[0x84/4] = data[0x74/4];
	data[0x88/4] = data[0x78/4];
	data[0x8C/4] = data[0x7C/4];

	if ( (data[0x80/4] != data[0x70/4]) && 
             (data[0x84/4] != data[0x74/4]) &&
	     (data[0x88/4] != data[0x78/4]) && 
	     (data[0x8C/4] != data[0x7C/4]) )
		printk( "Switching Preprocessor clock to full speed FAILED\n" );

	iounmap(data);
*/	
	return platform_add_devices(platform_7108,sizeof(platform_7108)/sizeof(struct platform_device*));
}

module_init(platform_init_7108);

#endif
