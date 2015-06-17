/*
 * PDK7105 Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _MB628_H_
#define _MB628_H_

#ifdef CONFIG_SH_ST_MB628

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
	    
	    .pio_reset_bank = 0x5,
	    .pio_reset_pin  = 0x4
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
	    
	    .pio_reset_bank = 0x5,
	    .pio_reset_pin  = 0x5
	  }
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
	.tsm_sram_buffer_size = 0xc00,
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

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
    return platform_add_devices(board_pdk7105,sizeof(board_pdk7105)/sizeof(struct platform_device*));
}

#endif

#endif /* _MB628_H_ */
