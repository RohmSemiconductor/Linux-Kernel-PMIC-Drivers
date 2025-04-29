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
#include <linux/compiler.h>
#include <linux/delay.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/bd71892.h>
#include "bdxxxx.h"

#define PMIC_DT_NAME "companion@47"

/*
 * Wait 10 mS before rechecking if state was changed
 * There is 10 attempts and always this delay before re-checking
 */
#define WAIT_FOR_IDLE_CHANGE_US 10000

DECLARE_GLOBAL_DATA_PTR;

static inline struct udevice *get_bd71892(void)
{
	return get_currdev(PMIC_DT_NAME);
}

static inline int bd71892_reg_write(uint reg, uint val)
{
	struct udevice *pmicdev = get_bd71892();

	if (!pmicdev)
		return -ENODEV;

	return pmic_reg_write(pmicdev, reg, val);
}

static inline int bd71892_reg_read(uint reg)
{
	struct udevice *pmicdev = get_bd71892();

	if (!pmicdev)
		return -ENODEV;

	return pmic_reg_read(pmicdev, reg);
}

static inline int bd71892_clrsetbits(uint reg, uint clr, uint set)
{
	struct udevice *pmicdev = get_bd71892();

	if (!pmicdev)
		return -ENODEV;

	return pmic_clrsetbits(pmicdev, reg, clr, set);
}

static inline int bd71892_pr_reas_bf(const struct reason_reg *r)
{
	return print_reason_bf_reg(PMIC_DT_NAME, r);
}

static inline int bd71892_pr_reas_field(const struct reason_reg_field *f)
{
	return print_reason_field_reg(PMIC_DT_NAME, f);
}

static const struct pmic_child_info pmic_children_info[] = {
	/* buck */
	{ .prefix = "bu", .driver = BD71892_BUCK_DRIVER},
	/* ldo */
	{ .prefix = "l", .driver = BD71892_LDO_DRIVER},
	{ },
};

enum {
	TYPE_VOLTAGE,
	TYPE_CURRENT,
	TYPE_POWER,
	TYPE_TEMPERATURE,
	#define TYPE_MAX TYPE_TEMPERATURE
};

static int bd71892_bind(struct udevice *dev)
{
	return bdxxxx_bind(dev, pmic_children_info);
}

static int bd71892_reg_count(struct udevice *dev)
{
	return BD71892_MAX_REGISTER - 1;
}

static inline int bd71892_i2c_is_locked(struct udevice *dev)
{
	uint8_t lockval;
	int ret;

	ret = bdxxxx_read(dev, BD71892_REG_I2C_LOCK, &lockval, 1);
	if (ret)
		return ret;

	return (lockval != BD71892_I2C_UNLOCKED);
}

static int bd71892_i2c_unlock(struct udevice *dev)
{
	int i;
	uint8_t writeval = 1;

	/*
	 * Data sheet says writing anything else except the
	 * BD71892_I2C_UNLOCK_VAL will lock the I2C. Writing the
	 * BD71892_I2C_UNLOCK_VAL 3 times is said to unlock the I2C. It is not
	 * specified what happens if the BD71892_I2C_UNLOCK_VAL is written when
	 * I2C has already been unlocked. Eg, if writing BD71892_I2C_UNLOCK_VAL
	 * 4 times locks the I2C again. Nor has it been said if the 3 writes
	 * should be consequent, or if there can be reads done in between.
	 * Hence, let's first write something else (hoping this will clear the
	 * 'unlock' sequence should it have been started already) - and then do
	 * 3 consequent BD71892_I2C_UNLOCK_VAL writes.
	 */
	bdxxxx_write(dev, BD71892_REG_I2C_LOCK, &writeval, 1);

	writeval = BD71892_I2C_UNLOCK_VAL;
	for (i = 0; i < 3; i++)
		bdxxxx_write(dev, BD71892_REG_I2C_LOCK, &writeval, 1);

	if (bd71892_i2c_is_locked(dev)) {
		printf("Failed to unlock\n");

		return -EIO;
	}

	return 0;
}

static int bd71892_probe(struct udevice *dev)
{
	int locked;

	debug("%s: '%s' probed\n", __func__, dev->name);

	locked = bd71892_i2c_is_locked(dev);
	if (locked < 0)
		return locked;

	if (locked)
		return bd71892_i2c_unlock(dev);

	return 0;
}

static struct dm_pmic_ops bd71892_ops = {
	.reg_count = bd71892_reg_count,
	.read = bdxxxx_read,
	.write = bdxxxx_write,
};

