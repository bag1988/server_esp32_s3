#ifndef XIAOMI_SCANNER_H
#define XIAOMI_SCANNER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "variables_info.h"
#include <ArduinoJson.h>

// Структура для эмуляции устройства Xiaomi
struct EmulatedXiaomiDevice {
    String did;           // Идентификатор устройства
    String model;         // Модель устройства
    String name;          // Имя устройства
    String type;          // Тип устройства (zigbee, ble, etc)
    bool online;          // Статус онлайн
    JsonObject properties; // Свойства устройства
};

// Функции для эмуляции устройств Xiaomi
void initXiaomiDeviceEmulation();
void updateEmulatedDevices();
String generateDeviceDid(const String& macAddress);
JsonObject getDeviceProperties(const DeviceData& device);


// Глобальные переменные
extern BLEScan* pBLEScan;
extern bool scanningActive;

// Функции
void setupXiaomiScanner();
void startXiaomiScan(uint32_t duration = XIAOMI_SCAN_DURATION);
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice);
bool parseXiaomiData(uint8_t* data, size_t length, float& temperature, uint8_t& humidity, uint8_t& battery);
void updateDevicesStatus();
void printDevicesData();

// Класс для обработки обнаруженных BLE устройств
class XiaomiScanCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice);
};
#endif // XIAOMI_SCANNER_H
