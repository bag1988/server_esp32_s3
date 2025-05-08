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

// Глобальные флаги для отслеживания активности WiFi и BLE
std::atomic<bool> wifiActive(false);
std::atomic<bool> bleActive(false);
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
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
float board_temperature = 0.0;
// Имена файлов
std::string DEVICES_FILE = "/devices.json";
std::string WIFI_CREDENTIALS_FILE = "/wifi_credentials.json";

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

    // Обработка пакетов miIO с защитой
    // if (xSemaphoreTake(wifiMutex, portMAX_DELAY) == pdTRUE)
    // {
    //     // Обработка пакетов miIO
    //     if (!bleActive)
    //     {
    //         wifiActive = true;
    //         miIO.handlePackets();
    //         xSemaphoreGive(wifiMutex);
    //         vTaskDelay(20 / portTICK_PERIOD_MS);
    //         wifiActive = false;
    //     }
    // }

    // Проверка подключения WiFi
    if (!wifiConnected && !bleActive && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
    {
        wifiActive = true;
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
        wifiActive = false;
    }

    // Обработка OTA обновлений
    if (wifiConnected && !bleActive)
    {
        wifiActive = true;
        handleOTA();
        wifiActive = false;
    }

    // Сканирование BLE устройств
    // Периодическое сканирование BLE
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL && !wifiActive)
    {
        bleActive = true;
        if (xSemaphoreTake(bleMutex, portMAX_DELAY) == pdTRUE)
        {
            startXiaomiScan();
            vTaskDelay(20 / portTICK_PERIOD_MS); // Добавьте задержку
            updateDevicesStatus();
            xSemaphoreGive(bleMutex);
            // Обновляем время последнего сканирования
            lastScanTime = millis();
        }
        bleActive = false;
    }

    // Обновляем данные эмулируемых устройств
    // if (!wifiActive)
    // {
    //     bleActive = true;
    //     updateEmulatedDevices();
    //     bleActive = false;
    // }

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

