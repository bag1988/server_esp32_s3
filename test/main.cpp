/**
 * Unit test runner using Unity
 */

#include <unity.h>
#include "arduino_mocks.h"

// Include test implementations
#include "crc32_test.cpp"
#include "control_gpio_test.cpp"
#include "device_manager_test.cpp"
#include "nvs_storage_test.cpp"

void setUp(void) {
    // Reset mocks before each test
    mock_millis = 0;
    pin_modes.clear();
    pin_values.clear();
    nvs_namespaces.clear();
}

void tearDown(void) {
    // Cleanup after each test
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    // Tests are auto-registered via TEST macros in included files
    // Unity will run all TEST/TEST_CASE macros
    
    return UNITY_END();
}