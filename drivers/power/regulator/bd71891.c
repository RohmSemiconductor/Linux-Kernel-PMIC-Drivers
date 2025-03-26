// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 ROHM Semiconductors
 *
 * ROHM BD71891 regulator driver
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <linux/bitops.h>
#include <linux/stringify.h>
#include <power/bd71891.h>
#include <power/voltage_range.h>
#include <power/pmic.h>
#include <power/regulator.h>

/* 1 to force LPM, 0 to normal */
enum {
	BD71891_REGULATOR_MODE_NORMAL,
	BD71891_REGULATOR_MODE_LPM,
};
/* BOOST does not have LPM */
#define BD71891_BUCK_MODE_MASK		BIT(1)
#define BD71891_LDO_MODE_MASK		BIT(4)

static struct dm_regulator_mode bd71891_ldo_modes[] = {
        { .id = BD71891_REGULATOR_MODE_NORMAL,
                .register_value = 0, .name = "NORMAL" },
        { .id = BD71891_REGULATOR_MODE_LPM,
                .register_value = BD71891_LDO_MODE_MASK, .name = "LOWPOWER" },
};

static struct dm_regulator_mode bd71891_buck_modes[] = {
        { .id = BD71891_REGULATOR_MODE_NORMAL,
                .register_value = 0, .name = "NORMAL" },
        { .id = BD71891_REGULATOR_MODE_LPM,
                .register_value = BD71891_BUCK_MODE_MASK, .name = "LOWPOWER" },
};

enum {
	BD71891_VAL_TYPE_RUNVOLT,
};

struct bd71891_plat {
	const char				*name;
	const struct regulator_picked_range	*pranges;
	unsigned int				num_pranges;
	int					id;
	int					pickreg;
	int					vsel_reg;
	int					en_reg;
	u8					sel_mask;
	bool					dvs;
	/*
	struct regulator_vrange	*ranges;
	unsigned int		numranges;
	u8			sel_mask;
	int			en_reg;
	int			vsel_reg;
	int			mode_reg;
	int			mode_mask;
	*/
};

#define BD_DATA(_id, _range) \
{ \
	.name = __stringify(_id),			\
	.pranges = (_range),				\
	.num_pranges = ARRAY_SIZE(_range),		\
	.id = (_id),					\
	.en_reg = BD71891_REG_##_id##_ON,		\
	.vsel_reg = BD71891_REG_##_id##_ON + 3,		\
	.sel_mask = (_id) == BOOST ? 0x3f : 0xff,	\
	/* TODO: Check from HQ, spec is unclear */	\
	.pickreg = BD71891_REG_##_id##_ON + 2,		\
	.dvs = (_id) < BOOST,				\
}

enum {
	BUCK1 = 0,
	BUCK2,
	BUCK3,
	BUCK4,
	BOOST,
	LDO1,
	LDO2,
	BD71891_REGULATOR_AMOUNT
};

struct regulator_picked_range {
	const struct regulator_vrange *ranges;
	unsigned int num_ranges;
	u8 pick_reg_val;
	u8 pick_reg_mask;
};

static const struct regulator_vrange buck124_vranges_0[] = {
	BD_RANGE(500000, 5000, 0, 0xa0),
	BD_RANGE(1300000, 0, 0xa1, 0xff),
};

static const struct regulator_vrange buck12_vranges_1[] = {
	BD_RANGE(1300000, 10000, 0, 0x46),
	BD_RANGE(2000000, 0, 0x47, 0xff),
};

static const struct regulator_vrange buck4_vranges_1[] = {
	BD_RANGE(1300000, 10000, 0, 0xc8),
	BD_RANGE(3000000, 0, 0xc9, 0xff),
};

static const struct regulator_vrange buck3_vranges_0[] = {
	BD_RANGE(500000, 5000, 0, 0xa0),
	BD_RANGE(1300000, 0, 0xa1, 0xff),
};

