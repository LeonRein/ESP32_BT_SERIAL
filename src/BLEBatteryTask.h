#pragma once

#include <stdint.h>

void startBLEBatteryTask(const char* device_name);
uint8_t batteryPercentage();