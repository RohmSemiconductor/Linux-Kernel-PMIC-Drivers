// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright 2022 ROHM Semiconductors
 *  Matti Vaittinen <mazziesaccount@gmail.com>
 */

#ifndef __BDXXXX_H
#define __BDXXXX_H

#include <command.h>
#include <errno.h>

/* Pointer to the selected PMIC device. */
struct udevice *get_currdev(const char *name);

struct reason_info {
	const char *reason;
	u8 value;
};

struct reason_reg {
	const char *explanation;
	const struct reason_info *reasons;
	u8 num_reasons;
	u8 reg;
};

struct reason_reg_field {
	struct reason_reg reason_reg;
	u8 mask;
};

#define REASON_INFO(_str, _val)		\
{ .reason = (_str), .value = (_val), }

#define for_each_reason(reas_reg, reas)					\
	for (int __index = 0;						\
	    __index < (reas_reg)->num_reasons &&			\
	    ((reas) = &(reas_reg)->reasons[__index]);			\
	    __index++)

#define for_each_valid_reason_bf(_reas_reg, _reas, _value)			\
	for_each_reason(_reas_reg, _reas)					\
	     if (!((_reas)->value & (_value))) {} else

#define for_each_valid_reason_field(_reas_reg, _reas, _mask, _value)		\
	for_each_reason(_reas_reg, _reas)					\
	     if ((((_reas)->value & (_mask)) != ((_value)&(_mask)))) {} else

static inline int cmd_failure(int ret)
{
	printf("Error: %d (%s)\n", ret, errno_str(ret));

	return CMD_RET_FAILURE;
}

static inline int cmd_ret(int ret)
{
	if (!ret)
		return CMD_RET_SUCCESS;

	return cmd_failure(ret);
}

int print_reason_reg(const char *pmicname, const struct reason_reg *r,
		     int mask, bool bitfield);
static inline int print_reason_field_reg(const char *pmicname,
					 const struct reason_reg_field *f)
{
	return print_reason_reg(pmicname, &f->reason_reg,
				f->mask, false);
}

static inline int print_reason_bf_reg(const char *pmicname,
				      const struct reason_reg *r)
{
	return print_reason_reg(pmicname, r, 0, true);
}

static inline int bdxxxx_write(struct udevice *dev, uint reg, const uint8_t *buff,
			 int len)
{
	if (dm_i2c_write(dev, reg, buff, len)) {
		pr_err("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static inline int bdxxxx_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	int ret;

	ret = dm_i2c_read(dev, reg, buff, len);
	if (ret)
		pr_err("read error (%d) from device: %p register: %#x!",
		       ret, dev, reg);

	return ret;
}

static inline int bdxxxx_bind(struct udevice *dev, const struct pmic_child_info *pmic_children_info)
{
	int children;
	ofnode regulators_node;

	regulators_node = dev_read_subnode(dev, "regulators");
	if (!ofnode_valid(regulators_node)) {
		debug("%s: %s regulators subnode not found!\n", __func__,
		      dev->name);
		return -ENXIO;
	}

	debug("%s: '%s' - found regulators subnode\n", __func__, dev->name);

	if (CONFIG_IS_ENABLED(PMIC_CHILDREN)) {
		children = pmic_bind_children(dev, regulators_node, pmic_children_info);
		if (!children)
			debug("%s: %s - no child found\n", __func__, dev->name);
	}
	/* Always return success for this device */
	return 0;
}

#endif //__BDXXXX_H
