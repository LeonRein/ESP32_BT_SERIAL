#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>

class CRC32 {
public:
    static uint32_t calculate(const uint8_t* data, size_t len);
};

template<typename T>
class ConfigManager {
public:
    explicit ConfigManager(int eepromStart = 0)
        : _eepromStart(eepromStart), _structPtr(nullptr) {}

    // Returns true if loaded from EEPROM (CRC OK), false if defaults used
    bool begin(T* structPtr) {
        _structPtr = structPtr;
        uint8_t buf[totalLen];

        EEPROM.begin(totalLen);
        readFromEEPROM(buf);

        uint32_t storedCrc;
        memcpy(&storedCrc, buf + dataLen, sizeof(uint32_t));
        uint32_t calcCrc = CRC32::calculate(buf, dataLen);
        if (storedCrc == calcCrc) {
            memcpy(structPtr, buf, dataLen);
            return true;
        } else {
            save(structPtr); // Save default if CRC mismatch
            return false;
        }
    }

    void save(const T* structPtr = nullptr) {
        if (!structPtr) structPtr = _structPtr;
        if (!structPtr) return;

        uint8_t buf[totalLen];
        memcpy(buf, structPtr, dataLen);
        uint32_t crc = CRC32::calculate(buf, dataLen);
        memcpy(buf + dataLen, &crc, sizeof(uint32_t));

        EEPROM.begin(totalLen);
        writeToEEPROM(buf);
    }

private:
    int _eepromStart;
    T* _structPtr;
    static constexpr size_t dataLen = sizeof(T);
    static constexpr size_t totalLen = dataLen + sizeof(uint32_t);

    void readFromEEPROM(uint8_t* buf) const {
        for (size_t i = 0; i < totalLen; ++i)
            buf[i] = EEPROM.read(_eepromStart + i);
    }

    void writeToEEPROM(const uint8_t* buf) const {
        for (size_t i = 0; i < totalLen; ++i)
            EEPROM.write(_eepromStart + i, buf[i]);
        EEPROM.commit();
    }
};