// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2019 ROHM Semiconductors
// bd71828-regulator.c ROHM BD71828GW-DS1 regulator driver
//

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/mfd/rohm-bd72720.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

/* Drivers should not do this. But we provide this custom kernel interface
 * for users to switch the run-level. Hence we need to get the rdev from
 * struct regulator
 */
#include "internal.h"

#define DVS_RUN_LEVELS 4

#define BD72720_MASK_LDON_HEAD GENMASK(2,0)

struct reg_init {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

struct run_lvl_ctrl {
	unsigned int voltage;
	bool enabled;
};

/*
 * This is sub-optimal as it wastes memory. We should not dublicate the data
 * which is global for all regulators (like the PMIC device pointer, regmap and
 * GPIO descs). Instead we should have the regulator specific data array
 * contained in PMIC specific struct, and then have a macro to get the pointer
 * to this containing struct from the array member, based on the regulator ID.
 *
 * Furthermore, most of the regulators don't support the sub-run states, so we
 * should encapsulate all sub-run state specific stuff in a struct, define the
 * necessary const structs for those regulators which do support sub-run states
 * and put only one pointer / regulator in the regulator data array. Also, the
 * 'allow_runlvl' could be replaced by checking if the pointer is NULL or not.
 *
 * Well, let's see if I find the time to do this ... better a bit wasteful than
 * half done implementation, right?
 */
struct bd71828_regulator_data {
	struct device *dev;
	struct regulator_desc desc;
	const struct rohm_dvs_config dvs;
	int sub_run_mode_reg;
	int sub_run_mode_mask;
	struct run_lvl_ctrl run_lvl[DVS_RUN_LEVELS];
	struct mutex dvs_lock;
	struct gpio_descs *gps;
	struct regmap *regmap;
	int (*get_run_level_i2c)(struct bd71828_regulator_data *rd);
	int (*get_run_level_gpio)(struct bd71828_regulator_data *rd);
	int (*set_run_level_i2c)(struct bd71828_regulator_data *rd, int val);
	int (*set_run_level_gpio)(struct bd71828_regulator_data *rd, int val);
	int (*of_set_runlvl_levels)(struct device_node *np,
		const struct regulator_desc *desc, struct regulator_config *cfg);
	bool allow_runlvl;
};

/*
 * BD71828 Buck voltages
 */
static const struct linear_range bd71828_buck1267_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0xef, 6250),
	REGULATOR_LINEAR_RANGE(2000000, 0xf0, 0xff, 0),
};

static const struct linear_range bd71828_buck3_volts[] = {
	REGULATOR_LINEAR_RANGE(1200000, 0x00, 0x0f, 50000),
	REGULATOR_LINEAR_RANGE(2000000, 0x10, 0x1f, 0),
};

static const struct linear_range bd71828_buck4_volts[] = {
	REGULATOR_LINEAR_RANGE(1000000, 0x00, 0x1f, 25000),
	REGULATOR_LINEAR_RANGE(1800000, 0x20, 0x3f, 0),
};

static const struct linear_range bd71828_buck5_volts[] = {
	REGULATOR_LINEAR_RANGE(2500000, 0x00, 0x0f, 50000),
	REGULATOR_LINEAR_RANGE(3300000, 0x10, 0x1f, 0),
};

/* BD71828 LDO voltages */
static const struct linear_range bd71828_ldo_volts[] = {
	REGULATOR_LINEAR_RANGE(800000, 0x00, 0x31, 50000),
	REGULATOR_LINEAR_RANGE(3300000, 0x32, 0x3f, 0),
};

/*
 * BD72720 Buck voltages
 */
static const struct linear_range bd72720_buck1234_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0xc0, 6250),
	REGULATOR_LINEAR_RANGE(1700000, 0xc1, 0xff, 0),
};

static const struct linear_range bd72720_buck589_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0x78, 10000),
	REGULATOR_LINEAR_RANGE(1700000, 0x79, 0xff, 0),
};

static const struct linear_range bd72720_buck67_volts[] = {
	REGULATOR_LINEAR_RANGE(1500000, 0x00, 0xb4, 10000),
	REGULATOR_LINEAR_RANGE(3300000, 0xb5, 0xff, 0),
};

/*
 * The BUCK10 on BD72720 has two modes of operation, depending on a LDON_HEAD
 * setting. When LDON_HEAD is 0x0, the behaviour is as with other bucks, eg.
 * voltage can be set to a values indicated below using the VSEL register.
 *
 * However, when LDON_HEAD is set to 0x1 ... 0x7, BUCK 10 voltage is, according
 * to the data-sheet, "automatically adjusted following LDON_HEAD setting and
 * clamped to BUCK10_VID setting".
 *
 * Again, reading the data-sheet shows a "typical connection" where the BUCK10
 * is used to supply the LDOs 1-4. My assumption is that in practice, this
 * means that the BUCK10 voltage will be adjusted based on the maximum output
 * of the LDO 1-4 (to minimize power loss). This makes sense.
 *
 * Auto-adjusting regulators aren't something I really like to model in the
 * driver though - and, if the auto-adjustment works as intended, then there
 * should really be no need to software to care about the buck10 voltages.
 * If enable/disable control is still needed, we can implement buck10 as a
 * regulator with only the enable/disable ops - and device-tree can be used
 * to model the supply-relations. I believe this could allow the regulator
 * framework to automagically disable the BUCK10 if all LDOs that are being
 * supplied by it are disabled.
 */
static const struct linear_range bd72720_buck10_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0xc0, 6250),
	REGULATOR_LINEAR_RANGE(1700000, 0xc1, 0xff, 0),
};
/* BD71828 LDO voltages */

static const struct linear_range bd72720_ldo1234_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0x50, 6250),
	REGULATOR_LINEAR_RANGE(1000000, 0x51, 0x7f, 0),
};

static const struct linear_range bd72720_ldo57891011_volts[] = {
	REGULATOR_LINEAR_RANGE(750000, 0x00, 0xff, 10000),
};

static const struct linear_range bd72720_ldo6_volts[] = {
	REGULATOR_LINEAR_RANGE(600000, 0x00, 0x78, 10000),
	REGULATOR_LINEAR_RANGE(1800000, 0x79, 0x7f, 0),
};

static const unsigned int bd71828_ramp_delay[] = { 2500, 5000, 10000, 20000 };

/*
 * TODO: BD72720 supports setting both the ramp-up and ramp-down values
 * separately. Do we need to support ramp-down setting?
 */
static const unsigned int bd72720_ramp_delay[] = { 5000, 7500, 10000, 12500 };

static int buck_set_hw_dvs_levels(struct device_node *np,
				  const struct regulator_desc *desc,
				  struct regulator_config *cfg)
{
	struct bd71828_regulator_data *data;

	data = container_of(desc, struct bd71828_regulator_data, desc);

