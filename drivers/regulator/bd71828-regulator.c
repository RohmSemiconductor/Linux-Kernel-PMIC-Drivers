// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2019 ROHM Semiconductors
// bd71828-regulator.c ROHM BD71828GW-DS1 regulator driver
//

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/module.h>
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

#define MAX_GPIO_DVS_BUCKS 4
#define DVS_RUN_LEVELS 4
#define BD71828_RUNLVL_DEFAULT_UV 500000

struct reg_init {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

struct run_lvl_ctrl {
	unsigned int voltage;
	bool enabled;
};

struct bd71828_regulator_data {
	struct regulator_desc desc;
	struct rohm_dvs_config dvs;
	const struct reg_init *reg_inits;
	int reg_init_amnt;
	struct run_lvl_ctrl run_lvl[DVS_RUN_LEVELS];
	struct mutex dvs_lock;
	struct gpio_descs *gps;
	struct regmap *regmap;
	bool allow_runlvl;
};

static const struct reg_init buck1_inits[] = {
	/*
	 * DVS Buck voltages can be changed by register values or via GPIO.
	 * Use register accesses by default.
	 */
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK1_CTRL,
		.val = BD71828_DVS_BUCK1_CTRL_I2C,
	},
};

static const struct reg_init buck1_gpio_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK1_CTRL,
		.val = BD71828_DVS_BUCK1_USE_RUNLVL,
	},
};

static const struct reg_init buck2_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK2_CTRL,
		.val = BD71828_DVS_BUCK2_CTRL_I2C,
	},
};

static const struct reg_init buck2_gpio_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK2_CTRL,
		.val = BD71828_DVS_BUCK2_USE_RUNLVL,
	},
};

static const struct reg_init buck6_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK6_CTRL,
		.val = BD71828_DVS_BUCK6_CTRL_I2C,
	},
};

static const struct reg_init buck6_gpio_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK6_CTRL,
		.val = BD71828_DVS_BUCK6_USE_RUNLVL,
	},
};

static const struct reg_init buck7_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK7_CTRL,
		.val = BD71828_DVS_BUCK7_CTRL_I2C,
	},
};

static const struct reg_init buck7_gpio_inits[] = {
	{
		.reg = BD71828_REG_PS_CTRL_1,
		.mask = BD71828_MASK_DVS_BUCK7_CTRL,
		.val = BD71828_DVS_BUCK7_USE_RUNLVL,
	},
};

static const struct regulator_linear_range bd71828_buck1267_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x00, 0xef, 6250),
	REGULATOR_LINEAR_RANGE(2000000, 0xf0, 0xff, 0),
};

static const struct regulator_linear_range bd71828_buck3_volts[] = {
	REGULATOR_LINEAR_RANGE(1200000, 0x00, 0x0f, 50000),
	REGULATOR_LINEAR_RANGE(2000000, 0x10, 0x1f, 0),
};

static const struct regulator_linear_range bd71828_buck4_volts[] = {
	REGULATOR_LINEAR_RANGE(1000000, 0x00, 0x1f, 25000),
	REGULATOR_LINEAR_RANGE(1800000, 0x20, 0x3f, 0),
};

static const struct regulator_linear_range bd71828_buck5_volts[] = {
	REGULATOR_LINEAR_RANGE(2500000, 0x00, 0x0f, 50000),
	REGULATOR_LINEAR_RANGE(3300000, 0x10, 0x1f, 0),
};

static const struct regulator_linear_range bd71828_ldo_volts[] = {
	REGULATOR_LINEAR_RANGE(800000, 0x00, 0x31, 50000),
	REGULATOR_LINEAR_RANGE(3300000, 0x32, 0x3f, 0),
};

static int bd71828_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	unsigned int val;

	switch (ramp_delay) {
	case 1 ... 2500:
		val = 0;
		break;
	case 2501 ... 5000:
		val = 1;
		break;
	case 5001 ... 10000:
		val = 2;
		break;
	case 10001 ... 20000:
		val = 3;
		break;
	default:
		val = 3;
		dev_err(&rdev->dev,
			"ramp_delay: %d not supported, setting 20mV/uS",
			 ramp_delay);
	}

	/*
	 * On BD71828 the ramp delay level control reg is at offset +2 to
	 * enable reg
	 */
	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg + 2,
				  BD71828_MASK_RAMP_DELAY,
				  val << (ffs(BD71828_MASK_RAMP_DELAY) - 1));
}

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
	 * RUN level registers are next to vsel_reg. RUN0 reg is next, then
	 * is the RUN 1 reg and so on...
	 */
	u8 reg = desc->vsel_reg + level + 1;
	u8 mask = BD71828_MASK_BUCK1267_VOLT;

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

