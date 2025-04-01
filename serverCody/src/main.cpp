#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include "variables_info.h"
#include "lcd_setting.h"
#include "web_server_setting.h"
#include "spiffs_setting.h"
#include "xiaomi_scanner.h"
#include "mi_io_protocol.h"
#include "mdns_service.h"
// Глобальные переменные
std::vector<DeviceData> devices;
// Доступные GPIO пины для ESP32-DevKitC-32
// GPIO 34-39 могут использоваться только как входы
// GPIO 6-11 обычно используются для подключения флеш-памяти и не рекомендуются для общего использования
// GPIO 0 имеет специальное назначение при загрузке (boot mode)
std::vector<GpioPin> availableGpio = {
    {2, "GPIO 2 (LED)"},
    {4, "GPIO 4"},
    {5, "GPIO 5"},
    {12, "GPIO 12"},
    {13, "GPIO 13"},
    {14, "GPIO 14"},
    {15, "GPIO 15"},
    {16, "GPIO 16"},
    {17, "GPIO 17"},
    {18, "GPIO 18"},
    {19, "GPIO 19"},
    {21, "GPIO 21"},
    {22, "GPIO 22"},
    {23, "GPIO 23"},
    {25, "GPIO 25"},
    {26, "GPIO 26"},
    {27, "GPIO 27"},
    {32, "GPIO 32"},
    {33, "GPIO 33"},
    {34, "GPIO 34 (только вход)"},
    {35, "GPIO 35 (только вход)"},
    {36, "GPIO 36 (только вход)"},
    {39, "GPIO 39 (только вход)"}};



// WiFi
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;

// Имена файлов
std::string DEVICES_FILE = "/devices.json";
std::string WIFI_CREDENTIALS_FILE = "/wifi_credentials.json";

// Определение задач для FreeRTOS
TaskHandle_t networkTask;
TaskHandle_t mainLogicTask;

// Мьютекс для защиты доступа к общим данным
SemaphoreHandle_t devicesMutex;

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

// Функция задачи для сетевых операций (ядро 0)
void networkTaskFunction(void *parameter)
{
    for (;;)
    {
        // Обработка пакетов miIO
        miIO.handlePackets();

        // Проверка подключения WiFi
        if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
        {
            connectWiFi();
        }
        // Сканирование BLE устройств
        // Периодическое сканирование BLE
        static unsigned long lastScanTime = 0;
        if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL)
        {
            startXiaomiScan(XIAOMI_SCAN_DURATION);
            updateDevicesStatus();
            // Обновляем время последнего сканирования
            lastScanTime = millis();
        }

        // Обновляем данные эмулируемых устройств
        updateEmulatedDevices();

        // Даем время другим задачам
        vTaskDelay(100 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
    }
}

// Функция для мониторинга памяти
void monitorMemory()
{
    Serial.printf("Свободно HEAP: %d байт\n", ESP.getFreeHeap());
    if (psramFound())
    {
        Serial.printf("Свободно PSRAM: %d байт\n", ESP.getFreePsram());
    }
}

// Функция задачи для основной логики (ядро 1)
void mainLogicTaskFunction(void *parameter)
{
    for (;;)
    {
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
                controlGPIO();
                xSemaphoreGive(devicesMutex);
                lastGpioControlTime = millis();
            }
        }
        // Добавляем переменную для отслеживания времени последнего сохранения
        unsigned long lastStatsSaveTime = 0;
        // Периодически сохраняем статистику (каждые 5 минут)
        unsigned long currentTime = millis();
        if (currentTime - lastStatsSaveTime > 300000 || currentTime < lastStatsSaveTime)
        {
            saveClientsToFile();
            lastStatsSaveTime = currentTime;
        }

        monitorMemory();
        // Даем время другим задачам
        vTaskDelay(20 / portTICK_PERIOD_MS); // Небольшая задержка для предотвращения перегрузки CPU
    }
}

