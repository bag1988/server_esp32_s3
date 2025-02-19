#include <ble_setting.h>
#include <lcd_setting.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEDescriptor.h>
#include <BLEScan.h>
#include <variables_info.h>
#include <spiffs_setting.h>

BLEServer *pServer = nullptr;

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
    //connectWiFi();               // Attempt to connect to WiFi
  }
};

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
