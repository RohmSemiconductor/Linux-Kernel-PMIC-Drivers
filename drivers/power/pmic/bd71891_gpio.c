// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <power/bd71891.h>

#include "bd718xx_gpio.h"

/*
 * We can potentially use GPIO1, INT0B, FLTB and EN for GPIO.
 */
enum {
	GPIO_GPIO1,
	GPIO_INT0B,
	GPIO_INT1B,
	GPIO_FLTB,
	GPIO_EN,
	GPIO_RESETB,
	GPIO_WDGB,
	GPIO_GATEDRV,

	NUM_GPIOS
};

static const u8 gpiomask[] = {
	BIT(0), BIT(1), BIT(2), BIT(3), BIT(4), BIT(5), BIT(6), BIT(7)
};
static const u8 gpio_conf_reg[] = {
	BD71891_REG_GPIO1_CFG, BD71891_REG_INTB0_CFG, BD71891_REG_INTB1_CFG,
	BD71891_REG_FLTB_CFG, BD71891_REG_EN_CFG, BD71891_REG_RESETB_CFG,
	BD71891_REG_WDGB_CFG, BD71891_REG_GATEDRV_CFG
};

/*
 * The GPIO1, EN and GATEDRV have 3 bit wide mode conf while the rest have 2
 * bit modes
 */
static const u8 modemask[] = {
	BD71891_GPIO_MODE_MASK, BD71891_GPIO_INTB_MODE_MASK,
	BD71891_GPIO_INTB_MODE_MASK, BD71891_GPIO_INTB_MODE_MASK,
	BD71891_GPIO_MODE_MASK, BD71891_GPIO_INTB_MODE_MASK,
	BD71891_GPIO_INTB_MODE_MASK, BD71891_GPIO_MODE_MASK,
};

static const struct bd718xx_gpio bd71891_gpio =
{
	.reg_sig_out = BD71891_REG_GPIO_OUT,
	.reg_int_src = BD71891_REG_INT_SRC_5,
	.num_gpios = NUM_GPIOS,
	.gpiomask = &gpiomask[0],
	.gpio_conf_reg = &gpio_conf_reg[0],
	.modemask = &modemask[0],
};

static int bd71891_gpio_get_value(struct udevice *dev, unsigned int gpio)
{
	return bd718xx_gpio_get_value(&bd71891_gpio, dev, gpio);
}

static int bd71891_gpio_set_value(struct udevice *dev, unsigned int gpio,
				  int level)
{
	return bd718xx_gpio_set_value(&bd71891_gpio, dev, gpio, level);
}

static int bd71891_gpio_direction_output(struct udevice *dev, unsigned int gpio,
				 int value)
{
	return bd718xx_gpio_direction_output(&bd71891_gpio, dev, gpio, value);
}

static int bd71891_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	return bd718xx_gpio_direction_input(&bd71891_gpio, dev, gpio);
}

static int bd71891_gpio_set_flags(struct udevice *dev, unsigned int gpio,
				  ulong flags)
{
	return bd718xx_gpio_set_flags(&bd71891_gpio, dev, gpio, flags);
}

static const struct dm_gpio_ops gpio_bd71891_ops = {
	/* TODO: Implement set_flags */
	.direction_input	= bd71891_gpio_direction_input,
	.direction_output	= bd71891_gpio_direction_output,
	.set_value		= bd71891_gpio_set_value,
	.get_value		= bd71891_gpio_get_value,
	.set_flags		= bd71891_gpio_set_flags,
};

static int bd71891_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = NUM_GPIOS;
	uc_priv->bank_name = "bd71891_";

	return 0;
}

U_BOOT_DRIVER(gpio_bd71891) = {
	.name	= "gpio_bd71891",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_bd71891_ops,
	.probe	= bd71891_gpio_probe,
};
