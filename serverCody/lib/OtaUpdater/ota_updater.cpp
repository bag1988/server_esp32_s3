#include "ota_updater.h"
#include <ArduinoOTA.h>
#include "logger.h"

void setupOTA() {
    // Настройка OTA обновлений
    ArduinoOTA.setHostname("xiaomi-gateway");
    ArduinoOTA.setPassword("admin");
    
    ArduinoOTA.onStart([]() {
        LOG_I("Начало OTA обновления");
    });
    
    ArduinoOTA.onEnd([]() {
        LOG_I("OTA обновление завершено");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        LOG_I("Прогресс: %u%%", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        LOG_E("Ошибка OTA: %u", error);
    });
    
    ArduinoOTA.begin();
    LOG_I("OTA обновления настроены");
}

void handleOTA() {
    ArduinoOTA.handle();
}
