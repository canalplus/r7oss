/*
 * mb680 Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _MB680_H_
#define _MB680_H_

#ifdef CONFIG_SH_ST_MB680

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>

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
#else
  .dev          = {
    .platform_data      = (struct plat_frontend_config[]) {
      {
	.nr_channels      = 2,
	.channels         = (struct plat_frontend_channel[]) {
	  {
		  .option         = STM_TSM_CHANNEL_0  | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID | STM_INVERT_CLOCK,
	    
	    .config         = (struct fe_config[]) {
	      {
		.demod_address = 0x1e,
		.tuner_id      = DVB_PLL_THOMSON_DTT759X,
		.tuner_address = 0x60
	      }
	    },
	    
	    .demod_attach   = demod_362_attach,
	    .demod_i2c_bus  = 2,
	    
	    .tuner_attach   = tuner_attach,
	    .tuner_i2c_bus  = 2,
	    
	    .lock = 1,
	    .drop = 1,
	    
	    .pio_reset_bank = 0x3,
	    .pio_reset_pin  = 0x2
	  } , {
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
	}
      }
    }
  }
#endif
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


static struct platform_device *board_mb680[] __initdata = {
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

    printk( "Switching Preprocessor clock to full speed.\n" );
    {
	// iomap so memory available
	volatile unsigned int *data = (unsigned int*)ioremap(0xfe000000,0x2000);

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

	    printk("Enable alt out clk for Starwin DVB-CI Chip\n");
	    /* from STAPI
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x010,0xC0DE);
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x0B4,0x8);
	     SYS_WriteRegDev32LE(ST7105_CKG_BASE_ADDRESS+0x010,0xC1A0);
	     SYS_SetBitsDev32LE (ST7105_CKG_BASE_ADDRESS+0x160,(1<<4)); // SYSTEM_CONFIG_24
	    */

#if 1
	    
	    data[0xB4/4] = 0x8;
//	    printk("0xc== %x\n",data[0x1160/4]);
	    data[0x1160/4] |= (1<<4);
//	    printk("0x1c== %x\n",data[0x1160/4]);

#endif	    
	    data[0x10/4] = 0xC1A0; // lock CKGB_LOCK again now that we are done
	    
	iounmap(data);
    }	


    return platform_add_devices(board_mb680,sizeof(board_mb680)/sizeof(struct platform_device*));
}

#endif

#endif /* _MB680_H_ */
