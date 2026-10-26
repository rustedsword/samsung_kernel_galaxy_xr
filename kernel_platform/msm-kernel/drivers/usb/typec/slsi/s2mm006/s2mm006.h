/*
 * s2mm006.h - S2MM006 USBPD device driver header
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
#ifndef __S2MM006_H
#define __S2MM006_H 
//__FILE__

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/power_supply.h>

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/common/pdic_policy.h>
#endif
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif
#include <linux/usb/typec.h>
#if IS_ENABLED(CONFIG_SEC_PD)
#include <linux/battery/sec_pd.h>
#endif

#define UNIT_FOR_VOLTAGE 50
#define UNIT_FOR_CURRENT 10
#define UNIT_FOR_POWER 250

#define UNIT_FOR_PPS_VOLTAGE 100
#define UNIT_FOR_PPS_VOLTAGE_RDO 20
#define UNIT_FOR_PPS_CURRENT 50

#define REG_I2C_SLV_CMD		0x10
#define REG_TX_SINK_CAPA_MSG    0x0220
#define REG_TX_REQUEST_MSG	0x0240
#define REG_RX_SRC_CAPA_MSG	0x0260

#define PDIC_FW_VERSION_INVALID -1
#define PDIC_I2C_VALUE_INVALID 274

/******************************************************************************/
/* definitions & structures                                                   */
/******************************************************************************/
#define USBPD006_DEV_NAME  "sec-s2mm006"
#define USBPD006_TYPEC_DEV_NAME  "sec-s2mm006_typec"

