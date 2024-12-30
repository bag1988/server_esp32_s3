#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BTAdvertisedDevice.h>
#include <BTScan.h>
#include <BLE2902.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// BLE server name
#define bleServerName "DHT11_ESP32C3"

#define DHTPIN 2 // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

// Uncomment the type of sensor in use:
#define DHTTYPE DHT11 // DHT 11
// #define DHTTYPE    DHT22     // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT_Unified dht(DHTPIN, DHTTYPE);

uint32_t delayMS;

bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "33b6ebbe-538f-4d4a-ba39-2ee04516ff39"
#define TEMP_CHARACT_SERVICE_UUID "ccfe71ea-e98b-4927-98e2-6c1b77d1f756"
#define NUM_CHARACT_SERVICE_UUID "6ed76625-573e-4caa-addf-3ddc5a283095"

// Temperature Characteristic and Descriptor
BLECharacteristic bmeTemperatureCelsiusCharacteristics(TEMP_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902)); // https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

// Humidity Characteristic and Descriptor
BLECharacteristic bmeHumidityCharacteristics(NUM_CHARACT_SERVICE_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2902)); // было 0x2903 //https://bitbucket.org/bluetooth-SIG/public/src/b012e68be90070f06ca43a9e24463cb86ca817c6/assigned_numbers/uuids/descriptors.yaml?at=main#descriptors.yaml-41

// Setup callbacks onConnect and onDisconnect
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    printf("Device connected, count device connected %d", pServer->getConnectedCount());
    deviceConnected = true;
  };
  void onDisconnect(BLEServer *pServer)
  {
    printf("Device disconnected, count device connected %d", pServer->getConnectedCount());
    deviceConnected = false;
  }
};

void initDHTSensor()
{
  printf("init DHT sensor");
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("°C"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("°C"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print(F("Sensor Type: "));
  Serial.println(sensor.name);
  Serial.print(F("Driver Ver:  "));
  Serial.println(sensor.version);
  Serial.print(F("Unique ID:   "));
  Serial.println(sensor.sensor_id);
  Serial.print(F("Max Value:   "));
  Serial.print(sensor.max_value);
  Serial.println(F("%"));
  Serial.print(F("Min Value:   "));
  Serial.print(sensor.min_value);
  Serial.println(F("%"));
  Serial.print(F("Resolution:  "));
  Serial.print(sensor.resolution);
  Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;

  printf("delayMS is %d", delayMS);
}

void setupBleServer()
{  
  printf("Create ble server %s", bleServerName);
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
  bmeHumidityCharacteristics.addDescriptor(&bmeHumidityDescriptor);

  // Start the service
  bmeService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

}

void setup()
{
  Serial.begin(115200);  
  setupBleServer();
  initDHTSensor();
}

void loopDHT()
{  
  // Delay between measurements.
  delay(delayMS);
  // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    Serial.println(F("Error reading temperature!"));
  }
  else
  {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    bmeTemperatureCelsiusCharacteristics.setValue(event.temperature);
    if (deviceConnected)
    {
      bmeTemperatureCelsiusCharacteristics.notify();
    }
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    Serial.println(F("Error reading humidity!"));
  }
  else
  {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    bmeHumidityCharacteristics.setValue(event.relative_humidity);
    if (deviceConnected)
    {
      bmeHumidityCharacteristics.notify();
    }
  }
}

void loop()
{
  loopDHT();
}