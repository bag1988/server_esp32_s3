#include <cstdint>
#ifndef XIAOMI_BLE_CLIENT_H
#define XIAOMI_BLE_CLIENT_H

// Функции для инициализации и работы с BLE клиентом Xiaomi
void setupXiaomiSensor();
void loopXiaomiSensor();
void updateSensorData(float temp, uint8_t humid, uint8_t battery);

#endif