static const struct regulator_vrange buck3_vranges_1[] = {
	BD_RANGE(1300000, 10000, 0, 0x46),
	BD_RANGE(2000000, 0, 0x47, 0xff),
};

/* BUCK3 SW FREQ must be set to 0  (1.5MHz) */
static const struct regulator_vrange buck3_vranges_23[] = {
	BD_RANGE(300000, 5000, 0, 0x8c),
	BD_RANGE(1000000, 0, 0x8d, 0xff),
};

static const struct regulator_vrange boost_vranges[] = {
	BD_RANGE(4800000, 10000, 0, 0x2d),
	BD_RANGE(5250000, 0, 0x2e, 0x3f),
};

static const struct regulator_vrange ldo_vranges[] = {
	BD_RANGE(500000, 5000, 0x0, 0x9f),
	BD_RANGE(1300000, 10000, 0xa0, 0xd2),
	BD_RANGE(1800000, 0, 0xd3, 0xff),
};

static const struct regulator_picked_range buck12_ranges[] = {
	{
		.ranges = buck124_vranges_0,
		.num_ranges = ARRAY_SIZE(buck124_vranges_0),
		.pick_reg_val = 0,
		.pick_reg_mask = BIT(0),
	}, {
		.ranges = buck12_vranges_1,
		.num_ranges = ARRAY_SIZE(buck12_vranges_1),
		.pick_reg_val = 1,
		.pick_reg_mask = BIT(0),
	},
};

static const struct regulator_picked_range buck3_ranges[] = {
	{
		.ranges = buck3_vranges_0,
		.num_ranges = ARRAY_SIZE(buck3_vranges_0),
		.pick_reg_val = 0,
		.pick_reg_mask = BIT(0),
	}, {
		.ranges = buck3_vranges_1,
		.num_ranges = ARRAY_SIZE(buck3_vranges_1),
		.pick_reg_val = 1,
		.pick_reg_mask = BIT(0),
	}, {
		.ranges = buck3_vranges_23,
		.num_ranges = ARRAY_SIZE(buck3_vranges_23),
		.pick_reg_val = 2,
		.pick_reg_mask = BIT(0),
	}, {
		.ranges = buck3_vranges_23,
		.num_ranges = ARRAY_SIZE(buck3_vranges_23),
		.pick_reg_val = 3,
		.pick_reg_mask = BIT(0),
	},
};

static const struct regulator_picked_range buck4_ranges[] = {
	{
		.ranges = buck124_vranges_0,
		.num_ranges = ARRAY_SIZE(buck124_vranges_0),
		.pick_reg_val = 0,
		.pick_reg_mask = BIT(0),
	}, {
		.ranges = buck4_vranges_1,
		.num_ranges = ARRAY_SIZE(buck4_vranges_1),
		.pick_reg_val = 1,
		.pick_reg_mask = BIT(0),
	},
};

static const struct regulator_picked_range boost_ranges[] = {
	{
		.ranges = boost_vranges,
		.num_ranges = ARRAY_SIZE(boost_vranges),
	},
};
static const struct regulator_picked_range ldo_ranges[] = {
	{
		.ranges = ldo_vranges,
		.num_ranges = ARRAY_SIZE(ldo_vranges),
	},
};

static struct bd71891_plat bd71891_reg_data[] = {
	BD_DATA(BUCK1, buck12_ranges),
	BD_DATA(BUCK2, buck12_ranges),
	BD_DATA(BUCK3, buck3_ranges),
	BD_DATA(BUCK4, buck4_ranges),
	BD_DATA(BOOST, boost_ranges),
	BD_DATA(LDO1, ldo_ranges),
	BD_DATA(LDO2, ldo_ranges),
};

static int __bd71891_get_enable(struct udevice *dev, int mask)
{
	struct bd71891_plat *plat = dev_get_plat(dev);
	int val;

	val = pmic_reg_read(dev->parent, plat->en_reg);
	if (val < 0)
		return val;

	return !!(val & mask);
}

