// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022 ROHM Semiconductors
// bd96801-regulator.c ROHM BD96801 regulator driver

/*
 * The DS2 sample does not allow controlling much of anything besides the BUCK
 * voltage tune value unless the PMIC is in STANDBY. This means the usual case
 * where PMIC is controlled using processor which is powered by the PMIC does
 * not allow much of control. Eg, enable/disable status or protection limits
 * can't be set.
 *
 * It is however possible that the PMIC is controlled (at least partially) from
 * some supervisor processor which stays alive even when PMIC goes to STANDBY.
 *
 * This calls for following actions:
 *  - add STANDBY check also to protection setting.
 *  - consider whether the ERRB IRQ would be worth handling
 *
 * Right. The demand for keeping processor alive when PMIC is configured has
 * emerged. The DS3 solves problem by allowing SW to specify power-rails which
 * are kept powered when the PMIC goes to STANDBY. Eg, SW can (at least in
 * theory)
 *
 * - Configure critical power-rails to be enabled at STANDBY
 * - Switch PMIC to STANDBY mode
 * - Perform the configuration
 * - Turn the PMIC back to the ACTIVE mode.
 *
 * Toggling the STANDBY mode from a regulator driver does definitely not sound like
 * "the right thing to do(TM)". That should probably be initiated by early boot
 * - or if it is required at later stage, then maybe by a consumer driver /
 *   user-space application.
 *
 * Still, if STANDBY-only configuratins are needed then someone should ensure
 * the STBY request line stays asserted until all the necessary configurations
 * are done. Using an evaluation board this can be done toggling a swicth
 * manually - but for any real use-case we would need a SW control for this.
 * This driver does not in any way ensure the PMIC stays in STANDBY. It only
 * checks if PMIC is in STANDBY when some configuration is started - and warns
 * if the state is not correct.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/mfd/rohm-generic.h>
#include <linux/mfd/rohm-bd96801.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/coupler.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/timer.h>

enum {
	BD96801_BUCK1,
	BD96801_BUCK2,
	BD96801_BUCK3,
	BD96801_BUCK4,
	BD96801_LDO5,
	BD96801_LDO6,
	BD96801_LDO7,
	BD96801_REGULATOR_AMOUNT,
};

enum {
	BD96801_PROT_OVP,
	BD96801_PROT_UVP,
	BD96801_PROT_OCP,
	BD96801_PROT_TEMP,
	BD96801_NUM_PROT,
};

#define BD96801_ALWAYS_ON_REG		0x3c
#define BD96801_REG_ENABLE		0x0b
#define BD96801_BUCK1_EN_MASK		BIT(0)
#define BD96801_BUCK2_EN_MASK		BIT(1)
#define BD96801_BUCK3_EN_MASK		BIT(2)
#define BD96801_BUCK4_EN_MASK		BIT(3)
#define BD96801_LDO5_EN_MASK		BIT(4)
#define BD96801_LDO6_EN_MASK		BIT(5)
#define BD96801_LDO7_EN_MASK		BIT(6)

#define BD96801_BUCK1_VSEL_REG		0x28
#define BD96801_BUCK2_VSEL_REG		0x29
#define BD96801_BUCK3_VSEL_REG		0x2a
#define BD96801_BUCK4_VSEL_REG		0x2b
#define BD96801_LDO5_VSEL_REG		0x25
#define BD96801_LDO6_VSEL_REG		0x26
#define BD96801_LDO7_VSEL_REG		0x27
#define BD96801_BUCK_VSEL_MASK		0x1F
#define BD96801_LDO_VSEL_MASK		0xff

#define BD96801_MASK_RAMP_DELAY		0xc0
#define BD96801_INT_VOUT_BASE_REG	0x21
#define BD96801_BUCK_INT_VOUT_MASK	0xff

#define BD96801_BUCK_VOLTS		256
#define BD96801_LDO_VOLTS		256

#define BD96801_OVP_MASK		0x03
#define BD96801_MASK_BUCK1_OVP_SHIFT	0x00
#define BD96801_MASK_BUCK2_OVP_SHIFT	0x02
#define BD96801_MASK_BUCK3_OVP_SHIFT	0x04
#define BD96801_MASK_BUCK4_OVP_SHIFT	0x06
#define BD96801_MASK_LDO5_OVP_SHIFT	0x00
#define BD96801_MASK_LDO6_OVP_SHIFT	0x02
#define BD96801_MASK_LDO7_OVP_SHIFT	0x04

#define BD96801_PROT_LIMIT_OCP_MIN	0x00
#define BD96801_PROT_LIMIT_LOW		0x01
#define BD96801_PROT_LIMIT_MID		0x02
#define BD96801_PROT_LIMIT_HI		0x03

#define BD96801_REG_BUCK1_OCP		0x32
#define BD96801_REG_BUCK2_OCP		0x32
#define BD96801_REG_BUCK3_OCP		0x33
#define BD96801_REG_BUCK4_OCP		0x33

#define BD96801_MASK_BUCK1_OCP_SHIFT	0x00
#define BD96801_MASK_BUCK2_OCP_SHIFT	0x04
#define BD96801_MASK_BUCK3_OCP_SHIFT	0x00
#define BD96801_MASK_BUCK4_OCP_SHIFT	0x04

#define BD96801_REG_LDO5_OCP		0x34
#define BD96801_REG_LDO6_OCP		0x34
#define BD96801_REG_LDO7_OCP		0x34

#define BD96801_MASK_LDO5_OCP_SHIFT	0x00
#define BD96801_MASK_LDO6_OCP_SHIFT	0x02
#define BD96801_MASK_LDO7_OCP_SHIFT	0x04

#define BD96801_MASK_SHD_INTB		BIT(7)
#define BD96801_INTB_FATAL		BIT(7)

#define BD96801_NUM_REGULATORS		7
#define BD96801_NUM_LDOS		4

/*
 * Ramp rates for bucks are controlled by bits [7:6] as follows:
 * 00 => 1 mV/uS
 * 01 => 5 mV/uS
 * 10 => 10 mV/uS
 * 11 => 20 mV/uS
 */
static const unsigned int buck_ramp_table[] = { 1000, 5000, 10000, 20000 };

/*
 * This is a voltage range that get's appended to selected
 * bd96801_buck_init_volts value. The range from 0x0 to 0xF is actually
 * bd96801_buck_init_volts + 0 ... bd96801_buck_init_volts + 150mV
 * and the range from 0x10 to 0x1f is bd96801_buck_init_volts - 150mV ...
 * bd96801_buck_init_volts - 0. But as the members of linear_range
 * are all unsigned I will apply offset of -150 mV to value in
 * linear_range - which should increase these ranges with
 * 150 mV getting all the values to >= 0.
 */
static const struct linear_range bd96801_tune_volts[] = {
	REGULATOR_LINEAR_RANGE(150000, 0x00, 0xF, 10000),
	REGULATOR_LINEAR_RANGE(0, 0x10, 0x1F, 10000),
};

static const struct linear_range bd96801_buck_init_volts[] = {
	REGULATOR_LINEAR_RANGE(500000 - 150000, 0x00, 0xc8, 5000),
	REGULATOR_LINEAR_RANGE(1550000 - 150000, 0xc9, 0xec, 50000),
	REGULATOR_LINEAR_RANGE(3300000 - 150000, 0xed, 0xff, 0),
};

static const struct linear_range bd96801_ldo_int_volts[] = {
	REGULATOR_LINEAR_RANGE(300000, 0x00, 0x78, 25000),
	REGULATOR_LINEAR_RANGE(3300000, 0x79, 0xff, 0),
};

#define BD96801_LDO_SD_VOLT_MASK	0x1
#define BD96801_LDO_MODE_MASK		0x6
#define BD96801_LDO_MODE_INT		0x0
#define BD96801_LDO_MODE_SD		0x2
#define BD96801_LDO_MODE_DDR		0x4

static int ldo_ddr_volt_table[] = {500000, 300000};
static int ldo_sd_volt_table[] = {3300000, 1800000};

/* Constant IRQ initialization data (templates) */
struct bd96801_irqinfo {
	int type;
	struct regulator_irq_desc irq_desc;
	int err_cfg;
	int wrn_cfg;
	const char *irq_name;
};

#define BD96801_IRQINFO(_type, _name, _irqoff_ms, _irqname)	\
{								\
	.type = (_type),					\
	.err_cfg = -1,						\
	.wrn_cfg = -1,						\
	.irq_name = (_irqname),					\
	.irq_desc = {						\
		.name = (_name),				\
		.irq_off_ms = (_irqoff_ms),			\
		.map_event = regulator_irq_map_event_simple,	\
	},							\
}

