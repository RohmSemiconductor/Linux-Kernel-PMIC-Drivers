// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for the linear_ranges helper.
 *
 * Copyright (C) 2020, ROHM Semiconductors.
 * Author: Matti Vaittinen <matti.vaittien@fi.rohmeurope.com>
 */


#include <linux/interrupt.h>
#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/simple_gauge.h>
#include <linux/regmap.h>

#include <linux/mfd/rohm-bd71815.h>

#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-0dC.h"
/*
#include "_bd71828-batdata/A01/out/Discharge-0p1C_Cont-15dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p1C_Cont-25dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p1C_Cont-45dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p1C_Cont-5dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-0dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-15dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-25dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-45dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p2C_Cont-5dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p35C_Cont-0dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p35C_Cont-15dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p35C_Cont-25dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p35C_Cont-45dC.h"
#include "_bd71828-batdata/A01/out/Discharge-0p35C_Cont-5dC.h"
#include "_bd71828-batdata/A01/out/Discharge-1p0C_Cont-0dC.h"
#include "_bd71828-batdata/A01/out/Discharge-1p0C_Cont-15dC.h"
#include "_bd71828-batdata/A01/out/Discharge-1p0C_Cont-25dC.h"
#include "_bd71828-batdata/A01/out/Discharge-1p0C_Cont-45dC.h"
#include "_bd71828-batdata/A01/out/Discharge-1p0C_Cont-5dC.h"
*/
/*
 * The temperature is last part of included battery data header.
 * <N>dC - where N is temperature and dC just tells it is defrees C
 * This define should be 0.1 degrees C
 */
//#define TEST_TEMP 0

/* First things first. I deeply dislike unit-tests. I have seen all the hell
 * breaking loose when people who think the unit tests are "the silver bullet"
 * to kill bugs get to decide how a company should implement testing strategy...
 *
 * Believe me, it may get _really_ ridiculous. It is tempting to think that
 * walking through all the possible execution branches will nail down 100% of
 * bugs. This may lead to ideas about demands to get certain % of "test
 * coverage" - measured as line coverage. And that is one of the worst things
 * you can do.
 *
 * Ask people to provide line coverage and they do. I've seen clever tools
 * which generate test cases to test the existing functions - and by default
 * these tools expect code to be correct and just generate checks which are
 * passing when ran against current code-base. Run this generator and you'll get
 * tests that do not test code is correct but just verify nothing changes.
 * Problem is that testing working code is pointless. And if it is not
 * working, your test must not assume it is working. You won't catch any bugs
 * by such tests. What you can do is to generate a huge amount of tests.
 * Especially if you were are asked to proivde 100% line-coverage x_x. So what
 * does these tests - which are not finding any bugs now - do?
 *
 * They add inertia to every future development. I think it was Terry Pratchet
 * who wrote someone having same impact as thick syrup has to chronometre.
 * Excessive amount of unit-tests have this effect to development. If you do
 * actually find _any_ bug from code in such environment and try fixing it...
 * ...chances are you also need to fix the test cases. In sunny day you fix one
 * test. But I've done refactoring which resulted 500+ broken tests (which had
 * really zero value other than proving to managers that we do do "quality")...
 *
 * After this being said - there are situations where UTs can be handy. If you
 * have algorithms which take some input and should produce output - then you
 * can implement few, carefully selected simple UT-cases which test this. I've
 * previously used this for example for netlink and device-tree data parsing
 * functions. Feed some data examples to functions and verify the output is as
 * expected. I am not covering all the cases but I will see the logic should be
 * working.
 *
 * Here we also do some minor testing. I don't want to go through all branches
 * or test more or less obvious things - but I want to see the main logic is
 * working. And I definitely don't want to add 500+ test cases that break when
 * some simple fix is done x_x. So - let's only add few, well selected tests
 * which ensure as much logic is good as possible.
 */

#define TEST_DESIGNED_CAP 1738000
#define TEST_MAX_VOLTAGE 4400000
#define soc_est_max_num 5

