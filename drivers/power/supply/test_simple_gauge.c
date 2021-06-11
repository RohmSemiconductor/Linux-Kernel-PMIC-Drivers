// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for the linear_ranges helper.
 *
 * Copyright (C) 2020, ROHM Semiconductors.
 * Author: Matti Vaittinen <matti.vaittien@fi.rohmeurope.com>
 */

//#define DRIVER_DT_TEST
#define KUNIT_TEST

#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/simple_gauge.h>
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

extern volatile int TEST_loop;
extern volatile int LOST_teep;

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
#define TEST_THR_VOLTAGE 4350000
/* uV - minimum voltage where system is operational (at SOC = 0 ) */
#define TEST_MIN_VOLTAGE 3400000
/* uAh */
#define TEST_DEGRADE_PER_CYCLE 169
/* 1 second */
#define TEST_JITTER_DEFAULT 9999999
//#define TEST_TEMP 250

#define test_age_correct_cap NULL
#define test_temp_correct_cap NULL
#define test_calibrate NULL
#define test_suspend_calibrate NULL
#define NUM_BAT_PARAMS 23
#define SOC_ZERO_INDEX (NUM_BAT_PARAMS - 2)


static int soc_table[NUM_BAT_PARAMS] = {
	1000,
	1000,
	950,
	900,
	850,
	800,
	750,
	700,
	650,
	600,
	550,
	500,
	450,
	400,
	350,
	300,
	250,
	200,
	150,
	100,
	50,
	0,
	-50
	/* unit 0.1% */
};

static int ocv_table[NUM_BAT_PARAMS] = {
	4400000,
	4375377,
	4314793,
	4257284,
	4200969,
	4146652,
	4094464,
	4048556,
	3997034,
	3959858,
	3917668,
	3860165,
	3837491,
	3817893,
	3801408,
	3788071,
	3775836,
	3752263,
	3732625,
	3698262,
	3680138,
	3637500,
	2668849
};	/* unit 1 micro V */

static int vdr_table_h[NUM_BAT_PARAMS] = {
	100,
	100,
	102,
	104,
	107,
	110,
	114,
	122,
	126,
	139,
	155,
	94,
	107,
	113,
	120,
	129,
	113,
	104,
	110,
	109,
	116,
	128,
	525
};

static int vdr_table_m[NUM_BAT_PARAMS] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	98,
	95,
	90,
	85,
	85,
	90,
	93,
	95,
	98,
	98,
	100,
	106,
	109,
	130,
	451
};

static int vdr_table_l[NUM_BAT_PARAMS] = {
	100,
	100,
	98,
	96,
	95,
	96,
	98,
	110,
	105,
	108,
	95,
	86,
	87,
	90,
	92,
	96,
	102,
	109,
	111,
	122,
	144,
	219,
	393
};

static int vdr_table_vl[NUM_BAT_PARAMS] = {
	100,
	100,
	98,
	95,
	94,
	94,
	95,
	105,
	96,
	97,
	87,
	84,
	84,
	86,
	90,
	93,
	101,
	110,
	117,
	130,
	157,
	195,
	31,
};

/* See bd71827_voltage_to_capacity */
static int test_get_soc_by_ocv(struct simple_gauge *sw __attribute__((unused)),
			       int ocv, int temp __attribute__((unused)),
			       int *soc);
static int test_get_soc_by_ocv(struct simple_gauge *sw, int ocv, int temp, int *soc)
{
	int i = 0;

	if (ocv > ocv_table[0]) {
		*soc = soc_table[0];
	} else {
		for (i = 0; soc_table[i] != -50; i++) {
			if ((ocv <= ocv_table[i]) && (ocv > ocv_table[i+1])) {
				*soc = (soc_table[i] - soc_table[i+1]) *
				      (ocv - ocv_table[i+1]) /
				      (ocv_table[i] - ocv_table[i+1]);
				*soc += soc_table[i+1];
				break;
			}
		}
		if (soc_table[i] == -50)
			*soc = soc_table[i];
	}

	return 0;
}