static const struct bd96801_irqinfo buck1_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck1-over-curr-h", 500,
			"buck1-overcurr-h"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck1-over-curr-l", 500,
			"buck1-overcurr-l"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck1-over-curr-n", 500,
			"buck1-overcurr-n"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "buck1-over-voltage", 500,
			"buck1-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "buck1-under-voltage", 500,
			"buck1-undervolt"),
	BD96801_IRQINFO(BD96801_PROT_TEMP, "buck1-over-temp", 500,
			"buck1-thermal")
};

static const struct bd96801_irqinfo buck2_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck2-over-curr-h", 500,
			"buck2-overcurr-h"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck2-over-curr-l", 500,
			"buck2-overcurr-l"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck2-over-curr-n", 500,
			"buck2-overcurr-n"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "buck2-over-voltage", 500,
			"buck2-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "buck2-under-voltage", 500,
			"buck2-undervolt"),
	BD96801_IRQINFO(BD96801_PROT_TEMP, "buck2-over-temp", 500,
			"buck2-thermal")
};

static const struct bd96801_irqinfo buck3_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck3-over-curr-h", 500,
			"buck3-overcurr-h"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck3-over-curr-l", 500,
			"buck3-overcurr-l"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck3-over-curr-n", 500,
			"buck3-overcurr-n"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "buck3-over-voltage", 500,
			"buck3-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "buck3-under-voltage", 500,
			"buck3-undervolt"),
	BD96801_IRQINFO(BD96801_PROT_TEMP, "buck3-over-temp", 500,
			"buck3-thermal")
};

static const struct bd96801_irqinfo buck4_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck4-over-curr-h", 500,
			"buck4-overcurr-h"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck4-over-curr-l", 500,
			"buck4-overcurr-l"),
	BD96801_IRQINFO(BD96801_PROT_OCP, "buck4-over-curr-n", 500,
			"buck4-overcurr-n"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "buck4-over-voltage", 500,
			"buck4-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "buck4-under-voltage", 500,
			"buck4-undervolt"),
	BD96801_IRQINFO(BD96801_PROT_TEMP, "buck4-over-temp", 500,
			"buck4-thermal")
};

static const struct bd96801_irqinfo ldo5_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "ldo5-overcurr", 500,
			"ldo5-overcurr"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "ldo5-over-voltage", 500,
			"ldo5-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "ldo5-under-voltage", 500,
			"ldo5-undervolt"),
};

static const struct bd96801_irqinfo ldo6_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "ldo6-overcurr", 500,
			"ldo6-overcurr"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "ldo6-over-voltage", 500,
			"ldo6-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "ldo6-under-voltage", 500,
			"ldo6-undervolt"),
};

static const struct bd96801_irqinfo ldo7_irqinfo[] = {
	BD96801_IRQINFO(BD96801_PROT_OCP, "ldo7-overcurr", 500,
			"ldo7-overcurr"),
	BD96801_IRQINFO(BD96801_PROT_OVP, "ldo7-over-voltage", 500,
			"ldo7-overvolt"),
	BD96801_IRQINFO(BD96801_PROT_UVP, "ldo7-under-voltage", 500,
			"ldo7-undervolt"),
};

struct bd96801_irq_desc {
	struct bd96801_irqinfo *irqinfo;
	int num_irqs;
};

struct bd96801_regulator_data {
	struct regulator_desc desc;
	const struct linear_range *init_ranges;
	int num_ranges;
	struct bd96801_irq_desc irq_desc;
	int initial_voltage;
	int ldo_vol_lvl;
	/* OCP tables are fixed size - four values */
	const int *ocp_table;
	u8 prot_reg_shift;
	u8 ocp_shift;
	u8 ovp_reg;
	u8 ovd_reg;
	u8 ocp_reg;
	int ldo_errs;
};

struct bd96801_pmic_data {
	struct bd96801_regulator_data regulator_data[BD96801_NUM_REGULATORS];
	struct regmap *regmap;
	int fatal_ind;
	int num_regulators;
};

/*
 * Return 0 if limit should be set.
 * Return 1 if limit should not be set but we want to proceed with regulator
 * registration.
 * Return other error to propagate issues to regulator framework.
 */
static int sanity_check_ovd_uvd(struct device *dev, struct bd96801_irqinfo *new,
				struct bd96801_irqinfo *old, int lim_uV,
				int severity, bool enable)
{
	int old_err = 0, old_wrn = 0;
	int *cfg;

	if (!new) {
		dev_warn(dev, "No protection IRQ\n");
		return -EOPNOTSUPP;
	}

	if (severity == REGULATOR_SEVERITY_ERR)
		cfg = &new->err_cfg;
	else
		cfg = &new->wrn_cfg;


	if (!enable) {
		*cfg = 0;

		return 1;
	}

	/* Don't allow overriding ERR with WRN */
	if (severity == REGULATOR_SEVERITY_WARN && new->err_cfg &&
	    new->err_cfg != -1) {
		dev_warn(dev, "Both WARNING and ERROR limits given.\n");
		return 1;
	}

	/*
	 * BD96801 has common limit for OVD and UVD.
	 * See that there is no existing settings
	 * conflicting. Warn if there is.
	 */
	if (old) {
		if (old->err_cfg && old->err_cfg != -1 && old->err_cfg != 1)
			old_err = old->err_cfg;
		if (old->wrn_cfg && old->wrn_cfg != -1 && old->wrn_cfg != 1)
			old_wrn = old->wrn_cfg;

		if (lim_uV && ((old_err && (old_err != lim_uV)) ||
		    (old_wrn && (old_wrn != lim_uV)))) {
			dev_warn(dev, "conflicting OVD and UVD limits given\n");
			/*
			 * If both OVD and UVD is configured, do not
			 * override ERR by WARN and don't increase already set
			 * limit.
			 */
			if (severity == REGULATOR_SEVERITY_WARN) {
				if (old_err || (old_wrn && old_wrn < lim_uV))
					return -1;
			} else {
				/*
				 * We prefer ERROR over WARNING even though it
				 * is likely to relax the limit.
				 */
				if (old_wrn && old_wrn < lim_uV)
					dev_warn(dev,
						 "Increasing warning limit\n");

				/*
				 * If for some reason the existing warning has
				 * strictier limit than our error - then we will
				 * just disable the warning to prevent it being
				 * errorneously sent. We won't leave warning
				 * and not send errors as we expect the errors
				 * to be much more severe.
				 */
				if (old_wrn && old_wrn > lim_uV) {
					dev_warn(dev,
						 "Disabling conflicting warning\n");
					old->wrn_cfg = 0;
				}

				if (old_err && old_err < lim_uV) {
					dev_warn(dev, "Leaving old limit %u\n",
						 old_err);

					return -1;
				}
				dev_warn(dev, "Using new limit %u\n", lim_uV);
			}
		}
	}

	if (lim_uV)
		*cfg = lim_uV;
	else
		*cfg = 1;

	/*
	 * The BD96801 has only one OVD IRQ. We can either use it for
	 * warning or for error - not for both
	 */
	if (new->err_cfg && new->wrn_cfg) {
		dev_warn(dev,
			 "Both WARN and ERROR limit given. Discarding WARN\n");
		new->wrn_cfg = 0;
	}

	return 0;
}

static int set_ovp_limit(struct regulator_dev *rdev, int lim_uV)
{
	int voltage, lim, set_uv, shift;
	struct bd96801_regulator_data *rdata;
	struct bd96801_pmic_data *pdata;
	struct device *dev;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);

	/*
	 * OVP can be configured to be 9%, 15% or 20% of the set voltage.
	 *
	 * Let's compute the OVP based on the current INT_VOUT + Vtune here.
	 *
	 * This is not 100% according to the spec as the (absolute) limit
	 * value in HW will vary depending on the set voltage.
	 *
	 * We could store the desired limit and re-compute the proportional
	 * OVP/UVP values when regulator voltage is adjusted. If we're getting
	 * out of the spec we could then change the setting of limits to ensure
	 * we stay below the absolute limit value given from DT.
	 *
	 * My initial guess on the use-cases is that the INT_VOUT is only set
	 * at boot - and the impact of Vtune is so small we can ignore the
	 * potenrial limit error for now. Let's fix this only if we see actual
	 * problems.
	 */
	voltage = regulator_get_voltage_rdev(rdev);
	if (voltage < 0)
		return voltage;

	set_uv = voltage * 9 / 100;

	if (set_uv > lim_uV) {
		dev_err(dev, "too small OVP limit %d\n",
			lim_uV);
		lim = BD96801_PROT_LIMIT_LOW;
	} else if (voltage * 15 / 100 > lim_uV) {
		lim = BD96801_PROT_LIMIT_LOW;
	} else if (voltage * 20 / 9 > lim_uV) {
		set_uv = voltage * 15 / 100;
		lim = BD96801_PROT_LIMIT_MID;
	} else {
		set_uv = voltage * 20 / 100;
		lim = BD96801_PROT_LIMIT_HI;
	}
	dev_info(dev,
		 "OVP limit %u requested. Setting %u\n",
			 lim_uV, set_uv);

	shift = rdata->prot_reg_shift;

	return regmap_update_bits(pdata->regmap, rdata->ovp_reg,
				  BD96801_OVP_MASK << shift,
				  lim << shift);
}

