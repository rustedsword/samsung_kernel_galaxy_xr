// SPDX-License-Identifier: GPL-2.0
/*
 * driver/../s2mm006.c - S2MM006 USB PD function driver
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
#define pr_fmt(fmt) "[PDIC] " fmt

#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/usb/typec/common/pdic_core.h>
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery_common.h>
#endif
#if IS_ENABLED(CONFIG_SEC_PD)
#include <linux/battery/sec_pd.h>
#endif

#include "s2mm006_ext.h"

#define S2MM006_PSY_NAME_POGO "s2mm006-pogo"
#define S2MM006_PSY_NAME_TYPEC "s2mm006-typec"
#define BATT_PACK_PID 0xA06D

int s2mm006_read_status(int is_pogo, int mode)
{
	struct power_supply *psy;
	struct s2mm006_data *usbpd_data;
	uint8_t ReadMSG[32];
	uint32_t *DataObject = (uint32_t *)ReadMSG;
	struct pdic_notifier_struct *pd_noti;

	EXTENDED_STATUS_TYPE *ext_status;
	EXTENDED_PPS_STATUS_TYPE *ext_pps_status;
	DATA_BATTERY_STATUS_TYPE *data_batt_status;
	EXTENDED_SRC_CAP_TYPE *ext_src_cap;
	int ret;

	if (is_pogo)
		psy = get_power_supply_by_name(S2MM006_PSY_NAME_POGO);
	else
		psy = get_power_supply_by_name(S2MM006_PSY_NAME_TYPEC);

	if (!psy) {
		pr_info("%s, Fail to get psy (%s)\n", __func__,
				is_pogo ? S2MM006_PSY_NAME_POGO : S2MM006_PSY_NAME_TYPEC);
		return -EINVAL;
	}

	usbpd_data = power_supply_get_drvdata(psy);
	if (!usbpd_data) {
		pr_info("%s, Fail to get drv data\n", __func__);
		return -EINVAL;
	}
	pd_noti = &usbpd_data->pd_noti;

	switch (mode) {
	case S2MM006_READ_SRCCAP_EXT:
		ret = s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_SRC_CAP_EXT, ReadMSG, 32);
		if (ret < 0) {
			pr_info("%s, Fail to read i2c\n", __func__);
			return -EINVAL;
		}
		if (*(uint16_t *)ReadMSG == 0) {
			pr_info("%s, Invalid MSG_Header\n", __func__);
			return -EINVAL;
		}

		ext_src_cap = (EXTENDED_SRC_CAP_TYPE *)(&DataObject[1]);
		//ret = ext_src_cap->object;
		ret = ext_src_cap->ID.ProductID;
		pd_noti->sink_status.vid = ext_src_cap->ID.VendorID;
		pd_noti->sink_status.pid = ext_src_cap->ID.ProductID;
		pd_noti->sink_status.xid = ext_src_cap->ID.XID >> 24;

		pr_info("%s, VID(0x%x), PID(0x%x), XID(0x%x)\n", __func__,
				ext_src_cap->ID.VendorID,
				ext_src_cap->ID.ProductID,
				ext_src_cap->ID.XID);
		break;
	case S2MM006_READ_STATUS:
		ret = s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_STATUS, ReadMSG, 32);
		if (ret < 0) {
			pr_info("%s, Fail to read i2c\n", __func__);
			pd_noti->sink_status.temp = 30;
			return 30;
		}
		if (*(uint16_t *)ReadMSG == 0) {
			pr_info("%s, Invalid MSG_Header\n", __func__);
			pd_noti->sink_status.temp = 30;
			return 30;
		}
		ext_status = (EXTENDED_STATUS_TYPE *)(&DataObject[1]);
		//ret = ext_status->object[0];
		ret = ext_status->BYTES.InternalTemp;
		pd_noti->sink_status.temp = ext_status->BYTES.InternalTemp;

		pr_info("%s, STATUS Temp(%d), event(0x%x)\n", __func__,
				ext_status->BYTES.InternalTemp,
				ext_status->BYTES.EventFlags);
		break;
	case S2MM006_READ_BATTERY_STATUS:
		ret = s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_BAT_STATUS, ReadMSG, 32);
		if (ret < 0) {
			pr_info("%s, Fail to read i2c\n", __func__);
			return -EINVAL;
		}
		if (*(uint16_t *)ReadMSG == 0) {
			pr_info("%s, Invalid MSG_Header\n", __func__);
			return -EINVAL;
		}

		data_batt_status = (DATA_BATTERY_STATUS_TYPE *)(&DataObject[1]);
		data_batt_status->BYTES.BatteryPresentCapacity += 1;
		pr_info("%s, BATTERY_STATUS capacity(%d)Wh\n", __func__, data_batt_status->BYTES.BatteryPresentCapacity);

		/* SOC(%) = read capacity * 100000 / (3 * 320 * 388) */
		/* ex) 289 * 100000 / (3 * 320 * 388) = 77.588% */
		pd_noti->sink_status.capacity = data_batt_status->BYTES.BatteryPresentCapacity * 100000 / 372480;
		ret = pd_noti->sink_status.capacity;

		pr_info("%s, BATTERY_STATUS capacity(%d)Wh, soc(%d)%%\n", __func__,
				data_batt_status->BYTES.BatteryPresentCapacity, pd_noti->sink_status.capacity);
		break;
	case S2MM006_READ_PPS_STATUS:
		ret = s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_PPS_STATUS, ReadMSG, 32);
		if (ret < 0) {
			pr_info("%s, Fail to read i2c\n", __func__);
			return -EINVAL;
		}
		if (*(uint16_t *)ReadMSG == 0) {
			pr_info("%s, Invalid MSG_Header\n", __func__);
			return -EINVAL;
		}

		ext_pps_status = (EXTENDED_PPS_STATUS_TYPE *)(&DataObject[1]);
		ret = ext_pps_status->object;

		pr_info("%s, PPS STATUS V(%d), A(%d)\n", __func__,
				ext_pps_status->BYTES.OutputVoltage * UNIT_FOR_PPS_VOLTAGE,
				ext_pps_status->BYTES.OutputCurrent * UNIT_FOR_PPS_CURRENT);
		break;
	default:
		ret = -EINVAL;
		pr_info("%s, invalid mode(%d)\n", __func__, mode);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s2mm006_read_status);

