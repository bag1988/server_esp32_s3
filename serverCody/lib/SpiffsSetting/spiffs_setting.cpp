#include <spiffs_setting.h>
#include <variables_info.h>
#include <convert_to_json.h>
#include <SPIFFS.h>
#include <Preferences.h>

Preferences preferences;

// Загружаем клиентов из JSON ++++++++++++++++++++++++++++++
void loadClientsFromFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Loading clients from file..."));
    File file = SPIFFS.open(DEVICES_FILE.c_str(), "r");
    if (!file || file.size() == 0)
    {
      Serial.println(F("Clients file doesn't exist or is empty"));
      file.close(); // Ensure file is closed before returning
      return;
    }
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
      devices.clear();
      while (file.available())
      {
        std::string json;
        while (file.available())
        {
          char c = file.read();
          json += c;
          if (c == '}')
            break;
        }
        auto d = fromJson(json);
        d.heatingStartTime = 0;
        d.totalHeatingTime = 0;
        d.isOnline = false;
        d.currentTemperature = d.targetTemperature;
        d.heatingActive = false;
        devices.push_back(d);
      }
      file.close();
      Serial.println(F("Clients loaded from file"));
      xSemaphoreGive(devicesMutex);
    }
  }
}
// Сохраняме клиентов в JSON +++++++++++++++++++++++++++++
void saveClientsToFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Saving clients to file..."));
    File file = SPIFFS.open(DEVICES_FILE.c_str(), "w");
    if (!file)
    {
      Serial.println(F("Failed to open clients file for writing"));
      return;
    }

    file.print("[\n");
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
    {
      for (size_t i = 0; i < devices.size(); ++i)
      {
        file.print(toJson(devices[i]).c_str());
        if (i != devices.size() - 1)
        {
          file.print(",\n");
        }
      }
      file.print("\n]");
      file.close();
      Serial.println(F("Clients saved to file"));
      xSemaphoreGive(devicesMutex);
    }
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
void saveGpioToFile()
{
  File file = SPIFFS.open("/gpio.json", FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open gpio.json for writing");
    return;
  }

  String json = "[";
  for (size_t i = 0; i < availableGpio.size(); i++)
  {
    json += "{\"pin\":" + String(availableGpio[i].pin) +
            ",\"name\":\"" + availableGpio[i].name.c_str() + "\"}";
    if (i < availableGpio.size() - 1)
    {
      json += ",";
    }
  }
  json += "]";

  file.println(json);
  file.close();
}

// Функция для загрузки GPIO из файла
void loadGpioFromFile()
{
  if (SPIFFS.exists("/gpio.json"))
  {
    File file = SPIFFS.open("/gpio.json", FILE_READ);
    if (!file)
    {
      Serial.println("Failed to open gpio.json for reading");
      return;
    }

    String json = file.readString();
    file.close();

    availableGpio = parseGpioPinsWithNames(json.c_str());
  }
}

// Добавляем функции для работы с токеном устройства

// Сохранение токена устройства
// Сохранение токена устройства
void saveDeviceToken(const char *token)
{
  if (preferences.begin("tokens", false)) // false = чтение-запись
  {
    preferences.putString("device_token", token);
    preferences.end();
    Serial.println("Токен устройства сохранен в Preferences");
  }
  else
  {
    Serial.println("Ошибка при сохранении токена устройства");

    // Для обратной совместимости также сохраняем в SPIFFS
    File file = SPIFFS.open("/device_token.txt", FILE_WRITE);
    if (file)
    {
      file.print(token);
      file.close();
      Serial.println("Токен устройства сохранен в SPIFFS (для обратной совместимости)");
    }
  }
}

// Загрузка токена устройства
String loadDeviceToken()
{
  String token = "";

  // Пытаемся загрузить из Preferences
  if (preferences.begin("tokens", true)) // true = только чтение
  {
    token = preferences.getString("device_token", "");
    preferences.end();

    // Если токен найден, возвращаем его
    if (token.length() == 32)
    {
      Serial.println("Загружен токен устройства из Preferences: " + token);
      return token;
    }
  }

  // Если токен не найден или некорректен, генерируем новый
  char newToken[33];
  for (int i = 0; i < 32; i++)
  {
    newToken[i] = "0123456789abcdef"[random(0, 16)];
  }
  newToken[32] = '\0';

  // Сохраняем новый токен
  saveDeviceToken(newToken);

  Serial.println("Сгенерирован новый токен устройства: " + String(newToken));
  return String(newToken);
}