static int get_ldo_xvd_limits(struct device *dev, struct bd96801_pmic_data *pdata,
			      struct bd96801_regulator_data *rdata,
			      int *lim_uV, int *reg)
{
	int ret, regu_xvd_limits[3];
	int val;

	ret = regmap_read(pdata->regmap, rdata->ldo_vol_lvl, &val);
	if (ret)
		return ret;

	if (val > 15) {
		if (val < 38) {
			regu_xvd_limits[0] = 16000;
			regu_xvd_limits[1] = 30000;
			regu_xvd_limits[2] = 40000;
		} else {
			regu_xvd_limits[0] = 36000;
			regu_xvd_limits[1] = 60000;
			regu_xvd_limits[2] = 80000;
		}
	}

	if (*lim_uV < regu_xvd_limits[0]) {
		dev_warn(dev, "Unsupported LDO UVD limit %d\n", *lim_uV);
		*lim_uV = regu_xvd_limits[0];
		*reg = BD96801_PROT_LIMIT_LOW;
	} else if (*lim_uV < regu_xvd_limits[1]) {
		*lim_uV = regu_xvd_limits[0];
		*reg = BD96801_PROT_LIMIT_LOW;
	} else if (*lim_uV < regu_xvd_limits[2]) {
		*lim_uV = regu_xvd_limits[1];
		*reg = BD96801_PROT_LIMIT_MID;
	} else {
		*lim_uV = regu_xvd_limits[1];
		*reg = BD96801_PROT_LIMIT_HI;
	}
	dev_info(dev, "LDO using xVD limit %u\n", *lim_uV);

	return 0;
}

/*
 * Supported detection limits for BUCKs are absolute values, (9mV not supported
 * after DS2), 15mV and 20mV. The limits for LDOs depend on initial voltage
 * register value. Scale limit to what is supported by HW.
 */
static int get_xvd_limits(struct regulator_dev *rdev, int *lim_uV, int *reg)
{
	struct bd96801_regulator_data *rdata;
	struct bd96801_pmic_data *pdata;
	struct device *dev;

	dev = rdev_get_dev(rdev);
	pdata = rdev_get_drvdata(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);

	dev_dbg(dev, "xVD limit %u requested\n", *lim_uV);

	if (rdata->ldo_vol_lvl)
		return get_ldo_xvd_limits(dev, pdata, rdata, lim_uV, reg);

	/*
	 * After the DS2 IC spec version setting 9 mV limit for bucks has been
	 * marked as 'forbidden'
	 */
	if (*lim_uV < 15000)
		dev_warn(dev, "Unsupported BUCK xVD limit %d\n", *lim_uV);

	if (*lim_uV < 20000) {
		*lim_uV = 15000;
		*reg = BD96801_PROT_LIMIT_MID;
	} else {
		*lim_uV = 20000;
		*reg = BD96801_PROT_LIMIT_HI;
	}

	dev_dbg(dev, "Using xVD limit %u\n", *lim_uV);

	return 0;
}

static inline int bd96801_in_stby(struct regmap *rmap)
{
	int ret, val;

	ret = regmap_read(rmap, BD96801_REG_PMIC_STATE, &val);
	if (ret)
		return ret;

	return (val == BD96801_STATE_STBY);
}

static int bd96801_set_ovp(struct regulator_dev *rdev, int lim_uV, int severity,
			   bool enable)
{
	int shift, stby;
	struct bd96801_pmic_data *pdata;
	struct bd96801_regulator_data *rdata;
	struct bd96801_irq_desc *idesc;
	struct device *dev;
	struct bd96801_irqinfo *ovp_iinfo = NULL;
	struct bd96801_irqinfo *uvp_iinfo = NULL;
	int reg;
	int ret;
	int i;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);
	idesc = &rdata->irq_desc;

	if (!idesc)
		return -EOPNOTSUPP;

	stby = bd96801_in_stby(rdev->regmap);
	if (stby < 0)
		return stby;
	if (!stby)
		dev_warn(dev, "Can't set OVP. PMIC not in STANDBY\n");

	if (severity == REGULATOR_SEVERITY_PROT) {
		if (!enable) {
			dev_err(dev, "Can't disable over voltage protection\n");
			return -EOPNOTSUPP;
		}
		if (!lim_uV)
			return 0;

		return set_ovp_limit(rdev, lim_uV);
	}

	/* See the comment at bd96801_set_uvp() below */
	if (enable && pdata->fatal_ind == 1) {
		dev_err(dev,
			"All errors are fatal. Can't provide notifications\n");
		if (severity == REGULATOR_SEVERITY_WARN)
			return -EINVAL;
	}

	if (lim_uV) {
		ret = get_xvd_limits(rdev, &lim_uV, &reg);
		if (ret)
			return ret;
	}
	for (i = 0; i < idesc->num_irqs; i++) {
		struct bd96801_irqinfo *iinfo = &idesc->irqinfo[i];

		if (iinfo->type == BD96801_PROT_OVP)
			ovp_iinfo = iinfo;

		if (iinfo->type == BD96801_PROT_UVP)
			uvp_iinfo = iinfo;
	}

	ret = sanity_check_ovd_uvd(dev, ovp_iinfo, uvp_iinfo, lim_uV,
				   severity, enable);

	if (ret) {
		if (ret == 1)
			return 0;
		return ret;
	}

	shift = rdata->prot_reg_shift;

	if (enable && lim_uV)
		return regmap_update_bits(pdata->regmap, rdata->ovd_reg,
					  BD96801_OVP_MASK << shift,
					  reg << shift);
	return 0;
}

static int bd96801_set_uvp(struct regulator_dev *rdev, int lim_uV, int severity,
			   bool enable)
{
	int shift, stby;
	struct bd96801_pmic_data *pdata;
	struct bd96801_regulator_data *rdata;
	struct bd96801_irq_desc *idesc;
	struct device *dev;
	struct bd96801_irqinfo *ovp_iinfo = NULL;
	struct bd96801_irqinfo *uvp_iinfo = NULL;
	int reg;
	int ret;
	int i;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);
	idesc = &rdata->irq_desc;

	if (!idesc)
		return -EOPNOTSUPP;

	stby = bd96801_in_stby(rdev->regmap);
	if (stby < 0)
		return stby;
	if (!stby)
		dev_warn(dev, "Can't set UVP. PMIC not in STANDBY\n");

	if (severity == REGULATOR_SEVERITY_PROT) {
		/* There is nothing we can do for UVP protection on BD96801 */
		if (!enable) {
			dev_err(dev, "Can't disable under voltage protection\n");
			return -EOPNOTSUPP;
		}
		if (lim_uV)
			dev_warn(dev,
				 "Can't set under voltage protection limit\n");
		return 0;
	}

	/*
	 * The PMIC provides option to turn all indications fatal. The
	 * OCP (protection) does utilize this. If DT enables OCP then we
	 * can't provide warning/error notifications as these events are also
	 * causing a shutdown. If this is the case we refuse to set the WARN
	 * limit - but allow setting ERROR limit in order to prevent HW damage
	 * if someone trusts on the ERRORs. The protection shutdown won't
	 * inform software so this is not exactly a graceful thing. Thus punt
	 * error log message also for the REGULATOR_SEVERITY_ERR config.
	 */
	if (enable && pdata->fatal_ind == 1) {
		dev_err(dev,
			"All errors are fatal. Can't provide notifications\n");
		if (severity == REGULATOR_SEVERITY_WARN)
			return -EINVAL;
	}

	if (lim_uV) {
		ret = get_xvd_limits(rdev, &lim_uV, &reg);
		if (ret)
			return ret;
	}
	for (i = 0; i < idesc->num_irqs; i++) {
		struct bd96801_irqinfo *iinfo = &idesc->irqinfo[i];

		if (iinfo->type == BD96801_PROT_OVP)
			ovp_iinfo = iinfo;

		if (iinfo->type == BD96801_PROT_UVP)
			uvp_iinfo = iinfo;
	}

	ret = sanity_check_ovd_uvd(dev, uvp_iinfo, ovp_iinfo, lim_uV,
				   severity, enable);

	if (ret) {
		if (ret == 1)
			return 0;
		return ret;
	}

	shift = rdata->prot_reg_shift;

	if (enable && lim_uV)
		return regmap_update_bits(pdata->regmap, rdata->ovd_reg,
					  BD96801_OVP_MASK << shift,
					  reg << shift);
	return 0;
}


