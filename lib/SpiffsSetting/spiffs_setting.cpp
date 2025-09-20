#include <spiffs_setting.h>

Preferences preferences;

void saveServerWorkTime()
{
  if (preferences.begin("server_setting", false))
  {
    preferences.putLong64("server_time", serverWorkTime);
    preferences.end();
  }
}

void loadServerWorkTime()
{
  if (preferences.begin("server_setting", true))
  {
    serverWorkTime = preferences.getLong64("server_time", 0);
    preferences.end();
  }
}
// Загружаем клиентов из Preferences как единый Blob
void loadClientsFromFile()
{
  Serial.println(F("Loading clients from Preferences..."));

  // Открываем пространство имен "devices" в режиме только для чтения
  if (preferences.begin("devices", true))
  {
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
      devices.clear();

      // Получаем размер сохраненного Blob
      size_t blobSize = preferences.getBytesLength("devices_blob");

      if (blobSize > 0)
      {
        // Выделяем буфер для данных
        uint8_t *buffer = new uint8_t[blobSize];

        if (buffer)
        {
          // Читаем данные в буфер
          size_t readSize = preferences.getBytes("devices_blob", buffer, blobSize);

          if (readSize == blobSize)
          {
            // Десериализуем JSON из буфера
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, buffer, blobSize);

            if (!error)
            {
              // Преобразуем JSON в объекты DeviceData
              JsonArray devicesArray = doc.as<JsonArray>();

              for (JsonObject deviceObj : devicesArray)
              {
                DeviceData device;

                // Заполняем поля устройства
                device.name = deviceObj["name"].as<const char *>();
                device.macAddress = deviceObj["mac"].as<const char *>();
                device.targetTemperature = deviceObj["target_temp"].as<float>();
                device.enabled = deviceObj["enabled"].as<bool>();

                // Парсим массив GPIO пинов
                if (!deviceObj["gpio_pins"].isNull())
                {
                  JsonArray pinsArray = deviceObj["gpio_pins"].as<JsonArray>();
                  for (int pin : pinsArray)
                  {
                    device.gpioPins.push_back(pin);
                  }
                }

                // Инициализируем дополнительные поля
                device.heatingStartTime = 0;
                device.totalHeatingTime = deviceObj["total_heating"].as<long>();
                device.isOnline = false;
                device.currentTemperature = device.targetTemperature;
                device.heatingActive = false;
                device.humidity = deviceObj["humidity"].as<float>();
                device.battery = deviceObj["battery"].as<uint8_t>();
                device.batteryV = deviceObj["batteryV"].as<uint16_t>();
                // Добавляем устройство в вектор
                devices.push_back(device);
              }

              Serial.printf("Loaded %d clients from Preferences blob\n", devices.size());
            }
            else
            {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error.c_str());
            }
          }
          else
          {
            Serial.println(F("Failed to read complete blob data"));
          }

          // Освобождаем память
          delete[] buffer;
        }
        else
        {
          Serial.println(F("Failed to allocate memory for blob data"));
        }
      }
      else
      {
        Serial.println(F("No devices data found in Preferences"));
      }

      xSemaphoreGive(devicesMutex);
    }
    else
    {
      Serial.println(F("Failed to take devicesMutex"));
    }

    preferences.end();
  }
  else
  {
    Serial.println(F("Failed to open 'devices' namespace"));
  }
}

// Сохраняем клиентов в Preferences как единый Blob
void saveClientsToFile()
{
  Serial.println("Начинаем сохранение устройств в файл");

  // Открываем пространство имен "devices" в режиме чтения-записи
  if (preferences.begin("devices", false))
  {
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
      // Создаем JSON-документ для сериализации
      JsonDocument doc;
      JsonArray devicesArray = doc.to<JsonArray>();

      // Добавляем каждое устройство в массив
      for (const auto &device : devices)
      {
        JsonObject deviceObj = devicesArray.add<JsonObject>();

        // Сохраняем основные поля
        deviceObj["name"] = device.name;
        deviceObj["mac"] = device.macAddress;
        deviceObj["target_temp"] = device.targetTemperature;
        deviceObj["enabled"] = device.enabled;
        deviceObj["total_heating"] = device.totalHeatingTime;
        deviceObj["humidity"] = device.humidity;
        deviceObj["battery"] = device.battery;
        deviceObj["batteryV"] = device.batteryV;
        // Сохраняем массив GPIO пинов
        for (int pin : device.gpioPins)
        {
          deviceObj["gpio_pins"].add(pin);
        }
      }

      // Определяем размер буфера для сериализации
      size_t bufferSize = measureJson(doc);

      if (bufferSize > 0)
      {
        // Выделяем буфер
        uint8_t *buffer = new uint8_t[bufferSize];

        if (buffer)
        {
          // Сериализуем JSON в буфер
          size_t serializedSize = serializeJson(doc, buffer, bufferSize);

          // Сохраняем буфер в Preferences
          if (preferences.putBytes("devices_blob", buffer, serializedSize))
          {
            Serial.printf("Saved %d clients to Preferences blob (%d bytes)\n",
                          devices.size(), serializedSize);
          }
          else
          {
            Serial.println(F("Failed to save devices blob to Preferences"));
          }

          // Освобождаем память
          delete[] buffer;
        }
        else
        {
          Serial.println(F("Failed to allocate memory for serialization"));
        }
      }
      else
      {
        Serial.println(F("Empty devices collection, nothing to save"));
      }

      xSemaphoreGive(devicesMutex);
    }
    else
    {
      Serial.println(F("Failed to take devicesMutex"));
    }

    preferences.end();
  }
  else
  {
    Serial.println(F("Failed to open 'devices' namespace"));
  }
}