/*
 ******************************************************************************
 * @file    EXT_SRAM.h
 * @author  Power Device Team
 * @version V1.0.0
 * @date    2016.03.28
 * @brief   Header for EXT SRAM map
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/

#define S2MM006_REG_DEVICE_HW_ID				0x00
#define S2MM006_REG_DEVICE_FW_ID				0x01
#define S2MM006_REG_BC_INT						0x02
#define S2MM006_REG_CC_INT						0x03
#define S2MM006_REG_PD_INT						0x04
#define S2MM006_REG_FACTORY_INT					0x05
#define S2MM006_REG_PROTECTION_INT				0x06
#define S2MM006_REG_PD_INT2						0x07
#define S2MM006_REG_BC_MASK						0x08
#define S2MM006_REG_CC_MASK						0x09
#define S2MM006_REG_PD_MASK						0x0A
#define S2MM006_REG_FACTORY_MASK				0x0B
#define S2MM006_REG_PROTECTION_MASK				0x0C
#define S2MM006_REG_PD_MASK2					0x0D
#define S2MM006_REG_BC_STATUS1					0x0E
#define S2MM006_REG_BC_STATUS2					0x0F

#define S2MM006_REG_BC_STATUS3					0x10
#define S2MM006_REG_CC_STATUS1					0x11
#define S2MM006_REG_CC_STATUS2					0x12
#define S2MM006_REG_PD_STATUS1					0x13
#define S2MM006_REG_PD_STATUS2					0x14
#define S2MM006_REG_FACTORY_STATUS				0x15
#define S2MM006_REG_PROTECTION_STATUS1			0x16
#define S2MM006_REG_PROTECTION_STATUS2			0x17
#define S2MM006_REG_FW_UPDATE_STATUS			0x18
#define S2MM006_REG_FACTORY_STATUS2				0x19
#define S2MM006_REG_PD_STATUS3					0x1A
#define S2MM006_REG_PD_STATUS4					0x1D
#define S2MM006_REG_Reserved_1B					0x1B
#define S2MM006_REG_VBUS_STATUS					0x1C
#define S2MM006_REG_Reserved_1D					0x1D
#define S2MM006_REG_DEVICE_FW_TEST_ID				0x1E
#define S2MM006_REG_DEVICE_BOOT_ID				0x1F
#define S2MM006_REG_PWR_CTRL1					0x20
#define S2MM006_REG_PWR_CTRL2					0x21
#define S2MM006_REG_CC_CTRL1					0x22
#define S2MM006_REG_CC_CTRL2					0x23
#define S2MM006_REG_CC_CTRL3					0x24
#define S2MM006_REG_CC_CTRL4					0x25
#define S2MM006_REG_CC_CTRL5					0x26
#define S2MM006_REG_CC_CTRL6					0x27
#define S2MM006_REG_SBU_CTRL1					0x28
#define S2MM006_REG_SBU_CTRL2					0x29
#define S2MM006_REG_FACTORY_CTRL				0x2A
#define S2MM006_REG_WATER_CTRL1					0x2B
#define S2MM006_REG_WATER_CTRL2					0x2C
#define S2MM006_REG_BC_CTRL1					0x2D
#define S2MM006_REG_BC_CTRL2					0x2E
#define S2MM006_REG_BC_CTRL3					0x2F

#define S2MM006_REG_BC_CTRL4					0x30
#define S2MM006_REG_AFC_CTRL1					0x31
#define S2MM006_REG_AFC_CTRL2					0x32
#define S2MM006_REG_AFC_CTRL3					0x33
#define S2MM006_REG_AFC_RX_DATA1				0x34
#define S2MM006_REG_AFC_RX_DATA2				0x35
#define S2MM006_REG_AFC_RX_DATA3				0x36
#define S2MM006_REG_AFC_RX_DATA4				0x37
#define S2MM006_REG_AFC_RX_DATA5				0x38
#define S2MM006_REG_AFC_RX_DATA6				0x39
#define S2MM006_REG_AFC_RX_DATA7				0x3A
#define S2MM006_REG_AFC_RX_DATA8				0x3B
#define S2MM006_REG_AFC_RX_DATA9				0x3C
#define S2MM006_REG_AFC_RX_DATA10				0x3D
#define S2MM006_REG_AFC_RX_DATA11				0x3E
#define S2MM006_REG_AFC_RX_DATA12				0x3F

#define S2MM006_REG_AFC_RX_DATA13				0x40
#define S2MM006_REG_AFC_RX_DATA14				0x41
#define S2MM006_REG_AFC_RX_DATA15				0x42
#define S2MM006_REG_AFC_RX_DATA16				0x43
#define S2MM006_REG_QC_CTRL1					0x44
#define S2MM006_REG_QC_TX_DATA					0x45
#define S2MM006_REG_QC_RX_DATA					0x46
#define S2MM006_REG_SBU1_STATUS					0x47
#define S2MM006_REG_SBU2_STATUS					0x48
#define S2MM006_REG_Reserved_49					0x49
#define S2MM006_REG_GPIO_CTRL1					0x4A
#define S2MM006_REG_GPIO_CTRL2					0x4B
#define S2MM006_REG_GPIO_CTRL3					0x4C
#define S2MM006_REG_GPIO_CTRL4					0x4D
#define S2MM006_REG_VBUS_CTRL					0x4E
#define S2MM006_REG_Reserved_4F					0x4F

#define S2MM006_REG_PD_CTRL1					0x50
#define S2MM006_REG_PD_CTRL2					0x51
#define S2MM006_REG_PD_CTRL3					0x52
#define S2MM006_REG_PD_CTRL4					0x53
#define S2MM006_REG_PD_CTRL5					0x54
#define S2MM006_REG_PD_CTRL6					0x55
#define S2MM006_REG_FW_UPDATE_CTRL1				0x56
#define S2MM006_REG_FW_UPDATE_CTRL2				0x57
#define S2MM006_REG_FW_UPDATE_CTRL3				0x58
#define S2MM006_REG_RX_DATA0					0x59
#define S2MM006_REG_RX_DATA1					0x5A
#define S2MM006_REG_RX_DATA2					0x5B
#define S2MM006_REG_RX_DATA3					0x5C
#define S2MM006_REG_RX_DATA4					0x5D
#define S2MM006_REG_RX_DATA5					0x5E
#define S2MM006_REG_RX_DATA6					0x5F

#define S2MM006_REG_RX_DATA7					0x60
#define S2MM006_REG_RX_DATA8					0x61
#define S2MM006_REG_RX_DATA9					0x62
#define S2MM006_REG_RX_DATA10					0x63
#define S2MM006_REG_RX_DATA11					0x64
#define S2MM006_REG_RX_DATA12					0x65
#define S2MM006_REG_RX_DATA13					0x66
#define S2MM006_REG_RX_DATA14					0x67
#define S2MM006_REG_RX_DATA15					0x68
#define S2MM006_REG_RX_DATA16					0x69
#define S2MM006_REG_RX_DATA17					0x6A
#define S2MM006_REG_RX_DATA18					0x6B
#define S2MM006_REG_RX_DATA19					0x6C
#define S2MM006_REG_RX_DATA20					0x6D
#define S2MM006_REG_RX_DATA21					0x6E
#define S2MM006_REG_RX_DATA22					0x6F

#define S2MM006_REG_RX_DATA23					0x70
#define S2MM006_REG_RX_DATA24					0x71
#define S2MM006_REG_RX_DATA25					0x72
#define S2MM006_REG_RX_DATA26					0x73
#define S2MM006_REG_RX_DATA27					0x74
#define S2MM006_REG_RX_DATA28					0x75
#define S2MM006_REG_RX_DATA29					0x76
#define S2MM006_REG_RX_DATA30					0x77
#define S2MM006_REG_RX_DATA31					0x78
#define S2MM006_REG_RX_DATA32					0x79
#define S2MM006_REG_RESERVED_7A					0x7A
#define S2MM006_REG_RESERVED_7B					0x7B
#define S2MM006_REG_RESERVED_7C					0x7C
#define S2MM006_REG_RESERVED_7D					0x7D
#define S2MM006_REG_RESERVED_7E					0x7E
#define S2MM006_REG_RESERVED_7F					0x7F

#define S2MM006_REG_TX_DATA0					0x80
#define S2MM006_REG_TX_DATA1					0x81
#define S2MM006_REG_TX_DATA2					0x82
#define S2MM006_REG_TX_DATA3					0x83
#define S2MM006_REG_TX_DATA4					0x84
#define S2MM006_REG_TX_DATA5					0x85
#define S2MM006_REG_TX_DATA6					0x86
#define S2MM006_REG_TX_DATA7					0x87
#define S2MM006_REG_TX_DATA8					0x88
#define S2MM006_REG_TX_DATA9					0x89
#define S2MM006_REG_TX_DATA10					0x8A
#define S2MM006_REG_TX_DATA11					0x8B
#define S2MM006_REG_TX_DATA12					0x8C
#define S2MM006_REG_TX_DATA13					0x8D
#define S2MM006_REG_TX_DATA14					0x8E
#define S2MM006_REG_TX_DATA15					0x8F

#define S2MM006_REG_TX_DATA16					0x90
#define S2MM006_REG_TX_DATA17					0x91
#define S2MM006_REG_TX_DATA18					0x92
#define S2MM006_REG_TX_DATA19					0x93
#define S2MM006_REG_TX_DATA20					0x94
#define S2MM006_REG_TX_DATA21					0x95
#define S2MM006_REG_TX_DATA22					0x96
#define S2MM006_REG_TX_DATA23					0x97
#define S2MM006_REG_TX_DATA24					0x98
#define S2MM006_REG_TX_DATA25					0x99
#define S2MM006_REG_TX_DATA26					0x9A
#define S2MM006_REG_TX_DATA27					0x9B
#define S2MM006_REG_TX_DATA28					0x9C
#define S2MM006_REG_TX_DATA29					0x9D
#define S2MM006_REG_TX_DATA30					0x9E
#define S2MM006_REG_TX_DATA31					0x9F

#define S2MM006_REG_TX_DATA32					0xA0
#define S2MM006_REG_RID_READ_Type_4uA			0xA1
#define S2MM006_REG_RID_READ_Type_320uA			0xA2
#define S2MM006_REG_RESERVED_A3					0xA3
#define S2MM006_REG_RESERVED_A4					0xA4
#define S2MM006_REG_RESERVED_A5					0xA5
#define S2MM006_REG_RESERVED_A6					0xA6
#define S2MM006_REG_RESERVED_A7					0xA7
#define S2MM006_REG_RESERVED_A8					0xA8
#define S2MM006_REG_RESERVED_A9					0xA9
#define S2MM006_REG_RESERVED_AA					0xAA
#define S2MM006_REG_RESERVED_AB					0xAB
#define S2MM006_REG_RESERVED_AC					0xAC
#define S2MM006_REG_RESERVED_AD					0xAD
#define S2MM006_REG_RESERVED_AE					0xAE
#define S2MM006_REG_RESERVED_AF					0xAF

#define S2MM006_REG_RESERVED_B0					0xB0
#define S2MM006_REG_RESERVED_B1					0xB1
#define S2MM006_REG_RESERVED_B2					0xB2
#define S2MM006_REG_RESERVED_B3					0xB3
#define S2MM006_REG_RESERVED_B4					0xB4
#define S2MM006_REG_RESERVED_B5					0xB5
#define S2MM006_REG_RESERVED_B6					0xB6
#define S2MM006_REG_RESERVED_B7					0xB7
#define S2MM006_REG_RESERVED_B8					0xB8
#define S2MM006_REG_RESERVED_B9					0xB9
#define S2MM006_REG_RESERVED_BA					0xBA
#define S2MM006_REG_RESERVED_BB					0xBB
#define S2MM006_REG_RESERVED_BC					0xBC
#define S2MM006_REG_RESERVED_BD					0xBD
#define S2MM006_REG_RESERVED_BE					0xBE
#define S2MM006_REG_RESERVED_BF					0xBF

#define S2MM006_REG_RESERVED_C0					0xC0
#define S2MM006_REG_RESERVED_C1					0xC1
#define S2MM006_REG_RESERVED_C2					0xC2
#define S2MM006_REG_RESERVED_C3					0xC3
#define S2MM006_REG_RESERVED_C4					0xC4
#define S2MM006_REG_RESERVED_C5					0xC5
#define S2MM006_REG_RESERVED_C6					0xC6
#define S2MM006_REG_RESERVED_C7					0xC7
#define S2MM006_REG_RESERVED_C8					0xC8
#define S2MM006_REG_RESERVED_C9					0xC9
#define S2MM006_REG_RESERVED_CA					0xCA
#define S2MM006_REG_RESERVED_CB					0xCB
#define S2MM006_REG_RESERVED_CC					0xCC
#define S2MM006_REG_RESERVED_CD					0xCD
#define S2MM006_REG_RESERVED_CE					0xCE
#define S2MM006_REG_RESERVED_CF					0xCF

#define S2MM006_REG_RESERVED_D0					0xD0
#define S2MM006_REG_RESERVED_D1					0xD1
#define S2MM006_REG_RESERVED_D2					0xD2
#define S2MM006_REG_RESERVED_D3					0xD3
#define S2MM006_REG_RESERVED_D4					0xD4
#define S2MM006_REG_RESERVED_D5					0xD5
#define S2MM006_REG_RESERVED_D6					0xD6
#define S2MM006_REG_RESERVED_D7					0xD7
#define S2MM006_REG_RESERVED_D8					0xD8
#define S2MM006_REG_RESERVED_D9					0xD9
#define S2MM006_REG_RESERVED_DA					0xDA
#define S2MM006_REG_RESERVED_DB					0xDB
#define S2MM006_REG_RESERVED_DC					0xDC
#define S2MM006_REG_RESERVED_ED					0xED
#define S2MM006_REG_RESERVED_EE					0xEE
#define S2MM006_REG_RESERVED_EF					0xEF

#define S2MM006_REG_RESERVED_E0					0xE0
#define S2MM006_REG_RESERVED_E1					0xE1
#define S2MM006_REG_RESERVED_E2					0xE2
#define S2MM006_REG_RESERVED_E3					0xE3
#define S2MM006_REG_RESERVED_E4					0xE4
#define S2MM006_REG_RESERVED_E5					0xE5
#define S2MM006_REG_RESERVED_E6					0xE6
#define S2MM006_REG_RESERVED_E7					0xE7
#define S2MM006_REG_RESERVED_E8					0xE8
#define S2MM006_REG_RESERVED_E9					0xE9
#define S2MM006_REG_RESERVED_EA					0xEA
#define S2MM006_REG_RESERVED_EB					0xEB
#define S2MM006_REG_RESERVED_EC					0xEC
#define S2MM006_REG_RESERVED_ED					0xED
#define S2MM006_REG_RESERVED_EE					0xEE
#define S2MM006_REG_RESERVED_EF					0xEF

#define S2MM006_REG_RESERVED_F0					0xF0
#define S2MM006_REG_RESERVED_F1					0xF1
#define S2MM006_REG_RESERVED_F2					0xF2
#define S2MM006_REG_RESERVED_F3					0xF3
#define S2MM006_REG_FW_PD_STATE0				0xF4
#define S2MM006_REG_FW_PD_STATE1				0xF5
#define S2MM006_REG_FW_PD_STATE2				0xF6
#define S2MM006_REG_FW_PD_STATE3				0xF7
#define S2MM006_REG_FW_MU_STATE0				0xF8
#define S2MM006_REG_FW_MU_STATE1				0xF9
#define S2MM006_REG_FW_MU_STATE2				0xFA
#define S2MM006_REG_FW_MU_STATE3				0xFB
#define S2MM006_REG_RESERVED_FW_IMG_SIZE_0		0xFC
#define S2MM006_REG_RESERVED_FW_IMG_SIZE_1		0xFD
#define S2MM006_REG_RESERVED_FW_IMG_SIZE_2		0xFE
#define S2MM006_REG_RESERVED_FW_IMG_SIZE_3		0xFF

#define S2MM006_RX_SRC_CAP						0x0A60
#define S2MM006_RX_SRC_CAP_EXT					0x0C40
#define S2MM006_RX_BAT_STATUS						0x0BE0
#define S2MM006_RX_STATUS						0x0C60
#define S2MM006_RX_PPS_STATUS						0x0DA0
#define S2MM006_RX_DIS_ID						0x0AE0
#define S2MM006_RX_DIS_SVID						0x0B00
#define S2MM006_RX_DIS_MODE						0x0B20
#define S2MM006_RX_ENTER_MODE					0x0B40
#define S2MM006_RX_EXIT_MODE					0x0B60
#define S2MM006_RX_ATTENTION					0x0B80
#define S2MM006_RX_DP_STATUS_UPDATE				0x0BA0
#define S2MM006_RX_DP_CONFIGURE					0x0BC0

#define S2MM006_TX_REQUEST						0x06A0
#define S2MM006_TX_SINK_CAPA					0x0680

/* S2MM006_REG_CC_STATUS1 */
#define S2MM006_CC_SINK							0x1
#define S2MM006_CC_SOURCE						0x2
#define S2MM006_CC_POWERED_CABLE				0x3
#define S2MM006_CC_DEBUG						0x4
#define S2MM006_CC_AUDIO						0x5

