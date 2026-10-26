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

#include "kxr_aphost.h"

int kxr_spi_xchg_read_response(kxr_aphost_t *aphost, char *buff, int size)
{
	kxr_spi_xchg_header_t *header = &aphost->xchg.header;
	int times = 20;
	int length;

	while (header->ack == 0) {
		if (times < 1) {
			return scnprintf(buff, PAGE_SIZE, "no need to ack\n");
		}

		msleep(10);
		times--;
	}

	header->ack = 0;

	switch (header->key) {
	case getMasterNordicVersionRequest:
		length = scnprintf(buff, PAGE_SIZE, "masterNordic fwVersion:%d.%d\n", header->args[1], header->args[0]);
		break;

	case bondJoyStickRequest:
	case disconnectJoyStickRequest:
	case setVibStateRequest:
	case hostEnterDfuStateRequest:
		length = scnprintf(buff, PAGE_SIZE, "requestType:%d ack:%d\n", header->key, header->ack);
		break;

	case getJoyStickBondStateRequest:
		length = scnprintf(buff, PAGE_SIZE, "left/right joyStick bond state:%d:%d\n", header->args[0] & 1, (header->args[0] >> 1) & 1);
		break;

	case getLeftJoyStickProductNameRequest:
		length = scnprintf(buff, PAGE_SIZE, "leftJoyStick productNameID:%d\n", header->args[0]);
		break;

	case getRightJoyStickProductNameRequest:
		length = scnprintf(buff, PAGE_SIZE, "rightJoyStick productNameID:%d\n", header->args[0]);
		break;

	case getLeftJoyStickFwVersionRequest:
		length = scnprintf(buff, PAGE_SIZE, "leftJoyStick fwVersion:%d.%d\n", header->args[1], header->args[0]);
		break;

	case getRightJoyStickFwVersionRequest:
		length = scnprintf(buff, PAGE_SIZE, "rightJoyStick fwVersion:%d.%d\n", header->args[1], header->args[0]);
		break;

	case getBleMacAddr:
		length = scnprintf(buff, PAGE_SIZE, "read ble addr:%2x.%2x.%2x\n", header->args[2], header->args[1], header->args[0]);
		break;

	default:
		length = scnprintf(buff, PAGE_SIZE, "invalid requestType: %d\n", header->key);
		break;
	}

	return length;
}

void kxr_spi_xchg_write_command(kxr_aphost_t *aphost, u32 command)
{
	kxr_spi_xchg_t *xchg = &aphost->xchg;
	kxr_spi_xchg_req_t *req = &xchg->req;
	kxr_spi_xchg_header_t *header = &req->header;

	header->key_ack = command >> 24;
	header->value = command >> 8;
	req->type = KXR_SPI_XCHG_TAG;
	xchg->header.ack = 0;

	if (kxr_spi_xfer_post_xchg(&aphost->xfer) && header->key == setPowerStateRequest) {
		kxr_aphost_power_mode_set(aphost, (kxr_spi_power_mode_t) req->header.args[0]);
	}
}

int kxr_spi_xchg_show_command(kxr_aphost_t *aphost, u32 command, char *buff, int size)
{
	kxr_spi_xchg_write_command(aphost, command);
	return kxr_spi_xchg_read_response(aphost, buff, size);
}

void kxr_spi_xchg_clear(kxr_spi_xchg_t *xchg)
{
	xchg->req.type = 0x00;
}

bool kxr_spi_xchg_sync(kxr_aphost_t *aphost)
{
	kxr_spi_xchg_t *xchg = &aphost->xchg;
	kxr_spi_xchg_req_t *req = &xchg->req;
	kxr_spi_xchg_rsp_t *rsp = &xchg->rsp;
	int ret;

	ret = kxr_spi_xfer_sync(&aphost->xfer, req, rsp, KXR_SPI_XCHG_SIZE);
	if (ret < 0) {
		return false;
	}

	if (rsp->header.ack) {
		memcpy(&xchg->header, &rsp->header, sizeof(kxr_spi_xchg_header_t));
	}

	req->type = 0x00;

	return false;
}

static ssize_t jsrequest_show(struct device *dev, struct device_attribute *attr, char *buff)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	return kxr_spi_xchg_read_response(aphost, buff, PAGE_SIZE);
}

static ssize_t jsrequest_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t size)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buff, 16, &value);
	if (ret < 0) {
		dev_err(dev, "Invalid jsrequest: %s\n", buff);
		return ret;
	}

	kxr_spi_xchg_write_command(aphost, value);

	return size;
}

static DEVICE_ATTR_RW(jsrequest);

int kxr_spi_xchg_probe(kxr_aphost_t *aphost)
{
	device_create_file(&aphost->xfer.spi->dev, &dev_attr_jsrequest);
	return 0;
}

void kxr_spi_xchg_remove(kxr_aphost_t *aphost)
{
	device_remove_file(&aphost->xfer.spi->dev, &dev_attr_jsrequest);
}

