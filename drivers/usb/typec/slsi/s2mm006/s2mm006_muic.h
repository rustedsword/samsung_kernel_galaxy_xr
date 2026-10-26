/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __S2MM006_MUIC_H__
#define __S2MM006_MUIC_H__

#include <linux/muic/common/muic.h>

#define MUIC_DEV_NAME	"muic-s2mm006"

enum s2mm006_muic_registers {
	S2MM006_MUIC_REG_BC_INT =			0x02,
	S2MM006_MUIC_REG_BC_MASK =			0x08,
	S2MM006_MUIC_REG_BC_STATUS1 =		0x0E,
	S2MM006_MUIC_REG_BC_STATUS2 =		0x0F,
	S2MM006_MUIC_REG_BC_STATUS3 =		0x10,

	S2MM006_MUIC_REG_FACTORY_STATUS =	0x15,

	S2MM006_MUIC_REG_BC_CTRL1 =			0x2D,
	S2MM006_MUIC_REG_BC_CTRL2 =			0x2E,
	S2MM006_MUIC_REG_BC_CTRL3 =			0x2F,
	S2MM006_MUIC_REG_BC_CTRL4 =			0x30,

	S2MM006_MUIC_REG_AFC_CTRL1 =		0x31,
	S2MM006_MUIC_REG_AFC_CTRL2 =		0x32,
	S2MM006_MUIC_REG_AFC_CTRL3 =		0x33,

	S2MM006_MUIC_REG_AFC_RX_DATA1 =		0x34,
	S2MM006_MUIC_REG_AFC_RX_DATA2 =		0x35,
	S2MM006_MUIC_REG_AFC_RX_DATA3 =		0x36,
	S2MM006_MUIC_REG_AFC_RX_DATA4 =		0x37,
	S2MM006_MUIC_REG_AFC_RX_DATA5 =		0x38,
	S2MM006_MUIC_REG_AFC_RX_DATA6 =		0x39,
	S2MM006_MUIC_REG_AFC_RX_DATA7 =		0x3A,
	S2MM006_MUIC_REG_AFC_RX_DATA8 =		0x3B,
	S2MM006_MUIC_REG_AFC_RX_DATA9 =		0x3C,
	S2MM006_MUIC_REG_AFC_RX_DATA10 =	0x3D,
	S2MM006_MUIC_REG_AFC_RX_DATA11 =	0x3E,
	S2MM006_MUIC_REG_AFC_RX_DATA12 =	0x3F,
	S2MM006_MUIC_REG_AFC_RX_DATA13 =	0x40,
	S2MM006_MUIC_REG_AFC_RX_DATA14 =	0x41,
	S2MM006_MUIC_REG_AFC_RX_DATA15 =	0x42,
	S2MM006_MUIC_REG_AFC_RX_DATA16 =	0x43,

	S2MM006_MUIC_REG_QC_CTRL1 =			0x44,
	S2MM006_MUIC_REG_QC_TX_DATA =		0x45,
	S2MM006_MUIC_REG_QC_RX_DATA =		0x46,
};

#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)
#define DEF_MASK(value, shift)	((value) << (shift))

/* s2mm006 muic register read/write related information defines. */
/* S2MM006 Control register */
/* S2MM006_MUIC_REG_BC_INT register (0x02) */
#define BC_INT_SYS_MESSAGE_SHIFT		5
#define BC_INT_ADC_MESSAGE_SHIFT		4
#define BC_INT_CHG_TYPE_SHIFT			3
#define BC_INT_DCDTMO_SHIFT				2
#define BC_INT_VBUS_ADC_SHIFT			1
#define BC_INT_VBUS_DET_SHIFT			0

