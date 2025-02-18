#include <Arduino.h>
#include <vector>
#include <string>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEDescriptor.h>
#include <BLEScan.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// WiFi Credentials Structure
struct WifiCredentials
{
  std::string ssid;
  std::string password;
};

// Client Data Structure (no change required)
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
// Global variable to track scanning state
extern bool bleScanning;
// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buttons
const int BUTTON_SELECT = 0;
const int BUTTON_LEFT = 2;
const int BUTTON_UP = 4;
const int BUTTON_DOWN = 15;
const int BUTTON_RIGHT = 13;
const int BUTTON_RST = 12;

// Web Server
AsyncWebServer server(80);

// BLE
#define SERVICE_UUID "Your_Service_UUID"
#define TEMPERATURE_UUID "Your_Temperature_UUID"
#define HUMIDITY_UUID "Your_Humidity_UUID"
#define WIFI_SERVICE_UUID "Your_WiFi_Service_UUID"                       // New WiFi Service UUID
#define SSID_CHARACTERISTIC_UUID "Your_SSID_Characteristic_UUID"         // New SSID Characteristic UUID
#define PASSWORD_CHARACTERISTIC_UUID "Your_Password_Characteristic_UUID" // New Password Characteristic UUID

// Data
std::vector<ClientData> clients;
int selectedClientIndex = 0;
bool bleScanning = false;

// File to store client configuration
const char *CLIENTS_FILE = "/clients.json";
const char *WIFI_CREDENTIALS_FILE = "/wifi_credentials.json"; // New file for WiFi credentials

// Temperature step constants
const float TEMPERATURE_STEP = 0.5;
const int MAX_GPIO = 39;
const int MIN_GPIO = 0;

// Common list of available GPIOs
std::vector<int> availableGpio = {25, 26, 27, 32, 33};

// Edit Modes
enum EditMode
{
  EDIT_TEMPERATURE,
  EDIT_GPIO
};

// State Variables
EditMode currentEditMode = EDIT_TEMPERATURE;
bool isEditing = false;
int gpioSelectionIndex = 0;
int currentGpioIndex = 0;

// Scrolling text
String scrollText = "";
int scrollPosition = 0;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_DELAY = 500;

// JsonDocument Memory Pool
const size_t JSON_DOCUMENT_SIZE = 4096;
char jsonBuffer[JSON_DOCUMENT_SIZE];

// WiFi Credentials (Global Variable)
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
const unsigned long WIFI_RECONNECT_DELAY = 10000; // 10 seconds

// Function Prototypes
void initLCD();
void initButtons();
void initSPIFFS();
void initBLE();
void initWebServer();
void loadClientsFromFile();
void saveClientsToFile();
void controlGPIO();
void updateLCD();
void handleButtons();
void updateScrollText();
void loadWifiCredentialsFromFile();
void saveWifiCredentialsToFile();
void handleWifiSetupBLE();
void connectWiFi();
void setup();
void loop();

void initLCD()
{
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");
}

void initButtons()
{
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_RST, INPUT_PULLUP);
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }
}

// BLE
class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if (pCharacteristic->getUUID().toString() == SSID_CHARACTERISTIC_UUID)
    {
      wifiCredentials.ssid = value;
      Serial.print("SSID received: ");
      Serial.println(wifiCredentials.ssid.c_str());
    }
    else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID)
    {
      wifiCredentials.password = value;
      Serial.print("Password received: ");
      Serial.println(wifiCredentials.password.c_str());
    }

    saveWifiCredentialsToFile(); // Save credentials to file
    connectWiFi();               // Attempt to connect to WiFi
  }
};

BLEServer *pServer = nullptr;

void handleWifiSetupBLE()
{
  BLEDevice::init("WiFiSetupServer");
  pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(WIFI_SERVICE_UUID);

  // SSID Characteristic
  BLECharacteristic *pSSIDCharacteristic = pService->createCharacteristic(
      SSID_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);
  pSSIDCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
  pSSIDCharacteristic->setValue(wifiCredentials.ssid); // Set initial value

  // Password Characteristic
  BLECharacteristic *pPasswordCharacteristic = pService->createCharacteristic(
      PASSWORD_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY);
  pPasswordCharacteristic->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
  pPasswordCharacteristic->setValue(wifiCredentials.password); // Set initial value

  pSSIDCharacteristic->setCallbacks(new MyCallbacks());
  pPasswordCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("WiFi Setup BLE Started");
}