	return rohm_regulator_set_dvs_levels(&data->dvs, np, desc, cfg->regmap);
}

static int set_runlevel_voltage(struct regmap *regmap,
				const struct regulator_desc *desc,
				unsigned int uv, unsigned int level)
{
	int i, ret = -EINVAL;
	/*
	 * On bot the BD71828 and BD7220 the RUN level registers are right after the
	 * vsel_reg, and the voltage values (and masks) are same as with normal vsel.
	 * RUN0 reg is next, then is the RUN 1 reg and so on...
	 */
	u8 reg = desc->vsel_reg + level + 1;
	u8 mask = desc->vsel_mask;

	for (i = 0; i < desc->n_voltages; i++) {
		ret = regulator_desc_list_voltage_linear_range(desc, i);
		if (ret < 0)
			continue;
		if (ret == uv) {
			i <<= ffs(desc->vsel_mask) - 1;
			ret = regmap_update_bits(regmap, reg, mask, i);
			break;
		}
	}
	return ret;
}

static int __set_runlvl_hw_dvs_levels(struct device_node *np, const struct regulator_desc *desc, int en_reg, int *en_masks)
{
	int i, ret;
	uint32_t uv;
	struct bd71828_regulator_data *data;
	const char *props[DVS_RUN_LEVELS] = { "rohm,dvs-runlevel0-voltage",
					      "rohm,dvs-runlevel1-voltage",
					      "rohm,dvs-runlevel2-voltage",
					      "rohm,dvs-runlevel3-voltage" };

	data = container_of(desc, struct bd71828_regulator_data, desc);

	mutex_lock(&data->dvs_lock);
	for (i = 0; i < DVS_RUN_LEVELS; i++) {
		ret = of_property_read_u32(np, props[i], &uv);
		if (ret) {
			if (ret != -EINVAL)
				goto unlock_out;
			uv = 0;
		}
		if (uv) {
			data->run_lvl[i].voltage = uv;
			data->run_lvl[i].enabled = true;

			ret = set_runlevel_voltage(data->regmap, desc, uv, i);

			if (ret)
				goto unlock_out;

			ret = regmap_set_bits(data->regmap, en_reg,
						 en_masks[i]);
		} else {
			ret = regmap_clear_bits(data->regmap, en_reg,
						 en_masks[i]);
		}
		if (ret)
			goto unlock_out;
	}

	ret = rohm_regulator_set_dvs_levels(&data->dvs, np, desc, data->regmap);

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

#define BD72720_MASK_RUN0_EN BIT(4)
#define BD72720_MASK_RUN1_EN BIT(5)
#define BD72720_MASK_RUN2_EN BIT(6)
#define BD72720_MASK_RUN3_EN BIT(7)

/* TODO: Make this work on BD72720 or add own function for it */
static int bd72720_set_runlvl_hw_dvs_levels(struct device_node *np,
					    const struct regulator_desc *desc,
					    struct regulator_config *cfg)
{
	/* On BD72720 the RUN[0...3] level enable is in same reg as the normal enable */
	int en_masks[DVS_RUN_LEVELS] = { BD72720_MASK_RUN0_EN,
					 BD72720_MASK_RUN1_EN,
					 BD72720_MASK_RUN2_EN,
					 BD72720_MASK_RUN3_EN };
	int en_reg = desc->enable_reg;

	return __set_runlvl_hw_dvs_levels(np, desc, en_reg, en_masks);
}
static int bd71828_set_runlvl_hw_dvs_levels(struct device_node *np,
				       const struct regulator_desc *desc,
				       struct regulator_config *cfg)
{
	/* On BD71828 the RUN level control reg is next to enable reg */
	int en_masks[DVS_RUN_LEVELS] = { BD71828_MASK_RUN0_EN,
					BD71828_MASK_RUN1_EN,
					BD71828_MASK_RUN2_EN,
					BD71828_MASK_RUN3_EN };
	int en_reg = desc->enable_reg + 1;
	
	return __set_runlvl_hw_dvs_levels(np, desc, en_reg, en_masks);
}


static int bd71828_ldo6_parse_dt(struct device_node *np,
			 const struct regulator_desc *desc,
			 struct regulator_config *cfg)
{
	int ret, i;
	uint32_t uv = 0;
	unsigned int en;
	struct regmap *regmap = cfg->regmap;
	static const char * const props[] = { "rohm,dvs-run-voltage",
					      "rohm,dvs-idle-voltage",
					      "rohm,dvs-suspend-voltage",
					      "rohm,dvs-lpsr-voltage" };
	unsigned int mask[] = { BD71828_MASK_RUN_EN, BD71828_MASK_IDLE_EN,
			       BD71828_MASK_SUSP_EN, BD71828_MASK_LPSR_EN };

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = of_property_read_u32(np, props[i], &uv);
		if (ret) {
			if (ret != -EINVAL)
				return ret;
			continue;
		}
		if (uv)
			en = 0xffffffff;
		else
			en = 0;

		ret = regmap_update_bits(regmap, desc->enable_reg, mask[i], en);
		if (ret)
			return ret;
	}
	return 0;
}

static int bd71828_dvs_gpio_set_run_level(struct bd71828_regulator_data *rd,
					  int val)
{
	DECLARE_BITMAP(values, 2);

	dev_dbg(rd->dev, "Setting runlevel (GPIO)\n");
	if (rd->gps->ndescs != 2)
		return -EINVAL;

	if (val < 0 || val > 3)
		return -EINVAL;

	values[0] = val;

	return gpiod_set_array_value_cansleep(rd->gps->ndescs, rd->gps->desc,
				     rd->gps->info, values);
}

#define BD72720_MASK_RUN_LVL_CTRL GENMASK(1,0)
static int bd72720_dvs_i2c_set_run_level(struct bd71828_regulator_data *rd,
					 int lvl)
{
	unsigned int reg;

	if (lvl < 0 || lvl > 2)
		return -EINVAL;

	dev_dbg(rd->dev, "Setting runlevel (%d) (i2c)\n", lvl);
	reg = lvl;
	
	return regmap_update_bits(rd->regmap, BD71828_REG_PS_CTRL_2,
				  BD72720_MASK_RUN_LVL_CTRL, reg);
}

/* Get current run level when RUN levels are controlled using I2C */
static int bd71828_dvs_i2c_set_run_level(struct bd71828_regulator_data *rd,
					 int lvl)
{
	unsigned int reg;

	dev_dbg(rd->dev, "Setting runlevel (%d) (i2c)\n", lvl);
	reg = lvl << (ffs(BD71828_MASK_RUN_LVL_CTRL) - 1);
	
	return regmap_update_bits(rd->regmap, BD71828_REG_PS_CTRL_3,
				  BD71828_MASK_RUN_LVL_CTRL, reg);
}

