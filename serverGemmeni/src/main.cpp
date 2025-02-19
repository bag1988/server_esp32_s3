#include <vector>
#include <string>
#include <WiFi.h>
#include <web_server_setting.h>
#include <variables_info.h>
#include <lcd_setting.h>
#include <ble_setting.h>
#include <spiffs_setting.h>

// Data
std::vector<ClientData> clients;
int selectedClientIndex = 0;
bool bleScanning = false;

// Common list of available GPIOs
std::vector<int> availableGpio = {25, 26, 27, 32, 33};

// State Variables
EditMode currentEditMode = EDIT_TEMPERATURE;
bool isEditing = false;
int gpioSelectionIndex = 0;
int currentGpioIndex = 0;

// Scrolling text
String scrollText = "";
int scrollPosition = 0;
unsigned long lastScrollTime = 0;

// WiFi Credentials (Global Variable)
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;

String CLIENTS_FILE = "/clients.json";
String WIFI_CREDENTIALS_FILE = "/wifi_credentials.json"; // New file for WiFi credentials

// GPIO Control +++++++++++++++++++++++++++++
void controlGPIO()
{
  std::vector<int> gpiosToTurnOn;

  // 1. Collect GPIOs to turn on
  for (auto &client : clients)
  {
    if (client.enabled && client.connected && (client.currentTemperature + 2) < client.targetTemperature)
    {
      gpiosToTurnOn.insert(gpiosToTurnOn.end(), client.gpioPins.begin(), client.gpioPins.end());
      client.gpioOnTime += CONTROL_DELAY / 1000;
    }
  }

  // 2. Remove duplicate GPIOs
  std::sort(gpiosToTurnOn.begin(), gpiosToTurnOn.end());
  gpiosToTurnOn.erase(std::unique(gpiosToTurnOn.begin(), gpiosToTurnOn.end()), gpiosToTurnOn.end());

  // 3. Turn off all GPIOs not in the list (only from availableGpio)
  for (int gpio : availableGpio)
  {
    bool shouldTurnOn = false;
    for (int gpioOn : gpiosToTurnOn)
    {
      if (gpio == gpioOn)
      {
        shouldTurnOn = true;
        break;
      }
    }
    digitalWrite(gpio, shouldTurnOn ? HIGH : LOW);
  }
}

// Setup and Loop
void setup()
{
  Serial.begin(115200);
  initLCD();
  initButtons();
  loadWifiCredentialsFromFile(); // Load WiFi credentials first
  connectWiFi();                 // Connect to WiFi

  handleWifiSetupBLE(); // Start BLE for WiFi setup
  loadClientsFromFile();
  initWebServer(); // Initialize Web Server if needed
  updateScrollText();
  updateLCD();
  Serial.println(F("Setup complete"));
}

void loop()
{
  handleButtons();

  if (!isEditing)
  {
    if (millis() - lastScrollTime > SCROLL_DELAY)
    {
      scrollPosition++;
      if (scrollPosition > scrollText.length())
      {
        scrollPosition = 0;
      }
      updateLCD();
      lastScrollTime = millis();
    }
  }

  static unsigned long lastGpioControlTime = 0;
  if (millis() - lastGpioControlTime > CONTROL_DELAY)
  {
    controlGPIO();
    lastGpioControlTime = millis();
  }
  // WiFi Reconnect Check
  if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
  {
    connectWiFi(); // Attempt to reconnect to WiFi every WIFI_RECONNECT_DELAY milliseconds
  }
  updateLCD(); // Update LCD to show WiFi status
}