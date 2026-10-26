/* SPDX-License-Identifier: GPL-2.0 */
/*
 * s2mm006_pd.h - S2MM006 pd function header
 *
 * Copyright (C) 2021 Samsung Electronics
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
#ifndef __S2MM006_PD_H__
#define __S2MM006_PD_H__

enum {
	S2MM006_READ_STATUS,
	S2MM006_READ_BATTERY_STATUS,
	S2MM006_READ_PPS_STATUS,
	S2MM006_READ_SRCCAP_EXT,
};

typedef union {
	u32 object;

	struct {
		u8 Reserved;
		u8 BatteryInfo;
		u16 BatteryPresentCapacity;
	} BYTES;
} DATA_BATTERY_STATUS_TYPE;

typedef union {
	u8 byte[8];
	u32 object[2];
	u64 ext_object;

	struct {
		u16 VendorID;
		u16 ProductID;
		u32 XID;
	} ID;
} EXTENDED_SRC_CAP_TYPE;

typedef union {
	u8 byte[7];
	u32 object[2];

	struct {
		u8 InternalTemp;
		u8 PresentInput;
		u8 PresentBatteryInput;
		u8 EventFlags;
		u8 TemperatureStatus;
		u8 PowerStatus;
		u8 PowerStageChange;
	} BYTES;
} EXTENDED_STATUS_TYPE;

typedef union {
	u8 byte[4];
	u32 object;

	struct {
		u16 OutputVoltage;
		u8 OutputCurrent;
		u8 RealTimeFlags;
	} BYTES;
} EXTENDED_PPS_STATUS_TYPE;

#endif /* __S2MM006_PD_H__ */