/* S2MM006_MUIC_REG_BC_MASK */
#define BC_INT_SYS_MESSAGE_MASK			BIT(BC_INT_SYS_MESSAGE_SHIFT)
#define BC_INT_ADC_MESSAGE_MASK			BIT(BC_INT_ADC_MESSAGE_SHIFT)
#define BC_INT_CHG_TYPE_MASK			BIT(BC_INT_CHG_TYPE_SHIFT)
#define BC_INT_DCDTMO_MASK				BIT(BC_INT_DCDTMO_SHIFT)
#define BC_INT_VBUS_ADC_MASK			BIT(BC_INT_VBUS_ADC_SHIFT)
#define BC_INT_VBUS_DET_MASK			BIT(BC_INT_VBUS_DET_SHIFT)

/* S2MM006_MUIC_REG_BC_STATUS1 */
#define BC_STATUS1_PR_CHG_TYPE_SHIFT	5
#define BC_STATUS1_PR_CHG_TYPE_WIDTH	3
#define BC_STATUS1_CHG_TYPE_SHIFT		1
#define BC_STATUS1_CHG_TYPE_WIDTH		3
#define BC_STATUS1_VBUS_DET_SHIFT		0

#define BC_STATUS1_PR_CHG_TYPE_MASK	MASK(BC_STATUS1_PR_CHG_TYPE_WIDTH, BC_STATUS1_PR_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_MASK	MASK(BC_STATUS1_CHG_TYPE_WIDTH, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_NO_CONN_MASK	DEF_MASK(0x0, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_TIMEOUT_MASK		DEF_MASK(0x1, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_SDP_MASK			DEF_MASK(0x2, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_CDP_MASK			DEF_MASK(0x3, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_DCP_MASK			DEF_MASK(0x4, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_HVDCP_MASK			DEF_MASK(0x5, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_HVDCP_AFC_MASK		DEF_MASK(0x6, BC_STATUS1_CHG_TYPE_SHIFT)
#define BC_STATUS1_CHG_TYPE_HVDCP_QC_MASK		DEF_MASK(0x7, BC_STATUS1_CHG_TYPE_SHIFT)

#define BC_STATUS1_VBUS_DET_MASK	BIT(BC_STATUS1_VBUS_DET_SHIFT)

/* S2MM006_MUIC_REG_BC_STATUS2 */
#define BC_STATUS2_VBUS_ADC_SHIFT		0
#define BC_STATUS2_VBUS_ADC_WIDTH		6
#define BC_STATUS2_VBUS_ADC_MASK		MASK(BC_STATUS2_VBUS_ADC_WIDTH, BC_STATUS2_VBUS_ADC_SHIFT)

/* S2MM006_MUIC_REG_BC_STATUS3 */
#define BC_STATUS3_SYS_MESSAGE_SHIFT	4
#define BC_STATUS3_SYS_MESSAGE_WIDTH	4
#define BC_STATUS3_SYS_MESSAGE_MASK		MASK(BC_STATUS3_SYS_MESSAGE_WIDTH, BC_STATUS3_SYS_MESSAGE_SHIFT)
#define BC_STATUS3_AFC_MESSAGE_SHIFT	0
#define BC_STATUS3_AFC_MESSAGE_WIDTH	4
#define BC_STATUS3_AFC_MESSAGE_MASK		MASK(BC_STATUS3_AFC_MESSAGE_WIDTH, BC_STATUS3_AFC_MESSAGE_SHIFT)

#define FACTORY_STATUS_RID_VALUE_SHIFT		3
#define FACTORY_STATUS_RID_VALUE_WIDTH		3
#define FACTORY_STATUS_RID_VALUE_MASK		MASK(FACTORY_STATUS_RID_VALUE_WIDTH, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_OPEN_MASK	DEF_MASK(0x0, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_1K_MASK	DEF_MASK(0x1, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_255K_MASK	DEF_MASK(0x2, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_301K_MASK	DEF_MASK(0x3, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_523K_MASK	DEF_MASK(0x4, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_619K_MASK	DEF_MASK(0x5, FACTORY_STATUS_RID_VALUE_SHIFT)
#define FACTORY_STATUS_RID_VALUE_GND_MASK	DEF_MASK(0x6, FACTORY_STATUS_RID_VALUE_SHIFT)

