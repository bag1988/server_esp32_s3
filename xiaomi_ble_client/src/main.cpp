#include <Arduino.h>
#include "xiaomi_ble_client.h"
#include <Wire.h>
#include <Adafruit_AHTX0.h> // Библиотека для работы с AHT20

// Создаем объект датчика
Adafruit_AHTX0 aht;

void setup() {
  Serial.begin(115200);
  Serial.println("Инициализация системы...");
  
  // Инициализация I2C для AHT20
  Wire.begin();
  
  // Инициализация датчика AHT20
  if (!aht.begin()) {
    Serial.println("Не удалось найти датчик AHT20. Проверьте подключение!");
    while (1) delay(10); // Остановка, если датчик не найден
  }
  Serial.println("Датчик AHT20 инициализирован успешно");
  
  // Инициализация BLE сервера
  setupXiaomiSensor();
}

void loop() {
  // Считывание данных с датчика AHT20
  sensors_event_t humidity, temp;
  
  if (aht.getEvent(&humidity, &temp)) {
    float temperature = temp.temperature;
    uint8_t humidityValue = (uint8_t)humidity.relative_humidity;
    
    // Вывод данных в консоль для отладки
    Serial.print("Температура: "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("Влажность: "); Serial.print(humidityValue); Serial.println(" %");
    
    // Обновление данных в BLE сервере
    // Предполагаем, что уровень батареи 100% (можно добавить реальное измерение)
    updateSensorData(temperature, humidityValue, 100);
  } else {
    Serial.println("Ошибка при чтении данных с датчика AHT20");
  }
  
  // Вызов основного цикла BLE сервера
  loopXiaomiSensor();
  
  // Небольшая задержка для стабильности
  delay(100);
}
