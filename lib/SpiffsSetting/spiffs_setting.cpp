#include <spiffs_setting.h>

Preferences preferences;

void saveServerSetting()
{
  if (preferences.begin("server_setting", false))
  {
    preferences.putLong64("server_time", serverWorkTime);
    preferences.putFloat("hysteresis_temp", hysteresisTemp);
    preferences.end();
  }
}

void loadServerWorkTime()
{
  if (preferences.begin("server_setting", true))
  {
    serverWorkTime = preferences.getLong64("server_time", 0);
    hysteresisTemp = preferences.getFloat("hysteresis_temp", 1.5);
    preferences.end();
  }
}

void loadClientsFromFile(std::vector<DeviceData>& devices)
{
  logAndSend("Loading clients from Preferences...");

  if (preferences.begin("devices", true))
  {
    devices.clear();

    size_t blobSize = preferences.getBytesLength("devices_blob");

    if (blobSize > 0)
    {
      uint8_t *buffer = new uint8_t[blobSize];

      if (buffer)
      {
        size_t readSize = preferences.getBytes("devices_blob", buffer, blobSize);

        if (readSize == blobSize)
        {
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, buffer, blobSize);

          if (!error)
          {
            JsonArray devicesArray = doc.as<JsonArray>();

            for (JsonObject deviceObj : devicesArray)
            {
              DeviceData device;

              device.name = deviceObj["name"].as<const char *>();
              device.macAddress = deviceObj["mac"].as<const char *>();
              device.targetTemperature = deviceObj["target_temp"].as<float>();
              device.enabled = deviceObj["enabled"].as<bool>();

              if (!deviceObj["gpio_pins"].isNull())
              {
                JsonArray pinsArray = deviceObj["gpio_pins"].as<JsonArray>();
                for (uint8_t pin : pinsArray)
                {
                  device.gpioPins.push_back(pin);
                }
              }

              device.heatingStartTime = 0;
              device.totalHeatingTime = deviceObj["total_heating"].as<long>();
              device.isOnline = false;
              device.currentTemperature = device.targetTemperature;
              device.heatingActive = false;
              device.humidity = deviceObj["humidity"].as<float>();
              device.battery = deviceObj["battery"].as<uint8_t>();
              device.batteryV = deviceObj["batteryV"].as<uint16_t>();

              devices.push_back(device);
            }
          }
          else
          {
            logAndSend("deserializeJson() failed: " + String(error.c_str()));
          }
        }
        else
        {
          logAndSend("Failed to read complete blob data");
        }

        delete[] buffer;
      }
      else
      {
        logAndSend("Failed to allocate memory for blob data");
      }
    }
    else
    {
      logAndSend("No devices data found in Preferences");
    }

    preferences.end();
  }
  else
  {
    logAndSend("Failed to open 'devices' namespace");
  }
}

void saveClientsToFile(const std::vector<DeviceData>& devices)
{
  logAndSend("Начинаем сохранение устройств в файл");

  if (preferences.begin("devices", false))
  {
    JsonDocument doc;
    JsonArray devicesArray = doc.to<JsonArray>();

    for (const auto &device : devices)
    {
      JsonObject deviceObj = devicesArray.add<JsonObject>();

      deviceObj["name"] = device.name;
      deviceObj["mac"] = device.macAddress;
      deviceObj["target_temp"] = device.targetTemperature;
      deviceObj["enabled"] = device.enabled;
      deviceObj["total_heating"] = device.totalHeatingTime;
      deviceObj["humidity"] = device.humidity;
      deviceObj["battery"] = device.battery;
      deviceObj["batteryV"] = device.batteryV;

      for (uint8_t pin : device.gpioPins)
      {
        deviceObj["gpio_pins"].add(pin);
      }
    }

    size_t bufferSize = measureJson(doc);

    if (bufferSize > 0)
    {
      uint8_t *buffer = new uint8_t[bufferSize];

      if (buffer)
      {
        size_t serializedSize = serializeJson(doc, buffer, bufferSize);

        if (preferences.putBytes("devices_blob", buffer, serializedSize))
        {
          logAndSend("Saved clients to Preferences blob");
        }
        else
        {
          logAndSend("Failed to save devices blob to Preferences");
        }

        delete[] buffer;
      }
      else
      {
        logAndSend("Failed to allocate memory for serialization");
      }
    }
    else
    {
      logAndSend("Empty devices collection, nothing to save");
    }

    preferences.end();
  }
  else
  {
    logAndSend("Failed to open 'devices' namespace");
  }
}

