#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h> /* Initilisation support */
#include <linux/kernel.h> /* Kernel support */
#include <linux/clk.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#ifdef CONFIG_ARCH_STI
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#else
#include <linux/stm/pad.h>
#include <linux/stm/clk.h>
#include <linux/stm/device.h>
#include <linux/stm/platform.h>
#endif

#include "ca_platform.h"
#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_protocol_t14.h"
#include "scr_protocol_t1.h"
#include "scr_io.h"
#include "scr_utils.h"
#include "scr_plugin.h"
#include "stm_event.h"

MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics SCR driver");
MODULE_LICENSE("GPL");

#define SCR_DEVICE_NAME "stm_scr"

EXPORT_SYMBOL(stm_scr_new);
EXPORT_SYMBOL(stm_scr_delete);
EXPORT_SYMBOL(stm_scr_transfer);
EXPORT_SYMBOL(stm_scr_reset);
EXPORT_SYMBOL(stm_scr_deactivate);
EXPORT_SYMBOL(stm_scr_abort);
EXPORT_SYMBOL(stm_scr_protocol_negotiation);
EXPORT_SYMBOL(stm_scr_get_capabilities);
EXPORT_SYMBOL(stm_scr_get_card_status);
EXPORT_SYMBOL(stm_scr_get_control);
EXPORT_SYMBOL(stm_scr_set_control);
EXPORT_SYMBOL(scr_set_reset_clock_frequency);

/* Need to export for the CA module */
EXPORT_SYMBOL(scr_asc_read);
EXPORT_SYMBOL(scr_asc_write);
EXPORT_SYMBOL(scr_asc_flush);
EXPORT_SYMBOL(scr_transfer_t1);

/* Export Vandor specific functionality*/
EXPORT_SYMBOL(scr_ca_transfer_t14);
EXPORT_SYMBOL(scr_ca_populate_init_parameters);
EXPORT_SYMBOL(scr_ca_recalculate_parameters);
EXPORT_SYMBOL(scr_ca_read);
EXPORT_SYMBOL(scr_ca_set_parity);
EXPORT_SYMBOL(scr_ca_recalculate_etu);
EXPORT_SYMBOL(scr_ca_set_stopbits);
EXPORT_SYMBOL(scr_ca_gpio_direction);
EXPORT_SYMBOL(get_platform_data);
EXPORT_SYMBOL(scr_ca_update_fi_di);
EXPORT_SYMBOL(scr_ca_enable_clock);

EXPORT_SYMBOL(ca_os_malloc_d);
EXPORT_SYMBOL(ca_os_free_d);
EXPORT_SYMBOL(ca_os_copy);

EXPORT_SYMBOL(ca_os_sema_initialize);
EXPORT_SYMBOL(ca_os_sema_terminate);
EXPORT_SYMBOL(ca_os_sema_wait);
EXPORT_SYMBOL(ca_os_sema_wait_timeout);
EXPORT_SYMBOL(ca_os_sema_signal);

EXPORT_SYMBOL(ca_os_hrt_sema_initialize);
EXPORT_SYMBOL(ca_os_hrt_sema_terminate);
EXPORT_SYMBOL(ca_os_hrt_sema_wait);
EXPORT_SYMBOL(ca_os_hrt_sema_wait_timeout);
EXPORT_SYMBOL(ca_os_hrt_sema_signal);

EXPORT_SYMBOL(ca_os_mutex_initialize);
EXPORT_SYMBOL(ca_os_mutex_terminate);
EXPORT_SYMBOL(ca_os_mutex_lock);
EXPORT_SYMBOL(ca_os_mutex_unlock);

EXPORT_SYMBOL(ca_os_wait_queue_wakeup);
EXPORT_SYMBOL(ca_os_wait_queue_initialize);
EXPORT_SYMBOL(ca_os_wait_queue_reinitialize);
EXPORT_SYMBOL(ca_os_wait_queue_deinitialize);

EXPORT_SYMBOL(ca_os_thread_create);
EXPORT_SYMBOL(ca_os_thread_terminate);
EXPORT_SYMBOL(ca_os_wait_thread);
EXPORT_SYMBOL(ca_os_task_exit);

