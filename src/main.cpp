#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include "variables_info.h"
#include "lcd_setting.h"
#include "web_server_setting.h"
#include "spiffs_setting.h"
#include "xiaomi_scanner.h"
#include "ota_setting.h"
#include <atomic>
#include "esp_system.h"         // Библиотека ESP-IDF для работы с системными функциями
#include "driver/temp_sensor.h" // Библиотека для работы с датчиком температуры
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#define KEYPAD_PIN 2 // GPIO2 соответствует A1 на ESP32-S3 UNO
#define NUM_LEDS 1   // Один светодиод
// Глобальные переменные
std::vector<DeviceData> devices;

Adafruit_NeoPixel pixels(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

std::vector<GpioPin> availableGpio = {
    {GPIO_NUM_1, 0, "GPIO 1"},
    {GPIO_NUM_4, 0, "GPIO 4"},
    {GPIO_NUM_5, 0, "GPIO 5"},
    {GPIO_NUM_6, 0, "GPIO 6"},
    {GPIO_NUM_7, 0, "GPIO 7"}};

// WiFi
WifiCredentials wifiCredentials;
unsigned long serverWorkTime = 0;
float hysteresisTemp = 1.5;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
float board_temperature = 0.0;

// Мьютекс для защиты доступа к общим данным
// Создаем мьютекс для защиты доступа к данным
SemaphoreHandle_t devicesMutex = xSemaphoreCreateMutex();

// Функция для создания эффекта радуги
void rainbow(int wait)
{
    for (long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256)
    {
        pixels.setPixelColor(0, pixels.gamma32(pixels.ColorHSV(firstPixelHue)));
        pixels.show();
        vTaskDelay(wait / portTICK_PERIOD_MS);
    }
}

// Функция для безопасного вычисления разницы времени с учетом переполнения millis()
unsigned long safeTimeDifference(unsigned long currentTime, unsigned long previousTime)
{
    // Если произошло переполнение
    if (currentTime < previousTime)
    {
        return (ULONG_MAX - previousTime) + currentTime + 1;
    }
    else
    {
        return currentTime - previousTime;
    }
}

// Управление GPIO
void controlGPIO()
{
    Serial.println("Проверка необходимости включения GPIO");
    std::vector<uint8_t> gpiosToTurnOn;
    unsigned long currentTime = millis();
    // Собираем GPIO для включения
    for (auto &device : devices)
    {
        if (device.isDataValid())
        {
            if (device.heatingActive)
            {
                // Вычисляем время, прошедшее с момента последнего обновления
                unsigned long elapsedTime = safeTimeDifference(currentTime, device.heatingStartTime);
                // Обновляем общее время работы
                device.totalHeatingTime += elapsedTime;
                // Обновляем время начала для следующего расчета
                device.heatingStartTime = currentTime;
            }

            // Serial.printf("Устройство %s: обогрев включен - %s, необходим обогрев - %s\r\n", device.name.c_str(), device.heatingActive ? "да" : "нет", (device.currentTemperature + hysteresisTemp) < device.targetTemperature ? "да" : "нет");
            if (!device.heatingActive && device.enabled && device.isOnline && (device.currentTemperature + hysteresisTemp) < device.targetTemperature)
            {
                device.heatingActive = true;
                device.heatingStartTime = currentTime; // Запоминаем время включения
                gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());

                // Serial.printf("Устройство %s: включаем обогрев (температура %.1f°C, целевая %.1f°C)\r\n", device.name.c_str(), device.currentTemperature, device.targetTemperature);
            }
            else if (device.heatingActive && device.currentTemperature >= device.targetTemperature)
            {
                // Температура достигла целевой - выключаем обогрев
                device.heatingActive = false;
                // Serial.printf("Устройство %s: выключаем обогрев (температура %.1f°C, целевая %.1f°C)\r\n", device.name.c_str(), device.currentTemperature, device.targetTemperature);
            }
            else if (!device.enabled && device.heatingActive)
            {
                // Если обогрев был активен, обновляем общее время работы перед выключением
                unsigned long elapsedTime = safeTimeDifference(currentTime, device.heatingStartTime);
                device.totalHeatingTime += elapsedTime;
                device.heatingActive = false;
            }
        }
        else if (device.isOnline)
        {
            device.isOnline = false;
            unsigned long elapsedTime = safeTimeDifference(currentTime, device.heatingStartTime);
            device.totalHeatingTime += elapsedTime;
            device.heatingActive = false;
            // Serial.printf("Устройство %s: нет данных\r\n", device.name.c_str());
        }
    }

    // Управляем GPIO
    for (auto &gpio : availableGpio)
    {
        bool shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio.pin) != gpiosToTurnOn.end();
        digitalWrite(gpio.pin, shouldTurnOn ? HIGH : LOW);
    }
}

void networkFunc()
{
    // Если активен режим OTA, пропускаем обычную обработку
    if (isOtaActive())
    {
        vTaskDelay(20 / portTICK_PERIOD_MS); // Небольшая задержка для стабильности
        return;
    }

    // Проверка подключения WiFi
    if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
    {
        connectWiFi();
        vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
    }

    // Обработка OTA обновлений
    if (wifiConnected)
    {
        handleOTA();
    }

    // Сканирование BLE устройств
    // Периодическое сканирование BLE
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL)
    {
        // Обновляем время последнего сканирования
        lastScanTime = millis();
        startXiaomiScan();
    }

    // Даем время другим задачам
    vTaskDelay(3000 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
}

