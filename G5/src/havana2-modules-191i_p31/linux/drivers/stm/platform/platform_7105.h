#ifndef PLATFORM_7105_H
#define PLATFORM_7105_H

static struct resource h264pp_resource_7105[] = {
        [0] = { .start = 0xFE540000,
                .end   = 0xFE5FFFFF,
                .flags = IORESOURCE_MEM,     },
        [1] = { .start = evt2irq(0x14c0),
                .end   = evt2irq(0x14c0),
                .flags = IORESOURCE_IRQ, },
};

struct platform_device h264pp_device_7105 = {
	.name          = "h264pp",
	.id            = -1,
	.num_resources = ARRAY_SIZE(h264pp_resource_7105),
	.resource      = &h264pp_resource_7105
};

#if 0
static struct plat_frontend_channel frontend_channels[] = {
  { STM_TSM_CHANNEL_0 | STM_DEMOD_AUTO | STM_TUNER_AUTO | STM_DISEQC_STONCHIP | STM_LNB_LNBH221 | STM_SERIAL_NOT_PARALLEL,
    .demod_i2c_bus     = 1,
    .demod_i2c_address = 0xff, /* Auto detect address */
    .demod_i2c_ad01    = 0x3,

    .tuner_i2c_bus     = 1,
    .tuner_i2c_address = 0xff,

    .lnb_i2c_bus       = 2,
    .lnb_i2c_address   = 0x0a,

    .lock = 0,
    .drop = 0,

    .pio_reset_bank    = 0x1,
    .pio_reset_pin     = 0x4
  }};

static struct plat_frontend_config frontend_config = {
  .diseqc_address       = 0x18068000,
  .diseqc_irq           = 0x81,
  .diseqc_pio_bank      = 5,
  .diseqc_pio_pin       = 5,

  .tsm_base_address     = 0xfe242000,

  .tsm_sram_buffer_size = 0xc00,
  .tsm_num_pti_alt_out  = 1,
  .tsm_num_1394_alt_out = 1,

  .nr_channels          = 1,
  .channels             = frontend_channels
};

static struct resource  pti_resource = {
  .start                = 0xfe230000,
  .end                  = 0xfe240000 - 1,
  .flags                = IORESOURCE_MEM,
};

static struct platform_device frontend_device = {
  .name    = "pti",
  .id      = -1,
  .dev     = {
	  .platform_data = &frontend_config,
  },
  .num_resources        = 1,
  .resource             = &pti_resource,
};
#endif

struct platform_device cap_device_7105 = {
    .name          = "cap",
    .id            = -1,
    .num_resources =  1,
    .resource      = (struct resource[]) {
        [0] = { .start = 0xFE20AE00,
                .end   = 0xFE20AEFF,
                .flags = IORESOURCE_MEM,},
//        [1] = { .start = ILC_IRQ(36), 
//                .end   = ILC_IRQ(37),
//        [1] = { .start = ILC_IRQ(17), 
//                .end   = ILC_IRQ(18),	   
//        [1] = { .start = evt2irq(0xba0), // BF 0xba0 - FF 0xbc0 
//                .end   = evt2irq(0xbc0),	   
//                .flags = IORESOURCE_IRQ },
    },
};


static struct platform_device *platform_7105[] __initdata = {
  //	&frontend_device,
	&h264pp_device_7105,
    &cap_device_7105,
};

static __init int platform_init_7105(void)
{

	// iomap so memory available
	int *data = (int*)ioremap(0xfe000000,0x1000);

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
	
//    register_board_drivers();     
    return platform_add_devices(platform_7105,sizeof(platform_7105)/sizeof(struct platform_device*));

}

module_init(platform_init_7105);

#endif