// Загружаем настройки Wifi +++++++++++++++++++++++++++
void loadWifiCredentialsFromFile()
{
  if (preferences.begin("wifi", true)) // true = только чтение
  {
    wifiCredentials.ssid = preferences.getString("ssid", "").c_str();
    wifiCredentials.password = preferences.getString("password", "").c_str();

    preferences.end();

    Serial.println("WiFi-креденциалы загружены из Preferences");
  }
  else
  {
    Serial.println("Нет доступа для чтения данных wifi");
  }
}
// Сохраняем настройки Wifi +++++++++++++++++++++++++++
void saveWifiCredentialsToFile()
{
  if (preferences.begin("wifi", false)) // false = чтение-запись
  {
    preferences.putString("ssid", wifiCredentials.ssid.c_str());
    preferences.putString("password", wifiCredentials.password.c_str());

    preferences.end();

    Serial.println("WiFi-креденциалы сохранены в Preferences");
  }
  else
  {
    Serial.println("Нет доступа для записи данных wifi");
  }
}
// Функция для сохранения GPIO в файл
// Функция для сохранения GPIO в Preferences как Blob
void saveGpioToFile()
{
  Serial.println(F("Saving GPIO to Preferences..."));

  // Открываем пространство имен "gpio" в режиме чтения-записи
  if (preferences.begin("gpio", false))
  {
    // Создаем JSON-документ для сериализации
    JsonDocument doc;
    JsonArray gpioArray = doc.to<JsonArray>();

    // Добавляем каждый GPIO в массив
    for (const auto &gpio : availableGpio)
    {
      JsonObject gpioObj = gpioArray.add<JsonObject>();
      gpioObj["pin"] = gpio.pin;
      gpioObj["name"] = gpio.name;
    }

    // Определяем размер буфера для сериализации
    size_t bufferSize = measureJson(doc);

    if (bufferSize > 0)
    {
      // Выделяем буфер
      uint8_t *buffer = new uint8_t[bufferSize];

      if (buffer)
      {
        // Сериализуем JSON в буфер
        size_t serializedSize = serializeJson(doc, buffer, bufferSize);

        // Сохраняем буфер в Preferences
        if (preferences.putBytes("gpio_blob", buffer, serializedSize))
        {
          Serial.printf("Saved %d GPIO pins to Preferences blob (%d bytes)\n",
                        availableGpio.size(), serializedSize);
        }
        else
        {
          Serial.println(F("Failed to save GPIO blob to Preferences"));
        }

        // Освобождаем память
        delete[] buffer;
      }
      else
      {
        Serial.println(F("Failed to allocate memory for serialization"));
      }
    }
    else
    {
      Serial.println(F("Empty GPIO collection, nothing to save"));
    }

    preferences.end();
  }
  else
  {
    Serial.println(F("Failed to open 'gpio' namespace"));
  }
}

// Функция для загрузки GPIO из Preferences как Blob
void loadGpioFromFile()
{
  Serial.println(F("Loading GPIO from Preferences..."));

  // Открываем пространство имен "gpio" в режиме только для чтения
  if (preferences.begin("gpio", true))
  {
    // Получаем размер сохраненного Blob
    size_t blobSize = preferences.getBytesLength("gpio_blob");

    if (blobSize > 0)
    {
      // Выделяем буфер для данных
      uint8_t *buffer = new uint8_t[blobSize];

      if (buffer)
      {
        // Читаем данные в буфер
        size_t readSize = preferences.getBytes("gpio_blob", buffer, blobSize);

        if (readSize == blobSize)
        {
          // Десериализуем JSON из буфера
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, buffer, blobSize);

          if (!error)
          {
            // Очищаем текущий вектор GPIO
            availableGpio.clear();

            // Преобразуем JSON в объекты GpioPin
            JsonArray gpioArray = doc.as<JsonArray>();

            for (JsonObject gpioObj : gpioArray)
            {
              GpioPin gpio;
              gpio.pin = gpioObj["pin"].as<int>();
              gpio.name = gpioObj["name"].as<const char *>();

              // Добавляем GPIO в вектор
              availableGpio.push_back(gpio);
            }

            Serial.printf("Loaded %d GPIO pins from Preferences blob\n", availableGpio.size());
          }
          else
          {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
          }
        }
        else
        {
          Serial.println(F("Failed to read complete blob data"));
        }

        // Освобождаем память
        delete[] buffer;
      }
      else
      {
        Serial.println(F("Failed to allocate memory for blob data"));
      }
    }
    preferences.end();
  }
}