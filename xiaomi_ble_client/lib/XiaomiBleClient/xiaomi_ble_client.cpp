#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp_system.h>

// Определение UUID сервисов и характеристик Xiaomi LYWSD03MMC
#define XIAOMI_SERVICE_UUID "EBE0CCB0-7A0A-4B0C-8A1A-6FF2997DA3A6"
#define TEMP_HUMID_CHAR_UUID "EBE0CCC1-7A0A-4B0C-8A1A-6FF2997DA3A6"
#define BATTERY_CHAR_UUID "EBE0CCC4-7A0A-4B0C-8A1A-6FF2997DA3A6"
#define UNITS_CHAR_UUID "EBE0CCC1-7A0A-4B0C-8A1A-6FF2997DA3A6"

// Определение UUID для стандартных сервисов
#define DEVICE_INFO_SERVICE_UUID "180A"
#define MODEL_NUMBER_UUID "2A24"
#define SERIAL_NUMBER_UUID "2A25"
#define FIRMWARE_REV_UUID "2A26"
#define HARDWARE_REV_UUID "2A27"
#define MANUFACTURER_UUID "2A29"

// Глобальные переменные для хранения данных датчика
float temperature = 25.0;
uint8_t humidity = 50;
uint8_t batteryLevel = 100;
uint8_t temperatureUnit = 0; // 0 - Celsius, 1 - Fahrenheit

// Характеристики BLE
BLECharacteristic *pTempHumidCharacteristic;
BLECharacteristic *pBatteryCharacteristic;
BLECharacteristic *pUnitsCharacteristic;
BLEAdvertising *pAdvertising;
BLEServer *pServer;

// Класс для обработки обратных вызовов сервера
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("Клиент подключился");

        // Получаем ID соединения (обычно 0 для первого соединения)
        uint16_t connId = 0;

        // Настраиваем параметры соединения для экономии энергии
        esp_ble_conn_update_params_t connParams;
        connParams.min_int = 100; // Минимальный интервал (1.25 мс * 100 = 125 мс)
        connParams.max_int = 200; // Максимальный интервал (1.25 мс * 200 = 250 мс)
        connParams.latency = 4;   // Slave latency (пропуск connection events)
        connParams.timeout = 600; // Таймаут соединения (10 мс * 600 = 6000 мс)

        // Обновляем параметры соединения
        esp_ble_gap_update_conn_params(&connParams);

        Serial.println("Параметры соединения обновлены для экономии энергии");
    }

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("Клиент отключен");
        // Перезапускаем рекламу, чтобы устройство снова стало видимым
        pServer->getAdvertising()->start();
    }
};

// Класс для обработки обратных вызовов характеристики единиц измерения
class UnitsCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            temperatureUnit = value[0];
            Serial.printf("Единица измерения изменена на: %s\n",
                          temperatureUnit == 0 ? "Celsius" : "Fahrenheit");
        }
    }
};

// Функция для обновления данных температуры и влажности
void updateSensorData(float temp, uint8_t humid, uint8_t battery)
{
    temperature = temp;
    humidity = humid;
    batteryLevel = battery;

    // Обновление характеристики температуры и влажности
    uint8_t tempData[5];
    int16_t tempRaw = (int16_t)(temperature * 100);
    tempData[0] = tempRaw & 0xFF;
    tempData[1] = (tempRaw >> 8) & 0xFF;
    tempData[2] = humidity;
    tempData[3] = 0; // Зарезервировано
    tempData[4] = 0; // Зарезервировано

    pTempHumidCharacteristic->setValue(tempData, 5);

    // Обновление характеристики батареи
    // pBatteryCharacteristic->setValue(&batteryLevel, 1);

    Serial.printf("Данные обновлены: Температура = %.2f°C, Влажность = %d%%, Батарея = %d%%\n",
                  temperature, humidity, batteryLevel);
}

// Функция для настройки рекламных пакетов в формате Xiaomi
void setupAdvertising()
{
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();

    BLEAdvertisementData advData;

    // Установка имени устройства
    advData.setName("LYWSD03MMC");

    // Добавление сервиса в рекламу
    advData.setCompleteServices(BLEUUID(XIAOMI_SERVICE_UUID));

    // Создание рекламных данных производителя (Manufacturer Data)
    std::string manufacturerData;
    uint8_t manData[16];

    // Идентификатор производителя Xiaomi (0x95FE)
    manData[0] = 0x95;
    manData[1] = 0xFE;

    // Тип устройства
    manData[2] = 0x5D;

    // Счетчик пакетов (просто для примера)
    manData[3] = 0x01;
    manData[4] = 0x00;

    uint8_t macAddr[6];
    esp_read_mac(macAddr, ESP_MAC_BT);
    // MAC-адрес
    memcpy(&manData[5], macAddr, 6);

    // Температура
    int16_t tempRaw = (int16_t)(temperature * 100);
    manData[11] = tempRaw & 0xFF;
    manData[12] = (tempRaw >> 8) & 0xFF;

    // Влажность
    manData[13] = humidity;

    // Уровень батареи
    manData[14] = batteryLevel;

    // Контрольная сумма (упрощенно)
    manData[15] = 0;
    for (int i = 0; i < 15; i++)
    {
        manData[15] ^= manData[i];
    }

    manufacturerData = std::string((char *)manData, 16);
    advData.setManufacturerData(manufacturerData);

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponseData(advData);

    // Настройка параметров рекламы
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
}

