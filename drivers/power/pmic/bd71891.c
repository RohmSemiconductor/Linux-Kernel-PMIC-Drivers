// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright 2025 ROHM Semiconductors
 *  Matti Vaittinen <mazziesaccount@gmail.com>
 */

#include <command.h>
#include <common.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <log.h>
#include <asm/global_data.h>
#include <dm/read.h>
#include <linux/delay.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/bd71891.h>
#include "bdxxxx.h"

#define PMIC_DT_NAME "pmic@4b"

/* Add debug checks to detect overflows in computations */
#define CHECK_OVERFLOW 1

/*
 * Wait 10 mS before rechecking if state was changed
 * There is 10 attempts and always this delay before re-checking
 */
#define WAIT_FOR_IDLE_CHANGE_US 10000

DECLARE_GLOBAL_DATA_PTR;

static uint g_r_sense;

static inline struct udevice *get_bd71891(void)
{
	return get_currdev(PMIC_DT_NAME);
}

int bd71891_pmic_write(uint reg, const uint8_t *buffer, int len)
{
	struct udevice *pmicdev = get_bd71891();

	if (!pmicdev)
		return -ENODEV;

	return pmic_write(pmicdev, reg, buffer, len);
}

static inline int bd71891_reg_write(uint reg, uint val)
{
	struct udevice *pmicdev = get_bd71891();

	if (!pmicdev)
		return -ENODEV;

	return pmic_reg_write(pmicdev, reg, val);
}

static inline int bd71891_pmic_read(uint reg, uint8_t *buf, int len)
{
	struct udevice *pmicdev = get_bd71891();

	if (!pmicdev)
		return -ENODEV;

	return pmic_read(pmicdev, reg, buf, len);
}

static inline int bd71891_reg_read(uint reg)
{
	struct udevice *pmicdev = get_bd71891();

	if (!pmicdev)
		return -ENODEV;

	return pmic_reg_read(pmicdev, reg);
}

static inline int bd71891_clrsetbits(uint reg, uint clr, uint set)
{
	struct udevice *pmicdev = get_bd71891();

	if (!pmicdev)
		return -ENODEV;

	return pmic_clrsetbits(pmicdev, reg, clr, set);
}

static inline int bd71891_pr_reas_bf(const struct reason_reg *r)
{
	return print_reason_bf_reg(PMIC_DT_NAME, r);
}

static inline int bd71891_pr_reas_field(const struct reason_reg_field *f)
{
	return print_reason_field_reg(PMIC_DT_NAME, f);
}

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

/* Commands */

const struct reason_info bd71891_boot_reas_info[] = {
	REASON_INFO("Cold reset", BIT(7)),
	REASON_INFO("Emergency recovery", BIT(6)),
	REASON_INFO("GATE_DRV open", BIT(2)),
	REASON_INFO("PMIC cold boot", BIT(1)),
	REASON_INFO("USB characterization completed", BIT(0)),
};

const struct reason_info bd71891_reset_reas_info_0[] = {
	REASON_INFO("External dev asserted FLT_B", BIT(7)),
	REASON_INFO("Voltage fault", BIT(6)),
	REASON_INFO("Bad VSYS Voltage", BIT(4)),
	REASON_INFO("Thermal Shuitdown", BIT(3)),
	REASON_INFO("PMIC_EN pin toggles", BIT(2)),
	REASON_INFO("SWRESET", BIT(1)),
	REASON_INFO("Watchdog", BIT(0)),
};

const struct reason_info bd71891_reset_reas_info_1[] = {
	REASON_INFO("LDO2 Power Failure", BIT(6)),
	REASON_INFO("LDO1 Power Failure", BIT(5)),
	REASON_INFO("BOOST Power Failure", BIT(4)),
	REASON_INFO("BUCK4 Power Failure", BIT(3)),
	REASON_INFO("BUCK3 Power Failure", BIT(2)),
	REASON_INFO("BUCK2 Power Failure", BIT(1)),
	REASON_INFO("BUCK1 Power Failure", BIT(0)),
};

const struct reason_info bd71891_reset_reas_info_2[] = {
	REASON_INFO("HDMI internal V3P3 VRFLT_RST", BIT(5)),
	REASON_INFO("BOOST HOCP", BIT(4)),
	REASON_INFO("BUCK4 OCP", BIT(3)),
	REASON_INFO("BUCK3 OCP", BIT(2)),
	REASON_INFO("BUCK2 OCP", BIT(1)),
	REASON_INFO("BUCK1 OCP", BIT(0)),
};

const struct reason_info bd71891_power_state_info[] = {
	REASON_INFO("IC Shutdown", 0),
	REASON_INFO("Emergency", 1),
	REASON_INFO("DEAD", 2),
	REASON_INFO("OTP LOAD", 3),
	REASON_INFO("HIBERNATE", 4),
	REASON_INFO("RUN", 5),
	REASON_INFO("IDLE", 6),
	REASON_INFO("USB characterization", 7),
};

const struct reason_reg bd71891_boot_reas = {
	.explanation = "BOOT FLAGS",
	.reasons = &bd71891_boot_reas_info[0],
	.num_reasons = ARRAY_SIZE(bd71891_boot_reas_info),
	.reg = BD71891_REG_BOOTSRC,
};

const struct reason_reg bd71891_reset_reas[] =
{
	{
		.explanation = "Reset reason flag 1 (RESETSRC1_FLG 0x08)",
		.reasons = &bd71891_reset_reas_info_0[0],
		.num_reasons = ARRAY_SIZE(bd71891_reset_reas_info_0),
		.reg = BD71891_REG_RESETSRC1,
	}, {
		.explanation = "Reset reason flag 2 (RESETSRC2_FLG 0x09)",
		.reasons = &bd71891_reset_reas_info_1[0],
		.num_reasons = ARRAY_SIZE(bd71891_reset_reas_info_1),
		.reg = BD71891_REG_RESETSRC2,
	}, {
		.explanation = "Reset reason flag 3 (RESETSRC3_FLG 0x0a)",
		.reasons = &bd71891_reset_reas_info_2[0],
		.num_reasons = ARRAY_SIZE(bd71891_reset_reas_info_2),
		.reg = BD71891_REG_RESETSRC3,
	},
};

const struct reason_reg_field bd71891_power_state = {
	.reason_reg = {
		.explanation = "PMIC state",
		.reasons = &bd71891_power_state_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_power_state_info),
		.reg = BD71891_REG_POWER_STATE,
	},
	.mask = 0x3,
};

const struct reason_info bd71891_adc_source_info[] = {
	REASON_INFO("Voltage", 0),
	REASON_INFO("Current", 1),
	REASON_INFO("Power", 2),
};

const struct reason_reg_field bd71891_adc_source = {
	.reason_reg = {
		.explanation = "ADC ACCUM SOURCE",
		.reasons = &bd71891_adc_source_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_adc_source_info),
		.reg = BD71891_REG_ADC_CTRL1,
	},
	.mask = BD71891_MASK_ADC_ACCUM_SRC,
};

static const struct reason_info bd71891_usb_char_cur_lim_info[] = {
	REASON_INFO("600", 0),
	REASON_INFO("700", 1),
	REASON_INFO("800", 2),
	REASON_INFO("900", 3),
	REASON_INFO("1000", 4),
	REASON_INFO("1100", 5),
	REASON_INFO("1200", 6),
	REASON_INFO("1300", 7),
};
static const struct reason_reg_field bd71891_usb_char_cur_lim = {
	.reason_reg = {
		.explanation = "USB charazterization current limit",
		.reasons = &bd71891_usb_char_cur_lim_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_cur_lim_info),
		.reg = BD71891_REG_USB_CHAR_CFG1,
	},
	.mask = BD71891_MASK_USB_CHAR_CURR_LIM,
};