/*
 * Driver uses fixed size OCP tables. If new variant with more OCP values is
 * added we need to handle different sizes and selectors
 */

/* 1.5 A ... 3 A step 0.5 A*/
static const int bd96801_buck12_ocp[] = { 1500000, 2000000, 2500000, 3000000 };

/* The extension PMIC supports protections from 3.5 A to 10 A */
static const int bd96802_buck12_ocp[] = { 3500000, 6000000, 7500000, 10000000 };

/* 3 A ... 6 A *step 1 A */
static const int bd96801_buck34_ocp[] = { 3000000, 4000000, 5000000, 6000000 };

/* 400 mA ... 550 mA, step 50 mA*/
static const int bd96801_ldo_ocp[] = { 400000, 450000, 500000, 550000 };

static int __drop_warns(struct bd96801_regulator_data *rdata,
			struct regmap *regmap, struct device *dev,
			struct bd96801_irqinfo *iinfo)
{
	int ret = 0;

	if (!iinfo->wrn_cfg && !iinfo->err_cfg)
		return 0;

	dev_err(dev, "All errors are fatal. Can't provide notifications\n");

	if (iinfo->wrn_cfg) {
		int mask;
		int val;

		if (iinfo->type == BD96801_PROT_OVP ||
		    iinfo->type == BD96801_PROT_UVP) {
			mask = BD96801_OVP_MASK << rdata->prot_reg_shift;
			val = BD96801_PROT_LIMIT_HI << rdata->prot_reg_shift;

			ret = regmap_update_bits(regmap, rdata->ovd_reg,
						 mask, val);
		} else if (iinfo->type == BD96801_PROT_OCP) {
			mask = BD96801_OVP_MASK << rdata->ocp_shift;
			val = BD96801_PROT_LIMIT_HI << rdata->ocp_shift;
			ret = regmap_update_bits(regmap, rdata->ocp_reg,
					 mask, val);
		}
		if (ret)
			return ret;

		iinfo->wrn_cfg = -1;
	}

	return 0;
}

/*
 * When we configure INTB to cause SHDN we will also make _all_ problems fatal.
 * This is likely to cause shutdown at too early phase if WARNs are configured.
 * Thus we reset all already configured WARN limits.
 *
 * NOTE: We don't cancel both WARNs and ERRs. The typical and expected recovery
 * for ERRs is anyways a shutdown (although graceful). We'd better to leave
 * those limits respected even though the shutdown won't be pretty. In most
 * cases this should still be better than allowing things to go off more than
 * ERR boundary.
 *
 * For any already added WARNs we just configure the max limit. (We _could_
 * store the HW default at startup and use it - but let's not overdo this. We
 * do punt out a big red error message - hopefully that is read by board R&D
 * folks and they will review limits and fix the offending DT limits...
 */
static int bd96801_drop_all_warns(struct device *dev,
				  struct bd96801_pmic_data *pdata)
{
	int i, ret;