EXPORT_SYMBOL(ca_os_get_time_in_sec);
EXPORT_SYMBOL(ca_os_get_time_in_milli_sec);
EXPORT_SYMBOL(ca_os_sleep_milli_sec);
EXPORT_SYMBOL(ca_os_sleep_usec);
EXPORT_SYMBOL(ca_os_time_now);

EXPORT_SYMBOL(ca_q_remove_node);
EXPORT_SYMBOL(ca_q_insert_node);
EXPORT_SYMBOL(ca_q_search_node);
EXPORT_SYMBOL(ca_os_message_q_initialize);
EXPORT_SYMBOL(ca_os_message_q_terminate);
EXPORT_SYMBOL(ca_os_message_q_send);
EXPORT_SYMBOL(ca_os_message_q_receive);

struct scr_platform_data_s scr_data[STM_SCR_MAX];

ca_os_semaphore_t g_scr_sem_lock;

static dev_t scr_dev;

static int scr_major;
static struct class *scr_class;
static void scr_register_default_ca_ops(void);

void scr_register_default_ca_ops(void)
{
	struct scr_ca_ops_s ca_ops;

	memset(&ca_ops, 0, sizeof(ca_ops));
	ca_ops.ca_set_param = scr_set_ca_default_param;
	scr_register_ca_ops(&ca_ops);
}
struct scr_platform_data_s *get_platform_data(void)
{
	return scr_data;
}

#ifdef CONFIG_OF
#ifdef CONFIG_ARCH_STI
static uint32_t st_407scr_regfields0[MAX_REGFIELDS] = {
	[CLK_MUL_SEL] = 7,
	[INPUT_CLK_SEL] = 9,
	[VCC_ENABLE] = 11,
	[DETECT_POL] = 12,
};
static uint32_t st_407scr_regfields1[MAX_REGFIELDS] = {
	[CLK_MUL_SEL] = 8,
	[INPUT_CLK_SEL] = 10,
	[VCC_ENABLE] = 15,
	[DETECT_POL] = 16,
};
static struct st_scr_data st_407scr_data[STM_SCR_MAX] = {
	[0] = {
		.reg_fields = &st_407scr_regfields0[0],
		.reg_offset = SRC407_OFFSET,
	},
	[1] = {
		.reg_fields = &st_407scr_regfields1[0],
		.reg_offset = SRC407_OFFSET,
	},
};
static struct of_device_id stm_scr_match[] = {
	{ .compatible = "st,stih407-scr", .data = &st_407scr_data },
	{},
};
#else
static struct of_device_id stm_scr_match[] = {
{
	.compatible = "st,scr",
},
	{},
};
#endif
static void *stm_asc_dt_get_data(struct platform_device *pdev)
{
	struct stm_plat_asc_data *data ;
#ifdef CONFIG_ARCH_STI
	struct device_node *np = pdev->dev.of_node;
#endif
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
#ifdef CONFIG_ARCH_STI
	data->c4_pin = devm_kzalloc(&pdev->dev, sizeof(*data->c4_pin),
			GFP_KERNEL);
	data->c7_pin = devm_kzalloc(&pdev->dev, sizeof(*data->c7_pin),
			GFP_KERNEL);
	data->c8_pin = devm_kzalloc(&pdev->dev, sizeof(*data->c8_pin),
			GFP_KERNEL);
	if (data->pinctrl == NULL) {
		data->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(data->pinctrl))
			return ERR_PTR(-EFAULT);
	}
	data->c4_pin->gpio_num = of_get_named_gpio(np, "c4-gpio", 0);
	data->c7_pin->gpio_num = of_get_named_gpio(np, "c7-gpio", 0);
	data->c8_pin->gpio_num = of_get_named_gpio(np, "c8-gpio", 0);
#else
	data->pad_config = stm_of_get_pad_config(&pdev->dev);
	if (!data->pad_config)
		return ERR_PTR(-EFAULT);
