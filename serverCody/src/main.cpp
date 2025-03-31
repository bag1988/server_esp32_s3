#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include "variables_info.h"
#include "lcd_setting.h"
#include "web_server_setting.h"
#include "spiffs_setting.h"
#include "xiaomi_scanner.h"

// Глобальные переменные
std::vector<DeviceData> devices;
int selectedDeviceIndex = 0;
std::vector<GpioPin> availableGpio = {
    {25, "Реле 1"},
    {26, "Реле 2"},
    {27, "Реле 3"},
    {32, "Датчик 1"},
    {33, "Датчик 2"}
};

// Состояние
EditMode currentEditMode = EDIT_TEMPERATURE;
bool isEditing = false;
int gpioSelectionIndex = 0;

// Прокрутка текста
std::string scrollText = "";
int scrollPosition = 0;
unsigned long lastScrollTime = 0;

// WiFi
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;

// Имена файлов
std::string DEVICES_FILE = "/devices.json";
std::string WIFI_CREDENTIALS_FILE = "/wifi_credentials.json";

// Управление GPIO
void controlGPIO() {
    std::vector<int> gpiosToTurnOn;

    // Собираем GPIO для включения
    for (auto& device : devices) {
        if (device.needsHeating()) {
            gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());
            device.gpioOnTime += CONTROL_DELAY / 1000;
        }
    }

    // Удаляем дубликаты GPIO
    std::sort(gpiosToTurnOn.begin(), gpiosToTurnOn.end());
    gpiosToTurnOn.erase(std::unique(gpiosToTurnOn.begin(), gpiosToTurnOn.end()), gpiosToTurnOn.end());

    // Управляем GPIO
    for (auto& gpio : availableGpio) {
        bool shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio.pin) != gpiosToTurnOn.end();
        digitalWrite(gpio.pin, shouldTurnOn ? HIGH : LOW);
    }
}

// Настройка
void setup() {
    Serial.begin(115200);
    Serial.println("Запуск serverGemmeni...");
    
    // Инициализация SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Ошибка инициализации SPIFFS");
        return;
    }
    
    loadGpioFromFile();

    // Инициализация GPIO
    for (auto& gpio : availableGpio) {
        pinMode(gpio.pin, OUTPUT);
        digitalWrite(gpio.pin, LOW);
    }
    
    // Инициализация LCD и кнопок
    initLCD();
    initButtons();
    
    // Загрузка настроек WiFi и подключение
    loadWifiCredentialsFromFile();
    connectWiFi();
    
    // Инициализация BLE сканера
    setupXiaomiScanner();
    
    // Загрузка данных устройств
    loadClientsFromFile();
    
    // Инициализация веб-сервера
    initWebServer();
    
    // Обновление текста прокрутки
    updateScrollText();
    updateLCD();
    
    Serial.println("Настройка завершена");
}

// Основной цикл
void loop() {
    // Обработка нажатий кнопок
    handleButtons();

    // Обновление прокрутки текста
    if (!isEditing) {
        if (millis() - lastScrollTime > SCROLL_DELAY) {
            scrollPosition++;
            if (scrollPosition > scrollText.length()) {
                scrollPosition = 0;
            }
            updateLCD();
            lastScrollTime = millis();
        }
    }
    // Управление GPIO
    static unsigned long lastGpioControlTime = 0;
    if (millis() - lastGpioControlTime > CONTROL_DELAY) {
        controlGPIO();
        lastGpioControlTime = millis();
    }
    
    // Периодическое сканирование BLE
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime > XIAOMI_SCAN_INTERVAL) {
        startXiaomiScan(XIAOMI_SCAN_DURATION);
        updateDevicesStatus();
        lastScanTime = millis();
    }
    
    // Проверка подключения WiFi
    if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY)) {
        connectWiFi();
    }
    
    // Обновление LCD
    updateLCD();

    delay(20); // Задержка 10 мс
}
