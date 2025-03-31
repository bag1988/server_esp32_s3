#ifndef VARIABLES_INFO_H
#define VARIABLES_INFO_H

#include <Arduino.h>
#include <vector>
#include <string>

// Константы
#define SCROLL_DELAY 300           // Задержка прокрутки текста (мс)
#define CONTROL_DELAY 30000        // Интервал проверки и управления GPIO (мс)
#define WIFI_RECONNECT_DELAY 60000 // Интервал попыток переподключения к WiFi (мс)
#define BUTTON_DEBOUNCE_DELAY 200  // Задержка для устранения дребезга кнопок (мс)
#define XIAOMI_SCAN_INTERVAL 60000 // Интервал сканирования датчиков Xiaomi (мс)
#define XIAOMI_SCAN_DURATION 5000  // Продолжительность сканирования BLE (мс)
#define DATA_TIMEOUT 300000        // Таймаут для данных датчика (5 минут)
#define XIAOMI_OFFLINE_TIMEOUT 300000 // 5 минут до перехода в оффлайн

// Пины для кнопок
#define BUTTON_UP 12
#define BUTTON_DOWN 14
#define BUTTON_SELECT 13

// Пины для LCD дисплея
#define LCD_RS 19
#define LCD_EN 23
#define LCD_D4 18
#define LCD_D5 17
#define LCD_D6 16
#define LCD_D7 15

// Режимы редактирования
enum EditMode {
    EDIT_TEMPERATURE,
    EDIT_GPIO,
    EDIT_ENABLED
};

// Структура для хранения учетных данных WiFi
struct WifiCredentials {
    std::string ssid;
    std::string password;
};

// Объединенная структура данных для клиента/датчика
struct DeviceData {
    std::string name;                  // Имя устройства
    std::string macAddress;            // MAC-адрес датчика
    float targetTemperature = 20.0; // Целевая температура
    float currentTemperature = 0.0; // Текущая температура
    int humidity = 0;             // Влажность
    int battery = 0;              // Уровень заряда батареи
    bool enabled = true;          // Включено ли устройство
    bool isOnline = false;        // Находится ли устройство в сети
    unsigned long lastUpdate = 0; // Время последнего обновления данных
    std::vector<int> gpioPins;    // Пины GPIO для управления
    unsigned long gpioOnTime = 0; // Время работы GPIO в секундах
    
    // Конструктор по умолчанию
    DeviceData() : 
        name(""),
        macAddress(""),
        currentTemperature(0.0),
        humidity(0),
        battery(0),
        lastUpdate(0),
        isOnline(false),
        targetTemperature(25.0),
        enabled(false),
        gpioOnTime(0) {}
    
    // Конструктор с основными параметрами
    DeviceData(const std::string& _name, const std::string& _mac) :
        name(_name),
        macAddress(_mac),
        currentTemperature(0.0),
        humidity(0),
        battery(0),
        lastUpdate(0),
        isOnline(false),
        targetTemperature(20.0),
        enabled(true),
        gpioOnTime(0) {}
    
    // Метод для проверки необходимости включения обогрева
    bool needsHeating() const {
        return enabled && isOnline && (currentTemperature + 2) < targetTemperature;
    }
    
    // Метод для обновления данных датчика
    void updateSensorData(float temp, uint8_t hum, uint8_t bat) {
        currentTemperature = temp;
        humidity = hum;
        battery = bat;
        lastUpdate = millis();
        isOnline = true;
    }
    
    // Метод для проверки актуальности данных
    bool isDataValid(unsigned long timeout = 300000) const {
        return isOnline && (millis() - lastUpdate < timeout);
    }
};


struct GpioPin {
    int pin;
    std::string name;
    
    GpioPin(int p, const std::string& n) : pin(p), name(n) {}
};
// Глобальные переменные (объявлены как extern)
extern std::vector<DeviceData> devices;
extern int selectedDeviceIndex;
extern std::vector<GpioPin> availableGpio;
extern EditMode currentEditMode;
extern bool isEditing;
extern int gpioSelectionIndex;
extern std::string scrollText;
extern int scrollPosition;
extern unsigned long lastScrollTime;
extern WifiCredentials wifiCredentials;
extern bool wifiConnected;
extern std::string DEVICES_FILE;
extern std::string WIFI_CREDENTIALS_FILE;
extern unsigned long lastWiFiAttemptTime;
#endif // VARIABLES_INFO_H
