// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 ROHM Semiconductor
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <power/pmic.h>
#include <power/bd71891.h>

/*
 * TODO: Create BD71891 GPI driver using common functions which can be split
 * out of the bd71892_gpio.c
 */

