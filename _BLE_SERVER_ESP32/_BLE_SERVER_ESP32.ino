#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>

#define NAMESPACE_FLASH "ble_device"
#define KEY_NAME_FLASH "ble_devices"

String knownBLEAddresses[] = { "aa:bc:cc:dd:ee:ee", "54:2c:7b:87:71:a2" };
int RSSI_THRESHOLD = -55;
bool device_found;
int scanTime = 5;  //In seconds
BLEScan *pBLEScan;
Preferences prefs;


typedef struct {
  String name;
  BLEAddress address;
  float temp;
  float hum;
  bool launch_pump;
  long active_second;
} ble_device_info;



class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    for (int i = 0; i < (sizeof(knownBLEAddresses) / sizeof(knownBLEAddresses[0])); i++) {
      if (strcmp(advertisedDevice.getAddress().toString().c_str(), knownBLEAddresses[i].c_str()) == 0) {
        device_found = true;
        break;
      } else {
        device_found = false;
      }
    }
    Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
  }
};
//запуск сканирования
void start_scan_ble() {
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    int rssi = device.getRSSI();
    Serial.print("RSSI: ");
    Serial.println(rssi);
    if (rssi > RSSI_THRESHOLD && device_found == true) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  pBLEScan->clearResults();  // delete results fromBLEScan buffer to release memory
}

void setup() {
  Serial.begin(115200);          //Enable UART on ESP32
  prefs.begin(NAMESPACE_FLASH);  // use namespace in flash

  size_t schLen = prefs.getBytesLength(KEY_NAME_FLASH);
  if (schLen > 0) {
    char buffer[schLen];  // prepare a buffer for the data
    prefs.getBytes(KEY_NAME_FLASH, buffer, schLen);

    ble_device_info *device_info = (ble_device_info *)buffer;  // cast the bytes into a struct ptr

    Serial.println("Сохраненные устройства BLE:");

    for (int i = 0; i < schLen; i++) {
      Serial.println(device_info[i].name);
    }
  } else {
    Serial.println("Нет сохраненных устройств BLE!");
  }

  //инициализация ble
  // Serial.println("Scanning...");  // Print Scanning
  pinMode(LED_BUILTIN, OUTPUT);  //режим работы контакта BUILTIN_LED на вывод данных
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();                                            //создаем новое сканирование
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());  //Init Callback Function (инициализируем функцию обратного вызова)
  pBLEScan->setActiveScan(true);                                              //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);                                                 // устанавливаем интервал сканирования
  pBLEScan->setWindow(99);                                                    // устанавливаем окно сканирования, менее или равно значению setInterval
}

void loop() {
  // put your main code here, to run repeatedly:
}
