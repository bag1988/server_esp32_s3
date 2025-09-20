#include "xiaomi_scanner.h"
#include "variables_info.h"
#include <algorithm>
#include <spiffs_setting.h>
#include "logger.h"
// Глобальные переменные
BLEScan *pBLEScan = nullptr;
bool scanningActive = false;
BLEServer *pServer = nullptr;

// Класс для обработки результатов сканирования
class XiaomiAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        LOG_I("Обнаружено устройство: %s", advertisedDevice.getAddress().toString().c_str());

        if (advertisedDevice.haveName())
        {
            LOG_I("Имя: %s", advertisedDevice.getName().c_str());
        }

        if (advertisedDevice.haveManufacturerData())
        {
            std::string data = advertisedDevice.getManufacturerData();
            String payloadStr = "";
            for (int i = 0; i < data.length(); i++)
            {
                payloadStr += ("%02X ", (uint8_t)data[i]);
            }
            LOG_I("Данные производителя: %s", payloadStr);
        }

        if (advertisedDevice.haveServiceData())
        {
            LOG_I("Сервисные данные: ");
            for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
            {
                LOG_I("UUID: %s", advertisedDevice.getServiceDataUUID(i).toString().c_str());

                String payloadStr = "";
                std::string data = advertisedDevice.getServiceData(i);
                for (int j = 0; j < data.length(); j++)
                {
                    payloadStr += ("%02X ", (uint8_t)data[j]);
                }
                LOG_I("Данные: %s", payloadStr);
            }
        }
        if (advertisedDevice.getPayloadLength() > 0)
        {

            uint8_t *payload = advertisedDevice.getPayload();
            size_t payloadLength = advertisedDevice.getPayloadLength();
            String payloadStr = "";
            for (int i = 0; i < payloadLength - 1; i++)
            {
                payloadStr += ("%02X ", (uint8_t)payload[i]);
            }
            LOG_I("Payload данные: %s", payloadStr);
        }

        LOG_I("//////////////////////////////////");
        processXiaomiAdvertisement(advertisedDevice);
    }
};

// BLE +++++++++++++++++++++++++++++++++++
class SetServerSettingCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();

        if (pCharacteristic->getUUID().toString() == SSID_CHARACTERISTIC_UUID)
        {
            wifiCredentials.ssid = value;
            LOG_I("SSID received: %s", wifiCredentials.ssid.c_str());
        }
        else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID)
        {
            wifiCredentials.password = value;
            LOG_I("Password received: ", wifiCredentials.password.c_str());
        }
        saveWifiCredentialsToFile(); // Save credentials to file
    }
};

