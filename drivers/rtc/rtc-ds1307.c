/*
 * rtc-ds1307.c - RTC driver for some mostly-compatible I2C chips.
 *
 *  Copyright (C) 2005 James Chapman (ds1337 core)
 *  Copyright (C) 2006 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/rtc.h>
#include <linux/bcd.h>



/* We can't determine type by probing, but if we expect pre-Linux code
 * to have set the chip up as a clock (turning on the oscillator and
 * setting the date and time), Linux can ignore the non-clock features.
 * That's a natural job for a factory or repair bench.
 */
enum ds_type {
	ds_1307,
	ds_1337,
	ds_1338,
	ds_1339,
	ds_1340,
	m41t00,
	// rs5c372 too?  different address...
};


/* RTC registers don't differ much, except for the century flag */
#define DS1307_REG_SECS		0x00	/* 00-59 */
#	define DS1307_BIT_CH		0x80
#	define DS1340_BIT_nEOSC		0x80
#define DS1307_REG_MIN		0x01	/* 00-59 */
#define DS1307_REG_HOUR		0x02	/* 00-23, or 1-12{am,pm} */
#	define DS1307_BIT_12HR		0x40	/* in REG_HOUR */
#	define DS1307_BIT_PM		0x20	/* in REG_HOUR */
#	define DS1340_BIT_CENTURY_EN	0x80	/* in REG_HOUR */
#	define DS1340_BIT_CENTURY	0x40	/* in REG_HOUR */
#define DS1307_REG_WDAY		0x03	/* 01-07 */
#define DS1307_REG_MDAY		0x04	/* 01-31 */
#define DS1307_REG_MONTH	0x05	/* 01-12 */
#	define DS1337_BIT_CENTURY	0x80	/* in REG_MONTH */
#define DS1307_REG_YEAR		0x06	/* 00-99 */

/* Other registers (control, status, alarms, trickle charge, NVRAM, etc)
 * start at 7, and they differ a LOT. Only control and status matter for
 * basic RTC date and time functionality; be careful using them.
 */
#define DS1307_REG_CONTROL	0x07		/* or ds1338 */
#	define DS1307_BIT_OUT		0x80
#	define DS1338_BIT_OSF		0x20
#	define DS1307_BIT_SQWE		0x10
#	define DS1307_BIT_RS1		0x02
#	define DS1307_BIT_RS0		0x01
#define DS1337_REG_CONTROL	0x0e
#	define DS1337_BIT_nEOSC		0x80
#	define DS1339_BIT_BBSQI		0x20
#	define DS1337_BIT_RS2		0x10
#	define DS1337_BIT_RS1		0x08
#	define DS1337_BIT_INTCN		0x04
#	define DS1337_BIT_A2IE		0x02
#	define DS1337_BIT_A1IE		0x01
#define DS1340_REG_CONTROL	0x07
#	define DS1340_BIT_OUT		0x80
#	define DS1340_BIT_FT		0x40
#	define DS1340_BIT_CALIB_SIGN	0x20
#	define DS1340_M_CALIBRATION	0x1f
#define DS1340_REG_FLAG		0x09
#	define DS1340_BIT_OSF		0x80
#define DS1337_REG_STATUS	0x0f
#	define DS1337_BIT_OSF		0x80
#	define DS1337_BIT_A2I		0x02
#	define DS1337_BIT_A1I		0x01
#define DS1339_REG_ALARM1_SECS	0x07
#define DS1339_REG_TRICKLE	0x10



struct ds1307 {
	u8			reg_addr;
	u8			regs[11];
	enum ds_type		type;
	unsigned long		flags;
#define HAS_NVRAM	0		/* bit 0 == sysfs file active */
#define HAS_ALARM	1		/* bit 1 == irq claimed */
	struct i2c_msg		msg[2];
	struct i2c_client	*client;
	struct rtc_device	*rtc;
	struct work_struct	work;
};

struct chip_desc {
	unsigned		nvram56:1;
	unsigned		alarm:1;
};

