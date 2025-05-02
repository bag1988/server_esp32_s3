#include "mqtt_client.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include "variables_info.h"
#include "logger.h"
#include <ArduinoJson.h>

// MQTT клиент
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Настройки MQTT
const char* mqtt_server = "192.168.1.100"; // Адрес MQTT брокера
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt_user";
const char* mqtt_password = "mqtt_password";

// Функция обратного вызова для получения сообщений
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    LOG_I("Получено сообщение [%s]", topic);
    
    // Преобразуем payload в строку
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    LOG_I("Содержимое: %s", message);
    
    // Обработка команд
    if (strcmp(topic, "xiaomi_gateway/command") == 0) {
        // Обработка команд
    }
}

// Настройка MQTT клиента
void setupMQTT() {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    LOG_I("MQTT клиент настроен");
}

// Подключение к MQTT брокеру
bool connectMQTT() {
    if (mqttClient.connected()) {
        return true;
    }
    
    LOG_I("Подключение к MQTT брокеру...");
    
    // Создаем уникальный ID клиента
    String clientId = "XiaomiGateway-";
    clientId += String(random(0xffff), HEX);
    
    // Пытаемся подключиться
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
        LOG_I("Подключено к MQTT брокеру");
        
        // Подписываемся на топики
        mqttClient.subscribe("xiaomi_gateway/command");
        
        return true;
    } else {
        LOG_E("Ошибка подключения к MQTT, rc=%d", mqttClient.state());
        return false;
    }
}

// Обработка MQTT сообщений
void handleMQTT() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    
    mqttClient.loop();
}

// Публикация данных датчиков
void publishSensorData() {
    if (!mqttClient.connected()) {
        if (!connectMQTT()) {
            return;
        }
    }
    
    if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE) {
        for (const auto& device : devices) {
            // Создаем топик для устройства
            String topic = "xiaomi_gateway/sensor/";
            topic += device.macAddress.c_str();
            topic.replace(":", "");
            
            // Создаем JSON с данными
            JsonDocument doc;
            doc["temperature"] = device.currentTemperature;
            doc["target_temperature"] = device.targetTemperature;
            doc["battery"] = device.battery;
            doc["enabled"] = device.enabled;
            doc["name"] = device.name.c_str();
            
            // Сериализуем JSON
            String payload;
            serializeJson(doc, payload);
            
            // Публикуем данные
            mqttClient.publish(topic.c_str(), payload.c_str());
        }
        
        xSemaphoreGive(devicesMutex);
    }
}
