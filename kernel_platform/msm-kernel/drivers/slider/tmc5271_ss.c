// SPDX-License-Identifier: GPL-2.0-or-later
/* TMC5271 Stepper Driver and Controller
 * Copyright (C) 2023 Analog Devices, Inc.
 */

#include "max77831.h"

static struct class *slide_class;

int tmc5271_reg_read(struct spi_device *spi,
			    u8 reg_addr,
			    u32 *reg_data)
{
	u8 tx[5];
	u8 rx[5];
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = 5,
	};
	int ret;

	tx[0] = 0x00 | (reg_addr & TMC5271_ADDRESS_MASK);
	tx[1] = 0x00;
	tx[2] = 0x00;
	tx[3] = 0x00;
	tx[4] = 0x00;

	ret = spi_sync_transfer(spi, &t, 1);

	tx[0] = 0x00 | (reg_addr & TMC5271_ADDRESS_MASK);
	tx[1] = 0x00;
	tx[2] = 0x00;
	tx[3] = 0x00;
	tx[4] = 0x00;

	ret |= spi_sync_transfer(spi, &t, 1);

	*reg_data = (rx[1] << 24) | (rx[2] << 16) |
		(rx[3] << 8) | (rx[4] << 0);

	return ret;
}
EXPORT_SYMBOL(tmc5271_reg_read);

int tmc5271_reg_write(struct spi_device *spi,
			     u8 reg_addr,
			     u32 reg_data)
{
	u8 tx[5];
	u8 rx[5];
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = 5,
	};
	int ret;

	tx[0] = TMC5271_WRITE_BIT | (reg_addr & TMC5271_ADDRESS_MASK);
	tx[1] = (reg_data & 0xff000000) >> 24;
	tx[2] = (reg_data & 0x00ff0000) >> 16;
	tx[3] = (reg_data & 0x0000ff00) >> 8;
	tx[4] = (reg_data & 0x000000ff) >> 0;

	ret = spi_sync_transfer(spi, &t, 1);

	return ret;
}
EXPORT_SYMBOL(tmc5271_reg_write);

int tmc5271_enable_quiescent_mode(struct spi_device *spi)
{
	struct tmc5271_priv *pdata = spi_get_drvdata(spi);
	int ret = 0;
	motor_info("%s enable Quiscent mode +++\n", __func__);
	if(pdata->shaft)
		ret |= tmc5271_reg_write(pdata->spi, TMC5271_REG_GCONF, 0x6000C003);//Enable Driver
	else
		ret |= tmc5271_reg_write(pdata->spi, TMC5271_REG_GCONF, 0x6000C007);//Enable Driver
	ret |= tmc5271_reg_write(pdata->spi, TMC5271_REG_X_ENC, 0x00000000);
	ret |= tmc5271_reg_write(pdata->spi, TMC5271_REG_ENCMODE, 0x00108000);//Enable Driver
	ret |= tmc5271_reg_write(pdata->spi, TMC5271_REG_PWMCONF, 0xC41C001D);//XTARGET=0
	motor_info("%s enable Quiscent mode ---\n", __func__);
	return ret;
}
EXPORT_SYMBOL(tmc5271_enable_quiescent_mode);

int tmc5271_disable_quiescent_mode(struct spi_device *spi)
{
	struct tmc5271_priv *data = spi_get_drvdata(spi);
	int ret = 0;
	motor_info("%s disable Quiscent mode +++\n", __func__);
	if(data->shaft)
		ret |= tmc5271_reg_write(data->spi, TMC5271_REG_GCONF, 0x60000003);//Enable Driver
	else
		ret |= tmc5271_reg_write(data->spi, TMC5271_REG_GCONF, 0x60000007);//Enable Driver
	ret |= tmc5271_reg_write(data->spi, TMC5271_REG_ENCMODE, 0x00108000);//Enable Driver
	ret |= tmc5271_reg_write(data->spi, TMC5271_REG_PWMCONF, 0xC40C001D);//XTARGET=0 to unroll
	motor_info("%s disable Quiscent mode ---\n", __func__);
	return ret;
}
EXPORT_SYMBOL(tmc5271_disable_quiescent_mode);

