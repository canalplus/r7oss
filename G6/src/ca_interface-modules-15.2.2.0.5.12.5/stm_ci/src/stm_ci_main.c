#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h> /* Initilisation support */
#include <linux/kernel.h> /* Kernel support */
#include <linux/clk.h>
#include <linux/semaphore.h>
#ifdef CONFIG_ARCH_STI
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#else
#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/emi.h>
#endif
#include <linux/of.h>
#include <linux/of_device.h>

#include "ca_platform.h"

#include "stm_ci.h"
#include "ci_protocol.h"
#include "ci_internal.h"
#include "ci_debug.h"
#include "ci_utils.h"

#define EMI_BANK_BASEADDR(b)		(0x800 + (0x10 * b))

#define EMI_STATUS_LOCK			0x018
#define EMI_LOCK			0x20
#define EMI_GENCFG			0x028
#define EMI_STATUS_CFG			0x010
#define EMI_BANK_ENABLE_OFFSET		0x60

MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics CI driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(stm_ci_new);
EXPORT_SYMBOL(stm_ci_delete);
EXPORT_SYMBOL(stm_ci_write);
EXPORT_SYMBOL(stm_ci_read);
EXPORT_SYMBOL(stm_ci_reset);
EXPORT_SYMBOL(stm_ci_get_capabilities);
EXPORT_SYMBOL(stm_ci_get_slot_status);
EXPORT_SYMBOL(stm_ci_get_status);
EXPORT_SYMBOL(stm_ci_set_command);
EXPORT_SYMBOL(stm_ci_read_cis);

#define	STM_CI_MAX 2
struct ci_hw_data_s ci_data[STM_CI_MAX];
static unsigned int ci_int_sig_interface_type;

#ifdef CONFIG_OF
#ifdef CONFIG_ARCH_STI
static void *stm_dt_get_sigmap(struct platform_device *pdev,
				struct device_node *sigmap_np)
{
	struct device_node *child = NULL;
	ci_signal_map *sig_map;
	int num = 0;
	int i = 0;
	uint32_t addr, sig_type;
	ci_epld_intr_reg *reg;
	struct device_node *epld_reg ;
	for_each_child_of_node(sigmap_np, child)
		num++;
	sig_map = devm_kzalloc(&pdev->dev, sizeof(*sig_map) * num, GFP_KERNEL);
	if (!sig_map) {
		dev_err(&pdev->dev, "Unable to allocate sigmap data\n");
		return ERR_PTR(-ENOMEM);
	}
	for_each_child_of_node(sigmap_np, child) {
		of_property_read_u32(child, "sigtype", &sig_type);
		sig_map[sig_type].sig_type = sig_type;
		i = sig_type;
		of_property_read_u32(child, "interface_type",
						&sig_map[i].interface_type);
		of_property_read_u32(child, "bit_mask",
						&sig_map[i].bit_mask);
		of_property_read_u32(child, "interrupt_num",
						&sig_map[i].interrupt_num);
		sig_map[i].active_high = of_get_property(child,
						"active_high", NULL) ? 1 : 0;
		switch (sig_map[i].interface_type) {
		case CI_GPIO:
			sig_map[i].data.gpio = of_get_gpio(child, 0);
			if (!sig_map[i].data.gpio)
				return ERR_PTR(-EFAULT);
			break;
		case CI_EPLD:
			of_property_read_u32(child, "epld_config_addr", &addr);
			sig_map[i].data.epld_config.addr = addr ;
			epld_reg = of_parse_phandle(child,
						"epld_config_int_reg", 0);
			if (epld_reg) {
				reg = &sig_map[i].data.epld_config.epld_int_reg;
				of_property_read_u32(epld_reg,
						"reset_set_addr",
						&reg->reset_set_addr);
				of_property_read_u32(epld_reg,
						"reset_set_bitmask",
						&reg->reset_set_bitmask);
				of_property_read_u32(epld_reg,
						"reset_clear_addr",
						&reg->reset_clear_addr);
				of_property_read_u32(epld_reg,
						"reset_clear_bitmask",
						&reg->reset_clear_bitmask);
				of_property_read_u32(epld_reg,
						"cd_status_addr",
						&reg->cd_status_addr);
				of_property_read_u32(epld_reg,
						"cd_status_bitmask",
						&reg->cd_status_bitmask);
				of_property_read_u32(epld_reg,
						"data_status_addr",
						&reg->data_status_addr);
				of_property_read_u32(epld_reg,
						"data_status_bitmask",
						&reg->data_status_bitmask);
				of_property_read_u32(epld_reg,
						"clear_addr",
						&reg->clear_addr);
				of_property_read_u32(epld_reg,
						"clear_bitmask",
						&reg->clear_bitmask);
				of_property_read_u32(epld_reg,
						"mask_addr",
						&reg->mask_addr);
				of_property_read_u32(epld_reg,
						"mask_bitmask",
						&reg->mask_bitmask);
				of_property_read_u32(epld_reg,
						"priority_addr",
						&reg->priority_addr);
				of_property_read_u32(epld_reg,
						"priority_bitmask",
						&reg->priority_bitmask);
			}
			break;
		case CI_UNKNOWN_INTERFACE:
			break;
		case CI_SYSIRQ:
			break;
		case CI_IIC:
			break;
		case CI_STARCI2WIN:
			break;
		case CI_NEW_INTERFACE:
			break;
		case CI_EMI:
			break;
		}
	}
	return sig_map;
}
#else
static void *stm_dt_get_sigmap(struct platform_device *pdev,
				struct device_node *sigmap_np)
{
	struct device_node *child = NULL;
	ci_signal_map *sig_map;
	int num = 0;
	int i = 0;
	uint32_t addr;
	ci_epld_intr_reg *reg;
	struct device_node *epld_reg ;

