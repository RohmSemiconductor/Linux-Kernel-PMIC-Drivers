// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright 2025 ROHM Semiconductors
 *  Matti Vaittinen <mazziesaccount@gmail.com>
 */

#include <dm.h>
#include <power/pmic.h>

#include "bdxxxx.h"

/*
 * This function just hard-codes the BD71885 PMIC dt node-name and seeks
 * the udev based on this. It really is not the way to go - but as this
 * code is only used as a referene for SCF code building a nice u-boot
 * specific way of obtaining the device does not warrant the effort.
 *
 * TODO: Revise  this if the u-boot driver is ever to be sent upstream.
 * We could probably do:
 *	for (ret = uclass_first_device(UCLASS_PMIC, &dev); dev;
 *	     ret = uclass_next_device(&dev)) {
 *
 * and see if the driver is bd71885 driver. That should work for one piece
 * of BD71885 at a time. We could also allow user to specify the device to
 * access (similar to the pmic/regulator commands).
 */
struct udevice *get_currdev(const char *name)
{
	static struct udevice *currdev;
	int ret;

	if (!currdev) {
		ret = pmic_get(name, &currdev);
		if (ret) {
			printf("Can't get PMIC: %s!\n", name);
			return NULL;
		}

		printf("dev: %d @ %s\n", dev_seq(currdev), currdev->name);
	}

	return currdev;
}

int print_reason_reg(const char *pmicname, const struct reason_reg *r,
		     int mask, bool bitfield)
{
	const struct reason_info *reas;
	struct udevice *currdev;
	int ret;

	currdev = get_currdev(pmicname);
	if (!currdev)
		return -ENODEV;

	ret = pmic_reg_read(currdev, r->reg);
	if (ret < 0)
		return ret;

	printf("%s:\n", r->explanation);
	if (bitfield)
		for_each_valid_reason_bf(r, reas, ret)
			printf("\t%s\n", reas->reason);
	else
		for_each_valid_reason_field(r, reas, mask, ret)
			printf("\t%s\n", reas->reason);

	printf("---\n\n");

	return 0;
}