static int buck_set_runlvl_hw_dvs_levels(struct device_node *np,
				       const struct regulator_desc *desc,
				       struct regulator_config *cfg)
{
	struct bd71828_regulator_data *data;
	/* Default to 'disabled' (0 uV) if no property found */
	uint32_t uv[DVS_RUN_LEVELS] = {0};
	int i, ret;
	const char *prop = "rohm,dvs-runlevel-microvolts";
	/* On BD71828 the RUN level control reg is next to enable reg */
	u8 en_reg = desc->enable_reg + 1;
	u8 en_masks[DVS_RUN_LEVELS] = { BD71828_MASK_RUN0_EN,
					BD71828_MASK_RUN1_EN,
					BD71828_MASK_RUN2_EN,
					BD71828_MASK_RUN3_EN };

	data = container_of(desc, struct bd71828_regulator_data, desc);

	ret = of_property_read_variable_u32_array(np, prop, uv, 1,
						  DVS_RUN_LEVELS);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	mutex_lock(&data->dvs_lock);
	for (i = 0; i < DVS_RUN_LEVELS; i++) {
		if (uv[i]) {
			data->run_lvl[i].voltage = uv[i];
			data->run_lvl[i].enabled = true;

			ret = set_runlevel_voltage(cfg->regmap, desc, uv[i], i);
			if (ret)
				goto unlock_out;

			ret = regmap_update_bits(cfg->regmap, en_reg,
						 en_masks[i], en_masks[i]);
		} else {
			data->run_lvl[i].voltage = BD71828_RUNLVL_DEFAULT_UV;
			ret = regmap_update_bits(cfg->regmap, en_reg,
						 en_masks[i], 0);
		}
		if (ret)
			goto unlock_out;
	}

	ret = rohm_regulator_set_dvs_levels(&data->dvs, np, desc, cfg->regmap);

unlock_out:
	mutex_unlock(&data->dvs_lock);

	return ret;
}

static int ldo6_parse_dt(struct device_node *np,
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

	if (rd->gps->ndescs != 2)
		return -EINVAL;

	if (val < 0 || val > 3)
		return -EINVAL;

	values[0] = val;

	return gpiod_set_array_value_cansleep(rd->gps->ndescs, rd->gps->desc,
				     rd->gps->info, values);
}

/* Get current run level when RUN levels are controlled using I2C */
static int bd71828_dvs_i2c_set_run_level(struct regmap *regmap,
					 int lvl)
{
	unsigned int reg;

	reg = lvl << (ffs(BD71828_MASK_RUN_LVL_CTRL) - 1);

	return regmap_update_bits(regmap, BD71828_REG_PS_CTRL_3,
				  BD71828_MASK_RUN_LVL_CTRL, reg);
}
/* Get current run level when RUN levels are controlled using I2C */
static int bd71828_dvs_i2c_get_run_level(struct regmap *regmap,
					 struct bd71828_regulator_data *rd)
{
	int ret;
	unsigned int val;

	ret = regmap_read(regmap, BD71828_REG_PS_CTRL_3, &val);
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

	mutex_lock(&data->dvs_lock);
	ret = bd71828_dvs_i2c_get_run_level(rdev->regmap, data);
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

	mutex_lock(&data->dvs_lock);
	ret = bd71828_dvs_gpio_get_run_level(data);
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

	mutex_lock(&data->dvs_lock);
	ret = bd71828_dvs_i2c_get_run_level(rdev->regmap, data);
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

	mutex_lock(&data->dvs_lock);
	ret = bd71828_dvs_gpio_get_run_level(data);
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
 * @regulator:  pointer to regulator for which the run-level voltage is changed
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

	if (rd->gps)
		return bd71828_dvs_gpio_set_run_level(rd, level);

	return bd71828_dvs_i2c_set_run_level(rd->regmap, level);
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

	if (!rd->gps)
		ret = bd71828_dvs_i2c_get_run_level(rd->regmap, rd);
	else
		ret = bd71828_dvs_gpio_get_run_level(rd);

	if (ret < 0)
		return ret;

	*level = (unsigned int) ret;

	return 0;
}
EXPORT_SYMBOL(bd71828_get_runlevel);


