// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the ROHM BD96811 pin configuration
 *
 * Copyright (C) 2023 ROHM Semiconductor
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinmux.h>

#include <linux/mfd/rohm-bd96811.h>

#include "pinctrl-utils.h"

#define BD96811_AD_TH_MAX 0xff

struct bd96811_pinctrl {
	struct regmap *regmap;
	struct device *dev;
	struct pinctrl_desc pdesc;
};

/* PINs - most functions are set by OTP. ADC1/DIN4 & ADC2/DOUT4 can be muxed */
static const struct pinctrl_pin_desc bd96811_pins[] = {
	PINCTRL_PIN(0, "DIN1"), /* EWnable input for GRP1 */
	PINCTRL_PIN(1, "DIN2"), /* EWnable input for GRP2 */
	PINCTRL_PIN(2, "DIN3"), /* EN_GRP3, ERR_IN */

	 /* ADC1 or DIN4 (ERR_CNT_CLR, ERR_IN, STR_SET1, STR_SET2) */
	PINCTRL_PIN(3, "DIN4"),

	/*
	 * PGD_GRP0, PGD_GRP1, PGD_GRP2, PGD_GRP3, PGD_SYS1, ERROUTB, REGOUT,
	 * STR_ENT
	 */
	PINCTRL_PIN(4, "DOUT1"),

	/*
	 * PGD_GRP0, PGD_GRP1, PGD_GRP2, PGD_GRP3, PGD_SYS2 ERROUTB, REGOUT,
	 * STR_ENT
	 */
	PINCTRL_PIN(5, "DOUT2"),

	/* AD0, PGD_GRP0, PGD_GRP1, PGD_GRP2, PGD_GRP3 */
	PINCTRL_PIN(6, "DOUT3"),
	PINCTRL_PIN(7, "DOUT4"), /* ADC2 or DOUT4 (PGD_GRP2) */
};

enum {
	BD96811_FSEL_GPIO_ADC1,
	BD96811_FSEL_GPIO_ADC2,
	BD96811_FSEL_DIN4,
	BD96811_FSEL_DOUT4,
	BD96811_NUM_FSEL,
};

/* Pin functions */
static const char * const bd96811_functions[BD96811_NUM_FSEL] = {
	[BD96811_FSEL_GPIO_ADC1] = "adc1",
	[BD96811_FSEL_GPIO_ADC2] = "adc2",
	[BD96811_FSEL_DIN4] = "din4",
	[BD96811_FSEL_DOUT4] = "dout4",
};

enum {
	BD96811_GRP_ADC1,
	BD96811_GRP_ADC2,
	BD96811_GRP_DIN4,
	BD96811_GRP_DOUT4,
};

static const char * const bd96811_pin_groups[] = {
	[BD96811_GRP_ADC1] = "adc1",
	[BD96811_GRP_ADC2] = "adc2",
	[BD96811_GRP_DIN4] = "din4",
	[BD96811_GRP_DOUT4] = "dout4",
};

static int bd96811_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(bd96811_pin_groups);
}

static const char *bd96811_get_group_name(struct pinctrl_dev *pctldev,
					  unsigned int group)
{
	return bd96811_pin_groups[group];
}

static int bd96811_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	return BD96811_NUM_FSEL;
}

static const char *bd96811_pmx_get_function_name(struct pinctrl_dev *pctldev,
						 unsigned int selector)
{
	return bd96811_functions[selector];
}

static int bd96811_pmx_get_function_groups(struct pinctrl_dev *pcdev,
					   unsigned int selector,
					   const char * const **groups,
					   unsigned int * const num_groups)
{
	if (selector <= BD96811_FSEL_DOUT4) {
		*groups = &bd96811_pin_groups[selector];
		*num_groups = 1;

		return 0;
	}

	return -EINVAL;
}

static int bd96811_pmx_set(struct pinctrl_dev *pcdev, unsigned int func,
			   unsigned int grp)
{
	struct bd96811_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	switch (func) {
	case BD96811_FSEL_GPIO_ADC1:
		if (grp != BD96811_GRP_ADC1)
			return -EINVAL;
		return regmap_write(data->regmap, BD96811_REG_AD1_TH, BD96811_AD_TH_MAX);

	case BD96811_FSEL_GPIO_ADC2:
		if (grp != BD96811_GRP_ADC2)
			return -EINVAL;
		return regmap_write(data->regmap, BD96811_REG_AD2_TH, BD96811_AD_TH_MAX);

	case BD96811_FSEL_DIN4:
		if (grp != BD96811_GRP_DIN4)
			return -EINVAL;
		return regmap_write(data->regmap, BD96811_REG_AD1_TH, 0);

	case BD96811_FSEL_DOUT4:
		if (grp != BD96811_GRP_DOUT4)
			return -EINVAL;
		return regmap_write(data->regmap, BD96811_REG_AD2_TH, 0);

	default:
		dev_err(data->dev, "Unsupported pin function, %d\n", func);
	}

	return -EINVAL;
}

static const struct pinmux_ops bd96811_pmxops = {
	.get_functions_count = bd96811_pmx_get_functions_count,
	.get_function_name = bd96811_pmx_get_function_name,
	.get_function_groups = bd96811_pmx_get_function_groups,
	.set_mux = bd96811_pmx_set,
};

static const struct pinctrl_ops bd96811_pctlops = {
	.get_groups_count = bd96811_get_groups_count,
	.get_group_name = bd96811_get_group_name,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
};

static const struct pinctrl_desc bd96811_pdesc = {
	.name = "bd96811-pinctrl",
	.pins = &bd96811_pins[0],
	.npins = ARRAY_SIZE(bd96811_pins),
	.pmxops = &bd96811_pmxops,
	.pctlops = &bd96811_pctlops,
};

static int bd96811_pinctrl_probe(struct platform_device *pdev)
{
	int ret;
	struct bd96811_pinctrl *data;
	struct device *dev = &pdev->dev;
	struct pinctrl_dev *pcdev;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap)
		return -ENODEV;

	data->dev = dev;
	data->pdesc = bd96811_pdesc;

	ret = devm_pinctrl_register_and_init(dev->parent, &data->pdesc,
					     data, &pcdev);
	if (ret) {
		dev_err(dev, "pincontrol registration failed\n");
		return ret;
	}

	ret = pinctrl_enable(pcdev);
	if (ret) {
		dev_err(dev, "pincontrol enabling failed\n");
		return ret;
	}

	return ret;
}

static const struct platform_device_id bd96811_pinctrl_id[] = {
	{ "bd96811-pinctrl" },
	{ },
};
MODULE_DEVICE_TABLE(platform, bd96811_pinctrl_id);

static struct platform_driver bd96811_pinctrl = {
	.driver = {
		.name = "bd96811-pins",
	},
	.probe = bd96811_pinctrl_probe,
	.id_table = bd96811_pinctrl_id,
};
module_platform_driver(bd96811_pinctrl);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BUD96811 PMIC pincontrol driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd96811-pinctrl");