// Definition of the MyAdvertisedDeviceCallbacks class
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID)))
    {
      Serial.print("Found our device!  address: ");
      Serial.println(advertisedDevice.getAddress().toString().c_str());

      // Check if client already exists
      bool found = false;
      for (auto &client : clients)
      {
        if (client.address == advertisedDevice.getAddress().toString())
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        ClientData newClient;
        newClient.address = advertisedDevice.getAddress().toString();
        // newClient.name = "Client_" + String(clients.size() + 1).c_str(); // Default name
        newClient.enabled = false;
        newClient.connected = false;
        newClient.targetTemperature = 20.0;
        newClient.currentTemperature = 0.0;
        newClient.currentHumidity = 0.0;
        newClient.gpioOnTime = 0;

        clients.push_back(newClient);
        saveClientsToFile(); // Save immediately
      }
    }
  }
};

// Function to start BLE scanning
void startBLEScan()
{
  if (bleScanning)
  {
    Serial.println("BLE scan already in progress!");
    return;
  }

  bleScanning = true; // Set the scanning flag

  Serial.println("Starting BLE scan...");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Set active scanning, this will increase discovery rate
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);

  BLEScanResults foundDevices = pBLEScan->start(5); // Scan for 5 seconds
  Serial.print("Scan done! Devices found: ");
  Serial.println(foundDevices.getCount());

  bleScanning = false; // Reset the scanning flag

  updateLCD(); // Update LCD after scanning
}

void initBLE()
{
  BLEDevice::init("Server");
}

// Connect to WiFi
void connectWiFi()
{
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiCredentials.ssid.c_str());

  WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
  lastWiFiAttemptTime = millis();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10)
  {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
  }
  else
  {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    wifiConnected = false;
    if (pServer != nullptr)
    {
      Serial.println("Restarting BLE Advertising...");
      pServer->startAdvertising();
    }
    else
    {
      Serial.println("BLE server not initialized");
    }
  }
}

void initWebServer()
{
  // GET /clients (get list of all clients)
  server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      const int capacity = JSON_ARRAY_SIZE(clients.size()) + clients.size() * JSON_OBJECT_SIZE(7);
      JsonDocument jsonDoc;//(jsonBuffer, JSON_DOCUMENT_SIZE); // Use the shared buffer
      JsonArray jsonClients = jsonDoc.to<JsonArray>();

      for (const auto& client : clients) {
          JsonObject jsonClient = jsonClients.createNestedObject();
          jsonClient["address"] = client.address.c_str();
          jsonClient["name"] = client.name.c_str();
          jsonClient["enabled"] = client.enabled;
          jsonClient["connected"] = client.connected;
          jsonClient["targetTemperature"] = client.targetTemperature;
          jsonClient["currentTemperature"] = client.currentTemperature;
          jsonClient["currentHumidity"] = client.currentHumidity;

          // You can add other fields as needed
      }

      String jsonString;
      serializeJson(jsonDoc, jsonString);
      request->send(200, "application/json", jsonString); });

  // GET /client/{address} (get info about a specific client)
  server.on("/client/{address}", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String address = request->pathArg(0); // Get address from URL

      // Find the client by address
      for (const auto& client : clients) {
          if (client.address == address.c_str()) {
              const int capacity = JSON_OBJECT_SIZE(7);
              JsonDocument jsonDoc;//(jsonBuffer, JSON_DOCUMENT_SIZE); // Use the shared buffer
              jsonDoc["address"] = client.address.c_str();
              jsonDoc["name"] = client.name.c_str();
              jsonDoc["enabled"] = client.enabled;
              jsonDoc["connected"] = client.connected;
              jsonDoc["targetTemperature"] = client.targetTemperature;
              jsonDoc["currentTemperature"] = client.currentTemperature;
              jsonDoc["currentHumidity"] = client.currentHumidity;

              String jsonString;
              serializeJson(jsonDoc, jsonString);
              request->send(200, "application/json", jsonString);
              return; // Client found, end processing
          }
      }

      // If client not found
      request->send(404, "text/plain", "Client not found"); });

  // POST /client/{address} (update info about a client)
  server.on("/client/{address}", HTTP_POST, [](AsyncWebServerRequest *request)
            {
      String address = request->pathArg(0);

      // Handle POST parameters
      if (request->hasParam("name", true)) {
          String newName = request->getParam("name", true)->value();
          // Find the client by address and update the name
          for (auto& client : clients) {
              if (client.address == address.c_str()) {
                  client.name = newName.c_str();
                  saveClientsToFile(); // Save changes to file
                  break;
              }
          }
      }

      if (request->hasParam("targetTemperature", true)) {
          String tempStr = request->getParam("targetTemperature", true)->value();
          float newTargetTemperature = tempStr.toFloat();
          // Find the client by address and update the target temperature
          for (auto& client : clients) {
              if (client.address == address.c_str()) {
                  client.targetTemperature = newTargetTemperature;
                  saveClientsToFile(); // Save changes to file
                  break;
              }
          }
      }

      // Send response
      request->send(200, "text/plain", "Client updated"); });

  // GET /scan (start BLE scan)
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      startBLEScan();
      request->send(200, "text/plain", "BLE Scan started"); });

  server.begin();
  Serial.println("Web server started");
}