	for (i = 0; i < BD96801_NUM_REGULATORS; i++) {
		struct bd96801_regulator_data *rdata;

		rdata = &pdata->regulator_data[i];
		for (i = 0; i < rdata->irq_desc.num_irqs; i++) {
			struct bd96801_irqinfo *iinfo;

			iinfo = &rdata->irq_desc.irqinfo[i];
			ret = __drop_warns(rdata, pdata->regmap, dev, iinfo);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int bd96801_set_oc_det(struct device *dev,
			      struct bd96801_pmic_data *pdata,
			      struct bd96801_regulator_data *rdata, bool enable,
			      int severity)
{
	struct bd96801_irqinfo *iinfo;
	bool found = false;
	int i, *cfg;

	if (enable && pdata->fatal_ind == 1) {
		dev_err(dev, "Can't support fatal and non fatal OCP\n");
		return -EINVAL;
	}

	/* Bucks have 3 OCP IRQs. mark them all. */
	for (i = 0; i < rdata->irq_desc.num_irqs; i++) {
		iinfo = &rdata->irq_desc.irqinfo[i];
		if (iinfo->type != BD96801_PROT_OCP)
			continue;

		if (severity == REGULATOR_SEVERITY_WARN) {
			if (enable && iinfo->err_cfg &&
			    iinfo->err_cfg != -1) {
				dev_err(dev,
					"Can't support both OCP WARN and ERR\n");
				return -EINVAL;
			}
			cfg = &iinfo->wrn_cfg;
		} else {
			if (enable && iinfo->err_cfg &&
			    iinfo->err_cfg != -1) {
				/* Print only once for this regulator's OCP */
				if (!found)
					dev_err(dev,
						"Can't support both OCP WARN and ERR\n");
				iinfo->wrn_cfg = 0;
			}
			cfg = &iinfo->err_cfg;
		}
		if (!enable)
			*cfg = 0;
		else
			*cfg = 1;

		found = true;
	}
	if (!found)
		return -EOPNOTSUPP;

	return 0;
}

static int bd96801_set_ocp(struct regulator_dev *rdev, int lim_uA,
				int severity, bool enable)
{
	struct bd96801_pmic_data *pdata;
	struct bd96801_regulator_data *rdata;
	struct device *dev;
	int reg, stby;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);

	/*
	 * Most of the configs can only be done when PMIC is in STANDBY
	 * And yes. This is racy, we don't know when PMIC state is changed.
	 * So we can't promise config works as PMIC state may change right
	 * after this check - but at least we can warn if this attempted
	 * when PMIC isn't in STANDBY.
	 */
	stby = bd96801_in_stby(rdev->regmap);
	if (stby < 0)
		return stby;
	if (!stby)
		dev_warn(dev, "Can't set OCP. PMIC not in STANDBY\n");

	if (severity == REGULATOR_SEVERITY_PROT) {
		if (enable) {
			if (pdata->fatal_ind == 0)
				dev_err(dev, "Conflicting protection settings.\n");

			pdata->fatal_ind = 1;
			bd96801_drop_all_warns(dev, pdata);
		} else {
			if (pdata->fatal_ind == 1) {
				dev_err(dev, "Conflicting protection settings.\n");
				return -EINVAL;
			}
			pdata->fatal_ind = 0;
		}
		if (!lim_uA)
			return 0;
	} else {
		int ret;

		ret = bd96801_set_oc_det(dev, pdata, rdata, enable, severity);
		if (ret)
			return ret;

		if (!enable || !lim_uA)
			return 0;
	}
	/*
	 * zero is valid selector for OCP unlike for OVP/UVP.
	 * We only set the limit for INT OCPH. OCPL OCPN and EXT_OCP limits
	 * are not supported. Those could probably be handled using own vendor
	 * DTS property.
	 */
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
}

#define BD96801_TSD_KELVIN	448
#define BD96801_TW_MIN_KELVIN	404
#define BD96801_TW_MAX_KELVIN	422

static int config_thermal_prot(struct bd96801_pmic_data *pdata,
			       struct device *dev, int lim, bool enable)
{
	if (enable) {
		/*
		 * If limit is not given we assume the protection
		 * refers to the TSD at BD96801_TSD_KELVIN. This
		 * is always enabled so we have a no-op here.
		 *
		 * If limit is given then we try using fatal INTB
		 */
		if (!lim)
			return 0;

		if (pdata->fatal_ind == 0)
			dev_err(dev, "Conflicting protection settings.\n");

		pdata->fatal_ind = 1;
		bd96801_drop_all_warns(dev, pdata);
	} else {
		if (pdata->fatal_ind == 1) {
			dev_err(dev,
				"Conflicting protection settings.\n");
			return -EINVAL;
		}
		pdata->fatal_ind = 0;
	}

	return 0;
}

static int bd96801_ldo_set_tw(struct regulator_dev *rdev, int lim, int severity,
			  bool enable)
{
	struct bd96801_regulator_data *rdata;
	struct bd96801_pmic_data *pdata;
	struct device *dev;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);

	/*
	 * Let's handle the TSD case, After this we can focus on INTB.
	 * See if given limit is the BD96801 TSD. If so, the enable request for
	 * protection is valid (TSD is always enabled and does always forcibly
	 * shut-down the PMIC. All other configurations for this temperature
	 * are unsupported.
	 */
	if (lim == BD96801_TSD_KELVIN) {
		if (severity == REGULATOR_SEVERITY_PROT && enable)
			return 0;

		dev_err(dev, "Unsupported TSD configuration\n");
		return -EINVAL;
	}

	/*
	 * The PMIC provides Thermal warning IRQ with limit that is not
	 * configurable. If protection matching this limit is configured we can
	 * use the INTB IRQ either for HW protection (fatal INTB), or WARN or
	 * ERROR level notifications.
	 */
	if (lim && (lim < BD96801_TW_MIN_KELVIN ||
		      lim > BD96801_TW_MAX_KELVIN)) {
		dev_err(dev, "Unsupported thermal protection limit\n");
		return -EINVAL;
	}

	if (severity == REGULATOR_SEVERITY_PROT)
		return config_thermal_prot(pdata, dev, lim, enable);
	if (!enable)
		return 0;

	if (rdata->ldo_errs) {
		dev_err(dev,
			"Multiple protection notification configs for %s\n",
			rdev->desc->name);
		return -EINVAL;
	}
	if (severity == REGULATOR_SEVERITY_ERR)
		rdata->ldo_errs = REGULATOR_ERROR_OVER_TEMP;
	else
		rdata->ldo_errs = REGULATOR_ERROR_OVER_TEMP_WARN;

	return 0;
}

static int ldo_map_notif(int irq, struct regulator_irq_data *rid,
			 unsigned long *dev_mask)
{
	int i;

	for (i = 0; i < rid->num_states; i++) {
		struct bd96801_regulator_data *rdata;
		struct regulator_dev *rdev;

		rdev = rid->states[i].rdev;
		rdata = container_of(rdev->desc, struct bd96801_regulator_data,
				     desc);
		rid->states[i].notifs = regulator_err2notif(rdata->ldo_errs);
		rid->states[i].errors = rdata->ldo_errs;
		*dev_mask |= BIT(i);
	}
	return 0;
}

/*
 * Data-Sheet states that for BUCKs the IC can monitor driver MOS temperature
 * or each internal power MOSFET temperature. Additionally there seems to be
 * measurement of temperature at the center of the chip. These temperature
 * detections will again generate INTB interrupt - and again, the INTB can
 * be set to shut-down the faulting power-output (or group of outputs). Limit
 * for thermal warning (INTB) is arounf 140 degree C. (Data-sheet says 131 to
 * 149 but I see no configuration for the limit.
 *
 * Let's use the same PROTECTION (fatal INTB behaviour) as we use with the
 * OCP.
 *
 * On top of this it seems the IC has TSD (thermal shut-down). This is not
 * configurable or maskable. The temperature limit for TSD is 175 degree C
 * but the measurement point is not mentioned (any of the measurements
 * exceed this?)
 */
static int bd96801_buck_set_tw(struct regulator_dev *rdev, int lim, int severity,
			       bool enable)
{
	struct bd96801_regulator_data *rdata;
	struct bd96801_pmic_data *pdata;
	struct bd96801_irqinfo *iinfo;
	struct device *dev;
	int i;

	dev = rdev_get_dev(rdev);
	rdata = container_of(rdev->desc, struct bd96801_regulator_data, desc);
	pdata = rdev_get_drvdata(rdev);

	/*
	 * Let's handle the TSD case, After this we can focus on INTB.
	 * See if given limit is the BD96801 TSD. If so, the enable request for
	 * protection is valid (TSD is always enabled and does always forcibly
	 * shut-down the PMIC). All other configurations for this temperature
	 * are unsupported.
	 */
	if (lim == BD96801_TSD_KELVIN) {
		if (severity == REGULATOR_SEVERITY_PROT && enable)
			return 0;

		dev_err(dev, "Unsupported TSD configuration\n");
		return -EINVAL;
	}

	for (i = 0; i < rdata->irq_desc.num_irqs; i++) {
		iinfo = &rdata->irq_desc.irqinfo[i];
		if (iinfo->type == BD96801_PROT_TEMP)
			break;
	}

	if (i == rdata->irq_desc.num_irqs)
		return -EOPNOTSUPP;
	/*
	 * If limit is given, see it is within the detection range mentioned
	 * in BD96801 data-sheet. We can't configure the limit but we can fail
	 * if limit is given and it does not fit in typical thermal warning
	 * detection range.
	 */
	if (lim && (lim < BD96801_TW_MIN_KELVIN ||
		      lim > BD96801_TW_MAX_KELVIN)) {
		dev_err(dev, "Unsupported thermal protection limit %u\n", lim);
		return -EINVAL;
	}

	if (severity == REGULATOR_SEVERITY_PROT)
		return config_thermal_prot(pdata, dev, lim, enable);

	if (pdata->fatal_ind == 1) {
		/*
		 * INTB is set fatal => there will be no warning to consumers.
		 * Let's still not fail the probe as this is not going to fry
		 * the HW - it rather will make us do protection shutdown too
		 * early. So just spill out a warning but let the boot proceed.
		 */
		dev_warn(dev, "INTB set fatal. Notifications not supported\n");
		return 0;
	}

	if (severity == REGULATOR_SEVERITY_ERR)
		iinfo->err_cfg = (enable) ? 1 : 0;
	else
		iinfo->wrn_cfg = (enable) ? 1 : 0;

	if (iinfo->wrn_cfg && iinfo->wrn_cfg != -1 && iinfo->err_cfg &&
	    iinfo->err_cfg != -1)
		dev_warn(dev, "Both temperature WARN and ERR given\n");

	return 0;
}

static int bd96801_list_voltage_lr(struct regulator_dev *rdev,
				   unsigned int selector)
{
	int voltage;
	struct bd96801_regulator_data *data;

	data = container_of(rdev->desc, struct bd96801_regulator_data, desc);

	/*
	 * The BD096801 has voltage setting in two registers. One giving the
	 * "initial voltage" (can be changed only when regulator is disabled.
	 * This driver caches the value and sets it only at startup. The other
	 * register is voltage tuning value which applies -150 mV ... +150 mV
	 * offset to the voltage.
	 *
	 * Note that the cached initial voltage stored in regulator data is
	 * 'scaled down' by the 150 mV so that all of our tuning values are
	 * >= 0. This is done because the linear_ranges uses unsigned values.
	 *
	 * As a result, we increase the tuning voltage which we get based on
	 * the selector by the stored initial_voltage.
	 */
	voltage = regulator_list_voltage_linear_range(rdev, selector);
	if (voltage < 0)
		return voltage;

	return voltage + data->initial_voltage;
}

/*
 * BD96801 does not allow controlling the output enable/disable status
 * unless PMIC is in STANDBY state. So this may be next to useless - unless
 * the PMIC is controlled from processor not powered by the PMIC. AFAIK
 * this really is a potential use-case with the BD96801 - hence these
 * controls are implemented.
 */
static int bd96801_enable_regmap(struct regulator_dev *rdev)
{
	int stby;

	stby = bd96801_in_stby(rdev->regmap);
	if (stby < 0)
		return stby;
	if (!stby)
		return -EBUSY;

	return regulator_enable_regmap(rdev);
}

static int bd96801_disable_regmap(struct regulator_dev *rdev)
{
	int stby;

	stby = bd96801_in_stby(rdev->regmap);
	if (stby < 0)
		return stby;
	if (!stby)
		return -EBUSY;

	return regulator_disable_regmap(rdev);
}

/*
 * Latest data-sheet says LDO voltages can only be changed in STANDBY(?)
 * I think the original limitation was that the LDO must not be enabled
 * when voltage is changed..
 */
static int bd96801_regulator_set_voltage_sel_restricted(struct regulator_dev *rdev,
							unsigned int sel)
{
	int stby;

	stby = bd96801_in_stby(rdev->regmap);
	if (stby)
		return -EBUSY;

	return rohm_regulator_set_voltage_sel_restricted(rdev, sel);
}

static const struct regulator_ops bd96801_ldo_table_ops = {
	.enable = bd96801_enable_regmap,
	.disable = bd96801_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_table,
	.set_voltage_sel = rohm_regulator_set_voltage_sel_restricted,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_over_voltage_protection = bd96801_set_ovp,
	.set_under_voltage_protection = bd96801_set_uvp,
	.set_over_current_protection = bd96801_set_ocp,
	.set_thermal_protection = bd96801_ldo_set_tw,
};

static const struct regulator_ops bd96801_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = bd96801_list_voltage_lr,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
	.set_over_voltage_protection = bd96801_set_ovp,
	.set_under_voltage_protection = bd96801_set_uvp,
	.set_over_current_protection = bd96801_set_ocp,
	.set_thermal_protection = bd96801_buck_set_tw,
};

