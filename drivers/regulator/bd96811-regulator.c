// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 ROHM Semiconductors

#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <linux/mfd/rohm-bd96811.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define BD96811_NUM_VOUTS 5
#define BD96811_REG_VOUT1_INITIAL_VOLT 0x0a

#define BD96811_VOUT12_VSEL_REG 0x1b
#define BD96811_VOUT34_VSEL_REG 0x1c
#define BD96811_VOUT13_VSEL_MASK GENMASK(3, 0)
#define BD96811_VOUT24_VSEL_MASK GENMASK(7, 4)

/*
 * The LDO 5 output voltage depends on 'LDO mode' and VSEL.
 *
 * Mode can be 'SD mode, LPDDR5_VTT mode or the default mode. This
 * is selected by bits [2:1].
 * VSEL is bit[0].
 *
 * In default mode '00', the VSEL bit does not matter. Output voltage is
 * what is indicated by the LDO5_VOL register which is set by OTP.
 *
 * In SD mode '01':
 * VSEL 0	1
 * 	3.3V	1.8V
 *
 * In LPDDR5_VTT mode '10':
 * VSEL	0	1
 * 	0.5V	0.3V
 *
 * We can combine this to a table*
 * 000 and 001 => LDO5_VOL,
 * 010 => 3.3 V
 * 011 => 1.8 V
 * 100 => 0.5 V
 * 101 => 0.3 V
 */
#define BD96811_LDO5_VSEL_REG 0x10
static const int bd96811_ldo5_vol_template[] = {0, 0, 3300000, 1800000, 500000,
						300000 };

#define BD96811_REG_RAMP	0x0f
#define BD96811_VOUT1_RAMP_MASK	GENMASK(1,0)
#define BD96811_VOUT2_RAMP_MASK	GENMASK(3,2)
#define BD96811_VOUT3_RAMP_MASK	GENMASK(5,4)
#define BD96811_VOUT4_RAMP_MASK	GENMASK(7,6)

static unsigned int vout_ramp_table[] = {900, 4500, 9000};

	enum {
		BD96811_VOUT1,
		BD96811_VOUT2,
		BD96811_VOUT3,
		BD96811_VOUT4,
		BD96811_VOUT5,
	};

/*
 * Initial voltage register (can be only set via OTP - Eg, RO reg for us):
 * Vout 2 can be either in BUCK or BOOST mode.
 * Vout 3 iand 4 can be either in BUCK or LDO mode.
 *
 * BUCK:
 * 0x00 - 0 mV
 * 0x01 to 0xd8	=> 500 mV to 2,650 mV (10 mV step)
 * 0xD9 to 0xff =>  2,675 mV to 3,625 mV (25 mV step)
 *
 * BOOST:
 * 0x00 - 0 mV
 * 0x1 to 0xd8 => 1,000 mV to 5,300 mV (20 mV step)
 * 0xd9 to 0xe6 => 5,350 mV to 6,000 mV (50 mV step)
 * 0xe7 to 0xff => 6,000 mV
 *
 * LDO:
 * ??
 */
static const struct linear_range bd96801_buck_ldo_init_volts[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x01, 0xd8, 10000),
	REGULATOR_LINEAR_RANGE(26750000, 0xd9, 0xff, 25000),
	REGULATOR_LINEAR_RANGE(3300000, 0xed, 0xff, 0),
};

static const struct linear_range bd96801_boost_init_volts[] = {
	REGULATOR_LINEAR_RANGE(1000000, 0x01, 0xd8, 20000),
	REGULATOR_LINEAR_RANGE(5350000, 0xd9, 0xe6, 50000),
	REGULATOR_LINEAR_RANGE(6000000, 0xe7, 0xff, 0),
};
enum {
	ROHM_REGULATOR_TYPE_DEFAULT,
	ROHM_REGULATOR_TYPE_BUCK,
	ROHM_REGULATOR_TYPE_BOOST,
	ROHM_REGULATOR_TYPE_LDO,
};

