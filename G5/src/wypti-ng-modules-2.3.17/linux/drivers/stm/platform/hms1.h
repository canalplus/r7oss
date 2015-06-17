/*
 * HMS1 Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _HMS1_H_
#define _HMS1_H_

#ifdef CONFIG_SH_ST_HMS1

#include <linux/stm/stm-frontend.h>

static struct platform_device pti1_device = {
  .name    = "stm-pti",
  .id      = 0,
  .num_resources        = 1,

  .resource             = (struct resource[]) {
    {
      .start = 0x19230000,
      .end   = 0x19240000 - 1,
      .flags = IORESOURCE_MEM,
    }
  },

  .dev          = {
    .platform_data      = (struct plat_frontend_config[]) {
      {
	.nr_channels      = 1,
	.channels         = (struct plat_frontend_channel[]) {
	  {
	    .option         = STM_TSM_CHANNEL_0 | STM_DISEQC_STONCHIP | STM_SERIAL_NOT_PARALLEL,
	    
	    .config         = (struct fe_config[]) {
	      {
		.demod_address = 0x1c,
		.tuner_id      = DVB_PLL_THOMSON_DTT759X,
		.tuner_address = 0x60
	      }
	    },
	    
	    .demod_attach   = demod_362_attach,
	    .demod_i2c_bus  = 1,
	    
	    .tuner_attach   = tuner_attach,
	    .tuner_i2c_bus  = 1,
	    
	    .lock = 0,
	    .drop = 0,
	    
	    .pio_reset_bank = 0x1,
	    .pio_reset_pin  = 0x4
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
      .start = 0x19260000,
      .end   = 0x19270000 - 1,
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
      .start                = 0x19242000,
      .end                  = 0x19243000 - 1,
      .flags                = IORESOURCE_MEM,
    },
    { /* SWTS Data Injection Registers */
      .start                = 0x1A300000,
      .end                  = 0x1A301000 - 1,
      .flags                = IORESOURCE_MEM,
    },  
    { /* FDMA Request Line */
      .start = 28,
      .end   = 28,
      .flags = IORESOURCE_DMA,
    }
  },

  .dev          = {
    .platform_data      = (struct plat_tsm_config[]) {
      {
	.tsm_sram_buffer_size = 0xc00,
	.tsm_num_channels = 6,
	.tsm_swts_channel = 3,
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


static struct platform_device *board_7109[] __initdata = {
	&tsm_device,
	&diseqc_device,
        &pti1_device,
        &pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
	return platform_add_devices(board_7109,sizeof(board_7109)/sizeof(struct platform_device*));
}

#endif

#endif /* _HMS1_H_ */