static const struct regulator_ops dvs_buck_gpio_ops = {
	.is_enabled = bd71828_dvs_gpio_is_enabled,
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
	.set_ramp_delay = bd71828_set_ramp_delay,
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
		.reg_inits = buck1_inits,
		.reg_init_amnt = ARRAY_SIZE(buck1_inits),
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
		.reg_inits = buck2_inits,
		.reg_init_amnt = ARRAY_SIZE(buck2_inits),
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
		.reg_inits = buck6_inits,
		.reg_init_amnt = ARRAY_SIZE(buck6_inits),
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
		.reg_inits = buck7_inits,
		.reg_init_amnt = ARRAY_SIZE(buck7_inits),
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
			.of_parse_cb = ldo6_parse_dt,
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

struct bd71828_gpio_cfg {
	bool use_gpio;
	unsigned int runlvl;
	struct gpio_descs *gps;
};

static void mark_regulator_runlvl_controlled(struct device *dev,
					     struct device_node *np,
					     struct bd71828_gpio_cfg *g)
{
	int i;

	for (i = 1; i <= ARRAY_SIZE(bd71828_rdata); i++) {
		if (!of_node_name_eq(np, bd71828_rdata[i-1].desc.of_match))
			continue;
		switch (i) {
		case 1:
		case 2:
		case 6:
		case 7:
			g->runlvl |= 1 << (i - 1);
			dev_dbg(dev, "buck %d runlevel controlled\n", i);
			break;
		default:
			dev_err(dev,
				"Only bucks 1,2,6,7 support run-level dvs\n");
			break;
		}
	}
}

static int get_runcontrolled_bucks_dt(struct device *dev,
				      struct bd71828_gpio_cfg *g)
{
	struct device_node *np;
	struct device_node *nproot = dev->of_node;
	const char *prop = "rohm,dvs-runlvl-ctrl";

	g->runlvl = 0;

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	for_each_child_of_node(nproot, np)
		if (of_property_read_bool(np, prop))
			mark_regulator_runlvl_controlled(dev, np, g);

	of_node_put(nproot);
	return 0;
}

static int check_dt_for_gpio_controls(struct device *d,
				      struct bd71828_gpio_cfg *g)
{
	int ret;

	ret = get_runcontrolled_bucks_dt(d, g);
	if (ret)
		return ret;

	g->use_gpio = false;

	/* If the run level control is not requested by any bucks we're done */
	if (!g->runlvl)
		return 0;

	g->gps = devm_gpiod_get_parent_array(d, "rohm,dvs-vsel", GPIOD_OUT_LOW);

	if (IS_ERR(g->gps)) {
		ret = PTR_ERR(g->gps);
		if (ret == -ENOENT)
			return 0;
		return ret;
	}

	if (g->gps->ndescs != 2)
		return -ENOENT;

	g->use_gpio = true;

	return 0;
}

static void set_buck_runlvl_controlled(struct rohm_regmap_dev *bd71828,
				      struct bd71828_regulator_data *rd,
				      struct bd71828_gpio_cfg *g)
{
	switch (rd->desc.id) {
	case BD71828_BUCK1:
		rd->reg_inits = buck1_gpio_inits;
		break;
	case BD71828_BUCK2:
		rd->reg_inits = buck2_gpio_inits;
		break;
	case BD71828_BUCK6:
		rd->reg_inits = buck6_gpio_inits;
		break;
	case BD71828_BUCK7:
		rd->reg_inits = buck7_gpio_inits;
		break;
	default:
		return;
	}

	/*
	 * Disallow setters. Get voltages/enable states based
	 * on current RUN level
	 */
	rd->allow_runlvl = true;

	if (g->use_gpio) {
		rd->gps = g->gps;
		rd->desc.ops = &dvs_buck_gpio_ops;
	} else {
		rd->desc.ops = &dvs_buck_i2c_ops;
	}
	rd->desc.of_parse_cb = buck_set_runlvl_hw_dvs_levels;
}

static int bd71828_probe(struct platform_device *pdev)
{
	struct rohm_regmap_dev *bd71828;
	int i, j, ret;
	struct regulator_config config = {
		.dev = pdev->dev.parent,
	};
	struct bd71828_gpio_cfg gcfg = {0};
	struct bd71828_regulator_data *rd;

	bd71828 = dev_get_drvdata(pdev->dev.parent);
	if (!bd71828) {
		dev_err(&pdev->dev, "No MFD driver data\n");
		return -EINVAL;
	}

	ret = check_dt_for_gpio_controls(pdev->dev.parent, &gcfg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get DVS gpio resources\n");
		return ret;
	}

	/*
	 * Allocate device data to allow controlling more than one PMICs
	 */
	rd = devm_kmalloc_array(&pdev->dev, ARRAY_SIZE(bd71828_rdata),
				sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, rd);

	for (i = 0; i < ARRAY_SIZE(bd71828_rdata); i++) {
		/* Use bd71828_rdata as template */
		rd[i] = bd71828_rdata[i];

		mutex_init(&rd[i].dvs_lock);
		if (gcfg.runlvl & (1 << i))
			set_buck_runlvl_controlled(bd71828, &rd[i], &gcfg);

		rd[i].regmap = bd71828->regmap;
	}

	config.regmap = bd71828->regmap;

	for (i = 0; i < ARRAY_SIZE(bd71828_rdata); i++) {
		struct regulator_dev *rdev;

		config.driver_data = &rd[i];

		rdev = devm_regulator_register(&pdev->dev,
					       &rd[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				rd[i].desc.name);
			return PTR_ERR(rdev);
		}
		for (j = 0; j < rd[i].reg_init_amnt; j++) {
			ret = regmap_update_bits(bd71828->regmap,
						 rd[i].reg_inits[j].reg,
						 rd[i].reg_inits[j].mask,
						 rd[i].reg_inits[j].val);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator %s init failed\n",
					rd[i].desc.name);
				return ret;
			}
		}
	}
	return 0;
}

static struct platform_driver bd71828_regulator = {
	.driver = {
		.name = "bd71828-pmic"
	},
	.probe = bd71828_probe,
};

module_platform_driver(bd71828_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71828 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd71828-pmic");