#define DGRD_TEMP_H_DEFAULT		450	/* 0.1 degrees C unit */
#define DGRD_TEMP_M_DEFAULT		250	/* 0.1 degrees C unit */
#define DGRD_TEMP_L_DEFAULT		50	/* 0.1 degrees C unit */
#define DGRD_TEMP_VL_DEFAULT		0	/* 0.1 degrees C unit */

#define BD71815_MASK_CONF_XSTB			BIT(1)

static struct resource bd71815_power_irqs[] = {
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_RMV, "bd71815-dcin-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CLPS_OUT, "bd71815-dcin-clps-out"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CLPS_IN, "bd71815-dcin-clps-in"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_OVP_RES, "bd71815-dcin-ovp-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_OVP_DET, "bd71815-dcin-ovp-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_MON_RES, "bd71815-dcin-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_DCIN_MON_DET, "bd71815-dcin-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_UV_RES, "bd71815-vsys-uv-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_UV_DET, "bd71815-vsys-uv-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_LOW_RES, "bd71815-vsys-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_LOW_DET, "bd71815-vsys-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_MON_RES, "bd71815-vsys-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_VSYS_MON_DET, "bd71815-vsys-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_WDG_TEMP, "bd71815-chg-wdg-temp"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_WDG_TIME, "bd71815-chg-wdg"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RECHARGE_RES, "bd71815-rechg-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RECHARGE_DET, "bd71815-rechg-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_RANGED_TEMP_TRANSITION, "bd71815-ranged-temp-transit"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_CHG_STATE_TRANSITION, "bd71815-chg-state-change"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_TEMP_NORMAL, "bd71815-bat-temp-normal"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_TEMP_ERANGE, "bd71815-bat-temp-erange"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_REMOVED, "bd71815-bat-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_DETECTED, "bd71815-bat-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_THERM_REMOVED, "bd71815-therm-rmv"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_THERM_DETECTED, "bd71815-therm-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_DEAD, "bd71815-bat-dead"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_SHORTC_RES, "bd71815-bat-short-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_SHORTC_DET, "bd71815-bat-short-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_LOW_VOLT_RES, "bd71815-bat-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_LOW_VOLT_DET, "bd71815-bat-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_VOLT_RES, "bd71815-bat-over-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_VOLT_DET, "bd71815-bat-over-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_MON_RES, "bd71815-bat-mon-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_MON_DET, "bd71815-bat-mon-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON1, "bd71815-bat-cc-mon1"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON2, "bd71815-bat-cc-mon2"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_CC_MON3, "bd71815-bat-cc-mon3"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_1_RES, "bd71815-bat-oc1-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_1_DET, "bd71815-bat-oc1-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_2_RES, "bd71815-bat-oc2-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_2_DET, "bd71815-bat-oc2-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_3_RES, "bd71815-bat-oc3-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_BAT_OVER_CURR_3_DET, "bd71815-bat-oc3-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_LOW_RES, "bd71815-temp-bat-low-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_LOW_DET, "bd71815-temp-bat-low-det"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_HI_RES, "bd71815-temp-bat-hi-res"),
	DEFINE_RES_IRQ_NAMED(BD71815_INT_TEMP_BAT_HI_DET, "bd71815-temp-bat-hi-det"),
};

static struct mfd_cell bd71815_mfd_cells[] = {
	{
		.name = "bd71815-power",
		.num_resources = ARRAY_SIZE(bd71815_power_irqs),
		.resources = &bd71815_power_irqs[0],
	},
};

static const struct regmap_config bd71815_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
/*	.volatile_table = &bd71815_volatile_regs, */
	.max_register = BD71815_MAX_REGISTER - 1,
	.cache_type = REGCACHE_NONE,
};

static unsigned int bit0_offsets[] = {11};		/* RTC IRQ */
static unsigned int bit1_offsets[] = {10};		/* TEMP IRQ */
static unsigned int bit2_offsets[] = {6, 7, 8, 9};	/* BAT MON IRQ */
static unsigned int bit3_offsets[] = {5};		/* BAT IRQ */
static unsigned int bit4_offsets[] = {4};		/* CHG IRQ */
static unsigned int bit5_offsets[] = {3};		/* VSYS IRQ */
static unsigned int bit6_offsets[] = {1, 2};		/* DCIN IRQ */
static unsigned int bit7_offsets[] = {0};		/* BUCK IRQ */