static const struct regulator_ops bd96801_ldo_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = bd96801_regulator_set_voltage_sel_restricted,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_over_voltage_protection = bd96801_set_ovp,
	.set_under_voltage_protection = bd96801_set_uvp,
	.set_over_current_protection = bd96801_set_ocp,
	.set_thermal_protection = bd96801_ldo_set_tw,
};

static int buck_set_initial_voltage(struct regmap *regmap, struct device *dev,
				    struct bd96801_regulator_data *data,
				    struct device_node *np)
{
	int ret = 0;

	if (data->num_ranges) {
		int sel, val, stby;
		bool found;
		u32 initial_uv;
		int reg = BD96801_INT_VOUT_BASE_REG + data->desc.id;

		/* See if initial value should be configured */
		ret = of_property_read_u32(np, "rohm,initial-voltage-microvolt",
					   &initial_uv);
		if (ret) {
			if (ret == EINVAL)
				goto get_initial;
			return ret;
		}

		/* We can only change initial voltage when PMIC is in STANDBY */
		stby = bd96801_in_stby(regmap);
		if (stby < 0)
			goto get_initial;

		if (!stby) {
			dev_warn(dev,
				 "Can't set initial voltage, PMIC not in STANDBY\n");
			goto get_initial;
		}

		/*
		 * Check if regulator is enabled - if not, then we can change
		 * the initial voltage.
		 */
		ret = regmap_read(regmap, data->desc.enable_reg, &val);
		if (ret)
			return ret;

		if ((val & data->desc.enable_mask) != data->desc.enable_mask) {
			dev_warn(dev,
				 "%s: enabled. Can't set initial voltage\n",
				 data->desc.name);
			goto get_initial;
		}

		dev_dbg(dev, "%s: Setting INITIAL voltage %u\n",
			data->desc.name,  initial_uv);
		/*
		 * The initial uV range is scaled down by 150mV to make all
		 * tuning values positive. Hence we decrease value by 150 mV
		 * so that the set voltage will really match the one requested
		 * via DT.
		 */
		ret = linear_range_get_selector_low_array(data->init_ranges,
							  data->num_ranges,
							  initial_uv - 150000, &sel,
							  &found);
		if (ret) {
			const struct linear_range *lr;
			int max;

			lr = &data->init_ranges[data->num_ranges - 1];
			max = linear_range_get_max_value(lr);

			dev_err(dev, "Unsupported initial voltage %u\n",
				initial_uv);
			dev_err(dev, "%u ranges, [%u .. %u]\n",
				data->num_ranges, data->init_ranges->min, max);
			return ret;
		}

		if (!found)
			dev_warn(dev,
				 "Unsupported initial voltage %u requested, setting lower\n",
				 initial_uv);

		ret = regmap_update_bits(regmap, reg,
					 BD96801_BUCK_INT_VOUT_MASK,
					 sel);
		if (ret)
			return ret;
get_initial:
		ret = regmap_read(regmap, reg, &sel);
		sel &= BD96801_BUCK_INT_VOUT_MASK;

		ret = linear_range_get_value_array(data->init_ranges,
						   data->num_ranges, sel,
						   &initial_uv);
		if (ret)
			return ret;

		data->initial_voltage = initial_uv;
		dev_dbg(dev, "Tune-scaled initial voltage %u\n",
			data->initial_voltage);
	}

	return 0;
}

static int set_ldo_initial_voltage(struct regmap *regmap,
				   struct device *dev,
				   struct bd96801_regulator_data *data,
				   struct device_node *np)
{
	int ret, val, stby;
	u32 initial_uv;
	int cfgreg = 0;
	int mask = BD96801_LDO_SD_VOLT_MASK | BD96801_LDO_MODE_MASK;

	ret = of_property_read_u32(np, "rohm,initial-voltage-microvolt",
				   &initial_uv);
	if (ret) {
		if (ret == EINVAL)
			goto get_initial;
		return ret;
	}

	/* We can only change initial voltage when PMIC is in STANDBY */
	stby = bd96801_in_stby(regmap);
	if (stby < 0)
		return stby;

	if (!stby) {
		dev_warn(dev, "Can't set initial voltage, PMIC not in STANDBY\n");
		goto get_initial;
	}

	ret = regmap_read(regmap, data->desc.enable_reg, &val);
	if (ret)
		return ret;

	if ((val & data->desc.enable_mask) != data->desc.enable_mask) {
		dev_warn(dev, "%s: enabled. Can't set initial voltage\n",
			 data->desc.name);
		goto get_initial;
	}

	/* Regulator is Disabled */
	dev_dbg(dev, "%s: Setting INITIAL voltage %u\n", data->desc.name,
		initial_uv);

	/*
	 * Should this be two properties - one for mode (SD/DDR) and
	 * one for voltage (3.3, 1.8, 0.5, 0.3?)
	 */
	switch (initial_uv) {
	case 300000:
		cfgreg |= BD96801_LDO_MODE_DDR;
		cfgreg |= 1;
		break;
	case 500000:
		cfgreg |= BD96801_LDO_MODE_DDR;
		break;
	case 1800000:
		cfgreg |= BD96801_LDO_MODE_SD;
		cfgreg |= 1;
		break;
	case 3300000:
		cfgreg |= BD96801_LDO_MODE_SD;
		break;
	default:
		dev_err(dev, "unsupported initial voltage for LDO\n");
		return -EINVAL;
	}
	ret = regmap_update_bits(regmap, data->ldo_vol_lvl, mask,
				 cfgreg);
	if (ret)
		return ret;

get_initial:
	if (!cfgreg) {
		ret = regmap_read(regmap, data->ldo_vol_lvl, &cfgreg);
		if (ret)
			return ret;
	}
	switch (cfgreg & BD96801_LDO_MODE_MASK) {
	case BD96801_LDO_MODE_DDR:
		data->desc.volt_table = ldo_ddr_volt_table;
		data->desc.n_voltages = ARRAY_SIZE(ldo_ddr_volt_table);
		break;
	case BD96801_LDO_MODE_SD:
		data->desc.volt_table = ldo_sd_volt_table;
		data->desc.n_voltages = ARRAY_SIZE(ldo_sd_volt_table);
		break;
	default:
		dev_info(dev, "Leaving LDO to normal mode");
		return 0;
	}

	/* SD or DDR mode => override default ops */
	data->desc.ops = &bd96801_ldo_table_ops,
	data->desc.vsel_mask = 1;
	data->desc.vsel_reg = data->ldo_vol_lvl;

	return 0;
}

static int set_initial_voltage(struct device *dev, struct regmap *regmap,
			struct bd96801_regulator_data *data,
			struct device_node *np)
{
	/* BUCK */
	if (data->desc.id <= BD96801_BUCK4)
		return buck_set_initial_voltage(regmap, dev, data, np);

	/* LDO */
	return set_ldo_initial_voltage(regmap, dev, data, np);
}

static int bd96801_walk_regulator_dt(struct device *dev, struct regmap *regmap,
				     struct bd96801_regulator_data *data,
				     int num)
{
	int i, ret;
	struct device_node *np;
	struct device_node *nproot = dev->parent->of_node;

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	for_each_child_of_node(nproot, np)
		for (i = 0; i < num; i++) {
			if (!of_node_name_eq(np, data[i].desc.of_match))
				continue;
			ret = set_initial_voltage(dev, regmap, &data[i], np);
			if (ret) {
				dev_err(dev,
					"Initializing voltages for %s failed\n",
					data[i].desc.name);
				of_node_put(np);
				of_node_put(nproot);

				return ret;
			}
			if (of_property_read_bool(np, "rohm,keep-on-stby")) {
				ret = regmap_set_bits(regmap,
						      BD96801_ALWAYS_ON_REG,
						      1 << data[i].desc.id);
				if (ret) {
					dev_err(dev,
						"failed to set %s on-at-stby\n",
						data[i].desc.name);
					of_node_put(np);
					of_node_put(nproot);

					return ret;
				}
			}
		}
	of_node_put(nproot);

	return 0;
}

/*
 * Template for regulator data. Probe will allocate dynamic / driver instance
 * struct so we should be on a safe side even if there were multiple PMICs to
 * control. Note that there is a plan to allow multiple PMICs to be used so
 * systems can scale better. I am however still slightly unsure how the
 * multi-PMIC case will be handled. I don't know if the processor will have I2C
 * acces to all of the PMICs or only the first one. I'd guess there will be
 * access provided to all PMICs for voltage scaling - but the errors will only
 * be informed via the master PMIC. Eg, we should prepare to support multiple
 * driver instances - either with or without the IRQs... Well, let's first
 * just support the simple and clear single-PMIC setup and ponder the multi PMIC
 * case later. What we can easly do for preparing is to not use static global
 * data for regulators though.
 */