void s2mm006_send_get_battery_status(void)
{
	struct power_supply *psy;
	struct s2mm006_data *usbpd_data;

	PD_CTRL3_Type pd_ctrl3;

	psy = get_power_supply_by_name(S2MM006_PSY_NAME_POGO);
	usbpd_data = power_supply_get_drvdata(psy);
	if (!usbpd_data) {
		pr_info("%s, Fail to get drv data\n", __func__);
		return;
	}

	/* State of Charge */
	pd_ctrl3.BITS.GetBattStatus = 1;

	/* temperature */
	pd_ctrl3.BITS.GetStatus = 1;

	s2mm006_write_byte(usbpd_data->i2c, S2MM006_REG_PD_CTRL3, &pd_ctrl3.BYTE);
}
EXPORT_SYMBOL_GPL(s2mm006_send_get_battery_status);

#if 0
void s2mm006_read_bsdo(void)
{
	struct power_supply *psy;
	struct s2mm006_data *usbpd_data;
	struct pdic_notifier_struct *pd_noti;

	psy = get_power_supply_by_name(S2MM006_PSY_NAME_POGO);
	usbpd_data = power_supply_get_drvdata(psy);
	if (!usbpd_data) {
		pr_info("%s, Fail to get drv data\n", __func__);
		return;
	}
	pd_noti = &usbpd_data->pd_noti;

	if (s2mm006_read_status(1, S2MM006_READ_SRCCAP_EXT) == BATT_PACK_PID) {
		s2mm006_read_status(1, S2MM006_READ_STATUS);
		s2mm006_read_status(1, S2MM006_READ_BATTERY_STATUS);
	} else {
		pr_info("%s, invalid PID\n", __func__);
		pd_noti->sink_status.capacity = -1;
		pd_noti->sink_status.temp = 30;
	}
}
EXPORT_SYMBOL_GPL(s2mm006_read_bsdo);
#endif