static int test_get_ocv_by_soc(struct simple_gauge *sw, int dsoc, int temp, int *ocv)
{
	int i = 0;

	if (dsoc > soc_table[0]) {
		*ocv = TEST_MAX_VOLTAGE;
		return 0;
	}
	if (dsoc == 0) {
		*ocv = ocv_table[SOC_ZERO_INDEX];
		return 0;
	}

	i = 0;
	while (i < NUM_BAT_PARAMS - 1) {
		if ((dsoc <= soc_table[i]) && (dsoc > soc_table[i+1])) {
			*ocv = (ocv_table[i] - ocv_table[i+1]) *
			       (dsoc - soc_table[i+1]) / (soc_table[i] -
			       soc_table[i+1]) + ocv_table[i+1];
			return 0;
		}
		i++;
	}

	*ocv = ocv_table[22];

	return 0;
}

static bool test_is_relaxed(struct simple_gauge *gauge, int *rex_volt)
{
	/* TODO: Add stub for testing relax voltage set CC stuff */
	return 0;
}

static int test_get_temp(struct simple_gauge *gauge, int *temp)
{
	*temp = TEST_TEMP;

	return 0;
}

//static int test_uah[] = {};
static int test_get_uah(struct simple_gauge *gauge, int *uah)
{
	static int iter = 0;

	*uah = test_uah[iter % ARRAY_SIZE(test_uah)];
	iter ++;

	return 0;
}

static int test_get_uah_from_full(struct simple_gauge *gauge, int *uah)
{
	pr_err("Should not be here!\n");

	return -EINVAL;
}

static int test_update_cc_uah(struct simple_gauge *gauge, int bcap)
{
	pr_err("Should not be here!\n");

	return -EINVAL;
}

static unsigned long int cycle_iter = 0;

static int test_set_cycle(struct simple_gauge *gauge, int old, int *new_cycle)
{
	cycle_iter = VALUES * *new_cycle;

	return 0;
}

static int test_get_cycle(struct simple_gauge *gauge, int *cycle)
{

	cycle_iter ++;
	*cycle = cycle_iter/VALUES;

	return 0;
}

//static int test_vsys[] = { };

static int test_get_vsys(struct simple_gauge *gauge, int *uv)
{
	static int iter = 0;

	*uv = test_vsys[iter % ARRAY_SIZE(test_vsys)];
	iter ++;

	return 0;
}

static void calc_vdr(int *res, int *vdr, int temp, int dgrd_temp,
		     int *vdr_hi, int dgrd_temp_hi, int items)
{
	int i;

	for (i = 0; i < items; i++)
		res[i] = vdr[i] + (temp - dgrd_temp) * (vdr_hi[i] - vdr[i]) /
			 (dgrd_temp_hi - dgrd_temp);
}

/* get VDR(Voltage Drop Rate) value by SOC */
static int test_get_vdr(int dsoc, int temp)
{
	int i = 0;
	int vdr = 100;
	int vdr_table[NUM_BAT_PARAMS];

	/* Calculate VDR by temperature */
	if (temp >= DGRD_TEMP_H_DEFAULT)
		for (i = 0; i < NUM_BAT_PARAMS; i++)
			vdr_table[i] = vdr_table_h[i];
	else if (temp >= DGRD_TEMP_M_DEFAULT)
		calc_vdr(vdr_table, vdr_table_m, temp, DGRD_TEMP_M_DEFAULT,
			 vdr_table_h, DGRD_TEMP_H_DEFAULT,
			 NUM_BAT_PARAMS);
	else if (temp >= DGRD_TEMP_L_DEFAULT)
		calc_vdr(vdr_table, vdr_table_l, temp, DGRD_TEMP_L_DEFAULT,
			 vdr_table_m, DGRD_TEMP_M_DEFAULT,
			 NUM_BAT_PARAMS);
	else if (temp >= DGRD_TEMP_VL_DEFAULT)
		calc_vdr(vdr_table, vdr_table_vl, temp,
			 DGRD_TEMP_VL_DEFAULT, vdr_table_l, DGRD_TEMP_L_DEFAULT,
			 NUM_BAT_PARAMS);
	else
		for (i = 0; i < NUM_BAT_PARAMS; i++)
			vdr_table[i] = vdr_table_vl[i];

	if (dsoc > soc_table[0]) {
		vdr = 100;
	} else if (dsoc == 0) {
		vdr = vdr_table[NUM_BAT_PARAMS - 2];
	} else {
		for (i = 0; i < NUM_BAT_PARAMS - 1; i++)
			if ((dsoc <= soc_table[i]) && (dsoc > soc_table[i+1])) {
				vdr = (vdr_table[i] - vdr_table[i+1]) *
				      (dsoc - soc_table[i+1]) /
				      (soc_table[i] - soc_table[i+1]) +
				      vdr_table[i+1];
				break;
			}
		if (i == NUM_BAT_PARAMS - 1)
			vdr = vdr_table[i];
	}
	return vdr;
}