// Функция инициализации BLE сервера
void setupBLEServer()
{
    // Инициализация BLE устройства
    BLEDevice::init("LYWSD03MMC");

    // Создание BLE сервера
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Создание сервиса данных Xiaomi
    BLEService *pXiaomiService = pServer->createService(XIAOMI_SERVICE_UUID);

    // Создание характеристики температуры и влажности
    pTempHumidCharacteristic = pXiaomiService->createCharacteristic(
        TEMP_HUMID_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pTempHumidCharacteristic->addDescriptor(new BLE2902());

    // Создание характеристики уровня батареи
    pBatteryCharacteristic = pXiaomiService->createCharacteristic(
        BATTERY_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ);

    // Создание характеристики единиц измерения
    pUnitsCharacteristic = pXiaomiService->createCharacteristic(
        UNITS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pUnitsCharacteristic->setCallbacks(new UnitsCallbacks());

    // Создание сервиса информации об устройстве
    BLEService *pDeviceInfoService = pServer->createService(DEVICE_INFO_SERVICE_UUID);

    // Характеристики информации об устройстве
    BLECharacteristic *pModelNumberCharacteristic = pDeviceInfoService->createCharacteristic(
        MODEL_NUMBER_UUID,
        BLECharacteristic::PROPERTY_READ);
    pModelNumberCharacteristic->setValue("LYWSD03MMC");

    BLECharacteristic *pSerialNumberCharacteristic = pDeviceInfoService->createCharacteristic(
        SERIAL_NUMBER_UUID,
        BLECharacteristic::PROPERTY_READ);
    pSerialNumberCharacteristic->setValue("12345678");

    BLECharacteristic *pFirmwareRevCharacteristic = pDeviceInfoService->createCharacteristic(
        FIRMWARE_REV_UUID,
        BLECharacteristic::PROPERTY_READ);
    pFirmwareRevCharacteristic->setValue("1.0.0");

    BLECharacteristic *pHardwareRevCharacteristic = pDeviceInfoService->createCharacteristic(
        HARDWARE_REV_UUID,
        BLECharacteristic::PROPERTY_READ);
    pHardwareRevCharacteristic->setValue("1.0.0");

    BLECharacteristic *pManufacturerCharacteristic = pDeviceInfoService->createCharacteristic(
        MANUFACTURER_UUID,
        BLECharacteristic::PROPERTY_READ);
    pManufacturerCharacteristic->setValue("Xiaomi");

    // Запуск сервисов
    pXiaomiService->start();
    pDeviceInfoService->start();

    // Настройка рекламы
    setupAdvertising();

    // Запуск рекламы
    pAdvertising->start();

    Serial.println("BLE сервер запущен, ожидание подключений...");
}

// Инициализация датчика
void setupXiaomiSensor()
{
    Serial.begin(115200);
    Serial.println("Инициализация Xiaomi LYWSD03MMC BLE клиента...");

    setupBLEServer();

    // Установка минимальной мощности передатчика
    // Возможные значения: ESP_PWR_LVL_N12, ESP_PWR_LVL_N9, ESP_PWR_LVL_N6, ESP_PWR_LVL_N3,
    // ESP_PWR_LVL_N0, ESP_PWR_LVL_P3, ESP_PWR_LVL_P6, ESP_PWR_LVL_P9
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N9); // -9 dBm
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N6);     // -6 dBm

    // Начальное обновление данных
    updateSensorData(temperature, humidity, batteryLevel);
}

// Основной цикл для обновления данных
void loopXiaomiSensor()
{
    // Периодическая отправка уведомлений
    static unsigned long lastNotifyTime = 0;
    if (millis() - lastNotifyTime > 30000) // Уведомления каждые 2 секунды
    {
        pTempHumidCharacteristic->notify();
        lastNotifyTime = millis();
    }

    // Обновляем только рекламные пакеты, если нужно
    static unsigned long lastAdvertisingUpdateTime = 0;

    if (millis() - lastAdvertisingUpdateTime > 30000)
    { // Обновление рекламы каждые 10 секунд
        // Обновление рекламных пакетов с текущими данными
        setupAdvertising();
        pAdvertising->start();

        lastAdvertisingUpdateTime = millis();
    }

    // Имитация разряда батареи (можно заменить на реальное измерение)
    static unsigned long lastBatteryUpdateTime = 0;
    if (millis() - lastBatteryUpdateTime > 60000)
    { // Проверка батареи каждую минуту
        if (random(0, 100) < 5 && batteryLevel > 0)
        { // 5% шанс уменьшения заряда батареи
            batteryLevel--;
            // Обновляем только характеристику батареи
            pBatteryCharacteristic->setValue(&batteryLevel, 1);
        }
        lastBatteryUpdateTime = millis();
    }
}