static const struct bd96801_pmic_data bd96802_data = {
	.regulator_data = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK1,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK1_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK1_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK1_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck1_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck1_irqinfo),
		},
		.prot_reg_shift = BD96801_MASK_BUCK1_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96802_buck12_ocp,
		.ocp_reg = BD96801_REG_BUCK1_OCP,
		.ocp_shift = BD96801_MASK_BUCK1_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK2,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK2_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK2_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK2_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck2_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck2_irqinfo),
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.prot_reg_shift = BD96801_MASK_BUCK2_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96802_buck12_ocp,
		.ocp_reg = BD96801_REG_BUCK2_OCP,
		.ocp_shift = BD96801_MASK_BUCK2_OCP_SHIFT,
	},
	},
	.fatal_ind = -1,
	.num_regulators = 2,
};

static const struct bd96801_pmic_data bd96801_data = {
	.regulator_data = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK1,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK1_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK1_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK1_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck1_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck1_irqinfo),
		},
		.prot_reg_shift = BD96801_MASK_BUCK1_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96801_buck12_ocp,
		.ocp_reg = BD96801_REG_BUCK1_OCP,
		.ocp_shift = BD96801_MASK_BUCK1_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK2,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK2_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK2_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK2_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck2_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck2_irqinfo),
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.prot_reg_shift = BD96801_MASK_BUCK2_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96801_buck12_ocp,
		.ocp_reg = BD96801_REG_BUCK2_OCP,
		.ocp_shift = BD96801_MASK_BUCK2_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("BUCK3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK3,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK3_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK3_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK3_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck3_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck3_irqinfo),
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.prot_reg_shift = BD96801_MASK_BUCK3_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96801_buck34_ocp,
		.ocp_reg = BD96801_REG_BUCK3_OCP,
		.ocp_shift = BD96801_MASK_BUCK3_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("BUCK4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_BUCK4,
			.ops = &bd96801_buck_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_tune_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_tune_volts),
			.n_voltages = BD96801_BUCK_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_BUCK4_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_BUCK4_VSEL_REG,
			.vsel_mask = BD96801_BUCK_VSEL_MASK,
			.ramp_reg = BD96801_BUCK4_VSEL_REG,
			.ramp_mask = BD96801_MASK_RAMP_DELAY,
			.ramp_delay_table = &buck_ramp_table[0],
			.n_ramp_values = ARRAY_SIZE(buck_ramp_table),
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&buck4_irqinfo[0],
			.num_irqs = ARRAY_SIZE(buck4_irqinfo),
		},
		.init_ranges = bd96801_buck_init_volts,
		.num_ranges = ARRAY_SIZE(bd96801_buck_init_volts),
		.prot_reg_shift = BD96801_MASK_BUCK4_OVP_SHIFT,
		.ovp_reg = BD96801_REG_BUCK_OVP,
		.ovd_reg = BD96801_REG_BUCK_OVD,
		.ocp_table = bd96801_buck34_ocp,
		.ocp_reg = BD96801_REG_BUCK4_OCP,
		.ocp_shift = BD96801_MASK_BUCK4_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "ldo5",
			.of_match = of_match_ptr("LDO5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_LDO5,
			.ops = &bd96801_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_ldo_int_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_ldo_int_volts),
			.n_voltages = BD96801_LDO_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_LDO5_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_LDO5_VSEL_REG,
			.vsel_mask = BD96801_LDO_VSEL_MASK,
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&ldo5_irqinfo[0],
			.num_irqs = ARRAY_SIZE(ldo5_irqinfo),
		},
		.ldo_vol_lvl = BD96801_LDO5_VOL_LVL_REG,
		.prot_reg_shift = BD96801_MASK_LDO5_OVP_SHIFT,
		.ovp_reg = BD96801_REG_LDO_OVP,
		.ovd_reg = BD96801_REG_LDO_OVD,
		.ocp_table = bd96801_ldo_ocp,
		.ocp_reg = BD96801_REG_LDO5_OCP,
		.ocp_shift = BD96801_MASK_LDO5_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "ldo6",
			.of_match = of_match_ptr("LDO6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_LDO6,
			.ops = &bd96801_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_ldo_int_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_ldo_int_volts),
			.n_voltages = BD96801_LDO_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_LDO6_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_LDO6_VSEL_REG,
			.vsel_mask = BD96801_LDO_VSEL_MASK,
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&ldo6_irqinfo[0],
			.num_irqs = ARRAY_SIZE(ldo6_irqinfo),
		},
		.ldo_vol_lvl = BD96801_LDO6_VOL_LVL_REG,
		.prot_reg_shift = BD96801_MASK_LDO6_OVP_SHIFT,
		.ovp_reg = BD96801_REG_LDO_OVP,
		.ovd_reg = BD96801_REG_LDO_OVD,
		.ocp_table = bd96801_ldo_ocp,
		.ocp_reg = BD96801_REG_LDO6_OCP,
		.ocp_shift = BD96801_MASK_LDO6_OCP_SHIFT,
	},
	{
		.desc = {
			.name = "ldo7",
			.of_match = of_match_ptr("LDO7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD96801_LDO7,
			.ops = &bd96801_ldo_ops,
			.type = REGULATOR_VOLTAGE,
			.linear_ranges = bd96801_ldo_int_volts,
			.n_linear_ranges = ARRAY_SIZE(bd96801_ldo_int_volts),
			.n_voltages = BD96801_LDO_VOLTS,
			.enable_reg = BD96801_REG_ENABLE,
			.enable_mask = BD96801_LDO7_EN_MASK,
			.enable_is_inverted = true,
			.vsel_reg = BD96801_LDO7_VSEL_REG,
			.vsel_mask = BD96801_LDO_VSEL_MASK,
			.owner = THIS_MODULE,
		},
		.irq_desc = {
			.irqinfo = (struct bd96801_irqinfo *)&ldo7_irqinfo[0],
			.num_irqs = ARRAY_SIZE(ldo7_irqinfo),
		},
		.ldo_vol_lvl = BD96801_LDO7_VOL_LVL_REG,
		.prot_reg_shift = BD96801_MASK_LDO7_OVP_SHIFT,
		.ovp_reg = BD96801_REG_LDO_OVP,
		.ovd_reg = BD96801_REG_LDO_OVD,
		.ocp_table = bd96801_ldo_ocp,
		.ocp_reg = BD96801_REG_LDO7_OCP,
		.ocp_shift = BD96801_MASK_LDO7_OCP_SHIFT,
	},
	},
	.fatal_ind = -1,
	.num_regulators = 7,
};

static int initialize_pmic_data(struct platform_device *pdev,
				struct bd96801_pmic_data *pdata)
{
	struct bd96801_pmic_data *pdata_template;
	struct device *dev = &pdev->dev;
	int r, i;

	pdata_template = (struct bd96801_pmic_data *)platform_get_device_id(pdev)->driver_data;
	if (!pdata_template)
		return -ENODEV;

	*pdata = *pdata_template;

	/*
	 * Allocate and initialize IRQ data for all of the regulators. We
	 * wish to modify IRQ information independently for each driver
	 * instance.
	 */
	for (r = 0; r < pdata->num_regulators; r++) {
		const struct bd96801_irqinfo *template;
		struct bd96801_irqinfo *new;
		int num_infos;

		template = pdata->regulator_data[r].irq_desc.irqinfo;
		num_infos = pdata->regulator_data[r].irq_desc.num_irqs;

		new = devm_kzalloc(dev, num_infos * sizeof(*new), GFP_KERNEL);
		if (!new)
			return -ENOMEM;

		pdata->regulator_data[r].irq_desc.irqinfo = new;

		for (i = 0; i < num_infos; i++)
			new[i] = template[i];
	}

	return 0;
}

static int bd96801_map_event_all(int irq, struct regulator_irq_data *rid,
			  unsigned long *dev_mask)
{
	int i;

	for (i = 0; i < rid->num_states; i++) {
		rid->states[i].notifs = REGULATOR_EVENT_FAIL;
		rid->states[i].errors = REGULATOR_ERROR_FAIL;
		*dev_mask |= BIT(i);
	}

	return 0;
}

static int bd96801_rdev_errb_irqs(struct platform_device *pdev,
				  struct regulator_dev *rdev)
{
	int i;
	void *retp;
	static const char * const single_out_errb_irqs[] = {
		"%s-pvin-err", "%s-ovp-err", "%s-uvp-err", "%s-shdn-err",
	};

