/*
 *  wireless_charger_property.c
 *  Samsung Mobile Wireless Charger Property Driver
 *
 *  Copyright (C) 2024 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/battery/sec_battery_common.h>
#include "wireless_charger_property.h"

struct wc_info {
	char *wireless_charger_name;
};
static struct wc_info *wc_pdata;

int wc_prop_set_sleep_mode(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_SLEEP_MODE, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_sleep_mode);

int wc_prop_set_rx_control(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_rx_control);

int wc_prop_set_powered_otg_control(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_powered_otg_control);

int wc_prop_set_input_voltage_regulation(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_input_voltage_regulation);

int wc_prop_get_input_voltage_regulation(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_input_voltage_regulation);

int wc_prop_set_pad_volt_ctrl(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_pad_volt_ctrl);

int wc_prop_get_pad_volt_ctrl(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_pad_volt_ctrl);

int wc_prop_set_manufacturer(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
		POWER_SUPPLY_PROP_MANUFACTURER, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_manufacturer);

int wc_prop_get_manufacturer(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_PROP_MANUFACTURER, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_manufacturer);

int wc_prop_set_otg_ctrl(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_otg_ctrl);

int wc_prop_set_rx_phm(int val)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	value.intval = val;
	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_RX_PHM, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_rx_phm);

int wc_prop_get_rx_phm(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_RX_PHM, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_rx_phm);

int wc_prop_set_2nd_done(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_WIRELESS_2ND_DONE, value);

	return ret;
}
EXPORT_SYMBOL(wc_prop_set_2nd_done);

int wc_prop_get_tx_id(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_tx_id);

int wc_prop_get_rx_vout(void)
{
	union power_supply_propval value = {0, };
	int ret = 0;

	ret = psy_do_property(wc_pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT, value);

	return value.intval;
}
EXPORT_SYMBOL(wc_prop_get_rx_vout);

int wc_prop_parse_dt(struct wc_info *wc)
{
	struct device_node *np;
	int ret = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	ret = of_property_read_string(np,
		"battery,wireless_charger_name", (char const **)&wc->wireless_charger_name);
	if (ret)
		pr_info("%s: Wireless charger name is Empty\n", __func__);

	return 0;
}

static int __init wc_prop_m_init(void)
{
	struct wc_info *wc = NULL;
	int ret = 0;

	pr_info("%s Loading\n", __func__);

	wc = kzalloc(sizeof(struct wc_info), GFP_KERNEL);
	if (!wc) {
		pr_err("%s failed to alloc wpc_enm\n", __func__);
		ret = -ENOMEM;
		goto failed_alloc;
	}

	ret = wc_prop_parse_dt(wc);
	if (ret < 0) {
		pr_err("%s failed to parse dt (ret = %d)\n", __func__, ret);
		goto failed_dt;
	}

	wc_pdata = wc;
	pr_info("%s Loaded\n", __func__);

	return ret;

failed_dt:
	kfree(wc);
failed_alloc:

	return ret;
}
module_init(wc_prop_m_init);

static void __exit wc_prop_m_exit(void)
{
}
module_exit(wc_prop_m_exit);

MODULE_DESCRIPTION("Samsung wireless charger property module Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
