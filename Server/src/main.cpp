// #include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
// #include <WebServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <dev_info.h>

// Имя сервера BLE(другое имя ESP32, на котором выполняется эскиз сервера)
#define SERVER_NAME "ESP32_BLE_CENTRAL_SERVER"
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"
#define SERVER_UUID "e1de7d6e-3104-4065-a187-2de5e5727b26"
#define SERVER_SERVICE_WIFI_UUID "93d971b2-4bb8-45d0-9ab3-74d7f881d828"
// #define SERVER_SERVICE_IP_UUID "b882babc-6942-494c-a47b-497b4caa86d4"
//  Структура данных для сохранения информации о BLE подключениях

#define DEELY_LOOP 1000

// Глобальные переменные
Preferences preferences;
AsyncWebServer server(80);
std::vector<DevInfo> devices_ble;
typedef struct
{
  int gpio;
  char name[20];
} GpioAccess;

GpioAccess gpioAccess[] = {
    {18, "котел"},
    {19, "насос"},
    {20, "ванна"},
    {21, "кухня"}};

// Основная функциональность BLE
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristicSetWifi = NULL;
// BLECharacteristic *pCharacteristicIp = NULL;
bool deviceConnected = false;
//  Инициализация данных
void loaddevices_ble()
{
  preferences.begin("device-data", false);
  size_t schLen = preferences.getBytesLength("device-data");
  char buffer[schLen]; // prepare a buffer for the data
  preferences.getBytes("schedule", buffer, schLen);
  if (schLen % sizeof(DevInfo))
  { // simple check that data fits
    Serial.println(F("Data is not correct size!"));
    return;
  }
  int bufferSize = sizeof(buffer) / sizeof(DevInfo);
  devices_ble = parseDevicesFromBytes(buffer, sizeof(buffer) - 1);
  preferences.end();
}
// Сохранение данных
void savedevices_ble()
{
  preferences.begin("device-data", false);
  auto byteData = convertDevicesToBytes(devices_ble);
  preferences.putBytes("device-data", byteData.data(), sizeof(byteData));
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
  String value = (char *)pData;
  DevInfo *find = findDevice(devices_ble, pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str());
  if (find != NULL)
  {
    find->temp = value.toFloat();
  }
}

static void humidityNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  String value = (char *)pData;
  DevInfo *find = findDevice(devices_ble, pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str());
  if (find != NULL)
  {
    find->humidity = value.toFloat();
  }
}

void startConnectWifi(String ssid, String password)
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
    pCharacteristicSetWifi->setValue(WiFi.localIP().toString().c_str());
    if (deviceConnected)
    {
      pCharacteristicSetWifi->notify();
    }
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
    String value = pCharacteristic->getValue().c_str();
    // Пример обработки данных в формате "SSID,PASSWORD"
    int delimiterPos = value.indexOf(",");
    if (delimiterPos != value.length())
    {
      String ssid = value.substring(0, delimiterPos);
      String password = value.substring(delimiterPos + 1);

      // Сохранение данных WiFi в память
      preferences.begin("wifi-creds", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();
      startConnectWifi(ssid, password);
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
        DevInfo *find = findDevice(devices_ble, ble_address.c_str());
        if (find == NULL)
        {
          DevInfo new_item;
          new_item.ble_address = new_item.ble_address;
          new_item.name = new_item.name;
          new_item.enabled = false;
          new_item.isConnected = true;
          new_item.gpioToEnable[0] = 18;
          new_item.targetTemp = 25.0;
          new_item.temp = 25.0;
          new_item.humidity = 0;
          new_item.totalTimeActive = 0;
          addDevice(devices_ble, new_item);
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
  // pCharacteristicIp = pService->createCharacteristic(
  //     SERVER_SERVICE_IP_UUID, BLECharacteristic::PROPERTY_NOTIFY);
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
    auto jsonString = convertDevicesToBytes(devices_ble);
    request->send(200, "application/json", jsonString.data()); });

  server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("update_dev")) {
      //DevInfo newDevs[MAX_DEVICES];
      //int newDevCount;
      auto buffer = request->arg("update_dev").c_str();
      auto newDevs = parseDevicesFromBytes(buffer, sizeof(buffer) - 1);

      if(newDevs.size())
      {
        updateDevices(devices_ble, newDevs);  
        savedevices_ble();
                request->send(200, "text/plain", "Updated");
                return;
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
  for (int i = 0; i < sizeof(gpioAccess) / sizeof(GpioAccess); i++)
  {
    pinMode(gpioAccess[i].gpio, OUTPUT);
  }
}

// Управление устройствами и GPIO
void manageDevicesAndControlGPIO()
{
  std::vector<DevInfo> findList = filterDevicesByTemp(devices_ble, DEELY_LOOP);

  if (findList.size())
  {
    std::vector<uint8_t> gpioList;

    for (auto &dev : findList)
    {
      for (int j = 0; j < sizeof(dev.gpioToEnable); j++)
      {
        if (dev.gpioToEnable[j] > 0)
        {
          gpioList.push_back(dev.gpioToEnable[j]);
        }
      }
    }

    for (int i = 0; i < sizeof(gpioAccess) / sizeof(GpioAccess); i++)
    {
      auto it = std::find(gpioList.begin(), gpioList.end(), gpioAccess[i].gpio);

      // Проверяем, найдено ли значение
      if (it != gpioList.end())
      {
        digitalWrite(gpioAccess[i].gpio, HIGH);
      }
      else
      {
        digitalWrite(gpioAccess[i].gpio, LOW);
      }
    }
  }

}

void setup()
{
  Serial.begin(115200);
  loaddevices_ble();
  setupBLE();
  setupGPIO();
  setupWebServer();

  // Подключение к сохраненной WiFi сети
  preferences.begin("wifi-creds", false);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  startConnectWifi(ssid, password);
}

void loop()
{
  manageDevicesAndControlGPIO();
  // server.handleClient(); // Обработка клиентских запросов на web-сервере
  delay(DEELY_LOOP); // Пауза для предотвращения излишней нагрузки на процессор
}