struct bd96811_pmic_data {
	struct regmap *regmap;
	struct device *dev;
	int hw_uvd_lim[BD96811_NUM_VOUTS];
	int hw_ovd_lim[BD96811_NUM_VOUTS];
	int ovd_uvd_reg[BD96811_NUM_VOUTS];
	struct regulator_desc desc[BD96811_NUM_VOUTS];
	int vout_table[BD96811_NUM_VOUTS][0xf];
	int vout_type[BD96811_NUM_VOUTS];
	int protections[BD96811_NUM_VOUTS];
	int fatal_int;
};

static int bd96811_is_enabled(struct regulator_dev *rdev)
{
	struct bd96811_pmic_data  *pd = rdev_get_drvdata(rdev);
	int ret, val, reg;

	reg = BD96811_REG_VOUT1_INITIAL_VOLT + rdev->desc->id;

	ret = regmap_read(pd->regmap, reg, &val);
	if (ret)
		return ret;

	return val;
}

static int bd96811_set_tw(struct regulator_dev *rdev, int lim, int severity,
			  bool enable)
{
	return -EINVAL;
}

static int bd96811_set_ocp(struct regulator_dev *rdev, int lim_uA,
				int severity, bool enable)
{
	struct bd96811_pmic_data  *pd = rdev_get_drvdata(rdev);

	/*
	 * OCP is only supported when Vout is in BUCK mode. The mode is set by
	 * OTP - so in practice, if the user does not have the Vout in BUCK
	 * mode then his IC does not support the OCP (even though another
	 * model of BD96811 might.
	 */
	if (pd->vout_type[rdev->desc->id] != ROHM_REGULATOR_TYPE_BUCK) {
		dev_err(pd->dev, "OCP not supported\n");

		return -EINVAL;
	}

	if (severity == REGULATOR_SEVERITY_PROT) {
/*
		if (enable) {
			if (pd->fatal_ind == 0)
				dev_err(dev, "Conflicting protection settings.\n");

			pd->fatal_ind = 1;
			bd96801_drop_all_warns(dev, pd);
		} else {
			if (pd->fatal_ind == 1) {
				dev_err(dev, "Conflicting protection settings.\n");
				return -EINVAL;
			}
			pd->fatal_ind = 0;
		}
		if (!lim_uA)
			return 0;
	} else {

		int ret;

		ret = bd96801_set_oc_det(dev, pd, rdata, enable, severity);
		if (ret)
			return ret;

		if (!enable || !lim_uA)
			return 0;
*/
	}
	/*
	 * zero is valid selector for OCP unlike for OVP/UVP.
	 * We only set the limit for INT OCPH. OCPL OCPN and EXT_OCP limits
	 * are not supported. Those could probably be handled using own vendor
	 * DTS property.
	 */
/*
	if (lim_uA > rdata->ocp_table[BD96801_PROT_LIMIT_MID]) {
		reg = BD96801_PROT_LIMIT_HI;
	} else if (lim_uA > rdata->ocp_table[BD96801_PROT_LIMIT_LOW]) {
		reg = BD96801_PROT_LIMIT_MID;
	} else if (lim_uA > rdata->ocp_table[BD96801_PROT_LIMIT_OCP_MIN]) {
		reg = BD96801_PROT_LIMIT_LOW;
	} else {
		if (lim_uA < rdata->ocp_table[BD96801_PROT_LIMIT_OCP_MIN])
			dev_warn(dev, "Can't support OCP %u, set %u\n",
					lim_uA,
					rdata->ocp_table[BD96801_PROT_LIMIT_OCP_MIN]);
		reg = 0;
	}

	return regmap_update_bits(pdata->regmap, rdata->ocp_reg,
					 BD96801_OVP_MASK << rdata->ocp_shift,
					 reg << rdata->ocp_shift);
*/
	return -EINVAL;

}