void s2mm006_select_pdo(int num)
{
	struct s2mm006_data *usbpd_data = get_s2mm006_usbpd_data();
	struct pdic_notifier_struct *pd_noti;
	RDO_Type RDO;
	PD_CTRL1_Type PD_CTRL1;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is NULL\n", __func__);
		return;
	}
	pd_noti = &usbpd_data->pd_noti;

	if (pd_noti->sink_status.selected_pdo_num == num)
		return;
	else if (num > pd_noti->sink_status.available_pdo_num)
		pd_noti->sink_status.selected_pdo_num = pd_noti->sink_status.available_pdo_num;
	else if (num < 1)
		pd_noti->sink_status.selected_pdo_num = 1;
	else
		pd_noti->sink_status.selected_pdo_num = num;
	pr_info(" %s : PDO(%d) is selected to change\n", __func__, pd_noti->sink_status.selected_pdo_num);

	RDO.DATA[0] = RDO.DATA[1] = 0;

	RDO.MSG.Header.Message_Type = 0x02;	/* REQUEST */
	RDO.MSG.Header.Number_of_obj = 1;	/* 1 RDO */

	RDO.MSG.PDO.FIXED.Object_Position = pd_noti->sink_status.selected_pdo_num;
	RDO.MSG.PDO.FIXED.Maximum_Operating_Current
		= pd_noti->sink_status.power_list[num].max_current / UNIT_FOR_CURRENT;
	RDO.MSG.PDO.FIXED.Operating_Current
		= pd_noti->sink_status.power_list[num].max_current / UNIT_FOR_CURRENT;
	RDO.MSG.PDO.FIXED.No_USB_Suspend = 1;
	RDO.MSG.PDO.FIXED.USB_Communications_Capable = 1;
	RDO.MSG.PDO.FIXED.Capacity_MisMatch = 0;
	RDO.MSG.PDO.FIXED.GiveBack_Flag = 0;

	s2mm006_write_bytes(usbpd_data->i2c, S2MM006_REG_TX_DATA0, &RDO.BYTE[0], 8);

	PD_CTRL1.BYTE = 0;
	PD_CTRL1.BITS.UpdateRdo = 1;
	s2mm006_write_bytes(usbpd_data->i2c, S2MM006_REG_PD_CTRL1, &PD_CTRL1.BYTE, 1);
}
EXPORT_SYMBOL(s2mm006_select_pdo);

