#include <web_server_setting.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <variables_info.h>
#include <spiffs_setting.h>
#include "xiaomi_scanner.h"
#include "device_manager.h"
#include "task_monitor.h"
#include <SPIFFS.h>

// Web Server
AsyncWebServer server(80);

// Создаем экземпляр AsyncEventSource
AsyncEventSource serialEvents("/log_events");

void connectWiFi()
{
    logAndSend("Connecting to WiFi: " + String(wifiCredentials.ssid.c_str()) + ", password: " + String(wifiCredentials.password.c_str()));
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
        logAndSend("WiFi connected. Ip: " + String(WiFi.localIP().toString().c_str()));
        wifiConnected = true;
    }
    else
    {
        logAndSend("Failed to connect to WiFi");
        wifiConnected = false;
    }
}

// web server +++++++++++++++++++++++++++++++++
void initWebServer()
{
    // Add event source handler
    server.addHandler(&serialEvents);

    // Event source connection handlers
    serialEvents.onConnect([](AsyncEventSourceClient *client)
                           {
         logAndSend("Client connected to SSE"); 
         client->send("Connected to ESP32 log stream"); });

// GET /clients (get list of all clients)
    server.on("/clients", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DeviceResponse response;
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_GET_DEVICES;
        sendDeviceCmd(cmd, &response);
        
        JsonDocument doc;
        JsonArray devicesArray = doc.to<JsonArray>();

        if (response.success) {
            for (const auto& device : response.devices) {
                JsonObject deviceObj = devicesArray.add<JsonObject>();
                
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
                
                JsonArray pinsArray = deviceObj["gpioPins"].to<JsonArray>();
                for (uint8_t pin : device.gpioPins) {
                    pinsArray.add(pin);
                }
            }
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr.c_str()); });

server.on("/availablegpio", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DeviceResponse response;
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
        sendDeviceCmd(cmd, &response);
        
        JsonDocument doc;
        JsonArray gpioArray = doc.to<JsonArray>();
        
        if (response.success) {
            for (const auto& gpio : response.gpioPins) {
                JsonObject gpioObj = gpioArray.add<JsonObject>();
                gpioObj["pin"] = gpio.pin;
                gpioObj["state"] = gpio.state;
                gpioObj["name"] = gpio.name;
            }
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr); });
    server.on("/availablegpio", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("availablegpio", true))
        {
            String jsonStr = request->getParam("availablegpio", true)->value();
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonStr);
            
            if (!error) {
                JsonArray gpioArray = doc.as<JsonArray>();
                for (JsonObject gpioObj : gpioArray) {
                    DeviceCmd cmd{};
                    cmd.type = DeviceCmdType::CMD_UPDATE_GPIO_CONFIG;
                    cmd.gpioPin = gpioObj["pin"].as<uint8_t>();
                    cmd.gpioState = gpioObj["state"].as<uint8_t>();
                    cmd.gpioName = gpioObj["name"].as<const char*>();
                    sendDeviceCmdAsync(cmd);
                }
                
                request->send(200, "text/plain", "availablegpio updated");
            } else {
                request->send(400, "text/plain", "Invalid JSON format");
            }
        } else {
            request->send(400, "text/plain", "availablegpio parameter not found");
        } });
server.on("/serverinfo", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DeviceResponse response;
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_GET_SERVER_INFO;
        sendDeviceCmd(cmd, &response);
        
        JsonDocument doc;
        doc["cpu_frequency_mhz"] = ESP.getCpuFreqMHz();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["processor_cores"] = ESP.getChipCores();
        doc["sdk_version"] = ESP.getSdkVersion();
        doc["sram_size_bytes"] = ESP.getHeapSize();
        doc["free_sram_bytes"] = ESP.getFreeHeap();
        doc["flash_size_bytes"] = ESP.getFlashChipSize();
        doc["flash_frequency_mhz"] = ESP.getFlashChipSpeed() / 1000000;
        doc["psram_size_bytes"] = ESP.getPsramSize();
        doc["free_psram_bytes"] = ESP.getFreePsram();
        doc["flash_mode"] = ESP.getFlashChipMode() == FM_QIO ? "QIO" : "DIO";
        doc["chip_id"] = ESP.getEfuseMac();
        doc["millis"] = formatHeatingTime(response.success ? response.serverWorkTime : serverWorkTime);
        doc["board_temperature"] = response.success ? response.boardTemperature : board_temperature;
        
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload.c_str()); });
// POST /client/{address} (update info about a client)
    server.on("/client", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("address", true))
        {
            String address = request->getParam("address", true)->value();
            
            DeviceCmd cmd{};
            cmd.type = DeviceCmdType::CMD_UPDATE_DEVICE_CONFIG;
            cmd.macAddress = address.c_str();
            bool hasChanges = false;
            
            if (request->hasParam("name", true)) {
                String newName = request->getParam("name", true)->value();
                cmd.name = newName.c_str();
                hasChanges = true;
            }
            
            if (request->hasParam("targetTemperature", true)) {
                String tempStr = request->getParam("targetTemperature", true)->value();
                cmd.targetTemperature = tempStr.toFloat();
                hasChanges = true;
            }
            
            if (request->hasParam("enabled", true)) {
                String tempStr = request->getParam("enabled", true)->value();
                cmd.enabled = tempStr == "true";
                hasChanges = true;
            }
            
            if (request->hasParam("gpioPins", true)) {
                String gpioStr = request->getParam("gpioPins", true)->value();
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, gpioStr);
                
                if (!error && doc.is<JsonArray>()) {
                    JsonArray pinsArray = doc.as<JsonArray>();
                    for (uint8_t pin : pinsArray) {
                        cmd.gpioPins.push_back(pin);
                    }
                    hasChanges = true;
                }
            }
            
            if (hasChanges) {
                sendDeviceCmdAsync(cmd);
                logAndSend("Получены изменения по HTTP для устройства: " + address);
                request->send(200, "text/plain", "Client updated");
            } else {
                request->send(400, "text/plain", "No valid parameters");
            }
            return;
        }                
        request->send(404, "text/plain", "Client not found"); });
