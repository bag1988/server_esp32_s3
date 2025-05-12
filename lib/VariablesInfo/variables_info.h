#ifndef VARIABLES_INFO_H
#define VARIABLES_INFO_H

#include <Arduino.h>
#include <vector>
#include <string>

// Константы
#define SCROLL_DELAY 300           // Задержка прокрутки текста (мс)
#define CONTROL_DELAY 30000        // Интервал проверки и управления GPIO (мс)
#define WIFI_RECONNECT_DELAY 60000 // Интервал попыток переподключения к WiFi (мс)
#define XIAOMI_SCAN_INTERVAL 60000 // Интервал сканирования датчиков Xiaomi (мс)
#define XIAOMI_SCAN_DURATION 30  // Продолжительность сканирования BLE (секунд)
#define XIAOMI_OFFLINE_TIMEOUT 300000 // 5 минут до перехода в оффлайн

// Структура для хранения учетных данных WiFi
struct WifiCredentials {
    std::string ssid;
    std::string password;
};

// Объединенная структура данных для клиента/датчика
struct DeviceData {
    std::string name;                  // Имя устройства
    std::string macAddress;            // MAC-адрес датчика
    float targetTemperature = 25.0; // Целевая температура
    float currentTemperature = 0.0; // Текущая температура
    float humidity = 0.0;             // Влажность
    int battery = 0;              // Уровень заряда батареи
    bool enabled = true;          // Включено ли устройство
    bool isOnline = false;        // Находится ли устройство в сети
    unsigned long lastUpdate = 0; // Время последнего обновления данных
    std::vector<int> gpioPins;    // Пины GPIO для управления    
    bool heatingActive; // Добавляем поле для отслеживания текущего состояния обогрева
    unsigned long heatingStartTime;    // Время последнего включения обогрева
    unsigned long totalHeatingTime;    // Общее время работы обогрева в миллисекундах
    
    // Конструктор по умолчанию
    DeviceData() : 
        name(""),
        macAddress(""),
        currentTemperature(25.0),
        humidity(0.0),
        battery(0),
        lastUpdate(0),
        isOnline(false),
        targetTemperature(25.0),
        enabled(false),
        heatingStartTime(0),
        totalHeatingTime(0) {}
    
    // Конструктор с основными параметрами
    DeviceData(const std::string& _name, const std::string& _mac) :
        name(_name),
        macAddress(_mac),
        currentTemperature(25.0),
        humidity(0.0),
        battery(0),
        lastUpdate(0),
        isOnline(true),
        targetTemperature(25.0),
        enabled(false),
        heatingStartTime(0),
        totalHeatingTime(0) {}
        
    // Метод для обновления данных датчика
    void updateSensorData(float temp, float hum, uint8_t bat) {
        currentTemperature = temp;
        humidity = hum;
        battery = bat;
        lastUpdate = millis();
        isOnline = true;
    }
    
    // Метод для проверки актуальности данных
    bool isDataValid(unsigned long timeout = XIAOMI_OFFLINE_TIMEOUT) const {
        return isOnline && (millis() - lastUpdate < timeout);
    }
};


struct GpioPin {
    int pin;
    std::string name;
    
    GpioPin() :
        pin(0),
        name(""){}
    GpioPin(int p, const std::string& n) : pin(p), name(n) {}
};
// Глобальные переменные (объявлены как extern)
extern std::vector<DeviceData> devices;
extern std::vector<GpioPin> availableGpio;
extern int gpioSelectionIndex;
extern WifiCredentials wifiCredentials;
extern bool wifiConnected;
extern unsigned long lastWiFiAttemptTime;
// Мьютекс для защиты доступа к общим данным
extern SemaphoreHandle_t devicesMutex;
extern float board_temperature;
extern unsigned long serverWorkTime;
#endif // VARIABLES_INFO_H