int s2mm006_select_pps(int num, int ppsVol, int ppsCur)
{
	struct s2mm006_data *usbpd_data = get_s2mm006_usbpd_data();
	struct pdic_notifier_struct *pd_noti;
	RDO_Type RDO;
	PD_CTRL1_Type PD_CTRL1;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is NULL\n", __func__);
		return -EINVAL;
	}
	pd_noti = &usbpd_data->pd_noti;

	if (num > pd_noti->sink_status.available_pdo_num) {
		pr_info("%s, num(%d) is higher than available pdo num\n", __func__, num);
		return -EINVAL;
	}

	if (!pd_noti->sink_status.power_list[num].apdo) {
		pr_info("%s, num(%d) is not apdo\n", __func__, num);
		return -EINVAL;
	}
	pd_noti->sink_status.selected_pdo_num = num;

	if (ppsVol > pd_noti->sink_status.power_list[num].max_voltage) {
		pr_info("%s, invalid pps Vol(%d, max(%d)mV\n", __func__,
			num, pd_noti->sink_status.power_list[num].max_voltage);
		ppsVol = pd_noti->sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_noti->sink_status.power_list[num].min_voltage) {
		pr_info("%s, invalid pps Vol(%d, min(%d)mV\n", __func__,
			num, pd_noti->sink_status.power_list[num].min_voltage);
		ppsVol = pd_noti->sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_noti->sink_status.power_list[num].max_current) {
		pr_info("%s, invalid pps Cur(%d, max(%d)mV\n", __func__,
			num, pd_noti->sink_status.power_list[num].max_current);
		ppsVol = pd_noti->sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s, ppsCur < 0\n", __func__);
		ppsCur = 0;
	}

	pr_info("%s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_noti->sink_status.selected_pdo_num, ppsVol, ppsCur);

	pd_noti->sink_status.pps_voltage = ppsVol;
	pd_noti->sink_status.pps_current = ppsCur;

	RDO.DATA[0] = RDO.DATA[1] = 0;

	RDO.MSG.PDO.PPS.Object_Position = pd_noti->sink_status.selected_pdo_num;
	RDO.MSG.PDO.PPS.Operating_Current = ppsCur / UNIT_FOR_PPS_CURRENT;
	RDO.MSG.PDO.PPS.Output_Voltage = ppsVol / UNIT_FOR_PPS_VOLTAGE_RDO;
	RDO.MSG.PDO.PPS.Capacity_MisMatch = 0;
	RDO.MSG.PDO.PPS.USB_Communications_Capable = 1;
	RDO.MSG.PDO.PPS.No_USB_Suspend = 1;

	s2mm006_write_bytes(usbpd_data->i2c, S2MM006_REG_TX_DATA0, &RDO.BYTE[0], 8);
	s2mm006_read_bytes(usbpd_data->i2c, S2MM006_REG_TX_DATA0, &RDO.BYTE[0], 8);
	pr_info("%s, Header[%x], RDO[%x]\n", __func__, RDO.DATA[0], RDO.DATA[1]);

	PD_CTRL1.BYTE = 0;
	PD_CTRL1.BITS.UpdateRdo = 1;
	s2mm006_write_bytes(usbpd_data->i2c, S2MM006_REG_PD_CTRL1, &PD_CTRL1.BYTE, 1);

	return 0;
}
EXPORT_SYMBOL(s2mm006_select_pps);

