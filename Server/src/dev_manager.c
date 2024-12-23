#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dev_manager.h"

DevInfo devices_ble[MAX_DEVICES];
int devCount = 0;

void addDevice(DevInfo dev) {
    if (devCount < MAX_DEVICES) {
        devices_ble[devCount++] = dev;
    }
}

DevInfo* findDevice(const char* ble_address) {
    for (int i = 0; i < devCount; i++) {
        if (strcmp(devices_ble[i].ble_address, ble_address) == 0) {
            return &devices_ble[i];
        }
    }
    return NULL;
}

void updateGpioToEnable(DevInfo* dev, int* gpioToEnable, int gpioCount) {
    memcpy(dev->gpioToEnable, gpioToEnable, gpioCount * sizeof(int));
}

void removeDevice(const char* ble_address) {
    for (int i = 0; i < devCount; ) {
        if (strcmp(devices_ble[i].ble_address, ble_address) == 0) {
            for (int j = i; j < devCount - 1; j++) {
                devices_ble[j] = devices_ble[j + 1];
            }
            devCount--;
        } else {
            i++;
        }
    }
}

void filterDevicesByTemp(DevInfo* findList[], int* findCount) {
    *findCount = 0;
    for (int i = 0; i < devCount; i++) {
        if (devices_ble[i].enabled && devices_ble[i].isConnected && (devices_ble[i].temp + 2) < devices_ble[i].targetTemp) {
            findList[*findCount] = &devices_ble[i];
            (*findCount)++;
        }
    }
}

void initDevicesFromBuffer(const DevInfo* buffer, int size) {
    if (size > MAX_DEVICES) {
        size = MAX_DEVICES;
    }
    memcpy(devices_ble, buffer, size * sizeof(DevInfo));
    devCount = size;
}