/* S2MM006_REG_CC_STATUS2 */
#define S2MM006_CC_1ACTIVE	0x1
#define S2MM006_CC_2ACTIVE	0x2

#define MAX_INT_STATUS_NUM 6
typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			HWVer:3,
			VendorID:5;
	} BITS;
} DEVICE_HW_ID_Type; /* 0x0000 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			FWVer:8;
	} BITS;
} DEVICE_FW_ID_Type; /* 0x0001 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			TestVer:8;
	} BITS;
} DEVICE_TEST_ID_Type; /* 0x0001 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			FWVer:8;
	} BITS;
} DEVICE_BOOT_ID_Type; /* 0x0001 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VbusDet:1,
			VbusADC:1,
			Reserved:1,
			ChgType:1,
			AfcMsg:1,
			SysMsg:1,
			Reserved_1:2;
	} BITS;
} BC_INT_Type;  /* 0x0002 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			vSafe0v:1,
			Attach:1,
			Detach:1,
			SrcCur:1,
			ccStatus:1,
			VconnStatus:1,
			VbusReq:1,
			Reserved:1;
	} BITS;
} CC_INT_Type;  /* 0x0003 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			PdMessage:1,
			PsReady:1,
			DataRole:1,
			PowerRole:1,
			VconnRole:1,
			BatteryStatus:1,
			DpConfigure:1,
			DpAttention:1;
	} BITS;
} PD_INT_Type;  /* 0x0004 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcidDet:1,
			RidDet:1,
			Factory:1,
			FcyWakeUp:1,
			FwUpdate:1,
			FwModeEnter:1,
			IfcSenseApInt:1,
			FwCmdReceive:1;
	} BITS;
} FACTORY_INT_Type;  /* 0x0005 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved0:1,
			Ocp:1,
			Ovp:1,
			Water:1,
			KillerDet:1,
			ResetDone:1,
			Reserved1:2;
	} BITS;
} PROTECTION_INT_Type;     /* 0x0006 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
		AlternateMode:1,
		RejectWait:1,
		Status:1,
		Reserved:5;
	} BITS;
} PD_INT2_Type;  /* 0x0007 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VbusDet:1,
			VbusADC:1,
			Reserved:1,
			ChgType:1,
			AfcMsg:1,
			SysMsg:1,
			Reserved_1:2;
	} BITS;
} BC_MASK_Type; /* 0x0008 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			vSafe0v:1,
			Attach:1,
			Detach:1,
			SrcCur:1,
			CcStatus:1,
			VconnStatus:1,
			Reserved:2;
	} BITS;
} CC_MASK_Type;  /* 0x0009 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			PdMessage:1,
			PsReady:1,
			DataRole:1,
			PowerRole:1,
			VconnRole:1,
			DpStatus:1,
			DpConfigure:1,
			DpAttention:1;
	} BITS;
} PD_MASK_Type;  /* 0x000A */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcidDet:1,
			RidDet:1,
			Factory:1,
			FcyWakeUp:1,
			FwUpdate:1,
			FwModeEnter:1,
			Reserved:2;
	} BITS;
} FACTORY_MASK_Type;  /* 0x000B */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcOvp:1,
			SbuOvp:1,
			DpDmOvp:1,
			VconnOcp:1,
			VconnScp:1,
			Water:1,
			KillerDet:1,
			ResetDone:1;
	} BITS;
} PROTECTION_MASK1_Type;     /* 0x000C */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
		AlternateMode:1,
		RejectWait:1,
		Status:1,
		Reserved:5;
	} BITS;
} PD_MASK2_Type;  /* 0x000D */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VbusDet:1,
			ChgType:3,
			Reserved:4;
	} BITS;
} BC_STATUS1_Type;     /* 0x000E */

typedef enum {
	CHG_TYPE_NO_CONNECTION	= 0,
	CHG_TYPE_TIMEOUT	= 1,
	CHG_TYPE_SDP		= 2,
	CHG_TYPE_CDP		= 3,
	CHG_TYPE_DCP		= 4,
	CHG_TYPE_HVDCP		= 5,
	CHG_TYPE_HVDCP_AFC	= 6,
	CHG_TYPE_HVDCP_QC	= 7,
} tCHG_TYPE;

