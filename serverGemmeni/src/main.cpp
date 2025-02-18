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
#include <convert_to_json.h>

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
#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMPERATURE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define HUMIDITY_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"
#define WIFI_SERVICE_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"            // New WiFi Service UUID
#define SSID_CHARACTERISTIC_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"     // New SSID and password Characteristic UUID
#define PASSWORD_CHARACTERISTIC_UUID "c5481513-22cb-4aae-9fe3-e9db5d06bf6f" // New Password Characteristic UUID

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
const unsigned long CONTROL_DELAY = 5000;

// WiFi Credentials (Global Variable)
WifiCredentials wifiCredentials;
bool wifiConnected = false;
unsigned long lastWiFiAttemptTime = 0;
const unsigned long WIFI_RECONNECT_DELAY = 10000; // 10 seconds

// Function Prototypes
void initLCD();
void initButtons();
// void initSPIFFS();
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

// void initSPIFFS()
// {
//   if (!SPIFFS.begin(true))
//   {
//     Serial.println("An Error has occurred while mounting SPIFFS");
//   }
// }

// BLE +++++++++++++++++++++++++++++++++++
class SetServerSettingCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if (pCharacteristic->getUUID().toString() == SSID_CHARACTERISTIC_UUID)
    {
      wifiCredentials.ssid = value;
      Serial.print(F("SSID received: "));
      Serial.println(F(wifiCredentials.ssid.c_str()));
    }
    else if (pCharacteristic->getUUID().toString() == PASSWORD_CHARACTERISTIC_UUID)
    {
      wifiCredentials.password = value;
      Serial.print(F("Password received: "));
      Serial.println(F(wifiCredentials.password.c_str()));
    }

    saveWifiCredentialsToFile(); // Save credentials to file
    connectWiFi();               // Attempt to connect to WiFi
  }
};

BLEServer *pServer = nullptr;

static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  for (auto &client : clients)
  {
    float val = *((float *)(pData)); // Температура
    if (client.address == pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString())
    {
      client.currentHumidity = val;
      break;
    }
  }
}

static void humidityNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  for (auto &client : clients)
  {
    float val = *((float *)(pData)); // Влажность
    if (client.address == pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString())
    {
      client.currentHumidity = val;
      break;
    }
  }
}

// характеристики для установки SSID по BLE ++++++++++++++++++++++++++
void handleWifiSetupBLE()
{
  BLEDevice::init(SERVER_NAME);
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

  pSSIDCharacteristic->setCallbacks(new SetServerSettingCallbacks());
  pPasswordCharacteristic->setCallbacks(new SetServerSettingCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(WIFI_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println(F("WiFi Setup BLE Started"));
}

// Definition of the ClientAdvertisedDeviceCallbacks class +++++++++++++++++++++++++++++++++++++
class ClientAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID)))
    {
      Serial.print(F("Found our device!  address: "));
      Serial.println(F(advertisedDevice.getAddress().toString().c_str()));

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
        newClient.name = ("Client_%d", (clients.size() + 1)); // Default name
        newClient.enabled = false;
        newClient.connected = false;
        newClient.targetTemperature = 20.0;
        newClient.currentTemperature = 0.0;
        newClient.currentHumidity = 0.0;
        newClient.gpioOnTime = 0;

        BLEClient *pClient = BLEDevice::createClient();
        Serial.println(F("Connecting to device"));
        if (pClient->connect(&advertisedDevice))
        {
          Serial.println(F("Device connected, subscribe service"));
          // Подписка на обновления характеристик
          BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));

          if (pRemoteService != nullptr)
          {
            newClient.connected = true;

            BLERemoteCharacteristic *pRemoteTempCharacteristic = pRemoteService->getCharacteristic(BLEUUID(TEMPERATURE_UUID));
            if (pRemoteTempCharacteristic != nullptr)
            {
              pRemoteTempCharacteristic->registerForNotify(temperatureNotifyCallback);
            }
            else
            {
              Serial.println(F("Характеристика температуры не найдена"));
              return;
            }
            BLERemoteCharacteristic *pRemoteHumCharacteristic = pRemoteService->getCharacteristic(BLEUUID(HUMIDITY_UUID));
            if (pRemoteHumCharacteristic != nullptr)
            {
              pRemoteHumCharacteristic->registerForNotify(humidityNotifyCallback);
            }
            else
            {
              Serial.println(F("Характеристика влажности не найдена"));
              return;
            }

            clients.push_back(newClient);
            saveClientsToFile(); // Save immediately
          }
        }
        else
        {
          Serial.printf("Не удалось подключиться к клиенту %s\n", newClient.name.c_str());
        }
      }
    }
  }
};

// Function to start BLE scanning
void startBLEScan()
{
  if (bleScanning)
  {
    Serial.println(F("BLE scan already in progress!"));
    return;
  }

  bleScanning = true; // Set the scanning flag

  Serial.println(F("Starting BLE scan..."));
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ClientAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Set active scanning, this will increase discovery rate
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);

  BLEScanResults foundDevices = pBLEScan->start(5); // Scan for 5 seconds
  Serial.print(F("Scan done! Devices found: "));
  Serial.println(F(foundDevices.getCount()));

  bleScanning = false; // Reset the scanning flag

  updateLCD(); // Update LCD after scanning
}

