// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Booster IC MAX77831
 */

#include "max77831.h"

static struct class *booster_class;

int max77831_i2c_write(struct i2c_client *client, u8 addr, u8 val)
{
	int ret = 0;
	u8 command = addr, value = val;
	struct tmc5271_priv *priv = NULL;

	if (!client)
		return ret;

	priv = i2c_get_clientdata(client);
	if (!priv)
		return ret;

	ret = i2c_smbus_write_byte_data(client, command, value);
	if (ret < 0)
		motor_err("%s: fail. %2x, %2x, %d\n", __func__, command, value, ret);

	motor_info("%s: addr = %2x, value = %2x\n", __func__, command, value);

	return ret;
}
EXPORT_SYMBOL(max77831_i2c_write);

int max77831_i2c_read(struct i2c_client *client, u8 addr)
{
	int ret = 0;
	u8 value = 0;
	struct tmc5271_priv *priv = NULL;

	if (!client)
		return ret;

	priv = i2c_get_clientdata(client);
	if (!priv)
		return ret;

	value = i2c_smbus_read_byte_data(client, addr);
	if (value < 0)
		motor_err("%s: fail. %2x, %2x\n", __func__, addr, value);

	motor_info("%s: addr = %2x read value = %2x\n", __func__, addr, value);

	return value;
}

/*
 * reg_show - sysfs read interface to show all register value
 */
static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);

	unsigned int val = 0, i=0;
	unsigned int len = sprintf(buf, "Register     Value\n");

	motor_info("%s +++\n", __func__);

	for (i = 0x10; i < 0x16; i++) {
		val = max77831_i2c_read(data->booster_client, i);
		len += sprintf(buf + len, "  0x%02x      0x%02x\n", i, val);
	}

	return len;
}

/*
 * reg_store - sysfs write interface to update given register by
 *      desired value.
 */
static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);

	u32 reg = 0;
	u32 regval = 0;
	int ret =0;

	motor_info("%s +++\n", __func__);
	if (sscanf(buf, "%x %x", &reg, &regval) != 2)
		return -EINVAL;
	if (reg > 255 || regval > 255)
		return -EINVAL;
	ret = max77831_i2c_write(data->booster_client, reg, regval);
	if (ret < 0)
		motor_err("Reg 0x%02x write fail (%d)\n", reg, ret);
	return size;
}
static DEVICE_ATTR(reg, 0664, reg_show, reg_store);


static ssize_t gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);

	unsigned int val = 0,len = 0;

	motor_info("%s +++\n", __func__);
	val = gpio_get_value(data->step_boost_en);
	motor_info("data->step_boost_en=%d, val=%d\n",data->step_boost_en, val);
	len = sprintf(buf, "boost_en:%d\n", val);

	return len;
}

static ssize_t gpio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);

	int value = 0;
	int ret =0;

	motor_info("%s +++\n", __func__);

	ret = sscanf(buf, "%d", &value);
	if (ret == 0 || value > 2 || value < 0)
		return -EINVAL;

	if (value == 1) {
		gpio_set_value(data->step_boost_en, HIGH);
	} else if (value == 0){
		gpio_set_value(data->step_boost_en, LOW);
	}

	return size;
}
static DEVICE_ATTR(gpio, 0664, gpio_show, gpio_store);


static ssize_t booster_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);

	u8 read_done;

	unsigned int val = 0,len = 0;

	motor_info("%s +++\n", __func__);
	read_done = max77831_i2c_read(data->booster_client, VOLTAGE_CHG);
	val = (read_done*10)/BOOST_V_DIV_FACTOR;
	len = sprintf(buf, " Boost IC voltage, regval: %d in Volts: %d.%d V \n", read_done, val/10 , val%10 );

	return len;
}

static ssize_t booster_voltage_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct max77831_drvdata *data = dev_get_drvdata(dev);
	u32 regval = 0;
	int ret =0;

	ret = sscanf(buf, "%d", &regval);
	if (ret == 0 || regval > 255)
		return -EINVAL;

	ret = max77831_i2c_write(data->booster_client, VOLTAGE_CHG, regval);
	if (ret < 0)
		motor_err(" Reg 0x%02x write fail (%d)\n", VOLTAGE_CHG, ret);

	return size;
}
static DEVICE_ATTR(booster_voltage, 0664, booster_voltage_show, booster_voltage_store);

static struct attribute *max77831_dev_attrs[] = {
	&dev_attr_reg.attr,
	&dev_attr_booster_voltage.attr,
	&dev_attr_gpio.attr,
//	&dev_attr_motor_enable.attr,
	NULL,
};

struct attribute_group max77831_dev_attr_group = {
	.attrs = max77831_dev_attrs,
};


