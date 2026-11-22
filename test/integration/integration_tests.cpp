/**
 * Integration tests for ESP32 target
 * Run on device with: pio test -e integration
 * Or upload test firmware and monitor serial
 */

#ifdef ARDUINO
#include <Arduino.h>
#include <unity.h>
#include "device_manager.h"
#include "task_monitor.h"
#include "nvs_storage.h"
#include "spiffs_setting.h"
#include "xiaomi_scanner.h"

// Test: Device Manager initialization
void test_device_manager_init() {
    initDeviceManager();
    TEST_ASSERT_NOT_NULL(deviceCmdQueue);
    TEST_ASSERT_NOT_NULL(deviceManagerTaskHandle);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    TEST_ASSERT_TRUE(uxTaskGetStackHighWaterMark(deviceManagerTaskHandle) > 500);
}

// Test: Device Manager command round-trip
void test_device_manager_cmd_roundtrip() {
    DeviceCmd cmd{};
    cmd.type = DeviceCmdType::CMD_ADD_DEVICE;
    cmd.name = "Test Device";
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    cmd.targetTemperature = 25.0;
    cmd.enabled = true;
    cmd.gpioPins = {1, 4};
    
    DeviceResponse response;
    sendDeviceCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    
    // Verify device was added
    cmd.type = DeviceCmdType::CMD_GET_DEVICE;
    cmd.macAddress = "aa:bb:cc:dd:ee:ff";
    sendDeviceCmd(cmd, &response);
    
    TEST_ASSERT_TRUE(response.success);
    TEST_ASSERT_TRUE(response.deviceFound);
    TEST_ASSERT_EQUAL_STRING("Test Device", response.device.name.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0, response.device.targetTemperature);
    TEST_ASSERT_TRUE(response.device.enabled);
}

// Test: NVS storage round-trip
void test_nvs_storage_roundtrip() {
    // Save WiFi credentials
    TEST_ASSERT_EQUAL(ESP_OK, NvsStorage::saveWifiCredentials("TestSSID", "TestPass"));
    
    std::string ssid, password;
    TEST_ASSERT_EQUAL(ESP_OK, NvsStorage::loadWifiCredentials(ssid, password));
    TEST_ASSERT_EQUAL_STRING("TestSSID", ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPass", password.c_str());
}

// Test: CRC32 verification
void test_crc32_verification() {
    const char* data = "test data for CRC";
    uint32_t crc1 = NvsStorage::calculateCrc32((const uint8_t*)data, strlen(data));
    uint32_t crc2 = NvsStorage::calculateCrc32((const uint8_t*)data, strlen(data));
    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
    
    // Modified data should have different CRC
    char modified[32];
    strcpy(modified, data);
    modified[0] ^= 0xFF;
    uint32_t crc3 = NvsStorage::calculateCrc32((const uint8_t*)modified, strlen(modified));
    TEST_ASSERT_NOT_EQUAL(crc1, crc3);
}

// Test: Task Monitor registration
void test_task_monitor_registration() {
    TaskHandle_t handle = nullptr;
    xTaskCreate([](void*) { vTaskDelay(1000); }, "test_task", 2048, nullptr, 1, &handle);
    
    TASK_MONITOR.registerTask(handle, "test_task", 100, false, nullptr, nullptr);
    
    // Wait for monitor to check
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // Cleanup
    vTaskDelete(handle);
}

// Test: GPIO control logic
void test_gpio_control_logic() {
    // This would test the actual hardware GPIO
    // For now just verify pins can be set
    pinMode(1, OUTPUT);
    digitalWrite(1, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(1));
    
    digitalWrite(1, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(1));
}

// Test: BLE scanner task
void test_ble_scanner_task() {
    // Verify BLE scanner task is running
    TEST_ASSERT_NOT_NULL(bleScannerTaskHandle);
    TEST_ASSERT_TRUE(uxTaskGetStackHighWaterMark(bleScannerTaskHandle) > 1000);
}

// Test: Web server endpoints
void test_web_server_endpoints() {
    // Verify web server is running
    // This would need HTTP client to test endpoints
    TEST_ASSERT_TRUE(true);  // Placeholder
}

// Unity test runner
void run_integration_tests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_device_manager_init);
    RUN_TEST(test_device_manager_cmd_roundtrip);
    RUN_TEST(test_nvs_storage_roundtrip);
    RUN_TEST(test_crc32_verification);
    RUN_TEST(test_task_monitor_registration);
    RUN_TEST(test_gpio_control_logic);
    RUN_TEST(test_ble_scanner_task);
    RUN_TEST(test_web_server_endpoints);
    
    UNITY_END();
}

// Arduino setup/loop for running tests on device
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize components
    SPIFFS.begin(true);
    NvsStorage::init();
    SpiffsSetting::init();
    initDeviceManager();
    initLCD();
    setupXiaomiScanner();
    initWebServer();
    TASK_MONITOR.begin(5000);
    
    // Run tests
    run_integration_tests();
}

void loop() {
    // Tests run once in setup
    delay(1000);
}

#endif // ARDUINO