static int bd96811_get_init_vol(struct bd96811_pmic_data *pd, int id, int *vol)
{
	const struct linear_range *r;
	int ret, sel, reg, num_ranges, type;

	reg = BD96811_REG_VOUT1_INITIAL_VOLT + id;
	type = pd->vout_type[id];

	ret = regmap_read(pd->regmap, reg, &sel);
	if (ret)
		return ret;

	if (type != ROHM_REGULATOR_TYPE_BOOST) {
		r = bd96801_buck_ldo_init_volts;
		num_ranges = ARRAY_SIZE(bd96801_buck_ldo_init_volts);
	} else {
		r = bd96801_boost_init_volts;
		num_ranges = ARRAY_SIZE(bd96801_boost_init_volts);
	}

	return linear_range_get_value_array(r, num_ranges, sel, vol);
}

static int bd96811_get_uvp_hw_limit(struct bd96811_pmic_data  *pd, int id,
				    int *ini_vol, int *lim)
{
	int ret;

	ret = bd96811_get_init_vol(pd, id, ini_vol);
	if (ret)
		return ret;

	if (pd->vout_type[id] != ROHM_REGULATOR_TYPE_BOOST) {
		/*
		 * The data sheet says the per IC variation for UVP is very
		 * large... We use MAX value here.
		 */
		if (*ini_vol >= 1000000)
			*lim = *ini_vol - (*ini_vol * 75 / 1000 + 1000000);
		else
			*lim = *ini_vol * 15 / 100;

	} else {
		*lim = *ini_vol * 18 / 100;
	}

	return 0;
}

static int bd96811_get_ovp_hw_limit(struct bd96811_pmic_data  *pd, int id,
				    int *ini_vol, int *lim)
{
	int ret;

	ret = bd96811_get_init_vol(pd, id, ini_vol);
	if (ret)
		return ret;

	/* We use the max values of OVP from the data-sheet. */
	if (pd->vout_type[id] != ROHM_REGULATOR_TYPE_BOOST &&
	    *ini_vol >= 2500000)
		*lim = *ini_vol * 15 / 100;
	else
		*lim = *ini_vol * 18 / 100;

	return 0;
}

#define BD96811_REG_OVD_UVD1234 0x12
#define BD96811_MASK_OVD_UVD1 GENMASK(1, 0)
#define BD96811_MASK_OVD_UVD2 GENMASK(3, 2)
#define BD96811_MASK_OVD_UVD3 GENMASK(5, 4)
#define BD96811_MASK_OVD_UVD4 GENMASK(7, 6)
#define BD96811_REG_LDO5_CTRL 0x10
#define BD96811_MASK_LDO5_OVD_UVD GENMASK(5, 4)


/*
 * The base UVD / OVD limit is set via OTP. It can be decreased by INI_VOL / 64,
 * used as such or increased by INI_VOL / 64 or INI_VOL / 32.
 *
 * Select the biggest safety limit which is lower than or equal to the requested
 * limit. We wan't to keep the safety settings _at least_ as restrictive as
 * requested.
 */
static void bd96811_find_limit_sel(struct device *dev, int hw_lim_base,
				   int ini_vol, int target_lim, int *new_lim,
				   int *sel)
{
	if (target_lim >= hw_lim_base + ini_vol / 32) {
		*sel = 3;
		*new_lim = hw_lim_base + ini_vol / 32;
	} else if (target_lim >= hw_lim_base + ini_vol / 64) {
		*sel = 2;
		*new_lim = hw_lim_base + ini_vol / 64;
	} else if (target_lim >= hw_lim_base) {
		*sel = 0;
		*new_lim = hw_lim_base;
	} else {
		*new_lim = hw_lim_base - ini_vol / 64;
		*sel = 1;
		if (*new_lim > target_lim) {
			dev_warn(dev, "Can't support UVD limit %u, using %u\n",
				 target_lim, *new_lim);
		}
	}
}