static struct regmap_irq_sub_irq_map bd718xx_sub_irq_offsets[] = {
	REGMAP_IRQ_MAIN_REG_OFFSET(bit0_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit1_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit2_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit3_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit4_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit5_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit6_offsets),
	REGMAP_IRQ_MAIN_REG_OFFSET(bit7_offsets),
};

static const struct regmap_irq bd71815_irqs[] = {
	REGMAP_IRQ_REG(BD71815_INT_BUCK1_OCP, 0, BD71815_INT_BUCK1_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK2_OCP, 0, BD71815_INT_BUCK2_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK3_OCP, 0, BD71815_INT_BUCK3_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK4_OCP, 0, BD71815_INT_BUCK4_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BUCK5_OCP, 0, BD71815_INT_BUCK5_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_OVP, 0, BD71815_INT_LED_OVP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_OCP, 0, BD71815_INT_LED_OCP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_LED_SCP, 0, BD71815_INT_LED_SCP_MASK),
	/* DCIN1 interrupts */
	REGMAP_IRQ_REG(BD71815_INT_DCIN_RMV, 1, BD71815_INT_DCIN_RMV_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CLPS_OUT, 1, BD71815_INT_CLPS_OUT_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CLPS_IN, 1, BD71815_INT_CLPS_IN_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_OVP_RES, 1, BD71815_INT_DCIN_OVP_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_OVP_DET, 1, BD71815_INT_DCIN_OVP_DET_MASK),
	/* DCIN2 interrupts */
	REGMAP_IRQ_REG(BD71815_INT_DCIN_MON_RES, 2, BD71815_INT_DCIN_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_DCIN_MON_DET, 2, BD71815_INT_DCIN_MON_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_WDOG, 2, BD71815_INT_WDOG_MASK),
	/* Vsys */
	REGMAP_IRQ_REG(BD71815_INT_VSYS_UV_RES, 3, BD71815_INT_VSYS_UV_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_UV_DET, 3, BD71815_INT_VSYS_UV_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_LOW_RES, 3, BD71815_INT_VSYS_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_LOW_DET, 3, BD71815_INT_VSYS_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_MON_RES, 3, BD71815_INT_VSYS_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_VSYS_MON_DET, 3, BD71815_INT_VSYS_MON_DET_MASK),
	/* Charger */
	REGMAP_IRQ_REG(BD71815_INT_CHG_WDG_TEMP, 4, BD71815_INT_CHG_WDG_TEMP_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_WDG_TIME, 4, BD71815_INT_CHG_WDG_TIME_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RECHARGE_RES, 4, BD71815_INT_CHG_RECHARGE_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RECHARGE_DET, 4, BD71815_INT_CHG_RECHARGE_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_RANGED_TEMP_TRANSITION, 4,
		       BD71815_INT_CHG_RANGED_TEMP_TRANSITION_MASK),
	REGMAP_IRQ_REG(BD71815_INT_CHG_STATE_TRANSITION, 4, BD71815_INT_CHG_STATE_TRANSITION_MASK),
	/* Battery */
	REGMAP_IRQ_REG(BD71815_INT_BAT_TEMP_NORMAL, 5, BD71815_INT_BAT_TEMP_NORMAL_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_TEMP_ERANGE, 5, BD71815_INT_BAT_TEMP_ERANGE_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_REMOVED, 5, BD71815_INT_BAT_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_DETECTED, 5, BD71815_INT_BAT_DETECTED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_THERM_REMOVED, 5, BD71815_INT_THERM_REMOVED_MASK),
	REGMAP_IRQ_REG(BD71815_INT_THERM_DETECTED, 5, BD71815_INT_THERM_DETECTED_MASK),
	/* Battery Mon 1 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_DEAD, 6, BD71815_INT_BAT_DEAD_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_SHORTC_RES, 6, BD71815_INT_BAT_SHORTC_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_SHORTC_DET, 6, BD71815_INT_BAT_SHORTC_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_LOW_VOLT_RES, 6, BD71815_INT_BAT_LOW_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_LOW_VOLT_DET, 6, BD71815_INT_BAT_LOW_VOLT_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_VOLT_RES, 6, BD71815_INT_BAT_OVER_VOLT_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_VOLT_DET, 6, BD71815_INT_BAT_OVER_VOLT_DET_MASK),
	/* Battery Mon 2 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_MON_RES, 7, BD71815_INT_BAT_MON_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_MON_DET, 7, BD71815_INT_BAT_MON_DET_MASK),
	/* Battery Mon 3 (Coulomb counter) */
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON1, 8, BD71815_INT_BAT_CC_MON1_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON2, 8, BD71815_INT_BAT_CC_MON2_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_CC_MON3, 8, BD71815_INT_BAT_CC_MON3_MASK),
	/* Battery Mon 4 */
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_1_RES, 9, BD71815_INT_BAT_OVER_CURR_1_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_1_DET, 9, BD71815_INT_BAT_OVER_CURR_1_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_2_RES, 9, BD71815_INT_BAT_OVER_CURR_2_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_2_DET, 9, BD71815_INT_BAT_OVER_CURR_2_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_3_RES, 9, BD71815_INT_BAT_OVER_CURR_3_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_BAT_OVER_CURR_3_DET, 9, BD71815_INT_BAT_OVER_CURR_3_DET_MASK),
	/* Temperature */
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_LOW_RES, 10, BD71815_INT_TEMP_BAT_LOW_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_LOW_DET, 10, BD71815_INT_TEMP_BAT_LOW_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_HI_RES, 10, BD71815_INT_TEMP_BAT_HI_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_BAT_HI_DET, 10, BD71815_INT_TEMP_BAT_HI_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_125_RES, 10,
		       BD71815_INT_TEMP_CHIP_OVER_125_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_125_DET, 10,
		       BD71815_INT_TEMP_CHIP_OVER_125_DET_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_VF_RES, 10,
		       BD71815_INT_TEMP_CHIP_OVER_VF_RES_MASK),
	REGMAP_IRQ_REG(BD71815_INT_TEMP_CHIP_OVER_VF_DET, 10,
		       BD71815_INT_TEMP_CHIP_OVER_VF_DET_MASK),
	/* RTC Alarm */
	REGMAP_IRQ_REG(BD71815_INT_RTC0, 11, BD71815_INT_RTC0_MASK),
	REGMAP_IRQ_REG(BD71815_INT_RTC1, 11, BD71815_INT_RTC1_MASK),
	REGMAP_IRQ_REG(BD71815_INT_RTC2, 11, BD71815_INT_RTC2_MASK),
};

