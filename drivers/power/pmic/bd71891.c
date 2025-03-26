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

static int bd71891_probe(struct udevice *dev)
{
	debug("%s: '%s' probed\n", __func__, dev->name);

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