static int test_zero_cap_adjust(struct simple_gauge *sw, int *effective_cap,
				int cc_uah, int vbat, int temp)
{
	int ocv_table_load[NUM_BAT_PARAMS];
	int i, ret;
	int ocv;
	int dsoc;
	int vdrop;
	unsigned int dsoc_at_newzero;
	int hack = 0;
	int hack2 = 0;

	/* Debug hack to display vdrop calc for strange values */
#if 0
	if (vbat == 3511871) {
		hack2 = 1;
		pr_info("******************* PROBLEM ROUND? *******************\n");
	}
	if (vbat == 3511871 || vbat == 3512438) {
		hack = 1;
		if (!hack2)
		pr_info("******************* REFERENCE ROUND *******************\n");
	}
#endif
	/*
	 * Calculate SOC from CC and effective battery cap.
	 * Use unit of 0.1% for dsoc to improve accuracy
	 */
	dsoc = cc_uah * 1000 / *effective_cap;

//	pr_info("cc_uah before zero-adjust %u\n", cc_uah);
//	pr_info("dcap at zero-adjust %u\n", dsoc);

	ret = test_get_ocv_by_soc(sw, dsoc, 0, &ocv);
	if (ret)
		return ret;

	vdrop = ocv - vbat;

//	pr_info("OCV corresponding dcap: %d\n", ocv);
//	pr_info("vbat: %d\n", vbat);
//	pr_info("vdrop: %d\n", vdrop);
//	pr_info("Look for SOC when OCV - vdrop is below sys limit (%d)\n",
	//	TEST_MIN_VOLTAGE);

	for (i = 1; i < NUM_BAT_PARAMS; i++) {
		ocv_table_load[i] = ocv_table[i] - vdrop;
/*		pr_info("ocv_table[i] %d - vdrop %d = %d (limit %d) - %s\n",
			ocv_table[i], vdrop, ocv_table_load[i],
			TEST_MIN_VOLTAGE,
			(ocv_table_load[i] <= TEST_MIN_VOLTAGE) ?
				"Found": "Not Found");
*/
		if (ocv_table_load[i] <= TEST_MIN_VOLTAGE) {
			break;
		}
	}

//	pr_info("SOC index near estimated system min voltage (%d), is %d\n",
//		 ocv_table_load[i], i);
	dsoc_at_newzero = soc_table[i];

	/*
	 * For improved accuracy ROHM helps customers to measure some
	 * battery voltage drop curves to do further SOC estimation improvement.
	 * If VDR tables are available we perform these corrections.
	 */
	if (i < NUM_BAT_PARAMS) {
		int j, k, m;
		int dv;
		int lost_cap, new_lost_cap;
		int dsoc0;
		int vdr, vdr0;

		dv = (ocv_table_load[i-1] - ocv_table_load[i]) / 5;
		for (j = 1; j < 5; j++) {
			if ((ocv_table_load[i] + dv * j) >
			    TEST_MIN_VOLTAGE) {
				break;
			}
		}
//		pr_info("Fine-tuned index is %d\n", j);
		dsoc_at_newzero += j * 10;
//		pr_info("New dsoc_at_zero = %d\n", dsoc_at_newzero);
//		pr_info("lost cap from dsoc_at_zero = %d\n", dsoc_at_newzero / 10 * *effective_cap / 100);
//		pr_info("((%d - %d) * 5 + (%d - 1)) * %d/100\n",
		//	NUM_BAT_PARAMS - 2, i, j, *effective_cap);
//		pr_info("SOC scaler by VDROP=%d\n",
//			((NUM_BAT_PARAMS - 2 - i) * 5 + (j - 1)));

		lost_cap = ((NUM_BAT_PARAMS - 2 - i) * 5 + (j - 1)) *
			    *effective_cap / 100;

//		pr_info("Lost cap from vdrop %d\n", lost_cap);

		for (m = 0; m < soc_est_max_num; m++) {
			new_lost_cap = lost_cap;
			dsoc0 = lost_cap * 1000 / *effective_cap;

//			if (hack)
//				pr_info("dsoc0 by lost cap = %d, m=%d\n", dsoc0, m);
			if ((dsoc >= 0 && dsoc0 > dsoc) ||
			    (dsoc < 0 && dsoc0 < dsoc)) {
				dsoc0 = dsoc;
//				if (hack)
//					pr_info("dsoc0 set from dsoc to = %d, m=%d\n", dsoc0, m);
			}

			vdr = test_get_vdr(dsoc, temp);
//			if (hack)
//				pr_info("vdr by dsoc = %d, m=%d\n", vdr, m);

			vdr0 = test_get_vdr(dsoc0, temp);
//			if (hack)
//				pr_info("vdr by dsoc0 = %d\n", vdr0);

			for (k = 1; k < NUM_BAT_PARAMS; k++) {
				ocv_table_load[k] = ocv_table[k] -
						    (ocv - vbat) * vdr0 / vdr;
				if (ocv_table_load[k] <= TEST_MIN_VOLTAGE) {
//					if (hack)
//						pr_info("1. ocv_table_load[k] at break %d, k=%d\n", ocv_table_load[k], k);
					break;
				}
			}
			if (k < NUM_BAT_PARAMS) {
				dv = (ocv_table_load[k-1] -
				     ocv_table_load[k]) / 5;
				for (j = 1; j < 5; j++)
					if ((ocv_table_load[k] + dv * j) >
					     TEST_MIN_VOLTAGE) {
//						if (hack)
//							pr_info("1. ocv_table_load[k]+dv *j  at break %d, j=%d, dv=%d\n", ocv_table_load[k] + dv * j, j, dv);
						break;
					}

				new_lost_cap = ((NUM_BAT_PARAMS - 2 - k) *
						 5 + (j - 1)) *
						*effective_cap / 100;
//				if (hack)
//					pr_info("new_lost_cap=%d, m=%d\n", new_lost_cap, m);
				if (soc_est_max_num == 1)
					lost_cap = new_lost_cap;
				else {
//					if (hack) {
//						pr_info("old lost cap=%d\n", lost_cap);
//						pr_info("Adding %d to old lost cap\n", (new_lost_cap - lost_cap) /(2 * (soc_est_max_num - m)));
//					}
					lost_cap += (new_lost_cap - lost_cap) /
						    (2 * (soc_est_max_num - m));
//					if (hack)
//						pr_info("lost_cap after m=%d is %d\n", m, lost_cap);
				}
			}
			if (new_lost_cap == lost_cap) {
//				if (hack)
//					pr_info("new_lost_cap == lost_cap %d\n", lost_cap);
				break;
			}
		}

//		pr_info("After all vdr table iterations lost cap %d\n", lost_cap);

//		pr_info("Old effective cap %d\n", *effective_cap);
		*effective_cap -= lost_cap;
//		pr_info("New effective cap %d\n", *effective_cap);
	}
//	if (hack2)
//		pr_info("******************* PROBLEM ROUND? *******************\n");
//	if (hack && !hack2)
//		pr_info("******************* REFERENCE ROUND *******************\n");

	return 0;
}

