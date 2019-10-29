// SPDX-License-Identifier: GPL-2.0-only
/*
 * LED Class Core
 *
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 */

#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <uapi/linux/uleds.h>
#include "leds.h"

DECLARE_RWSEM(leds_list_lock);
EXPORT_SYMBOL_GPL(leds_list_lock);

LIST_HEAD(leds_list);
EXPORT_SYMBOL_GPL(leds_list);

const char * const led_colors[LED_COLOR_ID_MAX] = {
	[LED_COLOR_ID_WHITE] = "white",
	[LED_COLOR_ID_RED] = "red",
	[LED_COLOR_ID_GREEN] = "green",
	[LED_COLOR_ID_BLUE] = "blue",
	[LED_COLOR_ID_AMBER] = "amber",
	[LED_COLOR_ID_VIOLET] = "violet",
	[LED_COLOR_ID_YELLOW] = "yellow",
	[LED_COLOR_ID_IR] = "ir",
};
EXPORT_SYMBOL_GPL(led_colors);

static int __led_set_brightness(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	if (!led_cdev->brightness_set)
		return -ENOTSUPP;

	led_cdev->brightness_set(led_cdev, value);

	return 0;
}

static int __led_set_brightness_blocking(struct led_classdev *led_cdev,
					 enum led_brightness value)
{
	if (!led_cdev->brightness_set_blocking)
		return -ENOTSUPP;

	return led_cdev->brightness_set_blocking(led_cdev, value);
}

static void led_timer_function(struct timer_list *t)
{
	struct led_classdev *led_cdev = from_timer(led_cdev, t, blink_timer);
	unsigned long brightness;
	unsigned long delay;

	if (!led_cdev->blink_delay_on || !led_cdev->blink_delay_off) {
		led_set_brightness_nosleep(led_cdev, LED_OFF);
		clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
		return;
	}

	if (test_and_clear_bit(LED_BLINK_ONESHOT_STOP,
			       &led_cdev->work_flags)) {
		clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
		return;
	}

	brightness = led_get_brightness(led_cdev);
	if (!brightness) {
		/* Time to switch the LED on. */
		if (test_and_clear_bit(LED_BLINK_BRIGHTNESS_CHANGE,
					&led_cdev->work_flags))
			brightness = led_cdev->new_blink_brightness;
		else
			brightness = led_cdev->blink_brightness;
		delay = led_cdev->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led_cdev->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led_cdev->blink_delay_off;
	}

	led_set_brightness_nosleep(led_cdev, brightness);

	/* Return in next iteration if led is in one-shot mode and we are in
	 * the final blink state so that the led is toggled each delay_on +
	 * delay_off milliseconds in worst case.
	 */
	if (test_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags)) {
		if (test_bit(LED_BLINK_INVERT, &led_cdev->work_flags)) {
			if (brightness)
				set_bit(LED_BLINK_ONESHOT_STOP,
					&led_cdev->work_flags);
		} else {
			if (!brightness)
				set_bit(LED_BLINK_ONESHOT_STOP,
					&led_cdev->work_flags);
		}
	}

	mod_timer(&led_cdev->blink_timer, jiffies + msecs_to_jiffies(delay));
}

static void set_brightness_delayed(struct work_struct *ws)
{
	struct led_classdev *led_cdev =
		container_of(ws, struct led_classdev, set_brightness_work);
	int ret = 0;

	if (test_and_clear_bit(LED_BLINK_DISABLE, &led_cdev->work_flags)) {
		led_cdev->delayed_set_value = LED_OFF;
		led_stop_software_blink(led_cdev);
	}

	ret = __led_set_brightness(led_cdev, led_cdev->delayed_set_value);
	if (ret == -ENOTSUPP)
		ret = __led_set_brightness_blocking(led_cdev,
					led_cdev->delayed_set_value);
	if (ret < 0 &&
	    /* LED HW might have been unplugged, therefore don't warn */
	    !(ret == -ENODEV && (led_cdev->flags & LED_UNREGISTERING) &&
	    (led_cdev->flags & LED_HW_PLUGGABLE)))
		dev_err(led_cdev->dev,
			"Setting an LED's brightness failed (%d)\n", ret);
}