static int bd96811_write_xvd_field(struct bd96811_pmic_data  *pd, int id,
				   int sel)
{
	int reg, mask, field;

	switch (id) {
	case BD96811_VOUT1:
		reg = BD96811_REG_OVD_UVD1234;
		field = FIELD_PREP(BD96811_MASK_OVD_UVD1, sel);
		mask = BD96811_MASK_OVD_UVD1;
		break;
	case BD96811_VOUT2:
		reg = BD96811_REG_OVD_UVD1234;
		field = FIELD_PREP(BD96811_MASK_OVD_UVD2, sel);
		mask = BD96811_MASK_OVD_UVD2;
		break;
	case BD96811_VOUT3:
		reg = BD96811_REG_OVD_UVD1234;
		field = FIELD_PREP(BD96811_MASK_OVD_UVD3, sel);
		mask = BD96811_MASK_OVD_UVD3;
		break;
	case BD96811_VOUT4:
		reg = BD96811_REG_OVD_UVD1234;
		field = FIELD_PREP(BD96811_MASK_OVD_UVD4, sel);
		mask = BD96811_MASK_OVD_UVD4;
		break;
	case BD96811_VOUT5:
		reg = BD96811_REG_LDO5_CTRL;
		field = FIELD_PREP(BD96811_MASK_LDO5_OVD_UVD, sel);
		mask = BD96811_MASK_LDO5_OVD_UVD;
		break;
	default:
		return -EINVAL;
		break;
	}

	return regmap_update_bits(pd->regmap, reg, mask, field);
}

static bool bd96811_ovd_uvd_conflict(struct bd96811_pmic_data  *pd, int id,
				     int sel)
{
	/*
	 * OVD and UVD setting is common. Detect case where both OVD and UVD
	 * are tried to be set in a way that the settings conflict
	 */
	if (pd->ovd_uvd_reg[id] != -1 && pd->ovd_uvd_reg[id] != sel) {
		dev_err(pd->dev, "Conflicting UVD / OVD settings\n");
		return true;
	}
	pd->ovd_uvd_reg[id] = sel;

	return false;
}

static int bd96811_update_uvd_reg_field(struct bd96811_pmic_data  *pd, int id,
				        int ini_vol, int target_lim)
{
	int sel, new_lim;

	if (!pd->hw_uvd_lim[id]) {
		dev_err(pd->dev, "OTP set UVD limit not known\n");
		return -EINVAL;
	}

	bd96811_find_limit_sel(pd->dev, pd->hw_uvd_lim[id], ini_vol, target_lim,
			       &new_lim, &sel);

	if (bd96811_ovd_uvd_conflict(pd, id, sel))
		return -EINVAL;

	dev_dbg(pd->dev, "vout%u  UVD limit: req %u, set %u\n",
		id + 1, target_lim, new_lim);


	return bd96811_write_xvd_field(pd, id, sel);
}

static int bd96811_update_ovd_reg_field(struct bd96811_pmic_data  *pd, int id,
				        int ini_vol, int target_lim)
{
	int sel, new_lim;

	if (!pd->hw_ovd_lim[id]) {
		dev_err(pd->dev, "OTP set OVD limit not known\n");
		return -EINVAL;
	}

	bd96811_find_limit_sel(pd->dev, pd->hw_ovd_lim[id], ini_vol, target_lim,
			       &new_lim, &sel);

	dev_dbg(pd->dev, "vout%u  OVD limit: req %u, set %u\n",
		id + 1, target_lim, new_lim);

	if (bd96811_ovd_uvd_conflict(pd, id, sel))
		return -EINVAL;

	return bd96811_write_xvd_field(pd, id, sel);
}

