#include <vector>
#include <string>
#include <web_server_setting.h>
#include <variables_info.h>
#include <lcd_setting.h>
#include <ble_setting.h>
#include <spiffs_setting.h>
#include <xiaomi_ble_scanner.h>
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

// Интервал сканирования BLE (в миллисекундах)
#define BLE_SCAN_INTERVAL 30000 // 30 секунд
#define BLE_SCAN_DURATION 5     // 5 секунд

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

  // Проверяем все датчики Xiaomi
  for (auto &pair : xiaomiSensors)
  {
    XiaomiSensorData &sensor = pair.second;

    // Проверяем, онлайн ли датчик и актуальны ли данные
    if (sensor.isOnline && (millis() - sensor.lastUpdate < 300000))
    {
      // Находим соответствующего клиента по MAC-адресу
      for (auto &client : clients)
      {
        if (client.address.c_str() == sensor.address.c_str() && client.enabled)
        {
          // Проверяем, нужно ли включать обогрев
          if ((sensor.temperature + 2) < client.targetTemperature)
          {
            gpiosToTurnOn.insert(gpiosToTurnOn.end(), client.gpioPins.begin(), client.gpioPins.end());
            client.gpioOnTime += CONTROL_DELAY / 1000;

            // Обновляем текущую температуру клиента
            client.currentTemperature = sensor.temperature;
          }
        }
      }
    }
  }

  // Удаляем дубликаты GPIO
  std::sort(gpiosToTurnOn.begin(), gpiosToTurnOn.end());
  gpiosToTurnOn.erase(std::unique(gpiosToTurnOn.begin(), gpiosToTurnOn.end()), gpiosToTurnOn.end());

  // Управляем GPIO
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

  // Инициализация GPIO
  for (int gpio : availableGpio)
  {
    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, LOW);
  }

  loadWifiCredentialsFromFile(); // Load WiFi credentials first
  connectWiFi();                 // Connect to WiFi

  // Инициализация сканера Xiaomi
  setupXiaomiScanner();

  handleWifiSetupBLE(); // Start BLE for WiFi setup
  loadClientsFromFile();
  initWebServer(); // Initialize Web Server if needed
  updateScrollText();
  updateLCD();
  Serial.println(F("Настройка завершена"));
}
// Обновление статуса датчиков
void updateSensorsStatus() {
  unsigned long currentTime = millis();
  
  for (auto& pair : xiaomiSensors) {
      XiaomiSensorData& sensor = pair.second;
      
      // Проверяем, не устарели ли данные (более 5 минут)
      if (currentTime - sensor.lastUpdate > 300000) {
          if (sensor.isOnline) {
              sensor.isOnline = false;
              Serial.print("Датчик ");
              Serial.print(sensor.name);
              Serial.println(" перешел в оффлайн (нет данных более 5 минут)");
          }
      }
  }
  
  // Обновляем статус подключения клиентов
  for (auto& client : clients) {
      bool wasConnected = client.connected;
      client.connected = false;
      
      // Проверяем, есть ли соответствующий онлайн-датчик
      for (auto& pair : xiaomiSensors) {
          XiaomiSensorData& sensor = pair.second;
          if (sensor.address == String(client.address.c_str()) && sensor.isOnline) {
              client.connected = true;
              client.currentTemperature = sensor.temperature;
              break;
          }
      }
      
      // Если статус изменился, выводим сообщение
      if (wasConnected != client.connected) {
          Serial.print("Клиент ");
          Serial.print(client.name.c_str());
          Serial.print(client.connected ? " подключен" : " отключен");
          Serial.println();
      }
  }
  
  // Обновляем текст прокрутки с новыми данными
  updateScrollText();
}
void loop()
{
  handleButtons();
  // Обновление прокрутки текста
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
  // Управление GPIO
  static unsigned long lastGpioControlTime = 0;
  if (millis() - lastGpioControlTime > CONTROL_DELAY)
  {
    controlGPIO();
    lastGpioControlTime = millis();
  }

  // Периодическое сканирование BLE
  static unsigned long lastScanTime = 0;
  if (millis() - lastScanTime > BLE_SCAN_INTERVAL)
  {
    startXiaomiScan(BLE_SCAN_DURATION);
    updateSensorsStatus();
    lastScanTime = millis();
  }

  // Проверка подключения WiFi
  if (!wifiConnected && (millis() - lastWiFiAttemptTime > WIFI_RECONNECT_DELAY))
  {
    connectWiFi();
  }
  updateLCD();
}