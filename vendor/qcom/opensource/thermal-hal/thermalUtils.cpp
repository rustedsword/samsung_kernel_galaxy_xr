/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials provided
 *	  with the distribution.
 *	* Neither the name of The Linux Foundation nor the names of its
 *	  contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above
 *	  copyright notice, this list of conditions and the following
 *	  disclaimer in the documentation and/or other materials provided
 *	  with the distribution.
 *	* Neither the name of Qualcomm Innovation Center, Inc. nor the
 *	  names of its contributors may be used to endorse or promote products
 *	  derived from this software without specific prior written permission.
 *
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "thermalUtils.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <hidl/HidlTransportSupport.h>

#include <iostream>
#include <map>
#include <string>

#include "thermalConfig.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

ThermalUtils::ThermalUtils(const ueventCB &inp_cb):
	cfg(),
	cmnInst(),
	monitor(std::bind(&ThermalUtils::ueventParse, this,
				std::placeholders::_1,
				std::placeholders::_2)),
	cb(inp_cb)
{
	int ret = 0;
	std::vector<struct target_therm_cfg> therm_cfg = cfg.fetchConfig();

	is_sensor_init = false;
	is_cdev_init = false;
	ret = cmnInst.initThermalZones(therm_cfg);
	if (ret > 0) {
		is_sensor_init = true;
		std::lock_guard<std::mutex> _lock(sens_cb_mutex);
		for (const auto &sensor_ptr : cmnInst.fetch_sensor_list()) {
			thermalConfig[sensor_ptr->sensor_name] =
			    sensor_ptr.get();
			cmnInst.read_temperature(*sensor_ptr);
			cmnInst.estimateSeverity(*sensor_ptr);
			cmnInst.initThreshold(*sensor_ptr);
		}
		for (const auto &sensor_ptr :
		     cmnInst.fetch_logic_sensor_list()) {
			logicSensorConfig.push_back(sensor_ptr.get());
		}
		monitor.start();
	}
	ret = cmnInst.initCdev();
	if (ret > 0) {
		is_cdev_init = true;
		cdevList = cmnInst.fetch_cdev_list();
	}
}

void ThermalUtils::notify(struct therm_sensor& sens)
{
	int severity = cmnInst.estimateSeverity(sens);
	if (severity != -1) {
		LOG(INFO) << "sensor: " << sens.sensor_name <<" temperature: "
			<< sens.t.value << " old: " <<
			(int)sens.lastThrottleStatus << " new: " <<
			(int)sens.t.throttlingStatus << std::endl;
		cb(sens.t);
		cmnInst.initThreshold(sens);
	}
}

int ThermalUtils::updateLogicSensor(struct logic_sensor &sens)
{
	int ret = sens.update_function(sens);
	if (ret < 0)
		return ret;
	notifyLogicSensor(sens);
	return 0;
}

void ThermalUtils::notifyLogicSensor(struct logic_sensor &sens)
{
	if (sens.lastThrottleStatus != sens.t.throttlingStatus) {
		LOG(INFO) << "sensor: " << sens.t.name
			  << " temperature: " << sens.t.value
			  << " old: " << (int)sens.lastThrottleStatus
			  << " new: " << (int)sens.t.throttlingStatus;
		cb(sens.t);
	}
}

void ThermalUtils::ueventParse(std::string sensor_name, int temp)
{
	LOG(INFO) << "uevent triggered for sensor: " << sensor_name
		<< std::endl;
	if (thermalConfig.find(sensor_name) == thermalConfig.end()) {
		LOG(DEBUG) << "sensor is not monitored:" << sensor_name
			<< std::endl;
		return;
	}
	std::lock_guard<std::mutex> _lock(sens_cb_mutex);
	struct therm_sensor *sens = thermalConfig[sensor_name];
	sens->t.value = (float)temp / (float)sens->mulFactor;
	notify(*sens);

	for (auto *logic_sensor : logicSensorConfig) {
		if (logic_sensor->thermal_sensors.count(logic_sensor->t.name) >
		    0) {
			updateLogicSensor(*logic_sensor);
		}
	}
}

int ThermalUtils::readTemperatures(hidl_vec<Temperature_1_0>& temp)
{
	std::unordered_map<std::string, struct therm_sensor>::iterator it;
	int ret = 0, idx = 0;
	std::vector<Temperature_1_0> _temp_v;

	if (!is_sensor_init)
		return 0;
	std::lock_guard<std::mutex> _lock(sens_cb_mutex);
	for (it = thermalConfig.begin(); it != thermalConfig.end();
			it++, idx++) {
		struct therm_sensor& sens = it->second;
		Temperature_1_0 _temp;

		/* v1 supports only CPU, GPU, Battery and SKIN */
		if (sens.t.type > TemperatureType::SKIN)
			continue;
		ret = cmnInst.read_temperature(sens);
		if (ret < 0)
			return ret;
		notify(sens);
		_temp.currentValue = sens.t.value;
		_temp.name = sens.t.name;
		_temp.type = (TemperatureType_1_0)sens.t.type;
		_temp.throttlingThreshold = sens.thresh.hotThrottlingThresholds[
					(size_t)sens.throt_severity];
		_temp.shutdownThreshold = sens.thresh.hotThrottlingThresholds[
					(size_t)ThrottlingSeverity::SHUTDOWN];
		_temp.vrThrottlingThreshold = sens.thresh.vrThrottlingThreshold;
		_temp_v.push_back(_temp);
	}

	temp = _temp_v;
	return temp.size();
}

int ThermalUtils::readTemperatures(bool filterType, TemperatureType type,
                                            hidl_vec<Temperature>& temp)
{
	int ret = 0;
	std::vector<Temperature> _temp;

	std::lock_guard<std::mutex> _lock(sens_cb_mutex);
	for (auto [_, sens]: thermalConfig) {
		if (filterType && sens->t.type != type)
			continue;
		ret = cmnInst.read_temperature(*sens);
		if (ret < 0)
			return ret;
		notify(*sens);
		_temp.push_back(sens->t);
	}

	for (auto &sens : logicSensorConfig) {
		if (filterType && sens->t.type != type) {
			continue;
		}
		updateLogicSensor(*sens);
		_temp.push_back(sens->t);
	}

	temp = _temp;
	return temp.size();
}

int ThermalUtils::readTemperatureThreshold(bool filterType, TemperatureType type,
                                            hidl_vec<TemperatureThreshold>& thresh)
{
	std::unordered_map<std::string, struct therm_sensor>::iterator it;
	std::vector<TemperatureThreshold> _thresh;

	for (auto [_, sens] : thermalConfig) {
		if (filterType && sens->t.type != type)
			continue;
		_thresh.push_back(sens->thresh);
	}

	for (auto &sens : logicSensorConfig) {
		if (filterType && sens->t.type != type)
			continue;
		_thresh.push_back(sens->thresh);
	}

	thresh = _thresh;
	return thresh.size();
}

int ThermalUtils::readCdevStates(bool filterType, cdevType type,
                                            hidl_vec<CoolingDevice>& cdev_out)
{
	int ret = 0;
	std::vector<CoolingDevice> _cdev;

	for (struct therm_cdev cdev: cdevList) {

		if (filterType && cdev.c.type != type)
			continue;
		ret = cmnInst.read_cdev_state(cdev);
		if (ret < 0)
			return ret;
		_cdev.push_back(cdev.c);
	}

	cdev_out = _cdev;

	return cdev_out.size();
}

int ThermalUtils::fetchCpuUsages(hidl_vec<CpuUsage>& cpu_usages)
{
	return cmnInst.get_cpu_usages(cpu_usages);
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
