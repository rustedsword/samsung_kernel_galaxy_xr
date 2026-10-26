#pragma once

/*
 * SPI controller driver for KXR firmware
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "kxr_spi_xfer.h"
#include "kxr_spi_xchg.h"
#include "kxr_spi_uart.h"
#include "kxr_spi_dfu.h"
#include "kxr_panel_notifier.h"

#define pr_red_info(fmt, args ...) \
	printk(KERN_INFO "\033[31m" fmt "\033[0m\n", ##args)

#define pr_green_info(fmt, args ...) \
	printk(KERN_INFO "\033[32m" fmt "\033[0m\n", ##args)

#define pr_blue_info(fmt, args ...) \
	printk(KERN_INFO "\033[34m" fmt "\033[0m\n", ##args)

#define pr_bold_info(fmt, args ...) \
	printk(KERN_INFO "\033[1m" fmt "\033[0m\n", ##args)

// ================================================================================

#define pr_err_info(fmt, args ...) \
	pr_err(fmt "\n", ##args)

#define pr_pos_info() \
	pr_err_info("%s:%d", __FILE__, __LINE__)

#define pr_func_pos() \
	pr_err_info("%s:%d", __FUNCTION__, __LINE__)

#define pr_func_info(fmt, args ...) \
	pr_err_info("%s:%d " fmt, __FUNCTION__, __LINE__, ##args)

#define pr_line_info() \
	pr_err_info("%d", __LINE__)

// ================================================================================

#define wr_err_info(fmt, args ...) \
	kxr_aphost_println(fmt, ##args)

#define wr_pos_info() \
	wr_err_info("%s:%d", __FILE__, __LINE__)

#define wr_func_pos() \
	wr_err_info("%s:%d", __FUNCTION__, __LINE__)

#define wr_func_info(fmt, args ...) \
	wr_err_info("%s:%d " fmt, __FUNCTION__, __LINE__, ##args)

#define wr_line_info() \
	wr_err_info("%d", __LINE__)

// ================================================================================

typedef enum {
	KXR_SPI_POWER_MODE_OFF,
	KXR_SPI_POWER_MODE_ON,
	KXR_SPI_POWER_MODE_DFU,
} kxr_spi_power_mode_t;

typedef struct kxr_aphost {
	struct mutex lock;

	bool panel_enabled;
	bool panel_updated;

	kxr_spi_dfu_t dfu;
	kxr_spi_xfer_t xfer;
	kxr_spi_xchg_t xchg;
	kxr_spi_uart_t uart;
	kxr_panel_notifier_t notifier;

#ifndef CONFIG_KXR_SIMULATION_TEST
	struct pinctrl *pinctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;

	struct gpio_desc *gpio_dfu;
	struct gpio_desc *gpio_vcc;
	struct gpio_desc *gpio_irq;
	struct gpio_desc *gpio_ledl;
	struct gpio_desc *gpio_ledr;

	struct regulator *pwr_v1p8;
#endif

	bool pwr_enabled;
} kxr_aphost_t;

void kxr_aphost_println(const char *fmt, ...);
bool kxr_aphost_power_mode_set(kxr_aphost_t *aphost, kxr_spi_power_mode_t mode);
kxr_spi_power_mode_t kxr_aphost_power_mode_get(void);

int kxr_spi_xfer_setup(struct spi_device *spi);
void kxr_spi_xchg_write_command(kxr_aphost_t *aphost, u32 command);
int kxr_spi_xchg_show_command(kxr_aphost_t *aphost, u32 command, char *buff, int size);
int kxr_spi_xfer_probe(kxr_aphost_t *aphost);
int kxr_spi_xfer_start(kxr_aphost_t *aphost);
void kxr_spi_xfer_remove(kxr_aphost_t *aphost);

bool kxr_spi_xchg_sync(kxr_aphost_t *aphost);
int kxr_spi_xchg_probe(kxr_aphost_t *aphost);
void kxr_spi_xchg_remove(kxr_aphost_t *aphost);

int kxr_spi_uart_probe(kxr_aphost_t *aphost);
void kxr_spi_uart_remove(kxr_aphost_t *aphost);
bool kxr_spi_uart_sync(kxr_aphost_t *aphost);

int kxr_spi_dfu_probe(kxr_aphost_t *aphost);
void kxr_spi_dfu_remove(kxr_aphost_t *aphost);

void kxr_aphost_set_panel_state(kxr_aphost_t *aphost, bool enabled);
bool kxr_aphost_get_panel_state(kxr_aphost_t *aphost);
int kxr_panel_notifier_probe(kxr_aphost_t *aphost);
void kxr_panel_notifier_remove(kxr_aphost_t *aphost);

static inline kxr_aphost_t *kxr_aphost_get_drv_data(const struct device *dev)
{
	return (kxr_aphost_t *) dev_get_drvdata(dev);
}

static inline kxr_aphost_t *kxr_aphost_get_uart_data(struct uart_port *port)
{
	return (kxr_aphost_t *) port->private_data;
}

static inline kxr_aphost_t *kxr_spi_dfu_get_aphost(kxr_spi_dfu_t *dfu)
{
	return container_of(dfu, kxr_aphost_t, dfu);
}

static inline kxr_aphost_t *kxr_spi_xfer_get_aphost(kxr_spi_xfer_t *xfer)
{
	return container_of(xfer, kxr_aphost_t, xfer);
}

static inline kxr_aphost_t *kxr_spi_xchg_get_aphost(kxr_spi_xchg_t *xchg)
{
	return container_of(xchg, kxr_aphost_t, xchg);
}

static inline kxr_aphost_t *kxr_spi_uart_get_aphost(kxr_spi_uart_t *uart)
{
	return container_of(uart, kxr_aphost_t, uart);
}

static inline kxr_aphost_t *kxr_panel_notifier_get_aphost(kxr_panel_notifier_t *notifier)
{
	return container_of(notifier, kxr_aphost_t, notifier);
}

static inline bool kxr_aphost_power_enabled(void)
{
	return kxr_aphost_power_mode_get() != KXR_SPI_POWER_MODE_OFF;
}

static inline bool kxr_aphost_power_disabled(void)
{
	return kxr_aphost_power_mode_get() == KXR_SPI_POWER_MODE_OFF;
}

