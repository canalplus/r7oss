/*
 * PDK7105 Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _PDK7105_H_
#define _PDK7105_H_

#if defined (CONFIG_SH_ST_HDK7105) || defined (CONFIG_SH_ST_PDK7105)

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
#include <linux/stm/pio.h>
#else
#include <linux/gpio.h>
#endif

static struct platform_device pti1_device = {
  .name    = "stm-pti",
  .id      = 0,
  .num_resources        = 1,

  .resource             = (struct resource[]) {
    {
      .start = 0xfe230000,
      .end   = 0xfe240000 - 1,
      .flags = IORESOURCE_MEM,
    }
  },
#if 0
  .dev          = {
    .platform_data      = (struct plat_frontend_config[]) {
      {
	.nr_channels      = 0,
      }
    }
  }
#endif
  .dev          = {
    .platform_data      = (struct plat_frontend_config[]) {

      {
	.nr_channels      = 1,
	.channels         = (struct plat_frontend_channel[]) {
	  {
	    .option         = STM_TSM_CHANNEL_0  | /*STM_SERIAL_NOT_PARALLEL |*/ STM_PACKET_CLOCK_VALID | STM_INVERT_CLOCK,
	    
	    .config         = (struct fe_config[]) {
	      {
		.demod_address = 0x1e,
		.tuner_id      = DVB_PLL_THOMSON_DTT759X,
		.tuner_address = 0x60
	      }
	    },
	    
#if 0
	    .demod_i2c_bus  = STM_NONE,
#else
	    .demod_attach   = demod_900_attach,
	    .demod_i2c_bus  = 0,
#endif
	    

	    .tuner_attach   = tuner_stv6110_attach,
	    .tuner_i2c_bus  = 1,
	    
	    .lnb_attach     = lnb_lnbh24_attach,
	    .lnb_i2c_bus    = 1,

	    .lock = 1,
	    .drop = 1,
	    
	    .pio_reset_bank = 0x3,
	    .pio_reset_pin  = 0x3
	  }
#if 0
 , {
		  .option         = STM_TSM_CHANNEL_1  | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID | STM_INVERT_CLOCK,
	    
	      .config         = (struct fe_config[]) {
	      {
		.demod_address = 0x1c,
		.tuner_id      = DVB_PLL_THOMSON_DTT759X,
		.tuner_address = 0x60
	      }
	    },
	    
	    .demod_attach   = demod_362_attach,
	    .demod_i2c_bus  = 3,
	    
	    .tuner_attach   = tuner_attach,
	    .tuner_i2c_bus  = 3,
	    
	    .lock = 1,
	    .drop = 1,
	    
	    .pio_reset_bank = 0x3,
	    .pio_reset_pin  = 0x3
	  }
#endif
	}
      }
    }
  }

};


static struct platform_device pti2_device = {
  .name    = "stm-pti",
  .id      = 1,
  .num_resources        = 1,

  .resource             = (struct resource[]) {
    {
      .start = 0xfe260000,
      .end   = 0xfe270000 - 1,
      .flags = IORESOURCE_MEM,
    }
  },

  .dev          = {
    .platform_data      = (struct plat_frontend_config[]) {
      {
	.nr_channels      = 0,
      }
    }
  }
};

static struct platform_device tsm_device = {
  .name    = "stm-tsm",
  .id      = -1,

  .num_resources        = 3,
  .resource             = (struct resource[]) {
    { /* TSM Config Registers */
      .start                = 0xfe242000,
      .end                  = 0xfe243000 - 1,
      .flags                = IORESOURCE_MEM,
    },
    { /* SWTS Data Injection Registers */
      .start                = 0xfe900000,
      .end                  = 0xfe901000 - 1,
      .flags                = IORESOURCE_MEM,
    },  
    { /* FDMA Request Line */
      .start = 7, /* could be 11*/
      .end   = 7,
      .flags = IORESOURCE_DMA,
    }
  },

  .dev          = {
    .platform_data      = (struct plat_tsm_config[]) {
      {
	.tsm_sram_buffer_size = 0x0c00,
	.tsm_num_channels = 8,
	.tsm_swts_channel = 4,
	.tsm_num_pti_alt_out  = 1,
	.tsm_num_1394_alt_out = 1,
      }
    }
  }
};


