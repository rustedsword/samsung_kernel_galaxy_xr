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

#define KXR_SPI_XCHG_SIZE				190
#define KXR_SPI_XCHG_TAG				0xA8

typedef enum {
	getMasterNordicVersionRequest = 1,
	setVibStateRequest,
	bondJoyStickRequest,
	disconnectJoyStickRequest,
	getJoyStickBondStateRequest,
	hostEnterDfuStateRequest,
	getLeftJoyStickProductNameRequest,
	getRightJoyStickProductNameRequest,
	getLeftJoyStickFwVersionRequest,
	getRightJoyStickFwVersionRequest,
	setPowerStateRequest,
	setBleMacAddr,//12
	getBleMacAddr,
	setBleAdvMode,//14
	invalidRequest,
} kxr_spi_xchg_cmd_t;

#pragma pack(1)

typedef union {
	struct {
		u32 key		:7;
		u32 ack		:1;
		u32 value	:24;
	};

	struct {
		u8 key_ack;
		u8 args[3];
	};
} kxr_spi_xchg_header_t;

typedef struct {
	u32 time;
	u8 buff[26];
} kxr_spi_xchg_node_t;

typedef union {
	struct {
		u8 type;
		kxr_spi_xchg_header_t header;
	};

	u8 buff[KXR_SPI_XCHG_SIZE];
} kxr_spi_xchg_req_t;

typedef struct {
	union {
		u32 header_value;
		kxr_spi_xchg_header_t header;
	};

	u8 node_size;
	u8 node_count;
	u32 time;
	kxr_spi_xchg_node_t nodes[6];
} kxr_spi_xchg_rsp_t;

#pragma pack()

typedef struct {
	kxr_spi_xchg_req_t req;
	kxr_spi_xchg_rsp_t rsp;
	kxr_spi_xchg_header_t header;
} kxr_spi_xchg_t;

void kxr_spi_xchg_clear(kxr_spi_xchg_t *xchg);