void mainlogicFunc()
{
    // Если активен режим OTA, пропускаем обычную обработку
    if (isOtaActive())
    {
        vTaskDelay(20 / portTICK_PERIOD_MS); // Небольшая задержка для стабильности
        return;
    }
    // Обработка нажатий кнопок
    handleButtons();
    // Обновление LCD
    updateLCDTask();

    // Управление GPIO
    static unsigned long lastGpioControlTime = 0;
    if (millis() - lastGpioControlTime > CONTROL_DELAY)
    {
        // Обработка логики управления устройствами
        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
        {
            lastGpioControlTime = millis();
            controlGPIO();
            xSemaphoreGive(devicesMutex);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
        updateDevicesInformation();
    }
    // Добавляем переменную для отслеживания времени последнего сохранения
    static unsigned long lastStatsSaveTime = 0;
    // Периодически сохраняем статистику (каждые 5 минут)
    unsigned long currentTime = millis();
    if ((currentTime - lastStatsSaveTime > 300000) || (currentTime < lastStatsSaveTime))
    {
        Serial.println("Сохранение статистики согласно таймаута, сохраняем результаты");
        saveClientsToFile();
        serverWorkTime += currentTime - lastStatsSaveTime;
        lastStatsSaveTime = currentTime;
        saveServerSetting();
    }
    //  Даем время другим задачам
    vTaskDelay(200 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
}

// Функция задачи для основной логики (ядро 1)
void mainLogicTaskFunction(void *parameter)
{
    for (;;)
    {
        mainlogicFunc();
    }
}
// Функция задачи для сетевых операций (ядро 0)
void networkTaskFunction(void *parameter)
{
    for (;;)
    {
        networkFunc();
    }
}
void createTasksStandart()
{
    xTaskCreate(
        mainLogicTaskFunction,   // Функция
        "mainLogicTaskFunction", // Имя
        4096,                    // Стек: 2048 слов = 8192 байт
        NULL,                    // Параметры
        1,                       // Приоритет
        NULL                     // Хэндл (не нужен)
    );

    xTaskCreate(
        networkTaskFunction,   // Функция
        "networkTaskFunction", // Имя
        8192,                  // Стек: 2048 слов = 8192 байт
        NULL,                  // Параметры
        1,                     // Приоритет
        NULL                   // Хэндл (не нужен)
    );
}

void ReadDataInSPIFFS()
{
    // Загрузка данных устройств
    loadWifiCredentialsFromFile();
    loadClientsFromFile();
    loadServerWorkTime();
}

// Настройка
void setup()
{
    Serial.begin(115200);
    Serial.println("Запуск системы...");
    pixels.begin();           // Инициализация NeoPixel
    pixels.setBrightness(50); // Установка яркости (0-255)
    pixels.show();            // Инициализация всех пикселей в 'выключено'

    // Инициализация и проверка PSRAM
    if (psramFound())
    {
        Serial.println("PSRAM найдена и инициализирована");
        // Serial.printf("Доступно PSRAM: %d байт\r\n", ESP.getFreePsram());
    }
    else
    {
        Serial.println("PSRAM не найдена! Некоторые функции могут работать некорректно");
    }

    // Инициализация SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Ошибка инициализации SPIFFS");
        return;
    }

    xTaskCreate([](void *parameter)
                {
                    ReadDataInSPIFFS();
                    vTaskDelete(NULL); }, "ReadSPIFFS", 4096, NULL, 1, NULL);

    // Загрузка данных устройств
    loadGpioFromFile();
    // Инициализация GPIO
    for (auto &gpio : availableGpio)
    {
        pinMode(gpio.pin, OUTPUT);
        digitalWrite(gpio.pin, LOW);
    }

    // Инициализация LCD и кнопок
    initLCD();

    connectWiFi();

    // Инициализация BLE сканера
    setupXiaomiScanner();

    // Инициализация веб-сервера
    initWebServer();

    // Обновление текста прокрутки
    initScrollText();
    updateLCD();
    createTasksStandart();

    if (wifiConnected)
    {
        initOTA();
        MDNS.addService("http", "tcp", 80);
        Serial.println("MDNS http service registered");
    }

    // Инициализация датчика температуры (старый API)
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());

    Serial.println("Датчик температуры инициализирован");
    Serial.println("Настройка завершена");
    Serial.println("Система готова к работе");
}

void loop()
{
    if (wifiConnected)
    {
        // Зеленый
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));
        pixels.show();
        // Чтение температуры (старый API)
        esp_err_t ret = temp_sensor_read_celsius(&board_temperature);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
    }
    else
    {
        // Красный
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        pixels.clear();
        pixels.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    //     // Синий
    //     //Serial.printf("Blue");
    //     pixels.setPixelColor(0, pixels.Color(0, 0, 255));
    //     pixels.show();
    //     delay(1000);
    //     // Белый
    //     //Serial.printf("White");
    //     pixels.setPixelColor(0, pixels.Color(255, 255, 255));
    //     pixels.show();
    //     delay(1000);
    //     // Радуга
    //     //Serial.printf("Rainbow");
    //     rainbow(10);
}