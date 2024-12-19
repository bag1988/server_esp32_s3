#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
//#include <WebServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <devInfoArray.h>

// Имя сервера BLE(другое имя ESP32, на котором выполняется эскиз сервера)
#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"
#define SERVER_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"
#define SERVER_SERVICE_WIFI_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"
#define SERVER_SERVICE_IP_UUID "b882babc-6942-494c-a47b-497b4caa86d4"
// Структура данных для сохранения информации о BLE подключениях

#define DEELY_LOOP 1000

// Глобальные переменные
Preferences preferences;
DevInfo *deviceData;
uint8_t gpioPins[] = {12, 13, 14, 15, 16}; // Глобальный массив GPIO
const uint8_t boilerPin = 17;              // GPIO котла
const uint8_t pumpPin = 18;                // GPIO насоса
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
  size_t schLen = preferences.getBytesLength("device-data");
  char buffer[schLen]; // prepare a buffer for the data
  preferences.getBytes("schedule", buffer, schLen);
  if (schLen % sizeof(DevInfo))
  { // simple check that data fits
    log_e("Data is not correct size!");
    return;
  }
  deviceData = (DevInfo *)buffer;
  preferences.end();
}
// Сохранение данных
void saveDeviceData(int index)
{
  preferences.begin("device-data", false);
  preferences.putBytes("device-data", deviceData, sizeof(deviceData));
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

static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string value = (char *)pData;
  std::string ble_address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  DevInfo *find = findInList(deviceData, ble_address);
  if (find != NULL)
  {
    find->temp = std::stof(value);
  }
}

static void humidityNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string value = (char *)pData;
  std::string ble_address = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  DevInfo *find = findInList(deviceData, ble_address);
  if (find != NULL)
  {
    find->humidity = std::stof(value);
  }
}

void startConnectWifi()
{
  if (!ssid.isEmpty() && !password.isEmpty())
  {
    WiFi.begin(ssid.c_str(), password.c_str());

    // Ожидание подключения
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println(F("Connecting to WiFi..."));
    }
    Serial.println(F("Connected to WiFi."));
    Serial.print(F("IP address: "));
    Serial.println(F(WiFi.localIP().toString().c_str()));
    pCharacteristicIp->setValue(WiFi.localIP().toString().c_str());
    pCharacteristicIp->notify();
  }
  else
  {
    Serial.println(F("No WiFi credentials found."));
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
      startConnectWifi();
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
      BLEDevice::getScan()->stop();
      BLEClient *pClient = BLEDevice::createClient();
      pClient->connect(&advertisedDevice);
      // Подписка на обновления характеристик
      BLERemoteService *pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
      if (pRemoteService != nullptr)
      {
        // Поиск в списке сохранненых
        std::string ble_address = advertisedDevice.getAddress().toString();
        DevInfo *find = findInList(deviceData, ble_address);
        if (find == NULL)
        {
          DevInfo newItem = {.ble_address = ble_address,
                             .name = advertisedDevice.getName(),
                             .enabled = false,
                             .isConnected = true,
                             .gpioToEnable = {0},
                             .pump = false,
                             .boiler = false,
                             .targetTemp = 25.0,
                             .temp = 25.0,
                             .humidity = 0.0,
                             .tempReductionTime = 0};

          addToList(deviceData, newItem);
        }
        //
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

  DevInfo *forStartArray = filterList(deviceData, DEELY_LOOP);

  // Сбор уникальных GPIO для включения
  uint8_t *uniqueGpioToEnable = getGpioArray(forStartArray);

  // Включение/выключение уникальных GPIO
  for (int i = 0; i < sizeof(gpioPins) / sizeof(uint8_t); i++)
  {
    if (anyGpioValue(uniqueGpioToEnable, gpioPins[i]))
    {
      digitalWrite(gpioPins[i], HIGH);
    }
    else
    {
      digitalWrite(gpioPins[i], LOW);
    }
  }

  digitalWrite(boilerPin, anyBoiler(forStartArray) ? HIGH : LOW);
  digitalWrite(pumpPin, anyPump(forStartArray) ? HIGH : LOW);

  free(forStartArray);
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

  startConnectWifi();
}

void loop()
{
  manageDevicesAndControlGPIO();
  // server.handleClient(); // Обработка клиентских запросов на web-сервере
  delay(DEELY_LOOP); // Пауза для предотвращения излишней нагрузки на процессор
}
