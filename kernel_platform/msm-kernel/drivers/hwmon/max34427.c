/*
 * max34417.c - Power Accumulator driver for the Maxim MAX34417
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/max34427.h>
#include <linux/of_gpio.h>
#include <asm/div64.h>
#ifdef CONFIG_DRV_SAMSUNG
#include <linux/sec_class.h>
#else
struct class *max34427_sec_class;
#endif

struct max34427_dev *max34427_dev_i2c;
struct device *max34427_dev;

static int max34427_update(void)
{
	int ret;

	mutex_lock(&max34427_dev_i2c->i2c_lock);
	ret = i2c_smbus_write_byte(max34427_dev_i2c->i2c, MAX34427_CMD_UPDATE);
	mutex_unlock(&max34427_dev_i2c->i2c_lock);

	if (ret < 0)
		pr_info("%s: update failed(%d)\n", __func__, ret);
	else
		pr_info("%s: update success\n", __func__);

	return ret;
}

static int max34427_read_reg(u8 cmd, u8 *data)
{
	int ret;

	mutex_lock(&max34427_dev_i2c->i2c_lock);
	ret = i2c_smbus_read_byte_data(max34427_dev_i2c->i2c, cmd);
	mutex_unlock(&max34427_dev_i2c->i2c_lock);

	if (ret < 0)
		pr_info("%s: Failed to read reg(0x%x), ret(%d)\n", __func__, cmd, ret);

	*data = ret;
	return ret;
}

static int max34427_read_block(u8 cmd, u64 *data)
{
	int i, ret;
	u8 temp[I2C_SMBUS_BLOCK_MAX] = {0,};
	u64 sum = 0;

	mutex_lock(&max34427_dev_i2c->i2c_lock);
	ret = i2c_smbus_read_block_data(max34427_dev_i2c->i2c, cmd, temp);
	mutex_unlock(&max34427_dev_i2c->i2c_lock);

	if (ret < 0) {
		pr_info("%s: CMD(0x%x) block read failed : %d\n", __func__, cmd, ret);
		return ret;
	}

	for (i = 0; i < ret; i++)
		sum |= ((u64)temp[i] << ((ret - i - 1) * 8));

	*data = sum;

	return ret;
}

static int max34427_write_reg(u8 cmd, u8 data)
{
	int ret;

	mutex_lock(&max34427_dev_i2c->i2c_lock);
	ret = i2c_smbus_write_byte_data(max34427_dev_i2c->i2c, cmd, data);
	mutex_unlock(&max34427_dev_i2c->i2c_lock);

	if (ret < 0)
		pr_info("%s: Failed to write reg(0x%x), ret(%d)\n", __func__, cmd, ret);

	return ret;
}

static ssize_t dev_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	u8 data;

	ret = max34427_read_reg(MAX34427_REG_DEV_ID, &data);
	if (ret < 0) {
		snprintf(buf, PAGE_SIZE, "Could not read device id, ret=%d\n", ret);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}
static DEVICE_ATTR_RO(dev_id);

static ssize_t update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;

	ret = max34427_update();
	if (ret < 0)
		return ret;

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}
static DEVICE_ATTR_RO(update);

static ssize_t control_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	u8 data;

	ret = max34427_read_reg(MAX34427_CMD_CONTROL, &data);
	if (ret < 0) {
		return snprintf(buf, PAGE_SIZE, "Could not read control register, ret=%d\n", ret);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

static ssize_t control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret, temp;

	ret = kstrtoint(buf, 0, &temp);
	if (ret)
		return ret;

	pr_info("%s: write control register 0x%x\n", __func__, temp);

	ret = max34427_write_reg(MAX34427_CMD_CONTROL, temp);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(control);

static ssize_t cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 data;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_ACC_COUNT, &data);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read cnt, ret=%d\n", ret);

	return snprintf(buf, PAGE_SIZE, "0x%x , %llu\n", data, data);
}
static DEVICE_ATTR_RO(cnt);

static ssize_t acc_ch1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 data;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_1, &data);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_ch1, ret=%d\n", ret);

	return snprintf(buf, PAGE_SIZE, "0x%x , %llu\n", data, data);
}
static DEVICE_ATTR_RO(acc_ch1);

static ssize_t acc_ch2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 data;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_2, &data);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_ch2, ret=%d\n", ret);

	return snprintf(buf, PAGE_SIZE, "0x%x , %llu\n", data, data);
}
static DEVICE_ATTR_RO(acc_ch2);

static ssize_t vol_ch1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 val, volt, rem;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_V_CH1, &val);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read vol_ch1, ret=%d\n", ret);

	volt = ((val >> 2) * 1000 * VOLT_CORR) / VOLT_FULL_SCALE;
	rem = do_div(volt, 1000);
	pr_info("%s: reg = %x, volt = %d.%03dV\n", __func__, val, volt, rem);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", volt, rem);
}
static DEVICE_ATTR_RO(vol_ch1);

static ssize_t vol_ch2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u64 val, volt, rem;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_V_CH2, &val);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read vol_ch2, ret=%d\n", ret);

	volt = ((val >> 2) * 1000 * VOLT_CORR) / VOLT_FULL_SCALE;
	rem = do_div(volt, 1000);
	pr_info("%s: reg = %x, volt = %d.%03dV\n", __func__, val, volt, rem);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", volt, rem);
}
static DEVICE_ATTR_RO(vol_ch2);

static ssize_t pwr_ch1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max34427_platform_data *pdata = max34427_dev_i2c->pdata;
	u64 acc, cnt, curr, rem;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_ACC_COUNT, &cnt);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_cnt, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_1, &acc);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read pwr_ch1, ret=%d\n", ret);

	if (!pdata) {
		pr_info("%s: pdata is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "pdata is null\n");
	}

	curr = ((acc / cnt) * pdata->corr_current * 1000) / CURRENT_FULL_SCALE;
	rem = do_div(curr, 1000);
	pr_info("%s: acc=%d, cnt=%d, corr=%d, curr=%d.%03dA\n",
			__func__, acc, cnt, pdata->corr_current, curr, rem);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", curr, rem);
}
static DEVICE_ATTR_RO(pwr_ch1);

static ssize_t pwr_ch2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max34427_platform_data *pdata = max34427_dev_i2c->pdata;
	u64 acc, cnt, curr, rem;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_ACC_COUNT, &cnt);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_cnt, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_2, &acc);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read pwr_ch2, ret=%d\n", ret);

	if (!pdata) {
		pr_info("%s: pdata is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "pdata is null\n");
	}

	curr = ((acc / cnt) * pdata->corr_current * 1000) / CURRENT_FULL_SCALE;
	rem = do_div(curr, 1000);
	pr_info("%s: acc=%d, cnt=%d, corr=%d, curr=%d.%03dA\n",
			__func__, acc, cnt, pdata->corr_current, curr, rem);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", curr, rem);
}
static DEVICE_ATTR_RO(pwr_ch2);

static ssize_t all_ch1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max34427_platform_data *pdata = max34427_dev_i2c->pdata;
	u64 acc, cnt, val, curr, volt, pwr, rem_curr, rem_volt, rem_pwr;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_ACC_COUNT, &cnt);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_cnt, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_1, &acc);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read pwr_ch1, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_V_CH1, &val);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read vol_ch1, ret=%d\n", ret);

	if (!pdata) {
		pr_info("%s: pdata is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "pdata is null\n");
	}

	curr = ((acc / cnt) * pdata->corr_current * 1000) / CURRENT_FULL_SCALE;
	volt = ((val >> 2) * 1000 * VOLT_CORR) / VOLT_FULL_SCALE;
	pwr = curr * volt;
	rem_curr = do_div(curr, 1000);
	rem_volt = do_div(volt, 1000);
	rem_pwr = do_div(pwr, 1000000) / 1000;

	pr_info("%s: acc=%d, cnt=%d, corr=%d, curr=%d.%03dA, pwr=%d.%03dW, volt=%d.%03dV\n", __func__,
			acc, cnt, pdata->corr_current, curr, rem_curr, pwr, rem_pwr, volt, rem_volt);

	return snprintf(buf, PAGE_SIZE, "%d.%03dA, %d.%03dW, %d.%03dV\n",
					curr, rem_curr, pwr, rem_pwr, volt, rem_volt);
}
static DEVICE_ATTR_RO(all_ch1);

static ssize_t all_ch2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct max34427_platform_data *pdata = max34427_dev_i2c->pdata;
	u64 acc, cnt, val, curr, volt, pwr, rem_curr, rem_volt, rem_pwr;
	int ret;

	ret = max34427_read_block(MAX34427_CMD_ACC_COUNT, &cnt);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read acc_cnt, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_PWR_ACC_2, &acc);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read pwr_ch2, ret=%d\n", ret);

	ret = max34427_read_block(MAX34427_CMD_V_CH2, &val);
	if (ret < 0)
		return snprintf(buf, PAGE_SIZE, "Could not read vol_ch2, ret=%d\n", ret);

	if (!pdata) {
		pr_info("%s: pdata is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "pdata is null\n");
	}

	curr = ((acc / cnt) * pdata->corr_current * 1000) / CURRENT_FULL_SCALE;
	volt = ((val >> 2) * 1000 * VOLT_CORR) / VOLT_FULL_SCALE;
	pwr = curr * volt;
	rem_curr = do_div(curr, 1000);
	rem_volt = do_div(volt, 1000);
	rem_pwr = do_div(pwr, 1000000) / 1000;

	pr_info("%s: acc=%d, cnt=%d, corr=%d, curr=%d.%03dA, pwr=%d.%03dW, volt=%d.%03dV\n", __func__,
			acc, cnt, pdata->corr_current, curr, rem_curr, pwr, rem_pwr, volt, rem_volt);

	return snprintf(buf, PAGE_SIZE, "%d.%03dA, %d.%03dW, %d.%03dV\n",
					curr, rem_curr, pwr, rem_pwr, volt, rem_volt);
}
static DEVICE_ATTR_RO(all_ch2);

static int max34427_dt_parse_pdata(struct device *dev,
					struct max34427_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 val;
	int ret;

	if (!np) {
		dev_err(dev, "Could not find max34427 dt node!\n");
		return -ENODEV;
	}

	pdata->pin_reset = of_get_named_gpio(np, "pin_reset", 0);
	pr_info("%s: pin_reset = %d\n", __func__, pdata->pin_reset);
	if (pdata->pin_reset < 0)
		pr_err("%s error reading pin_reset = %d\n",
			__func__, pdata->pin_reset);

	pdata->corr_current = 1;
	ret = of_property_read_u32(np, "corr_current", &val);
	if (ret)
		return -EINVAL;
	pdata->corr_current = (u64)val;
	pr_info("%s: corr_current = %d\n", __func__, pdata->corr_current);

	pdata->corr_power = 1;
	ret = of_property_read_u32(np, "corr_power", &val);
	if (ret)
		return -EINVAL;
	pdata->corr_power = (u64)val;
	pr_info("%s: corr_power = %d\n", __func__, pdata->corr_power);

	return 0;
}

static struct attribute *max34427_attributes[] = {
	&dev_attr_dev_id.attr,
	&dev_attr_update.attr,
	&dev_attr_cnt.attr,
	&dev_attr_control.attr,
	&dev_attr_acc_ch1.attr,
	&dev_attr_acc_ch2.attr,
	&dev_attr_vol_ch1.attr,
	&dev_attr_vol_ch2.attr,
	&dev_attr_pwr_ch1.attr,
	&dev_attr_pwr_ch2.attr,
	&dev_attr_all_ch1.attr,
	&dev_attr_all_ch2.attr,
	NULL
};

static const struct attribute_group max34427_attr_group = {
	.attrs = max34427_attributes,
};

static int max34427_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct max34427_dev *max34427 = NULL;
	struct max34427_platform_data *pdata = i2c->dev.platform_data;
	int ret = 0;

	max34427 = devm_kzalloc(&i2c->dev,
				sizeof(struct max34427_dev), GFP_KERNEL);
	if (!max34427)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
			sizeof(struct max34427_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			devm_kfree(&i2c->dev, max34427);
			return -ENOMEM;
		}

		ret = max34427_dt_parse_pdata(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err_pdata;
		}

		i2c->dev.platform_data = pdata;
	} else {
		pdata = i2c->dev.platform_data;
	}

	max34427->dev = &i2c->dev;
	max34427->i2c = i2c;
	max34427->pdata = pdata;
	max34427_dev_i2c = max34427;

	mutex_init(&max34427->i2c_lock);
	i2c_set_clientdata(i2c, max34427);

#ifdef CONFIG_DRV_SAMSUNG
	max34427_dev = sec_device_create(NULL, "max34427");
#else
	max34427_sec_class = class_create(THIS_MODULE, "max34427_sec");
	if (IS_ERR(max34427_sec_class)) {
		pr_err("%s: Failed to create class(sec) %ld\n", __func__, PTR_ERR(max34427_sec_class));
		ret = PTR_ERR(max34427_sec_class);
		goto err_pdata
	}
	max34427_dev = device_create(max34427_sec_class, NULL, 0, NULL, "max34427");
#endif

	ret = sysfs_create_group(&max34427_dev->kobj, &max34427_attr_group);
	if (ret) {
		dev_err(max34427->dev, "failed to create sysfs group\n");
		goto err_create_sysfs;
	}

	max34427->vdd = regulator_get(&i2c->dev, "vdd");
	if (IS_ERR(max34427->vdd)) {
		ret = PTR_ERR(max34427->vdd);
		pr_info("%s: Can't get vdd regulator, %d\n", __func__, ret);
		goto err_create_sysfs;
	} else {
		ret = regulator_enable(max34427->vdd);
		if (ret < 0) {
			pr_info("%s: Can't turn on vdd regulator, %d\n", __func__, ret);
			goto err_create_sysfs;
		}
	}

	msleep(1000);

	max34427->vio = regulator_get(&i2c->dev, "vio");
	if (IS_ERR(max34427->vio)) {
		ret = PTR_ERR(max34427->vio);
		pr_info("%s: Can't get vio regulator, %d\n", __func__, ret);
		goto err_create_sysfs;
	} else {
		ret = regulator_enable(max34427->vio);
		if (ret < 0) {
			pr_info("%s: Can't turn on vio regulator, %d\n", __func__, ret);
			goto err_create_sysfs;
		}
	}

	msleep(100);
	max34427_update();

	pr_info("%s: successfully probed.\n", __func__);

	return ret;

err_create_sysfs:
	sysfs_remove_group(&max34427_dev->kobj, &max34427_attr_group);
err_pdata:
	devm_kfree(&i2c->dev, max34427);
	devm_kfree(&i2c->dev, pdata);
	pr_info("[%s:%d] err\n", __func__, __LINE__);

	return ret;
}

#if defined(CONFIG_OF)
static const struct of_device_id max34427_i2c_dt_ids[] = {
	{ .compatible = "maxim,max34427" },
	{ },
};
MODULE_DEVICE_TABLE(of, max34427_i2c_dt_ids);

static const struct i2c_device_id max34427_id[] = {
	{"max34427", 0},
	{},
};
#endif

static void max34427_shutdown(struct i2c_client *i2c)
{

	int err = 0;

	if (max34427_dev_i2c->vdd) {
		if (regulator_is_enabled(max34427_dev_i2c->vdd)) {
			err = regulator_disable(max34427_dev_i2c->vdd);
			if (err < 0)
				pr_info("%s: Can't turn off vdd regulator, %d\n", __func__, err);
		} else
			pr_info("%s: vdd regulator already disabled\n", __func__);
	}

	if (max34427_dev_i2c->vio) {
		if (regulator_is_enabled(max34427_dev_i2c->vio)) {
			err = regulator_disable(max34427_dev_i2c->vio);
			if (err < 0)
				pr_info("%s: Can't turn off vio regulator, %d\n", __func__, err);
		} else
			pr_info("%s: vio regulator already disabled\n", __func__);
	}

}

static struct i2c_driver max34427_i2c_driver = {
	.driver = {
		.name = "max34427",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= max34427_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe = max34427_i2c_probe,
	.shutdown = max34427_shutdown,
	.id_table = max34427_id,
};

static int __init max34427_i2c_init(void)
{
	pr_info("%s:%s\n", "max34427", __func__);
	return i2c_add_driver(&max34427_i2c_driver);
}
subsys_initcall(max34427_i2c_init);

static void __exit max34427_i2c_exit(void)
{
	i2c_del_driver(&max34427_i2c_driver);
}
module_exit(max34427_i2c_exit);

MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com>");
MODULE_DESCRIPTION("MAX34427 Power Accumulator");
MODULE_LICENSE("GPL");

