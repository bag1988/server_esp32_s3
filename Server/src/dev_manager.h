#define DEV_MANAGER_H

//#include <stdbool.h>

#define MAX_GPIO 5
#define MAX_DEVICES 100

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

void addDevice(DevInfo dev);
DevInfo* findDevice(const char* ble_address);
void updateGpioToEnable(DevInfo* dev, int* gpioToEnable, int gpioCount);
void removeDevice(const char* ble_address);
void filterDevicesByTemp(DevInfo* findList[], int* findCount);
void initDevicesFromBuffer(const DevInfo* buffer, int size);

