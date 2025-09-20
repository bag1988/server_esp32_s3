#include <web_server_setting.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <variables_info.h>
#include <spiffs_setting.h>
#include "xiaomi_scanner.h"
#include <SPIFFS.h>
#include "logger.h"

// Web Server
AsyncWebServer server(80);

// Создаем экземпляр AsyncEventSource
AsyncEventSource events("/events");

void connectWiFi()
{
    LOG_I("Connecting to WiFi: %s", wifiCredentials.ssid.c_str());
    LOG_I(", password: %s", wifiCredentials.password.c_str());
    WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
    lastWiFiAttemptTime = millis();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        Serial.print("...");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        LOG_I("");
        LOG_I("WiFi connected");
        LOG_I("IP address: %s", WiFi.localIP().toString().c_str());
        wifiConnected = true;
    }
    else
    {
        LOG_I("");
        LOG_I("Failed to connect to WiFi");
        wifiConnected = false;
    }
}
// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{
    // Добавляем обработчик событий
    server.addHandler(&events);
    // GET /clients (get list of all clients)
    server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                JsonDocument doc;
                JsonArray devicesArray = doc.to<JsonArray>();

                if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
                    for (const auto& device : devices) {
                        JsonObject deviceObj = devicesArray.add<JsonObject>();
                        
                        // Заполняем основные поля устройства
                        deviceObj["name"] = device.name;
                        deviceObj["macAddress"] = device.macAddress;
                        deviceObj["currentTemperature"] = device.currentTemperature;
                        deviceObj["targetTemperature"] = device.targetTemperature;
                        deviceObj["enabled"] = device.enabled;
                        deviceObj["isOnline"] = device.isOnline;
                        deviceObj["heatingActive"] = device.heatingActive;
                        deviceObj["humidity"] = device.humidity;
                        deviceObj["battery"] = device.battery;
                        deviceObj["batteryV"] = device.batteryV;
                        deviceObj["lastUpdate"] = device.lastUpdate;
                        deviceObj["totalHeatingTime"] = device.totalHeatingTime;
                        
                        // Добавляем массив GPIO пинов
                        JsonArray pinsArray = deviceObj["gpioPins"].to<JsonArray>();
                        for (int pin : device.gpioPins) {
                            pinsArray.add(pin);
                        }
                    }
                    xSemaphoreGive(devicesMutex);
                }
                
                String response;
                serializeJson(doc, response);
                request->send(200, "application/json", response.c_str()); });

    server.on("/availablegpio", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  JsonDocument doc;
                  JsonArray gpioArray = doc.to<JsonArray>();
                  
                  for (const auto& gpio : availableGpio) {
                      JsonObject gpioObj = gpioArray.add<JsonObject>();
                      gpioObj["pin"] = gpio.pin;
                      gpioObj["name"] = gpio.name;
                  }
                  
                  String response;
                  serializeJson(doc, response);
                  request->send(200, "application/json", response.c_str()); });
    server.on("/availablegpio", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                    if (request->hasParam("availablegpio", true))
                    {
                        String jsonStr = request->getParam("availablegpio", true)->value();
                        
                        // Используем ArduinoJson 7.x для парсинга
                        JsonDocument doc;
                        DeserializationError error = deserializeJson(doc, jsonStr);
                        
                        if (!error) {
                            // Очищаем текущий вектор GPIO
                            availableGpio.clear();                            
                            // Парсим массив GPIO пинов
                            JsonArray gpioArray = doc.as<JsonArray>();
                            for (JsonObject gpioObj : gpioArray) {
                                GpioPin gpio;
                                gpio.pin = gpioObj["pin"].as<int>();
                                gpio.name = gpioObj["name"].as<const char*>();
                                availableGpio.push_back(gpio);
                            }
                            
                            saveGpioToFile();
                            request->send(200, "text/plain", "availablegpio updated");
                        } else {
                            request->send(400, "text/plain", "Invalid JSON format");
                        }
                    } else {
                        request->send(400, "text/plain", "availablegpio parameter not found");
                    } });
    server.on("/serverinfo", HTTP_GET, [](AsyncWebServerRequest *request)
              {

                unsigned long totalSeconds = serverWorkTime / 1000;
                unsigned long hours = totalSeconds / 3600;           // Получаем количество полных часов
                unsigned long minutes = (totalSeconds % 3600) / 60;  // Получаем остаток минут после часов
                unsigned long seconds = totalSeconds % 60;           // Получаем остаток секунд
                char buffer[20];
                sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
                JsonDocument doc;
                doc["cpu_frequency_mhz"] = ESP.getCpuFreqMHz();//Частота CPU
                doc["chip_revision"] = ESP.getChipRevision();//Ревизия чипа
                doc["processor_cores"] = ESP.getChipCores();//Ядер процессора
                doc["sdk_version"] = ESP.getSdkVersion();//Версия SDK
                doc["sram_size_bytes"] = ESP.getHeapSize();//Размер SRAM
                doc["free_sram_bytes"] = ESP.getFreeHeap();//Свободная SRAM
                doc["flash_size_bytes"] = ESP.getFlashChipSize();//Размер Flash
                doc["flash_frequency_mhz"] = ESP.getFlashChipSpeed() / 1000000;//Частота Flash
                doc["psram_size_bytes"] = ESP.getPsramSize();//Размер PSRAM
                doc["free_psram_bytes"] = ESP.getFreePsram();//Свободная PSRAM
                doc["flash_mode"] = ESP.getFlashChipMode() == FM_QIO ? "QIO" : "DIO";//Flash режим
                doc["chip_id"] = ESP.getEfuseMac();//Уникальный ID чипа
                doc["millis"] = String(buffer);
                doc["board_temperature"] = board_temperature;
                // Сериализуем JSON
                String payload;
                serializeJson(doc, payload);
                request->send(200, "application/json", payload.c_str()); });
    // POST /client/{address} (update info about a client)
    server.on("/client", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                if (request->hasParam("address", true))
                {
                    String address = request->getParam("address", true)->value();
                    bool isSaving = false;
                    
                    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
                        // Находим устройство по адресу
                        auto deviceIt = std::find_if(devices.begin(), devices.end(),
                                                [&address](const DeviceData &device) {
                                                    return device.macAddress == address.c_str();
                                                });
                        
                        if (deviceIt != devices.end()) {
                            // Обновляем имя устройства
                            if (request->hasParam("name", true)) {
                                String newName = request->getParam("name", true)->value();
                                deviceIt->name = newName.c_str();
                                isSaving = true;
                            }
                            
                            // Обновляем целевую температуру
                            if (request->hasParam("targetTemperature", true)) {
                                String tempStr = request->getParam("targetTemperature", true)->value();
                                float newTargetTemperature = tempStr.toFloat();
                                deviceIt->targetTemperature = newTargetTemperature;
                                isSaving = true;
                            }
                            
                            // Обновляем статус включения
                            if (request->hasParam("enabled", true)) {
                                String tempStr = request->getParam("enabled", true)->value();
                                bool enabled = tempStr == "true";
                                deviceIt->enabled = enabled;
                                isSaving = true;
                            }
                            
                            // Обновляем GPIO пины
                            if (request->hasParam("gpioPins", true)) {
                                String gpioStr = request->getParam("gpioPins", true)->value();
                                
                                // Парсим JSON с использованием ArduinoJson 7.x
                                JsonDocument doc;
                                DeserializationError error = deserializeJson(doc, gpioStr);
                                
                                if (!error) {
                                    // Очищаем текущий вектор GPIO пинов
                                    deviceIt->gpioPins.clear();
                                    
                                    // Если входные данные - массив
                                    if (doc.is<JsonArray>()) {
                                        JsonArray pinsArray = doc.as<JsonArray>();
                                        for (int pin : pinsArray) {
                                            deviceIt->gpioPins.push_back(pin);
                                        }
                                    }                                     
                                    
                                    isSaving = true;
                                }
                            }
                            
                        }
                        
                        xSemaphoreGive(devicesMutex);
                        
                        if (isSaving) {
                            LOG_I("Получены изменения по HTTP, сохраняем результаты");
                            saveClientsToFile(); // Save changes to file
                            request->send(200, "text/plain", "Client updated");
                            return;
                        }
                    }
                }                
                request->send(404, "text/plain", "Client not found"); });

    // GET /scan (start BLE scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                LOG_I("Получен запрос на запуск сканирования устройств");
                startXiaomiScan();
            request->send(200, "text/plain", "BLE Scan started"); });
    // Добавляем обработчик для получения статистики обогрева
    server.on("/heating_stats", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    JsonDocument doc;
    JsonArray statsArray = doc.to<JsonArray>();
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto& device : devices) {
            JsonObject deviceObj = statsArray.add<JsonObject>();
            deviceObj["name"] = device.name;
            deviceObj["macAddress"] = device.macAddress;
            deviceObj["currentTemperature"] = device.currentTemperature;
            deviceObj["targetTemperature"] = device.targetTemperature;
            deviceObj["heatingActive"] = device.heatingActive;
                       
            // Форматируем время для вывода
            unsigned long totalSeconds =  device.totalHeatingTime / 1000;
            unsigned long hours = totalSeconds / 3600;
            unsigned long minutes = (totalSeconds % 3600) / 60;
            unsigned long seconds = totalSeconds % 60;
            
            deviceObj["totalHeatingTimeMs"] =  device.totalHeatingTime;
            deviceObj["totalHeatingTimeFormatted"] =  String(hours) + ":" + String(minutes % 60) + ":" + String(seconds % 60);
        }
        xSemaphoreGive(devicesMutex);
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

    // Добавляем обработчик для сброса статистики обогрева
    server.on("/reset_stats", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        bool resetAll = true;
        String deviceMac = "";
        
        // Проверяем, нужно ли сбросить статистику для конкретного устройства
        if (request->hasParam("device", true)) {
            deviceMac = request->getParam("device", true)->value();
            resetAll = false;
        }
        
        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
            for (auto& device : devices) {
                if (resetAll || device.macAddress == deviceMac.c_str()) {
                    device.totalHeatingTime = 0;
                    if (device.heatingActive) {
                        // Если обогрев активен, сбрасываем время начала
                        device.heatingStartTime = millis();
                    }
                }
            }
            xSemaphoreGive(devicesMutex);
        }        
        LOG_I("Сброшена статистика, сохраняем результаты");
        // Сохраняем изменения
        saveClientsToFile();    
        serverWorkTime = 0;
        saveServerWorkTime();
        request->send(200, "text/plain", "Статистика сброшена"); });

    // Обработчик для корневого пути и /index
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });

    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html"); });
    server.on("/managment_device.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/managment_device.html", "text/html"); });
    server.on("/app.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/app.css", "text/css"); });
    server.on("/gpio_settings.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/gpio_settings.html", "text/html"); });
    // Добавляем обработчик для страницы статистики обогрева
    server.on("/heating_stats.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/heating_stats.html", "text/html"); });

    // Добавляем обработчик для получения логов в реальном времени через SSE
    server.on("/logs.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/logs.html", "text/html"); });

    server.begin();
    LOG_I("Web server started");
}
