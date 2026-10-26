#pragma once

#include <android-base/properties.h>

#include <set>
#include <string>

#include "thermalData.h"

namespace vendor::samsung::hardware::thermal
{

class LogicSensorConfig
{
      public:
	using logic_sensor =
	    android::hardware::thermal::V2_0::implementation::logic_sensor;
	using therm_sensor =
	    android::hardware::thermal::V2_0::implementation::therm_sensor;
	using Temperature = ::android::hardware::thermal::V2_0::Temperature;
	using TemperatureType =
	    android::hardware::thermal::V2_0::TemperatureType;
	using ThrottlingSeverity =
	    android::hardware::thermal::V2_0::ThrottlingSeverity;

	std::string name;
	TemperatureType type;
	// The set of thermal sensors that it depends on.
	// This will be the keys of logic_sensor::thermal_sensors.
	std::set<std::string> thermal_sensors;
	// See logic_sensor::update_function.
	std::function<int(logic_sensor &)> update_function;
};

std::map<std::string, std::vector<LogicSensorConfig>> logicSensorConfigs = {
    {"SM-I610",
     {LogicSensorConfig{
	 .name = "skin",
	 .type = LogicSensorConfig::TemperatureType::SKIN,
	 .thermal_sensors = {"AP", "LRP"},
	 .update_function = [consider_lrp = android::base::GetBoolProperty(
				 "vendor.thermal.skin_consider_lrp",
				 android::base::GetBoolProperty(
				     "persist.vendor.thermal.skin_consider_lrp",
				     true))](
				LogicSensorConfig::logic_sensor &sensor) {
		 auto ap_it = sensor.thermal_sensors.find("AP");
		 if (ap_it == sensor.thermal_sensors.end()) {
			 return -1;
		 }
		 auto &ap_temp = ap_it->second->t;
		 sensor.lastThrottleStatus = sensor.t.throttlingStatus;
		 sensor.t.value = ap_temp.value;
		 sensor.t.throttlingStatus = ap_temp.throttlingStatus;
		 if (!consider_lrp) {
			 return 0;
		 }

		 // Considers LRP if its severity is emergency or
		 // above.
		 auto lrp_it = sensor.thermal_sensors.find("LRP");
		 if (lrp_it == sensor.thermal_sensors.end()) {
			 return -1;
		 }
		 auto &lrp_temp = lrp_it->second->t;
		 if (lrp_temp.throttlingStatus >=
			 LogicSensorConfig::ThrottlingSeverity::EMERGENCY &&
		     lrp_temp.throttlingStatus > sensor.t.throttlingStatus) {
			 sensor.t.throttlingStatus = lrp_temp.throttlingStatus;
		 }
		 return 0;
	 }}}}};

} // namespace vendor::samsung::hardware::thermal