static int tmc5271_setup_general(struct spi_device *spi)
{

	struct tmc5271_priv *priv = spi_get_drvdata(spi);
	int ret;
	if(priv->shaft)
		ret = tmc5271_reg_write(priv->spi, TMC5271_REG_GCONF, 0x60000003);//Enable Stealthchop mode
	else
		ret = tmc5271_reg_write(priv->spi, TMC5271_REG_GCONF, 0x60000007);//Enable Stealthchop mode
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_DRV_CONF, 0x0000000F);//FSR=3, FSR_IREF=3
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_RAMPMODE, 0x00000000);//Ramp mode = 0(Position mode)
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_IHOLD_IRUN, 0x04011F00);//IHOLD=0, IRUN=31
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_TCOOLTHRS, 0x0000003D);//TCOOLTHRS 61(DEC)
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_XACTUAL, 0x00000000);//XACTUAL=0
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_VSTART, 0x00000000);//VSTART=0
	if(priv->shaft) {
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_A1, 0x0000E12E);//A1=57,646(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_V1, 0x000431BE);//V1=274,878(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_A2, 0x00008CBD);//A2=36,029(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_V2, 0x0004B7F6);//V2=309,238(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_AMAX, 0x00008CBD);//AMAX=36,029(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_VMAX, 0x00053E2D);//VMAX=343,597(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_DMAX, 0x00008CBD);//DMAX=36,029(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_D2, 0x00008CBD);//D2=36,029(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_D1, 0x00008CBD);//D1=36,029(DEC)
	}
	else {
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_A1, 0x00003A98);//A1=15,000 (DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_V1, 0x00061A80);//V1=400,000(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_A2, 0x00003A98);//A2=15,000 (DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_V2, 0x00061A80);//V2=400,000(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_AMAX, 0x00003A98);//AMAX=15,000 (DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_VMAX, 0x00061A80);//VMAX=400,000(DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_DMAX, 0x00003A98);//DMAX=15,000 (DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_D2, 0x00003A98);//D2=15,000 (DEC)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_D1, 0x00003A98);//D1=15,000 (DEC)
	}
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_VSTOP, 0x0000000A);//VSTOP=10
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_XTARGET, 0x00000000);//XTARGET=0
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_SW_MODE, 0x00000400);// 10 bit stallguard enable = 1
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_RAMP_STAT, 0x0000C0C0);
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_X_ENC, 0x00000000);
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_ENCMODE, 0x00108000);
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_ENC_CONST, 0x01000000);
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_ENC_DEVIATION, 0x0000D055);
//	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_CHOPCONF, 0x10410000);//MRES=0(256usteps)
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_PWMCONF, 0xC41C001D);//PWM_MEAS_SD_ENABLE=1, PWM_DIS_REG_STST=1
	if(priv->shaft)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_SG4_THRS, 0x0000032F);//Stall Gauard THRS : 47(DEC)
	else
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_SG4_THRS, 0x00000327);//Stall Gauard THRS : 39(DEC)

	motor_info("%s: reg settings update ret= %d \n", __func__, ret);
	return ret;

}

int tmc5271_goto_target(struct spi_device *spi, int value)
{

	struct tmc5271_priv *priv = spi_get_drvdata(spi);
	int ret = 0;

	motor_info("%s: uStep=%d(0x%x)\n", __func__, value, value);
	priv->status = MOVING;
	if(priv->shaft)
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_GCONF, 0x20000003);//Enable Driver
	else
		ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_GCONF, 0x20000007);//Enable Driver
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_CHOPCONF, 0x10410153);//Enable Driver
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_RAMPMODE, 0x00000003);//Ramp mode = 3(Hold mode)
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_XACTUAL, 0x00000000);//XACTUAL=0
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_RAMP_STAT, 0x0000C0C0);
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_XTARGET, value);//XTARGET=as per hw team to roll
	ret |= tmc5271_reg_write(priv->spi, TMC5271_REG_RAMPMODE, 0x00000000);//Ramp mode = 0(Position mode)
	return ret;
}
EXPORT_SYMBOL(tmc5271_goto_target);

int tmc5271_get_location(struct spi_device *spi)
{
	struct tmc5271_priv *priv = spi_get_drvdata(spi);

	int ret;
	u32 read_done;
	u32 *read_data = &read_done;

	motor_info("%s +++\n", __func__);

	ret = tmc5271_reg_read(priv->spi, TMC5271_REG_XACTUAL, read_data);
	motor_info("XACTUAL read as : 0x%x, uStep=%d\n", read_done, read_done);

	return read_done;
}
EXPORT_SYMBOL(tmc5271_get_location);