// JSON
void loadClientsFromFile()
{
  Serial.println("Loading clients from file...");
  File file = SPIFFS.open(CLIENTS_FILE, "r");
  if (!file || file.size() == 0)
  {
    Serial.println("Clients file doesn't exist or is empty");
    file.close(); // Ensure file is closed before returning
    return;
  }

  JsonDocument doc; //(jsonBuffer, JSON_DOCUMENT_SIZE);
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    file.close();
    return;
  }

  clients.clear();
  JsonArray jsonClients = doc.as<JsonArray>();
  for (JsonObject jsonClient : jsonClients)
  {
    ClientData client;
    client.address = jsonClient["address"].as<String>().c_str();
    client.name = jsonClient["name"].as<String>().c_str();
    client.enabled = jsonClient["enabled"].as<bool>();
    client.connected = jsonClient["connected"].as<bool>();
    client.targetTemperature = jsonClient["targetTemperature"].as<float>();
    client.currentTemperature = jsonClient["currentTemperature"].as<float>();
    client.currentHumidity = jsonClient["currentHumidity"].as<float>();
    client.gpioOnTime = jsonClient["gpioOnTime"].as<unsigned long>();

    JsonArray gpioArray = jsonClient["gpioPins"].as<JsonArray>();
    for (JsonVariant value : gpioArray)
    {
      client.gpioPins.push_back(value.as<int>());
    }

    clients.push_back(client);
  }

  file.close();
  Serial.println("Clients loaded from file");
}

void saveClientsToFile()
{
  Serial.println("Saving clients to file...");
  File file = SPIFFS.open(CLIENTS_FILE, "w");
  if (!file)
  {
    Serial.println("Failed to open clients file for writing");
    return;
  }

  JsonDocument doc; //(jsonBuffer, JSON_DOCUMENT_SIZE);
  JsonArray jsonClients = doc.to<JsonArray>();

  for (const auto &client : clients)
  {
    JsonObject jsonClient = jsonClients.createNestedObject();
    jsonClient["address"] = client.address;
    jsonClient["name"] = client.name;
    jsonClient["enabled"] = client.enabled;
    jsonClient["connected"] = client.connected;
    jsonClient["targetTemperature"] = client.targetTemperature;
    jsonClient["currentTemperature"] = client.currentTemperature;
    jsonClient["currentHumidity"] = client.currentHumidity;
    jsonClient["gpioOnTime"] = client.gpioOnTime;

    JsonArray gpioArray = jsonClient.createNestedArray("gpioPins");
    for (JsonVariant value : gpioArray)
    {
      gpioArray.add(value.as<int>());
    }
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write to clients file");
  }

  file.close();
  Serial.println("Clients saved to file");
}