/* Get current run level when RUN levels are controlled using I2C */
static int bd72720_dvs_i2c_get_run_level(struct bd71828_regulator_data *rd)
{
	int ret;
	unsigned int val;

	dev_dbg(rd->dev, "Getting runlevel (i2c)\n");

	ret = regmap_read(rd->regmap, BD72720_REG_PS_CTRL_2, &val);
	if (ret)
		return ret;

	return (val & BD72720_MASK_RUN_LVL_CTRL);
}

/* Get current run level when RUN levels are controlled using I2C */
static int bd71828_dvs_i2c_get_run_level(struct bd71828_regulator_data *rd)
{
	int ret;
	unsigned int val;

	dev_dbg(rd->dev, "Getting runlevel (i2c)\n");

	ret = regmap_read(rd->regmap, BD71828_REG_PS_CTRL_3, &val);
	if (ret)
		return ret;

	ret = (val & BD71828_MASK_RUN_LVL_CTRL);
	ret >>= ffs(BD71828_MASK_RUN_LVL_CTRL) - 1;

	return ret;
}

/* Get current RUN level when run levels are controlled by GPIO */
static int bd71828_dvs_gpio_get_run_level(struct bd71828_regulator_data *rd)
{
	int run_level;
	int ret;
	DECLARE_BITMAP(values, 2);

	values[0] = 0;
	dev_dbg(rd->dev, "Getting runlevel (gpio)\n");

	if (rd->gps->ndescs != 2)
		return -EINVAL;

	ret = gpiod_get_array_value_cansleep(rd->gps->ndescs, rd->gps->desc,
				     rd->gps->info, values);
	if (ret)
		return ret;

	run_level = values[0];

	return run_level;
}

/* 
 * To be used when BD71828 regulator is controlled by RUN levels
 * via I2C instead of GPIO
 */
static int bd71828_dvs_i2c_is_enabled(struct regulator_dev *rdev)
{
	struct bd71828_regulator_data *data = rdev_get_drvdata(rdev);
	int ret;

	if (!data->get_run_level_i2c) {
		dev_dbg(data->dev, "get_run_level_i2c is NULL\n");
		return -ENOENT;
	}

	mutex_lock(&data->dvs_lock);
	ret = data->get_run_level_i2c(data);
	if (ret < 0)
		goto unlock_out;

	ret = data->run_lvl[ret].enabled;

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

/* 
 * To be used when BD71828 regulator is controlled by RUN levels
 * via GPIO
 */
static int bd71828_dvs_gpio_is_enabled(struct regulator_dev *rdev)
{
	struct bd71828_regulator_data *data = rdev_get_drvdata(rdev);
	int ret;

	if (!data->get_run_level_gpio) {
		dev_dbg(data->dev, "get_run_level_gpio is NULL\n");
		return -ENOENT;
	}

	mutex_lock(&data->dvs_lock);
	ret = data->get_run_level_gpio(data);
	if (ret < 0 || ret >= DVS_RUN_LEVELS)
		goto unlock_out;

	ret = data->run_lvl[ret].enabled;

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

/* 
 * To be used when BD71828 regulator is controlled by RUN levels
 * via I2C instead of GPIO
 */
static int bd71828_dvs_i2c_get_voltage(struct regulator_dev *rdev)
{
	int ret;
	struct bd71828_regulator_data *data = rdev_get_drvdata(rdev);

	if (!data->get_run_level_i2c) {
		dev_dbg(data->dev, "get_run_level_i2c is NULL\n");
		return -ENOENT;
	}

	mutex_lock(&data->dvs_lock);
	ret = data->get_run_level_i2c(data);
	if (ret < 0)
		goto unlock_out;

	ret = data->run_lvl[ret].voltage;

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

/* 
 * To be used when BD71828 regulator is controlled by RUN levels
 * via GPIO
 */
static int bd71828_dvs_gpio_get_voltage(struct regulator_dev *rdev)
{
	int ret;
	struct bd71828_regulator_data *data = rdev_get_drvdata(rdev);


	if (!data->get_run_level_gpio) {
		dev_dbg(data->dev, "get_run_level_gpio is NULL\n");
		return -ENOENT;
	}

	mutex_lock(&data->dvs_lock);
	ret = data->get_run_level_gpio(data);
	if (ret < 0 || DVS_RUN_LEVELS <= ret)
		goto unlock_out;

	ret = data->run_lvl[ret].voltage;

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

/**
 * bd71828_set_runlevel_voltage - change run-level voltage
 *
 * @regulator:  pointer to regulator for which run-level voltage is to be changed
 * @uv:		New voltage for run-level in micro volts
 * @level:	run-level for which the voltage is to be changed
 *
 * Changes the run-level voltage for given regulator
 */
int bd71828_set_runlevel_voltage(struct regulator *regulator, unsigned int uv,
				 unsigned int level)
{
	struct regulator_dev *rdev = regulator->rdev;
	struct bd71828_regulator_data *data = rdev_get_drvdata(rdev);
	int ret;

	if (!data || !data->allow_runlvl)
		return -EINVAL; 

	mutex_lock(&data->dvs_lock);
	ret = set_runlevel_voltage(rdev->regmap, rdev->desc, uv, level);
	mutex_unlock(&data->dvs_lock);

	return ret;
}
EXPORT_SYMBOL(bd71828_set_runlevel_voltage);

/**
 * bd71828_set_runlevel - change system run-level.
 *
 * @regulator:	pointer to one of the BD71828 regulators obtained by
 *		call to regulator_get
 * @level:	New run-level the system should enter
 *
 * Changes the system to run-level which was given as argument. This
 * operation will change state of all regulators which are set to be
 * controlled by run-levels. Note that 'regulator' must point to a
 * regulator which is controlled by run-levels.
 */
int bd71828_set_runlevel(struct regulator *regulator, unsigned int level)
{
	struct regulator_dev *rdev = regulator->rdev;
	struct bd71828_regulator_data *rd = rdev_get_drvdata(rdev);

	if (!rd)
		return -ENOENT;

	if (!rd || !rd->allow_runlvl)
		return -EINVAL; 

	if (rd->gps) {
		if (!rd->set_run_level_gpio)
			return -EINVAL;
		return rd->set_run_level_gpio(rd, level);
	}
	if (!rd->set_run_level_i2c)
		return -EINVAL;

	return rd->set_run_level_i2c(rd, level);
}
EXPORT_SYMBOL(bd71828_set_runlevel);

/**
 * bd71828_get_runlevel - get the current system run-level.
 *
 * @regulator:	pointer to one of the BD71828 regulators obtained by
 *		call to regulator_get
 * @level:	Pointer to value where current run-level is stored
 *
 * Returns the current system run-level. Note that 'regulator' must
 * point to a regulator which is controlled by run-levels.
 */
int bd71828_get_runlevel(struct regulator *regulator, unsigned int *level)
{
	struct regulator_dev *rdev = regulator->rdev;
	struct bd71828_regulator_data *rd = rdev_get_drvdata(rdev);
	int ret;

	if (!rd)
		return -ENOENT;

	if (!rd || !rd->allow_runlvl)
		return -EINVAL; 

	if (!rd->gps) {
		if (!rd->get_run_level_i2c)
			return -ENOENT;
		ret = rd->get_run_level_i2c(rd);
	} else {
		if (!rd->get_run_level_gpio)
			return -ENOENT;
		ret = rd->get_run_level_gpio(rd);
	}
	if (0 > ret)
		return ret;

	*level = (unsigned) ret;

	return 0;
}
EXPORT_SYMBOL(bd71828_get_runlevel);


static const struct regulator_ops dvs_buck_gpio_ops = {
	.is_enabled = bd71828_dvs_gpio_is_enabled, /* Ok */
	.get_voltage = bd71828_dvs_gpio_get_voltage,
};

static const struct regulator_ops dvs_buck_i2c_ops = {
	.is_enabled = bd71828_dvs_i2c_is_enabled,
	.get_voltage = bd71828_dvs_i2c_get_voltage,
};


static const struct regulator_ops bd71828_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
};

static const struct regulator_ops bd71828_dvs_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
};

static const struct regulator_ops bd71828_ldo_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
};

static const struct regulator_ops bd71828_ldo6_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static const struct regulator_ops bd72720_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
};

