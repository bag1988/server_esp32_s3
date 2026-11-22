#include "arduino_mocks.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>

MockSerial Serial;
uint32_t mock_millis = 0;
std::map<uint8_t, uint8_t> pin_modes;
std::map<uint8_t, uint8_t> pin_values;

uint32_t millis() {
    return mock_millis;
}

void pinMode(uint8_t pin, uint8_t mode) {
    pin_modes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    pin_values[pin] = val;
}

int digitalRead(uint8_t pin) {
    auto it = pin_values.find(pin);
    return (it != pin_values.end()) ? it->second : 0;
}

int analogRead(uint8_t pin) {
    return 0;
}

void delay(uint32_t ms) {
    mock_millis += ms;
}

void delayMicroseconds(uint32_t us) {
    mock_millis += (us + 999) / 1000;
}

bool psramFound() {
    return true;
}

// FreeRTOS mocks
static uint32_t task_id_counter = 0;

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char* pcName, 
                       uint32_t usStackDepth, void* pvParameters,
                       uint32_t uxPriority, TaskHandle_t* pxCreatedTask) {
    task_id_counter++;
    if (pxCreatedTask) *pxCreatedTask = (TaskHandle_t)task_id_counter;
    printf("xTaskCreate: %s (id=%u)\n", pcName, task_id_counter);
    return pdTRUE;
}

void vTaskDelete(TaskHandle_t task) {
    printf("vTaskDelete: task=%p\n", task);
}

void vTaskDelay(uint32_t ticks) {
    mock_millis += ticks;
}

// Queue mocks
struct MockQueue {
    uint32_t item_size;
    uint32_t length;
    std::vector<std::vector<uint8_t>> items;
};

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
    MockQueue* q = new MockQueue();
    q->length = uxQueueLength;
    q->item_size = uxItemSize;
    printf("xQueueCreate: len=%u, item_size=%u\n", uxQueueLength, uxItemSize);
    return (QueueHandle_t)q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, uint32_t xTicksToWait) {
    MockQueue* q = (MockQueue*)xQueue;
    if (q->items.size() >= q->length) {
        return pdFALSE;
    }
    std::vector<uint8_t> item(q->item_size);
    memcpy(item.data(), pvItemToQueue, q->item_size);
    q->items.push_back(std::move(item));
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, uint32_t xTicksToWait) {
    MockQueue* q = (MockQueue*)xQueue;
    if (q->items.empty()) {
        return pdFALSE;
    }
    memcpy(pvBuffer, q->items.front().data(), q->item_size);
    q->items.erase(q->items.begin());
    return pdTRUE;
}

void vQueueDelete(QueueHandle_t xQueue) {
    delete (MockQueue*)xQueue;
}

// Semaphore mocks
SemaphoreHandle_t xSemaphoreCreateMutex() {
    return (SemaphoreHandle_t)1;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xTicksToWait) {
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    return pdTRUE;
}

// Task watchdog mocks
void esp_task_wdt_init(uint32_t timeout, bool panic) {
    printf("esp_task_wdt_init: timeout=%u, panic=%d\n", timeout, panic);
}

void esp_task_wdt_add(void* handle) {
    // no-op
}

void esp_task_wdt_reset() {
    // no-op
}

// NVS mocks
struct MockNVS {
    std::map<std::string, std::string> strings;
    std::map<std::string, std::vector<uint8_t>> blobs;
    std::map<std::string, uint32_t> u32s;
    std::map<std::string, uint64_t> u64s;
    std::map<std::string, uint8_t> u8s;
};

static std::map<std::string, MockNVS> nvs_namespaces;
static nvs_handle_t current_handle = 1;

esp_err_t nvs_flash_init() {
    return ESP_OK;
}

esp_err_t nvs_flash_erase() {
    nvs_namespaces.clear();
    return ESP_OK;
}

esp_err_t nvs_open(const char* name, uint32_t open_mode, nvs_handle_t* out_handle) {
    *out_handle = current_handle++;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) {
    // no-op
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value) {
    // Find namespace by handle (simplified)
    for (auto& ns : nvs_namespaces) {
        ns.second.strings[key] = value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out_value, size_t* length) {
    for (auto& ns : nvs_namespaces) {
        auto it = ns.second.strings.find(key);
        if (it != ns.second.strings.end()) {
            if (out_value) {
                strncpy(out_value, it->second.c_str(), *length - 1);
                out_value[*length - 1] = 0;
            }
            *length = it->second.length() + 1;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length) {
    for (auto& ns : nvs_namespaces) {
        ns.second.blobs[key] = std::vector<uint8_t>((uint8_t*)value, (uint8_t*)value + length);
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length) {
    for (auto& ns : nvs_namespaces) {
        auto it = ns.second.blobs.find(key);
        if (it != ns.second.blobs.end()) {
            if (out_value && *length >= it->second.size()) {
                memcpy(out_value, it->second.data(), it->second.size());
            }
            *length = it->second.size();
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value) {
    for (auto& ns : nvs_namespaces) {
        ns.second.u32s[key] = value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out_value) {
    for (auto& ns : nvs_namespaces) {
        auto it = ns.second.u32s.find(key);
        if (it != ns.second.u32s.end()) {
            *out_value = it->second;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u64(nvs_handle_t handle, const char* key, uint64_t value) {
    for (auto& ns : nvs_namespaces) {
        ns.second.u64s[key] = value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_get_u64(nvs_handle_t handle, const char* key, uint64_t* out_value) {
    for (auto& ns : nvs_namespaces) {
        auto it = ns.second.u64s.find(key);
        if (it != ns.second.u64s.end()) {
            *out_value = it->second;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value) {
    for (auto& ns : nvs_namespaces) {
        ns.second.u8s[key] = value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key) {
    for (auto& ns : nvs_namespaces) {
        ns.second.strings.erase(key);
        ns.second.blobs.erase(key);
        ns.second.u32s.erase(key);
        ns.second.u64s.erase(key);
        ns.second.u8s.erase(key);
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

SPIFFSClass SPIFFS;

void logAndSend(const String& message) {
    printf("[LOG] %s\n", message.c_str());
}

void logAndSendf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

String formatHeatingTime(unsigned long timeInMillis) {
    unsigned long hours = timeInMillis / 3600000;
    unsigned long minutes = (timeInMillis % 3600000) / 60000;
    unsigned long seconds = (timeInMillis % 60000) / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buf);
}