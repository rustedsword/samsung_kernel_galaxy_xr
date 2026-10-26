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

#include "kxr_panel_notifier.h"
#include "kxr_aphost.h"

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM
static struct drm_panel * kxr_panel_notifier_find_active_panel(struct device_node *np)
{
	struct device_node *node;
	struct drm_panel *panel;
	int count;
	int i;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0) {
		return NULL;
	}

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);

		if (!IS_ERR(panel)) {
			return panel;
		}
	}

	return NULL;
}
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_QCOM
static void kxr_panel_notifier_callback(enum panel_event_notifier_tag tag, struct panel_event_notification *notification, void *client_data)
{
	kxr_aphost_t *aphost = client_data;

	if (notification == NULL) {
		pr_err("Invalid notification\n");
		return;
	}

	aphost->notifier.work_fine = true;

	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_UNBLANK:
		if (notification->notif_data.early_trigger) {
			dev_info(&aphost->xfer.spi->dev, "resume notification pre commit\n");
		} else {
			dev_info(&aphost->xfer.spi->dev, "resume notification post commit\n");
			kxr_aphost_set_panel_state(aphost, true);
		}
		break;

	case DRM_PANEL_EVENT_BLANK:
		if (notification->notif_data.early_trigger) {
			dev_info(&aphost->xfer.spi->dev, "resume notification pre commit\n");
		} else {
			dev_info(&aphost->xfer.spi->dev, "suspend notification post commit\n");
			kxr_aphost_set_panel_state(aphost, false);
		}
		break;

	default:
		dev_info(&aphost->xfer.spi->dev, "kxr_panel_notifier_callback :%d\n", notification->notif_type);
		break;
	}
}

static int kxr_panel_notifier_register(kxr_panel_notifier_t *notifier, kxr_aphost_t *aphost)
{
	struct device *dev = &aphost->xfer.spi->dev;

	notifier->active_panel = kxr_panel_notifier_find_active_panel(dev->of_node);
	dev_info(dev, "active_panel = %p\n", notifier->active_panel);

	if (notifier->active_panel == NULL) {
		return -ENODEV;
	}

	notifier->cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY, PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_TOUCH, notifier->active_panel, kxr_panel_notifier_callback, aphost);
	if (notifier->cookie == NULL) {
		dev_err(&aphost->xfer.spi->dev, "Failed to register for panel events\n");
		return -ENODEV;
	}

	return -ENODEV;
}

static int kxr_panel_notifier_unregister(kxr_panel_notifier_t *notifier)
{
	pr_pos_info();

	if (notifier->active_panel && notifier->cookie) {
		panel_event_notifier_unregister(notifier->cookie);
	}

	return -ENODEV;
}
#elif CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM || CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_FB
static int kxr_panel_notifier_process_fb_event(struct notifier_block *block, int *blank, unsigned long event)
{
	kxr_panel_notifier_t *notifier = container_of(block, kxr_panel_notifier_t, fb_notifier);
	kxr_aphost_t *aphost = kxr_panel_notifier_get_aphost(notifier);

	switch (*blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		dev_info(&aphost->xfer.spi->dev, "DRM_PANEL_BLANK_UNBLANK, event = %ld\n", event);

		if (event == DRM_PANEL_EVENT_BLANK) {
			kxr_aphost_set_panel_state(aphost, true);
		}
		break;

	case DRM_PANEL_BLANK_POWERDOWN:
		dev_info(&aphost->xfer.spi->dev, "DRM_PANEL_BLANK_POWERDOWN, event = %ld\n", event);
		kxr_aphost_set_panel_state(aphost, false);
		break;

	default:
		dev_info(&aphost->xfer.spi->dev, "blank = %d, event = %ld\n", *blank, event);
		break;
	}

	return 0;
}

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM
static int kxr_panel_notifier_fb_notifier_callback_drm(struct notifier_block *block, unsigned long event, void *data)
{
	return kxr_panel_notifier_process_fb_event(block, ((struct drm_panel_notifier *) data)->data, event);
}
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_FB
static int kxr_panel_notifier_fb_notifier_callback(struct notifier_block *block, unsigned long event, void *data)
{
	return kxr_panel_notifier_process_fb_event(block, ((struct fb_event *) data)->data, event);
}
#endif