static const struct regulator_ops bd72720_buck10_ldon_head_op = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
};

static const struct bd71828_regulator_data bd71828_rdata[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK1,
			.ops = &bd71828_dvs_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck1267_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck1267_volts),
			.n_voltages = BD71828_BUCK1267_VOLTS,
			.enable_reg = BD71828_REG_BUCK1_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK1_VOLT,
			.vsel_mask = BD71828_MASK_BUCK1267_VOLT,
			.ramp_delay_table = bd71828_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd71828_ramp_delay),
			.ramp_reg = BD71828_REG_BUCK1_MODE,
			.ramp_mask = BD71828_MASK_RAMP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK1_VOLT,
			.run_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_reg = BD71828_REG_BUCK1_IDLE_VOLT,
			.idle_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_reg = BD71828_REG_BUCK1_SUSP_VOLT,
			.suspend_mask = BD71828_MASK_BUCK1267_VOLT,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
			/*
			 * LPSR voltage is same as SUSPEND voltage. Allow
			 * setting it so that regulator can be set enabled at
			 * LPSR state
			 */
			.lpsr_reg = BD71828_REG_BUCK1_SUSP_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK1267_VOLT,
		},
		.sub_run_mode_reg = BD71828_REG_PS_CTRL_1,
		.sub_run_mode_mask = BD71828_MASK_DVS_BUCK1_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd71828_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd71828_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd71828_set_runlvl_hw_dvs_levels,
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK2,
			.ops = &bd71828_dvs_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck1267_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck1267_volts),
			.n_voltages = BD71828_BUCK1267_VOLTS,
			.enable_reg = BD71828_REG_BUCK2_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK2_VOLT,
			.vsel_mask = BD71828_MASK_BUCK1267_VOLT,
			.ramp_delay_table = bd71828_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd71828_ramp_delay),
			.ramp_reg = BD71828_REG_BUCK2_MODE,
			.ramp_mask = BD71828_MASK_RAMP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK2_VOLT,
			.run_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_reg = BD71828_REG_BUCK2_IDLE_VOLT,
			.idle_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_reg = BD71828_REG_BUCK2_SUSP_VOLT,
			.suspend_mask = BD71828_MASK_BUCK1267_VOLT,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
			.lpsr_reg = BD71828_REG_BUCK2_SUSP_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK1267_VOLT,
		},
		.sub_run_mode_reg = BD71828_REG_PS_CTRL_1,
		.sub_run_mode_mask = BD71828_MASK_DVS_BUCK2_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd71828_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd71828_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd71828_set_runlvl_hw_dvs_levels,
	},
	{
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("BUCK3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK3,
			.ops = &bd71828_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck3_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck3_volts),
			.n_voltages = BD71828_BUCK3_VOLTS,
			.enable_reg = BD71828_REG_BUCK3_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK3_VOLT,
			.vsel_mask = BD71828_MASK_BUCK3_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * BUCK3 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK3_VOLT,
			.idle_reg = BD71828_REG_BUCK3_VOLT,
			.suspend_reg = BD71828_REG_BUCK3_VOLT,
			.lpsr_reg = BD71828_REG_BUCK3_VOLT,
			.run_mask = BD71828_MASK_BUCK3_VOLT,
			.idle_mask = BD71828_MASK_BUCK3_VOLT,
			.suspend_mask = BD71828_MASK_BUCK3_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK3_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	},
	{
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("BUCK4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK4,
			.ops = &bd71828_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck4_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck4_volts),
			.n_voltages = BD71828_BUCK4_VOLTS,
			.enable_reg = BD71828_REG_BUCK4_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK4_VOLT,
			.vsel_mask = BD71828_MASK_BUCK4_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * BUCK4 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK4_VOLT,
			.idle_reg = BD71828_REG_BUCK4_VOLT,
			.suspend_reg = BD71828_REG_BUCK4_VOLT,
			.lpsr_reg = BD71828_REG_BUCK4_VOLT,
			.run_mask = BD71828_MASK_BUCK4_VOLT,
			.idle_mask = BD71828_MASK_BUCK4_VOLT,
			.suspend_mask = BD71828_MASK_BUCK4_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK4_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	},
	{
		.desc = {
			.name = "buck5",
			.of_match = of_match_ptr("BUCK5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK5,
			.ops = &bd71828_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck5_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck5_volts),
			.n_voltages = BD71828_BUCK5_VOLTS,
			.enable_reg = BD71828_REG_BUCK5_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK5_VOLT,
			.vsel_mask = BD71828_MASK_BUCK5_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * BUCK5 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK5_VOLT,
			.idle_reg = BD71828_REG_BUCK5_VOLT,
			.suspend_reg = BD71828_REG_BUCK5_VOLT,
			.lpsr_reg = BD71828_REG_BUCK5_VOLT,
			.run_mask = BD71828_MASK_BUCK5_VOLT,
			.idle_mask = BD71828_MASK_BUCK5_VOLT,
			.suspend_mask = BD71828_MASK_BUCK5_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK5_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	},
	{
		.desc = {
			.name = "buck6",
			.of_match = of_match_ptr("BUCK6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK6,
			.ops = &bd71828_dvs_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck1267_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck1267_volts),
			.n_voltages = BD71828_BUCK1267_VOLTS,
			.enable_reg = BD71828_REG_BUCK6_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK6_VOLT,
			.vsel_mask = BD71828_MASK_BUCK1267_VOLT,
			.ramp_delay_table = bd71828_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd71828_ramp_delay),
			.ramp_reg = BD71828_REG_BUCK6_MODE,
			.ramp_mask = BD71828_MASK_RAMP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK6_VOLT,
			.run_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_reg = BD71828_REG_BUCK6_IDLE_VOLT,
			.idle_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_reg = BD71828_REG_BUCK6_SUSP_VOLT,
			.suspend_mask = BD71828_MASK_BUCK1267_VOLT,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
			.lpsr_reg = BD71828_REG_BUCK6_SUSP_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK1267_VOLT,
		},
		.sub_run_mode_reg = BD71828_REG_PS_CTRL_1,
		.sub_run_mode_mask = BD71828_MASK_DVS_BUCK6_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd71828_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd71828_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd71828_set_runlvl_hw_dvs_levels,
	},
	{
		.desc = {
			.name = "buck7",
			.of_match = of_match_ptr("BUCK7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_BUCK7,
			.ops = &bd71828_dvs_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_buck1267_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_buck1267_volts),
			.n_voltages = BD71828_BUCK1267_VOLTS,
			.enable_reg = BD71828_REG_BUCK7_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_BUCK7_VOLT,
			.vsel_mask = BD71828_MASK_BUCK1267_VOLT,
			.ramp_delay_table = bd71828_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd71828_ramp_delay),
			.ramp_reg = BD71828_REG_BUCK7_MODE,
			.ramp_mask = BD71828_MASK_RAMP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_BUCK7_VOLT,
			.run_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_reg = BD71828_REG_BUCK7_IDLE_VOLT,
			.idle_mask = BD71828_MASK_BUCK1267_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_reg = BD71828_REG_BUCK7_SUSP_VOLT,
			.suspend_mask = BD71828_MASK_BUCK1267_VOLT,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
			.lpsr_reg = BD71828_REG_BUCK7_SUSP_VOLT,
			.lpsr_mask = BD71828_MASK_BUCK1267_VOLT,
		},
		.sub_run_mode_reg = BD71828_REG_PS_CTRL_1,
		.sub_run_mode_mask = BD71828_MASK_DVS_BUCK7_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd71828_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd71828_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd71828_set_runlvl_hw_dvs_levels,
	},
	{
		.desc = {
			.name = "ldo1",
			.of_match = of_match_ptr("LDO1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO1,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO1_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO1_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * LDO1 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO1_VOLT,
			.idle_reg = BD71828_REG_LDO1_VOLT,
			.suspend_reg = BD71828_REG_LDO1_VOLT,
			.lpsr_reg = BD71828_REG_LDO1_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	}, {
		.desc = {
			.name = "ldo2",
			.of_match = of_match_ptr("LDO2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO2,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO2_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO2_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * LDO2 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO2_VOLT,
			.idle_reg = BD71828_REG_LDO2_VOLT,
			.suspend_reg = BD71828_REG_LDO2_VOLT,
			.lpsr_reg = BD71828_REG_LDO2_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	}, {
		.desc = {
			.name = "ldo3",
			.of_match = of_match_ptr("LDO3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO3,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO3_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO3_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * LDO3 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO3_VOLT,
			.idle_reg = BD71828_REG_LDO3_VOLT,
			.suspend_reg = BD71828_REG_LDO3_VOLT,
			.lpsr_reg = BD71828_REG_LDO3_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},

	}, {
		.desc = {
			.name = "ldo4",
			.of_match = of_match_ptr("LDO4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO4,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO4_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO4_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * LDO1 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO4_VOLT,
			.idle_reg = BD71828_REG_LDO4_VOLT,
			.suspend_reg = BD71828_REG_LDO4_VOLT,
			.lpsr_reg = BD71828_REG_LDO4_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},
	}, {
		.desc = {
			.name = "ldo5",
			.of_match = of_match_ptr("LDO5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO5,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO5_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO5_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.of_parse_cb = buck_set_hw_dvs_levels,
			.owner = THIS_MODULE,
		},
		/*
		 * LDO5 is special. It can choose vsel settings to be configured
		 * from 2 different registers (by GPIO).
		 *
		 * This driver supports only configuration where
		 * BD71828_REG_LDO5_VOLT_L is used.
		 */
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO5_VOLT,
			.idle_reg = BD71828_REG_LDO5_VOLT,
			.suspend_reg = BD71828_REG_LDO5_VOLT,
			.lpsr_reg = BD71828_REG_LDO5_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},

	}, {
		.desc = {
			.name = "ldo6",
			.of_match = of_match_ptr("LDO6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO6,
			.ops = &bd71828_ldo6_ops,
			.type = REGULATOR_VOLTAGE,
			.fixed_uV = BD71828_LDO_6_VOLTAGE,
			.n_voltages = 1,
			.enable_reg = BD71828_REG_LDO6_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.owner = THIS_MODULE,
			/*
			 * LDO6 only supports enable/disable for all states.
			 * Voltage for LDO6 is fixed.
			 */
			.of_parse_cb = bd71828_ldo6_parse_dt,
		},
	}, {
		.desc = {
			/* SNVS LDO in data-sheet */
			.name = "ldo7",
			.of_match = of_match_ptr("LDO7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD71828_LDO_SNVS,
			.ops = &bd71828_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd71828_ldo_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71828_ldo_volts),
			.n_voltages = BD71828_LDO_VOLTS,
			.enable_reg = BD71828_REG_LDO7_EN,
			.enable_mask = BD71828_MASK_RUN_EN,
			.vsel_reg = BD71828_REG_LDO7_VOLT,
			.vsel_mask = BD71828_MASK_LDO_VOLT,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			/*
			 * LDO7 only supports single voltage for all states.
			 * voltage can be individually enabled for each state
			 * though => allow setting all states to support
			 * enabling power rail on different states.
			 */
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR,
			.run_reg = BD71828_REG_LDO7_VOLT,
			.idle_reg = BD71828_REG_LDO7_VOLT,
			.suspend_reg = BD71828_REG_LDO7_VOLT,
			.lpsr_reg = BD71828_REG_LDO7_VOLT,
			.run_mask = BD71828_MASK_LDO_VOLT,
			.idle_mask = BD71828_MASK_LDO_VOLT,
			.suspend_mask = BD71828_MASK_LDO_VOLT,
			.lpsr_mask = BD71828_MASK_LDO_VOLT,
			.idle_on_mask = BD71828_MASK_IDLE_EN,
			.suspend_on_mask = BD71828_MASK_SUSP_EN,
			.lpsr_on_mask = BD71828_MASK_LPSR_EN,
		},

	},
};

#define BD72720_BUCK10_DESC_INDEX 10
#define BD72720_NUM_BUCK_VOLTS 0x100
#define BD72720_NUM_LDO_VOLTS 0x100
#define BD72720_NUM_LDO12346_VOLTS 0x80

static const struct bd71828_regulator_data bd72720_rdata[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("buck1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK1,
			.type = REGULATOR_VOLTAGE,

			/*
			 * The BD72720 BUCK1 and LDO1 support GPIO toggled
			 * sub-RUN states called RUN0, RUN1, RUN2 and RUN3.
			 * The "operating mode" (sub-RUN states or normal)
			 * can be changed by a register.
			 *
			 * When the sub-RUN states are used, the voltage and
			 * enable state depend on a state specific
			 * configuration. The voltage and enable configuration
			 * for BUCK1 and LDO1 can be defined for each sub-RUN
			 * state using BD72720_REG_[BUCK,LDO]1_VSEL_R[0,1,2,3]
			 * voltage selection registers and the bits
			 * BD72720_MASK_RUN_[0,1,2,3]_EN in the enable registers.
			 * The PMIC will change both the BUCK1 and and LDO1
			 * voltages to the states defined in these registers
			 * when "DVS GPIOs" are toggled.
			 *
			 * If RUN 0 .. RUN 4 states are to be used, the normal
			 * voltage configuration mechanisms do not apply
			 * and we will overwrite the ops and ignore the voltage
			 * setting/getting registers which are setup here.
			 */
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck1234_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK1_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK1_VSEL_RB,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK1_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK1_VSEL_RB,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_reg = BD72720_REG_BUCK1_VSEL_I,
			.idle_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_BUCK1_VSEL_S,
			.suspend_mask = BD72720_MASK_BUCK_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_BUCK1_VSEL_DI,
			.lpsr_mask = BD72720_MASK_BUCK_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
		.sub_run_mode_reg = BD72720_REG_PS_CTRL_2,
#define BD72720_MASK_DVS_BUCK1_CTRL BIT(4)
		.sub_run_mode_mask = BD72720_MASK_DVS_BUCK1_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd72720_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd72720_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd72720_set_runlvl_hw_dvs_levels,
	}, {
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("buck2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK2,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck1234_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK2_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK2_VSEL_R,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK2_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK2_VSEL_R,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_reg = BD72720_REG_BUCK2_VSEL_I,
			.idle_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_BUCK2_VSEL_S,
			.suspend_mask = BD72720_MASK_BUCK_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_BUCK2_VSEL_DI,
			.lpsr_mask = BD72720_MASK_BUCK_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("buck3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK3,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck1234_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK3_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK3_VSEL_R,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK3_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK3_VSEL_R,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_reg = BD72720_REG_BUCK3_VSEL_I,
			.idle_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_BUCK3_VSEL_S,
			.suspend_mask = BD72720_MASK_BUCK_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_BUCK3_VSEL_DI,
			.lpsr_mask = BD72720_MASK_BUCK_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("buck4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK4,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck1234_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK4_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK4_VSEL_R,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK4_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK4_VSEL_R,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_reg = BD72720_REG_BUCK4_VSEL_I,
			.idle_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_BUCK4_VSEL_S,
			.suspend_mask = BD72720_MASK_BUCK_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_BUCK4_VSEL_DI,
			.lpsr_mask = BD72720_MASK_BUCK_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck5",
			.of_match = of_match_ptr("buck5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK5,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck589_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck589_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK5_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK5_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK5_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK5_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck6",
			.of_match = of_match_ptr("buck6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK6,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck67_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck67_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK6_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK6_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK6_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK6_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck7",
			.of_match = of_match_ptr("buck7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK7,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck67_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck67_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK7_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK7_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK7_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK7_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck8",
			.of_match = of_match_ptr("buck8"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK8,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck589_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck589_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK8_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK8_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK8_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK8_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck9",
			.of_match = of_match_ptr("buck9"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK9,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck589_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck589_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK9_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK9_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK9_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK9_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "buck10",
			.of_match = of_match_ptr("buck10"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_BUCK10,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_buck10_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_buck10_volts),
			.n_voltages = BD72720_NUM_BUCK_VOLTS,
			.enable_reg = BD72720_REG_BUCK10_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_BUCK10_VSEL,
			.vsel_mask = BD72720_MASK_BUCK_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_BUCK10_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_BUCK10_VSEL,
			.run_mask = BD72720_MASK_BUCK_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo1",
			.of_match = of_match_ptr("ldo1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO1,
			.type = REGULATOR_VOLTAGE,

			/*
			 * The BD72720 BUCK1 and LDO1 support GPIO toggled
			 * sub-RUN states called RUN0, RUN1, RUN2 and RUN3.
			 * The "operating mode" (sub-RUN states or normal)
			 * can be changed by a register.
			 *
			 * When the sub-RUN states are used, the voltage and
			 * enable state depend on a state specific
			 * configuration. The voltage and enable configuration
			 * for BUCK1 and LDO1 can be defined for each sub-RUN
			 * state using BD72720_REG_[BUCK,LDO]1_VSEL_R[0,1,2,3]
			 * voltage selection registers and the bits
			 * BD72720_MASK_RUN_[0,1,2,3]_EN in the enable registers.
			 * The PMIC will change both the BUCK1 and and LDO1
			 * voltages to the states defined in these registers
			 * when "DVS GPIOs" are toggled.
			 *
			 * If RUN 0 .. RUN 4 states are to be used, the normal
			 * voltage configuration mechanisms do not apply
			 * and we will overwrite the ops and ignore the voltage
			 * setting/getting registers which are setup here.
			 */
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo1234_volts),
			.n_voltages = BD72720_NUM_LDO12346_VOLTS,
			.enable_reg = BD72720_REG_LDO1_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO1_VSEL_RB,
			.vsel_mask = BD72720_MASK_LDO12346_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO1_MODE1,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO1_VSEL_RB,
			.run_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_reg = BD72720_REG_LDO1_VSEL_I,
			.idle_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_LDO1_VSEL_S,
			.suspend_mask = BD72720_MASK_LDO12346_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_LDO1_VSEL_DI,
			.lpsr_mask = BD72720_MASK_LDO12346_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
		.sub_run_mode_reg = BD72720_REG_PS_CTRL_2,
#define BD72720_MASK_DVS_LDO1_CTRL BIT(5)
		.sub_run_mode_mask = BD72720_MASK_DVS_LDO1_CTRL,
		.get_run_level_gpio = bd71828_dvs_gpio_get_run_level,
		.set_run_level_gpio = bd71828_dvs_gpio_set_run_level,
		.get_run_level_i2c = bd72720_dvs_i2c_get_run_level,
		.set_run_level_i2c = bd72720_dvs_i2c_set_run_level,
		.of_set_runlvl_levels = bd72720_set_runlvl_hw_dvs_levels,
	}, {
		.desc = {
			.name = "ldo2",
			.of_match = of_match_ptr("ldo2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO2,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo1234_volts),
			.n_voltages = BD72720_NUM_LDO12346_VOLTS,
			.enable_reg = BD72720_REG_LDO2_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO2_VSEL_R,
			.vsel_mask = BD72720_MASK_LDO12346_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO2_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO2_VSEL_R,
			.run_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_reg = BD72720_REG_LDO2_VSEL_I,
			.idle_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_LDO2_VSEL_S,
			.suspend_mask = BD72720_MASK_LDO12346_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_LDO2_VSEL_DI,
			.lpsr_mask = BD72720_MASK_LDO12346_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo3",
			.of_match = of_match_ptr("ldo3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO3,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo1234_volts),
			.n_voltages = BD72720_NUM_LDO12346_VOLTS,
			.enable_reg = BD72720_REG_LDO3_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO3_VSEL_R,
			.vsel_mask = BD72720_MASK_LDO12346_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO3_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO3_VSEL_R,
			.run_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_reg = BD72720_REG_LDO3_VSEL_I,
			.idle_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_LDO3_VSEL_S,
			.suspend_mask = BD72720_MASK_LDO12346_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_LDO3_VSEL_DI,
			.lpsr_mask = BD72720_MASK_LDO12346_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo4",
			.of_match = of_match_ptr("ldo4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO4,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo1234_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo1234_volts),
			.n_voltages = BD72720_NUM_LDO12346_VOLTS,
			.enable_reg = BD72720_REG_LDO4_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO4_VSEL_R,
			.vsel_mask = BD72720_MASK_LDO12346_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO4_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO4_VSEL_R,
			.run_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_reg = BD72720_REG_LDO4_VSEL_I,
			.idle_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_reg = BD72720_REG_LDO4_VSEL_S,
			.suspend_mask = BD72720_MASK_LDO12346_VSEL,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_reg = BD72720_REG_LDO4_VSEL_DI,
			.lpsr_mask = BD72720_MASK_LDO12346_VSEL,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo5",
			.of_match = of_match_ptr("ldo5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO5,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO5_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO5_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO5_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO5_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo6",
			.of_match = of_match_ptr("ldo6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO6,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo6_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo6_volts),
			.n_voltages = BD72720_NUM_LDO12346_VOLTS,
			.enable_reg = BD72720_REG_LDO6_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO6_VSEL,
			.vsel_mask = BD72720_MASK_LDO12346_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO6_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO6_VSEL,
			.run_mask = BD72720_MASK_LDO12346_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo7",
			.of_match = of_match_ptr("ldo7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO7,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO7_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO7_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO7_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO7_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo8",
			.of_match = of_match_ptr("ldo8"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO8,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO8_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO8_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO8_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO8_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo9",
			.of_match = of_match_ptr("ldo9"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO9,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO9_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO9_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO9_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO9_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo10",
			.of_match = of_match_ptr("ldo10"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO10,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO10_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO10_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO10_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO10_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	}, {
		.desc = {
			.name = "ldo11",
			.of_match = of_match_ptr("ldo11"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD72720_LDO11,
			.type = REGULATOR_VOLTAGE,
			.ops = &bd72720_regulator_ops,
			.linear_ranges = bd72720_ldo57891011_volts,
			.n_linear_ranges = ARRAY_SIZE(bd72720_ldo57891011_volts),
			.n_voltages = BD72720_NUM_LDO_VOLTS,
			.enable_reg = BD72720_REG_LDO11_ON,
			.enable_mask = BD72720_MASK_RUN_B_EN,
			.vsel_reg = BD72720_REG_LDO11_VSEL,
			.vsel_mask = BD72720_MASK_LDO_VSEL,

			.ramp_delay_table = bd72720_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd72720_ramp_delay),
			.ramp_reg = BD72720_REG_LDO11_MODE,
			.ramp_mask = BD72720_MASK_RAMP_UP_DELAY,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND |
				     ROHM_DVS_LEVEL_LPSR, /* Deep idle in data-sheet? */
			.run_reg = BD72720_REG_LDO11_VSEL,
			.run_mask = BD72720_MASK_LDO_VSEL,
			.idle_on_mask = BD72720_MASK_IDLE_EN,
			.suspend_on_mask = BD72720_MASK_SUSPEND_EN,
			.lpsr_on_mask = BD72720_MASK_DEEP_IDLE_EN,
		},
	},
};

static int bd72720_buck10_ldon_head_mode(struct device *dev,
					 struct device_node *np,
					 struct regmap *regmap,
					 struct regulator_desc *buck10_desc)
{
	uint32_t ldon_head;
	int ldon_val;
	int ret;

	ret = of_property_read_u32(np, "rohm,ldon-head-mv", &ldon_head);
	if (ret == -EINVAL)
		return 0;
	if (ret)
		return ret;

	/*
	 * LDON_HEAD mode means the BUCK10 is used to supply LDOs 1-4 and
	 * the BUCK 10 voltage is automatically set to follow LDO 1-4
	 * settings. Thus the BUCK10 should not allow voltage [g/s]etting.
	 */
	buck10_desc->ops = &bd72720_buck10_ldon_head_op;

	ldon_val = ldon_head / 50 + 1;
	if (ldon_head > 300) {
		dev_warn(dev, "Unsupported LDON_HEAD, clamping to 300 mV\n");
		ldon_val = 7;
	}

	return regmap_update_bits(regmap, BD72720_REG_LDO1_MODE2,
				  BD72720_MASK_LDON_HEAD, ldon_val);
}

static bool mark_regulator_runlvl_controlled(struct device *dev,
					     struct device_node *np,
					     struct bd71828_regulator_data *rd,
					     int num_rd)
{
	int i;
	bool ret = false;

	for (i = 0; i < num_rd; i++)
		if (!of_node_name_eq(np, rd[i].desc.of_match)) {
			if (!rd[i].sub_run_mode_mask)
				dev_warn(rd->dev,
					 "%s: run-level dvs not supported\n",
					 rd[i].desc.name);
			else
				ret = rd[i].allow_runlvl = true;
		}
	return ret;
}

static int get_runcontrolled_bucks_dt(struct device *dev,
				      struct bd71828_regulator_data *rd,
				      int num_rd)
{
	struct device_node *np;
	struct device_node *nproot = dev->of_node;
	const char *prop = "rohm,dvs-runlvl-ctrl";
	int runctrl_needed = 0;

	/*g->runlvl = 0;*/

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	for_each_child_of_node(nproot, np)
		if (of_property_read_bool(np, prop))
			runctrl_needed += mark_regulator_runlvl_controlled(dev,
								np, rd, num_rd);
	of_node_put(nproot);

	return runctrl_needed;
}

static int check_dt_for_gpio_controls(struct device *d,
				      struct bd71828_regulator_data *rd,
				      int num_rd)
{
	int ret;

	ret = get_runcontrolled_bucks_dt(d, rd, num_rd);
	if (ret < 0)
		return ret;

	/* If the run level control is not requested by any bucks we're done */
	if (!ret)
		return 0;

	rd->allow_runlvl = true;

	rd->gps = devm_gpiod_get_array(d, "rohm,dvs-vsel", GPIOD_OUT_LOW);
	if (IS_ERR(rd->gps)) {
		ret = PTR_ERR(rd->gps);
		if (ret != -ENOENT)
			return ret;
	}

	if (ret == -ENOENT || rd->gps->ndescs != 2)
		rd->desc.ops = &dvs_buck_i2c_ops;
	else
		rd->desc.ops = &dvs_buck_gpio_ops;

	rd->desc.of_parse_cb = rd->of_set_runlvl_levels;

	return 0;
}

static ssize_t show_runlevel(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int runlevel;
	struct bd71828_regulator_data *rd = dev_get_drvdata(dev);

	if (!rd)
		return -ENOENT;

 	if (rd->gps) {
		if(!rd->get_run_level_gpio)
			return -ENOENT;
		runlevel =  rd->get_run_level_gpio(rd);
	} else {
		if(!rd->get_run_level_i2c)
			return -ENOENT;
		runlevel = rd->get_run_level_i2c(rd);
	}
	if (0 > runlevel)
		return runlevel;

	return sprintf(buf, "0x%x\n", runlevel);
}

static ssize_t set_runlevel(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct bd71828_regulator_data *rd = dev_get_drvdata(dev);
	long val;

	if (kstrtol(buf, 0, &val) != 0)
		return -EINVAL;

	if (rd->gps) {
		if (!rd->set_run_level_gpio)
			return -ENOENT;
		val = rd->set_run_level_gpio(rd, val);
	} else {
		if (!rd->set_run_level_i2c)
			return -ENOENT;
		val = rd->set_run_level_i2c(rd, val);
	}
	if (val)
		return val;

	return count;
}

static DEVICE_ATTR(runlevel, 0664, show_runlevel, set_runlevel);

static struct attribute *runlevel_attributes[] = {
	&dev_attr_runlevel.attr,
	NULL
};

static const struct attribute_group bd71828_attr_group = {
	.attrs	= runlevel_attributes,
};

static int bd71828_create_sysfs(struct platform_device *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &bd71828_attr_group);
}

static int bd71828_remove_sysfs(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &bd71828_attr_group);
	return 0;
}

static int bd71828_remove(struct platform_device *pdev)
{
	return bd71828_remove_sysfs(pdev);
}

/*
 * TODO: use __cleanup stuff if porting to a recent kernel
 */
static int bd72720_dt_parse(struct device *dev,
			    struct regulator_desc *buck10_desc,
			    struct regmap *regmap)
{
	struct device_node *nproot = dev->of_node;
	struct device_node *np;
	int ret;

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	np = of_get_child_by_name(nproot, "buck10");
	if (!np) {
		dev_err(dev, "failed to find buck10 regulator node\n");
		of_node_put(nproot);
		return -ENODEV;
	}
	of_node_put(nproot);
	ret = bd72720_buck10_ldon_head_mode(dev, np, regmap, buck10_desc);
	of_node_put(np);

	return ret;
}

static int bd71828_probe(struct platform_device *pdev)
{
	int i, ret, num_regulators;
	struct regulator_config config = {
		.dev = pdev->dev.parent,
	};
	enum rohm_chip_type chip = platform_get_device_id(pdev)->driver_data;
	struct bd71828_regulator_data *rdata;

	/* TODO: Check if BD72720 MFD device has 2 regmaps */
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!config.regmap)
		return -ENODEV;

 	switch (chip) {
	case ROHM_CHIP_TYPE_BD72720:
		rdata = devm_kmemdup(&pdev->dev, bd72720_rdata,
				   sizeof(bd72720_rdata), GFP_KERNEL);
		if (!rdata)
			return -ENOMEM;

		ret = bd72720_dt_parse(&pdev->dev, &rdata[BD72720_BUCK10_DESC_INDEX].desc,
				       config.regmap);
		if (ret)
			return ret;

		num_regulators = ARRAY_SIZE(bd72720_rdata);
		break;

	case ROHM_CHIP_TYPE_BD71828:
		rdata = devm_kmemdup(&pdev->dev, bd71828_rdata,
				   sizeof(bd71828_rdata), GFP_KERNEL);
		if (!rdata)
			return -ENOMEM;

		num_regulators = ARRAY_SIZE(bd71828_rdata);

		break;
	default:
		return dev_err_probe(&pdev->dev, -EINVAL,
				     "Unsupported device\n");
	}

	ret = check_dt_for_gpio_controls(pdev->dev.parent, rdata, num_regulators);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get DVS gpio resources\n");
		return ret;
	}

	for (i = 0; i < num_regulators; i++) {
		struct regulator_dev *rdev;
		struct bd71828_regulator_data *rd;

		rd = &rdata[i];
		rd->dev = &pdev->dev;
		config.driver_data = &rd[i];
		rdev = devm_regulator_register(&pdev->dev,
					       &rd->desc, &config);
		if (IS_ERR(rdev))
			return dev_err_probe(&pdev->dev, PTR_ERR(rdev),
					     "failed to register %s regulator\n",
					     rd->desc.name);
		if (rd->sub_run_mode_mask) {
			if (rd->allow_runlvl) {
				mutex_init(&rd->dvs_lock);
				ret = regmap_set_bits(config.regmap,
						      rd->sub_run_mode_reg,
						      rd->sub_run_mode_mask);
			} else {
				ret = regmap_clear_bits(config.regmap,
							rd->sub_run_mode_reg,
							rd->sub_run_mode_mask);
			}
			if (ret)
				dev_err_probe(&pdev->dev, ret,
					      "%s: Failed to configure sub-run-level\n",
					      rd->desc.name);
		}
	}
	/* TODO: Call only for BD71828 - or make it work w/ BD72720 */
	return bd71828_create_sysfs(pdev);
}

static const struct platform_device_id bd71828_pmic_id[] = {
	{ "bd71828-pmic", ROHM_CHIP_TYPE_BD71828 },
	{ "bd72720-pmic", ROHM_CHIP_TYPE_BD72720 },
	{ },
};
MODULE_DEVICE_TABLE(platform, bd71828_pmic_id);

static struct platform_driver bd71828_regulator = {
	.driver = {
		.name = "bd71828-pmic",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = bd71828_probe,
	.remove = bd71828_remove,
	.id_table = bd71828_pmic_id,
};

module_platform_driver(bd71828_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71828 voltage regulator driver");
MODULE_LICENSE("GPL");