	for_each_child_of_node(sigmap_np, child)
		num++;

	sig_map = devm_kzalloc(&pdev->dev, sizeof(*sig_map) * num, GFP_KERNEL);
	if (!sig_map) {
		dev_err(&pdev->dev, "Unable to allocate sigmap data\n");
		return ERR_PTR(-ENOMEM);
	}

	for_each_child_of_node(sigmap_np, child) {
		of_property_read_u32(child, "sigtype", &sig_map[i].sig_type);
		of_property_read_u32(child, "interface_type",
						&sig_map[i].interface_type);
		of_property_read_u32(child, "bit_mask",
						&sig_map[i].bit_mask);
		of_property_read_u32(child, "interrupt_num",
						&sig_map[i].interrupt_num);
		sig_map[i].active_high = of_get_property(child,
						"active_high", NULL) ? 1 : 0;

		switch (sig_map[i].interface_type) {
		case CI_GPIO:
			sig_map[i].data.pad_config =
				stm_of_get_pad_config_from_node(&pdev->dev,
								child, 0);
			if (!sig_map[i].data.pad_config)
				return ERR_PTR(-EFAULT);
			break;
		case CI_EPLD:
			of_property_read_u32(child, "epld_config_addr", &addr);
			sig_map[i].data.epld_config.addr = addr ;

			epld_reg = of_parse_phandle(child,
						"epld_config_int_reg", 0);
			if (epld_reg) {
				reg = &sig_map[i].data.epld_config.epld_int_reg;

				of_property_read_u32(epld_reg,
						"reset_set_addr",
						&reg->reset_set_addr);
				of_property_read_u32(epld_reg,
						"reset_set_bitmask",
						&reg->reset_set_bitmask);
				of_property_read_u32(epld_reg,
						"reset_clear_addr",
						&reg->reset_clear_addr);
				of_property_read_u32(epld_reg,
						"reset_clear_bitmask",
						&reg->reset_clear_bitmask);
				of_property_read_u32(epld_reg,
						"cd_status_addr",
						&reg->cd_status_addr);
				of_property_read_u32(epld_reg,
						"cd_status_bitmask",
						&reg->cd_status_bitmask);
				of_property_read_u32(epld_reg,
						"data_status_addr",
						&reg->data_status_addr);
				of_property_read_u32(epld_reg,
						"data_status_bitmask",
						&reg->data_status_bitmask);
				of_property_read_u32(epld_reg,
						"clear_addr",
						&reg->clear_addr);
				of_property_read_u32(epld_reg,
						"clear_bitmask",
						&reg->clear_bitmask);
				of_property_read_u32(epld_reg,
						"mask_addr",
						&reg->mask_addr);
				of_property_read_u32(epld_reg,
						"mask_bitmask",
						&reg->mask_bitmask);
				of_property_read_u32(epld_reg,
						"priority_addr",
						&reg->priority_addr);
				of_property_read_u32(epld_reg,
						"priority_bitmask",
						&reg->priority_bitmask);
			}

			break;

		case CI_UNKNOWN_INTERFACE:
			break;

		/*For future use */
		case CI_SYSIRQ:
			break;

		case CI_IIC:
			break;

		case CI_STARCI2WIN:
			break;

		case CI_NEW_INTERFACE:
			break;

		case CI_EMI:
			break;
		}

		i++;

	}
	return sig_map;
}
#endif
static int stm_dt_get_emiconfig(struct platform_device *pdev,
				struct device_node *emi_banks_np,
				ci_emi_bank_data **banksp)
{
	struct device_node *child = NULL ;
	ci_emi_bank_data *emi_banks;
	int num_banks = 0;
	int i, data_reg_count;

	for_each_child_of_node(emi_banks_np, child)
		num_banks++;

	if(!banksp)
		return -EFAULT;

	*banksp = devm_kzalloc(&pdev->dev,
			sizeof(*emi_banks) * num_banks, GFP_KERNEL);
	if (!(*banksp)) {
		dev_err(&pdev->dev, "Unable to allocate emi_config data\n");
		return -ENOMEM ;
	}

	emi_banks = *banksp;
	of_property_read_u32(emi_banks_np,
				"emi_bank_data_count",
				&data_reg_count);

	for (i = 0; i < num_banks; i++) {
		child = of_get_next_child(emi_banks_np, child);
		WARN_ON(of_property_read_u32(child, "bank_id",
					&emi_banks[i].bank_id));

		of_property_read_u32_array(child, "emi_bank_data_val",
					(u32 *)emi_banks[i].emi_config_data,
					data_reg_count);
		of_property_read_u32(child, "addrbits",
					&emi_banks[i].addrbits);
		if (of_get_property(child, "dvbci_mode", NULL))
			emi_banks[i].mode = DVBCI_MODE;
		else
			emi_banks[i].mode = EPLD_MODE;
	}
	return num_banks ;
}