#endif
	return data;
}
#ifdef CONFIG_ARCH_STI
static void *stm_scr_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct stm_scr_platform_data *data;
	struct device_node *cn = of_parse_phandle(np, "uart", 0);
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	data->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(data->pinctrl))
		return ERR_PTR(-EFAULT);
	data->class_sel_gpio = of_get_named_gpio(np, "class-sel-gpio", 0);
	if (!gpio_is_valid(data->class_sel_gpio))
		return ERR_PTR(-EINVAL);
	data->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
		if (!gpio_is_valid(data->reset_gpio))
		return ERR_PTR(-EINVAL);
	data->vcc_gpio = of_get_named_gpio(np, "vcc-gpio", 0);
	if (!gpio_is_valid(data->vcc_gpio))
		return ERR_PTR(-EINVAL);
	data->detect_gpio = of_get_named_gpio(np, "detect-gpio", 0);
	if (!gpio_is_valid(data->detect_gpio))
		return ERR_PTR(-EINVAL);
	if(of_property_read_u32(np, "asc-fifo-size", &data->asc_fifo_size))
		return ERR_PTR(-EFAULT);
	data->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscon");
	if (IS_ERR(data->regmap))
		return ERR_PTR(-EFAULT);
	match = of_match_device(stm_scr_match, &pdev->dev);
	if (!match)
		return ERR_PTR(-ENODEV);
	data->scr_config = (struct st_scr_data *)match->data;
	data->asc = of_find_device_by_node(cn);
	if (!data->asc) {
		dev_err(&data->asc->dev, "No uart device exists\n");
		return ERR_PTR(-EFAULT) ;
	}

	data->asc->dev.platform_data = stm_asc_dt_get_data(data->asc);
	if (!data->asc->dev.platform_data)
		return ERR_PTR(-EFAULT);
	return data ;
}

int sti_scr_set_config(struct stm_scr_platform_data *data, int id)
{
	int i;
	struct platform_device *pdev = data->asc;
	uint32_t mask, val, offset;
	struct st_scr_data *scr_config = &data->scr_config[id];
	offset = scr_config->reg_offset;
	data->pins_default = pinctrl_lookup_state(data->pinctrl,
			PINCTRL_STATE_DEFAULT);
	if (IS_ERR(data->pins_default)) {
		dev_err(&pdev->dev, "could not get default pinstate\n");
		return -EINVAL;
	} else
		pinctrl_select_state(data->pinctrl, data->pins_default);
	for (i=0; i<MAX_REGFIELDS; i++) {
		mask = scr_config->reg_fields[i];
		val = scr_config->reg_fields[i];
		regmap_update_bits(data->regmap, offset, 1 << mask, 1 << val);
	};
	return 0;
}
#else
static void *stm_scr_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_scr_platform_data *data;

	struct device_node *cn = of_parse_phandle(np, "uart", 0);
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);

	if (!data) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	of_property_read_string(np, "st,clk-id",
			(const char **)&data->clock_id);

	data->pad_config = stm_of_get_pad_config_index(&pdev->dev, 0);
	if (!data->pad_config)
		return ERR_PTR(-EFAULT);

	data->class_sel_pad = stm_of_get_pad_config_index(&pdev->dev, 1);
	if (!data->class_sel_pad)
		return ERR_PTR(-EFAULT);

	if(of_property_read_u32(np, "asc-fifo-size", &data->asc_fifo_size))
		return ERR_PTR(-EFAULT);

	data->device_config = stm_of_get_dev_config(&pdev->dev);

	data->asc = of_find_device_by_node(cn);
	if (!data->asc) {
		dev_err(&data->asc->dev, "No uart device exists\n");
		return ERR_PTR(-EFAULT) ;
	}

	data->asc->dev.platform_data = stm_asc_dt_get_data(data->asc);
	if (!data->asc->dev.platform_data)
		return ERR_PTR(-EFAULT);

	return data;
}
#endif
#else

static void *stm_scr_dt_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