typedef enum {
	NO_FW_UPDATE		= 0,
	MAIN_FW_UPDATE		= 1,
	FACTORY_FW_UPDATE	= 3,
} FW_UPDATE_Type;

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VbusAdc:6,
			Reserved:2;
	} BITS;
} BC_STATUS2_Type;     /* 0x000F */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			AfcMessage:4,
			SysMessage:4;
	} BITS;
} BC_STATUS3_Type;     /* 0x0010 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			vSafe0V:1,
			Attach:1,
			SrcCur:2,
			CcStatus:4;
	} BITS;
} CC_STATUS1_Type;     /* 0x0011 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcPinActive:2,
			VbusReq:1,
			Reserved:5;
	} BITS;
} CC_STATUS2_Type;     /* 0x0012 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			PdMessage:8;
	} BITS;
} PD_STATUS1_Type;     /* 0x0013 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			PsReady:2,
			DataRole:1,
			PowerRole:1,
			VconnRole:1,
			RejectWait:1,
			Reserved:2;
	} BITS;
} PD_STATUS2_Type;     /* 0x0014 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcidValue:3,
			RidValue:3,
			Factory:1,
			FcyWakeUp:1;
	} BITS;
} FACTORY_STATUS_Type;     /* 0x0015 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:4,
			Water:1,
			Hiccup:1,
			KillerDet:1,
			ResetDone:1;
	} BITS;
} PROTECTION_STATUS1_Type;     /* 0x0016 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Cc1Ovp:1,
			Cc2Ovp:1,
			Sbu1Ovp:1,
			Sbu2Ovp:1,
			DpOvp:1,
			DmOvp:1,
			VconnOcp:1,
			VconnScp:1;
	} BITS;
} PROTECTION_STATUS2_Type;     /* 0x0017 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			FwMode:1,
			FwCmdStatus:1,
			FwCmd:2,
			FwIntegrity:1,
			Reserved:3;
	} BITS;
} FW_UPDATE_STATUS_Type;     /* 0x0018 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			IfcSenseApStatus:1,
			Reserved:7;
	} BITS;
} FACTORY_STATUS2_Type;     /* 0x0019 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			AlternateMode:8;
	} BITS;
} PD_STATUS3_Type;     /* 0x001A */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			BatteryStatus:1,
			Reserved3:7;
	} BITS;
} PD_STATUS4_Type;     /* 0x001D */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved1:1,
			VbusConsumer:1,
			Reserved2:1,
			VbusCollisionPrevent:1,
			Reserved3:4;
	} BITS;
} VBUS_STATUS_Type;     /* 0x001C */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			PowerMode:5,
			ManualVolSel:2,
			ManualOff:1;
	} BITS;
} PWR_CTRL1_Type;     /* 0x0020 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:8;
	} BITS;
} PWR_CTRL2_Type;     /* 0x0021 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			TrySrcEn:1,
			TrySnkEn:1,
			PortRole:2,
			CcAudioEn:1,
			CcDebugEn:1,
			CcVpdEn:1,
			CcDetEn:1;
	} BITS;
} CC_CTRL1_Type;     /* 0x0022 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcDrpPeriod:2,
			CcDrpPhase:2,
			CcOpen:1,
			SnkVbusMask:1,
			Reserved:2;
	} BITS;
} CC_CTRL2_Type;     /* 0x0023 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:4,
			CcResetSnk:1,
			CcResetSrc:1,
			CcForceDisable:1,
			CcForceErr:1;
	} BITS;
} CC_CTRL3_Type;     /* 0x0024 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			CcExitEn:1,
			CcStLock:1,
			Reserved:1,
			VcSwap:1,
			PrSwap:1,
			DrSwap:1,
			AdvCur:2;
	} BITS;
} CC_CTRL4_Type;     /* 0x0025 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VconnScpEn:1,
			VconnOcpEn:1,
			VconnOcpRange:3,
			VbusDiscEn:1,
			VconnDiscEn:1,
			VconnEn:1;
	} BITS;
} CC_CTRL5_Type;     /* 0x0026 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Cc1OvpEn:1,
			Cc2OvpEn:1,
			CcOvpOvlo:2,
			CcOvpDeb:1,
			Reserved:3;
	} BITS;
} CC_CTRL6_Type;     /* 0x0027 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:2,
			AuxnPdEn:1,
			AuxnPuEn:1,
			AuxpPdEn:1,
			AuxpPuEn:1,
			SbuEn:1,
			SbuSel:1;
	} BITS;
} SBU_CTRL1_Type;     /* 0x0028 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Sbu1OvpEn:1,
			Sbu2OvpEn:1,
			SbuOvpOvlo:2,
			SbuOvpDeb:1,
			Reserved:3;
	} BITS;
} SBU_CTRL2_Type;     /* 0x0029 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Fcy_Id:3,
			Reserved:1,
			FactoryEn:1,
			LeakageTest:1,
			SleepTest:1,
			RidEn:1;
	} BITS;
} FACTORY_CTRL_Type;     /* 0x002A */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			WaterEn:1,
			SbuMeas:1,
			Reserved:1,
			WaterImpedance:5;
	} BITS;
} I2C_WATER_CTRL1_Type;     /* 0x002B */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			HiccupEn:1,
			Reserved:7;
	} BITS;
} I2C_WATER_CTRL2_Type;     /* 0x002C */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			VbusAdcReq:1,
			Reserved:1,
			DcdTmoRetry:1,
			DcdTimer:1,
			DcdTmoEn:1,
			Bc12Manual:1,
			Bc12CcEn:1,
			Bc12En:1;
	} BITS;
} BC_CTRL1_Type;     /* 0x002D */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			UsbSwStatus:1,
			UsbSwCtrl:1,
			UsbSwManual:1,
			DmForce:2,
			DpForce:2,
			DpDmManual:1;
	} BITS;
} BC_CTRL2_Type;     /* 0x002E */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:2,
			UartSwStatus:1,
			UartSwCtrl:1,
			UartSwManual:1,
			JtagSwStatus:1,
			JtagSwCtrl:1,
			JtagSwManual:1;
	} BITS;
} BC_CTRL3_Type;     /* 0x002F */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			DpOvpEn:1,
			DmOvpEn:1,
			DpDmOvpOvlo:2,
			DpDmOvpDeb:1,
			KillerDetEn:1,
			Reserved:2;
	} BITS;
} BC_CTRL4_Type;     /* 0x0030 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			AfcReq:1,
			Reserved:2,
			Result:4,
			AfcEn:1;
	} BITS;
} AFC_CTRL1_Type;     /* 0x0031 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:8;
	} BITS;
} AFC_CTRL2_Type;     /* 0x0032 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			AfcCurrent:4,
			AfcVoltage:4;
	} BITS;
} AFC_CTRL3_Type;     /* 0x0033 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			AfcRxData:8;
	} BITS;
} AFC_RX_DATA_Type;     /* 0x0034 ~ 0x0043 */


typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:3,
			Result:4,
			QcEn:1;
	} BITS;
} QC_CTRL1_Type;     /* 0x0044 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			QcTxData:8;
	} BITS;
} QC_TX_DATA_Type;     /* 0x0045 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			QcRxData:8;
	} BITS;
} QC_RX_DATA_Type;     /* 0x0046 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Sbu1Impedance:5,
			Sbu1Voltage:3;
	} BITS;
} SBU1_STATUS_Type;     /* 0x0047 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Sbu2Impedance:5,
			Sbu2Voltage:3;
	} BITS;
} SBU2_STATUS_Type;     /* 0x0048 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:1,
			I2c2ndControl:1,
			OvpDiscEnControl:1,
			RidControl:1,
			IfcSenseCcControl:1,
			CcDirControl:1,
			JigControl:1,
			ExtVol:1;
	} BITS;
} GPIO_CTRL1_Type;     /* 0x004A */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			RidSet:2,
			IfcSenseCcSet:2,
			CcDirSet:2,
			JigSet:2;
	} BITS;
} GPIO_CTRL2_Type;     /* 0x004B */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:2,
			Sda2Set:2,
			Scl2Set:2,
			OvpDiscEnSet:2;
	} BITS;
} GPIO_CTRL3_Type;     /* 0x004C */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:1,
			Sda2Dir:1,
			Scl2Dir:1,
			OvpDiscEnDir:1,
			RidDir:1,
			IfcSenseCcDir:1,
			CcDirDir:1,
			JigDir:1;
	} BITS;
} GPIO_CTRL4_Type;     /* 0x004D */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved1:1,
			VbusConsumer:1,
			Reserved2:2,
			VbusPort:1,
			Reserved3:3;
	} BITS;
} VBUS_CTRL_Type;	/* 0x004E */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			StartAlt:1,
			ContinueAlt:1,
			ExitAlt:1,
			Reserved:2,
			UpdateUvdm:1,
			UpdateRdo:1,
			PdEn:1;
	} BITS;
} PD_CTRL1_Type;     /* 0x0050 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Select_dp_pin:3,
			Dp_conf_usb:1,
			Reserved:4;
	} BITS;
} PD_CTRL2_Type;     /* 0x0051 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			GetBattStatus:1,
			GetStatus:1,
			Reserved:6;
	} BITS;
} PD_CTRL3_Type;     /* 0x0052 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			Reserved:8;
	} BITS;
} PD_CTRL_RESERV_Type;     /* 0x0051 ~ 0x0055 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			EnterFwUpdateMode:1,
			FwCmd:2,
			FwIntegrityCheck:1,
			SoftReset:1,
			Reserved:3;
	} BITS;
} FW_UPDATE_CTRL1_Type;     /* 0x0056 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			FwDataPacketSize;
	} BITS;
} FW_UPDATE_CTRL2_Type;     /* 0x0057 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			RxData:8;
	} BITS;

	struct {
	uint8_t
			FwData;
	} FW_BITS;
} RX_DATA_Type;     /* 0x0059 ~ 0x007A */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			TxData:8;
	} BITS;
} TX_DATA_Type;     /* 0x0081 ~ 0xA2 */

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			DATA;
	} BITS;
} RID_READ_Type;

typedef union {
	uint8_t BYTE;

	struct {
	uint8_t
			DATA;
	} BITS;
} FW_IMG_SIZE_Type;

typedef union {
	uint8_t DATA[MAX_INT_STATUS_NUM];
	struct {
		BC_INT_Type BC_INT;
		CC_INT_Type CC_INT;
		PD_INT_Type PD_INT;
		FACTORY_INT_Type FACTORY_INT;
		PROTECTION_INT_Type PROTECTION_INT;
		PD_INT2_Type PD_INT2;
	} BYTE;
} INT_STATUS_Type;

typedef enum {
	PS_READY_Nothing,
	PS_READY_RECEIVED,
	PS_READY_SENT,
	PS_READY_Reserved,
} PD_PS_RDY_Type;

typedef enum {
	CC_SINK,
	CC_SOURCE,
} CC_POWER_ROLE_Type;

typedef enum {
	CC_NORMAL,
	CC_OPEN,
} CC_OPEN_Type;

typedef enum {
	PDO_FIX,
	PDO_BAT,
	PDO_VAR,
	PDO_PPS,
} PD_PDO_Type;

typedef enum {
	S2MM006_MAIN,
	S2MM006_MAIN_TEST,
	S2MM006_FACTORY_BOOT,
} S2MM006_FW_ID;

