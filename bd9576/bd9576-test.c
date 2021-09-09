
#include <linux/module.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of_fdt.h>
//#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/miscdevice.h>

static struct miscdevice md =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dummy",
};


static struct regulator *g_r[6] = { 0 };
struct clk *g_c = NULL;
static const char *regunames[] =
{
	"VD50",
	"VD18",
	"VDDDR",
	"VD10",
	"VOUTL1",
	"VOUTS1",
};

static struct notifier_block notif[ARRAY_SIZE(regunames)];

static ssize_t reboot_test_store(struct kobject *ko, struct kobj_attribute *a,
				 const char *b, size_t c)
{
	int ms;

	if( 1 != sscanf(b,"%i",&ms))
		ms = 0;

	pr_info("Calling hw_protection_shutdown() wit tmo %i\n",ms);
	hw_protection_shutdown("just testing the reset", ms);

	return c;
}

static struct kobj_attribute reboot_test = __ATTR_WO(reboot_test);		\

#define BUCK_ATTR(_buckno) \
static ssize_t	buck##_buckno##_en_show (struct kobject *ko, struct kobj_attribute *a, char *b)				\
{														\
	int rval;												\
	struct regulator *r = g_r[_buckno-1];						\
	if(!IS_ERR(r))												\
	{													\
		*b = (regulator_is_enabled(r))?'1':'0';								\
		b[1]='\0';											\
		rval=2;												\
	}													\
	else													\
		rval = PTR_ERR(r);										\
	return rval;												\
}														\
static ssize_t	buck##_buckno##_en_store (struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c)		\
{														\
	int rval = -EINVAL;											\
	struct regulator *r;											\
														\
	if(c>1 && (*b == '1' || *b == '0'))									\
	{													\
		r =  g_r[_buckno-1];							\
		if(!IS_ERR(r))											\
		{												\
														\
			if(*b == '1')										\
				rval = regulator_enable(r);							\
			else											\
				rval = regulator_disable(r);							\
		}												\
		else												\
			pr_err("regulator_get('%s') failed with %d\n",regunames[_buckno-1],rval);		\
	}													\
	if(!rval && (rval=c))											\
		pr_info("%s: YaY!, Regulator '%s' %sbled\n",__func__,regunames[_buckno-1],(*b == '0')?"disa":"ena");		\
	else													\
		pr_err("Failed to toggle regulator state. error(%d)\n",rval);					\
	return rval;												\
}														\
static ssize_t	buck##_buckno##_set_show (struct kobject *ko, struct kobj_attribute *a, char *b)				\
{														\
	int rval;												\
	struct regulator *r = g_r[_buckno-1];						\
	if(!IS_ERR(r))												\
	{													\
		int v = regulator_get_voltage(r);								\
		if(v>0)												\
			rval = sprintf(b,"%d\n",v);								\
		else												\
			rval = v;										\
	}													\
	else													\
		rval = PTR_ERR(r);										\
	return rval;												\
}														\
static ssize_t	buck##_buckno##_set_store (struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c)		\
{														\
	int rval = -EINVAL;											\
	struct regulator *r;											\
														\
	int v=0,l=0;													\
\
	if( 2 == sscanf(b,"%i %i",&v,&l))									\
	{													\
		r =  g_r[_buckno-1];							\
		if(!IS_ERR(r))											\
		{												\
			pr_info("b%d: Calling: regulator_set_voltage(%d,%d)\n",_buckno,v,l);  \
			rval = regulator_set_voltage(r, v, l);							\
		}												\
	}													\
	else													\
		pr_err("sscanf returned %d\n", rval);								\
														\
	if(!rval && (rval=c))											\
		pr_info("YaY!, Regulator '%s' voltage set to %d\n",regunames[_buckno-1],v);			\
	else													\
		pr_err("Failed to set voltage (%d), limit (%d) error(%d)\n",v,l,rval);					\
	return rval;												\
}														\
static struct kobj_attribute buck_en_##_buckno = __ATTR_RW(buck##_buckno##_en);		\
static struct kobj_attribute buck_set_##_buckno = __ATTR_RO(buck##_buckno##_set) \

BUCK_ATTR(1); //Buck 1 ...VOUT1
BUCK_ATTR(2); // Vout2
BUCK_ATTR(3); // Vout3
BUCK_ATTR(4); // Vout4
BUCK_ATTR(5); // VoutL1
BUCK_ATTR(6); // VoutS1

#define BA(num)  \
	&buck_en_##num.attr, \
	&buck_set_##num.attr

static struct attribute *test_reguattrs[] = {
	BA(1),
	BA(2),
	BA(3),
	BA(4),
	BA(5),
	BA(6),
	&reboot_test.attr,
	NULL
};
#define NUM_TEST_GRPS 1
static const struct attribute_group test_attrs[NUM_TEST_GRPS] = {
	{
		.name = "regulators",
		.attrs = &test_reguattrs[0],
	},
/*
	{
		.name = "clk",
		.attrs = &test_clkattrs[0],
	},
*/
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
	g_k = kobject_create_and_add("mva_test", kernel_kobj);
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

static int notif_func(struct notifier_block *nb, unsigned long evt,
		      void *dt)
{
	smp_rmb();
	pr_info("nb %p Regulator-event %lu data %p\r\n",nb, evt, dt);

	return 0;
}

static void add_notifier(int i)
{
	notif[i].notifier_call = notif_func;
	devm_regulator_register_notifier(g_r[i], &notif[i]);
}

static int __init mva_test_init(void)
{
	int rv,i;
	pr_info("%s: Hello Peeps\n",__func__);
	if(!(rv = misc_register(&md)))
		if((rv = create_sysfs_for_tests()))
			misc_deregister(&md);
	for(i=0;i<ARRAY_SIZE(regunames);i++) {
		g_r[i]=regulator_get(md.this_device,regunames[i]);
		if (!g_r[i] || IS_ERR(g_r[i]))
			pr_err("regulator_get failed %ld\n", PTR_ERR(g_r[i]));
		else
			add_notifier(i);
	}
	return rv;
};

static void __exit mva_test_exit(void)
{
	int i;
		for(i=0;i<ARRAY_SIZE(regunames);i++)
			if(g_r[i])
			{
				regulator_put(g_r[i]);
				g_r[i] = NULL;
			}
	if(g_c && !IS_ERR(g_c))
		clk_put(g_c);
	g_c = NULL;
	pr_info("%s: Bye Bye\n",__func__);
	remove_sysfs_for_tests();
	misc_deregister(&md);
}

// Register these functions
module_init(mva_test_init);
module_exit(mva_test_exit);

MODULE_DESCRIPTION("module for allowing test BD regulators and clk");
MODULE_AUTHOR("Matti Vaittine <matti.vaittinen@fi.rohmeurope.com>");
MODULE_LICENSE("GPL");

