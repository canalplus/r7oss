/***********************************************************************
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stm/soc.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#if defined(CONFIG_ARM)
#include <asm/mach-types.h>
#include <asm/system.h>
#endif

#include "platform.h"
#include "vxi/soc/stiH416/stiH416reg.h"

/*
 * CCIR656 format
 */
#define BUS_TWIST_656      0x0
#define SWAP_N2_656        0x0<<4
#define SWAP_N3_656        0x1<<8
#define NIB_SEL_656        0x1<<12
/*
 * YUV 422 format
 */

/* VXI_DATA_SWAP_1 */
#define SWAP_BU_N1_YUV422  0x5
#define SWAP_BU_N2_YUV422  0x0<<4
#define SWAP_BU_N3_YUV422  0x1<<8

/* VXI_DATA_SWAP_2 */
#define SWAP_GY_N1_YUV422  0x4
#define SWAP_GY_N2_YUV422  0x2<<4
#define SWAP_GY_N3_YUV422  0x3<<8

/* VXI_DATA_SWAP_3 */
#define SWAP_RV_N1_YUV422  0x0
#define SWAP_RV_N2_YUV422  0x0<<4
#define SWAP_RV_N3_YUV422  0x0<<8

/* VXI_DATA_SWAP_4 */
#define BU_BIT_SEL_YUV422  0x7
#define GY_BIT_SEL_YUV422  0x7<<4
#define RV_BIT_SEL_YUV422  0x0<<8

/*
 * RGB 444 / YUV 444 format
 */

/* VXI_DATA_SWAP_1 */
#define SWAP_BU_N1_444  0x7
#define SWAP_BU_N2_444  0x4<<4
#define SWAP_BU_N3_444  0x5<<8

/* VXI_DATA_SWAP_2 */
#define SWAP_GY_N1_444  0x6
#define SWAP_GY_N2_444  0x2<<4
#define SWAP_GY_N3_444  0x3<<8

/* VXI_DATA_SWAP_3 */
#define SWAP_RV_N1_444  0x6
#define SWAP_RV_N2_444  0x0<<4
#define SWAP_RV_N3_444  0x1<<8

/* VXI_DATA_SWAP_4 */
#define BU_BIT_SEL_444  0x1
#define GY_BIT_SEL_444  0x7<<4
#define RV_BIT_SEL_444  0x0<<8

/* Bits per component*/
#define BPC_10          0x1<<10
#define BPC_8           0x0<<10

stm_vxi_input_t vxi_input_desc[] =
{
   {
      .input_id = 0,
      .interface_type = STM_VXI_INTERFACE_TYPE_CCIR656,
      .swap656 = BUS_TWIST_656|SWAP_N2_656|SWAP_N3_656|NIB_SEL_656,
      .bits_per_component = BPC_8,
   },
   {
      .input_id = 1,
      .interface_type = STM_VXI_INTERFACE_TYPE_SMPTE_274M_296M,
      .swap.data_swap1 = SWAP_BU_N1_YUV422|SWAP_BU_N2_YUV422|SWAP_BU_N3_YUV422,
      .swap.data_swap2 = SWAP_GY_N1_YUV422|SWAP_GY_N2_YUV422|SWAP_GY_N3_YUV422,
      .swap.data_swap3 = SWAP_RV_N1_YUV422|SWAP_RV_N2_YUV422|SWAP_RV_N3_YUV422,
      .swap.data_swap4 = BU_BIT_SEL_YUV422|GY_BIT_SEL_YUV422|RV_BIT_SEL_YUV422,
      .bits_per_component = BPC_10,
   },
   {
      .input_id = 2,
      .interface_type = STM_VXI_INTERFACE_TYPE_RGB_YUV_444,
      .swap.data_swap1 = SWAP_BU_N1_444|SWAP_BU_N2_444|SWAP_BU_N3_444,
      .swap.data_swap2 = SWAP_GY_N1_444|SWAP_GY_N2_444|SWAP_GY_N3_444,
      .swap.data_swap3 = SWAP_RV_N1_444|SWAP_RV_N2_444|SWAP_RV_N3_444,
      .swap.data_swap4 = BU_BIT_SEL_444|GY_BIT_SEL_444|RV_BIT_SEL_444,
      .bits_per_component = BPC_10,
   },
};

stm_vxi_platform_board_t  vxi_platform_board_data =
{
   .num_inputs = ARRAY_SIZE(vxi_input_desc),
   .inputs     = vxi_input_desc,
};

stm_vxi_platform_data_t vxi_platform_data =
{
   .soc = {STiH416_VXI_REGISTER_BASE,STiH416_VXI_REG_ADDR_SIZE},
   .board = &vxi_platform_board_data
};


static void release(struct device *dev) { }

static struct platform_device vxi_h416[] = {
  [0] = {
    .id = 0,
    .name = "stm-vxi",
    .dev = {
    .platform_data = &vxi_platform_data,
    .release = release,
  },
  },
};

int __init stm_vxi_probe_devices(int *nb_devices)
{
   int ret = 0;
  *nb_devices = ARRAY_SIZE(vxi_h416);
  printk(KERN_INFO "%s\n", __func__);
  ret = platform_device_register(vxi_h416);
  return ret;
}

void stm_vxi_cleanup_devices(void)
{
  printk(KERN_INFO "%s\n", __func__);
  platform_device_unregister(vxi_h416);
}
