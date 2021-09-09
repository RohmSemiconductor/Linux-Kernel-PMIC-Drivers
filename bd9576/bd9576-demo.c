#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
//#include <linux/fs.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>

#define SAMPLES_MAX 255

struct bd9576_irq_data {
	struct timespec time;
	uint64_t reason_mask1;
	uint64_t reason_mask2;
};

struct bd9576_driver_data {
	struct i2c_client *c;
	u8 status;
	u8 mask;
	u8 *substatus;
	u8 *submask;
	spinlock_t lock;
	int w_index;
	int r_index;
	struct bin_attribute b;
};


static struct bd9576_irq_data irqbuf[SAMPLES_MAX] = { { {0,0},0,0} };

static ssize_t dump_events(struct bd9576_driver_data *d, char *buf, size_t c)
{
	unsigned int unread;
	unsigned long flags;
	unsigned int count=c/sizeof(struct bd9576_irq_data);
	spin_lock_irqsave(&d->lock,flags);

	if(count > SAMPLES_MAX)
		count = SAMPLES_MAX;

	unread=d->w_index-d->r_index;
	if (unread > SAMPLES_MAX) {
		pr_warn("Skipped %u unread samples\n",unread - SAMPLES_MAX);
		d->r_index=d->w_index-SAMPLES_MAX+1;
	}
	pr_debug("%u samples in buff (rindex=%u, windex=%u)\n", unread, d->r_index, d->w_index);

	if(unread > count) {
		unread = count;
	}

	pr_debug("Reading %u items\n",unread);

	if(unread) {
		if ((d->r_index%SAMPLES_MAX)+unread >= SAMPLES_MAX) {
			/* first from r_index to end of array */
			unsigned start_idx = d->r_index%SAMPLES_MAX;
			unsigned write_amnt = SAMPLES_MAX-start_idx;
			unsigned write_size = write_amnt*sizeof(struct bd9576_irq_data);
			memcpy(buf,&irqbuf[start_idx],write_size);
			/* Then from array start to unread total-already read */
			start_idx = 0;
			memcpy(&buf[write_amnt], &irqbuf[0], (unread-write_amnt)*sizeof(struct bd9576_irq_data));
		} else {
			memcpy(buf, &irqbuf[d->r_index%SAMPLES_MAX], unread*sizeof(struct bd9576_irq_data));
		}
		pr_debug("Advancing rindex to %u (windex still %u)\n\n",d->r_index+unread,d->w_index);
		d->r_index+=unread;
	}
	spin_unlock_irqrestore(&d->lock,flags);
	return unread*sizeof(struct bd9576_irq_data);
}

static ssize_t bd9576_events_show(struct file *file, struct kobject *kobj,
				  struct bin_attribute *attr, char *buf,
				  loff_t pos, size_t count)
{
	struct bd9576_driver_data *d = container_of(attr, struct bd9576_driver_data, b);
	pr_debug("Show called: pos %lu, count %lu\n",(unsigned long) pos, (unsigned long)count);
	return dump_events(d, buf, count);
}

static const char *reason_dbg_txt[64] = {
	/*I2c/ Thermal reg 0x23 */
	"I2C Write Error FuSa Mode Type1",
	"I2C Write Error Status in FuSa Mode Type2 with 1 bit error",
	"I2C Write Error Status in FuSa Mode Type2 with more than 2 bit error",
	"unknown I2C/THERM -b3",
	"Thermal Shut Down Detection",
	"Thermal Warning",
	"unknown I2C/THERM -b6",
	"unknown I2C/THERM -b7",
	/* Over voltage protection */
	"Vout1 OVP",
	"Vout2 OVP",
	"Vout3 OVP",
	"Vout4 OVP",
	"Unknown OVP -b4",
	"Vout_L OVP",
	"Unknown OVP -b6",
	"Unknown OVP -b7",
	/* Short Circuit Protection */
	"Vout1 SCP",
	"Vout2 SCP",
	"Vout3 SCP",
	"Vout4 SCP",
	"Unknown SCP -b4",
	"Vout_L SCP",
	"Unknown SCP -b6",
	"Unknown SCP -b7",
	/* Over Current Protection */
	"Vout1 OCP",
	"Vout2 OCP",
	"Vout3 OCP",
	"Vout4A OCP",
	"Vout4B OCP",
	"Unknown OCP -b5",
	"VOUTS1 OCP",
	"LDSW OCP",
	/* Over voltage detection */
	"Vout1 OVD",
	"Vout2 OVD",
	"Vout3 OVD",
	"Vout4 OVD",
	"Unknown OVD -b4",
	"Vout_L1 OVD",
	"Unknown OVD -b6",
	"Unknown OVD -b7",
	/* Under Voltage Detection */
	"Vout1 UVD",
	"Vout2 UVD",
	"Vout3 UVD",
	"Vout4 UVD",
	"Unknown UVD -b4",
	"VoutL1 UVD",
	"VoutS1 Over Current Warning",
	"Unknown UVD -b7",
	/* Under Voltage Protection */
	"VIN1 UVP",
	"VIN2 UVP",
	"VIN3 UVP",
	"VIN4 UVP",
	"VIN5 UVP",
	"VIN6 UVP",
	"VIN7 UVP",
	"Unknown UVP -b7",
	/* System Status */
	"Self Diagnisis Err",
	"WatchDog timer Err",
	"Reference Voltage Mutual Monitoring Err",
	"Oscillator Mututal Monitor Error",
	"EEPROM CRC err",
	"EEPROM Internal State Completion",
	"Power-Off seq Hang-Up timer",
	"V1 Power-Off Hang-Up timer",
};

