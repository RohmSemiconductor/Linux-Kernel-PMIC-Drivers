
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
#include <linux/miscdevice.h>

static struct miscdevice md =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dummy",
};


static struct regulator *g_r[12] = { 0 };
struct clk *g_c = NULL;
static const char *regunames[] =
{
	"buck1",
	"buck2",
	"buck3",
	"buck4",
	"buck5",
	"buck6",
	"ldo1",
	"ldo2",
	"ldo3",
	"ldo4",
	"ldo5",
	"ldo6",
};

static ssize_t clk_en_store(struct kobject *ko, struct kobj_attribute *a, const char *b, size_t c);

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
static struct kobj_attribute buck_set_##_buckno = __ATTR_RW(buck##_buckno##_set) \

BUCK_ATTR(1); //Buck 1 ...
BUCK_ATTR(2);
BUCK_ATTR(3);
BUCK_ATTR(4);
BUCK_ATTR(5);
BUCK_ATTR(6); //...Buck 6
BUCK_ATTR(7); //LDO1...
BUCK_ATTR(8);
BUCK_ATTR(9);
BUCK_ATTR(10);
BUCK_ATTR(11);
BUCK_ATTR(12);

#define BA(num)  \
	&buck_en_##num.attr, \
	&buck_set_##num.attr

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
			pr_err("clk_get(NULL, bd71847-32k-out) has FAILED (%d)\n",(rval=PTR_ERR(g_c)));
	};
	if(!rval && (rval=c))
		pr_info("YaY!, Clk '%s' %sbled\n","bd71847-32k-out",(*b == '0')?"disa":"ena");
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

static struct attribute *test_reguattrs[] = {
	BA(1),
	BA(2),
	BA(3),
	BA(4),
	BA(5),
	BA(6),
	BA(7),
	BA(8),
	BA(9),
	BA(10),
	BA(11),
	BA(12),
	NULL
};
#define NUM_TEST_GRPS 2
static const struct attribute_group test_attrs[NUM_TEST_GRPS] = {
	{
		.name = "regulators",
		.attrs = &test_reguattrs[0],
	},
	{
		.name = "clk",
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
			pr_err("=regulator_get failed %ld\n", PTR_ERR(g_r[i]));
	}
	g_c = clk_get(NULL, "bd71847-32k-out");
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

