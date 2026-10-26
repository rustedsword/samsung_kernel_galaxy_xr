/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: js46.jeong
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2023, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#include "XR2_BOE_VX135KDP_SUB_panel.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	ss_panel_attach_set(vdd, true);
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d \n", vdd->ndx);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

	return 0;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

	return 0;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	vdd->panel_revision = 0;

	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n",
		vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

	return true;
}

#define get_bit(value, shift, width)    ((value >> shift) & (GENMASK(width - 1, 0)))

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int iter = 0;
	int upper = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 16, 8);
	int mid = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 8);
	int lower = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8);

	if (IS_ERR_OR_NULL(vdd)) {
			LCD_ERR(vdd, "vdd is null or error\n");
		return NULL;
	}

	LCD_INFO(vdd, "bl_level: %d, WRDISBV: %x\n",
			vdd->br_info.common_br.bl_level, vdd->br_info.common_br.gm2_wrdisbv);

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);

	for (iter = 0 ; iter < 20 ; iter++) {
		pcmds->cmds[3].ss_txbuf[3*iter+1] = upper;
		pcmds->cmds[3].ss_txbuf[3*iter+2] = mid;
		pcmds->cmds[3].ss_txbuf[3*iter+3] = lower;
	}

	LCD_INFO(vdd, "bl_level: %d, WRDISBV: %x\n",
			vdd->br_info.common_br.bl_level, vdd->br_info.common_br.gm2_wrdisbv);

	return pcmds;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HBM) {
		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d)\n", br_type);
	}
}

void XR2_BOE_VX135KDP_SUB_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s ++\n", ss_get_panel_name(vdd));
	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;

	/* Brightness */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2;

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* Secondary panel but sync with Main panel */
	vdd->sync_with_main = true;

	LCD_INFO(vdd, "--\n");
}
