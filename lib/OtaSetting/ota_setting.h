#ifndef OTA_SETTING_H
#define OTA_SETTING_H

#include <Arduino.h>

// Настройки OTA
#define OTA_HOSTNAME "ESP32-S3-Thermostat"  // Имя устройства для OTA
#define OTA_PASSWORD "admin123"             // Пароль для OTA (рекомендуется изменить)
#define OTA_PORT 3232                       // Порт для OTA (стандартный: 3232)

// Состояния OTA
enum OtaState {
    OTA_STATE_IDLE,           // Ожидание
    OTA_STATE_UPDATING,       // Идет обновление
    OTA_STATE_COMPLETE,       // Обновление завершено
    OTA_STATE_ERROR           // Ошибка обновления
};

// Текущее состояние OTA
extern OtaState otaState;
// Прогресс обновления (0-100%)
extern int otaProgress;
// Сообщение об ошибке
extern String otaErrorMessage;

// Инициализация OTA
bool initOTA();

// Обработка OTA обновлений (вызывать в loop)
void handleOTA();

// Принудительный переход в режим ожидания OTA
void enterOtaMode();

// Проверка, активен ли режим OTA
bool isOtaActive();

#endif // OTA_SETTING_H