void loadWifiCredentialsFromFile()
{
  Serial.println("Loading WiFi credentials from file...");
  File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "r");
  if (!file || file.size() == 0)
  {
    Serial.println("WiFi credentials file doesn't exist or is empty. Using default configuration.");
    wifiCredentials.ssid = "";
    wifiCredentials.password = "";
    file.close(); // Ensure file is closed
    return;
  }

  JsonDocument doc; //(jsonBuffer, JSON_DOCUMENT_SIZE);
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    Serial.print(F("deserializeJson (WiFi): "));
    Serial.println(error.c_str());
    file.close();
    return;
  }

  wifiCredentials.ssid = doc["ssid"].as<String>().c_str();
  wifiCredentials.password = doc["password"].as<String>().c_str();

  file.close();
  Serial.println("WiFi credentials loaded from file");
}

void saveWifiCredentialsToFile()
{
  Serial.println("Saving WiFi credentials to file...");
  File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "w");
  if (!file)
  {
    Serial.println("Failed to open WiFi credentials file for writing");
    return;
  }

  JsonDocument doc; //(jsonBuffer, JSON_DOCUMENT_SIZE);
  doc["ssid"] = wifiCredentials.ssid;
  doc["password"] = wifiCredentials.password;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write WiFi credentials to file");
  }

  file.close();
  Serial.println("WiFi credentials saved to file");
}