static struct simple_gauge_ops o =
{
	.is_relaxed = test_is_relaxed,
	.get_temp = test_get_temp,
	.get_uah_from_full = test_get_uah_from_full,
	.get_uah = test_get_uah,
	.update_cc_uah = test_update_cc_uah,
	.get_cycle = test_get_cycle,
	.set_cycle = test_set_cycle,
	.get_vsys = test_get_vsys,
#ifndef DRIVER_DT_TEST
	.get_soc_by_ocv = test_get_soc_by_ocv,
	.get_ocv_by_soc = test_get_ocv_by_soc,
	.zero_cap_adjust = test_zero_cap_adjust,
#endif
	.age_correct_cap = test_age_correct_cap,
	.temp_correct_cap = test_temp_correct_cap,
	.calibrate = test_calibrate,
	.suspend_calibrate = test_suspend_calibrate,
};

static struct simple_gauge_desc d =
{
	.poll_interval = TEST_JITTER_DEFAULT,
	.allow_set_cycle = true,
//	.amount_of_temp_dgr = ARRAY_SIZE(battery_temp_dgr_table),
//	.temp_dgr = battery_temp_dgr_table,
	.degrade_cycle_uah = TEST_DEGRADE_PER_CYCLE,
	.cap_adjust_volt_threshold = TEST_THR_VOLTAGE,
	.system_min_voltage = TEST_MIN_VOLTAGE,
#ifndef DRIVER_DT_TEST
	.designed_cap = TEST_DESIGNED_CAP,
#endif
};

