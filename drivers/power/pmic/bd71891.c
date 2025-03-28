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


DECLARE_GLOBAL_DATA_PTR;

static inline struct udevice *get_bd71891(void)
{
	return get_currdev(PMIC_DT_NAME);
}

static inline int bd71891_reg_read(uint reg)
{
	return pmic_reg_read(get_bd71891(), reg);
}

static inline int bd71891_clrsetbits(uint reg, uint clr, uint set)
{
	return pmic_clrsetbits(get_bd71891(), reg, clr, set);
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

static int do_set_state(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	static struct udevice *pmicdev;
	char *state;
	int ret;

	pmicdev = get_bd71891();

	if (!pmicdev)
		return cmd_failure(-ENODEV);

	if (argc != 2)
		return CMD_RET_USAGE;

	state = argv[1];

	if (!strcmp(state, "run")) {
		ret = bd71891_reg_read(BD71891_REG_PS_CTRL_1);
		if (ret < 0)
			return cmd_failure(ret);

		if (ret & BD71891_MASK_IDLE_MODE)
			ret = bd71891_clrsetbits(BD71891_REG_PS_CTRL_1,
						 BD71891_MASK_IDLE_MODE, 0);
		return cmd_ret(ret);
	} else if (!strcmp(state, "idle")) {
		ret = bd71891_reg_read(BD71891_REG_PS_CTRL_1);
		if (ret < 0)
			return cmd_failure(ret);

		if (!(ret & BD71891_MASK_IDLE_MODE))
			ret = bd71891_clrsetbits(BD71891_REG_PS_CTRL_1,
						 0, BD71891_MASK_IDLE_MODE);
		return cmd_ret(ret);

	} else {
		printf("Invalid state '%s' requested (supporting 'run'/'idle')\n", state);
		return CMD_RET_USAGE;
	}
}

static struct cmd_tbl subcmd[] = {
	U_BOOT_CMD_MKENT(chipinfo, 1, 1, do_chipinfo, "", ""),
	U_BOOT_CMD_MKENT(set_state, 2, 1, do_set_state, "", ""),
	/*U_BOOT_CMD_MKENT(dt_init, 1, 1, do_dt_init, "", ""),
	U_BOOT_CMD_MKENT(hibernate, 1, 1, do_hibernate, "", ""),
	U_BOOT_CMD_MKENT(get_state, 1, 1, do_get_state, "", ""),
	U_BOOT_CMD_MKENT(set_state, 2, 1, do_set_state, "", ""),
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
	"bd71891 set_state <state> - set run mode (idle, run)\n"
);