typedef enum {
	S2MM006_SINK,
	S2MM006_SOURCE,
} S2MM006_POWER_ROLE_Type;

typedef enum {
	S2MM006_UFP,
	S2MM006_DFP,
} S2MM006_DATA_ROLE_Type;

typedef enum {
	S2MM006_RP_CURRENT_DEFAULT,
	S2MM006_RP_CURRENT_1_5A,
	S2MM006_RP_CURRENT_3A,
	S2MM006_RP_CURRENT_RESERVED,
	S2MM006_RP_CURRENT_NOATTACH,
} S2MM006_SRC_RP_CURRENT_Type;

typedef enum {
	S2MM006_INACTIVE,
	S2MM006_CC1_ACTIVE,
	S2MM006_CC2_ACTIVE,
} S2MM006_CC_PIN_ACTIVE_Type;

typedef enum {
	S2MM006_RID_OPEN_0,
	S2MM006_RID_1K,
	S2MM006_RID_255K,
	S2MM006_RID_301K,
	S2MM006_RID_523K,
	S2MM006_RID_619K,
	S2MM006_RID_OPEN_6,
	S2MM006_RID_Reserved,
} S2MM006_RID_Type;

typedef enum {
	S2MM006_NOFACTORY,
	S2MM006_CCID,
	S2MM006_RID,
	S2MM006_USBID,
} S2MM006_FACTORY_ID_Type;

typedef enum {
	S2MM006_FACTORY_OPEN,
	S2MM006_FACTORY_1K,
	S2MM006_FACTORY_255K,
	S2MM006_FACTORY_301K,
	S2MM006_FACTORY_523K,
	S2MM006_FACTORY_619K,
	S2MM006_FACTORY_NOCON,
} S2MM006_FACTORY_SET_OPTION;

/* END OF S2MM006 I2C REGISTER MAP */

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
struct ifconn_state_work {
	struct work_struct ifconn_work;
	int dest;
	int id;
	int event;
	void *data;
};
#endif

typedef union {
	uint32_t DATA;
	struct {
		uint32_t Message_Type:4,
			Rsvd2_msg_header:1,
			Port_Data_Role:1,
			Specification_Revision:2,
			Port_Power_Role:1,
			Message_ID:3,
			Number_of_obj:3,
			Rsvd_msg_header:1,
			Reserved:16;
	} BITS;
} MSG_HEADER_Typedef;

typedef union {
	uint32_t DATA;
	uint8_t  BYTES[4];
	struct {
		uint32_t Maximum_Current:10,
			Voltage_Unit:10,
			Peak_Current:2,
			Reserved:3,
			Data_Role_Swap:1,
			USB_Comm_Capable:1,
			Externally_POW:1,
			Higher_Capability:1,
			Dual_Role_Power:1,
			PDO_Parameter:2;
	} BITS;
} SINK_FIXED_SUPPLY_Typedef;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t Operational_Current:10,
		Minimum_Voltage:10,
		Maximum_Voltage:10,
		PDO_Parameter:2;
	} BITS;
} SINK_VAR_SUPPLY_Typedef;

typedef union {
	MSG_HEADER_Typedef MSG_HEADER;
	SINK_FIXED_SUPPLY_Typedef MSG_FIXED_SUPPLY;
	SINK_VAR_SUPPLY_Typedef MSG_VAR_SUPPLY;
} SINK_CAPABILITY_MESSAGE_Typedef;

typedef union {
	uint32_t DATA;
	struct {
		uint32_t Reserved:32;
	} BITS;
} EXT_SRAM_Reserved_Type;


typedef union {
	uint32_t DATA;
	struct {
		uint32_t HW_VERSION_Reserved:32;
	} BITS;
} HW_VERSION_Type;

typedef union {
	uint32_t DATA;
	struct {
		uint32_t SW_VERSION_Reserved:32;
	} BITS;
} SW_VERSION_Type;

typedef union {
	uint32_t DATA;
	struct {
		uint32_t CMD_MODE_SEL:8,
			CMD_HOST:8,
			CMD_WR_DATA1:8,
			CMD_WR_DATA2:8;
	} BITS;
} I2C_SLV_CMD_Type;

typedef union {
	uint32_t DATA;
	struct {
		uint32_t RSP_WORD1:16,
			RSP_WORD2:16;
	} WORDS;
	struct {
		uint32_t RSP_BYTE1:8,
			RSP_BYTE2:8,
			RSP_BYTE3:8,
			RSP_BYTE4:8;
	} BYTES;
} I2C_SLV_RSP_Type;

typedef union {
	uint32_t DATA;
	uint8_t	BYTE[4];
	struct {
		uint32_t PD_State:8,
			RSP_BYTE1:8,
			PD_Next_State:8,
			RSP_BYTE2:8;
	} BYTES;
	struct {
		uint32_t PD_State:8,
			CC1_PLUG_STATE:3,
			RSP_BYTE1:1,
			CC2_PLUG_STATE:3,
			RSP_BYTE2:1,
			PD_Next_State:8,
			ATTACH_DONE:1,
			IS_SOURCE:1,
			IS_DFP:1,
			RP_CurrentLvl:2,
			VBUS_CC_Short:1,
			VBUS_SBU_Short:1,
			RESET:1;
	} BITS;
} FUNC_STATE_Type;

typedef union {
	uint32_t DATA;
	uint8_t	BYTE[4];
	struct {
		uint32_t AUTO_LP_ENABLE_BIT:1,
			LOW_POWER_BIT:1,
			Force_LP_BIT:1,
			WATER_DET:1,
			SW_JIGON:1,
			RUN_DRY:1,
			removing_charge_by_sbu_low:1,
			BOOTING_RUN_DRY:1,
			Sleep_Cable_Detect:1, //b8
			PDSTATE29_SBU_DONE:1, //b9
			RSP_BYTE:22;		//b10 ~ b31
	} BITS;
} LP_STATE_Type;

typedef union {
	uint32_t DATA;
	uint8_t	BYTE[4];
	struct {
		uint32_t Flash_State:8,
			Reserved:24;
	} BITS;
} FLASH_STATE_Type;


typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t SOP2p_Debug:1,
			SOPp_Debug:1,
			SOP2p:1,
			SOPp:1,
			SOP:1,
			RSVD_1:27;
	} BITS;
} SYNC_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t FR_Swap:1,
			Get_Source_Status:1,
			Get_Source_Extended:1,
			Not_Supported:1,
			Soft_Reset:1,
			Wait:1,
			VCONN_Swap:1,
			PR_Swap:1,
			DR_Swap:1,
			Get_Sink_Cap:1,
			Get_Source_Cap:1,
			PS_RDY:1,
			Ping:1,
			Reject:1,
			Accept:1,
			GotoMin:1,
			GoodCRC:1,
			RSVD_1:15;
	} BITS;
} CTRL_MSG_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t Attention:1,
			Exit_Mode:1,
			Enter_Mode:1,
			Discover_Modes:1,
			Discover_SVIDs:1,
			Discover_Identity:1,
			Source_Alert:1,
			Battery_Status:1,
			Vendor_Defined:1,
			BIST_TEST_Data:1,
			BIST_Carrier_Mode2:1,
			Request:1,
			Sink_Capability:1,
			Source_Capability:1,
			RSVD_1:18;
	} BITS;
} DATA_MSG_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t FW_UPDATE_Response:1,
			FW_UPDATA_Request:1,
			Security_Response:1,
			Security_Request:1,
			Manufacturer_Info:1,
			Battery_Capabilities:1,
			Get_Battery_Status:1,
			Get_Battery_Cap:1,
			Source_Status:1,
			Source_Cap_Extended:1,
			RSVD_1:21;
	} BITS;
} EXTENDED_MSG_STATUS_Type;


typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t Ctrl_Flag_Reserved_b0:1,        // b0
			Ctrl_Flag_GoodCRC:1,            // b1
			Ctrl_Flag_GotoMin:1,            // b2
			Ctrl_Flag_Accept:1,             // b3
			Ctrl_Flag_Reject:1,             // b4
			Ctrl_Flag_Ping:1,               // b5
			Ctrl_Flag_PS_RDY:1,             // b6
			Ctrl_Flag_Get_Source_Cap:1,     // b7
			Ctrl_Flag_Get_Sink_Cap:1,       // b8
			Ctrl_Flag_DR_Swap:1,            // b9
			Ctrl_Flag_PR_Swap:1,            // b10
			Ctrl_Flag_VCONN_Swap:1,         // b11
			Ctrl_Flag_Wait:1,               // b12
			Ctrl_Flag_Soft_Reset:1,         // b13
			Ctrl_Flag_Reserved_b14:1,       // b14
			Ctrl_Flag_Reserved_b15:1,       // b15
			Data_Flag_Reserved_b16:1,       // b16
			Data_Flag_SRC_Capability:1,     // b17
			Data_Flag_Request:1,            // b18
			Data_Flag_BIST:1,               // b19
			Data_Flag_SNK_Capability:1,     // b20
			Data_Flag_Reserved_05:1,        // b21
			Data_Flag_Reserved_06:1,        // b22
			Data_Flag_Reserved_07:1,        // b23
			Data_Flag_Reserved_08:1,        // b24
			Data_Flag_Reserved_09:1,        // b25
			Data_Flag_Reserved_10:1,        // b26
			Data_Flag_Reserved_11:1,        // b27
			Data_Flag_Reserved_12:1,        // b28
			Data_Flag_Reserved_13:1,        // b29
			Data_Flag_Reserved_14:1,        // b30
			Data_Flag_Vender_Defined:1;     // b31
	} BITS;
} MSG_IRQ_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
	    uint32_t Vdm_Flag_Reserve_b0:1,		// b0
			Vdm_Flag_Discover_ID:1,		// b1
			Vdm_Flag_Discover_SVIDs:1,	// b2
			Vdm_Flag_Discover_MODEs:1,	// b3
			Vdm_Flag_Enter_Mode:1,		// b4
			Vdm_Flag_Exit_Mode:1,		// b5
			Vdm_Flag_Attention:1,		// b6
			Vdm_Flag_Reserved:9,		// b7 - b15
			Vdm_Flag_DP_Status_Update:1,	// b16
			Vdm_Flag_DP_Configure:1,	// b17
			Vdm_Flag_Reserved2:14;		// b18 - b31
	} BITS;
} VDM_MSG_IRQ_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t AP_Req_Get:2,                   // b0-b1
			UPSM_By_I2C:1,                  // b2
			Reserved:1,                     // b3
			Is_HardReset:1,                 // b4
			FAC_Abnormal_Repeat_State:1,    // b5
			FAC_Abnormal_Repeat_RID:1,      // b6
			FAC_Abnormal_RID0:1,            // b7
			SBU1_CNT:8,                     // b8 - b15
			SBU2_CNT:8,                     // b16 - b23
			SBU_LOW_CNT:4,                  // b24 - b27
			Alt_Mode_By_I2C:2,              // b28 - b29
			AP_Req_Reserved_H:1,            // b30
			Func_Abnormal_State:1;          // b31
	} BITS;
} AP_REQ_GET_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t Ssm_Flag_Reserve_b0:1,		// b0
			Ssm_Flag_Identification:1,		// b1
			Ssm_Flag_RandomNumber:1,		// b2
			Ssm_Flag_Encrypted_Data:1,		// b3
			Ssm_Flag_Unstructured_Data:1,	// b4
			Ssm_Flag_Reserved:26,		// b5 - b30
			Ssm_Flag_AES_Done:1;
	} BITS;
} SSM_MSG_IRQ_STATUS_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t RID:8,
			KID:8,
			ACC_CMD:8,
			Reserved:8;
	} BITS;
} SSM_HW_ID_VALUE_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint32_t Product_ID:16,
			Reserved:16;
	} BITS;
} SSM_HW_PID_VALUE_Type;

typedef union {
	uint32_t DATA;
	uint8_t BYTES[4];
	struct {
		uint8_t Use_CMD1 : 8,
			Use_CMD2 : 8,
			Use_Data1 : 8,
			Use_Data2 : 8;
	} BITS;
} SSM_HW_USE_MSG_Type;

typedef struct {
	HW_VERSION_Type		HW_VERSION_1;		// 0x0000h
	HW_VERSION_Type		HW_VERSION_2;		// 0x0004h
	SW_VERSION_Type		SW_VERSION_1;		// 0x0008h
	SW_VERSION_Type		SW_VERSION_2;		// 0x000Ch
	I2C_SLV_CMD_Type		I2C_SLV_CMD;		// 0x0010h
	I2C_SLV_RSP_Type		I2C_SLV_RSP;		// 0x0014h
	EXT_SRAM_Reserved_Type	Reserved_18h;		// 0x0018h
	EXT_SRAM_Reserved_Type	Reserved_1Ch;		// 0x001Ch
	FUNC_STATE_Type			FUNC_STATE;		// 0x0020h
	FLASH_STATE_Type		FLASH_STATE;		// 0x0024h
	EXT_SRAM_Reserved_Type	Reserved_28h;		// 0x0028h
	EXT_SRAM_Reserved_Type	Reserved_2Ch;		// 0x002Ch
	SYNC_STATUS_Type		SYNC_STATUS;		// 0x0030h
	CTRL_MSG_STATUS_Type	CTRL_MSG_STATUS;	// 0x0034h
	DATA_MSG_STATUS_Type	DATA_MSG_STATUS;	// 0x0038h
	EXTENDED_MSG_STATUS_Type	EXTENDED_MSG_STATUS;	// 0x003Ch
	MSG_IRQ_STATUS_Type		MSG_IRQ_STATUS;		// 0x0040h
	VDM_MSG_IRQ_STATUS_Type	VDM_MSG_IRQ_STATUS;	// 0x0044h
	SSM_MSG_IRQ_STATUS_Type	SSM_MSG_IRQ_STATUS;	// 0x0048h
	AP_REQ_GET_STATUS_Type	AP_REQ_GET_STATUS;      // 0x004Ch
	SSM_HW_ID_VALUE_Type	SSM_HW_ID_VALUE;	// 0x0050h
	SSM_HW_PID_VALUE_Type	SSM_HW_PID_VALUE;	// 0x0054h
	SSM_HW_USE_MSG_Type		SSM_HW_USE_MSG;		// 0x0058h
} EXT_SRAM_Type;

typedef struct {
	uint32_t SW_Version;
	uint32_t SW_Start_Addr;
	uint32_t SW_End_Addr;
	uint32_t SW_CRC_VALUE;
} SW_IMAGE_HEADER_Type;

////////////////////////////////////////////////////////////////////////////////////////////
/*********************************** SAMSUNG Mobile Add ************************************/
////////////////////////////////////////////////////////////////////////////////////////////

typedef enum {
	// PDO Message
	MSG_Idx_TX_SRC_CAPA             = 1,
	MSG_Idx_TX_SINK_CAPA            = 2,
	MSG_Idx_TX_REQUEST              = 3,

	MSG_Idx_RX_SRC_CAPA             = 4,
	MSG_Idx_RX_SINK_CAPA            = 5,
	MSG_Idx_RX_REQUEST              = 6,

	// VDM User Message
	MSG_Idx_VDM_MSG_REQUEST         = 9,
	MSG_Idx_CTRL_MSG                = 10,

	// VDM TX Message
	MSG_Idx_TX_DISC_ID_RESP         = 17,
	MSG_Idx_TX_DISC_SVIDs_RESP      = 18,
	MSG_Idx_TX_DISC_MODE_RESP       = 19,
	MSG_Idx_TX_DISC_ENTER_MODE_RESP = 20,
	MSG_Idx_TX_DISC_EXIT_MODE_RESP  = 21,
	MSG_Idx_TX_DISC_ATTENTION_RESP  = 22,

	// VDM RX Message
	MSG_Idx_RX_DISC_ID_CABLE        = 25,
	MSG_Idx_RX_DISC_ID_RESP         = 26,
	MSG_Idx_RX_DISC_SVIDs_RESP      = 27,
	MSG_Idx_RX_DISC_MODE_RESP       = 28,
	MSG_Idx_RX_DISC_ENTER_MODE_RESP = 29,
	MSG_Idx_RX_DISC_EXIT_MODE_RESP  = 30,
	MSG_Idx_RX_DISC_ATTENTION_RESP  = 31

} Num_MSG_INDEX;

typedef struct {
	uint16_t Message_Type:4;
	uint16_t Rsvd2_msg_header:1;
	uint16_t Port_Data_Role:1;
	uint16_t Specification_Revision:2;
	uint16_t Port_Power_Role:1;
	uint16_t Message_ID:3;
	uint16_t Number_of_obj:3;
	uint16_t Rsvd_msg_header:1;
} MSG_HEADER_Type;

typedef struct {
	uint32_t Maximum_Allow_Power:10;
	uint32_t Minimum_Voltage:10;
	uint32_t Maximum_Voltage:10;
	uint32_t PDO_Parameter:2;
} SRC_BAT_SUPPLY_Typedef;

// ================== Capabilities Message ==============
// Source Capabilities