// GPIO Control
void controlGPIO()
{
  std::vector<int> gpiosToTurnOn;

  // 1. Collect GPIOs to turn on
  for (const auto &client : clients)
  {
    if (client.enabled && client.connected && client.currentTemperature < client.targetTemperature)
    {
      gpiosToTurnOn.insert(gpiosToTurnOn.end(), client.gpioPins.begin(), client.gpioPins.end());
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

  // 4. Update GPIO on-time
  for (auto &client : clients)
  {
    if (client.enabled && client.connected)
    {
      for (int gpio : client.gpioPins)
      {
        if (digitalRead(gpio) == HIGH)
        {
          client.gpioOnTime++;
        }
      }
    }
  }
}

// LCD Output
void updateLCD()
{
  lcd.clear();

  if (clients.empty())
  {
    lcd.setCursor(0, 0);
    lcd.print("No clients found");
    return;
  }

  const ClientData &currentClient = clients[selectedClientIndex];

  // Отображаем имя клиента в первой строке
  lcd.setCursor(0, 0);
  lcd.print(currentClient.name.c_str());
  if (isEditing)
  {
    lcd.print("*"); // Индикация общего режима редактирования
  }

  lcd.setCursor(0, 1);
  if (isEditing)
  {
    // Режим редактирования: Отображаем GPIO или температуру
    if (currentEditMode == EDIT_GPIO)
    {
      lcd.print("GPIO: ");
      lcd.print(availableGpio[currentGpioIndex]);
      lcd.setCursor(8, 1); // Второй столбец второй строки
      // Проверяем добавлен ли GPIO в список для текущего клиента
      bool found = false;
      for (int gpio : currentClient.gpioPins)
      {
        if (gpio == availableGpio[currentGpioIndex])
        {
          found = true;
          break;
        }
      }
      if (found)
      {
        lcd.print("IN");
      }
      else
      {
        lcd.print("OUT");
      }
    }
    else
    {
      // Режим EDIT_TEMPERATURE
      lcd.print("Temp: ");
      lcd.print(currentClient.currentTemperature);
      lcd.print("C");

      lcd.setCursor(8, 1);
      lcd.print("Tgt: ");
      lcd.print(currentClient.targetTemperature);
      lcd.print("C");
    }
  }
  else
  {
    // Режим бегущей строки
    String displayText = scrollText.substring(scrollPosition);
    if (displayText.length() < 16)
    {
      displayText += scrollText.substring(0, 16 - displayText.length());
    }
    displayText = displayText.substring(0, 16);
    lcd.print(displayText);
  }
  if (wifiConnected)
  {
    lcd.setCursor(15, 0); // Rightmost position of the first line
    lcd.print("C");       // "C" for connected
  }
  else
  {
    lcd.setCursor(15, 0);
    lcd.print("D"); // "D" for disconnected
  }
}

// Button Handling
void handleButtons()
{
  // SELECT: Переключение между режимами (Температура <-> GPIO) или включение/выключение редактирования
  if (digitalRead(BUTTON_SELECT) == LOW)
  {
    if (!isEditing)
    {
      // Переключение между режимами редактирования (Температура/GPIO)
      currentEditMode = (currentEditMode == EDIT_TEMPERATURE) ? EDIT_GPIO : EDIT_TEMPERATURE;
    }
    else
    {
      // Выход из режима редактирования
      isEditing = false;
      saveClientsToFile();
    }
    updateLCD();
    delay(200);
  }

  // Двойное условие
  if (digitalRead(BUTTON_SELECT) == LOW && !isEditing)
  {
    isEditing = true;
    currentGpioIndex = 0; // сбрасываем текущий gpio для выбора
    updateLCD();
    delay(200);
  }

  if (isEditing)
  {
    if (currentEditMode == EDIT_TEMPERATURE)
    {
      // Редактирование температуры
      if (digitalRead(BUTTON_UP) == LOW)
      {
        clients[selectedClientIndex].targetTemperature += TEMPERATURE_STEP;
        updateLCD();
        delay(200);
      }
      if (digitalRead(BUTTON_DOWN) == LOW)
      {
        clients[selectedClientIndex].targetTemperature -= TEMPERATURE_STEP;
        updateLCD();
        delay(200);
      }
    }
    else if (currentEditMode == EDIT_GPIO)
    {
      // Редактирование GPIO

      // Сдвиг индекса влево
      if (digitalRead(BUTTON_LEFT) == LOW)
      {
        if (currentGpioIndex > 0)
        {
          currentGpioIndex--;
          updateLCD();
          delay(200);
        }
      }

      // Сдвиг индекса вправо
      if (digitalRead(BUTTON_RIGHT) == LOW)
      {
        if (currentGpioIndex < availableGpio.size() - 1)
        {
          currentGpioIndex++;
          updateLCD();
          delay(200);
        }
      }

      // UP: Добавить GPIO в список для клиента (если его там нет)
      if (digitalRead(BUTTON_UP) == LOW)
      {
        int selectedGpio = availableGpio[currentGpioIndex];
        bool found = false;
        for (int gpio : clients[selectedClientIndex].gpioPins)
        {
          if (gpio == selectedGpio)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          clients[selectedClientIndex].gpioPins.push_back(selectedGpio);
          sort(clients[selectedClientIndex].gpioPins.begin(), clients[selectedClientIndex].gpioPins.end()); // Сортируем
          updateLCD();
          delay(200);
        }
      }

      // DOWN: Удалить GPIO из списка для клиента (если он там есть)
      if (digitalRead(BUTTON_DOWN) == LOW)
      {
        int selectedGpio = availableGpio[currentGpioIndex];
        for (size_t i = 0; i < clients[selectedClientIndex].gpioPins.size(); ++i)
        {
          if (clients[selectedClientIndex].gpioPins[i] == selectedGpio)
          {
            clients[selectedClientIndex].gpioPins.erase(clients[selectedClientIndex].gpioPins.begin() + i);
            updateLCD();
            delay(200);
            break;
          }
        }
      }
    }
  }
  else
  {
    // Переключение клиентов
    if (digitalRead(BUTTON_SELECT) == LOW)
    {
      selectedClientIndex = (selectedClientIndex + 1) % clients.size();
      updateScrollText(); // Обновляем текст бегущей строки при смене клиента
      updateLCD();
      delay(200);
    }
  }

  // Rst: reset esp32
  if (digitalRead(BUTTON_RST) == LOW)
  {
    ESP.restart();
  }
}

// Scrolling Text
void updateScrollText()
{
  if (clients.empty())
  {
    scrollText = "No clients found";
  }
  else
  {
    const ClientData &currentClient = clients[selectedClientIndex];
    // scrollText = currentClient.name + " Temp:" + String(currentClient.currentTemperature) + "C Tgt:" + String(currentClient.targetTemperature) + "C";
  }
  scrollPosition = 0;
}

// Setup and Loop
void setup()
{
  Serial.begin(115200);
  initLCD();
  initButtons();
  initSPIFFS();
  loadWifiCredentialsFromFile(); // Load WiFi credentials first
  connectWiFi();                 // Connect to WiFi
  initBLE();
  handleWifiSetupBLE(); // Start BLE for WiFi setup
  loadClientsFromFile();
  initWebServer(); // Initialize Web Server if needed
  updateScrollText();
  updateLCD();
  Serial.println("Setup complete");
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
  if (millis() - lastGpioControlTime > 5000)
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