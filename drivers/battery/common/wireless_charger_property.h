/*
 *  wireless_charger_property.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2024 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __WIRELESS_CHARGER_PROPERTY_H
#define __WIRELESS_CHARGER_PROPERTY_H __FILE__

#define WC_PROP_MODULE_NAME	"wc-prop-m"

extern int wc_prop_set_sleep_mode(int val);
extern int wc_prop_set_rx_control(int val);
extern int wc_prop_set_powered_otg_control(int val);
extern int wc_prop_set_input_voltage_regulation(int val);
extern int wc_prop_get_input_voltage_regulation(void);
extern int wc_prop_set_pad_volt_ctrl(int val);
extern int wc_prop_get_pad_volt_ctrl(void);
extern int wc_prop_set_manufacturer(int val);
extern int wc_prop_get_manufacturer(int val);
extern int wc_prop_set_otg_ctrl(int val);
extern int wc_prop_set_rx_phm(int val);
extern int wc_prop_get_rx_phm(void);
extern int wc_prop_set_2nd_done(void);
extern int wc_prop_get_tx_id(void);
extern int wc_prop_get_rx_vout(void);
#endif /* __WIRELESS_CHARGER_PROPERTY_H */
