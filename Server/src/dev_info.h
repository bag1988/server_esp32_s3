#ifndef DEV_INFO_H
#define DEV_INFO_H

#include <iostream>
#include <vector>
#include <string>

class DevInfo {
public:
    std::string ble_address;
    std::string name;
    uint8_t enabled;
    uint8_t isConnected;
    uint8_t gpioToEnable[10];
    float targetTemp;
    float temp;
    float humidity;
    int totalTimeActive;

    DevInfo();

    std::string toJson() const;
    static DevInfo fromJson(const std::string& json);
};

void addDevice(std::vector<DevInfo>& devs, const DevInfo& dev);
DevInfo* findDevice(std::vector<DevInfo>& devs, const std::string& ble_address);
void updateGpioToEnable(DevInfo& dev, const std::vector<uint8_t>& gpioToEnable);
std::vector<DevInfo> filterDevicesByTemp(std::vector<DevInfo>& devs, int deely_loop);
void initDevicesFromBuffer(std::vector<DevInfo>& devs, const char* buffer, size_t length);
void updateDevices(std::vector<DevInfo>& devs, const std::vector<DevInfo>& new_devs);
std::vector<DevInfo> parseDevicesFromBytes(const char* bytes, size_t length); 
std::vector<char> convertDevicesToBytes(const std::vector<DevInfo>& devs);
#endif // DEV_INFO_H
