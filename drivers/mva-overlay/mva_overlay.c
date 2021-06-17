#include <linux/module.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/of_fdt.h>
//#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>

#define MAX_OVERLAYS 1024

static DEFINE_MUTEX(overlay_id_mtx);

static ssize_t overlay_del_store(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buffer,
			loff_t pos, size_t size);
static ssize_t overlay_add_store(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buffer,
			loff_t pos, size_t size);

static struct bin_attribute overlay_add = {
	.attr = {
		.name = "overlay_add",
		.mode =  S_IWUSR,
	},
	.size=0,
	.read = NULL,
	.write = &overlay_add_store,
};

static struct bin_attribute overlay_del = {
	.attr = {
		.name = "overlay_del",
		.mode =  S_IWUSR,
	},
        .size=0,
        .read = NULL,
        .write = &overlay_del_store,
};


struct mva_overlay {
	struct kobject *k;
};
static struct mva_overlay g_o = {
	.k=NULL,
};
struct overlay_key {

	int ovcs_id;
	unsigned sum;

};
static struct overlay_key lazy_me_overlay_key_table[MAX_OVERLAYS] = {{0,0},};
static struct overlay_key * even_lazier_me_overlay_key_reserved_table[MAX_OVERLAYS]={NULL};


static void remove_sysfs_for_overlays(struct mva_overlay *o)
{
	if(o) {
	/* Here we should have locking */
		sysfs_remove_bin_file(o->k, &overlay_del);
		sysfs_remove_bin_file(o->k, &overlay_add);
		kobject_put(o->k);
		o->k=NULL;
	}
}
static int create_sysfs_for_overlays(struct mva_overlay *o)
{
	int rval=-EINVAL;
	/* Here we should have locking */
	if(o && !o->k)
	{
		rval=-ENOMEM;
		o->k = kobject_create_and_add("mva_overlay", kernel_kobj);
		if(o->k)
			if(!(rval=sysfs_create_bin_file(o->k, &overlay_del)))
				if(!(rval=sysfs_create_bin_file(o->k, &overlay_add)))
					pr_info("%s: sysfs created\n",__func__);
				else
					goto err_fail_attr;
			else
				goto err_fail;
		else
			pr_err("kobject_create_and_add failed\n");
	}
	if(0) {
		/* delete sysfs */
err_fail_attr:
		sysfs_remove_bin_file(o->k, &overlay_del);
err_fail:
		kobject_put(o->k);
		pr_err("%s: Failed %d\n",__func__,rval);
	}
	return rval;
}



static int terribly_lazy_me_do_inefficient_key_calculation_for_overlay(struct overlay_key *k, void *blob, ssize_t blob_size)
{
//	char *b=blob;
	unsigned long long i;
	int rv=-EINVAL;
	
	if(k&&blob)
		for(i=0,k->sum=0,rv=0;i<blob_size;i++)
			k->sum+=(unsigned)((char *)blob)[i];
	return rv;
}

static int delete_overlay(void *blob, ssize_t size)
{
	struct overlay_key search_key = {0,0};
	int i, r=-ENOENT;
	terribly_lazy_me_do_inefficient_key_calculation_for_overlay(&search_key, blob, size);
	mutex_lock(&overlay_id_mtx);
	for(i=0;i<MAX_OVERLAYS;i++)
		if(even_lazier_me_overlay_key_reserved_table[i])
			if(even_lazier_me_overlay_key_reserved_table[i]->sum == search_key.sum)
			{
				search_key.ovcs_id = even_lazier_me_overlay_key_reserved_table[i]->ovcs_id;
				even_lazier_me_overlay_key_reserved_table[i] = NULL;
				break;
			}
	mutex_unlock(&overlay_id_mtx);
	if(search_key.ovcs_id)
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,0)
		if( (r=of_overlay_destroy(search_key.ovcs_id)))
	#else
		if( (r=of_overlay_remove(&search_key.ovcs_id)))
	#endif
			pr_err("%s: Failed to remove overlay (%d)\n",__func__,r);
	return r;
}
static int create_overlay(void *blob, ssize_t size)
{
	int err;
	struct overlay_key *k=NULL;
	int ov_id,i;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,16,2)
	struct device_node *n;

	/* unflatten the tree */
	of_fdt_unflatten_tree(blob, NULL, &n);
	if (!n) {
		pr_err("%s: failed to unflatten tree\n", __func__);
		err = -EINVAL;
		goto out_err;
	}
	pr_debug("%s: unflattened OK\n", __func__);

	/* mark it as detached */
	of_node_set_flag(n, OF_DETACHED);
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,0)
	if ( (err = of_resolve_phandles(n)))
		pr_err("of_resolve_phandles failed %d\n", err);
	err = of_overlay_create(n);
	if( 0 <= (ov_id = err))
	#else
	if((err=of_overlay_apply(n, &ov_id)))
	#endif
