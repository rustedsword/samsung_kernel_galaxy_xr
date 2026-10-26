// SPDX-License-Identifier: GPL-2.0
/*
 *  sec_pm_log.h - header for SAMSUNG Power/Thermal logging.
 *
 */

#ifndef __LINUX_SEC_PM_LOG_H_
#define __LINUX_SEC_PM_LOG_H_

#if IS_ENABLED(CONFIG_SEC_PM_LOG)
void ss_thermal_print(const char *fmt, ...);
void ss_power_print(const char *fmt, ...);
#endif

#endif