static struct regmap_irq_chip bd71815_irq_chip = {
	.name = "bd71815_irq",
	.main_status = BD71815_REG_INT_STAT,
	.irqs = &bd71815_irqs[0],
	.num_irqs = ARRAY_SIZE(bd71815_irqs),
	.status_base = BD71815_REG_INT_STAT_01,
	.mask_base = BD71815_REG_INT_EN_01,
	.ack_base = BD71815_REG_INT_STAT_01,
	.mask_invert = true,
	.init_ack_masked = true,
	.num_regs = 12,
	.num_main_regs = 1,
	.sub_reg_offsets = &bd718xx_sub_irq_offsets[0],
	.num_main_status_bits = 8,
	.irq_reg_stride = 1,
};

static void update_register_vals(int iterator);

#if 0
static struct power_supply_temp_degr battery_temp_dgr_table[] = {
	{
		.temp_set_point = -20, /* -2 C - unit is 0.1 C */
		.degrade_at_set = 200, /* uAh */
		.temp_degrade_1C = 5, /* uAh / degree C */
	},
	{
		.temp_set_point = 200,
		.degrade_at_set = 100,
		.temp_degrade_1C = 2,
	},
	{
		.temp_set_point = -100,
		.degrade_at_set = 400,
		.temp_degrade_1C = 7,
	},
	{
		.temp_set_point = 400,
		.degrade_at_set = 0,
		.temp_degrade_1C = 1,
	},
};
#endif