static const struct reason_info bd71891_usb_char_itvl_info[] = {
	REASON_INFO("1", 0),
	REASON_INFO("4", 1),
	REASON_INFO("10", 2),
	REASON_INFO("20", 3),
};
static const struct reason_reg_field bd71891_usb_char_itvl = {
	.reason_reg = {
		.explanation = "USB charazterization, interval between two sinking current pulse",
		.reasons = &bd71891_usb_char_itvl_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_itvl_info),
		.reg = BD71891_REG_USB_CHAR_CFG1,
	},
	.mask = BD71891_MASK_USB_CHAR_ITVL,
};

static const struct reason_info bd71891_usb_char_pulse_w_info[] = {
	REASON_INFO("1", 0),
	REASON_INFO("2", 1),
	REASON_INFO("5", 2),
	REASON_INFO("10", 3),
};
static const struct reason_reg_field bd71891_usb_char_pulse_w = {
	.reason_reg = {
		.explanation = "USB charazterization, sinking current pulse width",
		.reasons = &bd71891_usb_char_pulse_w_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_pulse_w_info),
		.reg = BD71891_REG_USB_CHAR_CFG1,
	},
	.mask = BD71891_MASK_USB_CHAR_ITVL,
};

static const struct reason_info bd71891_usb_char_cur_avg_info[] = {
	REASON_INFO("16", 0),
	REASON_INFO("32", 1),
};
static const struct reason_reg_field bd71891_usb_char_cur_avg = {
	.reason_reg = {
		.explanation = "USB characterizaton, number of results to average",
		.reasons = &bd71891_usb_char_cur_avg_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_cur_avg_info),
		.reg = BD71891_REG_USB_CHAR_CFG2,
	},
	.mask = BD71891_MASK_USB_CHAR_CUR_AVG,
};

static const struct reason_info bd71891_usb_char_rsens_info[] = {
	REASON_INFO("10", 0),
	REASON_INFO("20", 1),
	REASON_INFO("30", 2),
};
static const struct reason_reg_field bd71891_usb_char_rsens = {
	.reason_reg = {
		.explanation = "USB characterizaton, sense resistor value mOhm",
		.reasons = &bd71891_usb_char_rsens_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_rsens_info),
		.reg = BD71891_REG_USB_CHAR_CFG2,
	},
	.mask = BD71891_MASK_USB_CHAR_RSENS,
};

static const struct reason_info bd71891_usb_char_v_start_info[] = {
	REASON_INFO("4000", 0),
	REASON_INFO("4250", 1),
	REASON_INFO("4500", 2),
	REASON_INFO("4750", 3),
};
static const struct reason_reg_field bd71891_usb_char_v_start = {
	.reason_reg = {
		.explanation = "USB characterizaton, start voltage mV",
		.reasons = &bd71891_usb_char_v_start_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_v_start_info),
		.reg = BD71891_REG_USB_CHAR_CFG2,
	},
	.mask = BD71891_MASK_USB_CHAR_V_START,
};

static const struct reason_info bd71891_usb_char_uvp_info[] = {
	REASON_INFO("3600", 0),
	REASON_INFO("4000", 1),
	REASON_INFO("4200", 2),
	REASON_INFO("4500", 3),
};
static const struct reason_reg_field bd71891_usb_char_uvp = {
	.reason_reg = {
		.explanation = "USB characterizaton, under voltage limit mV",
		.reasons = &bd71891_usb_char_uvp_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_uvp_info),
		.reg = BD71891_REG_USB_CHAR_CFG2,
	},
	.mask = BD71891_MASK_USB_CHAR_UVP,
};

static const struct reason_info bd71891_usb_char_vsys_debounce_info[] = {
	REASON_INFO("0", 0),
	REASON_INFO("1", 1),
	REASON_INFO("2", 2),
	REASON_INFO("4", 3),
};
static const struct reason_reg_field bd71891_usb_char_vsys_debounce = {
	.reason_reg = {
		.explanation = "USB characterization Vsys debounce time, uS",
		.reasons = &bd71891_usb_char_vsys_debounce_info[0],
		.num_reasons = ARRAY_SIZE(bd71891_usb_char_vsys_debounce_info),
		.reg = BD71891_REG_USB_CHAR_CFG3,
	},
	.mask = BD71891_MASK_USB_CHAR_VSYS_DEB,
};

/* Get the power-on reason */
static int do_chipinfo(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	int ret, i;

	ret = bd71891_pr_reas_bf(&bd71891_boot_reas);
	if (ret)
		return cmd_failure(ret);

	for (i = 0; i < ARRAY_SIZE(bd71891_reset_reas); i++) {
		ret = bd71891_pr_reas_bf(&bd71891_reset_reas[i]);
		if (ret)
			return cmd_failure(ret);
	}

	ret = bd71891_pr_reas_field(&bd71891_power_state);
	return cmd_ret(ret);
}

static int bd71891_get_power_state(void)
{
	return bd71891_reg_read(BD71891_REG_POWER_STATE);
}

static int toggle_idle_to(bool idle)
{
	int idlebit = idle ? BD71891_MASK_IDLE_MODE : 0;
	int ret, i;

	ret = bd71891_clrsetbits(BD71891_REG_PS_CTRL_1, BD71891_MASK_IDLE_MODE,
				 idlebit);
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_reg_read(BD71891_REG_PS_CTRL_1);
	if (ret < 0)
		return cmd_failure(ret);

	for (i = 0; (is_idle_mode(ret) != idle) && (i < 10); i++) {
		printf("Still in %s ...\n", (idle) ? "idle" : "run");
		udelay(WAIT_FOR_IDLE_CHANGE_US);

		ret = bd71891_reg_read(BD71891_REG_PS_CTRL_1);
		if (ret < 0)
			return cmd_failure(ret);

		if (is_run_mode(ret))
			break;
	}
	if (idle != is_idle_mode(ret))
		return cmd_failure(-ETIMEDOUT);

	return 0;
}

static int set_idle_ctrl_by_hpd(bool en)
{
	return bd71891_clrsetbits(BD71891_REG_HPD, BD71891_HPD_IDLE_EN, (en) ?
				 BD71891_HPD_IDLE_EN : 0);
}

static bool hdmi_hotplug_controls_idle(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_HPD);
	if (ret < 0) {
		printf("Failed to read HPD register\n");

		return true;
	}

	return (ret & BD71891_HPD_IDLE_EN);
}

#define BD71891_HPD_DEB_TIME_3US	(0x0 << 5)
#define BD71891_HPD_DEB_TIME_10US	(0x1 << 5)
#define BD71891_HPD_DEB_TIME_1000US	(0x2 << 5)
#define BD71891_HPD_DEB_TIME_10000US	(0x3 << 5)
#define BD71891_MASK_HPD_DEB_TIME	GENMASK(6, 5)

