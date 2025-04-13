#include "xiaomi_scanner.h"
#include "variables_info.h"
#include <algorithm>
#include <lcd_setting.h>
#include <spiffs_setting.h>
// Глобальные переменные
BLEScan *pBLEScan = nullptr;
bool scanningActive = false;
BLEServer *pServer = nullptr;
// Глобальный массив эмулируемых устройств
std::vector<EmulatedXiaomiDevice> emulatedDevices;

// Класс для обработки результатов сканирования
class XiaomiAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Обнаружено устройство: 66:c6:d2:0f:8c:71 Имя: L3250 Series Данные производителя: 40 00 00 6D 02
        // Обнаружено устройство: 10:2d:41:3b:76:6e Данные производителя: 8F 03 0A 10 42 0C 01 10 2D 41 3B 76 6D EC
        // Обнаружено устройство: a4:c1:38:e6:87:a9 Имя: LYWSD03MMC
        Serial.print("Обнаружено устройство: ");
        Serial.print(advertisedDevice.getAddress().toString().c_str());

        if (advertisedDevice.haveName())
        {
            Serial.print(" Имя: ");
            Serial.print(advertisedDevice.getName().c_str());
        }

        if (advertisedDevice.haveManufacturerData())
        {
            Serial.print(" Данные производителя: ");
            std::string data = advertisedDevice.getManufacturerData();
            for (int i = 0; i < data.length(); i++)
            {
                Serial.printf("%02X ", (uint8_t)data[i]);
            }
        }

        if (advertisedDevice.haveServiceData())
        {
            Serial.print(" Сервисные данные: ");
            for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
            {
                Serial.print(" UUID: ");
                Serial.print(advertisedDevice.getServiceDataUUID(i).toString().c_str());
                Serial.print(" Данные: ");
                std::string data = advertisedDevice.getServiceData(i);
                for (int j = 0; j < data.length(); j++)
                {
                    Serial.printf("%02X ", (uint8_t)data[j]);
                }
            }
        }

        Serial.println();

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
            Serial.print(F("SSID received: "));
            Serial.println(F(wifiCredentials.ssid.c_str()));
        }
        else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID)
        {
            wifiCredentials.password = value;
            Serial.print(F("Password received: "));
            Serial.println(F(wifiCredentials.password.c_str()));
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

    Serial.println("Сканер датчиков Xiaomi инициализирован");
    // Serial.println("Запускаем сервисы редактирования SSID и пароля");
    //  pServer = BLEDevice::createServer();
    //  BLEService *pService = pServer->createService(WIFI_SERVICE_UUID);

    // // SSID Characteristic
    // BLECharacteristic *pSSIDCharacteristic = pService->createCharacteristic(
    //     SSID_CHARACTERISTIC_UUID,
    //     BLECharacteristic::PROPERTY_READ |
    //         BLECharacteristic::PROPERTY_WRITE |
    //         BLECharacteristic::PROPERTY_NOTIFY);
    // pSSIDCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
    // pSSIDCharacteristic->setValue(wifiCredentials.ssid); // Set initial value

    // // Password Characteristic
    // BLECharacteristic *pPasswordCharacteristic = pService->createCharacteristic(
    //     PASSWORD_CHARACTERISTIC_UUID,
    //     BLECharacteristic::PROPERTY_READ |
    //         BLECharacteristic::PROPERTY_WRITE |
    //         BLECharacteristic::PROPERTY_NOTIFY);
    // pPasswordCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
    // pPasswordCharacteristic->setValue(wifiCredentials.password); // Set initial value

    // pSSIDCharacteristic->setCallbacks(new SetServerSettingCallbacks());
    // pPasswordCharacteristic->setCallbacks(new SetServerSettingCallbacks());

    // pService->start();
    // BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    // pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
    // pAdvertising->setScanResponse(true);
    // pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    // pAdvertising->setMaxPreferred(0x12);
    // BLEDevice::startAdvertising();
    // Serial.println(F("Сервисы редактирования SSID и пароля запущены"));
}

void startScan(uint32_t duration)
{
    BLEScanResults foundDevices = pBLEScan->start(duration, false);
    Serial.print("Найдено устройств: ");
    Serial.println(foundDevices.getCount());
    Serial.println("Очистка резельтатов сканирования");
    pBLEScan->clearResults();
    scanningActive = false;
}

// Запуск сканирования BLE
void startXiaomiScan(uint32_t duration)
{
    Serial.println("Начало сканирования датчиков Xiaomi...");
    if (scanningActive)
    {
        Serial.println("Сканирования датчиков Xiaomi уже запущено, выход");
        return;
    }
    scanningActive = true;
    uint32_t *durationPtr = new uint32_t(duration);

    xTaskCreate([](void *parameter)
                {
                    uint32_t duration = *(uint32_t*)parameter;
                    startXiaomiScan(duration);
                    delete (uint32_t*)parameter; // Освобождаем память
                    vTaskDelete(NULL); }, "scanBleDevice", 4096, durationPtr, 1, NULL);
}

