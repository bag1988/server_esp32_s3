#include "xiaomi_scanner.h"
#include "variables_info.h"
#include "device_manager.h"
#include "task_monitor.h"
#include <algorithm>
#include <spiffs_setting.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

// Global variables
BLEScan* pBLEScan = nullptr;
bool scanningActive = false;
BLEServer* pServer = nullptr;
QueueHandle_t bleEventQueue = nullptr;
TaskHandle_t bleScannerTaskHandle = nullptr;

// PSRAM buffer for BLE scan results
static const size_t BLE_BUFFER_SIZE = 8192;
static uint8_t* bleScanBuffer = nullptr;

// BLE Advertised Device Callbacks
class XiaomiAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Create event for queue
        BleEvent event;
        event.type = BleEventType::EVENT_DEVICE_FOUND;
        event.address = advertisedDevice.getAddress().toString().c_str();
        event.name = advertisedDevice.getName().c_str();
        event.hasName = advertisedDevice.haveName();
        
        if (advertisedDevice.haveServiceData() && advertisedDevice.getServiceDataCount() <= 5) {
            event.hasServiceData = true;
            event.serviceDataCount = advertisedDevice.getServiceDataCount();
            for (int i = 0; i < event.serviceDataCount; i++) {
                event.serviceData[i] = advertisedDevice.getServiceData(i);
                event.serviceUUID[i] = advertisedDevice.getServiceDataUUID(i);
            }
        }
        
        // Send to queue (non-blocking, drop if full)
        if (bleEventQueue != nullptr) {
            xQueueSend(bleEventQueue, &event, 0);
        }
    }
};

// BLE Server callbacks for WiFi config
class SetServerSettingCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        
        if (value.length() > 100) {
            logAndSend("Получено значение слишком большой длины: " + String(value.length()));
            return;
        }
        
        if (pCharacteristic->getUUID().toString() == SSID_CHARACTERISTIC_UUID) {
            wifiCredentials.ssid = value;
            logAndSend("SSID received: " + String(wifiCredentials.ssid.c_str()));
        } else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID) {
            wifiCredentials.password = value;
            logAndSend("Password received: " + String(wifiCredentials.password.c_str()));
        }
        SpiffsSetting::saveWifiCredentialsToFile();
    }
};