static int bd96811_set_uvp(struct regulator_dev *rdev, int lim_uV, int severity,
			   bool enable)
{
	struct bd96811_pmic_data  *pd = rdev_get_drvdata(rdev);
	int hw_prot_lim, id;
	int ret, ini_vol;

	if (severity == REGULATOR_SEVERITY_PROT) {
		/* UVP can't be disabled */
		if (!enable)
			return -EINVAL;
		if (!lim_uV)
			return 0;

		id = rdev->desc->id;

		/*
		 * There is unconditional UVP / OVP protection done by HW.
		 * See if that is sufficiently strict to meet the requested
		 * limit. If it is, then we're done.
		 */
		ret = bd96811_get_uvp_hw_limit(pd, id, &ini_vol, &hw_prot_lim);
		if (ret)
			return ret;

		if (lim_uV >= hw_prot_lim)
			return 0;

		/*
		 * The BD96811 has an option to make all detection level IRQs
		 * ito shut donw the power outputs. This basically changes the
		 * OVD/UVD to OVP/UVP. (Also, thermal warning will become a
		 * protection as well as over-current detection).
		 *
		 * If the fatality was enabled from device-tree, then we can
		 * support some different limits for OVP / UVP using the OVD
		 * / UVD limits.
		 */
		if (!pd->fatal_int) {
			dev_err(pd->dev, "Unsupported protection limit %u\n",
				lim_uV);
			return -EINVAL;
		}

		return bd96811_update_uvd_reg_field(pd, id, ini_vol, lim_uV);
	}

	/* If error detections are fatal, then we can't support UVD */
	if (pd->fatal_int) {
		dev_err(pd->dev, "Detections set fatal\n");
			return -EINVAL;
	}

	ret = bd96811_get_init_vol(pd, id, &ini_vol);
	if (ret)
		return ret;

	return bd96811_update_uvd_reg_field(pd, id, ini_vol, lim_uV);
}

static int bd96811_set_ovp(struct regulator_dev *rdev, int lim_uV, int severity,
			   bool enable)
{
	struct bd96811_pmic_data  *pd = rdev_get_drvdata(rdev);
	int hw_prot_lim, id;
	int ret, ini_vol;

	if (severity == REGULATOR_SEVERITY_PROT) {
		/* UVP can't be disabled */
		if (!enable)
			return -EINVAL;
		if (!lim_uV)
			return 0;

		id = rdev->desc->id;

		/*
		 * There is unconditional UVP / OVP protection done by HW.
		 * See if that is sufficiently strict to meet the requested
		 * limit. If it is, then we're done.
		 */
		ret = bd96811_get_ovp_hw_limit(pd, id, &ini_vol, &hw_prot_lim);
		if (ret)
			return ret;

		if (lim_uV >= hw_prot_lim)
			return 0;

		/*
		 * The BD96811 has an option to make all detection level IRQs
		 * ito shut donw the power outputs. This basically changes the
		 * OVD/UVD to OVP/UVP. (Also, thermal warning will become a
		 * protection as well as over-current detection).
		 *
		 * If the fatality was enabled from device-tree, then we can
		 * support some different limits for OVP / UVP using the OVD
		 * / UVD limits.
		 */
		if (!pd->fatal_int) {
			dev_err(pd->dev, "Unsupported protection limit %u\n",
				lim_uV);
			return -EINVAL;
		}

		return bd96811_update_ovd_reg_field(pd, id, ini_vol, lim_uV);
	}

	/* If error detections are fatal, then we can't support UVD */
	if (pd->fatal_int) {
		dev_err(pd->dev, "Detections set fatal\n");
			return -EINVAL;
	}

	ret = bd96811_get_init_vol(pd, id, &ini_vol);
	if (ret)
		return ret;

	return bd96811_update_ovd_reg_field(pd, id, ini_vol, lim_uV);
	return -EINVAL;
}

static const struct regulator_ops bd96811_ops = {
	.is_enabled = bd96811_is_enabled,
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
	.set_over_voltage_protection = bd96811_set_ovp,
	.set_under_voltage_protection = bd96811_set_uvp,
	.set_over_current_protection = bd96811_set_ocp,
	.set_thermal_protection = bd96811_set_tw,
};

