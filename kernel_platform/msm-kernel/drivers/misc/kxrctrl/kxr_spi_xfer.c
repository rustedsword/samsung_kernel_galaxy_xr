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

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "kxr_aphost.h"

static ulong kxr_spi_irq_times = 0;
module_param_named(irq_times, kxr_spi_irq_times, ulong, 0644);

static ulong kxr_spi_xfer_times = 0;
module_param_named(xfer_times, kxr_spi_xfer_times, ulong, 0644);

static ulong kxr_spi_xfer_bytes = 0;
module_param_named(xfer_bytes, kxr_spi_xfer_bytes, ulong, 0644);

static int kxr_spi_xfer_mode = KXR_SPI_WORK_MODE_USER;
module_param_named(xfer_mode, kxr_spi_xfer_mode, int, 0644);

int kxr_spi_xfer_sync(kxr_spi_xfer_t *xfer, const void *tx_buff, void *rx_buff, int length)
{
#ifndef CONFIG_KXR_SIMULATION_TEST
	struct spi_transfer transfer = {
		.tx_buf = tx_buff,
		.rx_buf = rx_buff,
		.len = length,
	};
	struct spi_message message;
	int ret;

	usleep_range(100, 200);

	spi_message_init(&message);
	spi_message_add_tail(&transfer, &message);

	kxr_spi_xfer_bytes += length;
	kxr_spi_xfer_times++;

	xfer->sync_busy = true;
	ret = xfer->sync(xfer->spi, &message);
	xfer->sync_busy = false;

	return ret;
#else
	return 0;
#endif
}

static int kxr_spi_xfer_sync_dummy(struct spi_device *spi, struct spi_message *message)
{
	msleep(100);
	return -EBUSY;
}

void kxr_spi_xfer_sync_enable(kxr_spi_xfer_t *xfer)
{
	dev_info(&xfer->spi->dev, "kxr_spi_xfer_sync_enable\n");
	xfer->sync = spi_sync;
}

void kxr_spi_xfer_sync_disable(kxr_spi_xfer_t *xfer)
{
	dev_info(&xfer->spi->dev, "kxr_spi_xfer_sync_disable\n");
	xfer->sync = kxr_spi_xfer_sync_dummy;

	if (xfer->sync_busy) {
		dev_info(&xfer->spi->dev, "wait spi sync\n");
		msleep(10);
	}
}

int kxr_spi_xfer_sync_user(kxr_spi_xfer_t *xfer, void __user *buff, int length, bool write, bool read)
{
	unsigned long remain;
	void *tx_buff;
	int ret;

	if (length > sizeof(xfer->tx_buff)) {
		dev_err(&xfer->spi->dev, "Invalid length: %d\n", length);
		return -EINVAL;
	}

	if (write) {
		tx_buff = xfer->tx_buff;

		remain = copy_from_user(tx_buff, buff, length);
		if (remain > 0) {
			dev_err(&xfer->spi->dev, "Failed to copy_from_user: %ld\n", remain);
			return -EFAULT;
		}
	} else {
		tx_buff = NULL;
	}

	kxr_spi_xfer_set_user(xfer, 3);

	ret = kxr_spi_xfer_sync(xfer, tx_buff, xfer->rx_buff, length);
	if (ret < 0) {
		dev_err(&xfer->spi->dev, "Failed to kxr_spi_xfer_sync: %d\n", ret);
		return ret;
	}

	if (read) {
		remain = copy_to_user(buff, xfer->rx_buff, length);
		if (remain > 0) {
			dev_err(&xfer->spi->dev, "Failed to copy_to_user: %ld\n", remain);
			return -EFAULT;
		}
	}

	return 0;
}

void kxr_spi_xfer_wait(kxr_spi_xfer_t *xfer)
{
#if KXR_SPI_XFER_COMPLETION
	wait_for_completion(&xfer->sync_completion);
	WRITE_ONCE(xfer->sync_completion.done, 0);
#else
#if KXR_SPI_XFER_WAIT_QUEUE
	wait_event_interruptible(xfer->wait_queue, READ_ONCE(xfer->sync_pending) == 0);
#else
	while (READ_ONCE(xfer->sync_pending) == 0) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
#endif

	WRITE_ONCE(xfer->sync_pending, 0);
#endif
}

void kxr_spi_xfer_wakeup(kxr_spi_xfer_t *xfer)
{
#if KXR_SPI_XFER_COMPLETION
	complete(&xfer->sync_completion);
#else
	WRITE_ONCE(xfer->sync_pending, 1);

#if KXR_SPI_XFER_WAIT_QUEUE
	wake_up_interruptible(&xfer->wait_queue);
#else
	wake_up_process(xfer->task);
#endif
#endif
}