static int print_hpd_info(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_HPD);
	if (ret < 0)
		return ret;

	printf("HDMI%sdetected\n", (ret & BD71891_HPD_DETECTED) ? " " : " not ");
	printf("HPD %s IDLE\n", (ret & BD71891_HPD_IDLE_EN) ?
	       "controls" : "does not control");

	switch (ret & BD71891_MASK_HPD_DEB_TIME)
	{
	case BD71891_HPD_DEB_TIME_3US:
		printf("HPD debounce time 3 uS\n");
		break;
	case BD71891_HPD_DEB_TIME_10US:
		printf("HPD debounce time 10 uS\n");
		break;
	case BD71891_HPD_DEB_TIME_1000US:
		printf("HPD debounce time 1 mS\n");
		break;
	case BD71891_HPD_DEB_TIME_10000US:
		printf("HPD debounce time 10 mS\n");
		break;
	}

	return 0;
}

static int print_pin(const char *pin, int nmos, int conn_pull, int soc_pull)
{
	static const char * const soc_pull_table[] = {
		"1.0k", "2.2k", "5.0k", "10k" };

	if (soc_pull >= ARRAY_SIZE(soc_pull_table))
		return -EINVAL;

	printf("%s NMOS: %d\n", pin, nmos ? 10 : 30);
	printf("%s PULL connector: %s\n", pin, conn_pull ? "enabled" : "open");
	printf("%s PULL SoC: %s\n", pin, soc_pull_table[soc_pull]);

	return 0;
}

static int print_hpd_pin_info(char * const pin)
{
	unsigned int val;
	int ret;

	ret = bd71891_reg_read(BD71891_REG_HDMI_CFG);
	if (ret < 0)
		return cmd_ret(ret);

	val = ret;

	if (!strcmp(pin, "cec"))
		return print_pin("CEC", val & BD71891_MASK_CEC_NMOS,
			  val & BD71891_MASK_CEC_PULL_CONN,
			  (val & BD71891_MASK_CEC_PULL_SOC) >> 2);
	if (!strcmp(pin, "i2c"))
		return print_pin("I2C", val & BD71891_MASK_I2C_NMOS,
			  val & BD71891_MASK_I2C_PULL_CONN,
			  (val & BD71891_MASK_I2C_PULL_SOC));

	printf("Invalid pin '%s'. Supported cec/i2c\n", pin);

	return cmd_ret(-EINVAL);
}

static int config_pull(int is_i2c, const char *side, const char *val)
{
	int i, mask;
	unsigned int soc_pull_shift = is_i2c ? 0 : 2;
	static const char * const pullvals[] = {
		"1000", "2200", "5000", "10000"
	};

	if (!strcmp(side, "conn")) {
		mask = is_i2c ? BD71891_MASK_I2C_PULL_CONN :
				    BD71891_MASK_CEC_PULL_CONN;
		int ret = -EINVAL;

		if (!strcmp(val, "0"))
			ret = bd71891_clrsetbits(BD71891_REG_HDMI_CFG, 0, mask);
		else if (!strcmp(val, "1750"))
			ret = bd71891_clrsetbits(BD71891_REG_HDMI_CFG, mask, 0);
		else
			printf("Invalid pull value '%s' for connector side. Supported values 0, 1750\n", val);

		return cmd_ret(ret);
	}
	if (strcmp(side, "soc")) {
		printf("Invalid pull-up location %s. Supported 'conn' and 'soc'\n", side);
		return CMD_RET_USAGE;
	}

	for (i = 0; i < ARRAY_SIZE(pullvals); i++)
		if (!strcmp(val, pullvals[i]))
			break;

	if (i == ARRAY_SIZE(pullvals)) {
		printf("Invalid pull value %s for SOC side. Supported 1000, 2200, 5000, 10000\n", val);

		return CMD_RET_USAGE;
	}
	mask = is_i2c ? BD71891_MASK_I2C_PULL_SOC :
			BD71891_MASK_CEC_PULL_SOC;

	return bd71891_clrsetbits(BD71891_REG_HDMI_CFG, mask,
				  i >> soc_pull_shift);
}

static int do_hpd_pin_set(int argc, char *const argv[])
{
	const char *pin = argv[1];
	const char *op = argv[2];
	int is_i2c;

	switch (argc)
	{
	case 4:
	{
		const char *val = argv[3];
		int mask, ret;

		if (strcmp(op, "nmos")) {
			printf("Invalid operation '%s' for amount of params. Expected 'nmos'\n", op);
			return CMD_RET_USAGE;
		}
		is_i2c = strcmp(pin, "cec");
		if (is_i2c && strcmp(pin, "i2c")) {
			printf("Invalid pin '%s'. Supported pins 'cec' and 'i2c'\n", pin);
			return CMD_RET_USAGE;
		}

		mask = is_i2c ? BD71891_MASK_I2C_NMOS : BD71891_MASK_CEC_NMOS;

		if (!strcmp(val, "10")) {
			ret = bd71891_clrsetbits(BD71891_REG_HDMI_CFG, 0, mask);
		} else if (!strcmp(val, "30")) {
			ret = bd71891_clrsetbits(BD71891_REG_HDMI_CFG, mask, 0);
		} else {
			printf("Invalid nmos value %s. Supported values 10 and 30\n", val);
			return CMD_RET_USAGE;
		}

		return cmd_ret(ret);
	}
	case 5:
	{
		const char *side = argv[3];
		const char *val = argv[4];

		if (!strcmp(op, "pull")) {
			printf("Invalid operation '%s' expected 'pull'\n", op);
			return CMD_RET_USAGE;
		}

		is_i2c = strcmp(pin, "cec");
		if (is_i2c && strcmp(pin, "i2c")) {
			printf("Invalid pin '%s' expected 'i2c' or 'cec'\n", pin);
			return CMD_RET_USAGE;
		}
		return config_pull(is_i2c, side, val);

	}
	default:
		printf("Invalid arguments\n");
		return CMD_RET_USAGE;
	}
}

static int do_hpd_pin_ctrl(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	int ret;

	if (argc > 2)
		return do_hpd_pin_set(argc, argv);

	if (argc == 2)
		return print_hpd_pin_info(argv[1]);

	ret = print_hpd_info();

	return cmd_ret(ret);
}

static int do_hpd_idle_ctrl(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	int ret;

	if (argc == 1) {
		printf("%d\n", hdmi_hotplug_controls_idle() ? 1 : 0);
		return cmd_ret(0);
	}

	if (argc != 2)
		return CMD_RET_USAGE;

	if (*argv[1] == '1')
		ret = set_idle_ctrl_by_hpd(true);
	else if (*argv[1] == '0')
		ret = set_idle_ctrl_by_hpd(false);
	else
		ret = -EINVAL;

	return cmd_ret(ret);
}

static int do_set_idle_state(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	int curr_state = bd71891_get_power_state();
	char *new_state;
	int ret;

	if (hdmi_hotplug_controls_idle()) {
		printf("IDLE controlled by HDMI hot-plug\n");

		return cmd_failure(-EBUSY);
	}

	if (argc != 2)
		return CMD_RET_USAGE;

	new_state = argv[1];

	if (!strcmp(new_state, "run")) {
		if (!is_idle_state(curr_state)) {
			printf("Not in IDLE, can't switch to RUN (state 0x%x)\n",
			       curr_state);
			return cmd_failure(-EINVAL);
		}

		ret = toggle_idle_to(false);

		return cmd_ret(ret);
	} else if (!strcmp(new_state, "idle")) {
		if (!is_run_state(curr_state)) {
			printf("Not in RUN, can't switch to IDLE (state 0x%x)\n",
			       curr_state);
			return cmd_failure(-EINVAL);
		}

		ret = toggle_idle_to(true);

		return cmd_ret(ret);
	}

	printf("Invalid state '%s' requested (supporting 'run'/'idle')\n",
	       new_state);

	return CMD_RET_USAGE;
}