void tmc5271_get_dump(struct tmc5271_priv *data)
{
	int val;
	motor_info("%s +++\n", __func__);

	val = gpio_get_value(data->gpio_sleep_n);
	motor_info("gpio_sleep_n  : %s\n", val ? "HIGH" : "LOW");
}
EXPORT_SYMBOL(tmc5271_get_dump);

void tmc5271_get_regdump(struct tmc5271_priv *data)
{
	int ret;
	u32 read_done;
	u32 *read_data = &read_done;
	motor_info("%s +++\n", __func__);

	ret = tmc5271_reg_read(data->spi, TMC5271_REG_GCONF, read_data);
	motor_info("GCONF read as : 0x%x, ret = %d\n", read_done, ret);

	ret = tmc5271_reg_read(data->spi, TMC5271_REG_RAMP_STAT, read_data);
	motor_info("RAMP_STAT read as : 0x%x, ret = %d\n", read_done, ret);

	ret = tmc5271_reg_read(data->spi, TMC5271_REG_PWMCONF, read_data);
	motor_info("PWMCONF read as : 0x%x, ret = %d\n", read_done, ret);

	ret = tmc5271_reg_read(data->spi, TMC5271_REG_XTARGET, read_data);
	motor_info("XTARGET read as : 0x%x, ret = %d\n", read_done, ret);

	ret = tmc5271_reg_read(data->spi, TMC5271_REG_AMAX, read_data);
	motor_info("AMAX read as 0x%x, ret = %d\n", read_done, ret);
}
EXPORT_SYMBOL(tmc5271_get_regdump);

static void tmc5271_sleep_n_enable(struct tmc5271_priv *data, int enable)
{
	if (enable == ON) {
		gpio_set_value(data->gpio_sleep_n, HIGH);
	} else {
		gpio_set_value(data->gpio_sleep_n, LOW);
	}
}

static void tmc5271_init_sequence(struct tmc5271_priv *data)
{
	int ret;

	tmc5271_sleep_n_enable(data, ON);	// take out of sleep
	msleep(10);
	ret = tmc5271_setup_general(data->spi);
	if(ret <0)
		motor_err("%s spi setup returned %d +++\n", __func__, ret);
	ret = tmc5271_enable_quiescent_mode(data->spi);
	if(ret <0)
		motor_err("%s spi setup returned %d +++\n", __func__, ret);

	tmc5271_get_regdump(data);

	motor_info("%s ON +++\n", __func__);
}

/*
 * reg_show - sysfs read interface to show all register value
 */
static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	u32 read_done;
	u32 *read_data = &read_done;
	unsigned int val = 0, i=0;
	unsigned int size = sizeof(reg_map)/sizeof(reg_map[0]);
	unsigned int len = sprintf(buf, "Register     Value\n");

	motor_info("%s +++\n", __func__);

	for (i = 0; i < size; i++) {
		val = tmc5271_reg_read(data->spi, reg_map[i], read_data);
		len += sprintf(buf + len, "  0x%02x      0x%08x\n", reg_map[i], read_done);
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
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	u8 reg = 0;
	u32 regval = 0;
	int ret =0;

	motor_info("%s +++\n", __func__);
	if (sscanf(buf, "%x %x", &reg, &regval) != 2)
		return -EINVAL;
	if (reg > 256)
		return -EINVAL;
	ret = tmc5271_reg_write(data->spi, reg, regval);
	if (ret < 0)
		motor_err("Reg 0x%02x write fail (%d)\n", reg, ret);
	return size;
}
static DEVICE_ATTR(reg, 0664, reg_show, reg_store);

static ssize_t x_target_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	int value, ret;

	motor_info("%s +++\n", __func__);

	ret = sscanf(buf, "%d", &value);
	if (ret == 0)
		return -EINVAL;

	tmc5271_disable_quiescent_mode(data->spi);

	motor_info("%s value dec=%d(uStep)\n", __func__, value);

	if (value == 0) {
		if(data->shaft)
			value = -1200000;
		else
			value = -400000;
		motor_info("%s go to home value\n", __func__);
	}

	ret = tmc5271_goto_target(data->spi, value);

	motor_info("%s ---\n", __func__);
	return size;
}
static DEVICE_ATTR_WO(x_target);

