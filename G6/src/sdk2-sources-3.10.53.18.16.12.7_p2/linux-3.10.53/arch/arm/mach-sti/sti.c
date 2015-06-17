/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/clk-provider.h>
#include <linux/of_gpio.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#ifdef CONFIG_SOC_BUS
#include <linux/sys_soc.h>
#endif
#include <linux/stat.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

/*
 * Temporary function to enable clocks in ClockgenTel:
 * -VCO to 540 MHz
 * -ETH0 (chan2) to 25 MHz
 * To be removed as soon as ClockenTel is supported by GCF
 */
static void stid127_setup_clockgentel(void)
{
	void __iomem *base;
	int tmp;

	base = ioremap(0xfe910000, SZ_128);
	if (!base) {
		pr_err("%s: failed to map QFS_TEL base address (%ld)\n",
				__func__, PTR_ERR(base));
		return;
	}

	/* setup VCO_USB to 540 MHz on QFS_TEL */
	writel(0x208, base);
	tmp = readl(base + 0x1c);
	writel(tmp & 0x1fe, base + 0x1c);
	tmp =  readl(base + 0x20);
	writel(tmp & 0x1fe, base + 0x20);

	/* Set ETH0 to 25MHz (chan 2)*/
	tmp = readl(base);
	writel(tmp | BIT(6), base);
	tmp = readl(base + 0x20);
	writel(tmp & ~BIT(3), base + 0x20);
	writel(0x125ccccd, base + 0xc);
	writel(0x124ccccd, base + 0xc);
	tmp = readl(base + 0x1c);
	writel(tmp & ~BIT(3), base + 0x1c);

	/* setup CCM_USB: divide (540MHz) src by 45 to get 12 MHz on phy USB2*/
	writel(0x2012d, base + 0x64);
	writel(0xc, base + 0x74);

	iounmap(base);

	return;
}

static void __init soc_info_populate(struct soc_device_attribute *soc_dev_attr)
{
	char *family;
	char *model;
	char *pch;
	int family_length;
	unsigned long dt_root;

	soc_dev_attr->machine = "STi";

	dt_root = of_get_flat_dt_root();
	model = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!model)
		return;

	pch = strchr(model, ' ');
	if (!pch)
		return;
	family_length = pch - model;

	family = kzalloc((family_length + 1) * sizeof(char), GFP_KERNEL);
	if (!family)
		return;

	/* Extract the SoC name from the "model" DT entry */
	strncat(family, model, family_length);
	soc_dev_attr->family = kasprintf(GFP_KERNEL, "%s", family);
}

static struct device *sti_soc_device_init(void)
{
	struct soc_device *soc_dev;
	struct soc_device_attribute *soc_dev_attr;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return NULL;

	soc_info_populate(soc_dev_attr);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr);
		return NULL;
	}

	return soc_device_to_device(soc_dev);
}

#define ST_GPIO_PINS_PER_BANK (8)
#define TSOUT1_BYTECLK_GPIO (5 * ST_GPIO_PINS_PER_BANK + 0)

/* On stid127-b2112 PIO5[0] (TSOUT1_BYTECLK) goes to CN10 (TSAERROR_TSAERROR).
 * On b2105, CN29 (NIM3) it is wired to SIS2_ERROR.
 * This patch is to force TSOUT1_BYTECLK to low for NIM3 connection with
 * Cannes
 */
void __init sti_init_machine_late(void)
{
	if (of_machine_is_compatible("st,stid127"))
		gpio_request_one(TSOUT1_BYTECLK_GPIO,
			GPIOF_OUT_INIT_LOW, "tsout1_byteclk");
}

void __init sti_init_machine(void)
{
	struct platform_device_info devinfo = { .name = "cpufreq-cpu0", };
	struct device *parent = NULL;

	parent = sti_soc_device_init();

	of_platform_populate(NULL, of_default_bus_match_table, NULL, parent);

	if (of_machine_is_compatible("st,stid127"))
		stid127_setup_clockgentel();

	platform_device_register_full(&devinfo);
}
