// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <power/bd71892.h>

/*
 * TODO: Split to two files, one with the BD71892 stuff and other with the bits
 * which are usable also for the BD71891.
 */

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

static const int gpiomask[] = { BIT(0), BIT(1), BIT(3), BIT(4) };
static const int gpio_conf_reg[] = { BD71892_REG_GPIO1_CFG, BD71892_REG_INTB_CFG,
				     BD71892_REG_FLTB_CFG, BD71892_REG_EN_CFG };

static int bd71892_gpio_get_value(struct udevice *dev, unsigned int gpio)
{
	struct udevice *pmic = dev_get_parent(dev);
	u8 value;
	int err;

	if (gpio >= NUM_GPIOS)
		return -EINVAL;

	/*
	 * The data sheet states the SRC reg is updated only when MODE is
	 * correctly set. Else, the bit will be fixed to '0'. So, this function
	 * works correctly only when GPIO direction is set correctly.
	 *
	 * We could add a check here to nag about wrong mode - but currently this
	 * is not done.
	 */
	err = pmic_reg_read(pmic, BD71892_REG_INT_SRC_5);
	if (err < 0) {
		pr_err("failed to read GPIO signal out register: %d\n", err);
		return err;
	}

	value = err;

	return value & gpiomask[gpio];
}

static int bd71892_gpio_set_value(struct udevice *dev, unsigned int gpio,
				  int level)
{
	struct udevice *pmic = dev_get_parent(dev);
	const char *l;
	u8 value;
	int err;

	if (gpio >= NUM_GPIOS)
		return -EINVAL;

	err = pmic_reg_read(pmic, BD71892_REG_GPIO_OUT);
	if (err < 0) {
		pr_err("failed to read GPIO signal out register: %d\n", err);
		return err;
	}
	value = err;

	if (level == 0) {
		value &= ~gpiomask[gpio];
		l = "low";
	} else {
		value |= gpiomask[gpio];
		l = "high";
	}

	err = pmic_reg_write(pmic, BD71892_REG_GPIO_OUT, value);
	if (err) {
		pr_err("failed to set GPIO#%u %s: %d\n", gpio, l, err);
		return err;
	}

	return 0;
}

int bd71892_set_gpio_direction(struct udevice *dev, unsigned int gpio, int mode)
{
	struct udevice *pmic = dev_get_parent(dev);
	int err, mask;

	if (gpio >= NUM_GPIOS)
		return -EINVAL;

	if (gpio == GPIO_INT0B)
		mask = BD71892_GPIO_INTB_MODE_MASK;
	else
		mask = BD71892_GPIO_MODE_MASK;

	err = pmic_clrsetbits(pmic, gpio_conf_reg[gpio], mask, BD71892_GPIO_MODE_OUT);
	if (err)
		pr_err("failed to configure GPIO#%u as output: %d\n", gpio,
		       err);

	return 0;
}

int bd71892_gpio_direction_output(struct udevice *dev, unsigned int gpio,
				 int value)
{
	struct udevice *pmic = dev_get_parent(dev);
	int err;

	if (gpio >= NUM_GPIOS)
		return -EINVAL;

	/* TODO: Check the purpose of 'value' param */
	/* TODO: Handling of GPIO polarity? */
	err = bd71892_gpio_set_value(pmic, gpio, value);
	if (err < 0) {
		pr_err("failed to set GPIO#%u to %s: %d\n", gpio, value ? "high" : "low", err);
		return err;
	}

	return bd71892_set_gpio_direction(dev, gpio, BD71892_GPIO_MODE_OUT);
}

int bd71892_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	return bd71892_set_gpio_direction(dev, gpio, BD71892_GPIO_MODE_IN);
}

static int bd71892_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->gpio_count = NUM_GPIOS;
	uc_priv->bank_name = "bd71892_";

	return 0;
}

static int bd71892_set_pulldown(struct udevice *dev, unsigned int gpio,
				int enable)
{
	struct udevice *pmic = dev_get_parent(dev);

	if (enable)
		return pmic_clrsetbits(pmic, gpio_conf_reg[gpio], 0,
				       BD71892_GPIO_MASK_PULLDOWN);
	return pmic_clrsetbits(pmic, gpio_conf_reg[gpio],
			       BD71892_GPIO_MASK_PULLDOWN,
			       BD71892_GPIO_MASK_PULLDOWN);
}

static int bd71892_set_out_type(struct udevice *dev, unsigned int gpio,
				int type)
{
	struct udevice *pmic = dev_get_parent(dev);

	return pmic_clrsetbits(pmic, gpio_conf_reg[gpio],
			       BD71892_GPIO_MASK_OUT_TYPE, type);
}

static int bd71892_gpio_set_flags(struct udevice *dev, unsigned int gpio,
				  ulong flags)
{
	int ret;

	if (gpio >= NUM_GPIOS)
		return -EINVAL;

	if (flags & GPIOD_IS_OUT) {
		u32 value = !!(flags & GPIOD_IS_OUT_ACTIVE);

		if (flags & GPIOD_OPEN_DRAIN)
			ret = bd71892_set_out_type(dev, gpio, BD71892_GPIO_OUT_TYPE_OPEN_DRAIN);
		else
			ret = bd71892_set_out_type(dev, gpio, BD71892_GPIO_OUT_TYPE_CMOS);
		if (ret)
			return ret;

		return bd71892_gpio_direction_output(dev, gpio, value);
	}
	if (flags & GPIOD_IS_IN) {
		ret = bd71892_set_pulldown(dev, gpio, flags & GPIOD_PULL_DOWN);
		if (ret)
			return ret;

		return bd71892_gpio_direction_input(dev, gpio);
	}

	return -EINVAL;
}

static const struct dm_gpio_ops gpio_bd71892_ops = {
	/* TODO: Implement set_flags */
	.direction_input	= bd71892_gpio_direction_input,
	.direction_output	= bd71892_gpio_direction_output,
	.set_value		= bd71892_gpio_set_value,
	.get_value		= bd71892_gpio_get_value,
	.set_flags		= bd71892_gpio_set_flags,
};

U_BOOT_DRIVER(gpio_bd71892) = {
	.name	= "gpio_bd71892",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_bd71892_ops,
	.probe	= bd71892_gpio_probe,
};