/* S2MM006_MUIC_REG_BC_CTRL1 */
#define BC_CTRL1_BC1P2_EN_SHIFT			7
#define BC_CTRL1_BC1P2_CC_EN_SHIFT		6
#define BC_CTRL1_BC1P2_MANUAL_SHIFT		5
#define BC_CTRL1_DCDTMO_EN_SHIFT		4
#define BC_CTRL1_DCD_TIMER_SHIFT		3
#define BC_CTRL1_DCDTMO_RETRY_SHIFT		2
#define BC_CTRL1_VBUS_ADC_REQ_SHIFT		0

#define BC_CTRL1_BC1P2_EN_MASK			BIT(BC_CTRL1_BC1P2_EN_SHIFT)
#define BC_CTRL1_BC1P2_CC_EN_MASK		BIT(BC_CTRL1_BC1P2_CC_EN_SHIFT)
#define BC_CTRL1_BC1P2_MANUAL_MASK		BIT(BC_CTRL1_BC1P2_MANUAL_SHIFT)
#define BC_CTRL1_DCDTMO_EN_MASK			BIT(BC_CTRL1_DCDTMO_EN_SHIFT)
#define BC_CTRL1_DCD_TIMER_MASK			BIT(BC_CTRL1_DCD_TIMER_SHIFT)
#define BC_CTRL1_DCDTMO_RETRY_MASK		BIT(BC_CTRL1_DCDTMO_RETRY_SHIFT)
#define BC_CTRL1_VBUS_ADC_REQ_MASK		BIT(BC_CTRL1_VBUS_ADC_REQ_SHIFT)
#define BC_CTRL1_BC1P2_DISABLE_NMASK		~(BC_CTRL1_BC1P2_EN_MASK)

/* S2MM006_MUIC_REG_BC_CTRL2 */
#define BC_CTRL2_DPDM_SWITCHING_WIDTH	3
#define BC_CTRL2_DPDM_SWITCHING_SHIFT	1
#define BC_CTRL2_MANUAL_SW_SHIFT		0

#define BC_CTRL2_DPDM_SWITCHING_MASK	MASK(BC_CTRL2_DPDM_SWITCHING_WIDTH, BC_CTRL2_DPDM_SWITCHING_SHIFT)
#define BC_CTRL2_MANUAL_SW_MASK			BIT(BC_CTRL2_MANUAL_SW_SHIFT)
#define BC_CTRL2_DPDM_SWITCHING_OPEN_MASK	DEF_MASK(0x0, BC_CTRL2_DPDM_SWITCHING_SHIFT)
#define BC_CTRL2_DPDM_SWITCHING_USB_MASK	DEF_MASK(0x1, BC_CTRL2_DPDM_SWITCHING_SHIFT)
#define BC_CTRL2_DPDM_SWITCHING_UART_MASK	DEF_MASK(0x2, BC_CTRL2_DPDM_SWITCHING_SHIFT)
#define BC_CTRL2_DPDM_SWITCHING_JTAG_MASK	DEF_MASK(0x3, BC_CTRL2_DPDM_SWITCHING_SHIFT)
#define BC_CTRL2_DPDM_SWITCHING_HICCUP_MASK	DEF_MASK(0x4, BC_CTRL2_DPDM_SWITCHING_SHIFT)

/* S2MM006_MUIC_REG_BC_CTRL3 */
#define BC_CTRL3_JTAG_SW_MANUAL_SHIFT		7
#define BC_CTRL3_JTAG_SW_CTRL_SHIFT			6
#define BC_CTRL3_JTAG_SW_STATUS_SHIFT		5
#define BC_CTRL3_UART_SW_MANUAL_SHIFT		4
#define BC_CTRL3_UART_SW_CTRL_SHIFT			3
#define BC_CTRL3_UART_SW_STATUS_SHIFT		2

