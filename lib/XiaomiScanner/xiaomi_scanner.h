#ifndef XIAOMI_SCANNER_H
#define XIAOMI_SCANNER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "variables_info.h"
#include <ArduinoJson.h>
// #define CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY 1
// #define CONFIG_BT_BTU_TASK_STACK_SIZE 4096
#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMPERATURE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define HUMIDITY_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"
#define WIFI_SERVICE_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"            // New WiFi Service UUID
#define SSID_CHARACTERISTIC_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"     // New SSID and password Characteristic UUID
#define PASSWORD_CHARACTERISTIC_UUID "c5481513-22cb-4aae-9fe3-e9db5d06bf6f" // New Password Characteristic UUID

// Глобальные переменные
extern BLEScan* pBLEScan;
extern bool scanningActive;
// Функции
void setupXiaomiScanner();
void startXiaomiScan(uint32_t duration = XIAOMI_SCAN_DURATION);
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice);
void printDevicesData();
#endif // XIAOMI_SCANNER_H