// Создание задач с большим стеком в PSRAM
void createTasks()
{
    // Размер стека в словах (1 слово = 4 байта)
    const uint32_t stackSize = 8192;

    // Создаем задачу с использованием PSRAM для стека
    if (psramFound())
    {
        // Выделяем память для стека в PSRAM
        StackType_t *taskStack = (StackType_t *)ps_malloc(stackSize * sizeof(StackType_t));
        StaticTask_t *taskBuffer = (StaticTask_t *)ps_malloc(sizeof(StaticTask_t));

        if (taskStack && taskBuffer)
        {
            xTaskCreateStatic(
                mainLogicTaskFunction,
                "MainLogicTask",
                stackSize,
                NULL,
                1,
                taskStack,
                taskBuffer);
        }
        else
        {
            // Создание задачи для основной логики на ядре 1
            xTaskCreatePinnedToCore(
                mainLogicTaskFunction, // Функция задачи
                "MainLogicTask",       // Имя задачи
                4096,                  // Размер стека
                NULL,                  // Параметры
                1,                     // Приоритет
                &mainLogicTask,        // Указатель на задачу
                1);                    // Ядро 1
        }

        StackType_t *taskStackN = (StackType_t *)ps_malloc(stackSize * sizeof(StackType_t));
        StaticTask_t *taskBufferN = (StaticTask_t *)ps_malloc(sizeof(StaticTask_t));

        if (taskStackN && taskBufferN)
        {
            xTaskCreateStatic(
                networkTaskFunction,
                "NetworkTask",
                stackSize,
                NULL,
                1,
                taskStackN,
                taskBufferN);
        }
        else
        {
            // Создание задачи для сетевых операций на ядре 0
            xTaskCreatePinnedToCore(
                networkTaskFunction, // Функция задачи
                "NetworkTask",       // Имя задачи
                8192,                // Размер стека (больше для сетевых операций)
                NULL,                // Параметры
                1,                   // Приоритет
                &networkTask,        // Указатель на задачу
                0);                  // Ядро 0
        }
    }
    else
    {
        // Если PSRAM не доступна, используем обычное создание задачи
        // Создание задачи для сетевых операций на ядре 0
        xTaskCreatePinnedToCore(
            networkTaskFunction, // Функция задачи
            "NetworkTask",       // Имя задачи
            8192,                // Размер стека (больше для сетевых операций)
            NULL,                // Параметры
            1,                   // Приоритет
            &networkTask,        // Указатель на задачу
            0);                  // Ядро 0

        // Создание задачи для основной логики на ядре 1
        xTaskCreatePinnedToCore(
            mainLogicTaskFunction, // Функция задачи
            "MainLogicTask",       // Имя задачи
            4096,                  // Размер стека
            NULL,                  // Параметры
            1,                     // Приоритет
            &mainLogicTask,        // Указатель на задачу
            1);                    // Ядро 1
    }
}

// Настройка
void setup()
{
    Serial.begin(115200);
    Serial.println("Запуск системы...");

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

    // Инициализация случайного генератора
    randomSeed(analogRead(0));

    // Инициализация SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("Ошибка инициализации SPIFFS");
        return;
    }
    // Создаем мьютекс для защиты доступа к данным
    devicesMutex = xSemaphoreCreateMutex();
    loadGpioFromFile();

    // Инициализация GPIO
    for (auto &gpio : availableGpio)
    {
        pinMode(gpio.pin, OUTPUT);
        digitalWrite(gpio.pin, LOW);
    }

    // Инициализация LCD и кнопок
    initLCD();
    updateScrollText();
    initButtons();

    // Загрузка настроек WiFi и подключение
    loadWifiCredentialsFromFile();
    connectWiFi();

    // Инициализация BLE сканера
    setupXiaomiScanner();

    // Загрузка данных устройств
    loadClientsFromFile();

    // Инициализация веб-сервера    initWebServer();

    // Обновление текста прокрутки
    // updateScrollText();
    // updateLCD();
    // Загрузка сохраненного токена устройства
    String token = loadDeviceToken();
    // Инициализация протокола miIO
    miIO.begin();

    miIO.setDeviceToken(token.c_str());

    // Инициализация эмуляции устройств Xiaomi
    initXiaomiDeviceEmulation();

    // Настройка mDNS для обнаружения в Mi Home
    setupMDNS();

    Serial.println("Система готова к работе");
    Serial.println("Токен устройства: " + token);
    createTasks();
    Serial.println("Настройка завершена");
}

// Основной цикл
void loop()
{
    // Основной цикл пуст, так как используются задачи FreeRTOS
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