static int __init scr_probe(struct platform_device * pdev)
{
	struct stm_scr_platform_data *plat_data = NULL;
	int id;
	int err = 0;
	struct clk *clk;

	if (pdev->dev.of_node) {
		scr_debug_print("SCR DT node exists \n");
		id = of_alias_get_id(pdev->dev.of_node, "scr");
		plat_data = stm_scr_dt_get_pdata(pdev);
		pdev->id = id;
	} else {
		plat_data = pdev->dev.platform_data;
		id = pdev->id;
	}

	if (!plat_data || IS_ERR(plat_data)) {
		dev_err(&pdev->dev, "No platform data found\n");
		return -ENODEV;
	}
#ifdef CONFIG_ARCH_STI
	if (pdev->dev.of_node) {
		err = sti_scr_set_config(plat_data, id);
		if (err)
			return err;
	}
#else
	if (pdev->dev.of_node) {
		scr_data[id].device_state = devm_stm_device_init(&pdev->dev,
						plat_data->device_config);

		if (!scr_data[id].device_state)
			return -EBUSY;
	}
#endif
	scr_data[id].fops = scr_get_fops();
	scr_data[id].scr_major = scr_major;
	scr_data[id].scr_class = scr_class;

	scr_data[id].base_address =
		platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scr_data[id].id = id;
	scr_data[id].scr_platform_data =
		(struct stm_scr_platform_data *)(plat_data);
	scr_data[id].asc_platform_device =
		(struct platform_device *)scr_data[id].
						scr_platform_data->asc;
#ifdef CONFIG_ARCH_STI
	scr_data[id].clk = of_clk_get_by_name(pdev->dev.of_node, "scr_clk");
	if(IS_ERR(scr_data[id].clk)) {
		ca_error_print("bad scr clk");
		return 1;
	} else
		scr_debug_print(" <%s>:: <%d> -- uart scr clk_p= %p",
			__func__, __LINE__,
			scr_data[id].clk);

	scr_data[id].clk_ext =
		of_clk_get_by_name(pdev->dev.of_node, "scr_clk_ext");
	if(IS_ERR(scr_data[id].clk_ext)) {
		ca_error_print("bad scr clk ext");
		return 1;
	} else
		scr_debug_print(" <%s>:: <%d> -- uart scr clk_ext p=%p",
			__func__, __LINE__,
			scr_data[id].clk_ext);
#else
	scr_data[id].clk = clk_get(&pdev->dev,
			scr_data[id].scr_platform_data->clock_id);
#endif
	scr_data[id].init = false;
	scr_debug_print(" <%s>:: <%d> -- uart %s.%d %s %x\n",
		__func__, __LINE__,
		pdev->name,
		id,
		scr_data[id].scr_platform_data->asc->name,
		(int)scr_data[id].base_address);

	scr_debug_print(" <%s>:: <%d> -- uart platform data %s\n",
		__func__, __LINE__,
		scr_data[id].asc_platform_device->name);
	scr_data[id].pdev = pdev;
	platform_set_drvdata(pdev, &scr_data[id]);

	/* Create the char device now */
	cdev_init(&scr_data[id].device_data.cdev,
			scr_data[id].fops);
	scr_data[id].device_data.cdev.owner = THIS_MODULE;
	scr_data[id].device_data.devt = MKDEV(scr_major, id);

	kobject_set_name(&scr_data[id].device_data.cdev.kobj, "scr%d", id);
	err = cdev_add(&scr_data[id].device_data.cdev,
			scr_data[id].device_data.devt, 1);
	if (err) {
		ca_error_print("character device add failed");
		goto add_fail;
	}

	scr_data[id].device_data.cdev_class_dev =
			device_create(scr_data[id].scr_class, NULL,
				scr_data[id].device_data.devt,
				&scr_data[id],
				"%s%d", SCR_DEVICE_NAME, id);
	if (IS_ERR(scr_data[id].device_data.cdev_class_dev)) {
		err = PTR_ERR(scr_data[id].device_data.cdev_class_dev);
		ca_error_print("device create add failed");
		goto create_fail;
	}

#ifdef CONFIG_ARCH_STI
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		printk("scr clock not found!%p\n", &pdev->dev);
	}
	else
		clk_prepare_enable(clk);
	
	err = pm_runtime_set_active(&pdev->dev);
	if (err) {
		pr_err("pm_runtime_set_active error 0x%x\n", err);
		goto error_post_pm;
	}

	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return 0;

error_post_pm:
	pm_runtime_disable(&pdev->dev);
create_fail:
	cdev_del(&scr_data[id].device_data.cdev);
add_fail:
	return err;
}

static int scr_remove(struct platform_device *pdev)
{
	int id;
	struct cdev *cdev_p = NULL;

	/* Disable power management for this device */
	pm_runtime_disable(&pdev->dev);
	if (pdev->dev.of_node) {
		scr_debug_print("SCR DT node exists \n");
		id = of_alias_get_id(pdev->dev.of_node, "scr");
	} else {
		id = pdev->id;
	}
	if ((scr_data[id].device_data.cdev_class_dev) &&\
		(scr_data[id].scr_class))
		device_destroy(scr_data[id].scr_class,
					scr_data[id].device_data.devt);
	cdev_p = &scr_data[id].device_data.cdev;
	if (cdev_p != NULL)
		cdev_del(cdev_p);
	return 0;
}

