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
    {GPIO_NUM_1, STATE_GPIO_AUTO, "GPIO 1"},
    {GPIO_NUM_4, STATE_GPIO_AUTO, "GPIO 4"},
    {GPIO_NUM_5, STATE_GPIO_AUTO, "GPIO 5"},
    {GPIO_NUM_6, STATE_GPIO_AUTO, "GPIO 6"},
    {GPIO_NUM_7, STATE_GPIO_AUTO, "GPIO 7"}};

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

// Управление GPIO
void controlGPIO()
{
    Serial.println("Проверка необходимости включения GPIO");
    std::vector<uint8_t> gpiosToTurnOn;

    static unsigned long lastcontrolGPIOTime = 0;
    unsigned long elapsedTime = millis() - lastcontrolGPIOTime;

    // Собираем GPIO для включения
    for (auto &device : devices)
    {
        if (device.isDataValid())
        {
            // Включаем обогрев если устройство доступно и температура ниже целевой
            if (!device.heatingActive && device.enabled && (device.currentTemperature + hysteresisTemp) < device.targetTemperature)
            {
                Serial.println("Включаем обогрев для: " + String(device.name.c_str()));
                device.heatingActive = true;
            }
            // Если температура достигла целевой - выключаем обогрев
            else if (device.heatingActive && device.currentTemperature >= device.targetTemperature)
            {
                Serial.println("Выключаем обогрев для:" + String(device.name.c_str()));
                device.heatingActive = false;
            }
            // Если обогрев был активен, обновляем общее время работы перед выключением
            else if (!device.enabled && device.heatingActive)
            {
                Serial.println("Устройство выключено. Выключаем обогрев для: " + String(device.name.c_str()));
                device.totalHeatingTime += elapsedTime;
                device.heatingActive = false;
            }

            // Обновляем общее время работы
            if (device.heatingActive)
            {
                // Обновляем общее время работы
                device.totalHeatingTime += elapsedTime;
                gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());
            }
        }
        else if (device.isOnline)
        {
            Serial.println("Устройство: " + String(device.name.c_str()) + " перешло в оффлайн");
            device.isOnline = false;
            device.totalHeatingTime += elapsedTime;
            device.heatingActive = false;
        }
    }

    // Управляем GPIO
    for (auto &gpio : availableGpio)
    {
        bool shouldTurnOn = false;
        if (gpio.state == STATE_GPIO_AUTO)
        {
            shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio.pin) != gpiosToTurnOn.end();
        }
        else
        {
            shouldTurnOn = gpio.state == STATE_GPIO_ON ? true : false;
        }

        digitalWrite(gpio.pin, shouldTurnOn ? HIGH : LOW);
        if (shouldTurnOn)
        {
            gpio.totalHeatingTime += elapsedTime;
        }
    }
    lastcontrolGPIOTime = millis();
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
        disabledButtonForOta(true);
        vTaskDelay(20 / portTICK_PERIOD_MS); // Небольшая задержка для стабильности
        return;
    }

    // Управление GPIO
    static unsigned long lastGpioControlTime = 0;
    if (millis() - lastGpioControlTime > CONTROL_DELAY)
    {
        lastGpioControlTime = millis();
        // Обработка логики управления устройствами
        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
        {
            controlGPIO();
            xSemaphoreGive(devicesMutex);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
    }
    // Добавляем переменную для отслеживания времени последнего сохранения
    static unsigned long lastStatsSaveTime = 0;
    // Периодически сохраняем статистику (каждые 5 минут)
    unsigned long currentTime = millis();
    if ((currentTime - lastStatsSaveTime > 300000) || (currentTime < lastStatsSaveTime))
    {
        Serial.println("Сохранение статистики согласно таймаута, сохраняем результаты");
        saveClientsToFile();
        saveGpioToFile();
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

    updateMainScreenLCD();

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