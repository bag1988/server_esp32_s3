#include "dev_info.h"
#include <sstream>
#include <algorithm>

DevInfo::DevInfo() : enabled(0), isConnected(0), targetTemp(0), temp(0), humidity(0), totalTimeActive(0)
{
    std::fill(std::begin(gpioToEnable), std::end(gpioToEnable), 0);
}

std::string DevInfo::toJson() const
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"ble_address\":\"" << ble_address << "\",";
    oss << "\"name\":\"" << name << "\",";
    oss << "\"enabled\":" << (enabled ? "true" : "false") << ",";
    oss << "\"isConnected\":" << (isConnected ? "true" : "false") << ",";

    oss << "\"gpioToEnable\":[";
    for (int i = 0; i < 10; ++i)
    {
        if (i > 0)
        {
            oss << ",";
        }
        oss << gpioToEnable[i];
    }
    oss << "],";

    oss << "\"targetTemp\":" << targetTemp << ",";
    oss << "\"temp\":" << temp << ",";
    oss << "\"humidity\":" << humidity << ",";
    oss << "\"totalTimeActive\":" << totalTimeActive;
    oss << "}";
    return oss.str();
}

DevInfo DevInfo::fromJson(const std::string &json)
{
    DevInfo dev;
    std::istringstream iss(json);
    std::string key;

    while (iss >> key)
    {
        if (key.find("\"ble_address\"") != std::string::npos)
        {
            iss.ignore(3); // Skip over ":" and first quote
            std::getline(iss, dev.ble_address, '\"');
        }
        else if (key.find("\"name\"") != std::string::npos)
        {
            iss.ignore(3);
            std::getline(iss, dev.name, '\"');
        }
        else if (key.find("\"enabled\"") != std::string::npos)
        {
            std::string value;
            iss >> value;
            dev.enabled = (value == "true");
        }
        else if (key.find("\"isConnected\"") != std::string::npos)
        {
            std::string value;
            iss >> value;
            dev.isConnected = (value == "true");
        }
        else if (key.find("\"gpioToEnable\"") != std::string::npos)
        {
            iss.ignore(2); // Skip over ":"
            char ch;
            for (int i = 0; i < 10; ++i)
            {
                iss >> dev.gpioToEnable[i];
                iss >> ch; // Read comma or closing bracket
            }
        }
        else if (key.find("\"targetTemp\"") != std::string::npos)
        {
            iss >> dev.targetTemp;
        }
        else if (key.find("\"temp\"") != std::string::npos)
        {
            iss >> dev.temp;
        }
        else if (key.find("\"humidity\"") != std::string::npos)
        {
            iss >> dev.humidity;
        }
        else if (key.find("\"totalTimeActive\"") != std::string::npos)
        {
            iss >> dev.totalTimeActive;
        }
    }
    return dev;
}

void addDevice(std::vector<DevInfo> &devs, const DevInfo &dev)
{
    devs.push_back(dev);
}

DevInfo *findDevice(std::vector<DevInfo> &devs, const std::string &ble_address)
{
    for (auto &dev : devs)
    {
        if (dev.ble_address == ble_address)
        {
            return &dev;
        }
    }
    return nullptr;
}

void updateGpioToEnable(DevInfo &dev, const std::vector<uint8_t> &gpioToEnable)
{
    std::copy(gpioToEnable.begin(), gpioToEnable.end(), dev.gpioToEnable);
}

std::vector<DevInfo> filterDevicesByTemp(std::vector<DevInfo> &devs, int deely_loop)
{
    std::vector<DevInfo> result;
    for (auto &dev : devs)
    {
        if (dev.enabled && dev.isConnected && (dev.temp + 2) < dev.targetTemp)
        {
            dev.totalTimeActive += deely_loop / 1000;
            result.push_back(dev);
        }
    }
    return result;
}

void initDevicesFromBuffer(std::vector<DevInfo> &devs, const char *buffer, size_t length)
{
    devs = parseDevicesFromBytes(buffer, length);
}

void updateDevices(std::vector<DevInfo> &devs, const std::vector<DevInfo> &newDevs)
{
    for (const auto &newDev : newDevs)
    {
        auto it = std::find_if(devs.begin(), devs.end(), [&newDev](const DevInfo &dev)
                               { return dev.ble_address == newDev.ble_address; });

        if (it != devs.end())
        {
            *it = newDev;
        }
        else
        {
            auto removeIt = std::remove_if(devs.begin(), devs.end(), [&newDev](const DevInfo &dev)
                                           { return dev.ble_address == newDev.ble_address; });
            devs.erase(removeIt, devs.end());
        }
    }
}
std::vector<DevInfo> parseDevicesFromBytes(const char *bytes, size_t length)
{
    std::vector<DevInfo> devs;
    std::string json(bytes, length);
    std::istringstream iss(json);
    std::string jsonStr;
    while (std::getline(iss, jsonStr, '}'))
    {
        jsonStr += '}';
        devs.push_back(DevInfo::fromJson(jsonStr));
        iss.ignore(1);
    }
    return devs;
}

std::vector<char> convertDevicesToBytes(const std::vector<DevInfo> &devs)
{
    std::string json = "[";
    for (size_t i = 0; i < devs.size(); ++i)
    {
        json += devs[i].toJson();
        if (i < devs.size() - 1)
        {
            json += ",";
        }
    }
    json += "]";
    return std::vector<char>(json.begin(), json.end());
}