static const struct udevice_id bd71892_ids[] = {
	{ .compatible = "rohm,bd71892", },
	{ }
};

U_BOOT_DRIVER(pmic_bd71892) = {
	.name = "bd71892_pmic",
	.id = UCLASS_PMIC,
	.of_match = bd71892_ids,
	.bind = bd71892_bind,
	.probe = bd71892_probe,
	.ops = &bd71892_ops,
};

/* Commands */

const struct reason_info bd71892_boot_reas_info[] = {
	REASON_INFO("Cold reset", BIT(7)),
	REASON_INFO("Emergency recovery", BIT(6)),
	REASON_INFO("PMIC cold boot", BIT(1)),
};

const struct reason_info bd71892_reset_reas_info_0[] = {
	REASON_INFO("External dev asserted FLT_B", BIT(7)),
	REASON_INFO("Voltage fault", BIT(6)),
	REASON_INFO("Bad VSYS Voltage", BIT(4)),
	REASON_INFO("Thermal Shuitdown", BIT(3)),
	REASON_INFO("PMIC_EN pin toggles", BIT(2)),
	REASON_INFO("SWRESET", BIT(1)),
};

const struct reason_info bd71892_reset_reas_info_1[] = {
	REASON_INFO("LDO1 Power Failure", BIT(5)),
	REASON_INFO("BUCK5 Power Failure", BIT(4)),
	REASON_INFO("BUCK4 Power Failure", BIT(3)),
	REASON_INFO("BUCK3 Power Failure", BIT(2)),
	REASON_INFO("BUCK2 Power Failure", BIT(1)),
	REASON_INFO("BUCK1 Power Failure", BIT(0)),
};

const struct reason_info bd71892_reset_reas_info_2[] = {
	REASON_INFO("BUCK5 OCP", BIT(4)),
	REASON_INFO("BUCK4 OCP", BIT(3)),
	REASON_INFO("BUCK3 OCP", BIT(2)),
	REASON_INFO("BUCK2 OCP", BIT(1)),
	REASON_INFO("BUCK1 OCP", BIT(0)),
};

const struct reason_info bd71892_power_state_info[] = {
	REASON_INFO("IC Shutdown", 0),
	REASON_INFO("Emergency", 1),
	REASON_INFO("DEAD", 2),
	REASON_INFO("OTP LOAD", 3),
	REASON_INFO("HIBERNATE", 4),
	REASON_INFO("RUN", 5),
	REASON_INFO("IDLE", 6),
};

const struct reason_reg bd71892_boot_reas = {
	.explanation = "BOOT FLAGS",
	.reasons = &bd71892_boot_reas_info[0],
	.num_reasons = ARRAY_SIZE(bd71892_boot_reas_info),
	.reg = BD71892_REG_BOOTSRC,
};

const struct reason_reg bd71892_reset_reas[] =
{
	{
		.explanation = "Reset reason flag 1 (RESETSRC1_FLG 0x08)",
		.reasons = &bd71892_reset_reas_info_0[0],
		.num_reasons = ARRAY_SIZE(bd71892_reset_reas_info_0),
		.reg = BD71892_REG_RESETSRC1,
	}, {
		.explanation = "Reset reason flag 2 (RESETSRC2_FLG 0x09)",
		.reasons = &bd71892_reset_reas_info_1[0],
		.num_reasons = ARRAY_SIZE(bd71892_reset_reas_info_1),
		.reg = BD71892_REG_RESETSRC2,
	}, {
		.explanation = "Reset reason flag 3 (RESETSRC3_FLG 0x0a)",
		.reasons = &bd71892_reset_reas_info_2[0],
		.num_reasons = ARRAY_SIZE(bd71892_reset_reas_info_2),
		.reg = BD71892_REG_RESETSRC3,
	},
};

const struct reason_reg_field bd71892_power_state = {
	.reason_reg = {
		.explanation = "PMIC state",
		.reasons = &bd71892_power_state_info[0],
		.num_reasons = ARRAY_SIZE(bd71892_power_state_info),
		.reg = BD71892_REG_POWER_STATE,
	},
	.mask = 0x3,
};