static ssize_t x_target_mm_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	int value, ret;
	u32 x_actual;
	u32 *read_data = &x_actual;

	motor_info("%s +++\n", __func__);

	ret = sscanf(buf, "%d", &value);
	if (ret == 0)
		return -EINVAL;

	tmc5271_disable_quiescent_mode(data->spi);

	motor_info("%s value=%d(mm)\n", __func__, value);

	if (value == 0) {
		if(data->shaft)
			value = -1200000;
		else
			value = -400000;
		motor_info("%s go to home value\n", __func__);
	}
	else {
		value = value * data->step_adj;
		motor_info("%s value=%d(uStep)\n", __func__, value);
		if(data->reverse_accel) {
			ret= tmc5271_reg_read(data->spi, TMC5271_REG_XACTUAL, read_data);
			if((value > 0) && (x_actual < 0))
				value = value + data->reverse_accel;
			else if((value < 0) && (x_actual > 0))
				value = value - data->reverse_accel;
		}
		motor_info("%s: adjusted uStep=%d(uStep)\n", __func__, value);
	}
	ret = tmc5271_goto_target(data->spi, value);

	motor_info("%s ---\n", __func__);
	return size;
}
static DEVICE_ATTR_WO(x_target_mm);

static ssize_t x_actual_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	int actual;
	unsigned int len;

	actual = tmc5271_get_location(data->spi);

	len = sprintf(buf, "XACTUAL=%d\n", actual);

	return len;
}
static DEVICE_ATTR_RO(x_actual);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	unsigned int len;

	if(data->status == MOVING)
		len = sprintf(buf, "MOVING\n");
	else if(data->status == REACHED)
		len = sprintf(buf, "REACHED\n");
	else if(data->status == STALLED)
		len = sprintf(buf, "STALLED\n");
	else
		len = sprintf(buf, "INVALID\n");

	return len;
}
static DEVICE_ATTR_RO(status);

static ssize_t x_enc_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	unsigned int len = 0;
	u32 read_done;
	u32 *read_data = &read_done;

	motor_info("%s +++\n", __func__);
	tmc5271_reg_read(data->spi, TMC5271_REG_X_ENC, read_data);
	len = sprintf(buf, "%d\n", read_done);
	motor_info("%s ---\n", __func__);
	return len;
}

static ssize_t x_enc_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	int value, ret;

	motor_info("%s +++\n", __func__);

	ret = sscanf(buf, "%d", &value);
	if (ret == 0)
		return -EINVAL;

	if(value == 0) { // clear
		motor_info("%s X_ENC clear.\n", __func__);
		ret = tmc5271_reg_write(data->spi, TMC5271_REG_X_ENC, 0x00000000);
		ret = tmc5271_reg_write(data->spi, TMC5271_REG_ENC_STATUS, 0x00000002);
	}
	return size;
}
static DEVICE_ATTR(x_enc, 0664, x_enc_show, x_enc_store);

static ssize_t spi_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmc5271_priv *data = dev_get_drvdata(dev);
	unsigned int len = 0;
	u32 read_done;
	u32 *read_data = &read_done;
	u8 version, silicon_rev;
	int i, ret;

	motor_info("%s +++\n", __func__);
	for (i = 0; i < 5; i++) {
		ret = tmc5271_reg_read(data->spi, TMC5271_REG_IOIN, read_data);
		motor_info("%s: IOIN: 0x%x(count:%d)\n", __func__, read_done, i+1);
		if (ret) {
			motor_info( "%s: Failed to read IOIN\n", __func__);
			len = sprintf(buf, "FAIL\n");
			motor_info("%s ---\n", __func__);
			return len;
		}
		else {
			version = (*read_data & TMC5271_VERSION_MASK) >> TMC5271_VERSION_SHIFT;
			silicon_rev = (*read_data & TMC5271_SILICON_RV_MASK) >> TMC5271_SILICON_RV_SHIFT;
			motor_info("%s: VERSION:0x%x, SILICON_REV:0x%x\n", __func__, version, silicon_rev);

			if ((version != TMC5271_VERSION) || (silicon_rev != TMC5271_SILICON_REV)) {
				len = sprintf(buf, "FAIL\n");
				motor_info("%s ---\n", __func__);
				return len;
			}
		}
		msleep(10);
	}
	len = sprintf(buf, "PASS\n");
	motor_info("%s ---\n", __func__);
	return len;

}
static DEVICE_ATTR_RO(spi_test);

