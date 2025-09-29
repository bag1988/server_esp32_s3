#include "xiaomi_scanner.h"
#include "variables_info.h"
#include <algorithm>
#include <spiffs_setting.h>

// Глобальные переменные
BLEScan *pBLEScan = nullptr;
bool scanningActive = false;
BLEServer *pServer = nullptr;
QueueHandle_t bleDataQueue = nullptr; // Очередь для передачи данных BLE
//  Класс для обработки результатов сканирования
class XiaomiAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Создаем структуру данных для передачи в очередь
        BLEDeviceData *deviceData = new BLEDeviceData();
        deviceData->address = advertisedDevice.getAddress().toString().c_str();
        Serial.println("Обнаружено устройство: " + String(deviceData->address.c_str()));

        deviceData->name = advertisedDevice.getName().c_str();

        if (advertisedDevice.haveServiceData() && advertisedDevice.getServiceDataCount() <= 5)
        {
            deviceData->hasServiceData = true;
            deviceData->serviceDataCount = advertisedDevice.getServiceDataCount();
            for (int i = 0; i < deviceData->serviceDataCount; i++)
            {
                deviceData->serviceData[i] = advertisedDevice.getServiceData(i);
                deviceData->serviceUUID[i] = advertisedDevice.getServiceDataUUID(i);
            }
        }
        else
        {
            Serial.println("Данных нет");
            delete deviceData;
            return;
        }

        // Отправляем данные в очередь
        if (bleDataQueue != nullptr)
        {
            if (xQueueSend(bleDataQueue, &deviceData, 0) != pdTRUE)
            {
                // Очередь заполнена, удаляем данные
                delete deviceData;
            }
        }
    }
};

// BLE +++++++++++++++++++++++++++++++++++
class SetServerSettingCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();

        // Проверяем длину значения перед обработкой
        if (value.length() > 100)
        { // Ограничиваем длину разумным значением
            Serial.println("Получено значение слишком большой длины: " + String(value.length()));
            return;
        }

        if (pCharacteristic->getUUID().toString() == SSID_CHARACTERISTIC_UUID)
        {
            wifiCredentials.ssid = value;
            Serial.println("SSID received: " + String(wifiCredentials.ssid.c_str()));
        }
        else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID)
        {
            wifiCredentials.password = value;
            Serial.println("Password received: " + String(wifiCredentials.password.c_str()));
        }
        saveWifiCredentialsToFile(); // Save credentials to file
    }
};

