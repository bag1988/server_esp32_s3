#include "mdns_service.h"
#include <ESPmDNS.h>
#include <WiFi.h>

void setupMDNS() {
    // Получаем последние 6 символов MAC-адреса для уникального имени
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String deviceName = "xiaomi-gateway-" + mac.substring(6);
    
    if (MDNS.begin(deviceName.c_str())) {
        // Регистрируем сервис _miio._udp
        MDNS.addService("_miio", "_udp", 54321);
        
        Serial.printf("mDNS запущен как %s.local\n", deviceName.c_str());
        Serial.println("Устройство должно быть обнаружено в приложении Mi Home");
    } else {
        Serial.println("Ошибка при запуске mDNS!");
    }
}