#define BC_CTRL3_JTAG_SW_MANUAL_MASK		BIT(BC_CTRL3_JTAG_SW_MANUAL_SHIFT)
#define BC_CTRL3_JTAG_SW_CTRL_MASK			BIT(BC_CTRL3_JTAG_SW_CTRL_SHIFT)
#define BC_CTRL3_JTAG_SW_STATUS_MASK		BIT(BC_CTRL3_JTAG_SW_STATUS_SHIFT)
#define BC_CTRL3_UART_SW_MANUAL_MASK		BIT(BC_CTRL3_UART_SW_MANUAL_SHIFT)
#define BC_CTRL3_UART_SW_CTRL_MASK			BIT(BC_CTRL3_UART_SW_CTRL_SHIFT)
#define BC_CTRL3_UART_SW_STATUS_MASK		BIT(BC_CTRL3_UART_SW_STATUS_SHIFT)

/* S2MM006_MUIC_REG_BC_CTRL4 */
#define BC_CTRL4_KILLER_DET_EN_SHIFT		5
#define BC_CTRL4_DPDM_OVP_DEB_SHIFT			4
#define BC_CTRL4_DPDM_OVP_OVLO_SHIFT		2
#define BC_CTRL4_DPDM_OVP_OVLO_WIDTH		2
#define BC_CTRL4_DM_OVP_EN_SHIFT			1
#define BC_CTRL4_DP_OVP_EN_SHIFT			0

#define BC_CTRL4_KILLER_DET_EN_MASK			BIT(BC_CTRL4_KILLER_DET_EN_SHIFT)
#define BC_CTRL4_DPDM_OVP_DEB_MASK			BIT(BC_CTRL4_DPDM_OVP_DEB_SHIFT)
#define BC_CTRL4_DPDM_OVP_OVLO_MASK			MASK(BC_CTRL4_DPDM_OVP_OVLO_WIDTH, BC_CTRL4_DPDM_OVP_OVLO_SHIFT)
#define BC_CTRL4_DM_OVP_EN_MASK				BIT(BC_CTRL4_DM_OVP_EN_SHIFT)
#define BC_CTRL4_DP_OVP_EN_MASK				BIT(BC_CTRL4_DP_OVP_EN_SHIFT)

/* S2MM006_MUIC_REG_AFC_CTRL1 */
#define AFC_CTRL1_AFC_EN_SHIFT				0
#define AFC_CTRL1_AFC_REQ_SHIFT				7

#define AFC_CTRL1_AFC_EN_MASK				BIT(AFC_CTRL1_AFC_EN_SHIFT)
#define AFC_CTRL1_AFC_REQ_MASK				BIT(AFC_CTRL1_AFC_EN_SHIFT)

/* S2MM006_MUIC_REG_QC_CTRL1 */
#define QC_CTRL1_QC_EN_SHIFT				7

#define QC_CTRL1_QC_EN_MASK				BIT(QC_CTRL1_QC_EN_SHIFT)

/* s2mm006-muic macros */
#define ENUM_STR(x, r) { case x: r = #x; break; }

#define REQUEST_IRQ(_irq, _dev_id, _name, _func)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, _func,	\
				0, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

#define IS_VBUS_5V(_VBUS) (_VBUS >= 4500 && _VBUS <= 5500)
#define IS_VBUS_9V(_VBUS) (_VBUS >= 8000 && _VBUS <= 10300)

/* end of macros */
enum s2mm006_muic_enable {
	S2MM006_DISABLE,
	S2MM006_ENABLE,
};

enum s2mm006_muic_adc_mode {
	S2MM006_ADC_ONESHOT,
	S2MM006_ADC_PERIODIC,
};

enum s2mm006_muic_detect_dev_read_val {
	DEVICE_TYPE1 = 0,
	DEVICE_TYPE2,
	DEVICE_TYPE3,
	ADC,
	DEVICE_APPLE,
	CHG_TYPE,
	SC_STATUS2,
	MAX_NUM,
};

