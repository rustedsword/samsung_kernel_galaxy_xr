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
#include <linux/of.h>
#include <linux/version.h>

#define KXR_PANEL_EVENT_TYPE_NONE			0
#define KXR_PANEL_EVENT_TYPE_EARLYSUSPEND	1
#define KXR_PANEL_EVENT_TYPE_DRM			2
#define KXR_PANEL_EVENT_TYPE_FB				3
#define KXR_PANEL_EVENT_TYPE_QCOM			4

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_EARLYSUSPEND
#elif LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
#ifdef CONFIG_QCOM_PANEL_EVENT_NOTIFIER
#include <linux/soc/qcom/panel_event_notifier.h>
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_QCOM
#else
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_NONE
#endif
#elif defined(CONFIG_DRM)
#include <drm/drm_panel.h>
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_DRM
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_FB
#define DRM_PANEL_EVENT_BLANK				FB_EVENT_BLANK
#define DRM_PANEL_BLANK_UNBLANK				FB_BLANK_UNBLANK
#define DRM_PANEL_BLANK_POWERDOWN			FB_BLANK_POWERDOWN
#elif defined(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
#include <linux/soc/qcom/panel_event_notifier.h>
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_QCOM
#else
#define CONFIG_KXR_PANEL_EVENT_TYPE			KXR_PANEL_EVENT_TYPE_NONE
#endif

typedef struct kxr_panel_notifier {
	bool registered;

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM
	struct drm_panel *active_panel;
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_QCOM
	void *cookie;
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_DRM || CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_FB
	struct notifier_block fb_notifier;
#endif

#if CONFIG_KXR_PANEL_EVENT_TYPE == KXR_PANEL_EVENT_TYPE_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
} kxr_panel_notifier_t;

