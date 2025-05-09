// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>

#include "bd718xx_gpio.h"

#define BD718XX_GPIO_MODE_OUT 0x3
#define BD718XX_GPIO_MODE_IN 0x2
#define BD718XX_GPIO_MASK_PULLDOWN BIT(6)
#define BD718XX_GPIO_MASK_OUT_TYPE BIT(4)
#define BD718XX_GPIO_OUT_TYPE_OPEN_DRAIN 0
#define BD718XX_GPIO_OUT_TYPE_CMOS BD718XX_GPIO_MASK_OUT_TYPE

int bd718xx_gpio_get_value(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio)
{
	struct udevice *pmic = dev_get_parent(dev);
	u8 value;
	int err;

	if (gpio >= gc->num_gpios)
		return -EINVAL;

	/*
	 * The data sheet states the SRC reg is updated only when MODE is
	 * correctly set. Else, the bit will be fixed to '0'. So, this function
	 * works correctly only when GPIO direction is set correctly.
	 *
	 * We could add a check here to nag about wrong mode - but currently this
	 * is not done.
	 */
	err = pmic_reg_read(pmic, gc->reg_int_src);
	if (err < 0) {
		pr_err("failed to read GPIO signal out register: %d\n", err);
		return err;
	}

	value = err;

	return value & gc->gpiomask[gpio];
}


int bd718xx_gpio_set_value(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio, int level)
{
	struct udevice *pmic = dev_get_parent(dev);
	const char *l;
	u8 value;
	int err;

	if (gpio >= gc->num_gpios)
		return -EINVAL;

	err = pmic_reg_read(pmic, gc->reg_sig_out);
	if (err < 0) {
		pr_err("failed to read GPIO signal out register: %d\n", err);
		return err;
	}
	value = err;

	if (level == 0) {
		value &= ~gc->gpiomask[gpio];
		l = "low";
	} else {
		value |= gc->gpiomask[gpio];
		l = "high";
	}

	err = pmic_reg_write(pmic, gc->reg_sig_out, value);
	if (err) {
		pr_err("failed to set GPIO#%u %s: %d\n", gpio, l, err);
		return err;
	}

	return 0;
}

static int bd718xx_set_gpio_direction(const struct bd718xx_gpio *gc,
				      struct udevice *dev, unsigned int gpio,
				      int mode)
{
	struct udevice *pmic = dev_get_parent(dev);
	int err;

	if (gpio >= gc->num_gpios)
		return -EINVAL;

	err = pmic_clrsetbits(pmic, gc->gpio_conf_reg[gpio], gc->modemask[gpio], mode);
	if (err)
		pr_err("failed to configure GPIO#%u as output: %d\n", gpio,
		       err);

	return 0;
}

int bd718xx_gpio_direction_output(const struct bd718xx_gpio *gc,
				  struct udevice *dev, unsigned int gpio,
				  int value)
{
	struct udevice *pmic = dev_get_parent(dev);
	int err;

	if (gpio >= gc->num_gpios)
		return -EINVAL;

	/* TODO: Check the purpose of 'value' param */
	/* TODO: Handling of GPIO polarity? */
	err = bd718xx_gpio_set_value(gc, pmic, gpio, value);
	if (err < 0) {
		pr_err("failed to set GPIO#%u to %s: %d\n", gpio,
		       value ? "high" : "low", err);
		return err;
	}

	return bd718xx_set_gpio_direction(gc, dev, gpio, BD718XX_GPIO_MODE_OUT);
}

int bd718xx_gpio_direction_input(const struct bd718xx_gpio *gc,
				 struct udevice *dev, unsigned int gpio)
{
	return bd718xx_set_gpio_direction(gc, dev, gpio, BD718XX_GPIO_MODE_IN);
}

static int bd718xx_set_pulldown(const struct bd718xx_gpio *gc,
				struct udevice *dev, unsigned int gpio,
				int enable)
{
	struct udevice *pmic = dev_get_parent(dev);

	if (enable)
		return pmic_clrsetbits(pmic, gc->gpio_conf_reg[gpio], 0,
				       BD718XX_GPIO_MASK_PULLDOWN);
	return pmic_clrsetbits(pmic, gc->gpio_conf_reg[gpio],
			       BD718XX_GPIO_MASK_PULLDOWN,
			       BD718XX_GPIO_MASK_PULLDOWN);
}

static int bd718xx_set_out_type(const struct bd718xx_gpio *gc, struct udevice *dev,
				unsigned int gpio, int type)
{
	struct udevice *pmic = dev_get_parent(dev);

	return pmic_clrsetbits(pmic, gc->gpio_conf_reg[gpio],
			       BD718XX_GPIO_MASK_OUT_TYPE, type);
}

int bd718xx_gpio_set_flags(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio, ulong flags)
{
	int ret;

	if (gpio >= gc->num_gpios)
		return -EINVAL;

	if (flags & GPIOD_IS_OUT) {
		u32 value = !!(flags & GPIOD_IS_OUT_ACTIVE);

		if (flags & GPIOD_OPEN_DRAIN)
			ret = bd718xx_set_out_type(gc, dev, gpio,
					BD718XX_GPIO_OUT_TYPE_OPEN_DRAIN);
		else
			ret = bd718xx_set_out_type(gc, dev, gpio,
					BD718XX_GPIO_OUT_TYPE_CMOS);
		if (ret)
			return ret;

		return bd718xx_gpio_direction_output(gc, dev, gpio, value);
	}
	if (flags & GPIOD_IS_IN) {
		ret = bd718xx_set_pulldown(gc, dev, gpio,
					   flags & GPIOD_PULL_DOWN);
		if (ret)
			return ret;

		return bd718xx_gpio_direction_input(gc, dev, gpio);
	}

	return -EINVAL;
}




