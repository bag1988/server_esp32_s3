/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "BLEDevice.h"

//Имя сервера BLE(другое имя ESP32, на котором выполняется эскиз сервера)
#define bleServerName "DHT11_ESP32C3"

#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"

/* UUID сервиса, характеристика, которую мы хотим прочитать*/
// BLE Service
static BLEUUID bmeServiceUUID(SERVICE_UUID);

// BLE Characteristics
//Temperature Celsius Characteristic
static BLEUUID temperatureCharacteristicUUID(TEMP_CHARACT_SERVICE_UUID);

// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID(NUM_CHARACT_SERVICE_UUID);

//Флажки, указывающие, должно ли начаться подключение и установлено ли соединение
static boolean doConnect = false;
static boolean connected = false;

//Адрес периферийного устройства. Адрес будет найден во время сканирования...
static BLEAddress* pServerAddress;

//Характеристики, с которыми мы хотим ознакомиться
static BLERemoteCharacteristic* temperatureCharacteristic;
static BLERemoteCharacteristic* humidityCharacteristic;

//Активировать уведомление
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

//Переменные для поддержания температуры и влажности
char* temperatureChar;
char* humidityChar;

//Флажки для проверки наличия новых показаний температуры и влажности
boolean newTemperature = false;
boolean newHumidity = false;

//Подключитесь к серверу BLE с соответствующим именем, сервисом и характеристиками
bool connectToServer(BLEAddress pAddress) {
  BLEClient* pClient = BLEDevice::createClient();

  // Connect to the remove BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");

  // Получите ссылку на сервис, который нам нужен, на удаленном сервере BLE.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }

  // Получите ссылку на характеристики в сервисе удаленного BLE-сервера.
  temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (temperatureCharacteristic == nullptr || humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");

  //Назначьте функции обратного вызова для характеристик
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  return true;
}

//Функция обратного вызова, которая вызывается при получении ответа с другого устройства
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) {                 //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop();                              //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());  //Address of advertiser is the one we need
      doConnect = true;                                                //Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};

//Когда сервер BLE отправляет новое значение температуры со свойством notify,
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                      uint8_t* pData, size_t length, bool isNotify) {
  //store temperature value
  temperatureChar = (char*)pData;
  newTemperature = true;
}

//Когда сервер BLE отправляет новые данные о влажности с помощью свойства notify
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
  //store humidity value
  humidityChar = (char*)pData;
  newHumidity = true;
  Serial.print(newHumidity);
}

void setup() {
  //Start serial communication
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  //Init BLE device
  BLEDevice::init("");

  // Извлекаем сканер и настраиваем обратный вызов, который мы хотим использовать, чтобы получать информацию, когда мы
  // обнаружим новое устройство.  Указываем, что мы хотим, чтобы сканирование было активным,
  // и запускаем сканирование в течение 30 секунд.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // Если флаг "doConnect" имеет значение true, то мы просканировали и нашли желаемый
  // BLE Сервер, к которому мы хотим подключиться.  Теперь мы подключаемся к нему.  Как только мы будем
  // подключено мы устанавливаем флаг подключено равным true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      //Активируйте свойство Уведомлять для каждой характеристики
      temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("Нам не удалось подключиться к серверу; Перезагрузите ваше устройство, чтобы снова выполнить поиск близлежащего сервера BLE.");
    }
    doConnect = false;
  }
  //if new temperature readings are available, print in the OLED
  if (newTemperature && newHumidity) {
    newTemperature = false;
    newHumidity = false;
  }
  delay(1000);  // Delay a second between loops.
}