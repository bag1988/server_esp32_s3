#include <unity.h>
#include "arduino_mocks.h"
#include <vector>
#include <string>
#include <cstring>

// NVS Storage tests using the mock NVS implementation
void setUp(void) {
    nvs_flash_init();
    nvs_namespaces.clear();
}

void tearDown(void) {
}

void test_save_and_load_string(void) {
    nvs_handle_t handle;
    nvs_open("test", NVS_READWRITE, &handle);
    
    nvs_set_str(handle, "ssid", "MyWiFi");
    nvs_set_str(handle, "password", "secret123");
    nvs_commit(handle);
    nvs_close(handle);
    
    nvs_open("test", NVS_READONLY, &handle);
    char buf[64];
    size_t len = sizeof(buf);
    nvs_get_str(handle, "ssid", buf, &len);
    TEST_ASSERT_EQUAL_STRING("MyWiFi", buf);
    
    len = sizeof(buf);
    nvs_get_str(handle, "password", buf, &len);
    TEST_ASSERT_EQUAL_STRING("secret123", buf);
    nvs_close(handle);
}

void test_save_and_load_blob(void) {
    nvs_handle_t handle;
    nvs_open("test", NVS_READWRITE, &handle);
    
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    nvs_set_blob(handle, "blob", data, sizeof(data));
    nvs_commit(handle);
    nvs_close(handle);
    
    nvs_open("test", NVS_READONLY, &handle);
    uint8_t out[10];
    size_t len = sizeof(out);
    nvs_get_blob(handle, "blob", out, &len);
    
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL(0, memcmp(data, out, 5));
    nvs_close(handle);
}

void test_save_and_load_u32(void) {
    nvs_handle_t handle;
    nvs_open("test", NVS_READWRITE, &handle);
    
    nvs_set_u32(handle, "counter", 42);
    nvs_set_u32(handle, "crc", 0xDEADBEEF);
    nvs_commit(handle);
    nvs_close(handle);
    
    nvs_open("test", NVS_READONLY, &handle);
    uint32_t counter, crc;
    nvs_get_u32(handle, "counter", &counter);
    nvs_get_u32(handle, "crc", &crc);
    
    TEST_ASSERT_EQUAL(42, counter);
    TEST_ASSERT_EQUAL(0xDEADBEEF, crc);
    nvs_close(handle);
}

void test_save_and_load_u64(void) {
    nvs_handle_t handle;
    nvs_open("test", NVS_READWRITE, &handle);
    
    nvs_set_u64(handle, "timestamp", 12345678901234ULL);
    nvs_commit(handle);
    nvs_close(handle);
    
    nvs_open("test", NVS_READONLY, &handle);
    uint64_t timestamp;
    nvs_get_u64(handle, "timestamp", &timestamp);
    
    TEST_ASSERT_EQUAL(12345678901234ULL, timestamp);
    nvs_close(handle);
}

void test_not_found_returns_error(void) {
    nvs_handle_t handle;
    nvs_open("test", NVS_READONLY, &handle);
    
    char buf[64];
    size_t len = sizeof(buf);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, nvs_get_str(handle, "nonexistent", buf, &len));
    
    uint32_t val;
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, nvs_get_u32(handle, "nonexistent", &val));
    nvs_close(handle);
}