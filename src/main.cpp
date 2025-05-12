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
#define KEYPAD_PIN 2 // GPIO1 соответствует A0 на ESP32-S3 UNO
#define NUM_LEDS 1   // Один светодиод
// Глобальные переменные
std::vector<DeviceData> devices;

Adafruit_NeoPixel pixels(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
// При подключении LCD Keypad Shield к ESP32-S3 UNO WROOM-1-N16R8 используются следующие пины:

// LCD интерфейс: GPIO8, GPIO9, GPIO4, GPIO5, GPIO6, GPIO7
// Кнопки: GPIO1 (аналоговый вход A0)
// Свободные пины ESP32-S3 UNO:
// Цифровые пины:

// GPIO0 (D0)
// GPIO2, GPIO3 (D2, D3)
// GPIO10-GPIO21 (D10-D13, и другие пины, не имеющие прямого соответствия с Arduino UNO)
// GPIO35-GPIO48 (дополнительные пины ESP32-S3)
// Аналоговые входы:

// GPIO2-GPIO7 (A1-A5) - обратите внимание, что некоторые из них уже используются для LCD (GPIO4-GPIO7)
// Другие ADC пины ESP32-S3
// Специальные пины:

// I2C: GPIO37 (SDA), GPIO36 (SCL) - если не используются для других целей
// SPI: GPIO11 (MOSI), GPIO13 (MISO), GPIO12 (SCK), GPIO10 (SS) - если не используются для других целей
// UART: GPIO43 (TX), GPIO44 (RX) - если не используются для отладки

std::vector<GpioPin> availableGpio = {
    {15, "GPIO 15"},
    {16, "GPIO 16"},
    {17, "GPIO 17"},
    {18, "GPIO 18"},
    {38, "GPIO 38"},
    {39, "GPIO 39"},
    {40, "GPIO 40"},
    {42, "GPIO 41"},
    {45, "GPIO 45"},
    {47, "GPIO 47"}};

// WiFi
WifiCredentials wifiCredentials;
unsigned long serverWorkTime = 0;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
float board_temperature = 0.0;

// Определение задач для FreeRTOS
TaskHandle_t networkTask;
TaskHandle_t mainLogicTask;

// Мьютекс для защиты доступа к общим данным
// Создаем мьютекс для защиты доступа к данным
SemaphoreHandle_t devicesMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t wifiMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t bleMutex = xSemaphoreCreateMutex();

// Функция для создания эффекта радуги
void rainbow(int wait)
{
    for (long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256)
    {
        pixels.setPixelColor(0, pixels.gamma32(pixels.ColorHSV(firstPixelHue)));
        pixels.show();
        delay(wait);
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
    std::vector<int> gpiosToTurnOn;
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

            Serial.printf("Устройство %s: обогрев включен - %s, необходим обогрев - %s\n",
                          device.name.c_str(), device.heatingActive ? "да" : "нет", (device.currentTemperature + 2) < device.targetTemperature ? "да" : "нет");
            if (!device.heatingActive && device.enabled && device.isOnline && (device.currentTemperature + 2) < device.targetTemperature)
            {
                device.heatingActive = true;
                device.heatingStartTime = currentTime; // Запоминаем время включения
                gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());

                Serial.printf("Устройство %s: включаем обогрев (температура %.1f°C, целевая %.1f°C)\n",
                              device.name.c_str(), device.currentTemperature, device.targetTemperature);
            }
            else if (device.heatingActive && device.currentTemperature >= device.targetTemperature)
            {
                // Температура достигла целевой - выключаем обогрев
                device.heatingActive = false;
                Serial.printf("Устройство %s: выключаем обогрев (температура %.1f°C, целевая %.1f°C)\n",
                              device.name.c_str(), device.currentTemperature, device.targetTemperature);
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
            device.heatingActive = false;
            Serial.printf("Устройство %s: нет данных\n", device.name.c_str());
        }
    }

    // Удаляем дубликаты GPIO
    std::sort(gpiosToTurnOn.begin(), gpiosToTurnOn.end());
    gpiosToTurnOn.erase(std::unique(gpiosToTurnOn.begin(), gpiosToTurnOn.end()), gpiosToTurnOn.end());

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
        if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE)
        {
            if (!heap_caps_check_integrity_all(true))
            {
                Serial.println("Проблема с целостностью памяти!");
            }
            connectWiFi();
            xSemaphoreGive(wifiMutex);
            vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
        }
    }

    // Обработка OTA обновлений
    if (wifiConnected )
    {
        handleOTA();
    }

    // Сканирование BLE устройств
    // Периодическое сканирование BLE
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL)
    {
        if (xSemaphoreTake(bleMutex, portMAX_DELAY) == pdTRUE)
        {
            startXiaomiScan();
            vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
            updateDevicesStatus();
            xSemaphoreGive(bleMutex);
            // Обновляем время последнего сканирования
            lastScanTime = millis();
        }
    }

    // Даем время другим задачам
    vTaskDelay(3000 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
}

