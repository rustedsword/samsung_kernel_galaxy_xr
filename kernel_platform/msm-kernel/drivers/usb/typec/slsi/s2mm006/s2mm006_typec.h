/* SPDX-License-Identifier: GPL-2.0 */
/*
 * s2mm006_typec.h - S2MM006 typec function header
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
#ifndef __S2MM006_TYPEC_H__
#define __S2MM006_TYPEC_H__

typedef enum {
	S2MM006_PORT_ROLE_SINK,
	S2MM006_PORT_ROLE_SOURCE,
	S2MM006_PORT_ROLE_DRP_0uA,
	S2MM006_PORT_ROLE_DRP,
	S2MM006_PORT_ROLE_DRP_TRY_SNK,
} S2MM006_PORT_ROLE_Type;

typedef enum {
	PD_DR_SWAP,
	PD_PR_SWAP,
	PD_VCONN_SWAP,
} S2MM006_SWAP_Type;

#endif /* __S2MM006_TYPEC_H__ */
