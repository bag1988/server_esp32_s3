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
#include "device_manager.h"
#include <atomic>
#include "esp_system.h"
#include "driver/temp_sensor.h"
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>

#define KEYPAD_PIN 2
#define NUM_LEDS 1

Adafruit_NeoPixel pixels(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

WifiCredentials wifiCredentials;
unsigned long serverWorkTime = 0;
float hysteresisTemp = 1.5;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
float board_temperature = 0.0;

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

// Forward declarations
void networkTaskFunction(void* parameter);
void lcdTaskFunction(void* parameter);

void networkFunc()
{
    if (isOtaActive())
    {
        vTaskDelay(20 / portTICK_PERIOD_MS);
        return;
    }

    if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
    {
        connectWiFi();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    if (wifiConnected)
    {
        handleOTA();
    }

    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL)
    {
        lastScanTime = millis();
        startXiaomiScan();
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
}

void createTasksStandart()
{
    xTaskCreate(
        networkTaskFunction,
        "networkTask",
        8192,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        lcdTaskFunction,
        "lcdTask",
        4096,
        NULL,
        1,
        NULL
    );
}

void networkTaskFunction(void* parameter)
{
    esp_task_wdt_add(nullptr);
    for (;;) {
        esp_task_wdt_reset();
        networkFunc();
    }
}

void lcdTaskFunction(void* parameter)
{
    esp_task_wdt_add(nullptr);
    for (;;) {
        esp_task_wdt_reset();
        handleButtons();
        updateMainScreenLCD();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void ReadDataInSPIFFS()
{
    // Загрузка данных устройств
}

// Настройка
void setup()
{
    Serial.begin(115200);
    logAndSend("Запуск системы...");
    pixels.begin();
    pixels.setBrightness(50);
    pixels.show();

    if (psramFound())
    {
        logAndSend("PSRAM найдена и инициализирована");
    }
    else
    {
        logAndSend("PSRAM не найдена! Некоторые функции могут работать некорректно");
    }

    if (!SPIFFS.begin(true))
    {
        logAndSend("Ошибка инициализации SPIFFS");
        return;
    }
    loadWifiCredentialsFromFile();
    loadServerWorkTime();

    initDeviceManager();

    initLCD();

    connectWiFi();

    setupXiaomiScanner();

    initWebServer();

    createTasksStandart();

    if (wifiConnected)
    {
        initOTA();
        MDNS.addService("http", "tcp", 80);
        logAndSend("MDNS http service registered");
    }

    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(temp_sensor_set_config(temp_sensor));
    ESP_ERROR_CHECK(temp_sensor_start());

    updateMainScreenLCD();

    logAndSend("Датчик температуры инициализирован");
    logAndSend("Настройка завершена");
    logAndSend("Система готова к работе");

    esp_task_wdt_init(30, true);
    esp_task_wdt_add(nullptr);
}

void loop()
{
    esp_task_wdt_reset();
    
    if (wifiConnected)
    {
        pixels.setPixelColor(0, pixels.Color(0, 255, 0));
        pixels.show();
        esp_err_t ret = temp_sensor_read_celsius(&board_temperature);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    else
    {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0));
        pixels.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        pixels.clear();
        pixels.show();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}