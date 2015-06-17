/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirx/hdmirxdrv.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/err.h>
/* Local Includes -------------------------------------------------*/
#include <hdmirx_drv.h>
#include "hdmirxplatform.h"
#include <stddefs.h>

/******************************************************************************
  G L O B A L   V A R I A B L E S
******************************************************************************/
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
static struct class *stminput_class;
#endif

/******************************************************************************
  FUNCTIONS PROTOTYPE
******************************************************************************/
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
#ifndef CONFIG_ARCH_STI
int __init stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev);
#else
int stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev);
#endif
void __exit stmhdmirx_class_device_unregister(struct platform_device *pdev);
#endif

static void *stmhdmirx_dt_get_pdata(struct platform_device *pdev);
#ifndef CONFIG_ARCH_STI
static int stm_hdmirx_of_port_get_gpio(struct device_node * node_p, stm_hdmirx_platform_port_t * port_p);
#endif
static int stm_hdmirx_of_board_get_ports(struct device_node * node_p,   stm_hdmirx_platform_board_t * board_p);
static int stm_hdmirx_of_soc_get_routes(struct device_node * node_p, stm_hdmirx_platform_soc_t * soc_p, struct resource ** res_p);

/**************************C O D E*********************************************/

/*******************************************************************************
 Name            : stm_hdmirx_probe
 Description     : Probe the HdmiRx device
 Parameters      :
 Assumptions     :
 Limitations     :
 Returns         :
 *******************************************************************************/
#ifndef CONFIG_ARCH_STI
static int __init stmhdmirx_probe(struct platform_device *pdev)
#else
static int stmhdmirx_probe(struct platform_device *pdev)
#endif
{
  stm_hdmirx_device_h device_h=NULL;
  stm_hdmirx_route_h route_h=NULL;
  stm_hdmirx_port_h port_h=NULL;

  pm_runtime_set_active(&pdev->dev);
  pm_runtime_enable(&pdev->dev);
  pdev->dev.platform_data = (stm_hdmirx_platform_data_t*) stmhdmirx_dt_get_pdata(pdev);
  if (hdmirx_init(pdev)!=0)
    {
      HDMI_PRINT("ERROR: HdmiRx Device is not probed\n");
      BUG();
      return -EINVAL;
    }
  if (stm_hdmirx_device_open(&device_h, 0) != 0)
    {
      printk(KERN_ALERT "HdmiRx Device Open failed \n");
      device_h=NULL;
      goto close;
    }
  if (stm_hdmirx_route_open(device_h, 0, &route_h) != 0)
    {
      printk("HdmiRx Route 0 Open failed \n");
      route_h=NULL;
      goto close;
    }
  if (stm_hdmirx_port_open(device_h, 0, &port_h) != 0)
    {
      printk("HdmiRx Port 0 Open failed \n");
      port_h=NULL;
      goto close;
    }

//#ifdef CONFIG_PM_RUNTIME
 // pm_runtime_suspend(&pdev->dev);
//#endif
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
  stmhdmirx_class_device_register(stminput_class,pdev);
#endif

close:
  if(port_h)
    stm_hdmirx_port_close(port_h);
  if(route_h)
    stm_hdmirx_route_close(route_h);
  if(device_h)
    stm_hdmirx_device_close(device_h);

  return 0;
}

/*******************************************************************************
 Name            : stm_hdmirx_remove
 Description     : Remove the HdmiRx device
 Parameters      :
 Assumptions     :
 Limitations     :
 Returns         :
 *******************************************************************************/
static int __exit stmhdmirx_remove(struct platform_device *pdev)
{
  hdmirx_term(pdev);

  /*
   * Missing pm_runtime_disable call in driver remove path caused
   * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
   */
  pm_runtime_disable(&pdev->dev);
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
  stmhdmirx_class_device_unregister(pdev);
#endif

  HDMI_PRINT("Removed Hdmirx device\n");
  return 0;
}

