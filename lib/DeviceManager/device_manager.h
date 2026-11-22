#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include "variables_info.h"

enum class DeviceCmdType : uint8_t {
    CMD_UPDATE_SENSOR_DATA,
    CMD_UPDATE_DEVICE_CONFIG,
    CMD_GET_DEVICES,
    CMD_GET_DEVICE,
    CMD_ADD_DEVICE,
    CMD_REMOVE_DEVICE,
    CMD_SAVE_DEVICES,
    CMD_LOAD_DEVICES,
    CMD_RESET_STATS,
    CMD_GET_GPIO_STATS,
    CMD_RESET_GPIO_STATS,
    CMD_UPDATE_GPIO_CONFIG,
    CMD_GET_SERVER_INFO
};

struct DeviceCmd {
    DeviceCmdType type;
    // For sensor data update
    std::string macAddress;
    float temperature;
    float humidity;
    uint8_t battery;
    uint16_t batteryV;
    // For device config update
    std::string name;
    float targetTemperature;
    bool enabled;
    std::vector<uint8_t> gpioPins;
    // For GPIO config update
    uint8_t gpioPin;
    uint8_t gpioState;
    std::string gpioName;
    // For stats reset
    bool resetAll;
    std::string resetDeviceMac;
    // Response queue handle (for sync commands)
    QueueHandle_t responseQueue;
};

struct DeviceResponse {
    bool success;
    std::string errorMessage;
    // For GET_DEVICES
    std::vector<DeviceData> devices;
    // For GET_DEVICE
    DeviceData device;
    bool deviceFound;
    // For server info
    unsigned long serverWorkTime;
    float boardTemperature;
    // For GPIO stats
    std::vector<GpioPin> gpioPins;
};

void initDeviceManager();
void sendDeviceCmd(const DeviceCmd& cmd, DeviceResponse* response = nullptr, TickType_t timeout = pdMS_TO_TICKS(5000));
void sendDeviceCmdAsync(const DeviceCmd& cmd);

extern QueueHandle_t deviceCmdQueue;
extern TaskHandle_t deviceManagerTaskHandle;

#endif // DEVICE_MANAGER_H