/* Get the power-on reason */
static int do_chipinfo(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	int ret, i;

	ret = bd71892_pr_reas_bf(&bd71892_boot_reas);
	if (ret)
		return cmd_failure(ret);

	for (i = 0; i < ARRAY_SIZE(bd71892_reset_reas); i++) {
		ret = bd71892_pr_reas_bf(&bd71892_reset_reas[i]);
		if (ret)
			return cmd_failure(ret);
	}

	ret = bd71892_pr_reas_field(&bd71892_power_state);
	return cmd_ret(ret);
}

static int bd71892_get_power_state(void)
{
	return bd71892_reg_read(BD71892_REG_POWER_STATE);
}

static int toggle_idle_to(bool idle)
{
	int idlebit = idle ? BD71892_MASK_IDLE_MODE : 0;
	int ret, i;

	ret = bd71892_clrsetbits(BD71892_REG_PS_CTRL_1, BD71892_MASK_IDLE_MODE,
				 idlebit);
	if (ret)
		return cmd_failure(ret);

	ret = bd71892_reg_read(BD71892_REG_PS_CTRL_1);
	if (ret < 0)
		return cmd_failure(ret);

	for (i = 0; (is_idle_mode(ret) != idle) && (i < 10); i++) {
		printf("Still in %s ...\n", (idle) ? "idle" : "run");
		udelay(WAIT_FOR_IDLE_CHANGE_US);

		ret = bd71892_reg_read(BD71892_REG_PS_CTRL_1);
		if (ret < 0)
			return cmd_failure(ret);

		if (is_run_mode(ret))
			break;
	}
	if (idle != is_idle_mode(ret))
		return cmd_failure(-ETIMEDOUT);

	return 0;
}