static int bd71891_get_enable(struct udevice *dev)
{
	return __bd71891_get_enable(dev, BD71891_MASK_RUN_ON);
}

static int bd71891_get_idle_enable(struct udevice *dev) __attribute__((unused));
static int bd71891_get_idle_enable(struct udevice *dev)
{
	return __bd71891_get_enable(dev, BD71891_MASK_IDLE_ON);
}

static int __bd71891_write_sels(struct udevice *dev, struct bd71891_plat *plat,
				int pickreg, unsigned int sel)
{
	int ret;

	ret = pmic_reg_write(dev->parent, plat->vsel_reg, sel);
	if (ret)
		return ret;

	if (plat->id >= BOOST)
		return 0;

	return pmic_reg_write(dev->parent, plat->pickreg, pickreg);
}

static int __bd71891_set_volt_value(struct udevice *dev, int uvolt, int flag)
{
	struct bd71891_plat *plat = dev_get_plat(dev);
	int pickreg, pick_idx, range_idx, ret;
	unsigned int sel;

	/* LDOs (and the BOOST?) don't support changing voltage while enabled */
	if (flag == BD71891_VAL_TYPE_RUNVOLT && !plat->dvs)
		if (bd71891_get_enable(dev))
			return -EBUSY;

	/*
	 * Boost and LDOs do'nt have multiple VSEL renges selectible by a
	 * RANGE_SEL
	 */
	if (plat->id < BOOST) {
		/* We don't want to change a range if this is not required.
		 * Hence, read currently used RANGE selector and scan it for
		 * suitable voltage.
		 */
		ret = pmic_reg_read(dev->parent, plat->pickreg);
		if (ret < 0) {
			printf("%s: Failed to read pick reg 0x%02x, ret %d\n",
			       plat->name, plat->pickreg, ret);

			return ret;
		}
		pickreg = ret;

		for (pick_idx = 0; pick_idx < plat->num_pranges; pick_idx++) {
			const struct regulator_picked_range *p;

			p = &plat->pranges[pick_idx];

			if (!((p->pick_reg_val & p->pick_reg_mask) ==
			     (pickreg & p->pick_reg_mask)))
				continue;

			for (range_idx = 0; range_idx < p->num_ranges; range_idx++) {
				const struct regulator_vrange *r = &p->ranges[range_idx];

				/*
				 * If we can support new voltage with current
				 * range, just write it and be done.
				 *
				 * TODO: Check from the HW team when new voltage
				 * is applied. With 'pickable voltage ranges',
				 * setting a new range is racy because writing
				 * both the range selector and vsel is not
				 * atomic. There has been different approaches
				 * to mitigate this issue, one of which is that
				 * a voltage change is not done until range
				 * selection bit is written. If this was used,
				 * then we should always write the range
				 * selection bit, no matter whether the range
				 * stays same or not (and not directly return
				 * after writing the sel here. That'd made this
				 * 'prefer current range' code obsolete too?).
				 */
				ret = vrange_find_selector(r, uvolt, &sel);
				if (!ret)
					return pmic_reg_write(dev->parent,
							      plat->vsel_reg, sel);
			}
		}
	}

	for (pick_idx = 0; pick_idx < plat->num_pranges; pick_idx++) {
		const struct regulator_picked_range *p = &plat->pranges[pick_idx];

		for (range_idx = 0; range_idx < p->num_ranges; range_idx++) {
			const struct regulator_vrange *r = &p->ranges[range_idx];

			ret = vrange_find_selector(r, uvolt, &sel);
			if (!ret) {
				/*
				 * NOTE: There is no protection for this RMW -
				 * cycle. We expect serializing of the voltage
				 * settings being done by the system or by the
				 * calling code.
				 */
				pickreg &= ~(p->pick_reg_mask);
				pickreg |= (p->pick_reg_val & p->pick_reg_mask);

				return __bd71891_write_sels(dev, plat, pickreg,
							    sel);
			}
		}
	}

	printf("%s: Unsupported voltage %u\n", plat->name, uvolt);
	return -EINVAL;
}