	for (i = 0; i < ARRAY_SIZE(single_out_errb_irqs); i++) {
		char tmp[255];
		int irq;
		struct regulator_irq_desc id = {
			.map_event = bd96801_map_event_all,
			.irq_off_ms = 1000,
		};
		struct regulator_dev *rdev_arr[1] = { rdev };

		snprintf(tmp, 255, single_out_errb_irqs[i], rdev->desc->name);
		tmp[254] = 0;
		id.name = tmp;

		irq = platform_get_irq_byname(pdev, tmp);
		if (irq < 0)
			continue;

		retp = devm_regulator_irq_helper(&pdev->dev, &id, irq, 0,
						 REGULATOR_ERROR_FAIL, NULL,
						 rdev_arr, 1);
		if (IS_ERR(retp))
			return PTR_ERR(retp);

	}
	return 0;
}

static int bd96801_global_errb_irqs(struct platform_device *pdev,
				    struct regulator_dev **rdev, int num_rdev)
{
	int i, num_irqs;
	void *retp;
	static const char * const global_errb_irqs[] = {
		"otp-err", "dbist-err", "eep-err", "abist-err", "prstb-err",
		"drmoserr1", "drmoserr2", "slave-err", "vref-err", "tsd",
		"uvlo-err", "ovlo-err", "osc-err", "pon-err", "poff-err",
		"cmd-shdn-err", "int-shdn-err"
	};

	num_irqs = ARRAY_SIZE(global_errb_irqs);
	for (i = 0; i < num_irqs; i++) {
		int irq;
		struct regulator_irq_desc id = {
			.name = global_errb_irqs[i],
			.map_event = bd96801_map_event_all,
			.irq_off_ms = 1000,
		};

		irq = platform_get_irq_byname(pdev, global_errb_irqs[i]);
		if (irq < 0)
			continue;

		retp = devm_regulator_irq_helper(&pdev->dev, &id, irq, 0,
						 REGULATOR_ERROR_FAIL, NULL,
						  rdev, num_rdev);
		if (IS_ERR(retp))
			return PTR_ERR(retp);
	}

	return 0;
}

static int bd96801_probe(struct platform_device *pdev)
{
	struct device *parent;
	int i, ret, irq;
	void *retp;
	struct regulator_config config = {};
	struct bd96801_regulator_data *rdesc;
	struct bd96801_pmic_data *pdata;
	struct regulator_dev *ldo_errs_rdev_arr[BD96801_NUM_LDOS];
	int ldo_errs_arr[BD96801_NUM_LDOS];
	int temp_notif_ldos = 0;
	struct regulator_dev *all_rdevs[BD96801_NUM_REGULATORS];
	bool use_errb;

	parent = pdev->dev.parent;

	pdata = devm_kzalloc(&pdev->dev, sizeof(bd96801_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	if (initialize_pmic_data(pdev, pdata))
		return -ENOMEM;

	pdata->regmap = dev_get_regmap(parent, NULL);
	if (!pdata->regmap) {
		dev_err(&pdev->dev, "No register map found\n");
		return -ENODEV;
	}

	rdesc = &pdata->regulator_data[0];

	config.driver_data = pdata;
	config.regmap = pdata->regmap;
	config.dev = parent;

	ret = of_property_match_string(pdev->dev.parent->of_node,
				       "interrupt-names", "errb");
	if (ret < 0)
		use_errb = false;
	else
		use_errb = true;

	ret = bd96801_walk_regulator_dt(&pdev->dev, pdata->regmap, rdesc,
					pdata->num_regulators);
	if (ret)
		return ret;

	for (i = 0; i < pdata->num_regulators; i++) {
		struct regulator_dev *rdev;
		struct regulator_dev *rdev_arr[1];
		struct bd96801_irq_desc *idesc = &rdesc[i].irq_desc;
		int j, stby;

		rdev = devm_regulator_register(&pdev->dev,
					       &rdesc[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				rdesc[i].desc.name);
			return PTR_ERR(rdev);
		}

		all_rdevs[i] = rdev;
		if (pdata->fatal_ind) {
			stby = bd96801_in_stby(pdata->regmap);
			if (stby < 0)
				return stby;

			if (!stby)
				dev_warn(&pdev->dev,
					 "PMIC not in STANDBY. Can't change INTB fatality\n");

			/*
			 * This means we may set the INTB fatality many times
			 * but it's better to enable it immediately after a
			 * regulator is enabled to protect early on.
			 */
			ret = regmap_update_bits(pdata->regmap,
						 BD96801_REG_SHD_INTB,
						 BD96801_MASK_SHD_INTB,
						 BD96801_INTB_FATAL);
			if (ret)
				return ret;
		}
		/*
		 * LDOs don't have own temperature monitoring. If temperature
		 * notification was requested for this LDO from DT then we will
		 * add the regulator to be notified if central IC temperature
		 * exceeds threshold.
		 */
		if (rdesc[i].ldo_errs) {
			ldo_errs_rdev_arr[temp_notif_ldos] = rdev;
			ldo_errs_arr[temp_notif_ldos] = rdesc[i].ldo_errs;
			temp_notif_ldos++;
		}
		if (!idesc)
			continue;
		/*
		 * TODO: Can we split adding the INTB notifiers in own
		 * function ?
		 */
		/* Register INTB handlers for configured protections */
		for (j = 0; j < idesc->num_irqs; j++) {
			struct bd96801_irqinfo *iinfo;
			int err = 0;
			int err_flags[] = {
				[BD96801_PROT_OVP] = REGULATOR_ERROR_REGULATION_OUT,
				[BD96801_PROT_UVP] = REGULATOR_ERROR_UNDER_VOLTAGE,
				[BD96801_PROT_OCP] = REGULATOR_ERROR_OVER_CURRENT,
				[BD96801_PROT_TEMP] = REGULATOR_ERROR_OVER_TEMP,

			};
			int wrn_flags[] = {
				[BD96801_PROT_OVP] = REGULATOR_ERROR_OVER_VOLTAGE_WARN,
				[BD96801_PROT_UVP] = REGULATOR_ERROR_UNDER_VOLTAGE_WARN,
				[BD96801_PROT_OCP] = REGULATOR_ERROR_OVER_CURRENT_WARN,
				[BD96801_PROT_TEMP] = REGULATOR_ERROR_OVER_TEMP_WARN,
			};

			iinfo = &idesc->irqinfo[j];
			/*
			 * Don't install IRQ handler if both error and warning
			 * notifications are explicitly disabled
			 */
			if (!iinfo->err_cfg && !iinfo->wrn_cfg)
				continue;

			if (WARN_ON(iinfo->type >= BD96801_NUM_PROT))
				return -EINVAL;

			if (iinfo->err_cfg)
				err = err_flags[iinfo->type];
			else if (iinfo->wrn_cfg)
				err = wrn_flags[iinfo->type];

			iinfo->irq_desc.data = pdata;
			irq = platform_get_irq_byname(pdev, iinfo->irq_name);
			if (irq < 0)
				return irq;
			/* Find notifications for this IRQ (WARN/ERR) */

			rdev_arr[0] = rdev;
			retp = devm_regulator_irq_helper(&pdev->dev,
							 &iinfo->irq_desc, irq,
							 0, err, NULL, rdev_arr,
							 1);
			if (IS_ERR(retp))
				return PTR_ERR(retp);
		}
		/* Register per regulator ERRB notifiers */
		if (use_errb) {
			ret = bd96801_rdev_errb_irqs(pdev, rdev);
			if (ret)
				return ret;
		}
	}
	if (temp_notif_ldos) {
		int irq;
		struct regulator_irq_desc tw_desc = {
			.name = "core-thermal",
			.irq_off_ms = 500,
			.map_event = ldo_map_notif,
		};

		irq = platform_get_irq_byname(pdev, "core-thermal");
		if (irq < 0)
			return irq;

		retp = devm_regulator_irq_helper(&pdev->dev, &tw_desc, irq, 0,
						 0, &ldo_errs_arr[0],
						 &ldo_errs_rdev_arr[0],
						 temp_notif_ldos);
		if (IS_ERR(retp))
			return PTR_ERR(retp);
	}

	if (use_errb)
		return bd96801_global_errb_irqs(pdev, all_rdevs,
						pdata->num_regulators);

	return 0;
}

static const struct platform_device_id bd96801_pmic_id[] = {
	{ "bd96801-pmic", (kernel_ulong_t)&bd96801_data },
	{ "bd96802-pmic", (kernel_ulong_t)&bd96802_data },
	{ },
};
MODULE_DEVICE_TABLE(platform, bd96801_pmic_id);

static struct platform_driver bd96801_regulator = {
	.driver = {
		.name = "bd96801-regulator"
	},
	.probe = bd96801_probe,
	.id_table = bd96801_pmic_id,
};

module_platform_driver(bd96801_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD96801 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd96801-pmic");
