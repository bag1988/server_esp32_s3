#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <set> // для хранения уникальных значений GPIO

#define NAMESPACE_FLASH "ble_device"
#define KEY_NAME_FLASH "ble_devices"
// Имя сервера BLE(другое имя ESP32, на котором выполняется эскиз сервера)
#define bleServerName "DHT11_ESP32C3"

#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"

#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"

#define SERVER_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"
#define SERVER_SERVICE_WIFI_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"
#define SERVER_SERVICE_IP_UUID "b882babc-6942-494c-a47b-497b4caa86d4"

// Структура данных для сохранения информации о BLE подключениях
struct DeviceData
{
  String ble_address;
  String name;
  bool enabled;
  bool isConnected;
  int gpioToEnable[5];
  bool pump;
  bool boiler;
  float targetTemp;
  float temp;
  float humidity;
  int tempReductionTime;
};

// Глобальные переменные
Preferences preferences;
DeviceData deviceData[10];
int gpioPins[] = {12, 13, 14, 15, 16}; // Глобальный массив GPIO
const int boilerPin = 17;              // GPIO котла
const int pumpPin = 18;                // GPIO насоса

AsyncWebServer server(80);
String ssid = "";
String password = "";

// Основная функциональность BLE
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristicSetWifi = NULL;
BLECharacteristic *pCharacteristicIp = NULL;
bool deviceConnected = false;

// Инициализация данных
void loadDeviceData()
{
  preferences.begin("device-data", false);
  for (int i = 0; i < 10; i++)
  {
    deviceData[i].ble_address = preferences.getString(("ble_address" + String(i)).c_str(), "");
    deviceData[i].name = preferences.getString(("name" + String(i)).c_str(), "");
    deviceData[i].isConnected = preferences.getBool(("enabled" + String(i)).c_str(), false);
    deviceData[i].isConnected = preferences.getBool(("isConnected" + String(i)).c_str(), false);
    for (int j = 0; j < 5; j++)
    {
      deviceData[i].gpioToEnable[j] = preferences.getInt(("gpio" + String(i) + "_" + String(j)).c_str(), -1);
    }
    deviceData[i].pump = preferences.getBool(("pump" + String(i)).c_str(), false);
    deviceData[i].boiler = preferences.getBool(("boiler" + String(i)).c_str(), false);
    deviceData[i].targetTemp = preferences.getFloat(("targetTemp" + String(i)).c_str(), 0.0);
    deviceData[i].temp = preferences.getFloat(("temp" + String(i)).c_str(), 0.0);
    deviceData[i].humidity = preferences.getFloat(("humidity" + String(i)).c_str(), 0.0);
    deviceData[i].tempReductionTime = preferences.getInt(("tempReductionTime" + String(i)).c_str(), 0);
  }
  preferences.end();
}

// Сохранение данных
void saveDeviceData(int index)
{
  preferences.begin("device-data", false);
  preferences.putString(("ble_address" + String(index)).c_str(), deviceData[index].ble_address.c_str());
  preferences.putString(("name" + String(index)).c_str(), deviceData[index].name.c_str());
  preferences.putBool(("enabled" + String(index)).c_str(), deviceData[index].enabled);
  preferences.putBool(("isConnected" + String(index)).c_str(), deviceData[index].isConnected);
  for (int j = 0; j < 5; j++)
  {
    preferences.putInt(("gpio" + String(index) + "_" + String(j)).c_str(), deviceData[index].gpioToEnable[j]);
  }
  preferences.putBool(("pump" + String(index)).c_str(), deviceData[index].pump);
  preferences.putBool(("boiler" + String(index)).c_str(), deviceData[index].boiler);
  preferences.putFloat(("targetTemp" + String(index)).c_str(), deviceData[index].targetTemp);
  preferences.putFloat(("temp" + String(index)).c_str(), deviceData[index].temp);
  preferences.putFloat(("humidity" + String(index)).c_str(), deviceData[index].humidity);
  preferences.putInt(("tempReductionTime" + String(index)).c_str(), deviceData[index].tempReductionTime);
  preferences.end();
}

class ServerConnectedClientCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                                      uint8_t *pData, size_t length, bool isNotify)
{
  std::string value = (char *)pData;
  std::string ble_address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  // Логика обработки новых данных и обновления deviceData
  // Парсинг значения и обновление данных устройства
  // Например, value может содержать JSON, который нужно распарсить
  // Здесь простой пример обновления targetTemp
  for (int i = 0; i < 10; i++)
  {
    if (deviceData[i].ble_address.c_str() == ble_address.c_str())
    { // Поиск устройства по блютуз адресу
      deviceData[i].temp = std::stof(value);
      saveDeviceData(i);
      break;
    }
  }
}

static void humidityNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                                      uint8_t *pData, size_t length, bool isNotify)
{
  std::string value = (char *)pData;
  std::string ble_address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  // Логика обработки новых данных и обновления deviceData
  // Парсинг значения и обновление данных устройства
  // Например, value может содержать JSON, который нужно распарсить
  // Здесь простой пример обновления targetTemp
  for (int i = 0; i < 10; i++)
  {
    if (deviceData[i].ble_address.c_str() == ble_address.c_str())
    { // Поиск устройства по блютуз адресу
      deviceData[i].humidity = std::stof(value);
      saveDeviceData(i);
      break;
    }
  }
}

class SetServerSettingCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    // Пример обработки данных в формате "SSID,PASSWORD"
    size_t delimiterPos = value.find(",");
    if (delimiterPos != std::string::npos)
    {
      ssid = value.substr(0, delimiterPos).c_str();
      password = value.substr(delimiterPos + 1).c_str();

      // Сохранение данных WiFi в память
      preferences.begin("wifi-creds", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();

      // Подключение к WiFi с новыми данными
      WiFi.begin(ssid.c_str(), password.c_str());

      // Ожидание подключения
      while (WiFi.status() != WL_CONNECTED)
      {
        delay(1000);
        Serial.println("Connecting to WiFi...");
      }

      Serial.println("Connected to WiFi.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      pCharacteristicIp->setValue(WiFi.localIP().toString().c_str());
      pCharacteristicIp->notify();
    }
  }
};

// Определение нужных характеристик
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    // Логика поиска нужных характеристик
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID)))
    {

      // Поиск в списке сохранненых

      //

      BLEDevice::getScan()->stop();
      BLEClient *pClient = BLEDevice::createClient();
      pClient->connect(&advertisedDevice);
      // Подписка на обновления характеристик
      BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
      if (pRemoteService != nullptr)
      {
        BLERemoteCharacteristic *pRemoteTempCharacteristic = pRemoteService->getCharacteristic(BLEUUID(TEMP_CHARACT_SERVICE_UUID));
        if (pRemoteTempCharacteristic != nullptr)
        {
          pRemoteTempCharacteristic->registerForNotify(temperatureNotifyCallback);
        }
        BLERemoteCharacteristic *pRemoteHumCharacteristic = pRemoteService->getCharacteristic(BLEUUID(NUM_CHARACT_SERVICE_UUID));
        if (pRemoteHumCharacteristic != nullptr)
        {
          pRemoteHumCharacteristic->registerForNotify(humidityNotifyCallback);
        }
      }
    }
  }
};

