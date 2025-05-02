#ifndef MI_IO_PROTOCOL_H
#define MI_IO_PROTOCOL_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "variables_info.h"

// Константы протокола miIO
#define MI_IO_PORT 54321
#define MI_IO_HELLO_MSG "MIIO_HELLO"
#define MI_IO_HELLO_SIZE 10

// Структура заголовка пакета miIO
typedef struct
{
    uint16_t magic;       // Всегда 0x2131
    uint16_t length;      // Длина пакета
    uint32_t unknown;     // Неизвестное поле, обычно 0x00000000
    uint32_t deviceId;    // ID устройства
    uint32_t timestamp;   // Временная метка
    uint8_t checksum[16]; // MD5 хеш
} MiIOHeader;

class MiIOProtocol
{
public:
    MiIOProtocol();
    void begin();
    void handlePackets();
    void setDeviceToken(const char *token);
    void setDeviceId(uint32_t id);

private:
    WiFiUDP udp;
    char deviceToken[32];
    uint32_t deviceId;
    uint32_t timestamp;

    void handleHello(IPAddress &remote_ip, uint16_t remote_port);
    void handleCommand(uint8_t *buffer, size_t len, IPAddress &remote_ip, uint16_t remote_port);
    void sendResponse(JsonDocument &doc, IPAddress &remote_ip, uint16_t remote_port);
    void encryptPacket(uint8_t *buffer, size_t len, uint8_t *output);
    void decryptPacket(uint8_t *buffer, size_t len, uint8_t *output);
    void calculateChecksum(uint8_t *buffer, size_t len, uint8_t *output);
};

extern MiIOProtocol miIO;

#endif // MI_IO_PROTOCOL_H