static void irq_to_mask(int irq, int sub, struct bd9576_irq_data *i)
{
	int linear_irq_no = 8*irq+sub;
	if (linear_irq_no>128) {
		pr_err("WTF?");
		i->reason_mask1 = i->reason_mask2 = 0;
		return;
	}
	if (linear_irq_no<64)
		i->reason_mask1 = (1LLU << linear_irq_no);
	if (linear_irq_no>=64)
		i->reason_mask2 = (1LLU << (linear_irq_no-64));
	pr_debug("%s\n", reason_dbg_txt[linear_irq_no]);
}

/* Note, this is called spin lock held. Don't do stupid things here */
static void handle_irq(int irq, int sub_bit, struct bd9576_driver_data *d,struct timespec *ts)
{
	struct bd9576_irq_data *i;

	i = &irqbuf[d->w_index%SAMPLES_MAX];
	irq_to_mask(irq,sub_bit,i);
	i->time = *ts;
	d->w_index++;
}

static irqreturn_t bd9576_isr(int irq, void *data)
{
	struct bd9576_driver_data *d = (struct bd9576_driver_data *) data;
	u8 status, mask;
	struct timespec ts;
	irqreturn_t rv = IRQ_NONE;
	unsigned int b;
	unsigned long unmasked_irqs;

	ktime_get_ts(&ts);
	status = i2c_smbus_read_byte_data(d->c, d->status);
	mask = i2c_smbus_read_byte_data(d->c, d->mask);
	pr_debug("status_addr=0x%02x, status=0x%x, mask_addr=0x%02x, mask=0x%x\n",d->status,status,d->mask,mask);
	unmasked_irqs = (status&(~mask));
	pr_debug("unmasked_irqs = %lu\n",unmasked_irqs);
	for_each_set_bit(b, &unmasked_irqs, 8*sizeof(status))
	{
		unsigned int sb;
		unsigned long unmasked_subs=0;

		u8 substatus = i2c_smbus_read_byte_data(d->c, d->substatus[b]);
		u8 submask = 0xff;

		if(d->submask[b])
			submask = i2c_smbus_read_byte_data(d->c, d->submask[b]);

		pr_debug("status_addr=0x%02x, substatus=0x%x, mask_addr=0x%02x, submask=0x%x\n",d->substatus[b], substatus, d->submask[b], submask);

		unmasked_subs = substatus&(~submask);
		for_each_set_bit(sb, &unmasked_subs, 8*sizeof(substatus))
		{
			spin_lock(&d->lock);
			handle_irq(b,sb,d,&ts);
			spin_unlock(&d->lock);
			rv = IRQ_HANDLED;
		}
		i2c_smbus_write_byte_data(d->c, d->substatus[b], unmasked_subs);
	}
	i2c_smbus_write_byte_data(d->c, d->status, unmasked_irqs);
	sysfs_notify(&d->c->dev.kobj, NULL, d->b.attr.name);
	return rv;
}

static u8 sub_status_regs[]	= { 0x23, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B };
static u8 sub_mask_regs[]	= { 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C };

static int init_driver_data(struct bd9576_driver_data *d, struct i2c_client *client)
{
	spin_lock_init(&d->lock);
	d->b.attr.name = "bd9576_irq";
	d->b.attr.mode = S_IRUSR;
	d->b.read = &bd9576_events_show;
	d->b.size = SAMPLES_MAX*sizeof(struct bd9576_irq_data);
	sysfs_bin_attr_init(&d->b);
	d->status = 0x30;
	d->mask = 0x31;
	d->submask = &sub_mask_regs[0];
	d->substatus = &sub_status_regs[0];
	d->c = client;
	dev_set_drvdata(&client->dev,d);
	return 0;
}
static int create_sysfs(struct bd9576_driver_data *d)
{
	if (d)
		return sysfs_create_bin_file(&d->c->dev.kobj, &d->b);
	return -EINVAL;
}
static void remove_sysfs(struct bd9576_driver_data *d)
{
	if (d)
		sysfs_remove_bin_file(&d->c->dev.kobj, &d->b);
}

static int bd9576_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int r = -ENODEV;
	struct bd9576_driver_data *d;

	dev_info(&client->dev, "%s : bd9576-demo i2c driver probed\n",__func__);
	dev_info(&client->dev, "%s : my i2c slave address is 0x%x, name is '%s'\n",__func__,(unsigned)client->addr,client->name);

	if(client->irq) {
		d = devm_kzalloc(&client->dev,sizeof(*d),GFP_KERNEL);
		if (d)
			r = init_driver_data(d,client);
		else
			r = -ENOMEM;
		if (!r)
			r = devm_request_threaded_irq(&client->dev,
						      client->irq, NULL,
						      bd9576_isr, IRQF_ONESHOT,
						      "bd9576-irq", d);
		if (!r)
			r = create_sysfs(d);
	}
	return r;
}

static int bd9576_remove(struct i2c_client *client)
{
	struct bd9576_driver_data *d;
	d = (struct bd9576_driver_data *)dev_get_drvdata(&client->dev);
	remove_sysfs(d);
	return 0;
}

static const struct of_device_id test_of_match[] = {
	{
		.compatible = "rohm,bd9576-demo",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, test_of_match);

static struct i2c_driver test_drv = {
	.driver = {
		.name	= "bd9576-demo",
		.of_match_table = of_match_ptr(test_of_match),
	},
	.probe = bd9576_probe,
	.remove = bd9576_remove,
};
module_i2c_driver(test_drv);


MODULE_DESCRIPTION("Driver for handling bd9576 interrupts");
MODULE_LICENSE("GPL");

