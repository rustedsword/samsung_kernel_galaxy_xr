/*
 * SPI controller driver for KXR firmware
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2023  KinecticsXR Incorporated. All rights reserved.
 */

#include "kxr_aphost.h"
#include <linux/regulator/consumer.h>

#define KXR_APHOST_DRV_NAME			"nordic,spicontroller"
#define KXR_DRIVER_VER				"v2.0.9"

static char kxr_aphost_drvier_ver[] = KXR_DRIVER_VER;
module_param_string(driver_ver, kxr_aphost_drvier_ver, sizeof(kxr_aphost_drvier_ver), 0444);

static char kxr_aphost_debug_message[128];
module_param_string(debug_message, kxr_aphost_debug_message, sizeof(kxr_aphost_debug_message), 0444);

void kxr_aphost_println(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(kxr_aphost_debug_message, sizeof(kxr_aphost_debug_message), fmt, ap);
	va_end(ap);
}

void kxr_aphost_set_panel_state(kxr_aphost_t *aphost, bool enabled)
{
	dev_info(&aphost->xfer.spi->dev, "kxr set_panel_state: %d->%d\n", aphost->panel_enabled, enabled);

	aphost->panel_enabled = enabled;
	aphost->panel_updated = true;

	if (enabled) {
		dev_info(&aphost->xfer.spi->dev, "kxr_aphost_set_panel_state : enabled - KXR_SPI_POWER_MODE_ON");
		kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_ON);
	}
}

bool kxr_aphost_get_panel_state(kxr_aphost_t *aphost)
{
	return aphost->panel_enabled;
}

#ifndef CONFIG_KXR_SIMULATION_TEST
static int kxr_aphost_power_mode = KXR_SPI_POWER_MODE_OFF;
module_param_named(power_mode, kxr_aphost_power_mode, int, 0644);

static struct regulator *kxr_aphost_find_regulator(struct device *dev)
{
	struct regulator *regulator;
	const char *name;

	if (of_property_read_string(dev->of_node, "nordic,v1p8en-supply", &name) >= 0) {
		dev_warn(dev, "regulator_name = %s", name);

		regulator = devm_regulator_get_optional(dev, name);
		if (!IS_ERR(regulator)) {
			return regulator;
		}
	}

	regulator = devm_regulator_get_optional(dev, "nordic,v1p8en");
	if (!IS_ERR(regulator)) {
		return regulator;
	}

	regulator = devm_regulator_get_optional(dev, "v1p8en");
	if (!IS_ERR(regulator)) {
		return regulator;
	}

	regulator = devm_regulator_get_optional(dev, "v1p8");
	if (!IS_ERR(regulator)) {
		return regulator;
	}

	return NULL;
}

static int kxr_aphost_init_gpios(kxr_aphost_t *aphost)
{
	struct device *dev = &aphost->xfer.spi->dev;

	aphost->gpio_irq = devm_gpiod_get(dev, "nordic,irq", GPIOD_IN);
	if (IS_ERR(aphost->gpio_irq)) {
		dev_err(dev, "Failed to devm_gpiod_get: nordic,irq\n");
		return PTR_ERR(aphost->gpio_irq);
	}

	aphost->gpio_dfu = devm_gpiod_get_from_of_node(dev, dev->of_node, "nordic,nordic-dfu", 0, GPIOD_OUT_HIGH, "nordic-dfu");
	if (IS_ERR(aphost->gpio_dfu)) {
		dev_warn(dev, "Failed to devm_gpiod_get_from_of_node: nordic,nordic-dfu\n");
		aphost->gpio_dfu = NULL;
	}

	aphost->gpio_vcc = devm_gpiod_get_optional(dev, "nordic,v1p8en", GPIOD_OUT_LOW);
	if (IS_ERR(aphost->gpio_vcc)) {
		dev_warn(dev, "Failed to devm_gpiod_get: nordic,v1p8en\n");
		aphost->gpio_vcc = NULL;
	}

	aphost->gpio_ledl = devm_gpiod_get_optional(dev, "nordic,ledl", GPIOD_OUT_LOW);
	if (IS_ERR(aphost->gpio_ledl)) {
		dev_warn(dev, "Failed to devm_gpiod_get: nordic,ledl\n");
		aphost->gpio_ledl = NULL;
	}

	aphost->gpio_ledr = devm_gpiod_get_optional(dev, "nordic,ledr", GPIOD_OUT_LOW);
	if (IS_ERR(aphost->gpio_ledr)) {
		dev_warn(dev, "Failed to devm_gpiod_get: nordic,ledr\n");
		aphost->gpio_ledr = NULL;
	}

	aphost->pwr_v1p8 = kxr_aphost_find_regulator(dev);
	if (IS_ERR(aphost->pwr_v1p8)) {
		dev_warn(dev, "Failed to kxr_aphost_find_regulator\n");
		aphost->pwr_v1p8 = NULL;
	}

	dev_warn(dev, "pwr_v1p8 = %p\n", aphost->pwr_v1p8);

	return 0;
}

