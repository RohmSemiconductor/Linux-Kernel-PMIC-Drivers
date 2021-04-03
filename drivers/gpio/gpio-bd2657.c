// SPDX-License-Identifier: GPL-2.0
/*
 * Support GPIOs on ROHM BD2657
 *
 * BD2657 has two GPIOs.
 * GPIO_0:
 * - can be controlled by SW (GPIO) - or set to be toggled by HW
 *   according to the PMIC power-state.
 * GPIO_1:
 * - PMIC_EN enable pin can be changed to GPO by PMIC OTP option. The correct
 *   setting for the PMIC installed on board must be configured from fwnode.
 *
 * Copyright 2021 ROHM Semiconductors.
 * Author: Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>
 */

#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/rohm-bd2657.h>

struct bd2657_gpio {
 	/* chip.parent points the MFD which provides DT node and regmap */
	struct gpio_chip chip;
	/* dev points to the platform device for devm and prints */
	struct device *dev;
	struct regmap *regmap;
};

static int bd2657gpo_get(struct gpio_chip *chip, unsigned int offset)
{
	struct bd2657_gpio *bd2657 = gpiochip_get_data(chip);
	int reg[] = {BD2657_REG_GPIO0_OUT, BD2657_REG_GPIO1_OUT};
	int ret, val;

	if (offset >= ARRAY_SIZE(reg))
		return -EINVAL;

	ret = regmap_read(bd2657->regmap, reg[offset], &val);
	if (ret)
		return ret;

	return (val) & BD2657_GPIO_OUT_MASK;
}

static void bd2657gpo_set(struct gpio_chip *chip, unsigned int offset,
			   int value)
{
	struct bd2657_gpio *bd2657 = gpiochip_get_data(chip);
	int reg[] = {BD2657_REG_GPIO0_OUT, BD2657_REG_GPIO1_OUT};
	int ret;

	if (offset >= ARRAY_SIZE(reg))
		return;

	if (value)
		ret = regmap_set_bits(bd2657->regmap, reg[offset],
				      BD2657_GPIO_OUT_MASK);
	else
		ret = regmap_clear_bits(bd2657->regmap, reg[offset],
					BD2657_GPIO_OUT_MASK);

	if (ret)
		dev_warn(bd2657->dev, "failed to toggle GPO\n");
}

static int bd2657_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
				   unsigned long config)
{
	struct bd2657_gpio *bdgpio = gpiochip_get_data(chip);
	int reg[] = {BD2657_REG_GPIO0_OUT, BD2657_REG_GPIO1_OUT};

	if (offset >= ARRAY_SIZE(reg))
		return -EINVAL;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		return regmap_update_bits(bdgpio->regmap,
					  reg[offset],
					  BD2657_GPIO_DRIVE_MASK,
					  BD2657_GPIO_OPEN_DRAIN);
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return regmap_update_bits(bdgpio->regmap,
					  reg[offset],
					  BD2657_GPIO_DRIVE_MASK,
					  BD2657_GPIO_PUSH_PULL);
	default:
		break;
	}
	return -ENOTSUPP;
}

/*
 * BD2657 GPIO is actually GPO
 *
 * There is some unofficial way of using the GPIO0 for input - but this is not
 * properly documented. Let's only support the GPO for now.
 */
static int bd2657gpo_direction_get(struct gpio_chip *gc, unsigned int offset)
{
	return GPIO_LINE_DIRECTION_OUT;
}

/* Template for GPIO chip */
static const struct gpio_chip bd2657gpo_chip = {
	.label			= "bd2657",
	.owner			= THIS_MODULE,
	.get			= bd2657gpo_get,
	.get_direction		= bd2657gpo_direction_get,
	.set			= bd2657gpo_set,
	.set_config		= bd2657_gpio_set_config,
	.can_sleep		= true,
};

#define BD2657_TWO_GPIOS	GENMASK(1,0)
#define BD2657_ONE_GPIO		BIT(1)

static int bd2657_init_valid_mask(struct gpio_chip *gc,
				  unsigned long *valid_mask,
				  unsigned int ngpios)
{
	pr_info("valid_mask init, ngpios %d, mask 0x%lx\n", ngpios, *valid_mask);
	if (device_property_present(gc->parent, "rohm,output-power-state-gpio"))
		*valid_mask &= ~BIT(0);

	pr_info("valid_mask init, returning  mask 0x%lx\n", *valid_mask);

	return 0;
}

static int gpo_bd2657_probe(struct platform_device *pdev)
{
	struct bd2657_gpio *g;
	struct device *parent, *dev;

	/*
	 * Bind devm lifetime to this platform device => use dev for devm.
	 * also the prints should originate from this device.
	 */
	dev = &pdev->dev;
	/* The device-tree and regmap come from MFD => use parent for that */
	parent = dev->parent;

	g = devm_kzalloc(dev, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;

	g->chip = bd2657gpo_chip;
	g->chip.ngpio = 2;
	g->chip.base = -1;
	g->chip.init_valid_mask = bd2657_init_valid_mask;
	g->chip.parent = parent;
	g->regmap = dev_get_regmap(parent, NULL);
	g->dev = dev;

	return devm_gpiochip_add_data(dev, &g->chip, g);
}

static struct platform_driver gpo_bd2657_driver = {
	.driver = {
		.name	= "bd2657-gpo",
	},
	.probe		= gpo_bd2657_probe,
};
module_platform_driver(gpo_bd2657_driver);

MODULE_ALIAS("platform:bd2657-gpo");
MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("GPO interface for BD2657");
MODULE_LICENSE("GPL");