#ifdef CONFIG_PM
static int stmhdmirx_set_power_state(struct platform_device *pdev, pm_message_t state)
{
  int retval = 0;

  /* Retreive Display's object handle */
  switch(state.event) {
    case PM_EVENT_ON:
      printk(KERN_INFO "++++ HDMI-RX PM_EVENT_ON!\n");
      // fall through
    case PM_EVENT_THAW:
      printk(KERN_INFO "++++ HDMI-RX PM_EVENT_THAW!\n");
      // fall through
    case PM_EVENT_RESTORE:
      {
        printk(KERN_INFO "++++ HDMI-RX PM_EVENT_RESTORE!\n");
        /* Power on platform resources */
        hdmirx_SetPowerState(pdev,HDMIRX_PM_RESTORE);
      }
      break;
    case PM_EVENT_RESUME:
      {
        printk(KERN_INFO "++++ HDMI-RX PM_EVENT_RESUME!\n");
        /* Power on platform resources */
        hdmirx_SetPowerState(pdev,HDMIRX_PM_RESUME);
      }
      break;
    case PM_EVENT_FREEZE:
      {
        printk(KERN_INFO "++++ HDMI-RX PM_EVENT_FREEZE!\n");
        hdmirx_SetPowerState(pdev,HDMIRX_PM_SUSPEND);
      }
      break;
    case PM_EVENT_SUSPEND:
      {
        printk(KERN_INFO "++++ HDMI-RX PM_EVENT_SUSPEND!\n");
        /* Power off platform resources */
        hdmirx_SetPowerState(pdev,HDMIRX_PM_SUSPEND);
      }
      break;
    default :
      printk(KERN_ERR " Unsupported PM event!\n");
      retval = -EINVAL;
      break;
  }
  /* We are done, close opened handle */

  return retval;
}

#ifdef CONFIG_PM_RUNTIME
static int stmhdmirx_suspend(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);
  stmhdmirx_set_power_state(pdev,PMSG_SUSPEND);

  return 0;
}

static int stmhdmirx_resume(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);
  stmhdmirx_set_power_state(pdev,PMSG_RESUME);

  return 0;
}
#endif /* CONFIG_PM_RUNTIME */

static int stmhdmirx_freeze(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);
  stmhdmirx_set_power_state(pdev,PMSG_FREEZE);

  return 0;
}

static int stmhdmirx_restore(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);
  stmhdmirx_set_power_state(pdev,PMSG_RESTORE);

  return 0;
}

const struct dev_pm_ops stmhdmirx_pm_ops = {
  .suspend         = stmhdmirx_freeze,
  .resume          = stmhdmirx_restore,
  .freeze          = stmhdmirx_freeze,
  .thaw            = stmhdmirx_restore,
  .poweroff        = stmhdmirx_freeze,
  .restore         = stmhdmirx_restore,
#ifdef CONFIG_PM_RUNTIME
  .runtime_suspend = stmhdmirx_suspend,
  .runtime_resume  = stmhdmirx_resume,
  .runtime_idle    = NULL,
#endif /* CONFIG_PM_RUNTIME */
};
#define HDMIRX_PM_OPS   (&stmhdmirx_pm_ops)
#else /* CONFIG_PM */
#define HDMIRX_PM_OPS   NULL
#endif /* CONFIG_PM */

static struct of_device_id stmhdmirx_match[] = {
  {
    .compatible = "st,hdmirx",
  },
  {},
};

static struct platform_driver stmhdmirx_driver =
{
  .probe    = stmhdmirx_probe,
  .remove   = __exit_p(stmhdmirx_remove),
  .driver   = {
    .name           = "hdmirx",
    .owner          = THIS_MODULE,
    .pm             = HDMIRX_PM_OPS,
    .of_match_table = of_match_ptr(stmhdmirx_match),
  }
};

/*******************************************************************************
 Name            : stm_hdmirx_init
 Description     : Module init of Hdmirx
 Parameters      :
 Assumptions     :
 Limitations     :
 Returns         :
 *******************************************************************************/
int __init stmhdmirx_init(void)
{
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
  stminput_class = class_create(THIS_MODULE, "stmhdmirx");
  if (IS_ERR(stminput_class))
    {
      HDMI_PRINT(KERN_ERR "%s: unable to create class device\n",__FUNCTION__);
      return -ENOMEM;
    }
#endif
  platform_driver_register(&stmhdmirx_driver);
  return 0;
}

