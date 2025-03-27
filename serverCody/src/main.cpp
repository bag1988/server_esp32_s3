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
std::vector<int> availableGpio = {25, 26, 27, 32, 33};

// Состояние
EditMode currentEditMode = EDIT_TEMPERATURE;
bool isEditing = false;
int gpioSelectionIndex = 0;

// Прокрутка текста
String scrollText = "";
int scrollPosition = 0;
unsigned long lastScrollTime = 0;

// WiFi
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;

// Имена файлов
String DEVICES_FILE = "/devices.json";
String WIFI_CREDENTIALS_FILE = "/wifi_credentials.json";

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
    for (int gpio : availableGpio) {
        bool shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio) != gpiosToTurnOn.end();
        digitalWrite(gpio, shouldTurnOn ? HIGH : LOW);
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
    
    // Инициализация GPIO
    for (int gpio : availableGpio) {
        pinMode(gpio, OUTPUT);
        digitalWrite(gpio, LOW);
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
    loadDevicesFromFile();
    
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
    
    // Обработка веб-сервера
    server.handleClient();
    
    // Обновление LCD
    updateLCD();
}