#ifndef CONFIG_MACH_STM_B2000
static void ci_emi_bank_config(uint32_t emi_base_v,
				ci_emi_bank_data *emi_bank_config)
{
	uint32_t data_config_offset;
	uint32_t i;

	writel(emi_bank_config->addrbits >> 22, (volatile void __iomem *)((uint32_t)emi_base_v +
				EMI_BANK_BASEADDR(emi_bank_config->bank_id)));

	data_config_offset = 0x100 + emi_bank_config->bank_id * 0x40;
	ci_debug_trace(CI_API_RUNTIME, "data_config_offset = %x\n",
				data_config_offset);

	for (i = 0 ; i < 4; i++)
		writel(emi_bank_config->emi_config_data[i],
				(volatile void __iomem *)((uint32_t)emi_base_v + data_config_offset + 8*i));

	writel((1 << emi_bank_config->bank_id),
				(volatile void __iomem *)((uint32_t)emi_base_v + EMI_LOCK));

	if (emi_bank_config->mode == DVBCI_MODE) {
		writel((1 << (emi_bank_config->bank_id + 1)),
				(volatile void __iomem *)((uint32_t)emi_base_v + EMI_GENCFG));

	}
	return;
}
#endif

static int ci_emi_config(struct ci_hw_data_s *hw_data_p)
{
	unsigned char __iomem *emi_base_v;
	struct resource	*base_address_info_p;
	uint32_t ip_mem_size;
	int i, error = 0;
#ifndef CONFIG_MACH_STM_B2000
	uint32_t max_bank_no = 0;
#endif
	struct stm_ci_platform_data *plat_data;

	plat_data = hw_data_p->platform_data;

	base_address_info_p = hw_data_p->emi_base_addr;
	if (!base_address_info_p) {
		printk(KERN_ERR "EMI base address notfound\n");
		return -ENXIO;
	}

	ip_mem_size = (base_address_info_p->end	-
			base_address_info_p->start + 1);

	emi_base_v = ioremap_nocache(base_address_info_p->start,
					ip_mem_size);
	if (!emi_base_v) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
			"CI EMI base addr failed\n");
		error =	-ENOMEM;
		goto error_iomap;
	}

	ci_debug_trace(CI_API_RUNTIME,
			"*** CONFIGURING EMIfor DVBCI ***\n");

#ifdef CONFIG_MACH_STM_B2000

	for (i = 0 ; i < plat_data->nr_banks; i++) {
		writel(plat_data->emi_config[i].addrbits >> 22,
			(uint32_t)emi_base_v +
			EMI_BANK_BASEADDR(plat_data->emi_config[i].bank_id));

		emi_bank_configure(plat_data->emi_config[i].bank_id,
				plat_data->emi_config[i].emi_config_data);

		if (plat_data->emi_config[i].mode == DVBCI_MODE)
			emi_config_pcmode(plat_data->emi_config[i].bank_id, 1);
		else
			emi_config_pcmode(plat_data->emi_config[i].bank_id, 0);
	}

