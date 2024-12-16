#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>

#define NAMESPACE_FLASH "ble_device"
#define KEY_NAME_FLASH "ble_devices"
//Имя сервера BLE(другое имя ESP32, на котором выполняется эскиз сервера)
#define bleServerName "DHT11_ESP32C3"

#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"

typedef struct {
  String name;
  String address;
  bool enabled;
  long active_second;
  float temp;
  float hum;
  bool launch_pump;
  bool connected;
  byte enabled_relay[10];
} ble_device_info;

ble_device_info* knownBLEAddresses;

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

bool device_found;
BLEScan* pBLEScan;
Preferences prefs;

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

bool connectToServer(BLEAddress pAddress) {
  BLEClient* pClient = BLEDevice::createClient();

  // Connect to the remove BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");

  // Получите ссылку на сервис, который нам нужен, на удаленном сервере BLE.
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(SERVICE_UUID);
    return (false);
  }

  // Получите ссылку на характеристики в сервисе удаленного BLE-сервера.
  temperatureCharacteristic = pRemoteService->getCharacteristic(TEMP_CHARACT_SERVICE_UUID);
  humidityCharacteristic = pRemoteService->getCharacteristic(NUM_CHARACT_SERVICE_UUID);

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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) {
      bool isFind = false;
      for (int i = 0; i < (sizeof(knownBLEAddresses) / sizeof(ble_device_info)); i++) {
        if (strcmp(advertisedDevice.getAddress().toString().c_str(), knownBLEAddresses[i].address.c_str()) == 0) {
          knownBLEAddresses[i].connected = true;
          Serial.println("Device found. Connecting!");
          isFind = true;
          break;
        }
      }
      //TODO как добавить новый элемент???????????????????????????
      if (!isFind) {
        //int sizeArr = sizeof(knownBLEAddresses) / sizeof(knownBLEAddresses)[0];
        //knownBLEAddresses[sizeArr] = new ble_device_info("", advertisedDevice.getAddress().toString(), 0, 0, false, 0, true);
      }
    }
  }
};



void setup() {
  Serial.begin(115200);          //Enable UART on ESP32
  prefs.begin(NAMESPACE_FLASH);  // use namespace in flash

  size_t schLen = prefs.getBytesLength(KEY_NAME_FLASH);
  if (schLen > 0) {
    char buffer[schLen];  // prepare a buffer for the data
    prefs.getBytes(KEY_NAME_FLASH, buffer, schLen);
    if (schLen % sizeof(ble_device_info)) {  // simple check that data fits
      log_e("Data is not correct size!");
      return;
    }
    knownBLEAddresses = (ble_device_info *)buffer;  // cast the bytes into a struct ptr

    Serial.println("Сохраненные устройства BLE:");

    for (int i = 0; i < schLen; i++) {
      Serial.println(knownBLEAddresses[i].name);
    }
  } else {
    Serial.println("Нет сохраненных устройств BLE!");
  }

  //инициализация ble
  Serial.println("Scanning...");  // Print Scanning
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();                                            //создаем новое сканирование
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());  //Init Callback Function (инициализируем функцию обратного вызова)
  pBLEScan->setActiveScan(true);                                              //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);                                                 // устанавливаем интервал сканирования
  pBLEScan->setWindow(99);                                                    // устанавливаем окно сканирования, менее или равно значению setInterval
}

void loop() {
  // Если флаг "doConnect" имеет значение true, то мы просканировали и нашли желаемый
  // BLE Сервер, к которому мы хотим подключиться.  Теперь мы подключаемся к нему.  Как только мы будем
  // подключено мы устанавливаем флаг подключено равным true.
  // if (doConnect == true) {
  //   if (connectToServer(*pServerAddress)) {
  //     Serial.println("We are now connected to the BLE Server.");
  //     //Активируйте свойство Уведомлять для каждой характеристики
  //     temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
  //     humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
  //     connected = true;
  //   } else {
  //     Serial.println("Нам не удалось подключиться к серверу; Перезагрузите ваше устройство, чтобы снова выполнить поиск близлежащего сервера BLE.");
  //   }
  //   doConnect = false;
  // }
  // //if new temperature readings are available, print in the OLED
  // if (newTemperature && newHumidity) {
  //   newTemperature = false;
  //   newHumidity = false;
  // }
  // delay(1000);  // Delay a second between loops.
}
