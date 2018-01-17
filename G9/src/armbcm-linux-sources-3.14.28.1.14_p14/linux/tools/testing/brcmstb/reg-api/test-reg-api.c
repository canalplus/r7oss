#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/gpio.h>
#include <linux/brcmstb/brcmstb.h>
#include <linux/brcmstb/gpio_api.h>

#define GIO_BANK_SIZE	0x20

static int test_gpio_bank(uint32_t offset, unsigned int width)
{
	unsigned int bank, banks;
	int ret;

	banks = width / GIO_BANK_SIZE;

	pr_info("Testing controller at 0x%08x, width: 0x%08x banks: %d\n",
		offset, width, banks);

	for (bank = 0; bank < banks; bank++) {
		ret = brcmstb_update32(offset + bank * GIO_BANK_SIZE, 0xf, 1);
		if (ret)
			pr_err("%s: failed to request bank %d\n", __func__, bank);

		ret = brcmstb_update32(offset + bank * GIO_BANK_SIZE, 0xf << 8, 1);
		if (ret)
			pr_err("%s: failed to request bank %d\n", __func__, bank);

		ret = brcmstb_update32(offset + bank * GIO_BANK_SIZE, 0xf << 16, 1);
		if (ret)
			pr_err("%s: failed to request bank %d\n", __func__, bank);

		ret = brcmstb_update32(offset + bank * GIO_BANK_SIZE, 0xf << 24, 1);
		if (ret)
			pr_err("%s: failed to request bank %d\n", __func__, bank);
	}

	return 0;
}

static unsigned int request_linux_gpios(unsigned int width, unsigned int base)
{
	unsigned int bank, banks;
	unsigned int num;

	banks = width / GIO_BANK_SIZE;

	for (bank = 0; bank < banks; bank++) {
		num = base + bank * 32;
	 	gpio_request(num, "Linux test");
		gpio_request(num + 8, "Linux test");
		gpio_request(num + 16, "Linux test");
		pr_info("%s: bank %d request GPIOs %d, %d, %d\n",
			__func__, bank, num, num + 8, num + 16);
	}

	return banks * 32;
}

static int __init test_init(void)
{
	unsigned int times, base;
	int ret;

	ret = brcmstb_update32(BCHP_UARTA_REG_START, 0xf, 1);
	if (ret >= 0)
		pr_err("%s: this should fail!\n", __func__);

	base = request_linux_gpios(BCHP_GIO_REG_END - BCHP_GIO_REG_START + 4, 0);
	request_linux_gpios(BCHP_GIO_AON_REG_END - BCHP_GIO_AON_REG_START + 4, base);

	for (times = 0; times < 2; times++) {

		ret = test_gpio_bank(BCHP_GIO_REG_START,
				     BCHP_GIO_REG_END - BCHP_GIO_REG_START + 4);
		if (ret)
			pr_err("%s: GIO_REG_START failed!\n", __func__);

		ret = test_gpio_bank(BCHP_GIO_AON_REG_START,
				     BCHP_GIO_AON_REG_END - BCHP_GIO_AON_REG_START + 4);
		if (ret)
			pr_err("%s: GIO_AON_REG_START failed!\n", __func__);
	}

	return 0;
}

static void __exit test_exit(void)
{
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Fainelli (Broadcom Corporation)");
