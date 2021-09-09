#include <linux/module.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of_fdt.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/rohm-bd71828.h>

static struct kobject *g_k = NULL;
static struct gpio_desc *g_r[4] = { NULL };

#define GPIO_ATTR(_buckno) \
static ssize_t	buck##_buckno##value_show (struct kobject *ko, struct kobj_attribute *a, char *b)				\
{														\
	int rval;												\
	struct gpio_desc *r = g_r[_buckno-1];						\
	if(!IS_ERR(r))												\
	{													\
		*b = (gpiod_get_value_cansleep(r))?'1':'0';                                                         \
		b[1]='\0';\
		rval=2;\
	}\
	else \
		rval = PTR_ERR(r);										\
	return rval;												\
}														\
static ssize_t	buck##_buckno##value_store (struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c)		\
{														\
	int rval = -EINVAL;											\
	struct gpio_desc *r;											\
														\
	if(c>1 && (*b == '1' || *b == '0'))									\
	{													\
		r =  g_r[_buckno-1];							\
		if(!IS_ERR(r))											\
		{												\
			if (GPIOF_DIR_OUT != gpiod_get_direction(r)) {						\
				pr_err("Can't set value for input GPIO\n");					\
				return -EINVAL;									\
			}											\
			if(*b == '1') {										\
				pr_info("Calling gpiod_set_value_cansleep(r,1);\n");		\
				gpiod_set_value_cansleep(r,1);				\
			}											\
			else {											\
				pr_info("Calling gpiod_set_value_cansleep(r,0);\n");		\
				gpiod_set_value_cansleep(r,0);				\
			}											\
			rval = c;										\
		}												\
		else												\
			pr_err("gpiod_get() has failed with %d?\n",rval);		\
	}\
	return rval;												\
}														\
static ssize_t	buck##_buckno##_direction_show (struct kobject *ko, struct kobj_attribute *a, char *b)				\
{														\
	int rval;												\
	struct gpio_desc *r = g_r[_buckno-1];						\
	if(!IS_ERR(r))												\
	{													\
		int v = gpiod_get_direction(r);								\
		if (v == GPIOF_DIR_IN || v == GPIOF_DIR_OUT) \
			rval = sprintf(b,"direction %s (%d)\n",(v == GPIOF_DIR_IN) ? "input" : "output", v);	\
		else {												\
			rval = v;										\
			pr_err("Failed to get GPIO dir (err %d)\n", rval);					\
		}												\
	}													\
	else													\
		rval = PTR_ERR(r);										\
	return rval;												\
}\
static ssize_t	buck##_buckno##_direction_store (struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c)		\
{														\
	int rval = -EINVAL;											\
	struct gpio_desc *r;											\
														\
	int v=0;													\
\
	if( 1 == sscanf(b,"%i",&v))									\
	{													\
		r =  g_r[_buckno-1];							\
		if(!IS_ERR(r))											\
		{	 \
			if (v){											\
				pr_info("b%d: Calling: gpiod_direction_input()\n",_buckno);  \
				rval = gpiod_direction_input(r); \
			} \
			else {\
				pr_info("b%d: Calling: gpiod_direction_output\n",_buckno);  \
				rval = gpiod_direction_output(r,0); \
			}\
		}												\
	}													\
	else													\
		pr_err("sscanf returned %d\n", rval);								\
														\
	if(!rval && (rval=c))											\
		pr_info("YaY!, gpio %d dir set\n", _buckno-1);			\
	else													\
		pr_err("Failed to set gpio dir (%d)\n",rval);					\
	return rval;												\
}														\
static struct kobj_attribute buck_out_##_buckno = __ATTR_RW(buck##_buckno##value);		\
static struct kobj_attribute gpio_dir_##_buckno = __ATTR_RW(buck##_buckno##_direction)

GPIO_ATTR(1);
/*
GPIO_ATTR(2);
GPIO_ATTR(3);
GPIO_ATTR(4);
*/

