/*
 * cb180 Platform registration
 *
 * Copyright (C) 2009/2010 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *         Andre' Draszik <andre.draszik@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#ifndef __cb180_h__
#define __cb180_h__

#if defined CONFIG_SH_ST_CB180 || defined CONFIG_SH_ST_DK263790_7105 || defined CONFIG_SH_ST_DK263790

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))    // STLinux 2.4
#include <linux/gpio.h>
#else
#include <linux/stm/pio.h>
#endif

static struct platform_device pti1_device = {
  .name = "stm-pti",
  .id = 0,

  .num_resources = 1,
  .resource = (struct resource[]) {
    [0] = { .start = 0xfe230000,
            .end   = 0xfe240000 - 1,
            .flags = IORESOURCE_MEM }
  },

  .dev = {
    .platform_data = (struct plat_frontend_config[]) {
      [0] = { .nr_channels = 4,
              .channels = (struct plat_frontend_channel[]) {
                [0] = { .option = STM_TSM_CHANNEL_0 | STM_SERIAL_NOT_PARALLEL,

                        .demod_attach = NULL,
                        .demod_i2c_bus = STM_NONE,

                        .tuner_attach = NULL,
                        .tuner_i2c_bus = STM_NONE,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x0,
                        .pio_reset_pin = 0x0
                      },
                [1] = { .option = STM_TSM_CHANNEL_1 | STM_SERIAL_NOT_PARALLEL,

                        .demod_attach = NULL,
                        .demod_i2c_bus = STM_NONE,

                        .tuner_attach = NULL,
                        .tuner_i2c_bus = STM_NONE,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x0,
                        .pio_reset_pin = 0x0
                      },
                [2] = { .option = STM_TSM_CHANNEL_2 | STM_SERIAL_NOT_PARALLEL,

                        .demod_attach = NULL,
                        .demod_i2c_bus = STM_NONE,

                        .tuner_attach = NULL,
                        .tuner_i2c_bus = STM_NONE,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x0,
                        .pio_reset_pin = 0x0
                      },
                [3] = { .option = STM_TSM_CHANNEL_3 | STM_SERIAL_NOT_PARALLEL,

                        .demod_attach = NULL,
                        .demod_i2c_bus = STM_NONE,

                        .tuner_attach = NULL,
                        .tuner_i2c_bus = STM_NONE,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x0,
                        .pio_reset_pin = 0x0
                      }
              }
            }
    }
  }
};


static struct platform_device pti2_device = {
  .name = "stm-pti",
  .id = 1,

  .num_resources = 1,
  .resource = (struct resource[]) {
    [0] = { .start = 0xfe260000,
            .end   = 0xfe270000 - 1,
            .flags = IORESOURCE_MEM
          }
  },

  .dev = {
    .platform_data = (struct plat_frontend_config[]) {
      [0] = { .nr_channels = 0 }
    }
  }
};

static struct platform_device tsm_device = {
  .name = "stm-tsm",
  .id = -1,

  .num_resources = 3,
  .resource = (struct resource[]) {
    [0] = { /* TSM Config Registers */
            .start = 0xfe242000,
            .end   = 0xfe243000 - 1,
            .flags = IORESOURCE_MEM
          },
    [1] = { /* SWTS Data Injection Registers */
            .start = 0xfe900000,
            .end   = 0xfe901000 - 1,
            .flags = IORESOURCE_MEM
          },
    [2] = { /* FDMA Request Line */
            .start = 7, /* could be 11 */
            .end   = 7,
            .flags = IORESOURCE_DMA
          }
  },

  .dev = {
    .platform_data = (struct plat_tsm_config[]) {
      [0] = { .tsm_sram_buffer_size = 0x0c00,
              .tsm_num_channels = 8,
              .tsm_swts_channel = 4,
              .tsm_num_pti_alt_out  = 1,
              .tsm_num_1394_alt_out = 1,
            }
    }
  }
};



static struct platform_device *board_cb180[] __initdata = {
  &tsm_device,
  &pti1_device,
  &pti2_device,
};


#define BOARD_SPECIFIC_CONFIG
static int
register_board_drivers(void)
{
  printk ("frontend: cb180\n");

  return platform_add_devices (board_cb180, ARRAY_SIZE (board_cb180));
}

#endif /* CONFIG_SH_ST_CB180 || CONFIG_SH_ST_DK263790_7105 || CONFIG_SH_ST_DK263790 */

#endif /* __cb180_h__ */
