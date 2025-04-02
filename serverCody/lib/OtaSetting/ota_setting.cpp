#include "ota_setting.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "variables_info.h"
#include "lcd_setting.h"
#include "web_server_setting.h"
// Инициализация переменных состояния
OtaState otaState = OTA_STATE_IDLE;
int otaProgress = 0;
String otaErrorMessage = "";
bool otaActive = false;
unsigned long otaStartTime = 0;
unsigned long otaTimeout = 5 * 60 * 1000; // 5 минут таймаут для режима OTA

bool initOTA() {
    if (!wifiConnected) {
        Serial.println("OTA: WiFi не подключен. OTA не инициализирован.");
        return false;
    }

    // Настройка имени устройства для OTA
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    
    // Настройка пароля для OTA
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    // Настройка порта (по умолчанию 3232)
    ArduinoOTA.setPort(OTA_PORT);
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        
        // Обновление состояния
        otaState = OTA_STATE_UPDATING;
        otaProgress = 0;
        
        // Отображение информации на LCD        
        displayText("OTA Update", 0, 0, true, true);
        displayText("Type: " + type, 0, 1, true);
        
        Serial.println("Start updating " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        // Обновление состояния
        otaState = OTA_STATE_COMPLETE;
        otaProgress = 100;
        
        // Отображение информации на LCD
        displayText("Update Complete", 0, 0, true, true);
        displayText("Rebooting...", 0, 1, true, true);
        
        Serial.println("\nEnd");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        unsigned int percentage = (progress / (total / 100));
        
        // Обновление состояния
        otaProgress = percentage;
        
        // Отображение прогресса на LCD
        String progressBar = "[";
        for (int i = 0; i < 10; i++) {
            if (i < percentage / 10) {
                progressBar += "=";
            } else {
                progressBar += " ";
            }
        }
        progressBar += "]";
        
        displayText(progressBar, 0, 1, true);
        displayText(String(percentage) + "%", 12, 1);
        
        Serial.printf("Progress: %u%%\r", percentage);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        // Обновление состояния
        otaState = OTA_STATE_ERROR;
        
        Serial.printf("Error[%u]: ", error);
        displayText("Update Error", 0, 0, true, true);
        
        switch (error) {
            case OTA_AUTH_ERROR:
                otaErrorMessage = "Auth Failed";
                break;
            case OTA_BEGIN_ERROR:
                otaErrorMessage = "Begin Failed";
                break;
            case OTA_CONNECT_ERROR:
                otaErrorMessage = "Connect Failed";
                break;
            case OTA_RECEIVE_ERROR:
                otaErrorMessage = "Receive Failed";
                break;
            case OTA_END_ERROR:
                otaErrorMessage = "End Failed";
                break;
            default:
                otaErrorMessage = "Unknown Error";
                break;
        }
        
        displayText(otaErrorMessage, 0, 1, true, true);
        Serial.println(otaErrorMessage);
        
        // Выход из режима OTA через 10 секунд после ошибки
        delay(10000);
        otaActive = false;
    });
    
    ArduinoOTA.begin();
    Serial.println("OTA ready");
    
    // Настройка mDNS для легкого обнаружения устройства
    if (MDNS.begin(OTA_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("arduino", "tcp", OTA_PORT);
        Serial.println("MDNS responder started");
        Serial.print("Device URL: http://");
        Serial.print(OTA_HOSTNAME);
        Serial.println(".local");
    }
    
    return true;
}

void handleOTA() {
    // Обработка OTA обновлений
    ArduinoOTA.handle();
    
    // Проверка таймаута режима OTA
    if (otaActive && millis() - otaStartTime > otaTimeout) {
        Serial.println("OTA: Таймаут режима OTA. Возврат к нормальной работе.");
        otaActive = false;
        displayText("OTA Timeout", 0, 0, true, true);
        displayText("Normal mode", 0, 1, true, true);
        delay(2000);
        updateLCD(); // Возврат к обычному отображению
    }
}

void enterOtaMode() {
    // Если WiFi не подключен, пытаемся подключиться
    if (!wifiConnected) {
        displayText("Connecting WiFi", 0, 0, true, true);
        displayText("for OTA...", 0, 1, true, true);
        
        // Попытка подключения к WiFi
        connectWiFi();
        
        if (!wifiConnected) {
            displayText("WiFi Failed", 0, 0, true, true);
            displayText("OTA Unavailable", 0, 1, true, true);
            delay(3000);
            updateLCD(); // Возврат к обычному отображению
            return;
        }
    }
    
    // Инициализация OTA, если еще не инициализирован
    if (otaState == OTA_STATE_IDLE) {
        if (!initOTA()) {
            displayText("OTA Init Failed", 0, 0, true, true);
            delay(3000);
            updateLCD(); // Возврат к обычному отображению
            return;
        }
    }
    
    // Активация режима OTA
    otaActive = true;
    otaStartTime = millis();
    
    // Отображение информации на LCD
    displayText("OTA Mode Active", 0, 0, true, true);
    displayText("IP: " + WiFi.localIP().toString(), 0, 1, true);
    
    Serial.println("OTA: Режим OTA активирован");
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println("Hostname: " + String(OTA_HOSTNAME) + ".local");
    Serial.println("Port: " + String(OTA_PORT));
    Serial.println("Waiting for update...");
}

bool isOtaActive() {
    return otaActive;
}