static void led_set_software_blink(struct led_classdev *led_cdev,
				   unsigned long delay_on,
				   unsigned long delay_off)
{
	int current_brightness;

	current_brightness = led_get_brightness(led_cdev);
	if (current_brightness)
		led_cdev->blink_brightness = current_brightness;
	if (!led_cdev->blink_brightness)
		led_cdev->blink_brightness = led_cdev->max_brightness;

	led_cdev->blink_delay_on = delay_on;
	led_cdev->blink_delay_off = delay_off;

	/* never on - just set to off */
	if (!delay_on) {
		led_set_brightness_nosleep(led_cdev, LED_OFF);
		return;
	}

	/* never off - just set to brightness */
	if (!delay_off) {
		led_set_brightness_nosleep(led_cdev,
					   led_cdev->blink_brightness);
		return;
	}

	set_bit(LED_BLINK_SW, &led_cdev->work_flags);
	mod_timer(&led_cdev->blink_timer, jiffies + 1);
}


static void led_blink_setup(struct led_classdev *led_cdev,
		     unsigned long *delay_on,
		     unsigned long *delay_off)
{
	if (!test_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags) &&
	    led_cdev->blink_set &&
	    !led_cdev->blink_set(led_cdev, delay_on, delay_off))
		return;

	/* blink with 1 Hz as default if nothing specified */
	if (!*delay_on && !*delay_off)
		*delay_on = *delay_off = 500;

	led_set_software_blink(led_cdev, *delay_on, *delay_off);
}

void led_init_core(struct led_classdev *led_cdev)
{
	INIT_WORK(&led_cdev->set_brightness_work, set_brightness_delayed);

	timer_setup(&led_cdev->blink_timer, led_timer_function, 0);
}
EXPORT_SYMBOL_GPL(led_init_core);

void led_blink_set(struct led_classdev *led_cdev,
		   unsigned long *delay_on,
		   unsigned long *delay_off)
{
	del_timer_sync(&led_cdev->blink_timer);

	clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
	clear_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags);
	clear_bit(LED_BLINK_ONESHOT_STOP, &led_cdev->work_flags);

	led_blink_setup(led_cdev, delay_on, delay_off);
}
EXPORT_SYMBOL_GPL(led_blink_set);

void led_blink_set_oneshot(struct led_classdev *led_cdev,
			   unsigned long *delay_on,
			   unsigned long *delay_off,
			   int invert)
{
	if (test_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags) &&
	     timer_pending(&led_cdev->blink_timer))
		return;

	set_bit(LED_BLINK_ONESHOT, &led_cdev->work_flags);
	clear_bit(LED_BLINK_ONESHOT_STOP, &led_cdev->work_flags);

	if (invert)
		set_bit(LED_BLINK_INVERT, &led_cdev->work_flags);
	else
		clear_bit(LED_BLINK_INVERT, &led_cdev->work_flags);

	led_blink_setup(led_cdev, delay_on, delay_off);
}
EXPORT_SYMBOL_GPL(led_blink_set_oneshot);

void led_stop_software_blink(struct led_classdev *led_cdev)
{
	del_timer_sync(&led_cdev->blink_timer);
	led_cdev->blink_delay_on = 0;
	led_cdev->blink_delay_off = 0;
	clear_bit(LED_BLINK_SW, &led_cdev->work_flags);
}
EXPORT_SYMBOL_GPL(led_stop_software_blink);

