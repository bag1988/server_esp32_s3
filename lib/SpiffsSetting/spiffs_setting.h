#ifndef SPIFFS_SETTING_H
#define SPIFFS_SETTING_H

#include <Arduino.h>
#include <string>
#include <vector>
#include "variables_info.h"
#include <ArduinoJson.h>

namespace SpiffsSetting {
    void loadClientsFromFile(std::vector<DeviceData>& devices);
    void saveClientsToFile(const std::vector<DeviceData>& devices);
    void loadWifiCredentialsFromFile();
    void saveWifiCredentialsToFile();
    void saveGpioToFile(const std::vector<GpioPin>& gpioPins);
    void loadGpioFromFile(std::vector<GpioPin>& gpioPins);
    void loadServerWorkTime();
    void saveServerSetting();
    void init();  // Initialize NVS
}

#endif