// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <power/bd71892.h>

#include "bd718xx_gpio.h"

/*
 * We can potentially use GPIO1, INT0B, FLTB and EN for GPIO.
 */
enum {
	GPIO_GPIO1,
	GPIO_INT0B,
	GPIO_FLTB,
	GPIO_EN,

	NUM_GPIOS
};

static const u8 modemask[] = {
	BD71892_GPIO_MODE_MASK, BD71892_GPIO_INTB_MODE_MASK,
	BD71892_GPIO_MODE_MASK, BD71892_GPIO_MODE_MASK,
};

static const u8 gpiomask[] = { BIT(0), BIT(1), BIT(3), BIT(4) };
static const u8 gpio_conf_reg[] = { BD71892_REG_GPIO1_CFG, BD71892_REG_INTB_CFG,
				     BD71892_REG_FLTB_CFG, BD71892_REG_EN_CFG };

static const struct bd718xx_gpio bd71892_gpio =
{
	.reg_sig_out = BD71892_REG_GPIO_OUT,
	.reg_int_src = BD71892_REG_INT_SRC_5,
	.num_gpios = NUM_GPIOS,
	.gpiomask = &gpiomask[0],
	.gpio_conf_reg = &gpio_conf_reg[0],
	.modemask = &modemask[0],
};

static int bd71892_gpio_get_value(struct udevice *dev, unsigned int gpio)
{
	return bd718xx_gpio_get_value(&bd71892_gpio, dev, gpio);
}

static int bd71892_gpio_set_value(struct udevice *dev, unsigned int gpio,
				  int level)
{
	return bd718xx_gpio_set_value(&bd71892_gpio, dev, gpio, level);
}

static int bd71892_gpio_direction_output(struct udevice *dev, unsigned int gpio,
				 int value)
{
	return bd718xx_gpio_direction_output(&bd71892_gpio, dev, gpio, value);
}

static int bd71892_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	return bd718xx_gpio_direction_input(&bd71892_gpio, dev, gpio);
}

static int bd71892_gpio_set_flags(struct udevice *dev, unsigned int gpio,
				  ulong flags)
{
	return bd718xx_gpio_set_flags(&bd71892_gpio, dev, gpio, flags);
}

static const struct dm_gpio_ops gpio_bd71892_ops = {
	/* TODO: Implement set_flags */
	.direction_input	= bd71892_gpio_direction_input,
	.direction_output	= bd71892_gpio_direction_output,
	.set_value		= bd71892_gpio_set_value,
	.get_value		= bd71892_gpio_get_value,
	.set_flags		= bd71892_gpio_set_flags,
};

static int bd71892_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = NUM_GPIOS;
	uc_priv->bank_name = "bd71892_";

	return 0;
}

U_BOOT_DRIVER(gpio_bd71892) = {
	.name	= "gpio_bd71892",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_bd71892_ops,
	.probe	= bd71892_gpio_probe,
};