// Обработка рекламного пакета
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice)
{
    bool isXiaomiDevice = false;
    std::string deviceAddress = advertisedDevice.getAddress().toString().c_str();

    // Проверка по данным производителя (стандартный метод)
    if (advertisedDevice.haveManufacturerData())
    {
        std::string manufacturerData = advertisedDevice.getManufacturerData();

        if (manufacturerData.length() >= 2 &&
            (uint8_t)manufacturerData[0] == 0x57 &&
            (uint8_t)manufacturerData[1] == 0x01)
        {
            isXiaomiDevice = true;
        }
    }

    // Проверка по имени устройства для LYWSD03MMC
    if (!isXiaomiDevice && advertisedDevice.haveName())
    {
        std::string deviceName = advertisedDevice.getName();
        if (deviceName == "LYWSD03MMC" ||
            deviceName.find("MJ_HT") != std::string::npos ||
            deviceName.find("Xiaomi") != std::string::npos ||
            deviceName.find("Mi") != std::string::npos)
        {
            isXiaomiDevice = true;
            Serial.print("Обнаружено устройство Xiaomi по имени: ");
            Serial.println(deviceName.c_str());
        }
    }

    // Проверка по сервисным данным
    if (!isXiaomiDevice && advertisedDevice.haveServiceData())
    {
        for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
        {
            std::string serviceData = advertisedDevice.getServiceData(i);
            // Некоторые устройства Xiaomi используют сервисный UUID FE95
            if (advertisedDevice.getServiceDataUUID(i).equals(BLEUUID("fe95")))
            {
                isXiaomiDevice = true;
                Serial.println("Обнаружено устройство Xiaomi по сервисному UUID FE95");
                break;
            }
        }
    }

    // Если это устройство Xiaomi, обрабатываем его
    if (isXiaomiDevice)
    {
        // Парсим данные из рекламного пакета
        float temperature = 0.0;
        uint8_t humidity = 0;
        uint8_t battery = 0;

        bool dataFound = false;

        // Пытаемся получить данные из сервисных данных для LYWSD03MMC
        if (advertisedDevice.haveServiceData())
        {
            for (int i = 0; i < advertisedDevice.getServiceDataCount(); i++)
            {
                std::string serviceData = advertisedDevice.getServiceData(i);

                // Проверяем сервисные данные на наличие информации о температуре/влажности
                if (serviceData.length() >= 5)
                {
                    // Для LYWSD03MMC с кастомной прошивкой (например, ATC)
                    // Формат: [UUID] [температура 2 байта] [влажность 1 байт] [батарея 1 байт] [счетчик 1 байт]
                    int16_t tempRaw = (int16_t)((serviceData[1] << 8) | serviceData[0]);
                    temperature = tempRaw / 10.0;
                    humidity = serviceData[2];
                    battery = serviceData[3];
                    dataFound = true;

                    Serial.println("Данные получены из сервисных данных");
                    break;
                }
            }
        }

        // Если данные не найдены в сервисных данных, пробуем стандартный метод
        if (!dataFound && advertisedDevice.haveManufacturerData())
        {
            uint8_t *dataPtr = (uint8_t *)advertisedDevice.getManufacturerData().data();
            size_t dataLen = advertisedDevice.getManufacturerData().length();

            dataFound = parseXiaomiData(dataPtr, dataLen, temperature, humidity, battery);
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
                    it->updateSensorData(temperature, humidity, battery);

                    Serial.print("Обновлены данные устройства: ");
                    Serial.print(it->name.c_str());
                    Serial.print(" (");
                    Serial.print(deviceAddress.c_str());
                    Serial.print(") - Температура: ");
                    Serial.print(temperature);
                    Serial.print("°C, Влажность: ");
                    Serial.print(humidity);
                    Serial.print("%, Батарея: ");
                    Serial.print(battery);
                    Serial.println("%");
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
                    newDevice.updateSensorData(temperature, humidity, battery);
                    devices.push_back(newDevice);

                    Serial.print("Найдено новое устройство: ");
                    Serial.print(deviceName.c_str());
                    Serial.print(" (");
                    Serial.print(deviceAddress.c_str());
                    Serial.println(")");
                }
                xSemaphoreGive(devicesMutex);
            }
            refreshLCDData();
        }
        else
        {
            Serial.print("Устройство Xiaomi обнаружено, но данные не найдены: ");
            Serial.println(deviceAddress.c_str());
        }
    }
}

// Парсинг данных Xiaomi из рекламного пакета
bool parseXiaomiData(uint8_t *data, size_t length, float &temperature, uint8_t &humidity, uint8_t &battery)
{
    // Проверяем минимальную длину данных
    if (length < 5)
        return false;

    // Пропускаем заголовок (ID производителя - 2 байта)
    uint8_t *p = data + 2;
    size_t remainingLength = length - 2;

    // Флаг, указывающий, были ли найдены какие-либо данные
    bool dataFound = false;

    // Парсим данные в формате TLV (Type-Length-Value)
    while (remainingLength >= 3)
    {
        uint8_t type = *p++;
        uint8_t dataLen = *p++;

        // Проверяем, достаточно ли данных
        if (dataLen + 2 > remainingLength)
            break;

        // Обрабатываем разные типы данных
        switch (type)
        {
        case 0x0D: // Температура и влажность
            if (dataLen >= 4)
            {
                // Температура (2 байта, little-endian, значение * 10)
                int16_t tempRaw = (int16_t)((p[1] << 8) | p[0]);
                temperature = tempRaw / 10.0;

                // Влажность (1 байт, значение)
                humidity = p[2];

                dataFound = true;
            }
            break;

        case 0x0A: // Батарея
            if (dataLen >= 1)
            {
                battery = p[0];
                dataFound = true;
            }
            break;

            // Можно добавить обработку других типов данных
        }

        // Переходим к следующему блоку данных
        p += dataLen;
        remainingLength -= (dataLen + 2);
    }

    return dataFound;
}