typedef struct {
	uint32_t Maximum_Current:7;
	uint32_t Reserved:1;
	uint32_t Minimum_Voltage:8;
	uint32_t Reserved2:1;
	uint32_t Maximum_Voltage:8;
	uint32_t Reserved3:2;
	uint32_t PPS_Power_Limit:1;
	uint32_t Reserved4:2;
	uint32_t PDO_Parameter:2;
} SRC_PPS_SUPPLY_Typedef;

typedef struct {
	uint32_t Maximum_Current:10;
	uint32_t Voltage_Unit:10;
	uint32_t Peak_Current:2;
	uint32_t Reserved:3;
	uint32_t Data_Role_Swap:1;
	uint32_t USB_Comm_Capable:1;
	uint32_t Externally_POW:1;
	uint32_t USB_Suspend_Support:1;
	uint32_t Dual_Role_Power:1;
	uint32_t PDO_Parameter:2;
} SRC_FIXED_SUPPLY_Typedef;

typedef struct {
	uint32_t Maximum_Current:10;
	uint32_t Minimum_Voltage:10;
	uint32_t Maximum_Voltage:10;
	uint32_t PDO_Parameter:2;
} SRC_VAR_SUPPLY_Typedef;

typedef struct {
	uint32_t Maximum_Operating_Current:10;
	uint32_t Operating_Current:10;
	uint32_t Reserved1:3;
	uint32_t Unchunked_Extended_Messaged_Supported:1;
	uint32_t No_USB_Suspend:1;
	uint32_t USB_Communications_Capable:1;
	uint32_t Capacity_MisMatch:1;
	uint32_t GiveBack_Flag:1;
	uint32_t Object_Position:3;
	uint32_t Reserved2:1;
} REQUEST_FIXED;

typedef struct {
	uint32_t Operating_Current:7;
	uint32_t Reserved1:2;
	uint32_t Output_Voltage:11;
	uint32_t Reserved2:3;
	uint32_t Unchunked_Extended_Messaged_Supported:1;
	uint32_t No_USB_Suspend:1;
	uint32_t USB_Communications_Capable:1;
	uint32_t Capacity_MisMatch:1;
	uint32_t Reserved3:1;
	uint32_t Object_Position:3;
	uint32_t Reserved4:1;
} REQUEST_PPS;

typedef union {
	uint8_t BYTE[8];
	uint32_t DATA[2];
	struct {
		MSG_HEADER_Type Header;
		union {
			REQUEST_FIXED FIXED;
			REQUEST_PPS PPS;
		} PDO;
	} MSG;
} RDO_Type;

typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Maximum_Current:10,
			Voltage_Unit:10,
			Peak_Current:2,
			Reserved:3,
			Data_Role_Swap:1,
			USB_Comm_Capable:1,
			Externally_POW:1,
			USB_Suspend_Support:1,
			Dual_Role_Power:1,
			PDO_Parameter:2;
	} BITS;
} U_SRC_FIXED_SUPPLY_Typedef;


typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Maximum_Current:10,
			Minimum_Voltage:10,
			Maximum_Voltage:10,
			PDO_Parameter:2;
	} BITS;
} U_SRC_VAR_SUPPLY_Typedef;


typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Maximum_Allow_Power:10,
			Minimum_Voltage:10,
			Maximum_Voltage:10,
			PDO_Parameter:2;
	} BITS;
} U_SRC_BAT_SUPPLY_Typedef;


    // Sink Capabilities
typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Maximum_Current:10,
			Voltage_Unit:10,
			Peak_Current:2,
			Reserved:3,
			Data_Role_Swap:1,
			USB_Comm_Capable:1,
			Externally_POW:1,
			Higher_Capability:1,
			Dual_Role_Power:1,
			PDO_Parameter:2;
	} BITS;
} U_SINK_FIXED_SUPPLY_Typedef;


typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Operational_Current:10,
			Minimum_Voltage:10,
			Maximum_Voltage:10,
			PDO_Parameter:2;
	} BITS;
} U_SINK_VAR_SUPPLY_Typedef;


typedef union {
	uint16_t DATA;

	struct {
		uint8_t BDATA[2];
	} BYTES;

	struct {
		uint32_t Operational_Power:10,
			Minimum_Voltage:10,
			Maximum_Voltage:10,
			PDO_Parameter:2;
	} BITS;
} U_SINK_BAT_SUPPLY_Typedef;

// ================== Request Message ================

typedef struct {
	uint32_t Maximum_OP_Current:10;  // 10mA
	uint32_t OP_Current:10;          // 10mA
	uint32_t Reserved_1:4;           // Set to Zero
	uint32_t No_USB_Suspend:1;
	uint32_t USB_Comm_Capable:1;
	uint32_t Capa_Mismatch:1;
	uint32_t GiveBack_Flag:1;        // GiveBack Support set to 1
	uint32_t Object_Position:3;      // 000 is reserved
	uint32_t Reserved_2:1;
} REQUEST_FIXED_SUPPLY_STRUCT_Typedef;

typedef union {
	uint32_t DATA;

	struct {
		uint8_t BDATA[4];
	} BYTES;

	struct {
		uint32_t Minimum_OP_Current:10,  // 10mA
			OP_Current:10,          // 10mA
			Reserved_1:4,           // Set to Zero
			No_USB_Suspend:1,
			USB_Comm_Capable:1,
			Capa_Mismatch:1,
			GiveBack_Flag:1,        // GiveBack Support set to 1
			Object_Position:3,      // 000 is reserved
			Reserved_2:1;
	} BITS;
} U_REQUEST_FIXED_SUPPLY_STRUCT_Typedef;

