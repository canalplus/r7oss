#ifndef _PLATFORM_710X_
#define _PLATFORM_710X_

static struct resource h264pp_resource_7109[] = {
        [0] = { .start = 0x19540000,
                .end   = 0x1954FFFF,
                .flags = IORESOURCE_MEM,     },
        [1] = { .start = 149,
                .end   = 149,
                .flags = IORESOURCE_IRQ, },
};


struct platform_device h264pp_device_7109 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources = ARRAY_SIZE(h264pp_resource_7109),
	.resource      = &h264pp_resource_7109
};

static struct platform_device *platform_7109[] __initdata = {
	&h264pp_device_7109,
};

static __init int platform_init_710x(void)
{
    //
    // Set Frequency Synthesiser FS1 channel3 equal the higher speed FS1 channel 2
    //

    printk( "Switching Preprocessor clock to full speed.\n" );
    {
	// iomap so memory available
	int *data = (int*)ioremap(0x19000000,0x1000);

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
	{ printk( "Switching Preprocessor clock to full speed FAILED\n" ); }

	iounmap(data);
    }

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID       (SYSCONF_BASE + 0x000)
    {
        
        int chip_revision;
        unsigned long chip_7109;
        
#if defined (CONFIG_KERNELVERSION) /* ST Linux 2.3 */
        chip_7109 = (boot_cpu_data.type == CPU_STB7109);
        chip_revision = boot_cpu_data.cut_major;
#else /* Assume STLinux 2.2 */
        unsigned long sysconf;
        sysconf = ctrl_inl(SYSCONF_DEVICEID);
        chip_7109 = (((sysconf >> 12) & 0x3ff) == 0x02c);
        chip_revision = (sysconf >> 28) +1;
#endif

        printk("Platform STx%s - cut %d\n",chip_7109?"7109":"7100",chip_revision);  
        
#ifdef BOARD_SPECIFIC_CONFIG
        register_board_drivers();     
#endif

        return platform_add_devices(platform_7109,sizeof(platform_7109)/sizeof(struct platform_device*));
    }
}

module_init(platform_init_710x);
#endif
