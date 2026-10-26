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

#define KXR_CACHE_SIZE_MIN			1024
#define KXR_CACHE_SIZE_FIX			8192

#if KXR_CACHE_SIZE_FIX > 0
#define KXR_CACHE_INIT(cache, size) \
	{ 0, 0, 0, 0 }

#define KXR_CACHE_SIZE(cache) \
	sizeof((cache)->buff)
#else
#define KXR_CACHE_INIT(cache, size) \
	{ 0, 0, 0, 0, (size) + KXR_CACHE_SIZE_MIN }

#define KXR_CACHE_SIZE(cache) \
	(cache)->buff_size
#endif

typedef struct {
	u16 wr_index;
	u16 wr_peek;
	u16 rd_index;
	u16 rd_peek;
#if KXR_CACHE_SIZE_FIX > 0
	u8 buff[KXR_CACHE_SIZE_FIX];
#else
	u16 buff_size;
	u8 buff[KXR_CACHE_SIZE_MIN];
#endif
} kxr_cache_t;

void kxr_cache_init(kxr_cache_t *cache, u16 size);
void kxr_cache_clear(kxr_cache_t *cache);
bool kxr_cache_write_byte(kxr_cache_t *cache, u8 value);
bool kxr_cache_read_byte(kxr_cache_t *cache, u8 *value);
u8 *kxr_cache_read_peek(kxr_cache_t *cache);
void kxr_cache_read_apply(kxr_cache_t *cache);
u8 *kxr_cache_write_peek(kxr_cache_t *cache);
void kxr_cache_write_apply(kxr_cache_t *cache);
u8 *kxr_cache_write(kxr_cache_t *cache, const u8 *buff, u16 length);
u8 *kxr_cache_write_user(kxr_cache_t *cache, const u8 __user *buff, u16 length);
u8 *kxr_cache_read(kxr_cache_t *cache, u8 *buff, u16 length);
u8 *kxr_cache_read_user(kxr_cache_t *cache, u8 __user *buff, u16 length);
u16 kxr_cache_write_remain(kxr_cache_t *cache);
u16 kxr_cache_read_remain(kxr_cache_t *cache);

static inline u16 kxr_cache_index_add(kxr_cache_t *cache, u16 index, u16 value)
{
	return (index + value) % KXR_CACHE_SIZE(cache);
}

static inline u16 kxr_cache_write_add(kxr_cache_t *cache, u16 value)
{
	return kxr_cache_index_add(cache, cache->wr_index, value);
}

static inline u16 kxr_cache_read_add(kxr_cache_t *cache, u16 value)
{
	return kxr_cache_index_add(cache, cache->rd_index, value);
}

static inline bool kxr_cache_is_empty(kxr_cache_t *cache)
{
	return cache->rd_index == cache->wr_index;
}

static inline bool kxr_cache_not_empty(kxr_cache_t *cache)
{
	return cache->rd_index != cache->wr_index;
}

static inline bool kxr_cache_is_full(kxr_cache_t *cache)
{
	return kxr_cache_write_add(cache, 1) == cache->rd_index;
}

static inline bool kxr_cache_not_full(kxr_cache_t *cache)
{
	return kxr_cache_write_add(cache, 1) != cache->rd_index;
}
