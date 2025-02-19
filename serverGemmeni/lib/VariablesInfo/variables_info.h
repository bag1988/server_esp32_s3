#ifndef VARIABLES_INFO_H
#define VARIABLES_INFO_H

#include <Arduino.h>
#include <vector>
#include <string>

struct ClientData
{
  std::string address;
  std::string name;
  bool enabled;
  bool connected;
  std::vector<int> gpioPins;
  float targetTemperature;
  float currentTemperature;
  float currentHumidity;
  unsigned long gpioOnTime;
};

// WiFi Credentials Structure
struct WifiCredentials
{
  std::string ssid;
  std::string password;
};

extern std::vector<ClientData> clients;

// Common list of available GPIOs
extern std::vector<int> availableGpio;

// WiFi Credentials (Global Variable)
extern WifiCredentials wifiCredentials;

// Buttons
const int BUTTON_SELECT = 0;
const int BUTTON_LEFT = 2;
const int BUTTON_UP = 4;
const int BUTTON_DOWN = 15;
const int BUTTON_RIGHT = 13;
const int BUTTON_RST = 12;

// BLE
#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMPERATURE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define HUMIDITY_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"
#define WIFI_SERVICE_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"            // New WiFi Service UUID
#define SSID_CHARACTERISTIC_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"     // New SSID and password Characteristic UUID
#define PASSWORD_CHARACTERISTIC_UUID "c5481513-22cb-4aae-9fe3-e9db5d06bf6f" // New Password Characteristic UUID

// Data
extern int selectedClientIndex;
extern bool bleScanning;

// File to store client configuration
extern String CLIENTS_FILE;
extern String WIFI_CREDENTIALS_FILE; // New file for WiFi credentials

// Temperature step constants
const float TEMPERATURE_STEP = 0.5;
const int MAX_GPIO = 39;
const int MIN_GPIO = 0;

// Edit Modes
enum EditMode
{
  EDIT_TEMPERATURE,
  EDIT_GPIO
};

// State Variables
extern EditMode currentEditMode;
extern bool isEditing;
extern int gpioSelectionIndex;
extern int currentGpioIndex;

// Scrolling text
extern String scrollText;
extern int scrollPosition;
extern unsigned long lastScrollTime;
const unsigned long SCROLL_DELAY = 500;
const unsigned long CONTROL_DELAY = 5000;

extern bool wifiConnected;
extern unsigned long lastWiFiAttemptTime;
const unsigned long WIFI_RECONNECT_DELAY = 10000; // 10 seconds

#endif