void loadWifiCredentialsFromFile()
{
  if (preferences.begin("wifi", true))
  {
    wifiCredentials.ssid = preferences.getString("ssid", "").c_str();
    wifiCredentials.password = preferences.getString("password", "").c_str();

    preferences.end();

    logAndSend("WiFi-креденциалы загружены из Preferences");
  }
  else
  {
    logAndSend("Нет доступа для чтения данных wifi");
  }
}

void saveWifiCredentialsToFile()
{
  if (preferences.begin("wifi", false))
  {
    preferences.putString("ssid", wifiCredentials.ssid.c_str());
    preferences.putString("password", wifiCredentials.password.c_str());

    preferences.end();

    logAndSend("WiFi-креденциалы сохранены в Preferences");
  }
  else
  {
    logAndSend("Нет доступа для записи данных wifi");
  }
}

void saveGpioToFile(const std::vector<GpioPin>& gpioPins)
{
  logAndSend("Saving GPIO to Preferences...");

  if (preferences.begin("gpio", false))
  {
    JsonDocument doc;
    JsonArray gpioArray = doc.to<JsonArray>();

    for (const auto &gpio : gpioPins)
    {
      JsonObject gpioObj = gpioArray.add<JsonObject>();
      gpioObj["pin"] = gpio.pin;
      gpioObj["state"] = gpio.state;
      gpioObj["name"] = gpio.name;
      gpioObj["total_heating"] = gpio.totalHeatingTime;
    }

    size_t bufferSize = measureJson(doc);

    if (bufferSize > 0)
    {
      uint8_t *buffer = new uint8_t[bufferSize];

      if (buffer)
      {
        size_t serializedSize = serializeJson(doc, buffer, bufferSize);

        if (preferences.putBytes("gpio_blob", buffer, serializedSize))
        {
          logAndSend("Saved GPIO pins to Preferences blob");
        }
        else
        {
          logAndSend("Failed to save GPIO blob to Preferences");
        }

        delete[] buffer;
      }
      else
      {
        logAndSend("Failed to allocate memory for serialization");
      }
    }
    else
    {
      logAndSend("Empty GPIO collection, nothing to save");
    }

    preferences.end();
  }
  else
  {
    logAndSend("Failed to open 'gpio' namespace");
  }
}

void loadGpioFromFile(std::vector<GpioPin>& gpioPins)
{
  logAndSend("Loading GPIO from Preferences...");

  if (preferences.begin("gpio", true))
  {
    size_t blobSize = preferences.getBytesLength("gpio_blob");

    if (blobSize > 0)
    {
      uint8_t *buffer = new uint8_t[blobSize];

      if (buffer)
      {
        size_t readSize = preferences.getBytes("gpio_blob", buffer, blobSize);

        if (readSize == blobSize)
        {
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, buffer, blobSize);

          if (!error)
          {
            gpioPins.clear();

            JsonArray gpioArray = doc.as<JsonArray>();

            for (JsonObject gpioObj : gpioArray)
            {
              GpioPin gpio;
              gpio.pin = gpioObj["pin"].as<uint8_t>();
              gpio.state = gpioObj["state"].as<uint8_t>();
              gpio.name = gpioObj["name"].as<const char *>();
              gpio.totalHeatingTime = gpioObj["total_heating"].as<long>();

              gpioPins.push_back(gpio);
            }
          }
          else
          {
            logAndSend("deserializeJson() failed: " + String(error.c_str()));
          }
        }
        else
        {
          logAndSend("Failed to read complete blob data");
        }

        delete[] buffer ;     
      }
      else
      {
        logAndSend("Failed to allocate memory for blob data");
      }
    }
    preferences.end();
  }
}