void led_set_brightness(struct led_classdev *led_cdev,
			enum led_brightness brightness)
{
	/*
	 * If software blink is active, delay brightness setting
	 * until the next timer tick.
	 */
	if (test_bit(LED_BLINK_SW, &led_cdev->work_flags)) {
		/*
		 * If we need to disable soft blinking delegate this to the
		 * work queue task to avoid problems in case we are called
		 * from hard irq context.
		 */
		if (brightness == LED_OFF) {
			set_bit(LED_BLINK_DISABLE, &led_cdev->work_flags);
			schedule_work(&led_cdev->set_brightness_work);
		} else {
			set_bit(LED_BLINK_BRIGHTNESS_CHANGE,
				&led_cdev->work_flags);
			led_cdev->new_blink_brightness = brightness;
		}
		return;
	}

	led_set_brightness_nosleep(led_cdev, brightness);
}
EXPORT_SYMBOL_GPL(led_set_brightness);

void led_set_brightness_nopm(struct led_classdev *led_cdev,
			      enum led_brightness value)
{
	/* Use brightness_set op if available, it is guaranteed not to sleep */
	if (!__led_set_brightness(led_cdev, value))
		return;

	/* If brightness setting can sleep, delegate it to a work queue task */
	led_cdev->delayed_set_value = value;
	schedule_work(&led_cdev->set_brightness_work);
}
EXPORT_SYMBOL_GPL(led_set_brightness_nopm);

void led_set_brightness_nosleep(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	led_cdev->brightness = min(value, led_cdev->max_brightness);

	if (led_cdev->flags & LED_SUSPENDED)
		return;

	led_set_brightness_nopm(led_cdev, led_cdev->brightness);
}
EXPORT_SYMBOL_GPL(led_set_brightness_nosleep);

int led_set_brightness_sync(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	if (led_cdev->blink_delay_on || led_cdev->blink_delay_off)
		return -EBUSY;

	led_cdev->brightness = min(value, led_cdev->max_brightness);

	if (led_cdev->flags & LED_SUSPENDED)
		return 0;

	return __led_set_brightness_blocking(led_cdev, led_cdev->brightness);
}
EXPORT_SYMBOL_GPL(led_set_brightness_sync);

