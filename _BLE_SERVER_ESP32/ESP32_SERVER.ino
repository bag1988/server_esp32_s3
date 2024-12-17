/*
Напиши код для ESP32 с использованием двух ядер в котором определены:
1) массив определенных GPIO которые могут быть включены;
2) GPIO котла для включения;
3) GPIO насоса для включения;
4) структура данных сохранения со следующими полями: 
-uuid ble подключения, 
-наименование устройства, 
-подключено ли в текущей момент, 
-какие GPIO из массива GPIO нужно включать, 
-нужно ли включать насос, 
-нужно ли включать котел,  
-поддерживаемая температура, 
-влажность, 
-общее время понижения температуры на 2 градуса относительно поддерживаемой температуры. 
5) массив содержащий все подключенные устройства.
Основная функциональность:
1) во время запуска считывает из памяти сохраненные данные содержащие информацию об подключениях по ble устройств со структурой данных описанною выше;
2) через блютуз server установить пароль и наименование wifi сети к которой подключаться, а также получить ip адрес сервера для управления данными;
3) выполняет поиск BLE устройств которые содержат нужные характеристики;
4) подписывается на обновления информации по протоколу BLE с сохранением новых данных в память;
5) сохраняет в память найденные устройства;
6) изменять наименование сохраненного устройства;
7) через web-server получать и менять температуру сработки любого подключенного устройства.
Отдельная функция которая выполняет действия:
1) находит в списке все устройства с температурой ниже на 2 градуса от заданной для этого устройства;
2) создает массив GPIO с уникальными значениями на основе найденного списка;
3) включает все GPIO из созданного массива, а все остальные из глобального массива GPIO выключает;
3) если в списке из пункта 1 есть хоть один который включает котел, то включает GPIO для котла, иначе выключает его;
4) если в списке из пункта 1 есть хоть один который включает насос, то включает GPIO для насоса, иначе выключает его.

 */
#include <BLEDevice.h> 
#include <BLEUtils.h> 
#include <BLEServer.h> 
#include <Preferences.h> 
#include <WiFi.h> 
#include <AsyncTCP.h> 
#include <ESPAsyncWebServer.h> 
#include <set> // для хранения уникальных значений GPIO

// Структура данных для сохранения информации о BLE подключениях
struct DeviceData {
  std::string uuid;
  std::string name;
  bool isConnected;
  int gpioToEnable[5];
  bool pump;
  bool boiler;
  float targetTemp;
  float humidity;
  int tempReductionTime;
};

// Глобальные переменные
Preferences preferences;
DeviceData deviceData[10];
int gpioPins[] = {12, 13, 14, 15, 16}; // Глобальный массив GPIO
const int boilerPin = 17;              // GPIO котла
const int pumpPin = 18;                // GPIO насоса

WebServer server(80); 
String ssid = ""; 
String password = "";

// Инициализация данных
void loadDeviceData() {
  preferences.begin("device-data", false);
  for (int i = 0; i < 10; i++) {
    deviceData[i].uuid = preferences.getString("uuid" + String(i), "");
    deviceData[i].name = preferences.getString("name" + String(i), "");
    deviceData[i].isConnected = preferences.getBool("isConnected" + String(i), false);
    for (int j = 0; j < 5; j++) {
      deviceData[i].gpioToEnable[j] = preferences.getInt("gpio" + String(i) + "_" + String(j), -1);
    }
    deviceData[i].pump = preferences.getBool("pump" + String(i), false);
    deviceData[i].boiler = preferences.getBool("boiler" + String(i), false);
    deviceData[i].targetTemp = preferences.getFloat("targetTemp" + String(i), 0.0);
    deviceData[i].humidity = preferences.getFloat("humidity" + String(i), 0.0);
    deviceData[i].tempReductionTime = preferences.getInt("tempReductionTime" + String(i), 0);
  }
  preferences.end();
}

// Сохранение данных
void saveDeviceData(int index) {
  preferences.begin("device-data", false);
  preferences.putString("uuid" + String(index), deviceData[index].uuid.c_str());
  preferences.putString("name" + String(index), deviceData[index].name.c_str());
  preferences.putBool("isConnected" + String(index), deviceData[index].isConnected);
  for (int j = 0; j < 5; j++) {
    preferences.putInt("gpio" + String(index) + "_" + String(j), deviceData[index].gpioToEnable[j]);
  }
  preferences.putBool("pump" + String(index), deviceData[index].pump);
  preferences.putBool("boiler" + String(index), deviceData[index].boiler);
  preferences.putFloat("targetTemp" + String(index), deviceData[index].targetTemp);
  preferences.putFloat("humidity" + String(index), deviceData[index].humidity);
  preferences.putInt("tempReductionTime" + String(index), deviceData[index].tempReductionTime);
  preferences.end();
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    // Пример обработки данных в формате "SSID,PASSWORD"
    size_t delimiterPos = value.find(",");
    if (delimiterPos != std::string::npos) {
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
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
      }

      Serial.println("Connected to WiFi.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
};



//Основная функциональность BLE
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

void setupBLE() {
  BLEDevice::init("ESP32_BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  BLEDevice::startAdvertising();
}

// Поиск BLE устройств и подписка на обновления
void scanForBLEDevices() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

// Обновление данных при изменении BLE характеристик
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    // Логика обработки новых данных и обновления deviceData
    updateDeviceData(value);
  }
};

