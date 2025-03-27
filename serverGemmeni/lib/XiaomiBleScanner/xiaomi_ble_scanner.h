#ifndef XIAOMI_BLE_SCANNER_H
#define XIAOMI_BLE_SCANNER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <map>

// Структура для хранения данных датчика
struct XiaomiSensorData {
    String address;
    float temperature;
    uint8_t humidity;
    uint8_t battery;
    uint32_t lastUpdate;
    bool isOnline;
    String name;
};

// Объявление глобальных переменных и функций
extern std::map<String, XiaomiSensorData> xiaomiSensors;
extern BLEScan* pBLEScan;

void setupXiaomiScanner();
void startXiaomiScan(uint32_t duration = 5);
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice);
bool parseXiaomiData(uint8_t* data, size_t length, XiaomiSensorData& sensorData);
void printSensorData();

#endif // XIAOMI_BLE_SCANNER_H