static int get_adc_accum(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_ADC_ACCUM_KICK);

	if (ret < 0) {
		printf("Could not get ADC ACCUM state\n");

		return 0;
	}

	return ret & BD71891_MASK_ADC_ACCUM_KICK;
}

static int set_adc_accum(bool enable, bool clear)
{
	uint val = 0;
	int ret;

	if (clear)
		val |= BD71891_MASK_ADC_ACCUM_CLR;

	if (enable)
		val |= BD71891_MASK_ADC_ACCUM_KICK;
	else
		val |= BD71891_MASK_ADC_ACCUM_STOP;

	printf("accum en=%d, clear=%d, write reg 0x%x val 0x%x\n", enable, clear,
	       BD71891_REG_ADC_ACCUM_KICK, val);
	ret = bd71891_reg_write(BD71891_REG_ADC_ACCUM_KICK, val);
	if (ret)
		printf("Failed to %s ADC accumulator\n",
		       enable ? "start" : "stop");

	return ret;
}

static int __stop_adc_accum(bool clear)
{
	int ret;

	ret = set_adc_accum(0, clear);

	if (ret)
		return ret;

	/* Ensure ADC is stopped prior returning */
	while (get_adc_accum())
		;

	return ret;
}

static int stop_adc_accum(void)
{
	return __stop_adc_accum(0);
}

static int stop_clear_adc_accum(void)
{
	return __stop_adc_accum(1);
}

static int start_adc_accum(void)
{
	return set_adc_accum(1, 0);
}

static int accum_stopped_config_helper(uint reg, uint mask,
				       uint val)
{
	int started, ret;

	started = get_adc_accum();
	if (started) {
		ret = stop_adc_accum();

		if (ret)
			return ret;
	}

	printf("Updating accum register 0x%x, mask 0x%x, val 0x%x\n",
	       reg, mask, val);

	ret = bd71891_clrsetbits(reg, mask, val);
	if (ret) {
		if (started)
			printf("ADC config failed, ADC stopped\n");

		return ret;
	}

	if (started) {
		ret = start_adc_accum();
		if (ret)
			printf("Could not restart ADC\n");
	}

	return ret;
}

static int do_adc_state(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	char *state;
	int ret;

	if (argc == 1) {
		if (get_adc_accum())
			printf("ADC ACCUM is started\n");
		else
			printf("ADC ACCUM is stopped\n");

		return CMD_RET_SUCCESS;
	}

	if (argc != 2)
		return CMD_RET_USAGE;

	state = argv[1];

	if (!strcmp(state, "start"))
		ret = start_adc_accum();
	else if (!strcmp(state, "stop"))
		ret = stop_adc_accum();
	else
		return CMD_RET_USAGE;

	return cmd_ret(ret);
}

static int __set_adc_source(int src)
{
	int ret;

	if (src < TYPE_VOLTAGE || src > TYPE_POWER) {
		printf("Bad ADC source\n");
		return -EINVAL;
	}

	src <<= BD71891_ADC_ACCUM_SRC_SHIFT;

	ret = accum_stopped_config_helper(BD71891_REG_ADC_CTRL1,
					  BD71891_MASK_ADC_ACCUM_SRC, src);
	if (ret)
		printf("Failed to set ADC source\n");

	return ret;
}

static int do_adc_source(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	char *src;
	int ret, src_no;

	if (argc == 1)
		return bd71891_pr_reas_field(&bd71891_adc_source);

	if (argc != 2)
		return CMD_RET_USAGE;

	src = argv[1];

	if (!strcmp(src, "voltage")) {
		src_no = TYPE_VOLTAGE;
	} else if (!strcmp(src, "current")) {
		src_no = TYPE_CURRENT;
	} else if (!strcmp(src, "power")) {
		src_no = TYPE_POWER;
	} else {
		printf("Unsupported ADC accum source\n");

		return CMD_RET_USAGE;
	}

	ret = __set_adc_source(src_no);
	if (ret)
		return cmd_failure(ret);

	return CMD_RET_SUCCESS;
}

static const long bd71891_adc_gain[] = { 5, 15, 30, 60 };

static int __get_adc_gain_idx(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_ADC_CTRL2);
	if (ret < 0)
		return ret;

	return ((ret & BD71891_MASK_ADC_GAIN) >> BD71891_ADC_GAIN_SHIFT);
}

static int get_adc_gain(void)
{
	int ret;

	ret = __get_adc_gain_idx();
	if (ret > 0)
		printf("ADC gain set to '%ld'\n", bd71891_adc_gain[ret]);

	return cmd_ret(ret);
}

static int do_adc_gain(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	long gain;
	int ret, i;
	char *eptr;

	if (argc == 1)
		return get_adc_gain();

	if (argc != 2)
		return CMD_RET_USAGE;

        gain = simple_strtol(argv[1], &eptr, 10);
        if (!*argv[1] || *eptr)
		goto ret_usage;

	for (i = 0; i < ARRAY_SIZE(bd71891_adc_gain); i++)
		if (bd71891_adc_gain[i] == gain)
			break;

	if (i == ARRAY_SIZE(bd71891_adc_gain))
		goto ret_usage;

	ret = accum_stopped_config_helper(BD71891_REG_ADC_CTRL2,
					  BD71891_MASK_ADC_GAIN,
					  i << BD71891_ADC_GAIN_SHIFT);
	return cmd_ret(ret);

ret_usage:
	printf("Bad gain. Should be one of [5, 15, 30, 60]\n");

	return CMD_RET_USAGE;
}

struct bd71891_adc_vol_src {
	const char * const name;
	const uint reso_uv;
};

#define BD71891_ADC_VOL(_name, _vol) { .name = (_name), .reso_uv = (_vol) }

static const struct bd71891_adc_vol_src adc_vol_sources[] = {
	BD71891_ADC_VOL("vsys", 5860),
	BD71891_ADC_VOL("gatedrv", 1170),
};

static int __get_adc_vol_source(const struct bd71891_adc_vol_src **src)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_ADC_CTRL1);
	if (ret < 0) {
		printf("Could not get ADC source\n");
		return ret;
	}

	ret &= BD71891_MASK_ADC_ACCUM_VOL_SRC;

	*src = &adc_vol_sources[ret];

	return 0;
}

static int get_adc_vol_source(void)
{
	int ret;
	const struct bd71891_adc_vol_src * src;

	ret = __get_adc_vol_source(&src);

	if (!ret)
		printf("ADC Accumulator voltage source set to '%s'\n",
		       src->name);

	return cmd_ret(ret);
}

static int __get_adc_source(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_ADC_CTRL1);
	if (ret < 0)
		return ret;

	ret &= BD71891_MASK_ADC_ACCUM_SRC;
	ret >>= BD71891_ADC_ACCUM_SRC_SHIFT;

	if (ret >= 3) {
		printf("Bad ADC accum source %d\n", ret);

		return -EINVAL;
	}

	return ret;
}