/* uV - voltage level where capacity zero point adjustment should be taken in
 * use
 */
//#define TEST_THR_VOLTAGE 4350000
/* uV - minimum voltage where system is operational (at SOC = 0 ) */
//#define TEST_MIN_VOLTAGE 3400000
/* uAh */
//#define TEST_DEGRADE_PER_CYCLE 169
/* 1 second */
//#define TEST_JITTER_DEFAULT 9999999

/* See bd71827_voltage_to_capacity */

/* Units of 1 C Degree */
static int test_get_temp(int iter)
{
	return TEST_TEMP;
}

/* Return current in uA */
static int bd71815_get_current(int iter)
{
	return test_current[iter % ARRAY_SIZE(test_current)];
}
/*
static int test_get_uah(int iter)
{
	return test_uah[iter % ARRAY_SIZE(test_uah)];
}
*/

static int test_get_time(int iter)
{
	return test_time[iter % ARRAY_SIZE(test_vsys)];
}

static int test_get_vsys_uv(int iter)
{
	return test_vsys[iter % ARRAY_SIZE(test_vsys)];
}

int get_delta_ccntd(int iter)
{
	return test_ccntd[iter % ARRAY_SIZE(test_ccntd)];
}

enum {
	STATE_DISCHARGING,
	STATE_CHARGING,
};

/* TODO: See from battery data what is the change of charge/time-unit */
bool test_is_relaxed(int iter)
{
	return false;
}

static void swgauge_test_soc(struct platform_device *pdev)
{
	int i, ret;
	struct simple_gauge *g;
	enum power_supply_property psp;
	union power_supply_propval soc, chg, chg_des, chg_now, cyc;

	g = dev_get_drvdata(&pdev->dev);

	for (i = 0; i < VALUES * 25; i++) {
		psp = POWER_SUPPLY_PROP_CAPACITY;
		ret = power_supply_get_property(g->psy, psp, &soc);
		psp = POWER_SUPPLY_PROP_CHARGE_FULL;
		ret |= power_supply_get_property(g->psy, psp, &chg);
		psp = POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN;
		ret |= power_supply_get_property(g->psy, psp, &chg_des);
		psp = POWER_SUPPLY_PROP_CHARGE_NOW;
		ret |= power_supply_get_property(g->psy, psp, &chg_now);
		psp = POWER_SUPPLY_PROP_CYCLE_COUNT;
		ret |= power_supply_get_property(g->psy, psp, &cyc);
		if (ret) {
			if ( i != 0 )
				pr_err("vituix man\n");
			continue;
		} else {
			pr_info("i=%u/%u SOC=%u FULL=%u DESIGN=%u NOW=%u cyc=%u, curr_iter %u\n",
				 i + 1, VALUES, soc.intval, chg.intval, chg_des.intval,
				 chg_now.intval, cyc.intval, (i%VALUES) + 1);
		}
		/* Set next set of simulated values to 'registers' */
		update_register_vals(i + 1);
		/* Run the gauge loop and compute new SOC etc */
		simple_gauge_run_blocking(g);
	}

	platform_device_put(pdev);
}

static u8 g_reg_arr[BD71815_MAX_REGISTER] = { 0 };

static int test_regmap_read(void *ctx, unsigned int reg, unsigned int *val)
{
	u8 *reg_base = ctx;

	*val = reg_base[reg];

	return 0;
}

static int test_regmap_write(void *ctx, unsigned int reg, unsigned int val)
{
	u8 *reg_base = ctx;

	reg_base[reg] = (u8)val;

	return 0;
}

const struct regmap_bus bd71815_test_bus = {
	.fast_io = true,
	.reg_write = test_regmap_write,
	.reg_read = test_regmap_read,
};

static void set_temp_registers(int temp)
{
	u8 btemp_vth = 200 - temp;

	g_reg_arr[BD71815_REG_VM_BTMP] = btemp_vth;
}
/*
 * With 10MOhm Rsens the register value corresponds to current in mA.
 * High bit indicates direction - set => discharging, unset => Charging.
 */
