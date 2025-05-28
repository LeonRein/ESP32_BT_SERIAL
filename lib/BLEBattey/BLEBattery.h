#ifndef BLEBattery_h
#define BLEBattery_h

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h> //Library to use BLE as server
#include <BLE2902.h> 

#define BATTERY_SERVICE_UUID        BLEUUID((uint16_t)0x180F)
#define BATTERY_LEVEL_UUID          BLEUUID((uint16_t)0x2A19)

extern bool _BLEClientConnected;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer);
};

class BLEBattery {
private:
    BLEBattery() = default;
    ~BLEBattery() = default;
    BLEBattery(const BLEBattery&) = delete;

    BLECharacteristic* _BatteryLevelCharacteristic = nullptr;
    BLEDescriptor* _BatteryLevelDescriptor = nullptr;
    BLEServer* pServer = nullptr;

public:
    void begin(String deviceName);
    void advertise();
    void setBatteryLevel(uint8_t level);
    bool isClientConnected() { return _BLEClientConnected; }
    static BLEBattery& getInstance() {
        static BLEBattery instance;
        return instance;
    }
    
};

// Only declare here
extern BLEBattery& bleBattery;

#endif // BLEBattery_h