static const struct chip_desc chips[] = {
[ds_1307] = {
	.nvram56	= 1,
},
[ds_1337] = {
	.alarm		= 1,
},
[ds_1338] = {
	.nvram56	= 1,
},
[ds_1339] = {
	.alarm		= 1,
},
[ds_1340] = {
},
[m41t00] = {
}, };

static const struct i2c_device_id ds1307_id[] = {
	{ "ds1307", ds_1307 },
	{ "ds1337", ds_1337 },
	{ "ds1338", ds_1338 },
	{ "ds1339", ds_1339 },
	{ "ds1340", ds_1340 },
	{ "m41t00", m41t00 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds1307_id);

/*----------------------------------------------------------------------*/

/*
 * The IRQ logic includes a "real" handler running in IRQ context just
 * long enough to schedule this workqueue entry.   We need a task context
 * to talk to the RTC, since I2C I/O calls require that; and disable the
 * IRQ until we clear its status on the chip, so that this handler can
 * work with any type of triggering (not just falling edge).
 *
 * The ds1337 and ds1339 both have two alarms, but we only use the first
 * one (with a "seconds" field).  For ds1337 we expect nINTA is our alarm
 * signal; ds1339 chips have only one alarm signal.
 */
static void ds1307_work(struct work_struct *work)
{
	struct ds1307		*ds1307;
	struct i2c_client	*client;
	struct mutex		*lock;
	int			stat, control;

	ds1307 = container_of(work, struct ds1307, work);
	client = ds1307->client;
	lock = &ds1307->rtc->ops_lock;

	mutex_lock(lock);
	stat = i2c_smbus_read_byte_data(client, DS1337_REG_STATUS);
	if (stat < 0)
		goto out;

	if (stat & DS1337_BIT_A1I) {
		stat &= ~DS1337_BIT_A1I;
		i2c_smbus_write_byte_data(client, DS1337_REG_STATUS, stat);

		control = i2c_smbus_read_byte_data(client, DS1337_REG_CONTROL);
		if (control < 0)
			goto out;

		control &= ~DS1337_BIT_A1IE;
		i2c_smbus_write_byte_data(client, DS1337_REG_CONTROL, control);

		/* rtc_update_irq() assumes that it is called
		 * from IRQ-disabled context.
		 */
		local_irq_disable();
		rtc_update_irq(ds1307->rtc, 1, RTC_AF | RTC_IRQF);
		local_irq_enable();
	}

out:
	if (test_bit(HAS_ALARM, &ds1307->flags))
		enable_irq(client->irq);
	mutex_unlock(lock);
}

static irqreturn_t ds1307_irq(int irq, void *dev_id)
{
	struct i2c_client	*client = dev_id;
	struct ds1307		*ds1307 = i2c_get_clientdata(client);

	disable_irq_nosync(irq);
	schedule_work(&ds1307->work);
	return IRQ_HANDLED;
}

/*----------------------------------------------------------------------*/

static int ds1307_get_time(struct device *dev, struct rtc_time *t)
{
	struct ds1307	*ds1307 = dev_get_drvdata(dev);
	int		tmp;

	/* read the RTC date and time registers all at once */
	ds1307->reg_addr = 0;
	ds1307->msg[1].flags = I2C_M_RD;
	ds1307->msg[1].len = 7;

	tmp = i2c_transfer(to_i2c_adapter(ds1307->client->dev.parent),
			ds1307->msg, 2);
	if (tmp != 2) {
		dev_err(dev, "%s error %d\n", "read", tmp);
		return -EIO;
	}

	dev_dbg(dev, "%s: %02x %02x %02x %02x %02x %02x %02x\n",
			"read",
			ds1307->regs[0], ds1307->regs[1],
			ds1307->regs[2], ds1307->regs[3],
			ds1307->regs[4], ds1307->regs[5],
			ds1307->regs[6]);

	t->tm_sec = bcd2bin(ds1307->regs[DS1307_REG_SECS] & 0x7f);
	t->tm_min = bcd2bin(ds1307->regs[DS1307_REG_MIN] & 0x7f);
	tmp = ds1307->regs[DS1307_REG_HOUR] & 0x3f;
	t->tm_hour = bcd2bin(tmp);
	t->tm_wday = bcd2bin(ds1307->regs[DS1307_REG_WDAY] & 0x07) - 1;
	t->tm_mday = bcd2bin(ds1307->regs[DS1307_REG_MDAY] & 0x3f);
	tmp = ds1307->regs[DS1307_REG_MONTH] & 0x1f;
	t->tm_mon = bcd2bin(tmp) - 1;

	/* assume 20YY not 19YY, and ignore DS1337_BIT_CENTURY */
	t->tm_year = bcd2bin(ds1307->regs[DS1307_REG_YEAR]) + 100;

	dev_dbg(dev, "%s secs=%d, mins=%d, "
		"hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		"read", t->tm_sec, t->tm_min,
		t->tm_hour, t->tm_mday,
		t->tm_mon, t->tm_year, t->tm_wday);

	/* initial clock setting can be undefined */
	return rtc_valid_tm(t);
}

static int ds1307_set_time(struct device *dev, struct rtc_time *t)
{
	struct ds1307	*ds1307 = dev_get_drvdata(dev);
	int		result;
	int		tmp;
	u8		*buf = ds1307->regs;

	dev_dbg(dev, "%s secs=%d, mins=%d, "
		"hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		"write", t->tm_sec, t->tm_min,
		t->tm_hour, t->tm_mday,
		t->tm_mon, t->tm_year, t->tm_wday);

	*buf++ = 0;		/* first register addr */
	buf[DS1307_REG_SECS] = bin2bcd(t->tm_sec);
	buf[DS1307_REG_MIN] = bin2bcd(t->tm_min);
	buf[DS1307_REG_HOUR] = bin2bcd(t->tm_hour);
	buf[DS1307_REG_WDAY] = bin2bcd(t->tm_wday + 1);
	buf[DS1307_REG_MDAY] = bin2bcd(t->tm_mday);
	buf[DS1307_REG_MONTH] = bin2bcd(t->tm_mon + 1);

	/* assume 20YY not 19YY */
	tmp = t->tm_year - 100;
	buf[DS1307_REG_YEAR] = bin2bcd(tmp);

	switch (ds1307->type) {
	case ds_1337:
	case ds_1339:
		buf[DS1307_REG_MONTH] |= DS1337_BIT_CENTURY;
		break;
	case ds_1340:
		buf[DS1307_REG_HOUR] |= DS1340_BIT_CENTURY_EN
				| DS1340_BIT_CENTURY;
		break;
	default:
		break;
	}

	ds1307->msg[1].flags = 0;
	ds1307->msg[1].len = 8;

	dev_dbg(dev, "%s: %02x %02x %02x %02x %02x %02x %02x\n",
		"write", buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6]);

	result = i2c_transfer(to_i2c_adapter(ds1307->client->dev.parent),
			&ds1307->msg[1], 1);
	if (result != 1) {
		dev_err(dev, "%s error %d\n", "write", tmp);
		return -EIO;
	}
	return 0;
}