// Функция для мониторинга памяти
void monitorMemory()
{
    static unsigned long lastWriteStatistic = 0;
    if (millis() - lastWriteStatistic > 5000)
    {
        Serial.printf("Свободно HEAP: %d байт\n", ESP.getFreeHeap());
        if (psramFound())
        {
            Serial.printf("Свободно PSRAM: %d байт\n", ESP.getFreePsram());
            Serial.printf("Наибольший свободный блок: %d байт\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
        lastWriteStatistic = millis();
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
void createTasks()
{
    const uint32_t networkStackSize = 8192;
    const uint32_t logicStackSize = 4096;

    xTaskCreatePinnedToCore(
        networkTaskFunction,
        "NetworkTask",
        networkStackSize,
        NULL,
        1,
        &networkTask,
        1);

    xTaskCreatePinnedToCore(
        mainLogicTaskFunction,
        "MainLogicTask",
        logicStackSize,
        NULL,
        1,
        &mainLogicTask,
        0);
}

// void createTasksOlddd()
// {
//     // Размер стека в словах (1 слово = 4 байта)
//     const uint32_t networkStackSize = 8192; // Для сетевых операций
//     const uint32_t logicStackSize = 4096;   // Для основной логики

//     Serial.println("Создание задач FreeRTOS с использованием PSRAM...");

//     // Проверяем наличие PSRAM
//     if (psramFound())
//     {
//         Serial.println("PSRAM найдена, размер: " + String(ESP.getFreePsram() / 1024) + " КБ");

//         // Настраиваем использование PSRAM для malloc
//         heap_caps_malloc_extmem_enable(4096); // Минимальный размер для внешней памяти

//         // Создаем задачу для сетевых операций на ядре 0
//         // Используем CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION для размещения стека в PSRAM
//         BaseType_t networkTaskResult = xTaskCreatePinnedToCore(
//             networkTaskFunction, // Функция задачи
//             "NetworkTask",       // Имя задачи
//             networkStackSize,    // Размер стека
//             NULL,                // Параметры
//             1,                   // Приоритет (повышенный)
//             &networkTask,        // Указатель на задачу
//             0);                  // Ядро 0

//         if (networkTaskResult != pdPASS)
//         {
//             Serial.println("Ошибка создания NetworkTask, код: " + String(networkTaskResult));

//             // Пробуем создать с меньшим стеком
//             networkTaskResult = xTaskCreatePinnedToCore(
//                 networkTaskFunction, "NetworkTask", networkStackSize / 2, NULL, 1, &networkTask, 0);

//             if (networkTaskResult != pdPASS)
//             {
//                 Serial.println("Повторная ошибка создания NetworkTask");
//             }
//             else
//             {
//                 Serial.println("NetworkTask создана с уменьшенным стеком");
//             }
//         }
//         else
//         {
//             Serial.println("NetworkTask создана успешно");
//         }

//         // Небольшая задержка между созданием задач
//         delay(100);

//         // Создаем задачу для основной логики на ядре 1
//         BaseType_t logicTaskResult = xTaskCreatePinnedToCore(
//             mainLogicTaskFunction, // Функция задачи
//             "MainLogicTask",       // Имя задачи
//             logicStackSize,        // Размер стека
//             NULL,                  // Параметры
//             1,                     // Приоритет
//             &mainLogicTask,        // Указатель на задачу
//             1);                    // Ядро 1

//         if (logicTaskResult != pdPASS)
//         {
//             Serial.println("Ошибка создания MainLogicTask, код: " + String(logicTaskResult));

//             // Пробуем создать с меньшим стеком
//             logicTaskResult = xTaskCreatePinnedToCore(
//                 mainLogicTaskFunction, "MainLogicTask", logicStackSize / 2, NULL, 1, &mainLogicTask, 1);

//             if (logicTaskResult != pdPASS)
//             {
//                 Serial.println("Повторная ошибка создания MainLogicTask");
//             }
//             else
//             {
//                 Serial.println("MainLogicTask создана с уменьшенным стеком");
//             }
//         }
//         else
//         {
//             Serial.println("MainLogicTask создана успешно");
//         }

//         // Мониторинг памяти после создания задач
//         Serial.println("Свободно HEAP: " + String(ESP.getFreeHeap() / 1024) + " КБ");
//         Serial.println("Свободно PSRAM: " + String(ESP.getFreePsram() / 1024) + " КБ");
//     }
//     else
//     {
//         Serial.println("PSRAM не найдена, используем стандартное создание задач");

//         // Создаем задачи с меньшим стеком
//         const uint32_t reducedNetworkStackSize = 8192;
//         const uint32_t reducedLogicStackSize = 4096;

//         // Создаем задачу для сетевых операций на ядре 0
//         xTaskCreatePinnedToCore(
//             networkTaskFunction,     // Функция задачи
//             "NetworkTask",           // Имя задачи
//             reducedNetworkStackSize, // Уменьшенный размер стека
//             NULL,                    // Параметры
//             1,                       // Приоритет
//             &networkTask,            // Указатель на задачу
//             0);                      // Ядро 0

//         // Создаем задачу для основной логики на ядре 1
//         xTaskCreatePinnedToCore(
//             mainLogicTaskFunction, // Функция задачи
//             "MainLogicTask",       // Имя задачи
//             reducedLogicStackSize, // Уменьшенный размер стека
//             NULL,                  // Параметры
//             1,                     // Приоритет
//             &mainLogicTask,        // Указатель на задачу
//             1);                    // Ядро 1
//     }

//     // Проверка успешности создания задач
//     if (networkTask != NULL && mainLogicTask != NULL)
//     {
//         Serial.println("Все задачи созданы успешно");
//     }
//     else
//     {
//         Serial.println("Ошибка при создании задач");
//     }

//     // Настройка мониторинга использования стека
//     // Будем периодически проверять в основном цикле
//     Serial.println("Настройка мониторинга стека завершена");
// }

// // Создание задач с большим стеком в PSRAM
// void createTasksOld()
// {
//     // Размер стека в словах (1 слово = 4 байта)
//     const uint32_t stackSize = 16384 * 2;

//     // Создаем задачу с использованием PSRAM для стека
//     if (psramFound())
//     {
//         // Выделяем память для стека в PSRAM
//         // Выделяем память для стека в PSRAM с выравниванием
//         StackType_t *taskStack = (StackType_t *)heap_caps_aligned_alloc(16, stackSize * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
//         // TCB во внутренней памяти
//         StaticTask_t *taskBuffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

//         if (taskStack && taskBuffer)
//         {
//             Serial.println("Используем создание задачи PSRAM");
//             xTaskCreateStatic(
//                 mainLogicTaskFunction,
//                 "MainLogicTask",
//                 stackSize,
//                 NULL,
//                 1,
//                 taskStack,
//                 taskBuffer);
//         }
//         else
//         {
//             Serial.println("Используем обычное создание задачи");
//             // Создание задачи для основной логики на ядре 1
//             xTaskCreatePinnedToCore(
//                 mainLogicTaskFunction, // Функция задачи
//                 "MainLogicTask",       // Имя задачи
//                 8192,                  // Размер стека
//                 NULL,                  // Параметры
//                 1,                     // Приоритет
//                 &mainLogicTask,        // Указатель на задачу
//                 1);                    // Ядро 1
//         }

//         // Выделяем память для стека в PSRAM с выравниванием
//         StackType_t *taskStackN = (StackType_t *)heap_caps_aligned_alloc(16, stackSize * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
//         // TCB во внутренней памяти
//         StaticTask_t *taskBufferN = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

//         if (taskStackN && taskBufferN)
//         {
//             Serial.println("Используем создание задачи PSRAM");
//             xTaskCreateStatic(
//                 networkTaskFunction,
//                 "NetworkTask",
//                 stackSize,
//                 NULL,
//                 2,
//                 taskStackN,
//                 taskBufferN);
//         }
//         else
//         {
//             Serial.println("Используем обычное создание задачи");
//             // Создание задачи для сетевых операций на ядре 0
//             xTaskCreatePinnedToCore(
//                 networkTaskFunction, // Функция задачи
//                 "NetworkTask",       // Имя задачи
//                 8192,                // Размер стека (больше для сетевых операций)
//                 NULL,                // Параметры
//                 2,                   // Приоритет
//                 &networkTask,        // Указатель на задачу
//                 0);                  // Ядро 0
//         }
//     }
//     else
//     {
//         Serial.println("Используем обычное создание задачи");
//         // Если PSRAM не доступна, используем обычное создание задачи
//         // Создание задачи для сетевых операций на ядре 0
//         xTaskCreatePinnedToCore(
//             networkTaskFunction, // Функция задачи
//             "NetworkTask",       // Имя задачи
//             8192,                // Размер стека (больше для сетевых операций)
//             NULL,                // Параметры
//             2,                   // Приоритет
//             &networkTask,        // Указатель на задачу
//             0);                  // Ядро 0

//         // Создание задачи для основной логики на ядре 1
//         xTaskCreatePinnedToCore(
//             mainLogicTaskFunction, // Функция задачи
//             "MainLogicTask",       // Имя задачи
//             8192,                  // Размер стека
//             NULL,                  // Параметры
//             1,                     // Приоритет
//             &mainLogicTask,        // Указатель на задачу
//             1);                    // Ядро 1
//     }
// }

void ReadDataInSPIFFS()
{
    loadGpioFromFile();
    // Загрузка данных устройств
    loadClientsFromFile();
    // loadWifiCredentialsFromFile();
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

    Serial.println("Система готова к работе");

    createTasks();
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

        if (ret == ESP_OK)
        {
            Serial.printf("Внутренняя температура: %.2f °C\n", board_temperature);
        }
        else
        {
            Serial.println("Ошибка при чтении датчика температуры");
        }
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