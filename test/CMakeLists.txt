cmake_minimum_required(VERSION 3.16)
project(esp32_heating_tests LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GTest
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/VariablesInfo
    ${CMAKE_SOURCE_DIR}/lib/DeviceManager
    ${CMAKE_SOURCE_DIR}/lib/NvsStorage
    ${CMAKE_SOURCE_DIR}/lib/SpiffsSetting
    ${CMAKE_SOURCE_DIR}/lib/TaskMonitor
    ${CMAKE_SOURCE_DIR}/lib/XiaomiScanner
    ${CMAKE_SOURCE_DIR}/lib/LcdSetting
    ${CMAKE_SOURCE_DIR}/lib/WebServerSettings
    ${CMAKE_SOURCE_DIR}/lib/OtaSetting
    ${CMAKE_SOURCE_DIR}/test
)

# Mock sources
set(MOCK_SOURCES
    test/arduino_mocks.cpp
)

# Test sources (now in test/ root)
set(TEST_SOURCES
    test/main.cpp
    test/crc32_test.cpp
    test/control_gpio_test.cpp
    test/device_manager_test.cpp
    test/nvs_storage_test.cpp
)

# Create test executable
add_executable(test_runner ${MOCK_SOURCES} ${TEST_SOURCES})

# Link with GTest
target_link_libraries(test_runner GTest::gtest GTest::gtest_main)

# Compile definitions
target_compile_definitions(test_runner PRIVATE
    UNIT_TEST
    ARDUINO=10819
    ESP32
    ARDUINO_ESP32S3_DEV
    ARDUINO_ARCH_ESP32
)

# Enable testing
enable_testing()
add_test(NAME unit_tests COMMAND test_runner)