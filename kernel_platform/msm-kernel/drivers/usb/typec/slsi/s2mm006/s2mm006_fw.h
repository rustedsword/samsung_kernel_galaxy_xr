/* SPDX-License-Identifier: GPL-2.0 */
/*
 * s2mm006_fw.h - S2MM006 fw function header
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

#ifndef _S2MM006_FW_H
#define _S2MM006_FW_H

#define PDIC_DEFAULT_UMS_FW                 "/sdcard/Firmware/usbpd/s2mm006.bin"

enum {
	FLASH_MODE_ENTER,
	FLASH_ERASE,
	FLASH_WRITE,
	FLASH_WRITE3,
	FLASH_WRITE4,
	FLASH_WRITE5,
	FLASH_WRITE6,
	FLASH_WRITE7,
	FLASH_WRITE8,
	FLASH_WRITE_BUILTIN,
	FLASH_WRITE_UMS,
	FLASH_MODE_EXIT,
};

enum {
	FLASH_MODE_NORMAL = 0x00,
	FLASH_MODE_FLASH = 0x80,
	FLASH_MODE_ERASE = 0x81,
};

enum {
	FLASH_FW_VER_MATCH = 0x00,
	FLASH_FW_VER_BOOT = 0x01,
	FLASH_FW_VER_MAIN = 0x02,

	/* ERROR FOR S2MM006 FLASH WRITE */
	EFLASH_VERIFY = 0x10,
	EFLASH_WRITE_SWVERSION,
	EFLASH_WRITE_SIZE,
	EFLASH_WRITE_CRC,
	EFLASH_WRITE_DONE,
};

enum {
	PRODUCT_NUM_GRACE = 0x00,
	PRODUCT_NUM_DREAM = 0x01,
	PRODUCT_NUM_GREAT = 0x0A,
	PRODUCT_NUM_VIEW2 = 0x10,
};

enum {
	FW_BUILT_IN = 0,
	FW_ADB,
	FW_SPU,
};

struct s2mm006_version {
	u8 main[3];
	u8 boot;
	u8 ver2[4];
};

struct s2mm006_fw {
	u8 boot;
	u8 main[3];
	unsigned char reserve[4];
	unsigned int size;
};

#define CMD_MODE_0x10 0x10
#define CMD_HOST_0x11	0x11
#define FLASH_STATUS_0x24 0x24

#define FLASH_MODE_ENTER_0x10	0x10
#define FLASH_WRITE_0x42	0x42
#define FLASH_ERASE_0x44	0x44
#define FLASH_MODE_EXIT_0x20	0x20

#define FLASH_WRITING_BYTE_SIZE_0x4 0x4

#endif
