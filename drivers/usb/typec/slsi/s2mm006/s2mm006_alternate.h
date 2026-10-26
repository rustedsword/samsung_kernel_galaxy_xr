/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 *	- S2MM006 USB CCIC Alternate mode header file
 *
 * Copyright (C) 2021 Samsung Electronics
 *
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
#ifndef __S2MM006_ALTERNATE_H__
#define __S2MM006_ALTERNATE_H__

#define ALTERNATE_MODE_NOT_READY	(1 << 0)
#define ALTERNATE_MODE_READY		(1 << 1)
#define ALTERNATE_MODE_STOP			(1 << 2)
#define ALTERNATE_MODE_START		(1 << 3)
#define ALTERNATE_MODE_RESET		(1 << 4)

#define S2MM006_SVID_VDO_MAX	6

/* VMD Message Register I2C address by S.LSI */
#define REG_VDM_MSG_REQ			0x02C0
#define REG_SSM_MSG_READ		0x0340
#define REG_SSM_MSG_SEND		0x0360

#define REG_TX_DIS_ID_RESPONSE		0x0400
#define REG_TX_DIS_SVID_RESPONSE	0x0420
#define REG_TX_DIS_MODE_RESPONSE	0x0440
#define REG_TX_ENTER_MODE_RESPONSE	0x0460
#define REG_TX_EXIT_MODE_RESPONSE	0x0480
#define REG_TX_DIS_ATTENTION_RESPONSE	0x04A0

#define REG_RX_DIS_ID_CABLE		0x0500
#define REG_RX_DIS_ID			0x0520
#define REG_RX_DIS_SVID			0x0540
#define REG_RX_MODE			0x0560
#define REG_RX_ENTER_MODE		0x0580
#define REG_RX_EXIT_MODE		0x05A0
#define REG_RX_DIS_ATTENTION		0x05C0
#define REG_RX_DIS_DP_STATUS_UPDATE	0x0600
#define REG_RX_DIS_DP_CONFIGURE		0x0620

#define S2MM006_SBU_CTRL1_SBU_SEL		(1 << 7)
#define S2MM006_SBU_CTRL1_SBU_EN		(1 << 6)
#define S2MM006_SBU_CTRL1_AUXP_PU_EN	(1 << 5)
#define S2MM006_SBU_CTRL1_AUXP_PD_EN	(1 << 4)
#define S2MM006_SBU_CTRL1_AUXN_PU_EN	(1 << 3)
#define S2MM006_SBU_CTRL1_AUXN_PD_EN	(1 << 2)

#define S2MM006_PD_CTRL1_START		(1 << 0)
#define S2MM006_PD_CTRL1_CONTINUE	(1 << 1)
#define S2MM006_PD_CTRL1_EXIT		(1 << 2)

#define S2MM006_PD_STATUS_DISCOVER_ID		(1 << 0)
#define S2MM006_PD_STATUS_DISCOVER_SVID		(1 << 1)
#define S2MM006_PD_STATUS_DISCOVER_MODE		(1 << 2)
#define S2MM006_PD_STATUS_ENTER_MODE		(1 << 3)
#define S2MM006_PD_STATUS_EXIT_MODE			(1 << 4)
#define S2MM006_PD_STATUS_DP_STATUS			(1 << 5)
#define S2MM006_PD_STATUS_DP_CONFIGURE		(1 << 6)
#define S2MM006_PD_STATUS_DP_ATTENTION		(1 << 7)

typedef enum {
	S2MM006_DP_STATUS = 0x1,
	S2MM006_DP_CONFIGURE,
	S2MM006_DP_ATTENTION,
} S2MM006_DISPLAYPORT_STATE;

typedef enum {
	S2MM006_PD_CTRL2_NO_SELETION	= 0x0,
	S2MM006_PD_CTRL2_PIN_A	= 0x1,
	S2MM006_PD_CTRL2_PIN_C	= 0x2,
	S2MM006_PD_CTRL2_PIN_D	= 0x3,
	S2MM006_PD_CTRL2_PIN_E	= 0x4,
	S2MM006_PD_CTRL2_PIN_B	= 0x5,
	S2MM006_PD_CTRL2_PIN_F	= 0x6,
} S2MM006_PD_CTRL2_SELECT_DP_PIN;

#endif /* __S2MM006_ALTERNATE_H__ */