static int s2mm006_src_capacity_information(const struct i2c_client *i2c, uint32_t *RX_SRC_CAPA_MSG,
		SEC_PD_SINK_STATUS *pd_sink_status, uint8_t *do_power_nego)
{
	uint32_t RdCnt;
	uint32_t PDO_cnt;
	uint32_t PDO_sel;
	int available_pdo_num = 0;
	int num_of_obj = 0;

	MSG_HEADER_Type *MSG_HDR;
	SRC_FIXED_SUPPLY_Typedef *MSG_FIXED_SUPPLY;
	SRC_VAR_SUPPLY_Typedef  *MSG_VAR_SUPPLY;
	SRC_BAT_SUPPLY_Typedef  *MSG_BAT_SUPPLY;
	SRC_PPS_SUPPLY_Typedef *MSG_PPS_SUPPLY;

	dev_info(&i2c->dev, "\n");
	for (RdCnt = 0; RdCnt < 8; RdCnt++)
		dev_info(&i2c->dev, "Rd_SRC_CAPA_%d : 0x%X\n", RdCnt, RX_SRC_CAPA_MSG[RdCnt]);

	MSG_HDR = (MSG_HEADER_Type *)&RX_SRC_CAPA_MSG[0];
	dev_info(&i2c->dev, "=======================================\n");
	dev_info(&i2c->dev, "    MSG Header\n");

	dev_info(&i2c->dev, "    Rsvd_msg_header         : %d\n", MSG_HDR->Rsvd_msg_header);
	dev_info(&i2c->dev, "    Number_of_obj           : %d\n", MSG_HDR->Number_of_obj);
	dev_info(&i2c->dev, "    Message_ID              : %d\n", MSG_HDR->Message_ID);
	dev_info(&i2c->dev, "    Port_Power_Role         : %d\n", MSG_HDR->Port_Power_Role);
	dev_info(&i2c->dev, "    Specification_Revision  : %d\n", MSG_HDR->Specification_Revision);
	dev_info(&i2c->dev, "    Port_Data_Role          : %d\n", MSG_HDR->Port_Data_Role);
	dev_info(&i2c->dev, "    Rsvd2_msg_header        : %d\n", MSG_HDR->Rsvd2_msg_header);
	dev_info(&i2c->dev, "    Message_Type            : %d\n", MSG_HDR->Message_Type);

	num_of_obj = MSG_HDR->Number_of_obj > MAX_PDO_NUM ? MAX_PDO_NUM : MSG_HDR->Number_of_obj;
	for (PDO_cnt = 0; PDO_cnt < num_of_obj; PDO_cnt++) {
		PDO_sel = (RX_SRC_CAPA_MSG[PDO_cnt + 1] >> 30) & 0x3;
		if (PDO_sel == PDO_FIX) {
			MSG_FIXED_SUPPLY = (SRC_FIXED_SUPPLY_Typedef *)&RX_SRC_CAPA_MSG[PDO_cnt + 1];
			if (MSG_FIXED_SUPPLY->Voltage_Unit <= (AVAILABLE_VOLTAGE/UNIT_FOR_VOLTAGE))
				available_pdo_num = PDO_cnt + 1;
			if (!(*do_power_nego) &&
				(pd_sink_status->power_list[PDO_cnt+1].max_voltage != MSG_FIXED_SUPPLY->Voltage_Unit * UNIT_FOR_VOLTAGE ||
				pd_sink_status->power_list[PDO_cnt+1].max_current != MSG_FIXED_SUPPLY->Maximum_Current * UNIT_FOR_CURRENT))
				*do_power_nego = 1;
			pd_sink_status->power_list[PDO_cnt+1].max_voltage = MSG_FIXED_SUPPLY->Voltage_Unit * UNIT_FOR_VOLTAGE;
			pd_sink_status->power_list[PDO_cnt+1].max_current = MSG_FIXED_SUPPLY->Maximum_Current * UNIT_FOR_CURRENT;
			pd_sink_status->power_list[PDO_cnt+1].comm_capable = MSG_FIXED_SUPPLY->USB_Comm_Capable;
			pd_sink_status->power_list[PDO_cnt+1].suspend = MSG_FIXED_SUPPLY->USB_Suspend_Support;
			pd_sink_status->power_list[PDO_cnt+1].apdo = false;
			pd_sink_status->power_list[PDO_cnt+1].pdo_type = FPDO_TYPE;
			pd_sink_status->power_list[PDO_cnt+1].accept = true;

			if (PDO_cnt == 0) {
				dev_info(&i2c->dev, "    Dual_Role_Power         : %d\n", MSG_FIXED_SUPPLY->Dual_Role_Power);
				dev_info(&i2c->dev, "    USB_Suspend_Support     : %d\n", MSG_FIXED_SUPPLY->USB_Suspend_Support);
				dev_info(&i2c->dev, "    Externally_POW          : %d\n", MSG_FIXED_SUPPLY->Externally_POW);
				dev_info(&i2c->dev, "    USB_Comm_Capable        : %d\n", MSG_FIXED_SUPPLY->USB_Comm_Capable);
				dev_info(&i2c->dev, "    Data_Role_Swap          : %d\n", MSG_FIXED_SUPPLY->Data_Role_Swap);
				dev_info(&i2c->dev, "    Reserved                : %d\n", MSG_FIXED_SUPPLY->Reserved);
				dev_info(&i2c->dev, "    Peak_Current            : %d\n", MSG_FIXED_SUPPLY->Peak_Current);
			}
			pr_info("%s, [%d] FIX (%d)mV / %dmW\n", __func__, PDO_cnt + 1,
					MSG_FIXED_SUPPLY->Voltage_Unit * UNIT_FOR_VOLTAGE,
					MSG_FIXED_SUPPLY->Maximum_Current * UNIT_FOR_CURRENT);
		} else if (PDO_sel == PDO_BAT) {
			MSG_BAT_SUPPLY = (SRC_BAT_SUPPLY_Typedef *)&RX_SRC_CAPA_MSG[PDO_cnt + 1];

			pr_info("%s, [%d] BAT (%d)mV-(%d)mV / %dmW\n", __func__, PDO_cnt + 1,
					MSG_BAT_SUPPLY->Minimum_Voltage * UNIT_FOR_VOLTAGE,
					MSG_BAT_SUPPLY->Maximum_Voltage * UNIT_FOR_VOLTAGE,
					MSG_BAT_SUPPLY->Maximum_Allow_Power * UNIT_FOR_POWER);
		} else if (PDO_sel == PDO_VAR) {
			MSG_VAR_SUPPLY = (SRC_VAR_SUPPLY_Typedef *)&RX_SRC_CAPA_MSG[PDO_cnt + 1];

			pr_info("%s, [%d] VAR (%d)mV-(%d)mV / %dmA\n", __func__, PDO_cnt + 1,
					MSG_VAR_SUPPLY->Minimum_Voltage * UNIT_FOR_VOLTAGE,
					MSG_VAR_SUPPLY->Maximum_Voltage * UNIT_FOR_VOLTAGE,
					MSG_VAR_SUPPLY->Maximum_Current * UNIT_FOR_CURRENT);
		} else if (PDO_sel == PDO_PPS) {
			MSG_PPS_SUPPLY = (SRC_PPS_SUPPLY_Typedef *)&RX_SRC_CAPA_MSG[PDO_cnt + 1];

			pd_sink_status->power_list[PDO_cnt+1].max_current = MSG_PPS_SUPPLY->Maximum_Current * UNIT_FOR_PPS_CURRENT;
			pd_sink_status->power_list[PDO_cnt+1].max_voltage = MSG_PPS_SUPPLY->Maximum_Voltage * UNIT_FOR_PPS_VOLTAGE;
			pd_sink_status->power_list[PDO_cnt+1].min_voltage = MSG_PPS_SUPPLY->Minimum_Voltage * UNIT_FOR_PPS_VOLTAGE;
			pd_sink_status->power_list[PDO_cnt+1].apdo = true;
			pd_sink_status->power_list[PDO_cnt+1].pdo_type = APDO_TYPE;
			pd_sink_status->power_list[PDO_cnt+1].accept = true;
			available_pdo_num = PDO_cnt + 1;
			pd_sink_status->has_apdo = true;

			pr_info("%s, [%d] PPS (%d)mV-(%d)mV / %dmA\n", __func__, PDO_cnt + 1,
					MSG_PPS_SUPPLY->Minimum_Voltage * UNIT_FOR_PPS_VOLTAGE,
					MSG_PPS_SUPPLY->Maximum_Voltage * UNIT_FOR_PPS_VOLTAGE,
					MSG_PPS_SUPPLY->Maximum_Current * UNIT_FOR_PPS_CURRENT);
		}
	}

	/* the number of available pdo list */
	pd_sink_status->available_pdo_num = available_pdo_num;
	dev_info(&i2c->dev, "=======================================\n");
	return available_pdo_num;
}

