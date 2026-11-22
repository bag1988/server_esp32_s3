#include <spiffs_setting.h>
#include "nvs_storage.h"
#include "variables_info.h"

namespace SpiffsSetting {

void init() {
    NvsStorage::init();
}

void loadClientsFromFile(std::vector<DeviceData>& devices) {
    logAndSend("Loading clients from NVS...");
    esp_err_t err = NvsStorage::loadDevices(devices);
    if (err != ESP_OK) {
        logAndSend("Failed to load devices from NVS: " + String(esp_err_to_name(err)));
    }
}

void saveClientsToFile(const std::vector<DeviceData>& devices) {
    logAndSend("Saving clients to NVS...");
    esp_err_t err = NvsStorage::saveDevices(devices);
    if (err != ESP_OK) {
        logAndSend("Failed to save devices to NVS: " + String(esp_err_to_name(err)));
    }
}

void loadWifiCredentialsFromFile() {
    std::string ssid, password;
    esp_err_t err = NvsStorage::loadWifiCredentials(ssid, password);
    if (err == ESP_OK) {
        wifiCredentials.ssid = ssid;
        wifiCredentials.password = password;
        logAndSend("WiFi credentials loaded from NVS");
    } else {
        logAndSend("Failed to load WiFi credentials from NVS: " + String(esp_err_to_name(err)));
    }
}

void saveWifiCredentialsToFile() {
    esp_err_t err = NvsStorage::saveWifiCredentials(wifiCredentials.ssid, wifiCredentials.password);
    if (err == ESP_OK) {
        logAndSend("WiFi credentials saved to NVS");
    } else {
        logAndSend("Failed to save WiFi credentials to NVS: " + String(esp_err_to_name(err)));
    }
}

void saveGpioToFile(const std::vector<GpioPin>& gpioPins) {
    logAndSend("Saving GPIO to NVS...");
    esp_err_t err = NvsStorage::saveGpioConfig(gpioPins);
    if (err != ESP_OK) {
        logAndSend("Failed to save GPIO to NVS: " + String(esp_err_to_name(err)));
    }
}

void loadGpioFromFile(std::vector<GpioPin>& gpioPins) {
    logAndSend("Loading GPIO from NVS...");
    esp_err_t err = NvsStorage::loadGpioConfig(gpioPins);
    if (err != ESP_OK) {
        logAndSend("Failed to load GPIO from NVS: " + String(esp_err_to_name(err)));
    }
}

void loadServerWorkTime() {
    unsigned long serverTime = 0;
    float hysteresis = 1.5;
    esp_err_t err = NvsStorage::loadServerSettings(serverTime, hysteresis);
    if (err == ESP_OK) {
        serverWorkTime = serverTime;
        hysteresisTemp = hysteresis;
        logAndSend("Server settings loaded from NVS");
    } else {
        logAndSend("Failed to load server settings from NVS: " + String(esp_err_to_name(err)));
    }
}

void saveServerSetting() {
    esp_err_t err = NvsStorage::saveServerSettings(serverWorkTime, hysteresisTemp);
    if (err != ESP_OK) {
        logAndSend("Failed to save server settings to NVS: " + String(esp_err_to_name(err)));
    }
}

} // namespace SpiffsSetting