static void set_current_regs(int current_ua)
{
	u16 *curr_reg = (u16 *)&g_reg_arr[BD71815_REG_CC_CURCD_U];

	*curr_reg = cpu_to_be16(current_ua/1000);

	if (current_ua < 0)
		g_reg_arr[BD71815_REG_CC_CURCD_U] |= 0x80;
	else
		g_reg_arr[BD71815_REG_CC_CURCD_U] &= (~0x80);
}

static void set_ccntd(int32_t d_ccntd)
{
	uint32_t ccntd;
	u32 *ccntdptr = (u32 *)&g_reg_arr[BD71815_REG_CC_CCNTD_3];

	ccntd = be32_to_cpu(*ccntdptr);

	ccntd += d_ccntd;
	*ccntdptr = cpu_to_be32(ccntd);
}

/*
 * Register value is mV - 13 bits.
 * We set this from same voltage as Vsys(?)
 */
static void set_vbat_avg(int voltage_uv)
{
	u16 *vbat_avg_reg = (u16 *)&g_reg_arr[BD71815_REG_VM_SA_VBAT_U];

	*vbat_avg_reg = cpu_to_be16(0x1FFF & (voltage_uv/1000));

}

static void set_min_vsys(int voltage_uv)
{
	u16 *min_reg = (u16 *)&g_reg_arr[BD71815_REG_VM_SA_VSYS_MIN_U];
	u16 min;

	min = be16_to_cpu(*min_reg);
	if (!min || min > voltage_uv / 1000)
		*min_reg = cpu_to_be16(*min_reg);
}

/*
 * uA TODO: Are these configurable? Can we trust them to be always set to
 * these values
 */
/* uA */
#define BD7181X_CHG_TERM_CURRENT 50000
/* Units of 1 C Degree */
#define MIN_FULL_CHG_TEMP       15
#define MAX_FULL_CHG_TEMP       45
/* Seconds */
#define THR_RELAX_TIME ((60 * 60) - 10)
/* uA */
#define THR_RELAX_CURRENT 5000

static void set_charge_status(int curr, int temp)
{
	uint8_t *reg = &g_reg_arr[BD71815_REG_CHG_STATE];
	static int prev_current = BD7181X_CHG_TERM_CURRENT;

	if (curr >= BD7181X_CHG_TERM_CURRENT) {
		/* Charging */
		*reg = 0x0E;
	} else if (curr < -BD7181X_CHG_TERM_CURRENT) {
		/* Discharging */
		*reg = 0x00;
	} else if ((prev_current >= BD7181X_CHG_TERM_CURRENT ) && (curr >=
		 -BD7181X_CHG_TERM_CURRENT) &&
		 (curr <= BD7181X_CHG_TERM_CURRENT)) {
		/* Full */
		*reg = 0x0F;

		if ((temp >= MIN_FULL_CHG_TEMP) && (temp <= MAX_FULL_CHG_TEMP)) {
			u8 *reg_full = &g_reg_arr[BD71815_REG_FULL_CCNTD_3];
			u8 *reg_cc   = &g_reg_arr[BD71815_REG_CC_CCNTD_3];
			int i;

			for (i = 0; i < 4; i++)
				reg_full[i] = reg_cc[i];
		}
	}
}

/*
 * The bd71827-power.c does not utilize rex_cc - only the
 * REG_REX_SA_VBAT. No need to set REX_CC.
 */
void set_relax_status(int curr, int time)
{
	static int rex_time = 0;
	u8 *rex_vbat = &g_reg_arr[BD71815_REG_REX_SA_VBAT_U];
	u8 *avg_vbat = &g_reg_arr[BD71815_REG_VM_SA_VBAT_U];

	if (curr <= THR_RELAX_CURRENT && curr >= -THR_RELAX_CURRENT)
		rex_time += time;
	else
		rex_time = 0;

	/* Set both high and low registers. They're in consecutive addresses */
	if (rex_time >= THR_RELAX_TIME) {
		rex_vbat[0] = avg_vbat[0];
		rex_vbat[1] = avg_vbat[1];
	} else {
		rex_vbat[0] = 0;
		rex_vbat[1] = 0;
	}
}

