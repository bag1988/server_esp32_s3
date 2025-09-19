#include <spiffs_setting.h>
#include "logger.h"
Preferences preferences;

void saveServerWorkTime()
{
  if (preferences.begin("server_setting", false))
  {
    preferences.putLong64("server_time", serverWorkTime);
  }
}

void loadServerWorkTime()
{
  if (preferences.begin("server_setting", false))
  {
    serverWorkTime = preferences.getLong64("server_time", 0);
  }
}
// Загружаем клиентов из Preferences как единый Blob
void loadClientsFromFile()
{
  LOG_I("Loading clients from Preferences...");

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
                device.battery = deviceObj["battery"].as<int>();

                // Добавляем устройство в вектор
                devices.push_back(device);
              }

              LOG_I("Loaded %d clients from Preferences blob", devices.size());
            }
            else
            {
              LOG_I("deserializeJson() failed: %s", error.c_str());
            }
          }
          else
          {
            LOG_I("Failed to read complete blob data");
          }

          // Освобождаем память
          delete[] buffer;
        }
        else
        {
          LOG_I("Failed to allocate memory for blob data");
        }
      }
      else
      {
        LOG_I("No devices data found in Preferences");
      }

      xSemaphoreGive(devicesMutex);
    }
    else
    {
      LOG_I("Failed to take devicesMutex");
    }

    preferences.end();
  }
  else
  {
    LOG_I("Failed to open 'devices' namespace");
  }
}

// Сохраняем клиентов в Preferences как единый Blob
void saveClientsToFile()
{
  LOG_I("Начинаем сохранение устройств в файл");

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
            LOG_I("Saved %d clients to Preferences blob (%d bytes)",
                          devices.size(), serializedSize);
          }
          else
          {
            LOG_I("Failed to save devices blob to Preferences");
          }

          // Освобождаем память
          delete[] buffer;
        }
        else
        {
          LOG_I("Failed to allocate memory for serialization");
        }
      }
      else
      {
        LOG_I("Empty devices collection, nothing to save");
      }

      xSemaphoreGive(devicesMutex);
    }
    else
    {
      LOG_I("Failed to take devicesMutex");
    }

    preferences.end();
  }
  else
  {
    LOG_I("Failed to open 'devices' namespace");
  }
}

// Загружаем настройки Wifi +++++++++++++++++++++++++++
void loadWifiCredentialsFromFile()
{
  if (preferences.begin("wifi", true)) // true = только чтение
  {
    preferences.begin("wifi", true); // true = только чтение

    wifiCredentials.ssid = preferences.getString("ssid", "Bag").c_str();
    wifiCredentials.password = preferences.getString("password", "01123581321").c_str();

    preferences.end();

    LOG_I("WiFi-креденциалы загружены из Preferences");
  }
  else
  {
    LOG_I("Нет доступа для чтения данных wifi");
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

    LOG_I("WiFi-креденциалы сохранены в Preferences");
  }
  else
  {
    LOG_I("Нет доступа для записи данных wifi");
  }
}
// Функция для сохранения GPIO в файл
// Функция для сохранения GPIO в Preferences как Blob
void saveGpioToFile()
{
  LOG_I("Saving GPIO to Preferences...");

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
          LOG_I("Saved %d GPIO pins to Preferences blob (%d bytes)",
                        availableGpio.size(), serializedSize);
        }
        else
        {
          LOG_I("Failed to save GPIO blob to Preferences");
        }

        // Освобождаем память
        delete[] buffer;
      }
      else
      {
        LOG_I("Failed to allocate memory for serialization");
      }
    }
    else
    {
      LOG_I("Empty GPIO collection, nothing to save");
    }

    preferences.end();
  }
  else
  {
    LOG_I("Failed to open 'gpio' namespace");
  }
}

// Функция для загрузки GPIO из Preferences как Blob
void loadGpioFromFile()
{
  LOG_I("Loading GPIO from Preferences...");

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

            LOG_I("Loaded %d GPIO pins from Preferences blob", availableGpio.size());
          }
          else
          {
            LOG_I("deserializeJson() failed: %s",error.c_str());
          }
        }
        else
        {
          LOG_I("Failed to read complete blob data");
        }

        // Освобождаем память
        delete[] buffer;
      }
      else
      {
        LOG_I("Failed to allocate memory for blob data");
      }
    }
  }
}