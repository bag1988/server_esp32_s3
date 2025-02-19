#ifndef BLE_SETTING_H
#define BLE_SETTING_H

#include <BLEServer.h>

extern BLEServer *pServer;

void handleWifiSetupBLE();
void startBLEScan();

#endif