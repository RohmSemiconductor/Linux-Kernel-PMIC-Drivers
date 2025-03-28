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
#include <linux/delay.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/bd71891.h>
#include "bdxxxx.h"

#define PMIC_DT_NAME "pmic@4b"

/*
 * Wait 10 mS before rechecking if state was changed
 * There is 10 attempts and always this delay before re-checking
 */
#define WAIT_FOR_IDLE_CHANGE_US 10000

DECLARE_GLOBAL_DATA_PTR;

static inline struct udevice *get_bd71891(void)
{
	return get_currdev(PMIC_DT_NAME);
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

static struct cmd_tbl subcmd[] = {
	U_BOOT_CMD_MKENT(chipinfo, 1, 1, do_chipinfo, "", ""),
	U_BOOT_CMD_MKENT(set_idle_state, 2, 1, do_set_idle_state, "", ""),
	U_BOOT_CMD_MKENT(hpd_idle_ctrl, 2, 1, do_hpd_idle_ctrl, "", ""),
	U_BOOT_CMD_MKENT(hpd_pin_ctrl, 2, 1, do_hpd_pin_ctrl, HPD_PINCTRL_USAGE, HPD_PINCTRL_HELP),
	/*U_BOOT_CMD_MKENT(dt_init, 1, 1, do_dt_init, "", ""),
	U_BOOT_CMD_MKENT(hibernate, 1, 1, do_hibernate, "", ""),
	U_BOOT_CMD_MKENT(adc_state, 2, 1, do_adc_state, "", ""),
	U_BOOT_CMD_MKENT(adc_source, 2, 1, do_adc_source, "", ""),
	U_BOOT_CMD_MKENT(adc_vol_source, 2, 1, do_adc_vol_source, "", ""),
	U_BOOT_CMD_MKENT(adc_gain, 2, 1, do_adc_gain, "", ""),
	U_BOOT_CMD_MKENT(adc_meas, 4, 1, do_adc_meas, "", ""),
	U_BOOT_CMD_MKENT(adc_limit, 3, 1, do_adc_limit, "", ""),
	U_BOOT_CMD_MKENT(adc_get, 2, 1, do_adc_get, "", ""),*/
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
	"bd71891 chipinfo - recorded power-on reasons and current power state\n"
	"bd71891 set_idle_state [idle, run] - set run mode\n"
	"bd71891 hpd_idle_ctrl [1,0] - Query or set HDMI detector's IDLE control\n"
	"bd71891 hpd_pin_ctrl - Query or configure HDMI pins\n"
);