static int bd96811_set_generic_items(struct regulator_desc *d, int id)
{
	static const struct regulator_desc desc_template[] = {
	{
		.name = "vout1",
		.of_match = of_match_ptr("vout1"),
		.regulators_node = of_match_ptr("regulators"),
		.id = BD96811_VOUT1,
		.ops = &bd96811_ops,
		.type = REGULATOR_VOLTAGE,
		.vsel_reg = BD96811_VOUT12_VSEL_REG,
		.vsel_mask = BD96811_VOUT13_VSEL_MASK,
		.ramp_reg = BD96811_REG_RAMP,
		.ramp_mask = BD96811_VOUT1_RAMP_MASK,
		.ramp_delay_table = &vout_ramp_table[0],
		.n_ramp_values = ARRAY_SIZE(vout_ramp_table),
		.owner = THIS_MODULE,
	},
	{
		.name = "vout2",
		.of_match = of_match_ptr("vout2"),
		.regulators_node = of_match_ptr("regulators"),
		.id = BD96811_VOUT2,
		.ops = &bd96811_ops,
		.type = REGULATOR_VOLTAGE,
		.vsel_reg = BD96811_VOUT12_VSEL_REG,
		.vsel_mask = BD96811_VOUT24_VSEL_MASK,
		.ramp_reg = BD96811_REG_RAMP,
		.ramp_mask = BD96811_VOUT2_RAMP_MASK,
		.ramp_delay_table = &vout_ramp_table[0],
		.n_ramp_values = ARRAY_SIZE(vout_ramp_table),
		.owner = THIS_MODULE,
	},
	{
		.name = "vout3",
		.of_match = of_match_ptr("vout3"),
		.regulators_node = of_match_ptr("regulators"),
		.id = BD96811_VOUT3,
		.ops = &bd96811_ops,
		.type = REGULATOR_VOLTAGE,
		.vsel_reg = BD96811_VOUT34_VSEL_REG,
		.vsel_mask = BD96811_VOUT13_VSEL_MASK,
		.ramp_reg = BD96811_REG_RAMP,
		.ramp_mask = BD96811_VOUT1_RAMP_MASK,
		.ramp_delay_table = &vout_ramp_table[0],
		.n_ramp_values = ARRAY_SIZE(vout_ramp_table),
		.owner = THIS_MODULE,
	},
	{
		.name = "vout4",
		.of_match = of_match_ptr("vout4"),
		.regulators_node = of_match_ptr("regulators"),
		.id = BD96811_VOUT4,
		.ops = &bd96811_ops,
		.type = REGULATOR_VOLTAGE,
		.vsel_reg = BD96811_VOUT34_VSEL_REG,
		.vsel_mask = BD96811_VOUT24_VSEL_MASK,
		.ramp_reg = BD96811_REG_RAMP,
		.ramp_mask = BD96811_VOUT1_RAMP_MASK,
		.ramp_delay_table = &vout_ramp_table[0],
		.n_ramp_values = ARRAY_SIZE(vout_ramp_table),
		.owner = THIS_MODULE,
	},
	{
		.name = "vout5",
		.of_match = of_match_ptr("vout5"),
		.regulators_node = of_match_ptr("regulators"),
		.id = BD96811_VOUT5,
		.ops = &bd96811_ops,
		.type = REGULATOR_VOLTAGE,
		.ramp_reg = BD96811_REG_RAMP,
		.ramp_mask = BD96811_VOUT1_RAMP_MASK,
		.ramp_delay_table = &vout_ramp_table[0],
		.n_ramp_values = ARRAY_SIZE(vout_ramp_table),
		.owner = THIS_MODULE,
	},
	};

	/*
	 * This means someone has added more nodes in this driver code and
	 * forgot to update the template here. It's better to catch it
	 * immediately here than let such code be further developed.
	 */
	 if (WARN_ON(ARRAY_SIZE(desc_template) <= id))
		return -EINVAL;

	*d = desc_template[id];

	return 0;
}

static int bd96811_initialize_tune_voltages_ldo5(struct bd96811_pmic_data *pd,
						 struct regulator_desc *d,
						 int init_vol)
{
	int i, *volts;

	volts = devm_kzalloc(pd->dev, sizeof(bd96811_ldo5_vol_template),
			     GFP_KERNEL);
	if (!volts)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(bd96811_ldo5_vol_template); i++)
		volts[i] = bd96811_ldo5_vol_template[i];

	volts[0] = volts[1] = init_vol;

	d->volt_table = volts;
	d->n_voltages = ARRAY_SIZE(bd96811_ldo5_vol_template);

	return 0;
}