#else

	for (i = 0 ; i < plat_data->nr_banks; i++) {
		ci_emi_bank_config((uint32_t)emi_base_v,
				&plat_data->emi_config[i]);
		if (plat_data->emi_config[i].bank_id >
			plat_data->emi_config[i+1].bank_id)
			max_bank_no = (plat_data->emi_config[i].bank_id + 1);
		else
			max_bank_no = (plat_data->emi_config[i].bank_id + 1);
	}

	writel(max_bank_no,(volatile void __iomem *)( (uint32_t)emi_base_v +
				(EMI_BANK_BASEADDR(0) +
				EMI_BANK_ENABLE_OFFSET)));

#endif
	iounmap(emi_base_v);
error_iomap:
	return error ;
}


static int ci_chipsel_config(struct ci_hw_data_s *hw_data_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint32_t ci_cs_add = 0;
	uint32_t val;
	unsigned char __iomem *epld_addr_v;
	struct resource	*base_address_info_p;
	uint32_t ip_mem_size;

	struct stm_ci_platform_data *platform_data_p;
	platform_data_p = hw_data_p->platform_data;

	/* Map DVBCI EPLD base address */
	base_address_info_p = hw_data_p->epld_base;
	ip_mem_size = (base_address_info_p->end	-
			base_address_info_p->start + 1);

	epld_addr_v = ioremap_nocache(base_address_info_p->start,
					ip_mem_size);
	if (!epld_addr_v) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"CI EPLD memory mapping failed\n");
		error =	-ENOMEM;
		goto error_iomap;
	}

	ci_cs_add = (uint32_t)(epld_addr_v +
				platform_data_p->chip_sel_offset);
	val = readl((volatile void __iomem *)ci_cs_add);

	val |= 1 << platform_data_p->chip_sel ;
	writel((uint32_t)val, (volatile void __iomem *)((uint32_t)ci_cs_add));

	iounmap(epld_addr_v);
error_iomap:
	return error;
}

static void *stm_ci_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_ci_platform_data *data;
	struct device_node *sigmap , *ci_emi_bank ;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	sigmap = of_parse_phandle(np, "ci_signal_map", 0);
	if (sigmap) {
		data->sigmap = stm_dt_get_sigmap(pdev, sigmap);
		if (!data->sigmap)
			return ERR_PTR(-EFAULT);
	}

	ci_emi_bank = of_parse_phandle(np, "ci-emi-bank", 0);
	if (ci_emi_bank) {
		data->nr_banks = stm_dt_get_emiconfig(pdev, ci_emi_bank,
							&data->emi_config);
	}

	if (of_find_property(np, "epld-enable", NULL)) {

		of_property_read_u32(np, "epld-enable", &data->epld_en_flag);
		of_property_read_u32(np, "dvbci_cs_number", &data->chip_sel);
		of_property_read_u32(np, "dvbci_cs_reg_offset",
			&data->chip_sel_offset);
	}

	of_property_read_u32(np, "dvbci_internal_signal_interface_type",
				&ci_int_sig_interface_type);
#ifdef CONFIG_ARCH_STI
	if (ci_int_sig_interface_type == CI_GPIO) {
		data->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(data->pinctrl))
			return ERR_PTR(PTR_ERR(data->pinctrl));
		data->pins_default = pinctrl_lookup_state(data->pinctrl,
				PINCTRL_STATE_DEFAULT);
		if (IS_ERR(data->pins_default)) {
			dev_err(&pdev->dev, "could not get default pinstate\n");
			return ERR_PTR(-EINVAL);
		} else
			pinctrl_select_state(data->pinctrl, data->pins_default);
	}
#else
	if (ci_int_sig_interface_type == CI_GPIO) {
		data->dvbci_pad_config = stm_of_get_pad_config_from_node(
					&pdev->dev, pdev->dev.of_node, 0);
		if (!data->dvbci_pad_config)
			return ERR_PTR(-EFAULT);

		data->dvbci_pad_state = devm_stm_pad_claim(&pdev->dev,
			data->dvbci_pad_config, "stm_ci");
		if (!data->dvbci_pad_state) {
			dev_err(&pdev->dev, "Pads request failed\n");
			return ERR_PTR(-ENODEV);
		}
	}
#endif

	return data;
}

#else
static void *stm_ci_dt_get_pdata(struct platform_device *pdev)
{
	return NULL ;
}
#endif