static int bd71891_set_volt_value(struct udevice *dev, int uvolt)
{
	return __bd71891_set_volt_value(dev, uvolt, BD71891_VAL_TYPE_RUNVOLT);
}

static int __bd71891_get_value(struct udevice *dev, struct bd71891_plat *plat,
			       const struct regulator_picked_range *p)
{
	unsigned int i, tmp;
	int vsel;

	vsel = pmic_reg_read(dev->parent, plat->vsel_reg);
	if (vsel < 0)
		return vsel;

	vsel &= plat->sel_mask;
	if (!(plat->sel_mask & 1)) {
		/*
		 * Gah. I don't remember if we have ffs in builtins or in uBoot
		 * library. AFAIR, all voltage selectors on BD718xx start from
		 * BIT(0). TODO: Checkthis
		 */
		printf ("TODO: sift value using ffs(mask).\n");
		return -ENOENT;
	}

	for (i = 0; i < p->num_ranges; i++)
		if (!vrange_find_value(&p->ranges[i], vsel, &tmp))
			return tmp;

	printf("Unknown voltage value\n");

	return -EINVAL;
}

static int bd71891_get_buck_volt_value(struct udevice *dev)
{
	struct bd71891_plat *plat = dev_get_plat(dev);
	//unsigned int tmp, sel;
	int pick_val = 0;
	int i;

	if (!plat->num_pranges)
		return -ENODEV;

	pick_val = pmic_reg_read(dev->parent, plat->pickreg);
	if (pick_val < 0)
		return pick_val;
	/*
	 * TODO: Move pick_reg_mask to plat as it is common for all
	 * ranges.
	 */
	for (i = 0; i < plat->num_pranges; i++) {
		const struct regulator_picked_range *p = &plat->pranges[i];

		if (p->pick_reg_val == (pick_val & p->pick_reg_mask))
			return __bd71891_get_value(dev, plat, p);
	}

	return -EINVAL;
}

#define bd71891_get_ldo_volt_value bd71891_get_boost_volt_value
static int bd71891_get_boost_volt_value(struct udevice *dev)
{
	struct bd71891_plat *plat = dev_get_plat(dev);

	if (!plat->num_pranges)
		return -ENODEV;

	return __bd71891_get_value(dev, plat, &plat->pranges[0]);
}


static int __bd71891_set_enable(struct udevice *dev, bool enable, int mask)
{
	struct bd71891_plat *plat = dev_get_plat(dev);
	int val;

	if (enable)
		val = mask;
	else
		val = 0;

	return pmic_clrsetbits(dev->parent, plat->en_reg, mask, val);
}

static int bd71891_set_enable(struct udevice *dev, bool enable)
{
	return __bd71891_set_enable(dev, enable, BD71891_MASK_RUN_ON);
}



static const struct dm_regulator_ops bd71891_buck_ops = {
	.get_value		= bd71891_get_buck_volt_value,
	.set_value		= bd71891_set_volt_value,
/*	.get_suspend_value	= bd71885_get_suspend_value,
	.set_suspend_value	= bd71885_set_suspend_value, */
	.get_enable		= bd71891_get_enable,
	.set_enable		= bd71891_set_enable,
/*	.get_suspend_enable	= bd71885_get_suspend_enable,
	.set_suspend_enable	= bd71885_set_suspend_enable,
	.get_mode		= bd71885_get_mode,
	.set_mode		= buck_set_mode, */
};

/*
 * TODO: Split the set value to buck and boost/LDO specific functions
 * to simplify the flow
 */