static void max77831_set_booster_voltage(struct i2c_client *client)
{
	int idx = 3;// idx_mid = 7, idx_high_v = 9;
	u8 read_data = 0;
	max77831_i2c_write(client, VOLTAGE_CHG, max77831_BoosterVolReg[idx].reg_val);
	msleep(5);
	read_data = max77831_i2c_read(client, VOLTAGE_CHG);
	motor_info("%s %dV(value : %02x) operation\n", __func__,
		max77831_BoosterVolReg[idx].voltage, max77831_BoosterVolReg[idx].reg_val);
}

static void max77831_set_boost_power(struct  max77831_drvdata *data)
{
	motor_info("%s +++\n", __func__);
	gpio_set_value(data->step_boost_en, HIGH);
	msleep(5);
	motor_info("%s ---\n", __func__);
}

static int max77831_parse_dt(struct max77831_drvdata *data)
{
	struct device_node *np = data->dev->of_node;

	motor_info("%s +++\n", __func__);

	data->step_boost_en = of_get_named_gpio(np, "boost-en", 0);
	if (!gpio_is_valid(data->step_boost_en))
		motor_err("%s: step_boost_en gpio is not specified\n", __func__);

	data->step_boost_intb = of_get_named_gpio(np, "boost-intb", 0);
	if (!gpio_is_valid(data->step_boost_intb))
		motor_err("%s: step_boost_intb gpio is not specified\n", __func__);

	data->reg_boost_vio = devm_regulator_get(data->dev, "booster_vio");
	if (IS_ERR_OR_NULL(data->reg_boost_vio)) {
		motor_err("%s booster_vio regulator get failed\n", __func__);
	}

	motor_info("%s ---\n", __func__);

	return 0;
}

static int max77831_gpio_init(struct max77831_drvdata *data)
{
	motor_info("%s +++\n", __func__);
	gpio_request(data->step_boost_en, "sliding_motor_boost_en");
	gpio_direction_output(data->step_boost_en, 0);
	gpio_set_value(data->step_boost_en, 1);
	max77831_set_boost_power(data);
	max77831_set_booster_voltage(data->booster_client);
	motor_info("%s ---\n", __func__);
	return 0;
}

static int max77831_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct max77831_drvdata *ddata = NULL;
	int ret = 0;
	motor_info("%s +++\n", __func__);

	ddata = kzalloc(sizeof(struct max77831_drvdata), GFP_KERNEL);
	if (unlikely(!ddata)) {
		motor_info("%s(%d) \n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		motor_info("%s: need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	i2c_set_clientdata(client, ddata);

	ddata->booster_client = client;
	motor_info("%s: %s %s\n", __func__, dev_name(&client->adapter->dev), of_node_full_name(client->dev.of_node));
	ddata->dev = &client->dev;

	ret = max77831_parse_dt(ddata);
	if (ret)
		return ret;

	ret = max77831_gpio_init(ddata);
	if (ret)
		return ret;

	ret = max77831_i2c_write(ddata->booster_client, INT_MASK_REG, 0x1f);

	ret = regulator_enable(ddata->reg_boost_vio);
	if(ret)
		motor_err("%s: fail to enable booster vio\n", __func__);

	ddata->dev = device_create(booster_class, NULL, MKDEV(0, 0), ddata, "max77831");
	if (ddata->dev == NULL) {
		motor_err("%s: Failed to create device for motor\n", __func__);
		return -ENOMEM;
	}
	ret = sysfs_create_group(&ddata->dev->kobj, &max77831_dev_attr_group);
	if (ret < 0)
		motor_err("[MOTOR]: Failed to register sysfs reg: %d\n", ret);

	motor_info("%s ---\n", __func__);
	return ret;
}

static struct i2c_device_id max77831_i2c_id[] = {
	{"max77831", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max77831_i2c_id);

static const struct of_device_id max77831_i2c_dt_ids[] = {
	{ .compatible = "i2c,max77831" },
	{ }
};

MODULE_DEVICE_TABLE(of, max77831_i2c_dt_ids);

static struct i2c_driver max77831_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "max77831",
		.of_match_table	= of_match_ptr(max77831_i2c_dt_ids),
	},
	.id_table = max77831_i2c_id,
	.probe = max77831_probe,
};

static int __init max77831_init(void)
{
	int status = 0;
	motor_info("%s +++\n", __func__);

	booster_class = class_create(THIS_MODULE, "motor_booster");
	if (IS_ERR(booster_class)) {
		motor_err("error in creating motor_booster class %d\n", PTR_ERR(booster_class));
		return -EIO;
	}

	status = i2c_add_driver(&max77831_i2c_driver);

	if (status < 0) {
		motor_err("[MOTOR] %s Failed to register I2C driver.\n", __func__);
		return -EINVAL;
	}
	motor_info("[MOTOR] %s ---\n", __func__);
	return status;

}
module_init(max77831_init);

static void __exit max77831_exit(void)
{
	motor_info("%s +++\n", __func__);
	i2c_del_driver(&max77831_i2c_driver);
}
module_exit(max77831_exit);

MODULE_DESCRIPTION("max77831 Driver");
MODULE_LICENSE("GPL v2");