/*
 * Voltage can be 'tuned' (Eg. set) by SW using tuning register. The voltage
 * will be the initial voltage increased / decreased by the percentage shown
 * in table below.
 *
 * Voltage tuning from the VOUTx_VOL voltage setting
 * Also, the UVD, OVD, UVP, and OVP threshold are shifted.
 * 0x0 +0.00 %
 * 0x1 +1.56 %
 * 0x2 +3.13 %
 * 0x3 +4.69 %
 * 0x4 +6.25 %
 * 0x5 +7.81 %
 * 0x6 +9.38 %
 * 0x7 +10.94 %
 * 0x8 -10.94 %
 * 0x9 -9.38 %
 * 0xA -7.81 %
 * 0xB -6.25 %
 * 0xC -4.69 %
 * 0xD -3.13 %
 * 0xE -1.56 %
 * 0xF -0.00 %
 *
 * So, after start-up, read initial voltage and build a voltage table for
 * regulator voltage setting / getting operations by adding the values
 * matching the percentages here.
 */
static int bd96811_initialize_tune_voltages(struct bd96811_pmic_data *pd,
				       struct regulator_desc *d, int init_vol)
{
	int i, *volts;
	/* 0,01 percent */
	static const int tuning_factors[] = {0, 156, 313, 469, 625, 781,
					     938, 1094, -1094, -938, -781,
					     -625, -469, -313, -156, 0};

	volts = devm_kzalloc(pd->dev, sizeof(tuning_factors), GFP_KERNEL);
	if (!volts)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(tuning_factors); i++)
		volts[i] = init_vol + init_vol * tuning_factors[i] / 10000;

	d->volt_table = volts;
	d->n_voltages = ARRAY_SIZE(tuning_factors);

	return 0;
}

static int bd96811_desc_populate(struct bd96811_pmic_data *pd,
				 struct device_node *np, int vout_id)
{
	struct regulator_desc *d = &pd->desc[vout_id];
	int  ret, init_vol;
	u32 type = ROHM_REGULATOR_TYPE_DEFAULT;

	ret = bd96811_set_generic_items(d, vout_id);
	if (ret)
		return ret;

	/*
	 * BD96811 regulator type can be BUCK, BOOST or LDO. The type must come
	 * from the DT.
	 */
	ret = of_property_read_u32(np, "rohm,regulator-type", &type);
	if (ret && ret != -EINVAL)
		return ret;

	/*
	 * BD96811 has OVD and UVD limits set by OTP. This limity can then be
	 * somewhat 'tuned' via a tune register but the base must come from DT
	 */
	ret = of_property_read_u32(np, "rohm,uvd-base-microvolt",
				   &pd->hw_uvd_lim[vout_id]);
	if (ret && ret != -EINVAL)
		return ret;

	ret = of_property_read_u32(np, "rohm,ovd-base-microvolt",
				   &pd->hw_ovd_lim[vout_id]);
	if (ret && ret != -EINVAL)
		return ret;

	switch (vout_id) {
	case BD96811_VOUT1:
		if (type != ROHM_REGULATOR_TYPE_BUCK &&
		    type != ROHM_REGULATOR_TYPE_DEFAULT) {
			dev_err(pd->dev, "Vout1 must be BUCK (type %d)\n", type);

			return -EINVAL;
		}
		type = ROHM_REGULATOR_TYPE_BUCK;
		break;
	case BD96811_VOUT2:
		if (type != ROHM_REGULATOR_TYPE_BOOST &&
		    type != ROHM_REGULATOR_TYPE_BUCK) {
			dev_err(pd->dev, "Vout2 must be BUCK/BOOST\n");

			return -EINVAL;
		}
		break;
	case BD96811_VOUT3:
	case BD96811_VOUT4:
		/* Vout 3, 4 is always either BUCK or LDO */
		if (type != ROHM_REGULATOR_TYPE_BUCK &&
		    type != ROHM_REGULATOR_TYPE_LDO) {
			dev_err(pd->dev, "Vout3/4 must be BUCK/LDO (type %d)\n",
				type);

			return -EINVAL;
		}
		break;
	case BD96811_VOUT5:
		/* Vout 5 is always a LDO */
		if (type != ROHM_REGULATOR_TYPE_LDO &&
		    type != ROHM_REGULATOR_TYPE_DEFAULT) {
			dev_err(pd->dev, "Vout5 must be LDO (type %d)\n", type);

			return -EINVAL;
		}
		type = ROHM_REGULATOR_TYPE_LDO;
		break;
	default:
		return -EINVAL;
	}
	/* The type determines support for over-current limit */
	pd->vout_type[vout_id] = type;
//	tune_reg = BD96811_REG_VOUT1_INITIAL_VOLT + vout_id;
	ret = bd96811_get_init_vol(pd, vout_id, &init_vol);
	if (ret)
		return ret;