typedef enum {
	S2MM006_MODE_NONE = -1,
	S2MM006_MODE_AUTO,
	S2MM006_MODE_MANUAL,
	S2MM006_MODE_MAX,
} t_mode_data;

typedef enum {
	S2MM006_PATH_OPEN = 0,
	S2MM006_PATH_USB,
	S2MM006_PATH_UART,
	S2MM006_PATH_JTAG,
	S2MM006_PATH_HICCUP,
	S2MM006_PATH_AUTO,
} t_path_data;

typedef enum {
	S2MM006_DETECT_NONE,
	S2MM006_DETECT_DONE,
	S2MM006_DETECT_HV_DONE,
	S2MM006_DETECT_SKIP,
	S2MM006_DETECT_DETACH,
} t_detect_status;

typedef enum {
	S2MM006_IRQ_CHECK_DONE,
	S2MM006_IRQ_SKIP,
} t_irq_status;

/* muic chip specific internal data structure
 * that setted at muic-xxxx.c file
 */
struct s2mm006_muic_data {
	struct device *dev;
	struct i2c_client *i2c;	/* i2c addr: 0x7C; MUIC */
	struct s2mm006_dev *s2mm006_dev;
	struct mutex muic_mutex;
	struct mutex switch_mutex;
	struct mutex bcd_rescan_mutex;
	struct mutex afc_mutex;
	struct wakeup_source *muic_ws;
	/* W/A waiting for the charger ic */
	struct delayed_work dcd_recheck;

	u8 reg[MAX_NUM];
	muic_attached_dev_t new_dev;

	bool is_vbus_adc_valid;
	bool afc_check;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	int rid;
#endif
	int adc;
	int vbvolt;
	int rescan_cnt;
	struct muic_share_data *sdata;
};

#if IS_ENABLED(CONFIG_MUIC_S2MM006) && IS_ENABLED(CONFIG_SEC_FACTORY)
extern void s2mm006_muic_dcd_rescan(struct s2mm006_muic_data *muic_data);
extern int s2mm006_muic_control_rid_adc(struct s2mm006_muic_data *muic_data,
		bool enable);
extern int _s2mm006_muic_get_vbus_state(struct s2mm006_muic_data *muic_data);
extern struct s2mm006_muic_data *s2mm006_muic_init(struct i2c_client *i2c);
extern int s2mm006_muic_remove(struct s2mm006_muic_data *muic_data);
extern void s2mm006_muic_shutdown(struct s2mm006_muic_data *muic_data);
extern int s2mm006_muic_suspend(struct s2mm006_muic_data *muic_data);
extern int s2mm006_muic_resume(struct s2mm006_muic_data *muic_data);
extern void s2mm006_muic_rid_det_isr(struct s2mm006_muic_data *muic_data);
extern void process_muic(struct s2mm006_muic_data *muic_data, u8 irq);
#else
static inline void s2mm006_muic_dcd_rescan(struct s2mm006_muic_data *muic_data) {}
static inline int s2mm006_muic_control_rid_adc(struct s2mm006_muic_data *muic_data,
		bool enable) {return 0; }
static inline int _s2mm006_muic_get_vbus_state(struct s2mm006_muic_data *muic_data)
	{return 0; }
static inline struct s2mm006_muic_data *s2mm006_muic_init(struct i2c_client *i2c)
	{return NULL; }
static inline int s2mm006_muic_remove(struct s2mm006_muic_data *muic_data)
	{return 0; }
static inline void s2mm006_muic_shutdown(struct s2mm006_muic_data *muic_data) {}
static inline int s2mm006_muic_suspend(struct s2mm006_muic_data *muic_data)
	{return 0; }
static inline int s2mm006_muic_resume(struct s2mm006_muic_data *muic_data)
	{return 0; }
static inline void s2mm006_muic_rid_det_isr(struct s2mm006_muic_data *muic_data) {}
static inline void process_muic(struct s2mm006_muic_data *muic_data, u8 irq) {}
#endif
#endif /* __S2MM006_MUIC_H__ */