server.on("/client", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("address", true))
        {
            String address = request->getParam("address", true)->value();
           
            DeviceCmd cmd{};
            cmd.type = DeviceCmdType::CMD_REMOVE_DEVICE;
            cmd.macAddress = address.c_str();
            sendDeviceCmdAsync(cmd);
            
            logAndSend("Удаляем устройство " + address);
            request->send(200, "text/plain", "Client remove");
            return;
        }                
        request->send(404, "text/plain", "Client not found"); });
    // GET /scan (start BLE scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                logAndSend("Получен запрос на запуск сканирования устройств");
                startXiaomiScan();
            request->send(200, "text/plain", "BLE Scan started"); });
    // Добавляем обработчик для получения статистики обогрева
    server.on("/heating_stats", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DeviceResponse response;
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_GET_DEVICES;
        sendDeviceCmd(cmd, &response);
        
        JsonDocument doc;
        JsonArray statsArray = doc.to<JsonArray>();
        
        if (response.success) {
            for (const auto& device : response.devices) {
                JsonObject deviceObj = statsArray.add<JsonObject>();
                deviceObj["name"] = device.name;
                deviceObj["macAddress"] = device.macAddress;
                deviceObj["currentTemperature"] = device.currentTemperature;
                deviceObj["targetTemperature"] = device.targetTemperature;
                deviceObj["heatingActive"] = device.heatingActive;            
                deviceObj["totalHeatingTimeMs"] =  device.totalHeatingTime;
                deviceObj["totalHeatingTimeFormatted"] = formatHeatingTime(device.totalHeatingTime);
            }
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr); });

    // Добавляем обработчик для сброса статистики обогрева
    server.on("/reset_stats", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        bool resetAll = true;
        String deviceMac = "";
        
        if (request->hasParam("device", true)) {
            deviceMac = request->getParam("device", true)->value();
            resetAll = false;
        }        
        
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_RESET_STATS;
        cmd.resetAll = resetAll;
        cmd.resetDeviceMac = deviceMac.c_str();
        sendDeviceCmdAsync(cmd);
        
        logAndSend("Сброшена статистика");
        request->send(200, "text/plain", "Статистика сброшена"); });

server.on("/heating_gpio_stats", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DeviceResponse response;
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_GET_GPIO_STATS;
        sendDeviceCmd(cmd, &response);
        
        JsonDocument doc;
        JsonArray statsArray = doc.to<JsonArray>();
        
        if (response.success) {
            for (const auto& gpio : response.gpioPins) {
                JsonObject gpioObj = statsArray.add<JsonObject>();
                gpioObj["pin"] = gpio.pin;  
                gpioObj["state"] = gpio.state;  
                gpioObj["name"] = gpio.name;            
                gpioObj["totalHeatingTimeFormatted"] = formatHeatingTime(gpio.totalHeatingTime);
            }
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr); });

    server.on("/reset_gpio_stats", HTTP_POST, [](AsyncWebServerRequest *request)
              {                
        DeviceCmd cmd{};
        cmd.type = DeviceCmdType::CMD_RESET_GPIO_STATS;
        sendDeviceCmdAsync(cmd);
        
        logAndSend("Сброшена статистика GPIO");
        request->send(200, "text/plain", "Статистика сброшена"); });

    server.on("/reset_work_time", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {       
        logAndSend("Сброшено время работы, сохраняем результаты");        
        serverWorkTime = 0;
        SpiffsSetting::saveServerSetting();
        request->send(200, "text/plain", "Статистика сброшена"); });

    server.on("/save_hysteresis_temp", HTTP_POST, [](AsyncWebServerRequest *request)
              {       
                if (request->hasParam("hysteresis_temp", true)) {
                    hysteresisTemp = request->getParam("hysteresis_temp", true)->value().toFloat();
                    logAndSend("Cохраняем настройки для гистерезиса");  
                    SpiffsSetting::saveServerSetting();
                    request->send(200, "text/plain", "Настройки для гистерезиса сохранены"); 
                }
     request->send(404, "text/plain", "Param not found"); });

    server.on("/get_hysteresis_temp", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", String(hysteresisTemp, 1)); });

    // Task health monitoring endpoint
    server.on("/task_health", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        TASK_MONITOR.getHealthReport(doc);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Restart task endpoint
    server.on("/restart_task", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("task", true)) {
            String taskName = request->getParam("task", true)->value();
            TASK_MONITOR.restartTask(taskName.c_str());
            request->send(200, "text/plain", "Task restart requested");
        } else {
            request->send(400, "text/plain", "Task name required");
        }
    });
    
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
              
    // Добавляем обработчик для страницы настройки устройств
    server.on("/setup_device.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/setup_device.html", "text/html"); });

    server.begin();
    logAndSend("Web server started");
}
