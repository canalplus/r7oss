/*---------------------------------------------------------------------------
* /drivers/stm/stm_power_hps_ir.c
* Copyright (C) 2010 STMicroelectronics Limited
* Author:
* May be copied or modified under the terms of the GNU General Public
* License.  See linux/COPYING for more information.
*----------------------------------------------------------------------------
*/
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#ifdef CONFIG_ARCH_STI
/* TO BE REPLACED AS SOON AS POWER MNGT WILL BE DELIVERED */
#include <linux/list.h>

enum stm_pm_notify_ret {
	STM_PM_RET_OK,
	STM_PM_RET_AGAIN,
};
struct stm_lpm_ir_fifo {
	u16 mark;
	u16 symbol;
};
struct stm_lpm_ir_key {
	u8 key_index;
	u8 num_patterns;
	struct stm_lpm_ir_fifo fifo[64];
};
struct stm_lpm_ir_keyinfo {
	u8 ir_id;
	u16 time_period;
	u16 time_out;
	u8 tolerance;
	struct stm_lpm_ir_key ir_key;
};
struct stm_pm_notify {
	struct list_head list;
	int irq;
	enum stm_pm_notify_ret (*notify)(void);
};
enum stm_pm_notify_ret stm_pm_early_check(int irq_reason);

//int stm_register_pm_notify(struct stm_pm_notify *notify);

//int stm_unregister_pm_notify(struct stm_pm_notify *notify);
#else
#include <linux/stm/lpm.h>
#include <linux/stm/platform.h>
#include <linux/stm/pm_notify.h>

#endif

#define SET_NEC

#define HPS_IR_POWER_DEBUG  0
#define HPS_IR_POWER_ERROR  1

#if HPS_IR_POWER_ERROR
#define power_error_print(fmt, ...)         pr_err(fmt,  ##__VA_ARGS__)
#else
#define power_error_print(fmt, ...)
#endif

#if HPS_IR_POWER_DEBUG
#define power_debug_print(fmt, ...)         pr_info(fmt,  ##__VA_ARGS__)
#else
#define power_debug_print(fmt, ...)
#endif


#define IR_IRQ  235
#define IR_BASE_ADDRSS  0xFE518000
#define IR_LENTH (4*1024)
#define IR_MARK_OFFSET 0x40
#define IR_SYMBOL_OFFSET 0x44

#define IRB_RX_STATUS           0x6C	/* receive status     */
#define LIRC_STM_IS_OVERRUN	0x04
#define LIRC_STM_CLEAR_OVERRUN	0x04
#define IRB_RX_INT_CLEAR        0x58	/* overrun status     */
#define IRB_RX_SYS              0X44	/* sym period capture */
#define IRB_RX_ON               0x40	/* pulse time capture */
#define IRB_RX_INT_STATUS       0x4C	/* IRQ status (R/W)   */
#define IRB_FIFO		0xFF04  /* Data avaiabilty in IR fifo*/
#define IRB_RX_CLEAR_BITS	0x3F	/* clear IR IP IT*/

static void  __iomem *ir_base_address;

enum stm_pm_notify_ret ir_notifier_fn(void)
{
	int i = 0;
	int count = 0;
	struct stm_lpm_ir_keyinfo hw_ir = {0};
	struct stm_lpm_ir_keyinfo ir_key[2];
	u16 low;
	u16 high;
	u16 offset;
	int fifo_status = 0;

	fifo_status = readl(ir_base_address + IRB_RX_STATUS);
	power_debug_print("fifo state %x\n", fifo_status);

