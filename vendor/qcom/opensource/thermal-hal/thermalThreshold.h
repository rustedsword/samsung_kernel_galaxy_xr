#pragma once

#include <array>
#include <string>
#include <unordered_map>

#include "thermalData.h"

namespace vendor::samsung::hardware::thermal {

using android::hardware::thermal::V2_0::implementation::THROTTLING_SEVERITY_LEN;

struct Threshold {
	std::array<float, THROTTLING_SEVERITY_LEN> values;
	std::array<float, THROTTLING_SEVERITY_LEN> hyst;
};

std::unordered_map<std::string, std::unordered_map<std::string, Threshold>>
    thresholds = {
	{"SM-I610",
	 {
	     {"AP",
	      {{50, 58, 69, 73, 80, UNKNOWN_TEMPERATURE, UNKNOWN_TEMPERATURE},
	       {4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f}}},
	     {"LRP",
	      {{49, 53, 57, 62, 65, 67, 80},
	       {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}}},
	 }},
};
} // namespace vendor::samsung::hardware::thermal
