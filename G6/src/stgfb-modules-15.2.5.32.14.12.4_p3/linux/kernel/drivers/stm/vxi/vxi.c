/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/vxi/vxi.c
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/err.h>

#include <stm_vxi.h>
#include "platform.h"

#if !defined(KBUILD_SYSTEM_INFO)
#define KBUILD_SYSTEM_INFO "<unknown>"
#endif
#if !defined(KBUILD_USER)
#define KBUILD_USER "<unknown>"
#endif
#if !defined(KBUILD_SOURCE)
#define KBUILD_SOURCE "<unknown>"
#endif
#if !defined(KBUILD_VERSION)
#define KBUILD_VERSION "<unknown>"
#endif
#if !defined(KBUILD_DATE)
#define KBUILD_DATE "<unknown>"
#endif

#define VXI_MAX_INPUT_NODES 10
static int nr_vxi_devices;
static stm_vxi_platform_data_t *pVxiPlatformData = NULL;
static stm_vxi_h *pVxi = NULL;

extern int __init stm_vxi_probe_devices(int *nb_devices);
extern void stm_vxi_cleanup_devices(void);

static void *stm_vxi_dt_get_pdata(struct platform_device *pdev)
{
  struct device_node *vxi_root_node_p = NULL;
  struct device_node *vxi_struct_node_p = NULL;
  struct device_node *vxi_inputs_node_p[VXI_MAX_INPUT_NODES];
  stm_vxi_platform_data_t        *vxi_platform_data = NULL;
  stm_vxi_platform_board_t       *board             = NULL;
  stm_vxi_input_t                *inputs            = NULL;
  struct resource                *resource          = NULL;
  unsigned int num_inputs = 0;
  char buffer[8];
  int i = 0;

  vxi_root_node_p = pdev->dev.of_node;
  vxi_struct_node_p    = of_parse_phandle(vxi_root_node_p, "inputs", 0);

  do
  {
    snprintf(buffer,8,"input-%d",num_inputs);
    vxi_inputs_node_p[num_inputs] = of_get_child_by_name(vxi_struct_node_p, buffer);
    num_inputs++;
  } while ((vxi_inputs_node_p[num_inputs-1] != NULL) && (num_inputs < VXI_MAX_INPUT_NODES));

  vxi_platform_data = devm_kzalloc(&pdev->dev,sizeof(stm_vxi_platform_data_t), GFP_KERNEL);
  board             = devm_kzalloc(&pdev->dev,sizeof(stm_vxi_platform_board_t), GFP_KERNEL);
  inputs            = devm_kzalloc(&pdev->dev,sizeof(stm_vxi_input_t)*num_inputs, GFP_KERNEL);
  if((!vxi_platform_data)||(!board)||(!inputs))
  {
    printk(KERN_ERR "%s : Unable to allocate platform data\n", __FUNCTION__);
    return ERR_PTR(-ENOMEM);
  }

  board->inputs = inputs;
  board->num_inputs = num_inputs;

  for(i=0;i<num_inputs;i++)
  {
    of_property_read_u32(vxi_inputs_node_p[i], "sinput_id"      , &(board->inputs[i].input_id));
    of_property_read_u32(vxi_inputs_node_p[i], "interface-type", &(board->inputs[i].interface_type));
    if(board->inputs[i].interface_type == 0)
    {
      of_property_read_u32(vxi_inputs_node_p[i], "swap656", &(board->inputs[i].swap656));
      of_property_read_u32(vxi_inputs_node_p[i], "bits-per-component",  &(board->inputs[i].bits_per_component));
    }
    else
    {
      of_property_read_u32(vxi_inputs_node_p[i], "data-swap1", &(board->inputs[i].swap.data_swap1));
      of_property_read_u32(vxi_inputs_node_p[i], "data-swap2", &(board->inputs[i].swap.data_swap2));
      of_property_read_u32(vxi_inputs_node_p[i], "data-swap3", &(board->inputs[i].swap.data_swap3));
      of_property_read_u32(vxi_inputs_node_p[i], "data-swap4", &(board->inputs[i].swap.data_swap4));
      of_property_read_u32(vxi_inputs_node_p[i], "bits-per-component",  &(board->inputs[i].bits_per_component));
    }
  }

  vxi_platform_data->board = board;
  resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vxi-reg");

  vxi_platform_data->soc.reg = (unsigned int)resource->start;
  vxi_platform_data->soc.size = (unsigned int)resource->end - resource->start +1;

  return (void*)(vxi_platform_data);
}

static int __init stm_vxi_probe(struct platform_device *pdev)
{
  int res=0;
  printk(KERN_INFO "stm_vxi_probe\n");

  pVxiPlatformData = (stm_vxi_platform_data_t*)stm_vxi_dt_get_pdata(pdev);
  pVxi = devm_kzalloc (&pdev->dev, sizeof (stm_vxi_h), GFP_KERNEL);
  if (!pVxi)
  {
    res = -ENOMEM;
    goto exit_error;
  }
  res = stm_vxi_open(0, pVxi);

  pm_runtime_set_active(&pdev->dev);
  pm_runtime_enable(&pdev->dev);
  pm_runtime_get(&pdev->dev);

exit_error:
  return res;
}