static int do_adc_vol_source(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	char *src;
	int ret, i;

	if (argc == 1)
		return get_adc_vol_source();

	if (argc != 2)
		return CMD_RET_USAGE;

	/* This is actually not compulsory. We could also allow setting the
	 * voltage source before setting the type to voltage. I did this check
	 * purely to help pointing out misconfiguration.
	 */
	if (TYPE_VOLTAGE !=  __get_adc_source()) {
		printf("ADC ACCUM not set to accumulate voltage\n");
		bd71891_pr_reas_field(&bd71891_adc_source);

		return cmd_failure(-EINVAL);
	}

	src = argv[1];

	for (i = 0; i < ARRAY_SIZE(adc_vol_sources); i++) {
		if (!strcmp(src, adc_vol_sources[i].name))
			break;

	}
	if (i == ARRAY_SIZE(adc_vol_sources)) {
		printf("Unsupported ADC accum voltage source\n");

		return CMD_RET_USAGE;
	}

	ret = accum_stopped_config_helper(BD71891_REG_ADC_CTRL1,
					  BD71891_MASK_ADC_ACCUM_VOL_SRC, i);

	return cmd_ret(ret);
}

static int get_operation_type(char *arg)
{
	if (!arg)
		return -EINVAL;

	switch(arg[0])
	{
	case 'v':
		return TYPE_VOLTAGE;
	case 'i':
		return TYPE_CURRENT;
	case 'p':
		return TYPE_POWER;
	case 't':
		return TYPE_TEMPERATURE;
	}

	printf("Invalid operation\n");
	return -EINVAL;
}

static int do_dt_init(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice *ud = get_bd71891();
	int ret;

	if (!ud)
		return cmd_ret(-ENODEV);

	ret =  dev_read_u32u(ud, "rohm,sense-resistor-mohms", &g_r_sense);
	if (!ret)
		printf("R_sense set to %u milli ohms\n", g_r_sense);
	else
		printf("No sense resistor value found\n");

	return 0;
}

static int scale_adc_volt(uint64_t orig, uint64_t *scaled)
{
	const struct bd71891_adc_vol_src *src;
	int ret;

	ret = __get_adc_vol_source(&src);
	if (ret)
		return ret;

	//printf("Scaling %llu to uV. Source %s\n", orig, src->name);

	/* Scale */
	*scaled = orig * src->reso_uv;

#ifdef CHECK_OVERFLOW
	{
		uint64_t tmp = *scaled;

		do_div(tmp, src->reso_uv);
		if (tmp != orig)
			printf("OVERFLOW, %llu != %llu\n",
			       (unsigned long long)tmp,
			       (unsigned long long)orig);
	}
#endif

	return 0;
}

static int gain_idx_resolution(unsigned int gain_idx)
{
	/* Unit 0.1 uV / register step */
	static const int gain2reso[] = { 2343, 781, 391, 195, };

	if (gain_idx < ARRAY_SIZE(gain2reso))
		return gain2reso[gain_idx];

	return -EINVAL;
}

static int get_adc_reg_reso(void)
{
	int ret;

	ret = __get_adc_gain_idx();
	if (ret < 0)
		return ret;

	ret = gain_idx_resolution(ret);
	if (ret < 0)
		return -EINVAL;

	return ret;
}


/* Scales to milli Amperes */
static int scale_adc_curr(uint64_t orig, uint64_t *scaled)
{
	unsigned reso, rsens;
	uint64_t curr;
	int ret;

	ret = get_adc_reg_reso();
	if (ret < 0)
		return ret;

	reso = (unsigned)ret;

	curr = orig * reso;
#ifdef CHECK_OVERFLOW
	{
		uint64_t tmp = curr;

		do_div(tmp, reso);
		if (tmp != orig)
			printf("scale_adc_curr(): OVERFLOW, %llu != %llu\n",
			       (unsigned long long)tmp,
			       (unsigned long long)orig);
	}
#endif

	/*
	 * V = resolution * reg_val
	 *
	 * V = RI => I = V/R
	 *
	 * Unit of V is 0.1 uV
	 * Unit of R is milli ohm
	 * => Unit of I is 100 uA
	 *
	 * For now I just assume Rsense
	 * will be magnitude of 10 milli-ohm and scale the computation to mA
	 * by multiplying the Rsense with 10 before the division.
	 *
	 * This may be a bad idea as:
	 * a) If Rsense is a lot smaller than the curr, then a loop-based
	 * implementation of do_div() may take plenty of time.
	 *
	 * b) When the current value is close to the Rsense we may get
	 * flooring or significant loss of accuracy.
	 *
	 * TODO:
	 * So, this scaling needs to be fitted according to the expected values,
	 * or a comparison logig for the relative sizes of curr and Rsens need
	 * to be used to select the optimal scale.
	 */
	rsens = g_r_sense * 10;
	do_div(curr, rsens);
	*scaled = curr;

	return 0;
}

static int scale_adc_pow(uint64_t orig, uint64_t *scaled)
{
	const struct bd71891_adc_vol_src *src;
	unsigned int rsens;
	uint64_t reso, power;
	int ret;

/*
 *  And Power is calculated by the following formula: ACC_POW_VAL =
 *  (ADC_VOL_VAL[9:0] * ADC_CUR_VAL[9:0]) / 64
 *
 *  Voltage resolution is in uV
 *
 *  Gain correction is in units of 0.1uV. Rsense is in units of milli ohm.
 *  By multiplying Rsense with 10 the current resolution is in mA.
 *  According to the formula above:
 *  => unit of power is nW / 64.
 */
	ret = __get_adc_vol_source(&src);
	if (ret)
		return ret;

	ret = get_adc_reg_reso();
	if (ret < 0)
		return ret;

	reso = (unsigned)ret;
	reso *= src->reso_uv;

#ifdef CHECK_OVERFLOW
	{
		uint64_t tmp = reso;

		do_div(tmp, src->reso_uv);

		if (tmp != ((unsigned)ret))
			printf("scale_adc_pow(): reso OVERFLOW, %llu != %u\n",
			       tmp, (unsigned)ret);
	}
#endif

	rsens = g_r_sense * 10;

	/* Can we overflow here? */
	power = orig * reso * 64;
#ifdef CHECK_OVERFLOW
	{
		uint64_t tmp = power;

		do_div(tmp, src->reso_uv);
		do_div(tmp, ret);
		do_div(tmp, 64);

		if (tmp != orig)
			printf("scale_adc_pow(): pow OVERFLOW, %llu != %llu\n",
			       tmp, orig);
	}
#endif

	do_div(power, rsens);
	*scaled = power;

	return 0;
}

/* Convert register value to
 * V => uV
 * I => uA
 * P => uW
 * t => mC
 *
 * TODO: Check these
 */
static int adc_scale_smp(uint type, uint *smp)
{
	uint64_t tmp;
	int ret;

	switch(type)
	{
	case TYPE_VOLTAGE:
		ret = scale_adc_volt(*smp, &tmp);
		if (ret)
			return ret;
		#ifdef CHECK_OVERFLOW
		if (tmp > 0xffffffff)
			printf("adc_scale_smp(): volt OVERFLOW %llu > %u\n",
			       tmp, 0xffffffff);
		#endif
		*smp = (uint)tmp;
		break;
	case TYPE_CURRENT:
		ret = scale_adc_curr(*smp, &tmp);
		if (ret)
			return ret;
		#ifdef CHECK_OVERFLOW
		if (tmp > 0xffffffff)
			printf("adc_scale_smp(): curr OVERFLOW %llu > %u\n",
			       tmp, 0xffffffff);
		#endif
		*smp = (uint)tmp;
		break;
	case TYPE_POWER:
		ret = scale_adc_pow(*smp, &tmp);
		if (ret)
			return ret;
		#ifdef CHECK_OVERFLOW
		if (tmp > 0xffffffff)
			printf("adc_scale_smp(): pow OVERFLOW %llu > %u\n",
			       tmp, 0xffffffff);
		#endif
		*smp = (uint)tmp;
		break;
	case TYPE_TEMPERATURE:
		tmp = 351300 - 2310 * *smp / 4;
		*smp = (uint)tmp;
		break;

	default:
		printf("adc_scale_smp(): Unsupported type\n");
		return -EINVAL;
	}

	return 0;
}