// Обновление статуса устройств
void updateDevicesStatus()
{
    unsigned long currentTime = millis();
    bool statusChanged = false;

    for (auto &device : devices)
    {
        // Проверяем, не устарели ли данные
        if (device.isOnline && (currentTime - device.lastUpdate > XIAOMI_OFFLINE_TIMEOUT))
        {
            device.isOnline = false;
            statusChanged = true;

            Serial.print("Устройство ");
            Serial.print(device.name.c_str());
            Serial.println(" перешло в оффлайн (нет данных более 5 минут)");
        }
    }

    // Если статус изменился, обновляем текст прокрутки
    if (statusChanged)
    {
        refreshLCDData();
    }
}

// Вывод данных всех устройств
void printDevicesData()
{
    Serial.println("\n--- Данные устройств Xiaomi ---");

    if (devices.empty())
    {
        Serial.println("Устройства не найдены");
        return;
    }

    for (const auto &device : devices)
    {
        Serial.print(device.name.c_str());
        Serial.print(" (");
        Serial.print(device.macAddress.c_str());
        Serial.print(") - ");

        if (device.isOnline)
        {
            Serial.print("Онлайн, Температура: ");
            Serial.print(device.currentTemperature);
            Serial.print("°C, Влажность: ");
            Serial.print(device.humidity);
            Serial.print("%, Батарея: ");
            Serial.print(device.battery);
            Serial.print("%, Целевая температура: ");
            Serial.print(device.targetTemperature);
            Serial.print("°C, Статус: ");
            Serial.println(device.enabled ? "Включено" : "Выключено");
        }
        else
        {
            Serial.println("Оффлайн (нет данных более 5 минут)");
        }
    }

    Serial.println("-----------------------------\n");
}

// Инициализация эмуляции устройств
void initXiaomiDeviceEmulation()
{
    // Очищаем массив
    emulatedDevices.clear();

    // Создаем эмулируемые устройства на основе обнаруженных датчиков
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
        for (const auto &device : devices)
        {
            EmulatedXiaomiDevice emulated;
            emulated.did = generateDeviceDid(device.macAddress.c_str());
            emulated.model = "lumi.sensor_ht.v1";
            emulated.name = device.name.c_str();
            emulated.type = "zigbee"; // Эмулируем Zigbee устройства
            emulated.online = true;

            // Добавляем устройство в массив
            emulatedDevices.push_back(emulated);
        }

        xSemaphoreGive(devicesMutex);
    }

    Serial.printf("Создано %d эмулируемых устройств Xiaomi\n", emulatedDevices.size());
}

// Обновление данных эмулируемых устройств
void updateEmulatedDevices()
{
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < devices.size() && i < emulatedDevices.size(); i++)
        {
            // Обновляем свойства устройства
            emulatedDevices[i].name = devices[i].name.c_str();
            emulatedDevices[i].online = true;

            // Обновляем DID, если MAC-адрес изменился
            String newDid = generateDeviceDid(devices[i].macAddress.c_str());
            if (emulatedDevices[i].did != newDid)
            {
                emulatedDevices[i].did = newDid;
            }
        }

        // Если количество устройств изменилось, пересоздаем массив
        if (devices.size() != emulatedDevices.size())
        {
            initXiaomiDeviceEmulation();
        }

        xSemaphoreGive(devicesMutex);
    }
}

// Генерация DID на основе MAC-адреса
String generateDeviceDid(const String &macAddress)
{
    // Используем последние 6 символов MAC-адреса
    String shortMac = macAddress;
    shortMac.replace(":", "");
    shortMac = shortMac.substring(6);

    // Формат: lumi.sensor_ht.XXXXXX
    return "lumi.sensor_ht." + shortMac;
}

// Получение свойств устройства в формате JSON
JsonObject getDeviceProperties(const DeviceData &device, JsonDocument &doc)
{
    JsonObject props = doc.add<JsonObject>();

    // Добавляем свойства устройства
    props["temperature"] = device.currentTemperature;
    props["humidity"] = 50;                                // Заглушка для влажности
    props["voltage"] = 3000 - (100 - device.battery) * 10; // Примерное преобразование процента в милливольты
    props["battery"] = device.battery;

    // Добавляем дополнительные свойства, которые ожидает Mi Home
    props["version"] = 1;
    props["state"] = device.enabled ? 1 : 0;

    return props;
}