// Initialize PSRAM buffer for BLE scanning
static bool initBleScanBuffer() {
    if (bleScanBuffer == nullptr && psramFound()) {
        bleScanBuffer = (uint8_t*)heap_caps_malloc(BLE_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (bleScanBuffer) {
            logAndSend("BLE scan buffer allocated in PSRAM (" + String(BLE_BUFFER_SIZE) + " bytes)");
            return true;
        }
    }
    // Fallback to internal RAM
    if (bleScanBuffer == nullptr) {
        bleScanBuffer = new uint8_t[BLE_BUFFER_SIZE];
        if (bleScanBuffer) {
            logAndSend("BLE scan buffer allocated in internal RAM (" + String(BLE_BUFFER_SIZE) + " bytes)");
            return true;
        }
    }
    logAndSend("ERROR: Failed to allocate BLE scan buffer");
    return false;
}

void setupXiaomiScanner() {
    // Create event queue
    if (bleEventQueue == nullptr) {
        bleEventQueue = xQueueCreate(20, sizeof(BleEvent));
        if (bleEventQueue == nullptr) {
            logAndSend("ERROR: Failed to create BLE event queue");
            return;
        }
    }
    
    // Initialize PSRAM buffer
    initBleScanBuffer();
    
    BLEDevice::init(SERVER_NAME);
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new XiaomiAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(false);
    
    logAndSend("Сканер датчиков Xiaomi инициализирован");
    logAndSend("Запускаем сервисы редактирования SSID и пароля");
    
    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(WIFI_SERVICE_UUID);
    
    BLECharacteristic* pSSIDCharacteristic = pService->createCharacteristic(
        SSID_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pSSIDCharacteristic->setValue("SSID");
    
    BLECharacteristic* pPasswordCharacteristic = pService->createCharacteristic(
        PASSWORD_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pPasswordCharacteristic->setValue("PASSWORD");
    
    pSSIDCharacteristic->setCallbacks(new SetServerSettingCallbacks());
    pPasswordCharacteristic->setCallbacks(new SetServerSettingCallbacks());
    
    pService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    
    logAndSend("Сервисы редактирования SSID и пароля запущены");
    
    // Create BLE scanner task
    xTaskCreate(
        bleScannerTask,
        "bleScanner",
        8192,
        nullptr,
        1,
        &bleScannerTaskHandle
    );
}

void startScan(uint32_t duration) {
    if (!pBLEScan) return;
    
    BLEScanResults foundDevices = pBLEScan->start(duration, false);
    pBLEScan->clearResults();
    scanningActive = false;
    
    // Send scan complete event
    BleEvent event;
    event.type = BleEventType::EVENT_SCAN_COMPLETE;
    event.errorCode = foundDevices.getCount();
    if (bleEventQueue) {
        xQueueSend(bleEventQueue, &event, 0);
    }
}

void startXiaomiScan() {
    if (scanningActive) {
        logAndSend("Сканирование датчиков Xiaomi уже запущено, выход");
        return;
    }
    scanningActive = true;
    logAndSend("Начало сканирования датчиков Xiaоми...");
    startScan(XIAOMI_SCAN_DURATION);
}

// BLE Scanner Task - event-driven processing
void bleScannerTask(void* parameter) {
    esp_task_wdt_add(nullptr);
    
    // Register with task monitor
    TASK_MONITOR.registerTask(nullptr, "bleScanner", 30000, false,
        [](void*) { setupXiaomiScanner(); }, nullptr);
    
    BleEvent event;
    for (;;) {
        esp_task_wdt_reset();
        TASK_CHECK_IN();
        
        // Wait for events with timeout
        if (xQueueReceive(bleEventQueue, &event, pdMS_TO_TICKS(5000)) == pdTRUE) {
            switch (event.type) {
                case BleEventType::EVENT_DEVICE_FOUND: {
                    // Convert to BLEDeviceData for processing
                    BLEDeviceData deviceData;
                    deviceData.address = event.address;
                    deviceData.name = event.name;
                    deviceData.hasName = event.hasName;
                    deviceData.hasServiceData = event.hasServiceData;
                    deviceData.serviceDataCount = event.serviceDataCount;
                    for (int i = 0; i < event.serviceDataCount; i++) {
                        deviceData.serviceData[i] = event.serviceData[i];
                        deviceData.serviceUUID[i] = event.serviceUUID[i];
                    }
                    processXiaomiAdvertisement(deviceData);
                    break;
                }
                case BleEventType::EVENT_SCAN_COMPLETE: {
                    logAndSend("BLE scan complete, found " + String(event.errorCode) + " devices");
                    break;
                }
                case BleEventType::EVENT_SCAN_ERROR: {
                    logAndSend("BLE scan error: " + String(event.errorCode));
                    break;
                }
            }
        }
        
        // Periodic scan trigger (handled by network task via startXiaomiScan)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void processXiaomiAdvertisement(BLEDeviceData& deviceData) {
    bool isXiaomiDevice = false;
    std::string deviceAddress = deviceData.address;
    bool isCustomFirmware = false;
    
    if (deviceData.hasServiceData && deviceData.serviceDataCount > 0) {
        logAndSend("Есть сервисные данные");
        for (int i = 0; i < deviceData.serviceDataCount; i++) {
            bool isAtc = deviceData.serviceUUID[i].equals(BLEUUID((uint16_t)0x181A)) ||
                         deviceData.serviceUUID[i].equals(BLEUUID("fe95"));
            if (isAtc) {
                if (deviceData.serviceData[i].length() >= 15) {
                    uint8_t mac[6];
                    for (int j = 0; j < 6; j++) {
                        mac[j] = (uint8_t)deviceData.serviceData[i][5 - j];
                    }
                    
                    BLEAddress packetAddr(mac);
                    BLEAddress deviceAddr(deviceAddress.c_str());
                    bool equalsMac = packetAddr.equals(deviceAddr);
                    
                    if (equalsMac) {
                        isXiaomiDevice = true;
                        isCustomFirmware = true;
                    }
                }
            }
        }
    }
    
    if (isXiaomiDevice) {
        float temperature = 0.0;
        float humidity = 0;
        uint8_t battery = 0;
        uint16_t batteryV = 0;
        bool dataFound = false;
        
        if (isCustomFirmware && deviceData.hasServiceData) {
            for (int i = 0; i < deviceData.serviceDataCount; i++) {
                std::string serviceData = deviceData.serviceData[i];
                BLEUUID serviceUUID = deviceData.serviceUUID[i];
                
                if ((serviceUUID.equals(BLEUUID((uint16_t)0x181A)) || serviceUUID.equals(BLEUUID("fe95"))) && serviceData.length() >= 15) {
                    int16_t temperatureRaw = 0;
                    int16_t humidityRaw = 0;
                    if (serviceData.size() == 15) {
                        temperatureRaw = (int16_t)((uint8_t)serviceData[7] << 8 | (uint8_t)serviceData[6]);
                        temperature = temperatureRaw / 100.0f;
                        
                        humidityRaw = (int16_t)((uint8_t)serviceData[9] << 8 | (uint8_t)serviceData[8]);
                        humidity = humidityRaw / 100.0f;
                        
                        batteryV = (int16_t)((uint8_t)serviceData[11] << 8 | (uint8_t)serviceData[10]);
                        battery = (uint8_t)serviceData[12];
                    }
                    
                    dataFound = true;
                    break;
                }
            }
        }
        
        if (dataFound) {
            DeviceCmd cmd{};
            cmd.type = DeviceCmdType::CMD_UPDATE_SENSOR_DATA;
            cmd.macAddress = deviceAddress;
            cmd.temperature = temperature;
            cmd.humidity = humidity;
            cmd.battery = battery;
            cmd.batteryV = batteryV;
            sendDeviceCmdAsync(cmd);
        } else {
            logAndSend("Устройство Xiaomi обнаружено, но данные не найдены: " + String(deviceAddress.c_str()));
        }
    }
}

void printDevicesData() {
    DeviceResponse response;
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_GET_DEVICES;
    sendDeviceCmd(cmd, &response);
    
    if (response.success) {
        for (const auto& device : response.devices) {
            logAndSend("Device: " + String(device.name.c_str()) + 
                       ", MAC: " + String(device.macAddress.c_str()) + 
                       ", Temp: " + String(device.currentTemperature) + 
                       ", Hum: " + String(device.humidity) + 
                       ", Bat: " + String(device.battery));
        }
    }
}