#define BD71891_ADC_CUR_VAL_BASE 0x8f
#define BD71891_ADC_VOL_VAL_BASE 0x8d
#define BD71891_ADC_TEMP_VAL_BASE 0x93
#define BD71891_ADC_CVT_VAL_HIMASK GENMASK(1, 0)
#define BD71891_ADC_POW_VAL_BASE 0x91
#define BD71891_ADC_POW_VAL_HIMASK GENMASK(3, 0)

static int adc_get_single_sample(uint type, uint *sample)
{
	int ret;
	char buf[2] __attribute__((aligned(2)));
	u16 *s;
	int himask[] = { BD71891_ADC_CVT_VAL_HIMASK, BD71891_ADC_CVT_VAL_HIMASK,
		BD71891_ADC_POW_VAL_HIMASK, BD71891_ADC_CVT_VAL_HIMASK };
	int smp_reg[] = {
		[TYPE_VOLTAGE] = BD71891_ADC_VOL_VAL_BASE,
		[TYPE_CURRENT] = BD71891_ADC_CUR_VAL_BASE,
		[TYPE_POWER] = BD71891_ADC_POW_VAL_BASE,
		[TYPE_TEMPERATURE] = BD71891_ADC_TEMP_VAL_BASE,
	};

	if (type > TYPE_MAX) {
		printf("Bad type\n");
		return -EINVAL;
	}

	ret = bd71891_pmic_read(smp_reg[type], &buf[0], 2);
	if (ret < 0) {
		printf("Read failed\n");
		return ret;
	}

	buf[0] &= himask[type];
	s = (u16 *)&buf[0];

	*sample = be16_to_cpu(*s);
	printf("Raw value from registers 0x%x\n", *sample);

	return adc_scale_smp(type, sample);
}

static int do_adc_get(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	int type, ret;
	uint sample;
	const char* unit[] = {
		[TYPE_VOLTAGE] = "uV",
		[TYPE_CURRENT] = "mA",
		[TYPE_POWER] = "nW",
		[TYPE_TEMPERATURE] = "mC"
	};

	if (argc != 2) {
		printf("expecting single argument [v,i,p,t]\n");
		return CMD_RET_USAGE;
	}

	type = get_operation_type(argv[1]);
	if (type < 0)
		return CMD_RET_USAGE;

	if (type == TYPE_CURRENT || type == TYPE_POWER) {
		if (!g_r_sense) {
			printf("Sense resistor value not known\n");
			return cmd_failure(-EINVAL);
		}
	}

	ret = adc_get_single_sample(type, &sample);
	if (!ret)
		printf("%u %s\n", sample, unit[type]);

	return cmd_ret(ret);
}

static int interval2reg(long interval)
{
	static const int ivals[] = { 50, 100, 1000, 10000, 100000, 1000000};
	int i;

	for (i = 0; i < ARRAY_SIZE(ivals); i++)
		if (ivals[i] == interval)
			return i;

	return -EINVAL;
}

#define BD71891_ADC_NUM_SAMPLES_BASE	0x82
#define BD71891_MAX_ADC_SAMPLES		0x3fffff

static int bd71891_adc_set_num_samples(long samples)
{
	u32 val = cpu_to_be32((u32)samples << 8);

	printf("Setting ADC num samples to %lu\n", samples);

	return bd71891_pmic_write(BD71891_ADC_NUM_SAMPLES_BASE, (char *)&val, 3);
}

#define BD71891_ADC_ACCUM_VAL_BASE	0x89
#define BD71891_ADC_ACCUM_CNT_BASE	0x86

static int get_adc_accum_avg(uint32_t *avg_value, uint32_t *accum)
{
	uint32_t *tmp2, val2;
	char buf[4] __attribute__((aligned(4))), buf2[4]__attribute__((aligned(4)));
	uint num_samples;
	u32 *tmp;
	int ret;

	tmp = (u32 *)&buf[0];
	*tmp = 0;

	/*
	 * TODO: Ensure accumulator is stopped to avoid value being changed
	 * betweem register reads.
	 */
	ret = bd71891_pmic_read(BD71891_ADC_ACCUM_CNT_BASE, &buf[1], 3);
	if (ret)
		return ret;

	num_samples = be32_to_cpu(*tmp);

	if (!num_samples) {
		printf("No samples collected\n");
		*avg_value = *accum = 0;

		return -EINVAL;
	}

	tmp2 = (uint32_t *)&buf2[0];
	*tmp2 = 0;

	ret = bd71891_pmic_read(BD71891_ADC_ACCUM_VAL_BASE, &buf2[0], 4);
	if (ret)
		return ret;

	val2 = be32_to_cpu(*tmp2);

	*accum = val2;
	val2 += num_samples / 2;
	*avg_value = val2 / num_samples;

	return 0;
}

static int get_avg_voltage(uint64_t *value, uint64_t *accum)
{
	int ret;
	uint32_t raw_accum, raw_value;

	ret = get_adc_accum_avg(&raw_value, &raw_accum);
	if (ret)
		return ret;

	ret = scale_adc_volt(raw_value, value);
	if (ret)
		return ret;

	ret = scale_adc_volt(raw_accum, accum);
	if (ret)
		return ret;

	return 0;
}

static int get_avg_current(uint64_t *value, uint64_t *accum)
{
	int ret;
	uint32_t raw_accum, raw_value;

	ret = get_adc_accum_avg(&raw_value, &raw_accum);
	if (ret)
		return ret;

	ret = scale_adc_curr(raw_value, value);
	if (ret)
		return ret;

	ret = scale_adc_curr(raw_accum, accum);
	if (ret)
		return ret;

	return 0;
}

static int get_avg_power(uint64_t *value, uint64_t *accum)
{
	int ret;
	uint32_t raw_accum, raw_value;

	ret = get_adc_accum_avg(&raw_value, &raw_accum);
	if (ret)
		return ret;

	ret = scale_adc_pow(raw_value, value);
	if (ret)
		return ret;

	ret = scale_adc_pow(raw_accum, accum);
	if (ret)
		return ret;

	return 0;
}

