#include "device_manager.h"
#include "spiffs_setting.h"
#include "task_monitor.h"
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

QueueHandle_t deviceCmdQueue = nullptr;
TaskHandle_t deviceManagerTaskHandle = nullptr;

std::vector<DeviceData> g_devices;
std::vector<GpioPin> g_availableGpio;

extern unsigned long serverWorkTime;
extern float board_temperature;
extern float hysteresisTemp;

static unsigned long lastStatsSaveTime = 0;
static unsigned long lastGpioControlTime = 0;

static void controlGPIOInternal() {
    unsigned long elapsedTime = millis() - lastGpioControlTime;
    lastGpioControlTime = millis();

    std::vector<uint8_t> gpiosToTurnOn;

    for (auto& device : g_devices) {
        if (device.isDataValid()) {
            if (!device.heatingActive && device.enabled && (device.currentTemperature + hysteresisTemp) < device.targetTemperature) {
                logAndSend("Включаем обогрев для: " + String(device.name.c_str()));
                device.heatingActive = true;
            } else if (device.heatingActive && device.currentTemperature >= device.targetTemperature) {
                logAndSend("Выключаем обогрев для: " + String(device.name.c_str()));
                device.heatingActive = false;
            } else if (!device.enabled && device.heatingActive) {
                logAndSend("Устройство выключено. Выключаем обогрев для: " + String(device.name.c_str()));
                device.totalHeatingTime += elapsedTime;
                device.heatingActive = false;
            }

            if (device.heatingActive) {
                device.totalHeatingTime += elapsedTime;
                gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());
            }
        } else if (device.isOnline) {
            logAndSend("Устройство: " + String(device.name.c_str()) + " перешло в оффлайн");
            device.isOnline = false;
            device.totalHeatingTime += elapsedTime;
            device.heatingActive = false;
        }
    }

    for (auto& gpio : g_availableGpio) {
        bool shouldTurnOn = false;
        if (gpio.state == STATE_GPIO_AUTO) {
            shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio.pin) != gpiosToTurnOn.end();
        } else {
            shouldTurnOn = gpio.state == STATE_GPIO_ON;
        }

        digitalWrite(gpio.pin, shouldTurnOn ? HIGH : LOW);
        if (shouldTurnOn) {
            gpio.totalHeatingTime += elapsedTime;
        }
    }
}

