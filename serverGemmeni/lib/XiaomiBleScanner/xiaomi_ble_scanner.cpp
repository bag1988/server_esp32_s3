#include <xiaomi_ble_scanner.h>

// Глобальные переменные
std::map<String, XiaomiSensorData> xiaomiSensors;
BLEScan* pBLEScan = nullptr;

// Класс для обработки результатов сканирования
class XiaomiAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Обработка рекламного пакета
        processXiaomiAdvertisement(advertisedDevice);
    }
};

// Инициализация сканера BLE
void setupXiaomiScanner() {
    BLEDevice::init("XiaomiScannerESP32");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new XiaomiAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); // Активное сканирование запрашивает дополнительную информацию
    pBLEScan->setInterval(100);    // Интервал между сканированиями (мс)
    pBLEScan->setWindow(99);       // Длительность сканирования (мс)
    
    Serial.println("Сканер BLE для датчиков Xiaomi инициализирован");
}

// Запуск сканирования BLE
void startXiaomiScan(uint32_t duration) {
    Serial.println("Начало сканирования BLE...");
    BLEScanResults foundDevices = pBLEScan->start(duration, false);
    Serial.print("Найдено устройств: ");
    Serial.println(foundDevices.getCount());
    pBLEScan->clearResults();
}

// Обработка рекламного пакета
void processXiaomiAdvertisement(BLEAdvertisedDevice advertisedDevice) {
    // Проверяем, есть ли у устройства производственные данные
    if (advertisedDevice.haveManufacturerData()) {
        std::string manufacturerData = advertisedDevice.getManufacturerData();
        
        // Проверяем, является ли это устройство Xiaomi
        // Xiaomi использует ID производителя 0x0157 (в little-endian: 0x5701)
        if (manufacturerData.length() >= 2 && 
            (uint8_t)manufacturerData[0] == 0x57 && 
            (uint8_t)manufacturerData[1] == 0x01) {
            
            // Получаем MAC-адрес устройства
            String deviceAddress = advertisedDevice.getAddress().toString().c_str();
            
            // Создаем или обновляем запись датчика
            XiaomiSensorData sensorData;
            sensorData.address = deviceAddress;
            sensorData.lastUpdate = millis();
            sensorData.isOnline = true;
            
            // Если устройство имеет имя, сохраняем его
            if (advertisedDevice.haveName()) {
                sensorData.name = advertisedDevice.getName().c_str();
            } else {
                sensorData.name = "Xiaomi " + deviceAddress.substring(deviceAddress.length() - 5);
            }
            
            // Парсим данные из рекламного пакета
            uint8_t* dataPtr = (uint8_t*)manufacturerData.data();
            size_t dataLen = manufacturerData.length();
            
            if (parseXiaomiData(dataPtr, dataLen, sensorData)) {
                // Обновляем или добавляем датчик в карту
                xiaomiSensors[deviceAddress] = sensorData;
                
                // Выводим информацию о датчике
                Serial.print("Обновлены данные датчика: ");
                Serial.print(sensorData.name);
                Serial.print(" (");
                Serial.print(deviceAddress);
                Serial.print(") - Температура: ");
                Serial.print(sensorData.temperature);
                Serial.print("°C, Влажность: ");
                Serial.print(sensorData.humidity);
                Serial.print("%, Батарея: ");
                Serial.print(sensorData.battery);
                Serial.println("%");
            }
        }
    }
}

// Парсинг данных Xiaomi из рекламного пакета
bool parseXiaomiData(uint8_t* data, size_t length, XiaomiSensorData& sensorData) {
    // Проверяем минимальную длину данных
    if (length < 5) return false;
    
    // Пропускаем заголовок (ID производителя - 2 байта)
    uint8_t* p = data + 2;
    size_t remainingLength = length - 2;
    
    // Флаг, указывающий, были ли найдены какие-либо данные
    bool dataFound = false;
    
    // Парсим данные в формате TLV (Type-Length-Value)
    while (remainingLength >= 3) { // Минимум 1 байт тип + 1 байт длина + 1 байт значение
        uint8_t type = *p++;
        uint8_t dataLen = *p++;
        
        // Проверяем, достаточно ли данных
        if (dataLen + 2 > remainingLength) break;
        
        // Обрабатываем разные типы данных
        switch (type) {
            case 0x0D: // Температура и влажность
                if (dataLen >= 4) {
                    // Температура (2 байта, little-endian, значение * 10)
                    int16_t tempRaw = (int16_t)((p[1] << 8) | p[0]);
                    sensorData.temperature = tempRaw / 10.0;
                    
                    // Влажность (1 байт, значение)
                    sensorData.humidity = p[2];
                    
                    dataFound = true;
                }
                break;
                
            case 0x0A: // Батарея
                if (dataLen >= 1) {
                    sensorData.battery = p[0];
                    dataFound = true;
                }
                break;
                
            // Можно добавить обработку других типов данных
        }
        
        // Переходим к следующему блоку данных
        p += dataLen;
        remainingLength -= (dataLen + 2);
    }
    
    return dataFound;
}

// Вывод данных всех датчиков
void printSensorData() {
    Serial.println("\n--- Данные датчиков Xiaomi ---");
    
    if (xiaomiSensors.empty()) {
        Serial.println("Датчики не найдены");
        return;
    }
    
    for (auto& pair : xiaomiSensors) {
        XiaomiSensorData& sensor = pair.second;
        
        // Проверяем, не устарели ли данные (более 5 минут)
        if (millis() - sensor.lastUpdate > 300000) {
            sensor.isOnline = false;
        }
        
        Serial.print(sensor.name);
        Serial.print(" (");
        Serial.print(sensor.address);
        Serial.print(") - ");
        
        if (sensor.isOnline) {
            Serial.print("Онлайн, Температура: ");
            Serial.print(sensor.temperature);
            Serial.print("°C, Влажность: ");
            Serial.print(sensor.humidity);
            Serial.print("%, Батарея: ");
            Serial.print(sensor.battery);
            Serial.println("%");
        } else {
            Serial.println("Оффлайн (нет данных более 5 минут)");
        }
    }
    
    Serial.println("-----------------------------\n");
}
