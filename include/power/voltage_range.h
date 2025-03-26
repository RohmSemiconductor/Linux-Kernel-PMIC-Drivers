// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 ROHM Semiconductors
 * Author: Matti Vaittinen <mazziesaccount@gmail.com>
 */

#ifndef __VOLTAGE_RANGE_H
#define __VOLTAGE_RANGE_H

#include <common.h>

#define BD_RANGE(_min, _vstep, _sel_low, _sel_hi) \
{ \
	.min_volt = (_min), .step = (_vstep), .min_sel = (_sel_low), \
	.max_sel = (_sel_hi), \
}


/**
 * struct regulator_vrange - describe linear range of voltages
 *
 * @min_volt:	smallest voltage in range
 * @step:	how much voltage changes at each selector step
 * @min_sel:	smallest selector in the range
 * @max_sel:	maximum selector in the range
 * @rangeval:	register value used to select this range if selectible
 *		ranges are supported
 */
struct regulator_vrange {
	unsigned int	min_volt;
	unsigned int	step;
	u8		min_sel;
	u8		max_sel;
};

int vrange_find_value(const struct regulator_vrange *r, unsigned int sel,
		      unsigned int *val);

int vrange_find_selector(const struct regulator_vrange *r, int val,
			 unsigned int *sel);
#endif
