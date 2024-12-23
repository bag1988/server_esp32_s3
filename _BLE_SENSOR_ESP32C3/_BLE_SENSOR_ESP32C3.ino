#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BTAdvertisedDevice.h>
#include <BTScan.h>
#include <BLE2902.h>

#include <DHT11.h>

//BLE server name
#define bleServerName "DHT11_ESP32C3"


float temp;
float tempF;
float hum;

DHT11 dht(2);  // выход DAT подключен к цифровому входу 2

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;


bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"


// Temperature Characteristic and Descriptor
BLECharacteristic bmeTemperatureCelsiusCharacteristics(TEMP_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902)); //https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

// Humidity Characteristic and Descriptor
BLECharacteristic bmeHumidityCharacteristics(NUM_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2902));//было 0x2903 //https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);  // LED

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *bmeService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
  // Temperature
  bmeService->addCharacteristic(&bmeTemperatureCelsiusCharacteristics);
  bmeTemperatureCelsiusDescriptor.setValue("BME temperature Celsius");
  bmeTemperatureCelsiusCharacteristics.addDescriptor(&bmeTemperatureCelsiusDescriptor);

  // Humidity
  bmeService->addCharacteristic(&bmeHumidityCharacteristics);
  bmeHumidityDescriptor.setValue("BME humidity");
  bmeHumidityCharacteristics.addDescriptor(new BLE2902());

  // Start the service
  bmeService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
    if ((millis() - lastTime) > timerDelay) {
      int temperature = 0;
      int humidity = 0;

      // Attempt to read the temperature and humidity values from the DHT11 sensor.
      int result = dht.readTemperatureHumidity(temperature, humidity);
      if (result == 0) {
        
        static char temperatureCTemp[6];
        dtostrf(temperature, 6, 2, temperatureCTemp);
        //Set temperature Characteristic value and notify connected client
        bmeTemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
        bmeTemperatureCelsiusCharacteristics.notify();
        Serial.print("Temperature Celsius: ");
        Serial.print(temperature);
        Serial.print(" ºC");

        //Notify humidity reading from BME
        static char humidityTemp[6];
        dtostrf(humidity, 6, 2, humidityTemp);
        //Set humidity Characteristic value and notify connected client
        bmeHumidityCharacteristics.setValue(humidityTemp);
        bmeHumidityCharacteristics.notify();
        Serial.print(" - Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");

        lastTime = millis();
      } else {
        // Print error message based on the error code.
        Serial.println(DHT11::getErrorString(result));
      }
    }
  }
}