int led_update_brightness(struct led_classdev *led_cdev)
{
	int ret = 0;

	if (led_cdev->brightness_get) {
		ret = led_cdev->brightness_get(led_cdev);
		if (ret >= 0) {
			led_cdev->brightness = ret;
			return 0;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(led_update_brightness);

u32 *led_get_default_pattern(struct led_classdev *led_cdev, unsigned int *size)
{
	struct fwnode_handle *fwnode = led_cdev->dev->fwnode;
	u32 *pattern;
	int count;

	count = fwnode_property_count_u32(fwnode, "led-pattern");
	if (count < 0)
		return NULL;

	pattern = kcalloc(count, sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		return NULL;

	if (fwnode_property_read_u32_array(fwnode, "led-pattern", pattern, count)) {
		kfree(pattern);
		return NULL;
	}

	*size = count;

	return pattern;
}
EXPORT_SYMBOL_GPL(led_get_default_pattern);

/* Caller must ensure led_cdev->led_access held */
void led_sysfs_disable(struct led_classdev *led_cdev)
{
	lockdep_assert_held(&led_cdev->led_access);

	led_cdev->flags |= LED_SYSFS_DISABLE;
}
EXPORT_SYMBOL_GPL(led_sysfs_disable);

/* Caller must ensure led_cdev->led_access held */
void led_sysfs_enable(struct led_classdev *led_cdev)
{
	lockdep_assert_held(&led_cdev->led_access);

	led_cdev->flags &= ~LED_SYSFS_DISABLE;
}
EXPORT_SYMBOL_GPL(led_sysfs_enable);

static int fw_is_match(struct fwnode_handle *fw,
		       struct led_fw_match_property *mp, void *val)
{
	void *cmp = mp->raw_val;
	int ret = -EINVAL;

	if (mp->raw_val) {
		ret = fwnode_property_read_u8_array(fw, mp->name, val,
						    mp->size);
	} else if (mp->intval) {
		cmp = mp->intval;
		switch (mp->size) {
		case 1:
			ret = fwnode_property_read_u8_array(fw, mp->name, val,
						    mp->size);
			break;
		case 2:
			ret = fwnode_property_read_u16_array(fw, mp->name, val,
						    mp->size);
			break;
		case 4:
			ret = fwnode_property_read_u32_array(fw, mp->name, val,
						    mp->size);
			break;
		case 8:
			ret = fwnode_property_read_u64_array(fw, mp->name, val,
						    mp->size);
			break;
		default:
			return -EINVAL;
		}
	}
	if (!ret && cmp)
		if (!memcmp(val, cmp, mp->size))
			return 1;

	return 0;
}
/**
 * led_get_fwnode - find fwnode for led
 * @parent	LED controller device
 * @init_data	led init data with match information
 *
 * Scans the firmware nodes and returns node matching the given init_data.
 * NOTE: Function increases refcount for found node. Caller must decrease
 * refcount using fwnode_handle_put when finished with node.
 */
struct fwnode_handle *led_get_fwnode(struct device *parent,
				      struct led_init_data *init_data)
{
	struct fwnode_handle *fw;

	/*
	 * This should never be called without init data. We could always return
	 * dev_fwnode() - but then we should pump-up the refcount
	 */
	if (!init_data)
		return NULL;

	/*
	 * For now we do only do node look-up for drivers which populate
	 * the new match properties. We could and perhaps should do
	 * fw = dev_fwnode(parent); if given fwnode is NULL. But in order not to
	 * break the existing setups we keep the old behaviour and just directly
	 * use the given init_data->fwnode no matter if it is NULL or not.
	 */

	if ((!init_data->match_property.name ||
	     !init_data->match_property.size) && !init_data->of_match)
		return fwnode_handle_get(init_data->fwnode);

	/* match information was given - do node look-up */

	if (!init_data->fwnode)
		fw = dev_fwnode(parent);
	else
		fw = init_data->fwnode;

	if (!fw)
		return NULL;

	/*
	 * Simple things are pretty. I think simplest is to use DT node-name
	 * for matching the node with LED - same way regulators use the node
	 * name to match with desc.
	 *
	 * This may not work with existing LED DT entries if the node name has
	 * been freely pickable. In order to this to work the binding doc
	 * for LED driver should define usable node names.
	 *
	 * If this is not working we can define specific match property which
	 * value we scan and use for matching for LEDs connected to the
	 * controller.
	 */
	if (init_data->match_property.name && init_data->match_property.size) {
		u8 *val;
		int ret;
		struct fwnode_handle *child;
		struct led_fw_match_property *mp;

		mp = &init_data->match_property;

		val = kzalloc(mp->size, GFP_KERNEL);
		if (!val)
			return ERR_PTR(-ENOMEM);

		fwnode_for_each_child_node(fw, child) {
			ret = fw_is_match(child, mp, val);
			if (ret > 0) {
				kfree(val);
				return child;
			}
			if (ret < 0) {
				dev_err(parent,
					"invalid fw match. Use raw_val?\n");
				fwnode_handle_put(child);
				break;
			}
		}
		kfree(val);
	}
	if (init_data->of_match)
		fw = fwnode_get_named_child_node(fw, init_data->of_match);

	return fw;
}
EXPORT_SYMBOL(led_get_fwnode);

int led_parse_fwnode_props(struct device *dev, struct fwnode_handle *fwnode,
			   struct led_properties *props)
{
	int ret = 0;

	if (!fwnode)
		return 0;

	if (fwnode_property_present(fwnode, "label")) {
		ret = fwnode_property_read_string(fwnode, "label", &props->label);
		if (ret)
			dev_err(dev, "Error parsing 'label' property (%d)\n", ret);
		return ret;
	}

	/*
	 * Please note, logic changed - if invalid property is found we bail
	 * early out without parsing the rest of the properties. Originally
	 * this was the case only for 'label' property. I don't know the
	 * rationale behind original logic allowing invalid properties to be
	 * given. If there is a reason then we should reconsider this.
	 * Intuitively it feels correct to just yell and quit if we hit value we
	 * don't understand - but intuition may be wrong at times :)
	 */
	if (fwnode_property_present(fwnode, "color")) {
		ret = fwnode_property_read_u32(fwnode, "color", &props->color);
		if (ret) {
			dev_err(dev, "Error parsing 'color' property (%d)\n", ret);
			return ret;
		} else if (props->color >= LED_COLOR_ID_MAX) {
			dev_err(dev, "LED color identifier out of range\n");
			return ret;
		}
		props->color_present = true;
	}

	if (fwnode_property_present(fwnode, "function")) {
		ret = fwnode_property_read_string(fwnode, "function",
						  &props->function);
		if (ret) {
			dev_err(dev,
				"Error parsing 'function' property (%d)\n",
				ret);
			return ret;
		}
	}

	if (fwnode_property_present(fwnode, "function-enumerator")) {
		ret = fwnode_property_read_u32(fwnode, "function-enumerator",
					       &props->func_enum);
		if (ret) {
			dev_err(dev,
				"Error parsing 'function-enumerator' property (%d)\n",
				ret);
			return ret;
		}
		props->func_enum_present = true;
	}

	if (fwnode_property_present(fwnode, "default-state")) {
		ret = fwnode_property_read_string(fwnode, "default-state",
						  &props->default_state);
		if (ret) {
			dev_err(dev,
				"Error parsing 'default-state' property (%d)\n",
				ret);
			return ret;
		}
	}

	if (fwnode_property_present(fwnode, "linux,default-trigger")) {
		ret = fwnode_property_read_string(fwnode,
						  "linux,default-trigger",
						  &props->default_trigger);
		if (ret)
			dev_err(dev,
				"Error parsing 'linux,default-trigger' property (%d)\n",
				ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(led_parse_fwnode_props);

int led_compose_name(struct device *dev, struct led_init_data *init_data,
		     struct led_properties *props, char *led_classdev_name)
{
	const char *devicename = init_data->devicename;

	if (!led_classdev_name)
		return -EINVAL;

	if (props->label) {
		/*
		 * If init_data.devicename is NULL, then it indicates that
		 * DT label should be used as-is for LED class device name.
		 * Otherwise the label is prepended with devicename to compose
		 * the final LED class device name.
		 */
		if (!devicename) {
			strscpy(led_classdev_name, props->label,
				LED_MAX_NAME_SIZE);
		} else {
			snprintf(led_classdev_name, LED_MAX_NAME_SIZE, "%s:%s",
				 devicename, props->label);
		}
	} else if (props->function || props->color_present) {
		char tmp_buf[LED_MAX_NAME_SIZE];

		if (props->func_enum_present) {
			snprintf(tmp_buf, LED_MAX_NAME_SIZE, "%s:%s-%d",
				 props->color_present ? led_colors[props->color] : "",
				 props->function ?: "", props->func_enum);
		} else {
			snprintf(tmp_buf, LED_MAX_NAME_SIZE, "%s:%s",
				 props->color_present ? led_colors[props->color] : "",
				 props->function ?: "");
		}
		if (init_data->devname_mandatory) {
			snprintf(led_classdev_name, LED_MAX_NAME_SIZE, "%s:%s",
				 devicename, tmp_buf);
		} else {
			strscpy(led_classdev_name, tmp_buf, LED_MAX_NAME_SIZE);

		}
	} else if (init_data->default_label) {
		if (!devicename) {
			dev_err(dev, "Legacy LED naming requires devicename segment");
			return -EINVAL;
		}
		snprintf(led_classdev_name, LED_MAX_NAME_SIZE, "%s:%s",
			 devicename, init_data->default_label);
	} else {
		struct fwnode_handle *fwnode = led_get_fwnode(dev, init_data);
		int ret = -EINVAL;

		if (is_of_node(fwnode)) {
			ret = 0;
			strscpy(led_classdev_name, to_of_node(fwnode)->name,
				LED_MAX_NAME_SIZE);
		}
		fwnode_handle_put(fwnode);

		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(led_compose_name);