	if (vout_id == BD96811_VOUT5)
		return bd96811_initialize_tune_voltages_ldo5(pd, d, init_vol);

	return bd96811_initialize_tune_voltages(pd, d, init_vol);
}

static int bd96811_walk_regulator_dt(struct device *dev, struct regmap *regmap,
				     struct bd96811_pmic_data *pd)
{
	int i, ret = -ENODEV;
	const char * const node_names[] = { "vout1", "vout2", "vout3", "vout4", "vout5" };
	struct device_node *np;
	struct device_node *nproot = dev->parent->of_node;

	pd->fatal_int = of_property_read_bool(nproot, "rohm,protect-enable");

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	for_each_child_of_node(nproot, np) {
		for (i = 0; i < ARRAY_SIZE(node_names); i++) {
			if (of_node_name_eq(np, node_names[i])) {
				ret = bd96811_desc_populate(pd, np, i);
				if (ret) {
					dev_err(pd->dev, "bad regulator data\n");
					of_node_put(np);
					break;
				}
			}
		}
	}
	of_node_put(nproot);

	return ret;
}

static int bd96811_probe(struct platform_device *pdev)
{
	struct bd96811_pmic_data *pdata;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i, ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pdata->regmap) {
		dev_err(&pdev->dev, "No register map found\n");
		return -ENODEV;
	}
	pdata->dev = &pdev->dev;

	/*
	 * The OVD and UVD limit settings are combined in same register. We
	 * warn if conflicting OVD and UVD are assigned to a Vout. In order
	 * to know if a conflicting value gets set we initialize this field
	 * to an invalid value to indicate that the limit is not set and
	 * any OVD or UVD is Ok.
	 *
	 * This is actually not ideal as the HW does not really support OVD
	 * and UVD to be independently enabled/disabled, so setting a new limit
	 * to one will also change the other. Can't help it but at least we can
	 * detect conflicting configuration attempts.
	 */
	for (i = 0; i < ARRAY_SIZE(pdata->ovd_uvd_reg); i++)
		pdata->ovd_uvd_reg[i] = -1;

	config.driver_data = pdata;
	config.regmap = pdata->regmap;
	config.dev = pdev->dev.parent;

	ret = bd96811_walk_regulator_dt(&pdev->dev, pdata->regmap, pdata);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(pdata->desc); i++) {
		rdev = devm_regulator_register(&pdev->dev,
					       &pdata->desc[i], &config);
		if (IS_ERR(rdev))
			return dev_err_probe(&pdev->dev, PTR_ERR(rdev),
					     "failed to register %s regulator\n",
					     pdata->desc[i].name);
	}

	return 0;
}

static const struct platform_device_id bd96811_pmic_id[] = {
	{ "bd96811-pmic", },
	{}
};
MODULE_DEVICE_TABLE(platform, bd96811_pmic_id);

static struct platform_driver bd96811_regulator = {
	.driver = {
		.name = "bd96811-regulator"
	},
	.probe = bd96811_probe,
	.id_table = bd96811_pmic_id,
};
module_platform_driver(bd96811_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD96801 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd96811-pmic");