static const struct dm_regulator_ops bd71891_ldo_ops = {
	.get_value		= bd71891_get_ldo_volt_value,
	.set_value		= bd71891_set_volt_value,
	.get_enable		= bd71891_get_enable,
	.set_enable		= bd71891_set_enable,
/*	.get_suspend_enable	= bd71885_get_suspend_enable,
	.set_suspend_enable	= bd71885_set_suspend_enable,
	.get_mode		= bd71885_get_mode,
	.set_mode		= ldo_set_mode, */
};

static const struct dm_regulator_ops bd71891_boost_ops = {
	.get_value		= bd71891_get_boost_volt_value,
	.set_value		= bd71891_set_volt_value,
/*	.get_suspend_value	= bd71885_get_suspend_value,
	.set_suspend_value	= bd71885_set_suspend_value, */
	.get_enable		= bd71891_get_enable,
	.set_enable		= bd71891_set_enable,
/*	.get_suspend_enable	= bd71885_get_suspend_enable,
	.set_suspend_enable	= bd71885_set_suspend_enable,
	.get_mode		= bd71885_get_mode,
	.set_mode		= buck_set_mode, */
};

static int bd71891_regulator_probe(struct udevice *dev)
{
	struct bd71891_plat *plat = dev_get_plat(dev);
	struct dm_regulator_uclass_plat *uc_pdata;
	int data_amnt = BD71891_REGULATOR_AMOUNT;
	int i, vendor, prod_id;
	struct udevice *parent;

	parent = dev_get_parent(dev);
	if (!parent) {
		printf("No parent\n");
		return -ENODEV;
	}

	prod_id = pmic_reg_read(parent, BD71891_REG_PROD_ID);
	if (prod_id < 0)
		return prod_id;

	vendor = pmic_reg_read(parent, BD71891_REG_VENDOR);
	if (vendor < 0)
		return vendor;

	if (prod_id != BD71891_PROD_ID_VAL || vendor != BD71891_VENDOR_VAL)
		printf("Unknown product/vendor\n");

	for (i = 0; i < data_amnt; i++) {
		if (!strcmp(dev->name, bd71891_reg_data[i].name)) {
			printf("Probed '%s'\n", dev->name);
			*plat = bd71891_reg_data[i];

			uc_pdata = dev_get_uclass_plat(dev);
			if (bd71891_reg_data[i].id < BOOST) {
				uc_pdata->mode = &bd71891_buck_modes[0];
				uc_pdata->mode_count = ARRAY_SIZE(bd71891_buck_modes);
				printf("Set mode ptr for buck id %d\n", bd71891_reg_data[i].id);
			} else if (bd71891_reg_data[i].id > BOOST) {
				uc_pdata->mode = &bd71891_ldo_modes[0];
				uc_pdata->mode_count = ARRAY_SIZE(bd71891_ldo_modes);
				printf("Set mode ptr for ldo id %d\n", bd71891_reg_data[i].id);
			}
			return bd71891_set_enable(dev, !!(uc_pdata->boot_on ||
						  uc_pdata->always_on));
		}
	}

	printf("Unknown regulator '%s'\n", dev->name);

	return -ENOENT;
}

U_BOOT_DRIVER(bd71891_buck) = {
	.name = BD71891_BUCK_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &bd71891_buck_ops,
	.probe = bd71891_regulator_probe,
	.plat_auto = sizeof(struct bd71891_plat),
};

U_BOOT_DRIVER(bd71891_ldo) = {
	.name = BD71891_LDO_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &bd71891_ldo_ops,
	.probe = bd71891_regulator_probe,
	.plat_auto = sizeof(struct bd71891_plat),
};

U_BOOT_DRIVER(bd71891_boost) = {
	.name = BD71891_BOOST_DRIVER,
	.id = UCLASS_REGULATOR,
	.ops = &bd71891_boost_ops,
	.probe = bd71891_regulator_probe,
	.plat_auto = sizeof(struct bd71891_plat),
};

