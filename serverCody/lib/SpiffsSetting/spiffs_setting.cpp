#include <spiffs_setting.h>
#include <variables_info.h>
#include <convert_to_json.h>
#include <SPIFFS.h>

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
        devices.push_back(fromJson(json));
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
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Loading WiFi credentials from file..."));
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE.c_str(), "r");
    if (!file || file.size() == 0)
    {
      Serial.println(F("WiFi credentials file doesn't exist or is empty. Using default configuration."));
      wifiCredentials.ssid = "";
      wifiCredentials.password = "";
      file.close(); // Ensure file is closed
      return;
    }

    wifiCredentials = fromJsonWifi(file.readString().c_str());

    file.close();
    Serial.println(F("WiFi credentials loaded from file"));
  }
}
// Сохраняем настройки Wifi +++++++++++++++++++++++++++
void saveWifiCredentialsToFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Saving WiFi credentials to file..."));
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE.c_str(), "w");
    if (!file)
    {
      Serial.println(F("Failed to open WiFi credentials file for writing"));
      return;
    }
    file.print(toJsonWifi(wifiCredentials).c_str());

    file.close();
    Serial.println(F("WiFi credentials saved to file"));
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
void saveDeviceToken(const char* token) {
  File file = SPIFFS.open("/device_token.txt", FILE_WRITE);
  if (!file) {
      Serial.println("Ошибка при открытии файла для записи токена");
      return;
  }
  
  file.print(token);
  file.close();
  Serial.println("Токен устройства сохранен");
}

// Загрузка токена устройства
String loadDeviceToken() {
  if (!SPIFFS.exists("/device_token.txt")) {
      // Файл не существует, генерируем новый токен
      char token[33];
      for (int i = 0; i < 32; i++) {
          token[i] = "0123456789abcdef"[random(0, 16)];
      }
      token[32] = '\0';
      
      // Сохраняем новый токен
      saveDeviceToken(token);
      
      return String(token);
  }
  
  File file = SPIFFS.open("/device_token.txt", FILE_READ);
  if (!file) {
      Serial.println("Ошибка при открытии файла для чтения токена");
      return "00000000000000000000000000000000"; // Токен по умолчанию
  }
  
  String token = file.readString();
  file.close();
  
  // Проверяем, что токен имеет правильную длину
  if (token.length() != 32) {
      Serial.println("Некорректный токен в файле, генерируем новый");
      
      char newToken[33];
      for (int i = 0; i < 32; i++) {
          newToken[i] = "0123456789abcdef"[random(0, 16)];
      }
      newToken[32] = '\0';
      
      // Сохраняем новый токен
      saveDeviceToken(newToken);
      
      return String(newToken);
  }
  
  Serial.println("Загружен токен устройства: " + token);
  return token;
}
