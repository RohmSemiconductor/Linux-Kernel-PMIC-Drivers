
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


/*
static struct miscdevice md =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dummy",
};
*/

struct clk *g_c = NULL;

static ssize_t clk_en_store(struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c);

static ssize_t clk_en_store(struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c)
{
	int rval = -EINVAL;

	if(c>1 && (*b == '1' || *b == '0'))
	{
		if(g_c && !IS_ERR(g_c))
		{

			if(*b == '1')
			{
				pr_info("%s: clk_prepare(g_c) returned %d\n",__func__,clk_prepare(g_c));
				rval = clk_enable(g_c);
			}
			else
			{
				clk_disable(g_c);
				clk_unprepare(g_c);
				rval = 0;
			}
		}
		else
			pr_err("clk_get(NULL, bd71828-32k-out) has FAILED (%d)\n",(rval=PTR_ERR(g_c)));
	};
	if(!rval && (rval=c))
		pr_info("YaY!, Clk '%s' %sbled\n","bd71828-32k-out",(*b == '0')?"disa":"ena");
	else
		pr_err("Failed to toggle clk state. error(%d)\n",rval);
	return rval;
}
static ssize_t	clk_en_show (struct kobject *ko, struct kobj_attribute *a, char *b)
{
	int rval;
	if(g_c && !(IS_ERR(g_c)))
	{
		unsigned long rate;
		rate = clk_get_rate(g_c);
		rval = sprintf(b,"%lu\n",rate);
	}
	else
	{
		rval = PTR_ERR(g_c);
		pr_err("clk_get has FAILED (%d)\n",rval);
	}
	return rval;
}

static struct kobj_attribute clk_en = __ATTR_RW(clk_en);

static struct attribute *test_clkattrs[] = {
	&clk_en.attr,
	NULL
};

#define NUM_TEST_GRPS 1
static const struct attribute_group test_attrs[NUM_TEST_GRPS] = {
	{
		.name = "clk2",
		.attrs = &test_clkattrs[0],
	},
};


static struct kobject *g_k = NULL;

static void remove_sysfs_for_tests(void)
{
	int i;
	if(g_k)
	{
	/* Here we should have locking */
		for(i=0;i<NUM_TEST_GRPS;i++)
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
	g_k = kobject_create_and_add("mva_test2", kernel_kobj);
	if(g_k)
		for(i=0;i<NUM_TEST_GRPS;i++)
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



static int mva_test_probe(struct platform_device *pdev)
{
	int rv;
	pr_info("%s: Hello Peeps\n",__func__);
		if((rv = create_sysfs_for_tests()))
			return rv;
	g_c = clk_get(&pdev->dev, "bd71828-32k-out");
	if(!g_c || IS_ERR(g_c))
	{
		pr_info("clk_get(&pdev->dev, \"bd71828-32k-out\"); did not work\n");
		if ( !(g_c = clk_get(NULL, "bd71828-32k-out") ) || IS_ERR(g_c))
		{
			pr_info("clk_get(NULL, \"bd71828-32k-out\"); did not work\n");
			if ( !(g_c = clk_get(&pdev->dev, NULL) ) || IS_ERR(g_c))
				pr_info("clk_get(&pdev->dev, NULL); did not work\n");
			else
				pr_info("YAY clk_get(&pdev->dev, NULL); did work\n");
		}
		else
			pr_info("YAY! clk_get(NULL, \"bd71828-32k-out\"); did work\n");
	}
	else
		pr_info("YAY clk_get(&pdev->dev, \"bd71828-32k-out\"); did work\n");
	return rv;
};

static int mva_test_remove(struct platform_device *pdev)
{
	if(g_c && !IS_ERR(g_c))
		clk_put(g_c);
	g_c = NULL;
	pr_info("%s: Bye Bye\n",__func__);
	remove_sysfs_for_tests();
	return 0;
}

static const struct of_device_id bd71828_test_of_match[] = {
        {
                .compatible = "rohm,clktest-bd71828",
        },
        { }
};
MODULE_DEVICE_TABLE(of, bd71828_test_of_match);

static struct platform_driver bd71828_test = {
        .driver = {
                .name = "bd718xx-test",
		.of_match_table = bd71828_test_of_match,
        },
        .probe = mva_test_probe,
	.remove = &mva_test_remove,
};

module_platform_driver(bd71828_test);



// Register these functions
//module_init(mva_test_init);
//module_exit(mva_test_exit);

MODULE_DESCRIPTION("module for allowing test BD regulators and clk");
MODULE_AUTHOR("Matti Vaittine <matti.vaittinen@fi.rohmeurope.com>");
MODULE_LICENSE("GPL");