static struct attribute *tmc5271_dev_attrs[] = {
	&dev_attr_reg.attr,
	&dev_attr_x_target.attr,
	&dev_attr_x_actual.attr,
	&dev_attr_status.attr,
	&dev_attr_x_enc.attr,
	&dev_attr_spi_test.attr,
	&dev_attr_x_target_mm.attr,
	NULL,
};

struct attribute_group tmc5271_dev_attr_group = {
	.attrs = tmc5271_dev_attrs,
};

static int tmc5271_gpio_init(struct tmc5271_priv *data)
{
	motor_info("%s +++\n", __func__);

	gpio_request(data->gpio_spi_cs, "sliding_motor_spi_cs");
	gpio_direction_output(data->gpio_spi_cs, 0);
	gpio_set_value(data->gpio_spi_cs, 1);

	gpio_request(data->gpio_sleep_n, "sliding_motor_sleep_n");
	gpio_direction_output(data->gpio_sleep_n, 1);
	gpio_set_value(data->gpio_sleep_n, 1);

	gpio_request(data->mt_drive_diag0, "sliding_motor_drive_diag");
	gpio_direction_input(data->mt_drive_diag0);
	if (gpio_is_valid(data->mt_drive_diag0)) {
		data->mt_diag0_irq = gpio_to_irq(data->mt_drive_diag0);
		if (data->mt_diag0_irq < 0) {
			motor_err("%s irq failed: gpio_int(%d) irq(%d)\n", __func__,
				data->mt_diag0_irq, data->mt_drive_diag0);
		} else {
			motor_info("%s irq ok!!: gpio_int(%d) irq(%d)\n", __func__,
				data->mt_diag0_irq, data->mt_drive_diag0);
		}
	}

	motor_info("%s ---\n", __func__);
	return 0;
}

static int tmc5271_parse_dt(struct tmc5271_priv *data)
{
	struct device_node *np = data->dev->of_node;

	int ret = 0;

	motor_info("%s +++\n", __func__);
	data->mt_drive_diag0 = of_get_named_gpio(np, "diag0-irq-gpio", 0);
	if (!gpio_is_valid(data->mt_drive_diag0))
		motor_err("%s: mot_drive_diag gpio is not specified\n", __func__);

	data->mt_drive_diag1 = of_get_named_gpio(np, "diag1-irq-gpio", 0);
	if (!gpio_is_valid(data->mt_drive_diag1))
		motor_err("%s: mot_drive_diag gpio is not specified\n", __func__);

	data->gpio_spi_cs = of_get_named_gpio(np, "cs-gpio", 0);
	if (!gpio_is_valid(data->gpio_spi_cs))
		motor_err("%s: gpio_spi_cs gpio is not specified\n", __func__);

	data->gpio_sleep_n = of_get_named_gpio(np, "sleep-n-gpio", 0);
	if (!gpio_is_valid(data->gpio_sleep_n))
		motor_err("%s: gpio_sleep_n gpio is not specified\n", __func__);

	ret = of_property_read_u32(np, "shaft", &data->shaft);
	if (ret) {
		pr_info("%s: not able to read shaft. This device is before hw_rev 14.\n", __func__);
	}

	ret = of_property_read_u32(np, "re_accel", &data->reverse_accel);
	if (ret) {
		pr_info("%s: not able to read reverse_acceleration. This device is before hw_rev 14.\n", __func__);
	}

	ret = of_property_read_u32(np, "adjust", &data->step_adj);
	if (ret) {
		pr_info("%s: not able to read adjust.\n", __func__);
	}

	motor_info("%s ---\n", __func__);
	return 0;
}