static int kxr_aphost_init_pinctrl(kxr_aphost_t *aphost)
{
	int ret;

	aphost->pinctrl = devm_pinctrl_get(&aphost->xfer.spi->dev);
	if (IS_ERR_OR_NULL(aphost->pinctrl)) {
		dev_err(&aphost->xfer.spi->dev, "Failed to devm_pinctrl_get\n");
		return PTR_ERR(aphost->pinctrl);
	}

	aphost->active = pinctrl_lookup_state(aphost->pinctrl, "nordic_default");
	if (IS_ERR_OR_NULL(aphost->active)) {
		dev_err(&aphost->xfer.spi->dev, "Failed to pinctrl_lookup_state: nordic_default\n");
		return PTR_ERR(aphost->active);
	}

	aphost->suspend = pinctrl_lookup_state(aphost->pinctrl, "nordic_sleep");
	if (IS_ERR_OR_NULL(aphost->suspend)) {
		dev_err(&aphost->xfer.spi->dev, "Failed to pinctrl_lookup_state: nordic_sleep\n");
		return PTR_ERR(aphost->suspend);
	}

	ret = pinctrl_select_state(aphost->pinctrl, aphost->active);
	if (ret < 0) {
		dev_err(&aphost->xfer.spi->dev, "Failed to pinctrl_select_state: %d\n", ret);
	}

	return 0;
}

static void kxr_aphost_set_power_gpio(kxr_aphost_t *aphost, bool value)
{
	if (aphost->gpio_vcc != NULL) {
		gpiod_set_value(aphost->gpio_vcc, value);
	}

	if (aphost->pwr_v1p8 != NULL && aphost->pwr_enabled != value) {
		int ret = regulator_set_voltage(aphost->pwr_v1p8, 1800000, 1800000);
		if(ret < 0) {
			dev_err(&aphost->xfer.spi->dev, "Failed to regulator_set_voltage: %d\n", ret);
		}

		if (value != 0) {
			ret = regulator_enable(aphost->pwr_v1p8);
			if(ret < 0) {
				dev_err(&aphost->xfer.spi->dev, "Failed to regulator_enable: %d\n", ret);
			}
		} else {
			ret = regulator_disable(aphost->pwr_v1p8);
			if(ret < 0) {
				dev_err(&aphost->xfer.spi->dev, "Failed to regulator_disable: %d\n", ret);
			}
		}
	}

	aphost->pwr_enabled = value;
}

static void kxr_aphost_set_dfu_gpio(kxr_aphost_t *aphost, bool value)
{
	if (aphost->gpio_dfu != NULL) {
		gpiod_set_value(aphost->gpio_dfu, value);
	}
}

bool kxr_aphost_power_mode_set(kxr_aphost_t *aphost, kxr_spi_power_mode_t mode)
{
	bool success = true;

	mutex_lock(&aphost->lock);

	switch (mode) {
	case KXR_SPI_POWER_MODE_OFF:
		dev_info(&aphost->xfer.spi->dev, "KXR_SPI_POWER_MODE_OFF\n");
		kxr_aphost_power_mode = KXR_SPI_POWER_MODE_OFF;
		kxr_spi_xfer_sync_disable(&aphost->xfer);
		kxr_spi_xfer_mode_set(&aphost->xfer, KXR_SPI_WORK_MODE_IDLE);
		kxr_aphost_set_power_gpio(aphost, false);
		kxr_aphost_set_dfu_gpio(aphost, true);
		break;

	case KXR_SPI_POWER_MODE_ON:
		dev_info(&aphost->xfer.spi->dev, "KXR_SPI_POWER_MODE_ON\n");
		kxr_aphost_power_mode = KXR_SPI_POWER_MODE_ON;
		kxr_spi_xfer_sync_enable(&aphost->xfer);
		kxr_aphost_set_dfu_gpio(aphost, true);
		kxr_aphost_set_power_gpio(aphost, true);
		kxr_spi_xfer_wakeup(&aphost->xfer);
		break;

	case KXR_SPI_POWER_MODE_DFU:
		dev_info(&aphost->xfer.spi->dev, "KXR_SPI_POWER_MODE_DFU\n");

		if (kxr_aphost_power_mode == KXR_SPI_POWER_MODE_DFU) {
			break;
		}

		kxr_aphost_power_mode = KXR_SPI_POWER_MODE_DFU;
		kxr_spi_xfer_sync_enable(&aphost->xfer);

		if (aphost->gpio_dfu != NULL) {
			kxr_aphost_set_power_gpio(aphost, false);
			kxr_aphost_set_dfu_gpio(aphost, false);
			msleep(100);
		}

		kxr_aphost_set_power_gpio(aphost, true);
		break;

	default:
		success = false;
		break;
	}

	mutex_unlock(&aphost->lock);

	return success;
}