void updateDeviceData(const std::string& value) {
  // Парсинг значения и обновление данных устройства
  // Например, value может содержать JSON, который нужно распарсить
  // Здесь простой пример обновления targetTemp
  for (int i = 0; i < 10; i++) {
    if (deviceData[i].uuid == "example_uuid") { // Пример соответствия UUID
      deviceData[i].targetTemp = std::stof(value);
      saveDeviceData(i);
      break;
    }
  }
}

// Определение нужных характеристик
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Логика поиска нужных характеристик
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      BLEDevice::getScan()->stop();
      BLEClient*  pClient  = BLEDevice::createClient();
      pClient->connect(&advertisedDevice);
      // Подписка на обновления характеристик
      BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
      if (pRemoteService != nullptr) {
        BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
        if (pRemoteCharacteristic != nullptr) {
          pRemoteCharacteristic->registerForNotify(new MyCharacteristicCallbacks());
        }
      }
      // Логика работы с устройством
    }
  }
};

//Web server для управления устройствами
void setupWebServer() {
  server.on("/get", HTTP_GET, []() {
    String json = "[";
    for (int i = 0; i < 10; i++) {
      if (!deviceData[i].uuid.empty()) {
        json += "{";
        json += "\"uuid\":\"" + String(deviceData[i].uuid.c_str()) + "\",";
        json += "\"name\":\"" + String(deviceData[i].name.c_str()) + "\",";
        json += "\"targetTemp\":" + String(deviceData[i].targetTemp);
        json += "},";
      }
    }
    json.remove(json.length() - 1);
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/set", HTTP_POST, []() {
    if (server.hasArg("uuid") && server.hasArg("targetTemp") && server.hasArg("name")) {
      String uuid = server.arg("uuid");
      float targetTemp = server.arg("targetTemp").toFloat();
      String name = server.arg("name");
      for (int i = 0; i < 10; i++) {
        if (deviceData[i].uuid == uuid.c_str()) {
          deviceData[i].targetTemp = targetTemp;
          deviceData[i].name = name.c_str();
          saveDeviceData(i);
          server.send(200, "text/plain", "Updated");
          return;
        }
      }
      server.send(404, "text/plain", "Device not found");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
}

void setup() {
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
  
  if (!ssid.isEmpty() && !password.isEmpty()) {
    WiFi.begin(ssid.c_str(), password.c_str());

    // Ожидание подключения
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No WiFi credentials found.");
  }
}

void loop() {
  manageDevicesAndControlGPIO();
  server.handleClient();  // Обработка клиентских запросов на web-сервере
  delay(1000);  // Пауза для предотвращения излишней нагрузки на процессор
}


void setupGPIO() {
  for (int i = 0; i < sizeof(gpioPins)/sizeof(int); i++) {
    pinMode(gpioPins[i], OUTPUT);
  }
  pinMode(boilerPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);
}

// Управление устройствами и GPIO
void manageDevicesAndControlGPIO() {
  bool activateBoiler = false;
  bool activatePump = false;

  // Сбор уникальных GPIO для включения
  std::set<int> uniqueGpioToEnable;

  for (int i = 0; i < 10; i++) {
    if (deviceData[i].isConnected && (deviceData[i].targetTemp - 2) > deviceData[i].tempReductionTime) {
      for (int j = 0; j < 5; j++) {
        int gpio = deviceData[i].gpioToEnable[j];
        if (gpio != -1) {
          uniqueGpioToEnable.insert(gpio);
        }
      }
      if (deviceData[i].pump) {
        activatePump = true;
      }
      if (deviceData[i].boiler) {
        activateBoiler = true;
      }
    }
  }

  // Включение/выключение уникальных GPIO
  for (int i = 0; i < sizeof(gpioPins)/sizeof(int); i++) {
    if (uniqueGpioToEnable.find(gpioPins[i]) != uniqueGpioToEnable.end()) {
      digitalWrite(gpioPins[i], HIGH);
    } else {
      digitalWrite(gpioPins[i], LOW);
    }
  }

  digitalWrite(boilerPin, activateBoiler ? HIGH : LOW);
  digitalWrite(pumpPin, activatePump ? HIGH : LOW);
}

