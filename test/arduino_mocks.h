// Arduino mocks for host-based unit testing
#ifndef ARDUINO_MOCKS_H
#define ARDUINO_MOCKS_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <functional>

// Arduino types
typedef unsigned long uint32_t;
typedef unsigned int uint16_t;
typedef unsigned char uint8_t;
typedef long int32_t;
typedef int int16_t;
typedef char int8_t;

// GPIO
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
#define LED_BUILTIN 2

// Pin modes
#define GPIO_NUM_0 0
#define GPIO_NUM_1 1
#define GPIO_NUM_2 2
#define GPIO_NUM_3 3
#define GPIO_NUM_4 4
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define GPIO_NUM_7 7
#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_20 20
#define GPIO_NUM_21 21

// Serial mock
class MockSerial {
public:
    void begin(uint32_t baud) { printf("Serial.begin(%lu)\n", baud); }
    void print(const char* s) { printf("%s", s); }
    void println(const char* s) { printf("%s\n", s); }
    template<typename T>
    void print(T val) { printf("%d", val); }
    template<typename T>
    void println(T val) { printf("%d\n", val); }
    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
};

extern MockSerial Serial;

// Millis mock
extern uint32_t mock_millis;
uint32_t millis();

// Pin mock
extern std::map<uint8_t, uint8_t> pin_modes;
extern std::map<uint8_t, uint8_t> pin_values;

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);

// String mock (use std::string)
using String = std::string;

// Delay mock
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

// ESP mock
class ESPClass {
public:
    static uint32_t getFreeHeap() { return 100000; }
    static uint32_t getHeapSize() { return 200000; }
    static uint32_t getPsramSize() { return 4000000; }
    static uint32_t getFreePsram() { return 3000000; }
    static uint32_t getFlashChipSize() { return 16000000; }
    static uint32_t getFlashChipSpeed() { return 80000000; }
    static uint8_t getFlashChipMode() { return 0; }
    static uint64_t getEfuseMac() { return 0x123456789ABC; }
    static uint8_t getChipRevision() { return 1; }
    static uint8_t getChipCores() { return 2; }
    static const char* getSdkVersion() { return "v5.0"; }
    static uint32_t getCpuFreqMHz() { return 240; }
    static void restart() { printf("ESP.restart()\n"); }
};

extern ESPClass ESP;

// WiFi mock
enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
};

class WiFiClass {
public:
    static void begin(const char* ssid, const char* pass) {
        printf("WiFi.begin(%s, %s)\n", ssid, pass);
    }
    static wl_status_t status() { return WL_CONNECTED; }
    static const char* localIP() { return "192.168.1.100"; }
    static uint8_t macAddress(uint8_t* mac) { return 0; }
};

extern WiFiClass WiFi;

// PSRAM mock
bool psramFound();

// Task mock (FreeRTOS)
#define portTICK_PERIOD_MS 1
typedef void* TaskHandle_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)

typedef void (*TaskFunction_t)(void*);

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char* pcName, 
                       uint32_t usStackDepth, void* pvParameters,
                       uint32_t uxPriority, TaskHandle_t* pxCreatedTask);

void vTaskDelete(TaskHandle_t task);
void vTaskDelay(uint32_t ticks);

// Queue mock
typedef void* QueueHandle_t;
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, uint32_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, uint32_t xTicksToWait);
void vQueueDelete(QueueHandle_t xQueue);

// Semaphore mock
typedef void* SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// Task watchdog mock
void esp_task_wdt_init(uint32_t timeout, bool panic);
void esp_task_wdt_add(void* handle);
void esp_task_wdt_reset();

// NVS mock
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND -1
#define ESP_ERR_NVS_NO_FREE_PAGES -2
#define ESP_ERR_NVS_NEW_VERSION_FOUND -3
#define ESP_ERR_NVS_CRC_MISMATCH -4

typedef int esp_err_t;
typedef void* nvs_handle_t;

#define NVS_READONLY 0
#define NVS_READWRITE 1

esp_err_t nvs_flash_init();
esp_err_t nvs_flash_erase();
esp_err_t nvs_open(const char* name, uint32_t open_mode, nvs_handle_t* out_handle);
void nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out_value, size_t* length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out_value);
esp_err_t nvs_set_u64(nvs_handle_t handle, const char* key, uint64_t value);
esp_err_t nvs_get_u64(nvs_handle_t handle, const char* key, uint64_t* out_value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);

// SPIFFS mock
class SPIFFSClass {
public:
    bool begin(bool format = false) { return true; }
};

extern SPIFFSClass SPIFFS;

// Logging mock
void logAndSend(const String& message);
void logAndSendf(const char* format, ...);

// String formatting
String formatHeatingTime(unsigned long timeInMillis);

#endif // ARDUINO_MOCKS_H