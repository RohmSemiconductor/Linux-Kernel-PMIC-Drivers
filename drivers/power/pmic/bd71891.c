// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright 2025 ROHM Semiconductors
 *  Matti Vaittinen <mazziesaccount@gmail.com>
 */

#include <common.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <log.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/bd71891.h>
#include "bdxxxx.h"

static const struct pmic_child_info pmic_children_info[] = {
	/* boost */
	{ .prefix = "bo", .driver = BD71891_BOOST_DRIVER},
	/* buck */
	{ .prefix = "bu", .driver = BD71891_BUCK_DRIVER},
	/* ldo */
	{ .prefix = "l", .driver = BD71891_LDO_DRIVER},
	{ },
};

enum {
	TYPE_VOLTAGE,
	TYPE_CURRENT,
	TYPE_POWER,
	TYPE_TEMPERATURE,
	#define TYPE_MAX TYPE_TEMPERATURE
};

static int bd71891_bind(struct udevice *dev)
{
	return bdxxxx_bind(dev, pmic_children_info);
}

static int bd71891_reg_count(struct udevice *dev)
{
	return BD71891_MAX_REGISTER - 1;
}

static inline int bd71891_i2c_is_locked(struct udevice *dev)
{
	uint8_t lockval;
	int ret;

	ret = bdxxxx_read(dev, BD71891_REG_I2C_LOCK, &lockval, 1);
	if (ret)
		return ret;

	return (lockval != BD71891_I2C_UNLOCKED);
}

static int bd71891_i2c_unlock(struct udevice *dev)
{
	int i;
	uint8_t writeval = 1;

	/*
	 * Data sheet says writing anything else except the
	 * BD71891_I2C_UNLOCK_VAL will lock the I2C. Writing the
	 * BD71891_I2C_UNLOCK_VAL 3 times is said to unlock the I2C. It is not
	 * specified what happens if the BD71891_I2C_UNLOCK_VAL is written when
	 * I2C has already been unlocked. Eg, if writing BD71891_I2C_UNLOCK_VAL
	 * 4 times locks the I2C again. Nor has it been said if the 3 writes
	 * should be consequent, or if there can be reads done in between.
	 * Hence, let's first write something else (hoping this will clear the
	 * 'unlock' sequence should it have been started already) - and then do
	 * 3 consequent BD71891_I2C_UNLOCK_VAL writes.
	 */
	bdxxxx_write(dev, BD71891_REG_I2C_LOCK, &writeval, 1);

	writeval = BD71891_I2C_UNLOCK_VAL;
	for (i = 0; i < 3; i++)
		bdxxxx_write(dev, BD71891_REG_I2C_LOCK, &writeval, 1);

	if (bd71891_i2c_is_locked(dev)) {
		printf("Failed to unlock\n");

		return -EIO;
	}

	return 0;
}

static int bd71891_probe(struct udevice *dev)
{
	int locked;

	debug("%s: '%s' probed\n", __func__, dev->name);

	locked = bd71891_i2c_is_locked(dev);
	if (locked < 0)
		return locked;

	if (locked)
		return bd71891_i2c_unlock(dev);

	return 0;
}

static struct dm_pmic_ops bd71891_ops = {
	.reg_count = bd71891_reg_count,
	.read = bdxxxx_read,
	.write = bdxxxx_write,
};

static const struct udevice_id bd71891_ids[] = {
	{ .compatible = "rohm,bd71891", },
	{ }
};

U_BOOT_DRIVER(pmic_bd71891) = {
	.name = "bd71891_pmic",
	.id = UCLASS_PMIC,
	.of_match = bd71891_ids,
	.bind = bd71891_bind,
	.probe = bd71891_probe,
	.ops = &bd71891_ops,
};