static int test_battery_get_property(struct simple_gauge *g,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 1;
		break;
/*	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bd71827_get_vbat(pwr, &tmp);
		val->intval = tmp;
		break;
*/
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
/*	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = curr_avg;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = curr;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = MAX_VOLTAGE_DEFAULT;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		val->intval = TEST_MIN_VOLTAGE;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = MAX_CURRENT_DEFAULT;
		break;
*/
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#define BAT_NAME			"bd71827_bat"
static enum power_supply_property test_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};
#if 0
static int bd70528_prop_is_writable(struct power_supply *psy,
				    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		return 1;
	default:
		break;
	}
	return 0;
}
#endif

static struct simple_gauge_psy test_bat_cfg = {
	.psy_name		= BAT_NAME,
//	.type = POWER_SUPPLY_TYPE_BATTERY,
	.additional_props	= test_battery_props,
	.num_additional_props	= ARRAY_SIZE(test_battery_props),
	.get_custom_property	= test_battery_get_property,
//	.property_is_writeable	= bd70528_prop_is_writable,
};

static void swgauge_test_soc(struct kunit *test)
{
	int i, ret;
	struct simple_gauge *g;
	struct platform_device *pdev = NULL;
	enum power_supply_property psp;
	union power_supply_propval soc, chg, chg_des, chg_now, cyc;

#ifdef DRIVER_DT_TEST
	pdev = (struct platform_device *)test;
	test_bat_cfg.of_node = pdev->dev.of_node;
#endif

	if (!pdev)
		pdev = platform_device_register_simple("test_gauge_device", -1, NULL,
					        0);

	g = psy_register_simple_gauge(&pdev->dev, &test_bat_cfg, &o, &d);

	for (i = 0; i < VALUES * 25; i++) {
//		msleep(70); /* delay until iteration is completed */
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
		simple_gauge_run_blocking(g);
	}

#ifndef DRIVER_DT_TEST
	platform_device_put(pdev);
#endif
//	KUNIT_EXPECT_EQ(test, 0, ret);
//	KUNIT_EXPECT_EQ(test, sel, swgauge2_sels[RANGE2_NUM_VALS - 1]);
//	KUNIT_EXPECT_FALSE(test, found);
}

#ifdef KUNIT_TEST
static struct kunit_case swgauge_test_cases[] = {
	KUNIT_CASE(swgauge_test_soc),
	{},
};

static struct kunit_suite swgauge_test_module = {
	.name = "swgauge-test",
	.test_cases = swgauge_test_cases,
};

kunit_test_suites(&swgauge_test_module);
#endif

#ifdef DRIVER_DT_TEST
static int test_probe(struct platform_device *pdev)
{
	swgauge_test_soc((struct kunit *)pdev);

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
#endif


MODULE_LICENSE("GPL");
