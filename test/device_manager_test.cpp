#include <unity.h>
#include "arduino_mocks.h"
#include <vector>
#include <string>
#include <queue>
#include <algorithm>

// Include DeviceManager structures
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
    std::string macAddress;
    float temperature;
    float humidity;
    uint8_t battery;
    uint16_t batteryV;
    std::string name;
    float targetTemperature;
    bool enabled;
    std::vector<uint8_t> gpioPins;
    uint8_t gpioPin;
    uint8_t gpioState;
    std::string gpioName;
    bool resetAll;
    std::string resetDeviceMac;
    void* responseQueue;
};

struct DeviceResponse {
    bool success;
    std::string errorMessage;
    std::vector<DeviceData> devices;
    DeviceData device;
    bool deviceFound;
    unsigned long serverWorkTime;
    float boardTemperature;
    std::vector<GpioPin> gpioPins;
};

// Mock DeviceManager functions
std::queue<DeviceCmd> cmd_queue;
std::vector<DeviceData> g_devices;
std::vector<GpioPin> g_availableGpio;
float hysteresisTemp = 1.5;
unsigned long serverWorkTime = 0;
float board_temperature = 25.0;

void handleCmd(const DeviceCmd& cmd, DeviceResponse* response) {
    DeviceResponse resp{};
    resp.success = true;

    switch (cmd.type) {
        case DeviceCmdType::CMD_UPDATE_SENSOR_DATA: {
            auto it = std::find_if(g_devices.begin(), g_devices.end(),
                [&cmd](const DeviceData& d) { return d.macAddress == cmd.macAddress; });
            if (it != g_devices.end()) {
                it->updateSensorData(cmd.temperature, cmd.humidity, cmd.battery, cmd.batteryV);
            } else {
                std::string deviceName = "Xiaomi " + cmd.macAddress.substr(cmd.macAddress.length() - 5);
                DeviceData newDevice(deviceName, cmd.macAddress);
                newDevice.updateSensorData(cmd.temperature, cmd.humidity, cmd.battery, cmd.batteryV);
                g_devices.push_back(newDevice);
            }
            break;
        }
        case DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG: {
            auto it = std::find_if(g_devices.begin(), g_devices.end(),
                [&cmd](const DeviceData& d) { return d.macAddress == cmd.macAddress; });
            if (it != g_devices.end()) {
                if (!cmd.name.empty()) it->name = cmd.name;
                if (cmd.targetTemperature > 0) it->targetTemperature = cmd.targetTemperature;
                it->enabled = cmd.enabled;
                if (!cmd.gpioPins.empty()) it->gpioPins = cmd.gpioPins;
            } else {
                resp.success = false;
                resp.errorMessage = "Device not found";
            }
            break;
        }
        case DeviceCmdType::CMD_GET_DEVICES: {
            resp.devices = g_devices;
            break;
        }
        case DeviceCmdType::CMD_GET_DEVICE: {
            auto it = std::find_if(g_devices.begin(), g_devices.end(),
                [&cmd](const DeviceData& d) { return d.macAddress == cmd.macAddress; });
            if (it != g_devices.end()) {
                resp.device = *it;
                resp.deviceFound = true;
            } else {
                resp.deviceFound = false;
            }
            break;
        }
        case DeviceCmdType::CMD_ADD_DEVICE: {
            DeviceData newDevice(cmd.name, cmd.macAddress);
            newDevice.targetTemperature = cmd.targetTemperature;
            newDevice.enabled = cmd.enabled;
            newDevice.gpioPins = cmd.gpioPins;
            g_devices.push_back(newDevice);
            break;
        }
        case DeviceCmdType::CMD_REMOVE_DEVICE: {
            g_devices.erase(
                std::remove_if(g_devices.begin(), g_devices.end(),
                    [&cmd](const DeviceData& d) { return d.macAddress == cmd.macAddress; }),
                g_devices.end()
            );
            break;
        }
        case DeviceCmdType::CMD_GET_GPIO_STATS: {
            resp.gpioPins = g_availableGpio;
            break;
        }
        case DeviceCmdType::CMD_UPDATE_GPIO_CONFIG: {
            auto it = std::find_if(g_availableGpio.begin(), g_availableGpio.end(),
                [&cmd](const GpioPin& g) { return g.pin == cmd.gpioPin; });
            if (it != g_availableGpio.end()) {
                it->state = cmd.gpioState;
                if (!cmd.gpioName.empty()) it->name = cmd.gpioName;
            } else {
                resp.success = false;
                resp.errorMessage = "GPIO pin not found";
            }
            break;
        }
        case DeviceCmdType::CMD_GET_SERVER_INFO: {
            resp.serverWorkTime = serverWorkTime;
            resp.boardTemperature = board_temperature;
            break;
        }
    }

    if (response) {
        *response = resp;
    }
}