static int __init ci_probe(struct platform_device *pdev)
{
	struct stm_ci_platform_data *plat_data = NULL;
	int id;
	ci_signal_map *sigmap;

	if (pdev->dev.of_node) {
		ci_debug_trace(CI_API_RUNTIME, "CI DT node exists\n");
		id = of_alias_get_id(pdev->dev.of_node, "ci");
		plat_data = stm_ci_dt_get_pdata(pdev);
	} else {
		plat_data = pdev->dev.platform_data;
		id = pdev->id;
	}

	if (!plat_data || IS_ERR(plat_data)) {
		dev_err(&pdev->dev, "No platform data found\n");
		return -ENODEV;
	}

	ci_data[id].platform_data	=
		(struct stm_ci_platform_data *)(plat_data);
	sigmap = ci_data[id].platform_data->sigmap;

	ci_data[id].pdev = pdev;
	ci_data[id].init = true;

	ci_data[id].ci_base = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "cibase");
	if (!ci_data[id].ci_base) {
		printk(KERN_ERR "CI base address not found!\n");
		return -EFAULT;
	}

	if (plat_data->epld_en_flag) {
		ci_data[id].epld_base = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "epldbase");
		if (!ci_data[id].epld_base) {
			printk(KERN_ERR "EPLD base address not found!\n");
			return -EFAULT;
		}
	}
	ci_data[id].attribute_mem_offset = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "attribute");
	ci_data[id].iomem_offset	= platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "iomem");

	/*This resource may not be needed in future as some setting may move to
	* targetpack*/

	ci_data[id].emi_base_addr = platform_get_resource_byname(
			pdev, IORESOURCE_MEM, "emi_base_addr");
	if (!ci_data[id].emi_base_addr) {
		printk(KERN_ERR "EMI base address not found!\n");
		return -EFAULT;
	}

	ci_data[id].dev_id = id;
	platform_set_drvdata(pdev, &ci_data[id]);

#ifdef CONFIG_OF
	ci_emi_config(&ci_data[id]);

	/*TODO : This chip select register is specific to board .
	* It may change with testing on other new boards(b2089) */

	if (plat_data->epld_en_flag)
		ci_chipsel_config(&ci_data[id]);
	else
		printk(KERN_ALERT "No EPLD\n");
#endif
	ci_debug_trace(CI_API_RUNTIME, "%s %d %x\n",
			pdev->name,
			id,
			(int)ci_data[id].ci_base->start);

	return 0;

}

static int ci_remove(struct platform_device *pdev)
{
	struct ci_hw_data_s *ci_data_handle;
	int id;
	ci_data_handle = platform_get_drvdata(pdev);
	ci_debug_trace(CI_API_RUNTIME, "id is %d\n" ,
				ci_data_handle->dev_id);
	id = ci_data_handle->dev_id;

	if (ci_int_sig_interface_type == CI_GPIO)
	{
#ifdef CONFIG_ARCH_STI
		devm_pinctrl_put(ci_data_handle->platform_data->pinctrl);
#else
		devm_stm_pad_release(&pdev->dev,
			ci_data_handle->platform_data->dvbci_pad_state);
#endif
	}
	ci_data[id].init = false;
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_ci_match[] = {
		{
				.compatible = "st,ci",
		},
			{},
};
#endif

static struct platform_driver __refdata	stm_ci_driver =	{
	.driver	= {
		.name =	"stm_ci",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_ci_match),
/*		.pm = &stm_ci_pm_ops, */
/*For power management*/
	},
	.probe = ci_probe,
	.remove	= ci_remove,
};

static int __init ci_init_module(void)
{
	uint32_t i = 0;
#ifndef CONFIG_OF
	arch_modprobe_func();
#endif
	printk(KERN_ALERT "Load	module stm_ci by %s (pid %i)\n",
			current->comm, current->pid);

	for (i = 0; i < STM_CI_MAX; i++) {
		ci_data[i].ci_base = 0;
		ci_data[i].epld_base = 0;
		ci_data[i].iomem_offset	= 0;
		ci_data[i].attribute_mem_offset	= 0;
	}
	ci_alloc_global_param();
	if (ci_register_char_driver() < 0)
		return -1;
	platform_driver_register(&stm_ci_driver);
	return 0;
}

static void __exit ci_exit_module(void)
{
	platform_driver_unregister(&stm_ci_driver);
	ci_unregister_char_driver();
	ci_dealloc_global_param();
	printk(KERN_ALERT "Unload module stm_ci	by %s (pid %i)\n",
			current->comm, current->pid);
}

module_init(ci_init_module);
module_exit(ci_exit_module);

