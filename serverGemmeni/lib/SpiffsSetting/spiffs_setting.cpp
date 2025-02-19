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
    File file = SPIFFS.open(CLIENTS_FILE, "r");
    if (!file || file.size() == 0)
    {
      Serial.println(F("Clients file doesn't exist or is empty"));
      file.close(); // Ensure file is closed before returning
      return;
    }
    clients.clear();
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
      clients.push_back(fromJson(json));
    }
    file.close();
    Serial.println(F("Clients loaded from file"));
  }
}
// Сохраняме клиентов в JSON +++++++++++++++++++++++++++++
void saveClientsToFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Saving clients to file..."));
    File file = SPIFFS.open(CLIENTS_FILE, "w");
    if (!file)
    {
      Serial.println(F("Failed to open clients file for writing"));
      return;
    }

    file.print("[\n");
    for (size_t i = 0; i < clients.size(); ++i)
    {
      file.print(toJson(clients[i]).c_str());
      if (i != clients.size() - 1)
      {
        file.print(",\n");
      }
    }
    file.print("\n]");
    file.close();
    Serial.println(F("Clients saved to file"));
  }
}
// Загружаем настройки Wifi +++++++++++++++++++++++++++
void loadWifiCredentialsFromFile()
{
  if (SPIFFS.begin(true))
  {
    Serial.println(F("Loading WiFi credentials from file..."));
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "r");
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
    File file = SPIFFS.open(WIFI_CREDENTIALS_FILE, "w");
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
