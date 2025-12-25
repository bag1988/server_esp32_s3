#include <web_server_setting.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <variables_info.h>
#include <spiffs_setting.h>
#include "xiaomi_scanner.h"
#include <SPIFFS.h>

// Web Server
AsyncWebServer server(80);

// Создаем экземпляр AsyncEventSource
AsyncEventSource serialEvents("/log_events");

// Task handle for serial streaming task
TaskHandle_t serialStreamingTaskHandle = NULL;

// Flag to track if clients are connected
volatile bool serialStreamClientsConnected = false;

void connectWiFi()
{
    Serial.println("Connecting to WiFi: " + String(wifiCredentials.ssid.c_str()) + ", password: " + String(wifiCredentials.password.c_str()));
    WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
    lastWiFiAttemptTime = millis();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected. Ip: " + String(WiFi.localIP().toString().c_str()));
        wifiConnected = true;
    }
    else
    {
        Serial.println("Failed to connect to WiFi");
        wifiConnected = false;
    }
}

// Function to read serial data
String readSerialData()
{
    String data = "";
    while (Serial.available())
    {
        data += (char)Serial.read();
    }
    return data;
}

// Task for continuous serial data streaming - only runs when clients are connected
void serialStreamingTask(void *parameter)
{
    while (serialStreamClientsConnected)
    {
        // Check if there's serial data available and send it to connected clients
        if (Serial.available())
        {
            String serialData = "";
            while (Serial.available())
            {
                serialData += (char)Serial.read();
            }
            // Send data to all connected SSE clients
            serialEvents.send(serialData.c_str(), "serial", millis());
        }
        // Small delay to prevent excessive CPU usage when no data is available
        vTaskDelay(50 / portTICK_PERIOD_MS); // Check every 50ms
    }
    // Delete the task when no longer needed
    vTaskDelete(NULL);
}

// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{
    // Add event source handler
    server.addHandler(&serialEvents);

    // Event source connection handlers
    serialEvents.onConnect([](AsyncEventSourceClient *client)
                           {
        // Send initial data when client connects
        String initialData = readSerialData();
        if(initialData.length() > 0) {
            client->send(initialData.c_str(), "serial", millis(), 1000);
        }
        // Set flag that clients are connected
        serialStreamClientsConnected = true;
        // Start the serial streaming task if not already running
        if (serialStreamingTaskHandle == NULL) {
            xTaskCreate(serialStreamingTask, "serialStreamingTask", 4096, NULL, 1, &serialStreamingTaskHandle);
        } });

    serialEvents.onDisconnect([](void *arg, AsyncEventSourceClient *client)
                              {
        // Check if there are still connected clients
        if (serialEvents.count() == 0) {
            // No more clients connected, set flag to false
            serialStreamClientsConnected = false;
            // Give some time for the task to detect the flag change and finish
            if (serialStreamingTaskHandle != NULL) {
                // Wait a bit for the task to finish naturally
                vTaskDelay(100 / portTICK_PERIOD_MS);
                // Reset task handle
                serialStreamingTaskHandle = NULL;
            }
        } });

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
                        for (uint8_t pin : device.gpioPins) {
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
                      gpioObj["state"] = gpio.state;
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
                                gpio.pin = gpioObj["pin"].as<uint8_t>();
                                gpio.state = gpioObj["state"].as<uint8_t>();
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
                doc["millis"] = formatHeatingTime(serverWorkTime);
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
                                        for (uint8_t pin : pinsArray) {
                                            deviceIt->gpioPins.push_back(pin);
                                        }
                                    }                                     
                                    
                                    isSaving = true;
                                }
                            }
                            
                        }
                        
                        xSemaphoreGive(devicesMutex);
                        
                        if (isSaving) {
                            Serial.println("Получены изменения по HTTP, сохраняем результаты");
                            saveClientsToFile(); // Save changes to file
                            request->send(200, "text/plain", "Client updated");
                            return;
                        }
                    }
                }                
                request->send(404, "text/plain", "Client not found"); });
    server.on("/client", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {
                if (request->hasParam("address", true))
                {
                    String address = request->getParam("address", true)->value();
                   
                    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
                        // Находим устройство по адресу
                        devices.erase(
                                std::remove_if(devices.begin(), devices.end(),
                                [&](const DeviceData& d) {
                                    return d.macAddress == address.c_str();
                                }),
                                devices.end()
                            );                                                                      
                        xSemaphoreGive(devicesMutex);
                        
                        Serial.println("Удаляем устройство "+ address);
                            saveClientsToFile(); // Save changes to file
                            request->send(200, "text/plain", "Client remove");
                            return;
                    }
                }                
                request->send(404, "text/plain", "Client not found"); });
    // GET /scan (start BLE scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                Serial.println("Получен запрос на запуск сканирования устройств");
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
            deviceObj["totalHeatingTimeMs"] =  device.totalHeatingTime;
            deviceObj["totalHeatingTimeFormatted"] = formatHeatingTime(device.totalHeatingTime);
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
        Serial.println("Сброшена статистика, сохраняем результаты");
        // Сохраняем изменения
        saveClientsToFile();
        request->send(200, "text/plain", "Статистика сброшена"); });

    server.on("/heating_gpio_stats", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    JsonDocument doc;
    JsonArray statsArray = doc.to<JsonArray>();
    for (const auto& gpio : availableGpio) {
            JsonObject gpioObj = statsArray.add<JsonObject>();
            gpioObj["pin"] = gpio.pin;  
            gpioObj["state"] = gpio.state;  
            gpioObj["name"] = gpio.name;            
            gpioObj["totalHeatingTimeFormatted"] = formatHeatingTime(gpio.totalHeatingTime);
        }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

    server.on("/reset_gpio_stats", HTTP_POST, [](AsyncWebServerRequest *request)
              {                
                for (auto &gpio : availableGpio)
                    {
                    gpio.totalHeatingTime = 0;
                    }
        Serial.println("Сброшена статистика, сохраняем результаты");
        // Сохраняем изменения
        saveGpioToFile();
        request->send(200, "text/plain", "Статистика сброшена"); });

    server.on("/reset_work_time", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {       
        Serial.println("Сброшено время работы, сохраняем результаты");        
        serverWorkTime = 0;
        saveServerSetting();
        request->send(200, "text/plain", "Статистика сброшена"); });

    server.on("/save_hysteresis_temp", HTTP_POST, [](AsyncWebServerRequest *request)
              {       
                if (request->hasParam("hysteresis_temp", true)) {
                    hysteresisTemp = request->getParam("hysteresis_temp", true)->value().toFloat();
                    Serial.println("Cохраняем настройки для гистерезиса");  
                    saveServerSetting();
                    request->send(200, "text/plain", "Настройки для гистерезиса сохранены"); 
                }
     request->send(404, "text/plain", "Param not found"); });

    server.on("/get_hysteresis_temp", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", String(hysteresisTemp, 1)); });

    // New endpoint for real-time serial data streaming using Server-Sent Events
    server.on("/serial_stream", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                if(request->header("Accept") != "text/event-stream") {
                    request->send(400, "text/plain", "This endpoint is for Server-Sent Events only");
                    return;
                }
                
                AsyncResponseStream *response = request->beginResponseStream("text/event-stream");
                response->addHeader("Cache-Control", "no-cache");
                response->addHeader("Connection", "keep-alive");
                
                String serialData = readSerialData(); // Using the function from variables_info
                
                if(serialData.length() > 0) {
                    response->printf("data: %s\n\n", serialData.c_str());
                } else {
                    response->printf("data: \n\n");
                }
                
                request->send(response); });

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
    Serial.println("Web server started");
}