static int measure_avg(int type, long samples, long interval)
{
	unsigned long tmp = interval, meas_time;
	static const int delay_arr[] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };
	int multiplier = 0;
	int ret, i;
	int ireg;

	ireg = interval2reg(interval);
	ret = stop_clear_adc_accum();
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_clrsetbits(BD71891_REG_ADC_CTRL2, BD71891_MASK_ADC_INTERVAL,
			      ireg);
	if (ret) {
		printf("Setting interval failed\n");

		return cmd_failure(ret);
	}
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_adc_set_num_samples(samples);
	if (ret) {
		printf("Setting sample amount failed\n");
		return cmd_failure(ret);
	}
	if (ret)
		return cmd_failure(ret);

	while (tmp > 1000) {
		tmp /= 10;
		multiplier++;
	}

	if (multiplier >= ARRAY_SIZE(delay_arr)) {
		printf("interval %lu too big? Max 1 000 000\n", interval);
		return cmd_failure(-EINVAL);
	}

	meas_time = samples * tmp;
	#ifdef CHECK_OVERFLOW
	if (meas_time / tmp != samples)
		printf("OVERFLOW: measure_avg() measurement time overflow SHOULD NOT HAPPEN - TMP SCALED DOWN, %lu != %lu\n",
		       meas_time / tmp, samples);
	#endif
	ret = start_adc_accum();
	if (ret)
		return cmd_failure(ret);

	/*
	 * Here we should catch the IRQ but for the sake of the simplicity
	 * we just sleep/delay for the time it takes to complete measurement.
	 *
	 * Note, we keep the CPU busy. This should probably be avoided in
	 * the product code using IRQs instead.
	 */
	for (i = 0; i < delay_arr[multiplier]; i++)
		udelay(meas_time);

	switch (type) {
	uint64_t avg, accum;

	case TYPE_VOLTAGE:
		ret = get_avg_voltage(&avg, &accum);
		printf("Samples %lu, interval %lu, average voltage %llu uV, accumulated %llu uV\n",
		       samples, interval, avg, accum);
		break;
	case TYPE_CURRENT:
		ret = get_avg_current(&avg, &accum);
		printf("Samples %lu, interval %lu, average current %llu mA accumulated %llu mA\n",
		       samples, interval, avg, accum);
		break;
	case TYPE_POWER:
		ret = get_avg_power(&avg, &accum);
		printf("Samples %lu, interval %lu, average power %llu nW accumulated %llu nW\n",
		       samples, interval, avg, accum);
		break;
	default:
		printf("Unknown type\n");
		ret = -EINVAL;
		break;
	}

	return cmd_ret(ret);
}

static int do_adc_meas(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	long samples, interval;
	int type, ret;
	char *eptr;

	if (argc != 4) {
		printf("bd71891 adc_meas [v, i, p] <num_samples> <interval>\n");
		return CMD_RET_USAGE;
	}

	type = get_operation_type(argv[1]);
	if (type < 0 || type == TYPE_TEMPERATURE) {
		printf("Unknown measurement type, known types [v, i, p]\n");
		return CMD_RET_USAGE;
	}

	if ( (type == TYPE_CURRENT || TYPE_POWER) && !g_r_sense) {
		printf("sense-resistor not known\n");

		return cmd_failure(-EINVAL);
	}

        samples = simple_strtol(argv[2], &eptr, 10);
        if (!*argv[2] || *eptr || ((unsigned long)samples) >= BD71891_MAX_ADC_SAMPLES)
		return CMD_RET_USAGE;

        interval = simple_strtol(argv[3], &eptr, 10);
        if (!*argv[3] || *eptr)
		return CMD_RET_USAGE;

	if (0 > interval2reg(interval))
		return CMD_RET_USAGE;

	ret = __set_adc_source(type);
	if (ret)
		return cmd_failure(ret);


	return measure_avg(type, samples, interval);
}

int usb_char_done(void)
{
	int ret;

	ret = bd71891_reg_read(BD71891_REG_BOOTSRC);
	if (ret < 0) {
		printf("Failed to read USB characterization completion status\n");

		return 0;
	}

	return ret & BD71891_MASK_USB_CHAR_DONE;
}

static int usb_char_read(void)
{
	int i, ret;

	for (i = 0; i < 10; i++) {
		int uc_done = usb_char_done();

		printf("poll [%d/10], USB characterization %s done...\n", i,
		       uc_done ? "IS" : "NOT");

		if (usb_char_done())
			break;

		mdelay(100);
	}
	if (i == 10) {
		printf("Giving up.\n");
		return -ETIMEDOUT;
	}

	for (i = 0; i < 20; i++) {
		int raw_usb_vol;

		raw_usb_vol = bd71891_reg_read(BD71891_REG_USB_CHAR_VOL0 - 2 * i);
		if (raw_usb_vol < 0)
			return cmd_ret(ret);

		printf("RAW USB voltage[%d] %u\n", i, raw_usb_vol);
	}
	for (i = 0; i < 20; i++) {
		int raw_usb_curr;

		raw_usb_curr = bd71891_reg_read(BD71891_REG_USB_CHAR_CURR0 - 2 * i);
		if (raw_usb_curr < 0)
			return cmd_ret(ret);

		printf("RAW USB current[%d] %u\n", i, raw_usb_curr);
	}

	return 0;
}

static int do_usb_char(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int ret;

	if (argc == 1)
		return usb_char_read();

	if (argc != 2 || strcmp(argv[1], "start"))
		return CMD_RET_USAGE;

	ret = bd71891_clrsetbits(BD71891_REG_BOOTSRC,
				 BD71891_MASK_USB_CHAR_DONE, 0);
	if (ret)
		return cmd_ret(ret);

	return usb_char_read();
}

static int usb_char_cfg_read(void)
{
	int ret;

	ret = bd71891_pr_reas_field(&bd71891_usb_char_cur_lim);
	if (ret)
		return cmd_failure(ret);
	printf("Units mA\n");

	ret = bd71891_pr_reas_field(&bd71891_usb_char_itvl);
	if (ret)
		return cmd_failure(ret);
	printf("Units mS\n");

	ret = bd71891_pr_reas_field(&bd71891_usb_char_pulse_w);
	if (ret)
		return cmd_failure(ret);
	printf("Units mS\n");

	ret = bd71891_pr_reas_field(&bd71891_usb_char_cur_avg);
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_pr_reas_field(&bd71891_usb_char_rsens);
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_pr_reas_field(&bd71891_usb_char_v_start);
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_pr_reas_field(&bd71891_usb_char_uvp);
	if (ret)
		return cmd_failure(ret);

	ret = bd71891_pr_reas_field(&bd71891_usb_char_vsys_debounce);
	if (ret)
		return cmd_failure(ret);

	return cmd_ret(ret);
}

static int bd71891_pr_reas_field_set(const struct reason_reg_field *field,
				     char *value)
{
	int ret, i;

	ret = bd71891_reg_read(field->reason_reg.reg);
	if (ret < 0)
		return cmd_ret(ret);

	for (i = 0; i < field->reason_reg.num_reasons; i++)
		if (!strcmp(field->reason_reg.reasons[i].reason, value))
			break;

	if (i == field->reason_reg.num_reasons) {
		printf("'%s': Unsupported value '%s'\n",
		       field->reason_reg.explanation, value);
		return CMD_RET_USAGE;
	}

	ret = bd71891_clrsetbits(field->reason_reg.reg, field->mask,
				 field->reason_reg.reasons[i].value);

	return cmd_ret(ret);
}

struct usb_char_cfg_field {
	const char *name;
	const struct reason_reg_field *field;
};
#define USB_CHAR_CFG_FIELD(_name, _field)				\
{ .name = (_name), .field = (_field), }