kxr_spi_power_mode_t kxr_aphost_power_mode_get(void)
{
	return kxr_aphost_power_mode;
}

static ssize_t jspower_show(struct device *dev, struct device_attribute *attr, char *buff)
{
	return scnprintf(buff, PAGE_SIZE, "%d\n", kxr_aphost_power_mode);
}

static ssize_t jspower_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t size)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	int mode;

	if (sscanf(buff, "%d", &mode) != 1) {
		return -EINVAL;
	}

	kxr_aphost_power_mode_set(aphost, mode);

	return size;
}

static DEVICE_ATTR_RW(jspower);

static int kxr_aphost_driver_probe(struct spi_device *spi)
{
	kxr_aphost_t *aphost;
	int ret;

	dev_info(&spi->dev, "version: %s\n", KXR_DRIVER_VER);

	ret = kxr_spi_xfer_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_setup: %d\n", ret);
		return ret;
	}

	aphost = devm_kzalloc(&spi->dev, sizeof(kxr_aphost_t), GFP_KERNEL);
	if (aphost == NULL) {
		dev_err(&spi->dev, "Failed to devm_kzalloc\n");
		return -ENOMEM;
	}

	spi_set_drvdata(spi, aphost);
	mutex_init(&aphost->lock);
	aphost->xfer.spi = spi;

	ret = kxr_aphost_init_pinctrl(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_aphost_init_pinctrl: %d\n", ret);
		goto out_mutex_destroy;
	}

	ret = kxr_aphost_init_gpios(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_aphost_init_gpios: %d\n", ret);
		goto out_mutex_destroy;
	}

	ret = kxr_spi_xfer_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xfer_probe: %d\n", ret);
		goto out_mutex_destroy;
	}

	ret = kxr_spi_xchg_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xchg_probe: %d\n", ret);
		goto out_kxr_spi_xfer_remove;
	}

	ret = kxr_spi_uart_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_uart_probe: %d\n", ret);
		goto out_kxr_spi_xchg_remove;
	}

	ret = kxr_spi_dfu_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_dfu_probe: %d\n", ret);
		goto out_kxr_spi_uart_remove;
	}

	ret = kxr_spi_xfer_start(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xfer_start: %d\n", ret);
		goto out_kxr_spi_dfu_remove;
	}

	kxr_panel_notifier_probe(aphost);
	device_create_file(&spi->dev, &dev_attr_jspower);
	kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_ON);

	return 0;

out_kxr_spi_dfu_remove:
	kxr_spi_dfu_remove(aphost);
out_kxr_spi_uart_remove:
	kxr_spi_uart_remove(aphost);
out_kxr_spi_xchg_remove:
	kxr_spi_xchg_remove(aphost);
out_kxr_spi_xfer_remove:
	kxr_spi_xfer_remove(aphost);
out_mutex_destroy:
	mutex_destroy(&aphost->lock);
	return ret;
}

static int kxr_aphost_driver_remove(struct spi_device *spi)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) spi_get_drvdata(spi);

	device_remove_file(&spi->dev, &dev_attr_jspower);

	kxr_panel_notifier_remove(aphost);
	kxr_spi_xfer_remove(aphost);
	kxr_spi_dfu_remove(aphost);
	kxr_spi_uart_remove(aphost);
	kxr_spi_xchg_remove(aphost);

	mutex_destroy(&aphost->lock);

	return 0;
}

static int kxr_aphost_suspend(struct device *dev)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) kxr_aphost_get_drv_data(dev);
	dev_info(&aphost->xfer.spi->dev, "kxr_aphost_suspend : KXR_SPI_POWER_MODE_OFF");
	kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_OFF);
	return 0;
}

static int kxr_aphost_resume(struct device *dev)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) kxr_aphost_get_drv_data(dev);

	if (aphost->panel_updated == false) {
		dev_info(&aphost->xfer.spi->dev, "kxr_aphost_resume : Not panel_updated / Forbid KXR_SPI_POWER_MODE_ON ");
		//kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_ON);
	}

	return 0;
}

static const struct of_device_id kxr_aphost_dt_match[] = {
	{ .compatible = KXR_APHOST_DRV_NAME }, {}
};

static const struct dev_pm_ops kxr_aphost_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(kxr_aphost_suspend, kxr_aphost_resume)
};

