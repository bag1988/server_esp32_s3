#define MAX_GPIO 5
#define MAX_DEVICES 10

typedef struct {
    char ble_address[18];
    char name[50];
    uint8_t enabled;
    uint8_t isConnected;
    uint8_t gpioToEnable[MAX_GPIO];
    float targetTemp;
    float temp;
    float humidity;
    int totalTimeActive;
} DevInfo;

// RTC_DATA_ATTR extern DevInfo devices_ble[MAX_DEVICES];
// RTC_DATA_ATTR extern int devCount;

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

void filterDevicesByTemp(DevInfo* findList[], int* findCount, int deely_loop) {
    *findCount = 0;
    for (int i = 0; i < devCount; i++) {
        if (devices_ble[i].enabled && devices_ble[i].isConnected && (devices_ble[i].temp + 2) < devices_ble[i].targetTemp) {
            devices_ble[i].totalTimeActive +=deely_loop/1000;
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

char* convertArrayToJSON() {
    // Буфер для JSON строки
    char* jsonString = (char*)malloc(10240);
    strcpy(jsonString, "[");

    for (int i = 0; i < devCount; i++) {
        char deviceJson[1024];
        strcpy(deviceJson, "{");

        // Конкатенация поля ble_address
        strcat(deviceJson, "\"ble_address\":\"");
        strcat(deviceJson, devices_ble[i].ble_address);
        strcat(deviceJson, "\",");

        // Конкатенация поля name
        strcat(deviceJson, "\"name\":\"");
        strcat(deviceJson, devices_ble[i].name);
        strcat(deviceJson, "\",");

        // Конкатенация поля enabled
        strcat(deviceJson, "\"enabled\":");
        strcat(deviceJson, devices_ble[i].enabled ? "true" : "false");
        strcat(deviceJson, ",");

        // Конкатенация поля isConnected
        strcat(deviceJson, "\"isConnected\":");
        strcat(deviceJson, devices_ble[i].isConnected ? "true" : "false");
        strcat(deviceJson, ",");

        // Конкатенация массива gpioToEnable
        strcat(deviceJson, "\"gpioToEnable\":[");
        for (int j = 0; j < MAX_GPIO; j++) {
            if (j > 0) {
                strcat(deviceJson, ",");
            }
            char gpioStr[12];
            sprintf(gpioStr, "%d", devices_ble[i].gpioToEnable[j]);
            strcat(deviceJson, gpioStr);
        }
        strcat(deviceJson, "],");

        // Конкатенация полей targetTemp, temp, humidity и totalTimeActive
        char tempStr[64];
        snprintf(tempStr, sizeof(tempStr), "\"targetTemp\":%.2f,", devices_ble[i].targetTemp);
        strcat(deviceJson, tempStr);

        snprintf(tempStr, sizeof(tempStr), "\"temp\":%.2f,", devices_ble[i].temp);
        strcat(deviceJson, tempStr);

        snprintf(tempStr, sizeof(tempStr), "\"humidity\":%.2f,", devices_ble[i].humidity);
        strcat(deviceJson, tempStr);

        snprintf(tempStr, sizeof(tempStr), "\"totalTimeActive\":%d", devices_ble[i].totalTimeActive);
        strcat(deviceJson, tempStr);

        strcat(deviceJson, "}");

        if (i > 0) {
            strcat(jsonString, ",");
        }
        strcat(jsonString, deviceJson);
    }
    strcat(jsonString, "]");

    return jsonString;
}

void parseArrayFromJSON(const char* json, DevInfo* devs, int* updateDevCount) {
    *updateDevCount = 0;
    const char* ptr = json;
    while (*ptr) {
        if (*ptr == '{') {
            DevInfo dev;
            char enabledStr[6];
            char isConnectedStr[6];
            
            sscanf(ptr, "{\"ble_address\":\"%17[^\"]\",\"name\":\"%49[^\"]\",\"enabled\":%5[^,],\"isConnected\":%5[^,],\"gpioToEnable\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d],\"targetTemp\":%f,\"temp\":%f,\"humidity\":%f,\"totalTimeActive\":%d}",
                   dev.ble_address, dev.name, enabledStr, isConnectedStr,
                   &dev.gpioToEnable[0], &dev.gpioToEnable[1], &dev.gpioToEnable[2], &dev.gpioToEnable[3], &dev.gpioToEnable[4],
                   &dev.gpioToEnable[5], &dev.gpioToEnable[6], &dev.gpioToEnable[7], &dev.gpioToEnable[8], &dev.gpioToEnable[9],
                   &dev.targetTemp, &dev.temp, &dev.humidity, &dev.totalTimeActive);

            dev.enabled = strcmp(enabledStr, "true") == 0 ? 1 : 0;
            dev.isConnected = strcmp(isConnectedStr, "true") == 0 ? 1 : 0;

            devs[(*updateDevCount)++] = dev;
        }
        ptr++;
    }
}

void updateDevices(DevInfo* newDevs, int newDevCount) {
    for (int i = 0; i < newDevCount; i++) {
        DevInfo* foundDev = NULL;
        for (int j = 0; j < devCount; j++) {
            if (strcmp(devices_ble[j].ble_address, newDevs[i].ble_address) == 0) {
                foundDev = &devices_ble[j];
                break;
            }
        }
        
        if (foundDev != NULL) {
            foundDev->enabled = newDevs[i].enabled;
            memcpy(foundDev->name, newDevs[i].name, sizeof foundDev->name);
            memcpy(foundDev->gpioToEnable, newDevs[i].gpioToEnable, sizeof newDevs[i].gpioToEnable);
        } else {
            removeDevice(newDevs[i].ble_address);
        }
    }
}