static int ds1307_read_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct i2c_client       *client = to_i2c_client(dev);
	struct ds1307		*ds1307 = i2c_get_clientdata(client);
	int			ret;

	if (!test_bit(HAS_ALARM, &ds1307->flags))
		return -EINVAL;

	/* read all ALARM1, ALARM2, and status registers at once */
	ds1307->reg_addr = DS1339_REG_ALARM1_SECS;
	ds1307->msg[1].flags = I2C_M_RD;
	ds1307->msg[1].len = 9;

	ret = i2c_transfer(to_i2c_adapter(client->dev.parent),
			ds1307->msg, 2);
	if (ret != 2) {
		dev_err(dev, "%s error %d\n", "alarm read", ret);
		return -EIO;
	}

	dev_dbg(dev, "%s: %02x %02x %02x %02x, %02x %02x %02x, %02x %02x\n",
			"alarm read",
			ds1307->regs[0], ds1307->regs[1],
			ds1307->regs[2], ds1307->regs[3],
			ds1307->regs[4], ds1307->regs[5],
			ds1307->regs[6], ds1307->regs[7],
			ds1307->regs[8]);

	/* report alarm time (ALARM1); assume 24 hour and day-of-month modes,
	 * and that all four fields are checked matches
	 */
	t->time.tm_sec = bcd2bin(ds1307->regs[0] & 0x7f);
	t->time.tm_min = bcd2bin(ds1307->regs[1] & 0x7f);
	t->time.tm_hour = bcd2bin(ds1307->regs[2] & 0x3f);
	t->time.tm_mday = bcd2bin(ds1307->regs[3] & 0x3f);
	t->time.tm_mon = -1;
	t->time.tm_year = -1;
	t->time.tm_wday = -1;
	t->time.tm_yday = -1;
	t->time.tm_isdst = -1;

	/* ... and status */
	t->enabled = !!(ds1307->regs[7] & DS1337_BIT_A1IE);
	t->pending = !!(ds1307->regs[8] & DS1337_BIT_A1I);

	dev_dbg(dev, "%s secs=%d, mins=%d, "
		"hours=%d, mday=%d, enabled=%d, pending=%d\n",
		"alarm read", t->time.tm_sec, t->time.tm_min,
		t->time.tm_hour, t->time.tm_mday,
		t->enabled, t->pending);

	return 0;
}