static void *stmhdmirx_dt_get_pdata(struct platform_device *pdev)
{
  struct device_node *hdmirx_root_node_p = pdev->dev.of_node;
  stm_hdmirx_platform_data_t     *platform_data;
  stm_hdmirx_platform_board_t    *board_p;
  stm_hdmirx_platform_soc_t      *soc_p;
  struct resource * resource[4];

  if(hdmirx_root_node_p == NULL)
    return ERR_PTR(-EINVAL);

  platform_data          = devm_kzalloc(&pdev->dev,sizeof(stm_hdmirx_platform_data_t), GFP_KERNEL);
  board_p                = devm_kzalloc(&pdev->dev,sizeof(stm_hdmirx_platform_board_t), GFP_KERNEL);
  soc_p                  = devm_kzalloc(&pdev->dev,sizeof(stm_hdmirx_platform_soc_t), GFP_KERNEL);
  if (!platform_data || !board_p || !soc_p)
  {
    printk("%s : Unable to allocate platform data\n",__FUNCTION__);
    return ERR_PTR(-ENOMEM);
  }
  platform_data->board=board_p;
  platform_data->soc=soc_p;

  pdev->id=0;
  of_property_read_u32(hdmirx_root_node_p, "clock-frequency-meas",  &(soc_p->meas_clk_freq_hz));

  soc_p->route=devm_kzalloc(&pdev->dev,sizeof(stm_hdmirx_platform_route_t)*STHDMIRX_MAX_ROUTE, GFP_KERNEL);
  if (!soc_p->route)
    return ERR_PTR(-ENOMEM);

  resource[0] = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reg-core");
  resource[1] = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reg-phy");
  resource[2] = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reg-clk");
  resource[3] = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reg-csm");
  if(stm_hdmirx_of_soc_get_routes(hdmirx_root_node_p, soc_p, resource))
    return ERR_PTR(-EINVAL);

  soc_p->csm.start_addr=(unsigned int) resource[3]->start;
  soc_p->csm.end_addr=(unsigned int) resource[3]->end;

  board_p->port=devm_kzalloc(&pdev->dev,sizeof(stm_hdmirx_platform_port_t)*STHDMIRX_MAX_PORT, GFP_KERNEL);
  if (!board_p->port)
    return ERR_PTR(-ENOMEM);

  if(stm_hdmirx_of_board_get_ports(hdmirx_root_node_p, board_p))
    return ERR_PTR(-EINVAL);

  /* Getting of pin config */
#ifndef CONFIG_ARCH_STI
  board_p->port->Pad_Config_p=stm_of_get_pad_config(&pdev->dev);
#else
  board_p->port->pinctrl_p=devm_pinctrl_get(&pdev->dev);
#endif

  return (void*)(platform_data);
}

static int stm_hdmirx_of_soc_get_routes(struct device_node * node_p, stm_hdmirx_platform_soc_t * soc_p, struct resource ** res_p)
{
  struct device_node *route_struct_node_p[STHDMIRX_MAX_ROUTE];
  unsigned int irq = 0;
  unsigned int route_id = 0;
  char buffer[8];

  irq = irq_of_parse_and_map(node_p, 0);
  if (irq == 0)
    return -EINVAL;

  do
  {
    snprintf(buffer,8,"route%d",route_id);
    route_struct_node_p[route_id] = of_get_child_by_name(node_p, buffer);
    if (route_struct_node_p[route_id] == NULL)
      break;

    of_property_read_u32(route_struct_node_p[route_id], "route-id",  &(soc_p->route[route_id].id));
    of_property_read_u32(route_struct_node_p[route_id], "output-pixel-width",  &(soc_p->route[route_id].output_pixel_width));
    of_property_read_u32(route_struct_node_p[route_id], "i2s-out-clk-scale-factor",  &(soc_p->route[route_id].i2s_out_clk_scale_factor));

    soc_p->route[route_id].core.start_addr=res_p[0]->start;
    soc_p->route[route_id].core.end_addr=res_p[0]->end;

    soc_p->route[route_id].phy.start_addr=res_p[1]->start;
    soc_p->route[route_id].phy.end_addr=res_p[1]->end;

    soc_p->route[route_id].clock_gen.start_addr=res_p[2]->start;
    soc_p->route[route_id].clock_gen.end_addr=res_p[2]->end;

    soc_p->route[route_id].irq_num=irq;

    of_property_read_u32(route_struct_node_p[route_id], "clk-gen-vid-id",  &(soc_p->route[route_id].clock_gen.video_clk_gen_id));
    of_property_read_u32(route_struct_node_p[route_id], "clk-gen-aud-id",  &(soc_p->route[route_id].clock_gen.audio_clk_gen_id));

    of_property_read_u8(route_struct_node_p[route_id], "rterm-mode",  &(soc_p->route[route_id].phy.rterm_mode));
    if(of_property_read_u8(route_struct_node_p[route_id], "rterm-val",  &(soc_p->route[route_id].phy.rterm_val))<0)
      /* Not defined in DT: set default RTerm value to 3 (Cannes2 cut2.1) */
      soc_p->route[route_id].phy.rterm_val = 3;

    route_id++;
  } while (route_id < STHDMIRX_MAX_ROUTE);
  soc_p->num_routes=route_id;
  return 0;
}