static int bd71891_usb_char_cfg(char *config, char *value)
{
	static const struct usb_char_cfg_field configs[] = {
		USB_CHAR_CFG_FIELD("Ilim", &bd71891_usb_char_cur_lim),
		USB_CHAR_CFG_FIELD("interval", &bd71891_usb_char_itvl),
		USB_CHAR_CFG_FIELD("pulse", &bd71891_usb_char_pulse_w),
		USB_CHAR_CFG_FIELD("Iavg", &bd71891_usb_char_cur_avg),
		USB_CHAR_CFG_FIELD("Rsens", &bd71891_usb_char_rsens),
		USB_CHAR_CFG_FIELD("Vstart", &bd71891_usb_char_v_start),
		USB_CHAR_CFG_FIELD("UVP", &bd71891_usb_char_uvp),
		USB_CHAR_CFG_FIELD("debounce", &bd71891_usb_char_vsys_debounce),
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(configs); i++)
		if (!strcmp(config, configs[i].name))
			return bd71891_pr_reas_field_set(configs[i].field, value);

	printf("Unknown USB characterization config. Known configs:\n");
	for (i = 0; i < ARRAY_SIZE(configs); i++)
		printf("%s\n", configs[i].name);

	return CMD_RET_USAGE;
}

static int do_usb_char_cfg(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	if (argc == 1)
		return usb_char_cfg_read();

	if (argc != 3)
		return CMD_RET_USAGE;

	return bd71891_usb_char_cfg(argv[1], argv[2]);
}

#define USB_CHAR_CFG_USAGE "usb_char_cfg [config value]\n"
#define USB_CHAR_CFG_HELP "usb_char_cfg config value] - get/set USB characterization config(s)\n" \
	"usb_char_cfg\n" 							\
	"\tGet USB charazterization configuration\n"				\
	"usb_char_cfg Ilim <val> - set current limit in mA\n"			\
	"\tSupported values 600, 700, 800, 900, 1000, 1100, 1200, 1300\n"	\
	"usb_char_cfg interval <val> - set sinking pulse interval mS\n"		\
	"\tSupported values 1, 4, 10, 20\n"					\
	"usb_char_cfg pulse <val> - set sinking pulse width mS\n"		\
	"\tSupported values 1, 2, 5, 10\n"					\
	"usb_char_cfg Iavg <val> - set number of current samples to average\n"	\
	"\tSupported values 16, 32\n"						\
	"usb_char_cfg Rsens <val> - set size of sense resistor mOhm\n"		\
	"\tSupported values 10, 20, 30\n"					\
	"usb_char_cfg Vstart <val> - set the voltage to start with mV\n"	\
	"\tSupported values 4000, 4250, 4500, 4750\n"				\
	"usb_char_cfg UVP <val> - set the under voltage detection limt mV\n"	\
	"\tSupported values 3600, 4000, 4200, 4500\n"				\

#define USB_CHAR_USAGE "usb_char [start]\n"
#define USB_CHAR_HELP "usb_char [start] - get USB characterization info\n" \
	"usb_char\n"							   \
	"\tGet USB charazterization status\n"				   \
	"usb_char start\n"						   \
	"\tStart the USB charazterization and get status\n"

#define HPD_PINCTRL_USAGE "hpd_pin_ctrl [pin [pull soc/conn value] [nmos value]]\n"
#define HPD_PINCTRL_HELP  "hpd_pin_ctrl - get HDMI HPD info\n" 		\
	"hpd_pin_ctrl pin - get HDMI HPD info for a specific pin\n" 	\
	"\tpin can be cec/i2c\n"					\
	"hpd_pin_ctrl cec/i2c nmos 10/30 - Configure parallel NMOS\n"	\
	"hpd_pin_ctrl cec/i2c pull soc/conn value - Configure pull-up\n" \
	"\tpin\tsoc/con\tpossible values:\n"				\
	"\tcec\tsoc\t1000, 2200, 5000, 10000 (values represent ohms)\n"	\
	"\tcec\tconn\t0(open), 26000 (26kOhm)\n"			\
	"\ti2c\tsoc\t1000, 2200, 5000, 10000 (values represent ohms)\n"	\
	"\ti2c\tconn\t0(open), 1750 (1.75kOhm)\n"

/*
 */
static struct cmd_tbl subcmd[] = {
	U_BOOT_CMD_MKENT(chipinfo, 1, 1, do_chipinfo, "", ""),
	U_BOOT_CMD_MKENT(set_idle_state, 2, 1, do_set_idle_state, "", ""),
	U_BOOT_CMD_MKENT(hpd_idle_ctrl, 2, 1, do_hpd_idle_ctrl, "", ""),
	U_BOOT_CMD_MKENT(hpd_pin_ctrl, 2, 1, do_hpd_pin_ctrl, HPD_PINCTRL_USAGE, HPD_PINCTRL_HELP),
	U_BOOT_CMD_MKENT(adc_source, 2, 1, do_adc_source, "", ""),
	U_BOOT_CMD_MKENT(adc_state, 2, 1, do_adc_state, "", ""),
	U_BOOT_CMD_MKENT(adc_gain, 2, 1, do_adc_gain, "", ""),
	U_BOOT_CMD_MKENT(adc_vol_source, 2, 1, do_adc_vol_source, "", ""),
	U_BOOT_CMD_MKENT(adc_get, 2, 1, do_adc_get, "", ""),
	U_BOOT_CMD_MKENT(dt_init, 1, 1, do_dt_init, "", ""),
	U_BOOT_CMD_MKENT(adc_meas, 4, 1, do_adc_meas, "", ""),
	U_BOOT_CMD_MKENT(usb_char, 2, 1, do_usb_char, USB_CHAR_USAGE, USB_CHAR_HELP),
	U_BOOT_CMD_MKENT(usb_char_cfg, 2, 1, do_usb_char_cfg, USB_CHAR_CFG_USAGE, USB_CHAR_CFG_HELP),

	/*
	 * TODO: Add commands for:
	 * - ADC: limit setting
	 * - ADC: constant measurement
	 * - USB characterization

	U_BOOT_CMD_MKENT(adc_meas, 4, 1, do_adc_meas, "", ""),
	U_BOOT_CMD_MKENT(adc_limit, 3, 1, do_adc_limit, "", ""),
	U_BOOT_CMD_MKENT(usb_char, 3, 1, do_usb_char, "", ""),
	*/
};

static int do_bd71891(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	struct cmd_tbl *cmd;

	argc--;
	argv++;

	cmd = find_cmd_tbl(argv[0], subcmd, ARRAY_SIZE(subcmd));
	if (cmd == NULL || argc > cmd->maxargs)
		return CMD_RET_USAGE;

	return cmd->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(bd71891, CONFIG_SYS_MAXARGS, 1, do_bd71891,
	"BD71891 sub-system",
	"bd71891 dt_init - initialize based on DT\n"
	"bd71891 chipinfo - recorded power-on reasons and current power state\n"
	"bd71891 set_idle_state [idle, run] - set run mode\n"
	"bd71891 hpd_idle_ctrl [1,0] - Query or set HDMI detector's IDLE control\n"
	"bd71891 hpd_pin_ctrl - Query or configure HDMI pins\n"
	"bd71891 adc_source [voltage power current] - Query or configure ADC ACCUM source\n"
	"bd71891 adc_state - get or set ADC accum state (start, stop)\n"
	"bd71891 adc_gain - get or set gain for ADC current accumulator\n"
	"bd71891 adc_vol_source - get or set ADC accum voltage source\n"
	"bd71891 adc_get [v, i, p, t] - get last measured value\n"
	"bd71891 adc_meas [v, i, p] <num_samples> <interval> - measure\n"
	"bd71891 usb_char [start] - USB characterization state\n"
	"bd71891 usb_char_cfg - USB characterization state\n"
);