static int ds1307_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct i2c_client       *client = to_i2c_client(dev);
	struct ds1307		*ds1307 = i2c_get_clientdata(client);
	unsigned char		*buf = ds1307->regs;
	u8			control, status;
	int			ret;

	if (!test_bit(HAS_ALARM, &ds1307->flags))
		return -EINVAL;

	dev_dbg(dev, "%s secs=%d, mins=%d, "
		"hours=%d, mday=%d, enabled=%d, pending=%d\n",
		"alarm set", t->time.tm_sec, t->time.tm_min,
		t->time.tm_hour, t->time.tm_mday,
		t->enabled, t->pending);

	/* read current status of both alarms and the chip */
	ds1307->reg_addr = DS1339_REG_ALARM1_SECS;
	ds1307->msg[1].flags = I2C_M_RD;
	ds1307->msg[1].len = 9;

	ret = i2c_transfer(to_i2c_adapter(client->dev.parent),
			ds1307->msg, 2);
	if (ret != 2) {
		dev_err(dev, "%s error %d\n", "alarm write", ret);
		return -EIO;
	}
	control = ds1307->regs[7];
	status = ds1307->regs[8];

	dev_dbg(dev, "%s: %02x %02x %02x %02x, %02x %02x %02x, %02x %02x\n",
			"alarm set (old status)",
			ds1307->regs[0], ds1307->regs[1],
			ds1307->regs[2], ds1307->regs[3],
			ds1307->regs[4], ds1307->regs[5],
			ds1307->regs[6], control, status);

	/* set ALARM1, using 24 hour and day-of-month modes */
	*buf++ = DS1339_REG_ALARM1_SECS;	/* first register addr */
	buf[0] = bin2bcd(t->time.tm_sec);
	buf[1] = bin2bcd(t->time.tm_min);
	buf[2] = bin2bcd(t->time.tm_hour);
	buf[3] = bin2bcd(t->time.tm_mday);

	/* set ALARM2 to non-garbage */
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = 0;

	/* optionally enable ALARM1 */
	buf[7] = control & ~(DS1337_BIT_A1IE | DS1337_BIT_A2IE);
	if (t->enabled) {
		dev_dbg(dev, "alarm IRQ armed\n");
		buf[7] |= DS1337_BIT_A1IE;	/* only ALARM1 is used */
	}
	buf[8] = status & ~(DS1337_BIT_A1I | DS1337_BIT_A2I);

	ds1307->msg[1].flags = 0;
	ds1307->msg[1].len = 10;

	ret = i2c_transfer(to_i2c_adapter(client->dev.parent),
			&ds1307->msg[1], 1);
	if (ret != 1) {
		dev_err(dev, "can't set alarm time\n");
		return -EIO;
	}

	return 0;
}