	/*
	 * Read IR Fifo here
	*/
	while ((readl(ir_base_address+IRB_RX_STATUS) & IRB_FIFO) != 0) {
		if (unlikely(readl(ir_base_address + IRB_RX_INT_STATUS)
		& LIRC_STM_IS_OVERRUN)) {
			power_error_print("IR RX overrun\n");
			writel(LIRC_STM_CLEAR_OVERRUN,
				ir_base_address + IRB_RX_INT_CLEAR);
		}
		hw_ir.ir_key.fifo[i].mark =
				readl(ir_base_address + IR_MARK_OFFSET);
		hw_ir.ir_key.fifo[i].symbol =
				readl(ir_base_address + IR_SYMBOL_OFFSET);
		i++;
	}
	count = i;
	/*clear IP IT*/
	writel(IRB_RX_CLEAR_BITS, ir_base_address + IRB_RX_INT_CLEAR);
#if HPS_IR_POWER_DEBUG
	power_debug_print("\n total i %d\n", i);
	for (i = 0; i < count; i++) {
		power_debug_print("recd mark %x , sym %x\n",
		hw_ir.ir_key.fifo[i].mark, hw_ir.ir_key.fifo[i].symbol);
	}
#endif
#ifdef SET_NEC
	ir_key[0].ir_id = 8;
	ir_key[0].time_period = 560;
	ir_key[0].tolerance = 65;
	ir_key[0].ir_key.key_index = 0x0;
	/*copy the mark and symbol data for NEC power key*/
	ir_key[0].ir_key.fifo[0].mark = 8960;
	ir_key[0].ir_key.fifo[0].symbol = 13440;
	for (i = 1; i < 9; i++) {
		ir_key[0].ir_key.fifo[i].mark = 560;
		ir_key[0].ir_key.fifo[i].symbol = 1120;
	}
	for (i = 9; i < 16; i++) {
		ir_key[0].ir_key.fifo[i].mark = 560*1;
		ir_key[0].ir_key.fifo[i].symbol = 560*4;
	}
	for (i = 16; i < 25; i++) {
		ir_key[0].ir_key.fifo[i].mark = 560;
		ir_key[0].ir_key.fifo[i].symbol = 1120;
	}
	for (i = 25; i < 32; i++) {
		ir_key[0].ir_key.fifo[i].mark = 560;
		ir_key[0].ir_key.fifo[i].symbol = 560*4;
	}
	ir_key[0].ir_key.fifo[32].mark = 560;
	ir_key[0].ir_key.fifo[32].symbol = 560*4;
	ir_key[0].ir_key.fifo[33].mark = 560;
	ir_key[0].ir_key.fifo[33].symbol = 560*2;
	ir_key[0].ir_key.num_patterns = 33;

	if (count <= ir_key[0].ir_key.num_patterns) {
		/* This is added for debug in case required
		* in future
		*/
		/* ir_key[0].ir_key.num_patterns = count;*/
		power_error_print("no sufficient data from IR fifo\n");
		return STM_PM_RET_AGAIN;
	}

	for (i = 0 ; i < ir_key[0].ir_key.num_patterns; i++) {
		/* get the low and high mark to compare*/
		offset = ir_key[0].ir_key.fifo[i].mark * ir_key[0].tolerance;
		offset /= 100;
		low = ir_key[0].ir_key.fifo[i].mark - offset;
		high = ir_key[0].ir_key.fifo[i].mark + offset;
		power_debug_print("Mark low %x and high %x mark %x\n",
			low, high, hw_ir.ir_key.fifo[i].mark);
		/* check if received mark is in with range*/
		if (hw_ir.ir_key.fifo[i].mark < low
			|| hw_ir.ir_key.fifo[i].mark > high) {
			power_error_print("mark no match\n");
			return STM_PM_RET_AGAIN;
		}
		/* get the low and high symbol to compare*/
		offset = ir_key[0].ir_key.fifo[i].symbol * ir_key[0].tolerance;
		offset /= 100;
		low = ir_key[0].ir_key.fifo[i].symbol - offset;
		high = ir_key[0].ir_key.fifo[i].symbol + offset;
		power_debug_print("symbol low %x and high %x sym %x\n",
			low, high, hw_ir.ir_key.fifo[i].symbol);
		/* check if received symbol is in with range*/
		if (hw_ir.ir_key.fifo[i].symbol < low
				|| hw_ir.ir_key.fifo[i].symbol > high) {
			power_error_print("sym no match\n");
			return STM_PM_RET_AGAIN;
		}
	}
#endif

	return STM_PM_RET_OK;
}

static struct stm_pm_notify ir_notifier;

static int __init stm_lpm_hps_init(void)
{

	int err;
	ir_notifier.irq = IR_IRQ;
	ir_notifier.notify = ir_notifier_fn;
	err = 0;//stm_register_pm_notify(&ir_notifier);
	if (err) {
		power_error_print("stm_register_pm_notify failed\n");
		return err;
	}
	ir_base_address = ioremap_nocache(IR_BASE_ADDRSS, IR_LENTH);
	if (ir_base_address == NULL) {
		power_error_print("ir_base_address map failed\n");
		return -ENOMEM;
	}

	return 0;
}

void __exit stm_lpm_hps_exit(void)
{
	//stm_unregister_pm_notify(&ir_notifier);
	iounmap(ir_base_address);

}

module_init(stm_lpm_hps_init);
module_exit(stm_lpm_hps_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("lpm device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
