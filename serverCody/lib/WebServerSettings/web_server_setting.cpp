#include <web_server_setting.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <variables_info.h>
#include <convert_to_json.h>
#include <spiffs_setting.h>
#include "xiaomi_scanner.h"
#include <SPIFFS.h>
// Web Server
AsyncWebServer server(80);

// Connect to WiFi +++++++++++++++++++++++++++++++++++
void connectWiFi()
{
    Serial.print(F("Connecting to WiFi: "));
    Serial.println(F(wifiCredentials.ssid.c_str()));

    WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
    lastWiFiAttemptTime = millis();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        delay(1000);
        Serial.print(F("..."));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(F(""));
        Serial.println(F("WiFi connected"));
        Serial.print(F("IP address: "));
        Serial.println(WiFi.localIP());
        wifiConnected = true;
    }
    else
    {
        Serial.println(F(""));
        Serial.println(F("Failed to connect to WiFi"));
        wifiConnected = false;
    }
}
// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{

    // Добавляем новый обработчик для получения токена устройства
    server.on("/device_token", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                String token = loadDeviceToken();
                request->send(200, "text/plain", token); });

    // Добавляем обработчик для сканирования устройств Mi Home
    server.on("/scan_mihome", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    // Обновляем список эмулируемых устройств
                updateEmulatedDevices();
                request->send(200, "text/plain", "Сканирование устройств Mi Home запущено"); });

    // Добавляем обработчик для получения статуса интеграции с Mi Home
    server.on("/mihome_status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                JsonDocument doc;
                doc["enabled"] = true;
                doc["device_token"] = loadDeviceToken();
                doc["device_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
                doc["ip_address"] = WiFi.localIP().toString();
                doc["device_count"] = devices.size();
                
                String response;
                serializeJson(doc, response);
                request->send(200, "application/json", response); });
// Добавляем обработчик для страницы настройки Mi Home
    server.on("/mihome", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/mihome.html", "text/html");
    });
    // GET /clients (get list of all clients)
    server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
              {
            std::string jsonVal = "[";

            if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
                for (size_t i = 0; i < devices.size(); ++i) {
                    jsonVal += toJson(devices[i]);
                    if (i != devices.size() - 1) {
                        jsonVal += ",";
                    }
                }
                xSemaphoreGive(devicesMutex);
            }
            jsonVal += "]";
            request->send(200, "application/json", jsonVal.c_str()); });

    server.on("/availablegpio", HTTP_GET, [](AsyncWebServerRequest *request)
              {
          std::string jsonVal = "[";
          for (size_t i = 0; i < availableGpio.size(); ++i) {
              jsonVal += "{\"pin\":" + std::to_string(availableGpio[i].pin) + 
                        ",\"name\":\"" + availableGpio[i].name + "\"}";
              if (i != availableGpio.size() - 1) {
                  jsonVal += ",";
              }
          }
          jsonVal += "]";
          request->send(200, "application/json", jsonVal.c_str()); });
    server.on("/availablegpio", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("availablegpio", true))
        {
            std::string json = request->getParam("availablegpio", true)->value().c_str();
            availableGpio = parseGpioPinsWithNames(json);
            saveGpioToFile();
            request->send(200, "text/plain", "availablegpio update");
        }
        request->send(400, "text/plain", "availablegpio no found"); });
    // POST /client/{address} (update info about a client)
    server.on("/client", HTTP_POST, [](AsyncWebServerRequest *request)
              {
            if (request->hasParam("address", true))
            {
                String address = request->getParam("address", true)->value();
                bool isSaving = false;
                if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
                // Handle POST parameters
                if (request->hasParam("name", true))
                {
                    String newName = request->getParam("name", true)->value();
                    // Find the client by address and update the name
                    for (auto& client : devices)
                    {
                        if (client.macAddress == address.c_str())
                        {
                            client.name = newName.c_str();
                            isSaving = true;
                            break;
                        }
                    }
                }

                if (request->hasParam("targetTemperature", true))
                {
                    String tempStr = request->getParam("targetTemperature", true)->value();
                    float newTargetTemperature = tempStr.toFloat();
                    // Find the client by address and update the target temperature
                    for (auto& client : devices)
                    {
                        if (client.macAddress == address.c_str())
                        {
                            client.targetTemperature = newTargetTemperature;
                            isSaving = true;
                            break;
                        }
                    }
                }

                if (request->hasParam("enabled", true))
                {
                    String tempStr = request->getParam("enabled", true)->value();
                    bool enabled = tempStr == "true";
                    // Find the client by address and update the target temperature
                    for (auto& client : devices)
                    {
                        if (client.macAddress == address.c_str())
                        {
                            client.enabled = enabled;
                            isSaving = true;
                            break;
                        }
                    }
                }

                if (request->hasParam("gpioPins", true))
                {
                    String newName = request->getParam("gpioPins", true)->value();
                    // Find the client by address and update the name
                    for (auto& client : devices)
                    {
                        if (client.macAddress == address.c_str())
                        {
                            // Извлекаем только номера пинов для совместимости
                            client.gpioPins = parseGpioPins(newName.c_str());
                            isSaving = true;
                            break;
                        }
                    }
                }
                xSemaphoreGive(devicesMutex);
                if (isSaving)
                {
                    saveClientsToFile(); // Save changes to file
                    // Send response
                    request->send(200, "text/plain", "Client updated");
                }
            }
            }
            request->send(200, "text/plain", "Client not find"); });

    // GET /scan (start BLE scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                startXiaomiScan();
            request->send(200, "text/plain", "BLE Scan started"); });
    // Добавьте этот код в функцию initWebServer() в файле web_server_setting.cpp

    // Обработчик для корневого пути и /index
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

    server.on("/index", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

    server.begin();
    Serial.println(F("Web server started"));
}