// Инициализация сканера BLE
void setupXiaomiScanner()
{
    // Создаем очередь для передачи данных BLE если она еще не создана
    if (bleDataQueue == nullptr)
    {
        bleDataQueue = xQueueCreate(10, sizeof(BLEDeviceData *));
    }

    BLEDevice::init(SERVER_NAME);
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new XiaomiAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(false); // Пассивное сканирование потребляет меньше ресурсов
                                    // pBLEScan->setInterval(XIAOMI_SCAN_INTERVAL); // Больше интервал
    // pBLEScan->setWindow(100);                    // Меньше окно сканирования
    // pBLEScan->start(0, false);                   // 0 = бесконечное сканирование

    Serial.println("Сканер датчиков Xiaomi инициализирован");
    Serial.println("Запускаем сервисы редактирования SSID и пароля");
    pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(WIFI_SERVICE_UUID);

    // SSID Characteristic
    BLECharacteristic *pSSIDCharacteristic = pService->createCharacteristic(
        SSID_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pSSIDCharacteristic->setValue("SSID");
    // Password Characteristic
    BLECharacteristic *pPasswordCharacteristic = pService->createCharacteristic(
        PASSWORD_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pPasswordCharacteristic->setValue("PASSWORD");

    pSSIDCharacteristic->setCallbacks(new SetServerSettingCallbacks());
    pPasswordCharacteristic->setCallbacks(new SetServerSettingCallbacks());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Сервисы редактирования SSID и пароля запущены");

    // Создаем задачу для обработки данных BLE из очереди
    xTaskCreate([](void *parameter)
                {
                    BLEDeviceData* deviceData;
                    while (true) {
                        // Ждем данные из очереди
                        if (bleDataQueue != nullptr && xQueueReceive(bleDataQueue, &deviceData, portMAX_DELAY) == pdTRUE) {
                            // Проверяем, что данные не нулевые
                            if (deviceData != nullptr) {
                                // Восстанавливаем данные для обработки
                                // Примечание: Из-за ограничений API мы не можем полностью восстановить объект
                                // Поэтому вызываем упрощенную версию обработки
                                processXiaomiAdvertisement(*deviceData);

                                // Освобождаем память
                                delete deviceData;
                            }
                        }
                    } }, "bleDataProcessor", 8192, nullptr, 1, nullptr);
}

void startScan(uint32_t duration)
{
    BLEScanResults foundDevices = pBLEScan->start(duration, false);
    Serial.println("Очистка резельтатов сканирования");
    pBLEScan->clearResults();
    scanningActive = false;
}

// Запуск сканирования BLE
void startXiaomiScan()
{
    Serial.println("Начало сканирования датчиков Xiaomi...");
    if (scanningActive)
    {
        Serial.println("Сканирования датчиков Xiaomi уже запущено, выход");
        return;
    }
    scanningActive = true;
    startScan(XIAOMI_SCAN_DURATION);
}

// Обработка рекламного пакета
void processXiaomiAdvertisement(BLEDeviceData &deviceData)
{
    bool isXiaomiDevice = false;
    std::string deviceAddress = deviceData.address;
    bool isCustomFirmware = false;

    // Проверка на кастомную прошивку ATC
    if (deviceData.hasServiceData && deviceData.serviceDataCount > 0)
    {
        Serial.println("Есть сервисные данные");
        for (int i = 0; i < deviceData.serviceDataCount; i++)
        {
            // Проверка на ATC прошивку (UUID: 0x181A или 0xFE95)
            bool isAtc = deviceData.serviceUUID[i].equals(BLEUUID((uint16_t)0x181A)) ||
                         deviceData.serviceUUID[i].equals(BLEUUID("fe95"));
            if (isAtc)
            {
                if (deviceData.serviceData[i].length() >= 15)
                {
                    // Проверка MAC-адреса в данных (для ATC)
                    uint8_t mac[6];
                    for (int j = 0; j < 6; j++)
                    {
                        mac[j] = (uint8_t)deviceData.serviceData[i][5 - j];
                    }

                    // Сравниваем MAC в пакете с MAC устройства
                    BLEAddress packetAddr(mac);
                    BLEAddress deviceAddr(deviceAddress.c_str());
                    bool equalsMac = packetAddr.equals(deviceAddr);

                    if (equalsMac)
                    {
                        isXiaomiDevice = true;
                        isCustomFirmware = true;
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
        if (isCustomFirmware && deviceData.hasServiceData)
        {
            for (int i = 0; i < deviceData.serviceDataCount; i++)
            {
                std::string serviceData = deviceData.serviceData[i];
                BLEUUID serviceUUID = deviceData.serviceUUID[i];

                // Проверка на ATC прошивку (UUID: 0x181A или 0xFE95)
                if ((serviceUUID.equals(BLEUUID((uint16_t)0x181A)) || serviceUUID.equals(BLEUUID("fe95"))) && serviceData.length() >= 15)
                {
                    // Формат ATC: байты 10-11 - температура, байт 12 - влажность, байт 13 - батарея

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

                    break;
                }
            }
        }

        // Если данные найдены, обновляем информацию об устройстве
        if (dataFound)
        {
            if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
            {
                //   Ищем устройство с таким MAC-адресом
                auto it = std::find_if(devices.begin(), devices.end(),
                                       [&deviceAddress](const DeviceData &device)
                                       {
                                           return device.macAddress == deviceAddress;
                                       });

                if (it != devices.end())
                {
                    Serial.println("Обновляем данные устройства: " + String(it->name.c_str()));
                    //   Устройство найдено, обновляем данные
                    it->updateSensorData(temperature, humidity, battery, batteryV);
                }
                else
                {
                    // Устройство не найдено, создаем новое
                    std::string deviceName = "Xiaomi " + deviceAddress.substr(deviceAddress.length() - 5);
                    //  Если устройство имеет имя, используем его
                    if (deviceData.hasName)
                    {
                        deviceName = deviceData.name;
                    }
                    Serial.println("Найдено новое устройство: " + String(deviceName.c_str()));

                    DeviceData newDevice(deviceName, deviceAddress);
                    newDevice.updateSensorData(temperature, humidity, battery, batteryV);
                    devices.push_back(newDevice);
                }
                xSemaphoreGive(devicesMutex);
            }
        }
        else
        {
            Serial.println("Устройство Xiaomi обнаружено, но данные не найдены: " + String(deviceAddress.c_str()));
        }
    }
}