void setupBLE()
{
  BLEDevice::init(SERVER_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerConnectedClientCallbacks());

  BLEService *pService = pServer->createService(SERVER_UUID);
  pCharacteristicSetWifi = pService->createCharacteristic(
      SERVER_SERVICE_WIFI_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicSetWifi->setCallbacks(new SetServerSettingCallbacks());
  pCharacteristicIp = pService->createCharacteristic(
      SERVER_SERVICE_IP_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pService->start();
  BLEDevice::startAdvertising();
}

// Поиск BLE устройств и подписка на обновления
void scanForBLEDevices()
{
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

// Web server для управления устройствами
void setupWebServer()
{
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = "[";
    for (int i = 0; i < 10; i++) {
      //if (!deviceData[i].ble_address.empty()) {
        json += "{";
        json += "\"ble_address\":\"" + String(deviceData[i].ble_address.c_str()) + "\",";
        json += "\"name\":\"" + String(deviceData[i].name.c_str()) + "\",";
        json += "\"targetTemp\":" + String(deviceData[i].targetTemp);
        json += "},";
    //  }
    }
    json.remove(json.length() - 1);
    json += "]";
    request->send(200, "application/json", json); });

  server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("ble_address") && request->hasArg("targetTemp") && request->hasArg("name")) {
      String ble_address = request->arg("ble_address");
      float targetTemp = request->arg("targetTemp").toFloat();
      String name = request->arg("name");
      for (int i = 0; i < 10; i++) {
        if (deviceData[i].ble_address == ble_address.c_str()) {
          deviceData[i].targetTemp = targetTemp;
          deviceData[i].name = name.c_str();
          saveDeviceData(i);
          request->send(200, "text/plain", "Updated");
          return;
        }
      }
      request->send(404, "text/plain", "Device not found");
    } else {
      request->send(400, "text/plain", "Bad Request");
    } });

  server.on("/start_scan", HTTP_POST, [](AsyncWebServerRequest *request)
            { scanForBLEDevices();
            request->send(200, "text/plain", "Scan Started"); });

  server.begin();
}

void setupGPIO()
{
  for (int i = 0; i < sizeof(gpioPins) / sizeof(int); i++)
  {
    pinMode(gpioPins[i], OUTPUT);
  }
  pinMode(boilerPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
}

// Управление устройствами и GPIO
void manageDevicesAndControlGPIO()
{
  bool activateBoiler = false;
  bool activatePump = false;

  // Сбор уникальных GPIO для включения
  std::set<int> uniqueGpioToEnable;

  for (int i = 0; i < 10; i++)
  {
    if (deviceData[i].isConnected && (deviceData[i].targetTemp - 2) > deviceData[i].tempReductionTime)
    {
      for (int j = 0; j < 5; j++)
      {
        int gpio = deviceData[i].gpioToEnable[j];
        if (gpio != -1)
        {
          uniqueGpioToEnable.insert(gpio);
        }
      }
      if (deviceData[i].pump)
      {
        activatePump = true;
      }
      if (deviceData[i].boiler)
      {
        activateBoiler = true;
      }
    }
  }

  // Включение/выключение уникальных GPIO
  for (int i = 0; i < sizeof(gpioPins) / sizeof(int); i++)
  {
    if (uniqueGpioToEnable.find(gpioPins[i]) != uniqueGpioToEnable.end())
    {
      digitalWrite(gpioPins[i], HIGH);
    }
    else
    {
      digitalWrite(gpioPins[i], LOW);
    }
  }

  digitalWrite(boilerPin, activateBoiler ? HIGH : LOW);
  digitalWrite(pumpPin, activatePump ? HIGH : LOW);
}

void setup()
{
  Serial.begin(115200);
  loadDeviceData();
  setupBLE();
  setupGPIO();
  setupWebServer();

  // Подключение к сохраненной WiFi сети
  preferences.begin("wifi-creds", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();

  if (!ssid.isEmpty() && !password.isEmpty())
  {
    WiFi.begin(ssid.c_str(), password.c_str());

    // Ожидание подключения
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    pCharacteristicIp->setValue(WiFi.localIP().toString().c_str());
    pCharacteristicIp->notify();
  }
  else
  {
    Serial.println("No WiFi credentials found.");
  }
}

void loop()
{
  manageDevicesAndControlGPIO();
  // server.handleClient(); // Обработка клиентских запросов на web-сервере
  delay(1000); // Пауза для предотвращения излишней нагрузки на процессор
}