void process_ps_ready_sent(struct s2mm006_data *usbpd_data)
{
	if (!usbpd_data->pd_contract) {
		usbpd_data->pd_contract = 1;
		pdic_policy_send_msg(usbpd_data->pp_data, MSG_EX_CNT, 0, 0);
	}
}

void process_ps_ready_received(struct s2mm006_data *usbpd_data)
{
	struct pdic_notifier_struct *pd_noti;
	uint8_t ReadMSG[32];
	int available_pdo_num;
	uint8_t do_power_nego = 0;
	int skip_event = 0;
	REQUEST_FIXED_SUPPLY_STRUCT_Typedef *request_power_number;

	pd_noti = &usbpd_data->pd_noti;

	pd_noti->event = PDIC_NOTIFY_EVENT_PD_SINK;

	s2mm006_read_bytes(usbpd_data->i2c, S2MM006_RX_SRC_CAP, ReadMSG, 32);
	available_pdo_num = s2mm006_src_capacity_information(usbpd_data->i2c, (uint32_t *)ReadMSG,
			&pd_noti->sink_status, &do_power_nego);

	s2mm006_read_bytes(usbpd_data->i2c, S2MM006_TX_REQUEST, ReadMSG, 32);
	request_power_number = (REQUEST_FIXED_SUPPLY_STRUCT_Typedef *)&ReadMSG[4];

	pr_info(" %s : Object_posision(%d), available_pdo_num(%d), selected_pdo_num(%d)\n", __func__,
		request_power_number->Object_Position, available_pdo_num, pd_noti->sink_status.selected_pdo_num);
	pd_noti->sink_status.current_pdo_num = request_power_number->Object_Position;

	if (available_pdo_num > 0) {
		if (request_power_number->Object_Position != pd_noti->sink_status.selected_pdo_num) {
			if (pd_noti->sink_status.selected_pdo_num == 0) {
				pr_info(" %s : PDO is not selected yet by default\n", __func__);
				pd_noti->sink_status.selected_pdo_num = pd_noti->sink_status.current_pdo_num;
			}
		} else {
			if (do_power_nego) {
				pr_info(" %s : PDO(%d) is selected, but power negotiation is requested\n",
					__func__, pd_noti->sink_status.selected_pdo_num);
				pd_noti->sink_status.selected_pdo_num = 0;
				pd_noti->event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
				skip_event = 1;
			} else {
				pr_info(" %s : PDO(%d) is selected, but same with previous list, so skip\n",
					__func__, pd_noti->sink_status.selected_pdo_num);
			}
		}
	} else {
		pr_info(" %s : PDO is not selected\n", __func__);
	}

	if (!usbpd_data->pd_contract) {
		if (usbpd_data->is_short) {
			pd_noti->sink_status.available_pdo_num = 1;
			pd_noti->sink_status.power_list[1].max_current =
				pd_noti->sink_status.power_list[1].max_current > 1800 ?
				1800 : pd_noti->sink_status.power_list[1].max_current;
			pd_noti->sink_status.has_apdo = false;
		}
		usbpd_data->pd_contract = true;
	}

	pdic_policy_send_msg(usbpd_data->pp_data, MSG_EX_CNT, skip_event, 0);
}

void s2mm006_pd_init(struct s2mm006_data *usbpd_data)
{
	usbpd_data->pd_noti.sink_status.available_pdo_num = 0;
	usbpd_data->pd_noti.sink_status.selected_pdo_num = 0;
	usbpd_data->pd_noti.sink_status.current_pdo_num = 0;
	usbpd_data->pd_noti.sink_status.pps_voltage = 0;
	usbpd_data->pd_noti.sink_status.pps_current = 0;
	usbpd_data->pd_noti.sink_status.temp = 30;
	usbpd_data->pd_noti.sink_status.capacity = -1;
	usbpd_data->pd_noti.sink_status.has_apdo = false;
	usbpd_data->pd_noti.sink_status.fp_sec_pd_select_pdo = s2mm006_select_pdo;
	usbpd_data->pd_noti.sink_status.fp_sec_pd_select_pps = s2mm006_select_pps;
	usbpd_data->pd_noti.sink_status.fp_sec_pd_get_bsdo = s2mm006_send_get_battery_status;
}