// Функция задачи для сетевых операций (ядро 0)
void networkTaskFunction(void *parameter)
{
    for (;;)
    {
        networkFunc();
    }
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

    // // Управление GPIO
    static unsigned long lastGpioControlTime = 0;
    if (millis() - lastGpioControlTime > CONTROL_DELAY)
    {
        // Обработка логики управления устройствами
        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
        {
            controlGPIO();
            xSemaphoreGive(devicesMutex);
            lastGpioControlTime = millis();
        }
    }
    // Добавляем переменную для отслеживания времени последнего сохранения
    static unsigned long lastStatsSaveTime = 0;
    // Периодически сохраняем статистику (каждые 5 минут)
    unsigned long currentTime = millis();
    if ((currentTime - lastStatsSaveTime > 300000) || (currentTime < lastStatsSaveTime))
    {
        Serial.println("Сохранение статистики согласно таймаута, сохраняем результаты");
        saveClientsToFile();
        serverWorkTime +=currentTime;
        saveServerWorkTime();
        lastStatsSaveTime = currentTime;
    }
    // monitorMemory();
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

void createTasksStandartNetwork()
{
    const uint32_t networkStackSize = 8192;

    xTaskCreatePinnedToCore(
        networkTaskFunction,
        "NetworkTask",
        networkStackSize,
        NULL,
        1,
        &networkTask,
        1);
}

void createTasksStandartMainLogic()
{
    const uint32_t logicStackSize = 4096;
    xTaskCreatePinnedToCore(
        mainLogicTaskFunction,
        "MainLogicTask",
        logicStackSize,
        NULL,
        1,
        &mainLogicTask,
        0);
}

void createTasksStandart()
{
    createTasksStandartNetwork();

    createTasksStandartMainLogic();
}

void ReadDataInSPIFFS()
{
    // Загрузка данных устройств
    loadGpioFromFile();
     // Загрузка данных устройств
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
        Serial.printf("Доступно PSRAM: %d байт\n", ESP.getFreePsram());
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
    // Инициализация GPIO
    for (auto &gpio : availableGpio)
    {
        pinMode(gpio.pin, OUTPUT);
        digitalWrite(gpio.pin, LOW);
    }

    // Инициализация LCD и кнопок
    initLCD();

    connectWiFi();

    // Инициализация OTA после подключения к WiFi
    if (wifiConnected)
    {
        initOTA();
    }

    // Инициализация BLE сканера
    setupXiaomiScanner();

    // Инициализация веб-сервера
    initWebServer();

    // Обновление текста прокрутки
    updateScrollText();
    updateLCD();

    Serial.println("Система готова к работе");

    createTasksStandart();
    Serial.println("Настройка завершена");

    // Инициализация датчика температуры (старый API)
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());

    Serial.println("Датчик температуры инициализирован");
}

void loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        // Зеленый
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));
        pixels.show();

        // Чтение температуры (старый API)

        esp_err_t ret = temp_sensor_read_celsius(&board_temperature);

        // if (ret == ESP_OK)
        // {
        //     Serial.printf("Внутренняя температура: %.2f °C\n", board_temperature);
        // }
        // else
        // {
        //     Serial.println("Ошибка при чтении датчика температуры");
        // }
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
    //     Serial.println("Blue");
    //     pixels.setPixelColor(0, pixels.Color(0, 0, 255));
    //     pixels.show();
    //     delay(1000);
    //     // Белый
    //     Serial.println("White");
    //     pixels.setPixelColor(0, pixels.Color(255, 255, 255));
    //     pixels.show();
    //     delay(1000);
    //     // Радуга
    //     Serial.println("Rainbow");
    //     rainbow(10);
}