void setUp(void) {
    g_devices.clear();
    g_availableGpio = {
        {1, STATE_GPIO_AUTO, "GPIO 1", 0},
        {4, STATE_GPIO_AUTO, "GPIO 4", 0}
    };
    hysteresisTemp = 1.5;
    serverWorkTime = 1000;
    board_temperature = 25.0;
    while (!cmd_queue.empty()) cmd_queue.pop();
}

void tearDown(void) {
}

void test_update_sensor_data_creates_new_device(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_SENSOR_DATA;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.temperature = 23.5;
    cmd.humidity = 45.0;
    cmd.battery = 80;
    cmd.batteryV = 3000;
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(1, g_devices.size());
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:ff", g_devices[0].macAddress.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 23.5, g_devices[0].currentTemperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 45.0, g_devices[0].humidity);
    TEST_ASSERT_EQUAL(80, g_devices[0].battery);
    TEST_ASSERT_TRUE(g_devices[0].isOnline);
}

void test_update_sensor_data_updates_existing_device(void) {
    DeviceData device("Existing", "aa:bb:cc:dd:ee:ff");
    device.targetTemperature = 25.0;
    g_devices.push_back(device);
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_SENSOR_DATA;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.temperature = 24.0;
    cmd.humidity = 50.0;
    cmd.battery = 75;
    cmd.batteryV = 2950;
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(1, g_devices.size());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 24.0, g_devices[0].currentTemperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 50.0, g_devices[0].humidity);
    TEST_ASSERT_EQUAL(75, g_devices[0].battery);
}

void test_get_devices_returns_all(void) {
    g_devices.push_back(DeviceData("Device 1", "aa:bb:cc:dd:ee:01"));
    g_devices.push_back(DeviceData("Device 2", "aa:bb:cc:dd:ee:02"));
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_GET_DEVICES;
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(2, response.devices.size());
}

void test_get_device_found(void) {
    g_devices.push_back(DeviceData("Test Device", "aa:bb:cc:dd:ee:ff"));
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_GET_DEVICE;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_TRUE(response.deviceFound);
    TEST_ASSERT_EQUAL_STRING("Test Device", response.device.name.c_str());
}

void test_get_device_not_found(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_GET_DEVICE;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_FALSE(response.deviceFound);
}

void test_update_device_config(void) {
    g_devices.push_back(DeviceData("Old Name", "aa:bb:cc:dd:ee:ff"));
    g_devices[0].targetTemperature = 22.0;
    g_devices[0].enabled = false;
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.name = "New Name";
    cmd.targetTemperature = 26.0;
    cmd.enabled = true;
    cmd.gpioPins = {1, 4};
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL_STRING("New Name", g_devices[0].name.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 26.0, g_devices[0].targetTemperature);
    TEST_ASSERT_TRUE(g_devices[0].enabled);
    TEST_ASSERT_EQUAL(2, g_devices[0].gpioPins.size());
    TEST_ASSERT_EQUAL(1, g_devices[0].gpioPins[0]);
    TEST_ASSERT_EQUAL(4, g_devices[0].gpioPins[1]);
}

void test_update_device_config_not_found(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.name = "New Name";
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_FALSE(response.success);
    TEST_ASSERT_EQUAL_STRING("Device not found", response.errorMessage.c_str());
}

void test_add_device(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_ADD_DEVICE;
    cmd.name = "New Device";
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.targetTemperature = 24.0;
    cmd.enabled = true;
    cmd.gpioPins = {1};
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(1, g_devices.size());
    TEST_ASSERT_EQUAL_STRING("New Device", g_devices[0].name.c_str());
}

void test_remove_device(void) {
    g_devices.push_back(DeviceData("Device 1", "aa:bb:cc:dd:ee:01"));
    g_devices.push_back(DeviceData("Device 2", "aa:bb:cc:dd:ee:02"));
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_REMOVE_DEVICE;
    cmd.macAddress = "aa:bb:cc:dd:ee:01";
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(1, g_devices.size());
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:02", g_devices[0].macAddress.c_str());
}

void test_update_gpio_config(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_GPIO_CONFIG;
    cmd.gpioPin = 1;
    cmd.gpioState = STATE_GPIO_ON;
    cmd.gpioName = "Heater 1";
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(STATE_GPIO_ON, g_availableGpio[0].state);
    TEST_ASSERT_EQUAL_STRING("Heater 1", g_availableGpio[0].name.c_str());
}

void test_update_gpio_config_not_found(void) {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_UPDATE_GPIO_CONFIG;
    cmd.gpioPin = 99;
    cmd.gpioState = STATE_GPIO_ON;
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_FALSE(response.success);
    TEST_ASSERT_EQUAL_STRING("GPIO pin not found", response.errorMessage.c_str());
}

void test_get_server_info(void) {
    serverWorkTime = 3600000;
    board_temperature = 42.5;
    
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_GET_SERVER_INFO;
    
    DeviceResponse response;
    handleCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_EQUAL(3600000, response.serverWorkTime);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 42.5, response.boardTemperature);
}