#pragma once

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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define MTU_SIZE		62 //as defined in serial bootloader
#define MAX_ACTUAL_PAYLOAD	(MTU_SIZE - 2) //(MTU_SIZE/2-2)
#define MAX_FRAME_SIZE 	4096

#define COMMAND_OBJECT	0x01 //For init packet
#define DATA_OBJECT		0x02 // For actual image packet

struct js_spi_info {
	uint8_t init_packet_path[100];
	uint8_t app_image_path[100];
	uint8_t version[100];
};

typedef struct {
	struct class *js_class;
	struct cdev js_cdev;
	dev_t js_devno;

	unsigned char txbuffer[255];
	unsigned char rxbuffer[255];

	uint8_t payload[MTU_SIZE];
	unsigned long delay_us;
	uint32_t firmware_crc_last;
	uint32_t dfu_state;

	struct js_spi_info info;
	char dat_path[1024];
	char bin_path[1024];
} kxr_spi_dfu_t;