static int __exit stm_vxi_remove (struct platform_device *pdev)
{
  int res=0;

  printk(KERN_INFO "stm_vxi_remove\n");
  res = stm_vxi_close(*pVxi);

  /*
   * Missing pm_runtime_disable call in driver remove path caused
   * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
   */
  pm_runtime_disable(&pdev->dev);

  return res;
}

#if defined(CONFIG_PM)
static int vxi_set_power_state(struct device *dev, pm_message_t state)
{
  int retval = 0;

  switch(state.event) {
    case PM_EVENT_RESUME:
      {
        printk(KERN_INFO "Resuming...\n");
        retval = stm_vxi_resume(*pVxi);
        printk(KERN_INFO "OK\n");
      }
      break;
    case PM_EVENT_SUSPEND:
      {
        printk(KERN_INFO "Suspending...\n");
        retval = stm_vxi_suspend(*pVxi);
        printk(KERN_INFO "OK\n");
      }
      break;
    case PM_EVENT_FREEZE:
        printk(KERN_INFO "Freezing...\n");
        retval = stm_vxi_freeze(*pVxi);
        printk(KERN_INFO "OK\n");
    break;
    case PM_EVENT_ON:
    case PM_EVENT_RESTORE:
    case PM_EVENT_THAW:
    default :
      printk(KERN_ERR "Unsupported PM event!\n");
      retval = -EINVAL;
      break;
  }

  return retval;
}

static int vxi_freeze(struct device *dev)
{
  return vxi_set_power_state(dev, PMSG_FREEZE);
}

static int vxi_suspend(struct device *dev)
{
  return vxi_set_power_state(dev, PMSG_SUSPEND);
}

static int vxi_resume(struct device *dev)
{
  return vxi_set_power_state(dev, PMSG_RESUME);
}

const struct dev_pm_ops stm_vxi_pm_ops = {
  .suspend         = vxi_suspend,
  .resume          = vxi_resume,
  .freeze          = vxi_freeze,
  .thaw            = vxi_resume,
  .poweroff        = vxi_suspend,
  .restore         = vxi_resume,
  .runtime_suspend = vxi_suspend,
  .runtime_resume  = vxi_resume,
  .runtime_idle    = NULL,
};

#define VXI_PM_OPS   (&stm_vxi_pm_ops)
#else /* CONFIG_PM */
#define VXI_PM_OPS   NULL
#endif /* CONFIG_PM */

static struct of_device_id stm_vxi_match[] = {
  {
      .compatible = "st,vxi-stih415",    // in line with stih416.dtsi
  },
  {},
};

static struct platform_driver stm_vxi_driver = {
  .driver.name  = "stm-vxi",
  .driver.owner = THIS_MODULE,
  .driver.pm    = VXI_PM_OPS,
  .driver.of_match_table = of_match_ptr(stm_vxi_match),
  .probe        = stm_vxi_probe,
  .remove       = __exit_p(stm_vxi_remove),
};

static int __init vxi_module_init(void)
{
  int res = 0;
  printk(KERN_INFO "vxi- module init\n");

  res = platform_driver_register (&stm_vxi_driver);
  printk(KERN_INFO "Register driver %d\n",nr_vxi_devices);
  return res;
}


static void __exit vxi_module_exit(void)
{
  printk(KERN_INFO "vxi- module exit\n");
  platform_driver_unregister (&stm_vxi_driver);
  printk(KERN_INFO "Unregister driver %d \n",nr_vxi_devices);
}

/******************************************************************************
 * Public Linux platform API to the vxi module
 */
int stmvxi_get_platform_data(stm_vxi_platform_data_t *pData)
{
    if (pVxiPlatformData != NULL) {
        memcpy(pData, pVxiPlatformData, sizeof(stm_vxi_platform_soc_t) + sizeof(unsigned int) + sizeof(stm_vxi_input_t) * (pVxiPlatformData->board->num_inputs));
    }
    else {
        printk(KERN_ERR "Internal error: pVxiPlatformData == NULL in stmvxi_get_platform_data\n");
    }
    return 0;
}

void stmvxi_get_vxi_platform_data_size(unsigned int *size)
{
    if (pVxiPlatformData != NULL) {
        *size = sizeof(stm_vxi_platform_soc_t) + sizeof(unsigned int) + sizeof(stm_vxi_input_t) * (pVxiPlatformData->board->num_inputs);
    }
    else {
        printk(KERN_ERR "Internal error: pVxiPlatformData == NULL in stmvxi_get_vxi_platform_data_size\n");
    }
}
/******************************************************************************
 *  Modularization
 */
#ifdef MODULE

MODULE_AUTHOR ("STMicroelectronics");
MODULE_DESCRIPTION ("VXI driver");
MODULE_LICENSE ("GPL");
MODULE_VERSION (KBUILD_VERSION);

module_init (vxi_module_init);
module_exit (vxi_module_exit);

EXPORT_SYMBOL(stmvxi_get_platform_data);
EXPORT_SYMBOL(stmvxi_get_vxi_platform_data_size);

EXPORT_SYMBOL(stm_vxi_open);
EXPORT_SYMBOL(stm_vxi_close);
EXPORT_SYMBOL(stm_vxi_get_capability);
EXPORT_SYMBOL(stm_vxi_get_input_capability);
EXPORT_SYMBOL(stm_vxi_set_ctrl);
EXPORT_SYMBOL(stm_vxi_get_ctrl);

#endif /* MODULE */