#ifdef CONFIG_PM
static void scr_set_card_status(scr_ctrl_t *scr_handle_p)
{
	int32_t error = 0;
	stm_event_t event;
	uint32_t frequency = 0;
	frequency = scr_handle_p->scr_caps.working_frequency;

	scr_handle_p->card_present = scr_card_present(scr_handle_p);
	if (scr_handle_p->card_present) {
		if (scr_handle_p->state == SCR_THREAD_STATE_CARD_NOT_PRESENT) {
			event.object = (void *)scr_handle_p;
			event.event_id = STM_SCR_EVENT_CARD_INSERTED;
			error = stm_event_signal(&event);
			if (error)
				ca_error_print("stm_event_signal "\
					"failed(%d)\n", error);
		}

		scr_debug_print("<%s> :: <%d> [PM] card present -- reset\n",
				__func__, __LINE__);
		scr_handle_p->scr_caps.working_frequency =
			scr_handle_p->reset_frequency;

		error = scr_internal_reset(scr_handle_p, STM_SCR_RESET_COLD);
		if (!error) {
			scr_handle_p->state = SCR_THREAD_STATE_PROCESS;

			scr_change_clock_frequency(scr_handle_p,
					frequency);

		} else {
			scr_error_print("<%s> :: <%d> [PM] reset failed\n",
					__func__, __LINE__);
			scr_internal_deactivate(scr_handle_p);
			scr_handle_p->state = SCR_THREAD_STATE_CARD_PRESENT;
		}
	} else {
		scr_debug_print("<%s> :: <%d> [PM] card not present\n",
					__func__, __LINE__);
		if (scr_handle_p->state != SCR_THREAD_STATE_IDLE) {
			event.object = (void *)scr_handle_p;
			event.event_id = STM_SCR_EVENT_CARD_REMOVED;
			error = stm_event_signal(&event);
			if (error)
				ca_error_print("stm_event_signal "\
					"failed(%d)\n", error);
		}
		scr_internal_deactivate(scr_handle_p);
		scr_handle_p->state = SCR_THREAD_STATE_IDLE;
	}
}

static int scr_runtime_suspend(struct device *dev)
{
	struct clk *clk;
	struct platform_device *pdev = to_platform_device(dev);

	printk("<%s> :: <%d> [PM] devid %d clk name = %s\n",
			__func__, __LINE__, pdev->id, pdev->name);
#ifdef CONFIG_ARCH_STI
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		printk("scr clock not found!%p\n", &pdev->dev);
	}
	else
		clk_disable_unprepare(clk);

	return 0;
}

static int scr_runtime_resume(struct device *dev)
{
	struct clk *clk;
	struct platform_device *pdev = to_platform_device(dev);
	printk("<%s> :: <%d> [PM] devid %d clk name = %s\n",
			__func__, __LINE__, pdev->id, pdev->name);
#ifdef CONFIG_ARCH_STI
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		printk("scr clock not found!%p\n", &pdev->dev);
	}
	else
		clk_prepare_enable(clk);

	return 0;
}
static int scr_runtime_idle(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	printk("<%s> :: <%d> [PM] devid %d clk name = %s\n",
			__func__, __LINE__, pdev->id, pdev->name);
	/* nothing to do */
	return 0;
}

static int scr_stm_suspend(struct device *dev)
{
	int32_t error = 0;
	scr_ctrl_t *scr_handle_p = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	scr_debug_print("<%s> :: <%d> [PM] devid %d\n",
		__func__, __LINE__, pdev->id);
#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then no
	* need to suspend again */
	if (pm_runtime_suspended(dev))
		return 0;
#endif
	if (!scr_handle_p) {
		scr_error_print("<%s> :: <%d> [PM] invalid device\n",
				__func__, __LINE__);
		return 0;
	}
	if ((VALIDATE_HANDLE(scr_handle_p->magic_num)) != 0) {
		scr_debug_print("<%s> :: <%d> [PM] SCR not initialized\n",
			__func__, __LINE__);
		/*Not an error, device present but not initialized*/
		return 0;
	}

	scr_asc_abort(scr_handle_p);
	error = scr_internal_deactivate(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] deactivation failed\n",
			__func__, __LINE__);
		return error;
	}
	scr_disable_irq(scr_handle_p);

