#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <Arduino.h>
#include <string>
#include <vector>
#include "variables_info.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>

namespace NvsStorage {

// Custom error code for CRC mismatch
#define ESP_ERR_NVS_CRC_MISMATCH (ESP_ERR_NVS_BASE + 0x10)

// Initialize NVS partition
esp_err_t init();

// WiFi credentials
esp_err_t saveWifiCredentials(const std::string& ssid, const std::string& password);
esp_err_t loadWifiCredentials(std::string& ssid, std::string& password);

// Device data
esp_err_t saveDevices(const std::vector<DeviceData>& devices);
esp_err_t loadDevices(std::vector<DeviceData>& devices);

// GPIO config
esp_err_t saveGpioConfig(const std::vector<GpioPin>& gpioPins);
esp_err_t loadGpioConfig(std::vector<GpioPin>& gpioPins);

// Server settings
esp_err_t saveServerSettings(unsigned long serverWorkTime, float hysteresisTemp);
esp_err_t loadServerSettings(unsigned long& serverWorkTime, float& hysteresisTemp);

} // namespace NvsStorage

#endif // NVS_STORAGE_H