static int do_set_idle_state(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	int curr_state = bd71892_get_power_state();
	char *new_state;
	int ret;

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

static int wait_adc_meas_complete(void)
{
	int kick;

	for (kick = bd71892_reg_read(BD71892_REG_ADC_KICK);
	     kick > -1;
	     kick = bd71892_reg_read(BD71892_REG_ADC_KICK))
		/* We could add small delay here to not choke the I2C */;

	if (kick != 0) {
		printf("Failed to read ADC_KICK\n");

		return kick;
	}

	return 0;
}

static int reg_to_temp(uint16_t *reg_be)
{
	return 351300 - be16_to_cpu(*reg_be) * 2310 / 4;
}

static int __read_temp_from_reg(int reg, int *temp_mc)
{
	int ret;
	char buf[2] __attribute__((aligned(2)));
	struct udevice *dev = get_bd71892();

	if (!dev)
		return -ENODEV;

	ret = pmic_read(dev, reg, &buf[0], 2);
	if (ret)
		return ret;

	*temp_mc = reg_to_temp((uint16_t *)&buf[0]);

	return 0;
}

static int do_read_temp(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int ret, temp_mc;

	ret = __read_temp_from_reg(BD71892_REG_ADC_TEMP_HI, &temp_mc);

	if (!ret)
		printf("measured temperature: %d mC \n", temp_mc);
	else
		printf("Failed to read temperature\n");

	return cmd_ret(ret);
}

int is_hiawatha(void)
{
	unsigned short one = 1;
	char *tmp = (char *)&one;

	return *tmp;
}

static int limit2regval(int limit, char *regs)
{
	int reg;

	/*
	 * limit_mC = 351300 - 2310 * regs / 4
	 *
	 *        (351300 - limit_mC) * 4
	 * regs = ----------------------
	 *               2310
	 */

	reg = (351300 - limit) * 4;
	reg /= 4;

	if ((reg & BD71892_MASK_ADC_TEMP_LIMIT) != reg) {
		printf("Unsupported limit %d\n", limit);
		return -EINVAL;
	}

	if (is_hiawatha()) {
		unsigned int tmp = limit;

		regs[0] = tmp >> 8;
		regs[1] = tmp;
	} else {
		unsigned int tmp = limit;

		regs[1] = tmp >> 8;
		regs[0] = tmp;
	}

	return 0;
}

static int write_temp_limit(int limit)
{
	u8 regval[2];
	int ret;

	ret = limit2regval(limit, &regval[0]);
	if (ret)
		return ret;

	/*
	 * NOTE! This is not atomic! It is possible the writes will generate a,
	 * smaller than intended, intermediate limit value when first register
	 * is written and second isn't. I don't see a way around this. So,
	 * the final and proper design should probably either mask the ADC WARN
	 * interrupt or be prepared to handle a bogus interrupt when limit
	 * is changed.
	 *
	 * TODO: See if writing both registers without I2C stop can update the
	 * limit atomically.
	 */
	ret = bd71892_reg_write(BD71892_REG_ADC_TEMP_LIMIT_HI, regval[0]);
	if (ret)
		return ret;

	return bd71892_reg_write(BD71892_REG_ADC_TEMP_LIMIT_LO, regval[1]);
}

static int do_temp_limit(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	int limit_mc, ret;
	char *eptr;

	if (argc == 1) {
		ret = __read_temp_from_reg(BD71892_REG_ADC_TEMP_LIMIT_HI,
					   &limit_mc);
		if (!ret)
			printf("Temperature limit %d mC\n", limit_mc);
		else
			printf("Temperature limit read failed\n");

		return cmd_ret(ret);
	}

	if (argc != 2)
		return CMD_RET_USAGE;

        limit_mc = simple_strtol(argv[1], &eptr, 10);
        if (!*argv[1] || *eptr)
		return CMD_RET_USAGE;

	ret = write_temp_limit(limit_mc);
	return cmd_ret(ret);
}

static int do_adc_meas(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int kick, ret;
	char buf[2];
	uint16_t *val_be = (uint16_t *)&buf[0];
	struct udevice *dev = get_bd71892();

	if (!dev)
		return cmd_ret(-ENODEV);

	ret = wait_adc_meas_complete();
	if (ret)
		return cmd_ret(ret);

	ret = bd71892_reg_write(BD71892_REG_ADC_KICK, 1);
	if (ret)
		return cmd_ret(kick);

	ret = wait_adc_meas_complete();
	if (ret)
		return cmd_ret(ret);

	ret = pmic_read(dev, BD71892_REG_ADC_VOL_HI, &buf[0], 2);
	if (ret)
		return cmd_ret(ret);

	printf("measured VSYS: %u mV\n", be16_to_cpu(*val_be) * 5860);

	return cmd_ret(0);
}

static struct cmd_tbl subcmd[] = {
	U_BOOT_CMD_MKENT(chipinfo, 1, 1, do_chipinfo, "", ""), /* Ok */
	U_BOOT_CMD_MKENT(set_idle_state, 2, 1, do_set_idle_state, "", ""),
	U_BOOT_CMD_MKENT(adc_meas, 1, 1, do_adc_meas, "", ""),
	U_BOOT_CMD_MKENT(read_temp, 1, 1, do_read_temp, "", ""),
	U_BOOT_CMD_MKENT(temp_limit, 1, 1, do_temp_limit, "", ""),
	/*U_BOOT_CMD_MKENT(dt_init, 1, 1, do_dt_init, "", ""),
	U_BOOT_CMD_MKENT(hibernate, 1, 1, do_hibernate, "", ""),
	U_BOOT_CMD_MKENT(adc_state, 2, 1, do_adc_state, "", ""),
	U_BOOT_CMD_MKENT(adc_source, 2, 1, do_adc_source, "", ""),
	U_BOOT_CMD_MKENT(adc_vol_source, 2, 1, do_adc_vol_source, "", ""),
	U_BOOT_CMD_MKENT(adc_gain, 2, 1, do_adc_gain, "", ""),
	U_BOOT_CMD_MKENT(adc_limit, 3, 1, do_adc_limit, "", ""),
	U_BOOT_CMD_MKENT(adc_get, 2, 1, do_adc_get, "", ""),*/
};

static int do_bd71892(struct cmd_tbl *cmdtp, int flag, int argc,
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


U_BOOT_CMD(bd71892, CONFIG_SYS_MAXARGS, 1, do_bd71892,
	"BD71892 sub-system",
	"bd71892 chipinfo - recorded power-on reasons and current power state\n"
	"bd71892 set_idle_state [idle, run] - set run mode\n"
	"bd71885 adc_meas - measure VSYS using ADC\n"
	"bd71885 read_temp - read the latest measured temperature\n"
	"bd71892 dt_init - initialize PMIC based on DT values\n"
	"bd71885 adc_state - get or set ADC accum state (start, stop)\n"
	"bd71885 adc_source - get or set ADC accum source (voltage, current, power)\n"
	"bd71885 adc_vol_source - get or set ADC accum voltage source\n"
	"bd71885 adc_gain - get or set gain for ADC current accumulator\n"
	"bd71885 adc_meas [v, i, p] <num_samples> <interval> - measure\n"
	"bd71885 adc_limit [a(ccum), p(ower), t(emperature)] <threshold value> - set limit\n"
	"bd71885 adc_get [v, i, p, t] - get last measured value\n"
);