static void initialize_initial_ocv_regs(int uv)
{
	u16 *pre_reg = (u16 *)&g_reg_arr[BD71815_REG_VM_OCV_PRE_U],
	    *post_reg = (u16 *)&g_reg_arr[BD71815_REG_VM_OCV_PST_U];

	*pre_reg = cpu_to_be16(uv/1000);
	*post_reg = cpu_to_be16(uv/1000);
}

/*
 * Read voltages etc from the measured battery-data.
 * Compute the register values according to this information.
 */
static void update_register_vals(int iterator)
{

	int temp, delta_ccntd, curr, voltage, time;

	temp = test_get_temp(iterator);
	time = test_get_time(iterator);
	delta_ccntd = get_delta_ccntd(iterator);
	curr = bd71815_get_current(iterator);
	voltage = test_get_vsys_uv(iterator);

	if (!iterator)
		initialize_initial_ocv_regs(voltage);

	set_temp_registers(temp);
	set_ccntd(delta_ccntd);
	set_current_regs(curr);
	set_charge_status(curr, temp);
	/*
	 * TODO: Ask from Shimada-san why VBAT and VSYS can both be set from
	 * the same voltage. What voltage is measured and why VBAT and VSYS
	 * are always equal? Is this a safe assumption?
	 *
	 * REG_VM_SA_VBAT_MAX, REG_VM_SA_VBAT_MIN, REG_VM_SA_VSYS_MAX are not
	 * used by the driver. No need to set them.
	 */
	set_vbat_avg(voltage);
	set_min_vsys(voltage);
	/*
 	 * This uses the VBAT_AVG to initialize VBAT_REX => The vbat_avg
 	 * must've been set (set_vbat_avg() been called) before this.
 	 * It might be safer to call this from the set_vbat_avg().
 	 */
	set_relax_status(curr, time);
}

static int initialize_register_vals(void)
{
	/* Say we just connected the battery => Driver should initialize CC */
	g_reg_arr[BD71815_REG_CONF] &= ~BD71815_MASK_CONF_XSTB;
	/*
	 * Set the initial battery OCV values BD71815_REG_VM_OCV_PRE_U,
	 * BD71815_REG_VM_OCV_PST_U,
	 */

	/*
	 * Just update the registers according to the first measurement
	 * so that the HW can estimate the initial CC based on VBAT or ??
	 */
	update_register_vals(0);

	return 0;
}

static int test_probe(struct platform_device *pdev)
{
	struct regmap_irq_chip_data *irq_data;
	int ret;
	struct regmap *regmap;
	struct regmap_irq_chip *irqchip;
	int irq = platform_get_irq(pdev, 0);
	struct device *dev = &pdev->dev;

	irqchip = &bd71815_irq_chip;

	regmap = devm_regmap_init(dev, &bd71815_test_bus, &g_reg_arr,
				  &bd71815_regmap);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to initialize Regmap\n");
		return PTR_ERR(regmap);
	}

	ret = devm_regmap_add_irq_chip(dev, regmap, irq,
				       IRQF_ONESHOT, 0, &bd71815_irq_chip, &irq_data);
	if (ret) {
		dev_err(dev, "Failed to add IRQ chip\n");
		return ret;
	}

	initialize_register_vals();

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO, bd71815_mfd_cells,
				   ARRAY_SIZE(bd71815_mfd_cells), NULL, 0,
				   regmap_irq_get_domain(irq_data));
	if (ret) {
		dev_err(dev, "Failed to create subdevices\n");
		return ret;
	}

	swgauge_test_soc(pdev);

	return 0;
}

static const struct of_device_id test_of_match[] = {
	{ .compatible = "rohm,test-swgauge", },
	{ },
};
MODULE_DEVICE_TABLE(of, test_of_match);

static struct platform_driver test_driver = {
	.driver = {
		   .name = "test-swgauge",
		   .owner = THIS_MODULE,
		   .of_match_table = test_of_match,
		   },
	.probe = test_probe,
};

module_platform_driver(test_driver);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71837 voltage regulator driver");
MODULE_LICENSE("GPL");
