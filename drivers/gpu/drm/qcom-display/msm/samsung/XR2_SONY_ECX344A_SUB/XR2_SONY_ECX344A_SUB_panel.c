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
#include "XR2_SONY_ECX344A_SUB_panel.h"

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

#define bit_value(value, bit)    (value & BIT(bit)) ? 1 : 0

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char* buf;
	enum LU_IDX {
		LU1000 = 0,
		LU0900,
		LU0800,
		LU0650,
		LU0500,
		LU_MAX,
	};
	enum ID_IDX {
		DDB1 = 0,
		DDB2,
		DDB3,
		DDB4,
		OTP1,
		OTP2,
		OTP3,
		OTP4,
		OTP5,
		OTP6,
		OTP7,
		ID_MAX,
	};
	int id_addr[ID_MAX] = {0xA1, 0xA1, 0xA1, 0xA1, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76};
	int buffer_len = ID_MAX;
	int lu_tb[LU_MAX];
	int len = 0;
	char temp[50];
	int id_idx, lu_idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);

	if (pcmds->count) {
		buf = kzalloc(buffer_len, GFP_KERNEL);

		/* read DDB */
		for (id_idx = DDB1 ; id_idx <= DDB4 ; id_idx++) {
			pcmds->cmds[0].ss_txbuf[0] = id_addr[id_idx];
			pcmds->read_startoffset = id_idx;
			ss_panel_data_read(vdd, RX_MODULE_INFO, &buf[id_idx], LEVEL1_KEY);
		}

		/* read OTP0X */
		for (id_idx = OTP1 ; id_idx <= OTP7 ; id_idx++) {
			pcmds->cmds[0].ss_txbuf[0] = id_addr[id_idx];
			pcmds->read_startoffset = 0;
			ss_panel_data_read(vdd, RX_MODULE_INFO, &buf[id_idx], LEVEL1_KEY);
		}

		/* calculate lu_tbs */
		for (lu_idx = LU1000 ; lu_idx <= LU0650 ; lu_idx++) {
			lu_tb[lu_idx] = (((bit_value(buf[OTP2+lu_idx], 7)) << 9) | ((bit_value(buf[OTP2+lu_idx], 6)) << 8) |
					((bit_value(buf[OTP2+lu_idx], 5)) << 7) | 	((bit_value(buf[OTP2+lu_idx], 4)) << 6) |
					((bit_value(buf[OTP2+lu_idx], 3)) << 5) | ((bit_value(buf[OTP2+lu_idx], 2)) << 4) |
					((bit_value(buf[OTP2+lu_idx], 1)) << 3) | ((bit_value(buf[OTP2+lu_idx], 0)) << 2) |
					((bit_value(buf[OTP1], 7-(lu_idx * 2))) << 1 ) | (bit_value(buf[OTP1],6-(lu_idx*2)))) * 2;
		}

		lu_tb[LU0500] = (((bit_value(buf[OTP7], 7)) << 9) | ((bit_value(buf[OTP7], 6)) << 8) |
					((bit_value(buf[OTP7], 5)) << 7) | 	((bit_value(buf[OTP7], 4)) << 6) |
					((bit_value(buf[OTP7], 3)) << 5) | ((bit_value(buf[OTP7], 2)) << 4) |
					((bit_value(buf[OTP7], 1)) << 3) | ((bit_value(buf[OTP7], 0)) << 2) |
					((bit_value(buf[OTP6], 7)) << 1 ) | ((bit_value(buf[OTP6],6)))) * 2;

		len += sprintf(temp + len, "%02x%02x%02x%02x%04d%04d%04d%04d%04d\n",
			buf[0], buf[1], buf[2], buf[3], lu_tb[LU1000], lu_tb[LU0900], lu_tb[LU0800], lu_tb[LU0650], lu_tb[LU0500]);

		vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);

		if (!vdd->cell_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		else {
			vdd->cell_id_len = len;
			strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
			LCD_INFO(vdd, "CELL ID : [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);
		}
		kfree(buf);
	} else {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	return true;
}

#define get_bit(value, shift, width)    ((value >> shift) & (GENMASK(width - 1, 0)))
#define MAX_VR_BL_LEVEL 8

static int br_duty_table[MAX_VR_BL_LEVEL] = {
	0x1EA1EA,
	0x28E28E,
	0x332332,
	0x332332,
	0x332332,
	0x332332,
	0x332332,
	0x332332
};

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int iter = 0;
	int upper = get_bit(br_duty_table[vdd->br_info.common_br.cd_idx], 16, 8);
	int mid = get_bit(br_duty_table[vdd->br_info.common_br.cd_idx], 8, 8);
	int lower = get_bit(br_duty_table[vdd->br_info.common_br.cd_idx], 0, 8);
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
			LCD_ERR(vdd, "vdd is null or error\n");
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = vdd->br_info.common_br.gm2_wrdisbv;
	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0xB4);
	for (iter = 0 ; iter < 20 ; iter++) {
		pcmds->cmds[cmd_idx].ss_txbuf[3*iter+1] = upper;
		pcmds->cmds[cmd_idx].ss_txbuf[3*iter+2] = mid;
		pcmds->cmds[cmd_idx].ss_txbuf[3*iter+3] = lower;
	}

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

void XR2_SONY_ECX344A_SUB_init(struct samsung_display_driver_data *vdd)
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

	/* Secondary panel but sync with Main panel */
	vdd->sync_with_main = true;

	LCD_INFO(vdd, "--\n");
}
