#include <web_server_setting.h>
#include <ble_setting.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <variables_info.h>
#include <convert_to_json.h>
#include <spiffs_setting.h>
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
        if (pServer != nullptr)
        {
            Serial.println(F("Restarting BLE Advertising..."));
            pServer->startAdvertising();
        }
        else
        {
            Serial.println(F("BLE server not initialized"));
        }
    }
}
// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{
    // GET /clients (get list of all clients)
    server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
              {
            std::string jsonVal = "[";

            for (size_t i = 0; i < clients.size(); ++i) {
                jsonVal += toJson(clients[i]);
                if (i != clients.size() - 1) {
                    jsonVal += ",";
                }
            }
            jsonVal += "]";
            request->send(200, "application/json", jsonVal.c_str()); });

    server.on("/availablegpio", HTTP_GET, [](AsyncWebServerRequest *request)
              {
            std::string jsonVal = "[";
            for (size_t i = 0; i < availableGpio.size(); ++i) {
                jsonVal += std::to_string(availableGpio[i]);
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
                size_t start = json.find("[") + 1;
                size_t end = json.find("]", start);
                std::string gpioPinsStr = json.substr(start, end - start);
                availableGpio = parseGpioPins(gpioPinsStr);
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
                // Handle POST parameters
                if (request->hasParam("name", true))
                {
                    String newName = request->getParam("name", true)->value();
                    // Find the client by address and update the name
                    for (auto& client : clients)
                    {
                        if (client.address == address.c_str())
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
                    for (auto& client : clients)
                    {
                        if (client.address == address.c_str())
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
                    for (auto& client : clients)
                    {
                        if (client.address == address.c_str())
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
                    for (auto& client : clients)
                    {
                        if (client.address == address.c_str())
                        {
                            client.gpioPins = parseGpioPins(newName.c_str());
                            isSaving = true;
                            break;
                        }
                    }
                }
                if (isSaving)
                {
                    saveClientsToFile(); // Save changes to file
                    // Send response
                    request->send(200, "text/plain", "Client updated");
                }

            }
            request->send(200, "text/plain", "Client not find"); });

    // GET /scan (start BLE scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
              {
            startBLEScan();
            request->send(200, "text/plain", "BLE Scan started"); });

    server.begin();
    Serial.println(F("Web server started"));
}