static int kxr_spi_xfer_thread(void *data)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) data;
	kxr_spi_xfer_t *xfer = &aphost->xfer;

	msleep(2000);

	while (kxr_spi_xfer_mode != KXR_SPI_WORK_MODE_EXIT) {
		while (xfer->handler(aphost));

		if (xfer->irq_disabled) {
			xfer->irq_disabled = false;
#ifndef CONFIG_KXR_SIMULATION_TEST
			enable_irq(xfer->irq);
#endif
		}

		kxr_spi_xfer_wait(xfer);
	}

	return 0;
}

#ifndef CONFIG_KXR_SIMULATION_TEST
static irqreturn_t kxr_spi_xfer_isr(int irq, void *dev_id)
{
	kxr_spi_xfer_t *xfer = (kxr_spi_xfer_t *) dev_id;

	disable_irq_nosync(irq);
	xfer->irq_disabled = true;
	kxr_spi_xfer_wakeup(xfer);
	kxr_spi_irq_times++;

	return IRQ_HANDLED;
}
#endif

static void kxr_spi_xfer_resume(kxr_aphost_t *aphost)
{
	kxr_spi_xfer_mode_set(&aphost->xfer, KXR_SPI_WORK_MODE_UART);
	kxr_spi_uart_clear(&aphost->uart);
	kxr_spi_xchg_clear(&aphost->xchg);
}

static bool kxr_spi_xfer_mode_user(kxr_aphost_t *aphost)
{
	kxr_spi_xfer_t *xfer = &aphost->xfer;
	int times;

	for (times = 10; times > 0; times--) {
		msleep(100);

		if (kxr_spi_xfer_mode != KXR_SPI_WORK_MODE_USER) {
			return true;
		}
	}

	pr_info("user_seconds = %d, mode = %d\n", xfer->user_seconds, kxr_spi_xfer_mode);

	if (xfer->user_seconds > 0) {
		xfer->user_seconds--;
	} else {
		kxr_spi_xfer_resume(aphost);
	}

	return true;
}

static bool kxr_spi_xfer_mode_idle(kxr_aphost_t *aphost)
{
	pr_pos_info();

	if (kxr_aphost_power_disabled()) {
		return false;
	}

	kxr_spi_xfer_resume(aphost);

	return true;
}

static bool kxr_spi_xfer_mode_exit(kxr_aphost_t *aphost)
{
	pr_pos_info();
	return false;
}

static bool kxr_spi_xfer_mode_uart(kxr_aphost_t *aphost)
{
	return kxr_spi_uart_sync(aphost);
}

static bool kxr_spi_xfer_mode_xchg(kxr_aphost_t *aphost)
{
#ifndef CONFIG_KXR_SIMULATION_TEST
	int times;
#endif

	kxr_spi_xfer_mode_set(&aphost->xfer, KXR_SPI_WORK_MODE_UART);

	kxr_spi_xchg_sync(aphost);

#ifndef CONFIG_KXR_SIMULATION_TEST
	for (times = 0; gpiod_get_value(aphost->gpio_irq) == 0 && times < 20; times++) {
		msleep(1);
	}
#endif

	kxr_spi_xchg_sync(aphost);

	return false;
}

bool kxr_spi_xfer_mode_set(kxr_spi_xfer_t *xfer, kxr_spi_work_mode_t mode)
{
	int mode_backup = kxr_spi_xfer_mode;

	if (mode_backup == mode || kxr_spi_xfer_exited()) {
		return false;
	}

	kxr_spi_xfer_mode = mode;

	switch (mode) {
	case KXR_SPI_WORK_MODE_USER:
		dev_info(&xfer->spi->dev, "KXR_SPI_WORK_MODE_USER\n");
		xfer->handler = kxr_spi_xfer_mode_user;

		if (xfer->user_seconds < KXR_SPI_XFER_USER_SECONDS) {
			xfer->user_seconds = KXR_SPI_XFER_USER_SECONDS;
		}

		kxr_spi_xfer_wakeup(xfer);
		msleep(20);
		break;

	case KXR_SPI_WORK_MODE_IDLE:
		dev_info(&xfer->spi->dev, "KXR_SPI_WORK_MODE_IDLE\n");
		xfer->handler = kxr_spi_xfer_mode_idle;
		break;

	case KXR_SPI_WORK_MODE_EXIT:
		dev_info(&xfer->spi->dev, "KXR_SPI_WORK_MODE_EXIT\n");
		xfer->handler = kxr_spi_xfer_mode_exit;
		kxr_spi_xfer_wakeup(xfer);
		break;

	case KXR_SPI_WORK_MODE_UART:
		dev_info(&xfer->spi->dev, "KXR_SPI_WORK_MODE_UART\n");
		xfer->handler = kxr_spi_xfer_mode_uart;
		break;

	case KXR_SPI_WORK_MODE_XCHG:
		dev_info(&xfer->spi->dev, "KXR_SPI_WORK_MODE_XCHG\n");
		xfer->handler = kxr_spi_xfer_mode_xchg;
		break;

	default:
		dev_err(&xfer->spi->dev, "Invalid xfer mode: %d", mode);
		kxr_spi_xfer_mode = mode_backup;
		return false;
	}

	return true;
}