#else
	if((err=of_overlay_fdt_apply(blob, size,  &ov_id)))
#endif

	if (err < 0) {
		pr_err("%s: Failed to create overlay (err=%d)\n",
				__func__, err);
		goto out_err;
	}
	mutex_lock(&overlay_id_mtx);
	for(i=0;i<MAX_OVERLAYS;i++)
		if(!even_lazier_me_overlay_key_reserved_table[i])
		{
			k=(even_lazier_me_overlay_key_reserved_table[i]=&lazy_me_overlay_key_table[i]);
			k->ovcs_id=ov_id;
		}
	mutex_unlock(&overlay_id_mtx);
	if(k)
		terribly_lazy_me_do_inefficient_key_calculation_for_overlay(k, blob, size);
	else
		pr_warn("%s: No free overlay key slot found - overlay can't be removed\n",__func__);

out_err:
	return err;
}
#define MAX_OVERLAY_PAGES 10
static char buf[MAX_OVERLAY_PAGES*PAGE_SIZE];

static ssize_t overlay_modify_store(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buffer,
			loff_t pos, size_t size,int create)
{
	int rv;
	static int index = 0;

	pr_info("%s: got overlay data (pos=%lu, size=%llu\n",__func__,(unsigned long)pos, (unsigned long long)size);

	/* Lazy implementation assumes that if size equals PAGE_SIE - then we have more data to come. When size is
	 * smaller than PAGE_SIZE we finally write the data. This of course breaks if overlay is exactly PAGE_SIZE. */
	if(pos+size>=MAX_OVERLAY_PAGES*PAGE_SIZE)
	{
		pr_err("%s: Overlay bigger than %lu (?) - cant process\n",__func__,MAX_OVERLAY_PAGES*PAGE_SIZE);
		return -ENOMEM;
	}

	if(size == PAGE_SIZE)
	{
		pr_info("%s: pos %u\n",__func__,(unsigned)pos);
		memcpy(&(buf[pos]),buffer,size);
		pr_info("%s: Copying data to buf[%u-%u]\n",__func__,(unsigned)pos,(unsigned)pos+size-1);
		index++;
		rv=size;
	}
	else
	{
		ssize_t bufsize=pos+size;
		index=0;
		memcpy(&(buf[pos]),buffer,size);
		pr_info("%s: Writing %llu byte overlay\n",__func__,(long long unsigned int)bufsize);
		if(create)
			rv=create_overlay(&buf[0], bufsize);
		else
			rv=delete_overlay(&buf[0], bufsize);
		if(rv)
			pr_err("%s: Failed to %s overlay (%d)\n",__func__,(create)?"add":"remove",rv);
		else
			rv=size;
	}

	return rv;
}
static ssize_t overlay_add_store(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buffer,
			loff_t pos, size_t size)
{
	return overlay_modify_store(filp,kobj,attr,buffer,pos,size,1);
}
static ssize_t overlay_del_store(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buffer,
			loff_t pos, size_t size)
{
	/* Calculate csum and search for the matching overlay key */
	/* If found, call of_overlay_remove(int *ovcs_id) */
	return overlay_modify_store(filp,kobj,attr,buffer,pos,size,0);
	//pr_err("%s: Not implemented\n",__func__);
	//return -ENOSYS;
}

static int __init mva_overlay_init(void)
{
	pr_info("%s: Hello Peeps\n",__func__);
	return create_sysfs_for_overlays(&g_o);
};

static void __exit mva_overlay_exit(void)
{
	pr_info("%s: Bye Bye\n",__func__);
	remove_sysfs_for_overlays(&g_o);
}

// Register these functions
module_init(mva_overlay_init);
module_exit(mva_overlay_exit);

MODULE_DESCRIPTION("module for allowing userspace to do DT overlay merges");
MODULE_AUTHOR("Matti Vaittine <matti.vaittinen@fi.rohmeurope.com>");
MODULE_LICENSE("GPL");

