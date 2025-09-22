#ifndef SPIFFS_SETTING_H
#define SPIFFS_SETTING_H
#include <Arduino.h>
#include <string.h>
#include <variables_info.h>
#include <Preferences.h>
#include <ArduinoJson.h>
void loadClientsFromFile();
void saveClientsToFile();
void loadWifiCredentialsFromFile();
void saveWifiCredentialsToFile();
void saveGpioToFile();
void loadGpioFromFile();
void loadServerWorkTime();
void saveServerSetting();
#endif