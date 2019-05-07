/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2018 ROHM Semiconductors */

#ifndef __LINUX_MFD_ROHM_H__
#define __LINUX_MFD_ROHM_H__

#include <linux/regmap.h>
#include <linux/regulator/driver.h>

enum rohm_chip_type {
	ROHM_CHIP_TYPE_BD71837 = 0,
	ROHM_CHIP_TYPE_BD71847,
	ROHM_CHIP_TYPE_BD70528,
	ROHM_CHIP_TYPE_BD71828,
	ROHM_CHIP_TYPE_AMOUNT
};

struct rohm_regmap_dev {
	struct device *dev;
	struct regmap *regmap;
};

enum {
	ROHM_DVS_LEVEL_UNKNOWN,
	ROHM_DVS_LEVEL_RUN,
	ROHM_DVS_LEVEL_IDLE,
	ROHM_DVS_LEVEL_SUSPEND,
	ROHM_DVS_LEVEL_LPSR,
#define ROHM_DVS_LEVEL_MAX ROHM_DVS_LEVEL_LPSR
};

struct rohm_dvs_config {
	uint64_t level_map;
	unsigned int run_reg;
	unsigned int run_mask;
	unsigned int run_on_mask;
	unsigned int idle_reg;
	unsigned int idle_mask;
	unsigned int idle_on_mask;
	unsigned int suspend_reg;
	unsigned int suspend_mask;
	unsigned int suspend_on_mask;
	unsigned int lpsr_reg;
	unsigned int lpsr_mask;
	unsigned int lpsr_on_mask;
};

#if IS_ENABLED(CONFIG_REGULATOR_ROHM)
int rohm_regulator_set_dvs_levels(const struct rohm_dvs_config *dvs,
				  struct device_node *np,
				  const struct regulator_desc *desc,
				  struct regmap *regmap);

#else
static inline int rohm_regulator_set_dvs_levels(const struct rohm_dvs_config *dvs,
						struct device_node *np,
						const struct regulator_desc *desc,
						struct regmap *regmap)
{
	return 0;
}
#endif //IS_ENABLED(CONFIG_REGULATOR_ROHM)

#endif
