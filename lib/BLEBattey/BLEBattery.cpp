// C++
#include "BLEBattery.h"

bool _BLEClientConnected = false;

BLEBattery& bleBattery = BLEBattery::getInstance();

void MyServerCallbacks::onConnect(BLEServer *pServer)
{
    _BLEClientConnected = true;
}

void MyServerCallbacks::onDisconnect(BLEServer *pServer)
{
    _BLEClientConnected = false;
}

void BLEBattery::begin(String deviceName)
{
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pBattery = pServer->createService(BATTERY_SERVICE_UUID);

    _BatteryLevelCharacteristic = new BLECharacteristic(
        BATTERY_LEVEL_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    // _BatteryLevelDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    // _BatteryLevelDescriptor->setValue("Percentage 0 - 100");
    // _BatteryLevelCharacteristic->addDescriptor(_BatteryLevelDescriptor);
    _BatteryLevelCharacteristic->addDescriptor(new BLE2902());
    uint8_t batteryLevel = 100; // Default battery level
    _BatteryLevelCharacteristic->setValue(&batteryLevel, 1);


    pBattery->addCharacteristic(_BatteryLevelCharacteristic);
    pBattery->start();

    pServer->getAdvertising()->addServiceUUID(BATTERY_SERVICE_UUID);
    advertise();
}

void BLEBattery::advertise()
{
    if (pServer) pServer->getAdvertising()->start();
}

void BLEBattery::setBatteryLevel(uint8_t level)
{
    if (_BatteryLevelCharacteristic) {
        _BatteryLevelCharacteristic->setValue(&level, 1);
        _BatteryLevelCharacteristic->notify();
    }
}
