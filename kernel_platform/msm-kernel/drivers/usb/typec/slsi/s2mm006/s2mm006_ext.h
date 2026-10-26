/* SPDX-License-Identifier: GPL-2.0 */
/*
 * s2mm006_ext.h - S2MM006 USB CC/PD external function definintion
 *
 * Copyright (C) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#ifndef __S2MM006_EXT_H
#define __S2MM006_EXT_H

#include <linux/i2c.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery_common.h>
#endif
#if IS_ENABLED(CONFIG_SEC_PD)
#include <linux/battery/sec_pd.h>
#endif
#include "s2mm006.h"
#include "s2mm006_fw.h"
#include "s2mm006_pd.h"
#include "s2mm006_sysfs.h"
#include "s2mm006_typec.h"
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006.c
////////////////////////////////////////////////////////////////////////////////
extern void *get_s2mm006_usbpd_data(void);
extern int s2mm006_read_bytes(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size);
extern int s2mm006_read_byte_flash(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size);
extern int s2mm006_write_bytes(const struct i2c_client *i2c, u16 reg, u8 *val, u16 size);
extern int s2mm006_read_byte(const struct i2c_client *i2c, u16 reg, u8 *val);
extern int s2mm006_write_byte(const struct i2c_client *i2c, u16 reg, u8 *val);
extern void s2mm006_reset(struct s2mm006_data *usbpd_data);
extern void s2mm006_set_factory_ctrl(struct s2mm006_data *usbpd_data,
		int usbid, int sleep);
extern void s2mm006_reconnect(struct s2mm006_data *usbpd_data);
extern void s2mm006_manual_LPM(struct s2mm006_data *usbpd_data, int cmd);
extern void s2mm006_cc_open(struct s2mm006_data *usbpd_data, uint32_t mode);
extern void s2mm006_om_reset(struct s2mm006_data *usbpd_data);
extern void s2mm006_init_register(struct s2mm006_data *usbpd_data);
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006_fw.c
////////////////////////////////////////////////////////////////////////////////
#if IS_ENABLED(CONFIG_S2MM006_FW_DOWNLOAD)
extern uint8_t s2mm006_get_header_fw_ver(int id);
extern const char *flashmode_to_string(u32 mode);
extern int s2mm006_flash_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type);
extern int s2mm006_flash_main_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type);
extern int s2mm006_flash_factory_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type);
extern int s2mm006_firmware_update_callback(void *data,
		void *fw_data, size_t fw_size);
extern int s2mm006_fw_sysfs_update(void *data, int mode);
extern unsigned long s2mm006_get_firmware_size(void *data);
#else
static inline uint8_t s2mm006_get_header_fw_ver(int id)
		{return 0; }
static inline const char *flashmode_to_string(u32 mode) {return NULL; }
static inline int s2mm006_flash_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type) {return 0; }
static inline int s2mm006_flash_main_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type) {return 0; }
static inline int s2mm006_flash_factory_fw(struct s2mm006_data *usbpd_data,
		void *buf, size_t size, unsigned int type) {return 0; }
static inline int s2mm006_firmware_update_callback(void *data,
		void *fw_data, size_t fw_size) {return 0; }
static inline unsigned long s2mm006_get_firmware_size(void *data) {return 0; }
static inline int s2mm006_fw_sysfs_update(void *data, int mode) {return 0; }
#endif
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006_pd.c
////////////////////////////////////////////////////////////////////////////////
extern void s2mm006_pd_init(struct s2mm006_data *usbpd_data);
extern void process_pd(void *data, u8 plug_attach_done, u8 *pdic_attach, MSG_IRQ_STATUS_Type *MSG_IRQ_State);
extern void process_ps_ready_sent(struct s2mm006_data *usbpd_data);
extern void process_ps_ready_received(struct s2mm006_data *usbpd_data);
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006_sysfs.c
////////////////////////////////////////////////////////////////////////////////
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
extern int s2mm006_pdic_sysfs_init(struct s2mm006_data *usbpd_data,
		ppdic_sysfs_property_t *ppdic_sysfs_prop);
#else
static inline int s2mm006_pdic_sysfs_init(struct s2mm006_data *usbpd_data,
		ppdic_sysfs_property_t *ppdic_sysfs_prop) {return 0; }
#endif
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006_typec.c
////////////////////////////////////////////////////////////////////////////////
#if IS_ENABLED(CONFIG_TYPEC)
extern int s2mm006_vconn_set(void *data, enum typec_role role);
extern int s2mm006_dr_set(void *data, enum typec_data_role role);
extern int s2mm006_pr_set(void *data, enum typec_role role);
extern int s2mm006_port_type_set(void *data, enum typec_port_type port_type);
#else
static inline int s2mm006_vconn_set(void *data, enum typec_role role) {return 0; }
static inline int s2mm006_dr_set(void *data, enum typec_data_role role) {return 0; }
static inline int s2mm006_pr_set(void *data, enum typec_role role) {return 0; }
static inline int s2mm006_port_type_set(void *data, enum typec_port_type port_type)
	{return 0; }
#endif
////////////////////////////////////////////////////////////////////////////////
// external functions in s2mm006_alternate.c
////////////////////////////////////////////////////////////////////////////////
#if IS_ENABLED(CONFIG_PDIC_ALTERNATE_MODE)
extern int s2mm006_sbu_control(void *data, int mode);
extern void s2mm006_set_enable_alternate_mode(void *data, int mode);
extern void s2mm006_int_altmode(struct s2mm006_data *usbpd_data);
extern int s2mm006_get_alt_info(void *data, struct pdic_alt_info *alt_info);
extern void s2mm006_alt_info_clear(void *data);
extern void s2mm006_dp_info_clear(void *data);
extern void s2mm006_alternate_init(struct s2mm006_data *usbpd_data);
extern void s2mm006_alternate_deinit(struct s2mm006_data *usbpd_data);
extern int dwc3_restart_usb_host_mode(int lanes);
#else
static inline int s2mm006_sbu_control(void *data, int mode) {return 0; }
static inline void s2mm006_set_enable_alternate_mode(void *data, int mode) {}
static inline void s2mm006_int_altmode(struct s2mm006_data *usbpd_data) {}
static inline int s2mm006_get_alt_info(void *data, struct pdic_alt_info *alt_info)
	{return 0; }
static inline void s2mm006_alt_info_clear(void *data) {}
static inline void s2mm006_dp_info_clear(void *data)
	{}
static inline void s2mm006_alternate_init(struct s2mm006_data *usbpd_data) {}
static inline void s2mm006_alternate_deinit(struct s2mm006_data *usbpd_data) {}
static inline int  dwc3_restart_usb_host_mode(int lanes) {}
#endif
#endif /* __S2MM006_EXT_H */