// Connect to WiFi +++++++++++++++++++++++++++++++++++
void connectWiFi()
{
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(F(wifiCredentials.ssid.c_str()));

  WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
  lastWiFiAttemptTime = millis();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10)
  {
    delay(1000);
    Serial.print(F("..."));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    wifiConnected = true;
  }
  else
  {
    Serial.println(F(""));
    Serial.println(F("Failed to connect to WiFi"));
    wifiConnected = false;
    if (pServer != nullptr)
    {
      Serial.println(F("Restarting BLE Advertising..."));
      pServer->startAdvertising();
    }
    else
    {
      Serial.println(F("BLE server not initialized"));
    }
  }
}
// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{
  // GET /clients (get list of all clients)
  server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
            {
     std::string jsonVal="[";

     for (size_t i = 0; i < clients.size(); ++i) {
      jsonVal += toJson(clients[i]);
      if (i != clients.size() - 1) {
        jsonVal += ",";
      }
     }
      jsonVal +="]";      
      request->send(200, "application/json", jsonVal.c_str()); });

  // POST /client/{address} (update info about a client)
  server.on("/client/{address}", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("address", true))
              {
                String address = request->getParam("address", true)->value();
                bool isSaving = false;
                // Handle POST parameters
                if (request->hasParam("name", true))
                {
                  String newName = request->getParam("name", true)->value();
                  // Find the client by address and update the name
                  for (auto &client : clients)
                  {
                    if (client.address == address.c_str())
                    {
                      client.name = newName.c_str();
                      isSaving = true;
                      break;
                    }
                  }
                }

                if (request->hasParam("targetTemperature", true))
                {
                  String tempStr = request->getParam("targetTemperature", true)->value();
                  float newTargetTemperature = tempStr.toFloat();
                  // Find the client by address and update the target temperature
                  for (auto &client : clients)
                  {
                    if (client.address == address.c_str())
                    {
                      client.targetTemperature = newTargetTemperature;
                      isSaving = true;
                      break;
                    }
                  }
                }

                if (request->hasParam("enabled", true))
                {
                  String tempStr = request->getParam("enabled", true)->value();
                  bool enabled = tempStr == "true";
                  // Find the client by address and update the target temperature
                  for (auto &client : clients)
                  {
                    if (client.address == address.c_str())
                    {
                      client.enabled = enabled;
                      isSaving = true;
                      break;
                    }
                  }
                }

                if (request->hasParam("gpioPins", true))
                {
                  String newName = request->getParam("gpioPins", true)->value();
                  // Find the client by address and update the name
                  for (auto &client : clients)
                  {
                    if (client.address == address.c_str())
                    {
                      client.gpioPins = parseGpioPins(newName.c_str());
                      isSaving = true;
                      break;
                    }
                  }
                }
                if (isSaving)
                {
                  saveClientsToFile(); // Save changes to file
                  // Send response
                request->send(200, "text/plain", "Client updated");
                }
                
              }
              request->send(200, "text/plain", "Client not find"); });

  // GET /scan (start BLE scan)
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      startBLEScan();
      request->send(200, "text/plain", "BLE Scan started"); });

  server.begin();
  Serial.println(F("Web server started"));
}

// Загружаем клиентов из JSON ++++++++++++++++++++++++++++++
void loadClientsFromFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Loading clients from file..."));
    File file = SPIFFS.open(CLIENTS_FILE, "r");
    if (!file || file.size() == 0)
    {
      Serial.println(F("Clients file doesn't exist or is empty"));
      file.close(); // Ensure file is closed before returning
      return;
    }
    clients.clear();
    while (file.available())
    {
      std::string json;
      while (file.available())
      {
        char c = file.read();
        json += c;
        if (c == '}')
          break;
      }
      clients.push_back(fromJson(json));
    }
    file.close();
    Serial.println(F("Clients loaded from file"));
  }
}
// Сохраняме клиентов в JSON +++++++++++++++++++++++++++++
void saveClientsToFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Saving clients to file..."));
    File file = SPIFFS.open(CLIENTS_FILE, "w");
    if (!file)
    {
      Serial.println(F("Failed to open clients file for writing"));
      return;
    }

    file.print("[\n");
    for (size_t i = 0; i < clients.size(); ++i)
    {
      file.print(toJson(clients[i]).c_str());
      if (i != clients.size() - 1)
      {
        file.print(",\n");
      }
    }
    file.print("\n]");
    file.close();
    Serial.println(F("Clients saved to file"));
  }
}
// Загружаем настройки Wifi +++++++++++++++++++++++++++
void loadWifiCredentialsFromFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Loading WiFi credentials from file..."));
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "r");
    if (!file || file.size() == 0)
    {
      Serial.println(F("WiFi credentials file doesn't exist or is empty. Using default configuration."));
      wifiCredentials.ssid = "";
      wifiCredentials.password = "";
      file.close(); // Ensure file is closed
      return;
    }

    wifiCredentials = fromJsonWifi(file.readString().c_str());

    file.close();
    Serial.println(F("WiFi credentials loaded from file"));
  }
}
// Сохраняем настройки Wifi +++++++++++++++++++++++++++
void saveWifiCredentialsToFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Saving WiFi credentials to file..."));
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "w");
    if (!file)
    {
      Serial.println(F("Failed to open WiFi credentials file for writing"));
      return;
    }
    file.print(toJsonWifi(wifiCredentials).c_str());

    file.close();
    Serial.println(F("WiFi credentials saved to file"));
  }
}

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
  // initSPIFFS();
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