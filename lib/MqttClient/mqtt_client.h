#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>

void setupMQTT();
void handleMQTT();
void publishSensorData();

#endif // MQTT_CLIENT_H