static int ds1307_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct i2c_client	*client = to_i2c_client(dev);
	struct ds1307		*ds1307 = i2c_get_clientdata(client);
	int			ret;

	switch (cmd) {
	case RTC_AIE_OFF:
		if (!test_bit(HAS_ALARM, &ds1307->flags))
			return -ENOTTY;

		ret = i2c_smbus_read_byte_data(client, DS1337_REG_CONTROL);
		if (ret < 0)
			return ret;

		ret &= ~DS1337_BIT_A1IE;

		ret = i2c_smbus_write_byte_data(client,
						DS1337_REG_CONTROL, ret);
		if (ret < 0)
			return ret;

		break;

	case RTC_AIE_ON:
		if (!test_bit(HAS_ALARM, &ds1307->flags))
			return -ENOTTY;

		ret = i2c_smbus_read_byte_data(client, DS1337_REG_CONTROL);
		if (ret < 0)
			return ret;

		ret |= DS1337_BIT_A1IE;

		ret = i2c_smbus_write_byte_data(client,
						DS1337_REG_CONTROL, ret);
		if (ret < 0)
			return ret;

		break;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static const struct rtc_class_ops ds13xx_rtc_ops = {
	.read_time	= ds1307_get_time,
	.set_time	= ds1307_set_time,
	.read_alarm	= ds1307_read_alarm,
	.set_alarm	= ds1307_set_alarm,
	.ioctl		= ds1307_ioctl,
};

/*----------------------------------------------------------------------*/

#define NVRAM_SIZE	56

static ssize_t
ds1307_nvram_read(struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct i2c_client	*client;
	struct ds1307		*ds1307;
	struct i2c_msg		msg[2];
	int			result;

	client = kobj_to_i2c_client(kobj);
	ds1307 = i2c_get_clientdata(client);

	if (unlikely(off >= NVRAM_SIZE))
		return 0;
	if ((off + count) > NVRAM_SIZE)
		count = NVRAM_SIZE - off;
	if (unlikely(!count))
		return count;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	buf[0] = 8 + off;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = count;
	msg[1].buf = buf;

	result = i2c_transfer(to_i2c_adapter(client->dev.parent), msg, 2);
	if (result != 2) {
		dev_err(&client->dev, "%s error %d\n", "nvram read", result);
		return -EIO;
	}
	return count;
}

static ssize_t
ds1307_nvram_write(struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct i2c_client	*client;
	u8			buffer[NVRAM_SIZE + 1];
	int			ret;

	client = kobj_to_i2c_client(kobj);

	if (unlikely(off >= NVRAM_SIZE))
		return -EFBIG;
	if ((off + count) > NVRAM_SIZE)
		count = NVRAM_SIZE - off;
	if (unlikely(!count))
		return count;

	buffer[0] = 8 + off;
	memcpy(buffer + 1, buf, count);

	ret = i2c_master_send(client, buffer, count + 1);
	return (ret < 0) ? ret : (ret - 1);
}

static struct bin_attribute nvram = {
	.attr = {
		.name	= "nvram",
		.mode	= S_IRUGO | S_IWUSR,
	},

	.read	= ds1307_nvram_read,
	.write	= ds1307_nvram_write,
	.size	= NVRAM_SIZE,
};

/*----------------------------------------------------------------------*/

static struct i2c_driver ds1307_driver;

static int __devinit ds1307_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct ds1307		*ds1307;
	int			err = -ENODEV;
	int			tmp;
	const struct chip_desc	*chip = &chips[id->driver_data];
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int			want_irq = false;

	if (!i2c_check_functionality(adapter,
			I2C_FUNC_I2C | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -EIO;

	if (!(ds1307 = kzalloc(sizeof(struct ds1307), GFP_KERNEL)))
		return -ENOMEM;

	ds1307->client = client;
	i2c_set_clientdata(client, ds1307);

	ds1307->msg[0].addr = client->addr;
	ds1307->msg[0].flags = 0;
	ds1307->msg[0].len = 1;
	ds1307->msg[0].buf = &ds1307->reg_addr;

	ds1307->msg[1].addr = client->addr;
	ds1307->msg[1].flags = I2C_M_RD;
	ds1307->msg[1].len = sizeof(ds1307->regs);
	ds1307->msg[1].buf = ds1307->regs;

	ds1307->type = id->driver_data;

	switch (ds1307->type) {
	case ds_1337:
	case ds_1339:
		/* has IRQ? */
		if (ds1307->client->irq > 0 && chip->alarm) {
			INIT_WORK(&ds1307->work, ds1307_work);
			want_irq = true;
		}

		ds1307->reg_addr = DS1337_REG_CONTROL;
		ds1307->msg[1].len = 2;

		/* get registers that the "rtc" read below won't read... */
		tmp = i2c_transfer(adapter, ds1307->msg, 2);
		if (tmp != 2) {
			pr_debug("read error %d\n", tmp);
			err = -EIO;
			goto exit_free;
		}

		ds1307->reg_addr = 0;
		ds1307->msg[1].len = sizeof(ds1307->regs);

		/* oscillator off?  turn it on, so clock can tick. */
		if (ds1307->regs[0] & DS1337_BIT_nEOSC)
			ds1307->regs[0] &= ~DS1337_BIT_nEOSC;

		/* Using IRQ?  Disable the square wave and both alarms.
		 * For ds1339, be sure alarms can trigger when we're
		 * running on Vbackup (BBSQI); we assume ds1337 will
		 * ignore that bit
		 */
		if (want_irq) {
			ds1307->regs[0] |= DS1337_BIT_INTCN | DS1339_BIT_BBSQI;
			ds1307->regs[0] &= ~(DS1337_BIT_A2IE | DS1337_BIT_A1IE);
		}

		i2c_smbus_write_byte_data(client, DS1337_REG_CONTROL,
							ds1307->regs[0]);

		/* oscillator fault?  clear flag, and warn */
		if (ds1307->regs[1] & DS1337_BIT_OSF) {
			i2c_smbus_write_byte_data(client, DS1337_REG_STATUS,
				ds1307->regs[1] & ~DS1337_BIT_OSF);
			dev_warn(&client->dev, "SET TIME!\n");
		}
		break;
	default:
		break;
	}

read_rtc:
	/* read RTC registers */

	tmp = i2c_transfer(adapter, ds1307->msg, 2);
	if (tmp != 2) {
		pr_debug("read error %d\n", tmp);
		err = -EIO;
		goto exit_free;
	}

	/* minimal sanity checking; some chips (like DS1340) don't
	 * specify the extra bits as must-be-zero, but there are
	 * still a few values that are clearly out-of-range.
	 */
	tmp = ds1307->regs[DS1307_REG_SECS];
	switch (ds1307->type) {
	case ds_1307:
	case m41t00:
		/* clock halted?  turn it on, so clock can tick. */
		if (tmp & DS1307_BIT_CH) {
			i2c_smbus_write_byte_data(client, DS1307_REG_SECS, 0);
			dev_warn(&client->dev, "SET TIME!\n");
			goto read_rtc;
		}
		break;
	case ds_1338:
		/* clock halted?  turn it on, so clock can tick. */
		if (tmp & DS1307_BIT_CH)
			i2c_smbus_write_byte_data(client, DS1307_REG_SECS, 0);

		/* oscillator fault?  clear flag, and warn */
		if (ds1307->regs[DS1307_REG_CONTROL] & DS1338_BIT_OSF) {
			i2c_smbus_write_byte_data(client, DS1307_REG_CONTROL,
					ds1307->regs[DS1307_REG_CONTROL]
					& ~DS1338_BIT_OSF);
			dev_warn(&client->dev, "SET TIME!\n");
			goto read_rtc;
		}
		break;
	case ds_1340:
		/* clock halted?  turn it on, so clock can tick. */
		if (tmp & DS1340_BIT_nEOSC)
			i2c_smbus_write_byte_data(client, DS1307_REG_SECS, 0);

		tmp = i2c_smbus_read_byte_data(client, DS1340_REG_FLAG);
		if (tmp < 0) {
			pr_debug("read error %d\n", tmp);
			err = -EIO;
			goto exit_free;
		}

		/* oscillator fault?  clear flag, and warn */
		if (tmp & DS1340_BIT_OSF) {
			i2c_smbus_write_byte_data(client, DS1340_REG_FLAG, 0);
			dev_warn(&client->dev, "SET TIME!\n");
		}
		break;
	case ds_1337:
	case ds_1339:
		break;
	}

	tmp = ds1307->regs[DS1307_REG_SECS];
	tmp = bcd2bin(tmp & 0x7f);
	if (tmp > 60)
		goto exit_bad;
	tmp = bcd2bin(ds1307->regs[DS1307_REG_MIN] & 0x7f);
	if (tmp > 60)
		goto exit_bad;

	tmp = bcd2bin(ds1307->regs[DS1307_REG_MDAY] & 0x3f);
	if (tmp == 0 || tmp > 31)
		goto exit_bad;

	tmp = bcd2bin(ds1307->regs[DS1307_REG_MONTH] & 0x1f);
	if (tmp == 0 || tmp > 12)
		goto exit_bad;

	tmp = ds1307->regs[DS1307_REG_HOUR];
	switch (ds1307->type) {
	case ds_1340:
	case m41t00:
		/* NOTE: ignores century bits; fix before deploying
		 * systems that will run through year 2100.
		 */
		break;
	default:
		if (!(tmp & DS1307_BIT_12HR))
			break;

		/* Be sure we're in 24 hour mode.  Multi-master systems
		 * take note...
		 */
		tmp = bcd2bin(tmp & 0x1f);
		if (tmp == 12)
			tmp = 0;
		if (ds1307->regs[DS1307_REG_HOUR] & DS1307_BIT_PM)
			tmp += 12;
		i2c_smbus_write_byte_data(client,
				DS1307_REG_HOUR,
				bin2bcd(tmp));
	}

	ds1307->rtc = rtc_device_register(client->name, &client->dev,
				&ds13xx_rtc_ops, THIS_MODULE);
	if (IS_ERR(ds1307->rtc)) {
		err = PTR_ERR(ds1307->rtc);
		dev_err(&client->dev,
			"unable to register the class device\n");
		goto exit_free;
	}

	if (want_irq) {
		err = request_irq(client->irq, ds1307_irq, 0,
			  ds1307->rtc->name, client);
		if (err) {
			dev_err(&client->dev,
				"unable to request IRQ!\n");
			goto exit_irq;
		}
		set_bit(HAS_ALARM, &ds1307->flags);
		dev_dbg(&client->dev, "got IRQ %d\n", client->irq);
	}

	if (chip->nvram56) {
		err = sysfs_create_bin_file(&client->dev.kobj, &nvram);
		if (err == 0) {
			set_bit(HAS_NVRAM, &ds1307->flags);
			dev_info(&client->dev, "56 bytes nvram\n");
		}
	}

	return 0;

exit_bad:
	dev_dbg(&client->dev, "%s: %02x %02x %02x %02x %02x %02x %02x\n",
			"bogus register",
			ds1307->regs[0], ds1307->regs[1],
			ds1307->regs[2], ds1307->regs[3],
			ds1307->regs[4], ds1307->regs[5],
			ds1307->regs[6]);
exit_irq:
	if (ds1307->rtc)
		rtc_device_unregister(ds1307->rtc);
exit_free:
	kfree(ds1307);
	return err;
}

static int __devexit ds1307_remove(struct i2c_client *client)
{
	struct ds1307		*ds1307 = i2c_get_clientdata(client);

	if (test_and_clear_bit(HAS_ALARM, &ds1307->flags)) {
		free_irq(client->irq, client);
		cancel_work_sync(&ds1307->work);
	}

	if (test_and_clear_bit(HAS_NVRAM, &ds1307->flags))
		sysfs_remove_bin_file(&client->dev.kobj, &nvram);

	rtc_device_unregister(ds1307->rtc);
	kfree(ds1307);
	return 0;
}

static struct i2c_driver ds1307_driver = {
	.driver = {
		.name	= "rtc-ds1307",
		.owner	= THIS_MODULE,
	},
	.probe		= ds1307_probe,
	.remove		= __devexit_p(ds1307_remove),
	.id_table	= ds1307_id,
};

static int __init ds1307_init(void)
{
	return i2c_add_driver(&ds1307_driver);
}
module_init(ds1307_init);

static void __exit ds1307_exit(void)
{
	i2c_del_driver(&ds1307_driver);
}
module_exit(ds1307_exit);

MODULE_DESCRIPTION("RTC driver for DS1307 and similar chips");
MODULE_LICENSE("GPL");