#ifdef CONFIG_ARCH_STI
	error = scr_delete_pio(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] pio suspend failed\n",
			__func__, __LINE__);
		return error;
	}
	error = scr_asc_freeze(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] asc suspend failed\n",
			__func__, __LINE__);
		return error;
	}
#endif
	return 0;
}

static int scr_stm_resume(struct device *dev)
{

#ifdef CONFIG_ARCH_STI
	int32_t error = 0;
#endif
	scr_ctrl_t *scr_handle_p = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then don't
	* resume */
	if (pm_runtime_suspended(dev))
		return 0;
#endif
	if (!scr_handle_p) {
		scr_error_print("<%s> :: <%d> [PM] invalid device\n",
			__func__, __LINE__);
		return 0;
	}
	scr_debug_print("<%s> :: <%d> [PM] pdevid %d\n",
		__func__, __LINE__, pdev->id);

	if ((VALIDATE_HANDLE(scr_handle_p->magic_num)) != 0) {
		scr_debug_print("<%s> :: <%d> [PM] SCR not initialized\n",
			__func__, __LINE__);
		/*Not an error, device present but not initialized*/
		return 0;
	}
#ifdef CONFIG_ARCH_STI
#ifndef CONFIG_OF
	if (scr_config(scr_data[pdev->id]))
		return -1;
#else
	error = sti_scr_set_config(
			scr_handle_p->scr_plat_data->scr_platform_data,
			pdev->id);
	if(error)
		return error;
#endif

	error = scr_asc_restore(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] ASC resume failed\n",
			__func__, __LINE__);
		return error;
	}

	scr_asc_abort(scr_handle_p);

	error = scr_initialise_pio(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] pio resume failed\n",
			__func__, __LINE__);
		return error;
	}
	scr_set_card_status(scr_handle_p);
	scr_disable_irq(scr_handle_p);
	scr_enable_irq(scr_handle_p, scr_handle_p->card_present);
#else
#ifdef CONFIG_OF
	stm_device_setup(scr_handle_p->device_state);
#endif
	/* Not error will occur*/
	scr_set_card_status(scr_handle_p);
	scr_enable_irq(scr_handle_p, scr_handle_p->card_present);
#endif
	return 0;
}

#ifndef CONFIG_OF
static int scr_config(struct scr_platform_data_s scr_data)
{
	unsigned int num, i;
	unsigned char __iomem *mapped_config_address;
	struct scr_config_data *config_data =
		scr_data.scr_platform_data->config_data;
	num = scr_data.scr_platform_data->num_config_data;
	for (i = 0; i < num; i++) {
		scr_debug_print("<%s> :: <%d> [PM] address %x mask %x\n",
			__func__, __LINE__,
			config_data[i].address,
			config_data[i].mask);
		mapped_config_address =
		ioremap_nocache(config_data[i].address,
		4096);
		if (!mapped_config_address)
			return -1;
		writel(readl(mapped_config_address) |
			config_data[i].mask,
			mapped_config_address);
		iounmap(mapped_config_address);
	}
	return 0;
}
#endif

#ifndef CONFIG_ARCH_STI
static int scr_stm_freeze(struct device *dev)
{
	int32_t error = 0;
	scr_ctrl_t *scr_handle_p = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	scr_debug_print("<%s> :: <%d> [PM] devid %d\n",
		__func__, __LINE__, pdev->id);
	if (!scr_handle_p) {
		scr_error_print("<%s> :: <%d> [PM] invalid device\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_handle_p->magic_num)) != 0) {
		scr_debug_print("<%s> :: <%d> [PM] SCR not initialized\n",
			__func__, __LINE__);
		/*Not an error, device present but not initialized*/
		return 0;
	}

	scr_asc_abort(scr_handle_p);
	error = scr_internal_deactivate(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] deactivation failed\n",
			__func__, __LINE__);
		return error;
	}
	error = scr_delete_pio(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] pio suspend failed\n",
			__func__, __LINE__);
		return error;
	}
	error = scr_asc_freeze(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] asc suspend failed\n",
			__func__, __LINE__);
		return error;
	}

	return 0;
}

