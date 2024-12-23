#define MAX_GPIO 5
#define MAX_DEVICES 10

typedef struct {
    char ble_address[18];
    char name[50];
    uint8_t enabled;
    uint8_t isConnected;
    int gpioToEnable[MAX_GPIO];
    float targetTemp;
    float temp;
    float humidity;
    int totalTimeActive;
} DevInfo;

extern DevInfo devices_ble[MAX_DEVICES];
extern int devCount;

// void addDevice(DevInfo dev);
// DevInfo* findDevice(const char* ble_address);
// void updateGpioToEnable(DevInfo* dev, int* gpioToEnable, int gpioCount);
// void removeDevice(const char* ble_address);
// void filterDevicesByTemp(DevInfo* findList[], int* findCount);
// void initDevicesFromBuffer(const DevInfo* buffer, int size);
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

char* convertArrayToJSON() {
    char* json = (char*)malloc(10240);  // Предполагаем, что 10 KB достаточно для JSON
    strcpy(json, "[");
    for (int i = 0; i < devCount; i++) {
        char devJson[1024];
        sprintf(devJson, "{");
        sprintf(devJson + strlen(devJson), "\"ble_address\":\"%s\",", devices_ble[i].ble_address);
        sprintf(devJson + strlen(devJson), "\"name\":\"%s\",", devices_ble[i].name);
        sprintf(devJson + strlen(devJson), "\"enabled\":%s,", devices_ble[i].enabled ? "true" : "false");
        sprintf(devJson + strlen(devJson), "\"isConnected\":%s,", devices_ble[i].isConnected ? "true" : "false");

        sprintf(devJson + strlen(devJson), "\"gpioToEnable\":[");
        for (int j = 0; j < 10; j++) {
            if (j > 0) {
                sprintf(devJson + strlen(devJson), ",");
            }
            sprintf(devJson + strlen(devJson), "%d", devices_ble[i].gpioToEnable[j]);
        }
        sprintf(devJson + strlen(devJson), "],");

        sprintf(devJson + strlen(devJson), "\"targetTemp\":%.2f,", devices_ble[i].targetTemp);
        sprintf(devJson + strlen(devJson), "\"temp\":%.2f,", devices_ble[i].temp);
        sprintf(devJson + strlen(devJson), "\"humidity\":%.2f,", devices_ble[i].humidity);
        sprintf(devJson + strlen(devJson), "\"totalTimeActive\":%d", devices_ble[i].totalTimeActive);
        sprintf(devJson + strlen(devJson), "}");

        if (i > 0) {
            strcat(json, ",");
        }
        strcat(json, devJson);
    }
    strcat(json, "]");
    return json;
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
            //*foundDev = newDevs[i];
        } else {
            removeDevice(newDevs[i].ble_address);
        }
    }
}