static int kxr_panel_notifier_register(kxr_panel_notifier_t *notifier, kxr_aphost_t *aphost)
{
#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM
	struct device *dev = &aphost->xfer.spi->dev;

	pr_pos_info();

	notifier->active_panel = kxr_panel_notifier_find_active_panel(dev->of_node);
	dev_info(dev, "active_panel = %p\n", notifier->active_panel);

	if (notifier->active_panel != NULL) {
		notifier->fb_notifier.notifier_call = kxr_panel_notifier_fb_notifier_callback_drm;
		return drm_panel_notifier_register(notifier->active_panel, &notifier->fb_notifier);
	}
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_FB
	notifier->fb_notifier.notifier_call = kxr_panel_notifier_fb_notifier_callback;
	return fb_register_client(&notifier->fb_notifier);
#else
	return -ENODEV;
#endif
}

static int kxr_panel_notifier_unregister(kxr_panel_notifier_t *notifier)
{
	pr_pos_info();

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM
	if (notifier->active_panel != NULL) {
		drm_panel_notifier_unregister(notifier->active_panel, &notifier->fb_notifier);
	}
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_FB
	return fb_unregister_client(&notifier->fb_notifier);
#else
	return -ENODEV;
#endif
}
#elif CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_EARLYSUSPEND
static void kxr_panel_notifier_early_suspend(struct early_suspend *handler)
{
	kxr_panel_notifier_t *notifier = container_of(handler, kxr_panel_notifier_t, early_suspend);
	kxr_aphost_t *aphost = kxr_panel_notifier_get_aphost(notifier);
	dev_info(&aphost->xfer.spi->dev, "kxr_panel_notifier_early_suspend blank = %d, event = %ld\n", *blank, event);
	kxr_aphost_set_panel_state(aphost, false);
}

static void kxr_panel_notifier_late_resume(struct early_suspend *handler)
{
	kxr_panel_notifier_t *notifier = container_of(handler, kxr_panel_notifier_t, early_suspend);
	kxr_aphost_t *aphost = kxr_panel_notifier_get_aphost(notifier);
	dev_info(&aphost->xfer.spi->dev, "kxr_panel_notifier_late_resume blank = %d, event = %ld\n", *blank, event);
	kxr_aphost_set_panel_state(aphost, true);
}

static int kxr_panel_notifier_register(kxr_panel_notifier_t *notifier, kxr_aphost_t *aphost)
{
	pr_pos_info();

	aphost->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	aphost->early_suspend.suspend = kxr_panel_notifier_early_suspend;
	aphost->early_suspend.resume = kxr_panel_notifier_late_resume;

	return register_early_suspend(&aphost->early_suspend);
}

static int kxr_panel_notifier_unregister(kxr_panel_notifier_t *notifier)
{
	pr_pos_info();
	return unregister_early_suspend(&notifier->early_suspend);
}
#else
int kxr_panel_notifier_register(kxr_panel_notifier_t *notifier, kxr_aphost_t *aphost)
{
	pr_pos_info();
	return -1;
}

int kxr_panel_notifier_unregister(kxr_panel_notifier_t *notifier)
{
	pr_pos_info();
	return -1;
}
#endif

int kxr_panel_notifier_probe(kxr_aphost_t *aphost)
{
	kxr_panel_notifier_t *notifier = &aphost->notifier;
	int ret;

	ret = kxr_panel_notifier_register(notifier, aphost);
	if (ret < 0) {
		dev_err(&aphost->xfer.spi->dev, "Failed to kxr_panel_notifier_register: %d\n", ret);
		notifier->registered = false;
	} else {
		notifier->registered = true;
	}

	return ret;
}

void kxr_panel_notifier_remove(kxr_aphost_t *aphost)
{
	kxr_panel_notifier_t *notifier = &aphost->notifier;

	if (notifier->registered) {
		kxr_panel_notifier_unregister(notifier);
	}
}