kxr_spi_work_mode_t kxr_spi_xfer_mode_get(void)
{
	return kxr_spi_xfer_mode;
}

bool kxr_spi_xfer_post_xchg(kxr_spi_xfer_t *xfer)
{
	if (kxr_spi_xfer_mode == KXR_SPI_WORK_MODE_USER) {
		return false;
	}

	kxr_spi_xfer_mode_set(xfer, KXR_SPI_WORK_MODE_XCHG);
	kxr_spi_xfer_wakeup(xfer);

	return true;
}

int kxr_spi_xfer_setup(struct spi_device *spi)
{
#ifndef CONFIG_KXR_SIMULATION_TEST
	int ret;

	spi->bits_per_word = KXR_SPI_XFER_BITS_PER_WORD;
	spi->max_speed_hz = KXR_SPI_XFER_SPEED_HZ;
	spi->mode = KXR_SPI_XFER_MODE;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to spi_setup: %d\n", ret);
		return ret;
	}
#endif

	return 0;
}

int kxr_spi_xfer_start(kxr_aphost_t *aphost)
{
	kxr_spi_xfer_t *xfer = &aphost->xfer;
#ifndef CONFIG_KXR_SIMULATION_TEST
	int ret;
#endif
	struct sched_param param = { .sched_priority = 1 };

	msleep(1000);

	xfer->task = kthread_create(kxr_spi_xfer_thread, aphost, "kxr-spi-xfer");
	if (IS_ERR(xfer->task)) {
		dev_err(&xfer->spi->dev, "Failed to kthread_create\n");
		return PTR_ERR(xfer->task);
	}

	sched_setscheduler_nocheck(xfer->task, SCHED_FIFO, &param);

	wake_up_process(xfer->task);

#ifndef CONFIG_KXR_SIMULATION_TEST
	if (aphost->gpio_irq != NULL) {
		xfer->irq = gpiod_to_irq(aphost->gpio_irq);

		ret = devm_request_irq(&xfer->spi->dev, xfer->irq, kxr_spi_xfer_isr, IRQF_TRIGGER_HIGH, "kxr-spi-xfer", xfer);
		if (ret < 0) {
			dev_err(&xfer->spi->dev, "Failed to request_irq: %d\n", ret);
			return ret;
		}
	}
#endif

	return 0;
}

static ssize_t xfer_mode_show(struct device *dev, struct device_attribute *attr, char *buff)
{
	return scnprintf(buff, PAGE_SIZE, "%d\n", kxr_spi_xfer_mode);
}

static ssize_t xfer_mode_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t size)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	int mode;

	if (sscanf(buff, "%d", &mode) != 1) {
		return -EINVAL;
	}

	kxr_spi_xfer_mode_set(&aphost->xfer, mode);

	return size;
}

static DEVICE_ATTR_RW(xfer_mode);

int kxr_spi_xfer_probe(kxr_aphost_t *aphost)
{
	kxr_spi_xfer_t *xfer = &aphost->xfer;

	kxr_spi_xfer_sync_enable(xfer);

#if KXR_SPI_XFER_COMPLETION
	init_completion(&xfer->sync_completion);
#else
#if KXR_SPI_XFER_WAIT_QUEUE
	init_waitqueue_head(&xfer->wait_queue);
#endif

	xfer->sync_pending = 0;
#endif

	kxr_spi_xfer_mode_set(xfer, KXR_SPI_WORK_MODE_UART);
	device_create_file(&xfer->spi->dev, &dev_attr_xfer_mode);

	return 0;
}

void kxr_spi_xfer_remove(kxr_aphost_t *aphost)
{
	kxr_spi_xfer_t *xfer = &aphost->xfer;

	kxr_spi_xfer_sync_disable(xfer);

	device_remove_file(&xfer->spi->dev, &dev_attr_xfer_mode);
	kxr_spi_xfer_mode_set(xfer, KXR_SPI_WORK_MODE_EXIT);
	kthread_stop(xfer->task);
}