static int stm_hdmirx_of_board_get_ports(struct device_node * node_p, stm_hdmirx_platform_board_t * board_p)
{
  struct device_node *port_struct_node_p[STHDMIRX_MAX_PORT];
  struct device_node *eq_config_node_p = NULL;
  unsigned int port_id = 0;
  int ret=0;
  char buffer[8];

  do
  {
    snprintf(buffer,8,"port%d",port_id);
    port_struct_node_p[port_id] = of_get_child_by_name(node_p, buffer);
    if (port_struct_node_p[port_id] == NULL)
      break;

    of_property_read_u32(port_struct_node_p[port_id], "port-id",  &(board_p->port[port_id].id));
    of_property_read_u32_array(port_struct_node_p[port_id],"csm-port-id",board_p->port[port_id].csm_port_id,3);
    of_property_read_u32(port_struct_node_p[port_id], "route-mask",  &(board_p->port[port_id].route_connectivity_mask));
    of_property_read_u32(port_struct_node_p[port_id], "route-eq-mode",  &(board_p->port[port_id].eq_mode));
    of_property_read_u32(port_struct_node_p[port_id], "route-op-mode",  &(board_p->port[port_id].op_mode));

    board_p->port[port_id].internal_edid = of_property_read_bool(port_struct_node_p[port_id], "edid-internal");
    board_p->port[port_id].enable_hpd = of_property_read_bool(port_struct_node_p[port_id], "hpd-enable");
    board_p->port[port_id].enable_ddc2bi = of_property_read_bool(port_struct_node_p[port_id], "ddc2bi-enable");
    of_property_read_u32(port_struct_node_p[port_id], "port-detect-option",  &(board_p->port[port_id].pd_config.option));

    board_p->port[port_id].edid_wp = of_get_named_gpio(node_p, "edid-wp-gpios", 0);
#ifndef CONFIG_ARCH_STI
    ret = stm_hdmirx_of_port_get_gpio(node_p, &(board_p->port[port_id]));
#endif
    eq_config_node_p = of_get_child_by_name(port_struct_node_p[port_id], "route-eq-config");
    if (eq_config_node_p == NULL)
    {
      ret = -EINVAL;
      break;
    }
    of_property_read_u32(eq_config_node_p, "low-freq-gain",  &(board_p->port[port_id].eq_config.low_freq_gain));
    of_property_read_u32(eq_config_node_p, "high-freq-gain",  &(board_p->port[port_id].eq_config.high_freq_gain));
    port_id++;
  } while (port_id < STHDMIRX_MAX_PORT);
  board_p->num_ports=port_id;
  return ret;
}
#ifndef CONFIG_ARCH_STI
static int stm_hdmirx_of_port_get_gpio(struct device_node * node_p, stm_hdmirx_platform_port_t * const port_p)
{
  enum of_gpio_flags gpio_flags;
  struct device_node *pad_struct_node_p;
  struct device_node *pins_struct_node_p;

  pad_struct_node_p = of_parse_phandle(node_p, "padcfg-0", 0);
  if (pad_struct_node_p == NULL)
    return -EINVAL;

  pins_struct_node_p=of_get_child_by_name(pad_struct_node_p, "st,pins");

  port_p->pd_config.pin.pd_pio = of_get_named_gpio_flags(pins_struct_node_p, "pd", 0, &gpio_flags);
  port_p->hpd_pio = of_get_named_gpio_flags(pins_struct_node_p, "hpd", 0, &gpio_flags);
  port_p->scl_pio = of_get_named_gpio_flags(pins_struct_node_p, "scl", 0, &gpio_flags);
  port_p->sda_pio = of_get_named_gpio_flags(pins_struct_node_p, "sda", 0, &gpio_flags);

  return 0;
}
#endif /* CONFIG_ARCH_STI */

/*******************************************************************************
Name            : stm_hdmirx_term
Description     : Module terminition of Hdmirx
Parameters      :
Assumptions     :
Limitations     :
Returns         :
*******************************************************************************/
void __exit stmhdmirx_exit(void)
{
  platform_driver_unregister(&stmhdmirx_driver);
#if defined(SDK2_ENABLE_HDMI_RX_ATTRIBUTES)
  class_destroy(stminput_class);
#endif
}

module_init(stmhdmirx_init);
module_exit(stmhdmirx_exit);

MODULE_DESCRIPTION("stm_hdmirx Platform Driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
