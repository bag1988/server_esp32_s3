#include <unity.h>
#include "arduino_mocks.h"
#include <vector>
#include <string>
#include <algorithm>

// Mock variables_info.h structures
enum stateGpioPin : uint8_t {
    STATE_GPIO_AUTO = 0,
    STATE_GPIO_ON = 1,
    STATE_GPIO_OFF = 2
};

struct GpioPin {
    uint8_t pin;
    uint8_t state;
    std::string name;
    unsigned long totalHeatingTime;
    GpioPin() : pin(0), state(STATE_GPIO_AUTO), name(""), totalHeatingTime(0) {}
    GpioPin(uint8_t p, uint8_t s, const std::string& n) : pin(p), state(s), name(n), totalHeatingTime(0) {}
};

struct DeviceData {
    std::string name;
    std::string macAddress;
    float targetTemperature = 25.0;
    float currentTemperature = 0.0;
    float humidity = 0.0;
    uint8_t battery = 0;
    bool enabled = true;
    bool isOnline = false;
    unsigned long lastUpdate = 0;
    std::vector<uint8_t> gpioPins;
    bool heatingActive = false;
    unsigned long heatingStartTime = 0;
    unsigned long totalHeatingTime = 0;
    uint16_t batteryV = 0;

    DeviceData() : name(""), macAddress(""), currentTemperature(25.0), humidity(0.0), battery(0), batteryV(0),
                   lastUpdate(0), isOnline(false), targetTemperature(25.0), enabled(false),
                   heatingStartTime(0), totalHeatingTime(0) {}

    DeviceData(const std::string& _name, const std::string& _mac) : name(_name), macAddress(_mac),
                                                                     currentTemperature(25.0), humidity(0.0), battery(0), batteryV(0),
                                                                     lastUpdate(0), isOnline(true), targetTemperature(25.0), enabled(false),
                                                                     heatingStartTime(0), totalHeatingTime(0) {}

    void updateSensorData(float temp, float hum, uint8_t bat, uint16_t batV) {
        currentTemperature = temp;
        humidity = hum;
        battery = bat;
        lastUpdate = millis();
        isOnline = true;
        batteryV = batV;
    }

    bool isDataValid(unsigned long timeout = 300000) const {
        return isOnline && (millis() - lastUpdate < timeout);
    }
};

// Global variables for testing
float hysteresisTemp = 1.5;
std::vector<DeviceData> g_devices;
std::vector<GpioPin> g_availableGpio;
static unsigned long lastGpioControlTime = 0;

static void controlGPIOInternal() {
    unsigned long elapsedTime = millis() - lastGpioControlTime;
    lastGpioControlTime = millis();

    std::vector<uint8_t> gpiosToTurnOn;

    for (auto& device : g_devices) {
        if (device.isDataValid()) {
            if (!device.heatingActive && device.enabled && (device.currentTemperature + hysteresisTemp) < device.targetTemperature) {
                device.heatingActive = true;
            } else if (device.heatingActive && device.currentTemperature >= device.targetTemperature) {
                device.heatingActive = false;
            } else if (!device.enabled && device.heatingActive) {
                device.totalHeatingTime += elapsedTime;
                device.heatingActive = false;
            }

            if (device.heatingActive) {
                device.totalHeatingTime += elapsedTime;
                gpiosToTurnOn.insert(gpiosToTurnOn.end(), device.gpioPins.begin(), device.gpioPins.end());
            }
        } else if (device.isOnline) {
            device.isOnline = false;
            device.totalHeatingTime += elapsedTime;
            device.heatingActive = false;
        }
    }

    for (auto& gpio : g_availableGpio) {
        bool shouldTurnOn = false;
        if (gpio.state == STATE_GPIO_AUTO) {
            shouldTurnOn = std::find(gpiosToTurnOn.begin(), gpiosToTurnOn.end(), gpio.pin) != gpiosToTurnOn.end();
        } else {
            shouldTurnOn = gpio.state == STATE_GPIO_ON;
        }

        digitalWrite(gpio.pin, shouldTurnOn ? HIGH : LOW);
        if (shouldTurnOn) {
            gpio.totalHeatingTime += elapsedTime;
        }
    }
}