#define GA(num)  \
	&gpio_dir_##num.attr, \
	&buck_out_##num.attr

static struct attribute *test_gpioattrs[] = {
	GA(1),
/*	GA(2),
	GA(3),
	GA(4),
*/
	NULL
};
static const struct attribute_group test_attrs[] = {
	{
		.name = "gpios",
		.attrs = &test_gpioattrs[0],
	},
};

static void remove_sysfs_for_tests(void)
{
	int i;
	if(g_k)
	{
	/* Here we should have locking */
		for(i=0;i<1;i++)
			sysfs_remove_group(g_k,&test_attrs[i]);
		 kobject_put(g_k);
	}
	g_k=NULL;
}
static int create_sysfs_for_tests(void)
{
	int rval=-EINVAL;
	int i;
	/* Here we should have locking */
	g_k = kobject_create_and_add("mva_test", kernel_kobj);
	if(g_k)
		for(i=0;i<1;i++)
			if( (rval = sysfs_create_group(g_k,&test_attrs[i])))
				goto err_fail;
	if(0) {
		/* delete sysfs */
err_fail:
		kobject_put(g_k);
		pr_err("%s: Failed %d\n",__func__,rval);
	}
	return rval;
}

static int bd71828_gpio_remove(struct platform_device *pdev)
{
	remove_sysfs_for_tests();
	return 0;
}
/*
static irqreturn_t hnd(int i, void *foo)
{
	pr_info("Yay, irq %d\n",i);
	return IRQ_HANDLED;
}
*/

//#define GPIO_IRQ_BASE BD71828_INT_GPIO0

static int bd71828_gpio_probe(struct platform_device *pdev)
{
	struct gpio_desc *gpio1 /* , *gpio2, *gpio3, *gpio4 */ ;
//	int ret;
	//const char *gponme[4] = {"bd71828-gpio1", "bd71828-gpio2",
	//			 "bd71828-gpio3"};

	if ( IS_ERR (gpio1 = devm_gpiod_get_index(&pdev->dev, "my", 0, GPIOD_OUT_LOW))) {
		pr_err("GPIO1 Fuck'd up %ld\n", PTR_ERR(gpio1));
		return PTR_ERR(gpio1);
	}
/*
	if ( IS_ERR (gpio2 = devm_gpiod_get_index(&pdev->dev, "my", 1, GPIOD_OUT)))
		pr_err("GPIO2 Fuck'd up %ld\n", PTR_ERR(gpio2));
	if ( IS_ERR (gpio3 = devm_gpiod_get_index(&pdev->dev, "my", 2, GPIOD_OUT)))
		pr_err("GPIO3 Fuck'd up %ld\n", PTR_ERR(gpio3));
*/

	g_r[0] = gpio1;
//	g_r[1] = gpio2;
//	g_r[2] = gpio3;
	return create_sysfs_for_tests();
/*
	for (i = 0; i < 3; i++) {
		irq[i] = platform_get_irq(pdev,i);
			if( irq[i] > 0 ) {
				ret = devm_request_threaded_irq(&pdev->dev,
								irq[i], NULL,
								hnd,
								IRQF_ONESHOT,
								gponme[i],
								NULL);
				if (ret)
					pr_err("vituks meni %d %d\n",i , ret);
			}
	}
*/
	return 0;
}

static struct of_device_id bd71828_of_match[] = {
	{
		.compatible = "rohm,foo-bd71828-gpio",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, bd71828_of_match);

struct platform_driver bd71828_gpio_consumer = {
        .driver = {
                .name = "bd71828-gpio-test",
		.of_match_table = bd71828_of_match,
        },
        .probe = bd71828_gpio_probe,
	.remove = bd71828_gpio_remove,
};

module_platform_driver(bd71828_gpio_consumer);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71828 gpio test driver");
MODULE_LICENSE("GPL");