/* DISEQC */

static struct platform_device diseqc_device = {
  .name    = "stm-diseqc",
  .id      = -1,
  .dev          = {
    .platform_data      = (struct plat_diseqc_config[]) {
      {
	.diseqc_pio_bank      = 5,
	.diseqc_pio_pin       = 5
      }
    }
  },

  .num_resources        = 2,
  .resource             = (struct resource[]) {
    { /* DiseqC Config Registers */
      .start                = 0x18068000,
      .end                  = 0x18069000 - 1,
      .flags                = IORESOURCE_MEM,
    },
    { /* DiseqC Interrupt */
      .start = 0x81,
      .end   = 0x81,
      .flags = IORESOURCE_IRQ,
    }
  }
};


static struct platform_device *board_pdk7105[] __initdata = {
	&tsm_device,
//	&diseqc_device,
        &pti1_device,
        &pti2_device,
};

static int starwin_attach_adapter(struct i2c_adapter *adapter);
static int starwin_detach_adapter(struct i2c_adapter *adapter);

static struct i2c_driver starwin_driver = {
	.attach_adapter  = starwin_attach_adapter,
	.detach_adapter  = starwin_detach_adapter,
	.driver         = {
		.owner          = THIS_MODULE,
		.name           = "i2c_starwin_frontend"
	}
};

static int starwin_attach_adapter(struct i2c_adapter *adapter)
{	
	if (adapter->nr == 2) {
		const char i2c_cmd[] = { 0x10, 0x0, 0x99 }; // send to addr 0x80
		int ret;
		struct i2c_msg msg = { .addr = 0x40, .flags = 0, .buf = i2c_cmd, .len = 3};

		ret = i2c_transfer (adapter, &msg, 1); 

		if (ret!=1)
			printk("failed to talk to the starwin chip...\n");
	
	}
	return 0;
}

static int starwin_detach_adapter(struct i2c_adapter *client)
{
	return 0;
}

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
	// iomap so memory available
	volatile unsigned int *data = (unsigned int*)ioremap(0xfe000000,0x2000);

	// unlock CKGB_LCK to allow clockgen programming
	data[0x10/4] = 0xC0DE;

	printk("Enable alt out clk for Starwin DVB-CI Chip\n");
	    /* from STAPI
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x010,0xC0DE);
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x0B4,0x8);
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x010,0xC1A0);
	     SYS_SetBitsDev32LE (ST7105_CKG_BASE_ADDRESS+0x160,(1<<4)); // SYSTEM_CONFIG_24
	    */

	    data[0xB4/4] = 0x8;
	    data[0x1160/4] |= (1<<4);

	    
#if 1
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
	{
	static struct stpio_pin *starwin_reset_pin;
		            
	    // Reset Starwin toggle PIO 5 pin 7	    
	    printk("Reseting StarWin DVB-CI\n");
	    starwin_reset_pin = stpio_request_set_pin(5, 7, "starwin_reset",STPIO_OUT, 1);
	    stpio_set_pin(starwin_reset_pin, 1);
	    udelay(100);
	    stpio_set_pin(starwin_reset_pin, 0);	    
	    udelay(100);
	}
#else
	{
#define STARWIN_RESET_PIN	stm_gpio(5, 7)
		            
	    // Reset Starwin toggle PIO 5 pin 7	    
	    printk("Reseting StarWin DVB-CI\n");

	    gpio_request(STARWIN_RESET_PIN, "starwin_reset");
	    gpio_direction_output(STARWIN_RESET_PIN, 1);
	    gpio_set_value(STARWIN_RESET_PIN, 1);		// Not strictly necessary, the set direction output sets the value to 1 anyway
	    udelay(100);
	    gpio_set_value(STARWIN_RESET_PIN, 0);
	    udelay(100);
	}
#endif

	    i2c_add_driver(&starwin_driver);
#endif	    
	    data[0x10/4] = 0xC1A0; // lock CKGB_LOCK again now that we are done
	    
	iounmap(data);

    return platform_add_devices(board_pdk7105,sizeof(board_pdk7105)/sizeof(struct platform_device*));
}

#endif

#endif /* _HMS1_H_ */
