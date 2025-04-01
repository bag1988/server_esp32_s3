#ifndef SPIFFS_SETTING_H
#define SPIFFS_SETTING_H
#include <Arduino.h>
#include <string.h>
void loadClientsFromFile();
void saveClientsToFile();
void loadWifiCredentialsFromFile();
void saveWifiCredentialsToFile();
void saveGpioToFile();
void loadGpioFromFile();
String loadDeviceToken();
void saveDeviceToken(const char* token);
#endif