static irqreturn_t tmc5271_qsc_enable_work(int irq, void *data)
{
	struct tmc5271_priv * pdata = (struct tmc5271_priv *)data;

	int ret = 0;
	u32 read_done;
	u32 *read_data = &read_done;
	char *reached[2] = { "MOTOR=REACHED", NULL };
	char *stallguard[2] = { "MOTOR=STALLGUARD", NULL };
	motor_info("%s +++\n", __func__);

	if (gpio_get_value(pdata->mt_drive_diag0) == 1) {
		motor_err("%s mt_drive_diag irq get error : irq(%d), info(%x)\n", __func__, irq, pdata);
		return IRQ_HANDLED;
	}
	motor_info("%s mt_drive_diag irq detected!!: irq(%d)\n", __func__, irq);

	ret = tmc5271_reg_read(pdata->spi, TMC5271_REG_RAMP_STAT, read_data);
	motor_info(" Before qsc en, RAMP STAT read as : 0x%x, ret = %d\n", read_done, ret);

	if( (read_done & 0x80) != 0){
		motor_info("The target position is reached.\n");
		kobject_uevent_env(&pdata->dev->kobj, KOBJ_CHANGE, reached);
		pdata->status = REACHED;
		tmc5271_enable_quiescent_mode(pdata->spi);
	} else if( (read_done & 0x40) != 0){
		motor_info("An active StallGuard stop event.\n");
		kobject_uevent_env(&pdata->dev->kobj, KOBJ_CHANGE, stallguard);
		pdata->status = STALLED;
		tmc5271_enable_quiescent_mode(pdata->spi);
	}

	return IRQ_HANDLED;
}

static int tmc5271_probe(struct spi_device *spi)
{
	struct tmc5271_priv *priv;
	int ret;
	motor_info("[MOTOR] %s +++\n", __func__);
	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv){
		motor_err("%s : Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	priv->dev = &spi->dev;
	priv->spi = spi;
	priv->enable = true;
	priv->velocity = 50000;
	priv->acceleration = 100;
	priv->status = REACHED;
	ret = tmc5271_parse_dt(priv);
	if (ret)
		return ret;
	ret = tmc5271_gpio_init(priv);
	if (ret)
		return ret;
	spi_set_drvdata(spi, priv);
	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0) {
		motor_err("%s: fail to set up SPI protocol driver\n", __func__);
		return ret;
	}

	priv->dev = device_create(slide_class, NULL, MKDEV(0, 0), priv, "tmc5271");
	if (priv->dev == NULL) {
		motor_err("%s: Failed to create device for motor\n", __func__);
		return -ENOMEM;
	}
	ret = sysfs_create_group(&priv->dev->kobj, &tmc5271_dev_attr_group);
	if (ret < 0)
		motor_err("[MOTOR]: Failed to register sysfs reg: %d\n", ret);

	tmc5271_init_sequence(priv);

	ret = request_threaded_irq(priv->mt_diag0_irq, NULL, tmc5271_qsc_enable_work,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , "tmc5271_qsc_enable_irq", priv);
	if (ret)
		motor_err("%s request irq failed: gpio_int(%d) irq(%d)\n", __func__, priv->mt_diag0_irq, priv->mt_drive_diag0);

	motor_info("[MOTOR] %s ---\n", __func__);
	return 0;
}

static const struct spi_device_id tmc5271_id[] = {
	{ TMC5271_NAME, 1 },
//	{ TMC5271_NAME, 1 },
	{ },
};

static const struct of_device_id tmc5271_of_match[] = {
	{
		.compatible = "adi,tmc5271",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, tmc5271_of_match);

static struct spi_driver tmc5271_driver = {
	.id_table = tmc5271_id,
	.driver = {
		.name = TMC5271_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = tmc5271_of_match,
	},
	.probe = tmc5271_probe,
};

static int __init tmc5271_init(void)
{
	int status = 0;
	motor_info("[MOTOR] %s +++\n", __func__);

	slide_class = class_create(THIS_MODULE, "right_motor");
	if (IS_ERR(slide_class)) {
		motor_err("error in creating sliding_motor class %d\n", PTR_ERR(slide_class));
		return -EIO;
	}

	status = spi_register_driver(&tmc5271_driver);

	if (status < 0) {
		motor_err("[MOTOR] %s Failed to register SPI driver.\n", __func__);
		return -EINVAL;
	}
	motor_info("[MOTOR] %s ---\n", __func__);
	return status;

}
module_init(tmc5271_init);

static void __exit tmc5271_exit(void)
{
	motor_info("[MOTOR] %s +++\n", __func__);
	spi_unregister_driver(&tmc5271_driver);
}
module_exit(tmc5271_exit);

MODULE_AUTHOR("Dragos Bogdan <dragos.bogdan@analog.com>");
MODULE_DESCRIPTION("TMC5271 Driver");
MODULE_LICENSE("GPL v2");
