#include "mi_io_protocol.h"
#include <MD5Builder.h>
#include <mbedtls/aes.h>
#include <time.h>
#include "mi_io_crypto.h"
#include <WiFi.h>
#include "xiaomi_scanner.h"
#include <spiffs_setting.h>
MiIOProtocol miIO;

MiIOProtocol::MiIOProtocol()
{
    // Генерируем случайный deviceId, если не задан
    deviceId = ESP.getEfuseMac() & 0xFFFFFFFF;
    timestamp = 0;
    strcpy(deviceToken, "0000000000000000000000000000000"); // Токен по умолчанию
}

void MiIOProtocol::begin()
{
    udp.begin(MI_IO_PORT);
    Serial.printf("MiIO протокол запущен на порту %d\n", MI_IO_PORT);
    Serial.printf("Device ID: %08X\n", deviceId);
}

void MiIOProtocol::setDeviceToken(const char *token)
{
    strncpy(deviceToken, token, 32);
    deviceToken[32] = '\0';
}

void MiIOProtocol::setDeviceId(uint32_t id)
{
    deviceId = id;
}

void MiIOProtocol::handlePackets()
{
    int packetSize = udp.parsePacket();
    if (packetSize)
    {
        IPAddress remote_ip = udp.remoteIP();
        uint16_t remote_port = udp.remotePort();

        char buffer[512];
        int len = udp.read(buffer, sizeof(buffer) - 1);
        buffer[len] = '\0';

        // Проверяем, это Hello пакет или обычный пакет
        if (len == MI_IO_HELLO_SIZE && strncmp(buffer, MI_IO_HELLO_MSG, MI_IO_HELLO_SIZE) == 0)
        {
            handleHello(remote_ip, remote_port);
        }
        else
        {
            // Это обычный пакет, нужно расшифровать
            uint8_t decrypted[512];
            decryptPacket((uint8_t *)buffer, len, decrypted);
            handleCommand(decrypted, len - sizeof(MiIOHeader), remote_ip, remote_port);
        }
    }
}

void MiIOProtocol::handleHello(IPAddress &remote_ip, uint16_t remote_port)
{
    // Отправляем ответ на Hello пакет
    char response[32];
    sprintf(response, "MIIO_HELLO %08X", deviceId);
    udp.beginPacket(remote_ip, remote_port);
    udp.write((uint8_t *)response, strlen(response));
    udp.endPacket();

    Serial.printf("Отправлен ответ на Hello: %s\n", response);
}

void MiIOProtocol::handleCommand(uint8_t *buffer, size_t len, IPAddress &remote_ip, uint16_t remote_port)
{
    // Парсим JSON команду
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buffer, len);

    if (error)
    {
        Serial.printf("Ошибка парсинга JSON: %s\n", error.c_str());
        return;
    }

    // Получаем ID метода и параметры
    const char *method = doc["method"];
    uint32_t id = doc["id"];

    Serial.printf("Получена команда: %s (id: %d)\n", method, id);

    // Создаем ответ
    JsonDocument response;
    response["id"] = id;

    // Обрабатываем различные методы
    if (strcmp(method, "miIO.info") == 0)
    {
        // Информация об устройстве
        JsonObject result = response["result"].to<JsonObject>();
        result["model"] = "lumi.gateway.v3"; // Эмулируем шлюз Xiaomi Gateway 3
        result["fw_ver"] = "1.4.7_0160";
        result["hw_ver"] = "ESP32";
        result["mac"] = WiFi.macAddress();
        result["token"] = deviceToken;
        result["life"] = millis() / 1000;
        result["wifi_fw_ver"] = "ESP-IDF 4.4";
        result["mmfree"] = ESP.getFreeHeap();
        result["netif"] = "ap";
    }
    else if (strcmp(method, "get_device_list") == 0 || strcmp(method, "get_device_prop") == 0)
    {
        // Обновляем список эмулируемых устройств
        updateEmulatedDevices();

        // Список устройств
        JsonArray resultArray = response["result"].to<JsonArray>();

        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
        {
            for (const auto &device : devices)
            {
                JsonObject deviceObj = resultArray.add<JsonObject>();

                // Формируем ID устройства на основе MAC-адреса
                String did = generateDeviceDid(device.macAddress.c_str());

                deviceObj["did"] = did;
                deviceObj["model"] = "lumi.sensor_ht.v1";
                deviceObj["type"] = "zigbee";
                deviceObj["online"] = true;

                // Добавляем свойства устройства
                JsonObject props = deviceObj["props"].to<JsonObject>();
                props["temperature"] = device.currentTemperature;
                props["humidity"] = 50; // Заглушка для влажности
                props["voltage"] = 3000 - (100 - device.battery) * 10;
                props["battery"] = device.battery;
            }

            xSemaphoreGive(devicesMutex);
        }
    }
    else if (strcmp(method, "get_device_prop_exp") == 0)
    {
        // Расширенное получение свойств устройств
        JsonArray params = doc["params"];
        JsonArray resultArray = response["result"].to<JsonArray>();

        if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
        {
            for (JsonVariant param : params)
            {
                const char *did = param[0];
                const char *siid = param[1];
                const char *piid = param[2];

                // Ищем устройство по did
                bool found = false;
                for (const auto &device : devices)
                {
                    String deviceDid = generateDeviceDid(device.macAddress.c_str());

                    if (deviceDid == did)
                    {
                        JsonObject resultObj = resultArray.add<JsonObject>();
                        resultObj["did"] = did;
                        resultObj["siid"] = siid;
                        resultObj["piid"] = piid;

                        // Определяем, какое свойство запрашивается
                        if (strcmp(siid, "2") == 0 && strcmp(piid, "1") == 0)
                        {
                            // Температура
                            resultObj["value"] = device.currentTemperature;
                        }
                        else if (strcmp(siid, "2") == 0 && strcmp(piid, "2") == 0)
                        {
                            // Влажность (заглушка)
                            resultObj["value"] = 50;
                        }
                        else if (strcmp(siid, "3") == 0 && strcmp(piid, "1") == 0)
                        {
                            // Заряд батареи
                            resultObj["value"] = device.battery;
                        }
                        else
                        {
                            // Неизвестное свойство
                            resultObj["value"] = 0;
                        }

                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    JsonObject resultObj = resultArray.add<JsonObject>();
                    resultObj["did"] = did;
                    resultObj["siid"] = siid;
                    resultObj["piid"] = piid;
                    resultObj["code"] = -704002; // Код ошибки: устройство не найдено
                }
            }

            xSemaphoreGive(devicesMutex);
        }
    }
    else if (strcmp(method, "set_device_prop") == 0)
    {
        // Установка свойств устройства
        JsonArray params = doc["params"];

        if (params.size() >= 3)
        {
            const char *did = params[0];
            const char *prop = params[1];
            JsonVariant value = params[2];

            if (xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE)
            {
                for (auto &device : devices)
                {
                    String deviceDid = generateDeviceDid(device.macAddress.c_str());

                    if (deviceDid == did)
                    {
                        // Обрабатываем различные свойства
                        if (strcmp(prop, "target_temperature") == 0)
                        {
                            Serial.println("Изменена целевая температура, сохраняем результаты");
                            device.targetTemperature = value.as<float>();
                            saveClientsToFile(); // Сохраняем изменения
                        }
                        else if (strcmp(prop, "enabled") == 0)
                        {
                            Serial.println("Изменения доступности устройства, сохраняем результаты");
                            device.enabled = value.as<bool>();
                            saveClientsToFile(); // Сохраняем изменения
                        }
                        else if (strcmp(prop, "name") == 0)
                        {
                            Serial.println("Изменено наименование, сохраняем результаты");
                            device.name = value.as<String>().c_str();
                            saveClientsToFile(); // Сохраняем изменения
                        }

                        break;
                    }
                }

                xSemaphoreGive(devicesMutex);
            }
        }

        // Отправляем успешный ответ
        response["result"] = "ok";
    }
    else if (strcmp(method, "local.status") == 0)
    {
        // Статус шлюза
        JsonObject result = response["result"].to<JsonObject>();
        result["cloud_link"] = true;
        result["wifi_link"] = true;
        result["life"] = millis() / 1000;
        result["zigbee"] = true;
        result["ble"] = true;
    }
    else
    {
        // Неизвестный метод
        response["error"] = "Unknown method";
    }

    // Отправляем ответ
    sendResponse(response, remote_ip, remote_port);
}