static int scr_stm_restore(struct device *dev)
{
	int32_t error = 0;
	scr_ctrl_t *scr_handle_p = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	if (!scr_handle_p) {
		scr_error_print("<%s> :: <%d> [PM] invalid device\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	scr_debug_print("<%s> :: <%d> [PM] pdev->id %d\n",
		__func__, __LINE__, pdev->id);
#ifndef CONFIG_OF
	if (scr_config(scr_data[pdev->id]))
		return -1;
#else
#ifdef CONFIG_ARCH_STI
	error = sti_scr_set_config(
			scr_handle_p->scr_plat_data->scr_platform_data,
			pdev->id);
	if(error)
		return error;
#else
	stm_device_setup(scr_handle_p->device_state);
#endif
#endif

	if ((VALIDATE_HANDLE(scr_handle_p->magic_num)) != 0) {
		scr_debug_print("<%s> :: <%d> [PM] SCR not initialized\n",
			__func__, __LINE__);
		/*Not an error, device present but not initialized*/
		return 0;
	}

	error = scr_asc_restore(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] ASC resume failed\n",
			__func__, __LINE__);
		return error;
	}

	scr_asc_abort(scr_handle_p);

	error = scr_initialise_pio(scr_handle_p);
	if (error) {
		scr_error_print("<%s> :: <%d> [PM] pio resume failed\n",
			__func__, __LINE__);
		return error;
	}

	scr_set_card_status(scr_handle_p);

	scr_disable_irq(scr_handle_p);
	scr_enable_irq(scr_handle_p, scr_handle_p->card_present);
	return 0;
}
#endif

static struct dev_pm_ops stm_scr_pm_ops = {
	.suspend = scr_stm_suspend, /* on standby/memstandby */
	.resume = scr_stm_resume, /* resume from standby/memstandby */
#ifndef CONFIG_ARCH_STI
	.freeze = scr_stm_freeze, /* hibernation */
	.restore = scr_stm_restore, /* resume from hibernation */
#endif
	.runtime_suspend = scr_runtime_suspend,
	.runtime_resume = scr_runtime_resume,
	.runtime_idle = scr_runtime_idle,
};
#endif

static struct platform_driver __refdata stm_scr_driver = {
	.driver = {
		.name = "stm_scr",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_scr_match),
#ifdef CONFIG_PM
		/*For power management*/
		.pm = &stm_scr_pm_ops,
#endif
	},
	.probe = scr_probe,
	.remove = scr_remove,
};

static int __init scr_init_module(void)
{
	int err = 0;
#ifndef CONFIG_OF
	arch_modprobe_func();
#endif
	printk(KERN_ALERT "Load module stm_scr by %s (pid %i)\n",
		current->comm, current->pid);
	scr_data[0].base_address = 0;
	scr_data[1].base_address = 0;
	ca_os_sema_initialize(&g_scr_sem_lock, 1);
	scr_class = class_create(THIS_MODULE, SCR_DEVICE_NAME);
	if (IS_ERR(scr_class)) {
		err = PTR_ERR(scr_class);
		goto class_fail;
	}

	err = alloc_chrdev_region(&scr_dev, 0, STM_SCR_MAX, SCR_DEVICE_NAME);
	if (err)
		goto chr_alloc_fail;

	scr_major = MAJOR(scr_dev);

	err = platform_driver_register(&stm_scr_driver);
	if (err)
		goto pltf_register_fail;

	scr_register_default_ca_ops();

	return 0;

pltf_register_fail:
	unregister_chrdev_region(scr_dev, STM_SCR_MAX);
chr_alloc_fail:
	class_destroy(scr_class);
class_fail:
	return err;
}

static void __exit scr_exit_module(void)
{
	platform_driver_unregister(&stm_scr_driver);
	/*TO DO error checking */
	class_destroy(scr_class);
	unregister_chrdev_region(scr_dev, STM_SCR_MAX);
	scr_data[0].base_address = 0;
	scr_data[1].base_address = 0;

	ca_os_sema_terminate(g_scr_sem_lock);
	printk(KERN_ALERT "Unload module stm_scr by %s (pid %i)\n",
		current->comm, current->pid);
}

module_init(scr_init_module);
module_exit(scr_exit_module);
