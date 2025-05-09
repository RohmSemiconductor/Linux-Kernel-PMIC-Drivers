// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <compiler.h>

struct udevice;

struct bd718xx_gpio {
	u8 reg_sig_out;
	u8 reg_int_src;
	const u8 *gpio_conf_reg;
	const u8 *gpiomask;
	const u8 *modemask;
	unsigned int num_gpios;
};

int bd718xx_gpio_get_value(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio);
int bd718xx_gpio_set_value(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio, int level);
int bd718xx_gpio_direction_output(const struct bd718xx_gpio *gc,
				  struct udevice *dev, unsigned int gpio,
				  int value);
int bd718xx_gpio_direction_input(const struct bd718xx_gpio *gc,
				 struct udevice *dev, unsigned int gpio);
int bd718xx_gpio_set_flags(const struct bd718xx_gpio *gc, struct udevice *dev,
			   unsigned int gpio, ulong flags);