void MiIOProtocol::sendResponse(JsonDocument &doc, IPAddress &remote_ip, uint16_t remote_port)
{
    // Сериализуем JSON
    String jsonStr;
    serializeJson(doc, jsonStr);

    // Подготавливаем пакет
    uint8_t packet[512];
    MiIOHeader *header = (MiIOHeader *)packet;

    // Заполняем заголовок
    header->magic = 0x2131;
    header->length = sizeof(MiIOHeader) + jsonStr.length();
    header->unknown = 0;
    header->deviceId = deviceId;
    header->timestamp = ++timestamp;

    // Копируем данные
    memcpy(packet + sizeof(MiIOHeader), jsonStr.c_str(), jsonStr.length());

    // Вычисляем контрольную сумму
    calculateChecksum(packet, header->length, header->checksum);

    // Шифруем пакет
    uint8_t encrypted[512];
    encryptPacket(packet, header->length, encrypted);

    // Отправляем пакет
    udp.beginPacket(remote_ip, remote_port);
    udp.write(encrypted, header->length);
    udp.endPacket();

    Serial.printf("Отправлен ответ: %s\n", jsonStr.c_str());
}

void MiIOProtocol::calculateChecksum(uint8_t *buffer, size_t len, uint8_t *output)
{
    // Для вычисления контрольной суммы используем MD5
    MD5Builder md5;
    md5.begin();
    md5.add(buffer, len - 16);           // Исключаем поле checksum
    md5.add((uint8_t *)deviceToken, 32); // Добавляем токен
    md5.calculate();
    md5.getBytes(output);
}

// Обновляем методы шифрования и дешифрования
void MiIOProtocol::encryptPacket(uint8_t *buffer, size_t len, uint8_t *output)
{
    // Преобразуем токен в ключ
    uint8_t key[16];
    for (int i = 0; i < 16; i++)
    {
        key[i] = 0;
        for (int j = 0; j < 2; j++)
        {
            char c = deviceToken[i * 2 + j];
            uint8_t val = 0;
            if (c >= '0' && c <= '9')
                val = c - '0';
            else if (c >= 'a' && c <= 'f')
                val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                val = c - 'A' + 10;
            key[i] = (key[i] << 4) | val;
        }
    }

    // Шифруем данные
    miioEncrypt(key, buffer, len, output);
}

void MiIOProtocol::decryptPacket(uint8_t *buffer, size_t len, uint8_t *output)
{
    // Преобразуем токен в ключ
    uint8_t key[16];
    for (int i = 0; i < 16; i++)
    {
        key[i] = 0;
        for (int j = 0; j < 2; j++)
        {
            char c = deviceToken[i * 2 + j];
            uint8_t val = 0;
            if (c >= '0' && c <= '9')
                val = c - '0';
            else if (c >= 'a' && c <= 'f')
                val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                val = c - 'A' + 10;
            key[i] = (key[i] << 4) | val;
        }
    }

    // Дешифруем данные
    miioDecrypt(key, buffer, len, output);
}
