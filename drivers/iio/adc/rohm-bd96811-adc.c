// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for ROHM BD96811 PMIC's ADC block
 *
 * Copyright (C) 2023 ROHM Semiconductors
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include <linux/iio/iio.h>

#include <linux/mfd/rohm-bd96811.h>

#define BD96811_REG_AD0_RES 0x52
#define BD96811_REG_AD1_RES 0x50
#define BD96811_REG_AD2_RES 0x51

enum {
	BD96811_CHAN_AD0,
	BD96811_CHAN_AD1,
	BD96811_CHAN_AD2,
	BD96811_NUM_HW_CHAN,
};

static const struct iio_chan_spec bd96811_channels[] = {
	{
		.type = IIO_VOLTAGE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.indexed = 1,
		.channel = BD96811_CHAN_AD0,
	}, {
		.type = IIO_VOLTAGE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.indexed = 1,
		.channel = BD96811_CHAN_AD1,

	}, {
		.type = IIO_VOLTAGE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.indexed = 1,
		.channel = BD96811_CHAN_AD2,

	},
	IIO_CHAN_SOFT_TIMESTAMP(BD96811_NUM_HW_CHAN),
};

struct bd96811_data {
	struct regmap *regmap;
	struct device *dev;
	int reg;
	bool has_adc0;
};

/*
 * The ROHM BD96811 PMIC is a PMIC which is highly configurable depending
 * on the OTP used at production. Amongst other configs, the DOUT3 can be
 * configured to serve as ADC0 input. The DIN4 can be configured to
 * ADC1 and DOUT4 to ADC2 using register interface.
 *
 * Unfortunately the BD96811 does not provide any means to read the OTP
 * configuration. Thus the driver has no way of knowing if the ADC0 is in
 * use. Hence, we require the precense of ADC0 to be explicitly indicated
 * using the device-tree property 'rohm,adc0-enabled'.
 */
static int chan_is_adc(struct bd96811_data *data, int chan)
{
	int val, reg, ret;

	if (chan == BD96811_CHAN_AD0)
		return data->has_adc0;

	reg = BD96811_REG_AD1_TH + chan - 1;

	ret = regmap_read(data->regmap, reg, &val);
	if (ret)
		dev_warn(data->dev, "Failed to read ADC threshold (%d)\n", ret);

	return ret | val;
}

static int bd96811_read_raw(struct iio_dev *idev,
			    struct iio_chan_spec const *chan, int *val,
			    int *val2, long mask)
{
	struct bd96811_data *data = iio_priv(idev);
	int reg[] = {BD96811_REG_AD0_RES, BD96811_REG_AD1_RES,
		     BD96811_REG_AD2_RES};
	int ret;

	switch (mask) {

	case IIO_CHAN_INFO_SCALE:
		/*
		 * Vadc = 1536 * ADx_VAL / 255 mV
		 * 1536 / 255 => 6.023529412
		 * => Scale: INT 6, NANO 23529412
		 */
		*val = 6;
		*val2 = 23529412;
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_RAW:
		if (chan->channel > ARRAY_SIZE(reg) || chan->channel < 0)
			return -EINVAL;

		if (!chan_is_adc(data, chan->channel)) {
			dev_err(data->dev, "Pin is not ADC\n");
			return -ENODEV;
		}

		ret = regmap_read(data->regmap, reg[chan->channel], val);
		if (ret)
			return ret;

		return IIO_VAL_INT;
	default:
		break;
	}

	return -EINVAL;
}

static const struct iio_info bd96811_info = {
	.read_raw = &bd96811_read_raw,
};

static int bd96811_probe(struct platform_device *pdev)
{
	struct bd96811_data *data;
	struct regmap *regmap;
	struct iio_dev *idev;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "no regmap\n");
		return -EINVAL;
	}

	idev = devm_iio_device_alloc(&pdev->dev, sizeof(*data));
	if (!idev)
		return -ENOMEM;

	data = iio_priv(idev);
	data->regmap = regmap;
	data->dev = &pdev->dev;
	data->has_adc0 = device_property_present(pdev->dev.parent,
						 "rohm,adc0-enabled");

	idev->channels = bd96811_channels;
	idev->num_channels = ARRAY_SIZE(bd96811_channels);
	idev->name = "bd96811";
	idev->info = &bd96811_info;
	idev->modes = INDIO_DIRECT_MODE;

	iio_device_set_parent(idev, pdev->dev.parent);

	return devm_iio_device_register(data->dev, idev);
}

static const struct platform_device_id bd96811_adc_id[] = {
	{ "bd96811-adc", },
	{}
};
MODULE_DEVICE_TABLE(platform, bd96811_adc_id);

static struct platform_driver bd96811_regulator = {
	.driver = {
		.name = "bd96811-adcconv"
	},
	.probe = bd96811_probe,
	.id_table = bd96811_adc_id,
};
module_platform_driver(bd96811_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD96811 PMIC's ADC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd96811-adc");