// Инициализация сканера BLE
void setupXiaomiScanner()
{
    BLEDevice::init(SERVER_NAME);
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new XiaomiAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Устанавливаем низкую мощность передатчика для экономии энергии
    // esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_N9); // -9 dBm

    LOG_I("Сканер датчиков Xiaomi инициализирован");
    LOG_I("Запускаем сервисы редактирования SSID и пароля");
    pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(WIFI_SERVICE_UUID);

    // SSID Characteristic
    BLECharacteristic *pSSIDCharacteristic = pService->createCharacteristic(
        SSID_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
    pSSIDCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
    pSSIDCharacteristic->setValue(wifiCredentials.ssid); // Set initial value

    // Password Characteristic
    BLECharacteristic *pPasswordCharacteristic = pService->createCharacteristic(
        PASSWORD_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
    pPasswordCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
    pPasswordCharacteristic->setValue(wifiCredentials.password); // Set initial value

    pSSIDCharacteristic->setCallbacks(new SetServerSettingCallbacks());
    pPasswordCharacteristic->setCallbacks(new SetServerSettingCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    LOG_I("Сервисы редактирования SSID и пароля запущены");
}

void startScan(uint32_t duration)
{
    BLEScanResults foundDevices = pBLEScan->start(duration, false);
    LOG_I("Найдено устройств: %s", foundDevices.getCount());
    LOG_I("Очистка резельтатов сканирования");
    pBLEScan->clearResults();
    scanningActive = false;
}

// Запуск сканирования BLE
void startXiaomiScan(uint32_t duration)
{
    LOG_I("Начало сканирования датчиков Xiaomi...");
    if (scanningActive)
    {
        LOG_I("Сканирования датчиков Xiaomi уже запущено, выход");
        return;
    }
    scanningActive = true;
    uint32_t *durationPtr = new uint32_t(duration);

    xTaskCreate([](void *parameter)
                {
                    uint32_t duration = *(uint32_t*)parameter;
                    startScan(duration);
                    delete (uint32_t*)parameter; // Освобождаем память
                    vTaskDelete(NULL); }, "scanBleDevice", 4096, durationPtr, 1, NULL);
}

// Обработка рекламного пакета
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice)
{
    bool isXiaomiDevice = false;
    std::string deviceAddress = advertisedDevice.getAddress().toString().c_str();
    bool isCustomFirmware = false;

    // Проверка на кастомную прошивку ATC
    if (advertisedDevice.haveServiceData())
    {
        LOG_I("Есть сервисные данные: %d", advertisedDevice.getServiceDataCount());
        for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
        {
            std::string serviceData = advertisedDevice.getServiceData(i);
            BLEUUID serviceUUID = advertisedDevice.getServiceDataUUID(i);

            // Проверка на ATC прошивку (UUID: 0x181A или 0xFE95)
            bool isAtc = serviceUUID.equals(BLEUUID((uint16_t)0x181A)) || serviceUUID.equals(BLEUUID("fe95"));
            LOG_I("Проверка на ATC прошивку пройдена: %s", isAtc ? "успешно" : "не успешно");
            if (isAtc)
            {
                if (serviceData.length() >= 15)
                {
                    // Проверка MAC-адреса в данных (для ATC)
                    uint8_t mac[6];
                    for (int j = 0; j < 6; j++)
                    {
                        mac[j] = (uint8_t)serviceData[5 - j];
                    }

                    // Сравниваем MAC в пакете с MAC устройства
                    BLEAddress packetAddr(mac);
                    bool equalsMac = packetAddr.equals(advertisedDevice.getAddress()); // || serviceData[0] == 0xA4 || serviceData[0] == 0x16;
                    LOG_I("Сравниваем MAC %s в пакете с MAC устройства: %s, равны: %s", advertisedDevice.getAddress().toString().c_str(), packetAddr.toString().c_str(), equalsMac ? "да" : "нет");

                    if (equalsMac)
                    {
                        isXiaomiDevice = true;
                        isCustomFirmware = true;
                        LOG_I("Обнаружено устройство с кастомной прошивкой ATC");
                    }
                }
            }
        }
    }

    // Если это устройство Xiaomi, обрабатываем его
    if (isXiaomiDevice)
    {
        // Парсим данные из рекламного пакета
        float temperature = 0.0;
        float humidity = 0;
        uint8_t battery = 0;
        uint16_t batteryV = 0;
        bool dataFound = false;

        // Обработка данных для устройств с кастомной прошивкой ATC
        if (isCustomFirmware && advertisedDevice.haveServiceData())
        {
            for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
            {
                std::string serviceData = advertisedDevice.getServiceData(i);
                BLEUUID serviceUUID = advertisedDevice.getServiceDataUUID(i);

                // Проверка на ATC прошивку (UUID: 0x181A или 0xFE95)
                if ((serviceUUID.equals(BLEUUID((uint16_t)0x181A)) || serviceUUID.equals(BLEUUID("fe95"))) && serviceData.length() >= 15)
                {
                    // Формат ATC: байты 10-11 - температура, байт 12 - влажность, байт 13 - батарея
                    LOG_I("Размер данных %d", serviceData.size());
                    int16_t temperatureRaw = 0;
                    int16_t humidityRaw = 0;
                    if (serviceData.size() == 15)
                    {
                        temperatureRaw = (int16_t)((uint8_t)serviceData[7] << 8 | (uint8_t)serviceData[6]);
                        temperature = temperatureRaw / 100.0f;

                        humidityRaw = (int16_t)((uint8_t)serviceData[9] << 8 | (uint8_t)serviceData[8]);
                        humidity = humidityRaw / 100.0f;

                        batteryV = (int16_t)((uint8_t)serviceData[11] << 8 | (uint8_t)serviceData[10]);

                        battery = (uint8_t)serviceData[12];
                    }

                    dataFound = true;
                    LOG_I("Парсинг данных ATC: Температура: %.1f°C, Влажность: %.1f%%, Батарея: %d%%, Напряжение: %d",
                          temperature, humidity, battery, batteryV);
                    break;
                }
            }
        }

        // Если данные найдены, обновляем информацию об устройстве
        if (dataFound)
        {
            if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
            {
                // Ищем устройство с таким MAC-адресом
                auto it = std::find_if(devices.begin(), devices.end(),
                                       [&deviceAddress](const DeviceData &device)
                                       {
                                           return device.macAddress == deviceAddress;
                                       });

                if (it != devices.end())
                {
                    // Устройство найдено, обновляем данные
                    it->updateSensorData(temperature, humidity, battery, batteryV);

                    LOG_I("Обновлены данные устройства: %s (%s) - Температура: %s°C, Влажность: %s%%, Батарея: %s%%"
                        , it->name.c_str(), deviceAddress.c_str(), temperature, humidity, battery);
                }
                else
                {
                    // Устройство не найдено, создаем новое
                    std::string deviceName = "Xiaomi " + deviceAddress.substr(deviceAddress.length() - 5);

                    // Если устройство имеет имя, используем его
                    if (advertisedDevice.haveName())
                    {
                        deviceName = advertisedDevice.getName().c_str();
                    }

                    DeviceData newDevice(deviceName, deviceAddress);
                    newDevice.updateSensorData(temperature, humidity, battery, batteryV);
                    devices.push_back(newDevice);

                    LOG_I("Найдено новое устройство: %s (%s)", deviceName.c_str(), deviceAddress.c_str());
                }
                xSemaphoreGive(devicesMutex);
            }
        }
        else
        {
            LOG_I("Устройство Xiaomi обнаружено, но данные не найдены: %s", deviceAddress.c_str());
        }
    }
}
