#include "BLEBatteryTask.h"
#include "BLEBattery.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include "BLEBattery.h" // Ensure BLEBattery is declared before use

uint8_t batteryPercentage()
{
    float vBat = analogReadMilliVolts(35);
    vBat *= 2;    // we divided by 2, so multiply back
    vBat /= 1000; // convert to volts!
    float pBat = 128.7445 + (1.778399 - 128.7445) / pow(1 + pow(vBat / 3.689705, 116.3086), 0.1018804);
    if (pBat < 0) pBat = 0;
    else if (pBat > 100) pBat = 100;
    return (uint8_t)pBat;
}

static void batteryTask(void *pvParameters)
{
    static bool oldBleConnectionStatus = false;
    for (;;)
    {
        bool currentBleConnectionStatus = bleBattery.isClientConnected();

        // Handle BLE connection events
        if (!oldBleConnectionStatus && currentBleConnectionStatus)
        {
            Serial.println("Bluetooth client connected.");
            bleBattery.setBatteryLevel(batteryPercentage());
        }
        else if (oldBleConnectionStatus && !currentBleConnectionStatus)
        {
            Serial.println("Bluetooth client disconnected.");
            delay(1000);
            bleBattery.advertise();
        }
        oldBleConnectionStatus = currentBleConnectionStatus;

        // Periodically update battery level
        uint8_t battery_level = batteryPercentage();
        bleBattery.setBatteryLevel(battery_level);

        delay(5000); // Delay for 5 seconds
    }
}

void startBLEBatteryTask(const char* device_name)
{
    bleBattery.begin(device_name);
    xTaskCreatePinnedToCore(
        batteryTask,
        "Battery Task",
        4096,
        NULL,
        1,
        NULL,
        1
    );
}