void setUp(void) {
    mock_millis = 0;
    lastGpioControlTime = 0;
    g_devices.clear();
    g_availableGpio.clear();
    pin_values.clear();
    hysteresisTemp = 1.5;
    
    g_availableGpio = {
        {1, STATE_GPIO_AUTO, "GPIO 1", 0},
        {4, STATE_GPIO_AUTO, "GPIO 4", 0},
        {5, STATE_GPIO_AUTO, "GPIO 5", 0},
        {6, STATE_GPIO_AUTO, "GPIO 6", 0},
        {7, STATE_GPIO_AUTO, "GPIO 7", 0}
    };
}

void tearDown(void) {
    mock_millis = 0;
}

void test_heating_activates_when_temp_below_target(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 20.0;
    device.gpioPins = {1, 4};
    device.isOnline = true;
    device.lastUpdate = millis();
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_TRUE(g_devices[0].heatingActive);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(1));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(4));
}

void test_heating_deactivates_when_temp_reaches_target(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 26.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis();
    device.heatingActive = true;
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_FALSE(g_devices[0].heatingActive);
    TEST_ASSERT_EQUAL(LOW, digitalRead(1));
}

void test_heating_stays_on_within_hysteresis(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 24.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis();
    device.heatingActive = true;
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_TRUE(g_devices[0].heatingActive);
}

void test_heating_turns_off_when_disabled(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = false;
    device.targetTemperature = 25.0;
    device.currentTemperature = 20.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis();
    device.heatingActive = true;
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_FALSE(g_devices[0].heatingActive);
    TEST_ASSERT_EQUAL(LOW, digitalRead(1));
}

void test_device_goes_offline_after_timeout(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 20.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis() - 400000;
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_FALSE(g_devices[0].isOnline);
    TEST_ASSERT_FALSE(g_devices[0].heatingActive);
}

void test_manual_gpio_overrides_auto(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 20.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis();
    g_devices.push_back(device);
    
    g_availableGpio[0].state = STATE_GPIO_OFF;
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_TRUE(g_devices[0].heatingActive);
    TEST_ASSERT_EQUAL(LOW, digitalRead(1));
}

void test_total_heating_time_accumulates(void) {
    DeviceData device("Test Device", "aa:bb:cc:dd:ee:ff");
    device.enabled = true;
    device.targetTemperature = 25.0;
    device.currentTemperature = 20.0;
    device.gpioPins = {1};
    device.isOnline = true;
    device.lastUpdate = millis();
    g_devices.push_back(device);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    mock_millis = 5000;
    controlGPIOInternal();
    
    TEST_ASSERT_GREATER_THAN(0, g_devices[0].totalHeatingTime);
    TEST_ASSERT_GREATER_THAN(0, g_availableGpio[0].totalHeatingTime);
}

void test_multiple_devices_shared_gpio(void) {
    DeviceData device1("Device 1", "aa:bb:cc:dd:ee:01");
    device1.enabled = true;
    device1.targetTemperature = 25.0;
    device1.currentTemperature = 20.0;
    device1.gpioPins = {1};
    device1.isOnline = true;
    device1.lastUpdate = millis();
    
    DeviceData device2("Device 2", "aa:bb:cc:dd:ee:02");
    device2.enabled = true;
    device2.targetTemperature = 25.0;
    device2.currentTemperature = 30.0;
    device2.gpioPins = {1};
    device2.isOnline = true;
    device2.lastUpdate = millis();
    
    g_devices.push_back(device1);
    g_devices.push_back(device2);
    
    mock_millis = 1000;
    lastGpioControlTime = 0;
    controlGPIOInternal();
    
    TEST_ASSERT_TRUE(g_devices[0].heatingActive);
    TEST_ASSERT_FALSE(g_devices[1].heatingActive);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(1));
}