static struct spi_driver kxr_aphost_driver = {
	.driver = {
		.name = KXR_APHOST_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = kxr_aphost_dt_match,
		.pm = &kxr_aphost_pm_ops,
	},
	.probe = kxr_aphost_driver_probe,
	.remove = kxr_aphost_driver_remove,
};

static int __init kxr_aphost_init(void)
{
	return spi_register_driver(&kxr_aphost_driver);
}

static void __exit kxr_aphost_exit(void)
{
	spi_unregister_driver(&kxr_aphost_driver);
}
#else
bool kxr_aphost_power_mode_set(kxr_aphost_t *aphost, kxr_spi_power_mode_t mode)
{
	return true;
}

kxr_spi_power_mode_t kxr_aphost_power_mode_get(void)
{
	return KXR_SPI_POWER_MODE_ON;
}

static int kxr_aphost_platform_driver_probe(struct platform_device *spi)
{
	kxr_aphost_t *aphost;
	int ret;

#ifndef CONFIG_KXR_SIMULATION_TEST
	ret = kxr_spi_xfer_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_setup: %d\n", ret);
		return ret;
	}
#endif

	aphost = devm_kzalloc(&spi->dev, sizeof(kxr_aphost_t), GFP_KERNEL);
	if (aphost == NULL) {
		dev_err(&spi->dev, "Failed to devm_kzalloc\n");
		return -ENOMEM;
	}

#ifndef CONFIG_KXR_SIMULATION_TEST
	spi_set_drvdata(spi, aphost);
	aphost->xfer.spi = spi;
#else
	platform_set_drvdata(spi, aphost);
	aphost->xfer.spi = spi;
#endif

	ret = kxr_spi_xfer_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xfer_probe: %d\n", ret);
		return ret;
	}

	ret = kxr_spi_xchg_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xchg_probe: %d\n", ret);
		goto out_kxr_spi_xfer_remove;
	}

	ret = kxr_spi_uart_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_uart_probe: %d\n", ret);
		goto out_kxr_spi_xchg_remove;
	}

	ret = kxr_spi_dfu_probe(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_dfu_probe: %d\n", ret);
		goto out_kxr_spi_uart_remove;
	}

	ret = kxr_spi_xfer_start(aphost);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to kxr_spi_xfer_start: %d\n", ret);
		goto out_kxr_spi_dfu_remove;
	}

	kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_ON);

	return 0;

out_kxr_spi_dfu_remove:
	kxr_spi_dfu_remove(aphost);
out_kxr_spi_uart_remove:
	kxr_spi_uart_remove(aphost);
out_kxr_spi_xchg_remove:
	kxr_spi_xchg_remove(aphost);
out_kxr_spi_xfer_remove:
	kxr_spi_xfer_remove(aphost);
	return ret;
}

static int kxr_aphost_platform_driver_remove(struct platform_device *pdev)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) platform_get_drvdata(pdev);

	kxr_spi_dfu_remove(aphost);
	kxr_spi_uart_remove(aphost);
	kxr_spi_xchg_remove(aphost);
	kxr_spi_xfer_remove(aphost);

	return 0;
}

static void kxr_aphost_platform_device_release(struct device *dev)
{
	pr_pos_info();
}

static struct platform_driver kxr_aphost_platform_driver = {
	.driver = {
		.name = KXR_APHOST_DRV_NAME,
	},

	.probe = kxr_aphost_platform_driver_probe,
	.remove = kxr_aphost_platform_driver_remove,
};

static struct platform_device kxr_aphost_platform_device = {
	.name = KXR_APHOST_DRV_NAME,
	.dev = {
		.release = kxr_aphost_platform_device_release,
	},
};

static int __init kxr_aphost_init(void)
{
	int ret;

	pr_pos_info();

	ret = platform_device_register(&kxr_aphost_platform_device);
	if (ret < 0) {
		pr_red_info("platform_device_register");
		return ret;
	}

	ret = platform_driver_register(&kxr_aphost_platform_driver);
	if (ret < 0) {
		pr_red_info("platform_driver_register");
		goto out_unregister_device;
	}

	return 0;

out_unregister_device:
	platform_device_unregister(&kxr_aphost_platform_device);

	return ret;
}

static void __exit kxr_aphost_exit(void)
{
	pr_pos_info();

	platform_device_unregister(&kxr_aphost_platform_device);
	platform_driver_unregister(&kxr_aphost_platform_driver);
}
#endif

module_init(kxr_aphost_init);
module_exit(kxr_aphost_exit);

MODULE_AUTHOR("Fuang.Cao <cavan.cfa@gmail.com>");
MODULE_DESCRIPTION("KXR SPI Host Driver");
MODULE_LICENSE("GPL");