static void handleCmd(const DeviceCmd& cmd, DeviceResponse* response) {
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
                logAndSend("Найдено новое устройство: " + String(deviceName.c_str()));
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
        case DeviceCmdType::CMD_SAVE_DEVICES: {
            SpiffsSetting::saveClientsToFile(g_devices);
            break;
        }
        case DeviceCmdType::CMD_LOAD_DEVICES: {
            SpiffsSetting::loadClientsFromFile(g_devices);
            break;
        }
        case DeviceCmdType::CMD_RESET_STATS: {
            for (auto& device : g_devices) {
                if (cmd.resetAll || device.macAddress == cmd.resetDeviceMac) {
                    device.totalHeatingTime = 0;
                    if (device.heatingActive) {
                        device.heatingStartTime = millis();
                    }
                }
            }
            SpiffsSetting::saveClientsToFile(g_devices);
            break;
        }
        case DeviceCmdType::CMD_GET_GPIO_STATS: {
            resp.gpioPins = g_availableGpio;
            break;
        }
        case DeviceCmdType::CMD_RESET_GPIO_STATS: {
            for (auto& gpio : g_availableGpio) {
                gpio.totalHeatingTime = 0;
            }
            SpiffsSetting::saveGpioToFile(g_availableGpio);
            break;
        }
        case DeviceCmdType::CMD_UPDATE_GPIO_CONFIG: {
            auto it = std::find_if(g_availableGpio.begin(), g_availableGpio.end(),
                [&cmd](const GpioPin& g) { return g.pin == cmd.gpioPin; });
            if (it != g_availableGpio.end()) {
                it->state = cmd.gpioState;
                if (!cmd.gpioName.empty()) it->name = cmd.gpioName;
                SpiffsSetting::saveGpioToFile(g_availableGpio);
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

static void deviceManagerTask(void* parameter) {
    esp_task_wdt_add(nullptr);
    
    // Register with task monitor
    TASK_MONITOR.registerTask(nullptr, "deviceManager", 5000, true, 
        [](void*) { initDeviceManager(); }, nullptr);

    DeviceCmd cmd;
    for (;;) {
        esp_task_wdt_reset();
        TASK_CHECK_IN();

        if (xQueueReceive(deviceCmdQueue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {
            DeviceResponse response{};
            handleCmd(cmd, &response);

            if (cmd.responseQueue != nullptr) {
                xQueueSend(cmd.responseQueue, &response, pdMS_TO_TICKS(100));
            }
        }

        unsigned long currentTime = millis();
        if ((currentTime - lastStatsSaveTime > 300000) || (currentTime < lastStatsSaveTime)) {
            logAndSend("Сохранение статистики согласно таймаута");
            SpiffsSetting::saveClientsToFile(g_devices);
            SpiffsSetting::saveGpioToFile(g_availableGpio);
            serverWorkTime += currentTime - lastStatsSaveTime;
            lastStatsSaveTime = currentTime;
            SpiffsSetting::saveServerSetting();
        }

        if (millis() - lastGpioControlTime > CONTROL_DELAY) {
            controlGPIOInternal();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void initDeviceManager() {
    SpiffsSetting::init();
    
    deviceCmdQueue = xQueueCreate(20, sizeof(DeviceCmd));
    if (deviceCmdQueue == nullptr) {
        logAndSend("Ошибка: не удалось создать очередь команд устройств");
        return;
    }

    SpiffsSetting::loadClientsFromFile(g_devices);
    SpiffsSetting::loadGpioFromFile(g_availableGpio);

    // Initialize default GPIO pins if empty
    if (g_availableGpio.empty()) {
        g_availableGpio.clear();
        g_availableGpio.emplace_back(GPIO_NUM_1, STATE_GPIO_AUTO, "GPIO 1");
        g_availableGpio.emplace_back(GPIO_NUM_4, STATE_GPIO_AUTO, "GPIO 4");
        g_availableGpio.emplace_back(GPIO_NUM_5, STATE_GPIO_AUTO, "GPIO 5");
        g_availableGpio.emplace_back(GPIO_NUM_6, STATE_GPIO_AUTO, "GPIO 6");
        g_availableGpio.emplace_back(GPIO_NUM_7, STATE_GPIO_AUTO, "GPIO 7");
    }

    for (auto& gpio : g_availableGpio) {
        pinMode(gpio.pin, OUTPUT);
        digitalWrite(gpio.pin, LOW);
    }

    xTaskCreate(
        deviceManagerTask,
        "deviceManager",
        8192,
        nullptr,
        1,
        &deviceManagerTaskHandle
    );

    // Update task monitor with actual handle
    if (deviceManagerTaskHandle) {
        TASK_MONITOR.registerTask(deviceManagerTaskHandle, "deviceManager", 5000, true, 
            [](void*) { initDeviceManager(); }, nullptr);
    }

    logAndSend("Device Manager запущен");
}

void sendDeviceCmd(const DeviceCmd& cmd, DeviceResponse* response, TickType_t timeout) {
    if (deviceCmdQueue == nullptr) {
        if (response) {
            response->success = false;
            response->errorMessage = "Device manager not initialized";
        }
        return;
    }

    QueueHandle_t respQueue = nullptr;
    if (response) {
        respQueue = xQueueCreate(1, sizeof(DeviceResponse));
    }

    DeviceCmd cmdCopy = cmd;
    cmdCopy.responseQueue = respQueue;

    if (xQueueSend(deviceCmdQueue, &cmdCopy, timeout) != pdTRUE) {
        if (respQueue) vQueueDelete(respQueue);
        if (response) {
            response->success = false;
            response->errorMessage = "Command queue full";
        }
        return;
    }

    if (respQueue) {
        if (xQueueReceive(respQueue, response, timeout) != pdTRUE) {
            response->success = false;
            response->errorMessage = "Response timeout";
        }
        vQueueDelete(respQueue);
    }
}

void sendDeviceCmdAsync(const DeviceCmd& cmd) {
    if (deviceCmdQueue != nullptr) {
        xQueueSend(deviceCmdQueue, &cmd, 0);
    }
}