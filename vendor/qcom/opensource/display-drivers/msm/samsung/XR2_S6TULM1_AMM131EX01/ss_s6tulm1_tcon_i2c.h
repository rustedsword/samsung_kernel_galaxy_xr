/*
 * ss_s6ulm1_tcon_i2c.h - Platform data for isl98608 buck driver
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include "../ss_dsi_panel_common.h"

int ss_s6tulm1_tcon_configure(struct samsung_display_driver_data *vdd, u16 data[][2], int size, bool is_left);
int ss_s6tulm1_tcon_init(void);
void ss_s6tulm1_tcon_exit(void);
