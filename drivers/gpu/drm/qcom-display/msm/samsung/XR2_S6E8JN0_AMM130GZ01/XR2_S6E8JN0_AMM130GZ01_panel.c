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
#include "XR2_S6E8JN0_AMM130GZ01_panel.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	ss_panel_attach_set(vdd, true);
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

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

	LCD_INFO_ONCE(vdd, "panel_revision = %c %d\n",
		vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

	return true;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *buf;
	int year, month, day;
	int hour, min;
	int ret;
	char temp[50];
	int buf_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);

	if (pcmds->count) {
		buf_len = pcmds->cmds[0].msg.rx_len;

		buf = kzalloc(buf_len, GFP_KERNEL);
		if (!buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
			kfree(buf);
			return false;
		}

		/* Manufacture Date */

		year = buf[6] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = buf[6] & 0x0f;
		day = buf[7] & 0x1f;
		hour = buf[8] & 0x0f;
		min = buf[9] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_INFO(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

		/* While Coordinates */

		vdd->mdnie.mdnie_x = buf[0] << 8 | buf[1];	/* X */
		vdd->mdnie.mdnie_y = buf[2] << 8 | buf[3];	/* Y */

		LCD_INFO(vdd, "X-%d Y-%d \n", vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		/* CELL ID (manufacture date + white coordinates) */
		/* Manufacture Date */
		len += sprintf(temp + len, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x000000\n",
			buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12],
			buf[0], buf[1], buf[2], buf[3]);

		vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->cell_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		else {
			vdd->cell_id_len = len;
			strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
			LCD_INFO(vdd, "CELL ID : [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	kfree(buf);

	return true;
}

#define get_bit(value, shift, width)    ((value >> shift) & (GENMASK(width - 1, 0)))

static struct dsi_panel_cmd_set *ss_brightness_gamma_mode2(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int upper = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 8);
	int lower = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8);
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = upper;
	pcmds->cmds[cmd_idx].ss_txbuf[2] = lower;

	LCD_INFO(vdd, "bl_level: %d, idx = %d, WRDISBV: %x\n",
			vdd->br_info.common_br.bl_level, vdd->br_info.common_br.cd_idx,
			vdd->br_info.common_br.gm2_wrdisbv);

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

void XR2_S6E8JN0_AMM130GZ01_init(struct samsung_display_driver_data *vdd)
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
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;

	/* Brightness */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2;

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	LCD_INFO(vdd, "--\n");
}