// Function Status (0x20)
typedef enum {
	State_PE_Initial_detach                     = 0,

	// Source Port
	State_PE_SRC_Startup                        = 1,
	State_PE_SRC_Send_Capabilities              = 2,
	State_PE_SRC_Negotiate_Capability           = 3,
	State_PE_SRC_Transition_Supply              = 4,
	State_PE_SRC_Ready                          = 5,
	State_PE_SRC_Disabled                       = 6,
	State_PE_SRC_Hard_Reset                     = 7,
	State_PE_SRC_Hard_Reset_Received            = 8,
	State_PE_SRC_Transition_to_default          = 9,
	State_PE_SRC_Give_Source_Cap                = 10,
	State_PE_SRC_Get_Sink_Cap                   = 11,
	State_PE_SRC_Send_Soft_Reset                = 12,
	State_PE_SRC_Soft_Reset                     = 13,
	//Source Startup Structured VDM Discover Identity
	State_PE_SRC_CABLE_VDM_Identity_Request	    = 14,
	State_PE_Reserved_fn_15                     = 15,

	// Sink Port
	State_PE_SNK_Startup                        = 16,
	State_PE_SNK_Discovery                      = 17,
	State_PE_SNK_Wait_for_Capabilities          = 18,
	State_PE_SNK_Evaluate_Capability            = 19,
	State_PE_SNK_Select_Capability              = 20,
	State_PE_SNK_Transition_Sink                = 21,
	State_PE_SNK_Ready                          = 22,
	State_PE_SNK_Hard_Reset                     = 23,
	State_PE_SNK_Transition_to_default          = 24,
	State_PE_SNK_Give_Sink_Cap                  = 25,
	State_PE_SNK_Send_Soft_Reset                = 26,
	State_PE_SNK_Soft_Reset                     = 27,
	State_ErrorRecovery                         = 28,

	State_PE_Reserved_fn_29                     = 29,
	State_PE_Wait_Detach                        = 30,
	State_PE_Reserved_fn_31                     = 31,


	//Type-C DFP to UFP Data Role Swap
	State_PE_DRS_DFP_UFP_Evaluate_DR_Swap       = 32,
	State_PE_DRS_DFP_UFP_Accept_DR_Swap         = 33,
	State_PE_DRS_DFP_UFP_Change_to_UFP          = 34,
	State_PE_DRS_DFP_UFP_Send_DR_Swap           = 35,
	State_PE_DRS_DFP_UFP_Reject_DR_Swap         = 36,

	//Type-C UFP to DFP Data Role Swap
	State_PE_DRS_UFP_DFP_Evaluate_DR_Swap       = 37,
	State_PE_DRS_UFP_DFP_Accept_DR_Swap         = 38,
	State_PE_DRS_UFP_DFP_Change_to_DFP          = 39,
	State_PE_DRS_UFP_DFP_Send_DR_Swap           = 40,
	State_PE_DRS_UFP_DFP_Reject_DR_Swap         = 41,

	//Type-C VCONN Swap
	State_PE_VCS_Send_Swap                      = 42,
	State_PE_VCS_Evaluate_Swap                  = 43,
	State_PE_VCS_Reject_Swap                    = 44,
	State_PE_VCS_Accept_Swap                    = 45,
	State_PE_VCS_Wait_For_VCONN                 = 46,
	State_PE_VCS_Turn_Off_VCONN                 = 47,
	State_PE_VCS_Turn_On_VCONN                  = 48,
	State_PE_VCS_Send_Ps_Rdy                    = 49,

	State_PE_Reserved_fn_50                     = 50,

	/* Power Role Swap */
	//Source to Sink Power Role Swap
	State_PE_PRS_SRC_SNK_Evaluate_PR_Swap       = 51,
	State_PE_PRS_SRC_SNK_Reject_PR_Swap         = 52,
	State_PE_PRS_SRC_SNK_Accept_PR_Swap         = 53,
	State_PE_PRS_SRC_SNK_Transition_to_off      = 54,
	State_PE_PRS_SRC_SNK_Assert_Rd              = 55,
	State_PE_PRS_SRC_SNK_Wait_Source_on         = 56,
	State_PE_PRS_SRC_SNK_Send_PR_Swap           = 57,

	//Sink to Source Power Role Swap
	State_PE_PRS_SNK_SRC_Evaluate_PR_Swap       = 58,
	State_PE_PRS_SNK_SRC_Reject_PR_Swap         = 59,

	State_PE_PRS_SNK_SRC_Accept_PR_Swap         = 60,
	State_PE_PRS_SNK_SRC_Transition_to_off      = 61,
	State_PE_PRS_SNK_SRC_Assert_Rp              = 62,
	State_PE_PRS_SNK_SRC_Source_on              = 63,
	State_PE_PRS_SNK_SRC_Send_PR_Swap           = 64,

	/* Alternate Mode */
	State_PE_DFP_UFP_VDM_Identity_Request       = 65,
	State_PE_DFP_VDM_SVIDs_Request              = 66,
	State_PE_DFP_VDM_Modes_Request              = 67,
	State_PE_DFP_VDM_Mode_Enter_Request         = 68,
	State_PE_DFP_VDM_Mode_Exit_Request          = 69,
	State_PE_DFP_VDM_Attention_Receive          = 70,
	State_PE_UFP_VDM_Attention_Request          = 71,
	State_PE_DFP_VDM_DP_Status_Update_Request   = 72,
	State_PE_DFP_VDM_DP_Configure_Request       = 73,
	State_PE_DFP_VDM_Send_NAK                   = 74,

	/* UnStructured VDM */
	State_PE_Unstructured_VDM_DATA_Request      = 75,
	State_PE_Unstructured_VDM_DATA_Receive      = 76,

	State_PE_SimpleVPD                          = 77,
	State_PE_CTUnAttachedSNK                    = 78,
	State_PE_Reserved_fn_79                     = 79,

	/* Not Supported Message */
	State_PE_Send_Not_Supported                 = 80,
	State_PE_Not_Supported_Received             = 81,
	State_PE_Chunk_Received                     = 82,
	State_PE_SNK_Send_Not_Supported             = 83,
	State_PE_SNK_Not_Supported_Received         = 84,

	State_PE_Reserved_fn_85                     = 85,

	/* BIST */
	State_PE_BIST_Transmit_Mode                 = 86,
	State_PE_BIST_Carrier_Mode_2                = 87,
	State_PE_BIST_TEST_DATA_MODE                = 88,

	State_PE_Reserved_fn_89                     = 89,

	State_PE_ErrorRecovery                      = 90,
	State_PE_AudioAccessory                     = 91,
	State_PE_TRYWAIT_SNK                        = 92,

	//Dual-Role Source Port Get Source Capabilities
	State_PE_DR_SRC_Get_Source_Cap              = 93,
	//Dual-Role Source Port Give Sink Capabilities
	State_PE_DR_SRC_Give_Sink_Cap               = 94,
	//Dual-Role Sink Port Get Sink Capabilities
	State_PE_DR_SNK_Get_Sink_Cap                = 95,
	//Dual-Role Sink Port Give Source Capabilities
	State_PE_DR_SNK_Give_Source_Cap             = 96,

	State_PE_Reserved_fn_103                    = 97,
	State_PE_Reserved_fn_104                    = 98,

	State_PE_Reserved_fn_105                    = 99,

	/* >>>>>>>>>>> USB PD R3.0 V1.1 Spec */
	/* Alert State */
	State_PE_SRC_Send_Source_Alert	            = 100,
	State_PE_SNK_Source_Alert_Received          = 101,
	State_PE_SNK_Send_Sink_Alert                = 102,
	State_PE_SRC_Sink_Alert_Received            = 103,

	/* Source Capabilities Extended State */
	State_PE_SNK_Get_Source_Cap_Ext             = 104,
	State_PE_SRC_Give_Source_Cap_Ext            = 105,

	/* DR Source Request Capabilities Extended State */
	State_PE_DR_SRC_Get_Source_Cap_Ext          = 106,
	State_PE_DR_SNK_Give_Source_Cap_Ext         = 107,

	/* Status State */
	State_PE_SNK_Get_Source_Status              = 108,
	State_PE_SRC_Give_Source_Status             = 109,
	State_PE_SRC_Get_Sink_Status                = 110,
	State_PE_SNK_Give_Sink_Status               = 111,

	/* PPS Status State */
	State_PE_SNK_Get_PPS_Status                 = 112,
	State_PE_SRC_Give_PPS_Status                = 113,

	/* Battery Capabilities State */
	State_PE_Get_Battery_Cap                    = 114,
	State_PE_Give_Battery_Cap                   = 115,

	/* Battery Status State */
	State_PE_Get_Battery_Status                 = 116,
	State_PE_Give_Battery_Status                = 117,

	/* Manufacturer information State */
	State_PE_Get_Manufacturer_Info              = 118,
	State_PE_Give_Manufacturer_Info             = 119,

	/* Country Code & Information State */
	State_PE_Get_Country_Code                   = 120,
	State_PE_Give_Country_Code                  = 121,
	State_PE_Get_Country_Info                   = 122,
	State_PE_Give_Country_Info                  = 123,
} function_status_t;

#define HOST_ON_BY_RD	1
#define HOST_ON_BY_RID000K	2

typedef enum {
	Rp_Sbu_check = 0,
	Rp_56K = 1,	/* 80uA */
	Rp_22K = 2,	/* 180uA */
	Rp_10K = 3,	/* 330uA */
	Rp_Abnormal = 4,
} PDIC_RP_CurrentLvl;

#define S2MM006_REG_MASK(reg, mask)	((reg & mask##_MASK) >> mask##_SHIFT)

#define BUF1_ADDR(addr)	(addr - S2MM006_REG_BC_STATUS1)		/* for s2mm006_get_int_status */
#define BUF2_ADDR(addr)	(addr - S2MM006_REG_FW_PD_STATE0)	/* for s2mm006_get_int_status */

struct s2mm006_data {
	struct device *dev;
	const char *name;
	const char *main_fw_name;
	struct i2c_client *i2c;
	struct wakeup_source *pdic_ws;
	struct delayed_work irq_work;
	wait_queue_head_t device_add_wait_q;
	struct mutex i2c_mutex;
	struct mutex irq_mutex;
	struct mutex alt_info_lock;
	int irq;
	int irq_gpio;
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	int irq_vbus;
	int irq_vbus_gpio;
#endif

#if defined(CONFIG_USE_DUAL_FW)
	int fw_version;
#endif
	int redriver_en;
	int s2mm006_om;
	int vbus_discharging;
	int water_detect_support;
	int support_pd;
	int otg_sw;
	int otg_buck_en;

	INT_STATUS_Type INT_STATUS;
	u8 firm_ver;
	int pd_state;
	int PowerRole;
	int DataRole;
	int SrcCur;
	int is_short;
	int cc_status;
	int cc_pin_active;
	int sbu_status;
	int cur_rid;
	int water_det;
	bool pd_contract;
	bool fcy_wakeup;

	bool suspended;
	bool keep_irq;
	int s2mm006_i2c_err;
	int pdic_check_at_booting;
	int wq_times;
	int usb_boot_mode;
	int set_en_alt_mode;
	int last_battery_status;

	int manual_lpm_mode;

	struct pdic_alt_info alt_info;

#if IS_ENABLED(CONFIG_SEC_PD)
	struct pdic_notifier_struct pd_noti;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	void *pp_data;
	struct pp_ic_data *ic_data;
	ppdic_data_t ppdic_data;
#endif
#if IS_ENABLED(CONFIG_S2MM006_POGO)
	int port_role;

	struct power_supply_desc pdic_desc;
	struct power_supply *psy_pdic;
#endif

	struct s2mm006_muic_data *muic_data;
};
#endif /* __S2MM006_H */
